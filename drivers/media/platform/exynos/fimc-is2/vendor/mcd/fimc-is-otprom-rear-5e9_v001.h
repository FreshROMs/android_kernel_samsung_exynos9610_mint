#ifndef FIMC_IS_OTPROM_REAR_5E9_V001_H
#define FIMC_IS_OTPROM_REAR_5E9_V001_H

/* Header Offset Addr Section */
#define OTP_HEADER_DIRECT_ADDR
#define OTP_5E9_GET_PAGE(a,b,c) ((((a) - (b) + (c))/64)+17)
#define OTP_5E9_GET_REG(a,b,c) (((a) - (b) + (c))%64)

#define HEADER_START_ADDR                            (0x0)
#define OTP_OEM_START_ADDR                           (0xF0)
#define OTP_HEADER_CAL_MAP_VER_START_ADDR            (0x50)
#define OTP_HEADER_VERSION_START_ADDR                (0x30)
#define OTP_HEADER_OEM_START_ADDR                    (0x8)
#define OTP_HEADER_OEM_END_ADDR                      (0xC)
#define OTP_HEADER_PROJECT_NAME_START_ADDR           (0x58)
#define OTP_HEADER_MODULE_ID_ADDR                    (0xA6)
#define OTP_HEADER_SENSOR_ID_ADDR                    (0xB0)

/* MTF DATA: AF Position & Resolution */
#define OTP_HEADER_MTF_DATA_ADDR                     0x60

/* OEM referenced section */
#define OTP_OEM_VER_START_ADDR                       0x192

/* Checksum referenced section */
#define OTP_CHECKSUM_HEADER_ADDR                     (0xEC)
#define OTP_CHECKSUM_OEM_ADDR                        (0x1AC)

/* etc section */
//#define FIMC_IS_MAX_CAL_SIZE_OTP                     (8 * 1024)   // It for use 5e9 for REAR0 
#define FIMC_IS_MAX_CAL_SIZE_REAR2                   (8 * 1024)
#define HEADER_CRC32_LEN_OTP                         (212)
#define OEM_CRC32_LEN_OTP                            (128)
#define OTP_USED_CAL_SIZE                            (OTP_CHECKSUM_OEM_ADDR + 4)

#define OTP_PAGE_ADDR				     0x0A02
#define OTP_REG_ADDR_START			     0x0A04
#define OTP_REG_ADDR_MAX			     0x0A43
#define OTP_PAGE_START_ADDR			     0x0401
#define OTP_START_PAGE                               0x11           //cal written from Page17

#define OTP_BANK

#ifdef OTP_BANK
#define OTP_START_ADDR                               0x0410
#define OTP_START_ADDR_BANK2                         0x0610
#define OTP_START_ADDR_BANK3                         0x0810
#endif

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

#define OTP_MODE_CHANGE

#ifdef OTP_MODE_CHANGE
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
#endif

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

static const u32 sensor_Global_size = 
    sizeof( sensor_Global ) / sizeof( sensor_Global[0] );


#endif /* FIMC_IS_OTPROM_REAR_5E9_V001_H */
