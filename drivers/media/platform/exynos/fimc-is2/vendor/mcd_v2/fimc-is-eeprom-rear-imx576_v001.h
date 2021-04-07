#ifndef FIMC_IS_EEPROM_REAR_IMX576_V001_H
#define FIMC_IS_EEPROM_REAR_IMX576_V001_H

/* Reference File
 * Universal EEPROM map data V001_20180621_for A7_2018_Lassen_16K_EEPROM_(IMX576_24M).xlsx
 */

/* EEPROM I2C Addr Section */
#define EEP_I2C_HEADER_VERSION_START_ADDR         0x30
#define EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR     0x40

/***** HEADER Referenced Section ******/
/* Header Offset Addr Section */
#define EEP_HEADER_VERSION_START_ADDR             0x30
#define EEP_HEADER_CAL_MAP_VER_START_ADDR         0x40
#define EEP_HEADER_OEM_START_ADDR                 0x00
#define EEP_HEADER_OEM_END_ADDR                   0x04
#define EEP_HEADER_AWB_START_ADDR                 0x08
#define EEP_HEADER_AWB_END_ADDR                   0x0C
#define EEP_HEADER_AP_SHADING_START_ADDR          0x10
#define EEP_HEADER_AP_SHADING_END_ADDR	         0x14
#define EEP_HEADER_SENSOR_CAL_START_ADDR          0x18
#define EEP_HEADER_SENSOR_CAL_END_ADDR            0x1C
#define EEP_HEADER_DUAL_DATA_START_ADDR           0x20
#define EEP_HEADER_DUAL_DATA_END_ADDR             0x24
/* HEADER CAL INFO */
#define EEP_HEADER_PROJECT_NAME_START_ADDR        0x48
#define EEP_HEADER_MODULE_ID_ADDR                 0xAE
#define EEP_HEADER_SENSOR_ID_ADDR                 0xB8

/* MTF DATA: AF Position & Resolution */
#define EEP_HEADER_MTF_DATA_ADDR                  0x50

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
#define EEP_AP_SHADING_VER_START_ADDR             0x2970
#define EEP_CHECKSUM_AP_SHADING_ADDR              0x298C
#define SHADING_CRC32_LEN                         (9968)

/***** SENSOR CAL Referenced section *****/
#define EEP_AP_SENSOR_CAL_START_ADDR              0x35D0
#define EEP_CHECKSUM_SENSOR_CAL_ADDR              0x35EC
#define SENSOR_CAL_CRC32_LEN                      (3136)

/***** DUAL DATA Referenced section *****/
#define EEP_AP_DUAL_DATA_START_ADDR               0x3E60
#define EEP_CHECKSUM_DUAL_DATA_ADDR               0x3E7C
#define DUAL_DATA_CRC32_LEN                       (2144)

/***** ETC SECTION *****/
#define FIMC_IS_MAX_CAL_SIZE                      (16 * 1024)

#endif