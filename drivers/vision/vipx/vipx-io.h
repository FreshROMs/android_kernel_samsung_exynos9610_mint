/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_IO_H__
#define __VIPX_IO_H__

#include <linux/io.h>

#define IOR8(port)		readb((const void *)&port)
#define IOR16(port)		readw((const void *)&port)
#define IOR32(port)		readl((const void *)&port)
#define IOR64(port)		readq((const void *)&port)

#ifdef DEBUG_LOG_IO_WRITE
#define IOW8(port, val)							\
	do {								\
		vipx_dbg("ADDR: %p, VAL: 0x%02x\r\n", &port, val);	\
		writeb(val, &port);					\
	} while (0)
#define IOW16(port, val)						\
	do {								\
		vipx_dbg("ADDR: %p, VAL: 0x%04x\r\n", &port, val);	\
		writew(val, &port);					\
	} while (0)
#define IOW32(port, val)						\
	do {								\
		vipx_dbg("ADDR: %p, VAL: 0x%08x\r\n", &port, val);	\
		writel(val, &port);					\
	} while (0)
#define IOW64(port, val)						\
	do {								\
		vipx_dbg("ADDR: %p, VAL: 0x%016llx\r\n", &port, val);	\
		writeq(val, &port);					\
	} while (0)
#else
#define IOW8(port, val)      writeb(val, &port)
#define IOW16(port, val)     writew(val, &port)
#define IOW32(port, val)     writel(val, &port)
#define IOW64(port, val)     writeq(val, &port)
#endif

void *vipx_io_copy_mem2io(void *dst, void *src, size_t size);
void *vipx_io_copy_io2mem(void *dst, void *src, size_t size);

#endif
