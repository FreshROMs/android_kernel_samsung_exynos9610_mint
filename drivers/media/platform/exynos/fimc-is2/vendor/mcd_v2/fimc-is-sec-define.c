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

#include <linux/i2c.h>

#include "fimc-is-sec-define.h"
#include "fimc-is-vender-specific.h"
#include "fimc-is-sec-util.h"
#include "fimc-is-device-eeprom.h"
#include "fimc-is-vender-rom-config.h"

#define FIMC_IS_LATEST_ROM_VERSION_M		'M'

/* crc_check_list is initialized in fimc_is_vender_probe */
bool crc32_check_list[SENSOR_POSITION_MAX][CRC32_SCENARIO_MAX];
bool check_latest_cam_module[SENSOR_POSITION_MAX] = {false, };
bool check_final_cam_module[SENSOR_POSITION_MAX] = {false, };

//static bool is_caldata_read = false;
bool force_caldata_dump = false;
bool is_dumped_fw_loading_needed = false;

static int cam_id = CAMERA_SINGLE_REAR;

static struct fimc_is_rom_info sysfs_finfo[SENSOR_POSITION_MAX];
static struct fimc_is_rom_info sysfs_pinfo[SENSOR_POSITION_MAX];

#ifdef FIMC_IS_REAR_MAX_CAL_SIZE
static char cal_buf_rear[FIMC_IS_REAR_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_FRONT_MAX_CAL_SIZE
static char cal_buf_front[FIMC_IS_FRONT_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_REAR2_MAX_CAL_SIZE
static char cal_buf_rear2[FIMC_IS_REAR2_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_FRONT2_MAX_CAL_SIZE
static char cal_buf_front2[FIMC_IS_FRONT2_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_REAR3_MAX_CAL_SIZE
static char cal_buf_rear3[FIMC_IS_REAR3_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_FRONT3_MAX_CAL_SIZE
static char cal_buf_front3[FIMC_IS_FRONT3_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_REAR4_MAX_CAL_SIZE
static char cal_buf_rear4[FIMC_IS_REAR4_MAX_CAL_SIZE];
#endif
#ifdef FIMC_IS_FRONT4_MAX_CAL_SIZE
static char cal_buf_front4[FIMC_IS_FRONT4_MAX_CAL_SIZE];
#endif


static char *cal_buf[SENSOR_POSITION_MAX] = {
#ifdef FIMC_IS_REAR_MAX_CAL_SIZE
	cal_buf_rear,
#else
	NULL,
#endif
#ifdef FIMC_IS_FRONT_MAX_CAL_SIZE
	cal_buf_front,
#else
	NULL,
#endif
#ifdef FIMC_IS_REAR2_MAX_CAL_SIZE
	cal_buf_rear2,
#else
	NULL,
#endif
#ifdef FIMC_IS_FRONT2_MAX_CAL_SIZE
	cal_buf_front2,
#else
	NULL,
#endif
#ifdef FIMC_IS_REAR3_MAX_CAL_SIZE
	cal_buf_rear3,
#else
	NULL,
#endif
#ifdef FIMC_IS_FRONT3_MAX_CAL_SIZE
	cal_buf_front3,
#else
	NULL,
#endif
#ifdef FIMC_IS_REAR4_MAX_CAL_SIZE
	cal_buf_rear4,
#else
	NULL,
#endif
#ifdef FIMC_IS_FRONT4_MAX_CAL_SIZE
	cal_buf_front4,
#else
	NULL,
#endif
};

static char *eeprom_cal_dump_path[SENSOR_POSITION_MAX] = {
	"dump/eeprom_rear_cal.bin",
	"dump/eeprom_front_cal.bin",
	"dump/eeprom_rear2_cal.bin",
	"dump/eeprom_front2_cal.bin",
	"dump/eeprom_rear3_cal.bin",
	"dump/eeprom_front3_cal.bin",
	"dump/eeprom_rear4_cal.bin",
	"dump/eeprom_front4_cal.bin",
	NULL,
	NULL,
};

static char *otprom_cal_dump_path = "dump/otprom_cal.bin";

char loaded_fw[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

/* local function */
#ifdef VENDER_CAL_STRUCT_VER2
static void *fimc_is_sec_search_rom_extend_data(const struct rom_extend_cal_addr *extend_data, char *name);
#endif

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

int fimc_is_sec_get_max_cal_size(struct fimc_is_core *core, int position)
{
	int size = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (!specific->rom_data[position].rom_valid) {
		err("Invalid position[%d]. This position don't have rom!\n", position);
		return size;
	}

	if (specific->rom_cal_map_addr[position] == NULL) {
		err("rom_%d: There is no cal map!\n", position);
		return size;
	}

	size = specific->rom_cal_map_addr[position]->rom_max_cal_size;

	if (!size) {
		err("Cal size is 0 (postion %d). Check cal size!", position);
	}

	return size;
}

int fimc_is_sec_get_cal_buf(int position, char **buf)
{
	*buf = cal_buf[position];

	if (*buf == NULL) {
		err("cal buf is null. position %d", position);
		return -EINVAL;
	}

	return 0;
}

int fimc_is_sec_get_front_cal_buf(char **buf)
{
	fimc_is_sec_get_cal_buf(SENSOR_POSITION_FRONT, buf);
	return 0;
}

int fimc_is_sec_get_sysfs_finfo_by_position(int position, struct fimc_is_rom_info **finfo)
{
	*finfo = &sysfs_finfo[position];

	if (*finfo == NULL) {
		err("finfo addr is null. postion %d", position);
		/*WARN(true, "finfo is null\n");*/
		return -EINVAL;
	}

	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_rom_info **finfo)
{
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, finfo);
	return 0;
}

int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_rom_info **finfo)
{
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_FRONT, finfo);
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_by_position(int position, struct fimc_is_rom_info **pinfo)
{
	*pinfo = &sysfs_pinfo[position];

	if (*pinfo == NULL) {
		err("finfo addr is null. postion %d", position);
		/*WARN(true, "finfo is null\n");*/
		return -EINVAL;
	}

	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_rear(struct fimc_is_rom_info **pinfo)
{
	fimc_is_sec_get_sysfs_pinfo_by_position(SENSOR_POSITION_REAR, pinfo);
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_rom_info **pinfo)
{
	fimc_is_sec_get_sysfs_pinfo_by_position(SENSOR_POSITION_FRONT, pinfo);
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
	err("%s: waring, you're calling the disabled func!", __func__);
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
	struct fimc_is_rom_info *finfo = NULL;

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
		/*Check ASCII code for dumpstate */
		if((finfo->cal_map_ver[0]>= '0') && (finfo->cal_map_ver[0]<= 'z') 
			&& (finfo->cal_map_ver[1]>= '0') && (finfo->cal_map_ver[1]<= 'z')
			&& (finfo->cal_map_ver[2]>= '0') && (finfo->cal_map_ver[2]<= 'z') 
			&& (finfo->cal_map_ver[3]>= '0') && (finfo->cal_map_ver[3]<= 'z')) {
			err("FROM core version is invalid. version is %c%c%c%c",
					finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		} else {
			err("FROM core version is invalid. version is out of bounds");
		}
		return 0;
	}

	return ret;
}

bool fimc_is_sec_check_rom_ver(struct fimc_is_core *core, int position)
{
	struct fimc_is_rom_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	char compare_version;
	u8 from_ver;
	u8 latest_from_ver;
	int rom_position = position;

	if (specific->skip_cal_loading) {
		err("skip_cal_loading implemented");
		return false;
	}

	if (fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo)) {
		err("failed get finfo. plz check position %d", position);
		return false;
	}

	if (specific->rom_share[position].check_rom_share == true)
		rom_position = specific->rom_share[position].share_position;


	if (!specific->rom_cal_map_addr[rom_position]) {
		err("failed get rom_cal_map_addr. plz check cal map addr(%d) \n", rom_position);
		return false;
	}

	latest_from_ver = specific->rom_cal_map_addr[rom_position]->cal_map_es_version;
	compare_version = specific->rom_cal_map_addr[rom_position]->camera_module_es_version;

	from_ver = fimc_is_sec_compare_ver(position);

	if ((from_ver < latest_from_ver) ||
		(finfo->header_ver[10] < compare_version)) {
		err("invalid from version. from_ver %c, header_ver[10] %c", from_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}

bool fimc_is_sec_check_eeprom_crc32(char *buf, int position)
{
	u32 *buf32 = NULL;
	u32 checksum, check_base, checksum_base, check_length;
	u32 address_boundary;
	int i;
	int rom_position = position;
	bool rom_common = false;
	bool crc32_check_temp, crc32_header_temp;

	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;
	const struct fimc_is_vender_rom_addr *rom_addr;
	struct fimc_is_rom_info *finfo = NULL;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	buf32 = (u32 *)buf;

	if (specific->rom_share[position].check_rom_share == true) {
		rom_position = specific->rom_share[position].share_position;
		rom_common = true;
	}

	rom_addr = specific->rom_cal_map_addr[rom_position];

	info("%s E\n", __func__);
	/***** Initial Value *****/
	for (i = CRC32_CHECK_HEADER; i < CRC32_SCENARIO_MAX; i++ ) {
		crc32_check_list[position][i] = true;
	}
	crc32_check_temp = true;
	crc32_header_temp = true;

	/***** SKIP CHECK CRC *****/
#ifdef SKIP_CHECK_CRC
	pr_warning("Camera[%d]: Skip check crc32\n", position);

	crc32_check_temp = true;
	crc32_check_list[position][CRC32_CHECK] = crc32_check_temp;

	return crc32_check_temp;
#endif

	/***** START CHECK CRC *****/
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	address_boundary = fimc_is_sec_get_max_cal_size(core, rom_position);

	/* HEADER DATA CRC CHECK */
	check_base = 0;
	checksum = 0;
	checksum_base = 0;
	check_length = rom_addr->rom_header_checksum_len;

#ifdef ROM_CRC32_DEBUG
	printk(KERN_INFO "[CRC32_DEBUG] Header CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
		check_length, finfo->header_section_crc_addr);
#endif

	checksum_base = finfo->header_section_crc_addr / 4;
	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_check_temp = false;
		crc32_header_temp = false;
		goto out;
	}
	else {
		crc32_header_temp = true;
	}

	/* OEM Cal Data CRC CHECK */
	check_length = 0;
	if (rom_common == true) {
		if (rom_addr->rom_sub_oem_checksum_len > 0)
			check_length = rom_addr->rom_sub_oem_checksum_len;
	} else {
		if (rom_addr->rom_oem_checksum_len > 0)
			check_length = rom_addr->rom_oem_checksum_len;
	}

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->oem_start_addr / 4;
		checksum_base = finfo->oem_section_crc_addr / 4;

#ifdef VENDER_CAL_STRUCT_VER2
		if (rom_addr->extend_cal_addr) {
			int32_t *addr;
			addr = (int32_t *)fimc_is_sec_search_rom_extend_data(rom_addr->extend_cal_addr, EXTEND_OEM_CHECKSUM);
			if (addr != NULL) {
				if (finfo->oem_start_addr != *addr)
					check_base = *addr / 4;
			}
		}
#else
#ifdef COUNT_EXTEND_CAL_DATA
		if (rom_addr->extend_cal_addr) {
			int i;
			int32_t addr;
			char name[] = "oem_checksum_base_addr";
			for(i = 0; i < COUNT_EXTEND_CAL_DATA; i++) {
				if (!strcmp(rom_addr->extend_cal_addr[i].name, name)) {
					addr = rom_addr->extend_cal_addr[i].addr;
					if (finfo->oem_start_addr != addr)
						check_base = addr / 4;

					break;
				}
			}
		}
#endif
#endif

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] OEM CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->oem_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->oem_start_addr, finfo->oem_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: OEM address has error: start(0x%08X), end(0x%08X)",
				position, finfo->oem_start_addr, finfo->oem_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the OEM (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check oem crc32\n", position);
	}

	/* AWB Cal Data CRC CHECK */
	check_length = 0;
	if (rom_common == true) {
		if (rom_addr->rom_sub_awb_checksum_len > 0)
			check_length = rom_addr->rom_sub_awb_checksum_len;
	} else {
		if (rom_addr->rom_awb_checksum_len > 0)
			check_length = rom_addr->rom_awb_checksum_len;
	}

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->awb_start_addr / 4;
		checksum_base = finfo->awb_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] AWB CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->awb_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->awb_start_addr, finfo->awb_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: AWB address has error: start(0x%08X), end(0x%08X)",
				position, finfo->awb_start_addr, finfo->awb_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the AWB (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check awb crc32\n", position);
	}

	/* Shading Cal Data CRC CHECK*/
	check_length = 0;
	if (rom_common == true) {
		if (rom_addr->rom_sub_shading_checksum_len > 0)
			check_length = rom_addr->rom_sub_shading_checksum_len;
	} else {
		if (rom_addr->rom_shading_checksum_len > 0)
			check_length = rom_addr->rom_shading_checksum_len;
	}

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->shading_start_addr / 4;
		checksum_base = finfo->shading_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] Shading CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->shading_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->shading_start_addr, finfo->shading_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: Shading address has error: start(0x%08X), end(0x%08X)",
				position, finfo->shading_start_addr, finfo->shading_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the Shading (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check shading crc32\n", position);
	}

#ifdef USE_AE_CAL
	/* AE Cal Data CRC CHECK */

	if (rom_addr->extend_cal_addr) {
		struct rom_ae_cal_data *ae_cal_data = NULL;

		ae_cal_data = (struct rom_ae_cal_data *)fimc_is_sec_search_rom_extend_data(rom_addr->extend_cal_addr, EXTEND_AE_CAL);

		check_length = 0;
		if(ae_cal_data) {
			if (ae_cal_data->rom_ae_checksum_addr > 0)
				check_length = ae_cal_data->rom_ae_checksum_len;

			if (check_length > 0) {
				checksum = 0;
				check_base = finfo->ae_cal_start_addr / 4;
				checksum_base = finfo->ae_cal_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
				printk(KERN_INFO "[CRC32_DEBUG] AE Cal Data CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
					check_length, finfo->ae_cal_section_crc_addr);
				printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
					finfo->ae_cal_start_addr, finfo->ae_cal_end_addr);
#endif

				if (check_base > address_boundary || checksum_base > address_boundary) {
					err("Camera[%d]: AE Cal Data address has error: start(0x%08X), end(0x%08X)",
						position, finfo->ae_cal_start_addr, finfo->ae_cal_end_addr);
					crc32_check_temp = false;
					goto out;
				}

				checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
				if (checksum != buf32[checksum_base]) {
					err("Camera[%d]: CRC32 error at the AE cal data (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
					crc32_check_temp = false;
					goto out;
				}
			} else {
				pr_warning("Camera[%d]: Skip to check AE Cal Data crc32\n", position);
			}

		}
	}
#endif

#ifdef SAMSUNG_LIVE_OUTFOCUS
	/* DUAL Cal Data CRC CHECK */
	check_length = 0;
	if (rom_addr->rom_dual_checksum_len > 0)
		check_length = rom_addr->rom_dual_checksum_len;

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->dual_data_start_addr / 4;
		checksum_base = finfo->dual_data_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] Dual Cal Data CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->dual_data_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->dual_data_start_addr, finfo->dual_data_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: Dual Cal Data address has error: start(0x%08X), end(0x%08X)",
				position, finfo->dual_data_start_addr, finfo->dual_data_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the DualData (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check Dual Cal Data crc32\n", position);
	}
#endif

#ifdef ENABLE_REMOSAIC_CAPTURE
	/* SENSOR Cal Data CRC CHECK */
	check_length = 0;
	if (rom_addr->rom_sensor_cal_checksum_len > 0)
		check_length = rom_addr->rom_sensor_cal_checksum_len;

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->sensor_cal_data_start_addr / 4;
		checksum_base = finfo->sensor_cal_data_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] Sensor Cal Data. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->sensor_cal_data_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->sensor_cal_data_start_addr, finfo->sensor_cal_data_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: Sensor Cal Data address has error: start(0x%08X), end(0x%08X)",
				position, finfo->sensor_cal_data_start_addr, finfo->sensor_cal_data_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the Sensor Cal Data (0x%08X != 0x%08X)",
				position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check Sensor Cal Data crc32\n", position);
	}
#endif

#ifdef USE_AP_PDAF
	/* PDAF Cal Data CRC CHECK */
	check_length = 0;
	if (rom_addr->rom_pdaf_checksum_len > 0)
		check_length = rom_addr->rom_pdaf_checksum_len;

	if (check_length > 0) {
		checksum = 0;
		check_base = finfo->ap_pdaf_start_addr / 4;
		checksum_base = finfo->ap_pdaf_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] PDAF Cal Data. check_length = %d, crc addr = 0x%08X\n",
			check_length, finfo->ap_pdaf_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			finfo->ap_pdaf_start_addr, finfo->ap_pdaf_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: PDAF Cal Data address has error: start(0x%08X), end(0x%08X)",
				position, finfo->ap_pdaf_start_addr, finfo->ap_pdaf_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: CRC32 error at the PDAF Cal Data (0x%08X != 0x%08X)",
				position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		}
	} else {
		pr_warning("Camera[%d]: Skip to check PDAF Cal Data crc32\n", position);
	}
#endif

out:
	crc32_check_list[position][CRC32_CHECK] = crc32_check_temp;
	crc32_check_list[position][CRC32_CHECK_HEADER] = crc32_header_temp;
	info("Camera[%d]: CRC32 Check Result - crc32_header_check=%d, crc32_check=%d\n",
		position, crc32_header_temp, crc32_check_temp);

	return crc32_check_temp && crc32_header_temp;
}

bool fimc_is_sec_check_otp_crc32(char *buf, int position) {
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksumFromOTP;
	int i;
	bool crc32_check_temp, crc32_header_temp;

	int32_t otp_header_start_addr;
	int32_t otp_header_checksum_addr;
	int32_t otp_header_checksum_len;
	int32_t otp_oem_checksum_addr;
	int32_t otp_oem_checksum_len;
	int32_t otp_awb_checksum_addr;
	int32_t otp_awb_checksum_len;

	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;
	const struct fimc_is_vender_rom_addr *rom_addr = NULL;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	buf32 = (u32 *)buf;

	rom_addr = specific->rom_cal_map_addr[position];

	otp_header_start_addr = rom_addr->rom_header_cal_data_start_addr;
	otp_header_checksum_addr = rom_addr->rom_header_checksum_addr;
	otp_header_checksum_len = rom_addr->rom_header_checksum_len;
	otp_oem_checksum_addr = rom_addr->rom_oem_checksum_addr;
	otp_oem_checksum_len = rom_addr->rom_oem_checksum_len;
	otp_awb_checksum_addr = rom_addr->rom_awb_checksum_addr;
	otp_awb_checksum_len = rom_addr->rom_awb_checksum_len;

	info("%s E\n", __func__);
	/***** Initial Value *****/
	for (i = CRC32_CHECK_HEADER; i < CRC32_SCENARIO_MAX; i++ ) {
		crc32_check_list[position][i] = true;
	}
	crc32_check_temp = true;
	crc32_header_temp = true;

	/***** SKIP CHECK CRC *****/
#ifdef SKIP_CHECK_CRC
	pr_warning("Camera[%d]: Skip check crc32\n", position);

	return crc32_check_temp;
#endif


	/***** HEADER checksum ****************************************************/
#ifdef ROM_CRC32_DEBUG
	printk(KERN_INFO "[CRC32_DEBUG] Header CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
		otp_header_checksum_len, otp_header_checksum_addr);
#endif

	checksumFromOTP = buf[otp_header_checksum_addr] + (buf[otp_header_checksum_addr+1] << 8)
					+ (buf[otp_header_checksum_addr+2] << 16) + (buf[otp_header_checksum_addr+3] << 24);

	checksum = (u32)getCRC((u16 *)&buf32[otp_header_start_addr], otp_header_checksum_len, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_check_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the header data section (0x%08X != 0x%08X)",
				checksum, checksumFromOTP);
		goto out;
	} else {
		crc32_check_temp = crc32_header_temp = true;
		//info("Camera: End checking CRC32 (0x%08X = 0x%08X)", checksum, checksumFromOTP);
	}

	/***** OEM checksum *******************************************************/
#ifdef OTP_OEM_START_ADDR
	if (otp_oem_checksum_addr > 0) {
#ifdef ROM_CRC32_DEBUG
	printk(KERN_INFO "[CRC32_DEBUG] OEM CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
		otp_oem_checksum_len, otp_oem_checksum_addr);
#endif

		checksumFromOTP = buf[otp_oem_checksum_addr] + (buf[otp_oem_checksum_addr+1] << 8)
						+ (buf[otp_oem_checksum_addr+2] << 16) + (buf[otp_oem_checksum_addr+3] << 24);

		checksum = (u32)getCRC((u16 *)&buf32[OTP_OEM_START_ADDR/4], otp_oem_checksum_len, NULL, NULL);

		if(checksum != checksumFromOTP) {
			crc32_check_temp = false;
			err("Camera: CRC32 error at the OEM data section (0x%08X != 0x%08X)",
					checksum, checksumFromOTP);
			goto out;
		} else {
			crc32_check_temp = true;
			//info("Camera: End checking CRC32 (0x%08X = 0x%08X)", checksum, checksumFromOTP);
		}
	}
#endif
		/***** AWB checksum *******************************************************/
#ifdef OTP_AWB_START_ADDR
	if (otp_awb_checksum_addr > 0) {
#ifdef ROM_CRC32_DEBUG
	printk(KERN_INFO "[CRC32_DEBUG] AWB CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
		otp_awb_checksum_len, otp_awb_checksum_addr);
#endif

		checksumFromOTP = buf[otp_awb_checksum_addr] + (buf[otp_awb_checksum_addr+1] << 8)
						+ (buf[otp_awb_checksum_addr+2] << 16) + (buf[otp_awb_checksum_addr+3] << 24);

		checksum = (u32)getCRC((u16 *)&buf32[OTP_AWB_START_ADDR/4], otp_awb_checksum_len, NULL, NULL);

		if(checksum != checksumFromOTP) {
			crc32_check_temp = false;
			err("Camera: CRC32 error at the AWB data section (0x%08X != 0x%08X)",
					checksum, checksumFromOTP);
			goto out;
		} else {
			crc32_check_temp = true;
			//info("Camera: End checking CRC32 (0x%08X = 0x%08X)", checksum, checksumFromOTP);
		}
	}
#endif

out:
	crc32_check_list[position][CRC32_CHECK_HEADER] = crc32_header_temp;
	crc32_check_list[position][CRC32_CHECK] = crc32_check_temp;
	info("Camera[%d]: crc32_header_check=%d, crc32_check=%d\n", position,
		crc32_check_list[position][CRC32_CHECK_HEADER], crc32_check_list[position][CRC32_CHECK]);

	return crc32_check_temp;
}

bool fimc_is_sec_check_cal_crc32(char *buf, int id)
{
	bool ret = true;
	int rom_type = ROM_TYPE_NONE;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;

	if (specific->rom_share[id].check_rom_share == true)
	{
		info("Camera[%d]: skip to check crc(shared rom data)\n", id);
	} else {
		rom_type = specific->rom_data[id].rom_type;

		if (rom_type == ROM_TYPE_EEPROM)
			ret = fimc_is_sec_check_eeprom_crc32(buf, id);
		else if (rom_type == ROM_TYPE_OTPROM)
			ret = fimc_is_sec_check_otp_crc32(buf, id);
		else {
			info("Camera[%d]: not support rom type(%d)\n", id, rom_type);
			ret = false;
		}
	}

	return ret;
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

	info("%s: Sensor position = %d\n", __func__, position);

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

	info("%s: Sensor position = %d\n", __func__, position);

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

int fimc_is_sec_check_caldata_reload(struct fimc_is_core *core)
{
	struct file *reload_key_fp = NULL;
	struct file *supend_resume_key_fp = NULL;
	mm_segment_t old_fs;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	reload_key_fp = filp_open("/data/vendor/camera/reload/r1e2l3o4a5d.key", O_RDONLY, 0);
	if (IS_ERR(reload_key_fp)) {
		reload_key_fp = NULL;
	} else {
		info("Reload KEY exist, reload cal data.\n");
		force_caldata_dump = true;
		specific->suspend_resume_disable = true;
	}

	if (reload_key_fp)
		filp_close(reload_key_fp, current->files);

	supend_resume_key_fp = filp_open("/data/vendor/camera/i1s2p3s4r.key", O_RDONLY, 0);
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

bool fimc_is_sec_readcal_dump(struct fimc_is_vender_specific *specific, char **buf, int size, int position)
{
	int ret = false;
	int rom_position = position;
	int rom_type = ROM_TYPE_NONE;
	int cal_size = 0;
	bool rom_valid = false;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/vendor/camera/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	}

	dump_fp = filp_open("/data/vendor/camera/dump", O_RDONLY, 0);
	if (IS_ERR(dump_fp)) {
		info("dump folder does not exist.\n");
		dump_fp = NULL;
		goto key_err;
	}

	if (specific->rom_share[position].check_rom_share == true)
		rom_position = specific->rom_share[position].share_position;

	rom_valid = specific->rom_data[rom_position].rom_valid;
	rom_type = specific->rom_data[rom_position].rom_type;
	cal_size = specific->rom_cal_map_addr[rom_position]->rom_max_cal_size;

	if(rom_valid == true) {
		char path[50] = FIMC_IS_CAL_SDCARD_PATH;

		if(rom_type == ROM_TYPE_EEPROM) {
			info("dump folder exist, Dump EEPROM cal data.\n");

			strcat(path, eeprom_cal_dump_path[position]);
			if (write_data_to_file(path, buf[0], size, &pos) < 0) {
				info("Failed to rear dump cal data.\n");
				goto dump_err;
			}

			ret = true;
		} else if (rom_type == ROM_TYPE_OTPROM) {
			info("dump folder exist, Dump OTPROM cal data.\n");

			strcat(path, otprom_cal_dump_path);
#ifdef SENSOR_OTP_5E9
			if (write_data_to_file(path, buf[1], 0xf, &pos) < 0) {
				info("Failed to dump cal data.\n");
				goto dump_err;
			}
			if (write_data_to_file_append(path, buf[0], cal_size, &pos) < 0) {
				info("Failed to dump cal data.\n");
				goto dump_err;
			}
#else
			if (write_data_to_file(path, buf[0], cal_size, &pos) < 0) {
				info("Failed to dump cal data.\n");
				goto dump_err;
			}
#endif
			ret = true;
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);

	set_fs(old_fs);

	return ret;
}

int fimc_is_sec_read_eeprom_header(struct device *dev, int position)
{
	int ret = 0;
	int rom_position = position;
	int32_t rom_header_ver_addr;
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;
	struct i2c_client *client = NULL;;

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);

	if (specific->rom_share[position].check_rom_share == true) {
		rom_position = specific->rom_share[position].share_position;
	}

	client = specific->rom_client[rom_position];
	if (!client) {
		err("(%s)eeprom i2c client is NULL\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (specific->rom_cal_map_addr[rom_position] == NULL) {
		err("(%s)rom_cal_map(%d) is NULL\n", __func__, rom_position);
		ret = -EINVAL;
		goto exit;
	}

	rom_header_ver_addr = specific->rom_cal_map_addr[rom_position]->rom_header_main_module_info_start_addr;
	/* if it is used common rom */
	if (position == SENSOR_POSITION_REAR2 || position == SENSOR_POSITION_FRONT2) {
		if (specific->rom_cal_map_addr[rom_position]->rom_header_sub_module_info_start_addr > 0) {
			rom_header_ver_addr = specific->rom_cal_map_addr[rom_position]->rom_header_sub_module_info_start_addr;
		}
	}

	if (rom_header_ver_addr < 0) {
		err("(%s)rom_header_ver_addr is NULL(%d)\n", __func__, rom_header_ver_addr);
		goto exit;
	}

	fimc_is_i2c_config(client, true);

	ret = fimc_is_i2c_read(client, header_version, (u32)rom_header_ver_addr, FIMC_IS_HEADER_VER_SIZE);

	//fimc_is_i2c_config(client, false); /* not used 'default' */

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(finfo->header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

exit:
	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	int rom_position = position;
	int cal_size = 0;
	char *buf = NULL;
	char *cal_buf[2] = {NULL, NULL};
	bool rom_common = false;

#ifdef USE_AE_CAL
	struct rom_ae_cal_data *ae_cal_data = NULL;
#endif

	int32_t cal_map_ver_start_addr, header_version_start_addr;
	int32_t temp_start_addr = 0, temp_end_addr = 0;

	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;
	struct i2c_client *client = NULL;

	const struct fimc_is_vender_rom_addr *rom_addr = NULL;

	if (specific->rom_share[position].check_rom_share == true) {
		rom_position = specific->rom_share[position].share_position;
		rom_common = true;
	}

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);

	cal_size = fimc_is_sec_get_max_cal_size(core, rom_position);

	if (!cal_size) {
		err("(%s) rom_[%d] cal_size is zero\n", __func__, rom_position);
		ret = -EINVAL;
		goto exit;
	}

	client = specific->rom_client[rom_position];
	if (!client) {
		err("(%s)eeprom i2c client is NULL\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_i2c_config(client, true);
	rom_addr = specific->rom_cal_map_addr[rom_position];

	cal_map_ver_start_addr = rom_addr->rom_header_cal_map_ver_start_addr;

	if (rom_common == true) {
		header_version_start_addr = rom_addr->rom_header_sub_module_info_start_addr;
	} else {
		header_version_start_addr = rom_addr->rom_header_main_module_info_start_addr;
	}

	info("Camera[%d]: ca_map_ver_addr = %#x, header_version_addr = %#x\n",
		position, cal_map_ver_start_addr, header_version_start_addr);

	if(cal_map_ver_start_addr < 0 || header_version_start_addr < 0)
		goto exit;

	ret = fimc_is_i2c_read(client, finfo->cal_map_ver, (u32)cal_map_ver_start_addr, FIMC_IS_CAL_MAP_VER_SIZE);
	ret = fimc_is_i2c_read(client, finfo->header_ver, (u32)header_version_start_addr, FIMC_IS_HEADER_VER_SIZE);

	//fimc_is_i2c_config(client, false); /* not used 'default' */

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera[%d]: EEPROM Cal map_version = %c%c%c%c\n", position,
		finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	if (!fimc_is_sec_check_rom_ver(core, position)) {
		info("Camera[%d]: Do not read eeprom cal data. EEPROM version is low.\n", position);
		return 0;
	}

crc_retry:
	info("Camera[%d]: I2C read cal data\n", position);
	fimc_is_i2c_config(client, true);

	ret = fimc_is_i2c_read(client, buf, 0x0, cal_size);

	//fimc_is_i2c_config(client, false); /* not used 'default' */

	if (ret) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("Camera[%d]: EEPROM Header Version = %s\n", position, finfo->header_ver);

/////////////////////////////////////////////////////////////////////////////////////
/* Header Data */
/////////////////////////////////////////////////////////////////////////////////////
	/* Header Data: OEM */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_oem_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_oem_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_oem_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_oem_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_oem_start_addr;
			temp_end_addr = rom_addr->rom_header_main_oem_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo->oem_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->oem_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("OEM start = 0x%08x, end = 0x%08x\n", finfo->oem_start_addr, finfo->oem_end_addr);
	}

	/* Header Data: AWB */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_awb_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_awb_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_awb_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_awb_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_awb_start_addr;
			temp_end_addr = rom_addr->rom_header_main_awb_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo->awb_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->awb_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("AWB start = 0x%08x, end = 0x%08x\n", finfo->awb_start_addr, finfo->awb_end_addr);
	}

	/* Header Data: Shading */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_shading_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_shading_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_shading_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_shading_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_shading_start_addr;
			temp_end_addr = rom_addr->rom_header_main_shading_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo->shading_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->shading_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("Shading start = 0x%08x, end = 0x%08x\n", finfo->shading_start_addr, finfo->shading_end_addr);
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
	}

#ifdef USE_AE_CAL
	/* Header Data: AE CAL DATA */
	if (rom_addr->extend_cal_addr) {
		ae_cal_data = (struct rom_ae_cal_data *)fimc_is_sec_search_rom_extend_data(rom_addr->extend_cal_addr, EXTEND_AE_CAL);
		if(ae_cal_data) {
			temp_start_addr = temp_end_addr = -1;
			if (ae_cal_data->rom_header_main_ae_start_addr >= 0) {
				temp_start_addr = ae_cal_data->rom_header_main_ae_start_addr;
				temp_end_addr = ae_cal_data->rom_header_main_ae_end_addr;
			}

			if (temp_start_addr >= 0) {
				finfo->ae_cal_start_addr = *((u32 *)&buf[temp_start_addr]);
				finfo->ae_cal_end_addr = *((u32 *)&buf[temp_end_addr]);
				info("AE Cal Data start = 0x%08x, end = 0x%08x\n", finfo->ae_cal_start_addr, finfo->ae_cal_end_addr);
			}
		}
	}
#endif

#ifdef ENABLE_REMOSAIC_CAPTURE
	/* Header Data: Sensor CAL (CrossTalk & LSC) */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_main_sensor_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_main_sensor_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_main_sensor_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo->sensor_cal_data_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->sensor_cal_data_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("Sensor Cal Data start = 0x%08x, end = 0x%08x\n",
			finfo->sensor_cal_data_start_addr, finfo->sensor_cal_data_end_addr);
	}
#endif

#ifdef SAMSUNG_LIVE_OUTFOCUS
	/* Header Data: Dual CAL */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_dual_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_dual_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_dual_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo->dual_data_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->dual_data_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("Dual Cal Data start = 0x%08x, end = 0x%08x\n", finfo->dual_data_start_addr, finfo->dual_data_end_addr);
	}
#endif

#ifdef USE_AP_PDAF
	/* Header Data: Dual CAL */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_pdaf_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_pdaf_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_pdaf_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo->ap_pdaf_start_addr = *((u32 *)&buf[temp_start_addr]);
		finfo->ap_pdaf_end_addr = *((u32 *)&buf[temp_end_addr]);
		info("PDAF Cal Data start = 0x%08x, end = 0x%08x\n", finfo->ap_pdaf_start_addr, finfo->ap_pdaf_end_addr);
	}
#endif

	/* Header Data: Header Module Info */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_module_info_start_addr;
		}
	} else {
		if (rom_addr->rom_header_main_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_module_info_start_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->header_ver, &buf[temp_start_addr], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	}

	/* Header Data: CAL MAP Version */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_cal_map_ver_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_cal_map_ver_start_addr;

		memcpy(finfo->cal_map_ver, &buf[temp_start_addr], FIMC_IS_CAL_MAP_VER_SIZE);
	}

	/* Header Data: PROJECT NAME */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_project_name_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_project_name_start_addr;

		memcpy(finfo->project_name, &buf[temp_start_addr], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	}

	/* Header Data: MODULE ID */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_module_id_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_module_id_addr;

		memcpy(finfo->rom_module_id, &buf[temp_start_addr], FIMC_IS_MODULE_ID_SIZE);
	} else {
		memset(finfo->rom_module_id, 0x0, FIMC_IS_MODULE_ID_SIZE);
	}

	/* Header Data: SENSOR ID */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_sensor_id_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_sensor_id_addr;
		}
	} else {
		if (rom_addr->rom_header_main_sensor_id_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_sensor_id_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->rom_sensor_id, &buf[temp_start_addr], FIMC_IS_SENSOR_ID_SIZE);
		finfo->rom_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
	}

	/* Header Data: MTF Data (Resolution) */
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_header_sub_mtf_data_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_mtf_data_addr;
		}
	} else {
		if (rom_addr->rom_header_main_mtf_data_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_mtf_data_addr;
		}
	}

	if (temp_start_addr >= 0)
			finfo->mtf_data_addr = temp_start_addr;

	/* Header Data: HEADER CAL CHECKSUM */
	if (rom_addr->rom_header_checksum_addr >= 0)
		finfo->header_section_crc_addr = rom_addr->rom_header_checksum_addr;

/////////////////////////////////////////////////////////////////////////////////////
/* OEM Data: OEM Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_sub_oem_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_sub_oem_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_oem_checksum_addr;
		}
	} else {
		if (rom_addr->rom_oem_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_oem_module_info_start_addr;
			temp_end_addr = rom_addr->rom_oem_checksum_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->oem_ver, &buf[temp_start_addr], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = temp_end_addr;
	}

	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_sub_oem_af_inf_position_addr >= 0) {
			temp_start_addr = rom_addr->rom_sub_oem_af_inf_position_addr;
			temp_end_addr = rom_addr->rom_sub_oem_af_macro_position_addr;
		}
	} else {
		if (rom_addr->rom_oem_af_inf_position_addr >= 0) {
			temp_start_addr = rom_addr->rom_oem_af_inf_position_addr;
			temp_end_addr = rom_addr->rom_oem_af_macro_position_addr;
		}
	}

	if (temp_start_addr >= 0)
		finfo->af_cal_pan = *((u32 *)&buf[temp_start_addr]);
	if (temp_end_addr >= 0)
		finfo->af_cal_macro = *((u32 *)&buf[temp_end_addr]);

/////////////////////////////////////////////////////////////////////////////////////
/* AWB Data: AWB Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_sub_awb_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_sub_awb_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_awb_checksum_addr;
		}
	} else {
		if (rom_addr->rom_oem_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_awb_module_info_start_addr;
			temp_end_addr = rom_addr->rom_awb_checksum_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->awb_ver, &buf[temp_start_addr], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* SHADING Data: Shading Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_common == true) {
		if (rom_addr->rom_sub_shading_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_sub_shading_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_shading_checksum_addr;
		}
	} else {
		if (rom_addr->rom_shading_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_shading_module_info_start_addr;
			temp_end_addr = rom_addr->rom_shading_checksum_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->shading_ver, &buf[temp_start_addr], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* AE Data: AE Module Info */
/////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_AE_CAL
	if(ae_cal_data) {
		temp_start_addr = temp_end_addr = -1;
		if (ae_cal_data->rom_ae_module_info_start_addr >= 0) {
			temp_start_addr = ae_cal_data->rom_ae_module_info_start_addr;
			temp_end_addr = ae_cal_data->rom_ae_checksum_addr;
		}

		if (temp_start_addr >= 0) {
			memcpy(finfo->ae_cal_ver, &buf[temp_start_addr], FIMC_IS_AE_CAL_VER_SIZE);
			finfo->ae_cal_ver[FIMC_IS_AE_CAL_VER_SIZE] = '\0';
			finfo->ae_cal_section_crc_addr = temp_end_addr;
		}
	}
#endif

/////////////////////////////////////////////////////////////////////////////////////
/* Dual CAL Data: Dual cal Module Info */
/////////////////////////////////////////////////////////////////////////////////////
#ifdef SAMSUNG_LIVE_OUTFOCUS
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_dual_module_info_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_dual_module_info_start_addr;
		temp_end_addr = rom_addr->rom_dual_checksum_addr;
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->dual_data_ver, &buf[temp_start_addr], FIMC_IS_DUAL_CAL_VER_SIZE);
		finfo->dual_data_ver[FIMC_IS_DUAL_CAL_VER_SIZE] = '\0';
		finfo->dual_data_section_crc_addr = temp_end_addr;
	}
#endif

/////////////////////////////////////////////////////////////////////////////////////
/* Sensor CAL Data: Sensor CAL Module Info */
/////////////////////////////////////////////////////////////////////////////////////
#ifdef ENABLE_REMOSAIC_CAPTURE
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_sensor_cal_module_info_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_sensor_cal_module_info_start_addr;
		temp_end_addr = rom_addr->rom_sensor_cal_checksum_addr;
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->sensor_cal_data_ver, &buf[temp_start_addr], FIMC_IS_SENSOR_CAL_DATA_VER_SIZE);
		finfo->sensor_cal_data_ver[FIMC_IS_SENSOR_CAL_DATA_VER_SIZE] = '\0';
		finfo->sensor_cal_data_section_crc_addr = temp_end_addr;
	}
#endif

/////////////////////////////////////////////////////////////////////////////////////
/* PDAF CAL Data: PDAF Module Info */
/////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_AP_PDAF
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_pdaf_module_info_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_pdaf_module_info_start_addr;
		temp_end_addr = rom_addr->rom_pdaf_checksum_addr;
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo->ap_pdaf_ver, &buf[temp_start_addr], FIMC_IS_AP_PDAF_VER_SIZE);
		finfo->ap_pdaf_ver[FIMC_IS_AP_PDAF_VER_SIZE] = '\0';
		finfo->ap_pdaf_section_crc_addr = temp_end_addr;
	}
#endif

	/* debug info dump */
#if defined(ROM_DEBUG)
	info("++++ EEPROM[%d] data info\n", position);
	info("1. Header info\n");
	info(" Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE], finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM], finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info(" project_name : %s\n", finfo->project_name);
	info(" Cal data map ver : %s\n", finfo->cal_map_ver);
	info(" Module ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
		finfo->rom_module_id[0], finfo->rom_module_id[1], finfo->rom_module_id[2],
		finfo->rom_module_id[3], finfo->rom_module_id[4], finfo->rom_module_id[5],
		finfo->rom_module_id[6], finfo->rom_module_id[7], finfo->rom_module_id[8],
		finfo->rom_module_id[9]);
	info("2. OEM info\n");
	info(" Module info : %s\n", finfo->oem_ver);
	info("3. AWB info\n");
	info(" Module info : %s\n", finfo->awb_ver);
	info("4. Shading info\n");
	info(" Module info : %s\n", finfo->shading_ver);

#ifdef USE_AE_CAL
	if(ae_cal_data) {
		if (ae_cal_data->rom_ae_module_info_start_addr >= 0) {
			info("0. AE Data info\n");
			info(" Module info : %s\n", finfo->ae_cal_ver);
		}
	}
#endif

#ifdef SAMSUNG_LIVE_OUTFOCUS
	if (rom_addr->rom_dual_module_info_start_addr >= 0) {
		info("0. Dual Data info\n");
		info(" Module info : %s\n", finfo->dual_data_ver);
	}
#endif

#ifdef ENABLE_REMOSAIC_CAPTURE
	if (rom_addr->rom_sensor_cal_module_info_start_addr >= 0) {
		info("0. Sensor Cal data info\n");
		info(" Module info : %s\n", finfo->sensor_cal_data_ver);
	}
#endif

#ifdef USE_AP_PDAF
	if (rom_addr->rom_pdaf_module_info_start_addr >= 0) {
		info("0. PDAF info\n");
		info(" Module info : %s\n", finfo->ap_pdaf_ver);
	}
#endif

#endif	//ROM_DEBUG

	info("---- EEPROM[%d] data info\n", position);

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (finfo->header_ver[3] == 'L')
		crc32_check_list[position][CRC32_CHECK_FACTORY] = crc32_check_list[position][CRC32_CHECK];
	else
		crc32_check_list[position][CRC32_CHECK_FACTORY] = false;


	if (specific->use_module_check) {
		/* Check this module is latest */
		if (sysfs_finfo[position].header_ver[10] >= rom_addr->camera_module_es_version) {
			check_latest_cam_module[position] = true;
		} else {
			check_latest_cam_module[position] = false;
		}
		/* Check this module is final for manufacture */
		if(sysfs_finfo[position].header_ver[10] == FIMC_IS_LATEST_ROM_VERSION_M) {
			check_final_cam_module[position] = true;
		} else {
			check_final_cam_module[position] = false;
		}
	} else {
		check_latest_cam_module[position] = true;
		check_final_cam_module[position] = true;
	}

	cal_buf[0] = buf;
	fimc_is_sec_readcal_dump(specific, cal_buf, cal_size, position);

exit:
	return ret;
}

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

int fimc_is_i2c_read_otp_5e9(struct fimc_is_core *core, struct i2c_client *client,
							void *buf, u32 start_addr, size_t size, int position)
{
	int page_num = 0;
	int reg_count = 0;
	int index = 0;
	int ret = 0;
	int32_t header_start_addr = 0;

	struct fimc_is_vender_specific *specific = core->vender.private_data;
	const struct fimc_is_vender_rom_addr *rom_addr = specific->rom_cal_map_addr[position];

	if (!rom_addr) {
		err("%s: otp_%d There is no cal map\n", __func__, position);
		ret = -EINVAL;
		return ret;
	}

	header_start_addr = rom_addr->rom_header_cal_data_start_addr;

	page_num = OTP_5E9_GET_PAGE(start_addr,OTP_PAGE_START_ADDR, header_start_addr);
	reg_count = OTP_5E9_GET_REG(start_addr,OTP_PAGE_START_ADDR, header_start_addr);
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
	int retry = FIMC_IS_CAL_RETRY_CNT;
	int cal_size = 0;
	char *buf = NULL;
	char temp_cal_buf[0x10] = {0};
	char *cal_buf[2] = {NULL, NULL};
	u8 otp_bank = 0;
	u16 start_addr = 0;

	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_rom_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct i2c_client *client = NULL;
	const struct fimc_is_vender_rom_addr *rom_addr = specific->rom_cal_map_addr[position];

	if (!rom_addr) {
		err("%s: otp_%d There is no cal map\n", __func__, position);
		ret = -EINVAL;
		goto exit;
	}

	info("fimc_is_sec_readcal_otprom_5e9 E\n");

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);
	cal_size = fimc_is_sec_get_max_cal_size(core, position);

	client = specific->rom_client[position];

	if (!client) {
		err("cis i2c client is NULL\n");
		return -EINVAL;
	}

	fimc_is_i2c_config(client, true);
	msleep(10);

	/* 0. write Sensor Init(global) */
	ret = fimc_is_sec_set_registers(client, sensor_Global, sensor_Global_size);
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
	info("otp_bank = %d\n", otp_bank);

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

	info("otp_start_addr = %x\n", start_addr);
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
	info("I2C read cal data\n\n");
	fimc_is_i2c_read_otp_5e9(core, client, buf, start_addr, OTP_USED_CAL_SIZE, position);

	/* HEARDER Data : Module/Manufacturer Information */
	memcpy(finfo->header_ver, &buf[rom_addr->rom_header_main_module_info_start_addr], FIMC_IS_HEADER_VER_SIZE);
	finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	/* HEARDER Data : Cal Map Version */
	memcpy(finfo->cal_map_ver, &buf[rom_addr->rom_header_cal_map_ver_start_addr], FIMC_IS_CAL_MAP_VER_SIZE);

	info("OTPROM header version = %s\n", finfo->header_ver);

	if (rom_addr->rom_header_main_oem_start_addr > 0) {
		finfo->oem_start_addr = *((u32 *)&buf[rom_addr->rom_header_main_oem_start_addr]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[rom_addr->rom_header_main_oem_end_addr]) - start_addr;
		info("OEM start = 0x%08x, end = 0x%08x\n", (finfo->oem_start_addr), (finfo->oem_end_addr));
	}

	if (rom_addr->rom_header_main_awb_start_addr > 0) {
		finfo->awb_start_addr = *((u32 *)&buf[rom_addr->rom_header_main_awb_start_addr]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[rom_addr->rom_header_main_awb_end_addr]) - start_addr;
		info("AWB start = 0x%08x, end = 0x%08x\n", (finfo->awb_start_addr), (finfo->awb_end_addr));
	}

	if (rom_addr->rom_header_main_shading_start_addr > 0) {
		finfo->shading_start_addr = *((u32 *)&buf[rom_addr->rom_header_main_shading_start_addr]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[rom_addr->rom_header_main_shading_start_addr]) - start_addr;
		info("Shading start = 0x%08x, end = 0x%08x\n", (finfo->shading_start_addr), (finfo->shading_end_addr));
	}

	if (rom_addr->rom_header_module_id_addr > 0) {
		memcpy(finfo->rom_module_id, &buf[rom_addr->rom_header_module_id_addr], FIMC_IS_MODULE_ID_SIZE);
		finfo->rom_module_id[FIMC_IS_MODULE_ID_SIZE] = '\0';
	}

	if (rom_addr->rom_header_main_sensor_id_addr > 0) {
		memcpy(finfo->rom_sensor_id, &buf[rom_addr->rom_header_main_sensor_id_addr], FIMC_IS_SENSOR_ID_SIZE);
		finfo->rom_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
	}

	if (rom_addr->rom_header_project_name_start_addr > 0) {
		memcpy(finfo->project_name, &buf[rom_addr->rom_header_project_name_start_addr], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	}

	if (rom_addr->rom_header_checksum_addr > 0) {
		finfo->header_section_crc_addr = rom_addr->rom_header_checksum_addr;
	}

	if (rom_addr->rom_header_main_mtf_data_addr > 0) {
		finfo->mtf_data_addr = rom_addr->rom_header_main_mtf_data_addr;
	}

	if (rom_addr->rom_oem_module_info_start_addr > 0) {
		memcpy(finfo->oem_ver, &buf[rom_addr->rom_oem_module_info_start_addr], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = rom_addr->rom_oem_checksum_addr;
	}

	if (rom_addr->rom_awb_module_info_start_addr > 0) {
		memcpy(finfo->awb_ver, &buf[rom_addr->rom_awb_module_info_start_addr], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = rom_addr->rom_awb_checksum_addr;
	}

	if (rom_addr->rom_shading_module_info_start_addr > 0) {
		memcpy(finfo->shading_ver, &buf[rom_addr->rom_shading_module_info_start_addr], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = rom_addr->rom_shading_checksum_addr;
	}

	if (rom_addr->rom_oem_af_inf_position_addr && rom_addr->rom_oem_af_macro_position_addr) {
		finfo->af_cal_pan = *((u32 *)&buf[rom_addr->rom_oem_af_inf_position_addr]);
		finfo->af_cal_macro = *((u32 *)&buf[rom_addr->rom_oem_af_macro_position_addr]);
	}

	if(finfo->cal_map_ver[0] != 'V') {
		info("Camera: Cal Map version read fail or there's no available data.\n");
		/* it for case of CRC fail at re-work module  */
		if(retry == FIMC_IS_CAL_RETRY_CNT && otp_bank != 0x1) {
			start_addr -= 0xf;
			info("%s : change start address(%x)\n",__func__,start_addr);
			retry--;
			goto crc_retry;
		}
		crc32_check_list[position][CRC32_CHECK_FACTORY] = false;

		goto exit;
	}

	printk(KERN_INFO "Camera[%d]: OTPROM Cal map_version = %c%c%c%c\n", position,
		finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	/* debug info dump */
#ifdef ROM_DEBUG
	info("++++ OTPROM data info\n");
	info(" Header info\n");
	info(" Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE], finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM], finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info(" Module ID : %c%c%c%c%c%X%X%X%X%X\n",
			finfo->rom_module_id[0], finfo->rom_module_id[1], finfo->rom_module_id[2],
			finfo->rom_module_id[3], finfo->rom_module_id[4], finfo->rom_module_id[5],
			finfo->rom_module_id[6], finfo->rom_module_id[7], finfo->rom_module_id[8],
			finfo->rom_module_id[9]);
#endif
	info("---- OTPROM data info\n");

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	/* 6. return to original mode */
	ret = fimc_is_sec_set_registers(client,
	sensor_mode_change_from_OTP_reg, sensor_mode_change_from_OTP_reg_size);
	if (unlikely(ret)) {
		err("failed to fimc_is_sec_set_registers (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	if (finfo->header_ver[3] == 'L')
		crc32_check_list[position][CRC32_CHECK_FACTORY] = crc32_check_list[position][CRC32_CHECK];
	else
		crc32_check_list[position][CRC32_CHECK_FACTORY] = false;

	if (specific->use_module_check) {
		/* Check this module is latest */
		if (sysfs_finfo[position].header_ver[10] >= rom_addr->camera_module_es_version) {
			check_latest_cam_module[position] = true;
		} else {
			check_latest_cam_module[position] = false;
		}
		/* Check this module is final for manufacture */
		if(sysfs_finfo[position].header_ver[10] == FIMC_IS_LATEST_ROM_VERSION_M) {
			check_final_cam_module[position] = true;
		} else {
			check_final_cam_module[position] = false;
		}
	} else {
		check_latest_cam_module[position] = true;
		check_final_cam_module[position] = true;
	}

	/* For CAL DUMP */
	fimc_is_i2c_read_otp_5e9(core, client, temp_cal_buf, OTP_PAGE_START_ADDR, 0xf, position);
	cal_buf[0] = buf;
	cal_buf[1] = temp_cal_buf;
	fimc_is_sec_readcal_dump(specific, cal_buf, cal_size, position);

exit:
	//fimc_is_i2c_config(client, false);  /* not used 'default' */

	info("%s X\n", __func__);
	return ret;
}
#else //SENSOR_OTP_5E9 end

/* 'fimc_is_sec_readcal_otprom_legacy' is not modified to MCD_V2 yet.
 *  If it is used to this fuction, must modify */
#if 0
int fimc_is_sec_readcal_otprom_legacy(struct device *dev, int position)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	int cal_size = 0;
	char *buf = NULL;
	char *cal_buf[2] = {NULL, NULL};
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_rom_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct i2c_client *client = NULL;

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

	info("fimc_is_sec_readcal_otprom E\n");

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &buf);
	cal_size = fimc_is_sec_get_max_cal_size(core, position);

	client = specific->rom_client[position];

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
	info("Camera: otp_bank = %d\n", otp_bank);

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
#if defined(SENSOR_OTP_HYNIX)
	fimc_is_sensor_write8(client, OTP_READ_START_ADDR_HIGH, (OTP_BANK_ADDR>>8)&0xff);
	fimc_is_sensor_write8(client, OTP_READ_START_ADDR_LOW, (OTP_BANK_ADDR)&0xff);
	fimc_is_sensor_write8(client, OTP_READ_MODE_ADDR, 0x01);
	fimc_is_sensor_read8(client, OTP_READ_ADDR, &data8);
#else
	fimc_is_sensor_read8(client, OTP_BANK_ADDR, &data8);
#endif
	otp_bank = data8;
	info("Camera: otp_bank = %d\n", otp_bank);
#if defined(SENSOR_OTP_HYNIX)
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

	info("Camera: otp_start_addr = %x\n", start_addr);

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
	info("Camera: I2C read full cal data\n\n");
	for (i = 0; i < OTP_USED_CAL_SIZE; i++) {
		fimc_is_sensor_read8(client, OTP_SINGLE_READ_ADDR, &data8);
		buf[i] = data8;
	}
#else
	/* read cal data */
	info("Camera: I2C read cal data\n\n");
#if defined(SENSOR_OTP_HYNIX)
	fimc_is_i2c_read_burst(client, buf, start_addr, OTP_USED_CAL_SIZE);
#else
	fimc_is_i2c_read(client, buf, start_addr, OTP_USED_CAL_SIZE);
#endif
#endif
#endif

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		info("FRONT OTPROM header version = %s\n", finfo->header_ver);
#if defined(OTP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[OTP_HEADER_OEM_START_ADDR_FRONT]) - start_addr;
		finfo->oem_end_addr = *((u32 *)&buf[OTP_HEADER_OEM_END_ADDR_FRONT]) - start_addr;
		info("OEM start = 0x%08x, end = 0x%08x\n",
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
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_SHADING_START_ADDR_FRONT)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR_FRONT]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR_FRONT]) - start_addr;
		info("Shading start = 0x%08x, end = 0x%08x\n",
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
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#if defined(OTP_HEADER_AWB_START_ADDR)
		finfo->awb_start_addr = *((u32 *)&buf[OTP_HEADER_AWB_START_ADDR]) - start_addr;
		finfo->awb_end_addr = *((u32 *)&buf[OTP_HEADER_AWB_END_ADDR]) - start_addr;
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
#endif
#if defined(OTP_HEADER_AP_SHADING_START_ADDR)
		finfo->shading_start_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_START_ADDR]) - start_addr;
		finfo->shading_end_addr = *((u32 *)&buf[OTP_HEADER_AP_SHADING_END_ADDR]) - start_addr;
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		info("Shading start = 0x%08x, end = 0x%08x\n",
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
		info("Camera: Cal Map version read fail or there's no available data.\n");
		crc32_check_factory_front = false;
		goto exit;
	}

	info("Camera: OTPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	/* debug info dump */
	info("++++ OTPROM data info\n");
	info(" Header info\n");
	info(" Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE], finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM], finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info(" Module ID : %c%c%c%c%c%X%X%X%X%X\n",
			finfo->eeprom_front_module_id[0], finfo->eeprom_front_module_id[1], finfo->eeprom_front_module_id[2],
			finfo->eeprom_front_module_id[3], finfo->eeprom_front_module_id[4], finfo->eeprom_front_module_id[5],
			finfo->eeprom_front_module_id[6], finfo->eeprom_front_module_id[7], finfo->eeprom_front_module_id[8],
			finfo->eeprom_front_module_id[9]);

	info("---- OTPROM data info\n");

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		if (!fimc_is_sec_check_front_otp_crc32(buf) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	}

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

	cal_buf[0] = buf;
	fimc_is_sec_readcal_dump(specific, cal_buf, cal_size, position);

exit:
	//fimc_is_i2c_config(client, false);  /* not used 'default' */

	info("fimc_is_sec_readcal_otprom X\n");

	return ret;
}
#endif //if 0
#endif //!SENSOR_OTP_5E9

int fimc_is_sec_readcal_otprom(struct device *dev, int position)
{
	int ret = 0;
#if defined(SENSOR_OTP_5E9)
	ret = fimc_is_sec_readcal_otprom_5e9(dev, position);
#else
	//ret = fimc_is_sec_readcal_otprom_legacy(dev, position);
#endif
	return ret;
}

int fimc_is_sec_fw_find_rear(struct fimc_is_core *core, int position)
{
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;
	u32 *sensor_id = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);

	if (position == SENSOR_POSITION_REAR3)
		sensor_id = &specific->rear3_sensor_id;
	else if (position == SENSOR_POSITION_REAR2)
		sensor_id = &specific->rear2_sensor_id;
	else
		sensor_id = &specific->rear_sensor_id;

	if (position == SENSOR_POSITION_REAR) {
		snprintf(finfo->load_fw_name, sizeof(FIMC_IS_DDK), "%s", FIMC_IS_DDK);
#if defined(USE_RTA_BINARY)
		snprintf(finfo->load_rta_fw_name, sizeof(FIMC_IS_RTA), "%s", FIMC_IS_RTA);
#endif
	}

	/* default firmware and setfile */
	if (*sensor_id == SENSOR_NAME_IMX576) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX576_SETF), "%s", FIMC_IS_IMX576_SETF);
	} else if (*sensor_id == SENSOR_NAME_S5K4HA) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_4HA_SETF), "%s", FIMC_IS_4HA_SETF);
	} else if (*sensor_id == SENSOR_NAME_S5K5E9) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_5E9_SETF), "%s", FIMC_IS_5E9_SETF);
	} else if (*sensor_id == SENSOR_NAME_GC5035) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_GC5035_SETF), "%s", FIMC_IS_GC5035_SETF);
	} else if (*sensor_id == SENSOR_NAME_IMX582) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX582_SETF), "%s", FIMC_IS_IMX582_SETF);
	} else if (*sensor_id == SENSOR_NAME_HI1336) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_HI1336_SETF), "%s", FIMC_IS_HI1336_SETF);
	} else if (*sensor_id == SENSOR_NAME_IMX586) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX586_SETF), "%s", FIMC_IS_IMX586_SETF);
	} else if (*sensor_id == SENSOR_NAME_S5K3L6) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_3L6_SETF), "%s", FIMC_IS_3L6_SETF);
	} else if (*sensor_id == SENSOR_NAME_S5KGM2) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_GM2_SETF), "%s", FIMC_IS_GM2_SETF);
	} else {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX576_SETF), "%s", FIMC_IS_IMX576_SETF);
	}

/*
	// This is for module dualization
	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_2P6_Q)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2P6_SETF), "%s", FIMC_IS_2P6_SETF);
		*sensor_id = SENSOR_NAME_S5K2P6;
	}
*/

	info("Camera[%d]%s: sensor id [%d]. load setfile [%s]\n",
			position, __func__, *sensor_id, finfo->load_setfile_name);

	return 0;
}

int fimc_is_sec_fw_find_front(struct fimc_is_core *core, int position)
{
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);

	/* default firmware and setfile */
	if (specific->front_sensor_id == SENSOR_NAME_S5K2X5) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2X5_SETF), "%s", FIMC_IS_2X5_SETF);
	} else if (specific->front_sensor_id == SENSOR_NAME_IMX616) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_IMX616_SETF), "%s", FIMC_IS_IMX616_SETF);
	} else if (specific->front_sensor_id == SENSOR_NAME_HI1336) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_HI1336_SETF), "%s", FIMC_IS_HI1336_SETF);
	} else if (specific->front_sensor_id == SENSOR_NAME_S5K3P8SP) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_3P8SP_SETF), "%s", FIMC_IS_3P8SP_SETF);
	} else {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_2X5_SETF), "%s", FIMC_IS_2X5_SETF);
	}

/*
	// This for the module dualization
	if (fimc_is_sec_fw_module_compare(finfo->header_ver, FW_3P8SP_P)) {
		snprintf(finfo->load_setfile_name, sizeof(FIMC_IS_3P8SP_SETF), "%s", FIMC_IS_3P8SP_SETF);
		specific->front_sensor_id = SENSOR_NAME_S5K3P8SP;
	}
*/

	info("Camera[%d]%s: sensor id [%d]. load setfile [%s]\n",
			position, __func__, specific->front_sensor_id, finfo->load_setfile_name);
	return 0;
}

int fimc_is_sec_fw_find(struct fimc_is_core *core, int position)
{
	int ret = 0;


	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		ret = fimc_is_sec_fw_find_front(core, position);
	}
	else {
		ret = fimc_is_sec_fw_find_rear(core,position);
	}

	return ret;
}

int fimc_is_sec_run_fw_sel(struct device *dev, int position)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;
	struct fimc_is_rom_info *default_finfo = NULL;
	char stat_buf[16], allstat_buf[128];
	int i, ret = 0;
	int rom_position = position;

	if (fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo)) {
		err("failed get finfo. plz check position %d", position);
		return -ENXIO;
	}
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_REAR, &default_finfo);

	/* Check reload cal data enabled */
	if (!default_finfo->is_check_cal_reload) {
		if (fimc_is_sec_file_exist("/data/vendor/camera/")) {
			fimc_is_sec_check_caldata_reload(core);
			default_finfo->is_check_cal_reload = true;
		}
	}

	if (position == SENSOR_POSITION_SECURE) {
		err("Not Support CAL ROM. [%d]", position);
		goto p_err;
	}

	if (specific->rom_share[position].check_rom_share == true)
		rom_position = specific->rom_share[position].share_position;

	if (default_finfo->is_caldata_read == false || force_caldata_dump) {
		ret = fimc_is_sec_run_fw_sel_from_rom(dev, SENSOR_POSITION_REAR, true);
		if (ret < 0) {
			err("failed to select firmware (%d)", ret);
			goto p_err;
		}
	}

	if (specific->rom_data[position].is_rom_read == false || force_caldata_dump) {
		ret = fimc_is_sec_run_fw_sel_from_rom(dev, position, false);
		if (ret < 0) {
			err("failed to select firmware (%d)", ret);
			goto p_err;
		}
	}

	memset(allstat_buf, 0x0, sizeof(allstat_buf));
	for (i = SENSOR_POSITION_REAR; i < SENSOR_POSITION_MAX; i++) {
		if (specific->rom_data[i].rom_valid == true || specific->rom_share[i].check_rom_share == true) {
			snprintf(stat_buf, sizeof(stat_buf), "[%d - %d] ", i, specific->rom_data[i].is_rom_read);
			strcat(allstat_buf, stat_buf);
		}
	}
	info("%s: Position status %s", __func__, allstat_buf);

p_err:
	if (specific->check_sensor_vendor) {
		if (fimc_is_sec_check_rom_ver(core, position)) {
			if (finfo->header_ver[3] != 'L') {
				err("Not supported module(position %d). Module ver = %s", position, finfo->header_ver);
				return -EIO;
			}
		}
	}

	return ret;
}

int fimc_is_sec_write_phone_firmware(int id)
{
	int ret = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	char fw_path[100];
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct fimc_is_rom_info *finfo = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(id, &finfo);

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

	strncpy(phone_fw_version, temp_buf + nread - (FIMC_IS_HEADER_VER_SIZE + IS_SIGNATURE_LEN), FIMC_IS_HEADER_VER_SIZE);
	strncpy(sysfs_pinfo[id].header_ver, temp_buf + nread - (FIMC_IS_HEADER_VER_SIZE + IS_SIGNATURE_LEN), FIMC_IS_HEADER_VER_SIZE);
	info("Camera[%d]: phone fw version: %s\n", id, phone_fw_version);

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

	return ret;
}

int fimc_is_get_dual_cal_buf(int slave_position, char **buf, int *size)
{
	int ret = -1;
	char *cal_buf;
	u32 rom_dual_cal_start_addr;
	u32 rom_dual_cal_size;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	const struct fimc_is_vender_rom_addr *rom_addr = NULL;

	ret = fimc_is_sec_get_cal_buf(SENSOR_POSITION_REAR, &cal_buf);
	if (ret < 0) {
		err("[%s]: get_cal_buf fail", __func__);
		return ret;
	}

	rom_addr = specific->rom_cal_map_addr[SENSOR_POSITION_REAR];
	if (rom_addr == NULL) {
		err("[%s]: rom_cal_map is NULL\n", __func__);
		return -EINVAL;
	}

	rom_dual_cal_start_addr = rom_addr->rom_dual_cal_data2_start_addr;
	rom_dual_cal_size       = rom_addr->rom_dual_cal_data2_size;

	*buf  = &cal_buf[rom_dual_cal_start_addr];
	*size = rom_dual_cal_size;

	return 0;
}

int fimc_is_get_remosaic_cal_buf(int slave_position, char **buf, int *size)
{
	int ret = -1;
	char *cal_buf;
	u32 start_addr, end_addr;
	u32 cal_size = 0;

	struct fimc_is_rom_info *finfo;

#ifdef DEBUG_XTALK_CAL_SIZE
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	const struct fimc_is_vender_rom_addr *rom_addr = NULL;
#endif
#ifdef REAR_XTALK_CAL_DATA_SIZE
	if (slave_position == SENSOR_POSITION_REAR)
		cal_size = REAR_XTALK_CAL_DATA_SIZE;
#endif
#ifdef FRONT_XTALK_CAL_DATA_SIZE
	if (slave_position == SENSOR_POSITION_FRONT)
		cal_size = FRONT_XTALK_CAL_DATA_SIZE;
#endif

	if (cal_size == 0)
		return ret;

	ret = fimc_is_sec_get_cal_buf(slave_position, &cal_buf);
	if (ret < 0) {
		err("[%s]: get_cal_buf fail", __func__);
		return ret;
	}

	ret = fimc_is_sec_get_sysfs_finfo_by_position(slave_position, &finfo);
	if (ret < 0) {
		err("[%s]: get_sysfs_finfo fail", __func__);
		return -EINVAL;
	}

	start_addr = finfo->sensor_cal_data_start_addr;
	end_addr = finfo->sensor_cal_data_end_addr;

#ifdef DEBUG_XTALK_CAL_SIZE
	rom_addr = specific->rom_cal_map_addr[slave_position];
	if (rom_addr == NULL) {
		err("[%s]: rom_cal_map is NULL\n", __func__);
		return -EINVAL;
	}

	info("[%s]: start_addr(0x%08X) end_addr(0x%08X) cal_size(%d) checksum(%d)\n",
		__func__, start_addr, end_addr, cal_size, rom_addr->rom_sensor_cal_checksum_len);
#endif

	if (start_addr < 0 || cal_size <= 0 || end_addr < (start_addr + cal_size)) {
		err("[%s]: invalid start_addr(0x%08X) end_addr(0x%08X) cal_size(%d) \n",
			__func__, start_addr, end_addr, cal_size);
		return -EINVAL;
	}

	*buf  = &cal_buf[start_addr];
	*size = cal_size;

	return 0;
}

int fimc_is_sec_run_fw_sel_from_rom(struct device *dev, int id, bool headerOnly)
{
	int i, ret = 0;

	int rom_position = id;
	int rom_type = ROM_TYPE_NONE;
	bool rom_valid = false;
	bool is_running_camera = false;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_rom_info *finfo = NULL;

	fimc_is_sec_get_sysfs_finfo_by_position(id, &finfo);

	/* Use mutex for cal data rom */
	mutex_lock(&specific->rom_lock);

	for (i = SENSOR_POSITION_REAR; i < SENSOR_POSITION_MAX; i++) {
		if (specific->running_camera[i] == true) {
			info("Camera Running Check: %d\n", specific->running_camera[i]);
			is_running_camera = true;
			break;
		}
	}

	if (specific->rom_share[id].check_rom_share == true)
		rom_position = specific->rom_share[id].share_position;

	if ((finfo->is_caldata_read == false) || (force_caldata_dump == true)) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			info("Forced Cal data dump!!! CAMERA_POSITION=%d\n", id);

		rom_type = specific->rom_data[rom_position].rom_type;
		rom_valid = specific->rom_data[rom_position].rom_valid;

		if (rom_valid == true) {
			if (specific->running_camera[rom_position] == false) {
				fimc_is_sec_rom_power_on(core, rom_position);
			}

			if (rom_type == ROM_TYPE_EEPROM) {
				info("Camera: Read Cal data from EEPROM[%d]\n", id);

				if(headerOnly) {
					info("Camera: Only Read Header[%d]\n", id);
					fimc_is_sec_read_eeprom_header(dev, id);
				} else {
					if(!fimc_is_sec_readcal_eeprom(dev, id)) {
						finfo->is_caldata_read = true;
						specific->rom_data[id].is_rom_read = true;
					}
				}
			} else if (rom_type == ROM_TYPE_OTPROM) {
				info("Camera: Read Cal data from OTPROM[%d]\n", id);

				if(!fimc_is_sec_readcal_otprom(dev, id)) {
					finfo->is_caldata_read = true;
					specific->rom_data[id].is_rom_read = true;
				}
			}
		}
	}

	fimc_is_sec_fw_find(core, id);

	if (headerOnly) {
		goto exit;
	}

	if (id != SENSOR_POSITION_REAR )
		goto exit;

	if (rom_valid == true)
		ret = fimc_is_sec_write_phone_firmware(id);

exit:
#if defined(USE_COMMON_CAM_IO_PWR)
	if (is_running_camera == false && rom_valid == true && force_caldata_dump == false) {
		fimc_is_sec_rom_power_off(core, rom_position);
	}
#else
	if (rom_valid == true && force_caldata_dump == false) {
		fimc_is_sec_rom_power_off(core, rom_position);
	}
#endif

	mutex_unlock(&specific->rom_lock);

	return ret;
}

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

void remove_dump_fw_file(void)
{
	mm_segment_t old_fs;
	int old_mask;
	long ret;
	char fw_path[100];
	struct fimc_is_rom_info *sysfs_finfo = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);

	/* RTA binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_rta_fw_name);

	ret = sys_unlink(fw_path);
	info("sys_unlink (%s) %ld", fw_path, ret);

	/* DDK binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_fw_name);

	ret = sys_unlink(fw_path);
	info("sys_unlink (%s) %ld", fw_path, ret);

	sys_umask(old_mask);
	set_fs(old_fs);

	is_dumped_fw_loading_needed = false;
}

#ifdef VENDER_CAL_STRUCT_VER2
static void *fimc_is_sec_search_rom_extend_data(const struct rom_extend_cal_addr *extend_data, char *name)
{
	void *ret = NULL;

	const struct rom_extend_cal_addr *cur;
	cur = extend_data;

	while (cur != NULL) {
		if (!strcmp(cur->name, name)) {
			if (cur->data != NULL) {
				ret = (void *)cur->data;
			} else {
				warn("[%s] : Found -> %s, but no data \n", __func__, cur->name);
				ret = NULL;
			}
			break;
		}
		cur = cur->next;
	}

	return ret;
}
#endif
