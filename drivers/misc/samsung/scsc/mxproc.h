/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * mx140 proc interface
 */

#ifndef MXPROC_H
#define MXPROC_H

struct mxproc;

int mxproc_create_ctrl_proc_dir(struct mxproc *mxproc, struct mxman *mxman);
void mxproc_remove_ctrl_proc_dir(struct mxproc *mxproc);
int mxproc_create_info_proc_dir(struct mxproc *mxproc, struct mxman *mxman);
void mxproc_remove_info_proc_dir(struct mxproc *mxproc);
extern int scsc_mx_list_services(struct mxman *mxman_p, char *buf, const size_t bufsz);
extern int mxman_print_rf_hw_version(struct mxman *mxman, char *buf, const size_t bufsz);

struct mxproc {
	struct mxman          *mxman;
	struct proc_dir_entry *procfs_ctrl_dir;
	u32                   procfs_ctrl_dir_num;
	struct proc_dir_entry *procfs_info_dir;
};

#endif /* MXPROC_H */
