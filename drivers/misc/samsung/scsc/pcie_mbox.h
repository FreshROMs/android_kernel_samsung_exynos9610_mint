/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Mailbox Hardware Emulation (Interface)
*
****************************************************************************/

#ifndef __PCIE_MBOX_H
#define __PCIE_MBOX_H

/* Uses */

#include "scsc_mif_abs.h"               /* for enum scsc_mif_abs_target */
#include "pcie_mbox_intgen.h"

/* Forward */

struct pcie_mbox_shared_data;

/* Types */

/**
 * Maxwell Mailbox Hardware Emulation.
 *
 * Uses structure in shared memory to emulate the MX Mailbox Hardware in
 * conjunction with matching logic in R4 & M4 firmware.
 *
 * The emulated hardware includes an array of simple 32 bit mailboxes and
 * 3 instances of Interrupt Generator (intgen) hardware (to ap, to r4 & to m4).
 */
struct pcie_mbox {
	/** Pointer to shared Mailbox emulation state */
	struct pcie_mbox_shared_data    *shared_data;

	/** Interrupt Generator Emulations */
	struct pcie_mbox_intgen ap_intgen;
	struct pcie_mbox_intgen r4_intgen;
	struct pcie_mbox_intgen m4_intgen;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct pcie_mbox_intgen m4_intgen_1;
#endif
};

/* Public Functions */

/**
 * Initialise the mailbox emulation.
 */
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
	);

/**
 * Get the AP interrupt source mask state as a bitmask.
 */
u32 pcie_mbox_get_ap_interrupt_masked_bitmask(const struct pcie_mbox *mbox);

/**
 * Get the AP interrupt source pending (set and not masked) state as a bitmask.
 */
u32 pcie_mbox_get_ap_interrupt_pending_bitmask(const struct pcie_mbox *mbox);

/**
 * Is the specified AP interrupt source pending (set and not masked)?
 */
bool pcie_mbox_is_ap_interrupt_source_pending(const struct pcie_mbox *mbox, int source_num);

/**
 * Clear the specified AP interrupt source.
 */
void pcie_mbox_clear_ap_interrupt_source(struct pcie_mbox *mbox, int source_num);

/**
 * Mask the specified AP interrupt source.
 */
void pcie_mbox_mask_ap_interrupt_source(struct pcie_mbox *mbox, int source_num);

/**
 * Unmask the specified AP interrupt source.
 *
 * The interrupt will trigger if the source is currently set.
 */
void pcie_mbox_unmask_ap_interrupt_source(struct pcie_mbox *mbox, int source_num);

/**
 * Set an outgoing interrupt source to R4 or M4 node.
 *
 * Triggers interrupt in target node if the source is not masked.
 */
void pcie_mbox_set_outgoing_interrupt_source(struct pcie_mbox *mbox, enum scsc_mif_abs_target target_node, int source_num);

/**
 * Get pointer to the specified 32bit Mailbox in shared memory.
 */
u32 *pcie_mbox_get_mailbox_ptr(struct pcie_mbox *mbox, u32 mbox_index);

#endif /* __PCIE_MBOX_H */

