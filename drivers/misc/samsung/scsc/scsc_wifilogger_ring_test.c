/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 *****************************************************************************/
/* Implements */
#include "scsc_wifilogger_ring_test.h"

/* Uses */
#include "scsc_wifilogger_internal.h"
#include <linux/uaccess.h>
static u32 seq;
static struct scsc_wlog_ring *the_ring;

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
#include "scsc_wifilogger_debugfs.h"

static struct scsc_wlog_debugfs_info di;
#endif /* CONFIG_SCSC_WIFILOGGER_DEBUGFS */

static int ring_test_loglevel_change_cb(struct scsc_wlog_ring *r,
					u32 new_loglevel)
{
	SCSC_TAG_DEBUG(WLOG,
		       "==>> TEST RING SETTING CUSTOM LogLevel for ring: %s -- to:%d\n",
		       r->st.name, new_loglevel);

	return 0;
}

bool ring_test_custom_init(struct scsc_wlog_ring *r)
{
	SCSC_TAG_DEBUG(WLOG, "Custom init for ring:%s\n", r->st.name);

	return true;
}

bool ring_test_custom_fini(struct scsc_wlog_ring *r)
{
	SCSC_TAG_DEBUG(WLOG, "Custom fini for ring:%s\n", r->st.name);

	return true;
}

/* Producer API */
int scsc_wifilogger_ring_test_write(char *buf, size_t blen)
{
	struct tring_hdr thdr;

	if (!the_ring)
		return 0;

	thdr.seq = seq++;
	thdr.fake = 666 & seq;

	return scsc_wlog_write_record(the_ring, buf, blen, &thdr, sizeof(thdr), WLOG_NORMAL, 0);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_test_write);

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
static ssize_t dfs_record_write(struct file *filp, const char __user *ubuf,
				size_t count, loff_t *f_pos)
{
	ssize_t				written;
	struct scsc_ring_test_object	*rto;

	if (!filp->private_data)
		return -EINVAL;
	rto = filp->private_data;

	count = count < rto->bsz ? count : rto->bsz;
	if (copy_from_user(rto->wbuf, ubuf, count))
		return -EINVAL;
	written = scsc_wifilogger_ring_test_write(rto->wbuf, count);
	if (!written && count)
		return -EAGAIN;
	wake_up_interruptible(&rto->rw_wq);
	*f_pos += written;

	return written;
}

const struct file_operations write_record_fops = {
	.owner = THIS_MODULE,
	.open = dfs_open,
	.write = dfs_record_write,
	.release = dfs_release,
};
#endif

bool scsc_wifilogger_ring_test_init(void)
{
	struct scsc_wlog_ring *r = NULL;
#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	struct scsc_ring_test_object *rto;
#endif

	r = scsc_wlog_ring_create(WLOGGER_RTEST_NAME,
				  RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				  ENTRY_TYPE_DATA, 32768,
				  WIFI_LOGGER_SCSC_TEST_RING_SUPPORTED,
				  ring_test_custom_init, ring_test_custom_fini,
				  NULL);

	if (!r) {
		SCSC_TAG_ERR(WLOG, "Failed to CREATE WiFiLogger ring: %s\n",
			     WLOGGER_RTEST_NAME);
		return false;
	}
	/* Registering custom loglevel change callback */
	scsc_wlog_register_loglevel_change_cb(r, ring_test_loglevel_change_cb);

	if (!scsc_wifilogger_register_ring(r)) {
		SCSC_TAG_ERR(WLOG, "Failed to REGISTER WiFiLogger ring: %s\n",
			     WLOGGER_RTEST_NAME);
		scsc_wlog_ring_destroy(r);
		return false;
	}
	the_ring = r;

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	/* This test object is shared between all the debugfs entries
	 * belonging to this ring.
	 */
	rto = init_ring_test_object(the_ring);
	if (rto) {
		scsc_register_common_debugfs_entries(the_ring->st.name, rto, &di);
		/* A write is specific to the ring...*/
		scsc_wlog_register_debugfs_entry(the_ring->st.name, "write_record",
						 &write_record_fops, rto, &di);
	}
#endif

	return true;
}
