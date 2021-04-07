/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_LIB_H
#define __SLSI_KIC_LIB_H

#ifdef CONFIG_SLSI_KIC_API_ENABLED
#include <scsc/kic/slsi_kic.h>
#endif

#include <scsc/kic/slsi_kic_prim.h>

/**
 * Library functions for sending information to kernel KIC, which will process
 * the event and take appropriate action, i.e. forward to relevant user
 * processes etc.
 */
#ifdef CONFIG_SLSI_KIC_API_ENABLED

static inline void slsi_kic_system_event(enum slsi_kic_system_event_category event_cat,
					 enum slsi_kic_system_events event, gfp_t flags)
{
	(void)slsi_kic_system_event_ind(event_cat, event, flags);
}


static inline void slsi_kic_service_information(enum slsi_kic_technology_type tech,
						struct slsi_kic_service_info  *info)
{
	(void)slsi_kic_service_information_ind(tech, info);
}

static inline void slsi_kic_firmware_event(uint16_t                                firmware_event_type,
					   enum slsi_kic_technology_type           tech_type,
					   uint32_t                                contain_type,
					   struct slsi_kic_firmware_event_ccp_host *event)
{
	(void)slsi_kic_firmware_event_ind(firmware_event_type, tech_type,
					  contain_type, event);
}

#else

#define slsi_kic_system_event(a, b, c) \
	do { \
		(void)(a); \
		(void)(b); \
		(void)(c); \
	} while (0)

#define slsi_kic_service_information(a, b) \
	do { \
		(void)(a); \
		(void)(b); \
	} while (0)

#define slsi_kic_firmware_event(a, b, c, d) \
	do { \
		(void)(a); \
		(void)(b); \
		(void)(c); \
		(void)(d); \
	} while (0)

#endif

#endif /* #ifndef __SLSI_KIC_LIB_H */
