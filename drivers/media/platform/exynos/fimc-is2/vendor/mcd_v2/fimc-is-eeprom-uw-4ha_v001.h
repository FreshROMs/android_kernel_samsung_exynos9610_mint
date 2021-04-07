#ifndef FIMC_IS_EEPROM_UW_4HA_V001_H
#define FIMC_IS_EEPROM_UW_4HA_V001_H

/* Reference File
 * Universal EEPROM map data V001_20190529_for_A50s_Ramen+_8K_(4HA_8M_UW).xlsx
 */
 
#define FIMC_IS_REAR3_MAX_CAL_SIZE (8 * 1024)

#define UW_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define UW_OEM_CHECKSUM_LEN        (0x0189 - 0x0100 + 0x1)
#define UW_AWB_CHECKSUM_LEN        (0x023B - 0x01C0 + 0x1)
#define UW_SHADING_CHECKSUM_LEN    (0x1C4F - 0x0260 + 0x1)
#define UW_AE_CHECKSUM_LEN         (0x1CCF - 0x1C70 + 0x1)

struct rom_ae_cal_data uw_4ha_ae_cal_info = {
	.rom_header_main_ae_start_addr  = 0x28,
	.rom_header_main_ae_end_addr    = 0x2C,
	.rom_ae_module_info_start_addr  = 0x1CD0,
	.rom_ae_checksum_addr           = 0x1CEC,
	.rom_ae_checksum_len            = UW_AE_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr uw_4ha_extend_cal_addr = {
	.name = EXTEND_AE_CAL,
	.data = &uw_4ha_ae_cal_info,
	.next = NULL,
};

const struct fimc_is_vender_rom_addr uw_4ha_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = FIMC_IS_REAR3_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x40,
	.rom_header_cal_map_ver_start_addr         = 0x50,
	.rom_header_project_name_start_addr        = 0x58,
	.rom_header_module_id_addr                 = 0xAE,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = 0x00,
	.rom_header_main_header_end_addr           = 0x04,
	.rom_header_main_oem_start_addr            = 0x08,  /* AF start address */
	.rom_header_main_oem_end_addr              = 0x0C,  /* AF end address */
	.rom_header_main_awb_start_addr            = 0x10,
	.rom_header_main_awb_end_addr              = 0x14,
	.rom_header_main_shading_start_addr        = 0x18,
	.rom_header_main_shading_end_addr          = 0x1C,
	.rom_header_main_sensor_cal_start_addr     = -1,
	.rom_header_main_sensor_cal_end_addr       = -1,
	.rom_header_dual_cal_start_addr            = -1,
	.rom_header_dual_cal_end_addr              = -1,
	.rom_header_pdaf_cal_start_addr            = -1,
	.rom_header_pdaf_cal_end_addr              = -1,

	.rom_header_sub_oem_start_addr             = -1,
	.rom_header_sub_oem_end_addr               = -1,
	.rom_header_sub_awb_start_addr             = -1,
	.rom_header_sub_awb_end_addr               = -1,
	.rom_header_sub_shading_start_addr         = -1,
	.rom_header_sub_shading_end_addr           = -1,

	.rom_header_main_mtf_data_addr             = -1,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = UW_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = 0x0100,
	.rom_oem_af_macro_position_addr            = 0x0108,
	.rom_oem_module_info_start_addr            = 0x018A,
	.rom_oem_checksum_addr                     = 0x01BC,
	.rom_oem_checksum_len                      = UW_OEM_CHECKSUM_LEN, /* AF checksum length */

	.rom_awb_module_info_start_addr            = 0x023C,
	.rom_awb_checksum_addr                     = 0x025C,
	.rom_awb_checksum_len                      = UW_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr        = 0x1C50,
	.rom_shading_checksum_addr                 = 0x1C6C,
	.rom_shading_checksum_len                  = UW_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr     = -1,
	.rom_sensor_cal_checksum_addr              = -1,
	.rom_sensor_cal_checksum_len               = -1,

	.rom_dual_module_info_start_addr           = -1,
	.rom_dual_checksum_addr                    = -1,
	.rom_dual_checksum_len                     = -1,

	.rom_pdaf_module_info_start_addr           = -1,
	.rom_pdaf_checksum_addr                    = -1,
	.rom_pdaf_checksum_len                     = -1,

	.rom_sub_oem_af_inf_position_addr          = -1,
	.rom_sub_oem_af_macro_position_addr        = -1,
	.rom_sub_oem_module_info_start_addr        = -1,
	.rom_sub_oem_checksum_addr                 = -1,
	.rom_sub_oem_checksum_len                  = -1,

	.rom_sub_awb_module_info_start_addr        = -1,
	.rom_sub_awb_checksum_addr                 = -1,
	.rom_sub_awb_checksum_len                  = -1,

	.rom_sub_shading_module_info_start_addr    = -1,
	.rom_sub_shading_checksum_addr             = -1,
	.rom_sub_shading_checksum_len              = -1,

	.rom_dual_cal_data2_start_addr             = -1,
	.rom_dual_cal_data2_size                   = -1,
	.rom_dual_tilt_x_addr                      = -1,
	.rom_dual_tilt_y_addr                      = -1,
	.rom_dual_tilt_z_addr                      = -1,
	.rom_dual_tilt_sx_addr                     = -1,
	.rom_dual_tilt_sy_addr                     = -1,
	.rom_dual_tilt_range_addr                  = -1,
	.rom_dual_tilt_max_err_addr                = -1,
	.rom_dual_tilt_avg_err_addr                = -1,
	.rom_dual_tilt_dll_version_addr            = -1,
	.rom_dual_shift_x_addr                     = -1,
	.rom_dual_shift_y_addr                     = -1,

	.extend_cal_addr                           = NULL,
};

#endif /* FIMC_IS_EEPROM_UW_4HA_V001_H */
