/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SCSC_PETERSON_H
#define __SCSC_PETERSON_H

#include <linux/delay.h>
#include "mif_reg.h"

#define R4_PROCESS      0
#define AP_PROCESS      1
#define DELAY_NS        100     /* delay in ns*/

static inline void peterson_mutex_init(struct peterson_mutex *p_mutex)
{
	if (!p_mutex) {
		pr_info("Mutex not declared\n");
		return;
	}

	p_mutex->flag[0] = false;
	p_mutex->flag[1] = false;
	p_mutex->turn = 0;
}

static inline void peterson_mutex_lock(struct peterson_mutex *p_mutex, unsigned int process)
{
	unsigned int other = 1 - process;

	p_mutex->flag[process] = true;
	/* write barrier */
	smp_wmb();
	p_mutex->turn = other;
	/* write barrier */
	smp_wmb();

	while ((p_mutex->flag[other]) && (p_mutex->turn == other))
		ndelay(DELAY_NS);
}

static inline void peterson_mutex_unlock(struct peterson_mutex *p_mutex, unsigned int process)
{
	p_mutex->flag[process] = false;
	/* write barrier */
	smp_wmb();
}
#endif
