#ifndef FIMC_IS_VENDOR_CONFIG_A50_H
#define FIMC_IS_VENDOR_CONFIG_A50_H

#include "fimc-is-eeprom-rear-imx576_v001.h"
#include "fimc-is-eeprom-rear-4ha_v001.h"
#include "fimc-is-otprom-rear-5e9_v001.h"
#include "fimc-is-eeprom-front-3j1_v001.h"


#define VENDER_PATH

#define CAMERA_MODULE_ES_VERSION_REAR 'A'
#define CAMERA_MODULE_ES_VERSION_REAR2 'A'
#define CAMERA_MODULE_ES_VERSION_REAR3 'A'

#define CAMERA_MODULE_ES_VERSION_FRONT 'A'

#define CAL_MAP_ES_VERSION_REAR '2'
#define CAL_MAP_ES_VERSION_REAR2 '1'
#define CAL_MAP_ES_VERSION_REAR3 '1'

#define CAL_MAP_ES_VERSION_FRONT '1'

#define FIMC_IS_HW_SENSOR_COUNT 4

#define CAMERA_SYSFS_V2

#define CAMERA_REAR2				/* Support Rear2 for Dual Camera */
#define CAMERA_REAR3				/* Support Rear3 */
#define SENSOR_OTP_5E9				/* Support read OTPROM for 5E9 */

#define SAMSUNG_LIVE_OUTFOCUS		/* Allocate memory For Dual Camera */
#define ENABLE_REMOSAIC_CAPTURE		/* Base Remosaic */
#define ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION		/* M2M and Rotation is used during Remosaic */

#define USE_COMMON_CAM_IO_PWR

#define USE_SSRM_CAMERA_INFO		/* Match with SAMSUNG_SSRM define of Camera Hal side */

#define EEPROM_DEBUG

#define USE_WDR_INTERFACE

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
/* #define CAMERA_HW_BIG_DATA_FILE_IO */
/* #define USE_CAMERA_HW_BIG_DATA_FOR_PANIC */
#define CSI_SCENARIO_SEN_REAR	(0) /* This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

/* VRA 1.4 improvement - adding VRA 1.4 interface : move from fimc-is-config.h */
/* Be enable this feature for New Model since A7 2018 */
#define ENABLE_VRA_LIBRARY_IMPROVE

/* this define should be used after A7 2018  */
#define USE_AI_CAMERA_INTERFACE     (1)
#define USE_MFHDR_CAMERA_INTERFACE  (1)

//#define USE_FACE_UNLOCK_AE_AWB_INIT /* for Face Unlock */

#endif /* FIMC_IS_VENDOR_CONFIG_A50_H */
