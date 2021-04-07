/* sound/soc/samsung/abox/abox_gic.h
 *
 * ALSA SoC Audio Layer - Samsung Abox GIC driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_GIC_H
#define __SND_SOC_ABOX_GIC_H

#define ABOX_GIC_IRQ_COUNT 16

struct abox_gic_irq_handler_t {
	irq_handler_t handler;
	void *dev_id;
};

struct abox_gic_data {
	void __iomem *gicd_base;
	void __iomem *gicc_base;
	phys_addr_t gicd_base_phys;
	phys_addr_t gicc_base_phys;
	int irq;
	struct abox_gic_irq_handler_t handler[ABOX_GIC_IRQ_COUNT];
	bool disabled;

};

/**
 * Generate interrupt
 * @param[in]	dev	pointer to abox_gic device
 * @param[in]	irq	irq number
 */
extern void abox_gic_generate_interrupt(struct device *dev, unsigned int irq);

/**
 * Register interrupt handler
 * @param[in]	dev	pointer to abox_gic device
 * @param[in]	irq	irq number
 * @param[in]	handler	function to be called on interrupt
 * @param[in]	dev_id	cookie for interrupt.
 * @return	error code or 0
 */
extern int abox_gic_register_irq_handler(struct device *dev,
		unsigned int irq, irq_handler_t handler, void *dev_id);

/**
 * Unregister interrupt handler
 * @param[in]	dev	pointer to abox_gic device
 * @param[in]	irq	irq number
 * @return	error code or 0
 */
extern int abox_gic_unregister_irq_handler(struct device *dev,
		unsigned int irq);

/**
 * Enable abox gic irq
 * @param[in]	dev	pointer to abox_gic device
 */
extern int abox_gic_enable_irq(struct device *dev);

/**
 * Disable abox gic irq
 * @param[in]	dev	pointer to abox_gic device
 */
extern int abox_gic_disable_irq(struct device *dev);

/**
 * Initialize abox gic
 * @param[in]	dev	pointer to abox_gic device
 */
extern void abox_gic_init_gic(struct device *dev);

#endif /* __SND_SOC_ABOX_GIC_H */
