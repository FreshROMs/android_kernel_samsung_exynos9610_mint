/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/defex_rules.h"

struct rule_item_struct *defex_packed_rules;
int packfiles_count, packfiles_size;

struct rule_item_struct *create_file_item(const char *name, int l);
struct rule_item_struct *add_file_item(struct rule_item_struct *base, const char *name, int l);
struct rule_item_struct *lookup_dir(struct rule_item_struct *base, const char *name, int l);
struct rule_item_struct *add_file_path(const char *file_path);
struct rule_item_struct *addline2tree(char *src_line, enum feature_types feature);
int lookup_tree(const char *file_path, int attribute);
int store_tree(FILE *f);


struct rule_item_struct *create_file_item(const char *name, int l)
{
	struct rule_item_struct *item;
	unsigned int offset;

	if (!name)
		l = 0;
	offset = packfiles_size;
	packfiles_size += (sizeof(struct rule_item_struct) + l);
	defex_packed_rules = realloc(defex_packed_rules, packfiles_size);
	packfiles_count++;
	item = GET_ITEM_PTR(offset);
	item->next_file = 0;
	item->next_level = 0;
	item->feature_type = 0;
	item->size = l;
	if (l)
		memcpy(item->name, name, l);
	return item;
}

struct rule_item_struct *add_file_item(struct rule_item_struct *base, const char *name, int l)
{
	struct rule_item_struct *item, *new_item = NULL;

	if (!base)
		return new_item;

	new_item = create_file_item(name, l);
	if (!base->next_level) {
		base->next_level = GET_ITEM_OFFSET(new_item);
	} else {
		item = GET_ITEM_PTR(base->next_level);
		while(item->next_file) {
			item = GET_ITEM_PTR(item->next_file);
		}
		item->next_file = GET_ITEM_OFFSET(new_item);
	}
	return new_item;
}

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

struct rule_item_struct *add_file_path(const char *file_path)
{
	const char *ptr, *next_separator;
	struct rule_item_struct *base, *cur_item = NULL;
	int l;

	if (!file_path || *file_path != '/')
		return NULL;
	if (!defex_packed_rules) {
		packfiles_count = 0;
		packfiles_size = 0;
		defex_packed_rules = create_file_item("HEAD", 4);
	}
	base = defex_packed_rules;
	ptr = file_path + 1;
	do {
		next_separator = strchr(ptr, '/');
		if (!next_separator)
			l = strlen(ptr);
		else
			l = next_separator - ptr;
		if (!l)
			return NULL; /* two slashes in sequence */
		cur_item = lookup_dir(base, ptr, l);
		if (!cur_item) {
			cur_item = add_file_item(base, ptr, l);
			/* slash wasn't found, it's a file */
			if (!next_separator)
				cur_item->feature_type |= feature_is_file;
		}
		base = cur_item;
		ptr += l;
		if (next_separator)
			ptr++;
	} while(*ptr);
	return cur_item;
}

int lookup_tree(const char *file_path, int attribute)
{
	const char *ptr, *next_separator;
	struct rule_item_struct *base, *cur_item = NULL;
	int l;

	if (!file_path || *file_path != '/' || !defex_packed_rules)
		return 0;
	base = defex_packed_rules;
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

struct rule_item_struct *addline2tree(char *src_line, enum feature_types feature)
{
	struct rule_item_struct *item = NULL;
	char *start_ptr, *end_ptr;

	start_ptr = strchr(src_line, '\"');
	if (start_ptr) {
		start_ptr++;
		end_ptr = strchr(start_ptr, '\"');
		if (end_ptr) {
			*end_ptr = 0;
			item = add_file_path(start_ptr);
			if (item)
				item->feature_type |= feature;
		}

	}
	return item;
}

int store_tree(FILE *f)
{
	unsigned char *ptr = (unsigned char *)defex_packed_rules;
	static char work_str[4096];
	int i, offset = 0, index = 0;

	work_str[0] = 0;
	fprintf(f, "const unsigned char defex_packed_rules[] = {\n");
	for(i = 0; i < packfiles_size; i++) {
		if (index)
			offset += sprintf(work_str + offset, ", ");
		offset += sprintf(work_str + offset, "0x%02x", ptr[i]);
		index++;
		if (index == 16) {
			fprintf(f, "\t%s,\n", work_str);
			index = 0;
			offset = 0;
		}
	}
	if (index)
		fprintf(f, "\t%s\n", work_str);
	fprintf(f, "};\n");
	return 0;
}

int main(int argc, char **argv)
{
	char *ptr, work_str[4096];

	if (argc == 3) {
		FILE *src_file = NULL, *dst_file = NULL;

		src_file = fopen(argv[1], "r");
		if (!src_file)
			goto show_info;
		dst_file = fopen(argv[2], "wt");
		if (!dst_file)
			goto show_info;

		while(!feof(src_file)) {
			if (!fgets(work_str, sizeof(work_str), src_file))
				break;
			ptr = strstr(work_str, "feature_safeplace_path");
			if (ptr) {
				addline2tree(work_str, feature_safeplace_path);
				continue;
			}

			ptr = strstr(work_str, "feature_ped_exception");
			if (ptr) {
				addline2tree(work_str, feature_ped_exception);
				continue;
			}
		}
		store_tree(dst_file);
		fclose(src_file);
		fclose(dst_file);
		if (!packfiles_count)
			goto show_info;
		return 0;
	}

show_info:
	printf("Defex rules pack utility.\nUSAGE:\n%s <SOURCE_FILE> <PACKED_FILE>\n", argv[0]);
	return -1;
}
