/* drivers/gpu/arm/.../platform/mali_kbase_platform.h
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
 * @file mali_kbase_platform.h
 * Platform-dependent init
 */

#ifndef _GPU_PLATFORM_H_
#define _GPU_PLATFORM_H_

#ifdef CONFIG_MALI_EXYNOS_TRACE
#define GPU_LOG(level, code, gpu_addr, info_val, msg, args...) \
do { \
	if (level >= gpu_get_debug_level()) { \
		printk(KERN_INFO "[G3D] "msg, ## args); \
	} \
	if (gpu_check_trace_code(KBASE_TRACE_CODE(code))) { \
		KBASE_TRACE_ADD_EXYNOS(gpu_get_device_structure(), code, NULL, NULL, gpu_addr, info_val); \
	} \
} while (0)
#else /* CONFIG_MALI_EXYNOS_TRACE */
#define GPU_LOG(level, code, gpu_addr, info_val, msg, args...) \
do { \
	if (level >= gpu_get_debug_level()) { \
		printk(KERN_INFO msg, ## args); \
	} \
} while (0)
#endif /* CONFIG_MALI_EXYNOS_TRACE */

#define GPU_DVFS_TABLE_LIST_SIZE(X)  ARRAY_SIZE(X)

#define BMAX_RETRY_CNT 10

typedef enum {
	DVFS_DEBUG_START = 0,
	DVFS_DEBUG,
	DVFS_INFO,
	DVFS_WARNING,
	DVFS_ERROR,
	DVFS_DEBUG_END,
} gpu_dvfs_debug_level;

typedef enum {
	GPU_L0,
	GPU_L1,
	GPU_L2,
	GPU_L3,
	GPU_L4,
	GPU_L5,
	GPU_L6,
	GPU_L7,
	GPU_MAX_LEVEL,
} gpu_clock_level;

typedef enum {
	TRACE_START = 0,
	TRACE_NONE,
	TRACE_DEFAULT,
	TRACE_CLK,
	TRACE_VOL,
	TRACE_NOTIFIER,
	TRACE_DVFS,
	TRACE_DUMP,
	TRACE_ALL,
	TRACE_END,
} gpu_dvfs_trace_level;

typedef enum {
	TMU_LOCK = 0,
	SYSFS_LOCK,
#ifdef CONFIG_CPU_THERMAL_IPA
	IPA_LOCK,
#endif /* CONFIG_CPU_THERMAL_IPA */
	BOOST_LOCK,
	PMQOS_LOCK,
#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	ASV_CALI_LOCK,
#endif
	NUMBER_LOCK
} gpu_dvfs_lock_type;

typedef enum {
	THROTTLING1 = 0,
	THROTTLING2,
	THROTTLING3,
	THROTTLING4,
	THROTTLING5,
	TRIPPING,
	TMU_LOCK_CLK_END,
} tmu_lock_clk;

typedef enum {
	GPU_MAX_CLOCK = 0,
	GPU_MAX_CLOCK_LIMIT,
	GPU_MIN_CLOCK,
	GPU_DVFS_START_CLOCK,
	GPU_DVFS_BL_CONFIG_CLOCK,
	GPU_GOVERNOR_TYPE,
	GPU_GOVERNOR_START_CLOCK_DEFAULT,
	GPU_GOVERNOR_START_CLOCK_INTERACTIVE,
	GPU_GOVERNOR_START_CLOCK_STATIC,
	GPU_GOVERNOR_START_CLOCK_BOOSTER,
	GPU_GOVERNOR_TABLE_DEFAULT,
	GPU_GOVERNOR_TABLE_INTERACTIVE,
	GPU_GOVERNOR_TABLE_STATIC,
	GPU_GOVERNOR_TABLE_BOOSTER,
	GPU_GOVERNOR_TABLE_SIZE_DEFAULT,
	GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE,
	GPU_GOVERNOR_TABLE_SIZE_STATIC,
	GPU_GOVERNOR_TABLE_SIZE_BOOSTER,
	GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK,
	GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD,
	GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY,
	GPU_DEFAULT_VOLTAGE,
	GPU_COLD_MINIMUM_VOL,
	GPU_VOLTAGE_OFFSET_MARGIN,
	GPU_TMU_CONTROL,
	GPU_TEMP_THROTTLING1,
	GPU_TEMP_THROTTLING2,
	GPU_TEMP_THROTTLING3,
	GPU_TEMP_THROTTLING4,
	GPU_TEMP_THROTTLING5,
	GPU_TEMP_TRIPPING,
	GPU_BOOST_MIN_LOCK,
	GPU_BOOST_EGL_MIN_LOCK,
	GPU_POWER_COEFF,
	GPU_DVFS_TIME_INTERVAL,
	GPU_DEFAULT_WAKEUP_LOCK,
	GPU_BUS_DEVFREQ,
	GPU_DYNAMIC_ABB,
	GPU_EARLY_CLK_GATING,
	GPU_DVS,
	GPU_INTER_FRAME_PM,
	GPU_PERF_GATHERING,
	GPU_RUNTIME_PM_DELAY_TIME,
	GPU_DVFS_POLLING_TIME,
	GPU_PMQOS_INT_DISABLE,
	GPU_PMQOS_MIF_MAX_CLOCK,
	GPU_PMQOS_MIF_MAX_CLOCK_BASE,
	GPU_CL_DVFS_START_BASE,
	GPU_DEBUG_LEVEL,
	GPU_TRACE_LEVEL,
	GPU_MO_MIN_CLOCK,
#ifdef CONFIG_MALI_VK_BOOST
	GPU_VK_BOOST_MAX_LOCK,
	GPU_VK_BOOST_MIF_MIN_LOCK,
#endif
	GPU_CONFIG_LIST_END,
} gpu_config_list;

typedef enum {
	GPU_JOB_CONFIG_FAULT,
	GPU_JOB_POWER_FAULT,
	GPU_JOB_READ_FAULT,
	GPU_JOB_WRITE_FAULT,
	GPU_JOB_AFFINITY_FAULT,
	GPU_JOB_BUS_FAULT,
	GPU_DATA_INVALIDATE_FAULT,
	GPU_TILE_RANGE_FAULT,
	GPU_OUT_OF_MEMORY_FAULT,
	GPU_DELAYED_BUS_FAULT,
	GPU_SHAREABILITY_FAULT,
	GPU_MMU_TRANSLATION_FAULT,
	GPU_MMU_PERMISSION_FAULT,
	GPU_MMU_TRANSTAB_BUS_FAULT,
	GPU_MMU_ACCESS_FLAG_FAULT,
	GPU_MMU_ADDRESS_SIZE_FAULT,
	GPU_MMU_MEMORY_ATTRIBUTES_FAULT,
	GPU_UNKNOWN,
	GPU_SOFT_STOP,
	GPU_HARD_STOP,
	GPU_RESET,
	GPU_EXCEPTION_LIST_END,
} gpu_excention_type;

typedef struct _gpu_attribute {
	int id;
	uintptr_t data;
} gpu_attribute;

typedef struct _gpu_dvfs_info {
	unsigned int clock;
	unsigned int voltage;
	int asv_abb;
	int min_threshold;
	int max_threshold;
	int down_staycount;
	unsigned long long time;
	int mem_freq;
	int int_freq;
	int cpu_freq;
	int cpu_max_freq;
	int g3dm_voltage;
} gpu_dvfs_info;

typedef struct _gpu_dvfs_governor_info {
	int id;
	char *name;
	void *governor;
	gpu_dvfs_info *table;
	int table_size;
	int start_clk;
} gpu_dvfs_governor_info;

typedef struct _gpu_dvfs_env_data {
	int utilization;
	int perf;
	int hwcnt;
} gpu_dvfs_env_data;

struct exynos_context {
	/* lock variables */
	struct mutex gpu_clock_lock;
	struct mutex gpu_dvfs_handler_lock;
	spinlock_t gpu_dvfs_spinlock;
#ifdef CONFIG_SCHED_EHMP
	struct mutex gpu_sched_hmp_lock;
#endif
	/* clock & voltage related variables */
	int clk_g3d_status;
#ifdef CONFIG_MALI_RT_PM
	struct exynos_pm_domain *exynos_pm_domain;
#endif /* CONFIG_MALI_RT_PM */

	/* dvfs related variables */
	gpu_dvfs_info *table;
	int table_size;
	int step;
	gpu_dvfs_env_data env_data;
	struct workqueue_struct *dvfs_wq;
	struct delayed_work *delayed_work;
#if defined(SET_MINLOCK)
	int custom_cpu_max_lock;
#endif
#ifdef CONFIG_MALI_DVFS
	bool dvfs_status;
	int utilization;
	int max_lock;
	int min_lock;
	int user_max_lock[NUMBER_LOCK];
	int user_min_lock[NUMBER_LOCK];
	int down_requirement;
	int governor_type;
	bool wakeup_lock;
	int dvfs_pending;

	/* For the interactive governor */
	struct {
		int highspeed_clock;
		int highspeed_load;
		int highspeed_delay;
		int delay_count;
	} interactive;
#ifdef CONFIG_CPU_THERMAL_IPA
	int norm_utilisation;
	int freq_for_normalisation;
	unsigned long long power;
	int time_tick;
	u32 time_busy;
	u32 time_idle;

	int ipa_power_coeff_gpu;
	int gpu_dvfs_time_interval;
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif /* CONFIG_MALI_DVFS */

	/* status */
	int cur_clock;
	int cur_voltage;
	int voltage_margin;

	/* gpu configuration */
	bool using_max_limit_clock;
	int gpu_max_clock;
	int gpu_max_clock_limit;
	int gpu_min_clock;
	int gpu_dvfs_start_clock;
	int gpu_dvfs_config_clock;
	int user_max_lock_input;
	int user_min_lock_input;

	/* gpu boost lock */
	int boost_gpu_min_lock;
	int boost_egl_min_lock;
	bool boost_is_enabled;
	bool tmu_status;
	int tmu_lock_clk[TMU_LOCK_CLK_END];
	int cold_min_vol;
	int gpu_default_vol;
	int gpu_default_vol_margin;

	bool devfreq_status;
	bool dynamic_abb_status;
	bool early_clk_gating_status;
	bool dvs_status;
	bool dvs_is_enabled;
	bool inter_frame_pm_feature;
	bool inter_frame_pm_status;
	bool inter_frame_pm_is_poweron;

	bool power_status;

	bool perf_gathering_status;

	int polling_speed;
	int runtime_pm_delay_time;
	bool pmqos_int_disable;

	int pmqos_mif_max_clock;
	int pmqos_mif_max_clock_base;

	int cl_dvfs_start_base;

	int debug_level;
	int trace_level;

	int fault_count;
	bool bigdata_uevent_is_sent;
	int gpu_exception_count[GPU_EXCEPTION_LIST_END];
	int balance_retry_count[BMAX_RETRY_CNT];
	gpu_attribute *attrib;
	int mo_min_clock;
	int *save_cpu_max_freq;

	unsigned int g3d_cmu_cal_id;
#ifdef CONFIG_MALI_PM_QOS
	bool is_pm_qos_init;
#endif /* CONFIG_MALI_PM_QOS */
	const struct kbase_pm_policy *cur_policy;

#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	bool gpu_auto_cali_status;
#endif

#ifdef CONFIG_SCHED_EHMP
	bool ctx_need_qos;
#endif

#ifdef CONFIG_MALI_VK_BOOST
	bool ctx_vk_need_qos;
	struct mutex gpu_vk_boost_lock;
	int gpu_vk_boost_max_clk_lock;
	int gpu_vk_boost_mif_min_clk_lock;
#endif
};

struct kbase_device *gpu_get_device_structure(void);
void gpu_set_debug_level(int level);
int gpu_get_debug_level(void);
void gpu_set_trace_level(int level);
bool gpu_check_trace_level(int level);
bool gpu_check_trace_code(int code);
void *gpu_get_config_attributes(void);
uintptr_t gpu_get_attrib_data(gpu_attribute *attrib, int id);
int gpu_platform_context_init(struct exynos_context *platform);

int gpu_set_rate_for_pm_resume(struct kbase_device *kbdev, int clk);
void gpu_clock_disable(struct kbase_device *kbdev);

bool balance_init(struct kbase_device *kbdev);
int exynos_gpu_init_hw(void *dev);

#endif /* _GPU_PLATFORM_H_ */
