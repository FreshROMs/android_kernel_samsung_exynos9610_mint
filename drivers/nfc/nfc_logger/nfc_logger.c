/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * NFC logger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

#include "nfc_logger.h"

#define BUF_SIZE	SZ_256K
#define MAX_STR_LEN	128
#define PROC_FILE_NAME	"nfclog"
#define LOG_PREFIX	"sec-nfc"

static char log_buf[BUF_SIZE];
static unsigned int g_curpos;
static int is_nfc_logger_init;
static int is_buf_full;
static int log_max_count = -1;

/* set max log count, if count is -1, no limit */
void nfc_logger_set_max_count(int count)
{
	log_max_count = count;
}

void nfc_logger_print(const char *fmt, ...)
{
	int len;
	va_list args;
	char buf[MAX_STR_LEN + 16];
	u64 time;
	unsigned long nsec;
	volatile unsigned int curpos;

	if (!is_nfc_logger_init)
		return;

	if (log_max_count == 0)
		return;
	else if (log_max_count > 0)
		log_max_count--;

	time = local_clock();
	nsec = do_div(time, 1000000000);
	len = snprintf(buf, sizeof(buf), "[%5lu.%06ld] ", (unsigned long)time, nsec / 1000);

	va_start(args, fmt);
	len += vsnprintf(buf + len, MAX_STR_LEN, fmt, args);
	va_end(args);

	curpos = g_curpos; 
	if (curpos + len >= BUF_SIZE) { 
		g_curpos = curpos = 0; 
		is_buf_full = 1;
	}
	memcpy(log_buf + curpos, buf, len);
	g_curpos += len;
}

void nfc_print_hex_dump(void *buf, void *pref, size_t size)
{
	uint8_t *ptr = buf;
	uint32_t i;
	char tmp[128] = {0x0, };
	char *ptmp = tmp;
	int len;

	if (!is_nfc_logger_init)
		return;

	if (log_max_count == 0)
		return;
	else if (log_max_count > 0)
		log_max_count--;

	for (i = 0; i < size; i++) {
		len = snprintf(ptmp, 4, "%02x ", *ptr++);
		ptmp = ptmp + len;
		if (((i+1)%16) == 0) {
			nfc_logger_print("%s%s\n", pref, tmp);
			ptmp = tmp;
		}
	}

	if (i % 16) {
		len = ptmp - tmp;
		tmp[len] = 0x0;
		nfc_logger_print("%s%s\n", pref, tmp);
	}
}

static ssize_t nfc_logger_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
	size_t size;
	volatile unsigned int curpos = g_curpos;

	if (is_buf_full || BUF_SIZE <= curpos)
		size = BUF_SIZE;
	else
		size = (size_t)curpos;

	if (pos >= size)
		return 0;

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, log_buf + pos, count))
		return -EFAULT;

	*offset += count;

	return count;
}

static const struct file_operations nfc_logger_ops = {
	.owner = THIS_MODULE,
	.read = nfc_logger_read,
};

int nfc_logger_init(void)
{
	struct proc_dir_entry *entry;

	if (is_nfc_logger_init)
		return 0;

	entry = proc_create(PROC_FILE_NAME, 0444, NULL, &nfc_logger_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, BUF_SIZE);
	is_nfc_logger_init = 1;
	nfc_logger_print("nfc logger init ok\n");

	return 0;
}
