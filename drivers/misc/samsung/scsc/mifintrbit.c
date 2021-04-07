/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Uses */
#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"

/* Implements */
#include "mifintrbit.h"

/* default handler just logs a warning and clears the bit */
static void mifintrbit_default_handler(int irq, void *data)
{
	struct mifintrbit *intr = (struct mifintrbit *)data;
	unsigned long     flags;

	spin_lock_irqsave(&intr->spinlock, flags);
	intr->mif->irq_bit_clear(intr->mif, irq);
	spin_unlock_irqrestore(&intr->spinlock, flags);
}

static void print_bitmaps(struct mifintrbit *intr)
{
	unsigned long dst1, dst2, dst3;

	bitmap_copy_le(&dst1, intr->bitmap_tohost, MIFINTRBIT_NUM_INT);
	bitmap_copy_le(&dst2, intr->bitmap_fromhost_r4, MIFINTRBIT_NUM_INT);
	bitmap_copy_le(&dst3, intr->bitmap_fromhost_m4, MIFINTRBIT_NUM_INT);
}

static void mifiintrman_isr(int irq, void *data)
{
	struct mifintrbit *intr = (struct mifintrbit *)data;
	unsigned long     flags;
	unsigned long int irq_reg = 0;
	int               bit;

	/* Avoid unused parameter error */
	(void)irq;

	spin_lock_irqsave(&intr->spinlock, flags);
	irq_reg = intr->mif->irq_get(intr->mif);

	print_bitmaps(intr);
	for_each_set_bit(bit, &irq_reg, MIFINTRBIT_NUM_INT) {
		if (intr->mifintrbit_irq_handler[bit] != mifintrbit_default_handler)
			intr->mifintrbit_irq_handler[bit](bit, intr->irq_data[bit]);
	}

	spin_unlock_irqrestore(&intr->spinlock, flags);
}

/* Public functions */
int mifintrbit_alloc_tohost(struct mifintrbit *intr, mifintrbit_handler handler, void *data)
{
	struct scsc_mif_abs *mif;
	unsigned long flags;
	int           which_bit = 0;

	spin_lock_irqsave(&intr->spinlock, flags);

	/* Search for free slots */
	which_bit = find_first_zero_bit(intr->bitmap_tohost, MIFINTRBIT_NUM_INT);

	if (which_bit >= MIFINTRBIT_NUM_INT)
		goto error;

	if (intr->mifintrbit_irq_handler[which_bit] != mifintrbit_default_handler) {
		spin_unlock_irqrestore(&intr->spinlock, flags);
		goto error;
	}

	/* Get abs implementation */
	mif = intr->mif;

	/* Mask to prevent spurious incoming interrupts */
	mif->irq_bit_mask(mif, which_bit);
	/* Clear the interrupt */
	mif->irq_bit_clear(mif, which_bit);

	/* Register the handler */
	intr->mifintrbit_irq_handler[which_bit] = handler;
	intr->irq_data[which_bit] = data;

	/* Once registration is set, and IRQ has been cleared, unmask the interrupt */
	mif->irq_bit_unmask(mif, which_bit);

	/* Update bit mask */
	set_bit(which_bit, intr->bitmap_tohost);

	spin_unlock_irqrestore(&intr->spinlock, flags);

	return which_bit;

error:
	spin_unlock_irqrestore(&intr->spinlock, flags);
	SCSC_TAG_ERR(MIF, "Error registering irq\n");
	return -EIO;
}

int mifintrbit_free_tohost(struct mifintrbit *intr, int which_bit)
{
	struct scsc_mif_abs *mif;
	unsigned long flags;

	if (which_bit >= MIFINTRBIT_NUM_INT)
		goto error;

	spin_lock_irqsave(&intr->spinlock, flags);
	/* Get abs implementation */
	mif = intr->mif;

	/* Mask to prevent spurious incoming interrupts */
	mif->irq_bit_mask(mif, which_bit);
	/* Set the handler with default */
	intr->mifintrbit_irq_handler[which_bit] = mifintrbit_default_handler;
	intr->irq_data[which_bit] = NULL;
	/* Clear the interrupt for hygiene */
	mif->irq_bit_clear(mif, which_bit);
	/* Update bit mask */
	clear_bit(which_bit, intr->bitmap_tohost);
	spin_unlock_irqrestore(&intr->spinlock, flags);

	return 0;

error:
	SCSC_TAG_ERR(MIF, "Error unregistering irq\n");
	return -EIO;
}

int mifintrbit_alloc_fromhost(struct mifintrbit *intr, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	int           which_bit = 0;
	unsigned long *p;


	spin_lock_irqsave(&intr->spinlock, flags);

	if (target == SCSC_MIF_ABS_TARGET_R4)
		p = intr->bitmap_fromhost_r4;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (target == SCSC_MIF_ABS_TARGET_M4)
		p = intr->bitmap_fromhost_r4;
	else if (target == SCSC_MIF_ABS_TARGET_M4_1)
		p = intr->bitmap_fromhost_r4;
#else
	else if (target == SCSC_MIF_ABS_TARGET_M4)
		p = intr->bitmap_fromhost_m4;
#endif
	else
		goto error;

	/* Search for free slots */
	which_bit = find_first_zero_bit(p, MIFINTRBIT_NUM_INT);

	if (which_bit == MIFINTRBIT_NUM_INT)
		goto error;

	/* Update bit mask */
	set_bit(which_bit, p);

	spin_unlock_irqrestore(&intr->spinlock, flags);

	return which_bit;
error:
	spin_unlock_irqrestore(&intr->spinlock, flags);
	SCSC_TAG_ERR(MIF, "Error allocating bit %d on %s\n",
		     which_bit, target ? "M4" : "R4");
	return -EIO;
}

int mifintrbit_free_fromhost(struct mifintrbit *intr, int which_bit, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	unsigned long *p;

	spin_lock_irqsave(&intr->spinlock, flags);

	if (which_bit >= MIFINTRBIT_NUM_INT)
		goto error;

	if (target == SCSC_MIF_ABS_TARGET_R4)
		p = intr->bitmap_fromhost_r4;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (target == SCSC_MIF_ABS_TARGET_M4)
		p = intr->bitmap_fromhost_r4;
	else if (target == SCSC_MIF_ABS_TARGET_M4_1)
		p = intr->bitmap_fromhost_r4;
#else
	else if (target == SCSC_MIF_ABS_TARGET_M4)
		p = intr->bitmap_fromhost_m4;
#endif
	else
		goto error;

	/* Clear bit mask */
	clear_bit(which_bit, p);
	spin_unlock_irqrestore(&intr->spinlock, flags);

	return 0;
error:
	spin_unlock_irqrestore(&intr->spinlock, flags);
	SCSC_TAG_ERR(MIF, "Error freeing bit %d on %s\n",
		     which_bit, target ? "M4" : "R4");
	return -EIO;
}

/* core API */
void mifintrbit_deinit(struct mifintrbit *intr)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&intr->spinlock, flags);
	/* Set all handlers to default before unregistering the handler */
	for (i = 0; i < MIFINTRBIT_NUM_INT; i++)
		intr->mifintrbit_irq_handler[i] = mifintrbit_default_handler;
	intr->mif->irq_unreg_handler(intr->mif);
	spin_unlock_irqrestore(&intr->spinlock, flags);
}

void mifintrbit_init(struct mifintrbit *intr, struct scsc_mif_abs *mif)
{
	int i;

	spin_lock_init(&intr->spinlock);
	/* Set all handlersd to default before hooking the hardware interrupt */
	for (i = 0; i < MIFINTRBIT_NUM_INT; i++)
		intr->mifintrbit_irq_handler[i] = mifintrbit_default_handler;

	/* reset bitmaps */
	bitmap_zero(intr->bitmap_tohost, MIFINTRBIT_NUM_INT);
	bitmap_zero(intr->bitmap_fromhost_r4, MIFINTRBIT_NUM_INT);
	bitmap_zero(intr->bitmap_fromhost_m4, MIFINTRBIT_NUM_INT);

	/**
	 * Pre-allocate/reserve MIF interrupt bits 0 in both
	 * .._fromhost_r4 and .._fromhost_m4 interrupt bits.
	 *
	 * These bits are used for purpose of forcing Panics from
	 * either MX manager or GDB monitor channels.
	 */
	set_bit(MIFINTRBIT_RESERVED_PANIC_R4, intr->bitmap_fromhost_r4);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	set_bit(MIFINTRBIT_RESERVED_PANIC_M4, intr->bitmap_fromhost_m4);
	set_bit(MIFINTRBIT_RESERVED_PANIC_M4_1, intr->bitmap_fromhost_m4_1);
#else
	set_bit(MIFINTRBIT_RESERVED_PANIC_M4, intr->bitmap_fromhost_m4);
#endif

	/* register isr with mif abstraction */
	mif->irq_reg_handler(mif, mifiintrman_isr, (void *)intr);

	/* cache mif */
	intr->mif = mif;
}
