#ifndef FIMC_IS_EEPROM_FRONT_IMX576_V001_H
#define FIMC_IS_EEPROM_FRONT_IMX576_V001_H

// The start address of checksum is not same with OEM CAL start address
const struct rom_extend_cal_addr imx576_front_extend_cal_addr[COUNT_EXTEND_CAL_DATA] = {
	{"oem_checksum_base_addr", 0x0190}
};

const struct fimc_is_vender_rom_addr front_imx576_cal_addr = {
	/* Set '-1' if not used */

	'A',				//char		camera_module_es_version;
	'1',				//char		cal_map_es_version;
	(16 * 1024),		//int32_t		rom_max_cal_size;

	0x00,			//int32_t		rom_header_cal_data_start_addr;
	0x28,			//int32_t		rom_header_main_module_info_start_addr;
	0x38,			//int32_t		rom_header_cal_map_ver_start_addr;
	0x40,			//int32_t		rom_header_project_name_start_addr;
	0xAE,			//int32_t		rom_header_module_id_addr;
	0xB8,			//int32_t		rom_header_main_sensor_id_addr;
	
	-1,				//int32_t		rom_header_sub_module_info_start_addr;
	-1,				//int32_t		rom_header_sub_sensor_id_addr;

	
	-1,				//int32_t		rom_header_main_header_start_addr;
	-1,				//int32_t		rom_header_main_header_end_addr;	
	0x00,			//int32_t		rom_header_main_oem_start_addr;
	0x04,			//int32_t		rom_header_main_oem_end_addr;
	0x08,			//int32_t		rom_header_main_awb_start_addr;
	0x0C,			//int32_t		rom_header_main_awb_end_addr;
	0x10,			//int32_t		rom_header_main_shading_start_addr;
	0x14,			//int32_t		rom_header_main_shading_end_addr;
	0x18,			//int32_t		rom_header_main_sensor_cal_start_addr;
	0x1C,			//int32_t		rom_header_main_sensor_cal_end_addr;
	-1,				//int32_t		rom_header_dual_cal_start_addr;
	-1,				//int32_t		rom_header_dual_cal_end_addr;
	-1,				//int32_t		rom_header_pdaf_cal_start_addr;
	-1,				//int32_t		rom_header_pdaf_cal_end_addr;

	-1,				//int32_t		rom_header_sub_oem_start_addr;
	-1,				//int32_t		rom_header_sub_oem_end_addr;
	-1,				//int32_t		rom_header_sub_awb_start_addr;
	-1,				//int32_t		rom_header_sub_awb_end_addr;
	-1,				//int32_t		rom_header_sub_shading_start_addr;
	-1,				//int32_t		rom_header_sub_shading_end_addr;

	0x48,			//int32_t		rom_header_main_mtf_data_addr;
	-1,				//int32_t		rom_header_sub_mtf_data_addr;
	
	0xFC,			//int32_t		rom_header_checksum_addr;
	(216),			//int32_t		rom_header_checksum_len;

	0x0100,			//int32_t		rom_oem_af_inf_position_addr;
	0x0108,			//int32_t		rom_oem_af_macro_position_addr;
	0x01E0,			//int32_t		rom_oem_module_info_start_addr;
	0x01FC,			//int32_t		rom_oem_checksum_addr;
	(48),			//int32_t		rom_oem_checksum_len;

	0x02E0,			//int32_t		rom_awb_module_info_start_addr;
	0x02FC,			//int32_t		rom_awb_checksum_addr;
	(32),			//int32_t		rom_awb_checksum_len;

	0x2FE0,			//int32_t		rom_shading_module_info_start_addr;
	0x2FFC,			//int32_t		rom_shading_checksum_addr;
	(9970),			//int32_t		rom_shading_checksum_len;

	0x3FE0,			//int32_t		rom_sensor_cal_module_info_start_addr;
	0x3FFC,			//int32_t		rom_sensor_cal_checksum_addr;
	(3136),			//int32_t		rom_sensor_cal_checksum_len;

	-1,				//int32_t		rom_dual_module_info_start_addr;
	-1,				//int32_t		rom_dual_checksum_addr;
	-1,				//int32_t		rom_dual_checksum_len;

	-1,				//int32_t		rom_pdaf_module_info_start_addr;
	-1,				//int32_t		rom_pdaf_checksum_addr;
	-1,				//int32_t		rom_pdaf_checksum_len;

	-1,				//int32_t		rom_sub_oem_af_inf_position_addr;
	-1,				//int32_t		rom_sub_oem_af_macro_position_addr;
	-1,				//int32_t		rom_sub_oem_module_info_start_addr;
	-1,				//int32_t		rom_sub_oem_checksum_addr;
	-1,				//int32_t		rom_sub_oem_checksum_len;


	-1,				//int32_t		rom_sub_awb_module_info_start_addr;
	-1,				//int32_t		rom_sub_awb_checksum_addr;
	-1,				//int32_t		rom_sub_awb_checksum_len;

	-1,				//int32_t		rom_sub_shading_module_info_start_addr;
	-1,				//int32_t		rom_sub_shading_checksum_addr;
	-1,				//int32_t		rom_sub_shading_checksum_len;

	-1,				//int32_t		rom_dual_cal_data2_start_addr;
	-1,				//int32_t		rom_dual_cal_data2_size;
	-1,				//int32_t		rom_dual_tilt_x_addr;
	-1,				//int32_t		rom_dual_tilt_y_addr;
	-1,				//int32_t		rom_dual_tilt_z_addr;
	-1,				//int32_t		rom_dual_tilt_sx_addr;
	-1,				//int32_t		rom_dual_tilt_sy_addr;
	-1,				//int32_t		rom_dual_tilt_range_addr;
	-1,				//int32_t		rom_dual_tilt_max_err_addr;
	-1,				//int32_t		rom_dual_tilt_avg_err_addr;
	-1,				//int32_t		rom_dual_tilt_dll_version_addr;
	-1,				//int32_t		rom_dual_shift_x_addr;
	-1,				//int32_t		rom_dual_shift_y_addr;

	imx576_front_extend_cal_addr,			//struct rom_extend_cal_addr		*extend_cal_addr;
};

#endif
