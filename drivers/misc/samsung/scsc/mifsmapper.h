/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __MIFSMAPPER_H
#define __MIFSMAPPER_H

#include <linux/mutex.h>

struct mifsmapper;
struct scsc_mif_abs;
struct mutex;

int mifsmapper_init(struct mifsmapper *smapper, struct scsc_mif_abs *mif);
u16 mifsmapper_get_alignment(struct mifsmapper *smapper);
int mifsmapper_alloc_bank(struct mifsmapper *smapper, bool large_bank, u32 entry_size, u16 *entries);
int mifsmapper_free_bank(struct mifsmapper *smapper, u8 bank);
int mifsmapper_get_entries(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 *entr);
int mifsmapper_free_entries(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 *entries);
void mifsmapper_configure(struct mifsmapper *smapper, u32 granularity);
int mifsmapper_write_sram(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 first_entry, dma_addr_t *addr);
u32 mifsmapper_get_bank_base_address(struct mifsmapper *smapper, u8 bank);
int mifsmapper_deinit(struct mifsmapper *smapper);

#define MIFSMAPPER_160	4
#define MIFSMAPPER_64	7

#define MIFSMAPPER_NOT_VALID		0
#define MIFSMAPPER_VALID		1

struct mifsmapper_bank {
	unsigned long *entries_bm;
	u32 num_entries;
	u32 num_entries_left;
	u32 mem_range_bytes;
	u8  phy_index;
	u32 granularity;
	bool in_use;
};

/* Inclusion in core.c treat it as opaque */
struct mifsmapper {
	bool                 in_use;
	spinlock_t           lock;
	struct scsc_mif_abs  *mif;
	struct mifsmapper_bank     *bank; /* Bank reference created after reading HW capabilities */
	unsigned long *bank_bm_large;
	unsigned long *bank_bm_small;
	u32 num_large_banks;
	u32 num_small_banks;
	u16 align;
};
#endif
