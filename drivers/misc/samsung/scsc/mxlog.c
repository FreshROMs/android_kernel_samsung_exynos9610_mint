/*****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include "scsc_mx_impl.h"
#include "mxmgmt_transport.h"
#include "mxlog_transport.h"
#include "fwhdr.h"
#include "mxlog.h"

/*
 * Receive handler for messages from the FW along the maxwell management transport
 */
static inline void mxlog_phase4_message_handler(const void *message,
						size_t length, u32 level,
						void *data)
{
	unsigned char *buf = (unsigned char *)message;

	SCSC_TAG_LVL(MX_FW, level, SCSC_PREFIX"%d: %s\n", (int)length, buf);
}

/**
 * This function is used to parse a NULL terminated format string
 * and report on the provided output bitmaps smap/lmap which args
 * are 'long' and which are signed..
 *
 * We will care only about length and specifier fields
 *
 * %[flags][width][.precision][length]specifier
 *
 * and since flags width and .precision are represented
 * by NON chars, we will grossly compare simply against an 'A',
 * because we are NOT trying to make a full sanity check here BUT only
 * to search for long and signed values to provide the proper cast.
 *
 * Supporting:
 *	- ESCAPES %%ld
 *
 *	- %x %X %d %ld %lld %i %li %lli %u %lu %llu %hd %hhd %hu %hhu
 *
 * NOT supporting:
 *	- %s -> MARKED AS UNSUPPORTED
 */
static inline void build_len_sign_maps(char *fmt, u32 *smap, u32 *lmap,
				       u32 *strmap)
{
	u32 p = 0;
	char *s = fmt;
	bool escaping = false;

	if (!s)
		return;
	for (; *s != '\0'; ++s) {
		/* Skip any escaped fmtstring like %%d and move on */
		if (escaping) {
			if (*s == ' ')
				escaping = false;
			continue;
		}
		if (*s != '%')
			continue;
		/* Start escape seq ... */
		if (*(s + 1) == '%') {
			escaping = true;
			continue;
		}
		/* skip [flags][width][.precision] if any */
		for (; *++s < 'A';)
			;
		if (*s == 'l') {
			*lmap |= (1 << p);
			/* %lld ? skip */
			if (*++s == 'l')
				s++;
		} else if (*s == 'h') {
			/* just skip h modifiers */
			/* hhd ? */
			if (*++s == 'h')
				s++;
		}
		if (*s == 'd' || *s == 'i')
			*smap |= (1 << p);
		else if (*s == 's')
			*strmap |= (1 << p);
		p++;
	}
}

/**
 * The binary protocol described at:
 *
 * http://wiki/Maxwell_common_firmware/Mxlog#Phase_5_:_string_decoded_on_the_host
 *
 * states that we'd receive the following record content on each mxlog
 * message from FW, where:
 *
 *  - each element is a 32bit word
 *  - 1st element is a record header
 *  - len = number of elements following the first element
 *
 *  |   1st         |  2nd    |  3rd    |   4th | 5th	| 6th
 *  -----------------------------------------------------------
 *  | sync|lvl|len || tstamp || offset || arg1 || arg2 || arg3.
 *  -----------------------------------------------------------
 *                  |  e l o g m s g    |
 *
 * BUT NOTE THAT: here we DO NOT receive 1st header element BUT
 * instead we got:
 * @message: pointer to 2nd element
 * @length: in bytes of the message (so starting from 2nd element) and
 *	    including tstamp and offset elements: we must calculate
 *	    num_args accordingly.
 * @level: the debug level already remapped from FW to Kernel namespace
 */
static inline void mxlog_phase5_message_handler(const void *message,
						size_t length, u32 level,
						void *data)
{
	struct mxlog  *mxlog = (struct mxlog *)data;
	struct mxlog_event_log_msg *elogmsg =
		(struct mxlog_event_log_msg *)message;

	if (length < MINIMUM_MXLOG_MSG_LEN_BYTES)
		return;
	if (mxlog && elogmsg) {
		int num_args = 0;
		char spare[MAX_SPARE_FMT + TSTAMP_LEN] = {};
		char *fmt = NULL;
		size_t fmt_sz = 0;
		u32 smap = 0, lmap = 0, strmap = 0;
		u32 *args = NULL;

		/* Check OFFSET sanity... beware of FW guys :D ! */
		if (elogmsg->offset >= MXLS_SZ(mxlog)) {
			SCSC_TAG_ERR(MX_FW,
				     "Received fmtstr OFFSET(%d) is OUT OF range(%zd)...skip..\n",
				     elogmsg->offset, MXLS_SZ(mxlog));
			return;
		}
		args = (u32 *)(elogmsg + 1);
		num_args =
			(length - MINIMUM_MXLOG_MSG_LEN_BYTES) /
			MXLOG_ELEMENT_SIZE;
		fmt = (char  *)(MXLS_DATA(mxlog) + elogmsg->offset);
		/* Avoid being fooled by a NON NULL-terminated strings too ! */
		fmt_sz = strnlen(fmt, MXLS_SZ(mxlog) - elogmsg->offset);
		if (fmt_sz >= MAX_SPARE_FMT - 1) {
			SCSC_TAG_ERR(MX_FW,
				     "UNSUPPORTED message length %zd ... truncated.\n",
				     fmt_sz);
			fmt_sz = MAX_SPARE_FMT - 2;
		}
		/* Pre-Process fmt string to be able to do proper casting */
		if (num_args)
			build_len_sign_maps(fmt, &smap, &lmap, &strmap);

		/* Add FW provided tstamp on front and proper \n at
		 * the end when needed
		 */
		snprintf(spare, MAX_SPARE_FMT + TSTAMP_LEN - 2, SCSC_PREFIX"%08X %s%c",
			 elogmsg->timestamp, fmt,
			 (fmt[fmt_sz] != '\n') ? '\n' : '\0');
		fmt = spare;

		switch (num_args) {
		case 0:
			SCSC_TAG_LVL(MX_FW, level, fmt);
			break;
		case 1:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 2:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 3:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 4:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[3], 3, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 5:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[3], 3, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[4], 4, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 6:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[3], 3, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[4], 4, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[5], 5, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 7:
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[3], 3, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[4], 4, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[5], 5, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[6], 6, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		case 8:
		default:
			if (num_args > MAX_MX_LOG_ARGS)
				SCSC_TAG_ERR(MX_FW,
					     "MXLOG: Too many args:%d ... print only first %d\n",
					     num_args, MAX_MX_LOG_ARGS);
			SCSC_TAG_LVL(MX_FW, level, fmt,
				     MXLOG_CAST(args[0], 0, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[1], 1, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[2], 2, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[3], 3, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[4], 4, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[5], 5, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[6], 6, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
				     MXLOG_CAST(args[7], 7, smap, lmap, strmap,
						MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
			break;
		}
	}
}

/* A generic message handler to multiplex between phases */
static void mxlog_message_handler(u8 phase, const void *message,
				  size_t length, u32 level, void *data)
{
	struct mxlog  *mxlog = (struct mxlog *)data;

	if (!mxlog) {
		SCSC_TAG_ERR(MX_FW, "Missing MXLOG reference.\n");
		return;
	}

	switch (phase) {
	case MX_LOG_PHASE_4:
		mxlog_phase4_message_handler(message, length, level, data);
		break;
	case MX_LOG_PHASE_5:
		if (mxlog->logstrings)
			mxlog_phase5_message_handler(message, length,
						     level, data);
		else
			SCSC_TAG_ERR(MX_FW,
				     "Missing LogStrings...dropping incoming PHASE5 message !\n");
		break;
	default:
		SCSC_TAG_ERR(MX_FW,
			     "MXLOG Unsupported phase %d ... dropping message !\n",
			     phase);
		break;
	}
}

static int mxlog_header_parser(u32 header, u8 *phase,
			       u8 *level, u32 *num_bytes)
{
	u32 fw2kern_map[] = {
		0, /* 0 MX_ERROR --> 0 KERN_EMERG .. it's panic.*/
		4, /* 1 MX_WARN --> 4 KERN_WARNING */
		5, /* 2 MX_MAJOR --> 5 KERN_NOTICE */
		6, /* 3 MX_MINOR --> 6 KERN_INFO */
		7, /* 4 MX_DETAIL --> 7 KERN_DEBUG */
	};
	u16 sync = ((header & 0xFFFF0000) >> 16);

	switch (sync) {
	case SYNC_VALUE_PHASE_4:
		*phase = MX_LOG_PHASE_4;
		/* len() field represent number of chars bytes */
		*num_bytes = header & 0x000000FF;
		break;
	case SYNC_VALUE_PHASE_5:
		*phase = MX_LOG_PHASE_5;
		/* len() field represent number of 4 bytes words */
		*num_bytes = (header & 0x000000FF) * 4;
		break;
	default:
		return -1;
	}
	/* Remap FW debug levels to KERN debug levels domain */
	*level = (header & 0x0000FF00) >> 8;
	if (*level < ARRAY_SIZE(fw2kern_map)) {
		*level = fw2kern_map[*level];
	} else {
		SCSC_TAG_ERR(MX_FW,
			     "UNKNOWN MX debug level %d ... marking as MX_DETAIL.\n",
			     *level);
		*level = fw2kern_map[ARRAY_SIZE(fw2kern_map) - 1];
	}

	return 0;
}

void mxlog_init(struct mxlog *mxlog, struct scsc_mx *mx, char *fw_build_id)
{
	int ret = 0;

	mxlog->mx = mx;
	mxlog->index = 0;
	mxlog->logstrings = NULL;

	/* File is in f/w profile directory */
	ret = mx140_file_request_debug_conf(mx,
		(const struct firmware **)&mxlog->logstrings,
		MX_LOG_LOGSTRINGS_PATH);

	if (!ret && mxlog->logstrings && mxlog->logstrings->data) {
		SCSC_TAG_INFO(MX_FW, "Loaded %zd bytes of log-strings from %s\n",
			      mxlog->logstrings->size, MX_LOG_LOGSTRINGS_PATH);
		if (fw_build_id && mxlog->logstrings->data[0] != 0x00 &&
		    mxlog->logstrings->size >= FW_BUILD_ID_SZ) {
			SCSC_TAG_INFO(MX_FW, "Log-strings is versioned...checking against fw_build_id.\n");
			if (strncmp(fw_build_id, mxlog->logstrings->data, FW_BUILD_ID_SZ)) {
				char found[FW_BUILD_ID_SZ] = {};

				/**
				 * NULL-terminate it just in case we fetched
				 * never-ending garbage.
				 */
				strncpy(found, mxlog->logstrings->data,
					FW_BUILD_ID_SZ - 1);
				SCSC_TAG_WARNING(MX_FW,
						"--> Log-strings VERSION MISMATCH !!!\n");
				SCSC_TAG_WARNING(MX_FW,
						"--> Expected: |%s|\n", fw_build_id);
				SCSC_TAG_WARNING(MX_FW,
						"--> FOUND: |%s|\n", found);
				SCSC_TAG_WARNING(MX_FW,
						"As a consequence the following mxlog debug messages could be corrupted.\n");
				SCSC_TAG_WARNING(MX_FW,
						"The whole firmware package should be pushed to device when updating (not only the mx140.bin).\n");
			}
		} else {
			SCSC_TAG_INFO(MX_FW, "Log-strings is not versioned.\n");
		}
	} else {
		SCSC_TAG_ERR(MX_FW, "Failed to read %s needed by MXlog Phase 5\n",
			     MX_LOG_LOGSTRINGS_PATH);
	}
	/* Registering a generic channel handler */
	mxlog_transport_register_channel_handler(scsc_mx_get_mxlog_transport(mx),
						 &mxlog_header_parser,
						 &mxlog_message_handler, mxlog);
}

void mxlog_release(struct mxlog *mxlog)
{
	mxlog_transport_register_channel_handler(scsc_mx_get_mxlog_transport(mxlog->mx),
						 NULL, NULL, NULL);
	if (mxlog->logstrings)
		mx140_release_file(mxlog->mx, mxlog->logstrings);
	mxlog->logstrings = NULL;
}

