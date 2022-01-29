/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _FS_FUSE_SHORCIRCUIT_H
#define _FS_FUSE_SHORCIRCUIT_H

#include "fuse_i.h"

#include <linux/fuse.h>
#include <linux/file.h>

void fuse_setup_shortcircuit(struct fuse_conn *fc, struct fuse_req *req);

ssize_t fuse_shortcircuit_read_iter(struct kiocb *iocb, struct iov_iter *to);

ssize_t fuse_shortcircuit_write_iter(struct kiocb *iocb, struct iov_iter *from);

void fuse_shortcircuit_release(struct fuse_file *ff);

#endif /* _FS_FUSE_SHORCIRCUIT_H */
