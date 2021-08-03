/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#include <linux/uaccess.h>
#include "scsc_wifilogger_debugfs.h"
#include "scsc_wifilogger_ring_pktfate.h"

static struct dentry *scsc_wlog_debugfs_global_root;

int dfs_open(struct inode *ino, struct file *filp)
{
	if (!filp->private_data) {
		filp->private_data = ino->i_private;
		if (!filp->private_data)
			return -EFAULT;
	}
	return 0;
}

int dfs_release(struct inode *ino, struct file *filp)
{
	return 0;
}

#define	SCSC_RING_TEST_STAT_SZ		512

static ssize_t dfs_stats_read(struct file *filp, char __user *ubuf,
			      size_t count, loff_t *f_pos)
{
	int				slen = 0;
	char				statstr[SCSC_RING_TEST_STAT_SZ] = {};
	struct scsc_ring_test_object	*rto = filp->private_data;

	slen = snprintf(statstr, SCSC_RING_TEST_STAT_SZ,
			"[%s]:: len:%d  state:%d  verbose:%d  min_data_size:%d  max_interval_sec:%d  drop_on_full:%d\n"
			"\tunread:%d  written:%d  read:%d  written_records:%d  dropped:%d  buf:%p\n",
			rto->r->st.name, rto->r->st.rb_byte_size, rto->r->state,
			rto->r->st.verbose_level, rto->r->min_data_size,
			rto->r->max_interval_sec, rto->r->drop_on_full,
			rto->r->st.written_bytes - rto->r->st.read_bytes,
			rto->r->st.written_bytes, rto->r->st.read_bytes,
			rto->r->st.written_records, rto->r->dropped, rto->r->buf);
	if (slen >= 0 && *f_pos < slen) {
		count = (count <= slen - *f_pos) ? count : (slen - *f_pos);
		if (copy_to_user(ubuf, statstr + *f_pos, count))
			return -EFAULT;
		*f_pos += count;
	} else {
		count = 0;
	}
	return count;
}

const struct file_operations stats_fops = {
	.owner = THIS_MODULE,
	.open = dfs_open,
	.read = dfs_stats_read,
	.release = dfs_release,
};

#ifdef CONFIG_SCSC_WIFILOGGER_TEST
static int dfs_read_record_open(struct inode *ino, struct file *filp)
{
	int ret;
	struct scsc_ring_test_object *rto;

	ret = dfs_open(ino, filp);
	if (ret)
		return ret;

	rto = filp->private_data;
	if (!mutex_trylock(&rto->readers_lock)) {
		SCSC_TAG_ERR(WLOG,
			     "Failed to get readers mutex...ONLY one reader allowed !!!\n");
		dfs_release(ino, filp);
		return -EPERM;
	}
	/* NO Log handler here...only raise verbosity */
	scsc_wifi_start_logging(1, 0x00, 0, 8192, rto->r->st.name);

	return ret;
}

static int dfs_read_record_release(struct inode *ino, struct file *filp)
{
	struct scsc_ring_test_object *rto = filp->private_data;

	/* Stop logging ... verbosity 0 */
	scsc_wifi_start_logging(0, 0x00, 0, 8192, rto->r->st.name);
	mutex_unlock(&rto->readers_lock);
	SCSC_TAG_DEBUG(WLOG, "Readers mutex released.\n");

	return dfs_release(ino, filp);
}

static ssize_t dfs_read_record(struct file *filp, char __user *ubuf,
			       size_t count, loff_t *f_pos)
{
	int ret;
	struct scsc_ring_test_object *rto;

	if (!filp->private_data)
		return -EINVAL;
	rto = filp->private_data;

	while (scsc_wlog_is_ring_empty(rto->r)) {
		if (wait_event_interruptible(rto->rw_wq,
		    !scsc_wlog_is_ring_empty(rto->r)))
			return -ERESTARTSYS;
	}
	ret = scsc_wlog_read_records(rto->r, rto->rbuf, rto->bsz, false);
	count = ret <= count ? ret : count;
	if (copy_to_user(ubuf, rto->rbuf, count))
		return -EFAULT;
	*f_pos += count;

	return count;
}

const struct file_operations read_record_fops = {
	.owner = THIS_MODULE,
	.open = dfs_read_record_open,
	.read = dfs_read_record,
	.release = dfs_read_record_release,
};

static void on_ring_test_data_cb(char *ring_name, char *buf, int bsz,
				 struct scsc_wifi_ring_buffer_status *status,
				 void *ctx)
{
	struct scsc_ring_test_object	*a_rto, *head_rto = ctx;

	a_rto = kzalloc(sizeof(*a_rto), GFP_KERNEL);
	if (!a_rto)
		return;
	a_rto->rbuf = kmalloc(bsz, GFP_KERNEL);
	if (!a_rto->rbuf) {
		kfree(a_rto);
		return;
	}
	/* copy and pass over into a list to simulate a channel */
	memcpy(a_rto->rbuf, buf, bsz);
	a_rto->bsz = bsz;
	list_add_tail(&a_rto->elem, &head_rto->elem);
	wake_up_interruptible(&head_rto->drain_wq);
}

static int dfs_read_open(struct inode *ino, struct file *filp)
{
	int ret;
	struct scsc_ring_test_object *rto;

	ret = dfs_open(ino, filp);
	if (ret)
		return ret;

	/* Filp private data NOW contains rto */
	rto = filp->private_data;
	if (!mutex_trylock(&rto->readers_lock)) {
		SCSC_TAG_ERR(WLOG,
			     "Failed to get readers mutex...ONLY one reader allowed !!!\n");
		dfs_release(ino, filp);
		return -EPERM;
	}

	SCSC_TAG_DEBUG(WLOG,
		       "DebugFS Read opened...setting handlers...starting logging on: %s\n",
		       rto->r->st.name);
	scsc_wifi_set_log_handler(on_ring_test_data_cb, rto);
	scsc_wifi_start_logging(1, 0x00, 2, 8192, rto->r->st.name);

	return ret;
}

static ssize_t dfs_read(struct file *filp, char __user *ubuf,
			size_t count, loff_t *f_pos)
{
	size_t ret_count = 0;
	struct list_head *pos = NULL, *tlist = NULL;
	struct scsc_ring_test_object *head_rto, *a_rto;

	if (!filp->private_data)
		return -EINVAL;
	head_rto = filp->private_data;

	while (list_empty(&head_rto->elem)) {
		if (wait_event_interruptible(head_rto->drain_wq, !list_empty(&head_rto->elem)))
			return -ERESTARTSYS;
	}

	list_for_each_safe(pos, tlist, &head_rto->elem) {
		a_rto = list_entry(pos, struct scsc_ring_test_object, elem);
		SCSC_TAG_DEBUG(WLOG, "Processing list item: %p\n", a_rto);
		if (!a_rto || ret_count + a_rto->bsz >= count) {
			SCSC_TAG_DEBUG(WLOG, "BREAK OUT on:%p\n", a_rto);
			list_del(pos);
			if (a_rto) {
				kfree(a_rto->rbuf);
				kfree(a_rto);
			}
			break;
		}
		if (copy_to_user(ubuf + ret_count, a_rto->rbuf, a_rto->bsz))
			return -EFAULT;
		ret_count += a_rto->bsz;
		list_del(pos);
		kfree(a_rto->rbuf);
		kfree(a_rto);
	}
	*f_pos += ret_count;

	return count;
}

static int dfs_read_release(struct inode *ino, struct file *filp)
{
	int ret;
	struct scsc_ring_test_object *head_rto, *a_rto;

	head_rto = filp->private_data;
	if (head_rto) {
		struct list_head *pos = NULL, *tlist = NULL;

		list_for_each_safe(pos, tlist, &head_rto->elem) {
			a_rto = list_entry(pos, struct scsc_ring_test_object, elem);
			list_del(pos);
			if (a_rto) {
				kfree(a_rto->rbuf);
				kfree(a_rto);
				a_rto = NULL;
			}
		}
	}

	ret = dfs_read_record_release(ino, filp);
	scsc_wifi_reset_log_handler();

	return ret;
}

const struct file_operations read_fops = {
	.owner = THIS_MODULE,
	.open = dfs_read_open,
	.read = dfs_read,
	.release = dfs_read_release,
};

#define BUF_LEN		16
static ssize_t dfs_verbose_read(struct file *filp, char __user *ubuf,
				size_t count, loff_t *f_pos)
{
	char				buf[BUF_LEN] = {};
	struct scsc_ring_test_object	*rto;

	if (!filp->private_data)
		return -EINVAL;
	rto = filp->private_data;

	count = snprintf(buf, BUF_LEN, "%d\n", rto->r->st.verbose_level);
	if (copy_to_user(ubuf, buf, count))
		return -EFAULT;
	/* emit EOF after having spitted the value once */
	count = !*f_pos ? count : 0;
	*f_pos += count;

	return count;
}

static ssize_t dfs_verbose_write(struct file *filp, const char __user *ubuf,
				size_t count, loff_t *f_pos)
{
	char				buf[BUF_LEN] = {};
	size_t				written;
	unsigned long			verb;
	struct scsc_ring_test_object	*rto;

	if (!filp->private_data)
		return -EINVAL;
	rto = filp->private_data;

	count = count < BUF_LEN ? count : BUF_LEN - 1;
	if (copy_from_user(buf, ubuf, count))
		return -EINVAL;
	if (!kstrtoul((const char *)buf, 10, &verb))
		scsc_wlog_ring_change_verbosity(rto->r, verb);
	written = strlen(buf);
	*f_pos += written;

	SCSC_TAG_DEBUG(WLOG, "Changed verbosity on ring %s to %d\n",
		       rto->r->st.name, rto->r->st.verbose_level);

	switch (rto->r->st.verbose_level) {
	case 10:
		SCSC_TAG_DEBUG(WLOG, "Ring '%s'  --  RING FULL DRAIN !\n",
			       rto->r->st.name);
		scsc_wifi_get_ring_data(rto->r->st.name);
		break;
	case 20:
		scsc_wlog_flush_ring(rto->r);
		SCSC_TAG_DEBUG(WLOG, "Ring '%s'  --  RING FLUSH !\n",
			       rto->r->st.name);
		break;
	case 30:
		scsc_wlog_ring_set_drop_on_full(rto->r);
		SCSC_TAG_DEBUG(WLOG, "Ring '%s'  --  RING SET DROP ON FULL !\n",
			       rto->r->st.name);
		break;
	case 40:
		if (rto->r->features_mask & WIFI_LOGGER_PACKET_FATE_SUPPORTED) {
			scsc_wifilogger_ring_pktfate_start_monitoring();
			SCSC_TAG_DEBUG(WLOG, "PKTFATE MONITORING STARTED !\n");
		}
		break;
	case 50:
		{
		int i, num_rings = 10;
		struct scsc_wifi_ring_buffer_status status[10];

		scsc_wifilogger_get_rings_status(&num_rings, status);
		SCSC_TAG_INFO(WLOG, "Returned rings: %d\n", num_rings);
		for (i = 0; i < num_rings; i++)
			SCSC_TAG_INFO(WLOG, "Retrieved ring: %s\n", status[i].name);
		}
		break;
	default:
		break;
	}

	return written;
}

const struct file_operations verbosity_fops = {
	.owner = THIS_MODULE,
	.open = dfs_open,
	.read = dfs_verbose_read,
	.write = dfs_verbose_write,
	.release = dfs_release,
};

#endif /* CONFIG_SCSC_WIFILOGGER_TEST */

/*** Public ***/

struct scsc_ring_test_object *init_ring_test_object(struct scsc_wlog_ring *r)
{
	struct scsc_ring_test_object	*rto;

	rto = kzalloc(sizeof(*rto), GFP_KERNEL);
	if (!rto)
		return rto;

#ifdef CONFIG_SCSC_WIFILOGGER_TEST
	rto->bsz = MAX_RECORD_SZ;
	rto->rbuf = kzalloc(rto->bsz, GFP_KERNEL);
	if (!rto->rbuf) {
		kfree(rto);
		return NULL;
	}
	rto->wbuf = kzalloc(rto->bsz, GFP_KERNEL);
	if (!rto->wbuf) {
		kfree(rto->rbuf);
		kfree(rto);
		return NULL;
	}
	/* used by on_ring_data_cb simulation test */
	INIT_LIST_HEAD(&rto->elem);
	init_waitqueue_head(&rto->drain_wq);
	init_waitqueue_head(&rto->rw_wq);
	mutex_init(&rto->readers_lock);
#endif
	rto->r = r;

	return rto;
}

void *scsc_wlog_register_debugfs_entry(const char *ring_name,
				       const char *fname,
				       const struct file_operations *fops,
				       void *rto,
				       struct scsc_wlog_debugfs_info *di)
{
	if (!ring_name || !fname || !fops || !rto || !di)
		return NULL;

	/* create root debugfs dirs if they not already exists */
	if (!di->rootdir) {
		if (scsc_wlog_debugfs_global_root) {
			di->rootdir = scsc_wlog_debugfs_global_root;
		} else {
			di->rootdir = debugfs_create_dir(SCSC_DEBUGFS_ROOT_DIRNAME, NULL);
			scsc_wlog_debugfs_global_root = di->rootdir;
		}
		if (!di->rootdir)
			goto no_rootdir;
	}

	if (!di->ringdir) {
		di->ringdir = debugfs_create_dir(ring_name, di->rootdir);
		if (!di->ringdir)
			goto no_ringdir;
	}

	/* Saving ring ref @r to Inode */
	debugfs_create_file(fname, 0664, di->ringdir, rto, fops);

	return di;

no_ringdir:
no_rootdir:
	SCSC_TAG_ERR(WLOG, "Failed WiFiLogger Debugfs basic initialization\n");
	return NULL;
}

void scsc_wifilogger_debugfs_remove_top_dir_recursive(void)
{
	if (!scsc_wlog_debugfs_global_root)
		return;

	debugfs_remove_recursive(scsc_wlog_debugfs_global_root);

	SCSC_TAG_INFO(WLOG, "Wi-Fi Logger Debugfs Cleaned Up\n");
}

void scsc_register_common_debugfs_entries(char *ring_name, void *rto,
					   struct scsc_wlog_debugfs_info *di)
{
	scsc_wlog_register_debugfs_entry(ring_name, "stats",
					 &stats_fops, rto, di);
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
	scsc_wlog_register_debugfs_entry(ring_name, "verbose_level",
					 &verbosity_fops, rto, di);
	scsc_wlog_register_debugfs_entry(ring_name, "read_record",
					 &read_record_fops, rto, di);
	scsc_wlog_register_debugfs_entry(ring_name, "read",
					 &read_fops, rto, di);
#endif
}
