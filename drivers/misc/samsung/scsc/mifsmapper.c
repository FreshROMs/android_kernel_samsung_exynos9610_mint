/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* uses */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsc/scsc_logring.h>
#include <linux/bitmap.h>
#include "scsc_mif_abs.h"

/* Implements */
#include "mifsmapper.h"


static int mifsmapper_get_num_banks(u8 *phy_map, u8 *log_map, bool large)
{
	u8 i = 0, count = 0;

	for (i = 0; i < SCSC_MIF_SMAPPER_MAX_BANKS; i++) {
		if (large && phy_map[i] == SCSC_MIF_ABS_LARGE_BANK) {
			log_map[count] = i;
			count++;
		} else if (!large && phy_map[i] == SCSC_MIF_ABS_SMALL_BANK) {
			log_map[count] = i;
			count++;
		}
	}

	return count;
}

int mifsmapper_init(struct mifsmapper *smapper, struct scsc_mif_abs *mif)
{
	/* TODO: Protect the function if allocations fail */
	struct scsc_mif_smapper_info bank_info;
	u8 i = 0, total_num_banks;
	u8 phy_map[SCSC_MIF_SMAPPER_MAX_BANKS] = { 0 };
	u8 log_map_large[SCSC_MIF_SMAPPER_MAX_BANKS] = { 0 };
	u8 log_map_small[SCSC_MIF_SMAPPER_MAX_BANKS] = { 0 };

	if (smapper->in_use)
		return -EBUSY;

	SCSC_TAG_INFO(MIF, "Init SMAPPER\n");

	spin_lock_init(&smapper->lock);
	/* Get physical mapping of the banks */
	if (mif->mif_smapper_get_mapping(mif, phy_map, &smapper->align)) {
		SCSC_TAG_ERR(MIF, "SMAPPER is not present\n");
		return -EINVAL;
	}

	smapper->in_use = true;
	smapper->mif = mif;

	smapper->num_large_banks = mifsmapper_get_num_banks(phy_map, log_map_large, true);
	smapper->num_small_banks = mifsmapper_get_num_banks(phy_map, log_map_small, false);
	total_num_banks = smapper->num_large_banks + smapper->num_small_banks;

	smapper->bank = kmalloc_array(total_num_banks, sizeof(struct mifsmapper_bank),
				      GFP_KERNEL);

	smapper->bank_bm_large = kmalloc(BITS_TO_LONGS(smapper->num_large_banks) * sizeof(unsigned long), GFP_KERNEL);
	bitmap_zero(smapper->bank_bm_large, smapper->num_large_banks);

	smapper->bank_bm_small = kmalloc(BITS_TO_LONGS(smapper->num_small_banks) * sizeof(unsigned long), GFP_KERNEL);
	bitmap_zero(smapper->bank_bm_small, smapper->num_small_banks);

	/* LSB bit of banks will be the large banks the rest will be the small banks */
	/* Get large bank info */
	for (; i < smapper->num_large_banks; i++) {
		/* get phy bank */
		mif->mif_smapper_get_bank_info(mif, log_map_large[i], &bank_info);
		smapper->bank[i].entries_bm = kmalloc(BITS_TO_LONGS(bank_info.num_entries) * sizeof(unsigned long), GFP_KERNEL);
		smapper->bank[i].num_entries = bank_info.num_entries;
		smapper->bank[i].mem_range_bytes = bank_info.mem_range_bytes;
		smapper->bank[i].phy_index = log_map_large[i];
		SCSC_TAG_INFO(MIF, "phy bank %d mapped to logical %d. Large, entries %d range 0x%x\n",
			      log_map_large[i], i, bank_info.num_entries, bank_info.mem_range_bytes);
		bitmap_zero(smapper->bank[i].entries_bm, bank_info.num_entries);
	}

	/* Get small bank info */
	for (; i < total_num_banks; i++) {
		/* get phy bank */
		mif->mif_smapper_get_bank_info(mif, log_map_small[i - smapper->num_large_banks], &bank_info);
		smapper->bank[i].entries_bm = kmalloc(BITS_TO_LONGS(bank_info.num_entries) * sizeof(unsigned long), GFP_KERNEL);
		smapper->bank[i].num_entries = bank_info.num_entries;
		smapper->bank[i].mem_range_bytes = bank_info.mem_range_bytes;
		smapper->bank[i].phy_index = log_map_small[i - smapper->num_large_banks];
		SCSC_TAG_INFO(MIF, "phy bank %d mapped to logical %d. Small, entries %d range 0x%x\n",
			      log_map_small[i - smapper->num_large_banks], i, bank_info.num_entries, bank_info.mem_range_bytes);
		bitmap_zero(smapper->bank[i].entries_bm, bank_info.num_entries);
	}

	return 0;
}

u16 mifsmapper_get_alignment(struct mifsmapper *smapper)
{
	return smapper->align;
}

int mifsmapper_alloc_bank(struct mifsmapper *smapper, bool large_bank, u32 entry_size, u16 *entries)
{
	struct mifsmapper_bank *bank;
	unsigned long *bitmap;
	u8           max_banks, offset = 0;
	int          which_bit = 0;

	spin_lock(&smapper->lock);

	if (!smapper->in_use)
		goto error;

	bank = smapper->bank;
	if (large_bank) {
		max_banks = smapper->num_large_banks;
		bitmap = smapper->bank_bm_large;
	} else {
		max_banks = smapper->num_small_banks;
		bitmap = smapper->bank_bm_small;
		offset = smapper->num_large_banks;
	}

	/* Search for free slots */
	which_bit = find_first_zero_bit(bitmap, max_banks);
	if (which_bit >= max_banks)
		goto error;

	/* Update bit mask */
	set_bit(which_bit, bitmap);

	/* Retrieve Bank capabilities and return the number of entries available */
	 /* size must be a power of 2 */
	/* TODO : check that granularity is correct */
	BUG_ON(!is_power_of_2(entry_size));

	/* Clear bank entries */
	bitmap_zero(bank[which_bit + offset].entries_bm, bank[which_bit + offset].num_entries);

	*entries = bank[which_bit + offset].mem_range_bytes/entry_size;
	/* Saturate */
	if (*entries > bank[which_bit + offset].num_entries)
		*entries = bank[which_bit + offset].num_entries;
	else if (*entries < bank[which_bit + offset].num_entries) {
		u16 i;

		SCSC_TAG_INFO(MIF, "Nominal entries %d reduced to %d\n",
			     bank[which_bit + offset].num_entries, *entries);

		for (i = *entries; i < bank[which_bit + offset].num_entries; i++)
			/* Mark the MSB of the bitmap as used */
			set_bit(i, bank[which_bit + offset].entries_bm);
	}
	/* Update number of entries */
	bank[which_bit + offset].num_entries = *entries;
	bank[which_bit + offset].num_entries_left = *entries;
	bank[which_bit + offset].in_use = true;
	bank[which_bit + offset].granularity = entry_size;

	SCSC_TAG_INFO(MIF, "entries %d bank.num_entries %d large bank %d logical bank %d entries left %d\n", *entries, bank[which_bit + offset].num_entries, large_bank, which_bit + offset,
			bank[which_bit + offset].num_entries_left);

	spin_unlock(&smapper->lock);
	return which_bit + offset;

error:
	SCSC_TAG_ERR(MIF, "Error allocating bank\n");

	*entries = 0;
	spin_unlock(&smapper->lock);
	return -EIO;
}

int mifsmapper_free_bank(struct mifsmapper *smapper, u8 bank)
{
	unsigned long *bitmap;
	u8           max_banks, offset = 0;
	struct mifsmapper_bank *bank_en;

	spin_lock(&smapper->lock);

	if (!smapper->in_use || ((bank >= (smapper->num_large_banks + smapper->num_small_banks))))
		goto error;

	/* check if it is a large or small bank */
	if (bank >= smapper->num_large_banks) {
		max_banks = smapper->num_small_banks;
		bitmap = smapper->bank_bm_small;
		offset = bank - smapper->num_large_banks;
	} else {
		max_banks = smapper->num_large_banks;
		bitmap = smapper->bank_bm_large;
		offset = bank;
	}

	/* Update bit mask */
	if (!test_and_clear_bit(offset, bitmap))
		SCSC_TAG_ERR(MIF, "bank was not allocated\n");

	bank_en = smapper->bank;
	bank_en[bank].in_use = false;

	spin_unlock(&smapper->lock);

	return 0;
error:
	SCSC_TAG_ERR(MIF, "Error freeing bank %d\n", bank);
	spin_unlock(&smapper->lock);

	return -EIO;
}

int mifsmapper_get_entries(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 *entries)
{
	struct mifsmapper_bank *bank_en;
	unsigned long *bitmap;
	u32           max_bits, i, ent;

	if (!smapper->bank)
		return -EINVAL;

	bank_en = smapper->bank;

	if (!bank_en[bank].in_use) {
		SCSC_TAG_ERR(MIF, "Bank %d not allocated.\n", bank);
		return -EINVAL;
	}


	max_bits = bank_en[bank].num_entries_left;
	ent = bank_en[bank].num_entries;
	if (num_entries > max_bits) {
		SCSC_TAG_ERR(MIF, "Not enough entries. Requested %d, left %d\n", num_entries, max_bits);
		return -ENOMEM;
	}

	bitmap = bank_en[bank].entries_bm;

	for (i = 0; i < num_entries; i++) {
		entries[i] = find_first_zero_bit(bitmap, ent);
		if (entries[i] >= ent)
			return -EIO;
		/* Update bit mask */
		set_bit(entries[i], bitmap);
	}

	smapper->bank[bank].num_entries_left -= num_entries;

	return 0;
}

int mifsmapper_free_entries(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 *entries)
{
	struct mifsmapper_bank *bank_en;
	unsigned long *bitmap;
	u32           max_bits, i, ent, total = 0;

	if (!smapper->bank)
		return -EINVAL;

	bank_en = smapper->bank;

	if (!bank_en[bank].in_use) {
		SCSC_TAG_ERR(MIF, "Bank %d not allocated.\n", bank);
		return -EINVAL;
	}


	max_bits = bank_en[bank].num_entries_left;
	ent = bank_en[bank].num_entries;
	if ((max_bits + num_entries) > ent) {
		SCSC_TAG_ERR(MIF, "Tried to free more entries. Requested %d, left %d\n", num_entries, max_bits);
		return -ENOMEM;
	}

	bitmap = bank_en[bank].entries_bm;

	for (i = 0; i < num_entries; i++) {
		/* Update bit mask */
		if (!test_and_clear_bit(entries[i], bitmap))
			SCSC_TAG_ERR(MIF, "entry was not allocated\n");
		else
			total++;
	}

	smapper->bank[bank].num_entries_left += total;

	return 0;
}

void mifsmapper_configure(struct mifsmapper *smapper, u32 granularity)
{
	struct scsc_mif_abs *mif;
	/* Get abs implementation */
	mif = smapper->mif;

	mif->mif_smapper_configure(mif, granularity);
}

int mifsmapper_write_sram(struct mifsmapper *smapper, u8 bank, u8 num_entries, u8 first_entry, dma_addr_t *addr)
{
	struct scsc_mif_abs *mif;

	if (!smapper->bank[bank].in_use) {
		SCSC_TAG_ERR(MIF, "Bank %d not allocated.\n", bank);
		return -EINVAL;
	}

	/* Get abs implementation */
	mif = smapper->mif;

	/* use the phy address of the bank */
	return mif->mif_smapper_write_sram(mif, smapper->bank[bank].phy_index, num_entries, first_entry, addr);
}

u32 mifsmapper_get_bank_base_address(struct mifsmapper *smapper, u8 bank)
{
	struct scsc_mif_abs *mif;

	/* Get abs implementation */
	mif = smapper->mif;

	return mif->mif_smapper_get_bank_base_address(mif, bank);
}

int mifsmapper_deinit(struct mifsmapper *smapper)
{
	u8 i = 0, total_num_banks;

	spin_lock(&smapper->lock);

	SCSC_TAG_INFO(MIF, "Deinit SMAPPER\n");

	if (!smapper->in_use) {
		spin_unlock(&smapper->lock);
		return -ENODEV;
	}

	total_num_banks = smapper->num_large_banks + smapper->num_small_banks;
	for (; i < total_num_banks; i++) {
		kfree(smapper->bank[i].entries_bm);
		smapper->bank[i].num_entries = 0;
		smapper->bank[i].mem_range_bytes = 0;
	}

	kfree(smapper->bank_bm_large);
	kfree(smapper->bank_bm_small);
	kfree(smapper->bank);

	smapper->in_use = false;

	spin_unlock(&smapper->lock);
	return 0;
}
