/*
 * Copyright (C) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __DMA_PL330_H_
#define __DMA_PL330_H_ __FILE__

/*
 * PL330 can assign any channel to communicate with
 * any of the peripherals attched to the DMAC.
 * For the sake of consistency across client drivers,
 * We keep the channel names unchanged and only add
 * missing peripherals are added.
 * Order is not important since DMA PL330 API driver
 * use these just as IDs.
 */
enum dma_ch {
	DMACH_UART0_RX = 0,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_UART5_RX,
	DMACH_UART5_TX,
	DMACH_USI_RX,
	DMACH_USI_TX,
	DMACH_IRDA,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_EXTERNAL,
	DMACH_PWM,
	DMACH_SPDIF,
	DMACH_HSI_RX,
	DMACH_HSI_TX,
	DMACH_PCM0_TX,
	DMACH_PCM0_RX,
	DMACH_PCM1_TX,
	DMACH_PCM1_RX,
	DMACH_PCM2_TX,
	DMACH_PCM2_RX,
	DMACH_MSM_REQ3,
	DMACH_MSM_REQ2,
	DMACH_MSM_REQ1,
	DMACH_MSM_REQ0,
	DMACH_SLIMBUS0_RX,
	DMACH_SLIMBUS0_TX,
	DMACH_SLIMBUS0AUX_RX,
	DMACH_SLIMBUS0AUX_TX,
	DMACH_SLIMBUS1_RX,
	DMACH_SLIMBUS1_TX,
	DMACH_SLIMBUS2_RX,
	DMACH_SLIMBUS2_TX,
	DMACH_SLIMBUS3_RX,
	DMACH_SLIMBUS3_TX,
	DMACH_SLIMBUS4_RX,
	DMACH_SLIMBUS4_TX,
	DMACH_SLIMBUS5_RX,
	DMACH_SLIMBUS5_TX,
	DMACH_MIPI_HSI0,
	DMACH_MIPI_HSI1,
	DMACH_MIPI_HSI2,
	DMACH_MIPI_HSI3,
	DMACH_MIPI_HSI4,
	DMACH_MIPI_HSI5,
	DMACH_MIPI_HSI6,
	DMACH_MIPI_HSI7,
	DMACH_DISP1,
	DMACH_MTOM_0,
	DMACH_MTOM_1,
	DMACH_MTOM_2,
	DMACH_MTOM_3,
	DMACH_MTOM_4,
	DMACH_MTOM_5,
	DMACH_MTOM_6,
	DMACH_MTOM_7,
	/* END Marker, also used to denote a reserved channel */
	DMACH_MAX,
};

struct s3c2410_dma_client {
	char	*name;
};

static inline bool samsung_dma_has_circular(void)
{
	return true;
}

static inline bool samsung_dma_is_dmadev(void)
{
	return true;
}

static inline bool samsung_dma_has_infiniteloop(void)
{
	return true;
}

#include <linux/dmaengine.h>

struct samsung_dma_req {
	enum dma_transaction_type cap;
	struct s3c2410_dma_client *client;
};

struct samsung_dma_prep {
	enum dma_transaction_type cap;
	enum dma_transfer_direction direction;
	dma_addr_t buf;
	unsigned long period;
	unsigned long len;
	void (*fp)(void *data);
	void *fp_param;
	unsigned int infiniteloop;
};

struct samsung_dma_config {
	enum dma_transfer_direction direction;
	enum dma_slave_buswidth width;
	u32 maxburst;
	dma_addr_t fifo;
};

struct samsung_dma_ops {
	unsigned long(*request)(enum dma_ch ch, struct samsung_dma_req *param,
				struct device *dev, char *ch_name);
	int (*release)(unsigned long ch, void *param);
	int (*config)(unsigned long ch, struct samsung_dma_config *param);
	int (*prepare)(unsigned long ch, struct samsung_dma_prep *param);
	int (*trigger)(unsigned long ch);
	int (*started)(unsigned long ch);
	int (*getposition)(unsigned long ch, dma_addr_t *src, dma_addr_t *dst);
	int (*flush)(unsigned long ch);
	int (*stop)(unsigned long ch);
	int (*debug)(unsigned long ch);
};

extern void *samsung_dmadev_get_ops(void);
extern void *s3c_dma_get_ops(void);

static inline void *__samsung_dma_get_ops(void)
{
	if (samsung_dma_is_dmadev())
		return samsung_dmadev_get_ops();
	else
		return s3c_dma_get_ops();
}

/*
 * samsung_dma_get_ops
 * get the set of samsung dma operations
 */
#ifdef CONFIG_SAMSUNG_DMADEV
#define samsung_dma_get_ops() __samsung_dma_get_ops()
#else
#define samsung_dma_get_ops() NULL
#endif

#endif	/* __DMA_PL330_H_ */
