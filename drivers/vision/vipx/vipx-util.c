/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vipx-log.h"
#include "vipx-util.h"

void vipx_util_bitmap_init(unsigned long *bitmap, unsigned int nbits)
{
	bitmap_zero(bitmap, nbits);
}

unsigned int vipx_util_bitmap_get_zero_bit(unsigned long *bitmap,
		unsigned int nbits)
{
	int bit, old_bit;

	while ((bit = find_first_zero_bit(bitmap, nbits)) != nbits) {
		old_bit = test_and_set_bit(bit, bitmap);
		if (!old_bit)
			return bit;
	}

	return nbits;
}

void vipx_util_bitmap_clear_bit(unsigned long *bitmap, int bit)
{
	clear_bit(bit, bitmap);
}

