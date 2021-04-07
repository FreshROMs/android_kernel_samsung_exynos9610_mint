#ifndef FIMC_IS_OTPROM_REAR_5E9_V001_H
#define FIMC_IS_OTPROM_REAR_5E9_V001_H

/* Header Offset Addr Section */
#define OTP_HEADER_DIRECT_ADDR
#define OTP_5E9_GET_PAGE(a,b,c) ((((a) - (b) + (c))/64)+17)
#define OTP_5E9_GET_REG(a,b,c) (((a) - (b) + (c))%64)


/* OEM referenced section */
#define OTP_OEM_START_ADDR    (0xF0)			// For Check the checksum
#define OTP_USED_CAL_SIZE     (0x1AC + 4)		// For Check the checksum


const struct fimc_is_vender_rom_addr rear_5e9_otp_cal_addr = {
	/* Set '-1' if not used */

	'A',					//char		camera_module_es_version;
	(1),					//u8			cal_map_es_version;
	OTP_USED_CAL_SIZE,		//int32_t		rom_max_cal_size;

	0x00,			//int32_t		rom_header_cal_data_start_addr;
	0x30,			//int32_t		rom_header_main_module_info_start_addr;
	0x50,			//int32_t		rom_header_cal_map_ver_start_addr;
	0x58,			//int32_t		rom_header_project_name_start_addr;
	0xA6,			//int32_t		rom_header_module_id_addr;
	0xB0,			//int32_t		rom_header_main_sensor_id_addr;
	
	-1,				//int32_t		rom_header_sub_module_info_start_addr;
	-1,				//int32_t		rom_header_sub_sensor_id_addr;

	-1,				//int32_t		rom_header_main_header_start_addr;
	-1,				//int32_t		rom_header_main_header_end_addr;	
	0x08,			//int32_t		rom_header_main_oem_start_addr;
	0x0C,			//int32_t		rom_header_main_oem_end_addr;
	-1,				//int32_t		rom_header_main_awb_start_addr;
	-1,				//int32_t		rom_header_main_awb_end_addr;
	-1,				//int32_t		rom_header_main_shading_start_addr;
	-1,				//int32_t		rom_header_main_shading_end_addr;
	-1,				//int32_t		rom_header_main_sensor_cal_start_addr;
	-1,				//int32_t		rom_header_main_sensor_cal_end_addr;
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

	0x60,			//int32_t		rom_header_mtf_data_addr;
	-1,				//int32_t		rom_header_sub_mtf_data_addr;
	
	0xEC,			//int32_t		rom_header_checksum_addr;
	(212),			//int32_t		rom_header_checksum_len;

	-1,				//int32_t		rom_oem_af_inf_position_addr;
	-1,				//int32_t		rom_oem_af_macro_position_addr;
	0x0192,			//int32_t		rom_oem_module_info_start_addr;
	0x01AC,			//int32_t		rom_oem_checksum_addr;
	(128),			//int32_t		rom_oem_checksum_len;

	-1,				//int32_t		rom_awb_module_info_start_addr;
	-1,				//int32_t		rom_awb_checksum_addr;
	-1,				//int32_t		rom_awb_checksum_len;

	-1,				//int32_t		rom_shading_module_info_start_addr;
	-1,				//int32_t		rom_shading_checksum_addr;
	-1,				//int32_t		rom_shading_checksum_len;

	-1,				//int32_t		rom_sensor_cal_module_info_start_addr;
	-1,				//int32_t		rom_sensor_cal_checksum_addr;
	-1,				//int32_t		rom_sensor_cal_checksum_len;

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

	NULL,			//void*		extended_cal_addr;
};

#define OTP_PAGE_ADDR				     0x0A02
#define OTP_REG_ADDR_START			     0x0A04
#define OTP_REG_ADDR_MAX			     0x0A43
#define OTP_PAGE_START_ADDR			     0x0401
#define OTP_START_PAGE                               0x11           //cal written from Page17

#define OTP_START_ADDR                               0x0410
#define OTP_START_ADDR_BANK2                         0x0610
#define OTP_START_ADDR_BANK3                         0x0810
#define OTP_BANK_ADDR                                0x401

static const u32 OTP_Init_reg[] = {
	0x0A00, 0x04, 0x1,
	0x0A00, 0x00, 0x1,
};

static const u32 OTP_Init_size =
	sizeof(OTP_Init_reg) / sizeof(OTP_Init_reg[0]);


static const u32 OTP_first_page_select_reg[] = {
	0x0A00, 0x04, 0x1,
	0x0A02, 0x02, 0x1,
	0x0A00, 0x01, 0x1,
};

static const u32 OTP_first_page_select_reg_size =
	sizeof(OTP_first_page_select_reg) / sizeof(OTP_first_page_select_reg[0]);

static const u32 OTP_second_page_select_reg[] = {
	0x0A00, 0x04, 0x1,
	0x0A02, 0x03, 0x1,
	0x0A00, 0x01, 0x1,
};

static const u32 OTP_second_page_select_reg_size =
	sizeof(OTP_second_page_select_reg) / sizeof(OTP_second_page_select_reg[0]);

static const u32 sensor_mode_change_to_OTP_reg[] = {
	0x0A00, 0x04, 0x1,
	0x0A02, 0x02, 0x1,
	0x0A00, 0x01, 0x1,
};

static const u32 sensor_mode_change_to_OTP_reg_size =
	sizeof(sensor_mode_change_to_OTP_reg) / sizeof(sensor_mode_change_to_OTP_reg[0]);

static const u32 sensor_mode_change_from_OTP_reg[] = {
	0x0A00, 0x04, 0x1,
	0x0A00, 0x00, 0x1,
};

static const u32 sensor_mode_change_from_OTP_reg_size =
	sizeof(sensor_mode_change_from_OTP_reg) / sizeof(sensor_mode_change_from_OTP_reg[0]);

static const u32 sensor_Global[] = {
	/* Analog Global Setting */
	0x0100, 0x00, 0x1, 
	0x0A02, 0x3F, 0x1, 
	0x3B45, 0x01, 0x1, 
	0x3290, 0x10, 0x1, 
	0x0B05, 0x01, 0x1, 
	0x3069, 0x87, 0x1, 
	0x3074, 0x06, 0x1, 
	0x3075, 0x2F, 0x1, 
	0x301F, 0x20, 0x1, 
	0x306B, 0x9A, 0x1, 
	0x3091, 0x1B, 0x1, 
	0x306E, 0x71, 0x1, 
	0x306F, 0x28, 0x1, 
	0x306D, 0x08, 0x1, 
	0x3084, 0x16, 0x1, 
	0x3070, 0x0F, 0x1, 
	0x306A, 0x79, 0x1, 
	0x30B0, 0xFF, 0x1, 
	0x30C2, 0x05, 0x1, 
	0x30C4, 0x06, 0x1, 
	0x3012, 0x4E, 0x1, 
	0x3080, 0x08, 0x1, 
	0x3083, 0x14, 0x1, 
	0x3200, 0x01, 0x1, 
	0x3081, 0x07, 0x1, 
	0x307B, 0x85, 0x1, 
	0x307A, 0x0A, 0x1, 
	0x3079, 0x0A, 0x1, 
	0x308A, 0x20, 0x1, 
	0x308B, 0x08, 0x1, 
	0x308C, 0x0B, 0x1, 
	0x392F, 0x01, 0x1, 
	0x3930, 0x00, 0x1, 
	0x3924, 0x7F, 0x1, 
	0x3925, 0xFD, 0x1, 
	0x3C08, 0xFF, 0x1, 
	0x3C09, 0xFF, 0x1, 
	0x3C31, 0xFF, 0x1, 
	0x3C32, 0xFF, 0x1, 
};

static const u32 sensor_Global_size = sizeof( sensor_Global ) / sizeof( sensor_Global[0] );


#endif /* FIMC_IS_OTPROM_REAR_5E9_V001_H */
