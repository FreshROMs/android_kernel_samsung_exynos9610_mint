/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include "include/defex_rules.h"

const struct static_rule defex_static_rules[] = {
	{feature_ped_path,"/"},
#ifndef DEFEX_USE_PACKED_RULES
	{feature_ped_exception,"/system/bin/run-as"},
	{feature_safeplace_path,"/init"},
	{feature_safeplace_path,"/sbin/"},
	{feature_safeplace_path,"/system/bin/"},
	{feature_safeplace_path,"/system/xbin/"},
	{feature_safeplace_path,"/system/vendor/bin/"},
	{feature_safeplace_path,"/vendor/bin/"},
	{feature_safeplace_path,"/tmp/update_binary"},
#endif /* DEFEX_USE_PACKED_RULES */
	{feature_safeplace_status,"1"},
	{feature_ped_status,"1"},
};

const int static_rule_count = sizeof(defex_static_rules) / sizeof(defex_static_rules[0]);
