/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFRAMMAN_H
#define __MIFRAMMAN_H

#include <linux/mutex.h>
#include "scsc/api/bt_audio.h"

/* TODO:  Needs to define the max mem */

struct miframman;
struct mifabox;


void miframman_init(struct miframman *ram, void *start_dram, size_t size_pool, void *start_region);
void miframabox_init(struct mifabox *mifabox, void *start_aboxram);
void *miframman_alloc(struct miframman *ram, size_t nbytes, size_t align, int tag);
void miframman_free(struct miframman *ram, void *mem);
void miframman_deinit(struct miframman *ram);
void miframabox_deinit(struct mifabox *mifabox);
void miframman_log(struct miframman *ram, struct seq_file *fd);

#if defined(CONFIG_SOC_EXYNOS3830) || defined(CONFIG_SOC_EXYNOS7885)
#define MIFRAMMAN_MAXMEM                (12 * 1024 * 1024)
#else
#define MIFRAMMAN_MAXMEM                (16 * 1024 * 1024)
#endif

#define MIFRAMMAN_BLOCK_SIZE    (64)

#define MIFRAMMAN_NUM_BLOCKS    ((MIFRAMMAN_MAXMEM) / (MIFRAMMAN_BLOCK_SIZE))

/* Block status in lower nibble */
#define MIFRAMMAN_BLOCK_STATUS_MASK	0x0f
#define BLOCK_FREE      0
#define BLOCK_INUSE     1
#define BLOCK_BOUND     2	/* Block allocation boundary */

/* Block owner in upper nibble */
#define MIFRAMMAN_BLOCK_OWNER_MASK	0xf0
#define MIFRAMMAN_BLOCK_OWNER_SHIFT	4

#define MIFRAMMAN_OWNER_COMMON	0	/* Owner tag for Common driver */

/* Inclusion in core.c treat it as opaque */
struct miframman {
	void         *start_region;                /* Base address of region containing the pool */
	void         *start_dram;                  /* Base address of allocator pool */
	size_t       size_pool;                    /* Size of allocator pool */
	char         bitmap[MIFRAMMAN_NUM_BLOCKS]; /* Zero initialized-> all blocks free */
	u32          num_blocks;                   /* Blocks of MIFRAMMAN_BLOCK_SIZE in pool */
	u32          free_mem;                     /* Bytes remaining in allocator pool */
	struct mutex lock;
};

struct mifabox {
	struct scsc_bt_audio_abox *aboxram;
};
#endif
