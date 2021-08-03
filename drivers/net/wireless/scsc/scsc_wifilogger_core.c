/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
/* Uses */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <scsc/scsc_logring.h>
#include <linux/timer.h>
#include <linux/uaccess.h>

/* Implements */
#include "scsc_wifilogger_core.h"

static atomic_t next_ring_id;

static void wlog_drain_worker(struct work_struct *work)
{
	struct scsc_wlog_ring	*r;

	r = container_of(work, struct scsc_wlog_ring, drain_work);

	if (r && r->ops.drain_ring)
		r->ops.drain_ring(r, r->flushing ? r->st.rb_byte_size : DEFAULT_DRAIN_CHUNK_SZ(r));
}

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
static void drain_timer_callback(struct timer_list *t)
#else
static void drain_timer_callback(unsigned long data)
#endif
{
#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
	struct scsc_wlog_ring *r = from_timer(r, t, drain_timer);
#else
	struct scsc_wlog_ring *r = (struct scsc_wlog_ring *)data;
#endif
	SCSC_TAG_DBG4(WLOG, "TIMER DRAIN : %p\n", r);
	/* we should kick the workqueue here...no sleep */
	queue_work(r->drain_workq, &r->drain_work);

	if (r->st.verbose_level && r->max_interval_sec) {
		mod_timer(&r->drain_timer,
			  jiffies + msecs_to_jiffies(r->max_interval_sec * 1000));
		SCSC_TAG_DBG4(WLOG, "TIMER RELOADED !!!\n");
	}
}

static int wlog_ring_init(struct scsc_wlog_ring *r)
{
	/* Allocate buffer and spare area */
	r->buf = kzalloc(r->st.rb_byte_size + MAX_RECORD_SZ, GFP_KERNEL);
	if (!r->buf)
		return -ENOMEM;
	r->drain_sz = DRAIN_BUF_SZ;
	r->drain_buf = kzalloc(r->drain_sz, GFP_KERNEL);
	if (!r->drain_buf) {
		kfree(r->buf);
		return -ENOMEM;
	}
	mutex_init(&r->drain_lock);

	r->drain_workq = create_workqueue("wifilogger");
	INIT_WORK(&r->drain_work, wlog_drain_worker);
#if KERNEL_VERSION(4,19,0) <= LINUX_VERSION_CODE
       timer_setup(&r->drain_timer, drain_timer_callback, 0);
#else
       setup_timer(&r->drain_timer, drain_timer_callback, (unsigned long)r);
#endif
	r->st.ring_id = atomic_read(&next_ring_id);
	atomic_inc(&next_ring_id);

	SCSC_TAG_DBG3(WLOG, "Workers initialized for ring[%p]: %s\n",
		      r, r->st.name);

	return 0;
}

static void wlog_ring_finalize(struct scsc_wlog_ring *r)
{
	if (!r)
		return;

	cancel_work_sync(&r->drain_work);
	del_timer_sync(&r->drain_timer);
	destroy_workqueue(r->drain_workq);

	r->initialized = false;
	kfree(r->drain_buf);
	kfree(r->buf);
	r->buf = NULL;
}

static wifi_error wlog_get_ring_status(struct scsc_wlog_ring *r,
				       struct scsc_wifi_ring_buffer_status *status)
{
	if (!r || !status)
		return WIFI_ERROR_INVALID_ARGS;
	//TODO locking SRCU ?
	*status = r->st;

	return WIFI_SUCCESS;
}

static int wlog_read_records(struct scsc_wlog_ring *r, u8 *buf,
			     size_t blen, u32 *records,
			     struct scsc_wifi_ring_buffer_status *status,
			     bool is_user)
{
	u16 read_bytes = 0, rec_sz = 0;
	u32 got_records = 0, req_records = -1;
	int ret_ignore = 0;

	if (scsc_wlog_ring_is_flushing(r))
		return 0;

	/**
	 * req_records has been loaded with a max u32 value by default
	 *  on purpose...if a max number of records is provided in records
	 *  update req_records accordingly
	 */
	if (records)
		req_records = *records;
	/**
	 * We have ONLY ONE READER at any time that consumes data, impersonated
	 * here by the drain_ring drainer callback, whose read-ops are ensured
	 * atomic by the drain_lock mutex: this will guard against races
	 * between the periodic-drain worker and the threshold-drain procedure
	 * triggered by the write itself.
	 *
	 * But we want also to guard against any direct read_record invokation
	 * like in test rings via debugfs so we add a read spinlock: this last
	 * won't lead to any contention here anyway most of the time in a
	 * real scenario so the same reason we don't need either any irqsave
	 * spinlock version....so latency also is not impacted.
	 */
	raw_spin_lock(&r->rlock);
	while (!scsc_wlog_is_ring_empty(r) && got_records < req_records) {
		rec_sz = REC_SZ(r, RPOS(r));
		if (read_bytes + rec_sz > blen)
			break;
		/**
		 * Rollover is transparent on read...last written material in
		 * spare is still there...
		 */
		if (is_user)
			/* This case invoked from netlink api */
			ret_ignore = copy_to_user(buf + read_bytes, REC_START(r, RPOS(r)), rec_sz);
		else
			/* This case will be invoked from debugfs */
			memcpy(buf + read_bytes, REC_START(r, RPOS(r)), rec_sz);

		read_bytes += rec_sz;
		r->st.read_bytes += rec_sz;
		got_records++;
	}
	if (status)
		*status = r->st;
	raw_spin_unlock(&r->rlock);

	if (records)
		*records = got_records;
	SCSC_TAG_DBG4(WLOG, "BytesRead:%d  -- RecordsRead:%d\n",
		      read_bytes, got_records);

	return read_bytes;
}

static int wlog_default_ring_drainer(struct scsc_wlog_ring *r, size_t drain_sz)
{
	int rval = 0, drained_bytes = 0;
	size_t chunk_sz = drain_sz <= r->drain_sz ? drain_sz : r->drain_sz;
	struct scsc_wifi_ring_buffer_status ring_status = {};

	/* An SRCU on callback here would better */
	mutex_lock(&r->drain_lock);
	do {
		/* drain ... consumes data */
		rval = r->ops.read_records(r, r->drain_buf, chunk_sz, NULL, &ring_status, false);
		/* and push...if any callback defined */
		if (!r->flushing) {
			mutex_lock(&r->wl->lock);
			if (rval > 0 && r->wl->on_ring_buffer_data_cb) {
				SCSC_TAG_DEBUG(WLOG,
					       "Invoking registered log_handler:%p to drain %d bytes\n",
					       r->wl->on_ring_buffer_data_cb, rval);
				r->wl->on_ring_buffer_data_cb(r->st.name, r->drain_buf, rval,
							      &ring_status, r->wl->on_ring_buffer_ctx);
				SCSC_TAG_DBG4(WLOG, "Callback processed %d bytes\n", rval);
			}
			mutex_unlock(&r->wl->lock);
		}
		drained_bytes += rval;
	} while (rval && drained_bytes <= drain_sz);
	SCSC_TAG_DBG3(WLOG, "%s %d bytes\n", (r->flushing) ? "Flushed" : "Drained",
		      drained_bytes);

	/* Execute flush if required... */
	if (r->flushing) {
		unsigned long flags;

		/* Inhibit writers momentarily */
		raw_spin_lock_irqsave(&r->wlock, flags);
		r->dropped = 0;
		r->st.written_records = 0;
		r->st.read_bytes = r->st.written_bytes = 0;
		r->flushing = false;
		raw_spin_unlock_irqrestore(&r->wlock, flags);
		SCSC_TAG_INFO(WLOG, "Ring '%s' flushed.\n", r->st.name);
	}
	mutex_unlock(&r->drain_lock);

	return drained_bytes;
}

/**
 * A generic write that takes care to build the final payload created
 * concatenating:
 *  - the common record-header
 *  - an optionally provided ring_hdr
 *  - the provided payload buf
 *
 * The optional header is passed down as a separate parameters to avoid
 * unnecessary intermediate copies: this function will copy all the bits
 * in place directly into the proper calculated ring position.
 *
 * By design a read-end-point is always provided by the framework
 * (in terms of netlink channels towards the WiFi-HAL) so we spawn a
 * configurable reader-worker upon start of logging, and the same reader
 * is also invoked when ring is running out of space: for these reasons
 * the ring is meant NOT to overwrite itself ever.
 *
 * If NO periodic reader is spawned NOR a min_data_size threshold was
 * specified to force kick the periodic drainer, we could just end-up
 * filling up the ring: in that case we just drop and account for it.
 *
 * Data is drained and pushed periodically upstream using the
 * on_ring_buffer_data_cb if any provided and periodic drain was
 * configured.
 *
 * @r: the referenced ring
 * @buf: payload
 * @blen: payload_sz
 * @ring_hdr: upper-layer-record-header
 * @hlen: upper-layer-record-header length
 * @verbose_level: loglevel for this message (to be checked against)
 * @timestamp: a providewd timestamp (if any). If zero a timestamp will be
 *	       calculated.
 *
 * Final injected record will be composed as follows:
 *
 * |common_hdr|ring_hdr|buf|
 *
 * where the common header is compued and filled in by this function, and the
 * provided additional upper-layer header ring_hdr could be not provided.
 *
 * THIS BASIC RING OPERATION IS THE WORKHORSE USED BY THE PRODUCER API IMPLEMENTED
 * BY REAL RINGS, AND AS SUCH COULD BE INVOKED FROM ANY CONTEXTS...SO IT MUST NOT SLEEP.
 */
static int wlog_write_record(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
			     void *ring_hdr, size_t hlen, u32 verbose_level, u64 timestamp)
{
	u8 *start = NULL;
	u16 chunk_sz;
	unsigned long flags;

	if (scsc_wlog_ring_is_flushing(r))
		return 0;

	/* Just drop messages above configured verbose level. 0 is disabled */
	if (!scsc_wlog_is_message_allowed(r, verbose_level))
		return 0;

	//TODO Account for missing timestamp
	chunk_sz = sizeof(struct scsc_wifi_ring_buffer_entry) + hlen + blen;
	if (chunk_sz > MAX_RECORD_SZ) {
		SCSC_TAG_WARNING(WLOG, "Dropping record exceeding %d bytes\n",
				 chunk_sz);
		return 0;
	}

	raw_spin_lock_irqsave(&r->wlock, flags);
	/**
	 * Are there enough data to drain ?
	 * if so...drain...queueing work....
	 * if not (min_data_size ==  0) just do nothing
	 */
	if (!r->drop_on_full && r->min_data_size &&
	    AVAIL_BYTES(r) >= r->min_data_size)
		queue_work(r->drain_workq, &r->drain_work);
	/**
	 * If no min_data_size was specified, NOR a periodic read-worker
	 * was configured (i.e. max_interval_sec == 0), we could end up
	 * filling up the ring...in that case just drop...accounting for it.
	 *
	 * This is the case when packet_fate rings fills up...
	 */
	if (!CAN_FIT(r, chunk_sz)) {
		SCSC_TAG_DBG4(WLOG, "[%s]:: dropped %zd bytes\n",
			      r->st.name, blen + hlen);
		r->dropped += blen + hlen;
		raw_spin_unlock_irqrestore(&r->wlock, flags);
		return 0;
	}

	start = REC_START(r, WPOS(r));
	REC_HEADER_FILL(start, hlen + blen, timestamp, (u8)r->st.flags, r->type);
	start += sizeof(struct scsc_wifi_ring_buffer_entry);
	if (hlen) {
		memcpy(start, ring_hdr, hlen);
		start += hlen;
	}
	if (blen)
		memcpy(start, buf, blen);
	/* Account for rollover using spare area at end of ring... */
	if (start + blen > BUF_END(r))
		memcpy(BUF_START(r), BUF_END(r), start + blen - BUF_END(r));
	r->st.written_bytes += chunk_sz;
	r->st.written_records++;
	raw_spin_unlock_irqrestore(&r->wlock, flags);

	return chunk_sz;
}

static int wlog_default_ring_config_change(struct scsc_wlog_ring *r,
					   u32 verbose_level, u32 flags,
					   u32 max_interval_sec,
					   u32 min_data_size)
{
	u32 old_interval_sec;

	SCSC_TAG_DEBUG(WLOG, "Ring: %s  --  configuration change.\n",
		       r->st.name);

	r->min_data_size = min_data_size;
	old_interval_sec = r->max_interval_sec;
	r->max_interval_sec = max_interval_sec;

	if (r->state == RING_STATE_SUSPEND && r->st.verbose_level) {
		/* Restarting timeri where required ...
		 * it will take care to queue_work back.
		 */
		if (r->max_interval_sec)
			mod_timer(&r->drain_timer,
				  jiffies + msecs_to_jiffies(r->max_interval_sec * 1000));
		r->state = RING_STATE_ACTIVE;
		SCSC_TAG_INFO(WLOG, "ACTIVATED ring: %s\n", r->st.name);
	} else if (r->state == RING_STATE_ACTIVE && !r->st.verbose_level) {
		/* Stop timer, cancel pending work */
		del_timer_sync(&r->drain_timer);
		cancel_work_sync(&r->drain_work);
		r->state = RING_STATE_SUSPEND;
		SCSC_TAG_INFO(WLOG, "SUSPENDED ring: %s\n", r->st.name);
	} else if (r->state == RING_STATE_ACTIVE) {
		if (old_interval_sec != r->max_interval_sec) {
			if (!r->max_interval_sec)
				del_timer_sync(&r->drain_timer);
			else
				mod_timer(&r->drain_timer,
					  jiffies + msecs_to_jiffies(r->max_interval_sec * 1000));
		}
		SCSC_TAG_INFO(WLOG, "RECONFIGURED ring: %s\n", r->st.name);
	}

	return 0;
}

static wifi_error wlog_start_logging(struct scsc_wlog_ring *r,
				     u32 verbose_level, u32 flags,
				     u32 max_interval_sec,
				     u32 min_data_size)
{
	if (!r)
		return WIFI_ERROR_INVALID_ARGS;

	scsc_wlog_ring_change_verbosity(r, verbose_level);
	wlog_default_ring_config_change(r, verbose_level, flags,
					max_interval_sec, min_data_size);

	return WIFI_SUCCESS;
}

static struct scsc_wlog_ring_ops default_ring_ops = {
	.init = NULL,
	.finalize = NULL,
	.get_ring_status = wlog_get_ring_status,
	.read_records = wlog_read_records,
	.write_record = wlog_write_record,
	.loglevel_change = NULL,
	.drain_ring = wlog_default_ring_drainer,
	.start_logging = wlog_start_logging,
};

void scsc_wlog_ring_destroy(struct scsc_wlog_ring *r)
{
	if (!r || r->registered) {
		SCSC_TAG_ERR(WLOG, "Cannot destroy ring r:%p\n", r);
		return;
	}
	/* If initialized call custom finalizer at first..reverse order */
	if (r->initialized && r->ops.finalize)
		r->ops.finalize(r);
	wlog_ring_finalize(r);
	kfree(r);
}

struct scsc_wlog_ring *scsc_wlog_ring_create(char *ring_name, u32 flags,
					     u8 type, u32 size,
					     unsigned int features_mask,
					     init_cb init, finalize_cb fini,
					     void *priv)
{
	struct scsc_wlog_ring *r = NULL;

	WARN_ON(!ring_name || !size);

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return r;
	r->type = type;
	r->st.flags = flags;
	r->st.rb_byte_size = size;
	if (snprintf(r->st.name, RING_NAME_SZ - 1, "%s", ring_name) >= RING_NAME_SZ)
		SCSC_TAG_WARNING(WLOG, "Ring name too long...truncated to: %s\n",
				r->st.name);
	/* Setup defaults and configure init finalize if any provided */
	memcpy(&r->ops, &default_ring_ops, sizeof(struct scsc_wlog_ring_ops));
	r->ops.init = init;
	r->ops.finalize = fini;
	r->priv = priv;
	/* Basic common initialization is called first */
	if (wlog_ring_init(r)) {
		SCSC_TAG_ERR(WLOG,
			     "Wi-Fi Logger Ring %s basic initialization failed.\n",
			     r->st.name);
		kfree(r);
		return NULL;
	}
	if (r->ops.init) {
		if (r->ops.init(r)) {
			SCSC_TAG_DBG4(WLOG,
				      "Ring %s custom init completed\n",
				      r->st.name);
		} else {
			SCSC_TAG_ERR(WLOG,
				     "Ring %s custom init FAILED !\n",
				     r->st.name);
			scsc_wlog_ring_destroy(r);
			return NULL;
		}
	}
	r->features_mask = features_mask;
	raw_spin_lock_init(&r->rlock);
	raw_spin_lock_init(&r->wlock);
	r->initialized = true;
	SCSC_TAG_DEBUG(WLOG, "Ring '%s' initialized.\n", r->st.name);

	return r;
}

int scsc_wlog_register_loglevel_change_cb(struct scsc_wlog_ring *r,
					  int (*callback)(struct scsc_wlog_ring *r, u32 new_loglevel))
{
	if (!callback)
		r->ops.loglevel_change = NULL;
	else
		r->ops.loglevel_change = callback;

	return 0;
}

int scsc_wlog_drain_whole_ring(struct scsc_wlog_ring *r)
{
	SCSC_TAG_INFO(WLOG, "Draining whole ring %s\n", r->st.name);
	return r->ops.drain_ring(r, r->st.rb_byte_size);
}

void scsc_wlog_flush_ring(struct scsc_wlog_ring *r)
{
	r->flushing = true;
	/* kick the workq...which will take care of flushing */
	queue_work(r->drain_workq, &r->drain_work);
}
