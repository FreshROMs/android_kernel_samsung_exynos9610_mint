#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/cal-if.h>
#include "../../soc/samsung/cal-if/acpm_dvfs.h"
#include "pmu-gnss.h"
#include "gnss_prj.h"

/* For connectivity I/F */
//#define SMC_CMD_CONN_IF		(0x82000710)
/* Connectivity sub system */
#define EXYNOS_GNSS		(0)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC	(0)

static int gnss_request_tzpc(void);
static void gnss_request_gnss2ap_baaw(void);

#ifdef USE_IOREMAP_NOPMU
#if defined(CONFIG_SOC_EXYNOS7870)
#define PMU_ADDR		(0x10480000)
#define PMU_SIZE		(SZ_64K)
#elif defined(CONFIG_SOC_EXYNOS7880)
#define PMU_ADDR		(0x106B0000)
#define PMU_SIZE		(SZ_64K)
#elif defined(CONFIG_SOC_EXYNOS7570)
#define PMU_ADDR		(0x11C80000)
#define PMU_SIZE		(SZ_64K)
#elif defined(CONFIG_SOC_EXYNOS7872)
#define PMU_ADDR		(0x11C80000)
#define PMU_SIZE		(SZ_64K)
#elif defined(CONFIG_SOC_EXYNOS9610)
#define PMU_ADDR		(0x11860000)
#define PMU_SIZE		(SZ_64K)
#endif

static void __iomem *pmu_reg;
static int gnss_nopmu_read(unsigned int reg_offset, unsigned int *ret)
{
	*ret = __raw_readl(pmu_reg + reg_offset);
	return 0;
}

static int gnss_nopmu_write(unsigned int reg_offset, unsigned int val)
{
	unsigned int tmp, tmp2;
	tmp = __raw_readl(pmu_reg + reg_offset);
	__raw_writel(val, pmu_reg + reg_offset);
	tmp2 = __raw_readl(pmu_reg + reg_offset);

	return (tmp == tmp2) ? 0 : -EINVAL;
}

static int gnss_nopmu_update(unsigned int reg_offset, unsigned int mask,
			unsigned int val)
{
	unsigned int memcfg_val;
	unsigned int tmp, tmp2;
	tmp = __raw_readl(pmu_reg + reg_offset);
	memcfg_val = tmp;
	memcfg_val &= ~mask;
	memcfg_val |= val;
	__raw_writel(memcfg_val, pmu_reg + reg_offset);
	tmp2 = __raw_readl(pmu_reg + reg_offset);

	return (memcfg_val == tmp2) ? 0 : -EINVAL;
}

#define gnss_pmu_read	gnss_nopmu_read
#define gnss_pmu_write	gnss_nopmu_write
#define gnss_pmu_update	gnss_nopmu_update

#else

#define gnss_pmu_read	exynos_pmu_read
#define gnss_pmu_write	exynos_pmu_write
#define gnss_pmu_update exynos_pmu_update

#endif /* USE_IOREMAP_NOPMU */



#if defined(CONFIG_SOC_EXYNOS9610)
static u32 g_shmem_size;
static u32 g_shmem_base;

#define BAAW_GNSS_CMGP_ADDR	(0x13EE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13ED0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

static void __iomem *baaw_cmgp_reg;
static void __iomem *baaw_dbus_reg;

int gnss_cmgp_read(unsigned int reg_offset, unsigned int *ret)
{
	if (baaw_cmgp_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_cmgp_reg + reg_offset);
	return 0;
}

int gnss_cmgp_write(unsigned int reg_offset, unsigned int val)
{
	if (baaw_cmgp_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_cmgp_reg + reg_offset);
	gif_info("ADDR:%08X DATA:%08X => Read to verify:%08X\n", BAAW_GNSS_CMGP_ADDR + reg_offset, val, __raw_readl(baaw_cmgp_reg + reg_offset));

	return 0;
}

int gnss_dbus_read(unsigned int reg_offset, unsigned int *ret)
{
	if (baaw_dbus_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_dbus_reg + reg_offset);
	return 0;
}

int gnss_dbus_write(unsigned int reg_offset, unsigned int val)
{
	if (baaw_dbus_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_dbus_reg + reg_offset);
	gif_info("ADDR:%08X DATA:%08X => Read to verify:%08X\n", BAAW_GNSS_DBUS_ADDR + reg_offset, val, __raw_readl(baaw_dbus_reg + reg_offset));

	return 0;
}
#endif

void __set_shdmem_size(u32 reg_offset, u32 memsz)
{
	memsz /= MEMSIZE_RES;
	gnss_pmu_update(reg_offset, MEMSIZE_MASK, memsz << MEMSIZE_OFFSET);
}

void set_shdmem_size(u32 memsz)
{
	gif_info("Set shared mem size: %dB\n", memsz);

	__set_shdmem_size(EXYNOS_PMU_GNSS2AP_MEM_CONFIG0, memsz);
}

void __set_shdmem_base(u32 reg_offset, u32 shmem_base)
{
	u32 base_addr;
	base_addr = (shmem_base >> MEMBASE_ADDR_SHIFT);

	gnss_pmu_update(reg_offset, MEMBASE_ADDR_MASK << MEMBASE_ADDR_OFFSET,
			base_addr << MEMBASE_ADDR_OFFSET);
}

void set_shdmem_base(u32 shmem_base)
{
	gif_info("Set shared mem baseaddr : 0x%x\n", shmem_base);

	__set_shdmem_base(EXYNOS_PMU_GNSS2AP_MEM_CONFIG1, shmem_base);
}

void exynos_sys_powerdown_conf_gnss(void)
{
	gnss_pmu_write(EXYNOS_PMU_RESET_AHEAD_GNSS_SYS_PWR_REG, 0);
	gnss_pmu_write(EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG, 0);
	gnss_pmu_write(EXYNOS_PMU_LOGIC_RESET_GNSS_SYS_PWR_REG, 0);
	gnss_pmu_write(EXYNOS_PMU_TCXO_GATE_GNSS_SYS_PWR_REG, 0);
	gnss_pmu_write(EXYNOS_PMU_GNSS_DISABLE_ISO_SYS_PWR_REG, 1);
	gnss_pmu_write(EXYNOS_PMU_GNSS_RESET_ISO_SYS_PWR_REG, 0);
	gnss_pmu_write(EXYNOS_PMU_CENTRAL_SEQ_GNSS_CONFIGURATION, 0);
}

static int gnss_pmu_clear_interrupt(enum gnss_int_clear gnss_int)
{
	int ret = 0;

	gif_info("gnss_int = %d\n", gnss_int);
	switch (gnss_int) {
	case GNSS_INT_WAKEUP_CLEAR:
		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_WAKEUP_REQ_CLR, GNSS_WAKEUP_REQ_CLR);
		break;
	case GNSS_INT_ACTIVE_CLEAR:
		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_ACTIVE_REQ_CLR, GNSS_ACTIVE_REQ_CLR);
		break;
	case GNSS_INT_WDT_RESET_CLEAR:
		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_RESET_REQ_CLR, GNSS_RESET_REQ_CLR);
		break;
	default:
		gif_err("Unexpected interrupt value!\n");
		return -EIO;
	}

	if (ret < 0) {
		gif_err("ERR! GNSS Reset Fail: %d\n", ret);
		return -EIO;
	}

	return ret;
}

static void gnss_pmu_check_status(void)
{
	u32 gnss_ctrl;
	u32 gnss_pwr_req;
	u32 gnss_tcxo_req;
	u32 gnss_mif_req;

	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
	gnss_pmu_read(EXYNOS_PMU_SHARED_PWR_REQ_GNSS_CONTROL, &gnss_pwr_req);
	gnss_pmu_read(EXYNOS_PMU_SHARED_TCXO_REQ_GNSS_CONTROL, &gnss_tcxo_req);
	gnss_pmu_read(EXYNOS_PMU_SHARED_MIF_REQ_GNSS_CONTROL, &gnss_mif_req);

	gif_info("PMU_GNSS_CTRL_S[0x%08x]\n", gnss_ctrl);
	gif_info("PMU_GNSS_SHARED_PWR_REQ[0x%08x]\n", gnss_pwr_req);
	gif_info("PMU_GNSS_SHARED_TCXO_REQ[0x%08x]\n", gnss_tcxo_req);
	gif_info("PMU_GNSS_SHARED_MIF_REQ[0x%08x]\n", gnss_mif_req);
}

static int gnss_pmu_release_reset(void)
{
	u32 __maybe_unused gnss_ctrl = 0;
	int ret = 0;

#if defined(CONFIG_SOC_EXYNOS7872)
	pr_err("%s: call exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_LOCK) before release GNSS\n", __func__);
	exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_LOCK);
#endif

#ifdef CONFIG_GNSS_PMUCAL
	ret = cal_gnss_reset_release();
	if (ret)
		return -EINVAL;
#else
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
	if (!(gnss_ctrl & GNSS_PWRON)) {
		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_PWRON,
				GNSS_PWRON);
		if (ret < 0) {
			gif_err("ERR! write Fail: %d\n", ret);
			ret = -EIO;
		}
	}
	ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_S, GNSS_START, GNSS_START);
	if (ret < 0) {
		gif_err("ERR! GNSS Release Fail: %d\n", ret);
	} else {
		gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_S, &gnss_ctrl);
		gif_info("PMU_GNSS_CTRL_S[0x%08x]\n", gnss_ctrl);
		ret = -EIO;
	}
#endif

#if defined(CONFIG_SOC_EXYNOS7872)
	pr_err("%s: call exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_UNLOCK) after release GNSS\n", __func__);
	exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_UNLOCK);
#endif

	return ret;
}

static int gnss_pmu_hold_reset(void)
{
	int ret = 0;

#if defined(CONFIG_SOC_EXYNOS7872)
	pr_err("%s: call exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_LOCK) before reset GNSS\n", __func__);
	exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_LOCK);
#endif

#ifdef CONFIG_GNSS_PMUCAL
	cal_gnss_reset_assert();
	mdelay(50);
#else
	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_gnss();

	ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_RESET_SET,
			GNSS_RESET_SET);
	if (ret < 0) {
		gif_err("ERR! GNSS Reset Fail: %d\n", ret);
		ret = -1;
		goto exit;
	}

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);
exit:
#endif

#if defined(CONFIG_SOC_EXYNOS7872)
	pr_err("%s: call exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_UNLOCK) after reset GNSS\n", __func__);
	exynos_acpm_set_flag(MASTER_ID_GNSS, FLAG_UNLOCK);
#endif

	return ret;
}

static int gnss_request_tzpc(void)
{
	int ret;

	ret = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_GNSS << 31) |
			EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret)
		gif_err("ERR: fail to TZPC setting - %X\n", ret);

	return ret;
}

static void gnss_request_gnss2ap_baaw(void)
{
	gif_info("Config GNSS2AP BAAW\n");

	gif_info("ap mailbox configuration\n");
	gnss_cmgp_write(0x40, 0x000b4190);
	gnss_cmgp_write(0x44, 0x000b41a0);
	gnss_cmgp_write(0x48, 0x00011a00);
	gnss_cmgp_write(0x4C, 0x80000003);

	gif_info("DRAM Configuration\n");
	gnss_dbus_write(0x0, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x4, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x8, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0xC, 0x80000003);

	gnss_dbus_write(0x10, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x14, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x18, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x1C, 0x80000003);

	gif_info("apm2gnss mailbox configuration\n");
	gnss_cmgp_write(0x0, 0x000b4120);
	gnss_cmgp_write(0x4, 0x000b4130);
	gnss_cmgp_write(0x8, 0x000119f0);
	gnss_cmgp_write(0xC, 0x80000003);

	gif_info("cp2gnss mailbox configuration\n");
	gnss_cmgp_write(0x10, 0x000b4150);
	gnss_cmgp_write(0x14, 0x000b4160);
	gnss_cmgp_write(0x18, 0x00011940);
	gnss_cmgp_write(0x1C, 0x80000003);

	gif_info("chub mailbox configuration\n");
	gnss_cmgp_write(0x20, 0x000b4160);
	gnss_cmgp_write(0x24, 0x000b4170);
	gnss_cmgp_write(0x28, 0x00011990);
	gnss_cmgp_write(0x2C, 0x80000003);

	gif_info("wifi mailbox configuration\n");
	gnss_cmgp_write(0x30, 0x000b4170);
	gnss_cmgp_write(0x34, 0x000b4180);
	gnss_cmgp_write(0x38, 0x000119e0);
	gnss_cmgp_write(0x3C, 0x80000003);

	gif_info("chub iram configuration\n");
	gnss_cmgp_write(0x50, 0x000b3900);
	gnss_cmgp_write(0x54, 0x000b3a00);
	gnss_cmgp_write(0x58, 0x00011200);
	gnss_cmgp_write(0x5C, 0x80000003);

	gif_info("CMGP PERIS configuration\n");
	gnss_cmgp_write(0x60, 0x000b4200);
	gnss_cmgp_write(0x64, 0x000b4400);
	gnss_cmgp_write(0x68, 0x00011c00);
	gnss_cmgp_write(0x6C, 0x80000003);
}

static int gnss_pmu_power_on(enum gnss_mode mode)
{
	u32 __maybe_unused gnss_ctrl;
	int ret = 0;

	gif_err("mode[%d]\n", mode);

#ifdef CONFIG_GNSS_PMUCAL
	if (mode == GNSS_POWER_ON) {
		if (cal_gnss_status() > 0) {
			gif_err("GNSS is already Power on, try reset\n");
			cal_gnss_reset_assert();
		} else {
			gif_info("GNSS Power On\n");
			cal_gnss_init();
		}
	} else {
		cal_gnss_reset_release();
	}
#else
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
	if (mode == GNSS_POWER_ON) {
		if (!(gnss_ctrl & GNSS_PWRON)) {
			ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
					GNSS_PWRON, GNSS_PWRON);
			if (ret < 0)
				gif_err("ERR! write Fail: %d\n", ret);
		}

		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_S, GNSS_START,
				GNSS_START);
		if (ret < 0)
			gif_err("ERR! write Fail: %d\n", ret);
	} else {
		ret = gnss_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_PWRON, 0);
		if (ret < 0) {
			gif_err("ERR! write Fail: %d\n", ret);
			return ret;
		}
		/* set sys_pwr_cfg registers */
		exynos_sys_powerdown_conf_gnss();
	}
#endif
	return ret;
}

static int gnss_pmu_init_conf(struct gnss_ctl *gc)
{
	u32 __maybe_unused shmem_size, shmem_base;

#ifdef USE_IOREMAP_NOPMU
	pmu_reg = devm_ioremap(gc->dev, PMU_ADDR, PMU_SIZE);
	if (pmu_reg == NULL)
		gif_err("%s: pmu ioremap failed.\n", gc->gnss_data->name);
	else
		gif_err("pmu_reg : 0x%p\n", pmu_reg);
#endif

#if defined(CONFIG_SOC_EXYNOS9610)
	baaw_cmgp_reg = devm_ioremap(gc->dev, BAAW_GNSS_CMGP_ADDR, BAAW_GNSS_CMGP_SIZE);
	if (baaw_cmgp_reg == NULL) {
		gif_err("%s: pmu ioremap failed.\n", gc->gnss_data->name);
		return -EIO;
	} else
		gif_err("baaw_cmgp_reg : 0x%p\n", baaw_cmgp_reg);

	baaw_dbus_reg = devm_ioremap(gc->dev, BAAW_GNSS_DBUS_ADDR, BAAW_GNSS_DBUS_SIZE);
	if (baaw_dbus_reg == NULL) {
		gif_err("%s: pmu ioremap failed.\n", gc->gnss_data->name);
		return -EIO;
	} else
		gif_err("baaw_dbus_reg : 0x%p\n", baaw_dbus_reg);

	g_shmem_size = gc->gnss_data->shmem_size;
	g_shmem_base = gc->gnss_data->shmem_base;

	gif_info("GNSS SHM address:%X size:%X\n", g_shmem_base, g_shmem_size);
#else
	shmem_size = gc->gnss_data->shmem_size;
	shmem_base = gc->gnss_data->shmem_base;

	set_shdmem_size(shmem_size);
	set_shdmem_base(shmem_base);

	/* set access window for GNSS */
	gnss_pmu_write(EXYNOS_PMU_GNSS2AP_MIF_ACCESS_WIN0, 0x0);
	gnss_pmu_write(EXYNOS_PMU_GNSS2AP_PERI_ACCESS_WIN0, 0x0);
	gnss_pmu_write(EXYNOS_PMU_GNSS2AP_PERI_ACCESS_WIN1, 0x0);
#endif

	return 0;
}

static struct gnssctl_pmu_ops pmu_ops = {
	.init_conf = gnss_pmu_init_conf,
	.hold_reset = gnss_pmu_hold_reset,
	.check_status = gnss_pmu_check_status,
	.release_reset = gnss_pmu_release_reset,
	.power_on = gnss_pmu_power_on,
	.clear_int = gnss_pmu_clear_interrupt,
	.req_security = gnss_request_tzpc,
	.req_baaw = gnss_request_gnss2ap_baaw,
};

void gnss_get_pmu_ops(struct gnss_ctl *gc)
{
	gc->pmu_ops = &pmu_ops;
	return;
}
