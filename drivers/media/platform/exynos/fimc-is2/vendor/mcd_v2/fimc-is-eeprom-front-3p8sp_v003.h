#ifndef FIMC_IS_EEPROM_FRONT_3P8SP_V003_H
#define FIMC_IS_EEPROM_FRONT_3P8SP_V003_H

/* EEPROM I2C Addr Section */
#define EEP_I2C_HEADER_VERSION_START_ADDR_FRONT           0x28
#define EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT       0x38

/* Header Offset Addr Section */
#define EEP_HEADER_VERSION_START_ADDR_FRONT               0x28
#define EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT           0x38

//#define EEP_HEADER_DATA_START_ADDR_FRONT                  0x38
//#define EEP_HEADER_DATA_END_ADDR_FRONT                    0x3C

#define EEP_HEADER_OEM_START_ADDR_FRONT                   0x0
#define EEP_HEADER_OEM_END_ADDR_FRONT                     0x4
#define EEP_HEADER_AWB_START_ADDR_FRONT                   0x8
#define EEP_HEADER_AWB_END_ADDR_FRONT                     0xC
#define EEP_HEADER_AP_SHADING_START_ADDR_FRONT            0x10
#define EEP_HEADER_AP_SHADING_END_ADDR_FRONT              0x14
#define EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT    0x18
#define EEP_HEADER_CROSSTALK_CAL_DATA_END_ADDR_FRONT      0x1C
#define EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT          0x40
#define EEP_HEADER_MTF_DATA_ADDR_FRONT                    0x48
#define EEP_HEADER_MODULE_ID_ADDR_FRONT                   0xAE
#define EEP_HEADER_SENSOR_ID_ADDR_FRONT                   0xB8

/* OEM referenced section */
#define EEP_OEM_VER_START_ADDR_FRONT                      0x1E0
/* AWB referenced section */
#define EEP_AWB_VER_START_ADDR_FRONT                      0x2E0
/* AP Shading referenced section */
#define EEP_AP_SHADING_VER_START_ADDR_FRONT               0x16E0
/* AP Shading referenced section */
#define EEP_CROSSTALK_CAL_DATA_VER_START_ADDR_FRONT       0x1FE0


/* Checksum referenced section */
#define EEP_CHECKSUM_HEADER_ADDR_FRONT                    0xFC
#define EEP_CHECKSUM_OEM_ADDR_FRONT                       0x1FC
#define EEP_CHECKSUM_AWB_ADDR_FRONT                       0x2FC
#define EEP_CHECKSUM_AP_SHADING_ADDR_FRONT                0x16FC
#define EEP_CHECKSUM_CROSSTALK_CAL_DATA_ADDR_FRONT        0x1FFC


/* 3P8SP XTALK Cal Data */
#define EEP_XTALK_CAL_START_ADDR_FRONT                    0x1700
#define EEP_XTALK_CAL_DATA_SIZE_FRONT                     (2 * 1024)


/* etc section */
#define FIMC_IS_MAX_CAL_SIZE_FRONT                        (16 * 1024)

/* Module Data Checksum -> calculated from EEPROM map xls Checksum section as (EndAdd - StartAdd + 0x1)*/
#define HEADER_CRC32_LEN_FRONT                            (0xD7-0x00 + 0x1)
#define AWB_CRC32_LEN_FRONT                               (0x21F-0x200 + 0x1)
#define SHADING_CRC32_LEN_FRONT                           (0x167F-0x300 + 0x1)

#endif /* FIMC_IS_EEPROM_FRONT_3P8SP_V003_H */
