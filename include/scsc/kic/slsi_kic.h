/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_H
#define __SLSI_KIC_H

#include <scsc/kic/slsi_kic_prim.h>

int slsi_kic_system_event_ind(enum slsi_kic_system_event_category event_cat,
			      enum slsi_kic_system_events event, gfp_t flags);
int slsi_kic_service_information_ind(enum slsi_kic_technology_type tech,
				     struct slsi_kic_service_info  *info);

int slsi_kic_firmware_event_ind(uint16_t firmware_event_type, uint32_t tech_type,
				uint32_t contain_type,
				struct slsi_kic_firmware_event_ccp_host *event);

#endif /* #ifndef __SLSI_KIC_H */
