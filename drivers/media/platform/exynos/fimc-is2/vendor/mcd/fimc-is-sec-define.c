/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-sec-define.h"
#include "fimc-is-vender-specific.h"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)\
    || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#include <linux/i2c.h>
#include "fimc-is-device-eeprom.h"
#endif

bool crc32_fw_check = true;
bool crc32_setfile_check = true;
bool crc32_front_setfile_check = true;
bool crc32_check = true;
bool crc32_check_factory = true;
bool crc32_header_check = true;
bool crc32_header_check_front = true;
bool crc32_check_factory_front = true;
bool crc32_check_factory_front2 = true;

bool fw_version_crc_check = true;
bool is_latest_cam_module = false;
bool is_latest_cam_module_front = false;
bool is_latest_cam_module_front2 = false;
bool is_final_cam_module = false;
#if defined(CONFIG_SOC_EXYNOS5433)
bool is_right_prj_name = true;
#endif

bool crc32_check_rear2 = true;			/*REAR2 CAL DATA -TELE-*/
bool crc32_check_factory_rear2 = true;	/*REAR2 CAL DATA -TELE-*/
#define FIMC_IS_DEFAULT_CAL_SIZE	(20 * 1024)
#define FIMC_IS_DUMP_CAL_SIZE	(172 * 1024)
#define FIMC_IS_LATEST_FROM_VERSION_M	'M'

//static bool is_caldata_read = false;
//static bool is_c1_caldata_read = false;
bool force_caldata_dump = false;

static int cam_id = CAMERA_SINGLE_REAR;
bool is_dumped_fw_loading_needed = false;
bool is_dumped_c1_fw_loading_needed = false;
char fw_core_version;
//struct class *camera_class;
//struct device *camera_front_dev; /*sys/class/camera/front*/
//struct device *camera_rear_dev; /*sys/class/camera/rear*/
static struct fimc_is_from_info sysfs_finfo;
static struct fimc_is_from_info sysfs_pinfo;
bool crc32_check_front = true;
bool crc32_check_front2 = true;			/* FRONT2 CAL DATA */
bool is_final_cam_module_front = false;
bool is_final_cam_module_front2 = false;
static struct fimc_is_from_info sysfs_finfo_front;
static struct fimc_is_from_info sysfs_pinfo_front;

/* Rear 2 */
bool crc32_header_check_rear2;
bool crc32_setfile_check_rear2;
bool is_latest_cam_module_rear2;
bool is_final_cam_module_rear2;
#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
static char cal_buf_rear2[FIMC_IS_MAX_CAL_SIZE_REAR2];
static struct fimc_is_from_info sysfs_finfo_rear2;
//static struct fimc_is_from_info sysfs_pinfo_rear2;
#endif

/* Rear 3 */
bool crc32_check_factory_rear3 = true;
bool crc32_check_rear3 = true;
bool crc32_header_check_rear3 = true;
bool crc32_setfile_check_rear3 = true;
bool is_latest_cam_module_rear3 = false;
bool is_final_cam_module_rear3 = false;
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR3
static struct fimc_is_from_info sysfs_finfo_rear3;
//static struct fimc_is_from_info sysfs_pinfo_rear3;
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
static char cal_buf_front[FIMC_IS_MAX_CAL_SIZE_FRONT];
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
static char cal_buf[FIMC_IS_MAX_CAL_SIZE];
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR3
static char cal_buf_rear3[FIMC_IS_MAX_CAL_SIZE_REAR3];
#endif
#ifdef CAMERA_MODULE_DUALIZE
static char fw_buf[FIMC_IS_MAX_FW_BUFFER_SIZE];
#endif
char loaded_fw[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
char loaded_companion_fw[30] = {0, };

bool fimc_is_sec_get_force_caldata_dump(void)
{
	return force_caldata_dump;
}

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		info("forced caldata dump enabled!!\n");
	return 0;
}

int fimc_is_sec_get_max_cal_size(int position)
{
	int size = 0;

	switch (position) {
	case SENSOR_POSITION_REAR:
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR
		size = FIMC_IS_MAX_CAL_SIZE;
#endif
		break;
	case SENSOR_POSITION_REAR2:
#if defined(FIMC_IS_MAX_CAL_SIZE_REAR2)
		size = FIMC_IS_MAX_CAL_SIZE_REAR2;
#elif defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) && defined(CAMERA_REAR2)
		size = FIMC_IS_MAX_CAL_SIZE;
#endif
		break;
	case SENSOR_POSITION_REAR3:
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR3
		size = FIMC_IS_MAX_CAL_SIZE_REAR3;
#endif
		break;
	case SENSOR_POSITION_FRONT:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		size = FIMC_IS_MAX_CAL_SIZE_FRONT;
#endif
		break;
	case SENSOR_POSITION_FRONT2:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) && defined(CAMERA_FRONT2)
		size = FIMC_IS_MAX_CAL_SIZE_FRONT;
#endif
		break;
	default:
		err("Invalid postion %d. Check the position", position);
		break;
	}

	if (!size) {
		err("Cal size is 0 (postion %d). Check cal size", position);
		/* WARN(true, "Cal size is 0\n"); */
	}

	return size;
}

int fimc_is_sec_get_sysfs_finfo_by_position(int position, struct fimc_is_from_info **finfo)
{
	*finfo = NULL;

	switch (position){
	case SENSOR_POSITION_REAR:
		*finfo = &sysfs_finfo; /* default */
		break;
	case SENSOR_POSITION_REAR2:
#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
		*finfo = &sysfs_finfo_rear2;
#elif defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) && defined(CAMERA_REAR2)
		*finfo = &sysfs_finfo;
#else
		/* Temp: get rear finfo until enable rear2 otprom. */
		*finfo = &sysfs_finfo;
#endif
		break;
	case SENSOR_POSITION_REAR3:
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR3
		*finfo = &sysfs_finfo_rear3;
#endif
		break;
	case SENSOR_POSITION_FRONT:
		*finfo = &sysfs_finfo_front;
		break;
	case SENSOR_POSITION_FRONT2:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) && defined(CAMERA_FRONT2)
		*finfo = &sysfs_finfo_front;
#endif
		break;
	default:
		err("Invalid postion %d. Check the position", position);
		break;
	}

	if (*finfo == NULL) {
		err("finfo addr is null. postion %d", position);
		/*WARN(true, "finfo is null\n");*/
		return -EINVAL;
	}
	
	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo)
{
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, finfo);
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo;
	return 0;
}

int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo)
{
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_FRONT, finfo);
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo_front;
	return 0;
}

int fimc_is_sec_get_cal_buf(int position, char **buf)
{
	*buf = NULL;

	switch (position) {
	case SENSOR_POSITION_REAR:
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR
		*buf = &cal_buf[0];
#endif
		break;
	case SENSOR_POSITION_REAR2:
#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
		*buf = &cal_buf_rear2[0];
#elif defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) && defined(CAMERA_REAR2)
		*buf = &cal_buf[0];
#endif
		break;
	case SENSOR_POSITION_REAR3:
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_REAR3
		*buf = &cal_buf_rear3[0];
#endif
		break;
	case SENSOR_POSITION_FRONT:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		*buf = &cal_buf_front[0];
#endif
		break;
	case SENSOR_POSITION_FRONT2:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) && defined(CAMERA_FRONT2)
		*buf = &cal_buf_front[0];
#endif
		break;
	default:
		err("Invalid postion %d. Check the position", position);
		break;
	}

	if (*buf == NULL) {
		err("cal buf is null. postion %d", position);
		/* WARN(true, "cal buf is null\n"); */
		return -EINVAL;
	}

	return 0;
}

int fimc_is_sec_get_front_cal_buf(char **buf)
{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	fimc_is_sec_get_cal_buf(SENSOR_POSITION_FRONT, buf);
#else
	*buf = NULL;
#endif
	return 0;
}

int fimc_is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_fw(char *buf)
{
	strncpy(loaded_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_get_loaded_c1_fw(char **buf)
{
	*buf = &loaded_companion_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_c1_fw(char *buf)
{
	strncpy(loaded_companion_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int fimc_is_sec_get_camid(void)
{
	return cam_id;
}

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name)
{
#if 0
	char buf[1];
	loff_t pos = 0;
	int pixelSize;

	read_data_from_file("/data/CameraID.txt", buf, 1, &pos);
	if (buf[0] == '0')
		cam_id = CAMERA_SINGLE_REAR;
	else if (buf[0] == '1')
		cam_id = CAMERA_SINGLE_FRONT;
	else if (buf[0] == '2')
		cam_id = CAMERA_DUAL_REAR;
	else if (buf[0] == '3')
		cam_id = CAMERA_DUAL_FRONT;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		}
	}

	if (cam_id == CAMERA_SINGLE_FRONT ||
		cam_id == CAMERA_DUAL_FRONT) {
		snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
	}
#else
	err("%s: waring, you're calling the disabled func!", __func__);
#endif
	return 0;
}

int fimc_is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER]
		|| fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE]
		|| fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1]
		|| fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]
		|| fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

u8 fimc_is_sec_compare_ver(int position)
{
	u32 from_ver = 0, def_ver = 0, def_ver2 = 0;
	u8 ret = 0;
	char ver[3] = {'V', '0', '0'};
	char ver2[3] ={'V', 'F', '0'};
	struct fimc_is_from_info *finfo = NULL;

	if (fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo)) {
		err("failed get finfo. plz check position %d", position);
		return 0;
	}
	def_ver = ver[0] << 16 | ver[1] << 8 | ver[2];
	def_ver2 = ver2[0] << 16 | ver2[1] << 8 | ver2[2];
	from_ver = finfo->cal_map_ver[0] << 16 | finfo->cal_map_ver[1] << 8 | finfo->cal_map_ver[2];
	if ((from_ver == def_ver) || (from_ver == def_ver2)) {
		return finfo->cal_map_ver[3];
	} else {
		err("FROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		return 0;
	}

	return ret;
}

bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position)
{
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	char compare_version;
	u8 from_ver;
	u8 latest_from_ver;

	if (specific->skip_cal_loading) {
		err("skip_cal_loading implemented");
		return false;
	}

	if (fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo)) {
		err("failed get finfo. plz check position %d", position);
		return false;
	}

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
#if defined(CAL_MAP_ES_VERSION_FRONT) && defined(CAMERA_MODULE_ES_VERSION_FRONT)
		latest_from_ver = CAL_MAP_ES_VERSION_FRONT;
		compare_version = CAMERA_MODULE_ES_VERSION_FRONT;
#endif
	} else if (position == SENSOR_POSITION_REAR3) {
#if defined(CAL_MAP_ES_VERSION_REAR3) && defined(CAMERA_MODULE_ES_VERSION_REAR3)
		latest_from_ver = CAL_MAP_ES_VERSION_REAR3;
		compare_version = CAMERA_MODULE_ES_VERSION_REAR3;
#endif
	} else if (position == SENSOR_POSITION_REAR2) {
#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
	latest_from_ver = CAL_MAP_ES_VERSION_REAR2;
	compare_version = CAMERA_MODULE_ES_VERSION_REAR2;
#endif
	} else {
#if defined(CAL_MAP_ES_VERSION_REAR) && defined(CAMERA_MODULE_ES_VERSION_REAR)
		latest_from_ver = CAL_MAP_ES_VERSION_REAR;
		compare_version = CAMERA_MODULE_ES_VERSION_REAR;
#endif
	}

	from_ver = fimc_is_sec_compare_ver(position);

	if ((from_ver < latest_from_ver) ||
		(finfo->header_ver[10] < compare_version)) {
		err("invalid from version. from_ver %c, header_ver[10] %c", from_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
bool fimc_is_sec_check_front_cal_crc32(char *buf, int position)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_header_temp;
	bool crc32_oem_check, crc32_awb_check, crc32_shading_check;
#if defined(CAMERA_FRONT2)
	bool crc32_crosstalk_cal_data_check, crc32_awb2_check, crc32_shading2_check;
#endif
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_companion_retention *ret_data;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	ret_data = &specific->retention_data;
	buf32 = (u32 *)buf;

	info("+++ %s +++\n", __func__);

#if defined(SKIP_CHECK_CRC)
	pr_warning("%s: front skip to check crc\n", __func__);
	crc32_check_front = true;
	crc32_check_front2 = true;
	crc32_header_check_front = true;
	return crc32_check_front && crc32_check_front2;
#endif /* SKIP_CHECK_CRC */

	crc32_temp = true;
	crc32_check_front2 = true;
	address_boundary = fimc_is_sec_get_max_cal_size(position);

	/* Header data */
	check_base = 0;
	checksum = 0;
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	check_length = HEADER_CRC32_LEN_FRONT;

	checksum_base = finfo->header_section_crc_addr / 4;
	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_check_front2 = false;
		crc32_header_temp = false;
		goto out;
	} else {
		crc32_header_temp = true;
	}

	/* OEM */
	crc32_oem_check = false;
	check_length = 0;

#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
	crc32_oem_check = true;
#if defined(OEM_CRC32_LEN_FRONT)
	check_length = OEM_CRC32_LEN_FRONT;
#else
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
#endif /* OEM_CRC32_LEN_FRONT */
#endif /* EEP_HEADER_OEM_START_ADDR_FRONT */

	if (crc32_oem_check) {
#if defined(EEP_CHECKSUM_OEM_BASE_ADDR_FRONT)
		if(finfo->oem_start_addr != EEP_CHECKSUM_OEM_BASE_ADDR_FRONT)
			check_base = EEP_CHECKSUM_OEM_BASE_ADDR_FRONT / 4;
		else
#endif
			check_base = finfo->oem_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->oem_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: OEM address has error: start(0x%08X), end(0x%08X)",
				finfo->oem_start_addr, finfo->oem_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: front skip to check oem crc32\n", __func__);
	}

	/* AWB */
	crc32_awb_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AWB_START_ADDR_FRONT)
	crc32_awb_check = true;
#if defined(AWB_CRC32_LEN_FRONT)
	check_length = AWB_CRC32_LEN_FRONT;
#else
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;
#endif /* AWB_CRC32_LEN_FRONT */
#endif /* EEP_HEADER_AWB_START_ADDR_FRONT */

	if (crc32_awb_check) {
		check_base = finfo->awb_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->awb_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: AWB address has error: start(0x%08X), end(0x%08X)",
				finfo->awb_start_addr, finfo->awb_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: front skip to check awb crc32\n", __func__);
	}

	/* Shading */
	crc32_shading_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AP_SHADING_START_ADDR_FRONT)
	crc32_shading_check = true;
#if defined(SHADING_CRC32_LEN_FRONT)
	check_length = SHADING_CRC32_LEN_FRONT;
#else
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;
#endif /* SHADING_CRC32_LEN_FRONT */
#endif /* EEP_HEADER_AP_SHADING_START_ADDR_FRONT */

	if(crc32_shading_check) {
		check_base = finfo->shading_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->shading_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Shading address has error: start(0x%08X), end(0x%08X)",
				finfo->shading_start_addr, finfo->shading_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: front skip to check shading crc32\n", __func__);
	}

out:
#if defined(CAMERA_FRONT2)
	/* CROSSTALK CAL DATA */
	crc32_crosstalk_cal_data_check = false;
	check_length = 0;

#if defined(EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT)
	crc32_crosstalk_cal_data_check = true;
#if defined(CROSSTALK_CRC32_LEN_FRONT)
	check_length = CROSSTALK_CRC32_LEN_FRONT;
#else
	check_length = (finfo->crosstalk_cal_data_end_addr - finfo->crosstalk_cal_data_start_addr + 1) ;
#endif /* CROSSTALK_CRC32_LEN_FRONT */
#endif /* EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT */

	if(crc32_crosstalk_cal_data_check) {
		check_base = finfo->crosstalk_cal_data_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->crosstalk_cal_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Crosstalk cal data address has error: start(0x%08X), end(0x%08X)",
				finfo->crosstalk_cal_data_start_addr, finfo->crosstalk_cal_data_end_addr);
			crc32_temp = false;
			goto out2;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Crosstalk cal data (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out2;
		}
	} else {
		pr_warning("%s: front skip to check Crosstalk cal data crc32\n", __func__);
	}

	/* AWB2 */
	crc32_awb2_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AWB_START_ADDR_FRONT2)
	crc32_awb2_check = true;
#if defined(AWB_CRC32_LEN_FRONT2)
	check_length = AWB_CRC32_LEN_FRONT2;
#else
	check_length = (finfo->awb2_end_addr - finfo->awb2_start_addr + 1) ;
#endif /* AWB_CRC32_LEN_FRONT2 */
#endif /* EEP_HEADER_AWB_START_ADDR_FRONT2 */

	if (crc32_awb2_check) {
		check_base = finfo->awb2_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->awb2_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: AWB2 address has error: start(0x%08X), end(0x%08X)",
				finfo->awb2_start_addr, finfo->awb2_end_addr);
			crc32_check_front2 = false;
			goto out2;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the AWB2 (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_check_front2 = false;
			goto out2;
		}
	} else {
		pr_warning("%s: front skip to check awb2 crc32\n", __func__);
	}

	/* Shading2 */
	crc32_shading2_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AP_SHADING_START_ADDR_FRONT2)
	crc32_shading2_check = true;
#if defined(SHADING_CRC32_LEN_FRONT2)
	check_length = SHADING_CRC32_LEN_FRONT2;
#else
	check_length = (finfo->shading2_end_addr - finfo->shading2_start_addr + 1) ;
#endif /* SHADING_CRC32_LEN_FRONT2 */
#endif /* EEP_HEADER_AP_SHADING_START_ADDR_FRONT2 */

	if(crc32_shading2_check) {
		check_base = finfo->shading2_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->shading2_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Shading2 address has error: start(0x%08X), end(0x%08X)",
				finfo->shading2_start_addr, finfo->shading2_end_addr);
			crc32_check_front2 = false;
			goto out2;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Shading2 (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_check_front2 = false;
			goto out2;
		}
	} else {
		pr_warning("%s: front skip to check shading2 crc32\n", __func__);
	}
out2:
#endif
	crc32_check_front = crc32_temp;
	crc32_header_check_front = crc32_header_temp;
	info("[%s] crc32_check_front %d crc32_check_front2 %d crc32_header_check_front %d\n", __func__, crc32_check_front, crc32_check_front2, crc32_header_check_front);
	return crc32_check_front && crc32_check_front2;
}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
bool fimc_is_sec_check_rear_cal_crc32(char *buf, int position)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_temp2, crc32_header_temp;
	bool crc32_oem_check, crc32_awb_check, crc32_shading_check;
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
	bool crc32_oem2_check, crc32_awb2_check, crc32_shading2_check, crc32_dualdata_check;
#endif
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_companion_retention *ret_data;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	ret_data = &specific->retention_data;
	buf32 = (u32 *)buf;

	info("+++ %s\n", __func__);

	/***** Variable Initial *****/
	crc32_temp = true;
	crc32_temp2 = true;
	crc32_check = true;
	crc32_check_rear2 = true;

	/***** SKIP CHECK CRC *****/
#if defined(SKIP_CHECK_CRC)
	pr_warning("%s: rear & rear2 skip to check crc\n", __func__);

	return crc32_check && crc32_check_rear2;
#endif

	/***** START CHECK CRC *****/

	address_boundary = fimc_is_sec_get_max_cal_size(position);

	/* HEADER DATA CRC CHECK */
	check_base = 0;
	checksum = 0;
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	check_length = HEADER_CRC32_LEN;

	checksum_base = finfo->header_section_crc_addr / 4;
	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_temp2 = false;
		crc32_header_temp = false;
		goto out;
	} else {
		crc32_header_temp = true;
	}

	/* MAIN OEM DATA CRC CHECK */
	crc32_oem_check = false;
	check_length = 0;

#if defined(EEP_HEADER_OEM_START_ADDR)
	crc32_oem_check = true;
#if defined(OEM_CRC32_LEN)
	check_length = OEM_CRC32_LEN;
#else
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
#endif
#endif

	if (crc32_oem_check) {
		check_base = finfo->oem_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->oem_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: OEM address has error: start(0x%08X), end(0x%08X)",
				finfo->oem_start_addr, finfo->oem_end_addr);
			crc32_temp = false;
			goto rear2_check;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto rear2_check;
		}
	} else {
		pr_warning("%s: rear skip to check oem crc32\n", __func__);
	}

	/* MAIN AWB DATA CRC CHECK */
	crc32_awb_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AWB_START_ADDR)
	crc32_awb_check = true;
#if defined(AWB_CRC32_LEN)
	check_length = AWB_CRC32_LEN;
#else
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;
#endif
#endif

	if (crc32_awb_check) {
		check_base = finfo->awb_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->awb_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: AWB address has error: start(0x%08X), end(0x%08X)",
				finfo->awb_start_addr, finfo->awb_end_addr);
			crc32_temp = false;
			goto rear2_check;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto rear2_check;
		}
	} else {
		pr_warning("%s: rear skip to check awb crc32\n", __func__);
	}

	/* MAIN SHADING DATA CRC CHECK*/
	crc32_shading_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AP_SHADING_START_ADDR)
	crc32_shading_check = true;
#if defined(SHADING_CRC32_LEN)
	check_length = SHADING_CRC32_LEN;
#else
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;
#endif
#endif

	if (crc32_shading_check) {
		check_base = finfo->shading_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->shading_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Shading address has error: start(0x%08X), end(0x%08X)",
				finfo->shading_start_addr, finfo->shading_end_addr);
			crc32_temp = false;
			goto rear2_check;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto rear2_check;
		}
	} else {
		pr_warning("%s: rear skip to check shading crc32\n", __func__);
	}

#if defined(EEP_HEADER_AP_PDAF_START_ADDR)
	/* MAIN PDAF CAL DATA CRC CHECK */
	check_base = finfo->ap_pdaf_start_addr / 4;
	checksum = 0;
	check_length = (finfo->ap_pdaf_end_addr - finfo->ap_pdaf_start_addr + 1);
	checksum_base = finfo->ap_pdaf_section_crc_addr / 4;

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: pdaf address has error: start(0x%08X), end(0x%08X)",
			finfo->ap_pdaf_start_addr, finfo->ap_pdaf_end_addr);
		crc32_temp = false;
		goto rear2_check;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the pdaf cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto rear2_check;
	}
#endif


rear2_check:
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
	/* SUB OEM DATA CRC CHECK */
	crc32_oem2_check = false;
	check_length = 0;

#if defined(EEP_HEADER_OEM_START_ADDR_REAR2)
	crc32_oem2_check = true;
#if defined(OEM_CRC32_LEN_REAR2)
	check_length = OEM_CRC32_LEN_REAR2;
#else
	check_length = (finfo->oem2_end_addr - finfo->oem2_start_addr + 1);
#endif
#endif

	if (crc32_oem2_check) {
		check_base = finfo->oem2_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->oem2_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: OEM2 address has error: start(0x%08X), end(0x%08X)",
				finfo->oem2_start_addr, finfo->oem2_end_addr);
			crc32_temp2 = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OEM2 (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp2 = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear sub skip to check oem crc32\n", __func__);
	}

	/* SUB AWB DATA CRC CHECK */
	crc32_awb2_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AWB_START_ADDR_REAR2)
	crc32_awb2_check = true;
#if defined(AWB_CRC32_LEN_REAR2)
	check_length = AWB_CRC32_LEN_REAR2;
#else
	check_length = (finfo->awb2_end_addr - finfo->awb2_start_addr + 1) ;
#endif
#endif

	if (crc32_awb2_check) {
		check_base = finfo->awb2_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->awb2_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: AWB2 address has error: start(0x%08X), end(0x%08X)",
				finfo->awb2_start_addr, finfo->awb2_end_addr);
			crc32_temp2 = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the AWB2 (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp2 = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear sub skip to check awb crc32\n", __func__);
	}

	/* SUB SHADING DATA CRC CHECK*/
	crc32_shading2_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AP_SHADING_START_ADDR_REAR2)
	crc32_shading2_check = true;
#if defined(SHADING_CRC32_LEN_REAR2)
	check_length = SHADING_CRC32_LEN_REAR2;
#else
	check_length = (finfo->shading2_end_addr - finfo->shading2_start_addr + 1) ;
#endif
#endif

	if (crc32_shading2_check) {
		check_base = finfo->shading2_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->shading2_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Shading2 address has error: start(0x%08X), end(0x%08X)",
				finfo->shading2_start_addr, finfo->shading2_end_addr);
			crc32_temp2 = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Shading2 (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp2 = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear sub skip to check shading crc32\n", __func__);
	}

	/* DUAL DATA CRC CHECK*/
	crc32_dualdata_check = false;
	check_length = 0;

#if defined(EEP_HEADER_DUAL_DATA_START_ADDR)
	crc32_dualdata_check = true;
#if defined(DUAL_DATA_CRC32_LEN)
	check_length = DUAL_DATA_CRC32_LEN;
#else
	check_length = (finfo->dual_data_end_addr - finfo->dual_data_start_addr + 1) ;
#endif
#endif

	if (crc32_dualdata_check) {
		check_base = finfo->dual_data_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->dual_data_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: DualData address has error: start(0x%08X), end(0x%08X)",
				finfo->dual_data_start_addr, finfo->dual_data_end_addr);
			crc32_temp2 = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the DualData (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp2 = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear sub skip to check DualData crc32\n", __func__);
	}

#endif /* defined(CAMERA_REAR2) */

out:
	crc32_check = crc32_temp;
	crc32_check_rear2 = crc32_temp2;
	crc32_header_check = crc32_header_temp;
	info("crc32_header_check=%d, crc32_check=%d, crc32_check_rear2=%d\n", crc32_header_check, crc32_check, crc32_check_rear2);

	return crc32_check && crc32_check_rear2;
}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
bool fimc_is_sec_check_rear3_cal_crc32(char *buf, int position)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_temp2, crc32_header_temp;
	bool crc32_oem_check, crc32_awb_check, crc32_shading_check;
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_companion_retention *ret_data;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	ret_data = &specific->retention_data;
	buf32 = (u32 *)buf;

	info("+++ %s\n", __func__);

	/***** Variable Initial *****/
	crc32_temp = true;
	crc32_temp2 = true;
	crc32_check = true;
	crc32_check_rear2 = true;

	/***** SKIP CHECK CRC *****/
#if defined(SKIP_CHECK_CRC)
	pr_warning("%s: rear & rear2 skip to check crc\n", __func__);
	return crc32_check && crc32_check_rear2;
#endif

	/***** START CHECK CRC *****/

	address_boundary = fimc_is_sec_get_max_cal_size(position);

	/* HEADER DATA CRC CHECK */
	check_base = 0;
	checksum = 0;
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	check_length = HEADER_CRC32_LEN_REAR3;

	checksum_base = finfo->header_section_crc_addr / 4;
	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_temp2 = false;
		crc32_header_temp = false;
		goto out;
	} else {
		crc32_header_temp = true;
	}

	/* MAIN OEM DATA CRC CHECK */
	crc32_oem_check = false;
	check_length = 0;

#if defined(EEP_HEADER_OEM_START_ADDR_REAR3)
	crc32_oem_check = true;
#if defined(OEM_CRC32_LEN_REAR3)
	check_length = OEM_CRC32_LEN_REAR3;
#else
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
#endif
#endif

	if (crc32_oem_check) {
		check_base = finfo->oem_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->oem_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: OEM address has error: start(0x%08X), end(0x%08X)",
				finfo->oem_start_addr, finfo->oem_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear skip to check oem crc32\n", __func__);
	}

	/* MAIN AWB DATA CRC CHECK */
	crc32_awb_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AWB_START_ADDR_REAR3)
	crc32_awb_check = true;
#if defined(AWB_CRC32_LEN_REAR3)
	check_length = AWB_CRC32_LEN_REAR3;
#else
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;
#endif
#endif

	if (crc32_awb_check) {
		check_base = finfo->awb_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->awb_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: AWB address has error: start(0x%08X), end(0x%08X)",
				finfo->awb_start_addr, finfo->awb_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear skip to check awb crc32\n", __func__);
	}

	/* MAIN SHADING DATA CRC CHECK*/
	crc32_shading_check = false;
	check_length = 0;

#if defined(EEP_HEADER_AP_SHADING_START_ADDR_REAR3)
	crc32_shading_check = true;
#if defined(SHADING_CRC32_LEN_REAR3)
	check_length = SHADING_CRC32_LEN_REAR3;
#else
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;
#endif
#endif

	if (crc32_shading_check) {
		check_base = finfo->shading_start_addr / 4;
		checksum = 0;
		checksum_base = finfo->shading_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: Shading address has error: start(0x%08X), end(0x%08X)",
				finfo->shading_start_addr, finfo->shading_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	} else {
		pr_warning("%s: rear skip to check shading crc32\n", __func__);
	}

#if defined(EEP_HEADER_AP_PDAF_START_ADDR_REAR3)
	/* MAIN PDAF CAL DATA CRC CHECK */
	check_base = finfo->ap_pdaf_start_addr / 4;
	checksum = 0;
	check_length = (finfo->ap_pdaf_end_addr - finfo->ap_pdaf_start_addr + 1);
	checksum_base = finfo->ap_pdaf_section_crc_addr / 4;

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: pdaf address has error: start(0x%08X), end(0x%08X)",
			finfo->ap_pdaf_start_addr, finfo->ap_pdaf_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the pdaf cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}
#endif

out:
	crc32_check = crc32_temp;
	crc32_check_rear2 = crc32_temp2;
	crc32_header_check = crc32_header_temp;
	info("crc32_header_check=%d, crc32_check=%d, crc32_check_rear2=%d\n", crc32_header_check, crc32_check, crc32_check_rear2);

	return crc32_check && crc32_check_rear2;
}
#endif


bool fimc_is_sec_check_cal_crc32(char *buf, int id)
{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if(id == SENSOR_POSITION_FRONT || id == SENSOR_POSITION_FRONT2)
		return fimc_is_sec_check_front_cal_crc32(buf, id);
	else
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
	if (id == SENSOR_POSITION_REAR3)
		return fimc_is_sec_check_rear3_cal_crc32(buf, id);
	else
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		return fimc_is_sec_check_rear_cal_crc32(buf, id);
#endif

	return false;
}

#ifdef CAMERA_MODULE_DUALIZE
bool fimc_is_sec_check_fw_crc32(char *buf, u32 checksum_seed, unsigned long size)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 FW\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_fw_check = false;
	} else {
		crc32_fw_check = true;
	}

	info("Camera: End checking CRC32 FW\n");

	return crc32_fw_check;
}
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
bool fimc_is_sec_check_front_otp_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	bool crc32_temp, crc32_header_temp;
	u32 checksumFromOTP;

	buf32 = (u32 *)buf;
	checksumFromOTP = buf[OTP_CHECKSUM_HEADER_ADDR_FRONT] +( buf[OTP_CHECKSUM_HEADER_ADDR_FRONT+1] << 8)
			+( buf[OTP_CHECKSUM_HEADER_ADDR_FRONT+2] << 16) + (buf[OTP_CHECKSUM_HEADER_ADDR_FRONT+3] << 24);

	/* Header data */
	checksum = (u32)getCRC((u16 *)&buf32[HEADER_START_ADDR_FRONT], HEADER_CRC32_LEN_FRONT, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the header data section (0x%08X != 0x%08X)",
					checksum, checksumFromOTP);
	} else {
		crc32_temp = crc32_header_temp = true;
		pr_info("Camera: End checking CRC32 (0x%08X = 0x%08X)",
					checksum, checksumFromOTP);
	}

	crc32_check_front = crc32_temp;
	crc32_header_check_front = crc32_header_temp;
	return crc32_check_front;
}
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
bool fimc_is_sec_check_otp_crc32(char *buf,int position)
{
	u32 *buf32 = NULL;
	u32 checksum;
	bool crc32_temp, crc32_header_temp;
	u32 checksumFromOTP;

	buf32 = (u32 *)buf;
	checksumFromOTP = buf[OTP_CHECKSUM_HEADER_ADDR] +( buf[OTP_CHECKSUM_HEADER_ADDR+1] << 8)
			+( buf[OTP_CHECKSUM_HEADER_ADDR+2] << 16) + (buf[OTP_CHECKSUM_HEADER_ADDR+3] << 24);

	/* Header data */
	checksum = (u32)getCRC((u16 *)&buf32[HEADER_START_ADDR], HEADER_CRC32_LEN_OTP, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the header data section (0x%08X != 0x%08X)",
				checksum, checksumFromOTP);
		goto out;
	} else {
		crc32_temp = crc32_header_temp = true;
		pr_info("Camera: End checking CRC32 (0x%08X = 0x%08X)",
				checksum, checksumFromOTP);
	}
#if defined(OTP_CHECKSUM_OEM_ADDR)
	buf32 = (u32 *)buf;
	checksumFromOTP = buf[OTP_CHECKSUM_OEM_ADDR] +( buf[OTP_CHECKSUM_OEM_ADDR+1] << 8)
		+( buf[OTP_CHECKSUM_OEM_ADDR+2] << 16) + (buf[OTP_CHECKSUM_OEM_ADDR+3] << 24);
	checksum = (u32)getCRC((u16 *)&buf32[OTP_OEM_START_ADDR/4], OEM_CRC32_LEN_OTP, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the OEM data section (0x%08X != 0x%08X)",
				checksum, checksumFromOTP);
		goto out;
	} else {
		crc32_temp = crc32_header_temp = true;
		pr_info("Camera: End checking CRC32 (0x%08X = 0x%08X)",
				checksum, checksumFromOTP);
	}
#endif
out:
	if( position == SENSOR_POSITION_REAR ) {
		crc32_check = crc32_temp;
		crc32_header_check = crc32_header_temp;
		info("crc32_header_check=%d, crc32_check=%d\n", crc32_header_check, crc32_check);
		return crc32_check;
	} else {
		crc32_check_rear2 = crc32_temp;
		crc32_header_check_rear2 = crc32_header_temp;
		info("crc32_header_check_rear2=%d, crc32_check_rear2=%d\n", crc32_header_check_rear2, crc32_check_rear2);
		return crc32_check_rear2;
	} 
}
#endif

#ifdef CAMERA_MODULE_DUALIZE
bool fimc_is_sec_check_setfile_crc32(char *buf)
{
	struct fimc_is_from_info *finfo = NULL;
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, &finfo);

	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2P2_B))
		checksum_seed = CHECKSUM_SEED_SETF_LL;
	else
		checksum_seed = CHECKSUM_SEED_SETF_LS;

	info("Camera: Start checking CRC32 Setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], finfo->setfile_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_setfile_check = false;
	} else {
		crc32_setfile_check = true;
	}

	info("Camera: End checking CRC32 Setfile\n");

	return crc32_setfile_check;
}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
bool fimc_is_sec_check_front_setfile_crc32(char *buf)
{
	struct fimc_is_from_info *finfo = NULL;
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	checksum_seed = CHECKSUM_SEED_FRONT_SETF_LL;
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, &finfo);

	info("Camera: Start checking CRC32 Front setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], finfo->front_setfile_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_front_setfile_check = false;
	} else {
		crc32_front_setfile_check = true;
	}

	info("Camera: End checking CRC32 front setfile\n");

	return crc32_front_setfile_check;
}
#endif

#ifdef CAMERA_MODULE_FRONT2_SETF_DUMP
bool fimc_is_sec_check_front2_setfile_crc32(char *buf)
{
	struct fimc_is_from_info *finfo = NULL;
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	checksum_seed = CHECKSUM_SEED_FRONT2_SETF_LL;
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, &finfo);

	info("Camera: Start checking CRC32 Front setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], finfo->front2_setfile_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_front2_setfile_check = false;
	} else {
		crc32_front2_setfile_check = true;
	}

	info("Camera: End checking CRC32 front2 setfile\n");

	return crc32_front2_setfile_check;
}
#endif
#endif

void remove_dump_fw_file(void)
{
	mm_segment_t old_fs;
	int old_mask;
	long ret;
	char fw_path[100];
	struct fimc_is_from_info *finfo = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	fimc_is_sec_get_sysfs_finfo(&finfo);

	/* RTA binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, finfo->load_rta_fw_name);

	ret = sys_unlink(fw_path);
	info("sys_unlink (%s) %ld", fw_path, ret);

	/* DDK binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, finfo->load_fw_name);

	ret = sys_unlink(fw_path);
	info("sys_unlink (%s) %ld", fw_path, ret);

	sys_umask(old_mask);
	set_fs(old_fs);

	is_dumped_fw_loading_needed = false;
}

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	if (force_caldata_dump) {
		sys_rmdir(name);
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	} else {
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	}
	if (fd < 0) {
		err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

ssize_t write_data_to_file_append(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	if (force_caldata_dump) {
		sys_rmdir(name);
		fd = sys_open(name, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0666);
	} else {
		fd = sys_open(name, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0664);
	}
	if (fd < 0) {
		err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_RDONLY, 0664);
	if (fd < 0) {
		if (-ENOENT == fd)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to open %s\n", __func__, fd, name);

		set_fs(old_fs);
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		tx = vfs_read(fp, buf, count, pos);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

bool fimc_is_sec_file_exist(char *name)
{
	mm_segment_t old_fs;
	bool exist = true;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_access(name, 0);
	if (ret) {
		exist = false;
		if (-ENOENT == ret)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to access %s\n", __func__, ret, name);
	}

	set_fs(old_fs);
	return exist;
}

void fimc_is_sec_make_crc32_table(u32 *table, u32 id)
{
	u32 i, j, k;

	for (i = 0; i < 256; ++i) {
		k = i;
		for (j = 0; j < 8; ++j) {
			if (k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}
		table[i] = k;
	}
}

#if 0 /* unused */
static void fimc_is_read_sensor_version(void)
{
	int ret;
	char buf[0x50];

	memset(buf, 0x0, 0x50);

	printk(KERN_INFO "+++ %s\n", __func__);

	ret = fimc_is_spi_read(buf, 0x0, 0x50);

	printk(KERN_INFO "--- %s\n", __func__);

	if (ret) {
		err("fimc_is_spi_read - can't read sensor version\n");
	}

	err("Manufacturer ID(0x40): 0x%02x\n", buf[0x40]);
	err("Pixel Number(0x41): 0x%02x\n", buf[0x41]);
}

static void fimc_is_read_sensor_version2(void)
{
	char *buf;
	char *cal_data;
	u32 cur;
	u32 count = SETFILE_SIZE/READ_SIZE;
	u32 extra = SETFILE_SIZE%READ_SIZE;

	printk(KERN_ERR "%s\n", __func__);

	buf = (char *)kmalloc(READ_SIZE, GFP_KERNEL);
	cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

	memset(buf, 0x0, READ_SIZE);
	memset(cal_data, 0x0, SETFILE_SIZE);

	for (cur = 0; cur < SETFILE_SIZE; cur += READ_SIZE) {
		fimc_is_spi_read(buf, cur, READ_SIZE);
		memcpy(cal_data+cur, buf, READ_SIZE);
		memset(buf, 0x0, READ_SIZE);
	}

	if (extra != 0) {
		fimc_is_spi_read(buf, cur, extra);
		memcpy(cal_data+cur, buf, extra);
		memset(buf, 0x0, extra);
	}

	info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);

	info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

	kfree(buf);
	kfree(cal_data);
}

static int fimc_is_get_cal_data(void)
{
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long ret = 0;
	u8 mem0 = 0, mem1 = 0;
	u32 CRC = 0;
	u32 DataCRC = 0;
	u32 IntOriginalCRC = 0;
	u32 crc_index = 0;
	int retryCnt = 2;
	u32 header_crc32 =	0x1000;
	u32 oem_crc32 =		0x2000;
	u32 awb_crc32 =		0x3000;
	u32 shading_crc32 = 0x6000;
	u32 shading_header = 0x22C0;

	char *cal_data;

	crc32_check = true;
	printk(KERN_INFO "%s\n", __func__);
	printk(KERN_INFO "+++ %s\n", __func__);

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "cal_map_version = %.4s\n", cal_map_version);

	if (cal_map_version[3] == '5') {
		shading_crc32 = 0x6000;
		shading_header = 0x22C0;
	} else if (cal_map_version[3] == '6') {
		shading_crc32 = 0x4000;
		shading_header = 0x920;
	} else {
		shading_crc32 = 0x5000;
		shading_header = 0x22C0;
	}

	/* Make CRC Table */
	fimc_is_sec_make_crc32_table((u32 *)&crc_table, 0xEDB88320);


	retry:
		cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

		memset(cal_data, 0x0, SETFILE_SIZE);

		mem0 = 0, mem1 = 0;
		CRC = 0;
		DataCRC = 0;
		IntOriginalCRC = 0;
		crc_index = 0;

		fimc_is_spi_read(cal_data, 0, SETFILE_SIZE);

		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x80)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made HEADER CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[header_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[header_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[header_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[header_crc32-1]&0x00ff);
		printk(KERN_INFO "Original HEADER CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;

		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0xC0)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x1000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x1000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made OEM CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[oem_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[oem_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[oem_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[oem_crc32-1]&0x00ff);
		printk(KERN_INFO "Original OEM CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x20)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x2000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x2000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made AWB CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[awb_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[awb_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[awb_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[awb_crc32-1]&0x00ff);
		printk(KERN_INFO "Original AWB CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (shading_header)/2; crc_index++) {

			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x3000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x3000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made SHADING CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[shading_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[shading_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[shading_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[shading_crc32-1]&0x00ff);
		printk(KERN_INFO "Original SHADING CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (crc32_check == true) {
			printk(KERN_INFO "make cal_data.bin~~~~ \n");
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		} else {
			if (retryCnt > 0) {
				set_fs(old_fs);
				retryCnt--;
				goto retry;
			}
		}

/*
		{
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		}
*/

		if (fp != NULL)
			filp_close(fp, current->files);

	out:
		set_fs(old_fs);
		kfree(cal_data);
		return err;

}

#endif

/**
 * fimc_is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
int fimc_is_sec_ldo_enabled(struct device *dev, char *name) {
	struct regulator *regulator = NULL;
	int enabled = 0;
	
	regulator = regulator_get_optional(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail \n", __func__, name);
		return -EINVAL;
	}

	enabled = regulator_is_enabled(regulator);

	regulator_put(regulator);

	if (enabled == 1)
		info("%s : %s is aleady enabled !!\n", __func__, name);
	else if (enabled == 0)
		info("%s : %s is not enabled !!\n", __func__, name);

	return enabled;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get_optional(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			err("%s : regulator_disable(%s) fail", __func__, name);
			goto exit;
		}
	}

exit:
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_rom_power_on(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}


int fimc_is_sec_rom_power_off(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}


#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) \
	|| defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
int fimc_is_i2c_read_burst(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2;
	u8 addr_buf[addr_size];
	int index = 0;

	if (!client) {
		info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	fimc_is_sensor_write8( client, OTP_READ_START_ADDR_HIGH, addr_buf[0]);
	fimc_is_sensor_write8( client, OTP_READ_START_ADDR_LOW, addr_buf[1]);
	fimc_is_sensor_write8(client, OTP_READ_MODE_ADDR, 0x01);
	for(index = 0; index <=size; index++){
		fimc_is_sensor_read8( client, OTP_READ_ADDR, buf+index);
	}

	return 0;
}
#endif
int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 2;
	u8 addr_buf[addr_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to write 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;

		info("%s: i2c_master_recv failed(%d), try %d\n", __func__,  ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to read 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

int fimc_is_i2c_write(struct i2c_client *client, u16 addr, u8 data)
{
	const u32 write_buf_size = 3, max_retry = 2;
	u8 write_buf[write_buf_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr+data */
	write_buf[0] = ((u16)addr) >> 8;
	write_buf[1] = (u8)addr;
	write_buf[2] = data;


	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, write_buf, write_buf_size);
		if (likely(write_buf_size == ret))
			break;

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

static int fimc_is_i2c_config(struct i2c_client *client, bool onoff)
{
	struct device *i2c_dev = client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c = NULL;

	info("(%s):onoff(%d)\n", __func__, onoff);
	if (onoff) {
		/* ON */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	} else {
		/* OFF */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "default");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	}

	return 0;
}
#endif /* CONFIG_CAMERA_EEPROM_SUPPORT_REAR || CONFIG_CAMERA_EEPROM_SUPPORT_FRONT */

int fimc_is_sec_check_reload(struct fimc_is_core *core)
{
	struct file *reload_key_fp = NULL;
	struct file *supend_resume_key_fp = NULL;
	mm_segment_t old_fs;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	reload_key_fp = filp_open("/data/media/0/reload/r1e2l3o4a5d.key", O_RDONLY, 0);
	if (IS_ERR(reload_key_fp)) {
		reload_key_fp = NULL;
	} else {
		info("Reload KEY exist, reload cal data.\n");
		force_caldata_dump = true;
		specific->suspend_resume_disable = true;
	}

	if (reload_key_fp)
		filp_close(reload_key_fp, current->files);

	supend_resume_key_fp = filp_open("/data/media/0/i1s2p3s4r.key", O_RDONLY, 0);
	if (IS_ERR(supend_resume_key_fp)) {
		supend_resume_key_fp = NULL;
	} else {
		info("Supend_resume KEY exist, disable runtime supend/resume. \n");
		specific->suspend_resume_disable = true;
	}

	if (supend_resume_key_fp)
		filp_close(supend_resume_key_fp, current->files);

	set_fs(old_fs);

	return 0;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_read_eeprom_header(struct device *dev, int position)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific;
	struct fimc_is_from_info *finfo = NULL;
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;

	specific = core->vender.private_data;
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	client = specific->eeprom_client[position];

#ifdef CAMERA_FRONT2
	if (position == SENSOR_POSITION_FRONT2)
		client = specific->eeprom_client[SENSOR_POSITION_FRONT];
#endif
	if (!client) {
		err("(%s)eeprom i2c client is NULL\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	if(position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_I2C_HEADER_VERSION_START_ADDR_FRONT, FIMC_IS_HEADER_VER_SIZE);
#endif
	} else if (position == SENSOR_POSITION_FRONT2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) && defined(EEP_I2C_HEADER_VERSION_START_ADDR_FRONT2)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_I2C_HEADER_VERSION_START_ADDR_FRONT2, FIMC_IS_HEADER_VER_SIZE);
#endif
	} else if (position == SENSOR_POSITION_REAR2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) && defined(EEP_I2C_HEADER_VERSION_START_ADDR_REAR2)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_I2C_HEADER_VERSION_START_ADDR_REAR2, FIMC_IS_HEADER_VER_SIZE);
#endif
	} else if (position == SENSOR_POSITION_REAR3) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_I2C_HEADER_VERSION_START_ADDR_REAR3, FIMC_IS_HEADER_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_I2C_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
#endif
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	if (position == SENSOR_POSITION_FRONT2) {
		memcpy(finfo->header2_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
		finfo->header2_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	} else if (position == SENSOR_POSITION_REAR2) {
		memcpy(finfo->header2_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
		finfo->header2_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	} else {
		memcpy(finfo->header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	}

exit:
	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);
	cal_size = fimc_is_sec_get_max_cal_size(position);
	client = specific->eeprom_client[position];

#ifdef CAMERA_FRONT2
	if (position == SENSOR_POSITION_FRONT2)
		client = specific->eeprom_client[SENSOR_POSITION_FRONT];
#endif
	if (!client) {
		err("(%s)eeprom i2c client is NULL\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
					EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT,
					FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
					EEP_I2C_HEADER_VERSION_START_ADDR_FRONT,
					FIMC_IS_HEADER_VER_SIZE);
#if defined(CAMERA_FRONT2)
		ret = fimc_is_i2c_read(client, finfo->header2_ver,
					EEP_I2C_HEADER_VERSION_START_ADDR_FRONT2,
					FIMC_IS_HEADER_VER_SIZE);
#endif
#endif
	} else if (position == SENSOR_POSITION_REAR3){
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
					EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_REAR3,
					FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
					EEP_I2C_HEADER_VERSION_START_ADDR_REAR3,
					FIMC_IS_HEADER_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
					EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR,
					FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
					EEP_I2C_HEADER_VERSION_START_ADDR,
					FIMC_IS_HEADER_VER_SIZE);
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
		ret = fimc_is_i2c_read(client, finfo->header2_ver,
					EEP_I2C_HEADER_VERSION_START_ADDR,
					FIMC_IS_HEADER_VER_SIZE);
#endif
#endif
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: EEPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

crc_retry:

	/* read cal data */
	info("Camera[%d]: I2C read cal data\n", position);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	ret = fimc_is_i2c_read(client, buf, 0x0, cal_size);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->need_i2c_config) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (ret) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		info("FRONT EEPROM header version = %s\n", finfo->header_ver);
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_FRONT]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_FRONT]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
#if defined(EEP_HEADER_AWB_START_ADDR_FRONT)
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_FRONT]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_FRONT]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(EEP_HEADER_AP_SHADING_START_ADDR_FRONT)
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_FRONT]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_FRONT]);
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver,
		       &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);
#if defined(EEP_HEADER_MODULE_ID_ADDR_FRONT)
		memcpy(finfo->eeprom_front_module_id, &buf[EEP_HEADER_MODULE_ID_ADDR_FRONT], FIMC_IS_MODULE_ID_SIZE);
#else
		memset(finfo->eeprom_front_module_id, 0x0, FIMC_IS_MODULE_ID_SIZE);
#endif
		memcpy(finfo->project_name,
		       &buf[EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = EEP_CHECKSUM_HEADER_ADDR_FRONT;

#if defined(EEP_HEADER_SENSOR_ID_ADDR_FRONT)
		memcpy(finfo->from_sensor_id, &buf[EEP_HEADER_SENSOR_ID_ADDR_FRONT], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif

#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR_FRONT], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = EEP_CHECKSUM_OEM_ADDR_FRONT;
#endif
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR_FRONT], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = EEP_CHECKSUM_AWB_ADDR_FRONT;

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR_FRONT;

#if defined(EEP_CROSSTALK_CAL_DATA_VER_START_ADDR_FRONT)
		/* CROSSTALK CAL Data : Module/Manufacturer Information */
		memcpy(finfo->crosstalk_cal_data_ver, &buf[EEP_CROSSTALK_CAL_DATA_VER_START_ADDR_FRONT], FIMC_IS_CROSSTALK_CAL_DATA_VER_SIZE);
		finfo->crosstalk_cal_data_ver[FIMC_IS_CROSSTALK_CAL_DATA_VER_SIZE] = '\0';
		finfo->crosstalk_cal_section_crc_addr = EEP_CHECKSUM_CROSSTALK_CAL_DATA_ADDR_FRONT;
#endif

#if defined(EEP_HEADER_MTF_DATA_ADDR_FRONT)
		finfo->mtf_data_addr = EEP_HEADER_MTF_DATA_ADDR_FRONT;
#endif /* EEP_HEADER_MTF_DATA_ADDR_FRONT */

#if defined(CAMERA_FRONT2)
		/* HEARDER Data : Module/Manufacturer Information of FRONT2 */
		memcpy(finfo->header2_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT2], FIMC_IS_HEADER_VER_SIZE);
		finfo->header2_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

#if defined(EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT)
		finfo->crosstalk_cal_data_start_addr = *((u32 *)&buf[EEP_HEADER_CROSSTALK_CAL_DATA_START_ADDR_FRONT]);
		finfo->crosstalk_cal_data_end_addr = *((u32 *)&buf[EEP_HEADER_CROSSTALK_CAL_DATA_END_ADDR_FRONT]);
		info("Crosstalk cal data start = 0x%08x, end = 0x%08x\n",
			(finfo->crosstalk_cal_data_start_addr), (finfo->crosstalk_cal_data_end_addr));
#endif

#if defined(EEP_HEADER_AWB_START_ADDR_FRONT2)
		finfo->awb2_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_FRONT2]);
		finfo->awb2_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_FRONT2]);
		info("AWB2 start = 0x%08x, end = 0x%08x\n",
			(finfo->awb2_start_addr), (finfo->awb2_end_addr));
#endif
#if defined(EEP_HEADER_AP_SHADING_START_ADDR_FRONT2)
		finfo->shading2_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_FRONT2]);
		finfo->shading2_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_FRONT2]);
		info("Shading2 start = 0x%08x, end = 0x%08x\n",
			(finfo->shading2_start_addr), (finfo->shading2_end_addr));
#endif

		/* HEARDER Data : Module/Manufacturer Information of FRONT2 */
		memcpy(finfo->header2_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT2], FIMC_IS_HEADER_VER_SIZE);
		finfo->header2_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

#if defined(EEP_HEADER_SENSOR_ID_ADDR_FRONT2)
		memcpy(finfo->from_sensor2_id, &buf[EEP_HEADER_SENSOR_ID_ADDR_FRONT2], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor2_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif

#if defined(EEP_HEADER_MTF_DATA2_ADDR_FRONT)
		finfo->mtf_data2_addr = EEP_HEADER_MTF_DATA2_ADDR_FRONT;
#endif /* EEP_HEADER_MTF_DATA2_ADDR_FRONT*/

		/* AWB2 Data : Module/Manufacturer Information of FRONT2*/
		memcpy(finfo->awb2_ver, &buf[EEP_AWB_VER_START_ADDR_FRONT2], FIMC_IS_AWB_VER_SIZE);
		finfo->awb2_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb2_section_crc_addr = EEP_CHECKSUM_AWB_ADDR_FRONT2;

		/* SHADING2 Data : Module/Manufacturer Information */
		memcpy(finfo->shading2_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_FRONT2], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading2_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading2_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR_FRONT2;
#endif /* CAMERA_FRONT2 */
#endif /* CONFIG_CAMERA_EEPROM_SUPPORT_FRONT */

	} else if (position == SENSOR_POSITION_REAR3) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		info("REAR3 EEPROM header version = %s, Fix me\n", finfo->header_ver);
#if defined(EEP_HEADER_OEM_START_ADDR_REAR3)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_REAR3]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_REAR3]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
#if defined(EEP_HEADER_AWB_START_ADDR_REAR3)
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_REAR3]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_REAR3]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(EEP_HEADER_AP_SHADING_START_ADDR_REAR3)
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_REAR3]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_REAR3]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
#endif
#if defined(EEP_HEADER_AP_PDAF_START_ADDR_REAR3)
		finfo->ap_pdaf_start_addr = *((u32 *)&buf[EEP_HEADER_AP_PDAF_START_ADDR_REAR3]);
		finfo->ap_pdaf_end_addr = *((u32 *)&buf[EEP_HEADER_AP_PDAF_END_ADDR_REAR3]);
		if (finfo->ap_pdaf_end_addr > 0x1fff) {
			err("AP PDAF end_addr has error!! 0x%08x", finfo->ap_pdaf_end_addr);
			finfo->ap_pdaf_end_addr = 0x1fff;
		}
		info("AP PDAF start = 0x%08x, end = 0x%08x\n",
			(finfo->ap_pdaf_start_addr), (finfo->ap_pdaf_end_addr));
#endif

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR_REAR3], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR_REAR3], FIMC_IS_CAL_MAP_VER_SIZE);
		/* MODULE ID : Module ID Information */
#if defined(EEP_HEADER_MODULE_ID_ADDR_REAR3)
		memcpy(finfo->from_module_id, &buf[EEP_HEADER_MODULE_ID_ADDR_REAR3], FIMC_IS_MODULE_ID_SIZE);
#else
		memset(finfo->from_module_id, 0x0, FIMC_IS_MODULE_ID_SIZE);
#endif
#if defined(EEP_HEADER_SENSOR_ID_ADDR_REAR3)
		memcpy(finfo->from_sensor_id, &buf[EEP_HEADER_SENSOR_ID_ADDR_REAR3], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif

		memcpy(finfo->project_name, &buf[EEP_HEADER_PROJECT_NAME_START_ADDR_REAR3], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = EEP_CHECKSUM_HEADER_ADDR_REAR3;

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR_REAR3], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = EEP_CHECKSUM_OEM_ADDR_REAR3;

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR_REAR3], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = EEP_CHECKSUM_AWB_ADDR_REAR3;

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_REAR3], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR_REAR3;

		/* READ AF CAL : PAN & MACRO */
#if defined(EEPROM_AF_CAL_PAN_ADDR_REAR3)
		finfo->af_cal_pan = *((u32 *)&buf[EEPROM_AF_CAL_PAN_ADDR_REAR3]);
#endif
#if defined(EEPROM_AF_CAL_MACRO_ADDR_REAR3)
		finfo->af_cal_macro = *((u32 *)&buf[EEPROM_AF_CAL_MACRO_ADDR_REAR3]);
#endif
#if defined(EEP_AP_PDAF_VER_START_ADDR_REAR3)
		/* PDAF Data : Module/Manufacturer Information */
		memcpy(finfo->ap_pdaf_ver, &buf[EEP_AP_PDAF_VER_START_ADDR_REAR3], FIMC_IS_AP_PDAF_VER_SIZE);
		finfo->ap_pdaf_ver[FIMC_IS_AP_PDAF_VER_SIZE] = '\0';
		finfo->ap_pdaf_section_crc_addr = EEP_CHECKSUM_AP_PDAF_ADDR_REAR3;
#endif
		/* MTF Data : AF Position & Resolution */
#if defined(EEP_HEADER_MTF_DATA_ADDR_REAR3)
		finfo->mtf_data_addr = EEP_HEADER_MTF_DATA_ADDR_REAR3;
#endif
#endif /* CONFIG_CAMERA_EEPROM_SUPPORT_REAR3 */
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		info("REAR EEPROM header version = %s\n", finfo->header_ver);
#if defined(EEP_HEADER_OEM_START_ADDR)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
#if defined(EEP_HEADER_AWB_START_ADDR)
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(EEP_HEADER_AP_SHADING_START_ADDR)
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR]);
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
#endif
#if defined(EEP_HEADER_AP_PDAF_START_ADDR)
		finfo->ap_pdaf_start_addr = *((u32 *)&buf[EEP_HEADER_AP_PDAF_START_ADDR]);
		finfo->ap_pdaf_end_addr = *((u32 *)&buf[EEP_HEADER_AP_PDAF_END_ADDR]);
		if (finfo->ap_pdaf_end_addr > 0x1fff) {
			err("AP PDAF end_addr has error!! 0x%08x", finfo->ap_pdaf_end_addr);
			finfo->ap_pdaf_end_addr = 0x1fff;
		}
		info("AP PDAF start = 0x%08x, end = 0x%08x\n",
			(finfo->ap_pdaf_start_addr), (finfo->ap_pdaf_end_addr));
#endif

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
		/* MODULE ID : Module ID Information */
#if defined(EEP_HEADER_MODULE_ID_ADDR)
		memcpy(finfo->from_module_id, &buf[EEP_HEADER_MODULE_ID_ADDR], FIMC_IS_MODULE_ID_SIZE);
#else
		memset(finfo->from_module_id, 0x0, FIMC_IS_MODULE_ID_SIZE);
#endif
#if defined(EEP_HEADER_SENSOR_ID_ADDR)
		memcpy(finfo->from_sensor_id, &buf[EEP_HEADER_SENSOR_ID_ADDR], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif

		memcpy(finfo->project_name, &buf[EEP_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = EEP_CHECKSUM_HEADER_ADDR;

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = EEP_CHECKSUM_OEM_ADDR;

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = EEP_CHECKSUM_AWB_ADDR;

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR;

		/* READ AF CAL : PAN & MACRO */
#if defined(EEPROM_AF_CAL_PAN_ADDR)
		finfo->af_cal_pan = *((u32 *)&buf[EEPROM_AF_CAL_PAN_ADDR]);
#endif
#if defined(EEPROM_AF_CAL_MACRO_ADDR)
		finfo->af_cal_macro = *((u32 *)&buf[EEPROM_AF_CAL_MACRO_ADDR]);
#endif
#if defined(EEP_AP_PDAF_VER_START_ADDR)
		/* PDAF Data : Module/Manufacturer Information */
		memcpy(finfo->ap_pdaf_ver, &buf[EEP_AP_PDAF_VER_START_ADDR], FIMC_IS_AP_PDAF_VER_SIZE);
		finfo->ap_pdaf_ver[FIMC_IS_AP_PDAF_VER_SIZE] = '\0';
		finfo->ap_pdaf_section_crc_addr = EEP_CHECKSUM_AP_PDAF_ADDR;
#endif
		/* MTF Data : AF Position & Resolution */
#if defined(EEP_HEADER_MTF_DATA_ADDR)
		finfo->mtf_data_addr = EEP_HEADER_MTF_DATA_ADDR;
#endif

#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
#if defined(EEP_HEADER_OEM_START_ADDR_REAR2)
		finfo->oem2_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_REAR2]);
		finfo->oem2_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_REAR2]);
		info("OEM2 start = 0x%08x, end = 0x%08x\n",
			(finfo->oem2_start_addr), (finfo->oem2_end_addr));
#endif
#if defined(EEP_HEADER_AWB_START_ADDR_REAR2)
		finfo->awb2_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_REAR2]);
		finfo->awb2_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_REAR2]);
		info("AWB2 start = 0x%08x, end = 0x%08x\n",
			(finfo->awb2_start_addr), (finfo->awb2_end_addr));
#endif
#if defined(EEP_HEADER_AP_SHADING_START_ADDR_REAR2)
		finfo->shading2_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_REAR2]);
		finfo->shading2_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_REAR2]);
		if (finfo->shading2_end_addr > 0x3fff) {
			err("Shading2 end_addr has error!! 0x%08x", finfo->shading2_end_addr);
			finfo->setfile_end_addr = 0x3fff;
		}
		info("Shading2 start = 0x%08x, end = 0x%08x\n",
			(finfo->shading2_start_addr), (finfo->shading2_end_addr));
#endif
#if defined(EEP_HEADER_DUAL_DATA_START_ADDR)
		finfo->dual_data_start_addr = *((u32 *)&buf[EEP_HEADER_DUAL_DATA_START_ADDR]);
		finfo->dual_data_end_addr = *((u32 *)&buf[EEP_HEADER_DUAL_DATA_END_ADDR]);
		info("DualData start = 0x%08x, end = 0x%08x\n",
			(finfo->dual_data_start_addr), (finfo->dual_data_end_addr));
#endif
		/* HEARDER2 Data : Sub Module/Manufacturer Information */
#if defined(EEP_HEADER_VERSION_START_ADDR_REAR2)
		memcpy(finfo->header2_ver, &buf[EEP_HEADER_VERSION_START_ADDR_REAR2], FIMC_IS_HEADER_VER_SIZE);
		finfo->header2_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
#endif
		/* OEM2 Data : Sub Module/Manufacturer Information */
#if defined(EEP_OEM_VER_START_ADDR_REAR2)
		memcpy(finfo->oem2_ver, &buf[EEP_OEM_VER_START_ADDR_REAR2], FIMC_IS_OEM_VER_SIZE);
		finfo->oem2_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem2_section_crc_addr = EEP_CHECKSUM_OEM_ADDR_REAR2;
#endif
		/* AWB2 Data : Sub Module/Manufacturer Information */
#if defined(EEP_AWB_VER_START_ADDR_REAR2)
		memcpy(finfo->awb2_ver, &buf[EEP_AWB_VER_START_ADDR_REAR2], FIMC_IS_AWB_VER_SIZE);
		finfo->awb2_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb2_section_crc_addr = EEP_CHECKSUM_AWB_ADDR_REAR2;
#endif
		/* SHADING2 Data : Sub Module/Manufacturer Information */
#if defined(EEP_AP_SHADING_VER_START_ADDR_REAR2)
		memcpy(finfo->shading2_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_REAR2], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading2_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading2_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR_REAR2;
#endif
#if defined(EEP_DUAL_DATA_VER_START_ADDR)
		/* DUAL Data : Sub Module/Manufacturer Information */
		memcpy(finfo->dual_data_ver, &buf[EEP_DUAL_DATA_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->dual_data_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		finfo->dual_data_section_crc_addr= EEP_CHECKSUM_DUAL_DATA_ADDR;
#endif

		/* READ AF CAL REAR2 : PAN & MACRO */
#if defined(EEPROM_AF_CAL2_PAN_ADDR_REAR2)
		finfo->af_cal2_pan = *((u32 *)&buf[EEPROM_AF_CAL2_PAN_ADDR_REAR2]);
#endif
#if defined(EEPROM_AF_CAL2_MACRO_ADDR_REAR2)
		finfo->af_cal2_macro = *((u32 *)&buf[EEPROM_AF_CAL2_MACRO_ADDR_REAR2]);
#endif
#if defined(EEP_HEADER_MTF_DATA2_ADDR)
		/* MTF Data : AF Position & Resolution */
		finfo->mtf_data2_addr = EEP_HEADER_MTF_DATA2_ADDR;
#endif
#else
#if defined(EEP_HEADER_DUAL_DATA_START_ADDR)
		finfo->dual_data_start_addr = *((u32 *)&buf[EEP_HEADER_DUAL_DATA_START_ADDR]);
		finfo->dual_data_end_addr = *((u32 *)&buf[EEP_HEADER_DUAL_DATA_END_ADDR]);
		info("DualData start = 0x%08x, end = 0x%08x\n",
			(finfo->dual_data_start_addr), (finfo->dual_data_end_addr));
#endif
#if defined(EEP_DUAL_DATA_VER_START_ADDR)
		/* DUAL Data : Sub Module/Manufacturer Information */
		memcpy(finfo->dual_data_ver, &buf[EEP_DUAL_DATA_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->dual_data_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		finfo->dual_data_section_crc_addr= EEP_CHECKSUM_DUAL_DATA_ADDR;
#endif
#endif /* CAMERA_REAR2 */
#endif /* CONFIG_CAMERA_EEPROM_SUPPORT_REAR */
	}

	/* debug info dump */
#if defined(EEPROM_DEBUG)
	info("++++ EEPROM[%d] data info\n", position);
	info("1. Header info\n");
	info(" Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info(" project_name : %s\n", finfo->project_name);
	info(" Cal data map ver : %s\n", finfo->cal_map_ver);
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR3) {
		info(" Module ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo->from_module_id[0], finfo->from_module_id[1], finfo->from_module_id[2],
			finfo->from_module_id[3], finfo->from_module_id[4], finfo->from_module_id[5],
			finfo->from_module_id[6], finfo->from_module_id[7], finfo->from_module_id[8],
			finfo->from_module_id[9]);
	} else {
		info(" Module ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo->eeprom_front_module_id[0], finfo->eeprom_front_module_id[1], finfo->eeprom_front_module_id[2],
			finfo->eeprom_front_module_id[3], finfo->eeprom_front_module_id[4], finfo->eeprom_front_module_id[5],
			finfo->eeprom_front_module_id[6], finfo->eeprom_front_module_id[7], finfo->eeprom_front_module_id[8],
			finfo->eeprom_front_module_id[9]);
	}
	info("2. OEM info\n");
	info(" Module info : %s\n", finfo->oem_ver);
	info("3. AWB info\n");
	info(" Module info : %s\n", finfo->awb_ver);
	info("4. Shading info\n");
	info(" Module info : %s\n", finfo->shading_ver);
#if defined(USE_AP_PDAF)
	if (position == SENSOR_POSITION_REAR) {
		info("5. PDAF info\n");
		info(" Module info : %s\n", finfo->ap_pdaf_ver);
	}
#endif

#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) {
		info("5. Header2 info\n");
		info(" Module info : %s\n", finfo->header2_ver);
		info(" ID : %c\n", finfo->header2_ver[FW_CORE_VER]);
		info(" Pixel num : %c%c\n", finfo->header2_ver[FW_PIXEL_SIZE],
			finfo->header2_ver[FW_PIXEL_SIZE+1]);
		info(" ISP ID : %c\n", finfo->header2_ver[FW_ISP_COMPANY]);
		info(" Sensor Maker : %c\n", finfo->header2_ver[FW_SENSOR_MAKER]);
		info(" Year : %c\n", finfo->header2_ver[FW_PUB_YEAR]);
		info(" Month : %c\n", finfo->header2_ver[FW_PUB_MON]);
		info(" Release num : %c%c\n", finfo->header2_ver[FW_PUB_NUM],
			finfo->header_ver[FW_PUB_NUM+1]);
		info(" Manufacturer ID : %c\n", finfo->header2_ver[FW_MODULE_COMPANY]);
		info(" Module ver : %c\n", finfo->header2_ver[FW_VERSION_INFO]);
		info("6. AWB2 info\n");
		info(" Module info : %s\n", finfo->awb2_ver);
		info("7. Shading2 info\n");
		info(" Module info : %s\n", finfo->shading2_ver);
		info("8. DualData info\n");
		info(" Module info : %s\n", finfo->dual_data_ver);
	}
#else
#if defined(EEP_DUAL_DATA_VER_START_ADDR)
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) {
		info("5. DualData info\n");
		info(" Module info : %s\n", finfo->dual_data_ver);
	}
#endif
#endif

#if defined(CAMERA_FRONT2)
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		info("5. Crosstalk cal data info\n");
		info(" Module info : %s\n", finfo->crosstalk_cal_data_ver);
		info("6. Header info of front2\n");
		info(" Module info : %s\n", finfo->header2_ver);
		info(" ID : %c\n", finfo->header2_ver[FW_CORE_VER]);
		info(" Pixel num : %c%c\n", finfo->header2_ver[FW_PIXEL_SIZE],
			finfo->header2_ver[FW_PIXEL_SIZE+1]);
		info(" ISP ID : %c\n", finfo->header2_ver[FW_ISP_COMPANY]);
		info(" Sensor Maker : %c\n", finfo->header2_ver[FW_SENSOR_MAKER]);
		info(" Year : %c\n", finfo->header2_ver[FW_PUB_YEAR]);
		info(" Month : %c\n", finfo->header2_ver[FW_PUB_MON]);
		info(" Release num : %c%c\n", finfo->header2_ver[FW_PUB_NUM],
			finfo->header_ver[FW_PUB_NUM+1]);
		info(" Manufacturer ID : %c\n", finfo->header2_ver[FW_MODULE_COMPANY]);
		info(" Module ver : %c\n", finfo->header2_ver[FW_VERSION_INFO]);
		info("7. AWB info\n");
		info(" Module info : %s\n", finfo->awb2_ver);
		info("8. Shading info\n");
		info(" Module info : %s\n", finfo->shading2_ver);
	}
#endif /* CAMERA_FRONT2 */

	info("---- EEPROM[%d] data info\n", position);
#endif

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
#if defined(CAMERA_FRONT2)
		if (finfo->header2_ver[3] == 'L') {
			crc32_check_factory_front2 = crc32_check_front2;
		} else
#endif
		{
			crc32_check_factory_front2 = false;
		}
	} else if (position == SENSOR_POSITION_REAR3) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		if (finfo->header_ver[3] == 'L')
			crc32_check_factory_rear3 = crc32_check_rear3;
		else
			crc32_check_factory_rear3 = false;
#endif
	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
		if (finfo->header2_ver[3] == 'L') {
			crc32_check_factory_rear2 = crc32_check_rear2;
		} else
#endif
		{
			crc32_check_factory_rear2 = false;
		}
	}

	/* check CAMERA_MODULE_ES_VERSION in EEPROM */
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION_FRONT) {
				is_latest_cam_module_front = true;
			} else {
				is_latest_cam_module_front = false;
			}
#if defined(CAMERA_FRONT2)
			if (finfo->header2_ver[10] >= CAMERA_MODULE_ES_VERSION_FRONT) {
				is_latest_cam_module_front2 = true;
			} else
#endif
			{
				is_latest_cam_module_front2 = false;
			}
		} else {
			is_latest_cam_module_front = true;
			is_latest_cam_module_front2 = true;
		}
	}
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
			if (finfo->header2_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
#endif
		} else {
			is_latest_cam_module = true;
		}
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
	if (position == SENSOR_POSITION_REAR3) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR3)
				is_latest_cam_module_rear3 = true;
			else
				is_latest_cam_module_rear3 = false;
		} else {
			is_latest_cam_module_rear3 = true;
		}
	}
#endif

	/* check FIMC_IS_LATEST_FROM_VERSION_M in EEPROM */
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
#if defined(CAMERA_FRONT2)
			if (finfo->header2_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front2 = true;
			} else
#endif
			{
				is_final_cam_module_front2 = false;
			}
		} else {
			is_final_cam_module_front = true;
			is_final_cam_module_front2 = true;
		}
	}
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
			if (finfo->header2_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
#endif
		} else {
			is_final_cam_module = true;
		}
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
	if (position == SENSOR_POSITION_REAR3) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M)
				is_final_cam_module_rear3 = true;
			else
				is_final_cam_module_rear3 = false;
		} else {
			is_final_cam_module_rear3 = true;
		}
	}
#endif

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump EEPROM cal data.\n");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) {
				if (write_data_to_file("/data/media/0/dump/eeprom_rear_cal.bin", buf, cal_size, &pos) < 0) {
					info("Failed to rear dump cal data.\n");
					goto dump_err;
				}
			}
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
			if (position == SENSOR_POSITION_REAR3) {
				if (write_data_to_file("/data/media/0/dump/eeprom_rear3_cal.bin", buf, cal_size, &pos) < 0) {
					info("Failed to rear dump cal data.\n");
					goto dump_err;
				}
			}
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
				if (write_data_to_file("/data/media/0/dump/eeprom_front_cal.bin", buf, cal_size, &pos) < 0) {
					info("Failed to front dump cal data.\n");
					goto dump_err;
				}
			}
#endif
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);

	set_fs(old_fs);
exit:
	return ret;
}
#endif /* CONFIG_CAMERA_EEPROM_SUPPORT_REAR || CONFIG_CAMERA_EEPROM_SUPPORT_FRONT */

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#define I2C_WRITE 3
#define I2C_BYTE  2
#define I2C_DATA  1
#define I2C_ADDR  0

enum i2c_write {
	I2C_WRITE_ADDR8_DATA8 = 0x0,
	I2C_WRITE_ADDR16_DATA8,
	I2C_WRITE_ADDR16_DATA16
};

int fimc_is_sec_set_registers(struct i2c_client *client, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;

	BUG_ON(!regs);

	for (i = 0; i < size; i += I2C_WRITE) {
#if defined( CAMERA_OTPROM_SUPPORT_FRONT_HYNIX )
		if (regs[i + I2C_ADDR] == 0xFFFF) {
			msleep(regs[i + I2C_DATA]);
			err("fimc_is_sensor_addr sleep %dms\n",regs[i + I2C_DATA]);
		}else{
			if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR8_DATA8) {
				ret = fimc_is_sensor_addr8_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("fimc_is_sensor_addr8_write8 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
					break;
				}
			} else if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR16_DATA8) {
				ret = fimc_is_sensor_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
					break;
                }
			} else if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR16_DATA16) {
				ret = fimc_is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("fimc_is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
					break;
				}
			}
		}
#else
		if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR8_DATA8) {
			ret = fimc_is_sensor_addr8_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("fimc_is_sensor_addr8_write8 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				break;
			}
		} else if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR16_DATA8) {
			ret = fimc_is_sensor_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				break;
			}
		} else if (regs[i + I2C_BYTE] == I2C_WRITE_ADDR16_DATA16) {
			ret = fimc_is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("fimc_is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				break;
			}
		}
#endif
	}
	return ret;
}

#endif
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#if defined(SENSOR_OTP_5E9)
int otprom_5e9_check_to_read(struct i2c_client *client)
{
	u8 data8=0;
	int ret;
	ret = fimc_is_sensor_write8(client, 0x0A00, 0x01);
	msleep(1);
	ret = fimc_is_sensor_read8(client, 0x0A01, &data8);
	return ret;

}

int fimc_is_i2c_read_otp_5e9(struct i2c_client *client, void *buf, u32 start_addr, size_t size)
{
	int page_num = 0;
	int reg_count = 0;
	int index = 0;
	int ret = 0;

	page_num = OTP_5E9_GET_PAGE(start_addr,OTP_PAGE_START_ADDR,HEADER_START_ADDR);
	reg_count = OTP_5E9_GET_REG(start_addr,OTP_PAGE_START_ADDR,HEADER_START_ADDR);
	fimc_is_sensor_write8(client, OTP_PAGE_ADDR, page_num);
	ret = otprom_5e9_check_to_read(client);

	for(index = 0; index<size ;index++)
	{
		if(reg_count >= 64)
		{
			page_num++;
			reg_count = 0;
			fimc_is_sensor_write8(client, OTP_PAGE_ADDR, page_num);
			ret = otprom_5e9_check_to_read(client);
		}
		fimc_is_sensor_read8(client, OTP_REG_ADDR_START+reg_count, buf+index);
		reg_count++;
	}

	return ret;
}

int fimc_is_sec_readcal_otprom_5e9(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char temp_cal_buf[0x10] = {0};
#ifdef OTP_BANK
	u8 otp_bank = 0;
#endif
	u16 start_addr = 0;

	pr_info("fimc_is_sec_readcal_otprom_5e9 E\n");

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);
	cal_size = fimc_is_sec_get_max_cal_size(position);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#if defined(CONFIG_USE_DIRECT_IS_CONTROL)
		client = specific->front_cis_client;
#else
		client = specific->eeprom_client[SENSOR_POSITION_FRONT];
#endif
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
#if defined(CONFIG_USE_DIRECT_IS_CONTROL)
		client = specific->rear_cis_client;
#else
		client = specific->eeprom_client[position];
#endif
#endif
	}

	if (!client) {
		err("cis i2c client is NULL\n");
		return -EINVAL;
	}

	fimc_is_i2c_config(client, true);
	msleep(10);

	/* 0. write Sensor Init(global) */
	ret = fimc_is_sec_set_registers(client,
	sensor_Global, sensor_Global_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	/* 1. write stream on */
	fimc_is_sensor_write8(client, 0x0100, 0x01);
	msleep(50);

	/* 2. write OTP page */
	fimc_is_sensor_write8(client, OTP_PAGE_ADDR, 0x11);
	ret = otprom_5e9_check_to_read(client);

	fimc_is_sensor_read8(client, OTP_REG_ADDR_START, &otp_bank);
	pr_info("Camera: otp_bank = %d\n", otp_bank);

	/* 3. selected page setting */
	switch(otp_bank) {
	case 0x1 :
		start_addr = OTP_START_ADDR;
		break;
	case 0x3 :
		start_addr = OTP_START_ADDR_BANK2;
		break;
	case 0x7 :
		start_addr = OTP_START_ADDR_BANK3;
		break;
	default :
		start_addr = OTP_START_ADDR;
		break;
	}

	pr_info("Camera: otp_start_addr = %x\n", start_addr);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	
	ret = fimc_is_sec_set_registers(client, OTP_Init_reg, OTP_Init_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

crc_retry:

	/* read cal data 
	 * 5E9 use page per 64byte */
	pr_info("Camera: I2C read cal data\n\n");
	fimc_is_i2c_read_otp_5e9(client, buf, start_addr, OTP_USED_CAL_SIZE);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		pr_info("FRONT OTPROM header version = %s\n", finfo->header_ver);
#if defined(OTP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[OTP_HEADER_OEM_START_ADDR_FRONT]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[OTP_HEADER_OEM_END_ADDR_FRONT]) - start_addr;
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
#if defined(OTP_HEADER_AWB_START_ADDR_FRONT)
#ifdef OTP_HEADER_DIRECT_ADDR_FRONT
		finfo->awb_start_addr = OTP_HEADER_AWB_START_ADDR_FRONT - start_addr;
		finfo->awb_end_addr = OTP_HEADER_AWB_END_ADDR_FRONT - start_addr;
#else
		finfo->awb_start_addr = *((u32 *)&buf[OTP_HEADER_AWB_START_ADDR_FRONT]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[OTP_HEADER_AWB_END_ADDR_FRONT]) - start_addr;
#endif
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_SHADING_START_ADDR_FRONT)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR_FRONT]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR_FRONT]) - start_addr;
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OTP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver,
			   &buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);
#if defined(OTP_HEADER_MODULE_ID_ADDR_FRONT)
		memcpy(finfo->eeprom_front_module_id,
			   &buf[OTP_HEADER_MODULE_ID_ADDR_FRONT], FIMC_IS_MODULE_ID_SIZE);
		finfo->eeprom_front_module_id[FIMC_IS_MODULE_ID_SIZE] = '\0';
#endif

#if defined(OTP_HEADER_PROJECT_NAME_START_ADDR_FRONT)
		memcpy(finfo->project_name,
			   &buf[OTP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
#endif
		finfo->header_section_crc_addr = OTP_CHECKSUM_HEADER_ADDR_FRONT;
#if defined(OTP_HEADER_OEM_START_ADDR_FRONT)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OTP_OEM_VER_START_ADDR_FRONT], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = OTP_CHECKSUM_OEM_ADDR_FRONT;
#endif
#if defined(OTP_AWB_VER_START_ADDR_FRONT)
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[OTP_AWB_VER_START_ADDR_FRONT], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = OTP_CHECKSUM_AWB_ADDR_FRONT;
#endif
#if defined(OTP_AP_SHADING_VER_START_ADDR_FRONT)
		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[OTP_AP_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = OTP_CHECKSUM_AP_SHADING_ADDR_FRONT;
#endif
#endif
	} else {
		pr_info("REAR OTPROM header version = %s\n", finfo->header_ver);
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		finfo->oem_start_addr = *((u32 *)&buf[OTP_HEADER_OEM_START_ADDR]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[OTP_HEADER_OEM_END_ADDR]) - start_addr;
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#if defined(OTP_HEADER_AWB_START_ADDR)
		finfo->awb_start_addr = *((u32 *)&buf[OTP_HEADER_AWB_START_ADDR]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[OTP_HEADER_AWB_END_ADDR]) - start_addr;
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_AP_SHADING_START_ADDR)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR]) - start_addr;
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OTP_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OTP_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
#if defined(OTP_HEADER_MODULE_ID_ADDR)
		memcpy(finfo->from_module_id,
			   &buf[OTP_HEADER_MODULE_ID_ADDR], FIMC_IS_MODULE_ID_SIZE);
		finfo->from_module_id[FIMC_IS_MODULE_ID_SIZE] = '\0';
#endif
#if defined(OTP_HEADER_SENSOR_ID_ADDR)
		memcpy(finfo->from_sensor_id, &buf[OTP_HEADER_SENSOR_ID_ADDR], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif
#if defined(OTP_HEADER_PROJECT_NAME_START_ADDR)
		memcpy(finfo->project_name, &buf[OTP_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = OTP_CHECKSUM_HEADER_ADDR;
#endif
#if defined(OTP_HEADER_MTF_DATA_ADDR)
		finfo->mtf_data_addr = OTP_HEADER_MTF_DATA_ADDR;
#endif
#if defined(OTP_OEM_VER_START_ADDR)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OTP_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = OTP_CHECKSUM_OEM_ADDR;
#endif
#if defined(OTP_AWB_VER_START_ADDR)
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[OTP_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = OTP_CHECKSUM_AWB_ADDR;
#endif
#if defined(OTP_AP_SHADING_VER_START_ADDR)
		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[OTP_AP_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = OTP_CHECKSUM_AP_SHADING_ADDR;

		finfo->af_cal_pan = *((u32 *)&buf[OTPROM_AF_CAL_PAN_ADDR]);
		finfo->af_cal_macro = *((u32 *)&buf[OTPROM_AF_CAL_MACRO_ADDR]);
#endif
#endif
	}

	if(finfo->cal_map_ver[0] != 'V') {
		pr_info("Camera: Cal Map version read fail or there's no available data.\n");
		/* it for case of CRC fail at re-work module  */
		if(retry == FIMC_IS_CAL_RETRY_CNT && otp_bank != 0x1) {
			start_addr -= 0xf;
			pr_info("%s : change start address(%x)\n",__func__,start_addr);
			retry--;
			goto crc_retry;
		}
		crc32_check_factory_front = false;
		goto exit;
	}

	pr_info("Camera: OTPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	/* debug info dump */
	pr_info("++++ OTPROM data info\n");
	pr_info(" Header info\n");
	pr_info(" Module info : %s\n", finfo->header_ver);
	pr_info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	pr_info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE], finfo->header_ver[FW_PIXEL_SIZE+1]);
	pr_info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	pr_info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	pr_info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	pr_info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	pr_info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM], finfo->header_ver[FW_PUB_NUM+1]);
	pr_info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	pr_info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	pr_info(" Module ID : %c%c%c%c%c%X%X%X%X%X\n",
			finfo->eeprom_front_module_id[0], finfo->eeprom_front_module_id[1], finfo->eeprom_front_module_id[2],
			finfo->eeprom_front_module_id[3], finfo->eeprom_front_module_id[4], finfo->eeprom_front_module_id[5],
			finfo->eeprom_front_module_id[6], finfo->eeprom_front_module_id[7], finfo->eeprom_front_module_id[8],
			finfo->eeprom_front_module_id[9]);

	pr_info("---- OTPROM data info\n");

	/* CRC check */
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (!fimc_is_sec_check_front_otp_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#else
	if (!fimc_is_sec_check_otp_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#endif

#if defined(OTP_MODE_CHANGE)
	/* 6. return to original mode */
	ret = fimc_is_sec_set_registers(client,
	sensor_mode_change_from_OTP_reg, sensor_mode_change_from_OTP_reg_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
	} else if (position == SENSOR_POSITION_REAR2) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_rear2 = crc32_check;
		} else {
			crc32_check_factory_rear2 = false;
		}

	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
	}

	if (!specific->use_module_check) {
		if (position == SENSOR_POSITION_REAR2) {
			is_latest_cam_module_rear2 = true;
		} else {
			is_latest_cam_module = true;
		}
	} else {
#if defined(CAMERA_MODULE_ES_VERSION_REAR)
		if (position == SENSOR_POSITION_REAR && sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else if (position == SENSOR_POSITION_REAR2 && sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR2) { //It need to check about rear2
			is_latest_cam_module_rear2 = true;
		} else
#endif 
		{
			is_latest_cam_module = false;
		}
	}

	if (position == SENSOR_POSITION_REAR) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		} else {
			is_final_cam_module = true;
		}
	 } else if (position == SENSOR_POSITION_REAR2) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_rear2 = true;
			} else {
				is_final_cam_module_rear2 = false;
			}
		} else {
			is_final_cam_module = true;
		}
	} else {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		pr_info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			pr_info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			pr_info("dump folder exist, Dump OTPROM cal data.\n");
			fimc_is_i2c_read_otp_5e9(client, temp_cal_buf, OTP_PAGE_START_ADDR, 0xf);
			if (write_data_to_file("/data/media/0/dump/otprom_cal.bin",
						temp_cal_buf, 0xf, &pos) < 0) {
				pr_info("Failed to dump cal data.\n");
				goto dump_err;
			}
			if (write_data_to_file_append("/data/media/0/dump/otprom_cal.bin",
						buf, OTP_USED_CAL_SIZE, &pos) < 0) {
				pr_info("Failed to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);

exit:
	fimc_is_i2c_config(client, false);

	pr_info("fimc_is_sec_readcal_otprom X\n");
	return ret;
}
#else //SENSOR_OTP_5E9 end
int fimc_is_sec_readcal_otprom_legacy(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
#ifdef OTP_BANK
	u8 data8 = 0;
	int otp_bank = 0;
#endif
#ifdef OTP_SINGLE_READ_ADDR
	int i = 0;
	u8 start_addr_h = 0;
	u8 start_addr_l= 0;
#endif
	u16 start_addr = 0;

	pr_info("fimc_is_sec_readcal_otprom E\n");

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);
	cal_size = fimc_is_sec_get_max_cal_size(position);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#if defined(CONFIG_USE_DIRECT_IS_CONTROL)
		client = specific->front_cis_client;
#else
		client = specific->eeprom_client[SENSOR_POSITION_FRONT];
#endif
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
#if defined(CONFIG_USE_DIRECT_IS_CONTROL)
		client = specific->rear_cis_client;
#else
		client = specific->eeprom_client[position];
#endif
#endif
	}

	if (!client) {
		err("eeprom i2c client is NULL\n");
		return -EINVAL;
	}

	fimc_is_i2c_config(client, true);
	msleep(10);

#if defined(OTP_NEED_INIT_SETTING)
	/* 0. sensor init */
	if (!force_caldata_dump) {
		ret = specific->cis_init_reg_write();
		if (unlikely(ret)) {
			err("failed to fimc_is_i2c_write (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}
#endif

#if defined(OTP_NEED_INIT_DIRECT)
	ret = fimc_is_sec_set_registers(client,
	sensor_Global, sensor_Global_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
#endif


#if defined(OTP_MODE_CHANGE)
	/* 1. mode change to OTP */
	ret = fimc_is_sec_set_registers(client,
	sensor_mode_change_to_OTP_reg, sensor_mode_change_to_OTP_reg_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
#endif

#if defined(OTP_BANK)
#if defined(OTP_SINGLE_READ_ADDR)
	/* 2. single read OTP Bank */
	fimc_is_sensor_write8(client, OTP_START_ADDR_HIGH, OTP_BANK_ADDR_HIGH);
	fimc_is_sensor_write8(client, OTP_START_ADDR_LOW, OTP_BANK_ADDR_LOW);
	fimc_is_sensor_write8(client, OTP_SINGLE_READ, 0x01);
	fimc_is_sensor_read8(client, OTP_SINGLE_READ_ADDR, &data8);
	otp_bank = data8;
	pr_info("Camera: otp_bank = %d\n", otp_bank);

	/* 3. selected page setting */
	switch(otp_bank) {
	case 1 :
		start_addr_h = OTP_BANK1_START_ADDR_HIGH;
		start_addr_l = OTP_BANK1_START_ADDR_LOW;
		break;
	case 3 :
		start_addr_h = OTP_BANK2_START_ADDR_HIGH;
		start_addr_l = OTP_BANK2_START_ADDR_LOW;
		break;
	case 7 :
		start_addr_h = OTP_BANK3_START_ADDR_HIGH;
		start_addr_l = OTP_BANK3_START_ADDR_LOW;
		break;
	default :
		start_addr_h = OTP_BANK1_START_ADDR_HIGH;
		start_addr_l = OTP_BANK1_START_ADDR_LOW;
		break;
	}
	start_addr = ((start_addr_h << 8)&0xff00) | (start_addr_l&0xff);
#else
	/* 2. read OTP Bank */
#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
	fimc_is_sensor_write8(client, OTP_READ_START_ADDR_HIGH, (OTP_BANK_ADDR>>8)&0xff);
	fimc_is_sensor_write8(client, OTP_READ_START_ADDR_LOW, (OTP_BANK_ADDR)&0xff);
	fimc_is_sensor_write8(client, OTP_READ_MODE_ADDR, 0x01);
	fimc_is_sensor_read8(client, OTP_READ_ADDR, &data8);
#else
	fimc_is_sensor_read8(client, OTP_BANK_ADDR, &data8);
#endif
	otp_bank = data8;
	pr_info("Camera: otp_bank = %d\n", otp_bank);
#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
    /* 3. selected page setting */
	switch(otp_bank) {
	case 0x1 :
		start_addr = OTP_START_ADDR;
		break;
	case 0x13 :
		start_addr = OTP_START_ADDR_BANK2;
		break;
	case 0x37 :
		start_addr = OTP_START_ADDR_BANK3;
		break;
	default :
		start_addr = OTP_START_ADDR;
		break;
	}

	pr_info("Camera: otp_start_addr = %x\n", start_addr);

	ret = fimc_is_i2c_read_burst(client, finfo->cal_map_ver,
				       OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT+start_addr,
				       FIMC_IS_CAL_MAP_VER_SIZE);
	ret = fimc_is_i2c_read_burst(client, finfo->header_ver,
				       OTP_HEADER_VERSION_START_ADDR_FRONT+start_addr,
				       FIMC_IS_HEADER_VER_SIZE);

#else
	start_addr = OTP_START_ADDR;

	/* 3. selected page setting */
	switch(otp_bank) {
	case 1 :
		ret = fimc_is_sec_set_registers(client,
		OTP_first_page_select_reg, OTP_first_page_select_reg_size);
		break;
	case 3 :
		ret = fimc_is_sec_set_registers(client,
		OTP_second_page_select_reg, OTP_second_page_select_reg_size);
		break;
	default :
		ret = fimc_is_sec_set_registers(client,
		OTP_first_page_select_reg, OTP_first_page_select_reg_size);
		break;
	}

	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
#endif
#endif
#endif

crc_retry:

#if defined(OTP_BANK)
#if defined(OTP_SINGLE_READ_ADDR)
	fimc_is_sensor_write8(client, OTP_START_ADDR_HIGH, start_addr_h);
	fimc_is_sensor_write8(client, OTP_START_ADDR_LOW, start_addr_l);
	fimc_is_sensor_write8(client, OTP_SINGLE_READ, 0x01);

	/* 5. full read cal data */
	pr_info("Camera: I2C read full cal data\n\n");
	for (i = 0; i < OTP_USED_CAL_SIZE; i++) {
		fimc_is_sensor_read8(client, OTP_SINGLE_READ_ADDR, &data8);
		buf[i] = data8;
	}
#else
	/* read cal data */
	pr_info("Camera: I2C read cal data\n\n");
#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
	fimc_is_i2c_read_burst(client, buf, start_addr, OTP_USED_CAL_SIZE);
#else
	fimc_is_i2c_read(client, buf, start_addr, OTP_USED_CAL_SIZE);
#endif
#endif
#endif

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		pr_info("FRONT OTPROM header version = %s\n", finfo->header_ver);
#if defined(OTP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[OTP_HEADER_OEM_START_ADDR_FRONT]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[OTP_HEADER_OEM_END_ADDR_FRONT]) - start_addr;
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
#if defined(OTP_HEADER_AWB_START_ADDR_FRONT)
#ifdef OTP_HEADER_DIRECT_ADDR_FRONT
		finfo->awb_start_addr = OTP_HEADER_AWB_START_ADDR_FRONT - start_addr;
		finfo->awb_end_addr = OTP_HEADER_AWB_END_ADDR_FRONT - start_addr;
#else
		finfo->awb_start_addr = *((u32 *)&buf[OTP_HEADER_AWB_START_ADDR_FRONT]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[OTP_HEADER_AWB_END_ADDR_FRONT]) - start_addr;
#endif
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_SHADING_START_ADDR_FRONT)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR_FRONT]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR_FRONT]) - start_addr;
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OTP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver,
			   &buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);
#if defined(OTP_HEADER_MODULE_ID_ADDR_FRONT)
		memcpy(finfo->eeprom_front_module_id,
			   &buf[OTP_HEADER_MODULE_ID_ADDR_FRONT], FIMC_IS_MODULE_ID_SIZE);
		finfo->eeprom_front_module_id[FIMC_IS_MODULE_ID_SIZE] = '\0';
#endif

#if defined(OTP_HEADER_PROJECT_NAME_START_ADDR_FRONT)
		memcpy(finfo->project_name,
			   &buf[OTP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
#endif
		finfo->header_section_crc_addr = OTP_CHECKSUM_HEADER_ADDR_FRONT;
#if defined(OTP_HEADER_OEM_START_ADDR_FRONT)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OTP_OEM_VER_START_ADDR_FRONT], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = OTP_CHECKSUM_OEM_ADDR_FRONT;
#endif
#if defined(OTP_AWB_VER_START_ADDR_FRONT)
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[OTP_AWB_VER_START_ADDR_FRONT], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = OTP_CHECKSUM_AWB_ADDR_FRONT;
#endif
#if defined(OTP_AP_SHADING_VER_START_ADDR_FRONT)
		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[OTP_AP_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = OTP_CHECKSUM_AP_SHADING_ADDR_FRONT;
#endif
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		finfo->oem_start_addr = *((u32 *)&buf[OTP_HEADER_OEM_START_ADDR]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[OTP_HEADER_OEM_END_ADDR]) - start_addr;
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#if defined(OTP_HEADER_AWB_START_ADDR)
		finfo->awb_start_addr = *((u32 *)&buf[OTP_HEADER_AWB_START_ADDR]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[OTP_HEADER_AWB_END_ADDR]) - start_addr;
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_AP_SHADING_START_ADDR)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR]) - start_addr;
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OTP_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OTP_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);

#if defined(OTP_HEADER_PROJECT_NAME_START_ADDR)
		memcpy(finfo->project_name, &buf[OTP_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = OTP_CHECKSUM_HEADER_ADDR;
#endif
#if defined(OTP_OEM_VER_START_ADDR)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OTP_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = OTP_CHECKSUM_OEM_ADDR;
#endif
#if defined(OTP_AWB_VER_START_ADDR)
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[OTP_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = OTP_CHECKSUM_AWB_ADDR;
#endif
#if defined(OTP_AP_SHADING_VER_START_ADDR)
		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[OTP_AP_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = OTP_CHECKSUM_AP_SHADING_ADDR;

		finfo->af_cal_pan = *((u32 *)&buf[OTPROM_AF_CAL_PAN_ADDR]);
		finfo->af_cal_macro = *((u32 *)&buf[OTPROM_AF_CAL_MACRO_ADDR]);
#endif
#endif
	}

	if(finfo->cal_map_ver[0] != 'V') {
		pr_info("Camera: Cal Map version read fail or there's no available data.\n");
		crc32_check_factory_front = false;
		goto exit;
	}

	pr_info("Camera: OTPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	/* debug info dump */
	pr_info("++++ OTPROM data info\n");
	pr_info(" Header info\n");
	pr_info(" Module info : %s\n", finfo->header_ver);
	pr_info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	pr_info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE], finfo->header_ver[FW_PIXEL_SIZE+1]);
	pr_info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	pr_info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	pr_info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	pr_info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	pr_info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM], finfo->header_ver[FW_PUB_NUM+1]);
	pr_info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	pr_info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	pr_info(" Module ID : %c%c%c%c%c%X%X%X%X%X\n",
			finfo->eeprom_front_module_id[0], finfo->eeprom_front_module_id[1], finfo->eeprom_front_module_id[2],
			finfo->eeprom_front_module_id[3], finfo->eeprom_front_module_id[4], finfo->eeprom_front_module_id[5],
			finfo->eeprom_front_module_id[6], finfo->eeprom_front_module_id[7], finfo->eeprom_front_module_id[8],
			finfo->eeprom_front_module_id[9]);

	pr_info("---- OTPROM data info\n");

	/* CRC check */
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (!fimc_is_sec_check_front_otp_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#else
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#endif

#if defined(OTP_MODE_CHANGE)
	/* 6. return to original mode */
	ret = fimc_is_sec_set_registers(client,
	sensor_mode_change_from_OTP_reg, sensor_mode_change_from_OTP_reg_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
#if defined(CAMERA_MODULE_ES_VERSION_REAR)
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else
#endif 
		{
			is_latest_cam_module = false;
		}
	}

	if (position == SENSOR_POSITION_REAR) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		} else {
			is_final_cam_module = true;
		}
	} else {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		pr_info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			pr_info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			pr_info("dump folder exist, Dump OTPROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/otprom_cal.bin", buf,
									OTP_USED_CAL_SIZE, &pos) < 0) {
				pr_info("Failed to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);

exit:
	fimc_is_i2c_config(client, false);

	pr_info("fimc_is_sec_readcal_otprom X\n");

	return ret;
}
#endif //!SENSOR_OTP_5E9

int fimc_is_sec_readcal_otprom(struct device *dev, int position)
{
	int ret = 0;
#if defined(SENSOR_OTP_5E9)
	ret = fimc_is_sec_readcal_otprom_5e9(dev, position);
#else 
	ret = fimc_is_sec_readcal_otprom_legacy(dev, position);
#endif
	return ret;
}
#endif /* CONFIG_CAMERA_OTPROM_SUPPORT_REAR || CONFIG_CAMERA_OTPROM_SUPPORT_FRONT */

#if 0 //not used for mid-tier !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_read_from_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	ret = fimc_is_spi_read(&core->spi0, header_version, FROM_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_check_status(struct fimc_is_core *core)
{
	int retry_read = 50;
	u8 temp[5] = {0x0, };
	int ret = 0;

	do {
		memset(temp, 0x0, sizeof(temp));
		fimc_is_spi_read_status_bit(&core->spi0, &temp[0]);
		if (retry_read < 0) {
			ret = -EINVAL;
			err("check status failed.");
			break;
		}
		retry_read--;
		msleep(3);
	} while (temp[0]);

	return ret;
}

#ifdef CAMERA_MODULE_DUALIZE
int fimc_is_sec_read_fw_from_sdcard(char *name, unsigned long *size)
{
	struct file *fw_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char data_path[100];
	int ret = 0;
	unsigned long fsize;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(data_path, sizeof(data_path), "%s", name);
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	fw_fp = filp_open(data_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fw_fp)) {
		info("%s does not exist.\n", data_path);
		fw_fp = NULL;
		ret = -EIO;
		goto fw_err;
	} else {
		info("%s exist, Dump from sdcard.\n", name);
		fsize = fw_fp->f_path.dentry->d_inode->i_size;
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			read_data_from_file(name, fw_buf, fsize, &pos);
			*size = fsize;
		} else {
			err("FW size is larger than FW buffer.\n");
			BUG();
		}
	}

fw_err:
	if (fw_fp)
		filp_close(fw_fp, current->files);
	set_fs(old_fs);

	return ret;
}

u32 fimc_is_sec_get_fw_crc32(char *buf, size_t size)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;
	checksum = (u32)getCRC((u16 *)&buf32[0], size, NULL, NULL);

	return checksum;
}

int fimc_is_sec_change_from_header(struct fimc_is_core *core)
{
	int ret = 0;
	u8 crc_value[4];
	u32 crc_result = 0;

	/* read header data */
	info("Camera: Start SPI read header data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	ret = fimc_is_spi_read(&core->spi0, fw_buf, 0x0, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	fw_buf[0x7] = (sysfs_finfo.bin_end_addr & 0xFF000000) >> 24;
	fw_buf[0x6] = (sysfs_finfo.bin_end_addr & 0xFF0000) >> 16;
	fw_buf[0x5] = (sysfs_finfo.bin_end_addr & 0xFF00) >> 8;
	fw_buf[0x4] = (sysfs_finfo.bin_end_addr & 0xFF);
	fw_buf[0x27] = (sysfs_finfo.setfile_end_addr & 0xFF000000) >> 24;
	fw_buf[0x26] = (sysfs_finfo.setfile_end_addr & 0xFF0000) >> 16;
	fw_buf[0x25] = (sysfs_finfo.setfile_end_addr & 0xFF00) >> 8;
	fw_buf[0x24] = (sysfs_finfo.setfile_end_addr & 0xFF);

	strncpy(&fw_buf[0x40], sysfs_finfo.header_ver, 9);
	strncpy(&fw_buf[0x64], sysfs_finfo.setfile_ver, FIMC_IS_ISP_SETFILE_VER_SIZE);

	fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_erase_sector(&core->spi0, 0x0);
	if (ret) {
		err("failed to fimc_is_spi_erase_sector (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0, fw_buf, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, HEADER_CRC32_LEN);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0FFC, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("Camera: End SPI read header data\n");

exit:
	return ret;
}

int fimc_is_sec_write_fw_to_from(struct fimc_is_core *core, char *name, bool first_section)
{
	int ret = 0;
	unsigned long i = 0;
	unsigned long size = 0;
	u32 start_addr = 0, erase_addr = 0, end_addr = 0;
	u32 checksum_addr = 0, crc_result = 0, erase_end_addr = 0;
	u8 crc_value[4];

	if (!strcmp(name, FIMC_IS_FW_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_FW_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.bin_start_addr;
		end_addr = (u32)size + start_addr - 1;
		sysfs_finfo.bin_end_addr = end_addr;
		checksum_addr = 0x3FFFFF;
		sysfs_finfo.fw_size = size;
		strncpy(sysfs_finfo.header_ver, &fw_buf[size - 11], 9);
	} else if (!strcmp(name, FIMC_IS_SETFILE_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_SETFILE_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.setfile_start_addr;
		end_addr = (u32)size + start_addr - 1;
		sysfs_finfo.setfile_end_addr = end_addr;
		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
			checksum_addr = FROM_WRITE_CHECKSUM_SETF_LL;
		else
			checksum_addr = FROM_WRITE_CHECKSUM_SETF_LS;
		sysfs_finfo.setfile_size = size;
		strncpy(sysfs_finfo.setfile_ver, &fw_buf[size - 64], 6);
	}
	else {
		err("Not supported binary type.");
		return -EIO;
	}

	if (ret < 0) {
		err("FW is not exist in sdcard.");
		return -EIO;
	}

	info("Start %s write to FROM.\n", name);

	if (first_section) {
		for (erase_addr = start_addr; erase_addr < erase_end_addr; erase_addr += FIMC_IS_FROM_ERASE_SIZE) {
			ret = fimc_is_spi_write_enable(&core->spi0);
			ret |= fimc_is_spi_erase_block(&core->spi0, erase_addr);
			if (ret) {
				err("failed to fimc_is_spi_erase_block (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
			ret = fimc_is_sec_check_status(core);
			if (ret) {
				err("failed to fimc_is_sec_check_status (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	for (i = 0; i < size; i += 256) {
		ret = fimc_is_spi_write_enable(&core->spi0);
		if (size - i >= 256) {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, 256);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		} else {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, size - i);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
		ret = fimc_is_sec_check_status(core);
		if (ret) {
			err("failed to fimc_is_sec_check_status (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, size);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_write_enable (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write(&core->spi0, checksum_addr -4 + 1, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("End %s write to FROM.\n", name);

exit:
	return ret;
}

int fimc_is_sec_write_fw(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
	struct file *key_fp = NULL;
	struct file *comp_fw_fp = NULL;
	struct file *setfile_fp = NULL;
	struct file *isp_fw_fp = NULL;
	mm_segment_t old_fs;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open(FIMC_IS_KEY_FROM_SDCARD, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		ret = -EIO;
		goto key_err;
	} else {
		comp_fw_fp = filp_open(FIMC_IS_COMPANION_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(comp_fw_fp)) {
			info("Companion FW does not exist.\n");
			comp_fw_fp = NULL;
			ret = -EIO;
			goto comp_fw_err;
		}

		setfile_fp = filp_open(FIMC_IS_SETFILE_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(setfile_fp)) {
			info("setfile does not exist.\n");
			setfile_fp = NULL;
			ret = -EIO;
			goto setfile_err;
		}

		isp_fw_fp = filp_open(FIMC_IS_FW_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(isp_fw_fp)) {
			info("ISP FW does not exist.\n");
			isp_fw_fp = NULL;
			ret = -EIO;
			goto isp_fw_err;
		}
	}

	info("FW file exist, Write Firmware to FROM .\n");

	if (fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM") <= 0)
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_COMPANION_FROM_SDCARD, true);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_SETFILE_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_FW_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}

	/* Off to reset FROM operation. Without this routine, spi read does not work. */
	if (!specific->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

	if (fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM") <= 0)
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

	ret = fimc_is_sec_change_from_header(core);
	if (ret) {
		err("fimc_is_sec_change_from_header failed.");
		ret = -EIO;
		goto err;
	}

err:
	if (!specific->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

isp_fw_err:
	if (isp_fw_fp)
		filp_close(isp_fw_fp, current->files);

setfile_err:
	if (setfile_fp)
		filp_close(setfile_fp, current->files);

comp_fw_err:
	if (comp_fw_fp)
		filp_close(comp_fw_fp, current->files);

key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);

	return ret;
}
#endif

int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	|| sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	sysfs_finfo.header_section_crc_addr = FROM_CHECKSUM_HEADER_ADDR;

	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	memcpy(sysfs_finfo.oem_ver, &cal_buf[FROM_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
	sysfs_finfo.oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
	sysfs_finfo.oem_section_crc_addr = FROM_CHECKSUM_OEM_ADDR;

	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	memcpy(sysfs_finfo.awb_ver, &cal_buf[FROM_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
	sysfs_finfo.awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
	sysfs_finfo.awb_section_crc_addr = FROM_CHECKSUM_AWB_ADDR;

	memcpy(sysfs_finfo.shading_ver, &cal_buf[FROM_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
	sysfs_finfo.shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
	sysfs_finfo.shading_section_crc_addr = FROM_CHECKSUM_SHADING_ADDR;

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	memcpy(sysfs_finfo.from_sensor_id, &cal_buf[EEP_HEADER_SENSOR_ID_ADDR_FRONT], FIMC_IS_SENSOR_ID_SIZE);
	sysfs_finfo.from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
	memcpy(sysfs_finfo.from_sensor2_id, &cal_buf[EEP_HEADER_SENSOR_ID_ADDR_FRONT2], FIMC_IS_SENSOR_ID_SIZE);
	sysfs_finfo.from_sensor2_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
	} else {
		crc32_check_factory = false;
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (specific->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M
#if defined(CAMERA_MODULE_CORE_CS_VERSION)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
#endif
		) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}

#ifdef CAMERA_MODULE_DUALIZE
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
int fimc_is_sec_get_front_setf_name(struct fimc_is_core *core)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	sensor_id = specific->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	memcpy(sysfs_finfo.load_front_setfile_name, module->setfile_name,
		sizeof(sysfs_finfo.load_front_setfile_name));

p_err:
	return ret;
}
#endif

int fimc_is_sec_readfw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
	u32 checksum_seed;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: FW need to be dumped\n");

crc_retry:
	/* read fw data */
	if (FIMC_IS_MAX_FW_BUFFER_SIZE >= FIMC_IS_MAX_FW_SIZE) {
		info("Camera: Start SPI read fw data\n");
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.bin_start_addr, FIMC_IS_MAX_FW_SIZE);
		if (ret) {
			err("failed to fimc_is_spi_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
		info("Camera: End SPI read fw data\n");

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
			|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
			checksum_seed = CHECKSUM_SEED_ISP_FW_LL;
		else
			checksum_seed = CHECKSUM_SEED_ISP_FW_LS;

		/* CRC check */
		if (!fimc_is_sec_check_fw_crc32(fw_buf, checksum_seed, sysfs_finfo.fw_size) && (retry > 0)) {
			retry--;
			goto crc_retry;
		} else if (!retry) {
			ret = -EINVAL;
			err("Camera: FW Data has dumped fail.. CRC ERROR ! \n");
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
		buf = (u8 *)fw_buf;
		fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.fw_size);
#endif
		if (write_data_to_file(fw_path, fw_buf, sysfs_finfo.fw_size, &pos) < 0) {
			ret = -EIO;
			goto exit;
		}

		info("Camera: FW Data has dumped successfully\n");
	} else {
		err("FW size is larger than FW buffer.\n");
		BUG();
	}

	info("Camera: FW Data has dumped successfully\n");

exit:
	return ret;
}

int fimc_is_sec_read_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: Setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B)) {
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr,
			FIMC_IS_MAX_SETFILE_SIZE_LL);
	} else {
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr,
			FIMC_IS_MAX_SETFILE_SIZE_LS);
	}
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
	pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	buf = (u8 *)fw_buf;
	fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.setfile_size);
#endif
	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Setfile has dumped successfully\n");

exit:
	return ret;
}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
int fimc_is_sec_read_front_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: Front setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read front setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.front_setfile_start_addr,
		FIMC_IS_MAX_SETFILE_SIZE_FRONT_LL);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read front setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_front_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_sec_get_front_setf_name(core);
	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH,
		sysfs_finfo.load_front_setfile_name);
	pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	buf = (u8 *)fw_buf;
	fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.front_setfile_size);
#endif
	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.front_setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Front setfile has dumped successfully\n");

exit:
	return ret;
}
#endif

#ifdef CAMERA_MODULE_FRONT2_SETF_DUMP
int fimc_is_sec_read_front2_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: Front2 setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read front setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.front_setfile_start_addr,
		FIMC_IS_MAX_SETFILE_SIZE_FRONT_LL);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read front setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_front_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_sec_get_front_setf_name(core);
	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH,
		sysfs_finfo.load_front_setfile_name);
	pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	buf = (u8 *)fw_buf;
	fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.front_setfile_size);
#endif
	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.front_setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Front setfile has dumped successfully\n");

exit:
	return ret;
}
#endif

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int fimc_is_sec_inflate_fw(u8 **buf, unsigned long *size)
{
	z_stream zs_inflate;
	int ret = 0;
	char *unzip_buf;

	unzip_buf = vmalloc(FIMC_IS_MAX_FW_BUFFER_SIZE);
	if (!unzip_buf) {
		err("failed to allocate memory\n");
		ret = -ENOMEM;
		goto exit;
	}
	memset(unzip_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	zs_inflate.workspace = vmalloc(zlib_inflate_workspacesize());
	ret = zlib_inflateInit2(&zs_inflate, -MAX_WBITS);
	if (ret != Z_OK) {
		err("Camera : inflateInit error\n");
	}

	zs_inflate.next_in = *buf;
	zs_inflate.next_out = unzip_buf;
	zs_inflate.avail_in = *size;
	zs_inflate.avail_out = FIMC_IS_MAX_FW_BUFFER_SIZE;

	ret = zlib_inflate(&zs_inflate, Z_NO_FLUSH);
	if (ret != Z_STREAM_END) {
		err("Camera : zlib_inflate error\n");
	}

	zlib_inflateEnd(&zs_inflate);
	vfree(zs_inflate.workspace);

	*size = FIMC_IS_MAX_FW_BUFFER_SIZE - zs_inflate.avail_out;
	memset(*buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	memcpy(*buf, unzip_buf, *size);
	vfree(unzip_buf);

exit:
	return ret;
}
#endif /* CAMERA_MODULE_COMPRESSED_FW_DUMP */
#endif /* CAMERA_MODULE_DUALIZE */
#endif /* !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) */

int fimc_is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int) (header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int) (header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver)
{
	struct regulator *regulator = NULL;
	int ret = 0;
	int minV, maxV;
	int pixelSize = 0;

	regulator = regulator_get_optional(dev, "cam_sensor_core_1.2v");
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get fail",
			__func__);
		return -EINVAL;
	}
	pixelSize = fimc_is_sec_get_pixel_size(header_ver);

	if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		if (pixelSize == 13) {
			minV = 1050000;
			maxV = 1050000;
		} else if (pixelSize == 8) {
			minV = 1100000;
			maxV = 1100000;
		} else {
			minV = 1050000;
			maxV = 1050000;
		}
	} else if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		minV = 1200000;
		maxV = 1200000;
	} else {
		minV = 1050000;
		maxV = 1050000;
	}

	ret = regulator_set_voltage(regulator, minV, maxV);

	if (ret >= 0)
		info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_fw_find(struct fimc_is_core *core, int position)
{
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_from_info *finfo = NULL;
	u32 *sensor_id = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	if (position == SENSOR_POSITION_REAR3)
		sensor_id = &specific->rear3_sensor_id;
	else
		sensor_id = &specific->rear_sensor_id;

	snprintf(finfo->load_fw_name, sizeof(FIMC_IS_DDK), "%s", FIMC_IS_DDK);
#if defined(USE_RTA_BINARY)
	snprintf(finfo->load_rta_fw_name, sizeof(FIMC_IS_RTA), "%s", FIMC_IS_RTA);
#endif

#ifdef ENABLE_IMX260
	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX260_D)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX260_SETF), "%s", FIMC_IS_IMX260_SETF);
		*sensor_id = SENSOR_NAME_IMX260;
	} else
#endif
	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2L1_C) ) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2L1_SETF), "%s", FIMC_IS_2L1_SETF);
		*sensor_id = SENSOR_NAME_S5K2L1;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX333_E) ||
				fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX333_F)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX333_SETF), "%s", FIMC_IS_IMX333_SETF);
		*sensor_id = SENSOR_NAME_IMX333;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2L2_E) ||
				fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2L2_F)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2L2_SETF), "%s", FIMC_IS_2L2_SETF);
		*sensor_id = SENSOR_NAME_S5K2L2;
	}  else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2P6_Q)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2P6_SETF), "%s", FIMC_IS_2P6_SETF);
		*sensor_id = SENSOR_NAME_S5K2P6;
	}  else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX576)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX576_SETF), "%s", FIMC_IS_IMX576_SETF);
		*sensor_id = SENSOR_NAME_IMX576;
	} else {
		/* default firmware and setfile */
		if (*sensor_id == SENSOR_NAME_S5K2L1) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2L1_SETF), "%s", FIMC_IS_2L1_SETF);
		} else if (*sensor_id == SENSOR_NAME_IMX258) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX258_SETF), "%s", FIMC_IS_IMX258_SETF);
		} else if (*sensor_id == SENSOR_NAME_IMX260) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX260_SETF), "%s", FIMC_IS_IMX260_SETF);
		} else if (*sensor_id == SENSOR_NAME_IMX333) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX333_SETF), "%s", FIMC_IS_IMX333_SETF);
		} else if (*sensor_id == SENSOR_NAME_S5K2L2) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2L2_SETF), "%s", FIMC_IS_2L2_SETF);
		} else if (*sensor_id == SENSOR_NAME_S5K2P6) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2P6_SETF), "%s", FIMC_IS_2P6_SETF);
		} else if (*sensor_id == SENSOR_NAME_S5K4H5YC) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_4H5YC_SETF), "%s", FIMC_IS_4H5YC_SETF);
		} else if (*sensor_id == SENSOR_NAME_S5K4HA) {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_4HA_SETF), "%s", FIMC_IS_4HA_SETF);
		} else if (*sensor_id == SENSOR_NAME_S5K3L2){
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
		} else if (*sensor_id == SENSOR_NAME_IMX576){
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX576_SETF), "%s", FIMC_IS_IMX576_SETF);
		} else {
			snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX260_SETF), "%s", FIMC_IS_IMX260_SETF);
		}
	}

	info("%s sensor id %d %s\n", __func__, *sensor_id, finfo->load_setfile_name);

	return 0;
}

int fimc_is_sec_fw_find_front(struct fimc_is_core *core)
{
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_from_info *finfo = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_FRONT, &finfo);

	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_4E6)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_4E6_SETF), "%s", FIMC_IS_4E6_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K4E6;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX320)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_IMX320_SETF), "%s", FIMC_IS_IMX320_SETF);
		specific->front_sensor_id = SENSOR_NAME_IMX320;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_3H1)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_3H1_SETF), "%s", FIMC_IS_3H1_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K3H1;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_3P8SP_P)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_3P8SP_SETF), "%s", FIMC_IS_3P8SP_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K3P8SP;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_3M3)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_3M3_SETF), "%s", FIMC_IS_3M3_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K3M3;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_4H5YC_P)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_4H5YC_SETF), "%s", FIMC_IS_4H5YC_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K4H5YC;
	} else if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_IMX576_C)) {
		snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_IMX576_FRONT_SETF), "%s", FIMC_IS_IMX576_FRONT_SETF);
		specific->front_sensor_id = SENSOR_NAME_IMX576;
	} else {
		/* default firmware and setfile */
		if (specific->front_sensor_id == SENSOR_NAME_S5K4E6) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_4E6_SETF), "%s", FIMC_IS_4E6_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_IMX320) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_IMX320_SETF), "%s", FIMC_IS_IMX320_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_S5K3H1) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_3H1_SETF), "%s", FIMC_IS_3H1_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_S5K3P8SP) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_3P8SP_SETF), "%s", FIMC_IS_3P8SP_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_S5K5E3) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_5E3_SETF), "%s", FIMC_IS_5E3_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_S5K4H5YC) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_4H5YC_SETF), "%s", FIMC_IS_4H5YC_SETF);
		} else if (specific->front_sensor_id == SENSOR_NAME_IMX576) {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_IMX576_FRONT_SETF), "%s", FIMC_IS_IMX576_FRONT_SETF);
		} else {
			snprintf(finfo->load_front_setfile_name, sizeof(FIMC_IS_IMX320_SETF), "%s", FIMC_IS_IMX320_SETF);
		}
	}

	info("%s sensor id %d %s\n", __func__, specific->front_sensor_id, finfo->load_front_setfile_name);
	return 0;
}

int fimc_is_sec_run_fw_sel(struct device *dev, int position)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_from_info *default_finfo = NULL;
	int rom_position = 0;
	int ret = 0;

	rom_position = position;
	if (fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo)) {
		err("failed get finfo. plz check position %d", position);
		return -ENXIO;	
	}
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, &default_finfo);

	/* Check reload cal data enabled */
	if (!default_finfo->is_check_cal_reload) {
		if (fimc_is_sec_file_exist("/data/media/0/")) {
			fimc_is_sec_check_reload(core);
			default_finfo->is_check_cal_reload = true;
		}
	}

	info("fimc_is_sec_run_fw_sel : Pos %d, R[%d] F[%d]"
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		" R3[%d]"
#endif
		"\n", position, sysfs_finfo.is_caldata_read, sysfs_finfo_front.is_caldata_read
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		, sysfs_finfo_rear3.is_caldata_read
#endif
	);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (!default_finfo->is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
#else
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, true);
#endif
#else
/* When using C2 retention, Cal loading for both front and rear cam will be done at a time */
#if 0 //not used for mid-tier !defined(CONFIG_COMPANION_C2_USE) && !defined(CONFIG_COMPANION_C3_USE)
			ret = fimc_is_sec_fw_sel(core, dev, true);
#endif
#endif
		}

		if (!finfo->is_caldata_read || force_caldata_dump) {
			ret = fimc_is_sec_fw_sel_eeprom(dev, position, false);
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	} else
#endif
	{
		if (!finfo->is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3) \
	|| defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
			switch (rom_position) {
			case SENSOR_POSITION_REAR:
			case SENSOR_POSITION_REAR3:
				break;
			case SENSOR_POSITION_REAR2:
#if defined(CAMERA_REAR2) && defined(CAMERA_REAR2_USE_COMMON_EEP)
				rom_position = SENSOR_POSITION_REAR;
#endif
				break;
			default:
				err("invalid rom postion %d", rom_position); /* secure, ... */
				goto p_err;
			}

			ret = fimc_is_sec_fw_sel_eeprom(dev, rom_position, false);
#else
#if 0 //not used for mid-tier
			ret = fimc_is_sec_fw_sel(core, dev, false);
#endif
#endif
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	}

p_err:
	if (specific->check_sensor_vendor) {
		if (fimc_is_sec_check_from_ver(core, position)) {
			if (finfo->header_ver[3] != 'L') {
				err("Not supported module(position %d). Module ver = %s", position, finfo->header_ver);
				return -EIO;
			}
		}
	}

	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3) \
	|| defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) \
	|| defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_ldo_enabled[2];
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_from_info *finfo = NULL;
	
	is_ldo_enabled[0] = false;
	is_ldo_enabled[1] = false;
	fimc_is_sec_get_sysfs_finfo_by_position(id, &finfo);

	/* Use mutex for i2c read */
	mutex_lock(&specific->spi_lock);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT || id == SENSOR_POSITION_FRONT2) {
		if (!finfo->is_caldata_read || force_caldata_dump) {
			if (force_caldata_dump)
				info("Front forced caldata dump!!\n");

			fimc_is_sec_rom_power_on(core, SENSOR_POSITION_FRONT);
			is_ldo_enabled[0] = true;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
			info("Camera: read cal data from Front EEPROM\n");
			if (!fimc_is_sec_readcal_eeprom(dev, id)) {
				finfo->is_caldata_read = true;
			}
#else
			info("Camera: read cal data from Front OTPROM\n");
			if (!fimc_is_sec_readcal_otprom(dev, id)) {
				finfo->is_caldata_read = true;
			}
#endif
			fimc_is_sec_fw_find_front(core);
		}
		goto exit;
	} else
#endif
	{
		if (!finfo->is_caldata_read || force_caldata_dump) {
			is_dumped_fw_loading_needed = false;
			if (force_caldata_dump)
				info("Rear forced caldata dump!!\n");

			fimc_is_sec_rom_power_on(core, id);
			is_ldo_enabled[0] = true;

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
			if (id == SENSOR_POSITION_REAR2) {
				info("Camera: read cal data from OTPROM[%d]\n", id);
				if (!fimc_is_sec_readcal_otprom(dev, id)) {
					finfo->is_caldata_read = true;
				}
			} else
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
			{
				info("Camera: read cal data from Rear EEPROM[%d]\n", id);
				if (headerOnly) {
					fimc_is_sec_read_eeprom_header(dev, id);
				} else {
					if (!fimc_is_sec_readcal_eeprom(dev, id)) {
						finfo->is_caldata_read = true;
					}
				}
			}
#endif
		}
	}

	fimc_is_sec_fw_find(core, id);
	if (headerOnly) {
		goto exit;
	}

	if (id == SENSOR_POSITION_REAR2)
		goto exit;

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, finfo->load_fw_name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
	if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
		temp_buf = fw_buf;
	} else
#endif
	{
		info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
		read_buf = vmalloc(fsize);
		if (!read_buf) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		temp_buf = read_buf;
	}
	nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
	if (read_buf) {
		vfree(read_buf);
		read_buf = NULL;
		temp_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}

	set_fs(old_fs);

exit:
#if defined(USE_COMMON_CAM_IO_PWR)
	if (is_ldo_enabled[0] &&
		(!specific->running_front_camera && !specific->running_front_second_camera
		&& !specific->running_rear_camera && !specific->running_rear_second_camera)) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		if (id == SENSOR_POSITION_FRONT || id == SENSOR_POSITION_FRONT2) {
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_FRONT);
		} else
#endif
#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
		if (id == SENSOR_POSITION_REAR2) {
                        fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR2);
		} else	
#endif
		{
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
		}
	}
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT) && defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
	if (id == SENSOR_POSITION_FRONT || id == SENSOR_POSITION_FRONT2) {
		if (force_caldata_dump && specific->running_front_camera)
		{
			fimc_is_sec_rom_power_off(core, id);
			msleep(20);
			fimc_is_sec_rom_power_on(core, id);
		}
	}
#endif
#else //USE_COMMON_CAM_IO_PWR
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT || id == SENSOR_POSITION_FRONT2) {
		if (is_ldo_enabled[0] && !specific->running_front_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_FRONT);

#if defined(CAMERA_OTPROM_SUPPORT_FRONT_HYNIX)
		if (force_caldata_dump && specific->running_front_camera)
		{
			fimc_is_sec_rom_power_off(core, id);
			msleep(20);
			fimc_is_sec_rom_power_on(core, id);
		}
#endif
		if (is_ldo_enabled[1] && !specific->running_rear_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	} else
#endif
	{
		if (is_ldo_enabled[0] && !specific->running_rear_camera && !specific->running_rear_second_camera)
			fimc_is_sec_rom_power_off(core, id);
	}
#endif

	mutex_unlock(&specific->spi_lock);

	return ret;
}
#endif

#if 0 //not used for mid-tier !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
#ifdef CAMERA_MODULE_DUALIZE
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;
	bool dump_flag = false;
	struct file *setfile_fp = NULL;
	char setfile_path[100];
#endif
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = false;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	bool is_FromPower_enabled = false;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null\n");
		return -EINVAL;
	}

	/* Use mutex for spi read */
	mutex_lock(&specific->spi_lock);

	if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		if (fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM") <= 0) {
			fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
			is_FromPower_enabled = true;
		}
		info("read cal data from FROM\n");

		if (headerOnly) {
			fimc_is_sec_read_from_header(dev);
		} else {
			if (!fimc_is_sec_readcal(core)) {
				sysfs_finfo.is_caldata_read = true;
			}
		}

		/*select AF actuator*/
		if (!crc32_header_check) {
			info("Camera : CRC32 error for all section.\n");
		}

		fimc_is_sec_fw_find(core);
		if (headerOnly) {
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped firmware(%s)\n", dump_fw_path);
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Dumped FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, temp_buf + nread-11, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone firmware(%s)", fw_path);
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n", fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

#if defined(CAMERA_MODULE_DUALIZE) && defined(CAMERA_MODULE_AVAILABLE_DUMP_VERSION)
		if (!strncmp(CAMERA_MODULE_AVAILABLE_DUMP_VERSION, sysfs_finfo.header_ver, 3)) {
			from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
			phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
			if (is_dump_existed) {
				dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);
			}

			info("from_fw_revision = %d, phone_fw_revision = %d, dump_fw_revision = %d\n",
				from_fw_revision, phone_fw_revision, dump_fw_revision);

			if (fimc_is_sec_compare_ver(SENSOR_POSITION_REAR) /* Check if a module is connected or not */
				&& (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version) ||
				   (from_fw_revision > phone_fw_revision))) {
				is_dumped_fw_loading_needed = true;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver,
								dump_fw_version)) {
						is_dump_needed = true;
					} else if (from_fw_revision > dump_fw_revision) {
						is_dump_needed = true;
					} else {
						is_dump_needed = false;
					}
				} else {
					is_dump_needed = true;
				}
			} else {
				is_dump_needed = false;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(phone_fw_version,
						dump_fw_version)) {
						is_dumped_fw_loading_needed = false;
					} else if (phone_fw_revision > dump_fw_revision) {
						is_dumped_fw_loading_needed = false;
					} else {
						is_dumped_fw_loading_needed = true;
					}
				} else {
					is_dumped_fw_loading_needed = false;
				}
			}

			if (force_caldata_dump) {
				if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version))
					|| (from_fw_revision > phone_fw_revision))
					dump_flag = true;
			} else {
				if (is_dump_needed) {
					dump_flag = true;
					crc32_fw_check = false;
					crc32_setfile_check = false;
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
					crc32_front_setfile_check = false;
#endif
				}
			}

			if (dump_flag) {
				info("Dump ISP Firmware.\n");
				ret = fimc_is_sec_readfw(core);
				msleep(20);
				ret |= fimc_is_sec_read_setfile(core);
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
				msleep(20);
				ret |= fimc_is_sec_read_front_setfile(core);
#endif
				if (ret < 0) {
					if (!crc32_fw_check || !crc32_setfile_check || !crc32_front_setfile_check) {
						is_dumped_fw_loading_needed = false;
						err("Firmware CRC is not valid. Does not use dumped firmware.\n");
					}
				}
			}

			if (phone_fw_version[0] == 0) {
				strcpy(sysfs_pinfo.header_ver, "NULL");
			}

			if (is_dumped_fw_loading_needed) {
				old_fs = get_fs();
				set_fs(KERNEL_DS);
				snprintf(setfile_path, sizeof(setfile_path), "%s%s",
					FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
				setfile_fp = filp_open(setfile_path, O_RDONLY, 0);
				if (IS_ERR_OR_NULL(setfile_fp)) {
					crc32_setfile_check = false;
					info("setfile does not exist. Retry setfile dump.\n");

					fimc_is_sec_read_setfile(core);
					setfile_fp = NULL;
				} else {
					if (setfile_fp) {
						filp_close(setfile_fp, current->files);
					}
					setfile_fp = NULL;
				}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
				memset(setfile_path, 0x0, sizeof(setfile_path));
				fimc_is_sec_get_front_setf_name(core);
				snprintf(setfile_path, sizeof(setfile_path), "%s%s",
					FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_front_setfile_name);
				setfile_fp = filp_open(setfile_path, O_RDONLY, 0);
				if (IS_ERR_OR_NULL(setfile_fp)) {
					crc32_front_setfile_check = false;
					info("setfile does not exist. Retry front setfile dump.\n");

					fimc_is_sec_read_front_setfile(core);
					setfile_fp = NULL;
				} else {
					if (setfile_fp) {
						filp_close(setfile_fp, current->files);
					}
					setfile_fp = NULL;
				}
#endif
				set_fs(old_fs);
			}
		}
#endif

		if (is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, sysfs_finfo.header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, dump_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else {
			strncpy(loaded_fw, phone_fw_version, FIMC_IS_HEADER_VER_SIZE);
		}

	} else {
		info("already loaded the firmware, Phone version=%s, F-ROM version=%s\n",
			sysfs_pinfo.header_ver, sysfs_finfo.header_ver);
	}

exit:
	if (is_FromPower_enabled &&
        ((specific->f_rom_power == FROM_POWER_SOURCE_REAR_SECOND && !specific->running_rear_second_camera) ||
         (specific->f_rom_power == FROM_POWER_SOURCE_REAR && !specific->running_rear_camera))) {
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	}

	mutex_unlock(&specific->spi_lock);

	return ret;
}
#endif
