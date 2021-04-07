#ifndef FIMC_IS_EEPROM_REAR_IMX586_V001_H
#define FIMC_IS_EEPROM_REAR_IMX586_V001_H

/* Reference File
 * This file not yet modified for A80
 * This is only for bring-up
 */

#define FIMC_IS_REAR_MAX_CAL_SIZE (10 * 1024)
#define FIMC_IS_REAR2_MAX_CAL_SIZE (10 * 1024)

#define REAR_HEADER_CHECKSUM_LEN (0x00EF - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN (0x01D1 - 0x0100 + 0x1)
#define REAR_AWB_CHECKSUM_LEN (0x025F - 0x0200 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN (0x08CF - 0x0280 + 0x1)
#define REAR_SENSOR_CAL_CHECKSUM_LEN (0x165F - 0x0900 + 0x1)
#define REAR_DUAL_CHECKSUM_LEN (0x1AEF - 0x1690 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN (0x02DF - 0x1B20 + 0x1)

const struct fimc_is_vender_rom_addr rear_imx586_cal_addr = {
	/* Set '-1' if not used */

	'A',				//char		camera_module_es_version;
	'1',				//char		cal_map_es_version;
	FIMC_IS_REAR_MAX_CAL_SIZE,		//int32_t		rom_max_cal_size;

	0x00,			//int32_t		rom_header_cal_data_start_addr;
	0x30,			//int32_t		rom_header_main_module_info_start_addr;
	0x50,			//int32_t		rom_header_cal_map_ver_start_addr;
	0x5C,			//int32_t		rom_header_project_name_start_addr;
	0xAE,			//int32_t		rom_header_module_id_addr;
	0xB8,			//int32_t		rom_header_main_sensor_id_addr;
	
	0x40,			//int32_t		rom_header_sub_module_info_start_addr;
	0xC8,			//int32_t		rom_header_sub_sensor_id_addr;

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
	0x20,			//int32_t		rom_header_dual_cal_start_addr;
	0x24,			//int32_t		rom_header_dual_cal_end_addr;
	0x28,			//int32_t		rom_header_pdaf_cal_start_addr;
	0x2c,			//int32_t		rom_header_pdaf_cal_end_addr;

	-1,				//int32_t		rom_header_sub_oem_start_addr;
	-1,				//int32_t		rom_header_sub_oem_end_addr;
	-1,				//int32_t		rom_header_sub_awb_start_addr;
	-1,				//int32_t		rom_header_sub_awb_end_addr;
	-1,				//int32_t		rom_header_sub_shading_start_addr;
	-1,				//int32_t		rom_header_sub_shading_end_addr;

	0x64,			//int32_t		rom_header_main_mtf_data_addr;
	-1,				//int32_t		rom_header_sub_mtf_data_addr;
	
	0xFC,			//int32_t		rom_header_checksum_addr;
	REAR_HEADER_CHECKSUM_LEN,		//int32_t		rom_header_checksum_len;

	0x0100,			//int32_t		rom_oem_af_inf_position_addr;
	0x0108,			//int32_t		rom_oem_af_macro_position_addr;
	0x01D2,			//int32_t		rom_oem_module_info_start_addr;
	0x01FC,			//int32_t		rom_oem_checksum_addr;
	REAR_OEM_CHECKSUM_LEN,			//int32_t		rom_oem_checksum_len;

	0x0260,			//int32_t		rom_awb_module_info_start_addr;
	0x027C,			//int32_t		rom_awb_checksum_addr;
	REAR_AWB_CHECKSUM_LEN,			//int32_t		rom_awb_checksum_len;

	0x08D0,			//int32_t		rom_shading_module_info_start_addr;
	0x08FC,			//int32_t		rom_shading_checksum_addr;
	REAR_SHADING_CHECKSUM_LEN,		//int32_t		rom_shading_checksum_len;

	0x1660,			//int32_t		rom_sensor_cal_module_info_start_addr;
	0x168C,			//int32_t		rom_sensor_cal_checksum_addr;
	REAR_SENSOR_CAL_CHECKSUM_LEN,	//int32_t		rom_sensor_cal_checksum_len;

	0x1AF0,			//int32_t		rom_dual_module_info_start_addr;
	0x1B1C,			//int32_t		rom_dual_checksum_addr;
	REAR_DUAL_CHECKSUM_LEN,		//int32_t		rom_dual_checksum_len;

	0x20E0,				//int32_t		rom_pdaf_module_info_start_addr;
	0x210C,				//int32_t		rom_pdaf_checksum_addr;
	REAR_PDAF_CHECKSUM_LEN,		//int32_t		rom_pdaf_checksum_len;

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

	0x1694,			//int32_t		rom_dual_cal_data2_start_addr;
	(512),			//int32_t		rom_dual_cal_data2_size;
	0x16F0,			//int32_t		rom_dual_tilt_x_addr;
	0x16F4,			//int32_t		rom_dual_tilt_y_addr;
	0x16F8,			//int32_t		rom_dual_tilt_z_addr;
	0x1750,			//int32_t		rom_dual_tilt_sx_addr;
	0x1754,			//int32_t		rom_dual_tilt_sy_addr;
	0x1974,			//int32_t		rom_dual_tilt_range_addr;
	0x1978,			//int32_t		rom_dual_tilt_max_err_addr;
	0x197C,			//int32_t		rom_dual_tilt_avg_err_addr;
	0x1690,			//int32_t		rom_dual_tilt_dll_version_addr;
	-1,				//int32_t		rom_dual_shift_x_addr;
	-1,				//int32_t		rom_dual_shift_y_addr;

	NULL,			//void*		extended_cal_addr;
};

#endif
