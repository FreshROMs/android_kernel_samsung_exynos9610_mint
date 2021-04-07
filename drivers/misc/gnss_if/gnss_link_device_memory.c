/*
 * Copyright (C) 2011 Samsung Electronics.
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

#include "gnss_prj.h"
#include "gnss_link_device_memory.h"

void gnss_msq_reset(struct mem_status_queue *msq)
{
	unsigned long flags;
	spin_lock_irqsave(&msq->lock, flags);
	msq->out = msq->in;
	spin_unlock_irqrestore(&msq->lock, flags);
}

/**
 * gnss_msq_get_free_slot
 * @trq : pointer to an instance of mem_status_queue structure
 *
 * Succeeds always by dropping the oldest slot if a "msq" is full.
 */
struct mem_status *gnss_msq_get_free_slot(struct mem_status_queue *msq)
{
	int qsize = MAX_MEM_LOG_CNT;
	int in;
	int out;
	unsigned long flags;
	struct mem_status *stat;

	spin_lock_irqsave(&msq->lock, flags);

	in = msq->in;
	out = msq->out;

	if (circ_get_space(qsize, in, out) < 1) {
		/* Make the oldest slot empty */
		out++;
		msq->out = (out == qsize) ? 0 : out;
	}

	/* Get a free slot */
	stat = &msq->stat[in];

	/* Make it as "data" slot */
	in++;
	msq->in = (in == qsize) ? 0 : in;

	spin_unlock_irqrestore(&msq->lock, flags);

	return stat;
}

struct mem_status *gnss_msq_get_data_slot(struct mem_status_queue *msq)
{
	int qsize = MAX_MEM_LOG_CNT;
	int in;
	int out;
	unsigned long flags;
	struct mem_status *stat;

	spin_lock_irqsave(&msq->lock, flags);

	in = msq->in;
	out = msq->out;

	if (in == out) {
		stat = NULL;
		goto exit;
	}

	/* Get a data slot */
	stat = &msq->stat[out];

	/* Make it "free" slot */
	out++;
	msq->out = (out == qsize) ? 0 : out;

exit:
	spin_unlock_irqrestore(&msq->lock, flags);
	return stat;
}

/**
 * gnss_memcpy16_from_io
 * @to: pointer to "real" memory
 * @from: pointer to IO memory
 * @count: data length in bytes to be copied
 *
 * Copies data from IO memory space to "real" memory space.
 */
void gnss_memcpy16_from_io(const void *to, const void __iomem *from, u32 count)
{
	u16 *d = (u16 *)to;
	u16 *s = (u16 *)from;
	u32 words = count >> 1;
	while (words--)
		*d++ = ioread16(s++);
}

/**
 * gnss_memcpy16_to_io
 * @to: pointer to IO memory
 * @from: pointer to "real" memory
 * @count: data length in bytes to be copied
 *
 * Copies data from "real" memory space to IO memory space.
 */
void gnss_memcpy16_to_io(const void __iomem *to, const void *from, u32 count)
{
	u16 *d = (u16 *)to;
	u16 *s = (u16 *)from;
	u32 words = count >> 1;
	while (words--)
		iowrite16(*s++, d++);
}

/**
 * gnss_memcmp16_to_io
 * @to: pointer to IO memory
 * @from: pointer to "real" memory
 * @count: data length in bytes to be compared
 *
 * Compares data from "real" memory space to IO memory space.
 */
int gnss_memcmp16_to_io(const void __iomem *to, const void *from, u32 count)
{
	u16 *d = (u16 *)to;
	u16 *s = (u16 *)from;
	int words = count >> 1;
	int diff = 0;
	int i;
	u16 d1;
	u16 s1;

	for (i = 0; i < words; i++) {
		d1 = ioread16(d);
		s1 = *s;
		if (d1 != s1) {
			diff++;
			gif_err("ERR! [%d] d:0x%04X != s:0x%04X\n", i, d1, s1);
		}
		d++;
		s++;
	}

	return diff;
}

/**
 * gnss_circ_read16_from_io
 * @dst: start address of the destination buffer
 * @src: start address of the buffer in a circular queue
 * @qsize: size of the circular queue
 * @out: offset to read
 * @len: length of data to be read
 *
 * Should be invoked after checking data length
 */
void gnss_circ_read16_from_io(void *dst, void *src, u32 qsize, u32 out, u32 len)
{
	if ((out + len) <= qsize) {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */
		gnss_memcpy16_from_io(dst, (src + out), len);
	} else {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */
		unsigned len1 = qsize - out;

		/* 1) data start (out) ~ buffer end */
		gnss_memcpy16_from_io(dst, (src + out), len1);

		/* 2) buffer start ~ data end (in - 1) */
		gnss_memcpy16_from_io((dst + len1), src, (len - len1));
	}
}

/**
 * gnss_circ_write16_to_io
 * @dst: pointer to the start of the circular queue
 * @src: pointer to the source
 * @qsize: size of the circular queue
 * @in: offset to write
 * @len: length of data to be written
 *
 * Should be invoked after checking free space
 */
void gnss_circ_write16_to_io(void *dst, void *src, u32 qsize, u32 in, u32 len)
{
	u32 space;

	if ((in + len) < qsize) {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */
		gnss_memcpy16_to_io((dst + in), src, len);
	} else {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */

		/* 1) space start (in) ~ buffer end */
		space = qsize - in;
		gnss_memcpy16_to_io((dst + in), src, ((len > space) ? space : len));

		/* 2) buffer start ~ data end */
		if (len > space)
			gnss_memcpy16_to_io(dst, (src + space), (len - space));
	}
}

/**
 * gnss_copy_circ_to_user
 * @dst: start address of the destination buffer
 * @src: start address of the buffer in a circular queue
 * @qsize: size of the circular queue
 * @out: offset to read
 * @len: length of data to be read
 *
 * Should be invoked after checking data length
 */
int gnss_copy_circ_to_user(void __user *dst, void *src, u32 qsize, u32 out, u32 len)
{
	if ((out + len) <= qsize) {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */
		if (copy_to_user(dst, (src + out), len)) {
			gif_err("ERR! <called by %pf> copy_to_user fail\n",
				CALLER);
			return -EFAULT;
		}
	} else {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */
		unsigned len1 = qsize - out;

		/* 1) data start (out) ~ buffer end */
		if (copy_to_user(dst, (src + out), len1)) {
			gif_err("ERR! <called by %pf> copy_to_user fail\n",
				CALLER);
			return -EFAULT;
		}

		/* 2) buffer start ~ data end (in?) */
		if (copy_to_user((dst + len1), src, (len - len1))) {
			gif_err("ERR! <called by %pf> copy_to_user fail\n",
				CALLER);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * gnss_copy_user_to_circ
 * @dst: pointer to the start of the circular queue
 * @src: pointer to the source
 * @qsize: size of the circular queue
 * @in: offset to write
 * @len: length of data to be written
 *
 * Should be invoked after checking free space
 */
int gnss_copy_user_to_circ(void *dst, void __user *src, u32 qsize, u32 in, u32 len)
{
	u32 space;
	u32 len1;

	if ((in + len) < qsize) {
		/*       (in) ----------- (out)   */
		/* 00 7e      -----------   7f 00 */
		if (copy_from_user((dst + in), src, len)) {
			gif_err("ERR! <called by %pf> copy_from_user fail\n",
				CALLER);
			return -EFAULT;
		}
	} else {
		/* ----- (out)         (in) ----- */
		/* -----   7f 00 00 7e      ----- */

		/* 1) space start (in) ~ buffer end */
		space = qsize - in;
		len1 = (len > space) ? space : len;
		if (copy_from_user((dst + in), src, len1)) {
			gif_err("ERR! <called by %pf> copy_from_user fail\n",
				CALLER);
			return -EFAULT;
		}

		/* 2) buffer start ~ data end */
		if (len > len1) {
			if (copy_from_user(dst, (src + space), (len - len1))) {
				gif_err("ERR! <called by %pf> copy_from_user fail\n",
						CALLER);
				return -EFAULT;
			}
		}
	}

	return 0;
}

/**
 * gnss_capture_mem_dump
 * @ld: pointer to an instance of link_device structure
 * @base: base virtual address to a memory interface medium
 * @size: size of the memory interface medium
 *
 * Captures a dump for a memory interface medium.
 *
 * Returns the pointer to a memory dump buffer.
 */
u8 *gnss_capture_mem_dump(struct link_device *ld, u8 *base, u32 size)
{
	u8 *buff = kzalloc(size, GFP_ATOMIC);
	if (!buff) {
		gif_err("%s: ERR! kzalloc(%d) fail\n", ld->name, size);
		return NULL;
	} else {
		gnss_memcpy16_from_io(buff, base, size);
		return buff;
	}
}

/**
 * gnss_trq_get_free_slot
 * @trq : pointer to an instance of trace_data_queue structure
 *
 * Succeeds always by dropping the oldest slot if a "trq" is full.
 */
struct trace_data *gnss_trq_get_free_slot(struct trace_data_queue *trq)
{
	int qsize = MAX_TRACE_SIZE;
	int in;
	int out;
	unsigned long flags;
	struct trace_data *trd;

	spin_lock_irqsave(&trq->lock, flags);

	in = trq->in;
	out = trq->out;

	/* The oldest slot can be dropped. */
	if (circ_get_space(qsize, in, out) < 1) {
		/* Free the data buffer in the oldest slot */
		trd = &trq->trd[out];
		kfree(trd->data);

		/* Make the oldest slot empty */
		out++;
		trq->out = (out == qsize) ? 0 : out;
	}

	/* Get a free slot and make it occupied */
	trd = &trq->trd[in++];
	trq->in = (in == qsize) ? 0 : in;

	spin_unlock_irqrestore(&trq->lock, flags);

	memset(trd, 0, sizeof(struct trace_data));

	return trd;
}

struct trace_data *gnss_trq_get_data_slot(struct trace_data_queue *trq)
{
	int qsize = MAX_TRACE_SIZE;
	int in;
	int out;
	unsigned long flags;
	struct trace_data *trd;

	spin_lock_irqsave(&trq->lock, flags);

	in = trq->in;
	out = trq->out;

	if (circ_get_usage(qsize, in, out) < 1) {
		spin_unlock_irqrestore(&trq->lock, flags);
		return NULL;
	}

	/* Get a data slot and make it empty */
	trd = &trq->trd[out++];
	trq->out = (out == qsize) ? 0 : out;

	spin_unlock_irqrestore(&trq->lock, flags);

	return trd;
}
