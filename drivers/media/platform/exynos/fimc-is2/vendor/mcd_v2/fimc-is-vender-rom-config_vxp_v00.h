#ifndef FIMC_IS_VENDER_ROM_CONFIG_VXP_V00_H
#define FIMC_IS_VENDER_ROM_CONFIG_VXP_V00_H

/***** [ ROM VERSION HISTORY] *******************************************
 *
 * < HW_REV 00 >
 *  rear eeprom version v001 : fimc-is-eeprom-rear-imx576_v001.h
 *  CAL_MAP_ES_VERSION_REAR  : 1
 *
 * < HW_REV 01 >
 *  rear eeprom version v002 : fimc-is-eeprom-rear-imx576_v002.h
 *  CAL_MAP_ES_VERSION_REAR  : 2
 *
 ***********************************************************************/

#include "fimc-is-eeprom-rear-imx576_v002.h"
#include "fimc-is-eeprom-rear-4ha_v001.h"
#include "fimc-is-otprom-rear-5e9_v001.h"
//#include "fimc-is-eeprom-front-imx576_v001.h"

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_imx576_cal_addr,		//[0] SENSOR_POSITION_REAR
	NULL,					//[1] SENSOR_POSITION_FRONT
	&rear_5e9_otp_cal_addr,	//[2] SENSOR_POSITION_REAR2
	NULL,					//[3] SENSOR_POSITION_SECURE
	NULL,					//[4] SENSOR_POSITION NOT DEFINED
	NULL,					//[5] SENSOR_POSITION_FRONT2
	&rear_4ha_cal_addr,		//[6] SENSOR_POSITION_REAR3
};

#endif

