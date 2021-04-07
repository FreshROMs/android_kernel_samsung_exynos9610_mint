/****************************************************************************
 *
 *   Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#ifndef __SCSC_LOGRING_COMMON_H__
#define __SCSC_LOGRING_COMMON_H__

enum scsc_log_level {
	SCSC_MIN_DBG = 0,
	SCSC_EMERG = SCSC_MIN_DBG,
	SCSC_ALERT,
	SCSC_CRIT,
	SCSC_ERR,
	SCSC_WARNING,
	SCSC_NOTICE,
	SCSC_INFO,
	SCSC_DEBUG,
	SCSC_DBG1 = SCSC_DEBUG,		/* 7 */
	SCSC_DBG2,
	SCSC_DBG3,
	SCSC_DBG4,			/* 10 */
	SCSC_FULL_DEBUG
};

#define SCSC_SOH			0x01
#define DEFAULT_DBGLEVEL		SCSC_INFO	/* KERN_INFO */
#define DEFAULT_DROPLEVEL		SCSC_FULL_DEBUG	/* DBG4 + 1 */
#define DEFAULT_ALL_DISABLED		-1
#define DEFAULT_DROP_ALL		0
#define DEFAULT_REDIRECT_DROPLVL	SCSC_DEBUG
#define DEFAULT_NO_REDIRECT		0
#define DEFAULT_TBUF_SZ		     4096

/**
 * Nested macros needed to force expansion of 'defval'
 * before stringification takes place. Allows for ONE level
 * of indirection specifying params.
 */
#define SCSC_MODPARAM_DESC(kparam, descr, eff, defval) \
	__SCSC_MODPARAM_DESC(kparam, descr, eff, defval)

#define __SCSC_MODPARAM_DESC(kparam, descr, eff, defval) \
	MODULE_PARM_DESC(kparam, " "descr " Effective @"eff " default=" # defval ".")


#endif /* __SCSC_LOGRING_COMMON_H__ */
