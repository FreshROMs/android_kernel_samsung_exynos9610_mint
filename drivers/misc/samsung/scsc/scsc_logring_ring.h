/******************************************************************************
 *
 *   Copyright (c) 2016-2017 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/

#ifndef _SCSC_LOGRING_RING_H_
#define _SCSC_LOGRING_RING_H_

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/hardirq.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/crc32.h>

#include <scsc/scsc_logring.h>

#include "scsc_logring_common.h"

#define SCSC_BINFO_LEN                    32
#define SCSC_HBUF_LEN                    128
/* A safe size to enforce on ingressing binary blobs; this accounts
 * for possible binary expansion while reading, in order to fit the reader
 * DEFAULT_TBUF_SZ in any possible case: this way we avoid to have truncated
 * data also on read while dumping big binary blobs.
 */
#define SCSC_MAX_BIN_BLOB_SZ		1920
/**
 * This spare area is used to prepare a logging entry before pushing it into
 * the ring and so it's the maximum length allowed for a log entry.
 * When this change (hugely) you should check the size of len field
 * in the following struct scsc_ring_record.
 */
#define BASE_SPARE_SZ                   2048
#define RNAME_SZ                          16
#define DEFAULT_RING_BUFFER_SZ	     1048576
#define DEFAULT_ENABLE_HEADER              1
#define DEFAULT_ENABLE_LOGRING             1
/* The default len, in bytes, of the binary blob to decode in ASCII
 * Human readable form. -1 means DECODE EVERYTHING !
 */
#define DEFAULT_BIN_DECODE_LEN            -1
#define DEBUGFS_ROOT     "/sys/kernel/debug"
#define DEBUGFS_RING0_ROOT           "ring0"

/**
 * Our ring buffer is allocated simply as a bunch of contiguos bytes.
 * Data is stored as a contiguos stream of concatenated records, each one
 * starting with a record descriptor of type scsc_ring_record: data content
 * is then appended to the record descriptor; in this way we can account
 * for different types of content, pushing the TAG describing the content
 * into the record descriptor itself, being then able to operate differently
 * on read depending on the type of content.
 * The tail and head references 'points' to the start of the first (oldest)
 * and the last (newest) record: any write will happen after the end
 * of the current head: these references in fact points to the starting byte
 * of the record descriptor modulus the ring size (they're NOT abosolute
 * pointers). Since our 'metadata' is embedded inside the ring itself (like
 * printk does) we never write variable string content in place BUT use
 * instead the spare area (allocated contiguosly at the end of the ring)
 * to expand the provided format string and then memcpy the content to
 * the final position after having properly updated the record descriptors
 * and eventually moved away the tail when overwriting.
 * Moreover we never wrap a record across the ring boundary: if there's NOT
 * enough space at the end of the ring, we simply place it at the start.
 * Moreover this structs holds a kcache reference to allocate temporary
 * buffers to use when double buffering is needed, a spinlock_t for
 * protection and a wait_queue_t for blocking I/O.
 *
 * @buf: the ring-buffer itself starts here
 * @spare: start of spare area (buf[bsz])
 * @name: a simple named identifier
 * @bsz: ring size
 * @ssz: size of spare (fixed at BASE_SPARE_SZ)
 * @head: newest record written (first byte)...next write after it
 * @tail: odelst record written (first byte)...full dump read will start
 * from here
 * @last: the last record before the end of the ring.
 * @records: the number of records
 * @written: a general progressive counter of total bytes written into
 * the ring
 * @lock: a spinlock_t to protetc concurrent access
 * @wq: a wait queue where to put sleeping processes waiting for input.
 * They're woken up at the end os scsc_printk().
 * @refc: a reference counter...currently unused.
 * @private: useful to hold some user provided data (used to hold debugfs
 * initdata related to this ring)
 * @kcache: a reference to a kmem_cache created at initialization time
 * to get fresh temporary buffers on the fly when copying to user and in
 * need of a double buffer
 */
struct scsc_ring_buffer {
	char              *buf;
	char              *spare;
	char              name[RNAME_SZ];
	size_t            bsz;
	size_t            ssz;
	loff_t            head;
	loff_t            tail;
	loff_t            last;
	int               records;
	int               wraps;
	int		  oos;
	u64               written;
	raw_spinlock_t    lock;
	wait_queue_head_t wq;
	atomic_t          refc;
	void *private;
};

/**
 * Our ring buffer is now built concatenating entries prepended by a record
 * that describes the content itself. This will allow us to store different
 * types of data (NOT only string) and to interpret it.
 * Each record is described by this struct that is laid out in front of the
 * effective content:
 *
 * | SYNC | CRC | tag | len | lev | ctx |  core | nsec  |  <buffer len  - - |
 *
 * @SYNC: a fixed pattern to search for when re-syncing after a reader
 *	  has got lost.
 * @CRC: CRC32 calculated, using kernel crc32_le, on the whole record header,
 *       taking care to substitute this field with the 32 LSB of this record
 *       relative starting position (relative to the absolute ring buffer
 *       start.
 * @tag: type of this record...matters expecially to identify binary data
 *	 record
 * @len: this is the length in bytes of buffer. All string content should
 *	 be NULL terminated. This length will anyway NEVER exceed
 *	 BASE_SPARE_SZ that's currently a few KB.
 * @lev: the debuglevel associated to this message.
 * @ctx: the execution context of the logged line:
 *	 SoftIRQ / Interrupt / Process
 * @core: the CPU core id
 * @nsec: the timestamp in nanoseconds
 */
struct scsc_ring_record {
	u32 sync;
	u32 crc;
	u8 tag;
	u16 len;
	u8 lev;
	u8 ctx;
	u8 core;
	s64 nsec;
} __packed; /* should NOT be needed */

#define SYNC_MAGIC	0xDEADBEEF
/**
 * Fill a scsc_ring_record descriptor
 * local_clock() is from the same family of time-func used
 * by printk returns nanoseconds
 */
#define SCSC_FILL_RING_RECORD(r, tag, lev) \
	do { \
		(r)->sync = SYNC_MAGIC; \
		(r)->crc = 0; \
		(r)->nsec = local_clock(); \
		(r)->tag = tag; \
		(r)->len = 0; \
		(r)->lev = lev; \
		(r)->ctx = ((in_interrupt()) ? \
			    ((in_softirq()) ? 'S' : 'I') : 'P'); \
		(r)->core = smp_processor_id(); \
	} while (0)


#define SCSC_RINGREC_SZ         (sizeof(struct scsc_ring_record))
#define SCSC_CRC_RINGREC_SZ	(SCSC_RINGREC_SZ - sizeof(SYNC_MAGIC))

#define SCSC_IS_RING_IN_USE(ring) \
	((atomic_read(&((struct scsc_ring_buffer *)(ring))->refc)) != 0)

#define SCSC_GET_RING_REFC(ring) \
	atomic_inc(&((struct scsc_ring_buffer *)(ring))->refc)

#define SCSC_PUT_RING_REFC(ring) \
	atomic_dec(&((struct scsc_ring_buffer *)(ring))->refc)


#define SCSC_GET_REC_BUF(p)             (((char *)(p)) + SCSC_RINGREC_SZ)

#define SCSC_GET_REC_LEN(recp)  (((struct scsc_ring_record *)(recp))->len)

#define SCSC_GET_REC_TAG(recp)  (((struct scsc_ring_record *)(recp))->tag)

#define SCSC_GET_REC_CRC(recp) (((struct scsc_ring_record *)(recp))->crc)

#define SCSC_GET_PTR(ring, pos)                 ((ring)->buf + (pos))

#define SCSC_GET_REC(ring, pos) \
	((struct scsc_ring_record *)(SCSC_GET_PTR((ring), (pos))))

#define SCSC_IS_REC_SYNC_VALID(recp)	((recp)->sync == SYNC_MAGIC)

#define SCSC_GET_HEAD_PTR(ring)         SCSC_GET_PTR((ring), (ring)->head)

#define SCSC_GET_NEXT_FREE_SLOT_PTR(ring) \
	(SCSC_GET_HEAD_PTR((ring)) + SCSC_RINGREC_SZ + \
	 SCSC_GET_REC_LEN(SCSC_GET_HEAD_PTR(ring)))

#define SCSC_GET_SLOT_LEN(ring, pos) \
	(((SCSC_GET_REC_LEN(SCSC_GET_PTR((ring), (pos)))) != 0) ? \
	 (SCSC_RINGREC_SZ + SCSC_GET_REC_LEN(SCSC_GET_PTR((ring), (pos)))) : 0)

#define SCSC_GET_NEXT_SLOT_POS(ring, pos) \
	((pos) + SCSC_GET_SLOT_LEN((ring), (pos)))

#define SCSC_RING_FREE_BYTES(rb) \
	(((rb)->head >= (rb)->tail) ? \
	 ((rb)->bsz - SCSC_GET_NEXT_SLOT_POS(rb, rb->head)) : \
	 ((rb)->tail - SCSC_GET_NEXT_SLOT_POS(rb, rb->head)))

#define SCSC_USED_BYTES(rb)     ((rb)->bsz - SCSC_RING_FREE_BYTES(rb))

#define SCSC_LOGGED_BYTES(rb)   (SCSC_USED_BYTES(rb) - \
				 ((rb)->records * SCSC_RINGREC_SZ))

#define SCSC_GET_NEXT_REC_ENTRY_POS(ring, rpos) \
	(rpos + SCSC_RINGREC_SZ + \
	 SCSC_GET_REC_LEN(SCSC_GET_PTR((ring), (rpos))))

/* Ring buffer API */
struct scsc_ring_buffer *alloc_ring_buffer(size_t bsz, size_t ssz,
					   const char *name) __init;
void free_ring_buffer(struct scsc_ring_buffer *rb);
void scsc_ring_truncate(struct scsc_ring_buffer *rb);
int push_record_string(struct scsc_ring_buffer *rb, int tag, int lev,
		       int prepend_header, const char *msg_head, va_list args);
int push_record_blob(struct scsc_ring_buffer *rb, int tag, int lev,
		     int prepend_header, const void *start, size_t len);
size_t read_next_records(struct scsc_ring_buffer *rb, int max_recs,
			 loff_t *last_read_rec, void *tbuf, size_t tsz);
struct scsc_ring_buffer *scsc_ring_get_snapshot(const struct scsc_ring_buffer *rb,
						void *snap_buf, size_t snap_sz,
						char *snap_name);
#endif /* _SCSC_LOGRING_RING_H_ */
