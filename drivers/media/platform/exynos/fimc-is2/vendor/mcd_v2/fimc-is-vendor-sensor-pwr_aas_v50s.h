#ifndef FIMC_IS_VENDOR_SENSOR_PWR_AAS_V50S_H
#define FIMC_IS_VENDOR_SENSOR_PWR_AAS_V50S_H

/***** This file is used to define sensor power pin name for only AAS_V50S *****/

#define USE_VENDOR_PWR_PIN_NAME


/***** REAR MAIN - IMX582 *****/
#define RCAM_AF_VDD        "vdd_ldo37"    /* RCAM1_AFVDD_2P8 */
#define IMX582_IOVDD       "CAM_VLDO3"    /* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define IMX582_AVDD1       "CAM_VLDO6"    /* RCAM1_AVDD1_2P9 */
#define IMX582_AVDD2       "gpio_ldo_en"  /* RCAM1_AVDD2_1P8 */
#define IMX582_DVDD        "CAM_VLDO1"    /* RCAM1_DVDD_1P1 */


/***** FRONT - IMX616 *****/
#define IMX616_AVDD        "CAM_VLDO5"    /* CAM_PMIC_VLDO7 */
#define IMX616_DVDD        "CAM_VLDO2"    /* CAM_PMIC_VLDO2 */
#define IMX616_IOVDD       "CAM_VLDO3"    /* CAM_PMIC_VLDO3 */


/***** REAR2 SUB - GC5035 *****/
#define GC5035_IOVDD       "CAM_VLDO3"    /* CAM_VDDIO_1P8 */
#define GC5035_AVDD        "CAM_VLDO7"    /* RCAM2_AVDD_2P8 */
#define GC5035_DVDD        "vdd_ldo44"    /* RCAM2_DVDD_1P2 */


/***** REAR3 WIDE - S5K4HA *****/
#define S5K4HA_IOVDD       "CAM_VLDO3"         /* CAM_VDDIO_1P8  */
#define S5K4HA_AVDD        "gpio_cam_a2p8_en"  /* RCAM3_AVDD_2P8: XGPIO35: GPM22[0] */
#define S5K4HA_DVDD        "CAM_VLDO4"         /* RCAM3_DVDD_1P2 */


/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR             /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */
//#define DIVISION_EEP_IO_PWR             /* Use Rear IO power for Front EEPROM i2c pull-up power */


#endif /* FIMC_IS_VENDOR_SENSOR_PWR_AAS_V50S_H */
