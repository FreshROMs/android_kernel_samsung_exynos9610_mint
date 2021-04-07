/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include "include/defex_debug.h"
#include "include/defex_internal.h"
#include "include/defex_rules.h"

#ifdef DEFEX_USE_PACKED_RULES
#include "defex_packed_rules.inc"
#endif

static struct kset *defex_kset;

static void parse_static_rules(const struct static_rule *rules, size_t max_len, int rules_number)
{
	int i;
	size_t count;
	const char *current_rule;
#if (defined(DEFEX_PERMISSIVE_PED) || defined(DEFEX_PERMISSIVE_SP))
	static const char permissive[2] = "2";
#endif /* DEFEX_PERMISSIVE_**/

	for (i = 0; i < rules_number; i++) {
		count = strnlen(rules[i].rule, max_len);
		current_rule = rules[i].rule;
		switch (rules[i].feature_type) {
#ifdef DEFEX_PED_ENABLE
		case feature_ped_status:
#ifdef DEFEX_PERMISSIVE_PED
			current_rule = permissive;
#endif /* DEFEX_PERMISSIVE_PED */
			task_defex_privesc_store_status(global_privesc_obj, NULL, current_rule, count);
			break;
#endif /* DEFEX_PED_ENABLE */
#ifdef DEFEX_SAFEPLACE_ENABLE
		case feature_safeplace_status:
#ifdef DEFEX_PERMISSIVE_SP
			current_rule = permissive;
#endif /* DEFEX_PERMISSIVE_SP */
			safeplace_status_store(global_safeplace_obj, NULL, current_rule, count);
			break;
#endif /* DEFEX_SAFEPLACE_ENABLE */
		}
	}

	printk(KERN_INFO "DEFEX_LSM started");
}

#ifdef DEFEX_USE_PACKED_RULES
struct rule_item_struct *lookup_dir(struct rule_item_struct *base, const char *name, int l)
{
	struct rule_item_struct *item = NULL;
	unsigned int offset;

	if (!base || !base->next_level)
		return item;
	item = GET_ITEM_PTR(base->next_level);
	do {
		if (item->size == l && !memcmp(name, item->name, l)) return item;
		offset = item->next_file;
		item = GET_ITEM_PTR(offset);
	} while(offset);
	return NULL;
}

int lookup_tree(const char *file_path, int attribute)
{
	const char *ptr, *next_separator;
	struct rule_item_struct *base, *cur_item = NULL;
	int l;

	if (!file_path || *file_path != '/')
		return 0;
	base = (struct rule_item_struct *)defex_packed_rules;
	ptr = file_path + 1;
	do {
		next_separator = strchr(ptr, '/');
		if (!next_separator)
			l = strlen(ptr);
		else
			l = next_separator - ptr;
		if (!l)
			return 0;
		cur_item = lookup_dir(base, ptr, l);
		if (!cur_item)
			break;
		if (cur_item->feature_type & attribute)
			return 1;
		base = cur_item;
		ptr += l;
		if (next_separator)
			ptr++;
	} while(*ptr);
	return 0;
}
#endif /* DEFEX_USE_PACKED_RULES */

int rules_lookup(const struct path *dpath, int attribute)
{
	int ret = 0;
#if (defined(DEFEX_SAFEPLACE_ENABLE) || defined(DEFEX_PED_ENABLE))
	char *target_file, *buff;
#ifndef DEFEX_USE_PACKED_RULES
	int i, count, end;
	const struct static_rule *current_rule;
#endif
	buff = kzalloc(PATH_MAX, GFP_ATOMIC);
	if (!buff)
		return ret;
	target_file = d_path(dpath, buff, PATH_MAX);
	if (IS_ERR(target_file)) {
		kfree(buff);
		return ret;
	}
#ifdef DEFEX_USE_PACKED_RULES
	ret = lookup_tree(target_file, attribute);
#else
	for (i = 0; i < static_rule_count; i++) {
		current_rule = &defex_static_rules[i];
		if (current_rule->feature_type == attribute) {
			end = strnlen(current_rule->rule, STATIC_RULES_MAX_STR);
			if(current_rule->rule[end - 1] == '/') {
				count = end;
			} else {
				count = strnlen(target_file, STATIC_RULES_MAX_STR);
				if (end > count) count = end;
			}
			if (!strncmp(current_rule->rule, target_file, count)) {
				ret = 1;
				break;
			}
		}
	}
#endif /* DEFEX_USE_PACKED_RULES */
	kfree(buff);
#endif
	return ret;
}

int defex_init_sysfs(void)
{
	defex_kset = kset_create_and_add("defex", NULL, NULL);
	if (!defex_kset)
		return -ENOMEM;

#if defined(DEFEX_DEBUG_ENABLE) && defined(DEFEX_SYSFS_ENABLE)
	if (defex_create_debug(defex_kset) != DEFEX_OK)
		goto kset_error;
#endif /* DEFEX_DEBUG_ENABLE && DEFEX_SYSFS_ENABLE */

#ifdef DEFEX_PED_ENABLE
	global_privesc_obj = task_defex_create_privesc_obj(defex_kset);
	if (!global_privesc_obj)
		goto privesc_error;
#endif /* DEFEX_PED_ENABLE */

#ifdef DEFEX_SAFEPLACE_ENABLE
	global_safeplace_obj = task_defex_create_safeplace_obj(defex_kset);
	if (!global_safeplace_obj)
		goto safeplace_error;
#endif /* DEFEX_SAFEPLACE_ENABLE */

	parse_static_rules(defex_static_rules, STATIC_RULES_MAX_STR, static_rule_count);
	return 0;

#ifdef DEFEX_SAFEPLACE_ENABLE
	task_defex_destroy_safeplace_obj(global_safeplace_obj);
safeplace_error:
#endif /* DEFEX_SAFEPLACE_ENABLE */

#ifdef DEFEX_PED_ENABLE
	task_defex_destroy_privesc_obj(global_privesc_obj);
privesc_error:
#endif /* DEFEX_PED_ENABLE */

#if defined(DEFEX_DEBUG_ENABLE) && defined(DEFEX_SYSFS_ENABLE)
kset_error:
	kset_unregister(defex_kset);
	defex_kset = NULL;
#endif /* DEFEX_DEBUG_ENABLE && DEFEX_SYSFS_ENABLE */
	return -ENOMEM;
}
