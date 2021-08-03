/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef _SCSC_WIFILOGGER_CORE_H_
#define _SCSC_WIFILOGGER_CORE_H_
/**
 * @file
 *
 * Implements a basic ring abstraction to be used as a common foundation
 * upon which all the Wi-Fi Logger real rings are built.
 *
 * It will provide:
 *
 * - the basic record header common to all rings' flavours as defined in
 *   scsc_wifilogger_types.h::struct scsc_wifi_ring_buffer_entry
 *
 *	| entry | flags | type | timestamp |
 *
 * - a set of common basic rings' methods: default_ring_ops
 *
 * - a common periodic worker used to periodically drain the rings using
 *   one of the above operations (when configured to do so)
 *
 * General Ring Architecture
 * -------------------------
 * The ring is constituted by a buffer of contiguos memory of specified
 * size followed by a spare area of MAX_RECORD_SZ; this latter area is used
 * when a record would not fit the physical end of the ring buffer and would
 * be going to wrap around: in a such a case we simply write down the record
 * content and let it spill over into the spare area; we'll then take care
 * to copy the overflown part from the spare area into the start of the
 * physical buffer. For this reason a limit of MAX_RECORD_SZ length is
 * enforced on write.
 *
 * Ring status is maintained inside <struct scsc_wifi_ring_buffer_status> that
 * is a well known and defined structure defined in scsc_wifilogger_types.h;
 * such structure format is expected by Framework itself when it queries for
 * ring status using Wi-Fi HAL.
 * Such structure contains also the @read_bytes and @written_bytes counters
 * needed for all the ring mechanics based on modulo-ring-size aritmethic.
 * Modulo arithmethic is achieved without using the % operator itself so ring
 * is expected to be of power-of-two size.
 *
 * Ring's basic operations are defined as follows:
 * - MULTIPLE concurrent writers are expected (and handled)
 *
 * - Only ONE active reader is expected at any time: such a reader could act
 *   out of the periodic reader worker or triggered by a write operation.
 *
 * - each ring read-behavior is configured by two params:
 *   + min_data_size: the minimum amount of available data that should trigger
 *     a read. Ignored if zero.
 *   + max_interval_sec: periodic-reader interval in seconds. Ignored if zero.
 *
 * NOTE THAT if both the above params are configured as zero, no periodic or
 * threshold reading process will be performed and, in absence of any kind of
 * polling-read mechanism, the ring would finally FILL-UP: in such a case all
 * the data received once the ring is full will be DROPPED.
 * This behavior fits the pkt_fate use case scenario.
 */

#include "scsc_wifilogger_types.h"
#include "scsc_wifilogger_internal.h"

#define	DRAIN_BUF_SZ		4096

#define	BUF_SZ(r)		((r)->st.rb_byte_size)
#define BUF_START(r)		((r)->buf)
#define BUF_END(r)		((r)->buf + BUF_SZ(r))
#define AVAIL_BYTES(r)		((r)->st.written_bytes - (r)->st.read_bytes)

/**
 * Avoid % when calculating ring-relative position
 * Ring SIZE MUST BE A POWER OF TWO....currently is enforced in
 * WiFi-Logger code since there's no way (API) to set the rings' sizes.
 */
#define	RPOS(r) \
	((r)->st.read_bytes & (BUF_SZ(r) - 1))

#define	WPOS(r) \
	((r)->st.written_bytes & (BUF_SZ(r) - 1))

#define WPOS_INC(r, bytes) \
	(((r)->st.written_bytes + (bytes)) & (BUF_SZ(r) - 1))

#define REC_PAYLOAD_SZ(r, pos) \
	(*((u16 *)((r)->buf + (pos))))

#define REC_SZ(r, pos) \
	(REC_PAYLOAD_SZ(r, pos) + sizeof(struct scsc_wifi_ring_buffer_entry))

#define	REC_START(r, pos)	((r)->buf + (pos))

#define IS_EMPTY(r) \
	((r)->st.read_bytes == (r)->st.written_bytes)

#define	CAN_FIT(r, bytes) \
	(bytes < BUF_SZ(r) && \
	 (IS_EMPTY(r) || \
	  (WPOS(r) < RPOS(r) && WPOS(r) + (bytes) < RPOS(r)) || \
	  (WPOS(r) > RPOS(r) && (WPOS(r) + (bytes) < BUF_SZ(r) || WPOS_INC(r, bytes) < RPOS(r)))))

#define REC_HEADER_FILL(ptr, payload_sz, rtimestamp, rflags, rtype) \
	do { \
		struct scsc_wifi_ring_buffer_entry *h = \
			(struct scsc_wifi_ring_buffer_entry *)(ptr); \
		\
		h->entry_size = (payload_sz); \
		h->flags |= (rflags) | RING_BUFFER_ENTRY_FLAGS_HAS_TIMESTAMP; \
		h->type = (rtype); \
		h->timestamp = (rtimestamp) ?: local_clock();\
	} while (0)

#define	MINIMUM_DRAIN_CHUNK_BYTES		1024
#define	DEFAULT_DRAIN_CHUNK_SZ(r)		((r)->st.rb_byte_size / 2)
#define	FORCE_DRAIN_CHUNK_SZ(r)			((r)->st.rb_byte_size / 4)
#define MAX_RECORD_SZ				8192

enum {
	RING_STATE_SUSPEND,
	RING_STATE_ACTIVE
};

struct scsc_wlog_ring;

typedef	bool (*init_cb)(struct scsc_wlog_ring *r);
typedef bool (*finalize_cb)(struct scsc_wlog_ring *r);

struct scsc_wlog_ring_ops {
	init_cb init;
	finalize_cb finalize;

	wifi_error (*get_ring_status)(struct scsc_wlog_ring *r,
				      struct scsc_wifi_ring_buffer_status *status);
	int (*read_records)(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
			    u32 *records, struct scsc_wifi_ring_buffer_status *status, bool is_user);
	int (*write_record)(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
			    void *hdr, size_t hlen, u32 verbose_level, u64 timestamp);
	int (*loglevel_change)(struct scsc_wlog_ring *r, u32 new_loglevel);
	int (*drain_ring)(struct scsc_wlog_ring *r, size_t drain_sz);
	wifi_error (*start_logging)(struct scsc_wlog_ring *r, u32 verbose_level,
				    u32 flags, u32 max_interval_sec,
				    u32 min_data_size);
};

struct scsc_wlog_ring {
	bool			initialized;
	bool			registered;
	bool			flushing;
	bool			drop_on_full;
	u8			state;

	u8			*buf;
	unsigned int		features_mask;
	u8			type;
	u32			min_data_size;
	u32			max_interval_sec;
	u32			dropped;
	u32			*verbosity;
	raw_spinlock_t		rlock, wlock;
	struct scsc_wifi_ring_buffer_status st;

	u8			*drain_buf;
	size_t			drain_sz;
	struct mutex		drain_lock;
	struct timer_list	drain_timer;
	struct work_struct	drain_work;
	struct workqueue_struct	*drain_workq;

	struct scsc_wlog_ring_ops ops;

	void			*priv;

	struct scsc_wifi_logger	*wl;
};

struct scsc_wlog_ring *scsc_wlog_ring_create(char *ring_name, u32 flags,
					     u8 type, u32 size,
					     unsigned int features_mask,
					     init_cb init, finalize_cb fini,
					     void *priv);

void scsc_wlog_ring_destroy(struct scsc_wlog_ring *r);

int scsc_wlog_register_loglevel_change_cb(struct scsc_wlog_ring *r,
					  int (*callback)(struct scsc_wlog_ring *r,
					  u32 new_loglevel));

int scsc_wlog_drain_whole_ring(struct scsc_wlog_ring *r);

static inline bool scsc_wlog_is_ring_empty(struct scsc_wlog_ring *r)
{
	return r->st.written_bytes == r->st.read_bytes;
}

static inline wifi_error scsc_wlog_get_ring_status(struct scsc_wlog_ring *r,
						   struct scsc_wifi_ring_buffer_status *status)
{
	if (!r)
		return WIFI_ERROR_INVALID_ARGS;

	return r->ops.get_ring_status(r, status);
}

static inline bool scsc_wlog_is_message_allowed(struct scsc_wlog_ring *r, u32 verbose_level)
{
	return r->st.verbose_level && verbose_level <= r->st.verbose_level;
}

static inline int scsc_wlog_read_records(struct scsc_wlog_ring *r, u8 *buf, size_t blen, bool is_user)
{
	return r->ops.read_records(r, buf, blen, NULL, NULL, false);
}

static inline int scsc_wlog_read_max_records(struct scsc_wlog_ring *r, u8 *buf,
					     size_t blen, u32 *max_records, bool is_user)
{
	return r->ops.read_records(r, buf, blen, max_records, NULL, is_user);
}

static inline int scsc_wlog_write_record(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
					 void *hdr, size_t hlen, u32 verbose_level, u64 timestamp)
{
	return r->ops.write_record(r, buf, blen, hdr, hlen, verbose_level, timestamp);
}

static inline wifi_error scsc_wlog_start_logging(struct scsc_wlog_ring *r,
						 u32 verbose_level, u32 flags,
						 u32 max_interval_sec, u32 min_data_size)
{
	return r->ops.start_logging(r, verbose_level, flags, max_interval_sec, min_data_size);
}

static inline void scsc_wlog_ring_set_drop_on_full(struct scsc_wlog_ring *r)
{
	r->drop_on_full = true;
}

static inline void scsc_wlog_register_verbosity_reference(struct scsc_wlog_ring *r, u32 *verbose_ref)
{
	r->verbosity = verbose_ref;
}

static inline bool scsc_wlog_ring_is_flushing(struct scsc_wlog_ring *r)
{
	if (!r->flushing)
		return false;

	SCSC_TAG_DBG4(WLOG, "Ring is flushing..abort pending read/write\n");
	return true;
}

static inline void scsc_wlog_ring_change_verbosity(struct scsc_wlog_ring *r, u32 verbose_level)
{
	if (r->st.verbose_level != verbose_level) {
		if (r->ops.loglevel_change)
			r->ops.loglevel_change(r, verbose_level);
		r->st.verbose_level = verbose_level;
		if (r->verbosity)
			*r->verbosity = r->st.verbose_level;
		SCSC_TAG_INFO(WLOG, "Ring: %s -- verbose_level changed to: %d\n",
			      r->st.name, r->st.verbose_level);
	}
}

void scsc_wlog_flush_ring(struct scsc_wlog_ring *r);

#endif /*_SCSC_WIFI_LOGGER_CORE_H_*/
