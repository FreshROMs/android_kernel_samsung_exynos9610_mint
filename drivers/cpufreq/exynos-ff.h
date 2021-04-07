#ifndef	__EXYNOS_FF_H__
#define	__EXYNOS_FF_H__

struct exynos_ff_driver {
	bool			big_dvfs_done;

	unsigned int		boost_threshold;
	unsigned int		cal_id;

	struct mutex		lock;
	struct cpumask		cpus;
};

static bool hwi_dvfs_req;
static atomic_t boost_throttling = ATOMIC_INIT(0);

#endif
