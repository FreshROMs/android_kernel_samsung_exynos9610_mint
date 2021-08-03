#ifndef FIMC_IS_VENDOR_CONFIG_AAS_V50_H
#define FIMC_IS_VENDOR_CONFIG_AAS_V50_H

#define VENDER_PATH

/***** CAL ROM DEFINE *****/
#define COUNT_EXTEND_CAL_DATA   (1)               /* For searching of extend cal data name. If it is not used then set '0' */
#define ROM_DEBUG
//#define ROM_CRC32_DEBUG
//#define SKIP_CHECK_CRC                          /* Skip the CRC CHECK of cal data */


/***** SUPPORT CAMERA DEFINE *****/
#define REAR_SUB_CAMERA            (SENSOR_POSITION_REAR2)    /* Supported Camera Type for rear bokeh */
#define REAR_ULTRA_WIDE_CAMERA     (SENSOR_POSITION_REAR3)    /* Supported Camera Type for rear ultra wide */
//#define FRONT_SUB_CAMERA         (SENSOR_POSITION_FRONT2)   /* Supported Camera Type for front bokeh */

#define CAMERA_REAR2               (REAR_ULTRA_WIDE_CAMERA)   /* For Rear2 of SYSFS */
#define CAMERA_REAR3               (REAR_SUB_CAMERA)          /* For Rear3 of SYSFS */
//#define CAMERA_FRONT2            (FRONT_SUB_CAMERA)         /* For Front2 of SYSFS */
#define USE_SHARED_ROM_REAR3                         		  /* Rear3 Camera share the ROM with Rear(Wide)*/


/***** SUPPORT FUCNTION DEFINE *****/
#define SAMSUNG_LIVE_OUTFOCUS                     /* Allocate memory For Dual Camera */
#define ENABLE_REMOSAIC_CAPTURE                   /* Base Remosaic */
#define ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION     /* M2M and Rotation is used during Remosaic */
#define USE_AP_PDAF			/* Support sensor PDAF SW Solution */
//#define USE_SENSOR_WDR		/* Support sensor WDR */
//#define SUPPORT_SENSOR_3DHDR		/* whether capable of 3DHDR or not */

/* VRA 1.4 improvement - adding VRA 1.4 interface : move from fimc-is-config.h */
/* Be enable this feature for New Model since A7 2018 */
#define ENABLE_VRA_LIBRARY_IMPROVE

#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING 	    /* Using NRC based softlanding for DW-9808*/


/***** DDK - DRIVER INTERFACE *****/
#define USE_AI_CAMERA_INTERFACE     (1)           /* This feature since A7 2018 */
#define USE_MFHDR_CAMERA_INTERFACE  (1)           /* This feature since A7 2018 */
//#define USE_FACE_UNLOCK_AE_AWB_INIT             /* for Face Unlock */


/***** HW DEFENDANT DEFINE *****/
//#define USE_COMMON_CAM_IO_PWR
//#define DIVISION_EEP_IO_PWR                     /* Use Rear IO power for Front EEPROM i2c pull-up power */


/***** SUPPORT EXTERNEL FUNCTION DEFINE *****/
#define USE_SSRM_CAMERA_INFO                      /* Match with SAMSUNG_SSRM define of Camera Hal side */

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
//#define USE_CAMERA_HW_BIG_DATA_FOR_PANIC
#define CSI_SCENARIO_SEN_REAR	(0)               /* This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

#undef OVERFLOW_PANIC_ENABLE_CSIS

#endif /* FIMC_IS_VENDOR_CONFIG_A50_H */
