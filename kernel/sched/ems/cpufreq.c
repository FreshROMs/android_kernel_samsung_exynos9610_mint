/*
 * Frequency variant cpufreq driver
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <trace/events/ems.h>

#include "../sched.h"
#include "ems.h"

/**********************************************************************
 * common APIs                                                        *
 **********************************************************************/
struct cpufreq_table {
	int frequency;
	int value;
};

void aigov_update_rate_limit_us(struct cpufreq_policy *policy, int up_rate_limit_ms, int down_rate_limit_ms);
int aigov_sysfs_add_attr(struct cpufreq_policy *policy, const struct attribute *attr);
struct cpufreq_policy *aigov_get_attr_policy(struct gov_attr_set *attr_set);

#if 0
static int (*fn_sysfs_add_attr)(struct cpufreq_policy *policy, const struct attribute *attr);
static struct cpufreq_policy (**fn_get_attr_policy)(struct gov_attr_set *attr_set);
static void (*fn_update_rate_limit_us)(struct cpufreq_policy *policy, int up_rate_limit_ms, int down_rate_limit_ms);

/* register call back when cpufreq governor initialized */
void cpufreq_register_hook(int (*func_sysfs_add_attr)(struct cpufreq_policy *policy, const struct attribute *attr),
		int (**func_get_attr_policy)(struct gov_attr_set *attr_set),
		int (*func_update_rate_limit_us)(struct cpufreq_policy *policy, int up_rate_limit_ms, int down_rate_limit_ms))
{
	fn_sysfs_add_attr = func_sysfs_add_attr;
	fn_get_attr_policy = func_get_attr_policy;
	fn_update_rate_limit_us = func_update_rate_limit_us;
}

/* unregister call back when cpufreq governor initialized */
void cpufreq_unregister_hook(void)
{
	fn_sysfs_add_attr = NULL;
	fn_get_attr_policy = NULL;
	fn_update_rate_limit_us = NULL;
}

void cpufreq_update_rate_limit_us(struct cpufreq_policy *policy, int up_rate_limit_ms, int down_rate_limit_ms)
{
	if (likely(fn_update_rate_limit_us)) {
		fn_update_rate_limit_us(policy, up_rate_limit_ms, down_rate_limit_ms);
		return;
	}

	return;
}

int cpufreq_sysfs_add_attr(struct cpufreq_policy *policy, const struct attribute *attr)
{
	if (likely(fn_sysfs_add_attr)) {
		fn_sysfs_add_attr(policy, attr);
		return;
	}

	return;
}

struct cpufreq_policy *cpufreq_get_attr_policy(struct gov_attr_set *attr_set)
{
	if (likely(fn_get_attr_policy)) {
		fn_get_attr_policy(attr_set);
		return;
	}

	return;
}
#endif

static int cpufreq_get_value(int freq, struct cpufreq_table *table)
{
	struct cpufreq_table *pos = table;
	int value = -EINVAL;

	for (; pos->frequency != CPUFREQ_TABLE_END; pos++)
		if (freq == pos->frequency) {
			value = pos->value;
			break;
		}

	return value;
}

static int cpufreq_get_table_size(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *cpufreq_table, *pos;
	int size = 0;

	cpufreq_table = policy->freq_table;
	if (unlikely(!cpufreq_table)) {
		pr_debug("%s: Unable to find frequency table\n", __func__);
		return -ENOENT;
	}

	cpufreq_for_each_valid_entry(pos, cpufreq_table)
		size++;

	return size;
}

static int cpufreq_fill_frequency_table(struct cpufreq_policy *policy,
					struct cpufreq_table *table)
{
	struct cpufreq_frequency_table *cpufreq_table, *pos;
	int index;

	cpufreq_table = policy->freq_table;
	if (unlikely(!cpufreq_table)) {
		pr_debug("%s: Unable to find frequency table\n", __func__);
		return -ENOENT;
	}

	index = 0;
	cpufreq_for_each_valid_entry(pos, cpufreq_table) {
		table[index].frequency = pos->frequency;
		index++;
	}
	table[index].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int cpufreq_update_table(unsigned int *src, int src_size,
					struct cpufreq_table *dst)
{
	struct cpufreq_table *pos, *last_pos = dst;
	unsigned int value = 0, freq = 0;
	int i;

	for (i = src_size - 1; i >= 0; i--) {
		value = src[i];
		freq  = (i <= 0) ? 0 : src[i - 1];

		for (pos = last_pos; pos->frequency != CPUFREQ_TABLE_END; pos++)
			if (pos->frequency >= freq) {
				pos->value = value;
			} else {
				last_pos = pos;
				break;
			}
	}

	return 0;
}

static int cpufreq_parse_value_dt(struct device_node *dn, const char *table_name,
						struct cpufreq_table *table)
{
	int size, ret = 0;
	unsigned int *temp;

	/* get the table from device tree source */
	size = of_property_count_u32_elems(dn, table_name);
	if (size <= 0)
		return size;

	temp = kzalloc(sizeof(unsigned int) * size, GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	ret = of_property_read_u32_array(dn, table_name, temp, size);
	if (ret)
		goto fail_parsing;

	cpufreq_update_table(temp, size, table);

fail_parsing:
	kfree(temp);
	return ret;
}

static void cpufreq_free(void *data)
{
	if (data)
		kfree(data);
}

static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

#define attr_cpufreq(type, name, table)						\
static ssize_t cpufreq_##name##_show(struct gov_attr_set *attr_set, char *buf)	\
{										\
	struct cpufreq_policy *policy = aigov_get_attr_policy(attr_set);	\
	struct cpufreq_##type *data = per_cpu(cpufreq_##type, policy->cpu);	\
	struct cpufreq_table *pos = data->table;				\
	int ret = 0;								\
										\
	for (; pos->frequency != CPUFREQ_TABLE_END; pos++)			\
		ret += sprintf(buf + ret, "%8d ratio:%3d \n",			\
					pos->frequency, pos->value);		\
										\
	return ret;								\
}										\
										\
static ssize_t cpufreq_##name##_store(struct gov_attr_set *attr_set,		\
				      const char *buf, size_t count)		\
{										\
	struct cpufreq_policy *policy = aigov_get_attr_policy(attr_set);	\
	struct cpufreq_##type *data = per_cpu(cpufreq_##type, policy->cpu);	\
	struct cpufreq_table *old_table = data->table;				\
	int *new_table = NULL;							\
	int ntokens;								\
										\
	new_table = get_tokenized_data(buf, &ntokens);				\
	if (IS_ERR(new_table))							\
		return PTR_RET(new_table);					\
										\
	cpufreq_update_table(new_table, ntokens, old_table);			\
	kfree(new_table);							\
										\
	return count;								\
}										\

int aigov_sysfs_add_attr(struct cpufreq_policy *policy, const struct attribute *attr);
struct cpufreq_policy *aigov_get_attr_policy(struct gov_attr_set *attr_set);

/**********************************************************************
 * cpufreq util boost                                                 *
 **********************************************************************/
struct cpufreq_pelt_boost {
	struct cpufreq_table *table;
	unsigned int ratio;
	unsigned long step_max_util;
};
DEFINE_PER_CPU(struct cpufreq_pelt_boost *, cpufreq_pelt_boost);

attr_cpufreq(pelt_boost, pelt_boost, table);
static struct governor_attr cpufreq_pelt_boost_attr = __ATTR_RW(cpufreq_pelt_boost);

unsigned long cpufreq_get_pelt_boost_util(int cpu, unsigned long util)
{
	struct cpufreq_pelt_boost *boost = per_cpu(cpufreq_pelt_boost, cpu);
	unsigned long boosted_util = 0;

	if (!boost || !boost->ratio)
		return util;

	if (util > boost->step_max_util) {
		trace_ems_freqvar_boost(cpu, boost->ratio, boost->step_max_util, util, 0);
		return util;
	}

	boosted_util = util * boost->ratio / 100;

	if (boost->step_max_util)
		boosted_util = min_t(unsigned long, boosted_util, boost->step_max_util);

	trace_ems_freqvar_boost(cpu, boost->ratio, boost->step_max_util, util, boosted_util);

	return boosted_util;
}

static void cpufreq_pelt_boost_free(struct cpufreq_pelt_boost *boost)
{
	if (boost)
		cpufreq_free(boost->table);

	cpufreq_free(boost);
}

static struct
cpufreq_pelt_boost *cpufreq_pelt_boost_alloc(struct cpufreq_policy *policy)
{
	struct cpufreq_pelt_boost *boost;
	int size;

	boost = kzalloc(sizeof(*boost), GFP_KERNEL);
	if (!boost)
		return NULL;

	size = cpufreq_get_table_size(policy);
	if (size <= 0)
		goto fail_alloc;

	boost->table = kzalloc(sizeof(struct cpufreq_table) * (size + 1), GFP_KERNEL);
	if (!boost->table)
		goto fail_alloc;

	return boost;

fail_alloc:
	cpufreq_pelt_boost_free(boost);
	return NULL;
}

/**********************************************************************
 * cpufreq up-scale ratio                                             *
 **********************************************************************/
struct cpufreq_tipping_point {
	struct cpufreq_table *table;
	int ratio;
};
DEFINE_PER_CPU(struct cpufreq_tipping_point *, cpufreq_tipping_point);

attr_cpufreq(tipping_point, tipping_point, table);
static struct governor_attr cpufreq_tipping_point_attr = __ATTR_RW(cpufreq_tipping_point);

unsigned int cpufreq_get_tipping_point(int cpu, unsigned int freq)
{
	struct cpufreq_tipping_point *tp = per_cpu(cpufreq_tipping_point, cpu);

	if (!tp)
		return freq + (freq >> 2);

	return (freq * (100 + tp->ratio)) / 100;
}

static void cpufreq_tipping_point_update(int cpu, int new_freq)
{
	struct cpufreq_tipping_point *tp;

	tp = per_cpu(cpufreq_tipping_point, cpu);
	if (!tp)
		return;

	tp->ratio = cpufreq_get_value(new_freq, tp->table);
}

static void cpufreq_tipping_point_free(struct cpufreq_tipping_point *tp)
{
	if (tp)
		cpufreq_free(tp->table);

	cpufreq_free(tp);
}

static struct
cpufreq_tipping_point *cpufreq_tipping_point_alloc(struct cpufreq_policy *policy)
{
	struct cpufreq_tipping_point *tp;
	int size;

	tp = kzalloc(sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return NULL;

	size = cpufreq_get_table_size(policy);
	if (size <= 0)
		goto fail_alloc;

	tp->table = kzalloc(sizeof(struct cpufreq_table) * (size + 1), GFP_KERNEL);
	if (!tp->table)
		goto fail_alloc;

	return tp;

fail_alloc:
	cpufreq_tipping_point_free(tp);
	return NULL;
}

static int cpufreq_tipping_point_init(struct device_node *dn, const struct cpumask *mask)
{
	struct cpufreq_tipping_point *tp;
	struct cpufreq_policy *policy;
	int cpu, ret = 0;

	policy = cpufreq_cpu_get(cpumask_first(mask));
	if (!policy)
		return -ENODEV;

	tp = cpufreq_tipping_point_alloc(policy);
	if (!tp) {
		ret = -ENOMEM;
		goto fail_init;
	}

	ret = cpufreq_fill_frequency_table(policy, tp->table);
	if (ret)
		goto fail_init;

	ret = cpufreq_parse_value_dt(dn, "tipping-point-scale", tp->table);
	if (ret)
		goto fail_init;

	for_each_cpu(cpu, mask)
		per_cpu(cpufreq_tipping_point, cpu) = tp;

	cpufreq_tipping_point_update(policy->cpu, policy->cur);

	ret = aigov_sysfs_add_attr(policy, &cpufreq_tipping_point_attr.attr);
	if (ret)
		goto fail_init;

	return 0;

fail_init:
	cpufreq_cpu_put(policy);
	cpufreq_tipping_point_free(tp);

	return ret;
}

static void cpufreq_pelt_boost_update(int cpu, int new_freq);
static int cpufreq_pelt_boost_init(struct device_node *dn, const struct cpumask *mask)
{
	struct cpufreq_pelt_boost *boost;
	struct cpufreq_policy *policy;
	int cpu, ret = 0;

	policy = cpufreq_cpu_get(cpumask_first(mask));
	if (!policy)
		return -ENODEV;

	boost = cpufreq_pelt_boost_alloc(policy);
	if (!boost) {
		ret = -ENOMEM;
		goto fail_init;
	}

	ret = cpufreq_fill_frequency_table(policy, boost->table);
	if (ret)
		goto fail_init;

	ret = cpufreq_parse_value_dt(dn, "util-boost-table", boost->table);
	if (ret)
		goto fail_init;

	for_each_cpu(cpu, mask)
		per_cpu(cpufreq_pelt_boost, cpu) = boost;

	cpufreq_pelt_boost_update(policy->cpu, policy->cur);

	ret = aigov_sysfs_add_attr(policy, &cpufreq_pelt_boost_attr.attr);
	if (ret)
		goto fail_init;

	return 0;

fail_init:
	cpufreq_cpu_put(policy);
	cpufreq_pelt_boost_free(boost);

	return ret;
}

/**********************************************************************
 * cpufreq rate limit                                                 *
 **********************************************************************/
struct cpufreq_rate_limit {
	struct cpufreq_table *up_rate_limit_table;
	struct cpufreq_table *down_rate_limit_table;
	int ratio;
};
DEFINE_PER_CPU(struct cpufreq_rate_limit *, cpufreq_rate_limit);

attr_cpufreq(rate_limit, up_rate_limit, up_rate_limit_table);
attr_cpufreq(rate_limit, down_rate_limit, down_rate_limit_table);
static struct governor_attr cpufreq_up_rate_limit = __ATTR_RW(cpufreq_up_rate_limit);
static struct governor_attr cpufreq_down_rate_limit = __ATTR_RW(cpufreq_down_rate_limit);

static void cpufreq_rate_limit_update(int cpu, int new_freq)
{
	struct cpufreq_rate_limit *rate_limit;
	int up_rate_limit, down_rate_limit;
	struct cpufreq_policy *policy;

	rate_limit = per_cpu(cpufreq_rate_limit, cpu);
	if (!rate_limit)
		return;

	up_rate_limit = cpufreq_get_value(new_freq, rate_limit->up_rate_limit_table);
	down_rate_limit = cpufreq_get_value(new_freq, rate_limit->down_rate_limit_table);

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;

	aigov_update_rate_limit_us(policy, up_rate_limit, down_rate_limit);

	cpufreq_cpu_put(policy);
}

static void cpufreq_rate_limit_free(struct cpufreq_rate_limit *rate_limit)
{
	if (rate_limit) {
		cpufreq_free(rate_limit->up_rate_limit_table);
		cpufreq_free(rate_limit->down_rate_limit_table);
	}

	cpufreq_free(rate_limit);
}

static struct
cpufreq_rate_limit *cpufreq_rate_limit_alloc(struct cpufreq_policy *policy)
{
	struct cpufreq_rate_limit *rate_limit;
	int size;

	rate_limit = kzalloc(sizeof(*rate_limit), GFP_KERNEL);
	if (!rate_limit)
		return NULL;

	size = cpufreq_get_table_size(policy);
	if (size <= 0)
		goto fail_alloc;

	rate_limit->up_rate_limit_table = kzalloc(sizeof(struct cpufreq_table)
					* (size + 1), GFP_KERNEL);
	if (!rate_limit->up_rate_limit_table)
		goto fail_alloc;

	rate_limit->down_rate_limit_table = kzalloc(sizeof(struct cpufreq_table)
					* (size + 1), GFP_KERNEL);
	if (!rate_limit->down_rate_limit_table)
		goto fail_alloc;

	return rate_limit;

fail_alloc:
	cpufreq_rate_limit_free(rate_limit);
	return NULL;
}

static int cpufreq_rate_limit_init(struct device_node *dn, const struct cpumask *mask)
{
	struct cpufreq_rate_limit *rate_limit;
	struct cpufreq_policy *policy;
	int cpu, ret = 0;

	policy = cpufreq_cpu_get(cpumask_first(mask));
	if (!policy)
		return -ENODEV;

	rate_limit = cpufreq_rate_limit_alloc(policy);
	if (!rate_limit) {
		ret = -ENOMEM;
		goto fail_init;
	}

	ret = cpufreq_fill_frequency_table(policy, rate_limit->up_rate_limit_table);
	if (ret)
		goto fail_init;

	ret = cpufreq_fill_frequency_table(policy, rate_limit->down_rate_limit_table);
	if (ret)
		goto fail_init;

	ret = cpufreq_parse_value_dt(dn, "up-rate-limit-table", rate_limit->up_rate_limit_table);
	if (ret)
		goto fail_init;

	ret = cpufreq_parse_value_dt(dn, "down-rate-limit-table", rate_limit->down_rate_limit_table);
	if (ret)
		goto fail_init;

	ret = aigov_sysfs_add_attr(policy, &cpufreq_up_rate_limit.attr);
	if (ret)
		goto fail_init;

	ret = aigov_sysfs_add_attr(policy, &cpufreq_down_rate_limit.attr);
	if (ret)
		goto fail_init;

	for_each_cpu(cpu, mask)
		per_cpu(cpufreq_rate_limit, cpu) = rate_limit;

	cpufreq_rate_limit_update(policy->cpu, policy->cur);

	return 0;

fail_init:
	cpufreq_rate_limit_free(rate_limit);
	cpufreq_cpu_put(policy);

	return ret;
}

static void cpufreq_pelt_boost_update(int cpu, int new_freq)
{
	struct cpufreq_pelt_boost *boost;

	boost = per_cpu(cpufreq_pelt_boost, cpu);
	if (!boost)
		return;

	boost->ratio = cpufreq_get_value(new_freq, boost->table);
	boost->step_max_util = et_get_freq_cap(cpu, new_freq) * boost->ratio / 100;
}

/**********************************************************************
 * cpufreq notifier callback                                          *
 **********************************************************************/
static int ems_cpufreq_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	cpufreq_pelt_boost_update(freq->cpu, freq->new);
	cpufreq_rate_limit_update(freq->cpu, freq->new);
	cpufreq_tipping_point_update(freq->cpu, freq->new);

	return 0;
}

static struct notifier_block ems_cpufreq_notifier = {
	.notifier_call  = ems_cpufreq_callback,
};

/**********************************************************************
 * initialization                                                     *
 **********************************************************************/
static int __init cpufreq_init(void)
{
	struct device_node *dn = NULL;
	struct cpumask shared_mask;
	const char *buf;

	while ((dn = of_find_node_by_type(dn, "ems-cpufreq"))) {
		/*
		 * shared-cpus includes cpus scaling at the sametime.
		 * it is called "sibling cpus" in the CPUFreq and
		 * masked on the realated_cpus of the policy
		 */
		if (of_property_read_string(dn, "shared-cpus", &buf))
			continue;

		cpumask_clear(&shared_mask);
		cpulist_parse(buf, &shared_mask);
		cpumask_and(&shared_mask, &shared_mask, cpu_possible_mask);
		if (cpumask_weight(&shared_mask) == 0)
			continue;

		cpufreq_pelt_boost_init(dn, &shared_mask);
		cpufreq_rate_limit_init(dn, &shared_mask);
		cpufreq_tipping_point_init(dn, &shared_mask);
	}

	cpufreq_register_notifier(&ems_cpufreq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);

	return 0;
}
late_initcall(cpufreq_init);
