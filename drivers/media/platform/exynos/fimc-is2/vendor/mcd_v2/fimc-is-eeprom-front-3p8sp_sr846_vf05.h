#ifndef FIMC_IS_EEPROM_FRONT_3P8SP_SR846_VF03_H
#define FIMC_IS_EEPROM_FRONT_3P8SP_SR846_VF03_H

/* EEPROM I2C Addr Section */
#define EEP_I2C_HEADER_VERSION_START_ADDR_FRONT           0x50
#define EEP_I2C_HEADER_VERSION_START_ADDR_FRONT2          0x60
#define EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT       0x70

/* Header Offset Addr Section */
#define EEP_HEADER_VERSION_START_ADDR_FRONT               0x50
#define EEP_HEADER_VERSION_START_ADDR_FRONT2              0x60
#define EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT           0x70
#define EEP_HEADER_DATA_START_ADDR_FRONT                  0x38
#define EEP_HEADER_DATA_END_ADDR_FRONT                    0x3C

#define EEP_HEADER_AWB_START_ADDR_FRONT                   0x0
#define EEP_HEADER_AWB_END_ADDR_FRONT                     0x4
#define EEP_HEADER_AP_SHADING_START_ADDR_FRONT            0x8
#define EEP_HEADER_AP_SHADING_END_ADDR_FRONT              0xC
#define EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT    0x10
#define EEP_HEADER_CROSSTALK_CAL_DATA_END_ADDR_FRONT      0x14

#define EEP_HEADER_AWB_START_ADDR_FRONT2                  0x20
#define EEP_HEADER_AWB_END_ADDR_FRONT2                    0x24
#define EEP_HEADER_AP_SHADING_START_ADDR_FRONT2           0x28
#define EEP_HEADER_AP_SHADING_END_ADDR_FRONT2             0x2C

#define EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT          0x7C
#define EEP_HEADER_MODULE_ID_ADDR_FRONT                   0xAE
#define EEP_HEADER_SENSOR_ID_ADDR_FRONT                   0xB8
#define EEP_HEADER_SENSOR_ID_ADDR_FRONT2                  0xC8

#define EEP_HEADER_MTF_DATA_ADDR_FRONT                   0x3A80
#define EEP_HEADER_MTF_DATA2_ADDR_FRONT                  0x3AB6

/* AWB referenced section */
#define EEP_AWB_VER_START_ADDR_FRONT                      0x1A0
#define EEP_AWB_VER_START_ADDR_FRONT2                     0x1E60
/* AP Shading referenced section */
#define EEP_AP_SHADING_VER_START_ADDR_FRONT               0x1560
#define EEP_AP_SHADING_VER_START_ADDR_FRONT2              0x3FE0
/* Crosstalk Cal Data referenced section */
#define EEP_CROSSTALK_CAL_DATA_VER_START_ADDR_FRONT       0x1DA0

/* 3P8SP XTALK Cal Data */
#define EEP_XTALK_CAL_START_ADDR_FRONT                    0x1580
#define EEP_XTALK_CAL_DATA_SIZE_FRONT                     (2 * 1024)

/* Front2 Cal Dual Calibration Data2 */
#define EEP_FRONT2_DUAL_CAL2                              0x3290
#define EEP_FRONT2_DUAL_CAL2_SIZE                         512

#define EEP_FRONT2_DUAL_TILT_X                            0x32EC
#define EEP_FRONT2_DUAL_TILT_Y                            0x32F0
#define EEP_FRONT2_DUAL_TILT_Z                            0x32F4
#define EEP_FRONT2_DUAL_TILT_SX                           0x334C
#define EEP_FRONT2_DUAL_TILT_SY                           0x3350
#define EEP_FRONT2_DUAL_TILT_RANGE                        0x3470
#define EEP_FRONT2_DUAL_TILT_MAX_ERR                      0x3474
#define EEP_FRONT2_DUAL_TILT_AVG_ERR                      0x3478
#define EEP_FRONT2_DUAL_TILT_DLL_VERSION                  0x346C

#define EEP_FRONT2_DUAL_SHIFT_X                           0x334C
#define EEP_FRONT2_DUAL_SHIFT_Y                           0x3350

/* Checksum referenced section */
#define EEP_CHECKSUM_HEADER_ADDR_FRONT                    0xFC
#define EEP_CHECKSUM_AWB_ADDR_FRONT                       0x1BC
#define EEP_CHECKSUM_AP_SHADING_ADDR_FRONT                0x157C
#define EEP_CHECKSUM_CROSSTALK_CAL_DATA_ADDR_FRONT        0x1DBC
#define EEP_CHECKSUM_AWB_ADDR_FRONT2                      0x1E7C
#define EEP_CHECKSUM_AP_SHADING_ADDR_FRONT2               0x3FFC

/* etc section */
#define FIMC_IS_MAX_CAL_SIZE_FRONT           (16 * 1024)

/* Module Data Checksum */
#define HEADER_CRC32_LEN_FRONT               (224)
#define AWB_CRC32_LEN_FRONT                  (128)
#define SHADING_CRC32_LEN_FRONT              (5008)
#define CROSSTALK_CRC32_LEN_FRONT            (2064)
#define AWB_CRC32_LEN_FRONT2                 (128)
#define SHADING_CRC32_LEN_FRONT2             (7296)

#endif /* FIMC_IS_EEPROM_FRONT_3P8SP_SR846_VF05_H */
