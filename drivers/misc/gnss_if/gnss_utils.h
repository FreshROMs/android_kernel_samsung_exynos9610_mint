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

#ifndef __GNSS_UTILS_H__
#define __GNSS_UTILS_H__

#include "gnss_prj.h"

/* #define DEBUG_GNSS_IPC_PKT	1 */

struct __packed gnss_log {
	u8 fmt_msg;
	u8 boot_msg;
	u8 dump_msg;
	u8 rfs_msg;
	u8 log_msg;
	u8 ps_msg;
	u8 router_msg;
	u8 debug_log;
};

extern struct gnss_log log_info;

static const char const *direction_string[] = {
	[TX] = "TX",
	[RX] = "RX"
};

static const inline char *dir_str(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "INVALID";
	else
		return direction_string[dir];
}

/* print IPC message packet */
void gnss_log_ipc_pkt(struct sk_buff *skb, enum direction dir);

#endif/*__GNSS_UTILS_H__*/

