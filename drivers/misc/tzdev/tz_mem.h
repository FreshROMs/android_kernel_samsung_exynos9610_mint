/*
 * Copyright (C) 2012-2017, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_MEM_H__
#define __TZ_MEM_H__

#include <linux/mm.h>
#include <linux/pid.h>

#include <tz_cred.h>

typedef void (*tzdev_mem_free_func_t)(void *);

struct tzdev_mem_reg {
	struct pid *pid;
	unsigned long nr_pages;
	struct page **pages;
	tzdev_mem_free_func_t free_func;
	void *free_data;
	unsigned int in_release;
	struct tz_cred cred;
};

int tzdev_mem_init(void);
void tzdev_mem_fini(void);

int tzdev_mem_register_user(void *ptr, unsigned long size, unsigned int write);
int tzdev_mem_release_user(unsigned int id);

int tzdev_mem_register(void *ptr, unsigned long size, unsigned int write,
		tzdev_mem_free_func_t free_func, void *free_data);
int tzdev_mem_release(unsigned int id);
int tzdev_get_user_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages, struct vm_area_struct **vmas);
int tzdev_migrate_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages);
void tzdev_put_user_pages(struct page **pages, unsigned long nr_pages);
void tzdev_decrease_pinned_vm(struct mm_struct *mm, unsigned long nr_pages);
void tzdev_mem_release_panic_handler(void);

#endif /* __TZ_MEM_H__ */
