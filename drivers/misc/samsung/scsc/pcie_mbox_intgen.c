/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Mailbox Interrupt Generator Emulation (Implementation)
*
****************************************************************************/

/* Implements */

#include "pcie_mbox_intgen.h"

/* Uses */

#include "pcie_mbox_intgen_shared_data.h"
#include "functor.h"
#include <scsc/scsc_logring.h>

/* Private Functions */

/**
 * Trigger the hardware interrupt associated with this Interrupt Generator.
 */
static void pcie_mbox_intgen_trigger_interrupt(struct pcie_mbox_intgen *intgen)
{
	/* Implementation is abstracted to hide the differences from this module */
	functor_call(intgen->trigger_interrupt_fn);
}

/* Public Functions */

void pcie_mbox_intgen_init(
	struct pcie_mbox_intgen                 *intgen,
	const char                              *name,
	struct pcie_mbox_intgen_shared_data     *shared_data,
	struct functor                          *trigger_interrupt_fn
	)
{
	strncpy(intgen->name, name, sizeof(intgen->name));
	intgen->shared_data = shared_data;
	intgen->trigger_interrupt_fn = trigger_interrupt_fn;
}

u32 pcie_mbox_intgen_get_masked_bitmask(const struct pcie_mbox_intgen *intgen)
{
	/* Compile bitmask from the emulation's separate source mask fields */

	u32 masked_bitmask = 0;
	int source_num;

	for (source_num = 0; source_num < PCIE_MIF_MBOX_NUM_INTR_SOURCES; ++source_num)
		if (intgen->shared_data->mask[source_num])
			masked_bitmask |= (1 << source_num);

	return masked_bitmask;
}

bool pcie_mbox_intgen_is_source_pending(const struct pcie_mbox_intgen *intgen, int source_num)
{
	return intgen->shared_data->status[source_num] && !intgen->shared_data->mask[source_num];
}

u32 pcie_mbox_intgen_get_pending_bitmask(const struct pcie_mbox_intgen *intgen)
{
	/* Compile bitmask from the emulation's separate source status and mask fields */

	u32 pending_bitmask = 0;
	int source_num;

	for (source_num = 0; source_num < PCIE_MIF_MBOX_NUM_INTR_SOURCES; ++source_num)
		if (pcie_mbox_intgen_is_source_pending(intgen, source_num))
			pending_bitmask |= (1 << source_num);

	return pending_bitmask;
}

void pcie_mbox_intgen_set_source(struct pcie_mbox_intgen *intgen, int source_num)
{
	if (source_num >= PCIE_MIF_MBOX_NUM_INTR_SOURCES) {
		SCSC_TAG_ERR(PCIE_MIF, "Invalid intgen source %d\n", source_num);
		return;
	}

	SCSC_TAG_DEBUG(PCIE_MIF, "Set source %s:%d. (P %08x, M %08x)\n",
		intgen->name, source_num,
		pcie_mbox_intgen_get_pending_bitmask(intgen),
		pcie_mbox_intgen_get_masked_bitmask(intgen)
	);

	intgen->shared_data->status[source_num] = 1;
	pcie_mbox_shared_data_wmb();
	if (!intgen->shared_data->mask[source_num])
		pcie_mbox_intgen_trigger_interrupt(intgen);
}

void pcie_mbox_intgen_clear_source(struct pcie_mbox_intgen *intgen, int source_num)
{
	if (source_num >= PCIE_MIF_MBOX_NUM_INTR_SOURCES) {
		SCSC_TAG_ERR(PCIE_MIF, "Invalid intgen source %d\n", source_num);
		return;
	}

	SCSC_TAG_DEBUG(PCIE_MIF, "Clear source %s:%d. (P %08x, M %08x)\n",
		intgen->name, source_num,
		pcie_mbox_intgen_get_pending_bitmask(intgen),
		pcie_mbox_intgen_get_masked_bitmask(intgen)
	);

	intgen->shared_data->status[source_num] = 0;
	pcie_mbox_shared_data_wmb();
}

void pcie_mbox_intgen_mask_source(struct pcie_mbox_intgen *intgen, int source_num)
{
	if (source_num >= PCIE_MIF_MBOX_NUM_INTR_SOURCES) {
		SCSC_TAG_ERR(PCIE_MIF, "Invalid intgen source %d\n", source_num);
		return;
	}

	SCSC_TAG_DEBUG(PCIE_MIF, "Mask source %s:%d.\n", intgen->name, source_num);

	intgen->shared_data->mask[source_num] = 1;
	pcie_mbox_shared_data_wmb();
}

void pcie_mbox_intgen_unmask_source(struct pcie_mbox_intgen *intgen, int source_num)
{
	if (source_num >= PCIE_MIF_MBOX_NUM_INTR_SOURCES) {
		SCSC_TAG_ERR(PCIE_MIF, "Invalid intgen source %d\n", source_num);
		return;
	}

	SCSC_TAG_DEBUG(PCIE_MIF, "UnMask source %s:%d.\n", intgen->name, source_num);

	intgen->shared_data->mask[source_num] = 0;
	pcie_mbox_shared_data_wmb();
	if (intgen->shared_data->status[source_num])
		pcie_mbox_intgen_trigger_interrupt(intgen);
}

