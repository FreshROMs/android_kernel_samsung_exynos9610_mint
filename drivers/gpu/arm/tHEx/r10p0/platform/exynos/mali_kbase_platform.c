/* drivers/gpu/arm/.../platform/mali_kbase_platform.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_platform.c
 * Platform-dependent init.
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_custom_interface.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include <soc/samsung/cal-if.h>
#include <linux/of_platform.h>

#if MALI_SEC_SECURE_RENDERING
#include <linux/smc.h>
#include <mali_kbase_device_internal.h>

/* SMC CALL return value for Successfully works */
#define GPU_SMC_TZPC_OK 0
#endif

#ifndef KHZ
#define KHZ (1000)
#endif

struct kbase_device *pkbdev;
static int gpu_debug_level;

struct kbase_device *gpu_get_device_structure(void)
{
	return pkbdev;
}

void gpu_set_debug_level(int level)
{
	gpu_debug_level = level;
}

int gpu_get_debug_level(void)
{
	return gpu_debug_level;
}

#ifdef CONFIG_MALI_EXYNOS_TRACE
struct kbase_trace exynos_trace_buf[KBASE_TRACE_SIZE];
extern const struct file_operations kbasep_trace_debugfs_fops;
static int gpu_trace_init(struct kbase_device *kbdev)
{
	kbdev->trace_rbuf = exynos_trace_buf;

	spin_lock_init(&kbdev->trace_lock);

/* below work : register entry from making debugfs create file to trace_dentry
 * is same work as kbasep_trace_debugfs_init */
#ifdef MALI_SEC_INTEGRATION
	kbdev->trace_dentry = debugfs_create_file("mali_trace", S_IRUGO,
			kbdev->mali_debugfs_directory, kbdev,
			&kbasep_trace_debugfs_fops);
#endif /* MALI_SEC_INTEGRATION */
	return 0;
}

static int gpu_trace_level;

void gpu_set_trace_level(int level)
{
	int i;

	if (level == TRACE_ALL) {
		for (i = TRACE_NONE + 1; i < TRACE_ALL; i++)
			gpu_trace_level |= (1U << i);
	} else if (level == TRACE_NONE) {
		gpu_trace_level = TRACE_NONE;
	} else {
		gpu_trace_level |= (1U << level);
	}
}

bool gpu_check_trace_level(int level)
{
	if (gpu_trace_level & (1U << level))
		return true;
	return false;
}

bool gpu_check_trace_code(int code)
{
	int level;
	switch (code) {
	case KBASE_TRACE_CODE(DUMMY):
		return false;
	case KBASE_TRACE_CODE(LSI_CLOCK_VALUE):
	case KBASE_TRACE_CODE(LSI_CLOCK_ON):
	case KBASE_TRACE_CODE(LSI_CLOCK_OFF):
	case KBASE_TRACE_CODE(LSI_GPU_MAX_LOCK):
	case KBASE_TRACE_CODE(LSI_GPU_MIN_LOCK):
	case KBASE_TRACE_CODE(LSI_SECURE_WORLD_ENTER):
	case KBASE_TRACE_CODE(LSI_SECURE_WORLD_EXIT):
	case KBASE_TRACE_CODE(LSI_SECURE_CACHE):
	case KBASE_TRACE_CODE(LSI_SECURE_CACHE_END):
	case KBASE_TRACE_CODE(LSI_KBASE_PM_INIT_HW):
	case KBASE_TRACE_CODE(LSI_IFPM_POWER_ON):
	case KBASE_TRACE_CODE(LSI_IFPM_POWER_OFF):
		level = TRACE_CLK;
		break;
	case KBASE_TRACE_CODE(LSI_VOL_VALUE):
		level = TRACE_VOL;
		break;
	case KBASE_TRACE_CODE(LSI_GPU_ON):
	case KBASE_TRACE_CODE(LSI_GPU_OFF):
	case KBASE_TRACE_CODE(LSI_ZAP_TIMEOUT):
	case KBASE_TRACE_CODE(LSI_RESET_GPU_EARLY_DUPE):
	case KBASE_TRACE_CODE(LSI_RESET_RACE_DETECTED_EARLY_OUT):
	case KBASE_TRACE_CODE(LSI_PM_SUSPEND):
	case KBASE_TRACE_CODE(LSI_SUSPEND):
	case KBASE_TRACE_CODE(LSI_RESUME):
	case KBASE_TRACE_CODE(LSI_TMU_VALUE):
		level = TRACE_NOTIFIER;
		break;
	case KBASE_TRACE_CODE(LSI_REGISTER_DUMP):
		level = TRACE_DUMP;
		break;
	default:
		level = TRACE_DEFAULT;
		break;
	}

	return gpu_check_trace_level(level);
}
#endif /* CONFIG_MALI_EXYNOS_TRACE */

uintptr_t gpu_get_attrib_data(gpu_attribute *attrib, int id)
{
	int i;

	for (i = 0; attrib[i].id != GPU_CONFIG_LIST_END; i++) {
		if (attrib[i].id == id)
			return attrib[i].data;
	}

	return 0;
}

static int gpu_validate_attrib_data(struct exynos_context *platform)
{
	uintptr_t data;
	gpu_attribute *attrib = (gpu_attribute *)gpu_get_config_attributes();
	unsigned long(*funcptr)(unsigned int);
	unsigned int cal_id;
	struct kbase_device *kbdev;
	struct device_node *np;

	platform->attrib = attrib;

	data = gpu_get_attrib_data(attrib, GPU_MAX_CLOCK);
	platform->gpu_max_clock = data == 0 ? 500 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_MAX_CLOCK_LIMIT);

	/*
	platform->gpu_max_clock_limit = data == 0 ? 500 : (u32) data;
	*/

	kbdev = gpu_get_device_structure();
	np = kbdev->dev->of_node;
	of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id);

	funcptr = (unsigned long(*)(unsigned int))data;
	platform->gpu_max_clock_limit = (int)funcptr(cal_id)/KHZ;

	data = gpu_get_attrib_data(attrib, GPU_MIN_CLOCK);
	platform->gpu_min_clock = data == 0 ? 160 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_START_CLOCK);
	platform->gpu_dvfs_start_clock = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_BL_CONFIG_CLOCK);
	platform->gpu_dvfs_config_clock = data == 0 ? 266 : (u32) data;

#ifdef CONFIG_MALI_DVFS
#ifdef CONFIG_CPU_THERMAL_IPA
	data = gpu_get_attrib_data(attrib, GPU_POWER_COEFF);
	platform->ipa_power_coeff_gpu = data == 0 ? 59 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_TIME_INTERVAL);
	platform->gpu_dvfs_time_interval = data == 0 ? 5 : (u32) data;
#endif /* CONFIG_CPU_THERMAL_IPA */
	data = gpu_get_attrib_data(attrib, GPU_DEFAULT_WAKEUP_LOCK);
	platform->wakeup_lock = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TYPE);
	platform->governor_type = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_DEFAULT);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_DEFAULT, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_DEFAULT);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_DEFAULT, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_DEFAULT);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_DEFAULT, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_STATIC);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_STATIC, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_STATIC);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_STATIC, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_STATIC);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_STATIC, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_BOOSTER);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_BOOSTER, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_BOOSTER);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_BOOSTER, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_BOOSTER);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_BOOSTER, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_INTERACTIVE);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_INTERACTIVE, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_INTERACTIVE);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_INTERACTIVE, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_INTERACTIVE, (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK);
	platform->interactive.highspeed_clock = data == 0 ? 500 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD);
	platform->interactive.highspeed_load = data == 0 ? 100 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY);
	platform->interactive.highspeed_delay = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_DVFS_POLLING_TIME);
	platform->polling_speed = data == 0 ? 100 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_PMQOS_INT_DISABLE);
	platform->pmqos_int_disable = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_PMQOS_MIF_MAX_CLOCK);
	platform->pmqos_mif_max_clock = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_PMQOS_MIF_MAX_CLOCK_BASE);
	platform->pmqos_mif_max_clock_base = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_CL_DVFS_START_BASE);
	platform->cl_dvfs_start_base = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING1);
	platform->tmu_lock_clk[THROTTLING1] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING2);
	platform->tmu_lock_clk[THROTTLING2] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING3);
	platform->tmu_lock_clk[THROTTLING3] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING4);
	platform->tmu_lock_clk[THROTTLING4] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING5);
	platform->tmu_lock_clk[THROTTLING5] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_TRIPPING);
	platform->tmu_lock_clk[TRIPPING] = data == 0 ? 266 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_BOOST_MIN_LOCK);
	platform->boost_gpu_min_lock = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_BOOST_EGL_MIN_LOCK);
	platform->boost_egl_min_lock = data == 0 ? 0 : (u32) data;
#endif /* CONFIG_MALI_DVFS */

	data = gpu_get_attrib_data(attrib, GPU_TMU_CONTROL);
	platform->tmu_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_DEFAULT_VOLTAGE);
	platform->gpu_default_vol = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_COLD_MINIMUM_VOL);
	platform->cold_min_vol = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_VOLTAGE_OFFSET_MARGIN);
	platform->gpu_default_vol_margin = data == 0 ? 37500 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_BUS_DEVFREQ);
	platform->devfreq_status = data == 0 ? 1 : data;
	data = gpu_get_attrib_data(attrib, GPU_DYNAMIC_ABB);
	platform->dynamic_abb_status = data == 0 ? 0 : data;
	data = gpu_get_attrib_data(attrib, GPU_EARLY_CLK_GATING);
	platform->early_clk_gating_status = data == 0 ? 0 : data;
	data = gpu_get_attrib_data(attrib, GPU_DVS);
	platform->dvs_status = data == 0 ? 0 : data;
	data = gpu_get_attrib_data(attrib, GPU_INTER_FRAME_PM);
	platform->inter_frame_pm_feature = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_PERF_GATHERING);
	platform->perf_gathering_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_RUNTIME_PM_DELAY_TIME);
	platform->runtime_pm_delay_time = data == 0 ? 50 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_DEBUG_LEVEL);
	gpu_debug_level = data == 0 ? DVFS_WARNING : (u32) data;
#ifdef CONFIG_MALI_EXYNOS_TRACE
	data = gpu_get_attrib_data(attrib, GPU_TRACE_LEVEL);
	gpu_set_trace_level(data == 0 ? TRACE_ALL : (u32) data);
#endif /* CONFIG_MALI_EXYNOS_TRACE */
	data = gpu_get_attrib_data(attrib, GPU_MO_MIN_CLOCK);
	platform->mo_min_clock = data == 0 ? 0 : (u32) data;

#ifdef CONFIG_MALI_VK_BOOST
	data = gpu_get_attrib_data(attrib, GPU_VK_BOOST_MAX_LOCK);
	platform->gpu_vk_boost_max_clk_lock = data == 0 ? 50 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_VK_BOOST_MIF_MIN_LOCK);
	platform->gpu_vk_boost_mif_min_clk_lock = data == 0 ? 50 : (u32) data;
#endif

	return 0;
}

static int gpu_context_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	struct mali_base_gpu_core_props *core_props;

	platform = kmalloc(sizeof(struct exynos_context), GFP_KERNEL);

	if (platform == NULL)
		return -1;

	memset(platform, 0, sizeof(struct exynos_context));
	kbdev->platform_context = (void *) platform;
	pkbdev = kbdev;

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);

#ifdef CONFIG_SCHED_EHMP
	mutex_init(&platform->gpu_sched_hmp_lock);
	platform->ctx_need_qos = false;
#endif

#ifdef CONFIG_MALI_VK_BOOST
	mutex_init(&platform->gpu_vk_boost_lock);
	platform->ctx_vk_need_qos = false;
#endif

	gpu_validate_attrib_data(platform);

	core_props = &(kbdev->gpu_props.props.core_props);
	core_props->gpu_freq_khz_max = platform->gpu_max_clock * 1000;

#if MALI_SEC_PROBE_TEST != 1
	kbdev->vendor_callbacks = (struct kbase_vendor_callbacks *)gpu_get_callbacks();
#endif

#ifdef CONFIG_MALI_EXYNOS_TRACE
	if (gpu_trace_init(kbdev) != 0)
		return -1;
#endif

#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	platform->gpu_auto_cali_status = false;
#endif

	platform->inter_frame_pm_status = platform->inter_frame_pm_feature;

	return 0;
}

#ifdef CONFIG_MALI_GPU_CORE_MASK_SELECTION
static void gpu_core_mask_set(struct kbase_device *kbdev)
{
	u64 default_core_mask = 0x0;
	void __iomem *core_fused_reg;
	u64 temp, core_info;
	u64 val;
	u64 core_stack[8] = {0, };
	int i = 0;
	void __iomem *lotid_fused_reg;
	u64 lotid_val, lotid_info;

	lotid_fused_reg = ioremap(0x10000004, SZ_8K);
	lotid_val = __raw_readl(lotid_fused_reg);
	lotid_info = lotid_val & 0xFFFFF;

	if (lotid_info == 0x3A8D3) {    /* core mask code for KC first lot */
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] first lot!!!\n");
		core_fused_reg = ioremap(0x1000903c, SZ_8K);	/* GPU DEAD CORE Info */
		val = __raw_readl(core_fused_reg);

		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] core fused reg info, Addr[0x%llx], Data[0x%llx]\n", (unsigned long long)core_fused_reg, val);
		core_info = (val >> 8) & 0xFFFFF;

		if (core_info) {	/* has dead core more 1-core */
			temp = (~core_info) & 0xFFFFF;

			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] core last info = 0x%llx\n", temp);
			core_stack[0] = temp & 0xF;	            /* core 0, 1, 2, 3 */
			core_stack[1] = (temp & 0x70) >> 4;	    /* core 4, 5, 6 */
			core_stack[2] = (temp & 0x380) >> 7;    /* core 7, 8, 9 */
			core_stack[4] = (temp & 0x3C00) >> 10;  /* core 10, 11, 12, 13 */
			core_stack[5] = (temp & 0x1C000) >> 14; /* core 14, 15, 16 */
			core_stack[6] = (temp & 0xE0000) >> 17; /* core 17, 18, 19 */

			for (i = 0; i < 8; i++) {
				if (i == 3 || i == 7)
					continue;

				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] before core stack[%d] = 0x%llx\n", i, core_stack[i]);
				if (core_stack[i] == 0xb)
					core_stack[i] = 3;	/* 0b1011 */
				if (core_stack[i] == 0xd)
					core_stack[i] = 1;	/* 0b1101 */
				if (core_stack[i] == 0x9)
					core_stack[i] = 1;	/* 0b1001 */
				if (core_stack[i] == 0x5)
					core_stack[i] = 1;	/* 0b101  */
				if (!(core_stack[i] == 0x1 || core_stack[i] == 0x3 || core_stack[i] == 0x7 || core_stack[i] == 0xf))
					core_stack[i] = 0;
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] after core stack[%d] = 0x%llx\n", i, core_stack[i]);

				if (i < 4) {
					default_core_mask |= (((core_stack[i] >> 0) & 0x1) << (0 + i));
					default_core_mask |= (((core_stack[i] >> 1) & 0x1) << (4 + i));
					default_core_mask |= (((core_stack[i] >> 2) & 0x1) << (8 + i));
					default_core_mask |= (((core_stack[i] >> 3) & 0x1) << (12 + i));
				} else {
					default_core_mask |= (((core_stack[i] >> 0) & 0x1) << (16 + i - 4));
					default_core_mask |= (((core_stack[i] >> 1) & 0x1) << (20 + i - 4));
					default_core_mask |= (((core_stack[i] >> 2) & 0x1) << (24 + i - 4));
					default_core_mask |= (((core_stack[i] >> 3) & 0x1) << (28 + i - 4));
				}
			}
			kbdev->pm.debug_core_mask_info = default_core_mask;
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] has dead core!, normal core mask = 0x%llx\n", default_core_mask);
		} else {
			kbdev->pm.debug_core_mask_info = 0x17771777;
		}
	} else {	/* Have to use this code since 'KC second lot' release */
		core_fused_reg = ioremap(0x1000A024, SZ_1K);    /* GPU DEAD CORE Info */
		val = __raw_readl(core_fused_reg);

		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] core fused reg info, Addr[0x%llx], Data[0x%llx]\n", (unsigned long long)core_fused_reg, val);
		core_info = val;
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] core shift info = 0x%llx\n", core_info);

		if (core_info) {        /* has dead core more 1-core */
			temp = (~core_info) & 0x17771777;

			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] core last info = 0x%llx\n", temp);
			core_stack[0] = temp & 0x1111;          /* core 0, 1, 2, 3 */
			core_stack[1] = (temp & 0x222);         /* core 4, 5, 6    */
			core_stack[2] = (temp & 0x444);         /* core 7, 8, 9    */
			core_stack[4] = (temp & 0x11110000) >> 16;      /* core 10, 11, 12, 13 */
			core_stack[5] = (temp & 0x2220000) >> 16;       /* core 14, 15, 16     */
			core_stack[6] = (temp & 0x4440000) >> 16;       /* core 17, 18, 19     */

			for (i = 0; i < 8; i++) {
				if (i == 3 || i == 7)
					continue;

				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] before core stack[%d] = 0x%llx\n", i, core_stack[i]);
				if(i==1 || i==5) core_stack[i] = core_stack[i] >> 1;
				if(i==2 || i==6) core_stack[i] = core_stack[i] >> 2;
				if (core_stack[i] == 0x1011)
					core_stack[i] = 0x0011;         /* 0b1011 */
				if (core_stack[i] == 0x1101)
					core_stack[i] = 0x0001;         /* 0b1101 */
				if (core_stack[i] == 0x1001)
					core_stack[i] = 0x0001;         /* 0b1001 */
				if (core_stack[i] == 0x101)
					core_stack[i] = 0x0001;		/* 0b101 */
				if (!(core_stack[i] == 0x1 || core_stack[i] == 0x11 || core_stack[i] == 0x111 || core_stack[i] == 0x1111))
					core_stack[i] = 0;
				if (i == 1 || i == 5)
					core_stack[i] = core_stack[i] << 1;
				if (i == 2 || i == 6)
					core_stack[i] = core_stack[i] << 2;
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] after core stack[%d] = 0x%llx\n", i, core_stack[i]);

				if (i < 4) {
					default_core_mask |= core_stack[i];
				} else {
					default_core_mask |= (core_stack[i]<<16);
				}
			}
			kbdev->pm.debug_core_mask_info = default_core_mask;
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "[GPU] has dead core!, normal core mask = 0x%llx\n", default_core_mask);
		} else {
			kbdev->pm.debug_core_mask_info = 0x17771777;
		}
	}
	iounmap(core_fused_reg);
	iounmap(lotid_fused_reg);
}
#endif

/**
 ** Exynos5 hardware specific initialization
 **/
static int kbase_platform_exynos5_init(struct kbase_device *kbdev)
{
	/* gpu context init */
	if (gpu_context_init(kbdev) < 0)
		goto init_fail;

#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
	if (gpu_device_specific_init(kbdev) < 0)
		goto init_fail;
#endif
	/* gpu control module init */
	if (gpu_control_module_init(kbdev) < 0)
		goto init_fail;

	/* gpu notifier init */
	if (gpu_notifier_init(kbdev) < 0)
		goto init_fail;

#ifdef CONFIG_MALI_DVFS
	/* gpu utilization moduel init */
	gpu_dvfs_utilization_init(kbdev);

	/* dvfs governor init */
	gpu_dvfs_governor_init(kbdev);

	/* dvfs handler init */
	gpu_dvfs_handler_init(kbdev);
#endif /* CONFIG_MALI_DVFS */

#ifdef CONFIG_MALI_DEBUG_SYS
	/* gpu sysfs file init */
	if (gpu_create_sysfs_file(kbdev->dev) < 0)
		goto init_fail;
#endif /* CONFIG_MALI_DEBUG_SYS */
	/* MALI_SEC_INTEGRATION */
#ifdef CONFIG_MALI_GPU_CORE_MASK_SELECTION
	gpu_core_mask_set(kbdev);
#endif

	return 0;

init_fail:
	kfree(kbdev->platform_context);

	return -1;
}

/**
 ** Exynos5 hardware specific termination
 **/
static void kbase_platform_exynos5_term(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	platform = (struct exynos_context *) kbdev->platform_context;

	gpu_notifier_term();

#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_handler_deinit(kbdev);
#endif /* CONFIG_MALI_DVFS */

	gpu_dvfs_utilization_deinit(kbdev);

	gpu_control_module_term(kbdev);

	kfree(kbdev->platform_context);
	kbdev->platform_context = 0;

#ifdef CONFIG_MALI_DEBUG_SYS
	gpu_remove_sysfs_file(kbdev->dev);
#endif /* CONFIG_MALI_DEBUG_SYS */
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

/* MALI_SEC_SECURE_RENDERING */
#if MALI_SEC_SECURE_RENDERING
static int exynos_secure_mode_enable(struct protected_mode_device *pdev)
{
	/* enable secure mode : TZPC */
	struct kbase_device *kbdev = pdev->data;
	int ret = 0;

	if (!kbdev)
		goto secure_out;

	if (!kbdev->protected_mode_support) {
		GPU_LOG(DVFS_ERROR, LSI_GPU_SECURE, 0u, 0u, "%s: wrong operation! DDK cannot support Secure Rendering\n", __func__);
		ret = -EINVAL;
		goto secure_out;
	}

	ret = exynos_smc(SMC_PROTECTION_SET, 0,
			PROT_G3D, SMC_PROTECTION_ENABLE);

	GPU_LOG(DVFS_INFO, LSI_SECURE_WORLD_ENTER, 0u, 0u, "LSI_SECURE_WORLD_ENTER\n");

	if (ret == GPU_SMC_TZPC_OK) {
		ret = 0;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: Enter Secure World by GPU\n", __func__);
	} else {
		GPU_LOG(DVFS_ERROR, LSI_GPU_SECURE, 0u, 0u, "%s: failed exynos_smc() ret : %d\n", __func__, ret);
	}

secure_out:
	return ret;
}

static int exynos_secure_mode_disable(struct protected_mode_device *pdev)
{
	/* Turn off secure mode and reset GPU : TZPC */
	struct kbase_device *kbdev = pdev->data;
	int ret = 0;

	if (!kbdev)
		goto secure_out;

	if (!kbdev->protected_mode_support) {
		GPU_LOG(DVFS_ERROR, LSI_GPU_SECURE, 0u, 0u, "%s: wrong operation! DDK cannot support Secure Rendering\n", __func__);
		ret = -EINVAL;
		goto secure_out;
	}

	ret = exynos_smc(SMC_PROTECTION_SET, 0,
			PROT_G3D, SMC_PROTECTION_DISABLE);

	GPU_LOG(DVFS_INFO, LSI_SECURE_WORLD_EXIT, 0u, 0u, "LSI_SECURE_WORLD_EXIT\n");

	if (ret == GPU_SMC_TZPC_OK) {
		ret = 0;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: Exit Secure World by GPU\n", __func__);
	} else {
		GPU_LOG(DVFS_ERROR, LSI_GPU_SECURE, 0u, 0u, "%s: failed exynos_smc() ret : %d\n", __func__, ret);
	}

secure_out:
	return ret;
}

struct protected_mode_ops exynos_protected_ops = {
	.protected_mode_enable = exynos_secure_mode_enable,
	.protected_mode_disable = exynos_secure_mode_disable
};
#endif

int kbase_platform_early_init(void)
{
	return 0;
}
