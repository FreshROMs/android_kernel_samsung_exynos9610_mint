#ifndef FIMC_IS_VENDER_ROM_CONFIG_AAS_V80_H
#define FIMC_IS_VENDER_ROM_CONFIG_AAS_V80_H

#include "fimc-is-eeprom-rear-imx586_v001.h"

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_imx586_cal_addr,			//[0] SENSOR_POSITION_REAR
	NULL,						//[1] SENSOR_POSITION_FRONT
	NULL,						//[2] SENSOR_POSITION_REAR2
	NULL,						//[3] SENSOR_POSITION_FRONT2
	NULL,						//[4] SENSOR_POSITION_REAR3
	NULL,						//[5] SENSOR_POSITION_FRONT3
	NULL,						//[6] SENSOR_POSITION_REAR4
	NULL,						//[7] SENSOR_POSITION_FRONT4
	NULL,						//[8] SP_REAR_TOF
	NULL							//[9] SP_FRONT_TOF
};

#endif /*FIMC_IS_VENDER_ROM_CONFIG_A80_H*/

