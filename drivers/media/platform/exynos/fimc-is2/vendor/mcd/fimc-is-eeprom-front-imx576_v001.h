#ifndef FIMC_IS_EEPROM_FRONT_IMX576_V001_H
#define FIMC_IS_EEPROM_FRONT_IMX576_V001_H

/* EEPROM I2C Addr Section */
#define EEP_I2C_HEADER_VERSION_START_ADDR_FRONT         0x28
#define EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT     0x38

/***** HEADER Referenced Section ******/
/* Header Offset Addr Section */
#define EEP_HEADER_VERSION_START_ADDR_FRONT             0x28
#define EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT         0x38
#define EEP_HEADER_OEM_START_ADDR_FRONT                 0x00
#define EEP_HEADER_OEM_END_ADDR_FRONT                   0x04
#define EEP_HEADER_AWB_START_ADDR_FRONT                 0x08
#define EEP_HEADER_AWB_END_ADDR_FRONT                   0x0C
#define EEP_HEADER_AP_SHADING_START_ADDR_FRONT          0x10
#define EEP_HEADER_AP_SHADING_END_ADDR_FRONT            0x14
#define EEP_HEADER_SENSOR_CAL_START_ADDR_FRONT          0x18
#define EEP_HEADER_SENSOR_CAL_END_ADDR_FRONT            0x1C

/* HEADER CAL INFO */
#define EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT        0x40
#define EEP_HEADER_MODULE_ID_ADDR_FRONT                 0xAE
#define EEP_HEADER_SENSOR_ID_ADDR_FRONT                 0xB8

/* MTF DATA: AF Position & Resolution */
#define EEP_HEADER_MTF_DATA_ADDR_FRONT                  0x48

/* HEADER CHECKSUM */
#define EEP_CHECKSUM_HEADER_ADDR_FRONT                  0xFC
#define HEADER_CRC32_LEN_FRONT                          (216)

/***** OEM Referenced Section *****/
#define EEPROM_AF_CAL_PAN_ADDR_FRONT                    0x0100
#define EEPROM_AF_CAL_MACRO_ADDR_FRONT                  0x0108

#define EEP_OEM_VER_START_ADDR_FRONT                    0x01E0
#define EEP_CHECKSUM_OEM_ADDR_FRONT                     0x01FC
#define EEP_CHECKSUM_OEM_BASE_ADDR_FRONT                0x0190
#define OEM_CRC32_LEN_FRONT                             (48)

/***** AWB Referenced section *****/
#define EEP_AWB_VER_START_ADDR_FRONT                    0x02E0
#define EEP_CHECKSUM_AWB_ADDR_FRONT                     0x02FC
#define AWB_CRC32_LEN_FRONT                             (32)

/***** Shading Referenced section *****/
#define EEP_AP_SHADING_VER_START_ADDR_FRONT             0x2FE0
#define EEP_CHECKSUM_AP_SHADING_ADDR_FRONT              0x2FFC
#define SHADING_CRC32_LEN_FRONT                         (9970)

/***** SENSOR CAL Referenced section *****/
#define EEP_AP_SENSOR_CAL_START_ADDR_FRONT              0x3000
#define EEP_CHECKSUM_SENSOR_CAL_ADDR_FRONT              0x3FFC
#define SENSOR_CAL_CRC32_LEN_FRONT                      (3136)

/***** ETC SECTION *****/
#define FIMC_IS_MAX_CAL_SIZE_FRONT                      (16 * 1024)

#endif
