/* sound/soc/samsung/abox/abox_bt.h
 *
 * ALSA SoC Audio Layer - Samsung Abox SCSC B/T driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_BT_H
#define __SND_SOC_ABOX_BT_H

#include <linux/device.h>

enum bt_sco {
	BT_SCO_ALL,
	BT_SCO_MIC,
	BT_SCO_SPK,
	BT_SCO_COUNT,
};

struct abox_bt_data {
	struct device *dev;
	struct abox_data *abox_data;
	phys_addr_t paddr_bt;
	bool active[BT_SCO_COUNT];
};

/**
 * Get rate of bluetooth audio interface
 * @param[in]	dev	pointer to abox_bt device
 * @return	sample rate or 0
 */
extern unsigned int abox_bt_get_rate(struct device *dev);

/**
 * Get IO virtual address of bluetooth tx buffer
 * @param[in]	dev	pointer to abox_bt device
 * @param[in]	stream	SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE
 * @return	IO virtual address or 0
 */
extern unsigned int abox_bt_get_buf_iova(struct device *dev, int stream);

/**
 * Returns true when the bt stream is active
 * @param[in]	dev	pointer to abox_bt device
 * @param[in]	stream	SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE
 * @return	true or false
 */
extern bool abox_bt_active(struct device *dev, int stream);

#endif /* __SND_SOC_ABOX_BT_H */
