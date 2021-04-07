#ifndef FIMC_IS_EEPROM_REAR_IMX576_V001_H
#define FIMC_IS_EEPROM_REAR_IMX576_V001_H

/* EEPROM I2C Addr Section */
#define EEP_I2C_HEADER_VERSION_START_ADDR         0x30
#define EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR     0x50

/***** HEADER Referenced Section ******/
/* Header Offset Addr Section */
#define EEP_HEADER_VERSION_START_ADDR             0x30
#define EEP_HEADER_CAL_MAP_VER_START_ADDR         0x50
#define EEP_HEADER_OEM_START_ADDR                 0x00
#define EEP_HEADER_OEM_END_ADDR                   0x04
#define EEP_HEADER_AWB_START_ADDR                 0x08
#define EEP_HEADER_AWB_END_ADDR                   0x0C
#define EEP_HEADER_AP_SHADING_START_ADDR          0x10
#define EEP_HEADER_AP_SHADING_END_ADDR            0x14
#define EEP_HEADER_SENSOR_CAL_START_ADDR          0x18
#define EEP_HEADER_SENSOR_CAL_END_ADDR            0x1C
#define EEP_HEADER_DUAL_DATA_START_ADDR           0x20
#define EEP_HEADER_DUAL_DATA_END_ADDR             0x24
/* HEADER CAL INFO */
#define EEP_HEADER_PROJECT_NAME_START_ADDR        0x5C
#define EEP_HEADER_MODULE_ID_ADDR                 0xA8
#define EEP_HEADER_SENSOR_ID_ADDR                 0xB8

/* MTF DATA: AF Position & Resolution */
#define EEP_HEADER_MTF_DATA_ADDR                  0x64

/* HEADER CHECKSUM */
#define EEP_CHECKSUM_HEADER_ADDR                  0xFC
#define HEADER_CRC32_LEN                          (224)

/***** OEM Referenced Section *****/
#define EEPROM_AF_CAL_PAN_ADDR                    0x0100
#define EEPROM_AF_CAL_MACRO_ADDR                  0x0108

#define EEP_OEM_VER_START_ADDR                    0x01D2
#define EEP_CHECKSUM_OEM_ADDR                     0x01FC
#define OEM_CRC32_LEN                             (210)

/***** AWB Referenced section *****/
#define EEP_AWB_VER_START_ADDR                    0x0260
#define EEP_CHECKSUM_AWB_ADDR                     0x027C
#define AWB_CRC32_LEN                             (96)

/***** Shading Referenced section *****/
#define EEP_AP_SHADING_VER_START_ADDR             0x1610
#define EEP_CHECKSUM_AP_SHADING_ADDR              0x162C
#define SHADING_CRC32_LEN                         (5008)

/***** SENSOR CAL Referenced section *****/
#define EEP_AP_SENSOR_CAL_START_ADDR              0x2270
#define EEP_CHECKSUM_SENSOR_CAL_ADDR              0x228C
#define SENSOR_CAL_CRC32_LEN                      (3136)

/***** DUAL DATA Referenced section *****/
#define EEP_DUAL_DATA_VER_START_ADDR              0x26F0
#define EEP_CHECKSUM_DUAL_DATA_ADDR               0x270C
#define DUAL_DATA_CRC32_LEN                       (1120)

/***** REAR2 Cal Dual Calibration Data2 *****/
#define EEP_DUAL_CAL_DATA2_ADDR                  0x2294
#define EEP_DUAL_CAL_DATA2_SIZE                  (512)

#define EEP_REAR2_DUAL_TILT_X                    0x22F0
#define EEP_REAR2_DUAL_TILT_Y                    0x22F4
#define EEP_REAR2_DUAL_TILT_Z                    0x22F8
#define EEP_REAR2_DUAL_TILT_SX                   0x2350
#define EEP_REAR2_DUAL_TILT_SY                   0x2354
#define EEP_REAR2_DUAL_TILT_RANGE                0x2474
#define EEP_REAR2_DUAL_TILT_MAX_ERR              0x2478
#define EEP_REAR2_DUAL_TILT_AVG_ERR              0x247C
#define EEP_REAR2_DUAL_TILT_DLL_VERSION          0x2290

/***** ETC SECTION *****/
#define FIMC_IS_MAX_CAL_SIZE                      (10 * 1024)

#endif

