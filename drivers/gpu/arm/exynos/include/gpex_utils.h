/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _MALI_EXYNOS_UTILS_H_
#define _MALI_EXYNOS_UTILS_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/sysfs.h>

struct kbase_device;

/* per process context (pointed by kctx) */
struct platform_context {
	int cmar_boost;
	pid_t pid;
};

typedef struct ifpo_info *ifpo_info_ptr;
typedef struct _clboost_info *clboost_info_ptr;
typedef struct _utils_info *utils_info_ptr;
typedef struct _clock_info *clock_info_ptr;
typedef struct dvfs_info *dvfs_info_ptr;
typedef struct _gts_info *gts_info_ptr;
typedef struct pm_info *pm_info_ptr;
typedef struct _qos_info *qos_info_ptr;
typedef struct _qos_table *qos_table_ptr;
typedef struct _clqos_table *clqos_table_ptr;
typedef struct _thermal_info *thermal_info_ptr;
typedef struct _tsg_info *tsg_info_ptr;
typedef struct _debug_info *debug_info_ptr;

typedef struct _bts_backend_info *bts_backend_info_ptr;
typedef struct _clock_backend_info *clock_backend_info_ptr;
typedef struct _debug_backend_info *debug_backend_info_ptr;
typedef struct _llc_coh_info *llc_coh_info_ptr;
typedef struct _smc_info *smc_info_ptr;

/**
 * struct exynos_context - contains pointers to each module's private structure. Per device.
 */
struct exynos_context {
	/* frontends */
	ifpo_info_ptr ifpo;
	clboost_info_ptr clboost_info;
	clock_info_ptr clk_info;
	dvfs_info_ptr dvfs;
	gts_info_ptr gts_info;
	pm_info_ptr pm;
	qos_info_ptr qos_info;
	qos_table_ptr qos_table;
	clqos_table_ptr clqos_table;
	thermal_info_ptr thermal;
	tsg_info_ptr tsg;

	/* common */
	utils_info_ptr utils_info;
	debug_info_ptr debug_info;

	/* backends */
	bts_backend_info_ptr bts_info;
	clock_backend_info_ptr pm_info;
	debug_backend_info_ptr dbg_info;
	llc_coh_info_ptr llc_coh_info;
	smc_info_ptr smc_info;
};


#ifndef GPEX_STATIC
#if IS_ENABLED(CONFIG_MALI_EXYNOS_UNIT_TESTS)
#define GPEX_STATIC
#else
#define GPEX_STATIC static
#endif
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define CSTD_UNUSED(x) ((void)(x))

enum { DEBUG = 1, INFO, WARNING, ERROR };

#ifndef GPU_LOG
#define GPU_LOG(level, msg, args...)                                                               \
	do {                                                                                       \
		if (level >= gpex_utils_get_debug_level()) {                                       \
			printk(KERN_INFO "[G3D] " msg, ##args);                                    \
		}                                                                                  \
	} while (0)
#endif

#define DUMMY 0
#define GPU_LOG_DETAILED(level, code, gpu_addr, info_val, msg, args...)                            \
	do {                                                                                       \
		if (level >= gpex_utils_get_debug_level()) {                                       \
			printk(KERN_INFO "[G3D] " msg, ##args);                                    \
		}                                                                                  \
	} while (0)

#define CREATE_SYSFS_DEVICE_READ_FUNCTION(__fname)                                                 \
	GPEX_STATIC ssize_t __fname##_sysfs_device(struct device *dev,                             \
						   struct device_attribute *attr, char *buf)       \
	{                                                                                          \
		return __fname(buf);                                                               \
	}

#define CREATE_SYSFS_DEVICE_WRITE_FUNCTION(__fname)                                                \
	GPEX_STATIC ssize_t __fname##_sysfs_device(                                                \
		struct device *dev, struct device_attribute *attr, const char *buf, size_t count)  \
	{                                                                                          \
		return __fname(buf, count);                                                        \
	}

#define CREATE_SYSFS_KOBJECT_READ_FUNCTION(__fname)                                                \
	GPEX_STATIC ssize_t __fname##_sysfs_kobject(struct kobject *dev,                           \
						    struct kobj_attribute *attr, char *buf)        \
	{                                                                                          \
		return __fname(buf);                                                               \
	}

#define CREATE_SYSFS_KOBJECT_WRITE_FUNCTION(__fname)                                               \
	GPEX_STATIC ssize_t __fname##_sysfs_kobject(                                               \
		struct kobject *dev, struct kobj_attribute *attr, const char *buf, size_t count)   \
	{                                                                                          \
		return __fname(buf, count);                                                        \
	}

#define EXTERN_SYSFS_DEVICE_READ_FUNCTION(__fname)                                                 \
	extern ssize_t __fname##_sysfs_device(struct device *dev, struct device_attribute *attr,   \
					      char *buf)

#define EXTERN_SYSFS_DEVICE_WRITE_FUNCTION(__fname)                                                \
	extern ssize_t __fname##_sysfs_device(struct device *dev, struct device_attribute *attr,   \
					      const char *buf, size_t count)

#define EXTERN_SYSFS_KOBJECT_READ_FUNCTION(__fname)                                                \
	extern ssize_t __fname##_sysfs_kobject(struct kobject *dev, struct kobj_attribute *attr,   \
					       char *buf)

#define EXTERN_SYSFS_KOBJECT_WRITE_FUNCTION(__fname)                                               \
	extern ssize_t __fname##_sysfs_kobject(struct kobject *dev, struct kobj_attribute *attr,   \
					       const char *buf, size_t count)

#define GPEX_UTILS_SYSFS_DEVICE_FILE_ADD(__name, __read_func, __write_func)                        \
	gpex_utils_sysfs_device_file_add(#__name, __read_func##_sysfs_device,                      \
					 __write_func##_sysfs_device)

#define GPEX_UTILS_SYSFS_DEVICE_FILE_ADD_RO(__name, __read_func)                                   \
	gpex_utils_sysfs_device_file_add(#__name, __read_func##_sysfs_device, NULL)

#define GPEX_UTILS_SYSFS_KOBJECT_FILE_ADD(__name, __read_func, __write_func)                       \
	gpex_utils_sysfs_kobject_file_add(#__name, __read_func##_sysfs_kobject,                    \
					  __write_func##_sysfs_kobject)

#define GPEX_UTILS_SYSFS_KOBJECT_FILE_ADD_RO(__name, __read_func)                                  \
	gpex_utils_sysfs_kobject_file_add(#__name, __read_func##_sysfs_kobject, NULL)

enum {
	MALI_EXYNOS_DEBUG_START = 0,
	MALI_EXYNOS_DEBUG,
	MALI_EXYNOS_INFO,
	MALI_EXYNOS_WARNING,
	MALI_EXYNOS_ERROR,
	MALI_EXYNOS_DEBUG_END,
};

typedef ssize_t (*sysfs_kobject_read_func)(struct kobject *, struct kobj_attribute *, char *);
typedef ssize_t (*sysfs_kobject_write_func)(struct kobject *, struct kobj_attribute *, const char *,
					    size_t);
typedef ssize_t (*sysfs_device_read_func)(struct device *, struct device_attribute *, char *);
typedef ssize_t (*sysfs_device_write_func)(struct device *, struct device_attribute *, const char *,
					   size_t);

int gpex_utils_get_debug_level(void);
struct device *gpex_utils_get_device(void);
struct kbase_device *gpex_utils_get_kbase_device(void);
struct exynos_context *gpex_utils_get_exynos_context(void);

int gpex_utils_init(struct device **dev);
void gpex_utils_term(void);

int gpex_utils_sysfs_device_file_add(char *name, sysfs_device_read_func read_func,
				     sysfs_device_write_func write_func);

int gpex_utils_sysfs_kobject_file_add(char *name, sysfs_kobject_read_func read_func,
				      sysfs_kobject_write_func write_func);

/* Only meant to be used once by gpex_platform module */
int gpex_utils_sysfs_kobject_files_create(void);
int gpex_utils_sysfs_device_files_create(void);

static inline ssize_t gpex_utils_sysfs_endbuf(char *buf, ssize_t len)
{
	if (len < PAGE_SIZE - 1) {
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	} else {
		buf[PAGE_SIZE - 2] = '\n';
		buf[PAGE_SIZE - 1] = '\0';
		len = PAGE_SIZE - 1;
	}

	return len;
}

void gpex_utils_sysfs_set_gpu_model_callback(sysfs_device_read_func show_gpu_model_fn);

#endif /* _MALI_EXYNOS_UTILS_H_ */
