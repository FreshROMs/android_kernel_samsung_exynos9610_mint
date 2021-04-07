/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Software Mailbox Interrupt Generator Emulation shared data
* structure.
*
****************************************************************************/

#ifndef __PCIE_MBOX_INTGEN_SHARED_DATA_H
#define __PCIE_MBOX_INTGEN_SHARED_DATA_H

/* Uses */

#include "pcie_mbox_shared_data_defs.h"
#include "linux/types.h"

/* Types */

/**
 * Mailbox Interrupt Generator Emulation shared state.
 *
 * Notes:
 * - Structure must be packed.
 * - All integers are LittleEndian.
 */
PCI_MBOX_SHARED_DATA_ATTR struct pcie_mbox_intgen_shared_data {
	/** Interrupt source mask state (whole word each to avoid RMW issues) */
	uint32_t mask[PCIE_MIF_MBOX_NUM_INTR_SOURCES];
	/** Interrupt source set state (whole word each to avoid RMW issues) */
	uint32_t status[PCIE_MIF_MBOX_NUM_INTR_SOURCES];
};

#endif /* __PCIE_MBOX_INTGEN_SHARED_DATA_H */

