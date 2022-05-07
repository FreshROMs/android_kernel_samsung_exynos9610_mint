/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TUI_IOCTL_H_
#define TUI_IOCTL_H_

#include <linux/types.h>

#define MAX_BUFFER_NUMBER 3

#ifndef u32
#define u32 uint32_t
#endif

/* Command header */
struct tlc_tui_command_t {
	u32     id;
	u32     data[2];
};

/* Response header */
struct tlc_tui_response_t {
	u32	id;
	u32	return_code;
	int	ion_fd[MAX_BUFFER_NUMBER];
	u32	screen_metrics[3];
};

/* Resolution */
struct tlc_tui_resolution_t {
	u32	width;
	u32	height;
};

/* Buffer Info */
struct tlc_tui_ioctl_buffer_info {
	__u32 num_of_buff;
	__u32 size;
	__u32 width;
	__u32 height;
	__u32 stride;
	__u32 bits_per_pixel;
};

/* ION fd */
struct tlc_tui_ioctl_ion_t {
	int buffer_fd;
};

/* Command IDs */
/*  */
#define TLC_TUI_CMD_NONE                0
/* Start TUI session */
#define TLC_TUI_CMD_START_ACTIVITY      1
/* Stop TUI session */
#define TLC_TUI_CMD_STOP_ACTIVITY       2
/*
 * Queue a buffer
 * IN: index of buffer to be queued
 */
#define TLC_TUI_CMD_QUEUE               3
/*
 * Queue a new buffer and dequeue the buffer currently displayed
 * IN: indexes of buffer to be queued
 */
#define TLC_TUI_CMD_QUEUE_DEQUEUE       4
/*
 * Alloc buffers
 * IN: number of buffers
 * OUT: ion fd
 */
#define TLC_TUI_CMD_ALLOC_FB            5
/* Free buffers */
#define TLC_TUI_CMD_FREE_FB             6
/* hide secure surface */
#define TLC_TUI_CMD_HIDE_SURFACE        7
#define TLC_TUI_CMD_GET_RESOLUTION      8

/* TLC_TUI_CMD_SET_RESOLUTION is for specific platforms
 * that rely on onConfigurationChanged to set resolution
 * it has no effect on Trustonic reference implementaton.
 */
#define TLC_TUI_CMD_SET_RESOLUTION      9

/* To get buffer info */
#define TLC_TUI_CMD_GET_BUFFER_INFO     10

/* Return codes */
#define TLC_TUI_OK                  0
#define TLC_TUI_ERROR               1
#define TLC_TUI_ERR_UNKNOWN_CMD     2

/*
 * defines for the ioctl TUI driver module function call from user space.
 */
#define TUI_DEV_NAME	"t-base-tui"

#define TUI_IO_MAGIC	't'

#define TUI_IO_NOTIFY	_IOW(TUI_IO_MAGIC, 1, u32)
#define TUI_IO_WAITCMD	_IOR(TUI_IO_MAGIC, 2, struct tlc_tui_command_t)
#define TUI_IO_ACK	_IOW(TUI_IO_MAGIC, 3, struct tlc_tui_response_t)
#define TUI_IO_INIT_DRIVER	_IO(TUI_IO_MAGIC, 4)
#define TUI_IO_GET_BUFFER_INFO	_IOR(TUI_IO_MAGIC, 5, \
				     struct tlc_tui_ioctl_buffer_info)
#define TUI_IO_GET_ION_FD	_IOR(TUI_IO_MAGIC, 6, \
				     struct tlc_tui_ioctl_ion_t)
#define TUI_IO_SET_RESOLUTION _IOW(TUI_IO_MAGIC, 9, \
				   struct tlc_tui_resolution_t)
#define TUI_IO_MAP _IOWR(TUI_IO_MAGIC, 10, struct tlc_tui_ioctl_buffer_info)

#ifdef INIT_COMPLETION
#define reinit_completion(x) INIT_COMPLETION(*(x))
#endif

#endif /* TUI_IOCTL_H_ */
