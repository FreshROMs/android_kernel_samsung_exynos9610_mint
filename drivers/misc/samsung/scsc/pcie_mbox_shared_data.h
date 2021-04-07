/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Software Mailbox Emulation shared data structure.
*
* Ref: SC-506707-DD - Structure version 2
*
****************************************************************************/

#ifndef __PCIE_MBOX_SHARED_DATA_H
#define __PCIE_MBOX_SHARED_DATA_H

/* Uses */

#include "pcie_mbox_shared_data_defs.h"
#include "pcie_mbox_intgen_shared_data.h"

/* Types */

/**
 * Mailbox Emulation Generator shared state.
 *
 * Notes:
 * - Structure must be packed.
 * - All integers are LittleEndian.
 */
PCI_MBOX_SHARED_DATA_ATTR struct pcie_mbox_shared_data {
	uint32_t mailbox[PCIE_MIF_MBOX_ISSR_COUNT];
	uint32_t magic;
	uint32_t version;
	struct pcie_mbox_intgen_shared_data ap_interrupt;
	struct pcie_mbox_intgen_shared_data r4_interrupt;
	struct pcie_mbox_intgen_shared_data m4_interrupt;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct pcie_mbox_intgen_shared_data m4_1_interrupt;
#endif
};

#endif /* __PCIE_MBOX_SHARED_DATA_H */

