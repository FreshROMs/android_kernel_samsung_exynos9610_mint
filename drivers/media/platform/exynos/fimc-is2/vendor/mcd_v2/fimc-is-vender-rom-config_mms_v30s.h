#ifndef FIMC_IS_VENDER_ROM_CONFIG_MMS_V30S_H
#define FIMC_IS_VENDER_ROM_CONFIG_MMS_V30S_H

#include "fimc-is-eeprom-rear-gm2_gc5035_v001.h"
#include "fimc-is-eeprom-front-3p8sp_v001.h"
#include "fimc-is-eeprom-uw-4ha_v001.h"

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_gm2_gc5035_cal_addr,			//[0] SENSOR_POSITION_REAR
	&front_3p8sp_cal_addr,				//[1] SENSOR_POSITION_FRONT
	NULL,						//[2] SENSOR_POSITION_REAR2
	NULL,						//[3] SENSOR_POSITION_FRONT2
	&uw_4ha_cal_addr,				//[4] SENSOR_POSITION_REAR3
	NULL,						//[5] SENSOR_POSITION_FRONT3
	NULL,						//[6] SENSOR_POSITION_REAR4
	NULL,						//[7] SENSOR_POSITION_FRONT4
	NULL,						//[8] SP_REAR_TOF
	NULL						//[9] SP_FRONT_TOF
};

#endif /*FIMC_IS_VENDER_ROM_CONFIG_MMS_V30S_H*/
