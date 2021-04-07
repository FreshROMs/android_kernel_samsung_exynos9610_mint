/****************************************************************************
*
* Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

/* Implements */

#include "pcie_mif.h"

/* Uses */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/moduleparam.h>
#include <asm/barrier.h>
#include <scsc/scsc_logring.h>
#include "pcie_mif_module.h"
#include "pcie_proc.h"
#include "pcie_mbox.h"
#include "functor.h"

#define PCIE_MIF_RESET_REQUEST_SOURCE 31

/* Module parameters */

static bool enable_pcie_mif_arm_reset = true;
module_param(enable_pcie_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_mif_arm_reset, "Enables ARM cores reset");

/* Types */

struct pcie_mif {
	struct scsc_mif_abs interface;
	struct pci_dev        *pdev;
	int dma_using_dac;            /* =1 if 64-bit DMA is used, =0 otherwise. */
	__iomem void          *registers;

	struct device         *dev;

	u8                    *mem;             /* DMA memory mapped to PCIe space for MX-AP comms */
	struct pcie_mbox mbox;                  /* mailbox emulation */
	size_t mem_allocated;
	dma_addr_t dma_addr;

	/* Callback function and dev pointer mif_intr manager handler */
	void (*r4_handler)(int irq, void *data);
	void                  *irq_dev;

	/* Reset Request handler and context */
	void (*reset_request_handler)(int irq_num_ignored, void *data);
	void *reset_request_handler_data;

	/**
	 * Functors to trigger, or simulate, MIF WLBT Mailbox interrupts.
	 *
	 * These functors isolates the Interrupt Generator logic
	 * from differences in physical interrupt generation.
	 */
	struct functor trigger_ap_interrupt;
	struct functor trigger_r4_interrupt;
	struct functor trigger_m4_interrupt;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct functor trigger_m4_1_interrupt;
#endif
};

/* Private Macros */

/** Upcast from interface member to pcie_mif */
#define pcie_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct pcie_mif, interface)

/** Upcast from trigger_ap_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_ap_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_ap_interrupt)

/** Upcast from trigger_r4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_r4_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_r4_interrupt)

/** Upcast from trigger_m4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_m4_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_m4_interrupt)

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
/** Upcast from trigger_m4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_m4_1_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_m4_1_interrupt)
#endif

/* Private Functions */

static void pcie_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;
}

static void pcie_mif_emulate_reset_request_interrupt(struct pcie_mif *pcie)
{
	/* The RESET_REQUEST interrupt is emulated over PCIe using a spare MIF interrupt source */
	if (pcie_mbox_is_ap_interrupt_source_pending(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE)) {
		/* Invoke handler if registered */
		if (pcie->reset_request_handler)
			pcie->reset_request_handler(0, pcie->reset_request_handler_data);
		/* Clear the source to emulate hardware interrupt behaviour */
		pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
	}
}

#ifdef CONFIG_SCSC_QOS
static int pcie_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	return 0;
}

static int pcie_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	return 0;
}

static int pcie_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	return 0;
}
#endif

irqreturn_t pcie_mif_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG(PCIE_MIF, "MIF Interrupt Received. (Pending 0x%08x, Mask 0x%08x)\n",
		pcie_mbox_get_ap_interrupt_pending_bitmask(&pcie->mbox),
		pcie_mbox_get_ap_interrupt_masked_bitmask(&pcie->mbox)
	);

	/*
	 * Intercept mailbox interrupt sources (numbers > 15) used to emulate other
	 * signalling paths missing from emulator/PCIe hardware.
	 */
	pcie_mif_emulate_reset_request_interrupt(pcie);

	/* Invoke the normal MIF interrupt handler */
	if (pcie->r4_handler != pcie_mif_irq_default_handler)
		pcie->r4_handler(irq, pcie->irq_dev);
	else
		SCSC_TAG_INFO(PCIE_MIF, "Any handler registered\n");

	return IRQ_HANDLED;
}

/**
 * Trigger, or simulate, inbound (to AP) PCIe interrupt.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_ap_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_ap_interrupt(trigger);

	/*
	 * Invoke the normal isr handler synchronously.
	 *
	 * If synchronous handling proves problematic then launch
	 * an async task or trigger GIC interrupt manually (if supported).
	 */
	(void)pcie_mif_isr(0, (void *)pcie);
};

/**
 * Trigger PCIe interrupt to R4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_r4_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_r4_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering R4 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG);
	mmiowb();
};

/**
 * Trigger PCIe interrupt to M4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_m4_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_m4_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering M4 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG2);
	mmiowb();
};

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
/**
 * Trigger PCIe interrupt to M4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_m4_1_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_m4_1_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering M4 1 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG3);
	mmiowb();
};
#endif

static void pcie_mif_destroy(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
}

static char *pcie_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	/* TODO */
	/* return "0" for the time being */
	return "0";
}

static int pcie_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	int ret;

	if (enable_pcie_mif_arm_reset || !reset) {
		/* Sanity check */
		iowrite32(0xdeadbeef, pcie->registers + SCSC_PCIE_SIGNATURE);
		mmiowb();
		ret = ioread32(pcie->registers + SCSC_PCIE_SIGNATURE);
		if (ret != 0xdeadbeef) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Can't acces BAR0 magic number. Readed: 0x%x Expected: 0x%x\n",
					 ret, 0xdeadbeef);
			return -ENODEV;
		}

		iowrite32(reset ? 1 : 0,
			  pcie->registers + SCSC_PCIE_GRST_OFFSET);
		mmiowb();
	} else
		SCSC_TAG_INFO(PCIE_MIF, "Not resetting ARM Cores enable_pcie_mif_arm_reset: %d\n",
			      enable_pcie_mif_arm_reset);
	return 0;
}

static void *pcie_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	int ret;
	size_t map_len = PCIE_MIF_ALLOC_MEM;

	if (allocated)
		*allocated = 0;

	if (map_len > (PCIE_MIF_PREALLOC_MEM - 1)) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating DMA memory, requested %zu, maximum %d, consider different size\n", map_len, PCIE_MIF_PREALLOC_MEM);
		return NULL;
	}

	/* should return PAGE_ALIGN Memory */
	pcie->mem = dma_alloc_coherent(pcie->dev,
				       PCIE_MIF_PREALLOC_MEM, &pcie->dma_addr, GFP_KERNEL);
	if (pcie->mem == NULL) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
		return NULL;
	}

	pcie->mem_allocated = map_len;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Allocated dma coherent mem: %p addr %p\n", pcie->mem, (void *)pcie->dma_addr);

	iowrite32((unsigned int)pcie->dma_addr,
		  pcie->registers + SCSC_PCIE_OFFSET);
	mmiowb();
	ret = ioread32(pcie->registers + SCSC_PCIE_OFFSET);
	SCSC_TAG_INFO(PCIE_MIF, "Read SHARED_BA 0x%0x\n", ret);
	if (ret != (unsigned int)pcie->dma_addr) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Can't acces BAR0 Shared BA. Readed: 0x%x Expected: 0x%x\n", ret, (unsigned int)pcie->dma_addr);
		return NULL;
	}

	/* Initialised the interrupt trigger functors required by mbox emulation */
	functor_init(&pcie->trigger_ap_interrupt, pcie_mif_trigger_ap_interrupt);
	functor_init(&pcie->trigger_r4_interrupt, pcie_mif_trigger_r4_interrupt);
	functor_init(&pcie->trigger_m4_interrupt, pcie_mif_trigger_m4_interrupt);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	functor_init(&pcie->trigger_m4_1_interrupt, pcie_mif_trigger_m4_1_interrupt);
#endif

	/* Initialise mailbox emulation to use shared memory at the end of PCIE_MIF_PREALLOC_MEM */
	pcie_mbox_init(
		&pcie->mbox,
		pcie->mem + PCIE_MIF_PREALLOC_MEM - PCIE_MIF_MBOX_RESERVED_LEN,
		pcie->registers,
		&pcie->trigger_ap_interrupt,
		&pcie->trigger_r4_interrupt,
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		&pcie->trigger_m4_interrupt,
		&pcie->trigger_m4_1_interrupt
#else
		&pcie->trigger_m4_interrupt
#endif
		);

	/* Return the max allocatable memory on this abs. implementation */
	if (allocated)
		*allocated = map_len;

	return pcie->mem;
}

/* HERE: Not sure why mem is passed in - its stored in pcie - as it should be */
static void pcie_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	dma_free_coherent(pcie->dev, PCIE_MIF_PREALLOC_MEM, pcie->mem, pcie->dma_addr);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Freed dma coherent mem: %p addr %p\n", pcie->mem, (void *)pcie->dma_addr);
}

static u32 pcie_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_masked_bitmask(&pcie->mbox);
}

static u32 pcie_mif_irq_get(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_pending_bitmask(&pcie->mbox);
}

static void pcie_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation component */
	pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, bit_num);
}

static void pcie_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox interrupt emulation component */
	pcie_mbox_mask_ap_interrupt_source(&pcie->mbox, bit_num);
}

static void pcie_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation component */
	pcie_mbox_unmask_ap_interrupt_source(&pcie->mbox, bit_num);
}

static void pcie_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif                 *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation sub-module */
	pcie_mbox_set_outgoing_interrupt_source(&pcie->mbox, target, bit_num);
}

static void pcie_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->r4_handler = handler;
	pcie->irq_dev = dev;
}

static void pcie_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->r4_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;
}

static void pcie_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->reset_request_handler = handler;
	pcie->reset_request_handler_data = dev;

	pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
	pcie_mbox_unmask_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
}

static void pcie_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie_mbox_mask_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
	pcie->reset_request_handler = NULL;
}

static u32 *pcie_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	/* delegate to mbox emulation sub-module */
	return pcie_mbox_get_mailbox_ptr(&pcie->mbox, mbox_index);
}

static int pcie_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	if ((u8 *)ptr > (pcie->mem + 4 * 1024 * 1024)) {
		SCSC_TAG_ERR(PCIE_MIF, "ooops limits reached\n");
		return -ENOMEM;
	}

	/* Ref is byte offset wrt start of shared memory */
	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)pcie->mem);

	return 0;
}

static void *pcie_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "\n");

	/* Check limits */
	if (ref >= 0 && ref < PCIE_MIF_ALLOC_MEM)
		return (void *)((uintptr_t)pcie->mem + (uintptr_t)ref);
	else
		return NULL;
}

static uintptr_t pcie_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	return virt_to_phys(pcie->mem) >> PAGE_SHIFT;
}

static struct device *pcie_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	return pcie->dev;
}

static void pcie_mif_irq_clear(void)
{
}

static void pcie_mif_dump_register(struct scsc_mif_abs *interface)
{
}

struct scsc_mif_abs *pcie_mif_create(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	struct scsc_mif_abs *pcie_if;
	struct pcie_mif     *pcie = (struct pcie_mif *)devm_kzalloc(&pdev->dev, sizeof(struct pcie_mif), GFP_KERNEL);
	u16 cmd;

	/* Avoid unused parameter error */
	(void)id;

	if (!pcie)
		return NULL;

	pcie_if = &pcie->interface;

	/* initialise interface structure */
	pcie_if->destroy = pcie_mif_destroy;
	pcie_if->get_uid = pcie_mif_get_uid;
	pcie_if->reset = pcie_mif_reset;
	pcie_if->map = pcie_mif_map;
	pcie_if->unmap = pcie_mif_unmap;
#ifdef MAILBOX_SETGET
	pcie_if->mailbox_set = pcie_mif_mailbox_set;
	pcie_if->mailbox_get = pcie_mif_mailbox_get;
#endif
	pcie_if->irq_bit_set = pcie_mif_irq_bit_set;
	pcie_if->irq_get = pcie_mif_irq_get;
	pcie_if->irq_bit_mask_status_get = pcie_mif_irq_bit_mask_status_get;
	pcie_if->irq_bit_clear = pcie_mif_irq_bit_clear;
	pcie_if->irq_bit_mask = pcie_mif_irq_bit_mask;
	pcie_if->irq_bit_unmask = pcie_mif_irq_bit_unmask;
	pcie_if->irq_reg_handler = pcie_mif_irq_reg_handler;
	pcie_if->irq_unreg_handler = pcie_mif_irq_unreg_handler;
	pcie_if->irq_reg_reset_request_handler = pcie_mif_irq_reg_reset_request_handler;
	pcie_if->irq_unreg_reset_request_handler = pcie_mif_irq_unreg_reset_request_handler;
	pcie_if->get_mbox_ptr = pcie_mif_get_mbox_ptr;
	pcie_if->get_mifram_ptr = pcie_mif_get_mifram_ptr;
	pcie_if->get_mifram_ref = pcie_mif_get_mifram_ref;
	pcie_if->get_mifram_pfn = pcie_mif_get_mif_pfn;
	pcie_if->get_mif_device = pcie_mif_get_mif_device;
	pcie_if->irq_clear = pcie_mif_irq_clear;
	pcie_if->mif_dump_registers = pcie_mif_dump_register;
	pcie_if->mif_read_register = NULL;
#ifdef CONFIG_SCSC_QOS
	pcie_if->mif_pm_qos_add_request = pcie_mif_pm_qos_add_request;
	pcie_if->mif_pm_qos_update_request = pcie_mif_pm_qos_update_request;
	pcie_if->mif_pm_qos_remove_request = pcie_mif_pm_qos_remove_request;
#endif
	/* Suspend/resume not supported in PCIe MIF */
	pcie_if->suspend_reg_handler = NULL;
	pcie_if->suspend_unreg_handler = NULL;

	/* Update state */
	pcie->pdev = pdev;

	pcie->dev = &pdev->dev;

	pcie->r4_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;

	/* Just do whats is necessary to meet the pci probe
	 * -BAR0 stuff
	 * -Interrupt (will be able to handle interrupts?)
	 */

	/* My stuff */
	pci_set_drvdata(pdev, pcie);

	rc = pcim_enable_device(pdev);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "Error enabling device.\n");
		return NULL;
	}

	/* This function returns the flags associated with this resource.*/
	/* esource flags are used to define some features of the individual resource.
	 *      For PCI resources associated with PCI I/O regions, the information is extracted from the base address registers */
	/* IORESOURCE_MEM = If the associated I/O region exists, one and only one of these flags is set */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		SCSC_TAG_ERR(PCIE_MIF, "Incorrect BAR configuration\n");
		return NULL;
	}

	/* old --- rc = pci_request_regions(pdev, "foo"); */
	/* Request and iomap regions specified by @mask (0x01 ---> BAR0)*/
	rc = pcim_iomap_regions(pdev, BIT(0), DRV_NAME);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "pcim_iomap_regions() failed. Aborting.\n");
		return NULL;
	}


	pci_set_master(pdev);

	/* Access iomap allocation table */
	/* return __iomem * const * */
	pcie->registers = pcim_iomap_table(pdev)[0];

	/* Set up a single MSI interrupt */
	if (pci_enable_msi(pdev)) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "Failed to enable MSI interrupts. Aborting.\n");
		return NULL;
	}
	rc = devm_request_irq(&pdev->dev, pdev->irq, pcie_mif_isr, 0,
			      DRV_NAME, pcie);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "Failed to register MSI handler. Aborting.\n");
		return NULL;
	}

/* if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
 *              SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "DMA mask 64bits.\n");
 *                              pcie->dma_using_dac = 1; */
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "DMA mask 32bits.\n");
		pcie->dma_using_dac = 0;
	} else {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to set DMA mask. Aborting.\n");
		return NULL;
	}

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);

	/* Make sure Mx is in the reset state */
	pcie_mif_reset(pcie_if, true);

	/* Create debug proc entry */
	pcie_create_proc_dir(pcie);

	return pcie_if;
}

void pcie_mif_destroy_pcie(struct pci_dev *pdev, struct scsc_mif_abs *interface)
{
	/* Create debug proc entry */
	pcie_remove_proc_dir();

	pci_disable_device(pdev);
}

struct pci_dev *pcie_mif_get_pci_dev(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	BUG_ON(!interface || !pcie);

	return pcie->pdev;
}

struct device *pcie_mif_get_dev(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	BUG_ON(!interface || !pcie);

	return pcie->dev;
}



/* Functions for proc entry */
int pcie_mif_set_bar0_register(struct pcie_mif *pcie, unsigned int value, unsigned int offset)
{
	iowrite32(value, pcie->registers + offset);
	mmiowb();

	return 0;
}

void pcie_mif_get_bar0(struct pcie_mif *pcie, struct scsc_bar0_reg *bar0)
{
	bar0->NEWMSG            = ioread32(pcie->registers + SCSC_PCIE_NEWMSG);
	bar0->SIGNATURE         = ioread32(pcie->registers + SCSC_PCIE_SIGNATURE);
	bar0->OFFSET            = ioread32(pcie->registers + SCSC_PCIE_OFFSET);
	bar0->RUNEN             = ioread32(pcie->registers + SCSC_PCIE_RUNEN);
	bar0->DEBUG             = ioread32(pcie->registers + SCSC_PCIE_DEBUG);
	bar0->AXIWCNT           = ioread32(pcie->registers + SCSC_PCIE_AXIWCNT);
	bar0->AXIRCNT           = ioread32(pcie->registers + SCSC_PCIE_AXIRCNT);
	bar0->AXIWADDR          = ioread32(pcie->registers + SCSC_PCIE_AXIWADDR);
	bar0->AXIRADDR          = ioread32(pcie->registers + SCSC_PCIE_AXIRADDR);
	bar0->TBD               = ioread32(pcie->registers + SCSC_PCIE_TBD);
	bar0->AXICTRL           = ioread32(pcie->registers + SCSC_PCIE_AXICTRL);
	bar0->AXIDATA           = ioread32(pcie->registers + SCSC_PCIE_AXIDATA);
	bar0->AXIRDBP           = ioread32(pcie->registers + SCSC_PCIE_AXIRDBP);
	bar0->IFAXIWCNT         = ioread32(pcie->registers + SCSC_PCIE_IFAXIWCNT);
	bar0->IFAXIRCNT         = ioread32(pcie->registers + SCSC_PCIE_IFAXIRCNT);
	bar0->IFAXIWADDR        = ioread32(pcie->registers + SCSC_PCIE_IFAXIWADDR);
	bar0->IFAXIRADDR        = ioread32(pcie->registers + SCSC_PCIE_IFAXIRADDR);
	bar0->IFAXICTRL         = ioread32(pcie->registers + SCSC_PCIE_IFAXICTRL);
	bar0->GRST              = ioread32(pcie->registers + SCSC_PCIE_GRST);
	bar0->AMBA2TRANSAXIWCNT = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIWCNT);
	bar0->AMBA2TRANSAXIRCNT = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIRCNT);
	bar0->AMBA2TRANSAXIWADDR        = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIWADDR);
	bar0->AMBA2TRANSAXIRADDR        = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIRADDR);
	bar0->AMBA2TRANSAXICTR  = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXICTR);
	bar0->TRANS2PCIEREADALIGNAXIWCNT        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIWCNT);
	bar0->TRANS2PCIEREADALIGNAXIRCNT        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIRCNT);
	bar0->TRANS2PCIEREADALIGNAXIWADDR       = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIWADDR);
	bar0->TRANS2PCIEREADALIGNAXIRADDR       = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIRADDR);
	bar0->TRANS2PCIEREADALIGNAXICTRL        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXICTRL);
	bar0->READROUNDTRIPMIN  = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPMIN);
	bar0->READROUNDTRIPMAX  = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPMAX);
	bar0->READROUNDTRIPLAST = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPLAST);
	bar0->CPTAW0            = ioread32(pcie->registers + SCSC_PCIE_CPTAW0);
	bar0->CPTAW1            = ioread32(pcie->registers + SCSC_PCIE_CPTAW1);
	bar0->CPTAR0            = ioread32(pcie->registers + SCSC_PCIE_CPTAR0);
	bar0->CPTAR1            = ioread32(pcie->registers + SCSC_PCIE_CPTAR1);
	bar0->CPTB0             = ioread32(pcie->registers + SCSC_PCIE_CPTB0);
	bar0->CPTW0             = ioread32(pcie->registers + SCSC_PCIE_CPTW0);
	bar0->CPTW1             = ioread32(pcie->registers + SCSC_PCIE_CPTW1);
	bar0->CPTW2             = ioread32(pcie->registers + SCSC_PCIE_CPTW2);
	bar0->CPTR0             = ioread32(pcie->registers + SCSC_PCIE_CPTR0);
	bar0->CPTR1             = ioread32(pcie->registers + SCSC_PCIE_CPTR1);
	bar0->CPTR2             = ioread32(pcie->registers + SCSC_PCIE_CPTR2);
	bar0->CPTRES            = ioread32(pcie->registers + SCSC_PCIE_CPTRES);
	bar0->CPTAWDELAY        = ioread32(pcie->registers + SCSC_PCIE_CPTAWDELAY);
	bar0->CPTARDELAY        = ioread32(pcie->registers + SCSC_PCIE_CPTARDELAY);
	bar0->CPTSRTADDR        = ioread32(pcie->registers + SCSC_PCIE_CPTSRTADDR);
	bar0->CPTENDADDR        = ioread32(pcie->registers + SCSC_PCIE_CPTENDADDR);
	bar0->CPTSZLTHID        = ioread32(pcie->registers + SCSC_PCIE_CPTSZLTHID);
	bar0->CPTPHSEL          = ioread32(pcie->registers + SCSC_PCIE_CPTPHSEL);
	bar0->CPTRUN            = ioread32(pcie->registers + SCSC_PCIE_CPTRUN);
	bar0->FPGAVER           = ioread32(pcie->registers + SCSC_PCIE_FPGAVER);
}
