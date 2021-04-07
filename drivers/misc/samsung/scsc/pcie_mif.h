/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

#ifndef __PCIE_MIF_H
#define __PCIE_MIF_H
#include <linux/pci.h>
#include "scsc_mif_abs.h"

#ifdef CONDOR
#define FPGA_OFFSET             0xb8000000
#else
#define FPGA_OFFSET             0x80000000
#endif

#define SCSC_PCIE_MAGIC_VAL             0xdeadbeef

#define SCSC_PCIE_GRST_OFFSET 0x48

/* BAR0 Registers [PCIeBridgeBAR0Regs.doc] */
#define SCSC_PCIE_NEWMSG        0x0
#define SCSC_PCIE_SIGNATURE     0x4
#define SCSC_PCIE_OFFSET        0x8
#define SCSC_PCIE_RUNEN         0xC
#define SCSC_PCIE_DEBUG         0x10
#define SCSC_PCIE_AXIWCNT       0x14
#define SCSC_PCIE_AXIRCNT       0x18
#define SCSC_PCIE_AXIWADDR      0x1C
#define SCSC_PCIE_AXIRADDR      0x20
#define SCSC_PCIE_TBD           0x24
#define SCSC_PCIE_AXICTRL       0x28
#define SCSC_PCIE_AXIDATA       0x2C
#define SCSC_PCIE_AXIRDBP       0x30
#define SCSC_PCIE_IFAXIWCNT     0x34
#define SCSC_PCIE_IFAXIRCNT     0x38
#define SCSC_PCIE_IFAXIWADDR    0x3C
#define SCSC_PCIE_IFAXIRADDR    0x40
#define SCSC_PCIE_IFAXICTRL     0x44
#define SCSC_PCIE_GRST          0x48
#define SCSC_PCIE_AMBA2TRANSAXIWCNT     0x4C
#define SCSC_PCIE_AMBA2TRANSAXIRCNT     0x50
#define SCSC_PCIE_AMBA2TRANSAXIWADDR    0x54
#define SCSC_PCIE_AMBA2TRANSAXIRADDR    0x58
#define SCSC_PCIE_AMBA2TRANSAXICTR      0x5C
#define SCSC_PCIE_TRANS2PCIEREADALIGNAXIWCNT    0x60
#define SCSC_PCIE_TRANS2PCIEREADALIGNAXIRCNT    0x64
#define SCSC_PCIE_TRANS2PCIEREADALIGNAXIWADDR   0x68
#define SCSC_PCIE_TRANS2PCIEREADALIGNAXIRADDR   0x6C
#define SCSC_PCIE_TRANS2PCIEREADALIGNAXICTRL    0x70
#define SCSC_PCIE_READROUNDTRIPMIN      0x74
#define SCSC_PCIE_READROUNDTRIPMAX      0x78
#define SCSC_PCIE_READROUNDTRIPLAST     0x7C
#define SCSC_PCIE_CPTAW0        0x80
#define SCSC_PCIE_CPTAW1        0x84
#define SCSC_PCIE_CPTAR0        0x88
#define SCSC_PCIE_CPTAR1        0x8C
#define SCSC_PCIE_CPTB0         0x90
#define SCSC_PCIE_CPTW0         0x94
#define SCSC_PCIE_CPTW1         0x98
#define SCSC_PCIE_CPTW2         0x9C
#define SCSC_PCIE_CPTR0         0xA0
#define SCSC_PCIE_CPTR1         0xA4
#define SCSC_PCIE_CPTR2         0xA8
#define SCSC_PCIE_CPTRES        0xAC
#define SCSC_PCIE_CPTAWDELAY    0xB0
#define SCSC_PCIE_CPTARDELAY    0xB4
#define SCSC_PCIE_CPTSRTADDR    0xB8
#define SCSC_PCIE_CPTENDADDR    0xBC
#define SCSC_PCIE_CPTSZLTHID    0xC0
#define SCSC_PCIE_CPTPHSEL      0xC4
#define SCSC_PCIE_CPTRUN        0xC8
#define SCSC_PCIE_FPGAVER       0xCC

/* from mx141 */
#define SCSC_PCIE_NEWMSG2       0xD0
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
#define SCSC_PCIE_NEWMSG3       0xD4
#endif

struct scsc_bar0_reg {
	u32 NEWMSG;
	u32 SIGNATURE;
	u32 OFFSET;
	u32 RUNEN;
	u32 DEBUG;
	u32 AXIWCNT;
	u32 AXIRCNT;
	u32 AXIWADDR;
	u32 AXIRADDR;
	u32 TBD;
	u32 AXICTRL;
	u32 AXIDATA;
	u32 AXIRDBP;
	u32 IFAXIWCNT;
	u32 IFAXIRCNT;
	u32 IFAXIWADDR;
	u32 IFAXIRADDR;
	u32 IFAXICTRL;
	u32 GRST;
	u32 AMBA2TRANSAXIWCNT;
	u32 AMBA2TRANSAXIRCNT;
	u32 AMBA2TRANSAXIWADDR;
	u32 AMBA2TRANSAXIRADDR;
	u32 AMBA2TRANSAXICTR;
	u32 TRANS2PCIEREADALIGNAXIWCNT;
	u32 TRANS2PCIEREADALIGNAXIRCNT;
	u32 TRANS2PCIEREADALIGNAXIWADDR;
	u32 TRANS2PCIEREADALIGNAXIRADDR;
	u32 TRANS2PCIEREADALIGNAXICTRL;
	u32 READROUNDTRIPMIN;
	u32 READROUNDTRIPMAX;
	u32 READROUNDTRIPLAST;
	u32 CPTAW0;
	u32 CPTAW1;
	u32 CPTAR0;
	u32 CPTAR1;
	u32 CPTB0;
	u32 CPTW0;
	u32 CPTW1;
	u32 CPTW2;
	u32 CPTR0;
	u32 CPTR1;
	u32 CPTR2;
	u32 CPTRES;
	u32 CPTAWDELAY;
	u32 CPTARDELAY;
	u32 CPTSRTADDR;
	u32 CPTENDADDR;
	u32 CPTSZLTHID;
	u32 CPTPHSEL;
	u32 CPTRUN;
	u32 FPGAVER;

	/* from mx141 */
	u32 NEWMSG2;
};

struct scsc_mif_abs *pcie_mif_create(struct pci_dev *pdev, const struct pci_device_id *id);
void pcie_mif_destroy_pcie(struct pci_dev *pdev, struct scsc_mif_abs *interface);
struct pci_dev      *pcie_mif_get_pci_dev(struct scsc_mif_abs *interface);
struct device       *pcie_mif_get_dev(struct scsc_mif_abs *interface);

struct pcie_mif;

void pcie_mif_get_bar0(struct pcie_mif *pcie, struct scsc_bar0_reg *bar0);
int pcie_mif_set_bar0_register(struct pcie_mif *pcie, unsigned int value, unsigned int offset);

#endif
