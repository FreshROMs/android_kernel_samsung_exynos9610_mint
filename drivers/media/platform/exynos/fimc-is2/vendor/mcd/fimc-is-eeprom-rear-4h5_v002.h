#ifndef FIMC_IS_EEPROM_REAR_4H5_V002_H
#define FIMC_IS_EEPROM_REAR_4H5_V002_H

/* Header referenced section */
#define EEP_HEADER_VERSION_START_ADDR      0x20
#define EEP_HEADER_CAL_MAP_VER_START_ADDR  0x30
#define EEP_HEADER_OEM_START_ADDR          0x0
#define EEP_HEADER_OEM_END_ADDR            0x4
#define EEP_HEADER_AWB_START_ADDR          0x8
#define EEP_HEADER_AWB_END_ADDR            0xC
#define EEP_HEADER_AP_SHADING_START_ADDR   0x10
#define EEP_HEADER_AP_SHADING_END_ADDR     0x14
#define EEP_HEADER_PROJECT_NAME_START_ADDR 0x38

/* OEM referenced section */
#define EEP_OEM_VER_START_ADDR         0x150

/* AWB referenced section */
#define EEP_AWB_VER_START_ADDR         0x220

/* AP Shading referenced section */
#define EEP_AP_SHADING_VER_START_ADDR  0X1D00

/* Checksum referenced section */
#define EEP_CHECKSUM_HEADER_ADDR       0xFC
#define EEP_CHECKSUM_OEM_ADDR          0x1FC
#define EEP_CHECKSUM_AWB_ADDR          0x2FC
#define EEP_CHECKSUM_AP_SHADING_ADDR   0x3BFC
#define EEP_CHECKSUM_C2_SHADING_ADDR   0x1FFC

/* etc section */
#define FIMC_IS_MAX_CAL_SIZE                (8 * 1024)
#define FIMC_IS_MAX_FW_SIZE                 (8 * 1024)
#define FIMC_IS_MAX_SETFILE_SIZE            (1120 * 1024)
#define HEADER_CRC32_LEN                    (80)

#endif /* FIMC_IS_EEPROM_REAR_4H5_V002_H */
