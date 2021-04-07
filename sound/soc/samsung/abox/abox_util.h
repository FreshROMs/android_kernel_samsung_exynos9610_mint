/* sound/soc/samsung/abox/abox_util.h
 *
 * ALSA SoC - Samsung Abox utility
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_UTIL_H
#define __SND_SOC_ABOX_UTIL_H

#include <sound/pcm.h>

/**
 * ioremap to virtual address but not request
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[in]	num		index of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *devm_not_request_and_map(struct platform_device *pdev,
		const char *name, unsigned int num, phys_addr_t *phys_addr,
		size_t *size);

/**
 * Request memory resource and map to virtual address
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[in]	num		index of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *devm_request_and_map(struct platform_device *pdev,
		const char *name, unsigned int num, phys_addr_t *phys_addr,
		size_t *size);

/**
 * Request memory resource and map to virtual address
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *devm_request_and_map_byname(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size);

/**
 * Request clock and prepare
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of clock
 * @return	pointer to clock
 */
extern struct clk *devm_clk_get_and_prepare(struct platform_device *pdev,
		const char *name);

/**
 * Read single long physical address (sleeping function)
 * @param[in]	addr		physical address
 * @return	value of the physical address
 */
extern u32 readl_phys(phys_addr_t addr);

/**
 * Write single long physical address (sleeping function)
 * @param[in]	val		value
 * @param[in]	addr		physical address
 */
extern void writel_phys(unsigned int val, phys_addr_t addr);

/**
 * Atomically increments @v, if @v was @r, set to 0.
 * @param[in]	v		pointer of type atomic_t
 * @param[in]	r		maximum range of @v.
 * @return	Returns old value
 */
static inline int atomic_inc_unless_in_range(atomic_t *v, int r)
{
	int ret;

	while ((ret = __atomic_add_unless(v, 1, r)) == r) {
		ret = atomic_cmpxchg(v, r, 0);
		if (ret == r)
			break;
	}

	return ret;
}

/**
 * Atomically decrements @v, if @v was 0, set to @r.
 * @param[in]	v		pointer of type atomic_t
 * @param[in]	r		maximum range of @v.
 * @return	Returns old value
 */
static inline int atomic_dec_unless_in_range(atomic_t *v, int r)
{
	int ret;

	while ((ret = __atomic_add_unless(v, -1, 0)) == 0) {
		ret = atomic_cmpxchg(v, 0, r);
		if (ret == 0)
			break;
	}

	return ret;
}

/**
 * Check whether the GIC is secure (sleeping function)
 * @return	true if the GIC is secure, false on otherwise
 */
extern bool is_secure_gic(void);

/**
 * Get SNDRV_PCM_FMTBIT_* within width_min and width_max.
 * @param[in]	width_min	minimum bit width
 * @param[in]	width_max	maximum bit width
 * @return	Bitwise and of SNDRV_PCM_FMTBIT_*
 */
extern u64 width_range_to_bits(unsigned int width_min,
		unsigned int width_max);

/**
 * Get character from substream direction
 * @param[in]	substream	substream
 * @return	'p' if direction is playback. 'c' if not.
 */
extern char substream_to_char(struct snd_pcm_substream *substream);

#endif /* __SND_SOC_ABOX_UTIL_H */
