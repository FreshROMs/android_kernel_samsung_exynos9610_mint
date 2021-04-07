/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Software Mailbox Emulation shared data definitions.
*
* Ref: SC-506707-DD - Structure version 2
*
****************************************************************************/

#ifndef __PCIE_MBOX_SHARED_DATA_DEFS_H
#define __PCIE_MBOX_SHARED_DATA_DEFS_H

/* Defines */

/** Offset of shared data structure from end of shared ram */
#define PCIE_MIF_MBOX_RESERVED_LEN              (0x400)

#define PCIE_MIF_MBOX_MAGIC_NUMBER              (0x3a11b0c5)

#define PCIE_MIF_MBOX_VERSION_NUMBER            (0x00000002)

/**
 * Number of mailboxes.
 *
 * Note: Current hardware supports 16 mailboxes. The extra mailboxes
 * in the emulation may be used to emulate other signals.
 */
#define PCIE_MIF_MBOX_ISSR_COUNT                (32)

/**
 * Number of interrupt sources per Interrupt Generator Emulation instance.
 *
 * Note: Current hardware supports 16 sources. The extra sources
 * in the emulation may be used to emulate other signals
 * (e.g. RESET_REQUEST from MX to AP).
 *
 */
#define PCIE_MIF_MBOX_NUM_INTR_SOURCES          (32)

/**
 * Structure must be packed.
 */
#define PCI_MBOX_SHARED_DATA_ATTR               __packed

/**
 * Write barrier for syncing writes to  pcie_mbox_shared_data
 * shared data area.
 *
 * HERE: Can we use something lighter? E.g. dma_wmb()?
 */
#define pcie_mbox_shared_data_wmb()             wmb()

#endif /* __PCIE_MBOX_SHARED_DATA_DEFS_H */

