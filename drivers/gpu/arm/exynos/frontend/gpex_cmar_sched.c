// SPDX-License-Identifier: GPL-2.0

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

/* Implements */
#include <gpex_cmar_sched.h>

/* Uses */
#include <linux/sched.h>

#include <gpex_utils.h>

static struct cpumask mask;
static struct cpumask full_mask;
static struct cpumask forced_mask;
static int is_forced_sched_enabled;
static int forced_min_index;
static int forced_max_index;
static int total_cpu_count;

int gpex_cmar_sched_set_forced_sched(int mode)
{
	if (mode) {
		if (is_forced_sched_enabled) {
			mask = forced_mask;
			GPU_LOG(MALI_EXYNOS_INFO, "cmar sched set forced AMIGO On: cpu mask=0x%x, min=%d, max=%d",
				is_forced_sched_enabled, mask, forced_min_index, forced_max_index);
		} else
			GPU_LOG(MALI_EXYNOS_INFO, "cmar sched set forced disabled");
	} else {
		mask = full_mask;
		GPU_LOG(MALI_EXYNOS_INFO, "cmar sched set forced AMIGO Off: cpu mask=0x%x", mask);
	}

	return 0;
}

static int gpex_cmar_sched_set_cpu(int min, int max, struct cpumask *mask)
{
	int index = 0;

	if ((min < 0) || (min > total_cpu_count - 1) || (max <= min) || (max > total_cpu_count)) {
		GPU_LOG(MALI_EXYNOS_WARNING, "%s: invalid value  min:%d, max:%d", __func__, min, max);
		return -ENOENT;
	}

	cpumask_clear(mask);
	for (index = min; index < max; index++)
		cpumask_set_cpu(index, mask);

	return 0;
}

int gpex_cmar_sched_set_affinity(void)
{
	return set_cpus_allowed_ptr(current, &mask);
}

static ssize_t show_cmar_forced_sched_enable(char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d", is_forced_sched_enabled);

	return gpex_utils_sysfs_endbuf(buf, len);
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_cmar_forced_sched_enable);

static ssize_t show_cmar_sched_min_index(char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d", forced_min_index);

	return gpex_utils_sysfs_endbuf(buf, len);
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_cmar_sched_min_index);

static ssize_t show_cmar_sched_max_index(char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d", forced_max_index);

	return gpex_utils_sysfs_endbuf(buf, len);
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_cmar_sched_max_index);

static ssize_t set_cmar_forced_sched_enable(const char *buf, size_t count)
{
	int ret, flag;

	ret = kstrtoint(buf, 0, &flag);

	if (ret) {
		GPU_LOG(MALI_EXYNOS_WARNING, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	is_forced_sched_enabled = flag;

	return count;
}
CREATE_SYSFS_DEVICE_WRITE_FUNCTION(set_cmar_forced_sched_enable);

static ssize_t set_cmar_sched_min_index(const char *buf, size_t count)
{
	int ret, index;

	ret = kstrtoint(buf, 0, &index);

	if (ret) {
		GPU_LOG(MALI_EXYNOS_WARNING, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if (gpex_cmar_sched_set_cpu(index, forced_max_index, &forced_mask) == 0)
		forced_min_index = index;

	return count;
}
CREATE_SYSFS_DEVICE_WRITE_FUNCTION(set_cmar_sched_min_index);

static ssize_t set_cmar_sched_max_index(const char *buf, size_t count)
{
	int ret, index;

	ret = kstrtoint(buf, 0, &index);

	if (ret) {
		GPU_LOG(MALI_EXYNOS_WARNING, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if (gpex_cmar_sched_set_cpu(forced_min_index, index, &forced_mask) == 0)
		forced_max_index = index;

	return count;
}
CREATE_SYSFS_DEVICE_WRITE_FUNCTION(set_cmar_sched_max_index);

int gpex_cmar_sched_sysfs_init(void)
{
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD(cmar_forced_sched_enable, show_cmar_forced_sched_enable,
					 set_cmar_forced_sched_enable);
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD(cmar_sched_min_index, show_cmar_sched_min_index,
					 set_cmar_sched_min_index);
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD(cmar_sched_max_index, show_cmar_sched_max_index,
					 set_cmar_sched_max_index);

	return 0;
}

int gpex_cmar_sched_init(void)
{
	is_forced_sched_enabled = 1;
	forced_min_index = 0;
	forced_max_index = 6;
	total_cpu_count = 8;

	gpex_cmar_sched_set_cpu(forced_min_index, forced_max_index, &forced_mask);
	gpex_cmar_sched_set_cpu(0, total_cpu_count, &full_mask);
	mask = full_mask;

	gpex_cmar_sched_sysfs_init();

	return 0;
}

void gpex_cmar_sched_term(void)
{
	mask = full_mask;
}
