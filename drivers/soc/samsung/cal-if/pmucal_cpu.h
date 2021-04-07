#ifndef __PMUCAL_CPU_H__
#define __PMUCAL_CPU_H__
#include "pmucal_common.h"

struct pmucal_cpu {
	u32 id;
	struct pmucal_seq *on;
	struct pmucal_seq *off;
	struct pmucal_seq *release;
	struct pmucal_seq *status;
	u32 num_on;
	u32 num_off;
	u32 num_release;
	u32 num_status;
};

/* APIs to be supported to PWRCAL interface */
extern int pmucal_cpu_enable(unsigned int cpu);
extern int pmucal_cpu_disable(unsigned int cpu);
extern int pmucal_cpu_is_enabled(unsigned int cpu);
extern int pmucal_cpu_cluster_enable(unsigned int cluster);
extern int pmucal_cpu_cluster_disable(unsigned int cluster);
extern int pmucal_cpu_cluster_is_enabled(unsigned int cluster);
extern int pmucal_cpu_init(void);

extern struct pmucal_cpu pmucal_cpu_list[];
extern unsigned int pmucal_cpu_list_size;
extern struct pmucal_cpu pmucal_cluster_list[];
extern unsigned int pmucal_cluster_list_size;
#ifdef CONFIG_FLEXPMU
extern unsigned int cpu_inform_c2;
extern unsigned int cpu_inform_cpd;
#endif
#endif
