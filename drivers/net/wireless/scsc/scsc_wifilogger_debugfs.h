/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef _SCSC_WIFILOGGER_DEBUGFS_H_
#define _SCSC_WIFILOGGER_DEBUGFS_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger.h"

#define SCSC_DEBUGFS_ROOT			"/sys/kernel/debug/wifilogger"
#define SCSC_DEBUGFS_ROOT_DIRNAME		"wifilogger"

struct scsc_wlog_debugfs_info {
	struct dentry *rootdir;
	struct dentry *ringdir;
};

struct scsc_ring_test_object {
	struct scsc_wlog_ring   *r;
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
	char *rbuf;
	char *wbuf;
	size_t bsz;
	struct list_head elem;
	wait_queue_head_t	drain_wq;
	wait_queue_head_t	rw_wq;
	struct mutex		readers_lock;
#endif
};

void *scsc_wlog_register_debugfs_entry(const char *ring_name,
				       const char *fname,
				       const struct file_operations *fops,
				       void *rto,
				       struct scsc_wlog_debugfs_info *di);

struct scsc_ring_test_object *init_ring_test_object(struct scsc_wlog_ring *r);

void scsc_wifilogger_debugfs_remove_top_dir_recursive(void);

void scsc_register_common_debugfs_entries(char *ring_name, void *rto,
					   struct scsc_wlog_debugfs_info *di);

int dfs_open(struct inode *ino, struct file *filp);

int dfs_release(struct inode *ino, struct file *filp);

#endif /* _SCSC_WIFILOGGER_DEBUGFS_H_ */

