/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

/* Interfaces it Uses */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/smc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <scsc/scsc_logring.h>
#include "mif_reg_S5E9630.h"
#include "platform_mif_module.h"
#include "mxman.h"
#include "miframman.h"
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#include <linux/soc/samsung/exynos-soc.h>
#endif
#ifdef CONFIG_SOC_EXYNOS9630
#include <linux/mfd/samsung/s2mpu11-regulator.h>
#include "../../../../drivers/soc/samsung/cal-if/acpm_dvfs.h"
#endif

#ifdef CONFIG_SCSC_SMAPPER
#include <linux/dma-mapping.h>
#include "mif_reg_smapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include <linux/pm_qos.h>
#endif

#if !defined(CONFIG_SOC_EXYNOS9630)
#error Target processor CONFIG_SOC_EXYNOS9630 not selected
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
/* Time to wait for CFG_REQ IRQ on 9610 */
#define WLBT_BOOT_TIMEOUT (HZ)

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#ifdef CONFIG_EXYNOS_ITMON
#include <soc/samsung/exynos-itmon.h>
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos-debug.h>
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
#include <linux/workqueue.h>
#endif

static unsigned long sharedmem_base;
static size_t sharedmem_size;

#ifdef CONFIG_SCSC_CHV_SUPPORT
static bool chv_disable_irq;
module_param(chv_disable_irq, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_disable_irq, "Do not register for irq");
#endif

#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
static u32          reg_bkp;
static bool         reg_update;
static void __iomem *gpio_base;
static bool         gpr4_debug;
module_param(gpr4_debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gpr4_debug, "GPR4 PIO muxes switching to the Maxwell. Default = N. Effective on Maxwell power on");
#endif

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static bool init_done;

#ifdef CONFIG_SCSC_QOS
struct qos_table {
	unsigned int freq_mif;
	unsigned int freq_int;
	unsigned int freq_cl0;
	unsigned int freq_cl1;
};
#endif

struct platform_mif {
	struct scsc_mif_abs    interface;
	struct scsc_mbox_s     *mbox;
	struct platform_device *pdev;

	struct device          *dev;

	struct {
		int irq_num;
		int flags;
		atomic_t irq_disabled_cnt;
	} wlbt_irq[PLATFORM_MIF_NUM_IRQS];

	/* MIF registers preserved during suspend */
	struct {
		u32 irq_bit_mask;
	} mif_preserve;

	/* register MBOX memory space */
	size_t        reg_start;
	size_t        reg_size;
	void __iomem  *base;

	/* register CMU memory space */
	struct regmap *cmu_base;

	void __iomem  *con0_base;

	/* pmu syscon regmap */
	struct regmap *pmureg;
#if defined(CONFIG_SOC_EXYNOS9630)
	struct regmap *i3c_apm_pmic;
	struct regmap *dbus_baaw;
	struct regmap *pbus_baaw;
	struct regmap *wlbt_remap;
	struct regmap *boot_cfg;

	/* Signalled when CFG_REQ IRQ handled */
	struct completion cfg_ack;

	/* State of CFG_REQ handler */
	enum wlbt_boot_state {
		WLBT_BOOT_IN_RESET = 0,
		WLBT_BOOT_WAIT_CFG_REQ,
		WLBT_BOOT_ACK_CFG_REQ,
		WLBT_BOOT_CFG_DONE,
		WLBT_BOOT_CFG_ERROR
	} boot_state;

#endif
#ifdef CONFIG_SCSC_SMAPPER
	/* SMAPPER */
	void __iomem  *smapper_base;
	u8            smapper_banks;
	struct {
		u8  bank;
		u32 ws;
		bool large;
		struct scsc_mif_smapper_info bank_info;
	} *smapper;
#endif
	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t        mem_size;
	void __iomem  *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void          (*r4_handler)(int irq, void *data);
	void          *irq_dev;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	void          (*reset_request_handler)(int irq, void *data);
	void          *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
	/* QoS table */
	struct qos_table *qos;
	bool qos_enabled;
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#ifdef CONFIG_EXYNOS_ITMON
	struct notifier_block itmon_nb;
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct work_struct cfgreq_wq;
	struct workqueue_struct *cfgreq_workq;
#endif
	bool reset_failed;
};

static void power_supplies_on(struct platform_mif *platform);
inline void platform_int_debug(struct platform_mif *platform);

extern int mx140_log_dump(void);

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base + offset);
}

inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base + offset);
}

#ifdef CONFIG_SCSC_SMAPPER
inline void platform_mif_reg_write_smapper(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->smapper_base + offset);
}

inline u32 platform_mif_reg_read_smapper(struct platform_mif *platform, u16 offset)
{
	return readl(platform->smapper_base + offset);
}

#define PLATFORM_MIF_SHIFT_SMAPPER_ADDR		11 /* From 36 bits addres to 25 bits */
#define PLATFORM_MIF_SHIFT_SMAPPER_END		4  /* End address aligment */

/* Platform is responsible to give the phys mapping of the SMAPPER maps */
static int platform_mif_smapper_get_mapping(struct scsc_mif_abs *interface, u8 *phy_map, u16 *align)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8 i;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Mapping %d banks\n", platform->smapper_banks);

	if (!platform->smapper_banks)
		return -EINVAL;

	for (i = 0; i < platform->smapper_banks; i++) {
		if (platform->smapper[i].large)
			phy_map[i] = SCSC_MIF_ABS_LARGE_BANK;
		else
			phy_map[i] = SCSC_MIF_ABS_SMALL_BANK;
	}

	if (align)
		*align = 1 << PLATFORM_MIF_SHIFT_SMAPPER_ADDR;

	return 0;
}

static int platform_mif_smapper_get_bank_info(struct scsc_mif_abs *interface, u8 bank, struct scsc_mif_smapper_info *bank_info)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform->smapper_banks)
		return -EINVAL;

	bank_info->num_entries = platform->smapper[bank].bank_info.num_entries;
	bank_info->mem_range_bytes = platform->smapper[bank].bank_info.mem_range_bytes;

	return 0;
}

static u8 platform_mif_smapper_granularity_to_bits(u32 granularity)
{
	if (granularity <= 2 * 1024)
		return 0;
	if (granularity <= 4 * 1024)
		return 1;
	if (granularity <= 8 * 1024)
		return 2;
	if (granularity <= 16 * 1024)
		return 3;
	if (granularity <= 32 * 1024)
		return 4;
	if (granularity <= 64 * 1024)
		return 5;
	if (granularity <= 128 * 1024)
		return 6;
	return 7;
}

static u32 platform_mif_smapper_get_bank_base_address(struct scsc_mif_abs *interface, u8 bank)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform->smapper)
		return 0;

	return platform->smapper[bank].ws;
}

/* Configure smapper according the memory map and range */
static void platform_mif_smapper_configure(struct scsc_mif_abs *interface, u32 granularity)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8 i;
	u8 gran;
	u8 nb = platform->smapper_banks;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Configure SMAPPER with granularity %d\n", granularity);

	gran = platform_mif_smapper_granularity_to_bits(granularity);

	platform_mif_reg_write_smapper(platform, SMAPPER_QCH_DISABLE, 1);
	platform_mif_reg_write_smapper(platform, ORIGIN_ADDR_AR, 0);
	platform_mif_reg_write_smapper(platform, ORIGIN_ADDR_AW, 0);
	/* Program SMAPPER memmap */
	for (i = 0; i < nb; i++) {
		/* Set ADDR_MAP_EN to 1'b0*/
		platform_mif_reg_write_smapper(platform, ADDR_MAP_EN(i), 0);
		/* Set START_ADDR */
		platform_mif_reg_write_smapper(platform, START_ADDR(i), platform->smapper[i].ws);
		/* Set ADDR_GRANULARITY - FIXED AT 4KB */
		platform_mif_reg_write_smapper(platform, ADDR_GRANULARITY(i), gran);
		/* WLAN_ADDR_MAP operation is started */
	}
	/* Set access window control (MSB 32bits Start/End address) */
	/* Remapped address should be ranged from AW_START_ADDR to AW_EN_ADDR */
	platform_mif_reg_write_smapper(platform, AW_START_ADDR, 0);
	platform_mif_reg_write_smapper(platform, AW_END_ADDR, dma_get_mask(platform->dev) >> PLATFORM_MIF_SHIFT_SMAPPER_END);
	smp_mb();
}

/* Caller is responsible of validating the phys address (alignment) */
static int platform_mif_smapper_write_sram(struct scsc_mif_abs *interface, u8 bank, u8 num_entries, u8 first_entry, dma_addr_t *addr)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8 i;
	u32 rb;

	if (!platform->smapper_banks)
		return -EINVAL;

	if (!platform->smapper_base) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "SMAPPER not enabled\n");
		return -EINVAL;
	}

	/* Set ADDR_MAP_EN to 1'b0*/
	platform_mif_reg_write_smapper(platform, ADDR_MAP_EN(bank), 0);
	/* Write mapping table to SRAM. Each entry consists of 25 bits MSB address to remap */
	for (i = 0; i < num_entries; i++) {
		if (!addr[i]) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "ADDR IS NULL at bank %d entry %d/%d\n", bank, first_entry + i, num_entries);
			return -EINVAL;
		}
		/* Set SRAM_WRITE_CTRL to 1'b1*/
		platform_mif_reg_write_smapper(platform, SRAM_WRITE_CTRL(bank), 1);
		platform_mif_reg_write_smapper(platform, SRAM_BANK_INDEX(bank, first_entry + i), addr[i] >> PLATFORM_MIF_SHIFT_SMAPPER_ADDR);
		/* check incorrect writings */
		platform_mif_reg_write_smapper(platform, SRAM_WRITE_CTRL(bank), 0);
		rb = platform_mif_reg_read_smapper(platform, SRAM_BANK_INDEX(bank, first_entry + i));
		if (rb != addr[i] >> PLATFORM_MIF_SHIFT_SMAPPER_ADDR) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "incorrect mapping detected rb 0x%x, addr 0x%x\n", rb, (u32)addr[i] >> PLATFORM_MIF_SHIFT_SMAPPER_ADDR);
			return -EFAULT;
		}
	}
	platform_mif_reg_write_smapper(platform, ADDR_MAP_EN(bank), 1);
	smp_mb();
	return 0;
}

static int platform_mif_parse_smapper(struct platform_mif *platform, struct device_node *np, u8 num_banks)
{
	/* SMAPPER parsing */
	struct device_node *np_banks;
	char node_name[50];
	u32 val[2];
	u8 i;
	u32 bank = 0, ws = 0, wsz = 0, ent = 0, large = 0;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "banks found: %d szof %zu\n", num_banks, sizeof(*platform->smapper));

	platform->smapper = kmalloc_array(num_banks, sizeof(*platform->smapper), GFP_KERNEL);

	if (!platform->smapper)
		return -ENOMEM;

	for (i = 0; i < num_banks; i++) {
		snprintf(node_name, sizeof(node_name), "smapper_bank_%d", i);
		np_banks = of_find_node_by_name(np, node_name);
		if (!np_banks) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "%s: could not find smapper_bank\n",
				node_name);
			kfree(platform->smapper);
			platform->smapper = NULL;
			return -ENOENT;
		}
		of_property_read_u32(np_banks, "bank_num", &bank);
		of_property_read_u32(np_banks, "fw_window_start", &ws);
		of_property_read_u32(np_banks, "fw_window_size", &wsz);
		of_property_read_u32(np_banks, "num_entries", &ent);
		of_property_read_u32(np_banks, "is_large", &large);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "bank %d fw_w_start 0x%x fw_w_sz 0x%x entries %d is_large %d\n",
				  bank, ws, wsz, ent, large);

		platform->smapper[i].bank = (u8)bank;
		platform->smapper[i].ws = ws;
		platform->smapper[i].large = (bool)large;
		platform->smapper[i].bank_info.num_entries = ent;
		platform->smapper[i].bank_info.mem_range_bytes = wsz;
	}

	/* Update the number of banks before returning */
	platform->smapper_banks = num_banks;

	of_property_read_u32_array(np, "smapper_reg", val, 2);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "smapper reg address 0x%x size 0x%x\n", val[0], val[1]);
	platform->smapper_base =
		devm_ioremap_nocache(platform->dev, val[0], val[1]);

	if (!platform->smapper_base) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error mapping smapper register region\n");
		kfree(platform->smapper);
		platform->smapper = NULL;
		return -ENOENT;
	}

	return 0;
}
#endif
#ifdef CONFIG_SCSC_QOS
static int platform_mif_parse_qos(struct platform_mif *platform, struct device_node *np)
{
	int len, i;

	platform->qos_enabled = false;

	len = of_property_count_u32_elems(np, "qos_table");
	if (!(len == 12)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "No qos table for wlbt, or incorrect size\n");
		return -ENOENT;
	}

	platform->qos = devm_kzalloc(platform->dev, sizeof(struct qos_table) * len / 4, GFP_KERNEL);
	if (!platform->qos)
		return -ENOMEM;

	of_property_read_u32_array(np, "qos_table", (unsigned int *)platform->qos, len);

	for (i = 0; i < len / 4; i++) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "QoS Table[%d] mif : %u int : %u cl0 : %u cl1: %u\n", i,
			platform->qos[i].freq_mif,
			platform->qos[i].freq_int,
			platform->qos[i].freq_cl0,
			platform->qos[i].freq_cl1);
	}

	platform->qos_enabled = true;
	return 0;
}

struct qos_table platform_mif_pm_qos_get_table(struct platform_mif *platform, enum scsc_qos_config config)
{
	struct qos_table table;

	switch (config) {
	case SCSC_QOS_MIN:
		table.freq_mif = platform->qos[0].freq_mif;
		table.freq_int = platform->qos[0].freq_int;
		table.freq_cl0 = platform->qos[0].freq_cl0;
		table.freq_cl1 = platform->qos[0].freq_cl1;
		break;

	case SCSC_QOS_MED:
		table.freq_mif = platform->qos[1].freq_mif;
		table.freq_int = platform->qos[1].freq_int;
		table.freq_cl0 = platform->qos[1].freq_cl0;
		table.freq_cl1 = platform->qos[1].freq_cl1;
		break;

	case SCSC_QOS_MAX:
		table.freq_mif = platform->qos[2].freq_mif;
		table.freq_int = platform->qos[2].freq_int;
		table.freq_cl0 = platform->qos[2].freq_cl0;
		table.freq_cl1 = platform->qos[2].freq_cl1;
		break;

	default:
		table.freq_mif = 0;
		table.freq_int = 0;
		table.freq_cl0 = 0;
		table.freq_cl1 = 0;
	}

	return table;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

	pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);
	pm_qos_add_request(&qos_req->pm_qos_req_cl0, PM_QOS_CLUSTER0_FREQ_MIN, table.freq_cl0);
	pm_qos_add_request(&qos_req->pm_qos_req_cl1, PM_QOS_CLUSTER1_FREQ_MIN, table.freq_cl1);

	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS update request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

	pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	pm_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
	pm_qos_update_request(&qos_req->pm_qos_req_cl1, table.freq_cl1);

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;


	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS remove request\n");
	pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	pm_qos_remove_request(&qos_req->pm_qos_req_int);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl0);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl1);

	return 0;
}
#endif

static void platform_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT handler not registered\n");
}

static void platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT reset_request handler not registered\n");
}

irqreturn_t platform_mif_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->r4_handler);
	if (platform->r4_handler != platform_mif_irq_default_handler)
		platform->r4_handler(irq, platform->irq_dev);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MIF Interrupt Handler not registered\n");

	return IRQ_HANDLED;
}

#ifdef CONFIG_SCSC_ENABLE_ALIVE_IRQ
irqreturn_t platform_alive_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received\n");

	return IRQ_HANDLED;
}
#endif

irqreturn_t platform_wdog_isr(int irq, void *data)
{
	int ret = 0;
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received %d\n", irq);
	platform_int_debug(platform);

	if (platform->reset_request_handler != platform_mif_irq_reset_request_default_handler) {
		if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
			/* Spurious interrupt from the SOC during CFG_REQ phase, just consume it */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Spurious wdog irq during cfg_req phase\n");
			return IRQ_HANDLED;
		} else {
			disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
			platform->reset_request_handler(irq, platform->irq_reset_request_dev);
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}

	/* The wakeup source isn't cleared until WLBT is reset, so change the interrupt type to suppress this */
	ret = regmap_update_bits(platform->pmureg, WAKEUP_INT_TYPE,
			RESETREQ_WLBT, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Set RESETREQ_WLBT wakeup interrput type to EDGE.\n");
	if (ret < 0)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Failed to Set WAKEUP_INT_TYPE[RESETREQ_WLBT]: %d\n", ret);

	return IRQ_HANDLED;
}

static void wlbt_regdump(struct platform_mif *platform)
{
	u32 val = 0;

	regmap_read(platform->pmureg, WLBT_STAT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STAT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CONFIGURATION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OPTION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OPTION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_NS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_S, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_S 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OUT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_EN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_TYPE, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_TYPE 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_DIR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_DIR 0x%x\n", val);

	regmap_read(platform->pmureg, SYSTEM_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "SYSTEM_OUT 0x%x\n", val);

	regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "VGPIO_TX_MONITOR 0x%x\n", val);
}

/*
 * Attached array contains the replacement PMU boot code which should
 * be programmed using the CBUS during the config phase.
 */
uint32_t ka_patch[] = {
	// Maxwell142 PMU+PROC combined boot ROM
	// IP Version: 0xA3
	// Major Version: 0xF, Minor Version: 0xF
	// PMU ROM version: 0x4
	// PROC  ROM version: 0x0
	// From excite 0605
	0xe8750002,
	0x1189113f,
	0x0e807558,
	0xe030b3e5,
	0x11a230fb,
	0xb1f50774,
	0x75fdb2b5,
	0x077400b1,
	0xadb5acf5,
	0x01b443fd,
	0xe020b3e5,
	0xfeb453fb,
	0xd880a3d2,
	0x83798278,
	0xa075d280,
	0x78377907,
	0x0754e6b0,
	0x800207b4,
	0x04f6d90b,
	0x7580f5c4,
	0x038000af,
	0x22098075,
	0x53f7f753,
	0x0479fece,
	0xce53fed9,
	0xd90c79fd,
	0xfbce53fe,
	0x53b79275,
	0x0074fd91,
	0xce5392f5,
	0xb79275f7,
	0x5302f943,
	0xf922fef9,
	0xfbd8fed9,
	0x019e7522,
	0x7501d275,
	0xc17580d5,
	0x74047880,
	0x838012c3,
	0x78c0c175,
	0x12827402,
	0x80758380,
	0x0291750d,
	0x75039375,
	0xa975029e,
	0x00002201,
};

static void wlbt_karam_dump(struct platform_mif *platform)
{
	unsigned int ka_addr = 0x1000;
	unsigned int val;
	unsigned int ka_array_size = ka_addr + (ARRAY_SIZE(ka_patch) * sizeof(ka_patch[0]));

	SCSC_TAG_INFO(PLAT_MIF, "Print KARAM area\n");
	SCSC_TAG_DEBUG(PLAT_MIF, "ka_array_size: %u\n", ka_array_size);

	regmap_write(platform->boot_cfg, 0x0, 0x1);

	while (ka_addr < ka_array_size) {
		regmap_read(platform->boot_cfg, ka_addr, &val);
		SCSC_TAG_INFO(PLAT_MIF, "0x%08x: 0x%08x\n", ka_addr, val);
		ka_addr += (unsigned int)sizeof(ka_patch[0]);
	}

	regmap_write(platform->boot_cfg, 0x0, 0x0);
}

void platform_cfg_req_irq_clean_pending(struct platform_mif *platform)
{
	int irq;
	int ret;
	bool pending = 0;
	char *irqs_name = {"CFG_REQ"};

	irq = platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num;
	ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);

	if (!ret) {
		if(pending == 1){
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d",
							  irq, irqs_name, pending);
			pending = 0;
			ret = irq_set_irqchip_state(irq, IRQCHIP_STATE_PENDING, pending);
		}
	}
}

void platform_set_wlbt_regs(struct platform_mif *platform)
{
	u64 ret64 = 0;
	const u64 EXYNOS_WLBT = 0x1;
	/*s32 ret = 0;*/
	unsigned int ka_addr = 0x1000;
	uint32_t *ka_patch_addr = ka_patch;
	unsigned int id;
	unsigned int val;

#define CHECK(x) do { \
	int retval = (x); \
	if (retval < 0) {\
		pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
		goto cfg_error; \
	} \
} while (0)

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	/* Set TZPC to non-secure mode */
	/* http://caeweb/~wshi/skylark/project-skylark.sim-basic/sky/regs/pmu_conf/pmu_conf/doc/main.html
	*/
	ret64 = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_WLBT << 32) | EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret64)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
		"Failed to set TZPC to non-secure mode: %llu\n", ret64);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"SMC_CMD_CONN_IF run successfully : %llu\n", ret64);

	/* WLBT_REMAP PMU_REMAP - PROC_RMP_BOOT_ADDR 0x14450400 */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_REMAP begin\n");
	CHECK(regmap_write(platform->wlbt_remap, 0x400, WLBT_DBUS_BAAW_0_START >> 12));
	regmap_read(platform->wlbt_remap, 0x400, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_REMAP: 0x%x.\n", val);

	/* CHIP_VERSION_ID 0x14450410, - update with AP view of SOC revision */
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "CHIP_VERSION_ID begin\n");
	regmap_read(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, &id);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read CHIP_VERSION_ID 0x%x\n", id);
	id &= ~(CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR);
	id |= ((exynos_soc_info.revision << CHIP_VERSION_ID_IP_MINOR_SHIFT) & (CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR));
	CHECK(regmap_write(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, id));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated CHIP_VERSION_ID 0x%x\n", id);

	/* DBUS_BAAW regions */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW begin\n");

	/* Shared DRAM mapping. The destination address is the location reserved
	 * by the kernel.
	 */
	CHECK(regmap_write(platform->dbus_baaw, 0x0, WLBT_DBUS_BAAW_0_START >> 12));
	CHECK(regmap_write(platform->dbus_baaw, 0x4, WLBT_DBUS_BAAW_0_END >> 12));
	CHECK(regmap_write(platform->dbus_baaw, 0x8, platform->mem_start >> 12)); // FW AP base addr >> 12
	CHECK(regmap_write(platform->dbus_baaw, 0xC, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->dbus_baaw, 0x0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, 0x4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, 0x8, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, 0xc, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);
#if 0
	/* Additional DRAM mappings for future use */
	CHECK(regmap_write(platform->dbus_baaw, 0x10, 0x000C0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x14, 0x000D0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x18, 0x000D0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x1C, WLBT_BAAW_ACCESS_CTRL));

	CHECK(regmap_write(platform->dbus_baaw, 0x20, 0x000D0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x24, 0x000E0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x28, 0x000E0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x2C, WLBT_BAAW_ACCESS_CTRL));

	CHECK(regmap_write(platform->dbus_baaw, 0x30, 0x000E0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x34, 0x000F0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x38, 0x000F0000));
	CHECK(regmap_write(platform->dbus_baaw, 0x3C, WLBT_BAAW_ACCESS_CTRL));
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW end\n");

	/* PBUS_BAAW regions */
	/* ref wlbt_if_S5E9630.c, updated for MX450 memory map */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PBUS_BAAW begin\n");

	/* Range for CP2WLBT mailbox */
	CHECK(regmap_write(platform->pbus_baaw, 0x0, WLBT_CBUS_BAAW_0_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x4, WLBT_CBUS_BAAW_0_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x8, WLBT_PBUS_MBOX_CP2WLBT_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0xC, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x8, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0xc, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);

	/* Range includes AP2WLBT,APM2WLBT,GNSS2WLBT mailboxes etc. */
	CHECK(regmap_write(platform->pbus_baaw, 0x10, WLBT_CBUS_BAAW_1_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x14, WLBT_CBUS_BAAW_1_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x18, WLBT_PBUS_MBOX_GNSS2WLBT_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x1C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x10, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x14, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x18, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x1c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_ENABLE_DONE: 0x%x.\n", val);

	CHECK(regmap_write(platform->pbus_baaw, 0x20, WLBT_CBUS_BAAW_2_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x24, WLBT_CBUS_BAAW_2_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x28, WLBT_PBUS_GPIO_CMGP_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x2C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x20, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x24, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x28, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x2c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_ENABLE_DONE: 0x%x.\n", val);

	CHECK(regmap_write(platform->pbus_baaw, 0x30, WLBT_CBUS_BAAW_3_START >>12));
	CHECK(regmap_write(platform->pbus_baaw, 0x34, WLBT_CBUS_BAAW_3_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x38, WLBT_PBUS_SYSREG_CMGP2WLBT_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x3C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x30, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x34, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x38, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x3c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_ENABLE_DONE: 0x%x.\n", val);

	CHECK(regmap_write(platform->pbus_baaw, 0x40, WLBT_CBUS_BAAW_4_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x44, WLBT_CBUS_BAAW_4_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x48, WLBT_PBUS_USI_CMG00_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x4C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x40, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x44, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x48, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x4c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_ENABLE_DONE: 0x%x.\n", val);

	CHECK(regmap_write(platform->pbus_baaw, 0x50, WLBT_CBUS_BAAW_5_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x54, WLBT_CBUS_BAAW_5_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x58, WLBT_PBUS_CHUB_USICHUB0_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x5C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x50, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x54, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x58, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x5c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_ENABLE_DONE: 0x%x.\n", val);

	CHECK(regmap_write(platform->pbus_baaw, 0x60, WLBT_CBUS_BAAW_6_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x64, WLBT_CBUS_BAAW_6_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x68, WLBT_PBUS_CHUB_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, 0x6C, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, 0x60, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x64, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x68, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, 0x6c, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_ENABLE_DONE: 0x%x.\n", val);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PBUS_BAAW end\n");

	/* PMU boot patch */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "AP accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, 0x0, 0x1));
	regmap_read(platform->boot_cfg, 0x0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch start\n");
	while (ka_patch_addr < (ka_patch + ARRAY_SIZE(ka_patch))) {
		CHECK(regmap_write(platform->boot_cfg, ka_addr, *ka_patch_addr));
		ka_addr += (unsigned int)sizeof(ka_patch[0]);
		ka_patch_addr++;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch done\n");

	/* Notify PMU of configuration done */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT PMU accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, 0x0, 0x0));
	regmap_read(platform->boot_cfg, 0x0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);

	/* WLBT FW could panic as soon as CFG_ACK is set, so change state.
	 * This allows early FW panic to be dumped.
	 */
	platform->boot_state = WLBT_BOOT_ACK_CFG_REQ;

	/* BOOT_CFG_ACK */
	CHECK(regmap_write(platform->boot_cfg, 0x4, 0x1));
	regmap_read(platform->boot_cfg, 0x4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_CFG_ACK: 0x%x\n", val);

	/* Mark as CFQ_REQ handled, so boot may continue */
	platform->boot_state = WLBT_BOOT_CFG_DONE;

	/* Signal triggering function that the IRQ arrived and CFG was done */
	complete(&platform->cfg_ack);
	goto done;

cfg_error:
	platform->boot_state = WLBT_BOOT_CFG_ERROR;
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "ERROR: WLBT Config failed. WLBT will not work\n");
	complete(&platform->cfg_ack);
done:
	return;
}

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
void platform_cfg_req_wq(struct work_struct *data)
{
	struct platform_mif *platform = container_of(data, struct platform_mif, cfgreq_wq);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");
	platform_set_wlbt_regs(platform);
}
#endif

irqreturn_t platform_cfg_req_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received\n");
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "disable_irq\n");

	/* mask the irq */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Was the CFG_REQ irq received from WLBT before we expected it?
	 * Typically this indicates an issue returning WLBT HW to reset.
	 */
	if (platform->boot_state != WLBT_BOOT_WAIT_CFG_REQ) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Spurious CFG_REQ IRQ from WLBT!\n");
		wlbt_regdump(platform);

		platform->reset_failed = true; /* prevent further interaction with HW */

		goto cfg_error;
	}

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	/* it is executed on process context. */
	queue_work(platform->cfgreq_workq, &platform->cfgreq_wq);
#else
	/* it is executed on interrupt context. */
	platform_set_wlbt_regs(platform);
#endif
cfg_error:
	return IRQ_HANDLED;
}

static bool platform_mif_reset_failure(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return false;

	return platform->reset_failed;
}

static void platform_mif_unregister_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform);
	/* Reset irq_disabled_cnt for WDOG IRQ since the IRQ itself is here unregistered and disabled */
	atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
#ifdef CONFIG_SCSC_ENABLE_ALIVE_IRQ
	/* if ALIVE irq is required  */
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform);
#endif
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform);
}

static int platform_mif_register_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	/* Register MBOX irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform_mif_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register WDOG irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering WDOG irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform->wlbt_irq[PLATFORM_MIF_WDOG].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform_wdog_isr,
			       platform->wlbt_irq[PLATFORM_MIF_WDOG].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register WDOG handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

#ifdef CONFIG_SCSC_ENABLE_ALIVE_IRQ
	/* Register ALIVE irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering ALIVE irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform->wlbt_irq[PLATFORM_MIF_ALIVE].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform_alive_isr,
			       platform->wlbt_irq[PLATFORM_MIF_ALIVE].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE(err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register ALIVE handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}
#endif

	/* Mark as WLBT in reset before enabling IRQ to guard against spurious IRQ */
	platform->boot_state = WLBT_BOOT_IN_RESET;
	smp_wmb(); /* commit before irq */

	/* Register WB2AP_CFG_REQ irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering CFG_REQ irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags);

	/* clean CFG_REQ PENDING interrupt. */
	platform_cfg_req_irq_clean_pending(platform);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform_cfg_req_isr,
			       platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register CFG_REQ handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Leave disabled until ready to handle */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	return 0;
}

static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_unregister_irq(platform);
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}


/* WLBT START */
static int platform_mif_start(struct scsc_mif_abs *interface, bool start)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* done as part of platform_mif_pmu_reset_release() init_done sequence */
#if 0
	//s32                 ret = 0;
	if (start)
		val = WLBT_START;

	/* See sequence in TODO update when available */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_S,
				 WLBT_START, val);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CTRL_S[WLBT_START]: %d\n", ret);
		return ret;
	}
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"update WIFI_CTRL_S[WLBT_START]: %d\n", ret);

#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "start %d\n", start);
	/* At this point WLBT should assert the CFG_REQ IRQ, so wait for it */
	if (start &&
	    wait_for_completion_timeout(&platform->cfg_ack, WLBT_BOOT_TIMEOUT) == 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for CFG_REQ IRQ\n");
		wlbt_regdump(platform);
		return -ETIMEDOUT;
	}

	/* only continue if CFG_REQ IRQ configured WLBT/PMU correctly */
	if (platform->boot_state == WLBT_BOOT_CFG_ERROR) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "CFG_REQ failed to configure WLBT.\n");
		return -EIO;
	}
	return 0;
}

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int		ret = 0;
	u32		val = 0;
	u32		v = 0;
	unsigned long	timeout;

	/* We're now ready for the IRQ */
	platform->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
	smp_wmb(); /* commit before irq */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");

	/* INIT SEQUENCE - First WLBT boot only, but we use all the time.
	 * Cold reset wrt. AP power sequencer, cold reset for WLBT
	 */

	/* init sequence from excite
	 * //20190812 APM team Change PMUCAL guide doc.
	 * SetBits((uinteger)REG_WLBT_CTRL_S, 3, 0x1, 0x1);
	 * SetBits((uinteger)REG_WLBT_OPTION, 3, 0x1, 0x1);
	 * udelay(1000);
	 * SetBits((uinteger)SYSTEM_OUT, 9, 0x1, 0x1);
	 * SetBits((uinteger)REG_WLBT_CONFIGURATION, 0, 0x1, 0x1);
	 * while (GetBits((uinteger)REG_VGPIO_TX_MONITOR, 29, 0x1) != 0x1)
	 * ;
	 * while (GetBits((uinteger)REG_WLBT_STATUS, 0, 0x1) != 0x1)
	 * ;
	 * while (GetBits((uinteger)REG_WLBT_IN, 4, 0x1) != 0x1)
	 * ;
	 */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");

	if (!init_done) {
		/* WLBT_CTRL_S[WLBT_START] = 1 enable */
		ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_S,
				WLBT_START, WLBT_START);
		if (ret < 0) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
					"Failed to update WLBT_CTRL_S[WLBT_START]: %d\n", ret);
			goto done;
		}
		regmap_read(platform->pmureg, WLBT_CTRL_S, &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"updated successfully WLBT_CTRL_S[WLBT_START]: 0x%x\n", val);
		init_done = true;
	}

	/* WLBT_OPTION[WLBT_OPTION_DATA] = 1 Power On */
	ret = regmap_update_bits(platform->pmureg, WLBT_OPTION,
			WLBT_OPTION_DATA, WLBT_OPTION_DATA);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_OPTION[WLBT_OPTION_DATA]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_OPTION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_OPTION[WLBT_OPTION_DATA]: 0x%x\n", val);

	/* SYSTEM_OUT[PWRRGTON_CON] = 1 Power On */
	ret = regmap_update_bits(platform->pmureg, SYSTEM_OUT,
			PWRRGTON_CON, PWRRGTON_CON);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update SYSTEM_OUT[PWRRGTON_CON]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, SYSTEM_OUT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully SYSTEM_OUT[PWRRGTON_CON]: 0x%x\n", val);

	/* VGPIO_TX_MONITOR[VGPIO_TX_MON_BIT29] = 0x1 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
		val &= (u32)VGPIO_TX_MON_BIT29;
		if (val) {
			SCSC_TAG_INFO(PLAT_MIF, "read VGPIO_TX_MONITOR 0x%x\n", val);
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!val) {
		regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for VGPIO_TX_MONITOR time-out: "
					"VGPIO_TX_MONITOR 0x%x\n", val);
	}

	udelay(1000);

	/* clean CFG_REQ PENDING interrupt. */
	platform_cfg_req_irq_clean_pending(platform);

	/* WLBT_CONFIGURATION[LOCAL_PWR_CFG] = 1 Power On */
	ret = regmap_update_bits(platform->pmureg, WLBT_CONFIGURATION,
			LOCAL_PWR_CFG, LOCAL_PWR_CFG);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CONFIGURATION[LOCAL_PWR_CFG]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CONFIGURATION[PWRRGTON_CON]: 0x%x\n", val);

	/* wait for power up complete WLBT_STATUS[WLBT_STATUS_BIT0] = 1 for Power On */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		val &= (u32)WLBT_STATUS_BIT0;
		if (val) {
			/* Power On complete */
			SCSC_TAG_INFO(PLAT_MIF, "Power On complete: WLBT_STATUS 0x%x\n", val);
			/* re affirming power on by reading WLBT_STATES */
			/* STATES[7:0] = 0x10 for Power Up */
			regmap_read(platform->pmureg, WLBT_STATES, &v);
			SCSC_TAG_INFO(PLAT_MIF, "Power On complete: WLBT_STATES 0x%x\n", v);
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!val) {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for power on time-out: "
					"WLBT_STATUS 0x%x, WLBT_STATES 0x%x\n", val, v);
	}

	/* wait for WLBT_IN[BUS_READY] = 1 for BUS READY state */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_IN, &val);
		val &= (u32)BUS_READY;
		if (val) {
			/* BUS ready indication signal -> 1: BUS READY state */
			SCSC_TAG_INFO(PLAT_MIF, "Bus Ready: WLBT_IN 0x%x\n", val);

			/* OK to break */
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!val) {
		regmap_read(platform->pmureg, WLBT_IN, &val);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for Bus Ready: WLBT_IN 0x%x\n", val);
	}

	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: 0x%x\n", val);

	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_EN, WLBT_ACTIVE_EN);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_EN]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_EN]: 0x%x\n", val);

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	ret = platform_mif_start(interface, true);
done:
	return ret;
}

static int platform_mif_pmu_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       timeout;
	int                 ret;
	u32                 val;

	/* wlbt_if_reset_assertion() - from wlbt_if_S5E9630.c
	 * 20190812 APM team Change PMUCAL guide doc.
	 * SetBits((uinteger)REG_WLBT_OPTION, 3, 0x1, 0x1);
	 * SetBits((uinteger)SYSTEM_OUT, 9, 0x1, 0x1);
	 * while (GetBits((uinteger)REG_VGPIO_TX_MONITOR, 29, 0x1) != 0x1)
	 * ;
	 * udelay(1000);
	 * SetBits((uinteger)REG_WLBT_CONFIGURATION, 0, 0x1, 0x0);
	 * udelay(1000);
	 * while (GetBits((uinteger)REG_WLBT_STATUS, 0, 0x1) != 0x0)
	 * ;
	 */

	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_EN, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_EN]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_EN]: 0x%x\n", val);

	/* Power Down */
	ret = regmap_write_bits(platform->pmureg, WLBT_CONFIGURATION,
		LOCAL_PWR_CFG, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CONFIGURATION[LOCAL_PWR_CFG]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CONFIGURATION[LOCAL_PWR_CFG]: 0x%x\n", val);

	/* Wait for power Off WLBT_STATUS[WLBT_STATUS_BIT0] = 0 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		val &= (u32)WLBT_STATUS_BIT0;
		if (val == 0) {
			SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
			/* re affirming power down by reading WLBT_STATES */
			/* STATES[7:0] = 0x80 for Power Down */
			regmap_read(platform->pmureg, WLBT_STATES, &val);
			SCSC_TAG_INFO(PLAT_MIF, "Power down complete: WLBT_STATES 0x%x\n", val);

			ret = 0; /* OK - return */
			goto done;
		}
	} while (time_before(jiffies, timeout));

	/* Timed out */
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for WLBT_STATUS status\n");

	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);
	ret = -ETIME;
done:
	return ret;
}

/* reset=0 - release from reset */
/* reset=1 - hold reset */
static int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 ret = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (enable_platform_mif_arm_reset || !reset) {
		if (!reset) { /* Release from reset */
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"SOC_VERSION: product_id 0x%x, rev 0x%x\n",
				exynos_soc_info.product_id, exynos_soc_info.revision);
#endif
			power_supplies_on(platform);

			ret = platform_mif_pmu_reset_release(interface);
		} else {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
			cancel_work_sync(&platform->cfgreq_wq);
			flush_workqueue(platform->cfgreq_workq);
#endif
			/* Put back into reset */
			ret = platform_mif_pmu_reset_assert(interface);
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
			 enable_platform_mif_arm_reset);
	return ret;
}

#define SCSC_STATIC_MIFRAM_PAGE_TABLE

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t      i;
	struct page **pages;
	void        *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: kmalloc of %zd byte pages table failed\n", (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT) * sizeof(*pages)];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));

		if (size > MIFRAMMAN_MAXMEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PLAT_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
					size, MIFRAMMAN_MAXMEM);
			return NULL;
		}
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

static void platform_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8                  i;

	if (allocated)
		*allocated = 0;

	platform->mem =
		platform_mif_map_region(platform->mem_start, platform->mem_size);

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Map: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < NUM_MBOX_PLAT; i++)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);

#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
	/* PIO muxes switching to the Maxwell subsystem */
	if (gpr4_debug) {
		reg_bkp = readl(gpio_base);
		writel(0x00444444, gpio_base);
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "[WARNING] Changing GPF2_CON from 0x%x to 0x%x\n", reg_bkp, readl(gpio_base));
		reg_update = true;
	}
#endif

#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_disable_irq == true) {
		if (allocated)
			*allocated = platform->mem_size;
		return platform->mem;
	}
#endif
	/* register interrupts */
	if (platform_mif_register_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		platform_mif_unmap_region(platform->mem);
		return NULL;
	}

	if (allocated)
		*allocated = platform->mem_size;
	/* Set the CR4 base address in Mailbox??*/
	return platform->mem;
}

/* HERE: Not sure why mem is passed in - its stored in platform - as it should be */
static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
	if (gpr4_debug && reg_update) {
		u32 prev = readl(gpio_base);
		writel(reg_bkp, gpio_base);
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "[WARNING] Restoring GPF2_CON from 0x%x to 0x%x\n", prev, readl(gpio_base));
	}
	reg_update = false;
#endif
	/* Avoid unused parameter error */
	(void)mem;

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	/* Restore PIO changed by Maxwell subsystem */
	if (chv_disable_irq == false)
		/* Unregister IRQs */
		platform_mif_unregister_irq(platform);
#else
	platform_mif_unregister_irq(platform);
#endif
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
	platform_mif_unmap_region(platform->mem);
	platform->mem = NULL;
}

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMR0: 0x%x\n", val);
	return val;
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INT-INTMSR0: 0x%x\n", val);

	return val;
}

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 reg;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	reg = INTGR1;
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit %d on target %d\n", bit_num, target);
}

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTCR0: bit %d\n", bit_num);
}

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 1 = Mask Interrupt */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit %d\n", val | (1 << bit_num), bit_num);
}

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 0 = Unmask Interrupt */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit %d\n", val & ~((1 << bit_num) << 16), bit_num);
}

/* Return the contents of the mask register */
static u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Write the mask register, destroying previous contents */
static void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->r4_handler = handler;
	platform->irq_dev = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->r4_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif reset_request int handler %pS in %p %p\n", handler, platform, interface);
	platform->reset_request_handler = handler;
	platform->irq_reset_request_dev = dev;
	if (atomic_read(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Default WDOG handler disabled by spurios IRQ...re-enabling.\n");
		enable_irq(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	}
}

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
}

static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = suspend;
	platform->resume_handler = resume;
	platform->suspendresume_data = data;
}

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;
}

static u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 *addr;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "mbox_index 0x%x\n", mbox_index);
	addr = platform->base + MAILBOX_WLBT_REG(ISSR(mbox_index));
	return addr;
}

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem + platform->mem_size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Unable to get pointer reference\n");
		return -ENOMEM;
	}

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem);

	return 0;
}

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size)
		return (void *)((uintptr_t)platform->mem + (uintptr_t)ref);
	else
		return NULL;
}

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_start) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)platform->mem_start + (uintptr_t)ref);
}

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return vmalloc_to_pfn(platform->mem);
}

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (id == SCSC_REG_READ_WLBT_STAT) {
		regmap_read(platform->pmureg, WLBT_STAT, val);
		return 0;
	}

	return -EIO;
}

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)));

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR1)));

	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = {PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG};
	char *irqs_name[] = {"MBOX", "WDOG"};

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret  = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE,  &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED,  &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d",
							  irq, irqs_name[i], pending, active, masked);
	}
	platform_mif_dump_register(&platform->interface);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem)
{
	SCSC_TAG_DEBUG(PLAT_MIF, "memory reserved: mem_base=%#lx, mem_size=%zd\n",
		       (unsigned long)remem->base, (size_t)remem->size);

	sharedmem_base = remem->base;
	sharedmem_size = remem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifibt_if, "exynos,wifibt_if", platform_mif_wifibt_if_reserved_mem_setup);
#endif

#ifdef CONFIG_EXYNOS_ITMON
static int wlbt_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct platform_mif *platform = container_of(nb, struct platform_mif, itmon_nb);
	int ret = NOTIFY_DONE;
	struct itmon_notifier *itmon_data = (struct itmon_notifier *)nb_data;

	if(!itmon_data) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "itmon_data is NULL");
		goto error_exit;
	}

	if (itmon_data->dest &&
		(!strncmp("WLBT", itmon_data->dest, sizeof("WLBT") - 1))) {
		wlbt_regdump(platform);
		if((itmon_data->target_addr >= PMU_BOOT_RAM_START)
			&& (itmon_data->target_addr <= PMU_BOOT_RAM_END))
			wlbt_karam_dump(platform);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if defined(GO_S2D_ID)
		dbg_snapshot_soc_do_dpm_policy(GO_S2D_ID);
#elif defined(CONFIG_S3C2410_WATCHDOG)
		s3c2410wdt_set_emergency_reset(0, 0);
#endif
#endif
		ret = NOTIFY_BAD;
	} else if (itmon_data->port &&
		(!strncmp("WLBT", itmon_data->port, sizeof("WLBT") - 1))) {
		wlbt_regdump(platform);
		ret = NOTIFY_OK;
	} else if (itmon_data->master &&
		(!strncmp("WLBT", itmon_data->master, sizeof("WLBT") - 1))) {
		wlbt_regdump(platform);
		ret = NOTIFY_OK;
	}

error_exit:
	return ret;
}
#endif

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	int                 err = 0;
	u8                  i = 0;
	struct resource     *reg_res;

#ifdef CONFIG_SCSC_SMAPPER
	u32                 smapper_banks = 0;
#endif

	if (!platform)
		return NULL;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev, "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->map = platform_mif_map;
	platform_if->unmap = platform_mif_unmap;
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
	platform_if->irq_reg_handler = platform_mif_irq_reg_handler;
	platform_if->irq_unreg_handler = platform_mif_irq_unreg_handler;
	platform_if->irq_reg_reset_request_handler = platform_mif_irq_reg_reset_request_handler;
	platform_if->irq_unreg_reset_request_handler = platform_mif_irq_unreg_reset_request_handler;
	platform_if->suspend_reg_handler = platform_mif_suspend_reg_handler;
	platform_if->suspend_unreg_handler = platform_mif_suspend_unreg_handler;
	platform_if->get_mbox_ptr = platform_mif_get_mbox_ptr;
	platform_if->get_mifram_ptr = platform_mif_get_mifram_ptr;
	platform_if->get_mifram_ref = platform_mif_get_mifram_ref;
	platform_if->get_mifram_pfn = platform_mif_get_mif_pfn;
	platform_if->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_dump_registers = platform_mif_dump_register;
	platform_if->mif_read_register = platform_mif_read_register;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
#ifdef CONFIG_SCSC_SMAPPER
	platform_if->mif_smapper_get_mapping = platform_mif_smapper_get_mapping;
	platform_if->mif_smapper_get_bank_info = platform_mif_smapper_get_bank_info;
	platform_if->mif_smapper_write_sram = platform_mif_smapper_write_sram;
	platform_if->mif_smapper_configure = platform_mif_smapper_configure;
	platform_if->mif_smapper_get_bank_base_address = platform_mif_smapper_get_bank_base_address;
#endif
#ifdef CONFIG_SCSC_QOS
	platform_if->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	platform_if->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	platform_if->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
#endif
	platform->reset_failed = false;
	platform_if->mif_reset_failure = platform_mif_reset_failure;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;

	platform->r4_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

#ifdef CONFIG_OF_RESERVED_MEM
	platform->mem_start = sharedmem_base;
	platform->mem_size = sharedmem_size;
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
#ifdef CONFIG_SCSC_SMAPPER
	platform->smapper = NULL;
#endif

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			(u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any sense.
		 * This may be an indication of an incorrect platform device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

	/* Memory resource - Phys Address of MAILBOX_WLBT register map */
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_res) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WLBT\n");
		err = -ENOENT;
		goto error_exit;
	}

	platform->reg_start = reg_res->start;
	platform->reg_size = resource_size(reg_res);

	platform->base =
		devm_ioremap_nocache(platform->dev, reg_res->start, resource_size(reg_res));

	if (!platform->base) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error mapping register region\n");
		err = -EBUSY;
		goto error_exit;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->reg_start %lx size %x base %p\n",
		(uintptr_t)platform->reg_start, (u32)platform->reg_size, platform->base);

#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Mapping GPF2_CON 0x138F0000\n");
	gpio_base = devm_ioremap_nocache(platform->dev, 0x138F0000, 4);
#endif

	/* Get the 4 IRQ resources */
	for (i = 0; i < 4; i++) {
		struct resource *irq_res;
		int             irqtag;

		irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!irq_res) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"No IRQ resource at index %d\n", i);
			err = -ENOENT;
			goto error_exit;
		}

		if (!strcmp(irq_res->name, "MBOX")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MBOX irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res->name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ALIVE irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res->name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res->name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CFG_REQ irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "Invalid irq res name: %s\n",
				irq_res->name);
			err = -EINVAL;
			goto error_exit;
		}
		platform->wlbt_irq[irqtag].irq_num = irq_res->start;
		platform->wlbt_irq[irqtag].flags = (irq_res->flags & IRQF_TRIGGER_MASK);
		atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);
	}

	/* PMU reg map - syscon */
	platform->pmureg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,syscon-phandle");
	if (IS_ERR(platform->pmureg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"syscon regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pmureg));
		err = -EINVAL;
		goto error_exit;
	}

	/* Completion event and state used to indicate CFG_REQ IRQ occurred */
	init_completion(&platform->cfg_ack);
	platform->boot_state = WLBT_BOOT_IN_RESET;

	/* I3C_APM_PMIC */
	platform->i3c_apm_pmic = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,i3c_apm_pmic-syscon-phandle");
	if (IS_ERR(platform->i3c_apm_pmic)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"i3c_apm_pmic regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->i3c_apm_pmic));
		err = -EINVAL;
		goto error_exit;
	}

	/* DBUS_BAAW */
	platform->dbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,dbus_baaw-syscon-phandle");
	if (IS_ERR(platform->dbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"dbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->dbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}

	/* PBUS_BAAW */
	platform->pbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,pbus_baaw-syscon-phandle");
	if (IS_ERR(platform->pbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"pbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}

	/* WLBT_REMAP */
	platform->wlbt_remap = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,wlbt_remap-syscon-phandle");
	if (IS_ERR(platform->wlbt_remap)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"wlbt_remap regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->wlbt_remap));
		err = -EINVAL;
		goto error_exit;
	}

	/* BOOT_CFG */
	platform->boot_cfg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,boot_cfg-syscon-phandle");
	if (IS_ERR(platform->boot_cfg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"boot_cfg regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->boot_cfg));
		err = -EINVAL;
		goto error_exit;
	}

#ifdef CONFIG_SCSC_SMAPPER
	/* SMAPPER parsing */
	if (!of_property_read_u32(pdev->dev.of_node, "smapper_num_banks", &smapper_banks))
		platform_mif_parse_smapper(platform, platform->dev->of_node, smapper_banks);

#endif
#ifdef CONFIG_SCSC_QOS
	platform_mif_parse_qos(platform, platform->dev->of_node);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

#ifdef CONFIG_EXYNOS_ITMON
	platform->itmon_nb.notifier_call = wlbt_itmon_notifier;
	itmon_notifier_chain_register(&platform->itmon_nb);
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	platform->cfgreq_workq = create_singlethread_workqueue("wlbt_cfg_reg_work");
	if (!platform->cfgreq_workq) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error creating CFG_REQ singlethread_workqueue\n");
		err = -ENOMEM;
		goto error_exit;
	}

	INIT_WORK(&platform->cfgreq_wq, platform_cfg_req_wq);
#endif

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	destroy_workqueue(platform->cfgreq_workq);
#endif
}

struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}

struct device *platform_mif_get_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->dev;
}

/* Preserve MIF registers during suspend.
 * If all users of the MIF (AP, mx140, CP, etc) release it, the registers
 * will lose their values. Save the useful subset here.
 *
 * Assumption: the AP will not change the register values between the suspend
 * and resume handlers being called!
 */
static void platform_mif_reg_save(struct platform_mif *platform)
{
	platform->mif_preserve.irq_bit_mask = __platform_mif_irq_bit_mask_read(platform);
}

/* Restore MIF registers that may have been lost during suspend */
static void platform_mif_reg_restore(struct platform_mif *platform)
{
	__platform_mif_irq_bit_mask_write(platform, platform->mif_preserve.irq_bit_mask);
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	/* Save the MIF registers.
	 * This must be done last as the suspend_handler may use the MIF
	 */
	platform_mif_reg_save(platform);

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	s32 ret;

	/* Restore the MIF registers.
	 * This must be done first as the resume_handler may use the MIF.
	 */
	platform_mif_reg_restore(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Clear WLBT_ACTIVE_CLR flag\n");
	/* Clear WLBT_ACTIVE_CLR flag in WLBT_CTRL_NS */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, WLBT_ACTIVE_CLR);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}

/* Temporary workaround to power up slave PMIC LDOs before FW APM/WLBT signalling
 * is complete
 */
static void power_supplies_on(struct platform_mif *platform)
{
	struct i2c_client i2c;

	/* HACK: Note only addr field is needed by s2mpu11_write_reg() */
	i2c.addr = 0x1;

	/* The APM IPC in FW will be used instead */
	if (disable_apm_setup) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs firmware controlled\n");
		return;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs on (PMIC i2c_addr = 0x%x)\n", i2c.addr);

	/* SLAVE PMIC
	 * echo 0x22 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE0 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x23 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE8 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x24 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x25 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x26 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x27 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 */

	s2mpu11_write_reg(&i2c, 0x22, 0xe0);
	s2mpu11_write_reg(&i2c, 0x23, 0xe8);
	s2mpu11_write_reg(&i2c, 0x24, 0xec);
	s2mpu11_write_reg(&i2c, 0x25, 0xec);
	s2mpu11_write_reg(&i2c, 0x26, 0xfc);
	s2mpu11_write_reg(&i2c, 0x27, 0xfc);
}
