/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

/* Interfaces it Uses */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/io.h>
#ifndef CONFIG_SOC_EXYNOS7570
#include <linux/smc.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>
#ifdef CONFIG_SOC_EXYNOS7570
#include "mif_reg.h"
#elif defined(CONFIG_SOC_EXYNOS7872)
#include "mif_reg_S5E7872.h"
#elif defined(CONFIG_SOC_EXYNOS7885)
#include "mif_reg_S5E7885.h"
#endif
#include "platform_mif_module.h"
#ifdef CONFIG_ARCH_EXYNOS
#include <linux/soc/samsung/exynos-soc.h>
#include <soc/samsung/exynos-pmu.h>
#endif
#include "mifintrbit.h"

#if !defined(CONFIG_SOC_EXYNOS7872) && !defined(CONFIG_SOC_EXYNOS7570) && !defined(CONFIG_SOC_EXYNOS7885)
#error Target processor CONFIG_SOC_EXYNOS7570 or CONFIG_SOC_EXYNOS7872 or CONFIG_SOC_EXYNOS7885 not selected
#endif

#if defined(CONFIG_SOC_EXYNOS7885)
#include <linux/mcu_ipc.h>
#endif

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
/* TODO: this will be put in a header */
extern int exynos_acpm_set_flag(void);
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif
static unsigned long sharedmem_base;
static size_t sharedmem_size;
static unsigned long btaboxsharedmem_base;
static size_t btaboxsharedmem_size;

struct scsc_btabox_data btaboxdata;

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

struct platform_mif {
	struct scsc_mif_abs    interface;
	struct scsc_mbox_s     *mbox;
	struct platform_device *pdev;

	struct device          *dev;

	struct {
		int irq_num;
		int flags;
		atomic_t irq_disabled_cnt;
	} wlbt_irq[3];

	/* MIF registers preserved during suspend */
	struct {
		u32 irq_bit_mask;
	} mif_preserve;

	/* register MBOX memory space */
	size_t        reg_start;
	size_t        reg_size;
	void __iomem  *base;

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	/* register MBOX memory space for M4 */
	size_t        reg_start_m4;
	size_t        reg_size_m4;
	void __iomem  *base_m4;
#endif
	/* register CMU memory space */
	struct regmap *cmu_base;
#ifdef CONFIG_SCSC_CLK20MHZ
	u32           usbpll_delay;
#endif

	void __iomem  *con0_base;

	/* pmu syscon regmap */
	struct regmap *pmureg;

	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t        mem_size;
	void __iomem  *mem;

	/* Shared memory - ABOX */
	unsigned long btaboxmem_start;
	size_t        btaboxmem_size;

	/* Callback function and dev pointer mif_intr manager handler */
	void          (*r4_handler)(int irq, void *data);
	void          *irq_dev;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	void          (*reset_request_handler)(int irq, void *data);
	void          *irq_reset_request_dev;

	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;
};

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)

#ifdef CONFIG_SCSC_CLK20MHZ
static void __platform_mif_usbpll_claim(struct platform_mif *platform, bool wlbt);
#endif

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
static void __platform_mif_fh_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num);
#endif

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base + offset);
}

inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base + offset);
}

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
inline void platform_mif_reg_write_m4(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_m4 + offset);
}

inline u32 platform_mif_reg_read_m4(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_m4 + offset);
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

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received\n");
	if (platform->reset_request_handler != platform_mif_irq_reset_request_default_handler) {
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		platform->reset_request_handler(irq, platform->irq_reset_request_dev);
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Clearing WIFI_RESET_REQ\n");
	ret = regmap_update_bits(platform->pmureg, WIFI_CTRL_NS,
				 WIFI_RESET_REQ_CLR, WIFI_RESET_REQ_CLR);
	if (ret < 0)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Failed to Set WIFI_CTRL_NS[WIFI_RESET_REQ_CLR]: %d\n", ret);
	return IRQ_HANDLED;
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
	if (IS_ERR_VALUE((unsigned long) err)) {
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
	if (IS_ERR_VALUE((unsigned long) err)) {
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

/* WLBT Power domain */
static int platform_mif_power(struct scsc_mif_abs *interface, bool power)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val = 0;
	s32                 ret = 0;

	/* See sequence in 8.6.6 */
	/* WIFI power on/off control  If WIFI_PWRON = 1
	 * and WIFI_START=1, WIFI enters to UP state.
	 * This bit is 0 as default value because WIFI
	 * should be reset at AP boot mode after Power-on Reset.  This bits reset is */
	if (power)
		val = WIFI_PWRON;
	ret = regmap_update_bits(platform->pmureg, WIFI_CTRL_NS,
				 WIFI_PWRON, val);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WIFI_CTRL_NS[WIFI_PWRON]: %d\n", ret);
		return ret;
	}
	return 0;
}

/* WLBT RESET */
static int platform_mif_hold_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val = 0;
	s32                 ret = 0;

	if (reset)
		val = WIFI_RESET_SET;
	/* See sequence in 8.6.6 */
	ret = regmap_update_bits(platform->pmureg, WIFI_CTRL_NS,
				 WIFI_RESET_SET, val);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WIFI_CTRL_NS[WIFI_RESET_SET]: %d\n", ret);
		return ret;
	}
	return 0;
}

/* WLBT START */
static int platform_mif_start(struct scsc_mif_abs *interface, bool start)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val = 0;
	s32                 ret = 0;

	if (start)
		val = WIFI_START;
	/* See sequence in 8.6.6 */
	ret = regmap_update_bits(platform->pmureg, WIFI_CTRL_S,
				 WIFI_START, val);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WIFI_CTRL_S[WIFI_START]: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SCSC_MX150_EXT_DUAL_FEM
	/* Control for device with external GPIO-controlled FEM.
	 * Note this also needs a change in the board-specific dts file
	 */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "exynos_pmu_update ext-fem\n");

	exynos_pmu_update(0x6200, (0x3 << 20), (0x3 << 20));
	exynos_pmu_update(0x6200, (0x3 << 4), (0x3 << 4));

	exynos_pmu_read(0x6200, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UART_IO_SHARE_CTRL: 0x%x\n", val);
#endif
	return 0;
}

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	int                 ret = 0;

#if defined(CONFIG_SOC_EXYNOS7885)
	exynos_pmu_shared_reg_enable();
#endif

	ret = platform_mif_power(interface, true);
	if (ret)
		goto exit;
	ret = platform_mif_hold_reset(interface, false);
	if (ret)
		goto exit;
	ret = platform_mif_start(interface, true);
	if (ret)
		goto exit;

exit:
#if defined(CONFIG_SOC_EXYNOS7885)
	exynos_pmu_shared_reg_disable();
#endif
	return ret;
}

static int platform_mif_pmu_reset(struct scsc_mif_abs *interface, u8 rst_case)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       timeout = jiffies + msecs_to_jiffies(500);
	int                 ret;
	u32                 val;

	if (rst_case == 0 || rst_case > 2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Incorrect pmu reset case %d\n", rst_case);
		return -EIO;
	}

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	ret = regmap_update_bits(platform->pmureg, RESET_AHEAD_WIFI_PWR_REG,
				 SYS_PWR_CFG_2, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update RESET_ASB_WIFI_SYS_PWR_REG %d\n", ret);
		return ret;
	}
#elif defined(CONFIG_SOC_EXYNOS7570)
	ret = regmap_update_bits(platform->pmureg, RESET_ASB_WIFI_SYS_PWR_REG,
				 SYS_PWR_CFG_2, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update RESET_ASB_WIFI_SYS_PWR_REG %d\n", ret);
		return ret;
	}

#endif
	ret = regmap_update_bits(platform->pmureg, CLEANY_BUS_WIFI_SYS_PWR_REG,
				 SYS_PWR_CFG, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update CLEANY_BUS_WIFI_SYS_PWR_REG%d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(platform->pmureg, LOGIC_RESET_WIFI_SYS_PWR_REG,
				 SYS_PWR_CFG_2, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update LOGIC_RESET_WIFI_SYS_PWR_REG %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(platform->pmureg, TCXO_GATE_WIFI_SYS_PWR_REG,
				 SYS_PWR_CFG, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update TCXO_GATE_WIFI_SYS_PWR_REG %d\n", ret);
		return ret;
	}
#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	ret = regmap_update_bits(platform->pmureg, WIFI_DISABLE_ISO_SYS_PWR_REG,
				 SYS_PWR_CFG, 1);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WIFI_DISABLE_ISO_SYS_PWR_REG %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(platform->pmureg, WIFI_RESET_ISO_SYS_PWR_REG,
				 SYS_PWR_CFG, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WIFI_RESET_ISO_SYS_PWR_REG %d\n", ret);
		return ret;
	}
#endif
	ret = regmap_update_bits(platform->pmureg, CENTRAL_SEQ_WIFI_CONFIGURATION,
				 SYS_PWR_CFG_16, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update CENTRAL_SEQ_WIFI_CONFIGURATION %d\n", ret);
		return ret;
	}

	if (rst_case == 1)
		ret = platform_mif_power(interface, false);
	else
		ret = platform_mif_hold_reset(interface, true);

	if (ret)
		return ret;

	do {
		regmap_read(platform->pmureg, CENTRAL_SEQ_WIFI_STATUS, &val);
		val &= STATES;
		val >>= 16;
		if (val == 0x80)
			return 0;
	} while (time_before(jiffies, timeout));

	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
		"Timeout waiting for CENTRAL_SEQ_WIFI_STATUS SM status\n");

	return -ETIME;
}

/* reset=0 - release from reset */
/* reset=1 - hold reset */
static int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 ret = 0;
	u32                 val;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (enable_platform_mif_arm_reset || !reset) {
		if (!reset) { /* Release from reset */
#ifdef CONFIG_ARCH_EXYNOS
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"SOC_VERSION: product_id 0x%x, rev 0x%x\n",
				exynos_soc_info.product_id, exynos_soc_info.revision);
#endif
			/* Enable R4 access to MIF resources */
			/* Address is fixed and its value is in the dts */
			/* It defines the base address of DRAM space which WIFI accesses.
			 * Default value is 0x0_6000_0000. It can configure only MSB 14-bit.
			 * You must align this base address with WIFI memory access size that WIFI_MEM_SIZE
			 * defines */
			/* >>12 represents 4K aligment (see size)*/
			val = (platform->mem_start & 0xFFFFFC000) >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG1, val);
			/* Size */
			/*It defines the DRAM memory size to which WLBT can
			 *      access.
			 *      Definition of the size has 4 KB resolution defined from
			 *      minimum 4 KB to maximum 1GB.
			 *      20'b0000_0000_0000_0000_0001 = 4KB
			 *      20'b0000_0000_0000_0000_0010 = 8KB
			 *      20'b0000_0000_0001_0000_0000 = 1 MB
			 *      20'b0000_0000_0010_0000_0000 = 2 MB
			 *      20'b0000_0000_0100_0000_0000 = 4 MB (default)
			 *      20'b0000_0000_1000_0000_0000 = 8 MB
			 *      20'b0000_1000_0000_0000_0000 = 128 MB
			 *      20'b0100_0000_0000_0000_0000 = 1 GB
			 */
			/* Size is fixed and its value is in the dts */
			/* >>12 represents 4K aligment (see address)*/
			/* For the time being we will hardcode the 12MiB
			 * partition as 8M + 4M. Configure the first partition */
			val = (8*1024*1024) >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG0, val);
			/* Configure the Second partition */
			val = ((platform->mem_start + 8*1024*1024) & 0xFFFFFC000) >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG3, val);
			val = (4*1024*1024) >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG2, val);

			/* BAAW1 for BT-ABOX shared memory */
			val = (platform->btaboxmem_start & 0xFFFFFC000) >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG5, val);
			val = platform->btaboxmem_size >> 12;
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG4, val);

#if (defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)) && defined(CONFIG_ACPM_DVFS)
			/* Immediately prior to reset release, set up ACPM
			 * to ensure BUCK2 gets the right voltage
			 */
			exynos_acpm_set_flag();
#endif
			ret = platform_mif_pmu_reset_release(interface);
		} else {
			/* Put back into reset */
			ret = platform_mif_pmu_reset(interface, 2);

			/* WLBT should be stopped/powered-down at this point */
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG0, 0x00000);
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG1, 0x00000);
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG2, 0x00000);
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG3, 0x00000);
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG4, 0x00000);
			regmap_write(platform->pmureg, WIFI2AP_MEM_CONFIG5, 0x00000);
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
			 enable_platform_mif_arm_reset);
	return ret;
}

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	int         i;
	struct page **pages;
	void        *vmem;

	size = PAGE_ALIGN(size);

	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return NULL;

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

	kfree(pages);
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
	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
		platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
#endif
	}
	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#ifdef CONFIG_SOC_EXYNOS7570
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR2), 0x0000ffff);
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#endif

	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#ifdef CONFIG_SOC_EXYNOS7570
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR2), 0x0000ffff);
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#endif
	/*Set WLBT_BOOT_TEST_RST_CFG to 0 to boot from external DRAM */
	regmap_write(platform->pmureg, WLBT_BOOT_TEST_RST_CFG, 0x00000);
	/* Add more requrired initialization here: */

#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
	/* PIO muxes switching to the Maxwell subsystem */
	/* GPR4_CON (0x13750040) = 0x00666666 */
	if (gpr4_debug) {
		reg_bkp = readl(gpio_base);
		writel(0x00666666, gpio_base);
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "[WARNING] Changing GPR4_CON from 0x%x to 0x%x", reg_bkp, readl(gpio_base));
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
	u32                 prev;

	if (gpr4_debug && reg_update) {
		prev = readl(gpio_base);
		writel(reg_bkp, gpio_base);
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "[WARNING] Restoring GPR4_CON from 0x%x to 0x%x", prev, readl(gpio_base));
	}
	reg_update = false;
#endif
	/* Avoid unused parameter error */
	(void)mem;

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#ifdef CONFIG_SOC_EXYNOS7570
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR2), 0x0000ffff);
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#endif

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
#ifdef CONFIG_SOC_EXYNOS7570
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR2), 0x0000ffff);
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#endif
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
	/* Generate INT to R4/M4 - VIC */
	if (target == SCSC_MIF_ABS_TARGET_R4)
		reg = INTGR1;
	else if (target == SCSC_MIF_ABS_TARGET_M4)
#ifdef CONFIG_SOC_EXYNOS7570
		reg = INTGR2;
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
		reg = INTGR1;
#endif
	else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect Target %d\n", target);
		return;
	}

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	/* Belt and braces. In the case of invoking monitor mode,
	 * ensure that the from-ap interrupt is unmasked. FW
	 * should already do this.
	 */
	if (target == SCSC_MIF_ABS_TARGET_R4 && bit_num == MIFINTRBIT_RESERVED_PANIC_R4)
		__platform_mif_fh_irq_bit_unmask(interface, bit_num);
#endif

#ifdef CONFIG_SOC_EXYNOS7570
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	if (target == SCSC_MIF_ABS_TARGET_R4)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
	else
		platform_mif_reg_write_m4(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
#endif
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

/* Unmask the from-host interrrupt bit.
 * Note that typically FW should unmask this, this is for special purposes only.
 */
#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
static void __platform_mif_fh_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val;
	unsigned long flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1));
	/* WRITE : 0 = Unmask Interrupt (no << 16  on MR1) */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), val & ~(1 << bit_num));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
		"UNMASK Setting INTMR1: 0x%x bit %d\n", val & ~(1 << bit_num), bit_num);

	/* Warn if this bit was previously masked, FW should have unmasked it */
	if (val & (1 << bit_num)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"INTMR1: 0x%x bit %d found masked!\n", val, bit_num);
	}
}
#endif

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

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR1)));
#ifdef CONFIG_SOC_EXYNOS7570
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR2 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR2)));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1(M4) 0x%08x\n", platform_mif_reg_read_m4(platform, MAILBOX_WLBT_REG(INTGR1)));
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR1)));
#ifdef CONFIG_SOC_EXYNOS7570
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR2 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR2)));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1(M4) 0x%08x\n", platform_mif_reg_read_m4(platform, MAILBOX_WLBT_REG(INTCR1)));
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1)));
#ifdef CONFIG_SOC_EXYNOS7570
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR2 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR2)));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1(M4) 0x%08x\n", platform_mif_reg_read_m4(platform, MAILBOX_WLBT_REG(INTMR1)));
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR1)));
#ifdef CONFIG_SOC_EXYNOS7570
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR2 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR2)));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1(M4) 0x%08x\n", platform_mif_reg_read_m4(platform, MAILBOX_WLBT_REG(INTSR1)));
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR1)));
#ifdef CONFIG_SOC_EXYNOS7570
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR2 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR2)));
#elif defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1(M4) 0x%08x\n", platform_mif_reg_read_m4(platform, MAILBOX_WLBT_REG(INTMSR1)));
#endif

	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_CLK20MHZ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Restore USBPLL owenership to the AP so that USB driver may control it */
	__platform_mif_usbpll_claim(platform, false);
#endif
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_CLK20MHZ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Restore USBPLL owenership to the wlbt */
	__platform_mif_usbpll_claim(platform, true);
#endif
}

static void platform_mif_get_abox_shared_mem(struct scsc_mif_abs *interface, void **data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");

	if (data) {
		btaboxdata.btaboxmem_start = platform->btaboxmem_start;
		btaboxdata.btaboxmem_size  = platform->btaboxmem_size;

		*data = &btaboxdata;
	}
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

static int __init platform_mif_btabox_if_reserved_mem_setup(struct reserved_mem *resmem)
{
	SCSC_TAG_DEBUG(PLAT_MIF, "btabox memory reserved: mem_base=%#lx, mem_size=%zd\n",
		(unsigned long)resmem->base, (size_t)resmem->size);

	btaboxsharedmem_base = resmem->base;
	btaboxsharedmem_size = resmem->size;

	return 0;
}
RESERVEDMEM_OF_DECLARE(btabox_rmem, "exynos,btabox_rmem", platform_mif_btabox_if_reserved_mem_setup);
#endif

#ifdef CONFIG_SCSC_CLK20MHZ
static void __platform_mif_usbpll_claim(struct platform_mif *platform, bool wlbt)
{

	s32 ret = 0;

	if (!platform->cmu_base) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "USBPLL claim not enabled\n");
		return;
	}

	/* Set USBPLL ownership, by setting AP2WLBT_USBPLL_WPLL_SEL */

	if (wlbt) {
		/* WLBT f/w has control */
		ret = regmap_update_bits(platform->cmu_base, USBPLL_CON1, AP2WLBT_USBPLL_WPLL_SEL, 0);
		if (ret < 0)
			goto error;
	} else {
		/* Ensure PLL runs */
		ret = regmap_update_bits(platform->cmu_base, USBPLL_CON1, AP2WLBT_USBPLL_WPLL_EN, AP2WLBT_USBPLL_WPLL_EN);
		if (ret < 0)
			goto error;

		/* AP has control */
		udelay(platform->usbpll_delay);
		ret = regmap_update_bits(platform->cmu_base, USBPLL_CON1, AP2WLBT_USBPLL_WPLL_SEL, AP2WLBT_USBPLL_WPLL_SEL);
		if (ret < 0)
			goto error;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "USBPLL_CON1 assigned to %s\n", wlbt ? "WLBT" : "AP");
	return;

error:
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Failed to update USBPLL_CON1 to %s\n", wlbt ? "WLBT" : "AP");

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
#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	struct resource     *reg_res_m4;
#endif
#ifdef CONFIG_SCSC_CLK20MHZ
	/* usb pll ownership */
	const char          *usbowner  = NULL;
	u32                 usbdelay   = 0;
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
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
	platform_if->get_abox_shared_mem = platform_mif_get_abox_shared_mem;

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
	platform->btaboxmem_start = btaboxsharedmem_base;
	platform->btaboxmem_size = btaboxsharedmem_size;
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding */
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
#ifndef CONFIG_SOC_EXYNOS7570
	/* TZASC configuration is required for WLBT to access DRAM from Katmai onward */
	/* Base address should be 4KB aligned. This call needs proper support in EL3_MON */
	err = exynos_smc(EXYNOS_SMC_WLBT_TZASC_CMD, WLBT_TZASC, platform->mem_start,
			 platform->mem_size);
	/* Anyway we keep on WLBT initialization even if TZASC failed to minimize disruption*/
	if (err)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "WLBT Failed to configure TZASC, err=%d. DRAM could be NOT accessible in secure mode\n", err);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT TZASC configured OK\n");
#endif

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"platform->mem_start 0x%lx platform->mem_size 0x%x\n",
		(uintptr_t)platform->mem_start, (u32)platform->mem_size);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"platform->btaboxmem_start 0x%lx platform->btaboxmem_size 0x%x\n",
		(uintptr_t)platform->btaboxmem_start, (u32)platform->btaboxmem_size);

	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any sense.
		 * This may be an indication of an incorrect platform device binding. */
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
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->reg_start %lx size %x base %p\n", (uintptr_t)platform->reg_start, (u32)platform->reg_size, platform->base);

#if defined(CONFIG_SOC_EXYNOS7872) || defined(CONFIG_SOC_EXYNOS7885)
	/* Memory resource for M4 MBOX bank - Phys Address of MAILBOX_WLBT register map */
	reg_res_m4 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!reg_res_m4) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error getting mem resource for MAILBOX_WLBT\n");
		err = -ENOENT;
		goto error_exit;
	}
	platform->reg_start_m4 = reg_res_m4->start;
	platform->reg_size_m4 = resource_size(reg_res_m4);

	platform->base_m4 =
		devm_ioremap_nocache(platform->dev, reg_res_m4->start, resource_size(reg_res_m4));

	if (!platform->base_m4) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error mapping M4 register region\n");
		err = -EBUSY;
		goto error_exit;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->reg_start_m4 %lx size_m4 %x base_m4 %p\n", (uintptr_t)platform->reg_start_m4, (u32)platform->reg_size_m4, platform->base_m4);
#endif

#ifdef CONFIG_SCSC_CLK20MHZ
	/* Get usbpll,owner if Maxwell has to provide 20Mhz clock to USB subsystem */
	platform->cmu_base = NULL;
	if (of_property_read_string(pdev->dev.of_node, "usbpll,owner", &usbowner)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "usbpll,owner of_property does not exist or is invalid\n");
		goto cont;
	} else {
		if (strcasecmp(usbowner, "y"))
			goto skip_usbpll;

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "usbpll,owner is enabled\n");

		/* CMU_FSYS reg map - syscon */
		platform->cmu_base = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,syscon-cmu_fsys");
		if (IS_ERR(platform->cmu_base)) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"syscon regmap lookup failed. Aborting. %ld\n", PTR_ERR(platform->cmu_base));
			goto skip_usbpll;
		}

		if (of_property_read_u32(pdev->dev.of_node, "usbpll,udelay", &usbdelay))
			goto skip_usbpll;
		platform->usbpll_delay = usbdelay;

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Mapping CMU_FSYS with %dus delay\n", usbdelay);

		goto cont;
skip_usbpll:
		platform->cmu_base = NULL;
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "usbpll owner skipped\n");
	}
cont:
#endif
#ifdef CONFIG_SCSC_GPR4_CON_DEBUG
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Mapping GPR4_CON 0x13750040\n");
	gpio_base =
		devm_ioremap_nocache(platform->dev, 0x13750040, 4);
#endif

	/* Get the 3 IRQ resources */
	for (i = 0; i < 3; i++) {
		struct resource *irq_res;
		int             irqtag;

		irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!irq_res) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "No IRQ resource at index %d\n", i);
			err = -ENOENT;
			goto error_exit;
		}

		if (!strcmp(irq_res->name, "MBOX")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MBOX irq %d flag 0x%x\n", (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res->name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ALIVE irq %d flag 0x%x\n", (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res->name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG irq %d flag 0x%x\n", (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_WDOG;
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
			"syscon regmap lookup failed. Aborting. %ld\n", PTR_ERR(platform->pmureg));
		err = -EINVAL;
		goto error_exit;
	}

#ifdef CONFIG_SCSC_CLK20MHZ
	/* Assign USBPLL ownership to WLBT f/w */
	__platform_mif_usbpll_claim(platform, true);
#endif

	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

	/* Clear WIFI_ACTIVE flag in WAKEUP_STAT */
	err = regmap_update_bits(platform->pmureg, WIFI_CTRL_NS, WIFI_ACTIVE_CLR, 1);
	if (err < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WIFI_CTRL_NS[WIFI_ACTIVE_CLR]: %d\n", err);
	}
	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_CLK20MHZ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Assign USBPLL ownership to AP */
	__platform_mif_usbpll_claim(platform, false);
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

	/* Clear WIFI_ACTIVE flag in WAKEUP_STAT */
	ret = regmap_update_bits(platform->pmureg, WIFI_CTRL_NS, WIFI_ACTIVE_CLR, 1);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WIFI_CTRL_NS[WIFI_ACTIVE_CLR]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}
