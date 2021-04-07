/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Mailbox Interrupt Generator Emulation (Interface)
*
****************************************************************************/

#ifndef __PCIE_MBOX_INTGEN_H
#define __PCIE_MBOX_INTGEN_H

/* Uses */

#include "linux/types.h"

/* Forward Types */

struct pcie_mbox_intgen_shared_data;
struct functor;

/* Public Types */

/**
 * Maxwell Mailbox Interrupt Generator Emulation descriptor
 *
 * Uses structures in shared memory to emulate the Interrupt Generation
 * Hardware in conjunction with matching logic in R4 & M4 firmware.
 *
 * This is used to implement 3 instances of the Interrupt Generation (intgen)
 * hardware (to ap, to r4 & to m4).
 */
struct pcie_mbox_intgen {
	char name[16];
	struct pcie_mbox_intgen_shared_data     *shared_data;
	struct functor                          *trigger_interrupt_fn;
};

/* Public Functions */

/**
 * Initialise this Interrupt Generator.
 */
void pcie_mbox_intgen_init(

	/** This Interrupt Generator emulation */
	struct pcie_mbox_intgen                 *intgen,

	/** Name for debugging purposes */
	const char                              *name,

	/** Pointer to shared data for this emulation */
	struct pcie_mbox_intgen_shared_data     *shared_data,

	/**
	* Functor to trigger associated hardware interrupt.
	*
	* The trigger is abstracted so that the same logic can be
	* re-used for the incoming and outgoing emulations.
	*/
	struct functor                          *trigger_interrupt_fn
	);

/**
 * Get this Interrupt Generator's source masked state as a bitmask.
 */
u32 pcie_mbox_intgen_get_masked_bitmask(const struct pcie_mbox_intgen *intgen);

/**
 * Get this Interrupt Generator's source pending state (set and not masked) as a bitmask.
 */
u32 pcie_mbox_intgen_get_pending_bitmask(const struct pcie_mbox_intgen *intgen);

/**
 * Set specified Interrupt Generator source.
 *
 * Triggers interrupt on the interrupt target if the source is not masked.
 */
void pcie_mbox_intgen_set_source(struct pcie_mbox_intgen *intgen, int source_num);

/**
 * Clear specified Interrupt Generator source.
 */
void pcie_mbox_intgen_clear_source(struct pcie_mbox_intgen *intgen, int source_num);

/**
 * Mask specified Interrupt Generator source.
 */
void pcie_mbox_intgen_mask_source(struct pcie_mbox_intgen *intgen, int source_num);

/**
 * Unmask specified Interrupt Generator source.
 *
 * The associated hardware interrupt will be triggered if
 * the specified source is currently set.
 */
void pcie_mbox_intgen_unmask_source(struct pcie_mbox_intgen *intgen, int source_num);

/**
 * Is the specified interrupt source pending (set and not masked)?
 */
bool pcie_mbox_intgen_is_source_pending(const struct pcie_mbox_intgen *intgen, int source_num);


#endif /* __PCIE_MBOX_INTGEN_H */

