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
	{feature_safeplace_status,"1"},
	{feature_immutable_status,"1"},
	{feature_ped_status,"1"},
#ifndef DEFEX_USE_PACKED_RULES
	/* Rules will be added here */
	/* Never modify the above line. Rules will be added for buildtime */
#endif /* DEFEX_USE_PACKED_RULES */
};

const int static_rule_count = sizeof(defex_static_rules) / sizeof(defex_static_rules[0]);
