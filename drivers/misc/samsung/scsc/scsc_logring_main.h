/********************************************************************************
 *
 *   Copyright (c) 2016 - 2017 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ********************************************************************************/

#ifndef _SCSC_LOGRING_MAIN_H_
#define _SCSC_LOGRING_MAIN_H_

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/log2.h>

#include "scsc_logring_common.h"

#define ADD_DEBUG_MODULE_PARAM(tagname, default_level, filter) \
	static int scsc_droplevel_ ## tagname = default_level; \
	module_param(scsc_droplevel_ ## tagname, int, S_IRUGO | S_IWUSR); \
	SCSC_MODPARAM_DESC(scsc_droplevel_ ## tagname, \
			   "Droplevels for the '" # tagname "' family.", \
			   "run-time", default_level)

#define IS_PRINTK_REDIRECT_ALLOWED(ff, level, tag) \
	((ff) == FORCE_PRK || \
	 ((ff) != NO_ECHO_PRK && (level) < scsc_redirect_to_printk_droplvl))

#endif /* _SCSC_LOGRING_MAIN_H_ */
