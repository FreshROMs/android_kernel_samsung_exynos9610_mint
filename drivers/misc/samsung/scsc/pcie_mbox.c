/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Mailbox Hardware Emulation (Implementation)
*
****************************************************************************/

/* Implements */

#include "pcie_mbox.h"

/* Uses */

#include <linux/pci.h>
#include <asm/barrier.h>
#include <scsc/scsc_logring.h>
#include "pcie_mbox_shared_data.h"
#include "pcie_mbox_intgen.h"

/* Private Functions */

/**
 * Initialise the mailbox emulation shared structure.
 */
static void pcie_mbox_shared_data_init(struct pcie_mbox_shared_data *shared_data)
{
	memset(shared_data, 0, sizeof(*shared_data));
	shared_data->magic = PCIE_MIF_MBOX_MAGIC_NUMBER;
	shared_data->version = PCIE_MIF_MBOX_VERSION_NUMBER;
	pcie_mbox_shared_data_wmb();
}

/* Public Functions */

void pcie_mbox_init(
	struct pcie_mbox        *mbox,
	void                    *shared_data_region,
	__iomem void            *pcie_registers,
	struct functor          *ap_interrupt_trigger,
	struct functor          *r4_interrupt_trigger,
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct functor          *m4_interrupt_trigger,
	struct functor          *m4_1_interrupt_trigger
#else
	struct functor          *m4_interrupt_trigger
#endif
	)
{
	mbox->shared_data = (struct pcie_mbox_shared_data *)shared_data_region;

	pcie_mbox_shared_data_init(mbox->shared_data);

	/* Interrupt Generator Emulations */

	pcie_mbox_intgen_init(&mbox->ap_intgen, "AP", &mbox->shared_data->ap_interrupt, ap_interrupt_trigger);
	pcie_mbox_intgen_init(&mbox->r4_intgen, "R4", &mbox->shared_data->r4_interrupt, r4_interrupt_trigger);
	pcie_mbox_intgen_init(&mbox->m4_intgen, "M4", &mbox->shared_data->m4_interrupt, m4_interrupt_trigger);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	pcie_mbox_intgen_init(&mbox->m4_intgen_1, "M4", &mbox->shared_data->m4_1_interrupt, m4_1_interrupt_trigger);
#endif
}

u32 pcie_mbox_get_ap_interrupt_masked_bitmask(const struct pcie_mbox *mbox)
{
	/* Delegate to ap intgen component */
	return pcie_mbox_intgen_get_masked_bitmask(&mbox->ap_intgen);
}

u32 pcie_mbox_get_ap_interrupt_pending_bitmask(const struct pcie_mbox *mbox)
{
	/* Delegate to ap intgen component */
	return pcie_mbox_intgen_get_pending_bitmask(&mbox->ap_intgen);
}

bool pcie_mbox_is_ap_interrupt_source_pending(const struct pcie_mbox *mbox, int source_num)
{
	return pcie_mbox_intgen_is_source_pending(&mbox->ap_intgen, source_num);
}

void pcie_mbox_clear_ap_interrupt_source(struct pcie_mbox *mbox, int source_num)
{
	/* Delegate to ap intgen component */
	pcie_mbox_intgen_clear_source(&mbox->ap_intgen, source_num);
}

void pcie_mbox_mask_ap_interrupt_source(struct pcie_mbox *mbox, int source_num)
{
	/* Delegate to ap intgen component */
	pcie_mbox_intgen_mask_source(&mbox->ap_intgen, source_num);
}

void pcie_mbox_unmask_ap_interrupt_source(struct pcie_mbox *mbox, int source_num)
{
	/* Delegate to ap intgen component */
	pcie_mbox_intgen_unmask_source(&mbox->ap_intgen, source_num);
}

void pcie_mbox_set_outgoing_interrupt_source(struct pcie_mbox *mbox, enum scsc_mif_abs_target target_node, int source_num)
{
	/* Delegate to appropriate intgen instance*/
	switch (target_node) {
	case SCSC_MIF_ABS_TARGET_R4:
		pcie_mbox_intgen_set_source(&mbox->r4_intgen, source_num);
		break;
	case SCSC_MIF_ABS_TARGET_M4:
		pcie_mbox_intgen_set_source(&mbox->m4_intgen, source_num);
		break;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	case SCSC_MIF_ABS_TARGET_M4_1:
		pcie_mbox_intgen_set_source(&mbox->m4_intgen, source_num);
		break;
#endif
	default:
		SCSC_TAG_ERR(PCIE_MIF, "Invalid interrupt target %d\n", target_node);
		return;
	}
}

u32 *pcie_mbox_get_mailbox_ptr(struct pcie_mbox *mbox, u32 mbox_index)
{
	if (mbox_index >= PCIE_MIF_MBOX_ISSR_COUNT) {
		SCSC_TAG_ERR(PCIE_MIF, "Invalid mailbox index %d\n", mbox_index);
		return NULL;
	}

	return &mbox->shared_data->mailbox[mbox_index];
}

