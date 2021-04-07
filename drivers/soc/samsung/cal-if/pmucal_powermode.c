#include "pwrcal-env.h"
#include "pmucal_system.h"
#include "pmucal_powermode.h"
#include "pmucal_rae.h"

#define read_mpidr() ({							\
	u64 __val;							\
	asm("mrs	%0, mpidr_el1" : "=r" (__val));			\
	__val;								\
})

static inline unsigned int linear_phycpu(unsigned int mpidr)
{
	unsigned int lvl = 0;

	lvl = (mpidr & MPIDR_MT_BITMASK) ? 1 : 0;
	return ((MPIDR_AFFINITY_LEVEL(mpidr, (1 + lvl)) << 2)
			| MPIDR_AFFINITY_LEVEL(mpidr, lvl));
}

void pmucal_powermode_hint(unsigned int mode)
{
	unsigned int mpidr = read_mpidr();
	unsigned int phycpu = linear_phycpu(mpidr);

	__raw_writel(mode, pmucal_cpuinform_list[phycpu].base_va
			+ pmucal_cpuinform_list[phycpu].offset);
}

void pmucal_powermode_hint_clear(void)
{
	unsigned int mpidr = read_mpidr();
	unsigned int phycpu = linear_phycpu(mpidr);

	__raw_writel(0, pmucal_cpuinform_list[phycpu].base_va
			+ pmucal_cpuinform_list[phycpu].offset);
}

int __init pmucal_cpuinform_init(void)
{
	int i, j;

	for (i = 0; i < cpu_inform_list_size; i++) {
		for (j = 0; j < pmucal_p2v_list_size; j++)
			if (pmucal_p2v_list[j].pa == (phys_addr_t)pmucal_cpuinform_list[i].base_pa)
				break;

		if (j != pmucal_p2v_list_size) {
			pmucal_cpuinform_list[i].base_va = pmucal_p2v_list[j].va;
		} else {
			pr_err("%s %s: there is no such PA in p2v_list (idx:%d)\n",
					PMUCAL_PREFIX, __func__, i);
			return -ENOENT;
		}

	}

	return 0;
}
