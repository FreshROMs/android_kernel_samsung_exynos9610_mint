/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SEC_DEFINE_H
#define FIMC_IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/zlib.h>

#include "fimc-is-core.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"
#include "crc32.h"
#include "fimc-is-companion.h"
#include "fimc-is-device-from.h"
#include "fimc-is-dt.h"

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define FW_ISP_COMPANY_BROADCOMM	'B'
#define FW_ISP_COMPANY_TN		'C'
#define FW_ISP_COMPANY_FUJITSU		'F'
#define FW_ISP_COMPANY_INTEL		'I'
#define FW_ISP_COMPANY_LSI		'L'
#define FW_ISP_COMPANY_MARVELL		'M'
#define FW_ISP_COMPANY_QUALCOMM		'Q'
#define FW_ISP_COMPANY_RENESAS		'R'
#define FW_ISP_COMPANY_STE		'S'
#define FW_ISP_COMPANY_TI		'T'
#define FW_ISP_COMPANY_DMC		'D'

#define FW_SENSOR_MAKER_SF		'F'
#define FW_SENSOR_MAKER_SLSI		'L'
#define FW_SENSOR_MAKER_SONY		'S'

#define FW_MODULE_COMPANY_SEMCO		'S'
#define FW_MODULE_COMPANY_GUMI		'O'
#define FW_MODULE_COMPANY_CAMSYS	'C'
#define FW_MODULE_COMPANY_PATRON	'P'
#define FW_MODULE_COMPANY_MCNEX		'M'
#define FW_MODULE_COMPANY_LITEON	'L'
#define FW_MODULE_COMPANY_VIETNAM	'V'
#define FW_MODULE_COMPANY_SHARP		'J'
#define FW_MODULE_COMPANY_NAMUGA	'N'
#define FW_MODULE_COMPANY_POWER_LOGIX	'A'
#define FW_MODULE_COMPANY_DI		'D'

#define FW_2P2_F		"F16LL"
#define FW_2P2_I		"I16LL"
#define FW_2P2_A		"A16LL"
#define FW_2P2_B		"B16LL"
#define FW_2P2_C		"C16LL"
#define FW_3L2			"C13LL"
#define FW_IMX135		"C13LS"
#define FW_IMX134		"D08LS"
#define FW_IMX228		"A20LS"
#define FW_IMX240_A		"A16LS"
#define FW_IMX240_B		"B16LS"
#define FW_IMX240_C		"C16LS"
#define FW_IMX240_Q		"H16US"
#define FW_IMX240_Q_C1		"H16UL"
#define FW_IMX260_C		"C12LS"
#define FW_2P2_12M		"G16LL"
#define FW_4H5			"F08LL"
#define FW_2P3			"J16LL"
#define FW_2T2			"A20LL"
#define FW_3P3			"D16LL"
#define FW_2L1_C		"C12LL"
#define FW_3P8_M		"M16LL"
#define FW_IMX219		"S08LS"
#define FW_IMX260_D		"D12LS"
#define FW_IMX333_E		"E12LS"		/* DREAM2 */
#define FW_IMX333_F		"F12LS"		/* DREAM */

#define FW_2L2_E		"E12LL"		/* DREAM2 */
#define FW_2L2_F		"F12LL"		/* DREAM */

#define FW_3M3_D		"D13LL"
#define FW_3M3			"E13LL"

#define FW_4E6			"D05LL"
#define FW_3H1			"V08LL"
#define FW_IMX320		"V08LS"

#define FW_2P6_Q		"Q16LL" /*JACKPOT REAR*/
#define FW_3P8SP_P		"P16LL" /*JACKPOT FRONT-1*/
#define FW_SR846_P		"P08LF" /*JACKPOT FRONT-2*/

#define FW_4H5YC_P		"P08LL" /*4H5YC FRONT*/
#define FW_4H5YC_Q		"Q08LL" /*4H5YC REAR*/

#define FW_COMP_IMX260_IMX320	"D12LS"
#define FW_COMP_2L2_3H1		"E12LL"
#define FW_COMP_IMX333_IMX320	"E12LS"
#define FW_COMP_IMX333_3H1	"E12LY"
#define FW_COMP_2L2_IMX320	"E12LX"

#define FW_IMX576		"B24LS"
#define FW_IMX576_C	"C24LS"

#define SDCARD_FW

#define FIMC_IS_DDK					"fimc_is_lib.bin"
#define FIMC_IS_RTA					"fimc_is_rta.bin"

#define FIMC_IS_FW_2P2				"fimc_is_fw2_2p2.bin"
#define FIMC_IS_FW_2P2_12M				"fimc_is_fw2_2p2_12m.bin"
#define FIMC_IS_FW_2P3				"fimc_is_fw2_2p3.bin"
#define FIMC_IS_FW_3P3				"fimc_is_fw2_3p3.bin"
#define FIMC_IS_FW_3L2				"fimc_is_fw2_3l2.bin"
#define FIMC_IS_FW_4H5				"fimc_is_fw2_4h5.bin"
#define FIMC_IS_FW_IMX134			"fimc_is_fw2_imx134.bin"
#define FIMC_IS_FW_IMX228			"fimc_is_fw2_imx228.bin"
#define FIMC_IS_FW_IMX240		"fimc_is_fw2_imx240.bin"
#define FIMC_IS_FW_IMX260		"fimc_is_fw2_imx260.bin"
#define FIMC_IS_FW_2T2_EVT0				"fimc_is_fw2_2t2_evt0.bin"
#define FIMC_IS_FW_2T2_EVT1				"fimc_is_fw2_2t2_evt1.bin"
#define FIMC_IS_FW_2L1					"fimc_is_fw2_2l1.bin"
#define FIMC_IS_FW_COMPANION_EVT0				"companion_fw_evt0.bin"
#define FIMC_IS_FW_COMPANION_EVT1				"companion_fw_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_EVT1				"companion_fw_2p2_evt1.bin"
#define FIMC_IS_FW_COMPANION_2T2_EVT1				"companion_fw_2t2_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_12M_EVT1				"companion_fw_2p2_12m_evt1.bin"
#define FIMC_IS_FW_COMPANION_2L1					"companion_fw_2l1.bin"
#define FIMC_IS_FW_COMPANION_IMX260					"companion_fw_imx260.bin"
#define FIMC_IS_FW_COMPANION_IMX240_EVT1				"companion_fw_imx240_evt1.bin"
#define FIMC_IS_FW_COMPANION_IMX228_EVT1				"companion_fw_imx228_evt1.bin"

#define FIMC_IS_FW_COMPANION_IMX260_IMX320			"companion_fw_imx260.bin"

#define FIMC_IS_FW_COMPANION_2L2_3H1				"companion_fw_2l2.bin"
#define FIMC_IS_FW_COMPANION_2L2_IMX320				"companion_fw_2l2_imx320.bin"
#define FIMC_IS_FW_COMPANION_IMX333_IMX320			"companion_fw_imx333_imx320.bin"
#define FIMC_IS_FW_COMPANION_IMX333_3H1				"companion_fw_imx333_3h1.bin"

/*#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw.bin" */
#define FIMC_IS_IMX260_SETF			"setfile_imx260.bin"
#define FIMC_IS_IMX258_SETF			"setfile_imx258.bin"
#define FIMC_IS_IMX240_SETF			"setfile_imx240.bin"
#define FIMC_IS_IMX228_SETF			"setfile_imx228.bin"
#define FIMC_IS_IMX135_SETF			"setfile_imx135.bin"
#define FIMC_IS_IMX134_SETF			"setfile_imx134.bin"
#define FIMC_IS_4H5_SETF			"setfile_4h5.bin"
#define FIMC_IS_3M3_SETF			"setfile_3m3.bin"
#define FIMC_IS_3L2_SETF			"setfile_3l2.bin"
#define FIMC_IS_6B2_SETF			"setfile_6b2.bin"
#define FIMC_IS_8B1_SETF			"setfile_8b1.bin"
#define FIMC_IS_6D1_SETF			"setfile_6d1.bin"
#define FIMC_IS_2P2_SETF			"setfile_2p2.bin"
#define FIMC_IS_2P2_12M_SETF			"setfile_2p2_12m.bin"
#define FIMC_IS_2T2_SETF			"setfile_2t2.bin"
#define FIMC_IS_2P3_SETF			"setfile_2p3.bin"
#define FIMC_IS_3P3_SETF			"setfile_3p3.bin"
#define FIMC_IS_2L1_SETF			"setfile_2l1.bin"
#define FIMC_IS_2P6_SETF			"setfile_2p6.bin"
#define FIMC_IS_3P8SP_SETF			"setfile_3p8sp.bin"
#define FIMC_IS_SR846_SETF			"setfile_sr846_front.bin"
#define FIMC_IS_4H5YC_SETF			"setfile_4h5yc.bin"
#define FIMC_IS_4HA_SETF			"setfile_4ha.bin"
#define FIMC_IS_5E3_SETF			"setfile_5e3.bin"
#define FIMC_IS_SR556_SETF			"setfile_sr556.bin"

#define FIMC_IS_IMX333_SETF			"setfile_imx333.bin"
#define FIMC_IS_2L2_SETF			"setfile_2l2.bin"

#define FIMC_IS_IMX320_SETF			"setfile_imx320.bin"
#define FIMC_IS_3H1_SETF			"setfile_3h1.bin"
#define FIMC_IS_4E6_SETF			"setfile_4e6.bin"
#define FIMC_IS_IMX576_SETF			"setfile_imx576.bin"
#define FIMC_IS_IMX576_FRONT_SETF	"setfile_imx576_front.bin"

#define FIMC_IS_COMPANION_MASTER_SETF			"companion_master_setfile.bin"
#define FIMC_IS_COMPANION_MODE_SETF			"companion_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MASTER_SETF			"companion_2p2_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MODE_SETF			"companion_2p2_mode_setfile.bin"
#define FIMC_IS_COMPANION_2L1_MASTER_SETF			"companion_2l1_master_setfile.bin"
#define FIMC_IS_COMPANION_2L1_MODE_SETF				"companion_2l1_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX260_MASTER_SETF			"companion_imx260_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX260_MODE_SETF			"companion_imx260_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MASTER_SETF			"companion_imx240_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MODE_SETF			"companion_imx240_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX228_MASTER_SETF			"companion_imx228_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX228_MODE_SETF			"companion_imx228_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MASTER_SETF			"companion_2p2_12m_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MODE_SETF			"companion_2p2_12m_mode_setfile.bin"
#define FIMC_IS_COMPANION_2T2_MASTER_SETF			"companion_2t2_master_setfile.bin"
#define FIMC_IS_COMPANION_2T2_MODE_SETF			"companion_2t2_mode_setfile.bin"

#define FIMC_IS_COMPANION_2L2_MASTER_SETF			"companion_2l2_master_setfile.bin"
#define FIMC_IS_COMPANION_2L2_MODE_SETF				"companion_2l2_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX333_MASTER_SETF			"companion_imx333_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX333_MODE_SETF			"companion_imx333_mode_setfile.bin"

#define FIMC_IS_CAL_SDCARD_FRONT		"/data/cal_data_front.bin"
#define FIMC_IS_FW_FROM_SDCARD		"/data/media/0/CamFW_Main.bin"
#define FIMC_IS_SETFILE_FROM_SDCARD		"/data/media/0/CamSetfile_Main.bin"
#define FIMC_IS_COMPANION_FROM_SDCARD	"/data/media/0/CamFW_Companion.bin"
#define FIMC_IS_KEY_FROM_SDCARD		"/data/media/0/1q2w3e4r.key"

#define FIMC_IS_HEADER_VER_SIZE      11
#define FIMC_IS_OEM_VER_SIZE         11
#define FIMC_IS_AWB_VER_SIZE         11
#define FIMC_IS_SHADING_VER_SIZE     11
#define FIMC_IS_CAL_MAP_VER_SIZE     4
#define FIMC_IS_PROJECT_NAME_SIZE    8
#define FIMC_IS_ISP_SETFILE_VER_SIZE 6
#define FIMC_IS_SENSOR_ID_SIZE       16
#define FIMC_IS_CAL_DLL_VERSION_SIZE 4
#define FIMC_IS_MODULE_ID_SIZE       10
#define FIMC_IS_CROSSTALK_CAL_DATA_VER_SIZE 11
#if defined(USE_AP_PDAF)
#define FIMC_IS_AP_PDAF_VER_SIZE     11
#endif

#define FIMC_IS_RESOLUTION_DATA_SIZE 54

#define FROM_VERSION_V002 '2'
#define FROM_VERSION_V003 '3'
#define FROM_VERSION_V004 '4'
#define FROM_VERSION_V005 '5'

/* #define CONFIG_RELOAD_CAL_DATA 1 */

enum {
        CC_BIN1 = 0,
        CC_BIN2,
        CC_BIN3,
        CC_BIN4,
        CC_BIN5,
        CC_BIN6,
        CC_BIN7,
        CC_BIN_MAX,
};

struct fimc_is_from_info {
	u32		bin_start_addr;		/* DDK */
	u32		bin_end_addr;
#ifdef USE_RTA_BINARY
	u32		rta_bin_start_addr;	/* RTA */
	u32		rta_bin_end_addr;
#endif
	u32		oem_start_addr;
	u32		oem_end_addr;
	u32		awb_start_addr;
	u32		awb_end_addr;
	u32		shading_start_addr;
	u32		shading_end_addr;
#if defined(CAMERA_FRONT2)
	u32		awb2_start_addr;
	u32		awb2_end_addr;
	u32		shading2_start_addr;
	u32		shading2_end_addr;
#endif
	u32		crosstalk_cal_data_start_addr; /* Cross Talk for Tetra sensor (Remosaic sensor)*/
	u32		crosstalk_cal_data_end_addr; /* Cross Talk for Tetra sensor (Remosaic sensor)*/
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
	u32		oem2_start_addr;
	u32		oem2_end_addr;
	u32		awb2_start_addr;
	u32		awb2_end_addr;
	u32		shading2_start_addr;
	u32		shading2_end_addr;	
	u32		dual_data_start_addr;
	u32		dual_data_end_addr;
#endif
	u32		setfile_start_addr;
	u32		setfile_end_addr;
	u32		front_setfile_start_addr;
	u32		front_setfile_end_addr;
	u32		header_section_crc_addr;
	u32		cal_data_section_crc_addr;
	u32		cal_data_start_addr;
	u32		cal_data_end_addr;
	u32		cal2_data_section_crc_addr;
	u32		cal2_data_start_addr;
	u32		cal2_data_end_addr;
	u32		oem_section_crc_addr;
	u32		awb_section_crc_addr;
	u32		shading_section_crc_addr;
	u32		comp_shading_section_crc_addr;
	u32		paf_cal_section_crc_addr;
	u32		concord_cal_section_crc_addr;
	u32		af_cal_pan;
	u32		af_cal_macro;
	u32		af_cal_d1;
	u32		af_cal2_pan;		/* Rear Second Sensor (TELE) */
	u32		af_cal2_macro;		/* Rear Second Sensor (TELE) */
	u32		af_cal2_d1;		/* Rear Second Sensor (TELE) */
	u32		mtf_data_addr;
	u32		mtf_data2_addr; 	/*TELE*/
	char		header_ver[FIMC_IS_HEADER_VER_SIZE + 1];
	char		header2_ver[FIMC_IS_HEADER_VER_SIZE + 1]; /* Rear Second Sensor (TELE) or Front Second Sensor */
	char		cal_map_ver[FIMC_IS_CAL_MAP_VER_SIZE + 1];
	char		setfile_ver[FIMC_IS_SETFILE_VER_SIZE + 1];
	char		oem_ver[FIMC_IS_OEM_VER_SIZE + 1];
	char		awb_ver[FIMC_IS_AWB_VER_SIZE + 1];
	char		shading_ver[FIMC_IS_SHADING_VER_SIZE + 1];
	char		crosstalk_cal_data_ver[FIMC_IS_CROSSTALK_CAL_DATA_VER_SIZE + 1]; /* Cross Talk for Tetra sensor (Remosaic sensor)*/
	u32		crosstalk_cal_section_crc_addr; /* Cross Talk for Tetra sensor (Remosaic sensor) */
#if defined(CAMERA_FRONT2)
	char		awb2_ver[FIMC_IS_AWB_VER_SIZE + 1];
	char		shading2_ver[FIMC_IS_SHADING_VER_SIZE + 1];
	u32		awb2_section_crc_addr;
	u32		shading2_section_crc_addr;
#endif
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
	char		oem2_ver[FIMC_IS_OEM_VER_SIZE + 1];
	char		awb2_ver[FIMC_IS_AWB_VER_SIZE + 1];
	char		shading2_ver[FIMC_IS_SHADING_VER_SIZE + 1];
	char		dual_data_ver[FIMC_IS_HEADER_VER_SIZE + 1];
	u32		oem2_section_crc_addr;
	u32		awb2_section_crc_addr;
	u32		shading2_section_crc_addr;
	u32		dual_data_section_crc_addr;
#else
#if defined(EEP_DUAL_DATA_VER_START_ADDR)
	char		dual_data_ver[FIMC_IS_HEADER_VER_SIZE + 1];
	u32		dual_data_section_crc_addr;
	u32		dual_data_start_addr;
	u32		dual_data_end_addr;
#endif
#endif
	char		load_fw_name[50]; 		/* DDK */
#ifdef USE_RTA_BINARY
	char		load_rta_fw_name[50]; 	/* RTA */
#endif

	char		load_setfile_name[50];
	char		load_front_setfile_name[50];
	char		project_name[FIMC_IS_PROJECT_NAME_SIZE + 1];
	char		from_sensor_id[FIMC_IS_SENSOR_ID_SIZE + 1];
	char		from_sensor2_id[FIMC_IS_SENSOR_ID_SIZE + 1]; /*REAR 2 Sensor ID or Front Second Sensor*/
	u8		from_module_id[FIMC_IS_MODULE_ID_SIZE + 1];
	u8		eeprom_front_module_id[FIMC_IS_MODULE_ID_SIZE + 1];
	bool		is_caldata_read;
	bool		is_check_cal_reload;
	u8		sensor_version;
	unsigned long		fw_size;
	unsigned long		setfile_size;
	unsigned long		front_setfile_size;

#if defined(USE_AP_PDAF)
	u32		ap_pdaf_section_crc_addr;
	u32		ap_pdaf_start_addr;
	u32		ap_pdaf_end_addr;
	char		ap_pdaf_ver[FIMC_IS_AP_PDAF_VER_SIZE + 1];
#endif
};

bool fimc_is_sec_get_force_caldata_dump(void);
int fimc_is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos);
bool fimc_is_sec_file_exist(char *name);

int fimc_is_sec_get_max_cal_size(int position);
int fimc_is_sec_get_sysfs_finfo_by_position(int position, struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo);
int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo);
int fimc_is_sec_get_front_cal_buf(char **buf);

int fimc_is_sec_get_cal_buf(int position, char **buf);
int fimc_is_sec_get_loaded_fw(char **buf);
int fimc_is_sec_set_loaded_fw(char *buf);
int fimc_is_sec_get_loaded_c1_fw(char **buf);
int fimc_is_sec_set_loaded_c1_fw(char *buf);

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name);
int fimc_is_sec_get_camid(void);
int fimc_is_sec_set_camid(int id);
int fimc_is_sec_get_pixel_size(char *header_ver);
int fimc_is_sec_fw_find(struct fimc_is_core *core, int position);
int fimc_is_sec_run_fw_sel(struct device *dev, int position);

int fimc_is_sec_readfw(struct fimc_is_core *core);
int fimc_is_sec_read_setfile(struct fimc_is_core *core);
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int fimc_is_sec_inflate_fw(u8 **buf, unsigned long *size);
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) \
    || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly);
#endif
int fimc_is_sec_write_fw(struct fimc_is_core *core, struct device *dev);
#if 0 //not used for mid-tier //!defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_readcal(struct fimc_is_core *core);
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly);
#endif
int fimc_is_sec_fw_revision(char *fw_ver);
int fimc_is_sec_fw_revision(char *fw_ver);
bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);
u8 fimc_is_sec_compare_ver(int position);
bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position);

bool fimc_is_sec_check_fw_crc32(char *buf, u32 checksum_seed, unsigned long size);
bool fimc_is_sec_check_cal_crc32(char *buf, int id);
void fimc_is_sec_make_crc32_table(u32 *table, u32 id);

int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on);
int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver);
int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on);
int fimc_is_sec_rom_power_on(struct fimc_is_core *core, int position);
int fimc_is_sec_rom_power_off(struct fimc_is_core *core, int position);
void remove_dump_fw_file(void);
#endif /* FIMC_IS_SEC_DEFINE_H */
