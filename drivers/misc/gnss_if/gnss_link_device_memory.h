/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GNSS_LINK_DEVICE_MEMORY_H__
#define __GNSS_LINK_DEVICE_MEMORY_H__

#include <linux/spinlock.h>

#include "gnss_prj.h"

#define MAX_TIMEOUT_CNT		1000

#define MAX_SKB_TXQ_DEPTH	1024

#define MAX_RETRY_CNT   3

enum circ_ptr_type {
	HEAD,
	TAIL,
};

static inline bool circ_valid(u32 qsize, u32 in, u32 out)
{
	if (in >= qsize)
		return false;

	if (out >= qsize)
		return false;

	return true;
}

static inline u32 circ_get_space(u32 qsize, u32 in, u32 out)
{
	return (in < out) ? (out - in - 1) : (qsize + out - in - 1);
}

static inline u32 circ_get_usage(u32 qsize, u32 in, u32 out)
{
	return (in >= out) ? (in - out) : (qsize - out + in);
}

static inline u32 circ_new_pointer(u32 qsize, u32 p, u32 len)
{
	p += len;
	return (p < qsize) ? p : (p - qsize);
}

/**
 * circ_read
 * @dst: start address of the destination buffer
 * @src: start address of the buffer in a circular queue
 * @qsize: size of the circular queue
 * @out: offset to read
 * @len: length of data to be read
 *
 * Should be invoked after checking data length
 */
static inline void circ_read(void *dst, void *src, u32 qsize, u32 out, u32 len)
{
	unsigned len1;

	if ((out + len) <= qsize) {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */
		memcpy(dst, (src + out), len);
	} else {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */

		/* 1) data start (out) ~ buffer end */
		len1 = qsize - out;
		memcpy(dst, (src + out), len1);

		/* 2) buffer start ~ data end (in?) */
		memcpy((dst + len1), src, (len - len1));
	}
}

/**
 * circ_write
 * @dst: pointer to the start of the circular queue
 * @src: pointer to the source
 * @qsize: size of the circular queue
 * @in: offset to write
 * @len: length of data to be written
 *
 * Should be invoked after checking free space
 */
static inline void circ_write(void *dst, void *src, u32 qsize, u32 in, u32 len)
{
	u32 space;

	if ((in + len) <= qsize) {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */
		memcpy((dst + in), src, len);
	} else {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */

		/* 1) space start (in) ~ buffer end */
		space = qsize - in;
		memcpy((dst + in), src, space);

		/* 2) buffer start ~ data end */
		memcpy(dst, (src + space), (len - space));
	}
}

/**
 * circ_dir
 * @dir: communication direction (enum direction)
 *
 * Returns the direction of a circular queue
 *
 */
static const inline char *circ_dir(enum direction dir)
{
	if (dir == TX)
		return "TXQ";
	else
		return "RXQ";
}

/**
 * circ_ptr
 * @ptr: circular queue pointer (enum circ_ptr_type)
 *
 * Returns the name of a circular queue pointer
 *
 */
static const inline char *circ_ptr(enum circ_ptr_type ptr)
{
	if (ptr == HEAD)
		return "head";
	else
		return "tail";
}

void gnss_memcpy16_from_io(const void *to, const void __iomem *from, u32 count);
void gnss_memcpy16_to_io(const void __iomem *to, const void *from, u32 count);
int gnss_memcmp16_to_io(const void __iomem *to, const void *from, u32 count);
void gnss_circ_read16_from_io(void *dst, void *src, u32 qsize, u32 out, u32 len);
void gnss_circ_write16_to_io(void *dst, void *src, u32 qsize, u32 in, u32 len);
int gnss_copy_circ_to_user(void __user *dst, void *src, u32 qsize, u32 out, u32 len);
int gnss_copy_user_to_circ(void *dst, void __user *src, u32 qsize, u32 in, u32 len);

#define MAX_MEM_LOG_CNT	8192
#define MAX_TRACE_SIZE	1024

struct mem_status {
	/* Timestamp */
	struct timespec ts;

	/* Direction (TX or RX) */
	enum direction dir;

	/* The status of memory interface at the time */
	u32 head[MAX_DIR];
	u32 tail[MAX_DIR];

	u16 int2ap;
	u16 int2gnss;
};

struct mem_status_queue {
	spinlock_t lock;
	u32 in;
	u32 out;
	struct mem_status stat[MAX_MEM_LOG_CNT];
};

struct circ_status {
	u8 *buff;
	u32 qsize;	/* the size of a circular buffer */
	u32 in;
	u32 out;
	u32 size;	/* the size of free space or received data */
};

struct trace_data {
	struct timespec ts;
	struct circ_status circ_stat;
	u8 *data;
	u32 size;
};

struct trace_data_queue {
	spinlock_t lock;
	u32 in;
	u32 out;
	struct trace_data trd[MAX_TRACE_SIZE];
};

void gnss_msq_reset(struct mem_status_queue *msq);
struct mem_status *gnss_msq_get_free_slot(struct mem_status_queue *msq);
struct mem_status *gnss_msq_get_data_slot(struct mem_status_queue *msq);

u8 *gnss_capture_mem_dump(struct link_device *ld, u8 *base, u32 size);
struct trace_data *gnss_trq_get_free_slot(struct trace_data_queue *trq);
struct trace_data *gnss_trq_get_data_slot(struct trace_data_queue *trq);

#endif
