#include <linux/cgroup.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/ems.h>

#include <trace/events/sched.h>

#include "sched.h"
#include "tune.h"

bool schedtune_initialized = false;
struct reciprocal_value schedtune_spc_rdiv;

#ifdef CONFIG_SCHED_EMS
static int sysbusy_state = SYSBUSY_STATE0;
#endif

/*
 * SchedTune root control group
 * The root control group is used to defined a system-wide boosting tuning,
 * which is applied to all tasks in the system.
 * Task specific boost tuning could be specified by creating and
 * configuring a child control group under the root one.
 * By default, system-wide boosting is disabled, i.e. no boosting is applied
 * to tasks which are not into a child control group.
 */
static struct schedtune
root_schedtune = {
	.boost	= 0,
#ifndef CONFIG_UCLAMP_TASK_GROUP
	.prefer_idle = 0,
#endif
};

/* Array of configured boostgroups */
static struct schedtune *allocated_group[BOOSTGROUPS_COUNT] = {
	&root_schedtune,
	NULL,
};

/* SchedTune boost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	bool idle;
	int boost_max;
	u64 boost_ts;
	struct {
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* Timestamp of boost activation */
		u64 ts;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

static inline bool schedtune_boost_timeout(u64 now, u64 ts)
{
	return ((now - ts) > SCHEDTUNE_BOOST_HOLD_NS);
}

static inline bool
schedtune_boost_group_active(int idx, struct boost_groups* bg, u64 now)
{
	if (bg->group[idx].tasks)
		return true;

	return !schedtune_boost_timeout(now, bg->group[idx].ts);
}

static void
schedtune_cpu_update(int cpu, u64 now)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int boost_max;
	u64 boost_ts;
	int idx;

	/* The root boost group is always active */
	boost_max = bg->group[0].boost;
	boost_ts = now;
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {
		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU or it has hold
		 * in effect from a previous task.
		 */
		if (!schedtune_boost_group_active(idx, bg, now))
			continue;

		/* This boost group is active */
		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
		boost_ts =  bg->group[idx].ts;
	}
	/* Ensures boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.*/
	boost_max = max(boost_max, 0);
	bg->boost_max = boost_max;
	bg->boost_ts = boost_ts;
}

static int
#ifdef CONFIG_SCHED_EMS
schedtune_boostgroup_update(struct schedtune *st, bool reset)
#else
schedtune_boostgroup_update(int idx, int boost)
#endif
{
	struct boost_groups *bg;
	int cur_boost_max;
	int old_boost;
	int cpu;
	u64 now;
#ifdef CONFIG_SCHED_EMS
	int idx = st->idx;
	int boost = 0;

	if (likely(!reset)) {
		switch (sysbusy_state) {
		case SYSBUSY_STATE1:
			boost = st->heavy_boost;
			break;
		case SYSBUSY_STATE2:
		case SYSBUSY_STATE3:
			boost = st->busy_boost;
			break;
		default:
			boost = st->boost;
			break;
		}
	}
#endif

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

		/*
		 * Keep track of current boost values to compute the per CPU
		 * maximum only when it has been affected by the new value of
		 * the updated boost group
		 */
		cur_boost_max = bg->boost_max;
		old_boost = bg->group[idx].boost;

		/* Update the boost value of this boost group */
		bg->group[idx].boost = boost;

		/* Check if this update increase current max */
		now = sched_clock_cpu(cpu);
		if (boost > cur_boost_max &&
			schedtune_boost_group_active(idx, bg, now)) {
			bg->boost_max = boost;
			bg->boost_ts = bg->group[idx].ts;

			trace_sched_tune_boostgroup_update(cpu, 1, bg->boost_max);
			continue;
		}

		/* Check if this update has decreased current max */
		if (cur_boost_max == old_boost && old_boost > boost) {
			schedtune_cpu_update(cpu, now);
			trace_sched_tune_boostgroup_update(cpu, -1, bg->boost_max);
			continue;
		}

		trace_sched_tune_boostgroup_update(cpu, 0, bg->boost_max);
	}

	return 0;
}

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

static inline bool
schedtune_update_timestamp(struct task_struct *p)
{
	if (sched_feat(SCHEDTUNE_BOOST_HOLD_ALL))
		return true;

	return task_has_rt_policy(p);
}

static inline void
schedtune_tasks_update(struct task_struct *p, int cpu, int idx, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	/* Update timeout on enqueue */
	if (task_count > 0) {
		u64 now = sched_clock_cpu(cpu);

		if (schedtune_update_timestamp(p))
			bg->group[idx].ts = now;

		/* Boost group activation or deactivation on that RQ */
		if (bg->group[idx].tasks == 1)
			schedtune_cpu_update(cpu, now);
	}

	trace_sched_tune_tasks_update(p, cpu, tasks, idx,
			bg->group[idx].boost, bg->boost_max,
			bg->group[idx].ts);
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_enqueue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (unlikely(!schedtune_initialized))
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, ENQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int schedtune_can_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct boost_groups *bg;
	struct rq_flags rq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;
	u64 now;

	if (unlikely(!schedtune_initialized))
		return 0;


	cgroup_taskset_for_each(task, css, tset) {

		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = task_rq_lock(task, &rq_flags);

		if (!task->on_rq) {
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * Boost group accouting is protected by a per-cpu lock and requires
		 * interrupt to be disabled to avoid race conditions on...
		 */
		cpu = cpu_of(rq);
		bg = &per_cpu(cpu_boost_groups, cpu);
		raw_spin_lock(&bg->lock);

		dst_bg = css_st(css)->idx;
		src_bg = task_schedtune(task)->idx;

		/*
		 * Current task is not changing boostgroup, which can
		 * happen when the new hierarchy is in use.
		 */
		if (unlikely(dst_bg == src_bg)) {
			raw_spin_unlock(&bg->lock);
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * This is the case of a RUNNABLE task which is switching its
		 * current boost group.
		 */

		/* Move task from src to dst boost group */
		tasks = bg->group[src_bg].tasks - 1;
		bg->group[src_bg].tasks = max(0, tasks);
		bg->group[dst_bg].tasks += 1;

		/* Update boost hold start for this group */
		now = sched_clock_cpu(cpu);
		bg->group[dst_bg].ts = now;

		/* Force boost group re-evaluation at next boost check */
		bg->boost_ts = now - SCHEDTUNE_BOOST_HOLD_NS;

		raw_spin_unlock(&bg->lock);
		task_rq_unlock(rq, task, &rq_flags);
	}

	return 0;
}

void schedtune_cancel_attach(struct cgroup_taskset *tset)
{
	/* This can happen only if SchedTune controller is mounted with
	 * other hierarchies ane one of them fails. Since usually SchedTune is
	 * mouted on its own hierarcy, for the time being we do not implement
	 * a proper rollback mechanism */
	WARN(1, "SchedTune cancel attach not implemented");
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_dequeue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (unlikely(!schedtune_initialized))
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions on...
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, DEQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int schedtune_cpu_boost(int cpu)
{
	struct boost_groups *bg;
	u64 now;

	bg = &per_cpu(cpu_boost_groups, cpu);
	now = sched_clock_cpu(cpu);

	/* Check to see if we have a hold in effect */
	if (schedtune_boost_timeout(now, bg->boost_ts))
		schedtune_cpu_update(cpu, now);

	return bg->boost_max;
}

static long
schedtune_margin(unsigned long capacity, unsigned long signal, long boost)
{
	long long margin = 0;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (CAPACITY - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		margin  = capacity - signal;
		margin *= boost;
	} else
		margin = -signal * boost;

	margin  = reciprocal_divide(margin, schedtune_spc_rdiv);

	if (boost < 0)
		margin *= -1;
	return margin;
}

inline int
schedtune_cpu_margin(unsigned long util, int cpu)
{
	int boost = schedtune_cpu_boost(cpu);
	unsigned long capacity;

	if (boost == 0)
		return 0;

	capacity = capacity_orig_of(cpu);

	return schedtune_margin(capacity, util, boost);
}

extern unsigned long task_util_est(struct task_struct *p);
inline long
schedtune_task_margin(struct task_struct *task)
{
	int boost = schedtune_task_boost(task);
	unsigned long util, capacity;

	if (boost == 0)
		return 0;

	capacity = capacity_orig_of(task_cpu(task));
	util = task_util_est(task);
	return schedtune_margin(capacity, util, boost);
}

#ifdef CONFIG_SCHED_EMS
int schedtune_task_group_idx(struct task_struct *p)
{
	struct schedtune *st;
	int group_idx;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get task cgroup idx */
	rcu_read_lock();
	st = task_schedtune(p);
	group_idx = st->idx;
	rcu_read_unlock();

	/* if group idx goes beyond allowed, return root */
	if (group_idx >= CGROUP_COUNT)
		group_idx = 0;

	return group_idx;
}
#endif

int schedtune_task_boost(struct task_struct *p)
{
	struct schedtune *st;
	int task_boost;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get task boost value */
	rcu_read_lock();
	st = task_schedtune(p);
	task_boost = st->boost;
	rcu_read_unlock();

	return task_boost;
}

#ifndef CONFIG_UCLAMP_TASK_GROUP
int schedtune_prefer_idle(struct task_struct *p)
{
	struct schedtune *st;
	int prefer_idle;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get prefer_idle value */
	rcu_read_lock();
	st = task_schedtune(p);
	prefer_idle = st->prefer_idle;
	rcu_read_unlock();

	return prefer_idle;
}
#endif

#ifndef CONFIG_UCLAMP_TASK_GROUP
static u64
prefer_idle_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->prefer_idle;
}

static int
prefer_idle_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 prefer_idle)
{
	struct schedtune *st = css_st(css);
	st->prefer_idle = !!prefer_idle;

	return 0;
}
#endif

static s64
boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->boost;
}

static int
boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 boost)
{
	struct schedtune *st = css_st(css);

	if (boost < 0 || boost > 100)
		return -EINVAL;

	st->boost = boost;

	/* Update CPU boost */
#ifdef CONFIG_SCHED_EMS
	schedtune_boostgroup_update(st, false);
#else
	schedtune_boostgroup_update(st->idx, st->boost);
#endif

	return 0;
}

#ifdef CONFIG_SCHED_EMS
static s64
heavy_boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->heavy_boost;
}

static int
heavy_boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 boost)
{
	struct schedtune *st = css_st(css);

	if (boost < 0 || boost > 100)
		return -EINVAL;

	st->heavy_boost = boost;

	/* Update CPU boost */
	schedtune_boostgroup_update(st, false);

	return 0;
}

static s64
busy_boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->busy_boost;
}

static int
busy_boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 boost)
{
	struct schedtune *st = css_st(css);

	if (boost < 0 || boost > 100)
		return -EINVAL;

	st->busy_boost = boost;

	/* Update CPU boost */
	schedtune_boostgroup_update(st, false);

	return 0;
}

/* stune api */
extern u64 ems_tex_enabled_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_tex_enabled_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled);
extern int ems_tex_pinning_cpus_stune_hook_read(struct seq_file *sf, void *v);
extern ssize_t ems_tex_pinning_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off);
extern u64 ems_tex_prio_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_tex_prio_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 prio);
extern u64 ems_tex_suppress_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_tex_suppress_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 prio);
extern int ems_sched_policy_stune_hook_read(struct seq_file *sf, void *v);
extern int ems_sched_policy_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 policy);
extern u64 ems_ontime_enabled_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_ontime_enabled_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled);
extern u64 ems_ntu_ratio_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_ntu_ratio_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 ratio);
extern u64 ems_global_task_boost_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_global_task_boost_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled);
extern s64 ems_gpu_boost_freq_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_gpu_boost_freq_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, s64 freq);
extern int ems_prefer_cpus_stune_hook_read(struct seq_file *sf, void *v);
extern ssize_t ems_prefer_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off);
extern u64 ems_small_task_threshold_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft);
extern int ems_small_task_threshold_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 threshold);
extern int ems_small_task_cpus_stune_hook_read(struct seq_file *sf, void *v);
extern ssize_t ems_small_task_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off);
#endif

static struct cftype files[] = {
#ifdef CONFIG_SCHED_EMS
	{
		.name = "freqboost",
		.read_s64 = boost_read,
		.write_s64 = boost_write,
	},
#else
	{
		.name = "boost",
		.read_s64 = boost_read,
		.write_s64 = boost_write,
	},
#endif
#ifndef CONFIG_UCLAMP_TASK_GROUP
	{
		.name = "prefer_idle",
		.read_u64 = prefer_idle_read,
		.write_u64 = prefer_idle_write,
	},
#endif
#ifdef CONFIG_SCHED_EMS
	{
		.name = "heavyboost",
		.read_s64 = heavy_boost_read,
		.write_s64 = heavy_boost_write,
	},
	{
		.name = "busyboost",
		.read_s64 = busy_boost_read,
		.write_s64 = busy_boost_write,
	},
	{
		.name = "sched_policy",
		.seq_show = ems_sched_policy_stune_hook_read,
		.write_u64 = ems_sched_policy_stune_hook_write,
	},
	{
		.name = "ontime_enabled",
		.read_u64 = ems_ontime_enabled_stune_hook_read,
		.write_u64 = ems_ontime_enabled_stune_hook_write,
	},
	{
		.name = "global_boost_enabled",
		.read_u64 = ems_global_task_boost_stune_hook_read,
		.write_u64 = ems_global_task_boost_stune_hook_write,
	},
	{
		.name = "gpu_boost_freq",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_s64 = ems_gpu_boost_freq_stune_hook_read,
		.write_s64 = ems_gpu_boost_freq_stune_hook_write,
	},
	{
		.name = "ntu_ratio",
		.read_u64 = ems_ntu_ratio_stune_hook_read,
		.write_u64 = ems_ntu_ratio_stune_hook_write,
	},
	{
		.name = "tex_enabled",
		.read_u64 = ems_tex_enabled_stune_hook_read,
		.write_u64 = ems_tex_enabled_stune_hook_write,
	},
	{
		.name = "tex_pinning_cpus",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = ems_tex_pinning_cpus_stune_hook_read,
		.write = ems_tex_pinning_cpus_stune_hook_write,
	},
	{
		.name = "tex_prio",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_u64 = ems_tex_prio_stune_hook_read,
		.write_u64 = ems_tex_prio_stune_hook_write,
	},
	{
		.name = "tex_suppress",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_u64 = ems_tex_suppress_stune_hook_read,
		.write_u64 = ems_tex_suppress_stune_hook_write,
	},
	{
		.name = "prefer_cpus",
		.seq_show = ems_prefer_cpus_stune_hook_read,
		.write = ems_prefer_cpus_stune_hook_write,
	},
	{
		.name = "small_task_cpus",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = ems_small_task_cpus_stune_hook_read,
		.write = ems_small_task_cpus_stune_hook_write,
	},
	{
		.name = "small_task_threshold",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_u64 = ems_small_task_threshold_stune_hook_read,
		.write_u64 = ems_small_task_threshold_stune_hook_write,
	},
#endif
	{ }	/* terminate */
};

static int
schedtune_boostgroup_init(struct schedtune *st)
{
	struct boost_groups *bg;
	int cpu;

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = st;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		bg->group[st->idx].boost = 0;
		bg->group[st->idx].tasks = 0;
		bg->group[st->idx].ts = 0;
	}

	return 0;
}

#ifdef CONFIG_SCHED_EMS
struct stune_param {
	char *name;
	s64 boost;

	u64 ems_heavy_boost;
	u64 ems_busy_boost;

	u64 ems_sched_policy;
	u64 ems_ontime_enabled;
	u64 ems_ntu_ratio;
	u64 ems_tex_enabled;
	u64 ems_global_task_boost;
};

static void
schedtune_set_default_values(struct cgroup_subsys_state *css)
{
	int i;
	static struct stune_param tgts[] = {
		/* cgroup            l-b h-b b-b e-p e-o  e-n e-t e-g */
		{"top-app",	           1,  3,  5,  0,  1,  25,  1,  1 },
		{"foreground",	       0,  1,  3,  0,  1,  25,  0,  0 },
		{"background",	       0,  0,  1,  1,  0,   5,  0,  0 },
		{"rt",		           3,  5,  7,  2,  0,   5,  0,  0 },
		{"camera-daemon",	   3,  7,  7,  3,  0,  25,  0,  0 },
		{"nnapi-hal",	       3,  5,  7,  3,  0,  25,  0,  0 },
		{"hot",	               0,  0,  0,  0,  0,   5,  0,  0 },
	};

	for (i = 0; i < ARRAY_SIZE(tgts); i++) {
		struct stune_param tgt = tgts[i];

		if (!strcmp(css->cgroup->kn->name, tgt.name)) {
			pr_info("stune_assist: setting values for %s: boost=%d heavy_boost=%d busy_boost=%d ems_sched_policy=%d ems_ontime_enabled=%d ems_ntu_ratio=%d ems_tex_enabled=%d ems_global_task_boost=%d\n",
				tgt.name, tgt.boost, tgt.ems_heavy_boost, tgt.ems_busy_boost, tgt.ems_sched_policy, tgt.ems_ontime_enabled,
				tgt.ems_ntu_ratio, tgt.ems_tex_enabled, tgt.ems_global_task_boost);

			boost_write(css, NULL, tgt.boost);
			heavy_boost_write(css, NULL, tgt.ems_heavy_boost);
			busy_boost_write(css, NULL, tgt.ems_busy_boost);

			ems_sched_policy_stune_hook_write(css, NULL, tgt.ems_sched_policy);
			ems_ontime_enabled_stune_hook_write(css, NULL, tgt.ems_ontime_enabled);
			ems_ntu_ratio_stune_hook_write(css, NULL, tgt.ems_ntu_ratio);
			ems_tex_enabled_stune_hook_write(css, NULL, tgt.ems_tex_enabled);
			ems_global_task_boost_stune_hook_write(css, NULL, tgt.ems_global_task_boost);
		}
	}
}

/****************************************************************/
/*		  sysbusy state change notifier			*/
/****************************************************************/
static int schedtune_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;
	int i;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	sysbusy_state = state;

	for (i = 0; i < BOOSTGROUPS_COUNT; i++) {
		struct schedtune *st = allocated_group[i];
		if (unlikely(!st))
			continue;

		schedtune_boostgroup_update(st, false);
	}

	return NOTIFY_OK;
}

static struct notifier_block schedtune_sysbusy_notifier = {
	.notifier_call = schedtune_sysbusy_notifier_call,
};
#endif

static struct cgroup_subsys_state *
schedtune_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct schedtune *st;
	int idx;

	if (!parent_css)
		return &root_schedtune.css;

	/* Allow only single level hierachies */
	if (parent_css != &root_schedtune.css) {
		pr_err("Nested SchedTune boosting groups not allowed\n");
		return ERR_PTR(-ENOMEM);
	}

	/* Allow only a limited number of boosting groups */
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {
		if (!allocated_group[idx])
			break;

		schedtune_set_default_values(&allocated_group[idx]->css);
	}

	if (idx == BOOSTGROUPS_COUNT) {
		pr_err("Trying to create more than %d SchedTune boosting groups\n",
		       BOOSTGROUPS_COUNT);
		return ERR_PTR(-ENOSPC);
	}

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		goto out;

	/* Initialize per CPUs boost group support */
	st->idx = idx;

	if (schedtune_boostgroup_init(st))
		goto release;

	return &st->css;

release:
	kfree(st);
out:
	return ERR_PTR(-ENOMEM);
}

static void
schedtune_boostgroup_release(struct schedtune *st)
{
	/* Reset this boost group */
#ifdef CONFIG_SCHED_EMS
	schedtune_boostgroup_update(st, true);
#else
	schedtune_boostgroup_update(st->idx, 0);
#endif

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = NULL;
}

static void
schedtune_css_free(struct cgroup_subsys_state *css)
{
	struct schedtune *st = css_st(css);

	schedtune_boostgroup_release(st);
	kfree(st);
}

struct cgroup_subsys schedtune_cgrp_subsys = {
	.css_alloc	= schedtune_css_alloc,
	.css_free	= schedtune_css_free,
	.can_attach     = schedtune_can_attach,
	.cancel_attach  = schedtune_cancel_attach,
	.legacy_cftypes	= files,
	.early_init	= 1,
};

static inline void
schedtune_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));
		raw_spin_lock_init(&bg->lock);
	}

	pr_info("schedtune: configured to support %d boost groups\n",
		BOOSTGROUPS_COUNT);

	schedtune_initialized = true;
}

/*
 * Initialize the cgroup structures
 */
static int
schedtune_init(void)
{
	schedtune_spc_rdiv = reciprocal_value(100);
	schedtune_init_cgroups();

	sysbusy_register_notifier(&schedtune_sysbusy_notifier);

	return 0;
}
postcore_initcall(schedtune_init);
