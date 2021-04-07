/*
 * Samsung Exynos5 SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_MEMORY_H__
#define __VIPX_MEMORY_H__

#include <linux/dma-buf.h>

#include "vs4l.h"
#include "vipx-common-type.h"

#define VIPX_CC_DRAM_BIN_DVADDR		(0x10000000)
#define VIPX_CC_DRAM_BIN_SIZE		(SZ_4M)
#define VIPX_MBOX_DVADDR		(0x11000000)
#define VIPX_MBOX_SIZE			(SZ_32K)
#define VIPX_HEAP_DVADDR		(0x12000000)
#define VIPX_HEAP_SIZE			(SZ_1M)
#define VIPX_LOG_DVADDR			(0x13000000)
#define VIPX_LOG_SIZE			(SZ_1M)
#define VIPX_MEMORY_MAX_SIZE		(SZ_16M)

#define VIPX_PRIV_MEM_NAME_LEN		(30)

struct vipx_system;
struct vipx_memory;

struct vipx_buffer {
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			reserved;
	unsigned int			offset;
	size_t                          size;
	size_t				dbuf_size;
	union {
		unsigned long           userptr;
		int                     fd;
	} m;

	enum dma_data_direction		direction;
	struct dma_buf                  *dbuf;
	struct dma_buf_attachment       *attachment;
	struct sg_table                 *sgt;

	dma_addr_t                      dvaddr;
	void                            *kvaddr;

	struct vs4l_roi                 roi;
};

struct vipx_memory_ops {
	int (*map_dmabuf)(struct vipx_memory *mem,
			struct vipx_buffer *buf);
	int (*unmap_dmabuf)(struct vipx_memory *mem,
			struct vipx_buffer *buf);
	int (*sync_for_device)(struct vipx_memory *mem,
			struct vipx_buffer *buf);
	int (*sync_for_cpu)(struct vipx_memory *mem,
			struct vipx_buffer *buf);
};

struct vipx_priv_mem {
	char				name[VIPX_PRIV_MEM_NAME_LEN];
	size_t				size;
	size_t				dbuf_size;
	long				flags;
	bool				kmap;
	bool				fixed_dvaddr;

	enum dma_data_direction		direction;
	struct dma_buf			*dbuf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sgt;

	dma_addr_t			dvaddr;
	void				*kvaddr;
};

struct vipx_memory {
	struct device			*dev;
	struct iommu_domain		*domain;
	const struct vipx_memory_ops	*mops;

	struct vipx_priv_mem		fw;
	struct vipx_priv_mem		mbox;
	struct vipx_priv_mem		heap;
	struct vipx_priv_mem		log;
};

int vipx_memory_open(struct vipx_memory *mem);
int vipx_memory_close(struct vipx_memory *mem);
int vipx_memory_probe(struct vipx_system *sys);
void vipx_memory_remove(struct vipx_memory *mem);

#endif
