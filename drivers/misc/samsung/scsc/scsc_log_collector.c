/********************************************************************************
 *
 *   Copyright (c) 2016 - 2019 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ********************************************************************************/
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/limits.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <scsc/scsc_log_collector.h>
#include "scsc_log_collector_proc.h"
#include "scsc_log_collector_mmap.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

#define SCSC_NUM_CHUNKS_SUPPORTED	13

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#define SCSC_SUPPORT_LOG_COLLECT_TO_FILE
#endif

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
#define TO_RAM				0
#define TO_FILE				1
#endif
/* Add-remove supported chunks on this kernel */
static u8 chunk_supported_sbl[SCSC_NUM_CHUNKS_SUPPORTED] = {
	SCSC_LOG_CHUNK_SYNC,
	SCSC_LOG_CHUNK_IMP,
	SCSC_LOG_CHUNK_MXL,
	SCSC_LOG_CHUNK_UDI,
	SCSC_LOG_CHUNK_BT_HCF,
	SCSC_LOG_CHUNK_WLAN_HCF,
	SCSC_LOG_CHUNK_HIP4_SAMPLER,
	SCSC_LOG_RESERVED_COMMON,
	SCSC_LOG_RESERVED_BT,
	SCSC_LOG_RESERVED_WLAN,
	SCSC_LOG_RESERVED_RADIO,
	SCSC_LOG_MINIMOREDUMP,
	SCSC_LOG_CHUNK_LOGRING,
};

static int scsc_log_collector_collect(enum scsc_log_reason reason, u16 reason_code);

static atomic_t in_collection;

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
/* Collect logs in an intermediate buffer to be collected at later time (mmap or wq) */
static bool collect_to_ram = true;
module_param(collect_to_ram, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(collect_to_ram, "Collect buffer in ram");
#endif

static char collection_dir_buf[256] = "/data/vendor/log/wifi";
module_param_string(collection_target_directory, collection_dir_buf, sizeof(collection_dir_buf), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(collection_target_directory, "Specify collection target directory");

static bool sable_collection_off;
static int sable_collection_off_set_param_cb(const char *val,
					     const struct kernel_param *kp)
{
	bool nval;

	if (!val || strtobool(val, &nval))
		return -EINVAL;

	if (sable_collection_off ^ nval) {
		sable_collection_off = nval;
		pr_info("Sable Log Collection is now %sABLED.\n",
			sable_collection_off ? "DIS" : "EN");
	}
	return 0;
}

/**
 * As described in struct kernel_param+ops the _get method:
 * -> returns length written or -errno.  Buffer is 4k (ie. be short!)
 */
static int sable_collection_off_get_param_cb(char *buffer,
					     const struct kernel_param *kp)
{
	return sprintf(buffer, "%c", sable_collection_off ? 'Y' : 'N');
}

static struct kernel_param_ops sable_collection_off_ops = {
	.set = sable_collection_off_set_param_cb,
	.get = sable_collection_off_get_param_cb,
};
module_param_cb(sable_collection_off, &sable_collection_off_ops, NULL, 0644);
MODULE_PARM_DESC(sable_collection_off, "Disable SABLE Log Collection. This will inhibit also MXLOGGER");

struct scsc_log_client {
	struct list_head list;
	struct scsc_log_collector_client *collect_client;
};
static struct scsc_log_collector_list { struct list_head list; } scsc_log_collector_list = {
	.list = LIST_HEAD_INIT(scsc_log_collector_list.list)
};

static struct scsc_log_status {
#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	struct file *fp;
#endif
	loff_t pos;
	bool in_collection;
	char fapi_ver[SCSC_LOG_FAPI_VERSION_SIZE];

	unsigned char *buf;
	struct workqueue_struct *collection_workq;
	struct work_struct	collect_work;
	enum scsc_log_reason    collect_reason;
	u16 reason_code;
	struct mutex collection_serial;
	bool observer_present;
	struct scsc_log_collector_mx_cb *singleton_mx_cb;
} log_status;

static DEFINE_MUTEX(log_mutex);

static void collection_worker(struct work_struct *work)
{
	struct scsc_log_status *ls;

	ls = container_of(work, struct scsc_log_status, collect_work);
	/* ls cannot be NULL due to pointer arithmetic */

	pr_info("SCSC running scheduled Log Collection - collect reason:%d reason code:%d\n",
		 ls->collect_reason, ls->reason_code);
	scsc_log_collector_collect(ls->collect_reason, ls->reason_code);
	atomic_set(&in_collection, 0);
}

/* Module init */
int __init scsc_log_collector(void)
{
	pr_info("Log Collector Init\n");

	log_status.in_collection = false;
	log_status.collection_workq = create_workqueue("log_collector");
	if (log_status.collection_workq)
		INIT_WORK(&log_status.collect_work, collection_worker);
	/* Update mxlogger status on init.*/
	pr_info("Sable Log Collection is now %sABLED.\n",
		sable_collection_off ? "DIS" : "EN");

	/* Create the buffer on the constructor */
	log_status.buf = vzalloc(SCSC_LOG_COLLECT_MAX_SIZE);
	if (IS_ERR_OR_NULL(log_status.buf)) {
		pr_err("open allocating memmory err = %ld\n", PTR_ERR(log_status.buf));
		log_status.buf = NULL;
	}

	mutex_init(&log_status.collection_serial);

	scsc_log_collect_proc_create();
	scsc_log_collector_mmap_create();
	return 0;
}

void __exit scsc_log_collector_exit(void)
{
	if (log_status.buf)
		vfree(log_status.buf);

	scsc_log_collect_proc_remove();
	if (log_status.collection_workq) {
		flush_workqueue(log_status.collection_workq);
		destroy_workqueue(log_status.collection_workq);
		log_status.collection_workq = NULL;
	}

	pr_info("Log Collect Unloaded\n");
}

module_init(scsc_log_collector);
module_exit(scsc_log_collector_exit);

static bool scsc_is_chunk_supported(u8 type)
{
	u8 i;

	for (i = 0; i < SCSC_NUM_CHUNKS_SUPPORTED; i++) {
		if (type == chunk_supported_sbl[i])
			return true;
	}

	return false;
}

static int scsc_log_collector_compare(void *priv, struct list_head *A, struct list_head *B)
{
	struct scsc_log_client *a = list_entry(A, typeof(*a), list);
	struct scsc_log_client *b = list_entry(B, typeof(*b), list);

	if (a->collect_client->type < b->collect_client->type)
		return -1;
	else
		return 1;
}

int scsc_log_collector_register_mx_cb(struct scsc_log_collector_mx_cb *mx_cb)
{
	mutex_lock(&log_mutex);
	pr_info("Register mx_cb functions\n");
	log_status.singleton_mx_cb = mx_cb;
	mutex_unlock(&log_mutex);
	return 0;
}
EXPORT_SYMBOL(scsc_log_collector_register_mx_cb);

int scsc_log_collector_unregister_mx_cb(struct scsc_log_collector_mx_cb *mx_cb)
{
	mutex_lock(&log_mutex);
	pr_info("Unregister mx_cb functions\n");
	log_status.singleton_mx_cb = NULL;
	mutex_unlock(&log_mutex);
	return 0;
}
EXPORT_SYMBOL(scsc_log_collector_unregister_mx_cb);

int scsc_log_collector_register_client(struct scsc_log_collector_client *collect_client)
{
	struct scsc_log_client *lc;

	if (!scsc_is_chunk_supported(collect_client->type)) {
		pr_info("Type not supported: %d\n", collect_client->type);
		return -EIO;
	}

	mutex_lock(&log_mutex);
	lc = kzalloc(sizeof(*lc), GFP_KERNEL);
	if (!lc) {
		mutex_unlock(&log_mutex);
		return -ENOMEM;
	}

	lc->collect_client = collect_client;
	list_add_tail(&lc->list, &scsc_log_collector_list.list);

	/* Sort the list */
	list_sort(NULL, &scsc_log_collector_list.list, scsc_log_collector_compare);

	pr_info("Registered client: %s\n", collect_client->name);
	mutex_unlock(&log_mutex);
	return 0;
}
EXPORT_SYMBOL(scsc_log_collector_register_client);

int scsc_log_collector_unregister_client(struct scsc_log_collector_client *collect_client)
{
	struct scsc_log_client *lc, *next;
	bool match = false;

	/* block any attempt of unregistering while a collection is in progres */
	mutex_lock(&log_mutex);
	list_for_each_entry_safe(lc, next, &scsc_log_collector_list.list, list) {
		if (lc->collect_client == collect_client) {
			match = true;
			list_del(&lc->list);
			kfree(lc);
		}
	}

	if (match == false)
		pr_err("FATAL, no match for given scsc_log_collector_client\n");

	pr_info("Unregistered client: %s\n", collect_client->name);
	mutex_unlock(&log_mutex);

	return 0;
}
EXPORT_SYMBOL(scsc_log_collector_unregister_client);


unsigned char *scsc_log_collector_get_buffer(void)
{
	return log_status.buf;
}

static int __scsc_log_collector_write_to_ram(char __user *buf, size_t count, u8 align)
{
	if (!log_status.in_collection || !log_status.buf)
		return -EIO;

	if (log_status.pos + count > SCSC_LOG_COLLECT_MAX_SIZE) {
		pr_err("Write will exceed SCSC_LOG_COLLECT_MAX_SIZE. Abort write\n");
		return -ENOMEM;
	}

	log_status.pos = (log_status.pos + align - 1) & ~(align - 1);
	/* Write buf to RAM */
	memcpy(log_status.buf + log_status.pos, buf, count);

	log_status.pos += count;

	return 0;
}

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
static int __scsc_log_collector_write_to_file(char __user *buf, size_t count, u8 align)
{
	int ret = 0;

	if (!log_status.in_collection)
		return -EIO;

	if (log_status.pos + count > SCSC_LOG_COLLECT_MAX_SIZE) {
		pr_err("Write will exceed SCSC_LOG_COLLECT_MAX_SIZE. Abort write\n");
		return -ENOMEM;
	}

	log_status.pos = (log_status.pos + align - 1) & ~(align - 1);
	/* Write buf to file */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = kernel_write(log_status.fp, buf, count, &log_status.pos);
#else
	ret = vfs_write(log_status.fp, buf, count, &log_status.pos);
#endif
	if (ret < 0) {
		pr_err("write file error, err = %d\n", ret);
		return ret;
	}
	return 0;
}
#endif

int scsc_log_collector_write(char __user *buf, size_t count, u8 align)
{
#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	if (collect_to_ram)
		return __scsc_log_collector_write_to_ram(buf, count, align);
	else
		return __scsc_log_collector_write_to_file(buf, count, align);
#else
	return __scsc_log_collector_write_to_ram(buf, count, align);
#endif
}
EXPORT_SYMBOL(scsc_log_collector_write);

#define align_chunk(ppos) (((ppos) + (SCSC_LOG_CHUNK_ALIGN - 1)) & \
			  ~(SCSC_LOG_CHUNK_ALIGN - 1))

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
static int __scsc_log_collector_collect(enum scsc_log_reason reason, u16 reason_code, u8 buffer)
#else
static int __scsc_log_collector_collect(enum scsc_log_reason reason,
					u16 reason_code)
#endif
{
	struct scsc_log_client *lc, *next;
#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	mm_segment_t old_fs;
	char memdump_path[128];
#endif
	int ret = 0;
	char version_fw[SCSC_LOG_FW_VERSION_SIZE] = {0};
	char version_host[SCSC_LOG_HOST_VERSION_SIZE] = {0};
	u32 mem_pos, temp_pos, chunk_size;
	ktime_t start;
	u8 num_chunks = 0;
	u16 first_chunk_pos = SCSC_LOG_OFFSET_FIRST_CHUNK;
	struct scsc_log_sbl_header sbl_header;
	struct scsc_log_chunk_header chk_header;
	u8 j;
	bool sbl_is_valid =  false;

	mutex_lock(&log_mutex);
	pr_info("Log collection triggered %s reason_code 0x%x\n",
		scsc_get_trigger_str((int)reason), reason_code);

	start = ktime_get();

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	if (buffer == TO_FILE) {
		snprintf(memdump_path, sizeof(memdump_path), "%s/%s.sbl",
			collection_dir_buf, scsc_get_trigger_str((int)reason));

		/* change to KERNEL_DS address limit */
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		log_status.fp = filp_open(memdump_path, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0664);
		if (IS_ERR(log_status.fp)) {
			pr_err("open file error, err = %ld\n", PTR_ERR(log_status.fp));
			mutex_unlock(&log_mutex);
			return PTR_ERR(log_status.fp);
		}
	} else if (!log_status.buf) {
#else
	if (!log_status.buf) {
#endif
		pr_err("RAM buffer not created. Aborting dump\n");
		mutex_unlock(&log_mutex);
		return -ENOMEM;
	}

	log_status.in_collection = true;
	/* Position index to start of the first chunk */
	log_status.pos = SCSC_LOG_OFFSET_FIRST_CHUNK;

	/* Call client init callbacks if any */
	list_for_each_entry_safe(lc, next, &scsc_log_collector_list.list, list) {
		if (lc->collect_client && lc->collect_client->collect_init)
			lc->collect_client->collect_init(lc->collect_client);
	}
	/* Traverse all the clients from the list.. Those would start calling scsc_log_collector_write!!*/
	/* Create chunk */
	list_for_each_entry_safe(lc, next, &scsc_log_collector_list.list, list) {
		if (lc->collect_client) {
			num_chunks++;
			/* Create Chunk */
			/* Store current post */
			temp_pos = log_status.pos;
			/* Make room for chunck header */
			log_status.pos += SCSC_CHUNK_HEADER_SIZE;
			/* Execute clients callbacks */
			if (lc->collect_client->collect(lc->collect_client, 0))
				goto exit;
			/* Write chunk headers */
			/* Align log_status.pos */
			mem_pos = log_status.pos = align_chunk(log_status.pos);
			chunk_size = log_status.pos - temp_pos - SCSC_CHUNK_HEADER_SIZE;
			/* rewind pos */
			log_status.pos = temp_pos;
			/* Write chunk header */
			memcpy(chk_header.magic, "CHK", 3);
			chk_header.type = (char)lc->collect_client->type;
			chk_header.chunk_size = chunk_size;
			scsc_log_collector_write((char *)&chk_header, sizeof(struct scsc_log_chunk_header), 1);
			/* restore position for next chunk */
			log_status.pos = mem_pos;
		}
	}
	/* Callbacks to clients have finished at this point. */
	/* Write file header */
	/* Move position to start of file */
	log_status.pos = 0;
	/* Write header */
	memset(&sbl_header, 0, sizeof(sbl_header));
	memcpy(sbl_header.magic, "SCSC", 4);
	sbl_header.version_major = SCSC_LOG_HEADER_VERSION_MAJOR;
	sbl_header.version_minor = SCSC_LOG_HEADER_VERSION_MINOR;
	sbl_header.num_chunks = num_chunks;
	sbl_header.trigger = reason;
	sbl_header.reason_code = reason_code;
	sbl_header.observer = log_status.observer_present;
	sbl_header.offset_data = first_chunk_pos;

	if (log_status.singleton_mx_cb->get_fw_version)
		log_status.singleton_mx_cb->get_fw_version(log_status.singleton_mx_cb, version_fw, SCSC_LOG_FW_VERSION_SIZE);

	memcpy(sbl_header.fw_version, version_fw, SCSC_LOG_FW_VERSION_SIZE);

	if (log_status.singleton_mx_cb->get_fw_version)
		log_status.singleton_mx_cb->get_drv_version(log_status.singleton_mx_cb, version_host, SCSC_LOG_HOST_VERSION_SIZE);
	memcpy(sbl_header.host_version, version_host, SCSC_LOG_HOST_VERSION_SIZE);
	memcpy(sbl_header.fapi_version, log_status.fapi_ver, SCSC_LOG_FAPI_VERSION_SIZE);

	memset(sbl_header.supported_chunks, SCSC_LOG_CHUNK_INVALID, SCSC_SUPPORTED_CHUNKS_HEADER);
	for (j = 0; j < SCSC_NUM_CHUNKS_SUPPORTED; j++)
		sbl_header.supported_chunks[j] = chunk_supported_sbl[j];

	scsc_log_collector_write((char *)&sbl_header, sizeof(struct scsc_log_sbl_header), 1);

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	if (buffer == TO_FILE) {
		/* Sync file from filesystem to physical media */
		ret = vfs_fsync(log_status.fp, 0);
		if (ret < 0) {
			pr_err("sync file error, error = %d\n", ret);
			goto exit;
		}
	}
#endif

	sbl_is_valid = true;
exit:
#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	if (buffer == TO_FILE) {
		/* close file before return */
		if (!IS_ERR(log_status.fp))
			filp_close(log_status.fp, current->files);

		/* restore previous address limit */
		set_fs(old_fs);
	}
#endif

	log_status.in_collection = false;

	list_for_each_entry_safe(lc, next, &scsc_log_collector_list.list, list) {
		if (lc->collect_client && lc->collect_client->collect_end)
			lc->collect_client->collect_end(lc->collect_client);
	}

	pr_info("Calling sable collection\n");

#ifdef CONFIG_SCSC_WLBTD
	if (sbl_is_valid && log_status.singleton_mx_cb->get_fw_version)
		log_status.singleton_mx_cb->call_wlbtd_sable(log_status.singleton_mx_cb, (u8)reason, reason_code);
#endif
	pr_info("Log collection end. Took: %lld\n", ktime_to_ns(ktime_sub(ktime_get(), start)));

	mutex_unlock(&log_mutex);

	return ret;
}

static int scsc_log_collector_collect(enum scsc_log_reason reason, u16 reason_code)
{
	int ret = -1;

	if (sable_collection_off) {
		pr_info("Sable Log collection is currently DISABLED (sable_collection_off=Y).\n");
		pr_info("Ignoring incoming Sable Collection request with Reason=%d.\n", reason);
		return ret;
	}

#ifdef SCSC_SUPPORT_LOG_COLLECT_TO_FILE
	if (collect_to_ram)
		ret = __scsc_log_collector_collect(reason, reason_code, TO_RAM);
	else
		ret = __scsc_log_collector_collect(reason, reason_code, TO_FILE);
#else
	ret = __scsc_log_collector_collect(reason, reason_code);
#endif

	return ret;
}

void scsc_log_collector_schedule_collection(enum scsc_log_reason reason, u16 reason_code)
{

	if (log_status.collection_workq) {
		mutex_lock(&log_status.collection_serial);
		pr_info("Log collection Schedule");

		/* Serialize with previous work if the reason is a FW panic */
		if (reason == SCSC_LOG_FW_PANIC)
			flush_work(&log_status.collect_work);
		else if (atomic_read(&in_collection)) {
			pr_info("Log collection %s reason_code 0x%x rejected. Collection already scheduled\n",
				scsc_get_trigger_str((int)reason), reason_code);
			mutex_unlock(&log_status.collection_serial);
			return;
		}
		log_status.collect_reason = reason;
		log_status.reason_code = reason_code;
		if (!queue_work(log_status.collection_workq, &log_status.collect_work)) {
			pr_info("Log collection %s reason_code 0x%x queue_work error\n",
				scsc_get_trigger_str((int)reason), reason_code);
			mutex_unlock(&log_status.collection_serial);
			return;
		}
		atomic_set(&in_collection, 1);
		pr_info("Log collection Scheduled");

		/* If dumping a FW panic (i.e. collecting a moredump), we need
		 * to wait for the collection to finish before returning.
		 */
		if (reason == SCSC_LOG_FW_PANIC)
			flush_work(&log_status.collect_work);

		mutex_unlock(&log_status.collection_serial);

	} else {
		pr_err("Log Collection Workqueue NOT available...aborting scheduled collection.\n");
	}
}
EXPORT_SYMBOL(scsc_log_collector_schedule_collection);

void scsc_log_collector_write_fapi(char __user *buf, size_t len)
{
	if (len > SCSC_LOG_FAPI_VERSION_SIZE)
		len = SCSC_LOG_FAPI_VERSION_SIZE;
	memcpy(log_status.fapi_ver, buf, len);
}
EXPORT_SYMBOL(scsc_log_collector_write_fapi);

void scsc_log_collector_is_observer(bool observer)
{
	log_status.observer_present = observer;
}
EXPORT_SYMBOL(scsc_log_collector_is_observer);

MODULE_DESCRIPTION("SCSC Log collector");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
