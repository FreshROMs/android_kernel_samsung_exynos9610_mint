/****************************************************************************
 *
 * Copyright (c) 2016-2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#include "scsc_logring_ring.h"

#ifdef CONFIG_SCSC_STATIC_RING_SIZE
static char a_ring[CONFIG_SCSC_STATIC_RING_SIZE + BASE_SPARE_SZ] __aligned(4);
#endif

static int  scsc_decode_binary_len = DEFAULT_BIN_DECODE_LEN;
module_param(scsc_decode_binary_len, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_decode_binary_len,
		   "When reading a binary record dump these bytes-len in ASCII human readable form when reading",
		   "run-time", DEFAULT_BIN_DECODE_LEN);

/*
 * NOTE_CREATING_TAGS: when adding a tag string here REMEMBER to add
 * it also where required, taking care to maintain the same ordering.
 * (Search 4 NOTE_CREATING_TAGS)
 */
const char *tagstr[MAX_TAG + 1] = {
	"binary",
	"bin_wifi_ctrl_rx",
	"bin_wifi_data_rx",
	"bin_wifi_ctrl_tx",
	"bin_wifi_data_tx",
	"wlbt",			/* this is the generic one...NO_TAG */
	"wifi_rx",
	"wifi_tx",
	"bt_common",
	"bt_h4",
	"bt_fw",
	"bt_rx",
	"bt_tx",
	"cpktbuff",
	"fw_load",
	"fw_panic",
	"gdb_trans",
	"mif",
	"clk20",
	"clk20_test",
	"fm",
	"fm_test",
	"mx_file",
	"mx_fw",
	"mx_sampler",
	"mxlog_trans",
	"mxman",
	"mxman_test",
	"mxmgt_trans",
	"mx_mmap",
	"mx_proc",
	"panic_mon",
	"pcie_mif",
	"plat_mif",
	"kic_common",
	"wlbtd",
	"wlog",
	"lerna",
	"mx_cfg",
#ifdef CONFIG_SCSC_DEBUG_COMPATIBILITY
	"init_deinit",
	"netdev",
	"cfg80211",
	"mlme",
	"summary_frames",
	"hydra",
	"tx",
	"rx",
	"udi",
	"wifi_fcq",
	"hip",
	"hip_init_deinit",
	"hip_fw_dl",
	"hip_sdio_op",
	"hip_ps",
	"hip_th",
	"hip_fh",
	"hip_sig",
	"func_trace",
	"test",
	"src_sink",
	"fw_test",
	"rx_ba",
	"tdls",
	"gscan",
	"mbulk",
	"flowc",
	"smapper",
#endif
	"test_me"
};

/**
 * Calculate and returns the CRC32 for the provided record and record pos.
 * Before calculating the CRC32 the crc field is temporarily substituted
 * with the 32 LSB record relative starting position.
 * Assumes the rec ptr area-validity has been checked upstream in the
 * caller chain.
 * We SKIP the fixed blob of the SYNC field that is placed ahead of
 * CRC field.
 * Assumes the related ring buffer is currently atomically accessed by
 * caller. MUST NOT SLEEP.
 */
static inline uint32_t get_calculated_crc(struct scsc_ring_record *rec,
					  loff_t pos)
{
	uint32_t calculated_crc = 0;
	uint32_t saved_crc = 0;

	saved_crc = rec->crc;
	rec->crc = (uint32_t)pos;
	/* we skip the fixed sync calculating crc */
	calculated_crc =
		crc32_le(~0, (unsigned char const *)&rec->crc,
			 SCSC_CRC_RINGREC_SZ);
	rec->crc = saved_crc;
	return calculated_crc;
}

/**
 * Checks for record CRC sanity.
 * Assumes the related ring buffer is currently atomically accessed by
 * caller. MUST NOT SLEEP.
 */
static inline bool is_record_crc_valid(struct scsc_ring_record *rec,
				       loff_t pos)
{
	uint32_t calculated_crc = 0;

	calculated_crc = get_calculated_crc(rec, pos);
	return calculated_crc == rec->crc;
}

/**
 * Calculate the proper CRC and set it into the crc field
 * Assumes the related ring buffer is currently atomically accessed by
 * caller. MUST NOT SLEEP.
 */
static inline void finalize_record_crc(struct scsc_ring_record *rec,
				       loff_t pos)
{
	uint32_t calculated_crc = 0;

	if (!rec)
		return;
	rec->crc = (uint32_t)pos;
	calculated_crc =
		crc32_le(~0, (unsigned char const *)&rec->crc,
			 SCSC_CRC_RINGREC_SZ);
	rec->crc = calculated_crc;
}

/**
 * This function analyzes the pos provided relative to the provided
 * ring, just to understand if it can be safely dereferenced.
 * Assumes RING is already locked.
 */
static inline bool is_ring_pos_safe(struct scsc_ring_buffer *rb,
				    loff_t pos)
{
	if (!rb || pos > rb->bsz || pos < 0)
		return false;
	/* NOT Wrapped */
	if (rb->head > rb->tail && pos > rb->head)
		return false;
	/* Wrapped... */
	if (rb->head < rb->tail &&
	    (pos > rb->head && pos < rb->tail))
		return false;
	return true;
}

/**
 * This sanitizes record header before using it.
 * It must be in the proper area related to head and tail and
 * the CRC must fit the header.
 */
static inline bool is_ring_read_pos_valid(struct scsc_ring_buffer *rb,
					  loff_t pos)
{
	if (!is_ring_pos_safe(rb, pos))
		goto oos;
	/* We do not check for SYNC before CRC since most of the time
	 * you are NOT OutOfSync and so you MUST check CRC anyway.
	 * It will be useful only for resync.
	 * At last...Check CRC ... doing this check LAST avoids the risk of
	 * dereferencing an already dangling pos pointer.
	 */
	if (!is_record_crc_valid(SCSC_GET_REC(rb, pos), pos))
		goto oos;
	return true;
oos:
	if (rb)
		rb->oos++;
	return false;
}


/**
 * Buid a header into the provided buffer,
 * and append the optional trail string
 */
static inline
int build_header(char *buf, int blen, struct scsc_ring_record *r,
		 const char *trail)
{
	int            written = 0;
	struct timeval tval = {};

	tval = ns_to_timeval(r->nsec);
	written = scnprintf(buf, blen,
			    "<%d>[%6lu.%06ld] [c%d] [%c] [%s] :: %s",
			    r->lev, tval.tv_sec, tval.tv_usec,
			    r->core, (char)r->ctx, tagstr[r->tag],
			    (trail) ? : "");
	return written;
}


/**
 * We're going to overwrite something writing from the head toward the tail
 * so we must search for the next tail far enough from head in oder not to be
 * overwritten: that will be our new tail after the wrap over.
 */
static inline
loff_t find_next_tail_far_enough_from_start(struct scsc_ring_buffer *rb,
					    loff_t start, int len)
{
	loff_t new_tail = rb->tail;

	while (start + len >= new_tail && new_tail != rb->last) {
		new_tail = SCSC_GET_NEXT_REC_ENTRY_POS(rb, new_tail);
		rb->records--;
	}
	if (start + len >= new_tail) {
		new_tail = 0;
		rb->records--;
	}
	return new_tail;
}

/**
 * This handles the just plain append of a record to head without
 * any need of wrapping or overwriting current tail
 * You can provide two buffer here: the second, hbuf, is optional
 * and will be written first. This is to account for the binary case
 * in which the record data are written at first into the spare area
 * (like we do with var strings, BUT then the bulk of binary data is
 * written directly in place into the ring without double copies.
 */
static inline
void scsc_ring_buffer_plain_append(struct scsc_ring_buffer *rb,
				   const char *srcbuf, int slen,
				   const char *hbuf, int hlen)
{
	/* empty condition is special case */
	if (rb->records)
		rb->head += SCSC_GET_SLOT_LEN(rb, rb->head);
	if (hbuf)
		memcpy(SCSC_GET_HEAD_PTR(rb), hbuf, hlen);
	else
		hlen = 0;
	memcpy(SCSC_GET_HEAD_PTR(rb) + hlen, srcbuf, slen);
	finalize_record_crc((struct scsc_ring_record *)SCSC_GET_HEAD_PTR(rb),
			    rb->head);
	rb->records++;
	if (rb->head > rb->last)
		rb->last = rb->head;
}


/**
 * This handles the case in which appending current record must account
 * for overwriting: this sitiation can happen at the end of ring if we do NOT
 * have enough space for the current record, or in any place when the buffer
 * has wrapped, head is before tail and there's not enough space to write
 * between current head and tail.
 */
static inline
void scsc_ring_buffer_overlap_append(struct scsc_ring_buffer *rb,
				     const char *srcbuf, int slen,
				     const char *hbuf, int hlen)
{
	if (rb->head < rb->tail &&
	    slen + hlen < rb->bsz - SCSC_GET_NEXT_SLOT_POS(rb, rb->head))
		rb->head += SCSC_GET_SLOT_LEN(rb, rb->head);
	else {
		rb->last = rb->head;
		rb->head = 0;
		rb->tail = 0;
		rb->wraps++;
	}
	rb->tail =
		find_next_tail_far_enough_from_start(rb, rb->head, slen + hlen);
	if (hbuf)
		memcpy(SCSC_GET_HEAD_PTR(rb), hbuf, hlen);
	else
		hlen = 0;
	memcpy(SCSC_GET_HEAD_PTR(rb) + hlen, srcbuf, slen);
	finalize_record_crc((struct scsc_ring_record *)SCSC_GET_HEAD_PTR(rb),
			    rb->head);
	rb->records++;
	if (rb->head > rb->last)
		rb->last = rb->head;
}


/**
 * This uses the spare area to prepare the record descriptor and to expand
 * the format string into the spare area in order to get the final lenght of
 * the whole record+data. Data is pre-pended with a header representing the
 * data hold in binary form in the record descriptor.
 * This data duplication helps when we'll read back a record holding string
 * data, we won't have to build the header on the fly during the read.
 */
static inline
int tag_writer_string(char *spare, int tag, int lev,
		      int prepend_header, const char *msg_head, va_list args)
{
	int                     written;
	char                    bheader[SCSC_HBUF_LEN] = {};
	struct scsc_ring_record *rrec;

	/* Fill record in place */
	rrec = (struct scsc_ring_record *)spare;
	SCSC_FILL_RING_RECORD(rrec, tag, lev);
	if (prepend_header)
		build_header(bheader, SCSC_HBUF_LEN, rrec, NULL);
	written = scnprintf(SCSC_GET_REC_BUF(spare),
			    BASE_SPARE_SZ - SCSC_RINGREC_SZ, "%s", bheader);
	/**
	 * NOTE THAT
	 * ---------
	 * vscnprintf retvalue is the number of characters which have been
	 * written into the @buf NOT including the trailing '\0'.
	 * If @size is == 0 the function returns 0.
	 * Here we enforce a line lenght limit equal to
	 * BASE_SPARE_SZ - SCSC_RINGREC_SZ.
	 */
	written += vscnprintf(SCSC_GET_REC_BUF(spare) + written,
			      BASE_SPARE_SZ - SCSC_RINGREC_SZ - written,
			      msg_head, args);
	/* complete record metadata */
	rrec->len = written;
	return written;
}

/**
 * A ring API function to push variable length format string into the buffer
 * After the record has been created and pushed into the ring any process
 * waiting on the related waiting queue is awakened.
 */
int push_record_string(struct scsc_ring_buffer *rb, int tag, int lev,
		       int prepend_header, const char *msg_head, va_list args)
{
	int           rec_len = 0;
	loff_t        free_bytes;
	unsigned long flags;

	/* Prepare ring_record and header if needed */
	raw_spin_lock_irqsave(&rb->lock, flags);
	rec_len = tag_writer_string(rb->spare, tag, lev, prepend_header,
				    msg_head, args);
	/* Line too long anyway drop */
	if (rec_len >= BASE_SPARE_SZ - SCSC_RINGREC_SZ) {
		raw_spin_unlock_irqrestore(&rb->lock, flags);
		return 0;
	}
	free_bytes = SCSC_RING_FREE_BYTES(rb);
	/**
	 * Evaluate if it's a trivial append or if we must account for
	 * any overwrap. Note that we do NOT truncate record across ring
	 * boundaries, if a record does NOT fit at the end of buffer,
	 * we'll write it from start directly.
	 */
	if (rec_len + SCSC_RINGREC_SZ < free_bytes)
		scsc_ring_buffer_plain_append(rb, rb->spare,
					      SCSC_RINGREC_SZ + rec_len,
					      NULL, 0);
	else
		scsc_ring_buffer_overlap_append(rb, rb->spare,
						SCSC_RINGREC_SZ + rec_len,
						NULL, 0);
	rb->written += rec_len;
	raw_spin_unlock_irqrestore(&rb->lock, flags);
	/* WAKEUP EVERYONE WAITING ON THIS BUFFER */
	wake_up_interruptible(&rb->wq);
	return rec_len;
}

/* This simply builds up a record descriptor for a binary entry. */
static inline
int tag_writer_binary(char *spare, int tag, int lev, size_t hexlen)
{
	struct scsc_ring_record *rrec;

	rrec = (struct scsc_ring_record *)spare;
	SCSC_FILL_RING_RECORD(rrec, tag, lev);
	rrec->len = hexlen;

	return hexlen;
}

/**
 * A ring API function to push binary data into the ring buffer. Binary data
 * is copied from the start/len specified location.
 * After the record has been created and pushed into the ring any process
 * waiting on the related waiting queue is awakened.
 */
int push_record_blob(struct scsc_ring_buffer *rb, int tag, int lev,
		     int prepend_header, const void *start, size_t len)
{
	loff_t        free_bytes;
	unsigned long flags;

	if (len > SCSC_MAX_BIN_BLOB_SZ)
		len = SCSC_MAX_BIN_BLOB_SZ;
	/* Prepare ring_record and header if needed */
	raw_spin_lock_irqsave(&rb->lock, flags);
	memset(rb->spare, 0x00, rb->ssz);
	tag_writer_binary(rb->spare, tag, lev, len);
	free_bytes = SCSC_RING_FREE_BYTES(rb);
	if (len + SCSC_RINGREC_SZ < free_bytes)
		scsc_ring_buffer_plain_append(rb, start, len,
					      rb->spare, SCSC_RINGREC_SZ);
	else
		scsc_ring_buffer_overlap_append(rb, start, len,
						rb->spare, SCSC_RINGREC_SZ);
	rb->written += len;
	raw_spin_unlock_irqrestore(&rb->lock, flags);
	/* WAKEUP EVERYONE WAITING ON THIS BUFFER */
	wake_up_interruptible(&rb->wq);
	return len;
}

/* A simple reader used to retrieve a string from the record
 * It always return ONE WHOLE RECORD if it fits the provided tbuf OR NOTHING.
 */
static inline
size_t tag_reader_string(char *tbuf, struct scsc_ring_buffer *rb,
			 int start_rec, size_t tsz)
{
	size_t max_chunk = SCSC_GET_REC_LEN(SCSC_GET_PTR(rb, start_rec));

	if (max_chunk <= tsz)
		memcpy(tbuf, SCSC_GET_REC_BUF(rb->buf + start_rec), max_chunk);
	else
		max_chunk = 0;
	return max_chunk;
}

/*
 * Helper to dump binary data in ASCII readable form up to
 * scsc_decode_binary_len bytes: when such modparam is set to -1
 * this will dump all the available data. Data is dumped onto the
 * output buffer with an endianity that conforms to the data as
 * dumped by the print_hex_dump() kernel standard facility.
 */
static inline
int binary_hexdump(char *tbuf, int tsz, struct scsc_ring_record *rrec,
		   int start, int dlen)
{
	int           i, j, bytepos;
	unsigned char *blob = SCSC_GET_REC_BUF(rrec);
	char          *hmap = "0123456789abcdef";

	/**
	 * Scan the buffer reversing endianity when appropriate and
	 * producing ASCII human readable output while obeying chosen
	 * maximum decoden_len dlen.
	 */
	for (j = start, i = 0; j < tsz && i < rrec->len && i < dlen; i += 4) {
		bytepos = (rrec->len - i - 1 >= 3) ? 3 : rrec->len - i - 1;
		/* Reverse endianity to little only on 4-byte boundary */
		if (bytepos == 3) {
			for (; bytepos >= 0; bytepos--) {
				if (i + bytepos >= dlen)
					continue;
				tbuf[j++] = hmap[blob[i + bytepos] >> 4 & 0x0f];
				tbuf[j++] = hmap[blob[i + bytepos] & 0x0f];
			}
		} else {
			int bb;

			/**
			 * Trailing bytes NOT aligned on a 4-byte boundary
			 * should be decoded maintaining the original endianity.
			 * This way we obtain a binary output perfectly equal
			 * to the one generated by the original UDI tools.
			 */
			for (bb = 0; bb <= bytepos; bb++) {
				if (i + bb >= dlen)
					break;
				tbuf[j++] = hmap[blob[i + bb] >> 4 & 0x0f];
				tbuf[j++] = hmap[blob[i + bb] & 0x0f];
			}
		}
	}
	return j;
}

/**
 * A reader used to dump binary records: this function first of all
 * builds a proper human readable header to identify the record with the
 * usual debuglevel and timestamps and then DUMPS some of the binary blob
 * in ASCII human readable form: how much is dumped depends on the module
 * param scsc_decode_binary_len (default 8 bytes).
 * ANYWAY ONLY ONE WHOLE RECORD IS DUMPED OR NOTHING IF IT DOES NOT FIT
 * THE PROVIDED DESTINATION BUFFER TBUF.
 */
static inline
size_t tag_reader_binary(char *tbuf, struct scsc_ring_buffer *rb,
		      int start_rec, size_t tsz)
{
	size_t                  written;
	int                     declen = scsc_decode_binary_len;
	struct scsc_ring_record *rrec;
	char                    bheader[SCSC_HBUF_LEN] = {};
	char                    binfo[SCSC_BINFO_LEN] = {};
	size_t			max_chunk;

	rrec = (struct scsc_ring_record *)SCSC_GET_PTR(rb, start_rec);
	if (declen < 0 || declen > rrec->len)
		declen = rrec->len;
	if (declen)
		snprintf(binfo, SCSC_BINFO_LEN, "HEX[%d/%d]: ",
			 declen, rrec->len);
	written = build_header(bheader, SCSC_HBUF_LEN, rrec,
			       declen ? binfo : "");
	/* Account for byte decoding: two ASCII char for each byte */
	max_chunk = written + (declen * 2);
	if (max_chunk <= tsz) {
		memcpy(tbuf, bheader, written);
		if (declen)
			written = binary_hexdump(tbuf, tsz - written,
						 rrec, written, declen);
		tbuf[written] = '\n';
		written++;
	} else {
		written = 0;
	}
	return written;
}

/**
 * This is a utility function to read from the specified ring_buffer
 * up to 'tsz' amount of data starting from position record 'start_rec'.
 * This function reads ONLY UP TO ONE RECORD and returns the effective
 * amount of data bytes read; it invokes the proper tag_reader_* helper
 * depending on the specific record is handling.
 * Data is copied to a TEMP BUFFER provided by user of this function,
 * IF AND ONLY IF a whole record CAN fit into the space available in the
 * destination buffer, otherwise record is NOT copied and 0 is returned.
 * This function DOES NOT SLEEP.
 * Caller IS IN CHARGE to SOLVE any sync issue on provided tbuf and
 * underlying ring buffer.
 *
 * @tbuf: a temp buffer destination for the read data
 * @rb: the ring_buffer to use.
 * @start_rec: the record from which to start expressed as a record
 * starting position.
 * @tsz: the available space in tbuf
 * @return size_t: returns the bytes effectively read.
 */
static inline size_t
_read_one_whole_record(void *tbuf, struct scsc_ring_buffer *rb,
		       int start_rec, size_t tsz)
{
	if (SCSC_GET_REC_TAG(SCSC_GET_PTR(rb, start_rec)) > LAST_BIN_TAG)
		return tag_reader_string(tbuf, rb, start_rec, tsz);
	else
		return tag_reader_binary(tbuf, rb, start_rec, tsz);
}


/**
 * This just inject a string into the buffer to signal we've gone
 * OUT OF SYNC due to Ring WRAPPING too FAST, noting how many bytes
 * we resynced.
 */
static inline size_t mark_out_of_sync(char *tbuf, size_t tsz,
				      int resynced_bytes)
{
	size_t		written = 0;
	struct timeval  tval = {};

	tval = ns_to_timeval(local_clock());
	/* We should write something even if truncated ... */
	written = scnprintf(tbuf, tsz,
			    "<7>[%6lu.%06ld] [c%d] [P] [OOS] :: [[[ OUT OF SYNC -- RESYNC'ED BYTES %d ]]]\n",
			    tval.tv_sec, tval.tv_usec, smp_processor_id(),
			    resynced_bytes);
	return written;
}

/**
 * Attempt resync searching for SYNC pattern and verifying CRC.
 * ASSUMES that the invalid_pos provided is anyway safe to access, since
 * it should be checked by the caller in advance.
 * The amount of resynced bytes are not necessarily the number of bytes
 * effectively lost....they could be much more...imagine the ring had
 * overwrap multiple times before detecting OUT OF SYNC.
 */
static inline loff_t reader_resync(struct scsc_ring_buffer *rb,
				   loff_t invalid_pos, int *resynced_bytes)
{
	int bytes = 0;
	loff_t sync_pos = rb->head;
	struct scsc_ring_record *candidate = SCSC_GET_REC(rb, invalid_pos);

	*resynced_bytes = 0;
	/* Walking thorugh the ring in search of the sync one byte at time */
	while (invalid_pos != rb->head &&
	       !SCSC_IS_REC_SYNC_VALID(candidate)) {
		invalid_pos = (invalid_pos < rb->last) ?
			(invalid_pos + sizeof(u8)) : 0;
		bytes += sizeof(u8);
		candidate = SCSC_GET_REC(rb, invalid_pos);
	}
	if (invalid_pos == rb->head ||
	    (SCSC_IS_REC_SYNC_VALID(candidate) &&
	     is_record_crc_valid(candidate, invalid_pos))) {
		sync_pos = invalid_pos;
		*resynced_bytes = bytes;
	}
	return sync_pos;
}

/**
 * An Internal API ring function to retrieve into the provided tbuf
 * up to N WHOLE RECORDS starting from *next_rec.
 * It STOPS collecting records if:
 *  - NO MORE RECORDS TO READ: last_read_record record is head
 *  - NO MORE SPACE: on provided destination tbuf to collect
 *    one more WHOLE record
 *  - MAX NUMBER OF REQUIRED RECORDS READ: if max_recs was passed in
 *    as ZERO it means read as much as you can till head is reached.
 *
 * If at start it detects and OUT OF SYNC, so that next_rec is
 * NO MORE pointing to a valid record, it tries to RE-SYNC on next
 * GOOD KNOWN record or to HEAD as last resource and injects into
 * the user buffer an OUT OF SYNC marker record.
 *
 * ASSUMES proper locking and syncing ALREADY inplace...does NOT SLEEP.
 */
size_t read_next_records(struct scsc_ring_buffer *rb, int max_recs,
			 loff_t *last_read_rec, void *tbuf, size_t tsz)
{
	size_t bytes_read = 0, last_read = -1;
	int resynced_bytes = 0, records = 0;
	loff_t next_rec = 0;

	/* Nothing to read...simply return 0 causing reader to exit */
	if (*last_read_rec == rb->head)
		return bytes_read;
	if (!is_ring_read_pos_valid(rb, *last_read_rec)) {
		if (is_ring_pos_safe(rb, *last_read_rec)) {
			/* Try to resync from *last_read_rec INVALID POS */
			next_rec = reader_resync(rb, *last_read_rec,
						 &resynced_bytes);
		} else {
			/* Skip to head...ONLY safe place known in tis case. */
			resynced_bytes = 0;
			next_rec = rb->head;
		}
		bytes_read += mark_out_of_sync(tbuf, tsz, resynced_bytes);
	} else {
		/* next to read....we're surely NOT already at rb->head here */
		next_rec = (*last_read_rec != rb->last) ?
			SCSC_GET_NEXT_SLOT_POS(rb, *last_read_rec) : 0;
	}
	do {
		/* Account for last read */
		last_read = bytes_read;
		bytes_read +=
			_read_one_whole_record(tbuf + bytes_read, rb,
					       next_rec, tsz - bytes_read);
		/* Did a WHOLE record fit into available tbuf ? */
		if (bytes_read != last_read) {
			records++;
			*last_read_rec = next_rec;
			if (*last_read_rec != rb->head)
				next_rec = (next_rec != rb->last) ?
					SCSC_GET_NEXT_SLOT_POS(rb, next_rec) : 0;
		}
	} while (*last_read_rec != rb->head &&
		 last_read != bytes_read &&
		 (!max_recs || records <= max_recs));

	return bytes_read;
}

/**
 * This function returns a static snapshot of the ring that can be used
 * for further processing using usual records operations.
 *
 * It returns a freshly allocated scsc_ring_buffer descriptor whose
 * internal references are exactly the same as the original buffer
 * being snapshot, and with all the sync machinery re-initialized.
 * Even if the current use-case does NOT make any use of spinlocks and
 * waitqueues in the snapshot image, we provide an initialized instance
 * in order to be safe for future (mis-)usage.
 *
 * It also takes care to copy the content of original ring buffer into
 * the new snapshot image (including the spare area) using the provided
 * pre-allocated snap_buf.
 *
 * Assumes ring is already spinlocked.
 *
 * @rb: the original buffer to snapshot
 * @snap_buf: the pre-allocated ring-buffer area to use for copying records
 * @snap_sz: pre-allocated area including spare
 * @snap_name: a human readable descriptor
 */
struct scsc_ring_buffer *scsc_ring_get_snapshot(const struct scsc_ring_buffer *rb,
						void *snap_buf, size_t snap_sz,
						char *snap_name)
{
	struct scsc_ring_buffer *snap_rb = NULL;

	if (!rb || !snap_buf || !snap_name || snap_sz != rb->bsz + rb->ssz)
		return snap_rb;

	/* Here we hold a lock starving writers...try to be quick using
	 * GFP_ATOMIC since scsc_ring_buffer is small enough (144 bytes)
	 */
	snap_rb = kzalloc(sizeof(*rb), GFP_ATOMIC);
	if (!snap_rb)
		return snap_rb;

	/* Copy original buffer content on provided snap_buf */
	if (memcpy(snap_buf, rb->buf, snap_sz)) {
		snap_rb->bsz = rb->bsz;
		snap_rb->ssz = rb->ssz;
		snap_rb->head = rb->head;
		snap_rb->tail = rb->tail;
		snap_rb->last = rb->last;
		snap_rb->written = rb->written;
		snap_rb->records = rb->records;
		snap_rb->wraps = rb->wraps;
		/* this is related to reads so must be re-init */
		snap_rb->oos = 0;
		strncpy(snap_rb->name, snap_name, RNAME_SZ - 1);
		/* Link the copies */
		snap_rb->buf = snap_buf;
		snap_rb->spare = snap_rb->buf + snap_rb->bsz;
		/* cleanup spare */
		memset(snap_rb->spare, 0x00, snap_rb->ssz);
		/* Re-init snapshot copies of sync tools */
		raw_spin_lock_init(&snap_rb->lock);
		init_waitqueue_head(&snap_rb->wq);
	} else {
		kfree(snap_rb);
		snap_rb = NULL;
	}

	return snap_rb;
}

/* Assumes ring is already spinlocked. */
void scsc_ring_truncate(struct scsc_ring_buffer *rb)
{
	rb->head = 0;
	rb->tail = 0;
	rb->records = 0;
	rb->written = 0;
	rb->wraps = 0;
	rb->last = 0;
	memset(rb->buf + rb->head, 0x00, SCSC_RINGREC_SZ);
}

/**
 * alloc_ring_buffer - Allocates and initializes a basic ring buffer,
 * including a basic spare area where to handle strings-splitting when
 * buffer wraps. Basic spinlock/mutex init takes place here too.
 *
 * @bsz: the size of the ring buffer to allocate in bytes
 * @ssz: the size of the spare area to allocate in bytes
 * @name: a name for this ring buffer
 */
struct scsc_ring_buffer __init *alloc_ring_buffer(size_t bsz, size_t ssz,
						  const char *name)
{
	struct scsc_ring_buffer *rb = kmalloc(sizeof(*rb), GFP_KERNEL);

	if (!rb)
		return NULL;
	rb->bsz = bsz;
	rb->ssz = ssz;
#ifndef CONFIG_SCSC_STATIC_RING_SIZE
	rb->buf = kzalloc(rb->bsz + rb->ssz, GFP_KERNEL);
	if (!rb->buf) {
		kfree(rb);
		return NULL;
	}
#else
	rb->buf = a_ring;
#endif
	rb->head = 0;
	rb->tail = 0;
	rb->last = 0;
	rb->written = 0;
	rb->records = 0;
	rb->wraps = 0;
	rb->oos = 0;
	rb->spare = rb->buf + rb->bsz;
	memset(rb->name, 0x00, RNAME_SZ);
	strncpy(rb->name, name, RNAME_SZ - 1);
	raw_spin_lock_init(&rb->lock);
	init_waitqueue_head(&rb->wq);

	return rb;
}

/*
 * free_ring_buffer - Free the ring what else...
 * ...does NOT account for spinlocks existence currently
 *
 * @rb: a pointer to the ring buffer to free
 */
void free_ring_buffer(struct scsc_ring_buffer *rb)
{
	if (!rb)
		return;
#ifndef CONFIG_SCSC_STATIC_RING_SIZE
	kfree(rb->buf);
#endif
	kfree(rb);
}
