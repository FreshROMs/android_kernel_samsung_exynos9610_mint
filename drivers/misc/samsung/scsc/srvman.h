/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _SRVMAN_H
#define _SRVMAN_H

#ifdef CONFIG_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#endif

#include "scsc/scsc_mx.h"

struct srvman;

enum error_status {
	ALLOWED_START_STOP,
	NOT_ALLOWED_START_STOP,
	NOT_ALLOWED_START,
};


void srvman_init(struct srvman *srvman, struct scsc_mx *mx);
int  srvman_suspend_services(struct srvman *srvman);
int  srvman_resume_services(struct srvman *srvman);
void srvman_freeze_services(struct srvman *srvman, struct mx_syserr_decode *syserr);
void srvman_freeze_sub_system(struct srvman *srvman, struct mx_syserr_decode *syserr);
void srvman_unfreeze_services(struct srvman *srvman, struct mx_syserr_decode *syserr);
void srvman_unfreeze_sub_system(struct srvman *srvman, struct mx_syserr_decode *syserr);
u8 srvman_notify_services(struct srvman *srvman, struct mx_syserr_decode *syserr);
u8 srvman_notify_sub_system(struct srvman *srvman, struct mx_syserr_decode *syserr);
void srvman_set_error_complete(struct srvman *srvman, enum error_status s);
void srvman_set_error(struct srvman *srvman, enum error_status s);
bool srvman_in_error_safe(struct srvman *srvman);
bool srvman_in_error(struct srvman *srvman);
void srvman_clear_error(struct srvman *srvman);
bool srvman_allow_close(struct srvman *srvman);
void srvman_deinit(struct srvman *srvman);

struct srvman {
	struct scsc_mx   *mx;
	struct list_head service_list;
	struct mutex     service_list_mutex;
	struct mutex     api_access_mutex;
	struct mutex     error_state_mutex;
	enum error_status error;
#ifdef CONFIG_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock sm_wake_lock;
#else
	struct wake_lock sm_wake_lock;
#endif
#endif
};


#endif
