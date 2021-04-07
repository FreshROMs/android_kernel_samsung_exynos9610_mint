/******************************************************************************
 *
 *   Copyright (c) 2016-2017 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/

#include <scsc/scsc_mx.h>
#include "scsc_logring_main.h"
#include "scsc_logring_debugfs.h"

static int  scsc_max_records_per_read = SCSC_DEFAULT_MAX_RECORDS_PER_READ;
module_param(scsc_max_records_per_read, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_max_records_per_read,
		   "Number of records a reader can try to get in a shot. 0 is infinite",
		   "run-time", SCSC_DEFAULT_MAX_RECORDS_PER_READ);

static int  scsc_double_buffer_sz = DEFAULT_TBUF_SZ;
module_param(scsc_double_buffer_sz, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_double_buffer_sz,
		   "Determines the size of the per-reader allocted double buffer.",
		   "run-time", DEFAULT_TBUF_SZ);

#define LOGRING_DEV_NAME	"scsc_logring"
#define DRV_NAME		LOGRING_DEV_NAME

/* Keep track of all the device numbers used. */
#define LOGRING_MAX_DEV 5

/**
 * BIG NOTE on DOUBLE BUFFERING.
 *
 * In order to extract data from the ring buffer, protected by spinlocks,
 * to user space we use a double buffer: data is so finally copied to
 * userspace from a temporary double buffer, after having copied into it
 * ALL the desired content and after all the spinlocks have been released.
 * In order to avoid use of an additional mutex to protect such temporary
 * buffer from multiple readers access we use a oneshot throwaway buffer
 * dedicated to each reader and allocated at opening time.
 * The most straightforward way to do this thing would have been to simply
 * allocate such buffer inside the read method and throw it away on exit:
 * this is what underlying printk mechanism does via a simple kmalloc.
 * BUT we decided INSTEAD to use this buffer ALSO as a sort of caching
 * area for each reader in order to cope with under-sized user read-request;
 * basically no matter what the user has asked in term of size of the read
 * request we'll ALWAYS RETRIEVE multiple of whole records from the ring,
 * one record being the minimum internal ring-read-request this way.
 * So no matter if the user ask for a few bytes, less than the next record
 * size, we'll retrieve ONE WHOLE record from the ring into the double buffer:
 * this way on the next read request we'll have already a cached copy of the
 * record and we could deal with it inside the read callback without the
 * need to access the ring anymore for such record.
 * The main reason for this is that if we had instead accessed the ring and
 * retrieved ONLY a fraction of the record, on the next request we could NOT
 * be able to provide the remaining part of the record because, being the ring
 * an overwriting buffer, it could have wrap in the meantime and we could have
 * simply lost that data: this condition would have lead us to return to
 * user partial truncated records when we hit this overwrap condition.
 * Following instead the approach of WHOLE records retrieval we can instead be
 * sure to always retrieve fully correct records, despite being vulnerable
 * anyway to loss of data (whole records) while reading if fast writers
 * overwrite our data. (since we'll never ever want to slow down and starve a
 * writer.)
 */
#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
static struct dentry *scsc_debugfs_root;
static atomic_t      scsc_debugfs_root_refcnt;
#endif
static char          *global_fmt_string = "%s";

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
static struct scsc_logring_mx_cb *mx_cb_single;

int scsc_logring_register_mx_cb(struct scsc_logring_mx_cb *mx_cb)
{
	mx_cb_single = mx_cb;
	return 0;
}
EXPORT_SYMBOL(scsc_logring_register_mx_cb);

int scsc_logring_unregister_mx_cb(struct scsc_logring_mx_cb *mx_cb)
{
	mx_cb_single = NULL;
	return 0;
}
EXPORT_SYMBOL(scsc_logring_unregister_mx_cb);
#endif
/**
 * Generic open/close calls to use with every logring debugfs file.
 * Any file in debugfs has an underlying associated ring buffer:
 * opening ANY of these with O_TRUNC leads to ring_buffer truncated
 * to zero len.
 */
static int debugfile_open(struct inode *ino, struct file *filp)
{
	struct scsc_ibox *i = NULL;

	if (!filp->private_data) {
		i = kzalloc(sizeof(*i), GFP_KERNEL);
		if (!i)
			return -EFAULT;
		i->rb = ino->i_private;
		filp->private_data = i;
	} else {
		i = filp->private_data;
	}
	/* tbuf sz is now runtime-configurable so try a few fallback methods */
	i->tbuf = kmalloc(scsc_double_buffer_sz, GFP_KERNEL);
	/* Making sure we fallback to a safe size DEFAULT_TBUF_SZ */
	if (!i->tbuf) {
		i->tbuf = vmalloc(scsc_double_buffer_sz);
		pr_err("LogRing: FAILED tbuf allocation of %d bytes...retried vmalloc()...\n",
		       scsc_double_buffer_sz);
		if (!i->tbuf) {
			scsc_double_buffer_sz = DEFAULT_TBUF_SZ;
			pr_err("LogRing: FAILED tbuf vmalloc...using DEFAULT %d bytes size.\n",
			       scsc_double_buffer_sz);
			i->tbuf = kmalloc(scsc_double_buffer_sz, GFP_KERNEL);
			if (!i->tbuf) {
				pr_err("LogRing: FAILED DEFINITELY allocation...aborting\n");
				kfree(i);
				return -ENOMEM;
			}
		} else {
			i->tbuf_vm = true;
		}
	}
	i->tsz = scsc_double_buffer_sz;
	pr_info("LogRing: Allocated per-reader tbuf of %d bytes\n",
		scsc_double_buffer_sz);
	/* Truncate when attempting to write RO files samlog and samsg */
	if (filp->f_flags & (O_WRONLY | O_RDWR) &&
	    filp->f_flags & O_TRUNC) {
		unsigned long    flags;

		raw_spin_lock_irqsave(&i->rb->lock, flags);
		scsc_ring_truncate(i->rb);
		raw_spin_unlock_irqrestore(&i->rb->lock, flags);
		pr_info("LogRing Truncated to zerolen\n");
		return -EACCES;
	}
	return 0;
}

static int debugfile_release(struct inode *ino, struct file *filp)
{
	struct scsc_ibox *i = NULL;

	if (!filp->private_data)
		return -EFAULT;
	i = filp->private_data;
	if (!i->tbuf_vm)
		kfree(i->tbuf);
	else
		vfree(i->tbuf);
	i->tbuf = NULL;

	/* Were we using a snapshot ? Free it.*/
	if (i->saved_live_rb) {
		vfree(i->rb->buf);
		kfree(i->rb);
	}
	/* Being paranoid... */
	filp->private_data = NULL;
	kfree(i);
	return 0;
}

/**
 * Initialize references for subsequent cached reads: in fact if
 * data retrieved from the ring was more than the count-bytes required by
 * the caller of this read, we can keep such data stored in tbuf and provide
 * it to this same reader on its next read-call.
 *
 * @i:		     contains references useful to this reader
 * @retrieved_bytes: how many bytes have been stored in tbuf
 * @count:	     a pointer to the count bytes required by this reader
 *		     for this call. We'll manipulate this to return an
 *		     appropriate number of bytes.
 */
static inline
size_t init_cached_read(struct scsc_ibox *i,
			  size_t retrieved_bytes, size_t *count)
{
	if (retrieved_bytes <= *count) {
		*count = retrieved_bytes;
	} else {
		i->t_off = *count;
		i->t_used = retrieved_bytes - *count;
		i->cached_reads += *count;
	}

	return 0;
}

/**
 * Here we'll serve to user space the next available chunk of
 * record directly from the tbuf double buffer without
 * accessing the ring anymore.
 *
 * @i:		     contains references useful to this reader
 * @count:	     a pointer to the count bytes required by this reader
 *		     for this call. We'll manipulate this to return an
 *		     appropriate number of bytes.
 */
static inline
size_t process_cached_read_data(struct scsc_ibox *i, size_t *count)
{
	size_t offset = 0;

	offset = i->t_off;
	if (i->t_used <= *count) {
		/* this was the last chunk cached */
		*count = i->t_used;
		i->t_off = 0;
		i->t_used = 0;
	} else {
		i->t_off += *count;
		i->t_used -= *count;
		i->cached_reads += *count;
	}

	return offset;
}

/**
 * This file operation read from the ring using common routines, starting its
 * read from head: in other words it immediately blocks waiting for some data to
 * arrive. As soon as some data arrives and head moves away, the freshly
 * available data is returned to userspace up to the required size , and this
 * call goes back to sleeping waiting for more data.
 *
 * NOTE
 * ----
 * The need to copy_to_user imposes the use of a temp buffer tbuf which is used
 * as a double buffer: being allocated to this reader on open() we do NOT need
 * any additional form of mutual exclusion.
 * Moreover we use such buffer here as an area to cache the retrieved records:
 * if the retrieved record size is bigger than the count bytes required by user
 * we'll return less data at first and then deal with the following requests
 * pumping data directly from the double buffer without accessing the ring.
 */
static ssize_t samsg_read(struct file *filp, char __user *ubuf,
			  size_t count, loff_t *f_pos)
{
	unsigned long    flags;
	loff_t           current_head = 0;
	struct scsc_ibox *i = NULL;
	size_t		 off = 0;
	size_t		 retrieved_bytes = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
        if (!filp->private_data || !access_ok(VERIFY_WRITE, ubuf, count))
#else
	if (!filp->private_data || !access_ok(ubuf, count))
#endif
		return -ENOMEM;
	if (filp->f_flags & O_NONBLOCK)
		return -EAGAIN;
	/* open() assures us that this private data is certainly non-NULL */
	i = filp->private_data;
	if (!i->t_used) {
		raw_spin_lock_irqsave(&i->rb->lock, flags);
		current_head = *f_pos ? i->f_pos : i->rb->head;
		while (current_head == i->rb->head) {
			raw_spin_unlock_irqrestore(&i->rb->lock, flags);
			if (wait_event_interruptible(i->rb->wq,
						     current_head != i->rb->head))
				return -ERESTARTSYS;
			raw_spin_lock_irqsave(&i->rb->lock, flags);
		}
		retrieved_bytes = read_next_records(i->rb,
						    scsc_max_records_per_read,
						    &current_head, i->tbuf, i->tsz);
		/* We MUST keep track of the the last known READ record
		 * in order to keep going from the same place on the next
		 * read call coming from the same userspace process...
		 * ...this could NOT necessarily be the HEAD at the end of this
		 * read if we asked for few records.
		 * So we must annotate the really last read record got back,
		 * returned in current_head, inside i->f_pos in order to have a
		 * reference for the next read call by the same reader.
		 */
		i->f_pos = current_head;
		raw_spin_unlock_irqrestore(&i->rb->lock, flags);
		/* ANYWAY we could have got back more data from the ring (ONLY
		 * multiple of whole records) than required by usersapce.
		 */
		off = init_cached_read(i, retrieved_bytes, &count);
	} else {
		/* Serve this read-request directly from cached data without
		 * accessing the ring
		 */
		off = process_cached_read_data(i, &count);
	}
	if (copy_to_user(ubuf, i->tbuf + off, count))
		return -EFAULT;
	*f_pos += count;
	return count;
}

/**
 * This seek op assumes let userspace believe that it's dealing with a regular
 * plain file, so f_pos is modified accordingly (linearly till the maximum
 * number SCSC_LOGGED_BYTES is reached); in fact it's up to
 * the read/write ops to properly 'cast' this value to a modulus value as
 * required by the underlying ring buffer. This operates only on samlog.
 */
loff_t debugfile_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t           newpos, maxpos;
	struct scsc_ibox *i = NULL;
	unsigned long    flags;

	if (!filp->private_data)
		return -EFAULT;
	i = filp->private_data;
	raw_spin_lock_irqsave(&i->rb->lock, flags);
	maxpos = SCSC_LOGGED_BYTES(i->rb) >= 1 ?
		 SCSC_LOGGED_BYTES(i->rb) - 1 : 0;
	raw_spin_unlock_irqrestore(&i->rb->lock, flags);
	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = (off <= maxpos) ? off : maxpos;
		break;
	case 1: /* SEEK_CUR */
		newpos = (filp->f_pos + off <= maxpos) ?
			 filp->f_pos + off : maxpos;
		break;
	case 2: /* SEEK_END */
		newpos = maxpos;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

static int samsg_open(struct inode *ino, struct file *filp)
{
	int ret;
#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
	struct scsc_debugfs_info *di = (struct scsc_debugfs_info *)container_of(ino->i_cdev,
		struct scsc_debugfs_info, cdev_samsg);

	ino->i_private = di->rb;
	filp->private_data = NULL;
#endif
	ret = debugfile_open(ino, filp);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	if (!ret && mx_cb_single && mx_cb_single->scsc_logring_register_observer)
		mx_cb_single->scsc_logring_register_observer(mx_cb_single, "LOGRING");
#endif
	return ret;
}

static int samsg_release(struct inode *ino, struct file *filp)
{
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	if (mx_cb_single && mx_cb_single->scsc_logring_unregister_observer)
		mx_cb_single->scsc_logring_unregister_observer(mx_cb_single, "LOGRING");
#endif

	return debugfile_release(ino, filp);
}

const struct file_operations samsg_fops = {
	.owner = THIS_MODULE,
	.open = samsg_open,
	.read = samsg_read,
	.release = samsg_release,
};

/**
 * This is the samlog open and it is used by samlog read-process to grab
 * a per-reader dedicated static snapshot of the ring, in order to be able
 * then to fetch records from a static immutable image of the ring buffer,
 * without the need to stop the ring in the meantime.
 * This way samlog dumps exactly a snapshot at-a-point-in-time of the ring
 * limiting at the same time the contention with the writers: ring is
 * 'spinlocked' ONLY durng the snapshot phase.
 * Being the snapshot buffer big as the ring we use a vmalloc to limit
 * possibility of failures (especially on non-AOSP builds).
 * If such vmalloc allocation fails we then quietly keep on using the old
 * method that reads directly from the live buffer.
 */
static int debugfile_open_snapshot(struct inode *ino, struct file *filp)
{
	int ret;

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
	struct scsc_debugfs_info *di = (struct scsc_debugfs_info *)container_of(ino->i_cdev,
		struct scsc_debugfs_info, cdev_samlog);

	ino->i_private = di->rb;
	filp->private_data = NULL;
#endif
	ret = debugfile_open(ino, filp);
	/* if regular debug_file_open has gone through, attempt snapshot */
	if (!ret) {
		/* filp && filp->private_data NON-NULL by debugfile_open */
		void			*snap_buf;
		size_t			snap_sz;
		struct scsc_ibox	*i = filp->private_data;

		/* This is read-only...no spinlocking needed */
		snap_sz = i->rb->bsz + i->rb->ssz;
		/* Allocate here to minimize lock time... */
		snap_buf = vmalloc(snap_sz);
		if (snap_buf) {
			struct scsc_ring_buffer		*snap_rb;
			char				snap_name[RNAME_SZ] = "snapshot";
			unsigned long			flags;

			snprintf(snap_name, RNAME_SZ, "%s_snap", i->rb->name);
			/* lock while snapshot is taken */
			raw_spin_lock_irqsave(&i->rb->lock, flags);
			snap_rb = scsc_ring_get_snapshot(i->rb, snap_buf, snap_sz, snap_name);
			raw_spin_unlock_irqrestore(&i->rb->lock, flags);
			if (snap_rb) {
				/* save real ring and swap into the snap_shot */
				i->saved_live_rb = i->rb;
				i->rb = snap_rb;
			} else {
				vfree(snap_buf);
				snap_buf = NULL;
			}
		}

		/* Warns when not possible to use a snapshot */
		if (!snap_buf)
			pr_warn("LogRing: no snapshot available, samlog dump from live ring.\n");
	}

	return ret;
}

/**
 * samlog_read - Reads from the ring buffer the required number of bytes
 * starting from the start of the ring. It is usually used to dump the
 * whole ring buffer taking a snapshot at-a-point-in-time.
 *
 * If it had been possible at opening time to take a static snapshot of
 * the ring, this routine will fetch records from such a snapshot without
 * the need to lock the ring; if instead no snapshot was taken it reverts
 * to the usual locked-access pattern.
 *
 * This function as a usual .read fops returns the number of bytes
 * effectively read, and this could:
 *  - equal the required count bytes
 *  - be less than the required bytes if: less data WAS available
 *    (since we only GOT whole records at time from the ring)
 *    Returning less bytes usually triggers the userapp to reissue the syscall
 *    to complete the read up to the originaly required number of bytes.
 *  - be ZERO if NO more data available..this causes the reading userspace
 *    process to stop reading usually.
 */
static ssize_t samlog_read(struct file *filp, char __user *ubuf,
			   size_t count, loff_t *f_pos)
{
	struct scsc_ibox *i = NULL;
	size_t		 off = 0, retrieved_bytes = 0;

	if (!filp->private_data)
		return -EFAULT;
	i = filp->private_data;
	if (!i->t_used) {
		unsigned long    flags;

		/* Lock ONLY if NOT using a snapshot */
		if (!i->saved_live_rb)
			raw_spin_lock_irqsave(&i->rb->lock, flags);
		/* On first read from userspace f_pos will be ZERO and in this
		 * case we'll want to trigger a read from the very beginning of
		 * ring (tail) and set i->f_pos accordingly.
		 * Internal RING API returns in i->f_pos the next record to
		 * read: when reading process has wrapped over you'll get back
		 * an f_pos ZERO as next read.
		 */
		if (*f_pos == 0)
			i->f_pos = i->rb->tail;
		retrieved_bytes = read_next_records(i->rb,
						    scsc_max_records_per_read,
						    &i->f_pos, i->tbuf, i->tsz);
		if (!i->saved_live_rb)
			raw_spin_unlock_irqrestore(&i->rb->lock, flags);
		/* ANYWAY we could have got back more data from the ring (ONLY
		 * multiple of whole records) than required by userspace.
		 */
		off = init_cached_read(i, retrieved_bytes, &count);
	} else {
		/* Serve this read-request directly from cached data without
		 * accessing the ring
		 */
		off = process_cached_read_data(i, &count);
	}
	if (copy_to_user(ubuf, i->tbuf + off, count))
		return -EFAULT;
	*f_pos += count;
	return count;
}

const struct file_operations samlog_fops = {
	.owner = THIS_MODULE,
	.open = debugfile_open_snapshot,
	.read = samlog_read,
	.llseek = debugfile_llseek,
	.release = debugfile_release,
};

static int statfile_open(struct inode *ino, struct file *filp)
{
#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
	if (!filp->private_data)
		filp->private_data = ino->i_private;
	if (!filp->private_data)
		return -EFAULT;
#else
	struct scsc_debugfs_info *di = (struct scsc_debugfs_info *)container_of(ino->i_cdev,
		struct scsc_debugfs_info, cdev_stat);

	ino->i_private = di->rb;
	filp->private_data = ino->i_private;
#endif
	return 0;
}

static int statfile_release(struct inode *ino, struct file *filp)
{
#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
	if (!filp->private_data)
		filp->private_data = ino->i_private;
	if (!filp->private_data)
		return -EFAULT;
#else
	filp->private_data = NULL;
#endif
	return 0;
}


/* A simple read to dump some stats about the ring buffer. */
static ssize_t statfile_read(struct file *filp, char __user *ubuf,
			     size_t count, loff_t *f_pos)
{
	unsigned long           flags;
	size_t                  bsz = 0;
	loff_t                  head = 0, tail = 0, used = 0, max_chunk = 0, logged = 0,
				last = 0;
	int                     slen = 0, records = 0, wraps = 0, oos = 0;
	u64                     written = 0;
	char                    statstr[STATSTR_SZ] = {};
	struct scsc_ring_buffer *rb = filp->private_data;

	raw_spin_lock_irqsave(&rb->lock, flags);
	bsz = rb->bsz;
	head = rb->head;
	tail = rb->tail;
	last = rb->last;
	written = rb->written;
	records = rb->records;
	wraps = rb->wraps;
	oos = rb->oos;
	used = SCSC_USED_BYTES(rb);
	max_chunk = SCSC_RING_FREE_BYTES(rb);
	logged = SCSC_LOGGED_BYTES(rb);
	raw_spin_unlock_irqrestore(&rb->lock, flags);

	slen = snprintf(statstr, STATSTR_SZ,
			"sz:%zd  used:%lld  free:%lld  logged:%lld  records:%d\nhead:%lld  tail:%lld  last:%lld  written:%lld  wraps:%d  oos:%d\n",
			bsz, used, max_chunk, logged, records,
			head, tail, last, written, wraps, oos);
	if (slen >= 0 && *f_pos < slen) {
		count = (count <= slen - *f_pos) ? count : (slen - *f_pos);
		if (copy_to_user(ubuf, statstr + *f_pos, count))
			return -EFAULT;
		*f_pos += count;
	} else
		count = 0;
	return count;
}

const struct file_operations stat_fops = {
	.owner = THIS_MODULE,
	.open = statfile_open,
	.read = statfile_read,
	.release = statfile_release,
};

/**
 * This implement samwrite interface to INJECT log lines into the ring from
 * user space. The support, thought as an aid for testing mainly, is
 * minimal, so the interface allows only for simple %s format string injection.
 */
static int samwritefile_open(struct inode *ino, struct file *filp)
{
	if (!filp->private_data) {
		struct write_config *wc =
			kzalloc(sizeof(struct write_config), GFP_KERNEL);
		if (wc) {
			wc->fmt = global_fmt_string;
			wc->buf_sz = SAMWRITE_BUFSZ;
		}
		filp->private_data = wc;
	}
	if (!filp->private_data)
		return -EFAULT;
	return 0;
}


static int samwritefile_release(struct inode *ino, struct file *filp)
{
	kfree(filp->private_data);
	filp->private_data = NULL;
	return 0;
}

/**
 * User injected string content is pushed to the ring as simple %s fmt string
 * content using the TEST_ME tag. Default debuglevel (6 - INFO)will be used.
 */
static ssize_t samwritefile_write(struct file *filp, const char __user *ubuf,
				  size_t count, loff_t *f_pos)
{
	ssize_t             written_bytes = 0;
	struct write_config *wc = filp->private_data;

	if (wc) {
		/* wc->buf is null terminated as it's kzalloc'ed */
		count = count < wc->buf_sz ? count : wc->buf_sz - 1;
		if (copy_from_user(wc->buf, ubuf, count))
			return -EINVAL;
		written_bytes = scsc_printk_tag(NO_ECHO_PRK, TEST_ME,
						wc->fmt, wc->buf);
		/* Handle the case where the message is filtered out by
		 * droplevel filters...zero is returned BUT we do NOT want
		 * the applications to keep trying...it's NOT a transient
		 * error...at least till someone changes droplevels.
		 */
		if (!written_bytes) {
			pr_info("samwrite wrote 0 bytes...droplevels filtering ?\n");
			return -EPERM;
		}
		/* Returned written bytes should be normalized since
		 * lower level functions returns the number of bytes
		 * effectively written including the prepended header
		 * file... IF, when required to write n, we return n+X,
		 * some applications could behave badly trying to access
		 * file at *fpos=n+X next time, ending up in a regular
		 * EFAULT error anyway.
		 */
		if (written_bytes > count)
			written_bytes = count;
		*f_pos += written_bytes;
	}

	return written_bytes;
}

const struct file_operations samwrite_fops = {
	.owner = THIS_MODULE,
	.open = samwritefile_open,
	.write = samwritefile_write,
	.release = samwritefile_release,
};

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
static int samlog_devfs_init(struct scsc_debugfs_info *di)
{
	int ret;

	pr_info("%s init\n", __func__);
	/* allocate device number */
	ret = alloc_chrdev_region(&di->devt, 0, LOGRING_MAX_DEV, DRV_NAME);
	if (ret) {
		pr_err("%s. Failed to register character device\n", __func__);
		return ret;
	}

	di->logring_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(di->logring_class)) {
		unregister_chrdev_region(di->devt, LOGRING_MAX_DEV);
		pr_err("%s. Failed to create character class\n", __func__);
		return PTR_ERR(di->logring_class);
	}

	pr_info("%s allocated device number major %i minor %i\n",
		__func__, MAJOR(di->devt), MINOR(di->devt));

	return 0;
}

static void samlog_remove_chrdev(struct scsc_debugfs_info *di)
{
	pr_info("%s\n", __func__);

	/* Destroy device. */
	device_destroy(di->logring_class, di->devt);

	/* Unregister the device class.*/
	class_unregister(di->logring_class);

	/* Destroy created class. */
	class_destroy(di->logring_class);

	unregister_chrdev_region(di->devt, LOGRING_MAX_DEV);
}

static int samlog_create_char_dev(struct scsc_debugfs_info *di, struct cdev *cdev,
				  const struct file_operations *fops, const char *name, int minor)
{
	dev_t devn;
	char dev_name[20];
	int ret;
	struct device *device;

	pr_info("%s\n", __func__);

	devn = MKDEV(MAJOR(di->devt), MINOR(minor));

	cdev_init(cdev, fops);
	ret = cdev_add(cdev, devn, 1);
	if (ret < 0) {
		pr_err(	"couldn't create SAMSG char device\n");
		return ret;
	}

	snprintf(dev_name, sizeof(dev_name), name);
	/* create driver file */
	device = device_create(di->logring_class, NULL, devn,
				     NULL, dev_name);
	if (IS_ERR(device)) {
		pr_err(	"couldn't create samlog driver file for %s\n", name);
		ret = PTR_ERR(device);
		return ret;
	}
	pr_err("create dev %d\n", cdev->dev);
	return 0;
}

static int samlog_remove_char_dev(struct scsc_debugfs_info *di, struct cdev *cdev)
{
	pr_info("%s\n", __func__);

	/* Destroy device. */
	pr_err("destroy dev %d\n", cdev->dev);
	device_destroy(di->logring_class, cdev->dev);
	/* remove char device*/
	cdev_del(cdev);

	return 0;
}
#endif
/**
 * Initializes debugfs support build the proper debugfs file dentries:
 * - entries in debugfs are created under /sys/kernel/debugfs/scsc/@name/
 * - using the provided rb ring buffer as underlying ring buffer, storing it
 *   into inode ptr for future retrieval (via open)
 * - registers the proper fops
 */
void __init *samlog_debugfs_init(const char *root_name, void *rb)
{
	struct scsc_debugfs_info *di = NULL;

	if (!rb || !root_name)
		return NULL;
	di = kmalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return NULL;
#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
	if (!scsc_debugfs_root) {
		/* I could have multiple rings debugfs entry all rooted at
		 * the same /sys/kernel/debug/scsc/...so such entry could
		 * already exist.
		 */
		scsc_debugfs_root = debugfs_create_dir(SCSC_DEBUGFS_ROOT, NULL);
		if (!scsc_debugfs_root)
			goto no_root;
	}
	di->rootdir = scsc_debugfs_root;
	di->bufdir = debugfs_create_dir(root_name, di->rootdir);
	if (!di->bufdir)
		goto no_buf;
	atomic_inc(&scsc_debugfs_root_refcnt);
	/* Saving ring ref @rb to Inode */
	di->samsgfile = debugfs_create_file(SCSC_SAMSG_FNAME, 0444,
					    di->bufdir, rb, &samsg_fops);
	if (!di->samsgfile)
		goto no_samsg;
	/* Saving ring ref @rb to Inode */
	di->samlogfile = debugfs_create_file(SCSC_SAMLOG_FNAME, 0444,
					     di->bufdir, rb, &samlog_fops);
	if (!di->samlogfile)
		goto no_samlog;
	di->statfile = debugfs_create_file(SCSC_STAT_FNAME, 0444,
					   di->bufdir, rb, &stat_fops);
	if (!di->statfile)
		goto no_statfile;

	di->samwritefile = debugfs_create_file(SCSC_SAMWRITE_FNAME, 0220,
					       di->bufdir, NULL,
					       &samwrite_fops);
	if (!di->samwritefile)
		goto no_samwrite;

	pr_info("Samlog Debugfs Initialized\n");
#else
	di->rb = rb;

	if (samlog_devfs_init(di))
		goto no_chrdev;
	if (samlog_create_char_dev(di, &di->cdev_samsg, &samsg_fops, SCSC_SAMSG_FNAME, 0))
		goto no_samsg;
	if (samlog_create_char_dev(di, &di->cdev_samlog, &samlog_fops, SCSC_SAMLOG_FNAME, 1))
		goto no_samlog;
	if (samlog_create_char_dev(di, &di->cdev_stat, &stat_fops, SCSC_STAT_FNAME, 2))
		goto no_statfile;
	if (samlog_create_char_dev(di, &di->cdev_samwrite, &samwrite_fops, SCSC_SAMWRITE_FNAME, 3))
		goto no_samwrite;

	pr_info("Samlog Devfs Initialized\n");
#endif
	return di;

#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
no_samwrite:
	debugfs_remove(di->statfile);
no_statfile:
	debugfs_remove(di->samlogfile);
no_samlog:
	debugfs_remove(di->samsgfile);
no_samsg:
	debugfs_remove(di->bufdir);
	atomic_dec(&scsc_debugfs_root_refcnt);
no_buf:
	if (!atomic_read(&scsc_debugfs_root_refcnt)) {
		debugfs_remove(scsc_debugfs_root);
		scsc_debugfs_root = NULL;
	}
no_root:
#else
no_samwrite:
	samlog_remove_char_dev(di, &di->cdev_stat);
no_statfile:
	samlog_remove_char_dev(di, &di->cdev_samlog);
no_samlog:
	samlog_remove_char_dev(di, &di->cdev_samsg);
no_samsg:
	/* remove alloc_char dev*/
	samlog_remove_chrdev(di);
no_chrdev:
#endif
	kfree(di);
	return NULL;
}

void __exit samlog_debugfs_exit(void **priv)
{
	struct scsc_debugfs_info **di = NULL;

	if (!priv)
		return;
	di = (struct scsc_debugfs_info **)priv;
	if (di && *di) {
#ifdef CONFIG_SCSC_LOGRING_DEBUGFS
		debugfs_remove_recursive(scsc_debugfs_root);
		atomic_dec(&scsc_debugfs_root_refcnt);
		if (!atomic_read(&scsc_debugfs_root_refcnt)) {
			debugfs_remove(scsc_debugfs_root);
			scsc_debugfs_root = NULL;
		}
#else
		samlog_remove_char_dev(*di, &(*di)->cdev_samwrite);
		samlog_remove_char_dev(*di, &(*di)->cdev_stat);
		samlog_remove_char_dev(*di, &(*di)->cdev_samlog);
		samlog_remove_char_dev(*di, &(*di)->cdev_samsg);
		/* remove alloc_char dev*/
		samlog_remove_chrdev(*di);
#endif
		kfree(*di);
		*di = NULL;
	}
	pr_info("Debugfs Cleaned Up\n");
}
