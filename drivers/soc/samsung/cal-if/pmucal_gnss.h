#ifndef __PMUCAL_GNSS_H__
#define __PMUCAL_GNSS_H__
#include "pmucal_common.h"

struct pmucal_gnss {
	struct pmucal_seq *init;
	struct pmucal_seq *status;
	struct pmucal_seq *reset_assert;
	struct pmucal_seq *reset_release;
	struct pmucal_seq *gnss_reset_req_clear;
	u32 num_init;
	u32 num_status;
	u32 num_reset_assert;
	u32 num_reset_release;
	u32 num_gnss_reset_req_clear;
};

/* APIs to be supported to PWRCAL interface */
extern int pmucal_gnss_initialize(void);

extern int pmucal_gnss_init(void);
extern int pmucal_gnss_status(void);
extern int pmucal_gnss_reset_assert(void);
extern int pmucal_gnss_reset_release(void);
extern int pmucal_gnss_reset_req_clear(void);

extern struct pmucal_gnss pmucal_gnss_list;
extern unsigned int pmucal_gnss_list_size;
#endif
