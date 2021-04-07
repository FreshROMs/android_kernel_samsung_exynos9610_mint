#ifndef FIMC_IS_VENDOR_SENSOR_PWR_AAS_V80_H
#define FIMC_IS_VENDOR_SENSOR_PWR_AAS_V80_H

/***** USE SENSOR POWER PIN NAME FOR ONLY A80 *****/
#define USE_VENDOR_PWR_PIN_NAME


/***** REAR MAIN - IMX586 *****/
#define RCAM_AF_VDD             "CAM_VLDO5"       /* RCAM1_AFVDD_2P8 */
#define IMX586_IFVDD            "vdd_ldo37"       /* CAM_VDDIO_1P8 : IFPMIC LDO37 is used for all camera commonly */
#define IMX586_AVDD1            "CAM_VLDO7"       /* RCAM1_AVDD1_2P9 */
#define IMX586_AVDD2            "CAM_VLDO3"       /* RCAM1_AVDD2_1P8 */
#define IMX586_DVDD1            "CAM_VLDO2"       /* RCAM1_DVDD1_1P1 */
#define IMX586_DVDD2            "CAM_VLDO4"       /* RCMA1_DVDD2_1P1 */


/***** FRONT - S5KGD1 *****/






/***** REAR2 SUB - GC5035 *****/





/***** REAR3 WIDE - 4HA *****/




/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR                     /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */

#endif
