#ifndef FIMC_IS_EEPROM_FRONT_IMX616_V001_H
#define FIMC_IS_EEPROM_FRONT_IMX616_V001_H

/* Reference File
 * Universal EEPROM map data_V001_20190415_for LSI AP_IMX616_Front(A50s).xlsx
 */

// The start address of checksum is not same with OEM CAL start address
#define FIMC_IS_FRONT_MAX_CAL_SIZE (8 * 1024)

static int front_imx616_oem_checksum_base_addr = 0x0190;

const struct rom_extend_cal_addr front_imx616_extend_cal_addr = {
	.name = EXTEND_OEM_CHECKSUM,
	.data = &front_imx616_oem_checksum_base_addr,
	.next = NULL,
};

const struct fimc_is_vender_rom_addr front_imx616_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = (8 * 1024),

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x28,
	.rom_header_cal_map_ver_start_addr         = 0x38,
	.rom_header_project_name_start_addr        = 0x40,
	.rom_header_module_id_addr                 = 0xAE,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = -1,
	.rom_header_main_header_end_addr           = -1,
	.rom_header_main_oem_start_addr            = 0x00,
	.rom_header_main_oem_end_addr              = 0x04,
	.rom_header_main_awb_start_addr            = 0x08,
	.rom_header_main_awb_end_addr              = 0x0C,
	.rom_header_main_shading_start_addr        = 0x10,
	.rom_header_main_shading_end_addr          = 0x14,
	.rom_header_main_sensor_cal_start_addr     = 0x18,
	.rom_header_main_sensor_cal_end_addr       = 0x1C,
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

	.rom_header_main_mtf_data_addr             = 0x48,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = (216),

	.rom_oem_af_inf_position_addr              = 0x0100,
	.rom_oem_af_macro_position_addr            = 0x0108,
	.rom_oem_module_info_start_addr            = 0x01E0,
	.rom_oem_checksum_addr                     = 0x01FC,
	.rom_oem_checksum_len                      = (48),

	.rom_awb_module_info_start_addr            = 0x02E0,
	.rom_awb_checksum_addr                     = 0x02FC,
	.rom_awb_checksum_len                      = (32),

	.rom_shading_module_info_start_addr        = 0x16E0,
	.rom_shading_checksum_addr                 = 0x16FC,
	.rom_shading_checksum_len                  = (5024),

	.rom_sensor_cal_module_info_start_addr     = 0x1FE0,
	.rom_sensor_cal_checksum_addr              = 0x1FFC,
	.rom_sensor_cal_checksum_len               = (1836),

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

	.extend_cal_addr                           = &front_imx616_extend_cal_addr,
};

#endif
