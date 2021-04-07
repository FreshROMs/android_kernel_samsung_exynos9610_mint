#ifndef __PMUCAL_SHUB_H__
#define __PMUCAL_SHUB_H__
#include "pmucal_common.h"

struct pmucal_shub {
	struct pmucal_seq *init;
	struct pmucal_seq *status;
	struct pmucal_seq *reset_assert;
	struct pmucal_seq *reset_release_config;
	struct pmucal_seq *reset_release;
	u32 num_init;
	u32 num_status;
	u32 num_reset_assert;
	u32 num_reset_release_config;
	u32 num_reset_release;
};

/* APIs to be supported to PWRCAL interface */
extern int pmucal_shub_initialize(void);
extern int pmucal_shub_init(void);
extern int pmucal_shub_status(void);
extern int pmucal_shub_reset_assert(void);
extern int pmucal_shub_reset_release_config(void);
extern int pmucal_shub_reset_release(void);
extern struct pmucal_shub pmucal_shub_list;
extern unsigned int pmucal_shub_list_size;
#endif
