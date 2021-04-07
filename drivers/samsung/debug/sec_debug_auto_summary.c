/*
 * sec_debug_auto_summary.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/memblock.h>
#include <linux/sec_debug.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/sections.h>

#define AUTO_SUMMARY_SIZE 0xf3c
#define AUTO_SUMMARY_MAGIC 0xcafecafe
#define AUTO_SUMMARY_TAIL_MAGIC 0x00c0ffee
#define AUTO_SUMMARY_EDATA_MAGIC 0x43218765

enum {
	PRIO_LV0 = 0,
	PRIO_LV1,
	PRIO_LV2,
	PRIO_LV3,
	PRIO_LV4,
	PRIO_LV5,
	PRIO_LV6,
	PRIO_LV7,
	PRIO_LV8,
	PRIO_LV9
};

#define SEC_DEBUG_AUTO_COMM_BUF_SIZE 10

enum sec_debug_FREQ_INFO {
	FREQ_INFO_CLD0 = 0,
	FREQ_INFO_CLD1,
	FREQ_INFO_INT,
	FREQ_INFO_MIF,
	FREQ_INFO_MAX,
};

struct sec_debug_auto_comm_buf {
	int reserved_0;
	int reserved_1;
	int reserved_2;
	unsigned int offset;
	char buf[SZ_4K];
};

struct sec_debug_auto_comm_log_idx {
	atomic_t logging_entry;
	atomic_t logging_disable;
	atomic_t logging_count;
};
	
static struct sec_debug_auto_comm_log_idx ac_idx[SEC_DEBUG_AUTO_COMM_BUF_SIZE];

struct sec_debug_auto_comm_freq_info {
	int old_freq;
	int new_freq;
	u64 time_stamp;
	int en;
	u64 last_freq_info;
};

struct sec_debug_auto_comm_extra_data {
	unsigned long magic;
	unsigned long data[10];
};

struct sec_debug_auto_summary {
	int header_magic;
	int fault_flag;
	int lv5_log_cnt;
	u64 lv5_log_order;
	int order_map_cnt;
	int order_map[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
	struct sec_debug_auto_comm_buf auto_comm_buf[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
	struct sec_debug_auto_comm_freq_info freq_info[FREQ_INFO_MAX];

	/* for code diff */
	u64 pa_text;
	u64 pa_start_rodata;
	int tail_magic;

	struct sec_debug_auto_comm_extra_data edata;
};

static struct sec_debug_auto_summary *auto_summary_info;
static char *auto_summary_buf;

struct auto_summary_log_map {
	char prio_level;
	char max_count;
};

static const struct auto_summary_log_map init_data[SEC_DEBUG_AUTO_COMM_BUF_SIZE] = {
	{PRIO_LV0, 0},
	{PRIO_LV5, 8},
	{PRIO_LV9, 0},
	{PRIO_LV5, 0},
	{PRIO_LV5, 0},
	{PRIO_LV1, 7},
	{PRIO_LV2, 8},
	{PRIO_LV5, 0},
	{PRIO_LV5, 8},
	{PRIO_LV0, 0}
};

void sec_debug_auto_summary_log_disable(int type)
{
	atomic_inc(&(ac_idx[type].logging_disable));
}

void sec_debug_auto_summary_log_once(int type)
{
	if (atomic64_read(&(ac_idx[type].logging_entry)))
		sec_debug_auto_summary_log_disable(type);
	else
		atomic_inc(&(ac_idx[type].logging_entry));
}

static inline void sec_debug_hook_auto_comm_lastfreq(int type,
		int old_freq, int new_freq, u64 time, int en)
{
	if (type < FREQ_INFO_MAX) {
		auto_summary_info->freq_info[type].old_freq = old_freq;
		auto_summary_info->freq_info[type].new_freq = new_freq;
		auto_summary_info->freq_info[type].time_stamp = time;
		auto_summary_info->freq_info[type].en = en;
	}
}

static inline void sec_debug_hook_auto_comm(int type, const char *buf, size_t size)
{
	struct sec_debug_auto_comm_buf *p = &auto_summary_info->auto_comm_buf[type];
	unsigned int offset = p->offset;

	if (atomic64_read(&(ac_idx[type].logging_disable)))
		return;

	if (offset + size > SZ_4K)
		return;

	if (init_data[type].max_count &&
	    (atomic64_read(&(ac_idx[type].logging_count)) > init_data[type].max_count))
		return;

	if (!(auto_summary_info->fault_flag & 1 << type)) {
		auto_summary_info->fault_flag |= 1 << type;
		if (init_data[type].prio_level == PRIO_LV5) {
			auto_summary_info->lv5_log_order |= type << auto_summary_info->lv5_log_cnt * 4;
			auto_summary_info->lv5_log_cnt++;
		}
		auto_summary_info->order_map[auto_summary_info->order_map_cnt++] = type;
	}

	atomic_inc(&(ac_idx[type].logging_count));

	memcpy(p->buf + offset, buf, size);
	p->offset += (unsigned int)size;
}

extern ulong entry_dbg_saved_data[];

static void sec_auto_summary_init_print_buf(unsigned long base)
{
	auto_summary_buf = (char *)base;
	auto_summary_info = (struct sec_debug_auto_summary *)(base + SZ_4K);

	memset(auto_summary_info, 0, sizeof(struct sec_debug_auto_summary));

	auto_summary_info->pa_text = virt_to_phys(_text);
	auto_summary_info->pa_start_rodata = virt_to_phys(__start_rodata);

	auto_summary_info->edata.data[0] = virt_to_phys(entry_dbg_saved_data);

	register_set_auto_comm_buf(sec_debug_hook_auto_comm);
	register_set_auto_comm_lastfreq(sec_debug_hook_auto_comm_lastfreq);
}

static unsigned long sas_size;
static unsigned long sas_base;

static void * __init sec_auto_summary_remap(unsigned long size, unsigned long base)
{
	unsigned long i;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size;
	struct page *page;
	struct page **pages;
	void *addr;

	pr_info("%s: base: 0x%lx size: 0x%lx\n", __func__, base, size);

	page_size = (size + PAGE_SIZE - 1) / PAGE_SIZE; 

	pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: failed to allocate pages\n", __func__);

		return NULL;
	}

	page = phys_to_page(base);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	addr = vm_map_ram(pages, page_size, -1, prot);
	if (!addr) {
		pr_err("%s: failed to mapping between virt and phys\n", __func__);
		kfree(pages);
		return NULL;
	}

	pr_info("%s: virt: 0x%p\n", __func__, addr);

	kfree(pages);

	return addr;
}

static int __init sec_auto_summary_log_setup(char *str)
{
	unsigned long size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);

		return -1;
	}

	if (size < sizeof(struct sec_debug_auto_summary)) {
		pr_err("%s: not enough size for auto summary\n", __func__);

		return -1;
	}

	sas_base = base;
	sas_size = size;

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(base, size) ||
	    memblock_reserve(base, size)) {
#else
	if (reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		/* size is not match with -size and size + sizeof(...) */
		pr_err("%s: failed to reserve size:0x%lx at base 0x%lx\n",
		       __func__, size, base);

		sas_base = 0;
		sas_size = 0;

		return -1;
	}

	return 1;
}
__setup("auto_comment_log=", sec_auto_summary_log_setup);

static int __init sec_auto_summary_base_init(void)
{
	void *addr;

	if (!sas_size) {
		pr_err("%s: failed to get size of auto summary\n", __func__);

		return -1;
	}

	addr = sec_auto_summary_remap(sas_size, sas_base);
	if (!addr) {
		pr_err("%s: failed to remap size:0x%lx at base 0x%lx\n",
				__func__, sas_size, sas_base);
		return -1;
	}

	sec_auto_summary_init_print_buf((unsigned long)addr);

	return 0;
}
early_initcall(sec_auto_summary_base_init);

static int __init sec_auto_summary_log_init(void)
{
	if (auto_summary_info) {
		auto_summary_info->header_magic = AUTO_SUMMARY_MAGIC;
		auto_summary_info->tail_magic = AUTO_SUMMARY_TAIL_MAGIC;
		auto_summary_info->edata.magic = AUTO_SUMMARY_EDATA_MAGIC;
	}

	pr_debug("%s, done.\n", __func__);

	return 0;
}
subsys_initcall(sec_auto_summary_log_init);

static ssize_t sec_reset_auto_summary_proc_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!auto_summary_buf) {
		pr_err("%s : buffer is null\n", __func__);
		return -ENODEV;
	}

	if (reset_reason >= RR_R && reset_reason <= RR_N) {
		pr_err("%s : reset_reason %d\n", __func__, reset_reason);
		return -ENOENT;
	}

	if (pos >= AUTO_SUMMARY_SIZE) {
		pr_err("%s : pos 0x%llx\n", __func__, pos);
		return -ENOENT;
	}

	count = min(len, (size_t)(AUTO_SUMMARY_SIZE - pos));
	if (copy_to_user(buf, auto_summary_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_reset_auto_summary_proc_fops = {
	.owner = THIS_MODULE,
	.read = sec_reset_auto_summary_proc_read,
};

static int __init sec_debug_auto_summary_init(void)
{
	struct proc_dir_entry *entry;
	int i;

	for (i = 0; i < SEC_DEBUG_AUTO_COMM_BUF_SIZE; i++) {
		atomic_set(&(ac_idx[i].logging_entry), 0);
		atomic_set(&(ac_idx[i].logging_disable), 0);
		atomic_set(&(ac_idx[i].logging_count), 0);
	}

	entry = proc_create("auto_comment", S_IWUGO, NULL,
			    &sec_reset_auto_summary_proc_fops);

	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, AUTO_SUMMARY_SIZE);
	return 0;
}

device_initcall(sec_debug_auto_summary_init);
