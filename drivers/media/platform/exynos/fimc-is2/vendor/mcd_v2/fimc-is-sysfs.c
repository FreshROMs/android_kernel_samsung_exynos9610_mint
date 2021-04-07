/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/device.h>
#include "fimc-is-sysfs.h"
#include "fimc-is-core.h"
#include "fimc-is-err.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-vender-specific.h"
#ifdef CONFIG_OIS_USE
#include "fimc-is-device-ois.h"
#endif
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif

/* #define FORCE_CAL_LOAD */

extern struct kset *devices_kset;
extern struct device *fimc_is_dev;

struct class *camera_class = NULL;
struct device *camera_front_dev;
struct device *camera_rear_dev;
#ifdef CONFIG_OIS_USE
struct device *camera_ois_dev;
#endif

static struct fimc_is_core *sysfs_core;

static struct fimc_is_rom_info *finfo[SENSOR_POSITION_MAX] = {NULL, };
static struct fimc_is_rom_info *pinfo = NULL;

static struct fimc_is_cam_info cam_infos[CAM_INFO_MAX];
static struct fimc_is_common_cam_info common_cam_infos;

extern bool crc32_check_list[SENSOR_POSITION_MAX][CRC32_SCENARIO_MAX];
extern bool check_latest_cam_module[SENSOR_POSITION_MAX];
extern bool check_final_cam_module[SENSOR_POSITION_MAX];

extern bool force_caldata_dump;
static bool check_module_init = false;

#ifdef CONFIG_OIS_USE
static bool check_ois_power = false;
int ois_threshold;
#endif


int fimc_is_get_cam_info(struct fimc_is_cam_info **caminfo)
{
	*caminfo = cam_infos;
	return 0;
}

void fimc_is_get_common_cam_info(struct fimc_is_common_cam_info **caminfo)
{
	*caminfo = &common_cam_infos;
}

static int read_from_firmware_version(int position)
{
	int ret = 0;
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

	fimc_is_sec_get_sysfs_pinfo_rear(&pinfo);
	fimc_is_sec_get_sysfs_finfo_by_position(position, &(finfo[position]));

	fimc_is_vender_check_hw_init_running();

	if (force_caldata_dump || !(finfo[position]->is_caldata_read)) {
		if (force_caldata_dump)
			info("read_from_firmware_version : forced caldata dump!!\n");

		ret = fimc_is_sec_run_fw_sel(is_dev, position);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}
	}

	return 0;
}

#ifdef CONFIG_OIS_USE
static bool read_ois_version(void)
{
	bool ret = true;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (!specific->running_camera[SENSOR_POSITION_REAR]) {
		if (!specific->ois_ver_read) {
			fimc_is_ois_gpio_on(sysfs_core);
			msleep(150);

			ret = fimc_is_ois_check_fw(sysfs_core);
			if (!specific->running_camera[SENSOR_POSITION_REAR]) {
				fimc_is_ois_gpio_off(sysfs_core);
			}
		}
	}
	return ret;
}
#endif

static int fimc_is_get_sensor_data(char *maker, char *name, int position)
{
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module;
	int i = 0;

	if (!fimc_is_dev) {
		err("%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (module == NULL) {
		err("%s: Could not find sensor id.", __func__);
		return -EINVAL;
	}

	if(module) {
		if (maker != NULL)
			sprintf(maker, "%s", module->sensor_maker ?
					module->sensor_maker : "UNKNOWN");
		if (name != NULL)
			sprintf(name, "%s", module->sensor_name ?
					module->sensor_name : "UNKNOWN");
		return 0;
	}

	err("%s: there's no matched sensor id", __func__);

	return -ENODEV;
}

int svc_cheating_prevent_device_file_create(struct kobject **obj)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *Camera;

	/* To find svc kobject */
	svc_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data))
		        pr_info("Failed to create sys/devices/svc already exist svc : 0x%pK\n", data);
		else
			pr_info("Success to create sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Success to find svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	Camera = kobject_create_and_add("Camera", data);
	if (IS_ERR_OR_NULL(Camera))
	        pr_info("Failed to create sys/devices/svc/Camera : 0x%pK\n", Camera);
	else
		pr_info("Success to create sys/devices/svc/Camera : 0x%pK\n", Camera);


	*obj = Camera;
	return 0;
}


/*****                      [SSRM SYSFS LIST ]                                         *****
 *
 * ssrm_camera_info    - camera_ssrm_camera_info_show / camera_ssrm_camera_info_store  *****
 *                                                                                     *****
 *******************************************************************************************/

#ifdef USE_SSRM_CAMERA_INFO
struct ssrm_camera_data SsrmCameraInfo[FIMC_IS_SENSOR_COUNT];

static ssize_t camera_ssrm_camera_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ssrm_camera_data temp;
	int ret_count;
	int index = -1;
	int i = 0;

	ret_count = sscanf(buf, "%d%d%d%d%d%d%d", &temp.operation, &temp.cameraID, &temp.previewMinFPS,
		&temp.previewMaxFPS, &temp.previewSizeWidth,  &temp.previewSizeHeight, &temp.sensorOn);

	if (ret_count > sizeof(SsrmCameraInfo)/sizeof(int))
		return -EINVAL;

	switch (temp.operation) {
	case SSRM_CAMERA_INFO_CLEAR:
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) { /* clear */
			if (SsrmCameraInfo[i].cameraID == temp.cameraID) {
				SsrmCameraInfo[i].previewMaxFPS = 0;
				SsrmCameraInfo[i].previewMinFPS = 0;
				SsrmCameraInfo[i].previewSizeHeight = 0;
				SsrmCameraInfo[i].previewSizeWidth = 0;
				SsrmCameraInfo[i].sensorOn = 0;
				SsrmCameraInfo[i].cameraID = -1;
			}
		}
		break;

	case SSRM_CAMERA_INFO_SET:
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) { /* find empty space*/
			if (SsrmCameraInfo[i].cameraID == -1) {
				index = i;
				break;
			}
		}

		if (index == -1)
			return -EPERM;

		memcpy(&SsrmCameraInfo[i], &temp, sizeof(temp));
		break;

	case SSRM_CAMERA_INFO_UPDATE:
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
			if (SsrmCameraInfo[i].cameraID == temp.cameraID) {
				SsrmCameraInfo[i].previewMaxFPS = temp.previewMaxFPS;
				SsrmCameraInfo[i].previewMinFPS = temp.previewMinFPS;
				SsrmCameraInfo[i].previewSizeHeight = temp.previewSizeHeight;
				SsrmCameraInfo[i].previewSizeWidth = temp.previewSizeWidth;
				SsrmCameraInfo[i].sensorOn = temp.sensorOn;
				break;
			}
		}
		break;
	default:
		break;
	}

	return count;
}

static ssize_t camera_ssrm_camera_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp_buffer[50] = {0,};
	int i = 0;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		if (SsrmCameraInfo[i].cameraID != -1) {
			strncat(buf, "ID=", strlen("ID="));
			sprintf(temp_buffer, "%d;", SsrmCameraInfo[i].cameraID);
			strncat(buf, temp_buffer, strlen(temp_buffer));

			strncat(buf, "ON=", strlen("ON="));
			sprintf(temp_buffer, "%d;", SsrmCameraInfo[i].sensorOn);
			strncat(buf, temp_buffer, strlen(temp_buffer));

			if (SsrmCameraInfo[i].previewMinFPS && SsrmCameraInfo[i].previewMaxFPS) {
				strncat(buf, "FPS=", strlen("FPS="));
				sprintf(temp_buffer, "%d,%d;",
					SsrmCameraInfo[i].previewMinFPS, SsrmCameraInfo[i].previewMaxFPS);
				strncat(buf, temp_buffer, strlen(temp_buffer));
			}
			if (SsrmCameraInfo[i].previewSizeWidth && SsrmCameraInfo[i].previewSizeHeight) {
				strncat(buf, "SIZE=", strlen("SIZE="));
				sprintf(temp_buffer, "%d,%d;",
					SsrmCameraInfo[i].previewSizeWidth, SsrmCameraInfo[i].previewSizeHeight);
				strncat(buf, temp_buffer, strlen(temp_buffer));
			}
			strncat(buf, "\n", strlen("\n"));
		}
	}
	return strlen(buf);
}

static DEVICE_ATTR(ssrm_camera_info, 0644, camera_ssrm_camera_info_show, camera_ssrm_camera_info_store);
#endif /* USE_SSRM_CAMERA_INFO */

#if defined(REAR_SUB_CAMERA) || defined(FRONT_SUB_CAMERA)
static ssize_t camera_tilt_show(char *buf, int position)
{
	int rom_position = position;
	char *cal_buf = NULL;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (specific->rom_share[position].check_rom_share == true) {
		rom_position = specific->rom_share[position].share_position;
	}

	read_from_firmware_version(rom_position);
	fimc_is_sec_get_cal_buf(rom_position, &cal_buf);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, rom_position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (specific->rom_cal_map_addr[rom_position]->rom_dual_cal_data2_size > 0) {
		int32_t dual_tilt_x = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_x_addr;
		int32_t dual_tilt_y = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_y_addr;
		int32_t dual_tilt_z = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_z_addr;
		int32_t dual_tilt_sx = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_sx_addr;
		int32_t dual_tilt_sy = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_sy_addr;
		int32_t dual_tilt_range = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_range_addr;
		int32_t dual_tilt_max_err = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_max_err_addr;
		int32_t dual_tilt_avg_err = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_avg_err_addr;
		int32_t dual_tilt_dll_version = specific->rom_cal_map_addr[rom_position]->rom_dual_tilt_dll_version_addr;

		if (crc32_check_list[rom_position][CRC32_CHECK_FW_VER] &&
			crc32_check_list[rom_position][CRC32_CHECK_FACTORY]) {
			s32 *x = NULL, *y = NULL, *z = NULL, *sx = NULL, *sy = NULL;
			s32 *range = NULL, *max_err = NULL, *avg_err = NULL, *dll_version = NULL;

			x = (s32 *)&cal_buf[dual_tilt_x];
			y = (s32 *)&cal_buf[dual_tilt_y];
			z = (s32 *)&cal_buf[dual_tilt_z];
			sx = (s32 *)&cal_buf[dual_tilt_sx];
			sy = (s32 *)&cal_buf[dual_tilt_sy];
			range = (s32 *)&cal_buf[dual_tilt_range];
			max_err = (s32 *)&cal_buf[dual_tilt_max_err];
			avg_err = (s32 *)&cal_buf[dual_tilt_avg_err];
			dll_version = (s32 *)&cal_buf[dual_tilt_dll_version];

			return sprintf(buf, "1 %d %d %d %d %d %d %d %d %d\n",
								*x, *y, *z, *sx, *sy, *range, *max_err, *avg_err, *dll_version);
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}
#endif

/*****                      [ REAR SYSFS LSIT ]                                *****
 *                                                                             *****
 * rear_sensorid         - camera_rear_sensorid_show                           *****
 * rear_moduleid         - camera_rear_moduleid_show                           *****
 * rear_camtype          - camera_rear_camtype_show                            *****
 * rear_camfw            - camera_rear_camfw_show / camera_rear_camfw_write    *****
 * rear_camfw_full       - camera_rear_camfw_full_show                         *****
 * rear_caminfo          - camera_rear_info_show                               *****
 * rear_checkfw_user     - camera_rear_checkfw_user_show                       *****
 * rear_checkfw_factory  - camera_rear_checkfw_factory_show                    *****
 * rear_calcheck         - camera_rear_calcheck_show                           *****
 * fw_update             - camera_hw_init_show                                 *****
 * supported_cameraIds   - camera_supported_cameraIds_show                     *****
 * rear_sensor_standby   - camera_rear_sensor_standby                          *****
 * rear_sensorid_exif    - camera_rear_sensorid_exif_show                      *****
 * rear_mtf_exif         - camera_rear_mtf_exif_show                           *****
 * rear_afcal            - camera_rear_afcal_show                              *****
 * rear_dualcal          - camera_rear_dualcal_show                            *****
 * rear_dualcal_size     - camera_rear_dualcal_size_show                       *****
 * rear_force_cal_load   - camera_rear_force_cal_load_show                     *****
 *                                                                             *****
 ***********************************************************************************/

static ssize_t camera_rear_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct fimc_is_vender_specific *specific;
	struct device *is_dev = NULL;

	if (!fimc_is_dev) {
		dev_err(dev, "%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	is_dev = &sysfs_core->ischain[0].pdev->dev;

	dev_info(dev, "%s: E", __func__);
	if (force_caldata_dump == false) {
		fimc_is_vender_check_hw_init_running();
		ret = fimc_is_sec_run_fw_sel(is_dev, SENSOR_POSITION_REAR);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}
	}

	specific = sysfs_core->vender.private_data;

	return sprintf(buf, "%d\n", specific->rear_sensor_id);
}

static ssize_t camera_rear_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	read_from_firmware_version(position);

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[5],
			finfo[position]->rom_module_id[6], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9]);
}

static ssize_t camera_rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	char sensor_maker[50];
	char sensor_name[50];

	ret = fimc_is_get_sensor_data(sensor_maker, sensor_name, SENSOR_POSITION_REAR);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	char *loaded_fw = NULL;
	char command_ack[20] = {0, };
	struct exynos_platform_fimc_is *core_pdata = NULL;

	if (!fimc_is_dev) {
		dev_err(dev, "%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core_pdata = dev_get_platdata(fimc_is_dev);

#if defined(FRONT_CAMERA_ONLY_SUPPORT)
	err(" N,N There is no REAR CAMERA : FRONT_CAMERA_ONLY_SUPPORT");
	return sprintf(buf, "%s %s\n", "N", "N");
#endif

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
#ifdef CAMERA_REAR2
		strcpy(command_ack, "NG_FWCDCD4");
#else
		strcpy(command_ack, "NG_FWCD");
#endif
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	loaded_fw = pinfo->header_ver;
	fimc_is_sec_set_loaded_fw(loaded_fw);

	if(crc32_check_list[position][CRC32_CHECK_FW_VER] == true) {
		bool crc32_check_fw = crc32_check_list[position][CRC32_CHECK_FW];
		bool crc32_check_factory = crc32_check_list[position][CRC32_CHECK_FACTORY];
		bool crc32_check_setfile = crc32_check_list[position][CRC32_CHECK_SETFILE];

		if ( crc32_check_fw && crc32_check_factory && crc32_check_setfile
#ifdef CAMERA_REAR2
			&& crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY] == true
#endif
		) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");

			strcpy(command_ack, "NG_");

			if (!crc32_check_fw)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_check_setfile)
				strcat(command_ack, "SET");
#ifdef CAMERA_REAR2
			if (!crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD4");
#endif
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
#ifdef CAMERA_REAR2
		strcpy(command_ack, "NG_FWCDCD4");
#else
		strcpy(command_ack, "NG_FWCD");
#endif

		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if ((size == 1 || size == 2) && (buf[0] == 'F' || buf[0] == 'f')) {
		fimc_is_sec_set_force_caldata_dump(true);
		ret = size;
	} else {
		fimc_is_sec_set_force_caldata_dump(false);
	}
	return ret;
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	char command_ack[20] = {0, };
	char *loaded_fw;

#if defined(FRONT_CAMERA_ONLY_SUPPORT)
	err(" N,N There is no REAR CAMERA : FRONT_CAMERA_ONLY_SUPPORT");
	return sprintf(buf, "%s %s %s\n", "N", "N", "N");
#endif

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
#ifdef CAMERA_REAR2
		strcpy(command_ack, "NG_FWCDCD4");
#else
		strcpy(command_ack, "NG_FWCD");
#endif
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

	loaded_fw = pinfo->header_ver;
	fimc_is_sec_set_loaded_fw(loaded_fw);

	if(crc32_check_list[position][CRC32_CHECK_FW_VER] == true) {
		bool crc32_check_fw = crc32_check_list[position][CRC32_CHECK_FW];
		bool crc32_check_factory = crc32_check_list[position][CRC32_CHECK_FACTORY];
		bool crc32_check_setfile = crc32_check_list[position][CRC32_CHECK_SETFILE];

		if (crc32_check_fw && crc32_check_factory && crc32_check_setfile
#ifdef CAMERA_REAR2
			&& crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY]
#endif
		) {
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, pinfo->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");

			if (!crc32_check_fw)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_check_setfile)
				strcat(command_ack, "SET");
#ifdef CAMERA_REAR2
			if (!crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD4");
#endif
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, pinfo->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
#ifdef CAMERA_REAR2
		strcat(command_ack, "FWCDCD4");
#else
		strcat(command_ack, "FWCD");
#endif
		return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, pinfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[130] = {0, };

#ifdef CONFIG_OF
	struct fimc_is_cam_info *rear_cam_info = &(cam_infos[CAM_INFO_REAR]);

	if(!rear_cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch(rear_cam_info->isp) {
			case CAM_INFO_ISP_TYPE_INTERNAL :
				strncat(camera_info, "INT;", strlen("INT;"));
				break;
			case CAM_INFO_ISP_TYPE_EXTERNAL :
				strncat(camera_info, "EXT;", strlen("EXT;"));
				break;
			case CAM_INFO_ISP_TYPE_SOC :
				strncat(camera_info, "SOC;", strlen("SOC;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch(rear_cam_info->cal_memory) {
			case CAM_INFO_CAL_MEM_TYPE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CAL_MEM_TYPE_FROM :
			case CAM_INFO_CAL_MEM_TYPE_EEPROM :
			case CAM_INFO_CAL_MEM_TYPE_OTP :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch(rear_cam_info->read_version) {
			case CAM_INFO_READ_VER_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_READ_VER_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch(rear_cam_info->core_voltage) {
			case CAM_INFO_CORE_VOLT_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CORE_VOLT_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch(rear_cam_info->upgrade) {
			case CAM_INFO_FW_UPGRADE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_UPGRADE_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_FW_UPGRADE_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch(rear_cam_info->fw_write) {
			case CAM_INFO_FW_WRITE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_WRITE_OS :
				strncat(camera_info, "OS;", strlen("OS;"));
				break;
			case CAM_INFO_FW_WRITE_SD :
				strncat(camera_info, "SD;", strlen("SD;"));
				break;
			case CAM_INFO_FW_WRITE_ALL :
				strncat(camera_info, "ALL;", strlen("ALL;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch(rear_cam_info->fw_dump) {
			case CAM_INFO_FW_DUMP_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_DUMP_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch(rear_cam_info->companion) {
			case CAM_INFO_COMPANION_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_COMPANION_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch(rear_cam_info->ois) {
			case CAM_INFO_OIS_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_OIS_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch(rear_cam_info->valid) {
			case CAM_INFO_INVALID :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_VALID :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}
#endif

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if(crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY]
#ifdef CAMERA_REAR2
			&& crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY]
#endif
		) {
			if (!check_latest_cam_module[SENSOR_POSITION_REAR]
#ifdef CAMERA_REAR2
			|| !check_latest_cam_module[CAMERA_REAR2]
#endif
			) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };

#ifdef CONFIG_OIS_USE
	struct fimc_is_ois_info *ois_minfo = NULL;
	bool ois_ret = false;

	ois_ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if(crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_SETFILE]
#ifdef CAMERA_REAR2
			&& crc32_check_list[CAMERA_REAR2][CRC32_CHECK_FACTORY]
#endif
		) {
			if (!check_final_cam_module[SENSOR_POSITION_REAR]
#ifdef CAMERA_REAR2
				|| !check_final_cam_module[CAMERA_REAR2]
#endif
			) {
				err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
#ifdef CONFIG_OIS_USE
				if (ois_minfo->checksum != 0x00 || ois_minfo->caldata != 0x00 || !ois_ret) {
					err(" NG, OIS crc check fail, checksum = 0x%02x, caldata = 0x%02x, ois_ret = %d",
						ois_minfo->checksum, ois_minfo->caldata, ois_ret);
					strcpy(command_ack, "NG_CRC\n");
				} else {
					strcpy(command_ack, "OK\n");
				}
#else
				strcpy(command_ack, "OK\n");
#endif
			}
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool rom_valid = false;
	char rear_sensor[10] = {0, };
	char front_sensor[10] = {0, };

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;
	rom_valid = specific->rom_data[SENSOR_POSITION_FRONT].rom_valid;

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_REAR)
		&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY])
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

	if (rom_valid) {
		read_from_firmware_version(SENSOR_POSITION_FRONT);

		if (fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_FRONT)
			&& crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY])
			strcpy(front_sensor, "Normal");
		else
			strcpy(front_sensor, "Abnormal");

		return sprintf(buf, "%s %s\n", rear_sensor, front_sensor);
	} else {
		return sprintf(buf, "%s %s\n", rear_sensor, "Null");
	}
}

static ssize_t camera_hw_init_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender *vender;
	int i;

	vender = &sysfs_core->vender;

	if (!check_module_init) {
		fimc_is_vender_hw_init(vender);
		check_module_init = true;
#ifdef USE_SSRM_CAMERA_INFO
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
			SsrmCameraInfo[i].cameraID = -1;
		}
#endif
	}

	return sprintf(buf, "%s\n", "HW init done.");
}

static ssize_t camera_supported_cameraIds_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp_buf[FIMC_IS_SENSOR_COUNT];
	char *end = "\n";
	int i;

	for (i=0; i < common_cam_infos.max_supported_camera; i++)
	{
		sprintf(temp_buf, "%d ", common_cam_infos.supported_camera_ids[i]);
		strncat(buf, temp_buf, strlen(temp_buf));
	}
	strncat(buf, end, strlen(end));

	return strlen(buf);
}

static ssize_t camera_rear_sensor_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Rear sensor standby \n");
}

static ssize_t camera_rear_sensor_standby(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	switch (buf[0]) {
	case '0':
		break;
	case '1':
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_rear_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "%s\n", finfo[SENSOR_POSITION_REAR]->rom_sensor_id);
}

static ssize_t camera_rear_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(SENSOR_POSITION_REAR);
	fimc_is_sec_get_cal_buf(SENSOR_POSITION_REAR, &cal_buf);

	memcpy(buf, &cal_buf[finfo[SENSOR_POSITION_REAR]->mtf_data_addr], FIMC_IS_RESOLUTION_DATA_SIZE);
	return FIMC_IS_RESOLUTION_DATA_SIZE;
}

static ssize_t camera_rear_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "1 %d %d\n",
		finfo[SENSOR_POSITION_REAR]->af_cal_macro, finfo[SENSOR_POSITION_REAR]->af_cal_pan);
}

#if defined(REAR_SUB_CAMERA)
static ssize_t camera_rear_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_addr, dual_cal_data2_size;
	char *cal_buf;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	read_from_firmware_version(position);
	fimc_is_sec_get_cal_buf(position, &cal_buf);

	dual_cal_data2_addr = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_start_addr;
	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_addr > 0 && dual_cal_data2_size > 0) {
		memcpy(buf, &cal_buf[dual_cal_data2_addr], dual_cal_data2_size);
	  	return dual_cal_data2_size;
	}

	return 0;
}

static ssize_t camera_rear_dualcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_size;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_size > 0)
		return sprintf(buf, "%d\n", dual_cal_data2_size);
	else
		return 0;
}
#endif

#ifdef FORCE_CAL_LOAD
static ssize_t camera_rear_force_cal_load_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fimc_is_sec_set_force_caldata_dump(true);
	return sprintf(buf, "FORCE CALDATA LOAD\n");
}
#endif

static DEVICE_ATTR(rear_sensorid,        S_IRUGO, camera_rear_sensorid_show,        NULL);
static DEVICE_ATTR(rear_moduleid,        S_IRUGO, camera_rear_moduleid_show,        NULL);
static DEVICE_ATTR(rear_camtype,         S_IRUGO, camera_rear_camtype_show,         NULL);
static DEVICE_ATTR(rear_camfw,           S_IRUGO, camera_rear_camfw_show,           camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full,      S_IRUGO, camera_rear_camfw_full_show,      NULL);
static DEVICE_ATTR(rear_caminfo,         S_IRUGO, camera_rear_info_show,            NULL);
static DEVICE_ATTR(rear_checkfw_user,    S_IRUGO, camera_rear_checkfw_user_show,    NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO, camera_rear_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear_calcheck,        S_IRUGO, camera_rear_calcheck_show,        NULL);
static DEVICE_ATTR(fw_update,            S_IRUGO, camera_hw_init_show,              NULL);
static DEVICE_ATTR(supported_cameraIds,  S_IRUGO, camera_supported_cameraIds_show,  NULL);
static DEVICE_ATTR(rear_sensor_standby,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
                                                  camera_rear_sensor_standby_show,  camera_rear_sensor_standby);
static DEVICE_ATTR(rear_sensorid_exif,   S_IRUGO, camera_rear_sensorid_exif_show,   NULL);
static DEVICE_ATTR(rear_mtf_exif,        S_IRUGO, camera_rear_mtf_exif_show,        NULL);
static DEVICE_ATTR(rear_afcal,           S_IRUGO, camera_rear_afcal_show,           NULL);
#if defined(REAR_SUB_CAMERA)
static DEVICE_ATTR(rear_dualcal,         S_IRUGO, camera_rear_dualcal_show,          NULL);
static DEVICE_ATTR(rear_dualcal_size,    S_IRUGO, camera_rear_dualcal_size_show,     NULL);
#endif
#ifdef FORCE_CAL_LOAD
static DEVICE_ATTR(rear_force_cal_load,  S_IRUGO, camera_rear_force_cal_load_show,  NULL);
#endif

static DEVICE_ATTR(SVC_rear_module,      S_IRUGO, camera_rear_moduleid_show,        NULL);



/*****                    [ FRONT SYSFS LSIT ]                              *****
 *                                                                          *****
 * front_sensorid            - camera_front_sensorid_show                   *****
 * front_moduleid            - camera_front_moduleid_show                   *****
 * front_camtype             - camera_front_camtype_show                    *****
 * front_camfw               - camera_front_camfw_show                      *****
 * front_camfw_full          - camera_front_camfw_full_show                 *****
 * front_caminfo             - camera_front_info_show                       *****
 * front_checkfw_user        - camera_front_checkfw_user_show               *****
 * front_checkfw_factory     - camera_front_checkfw_factory_show            *****
 * front_sensorid_exif       - camera_front_sensorid_exif_show              *****
 * front_mtf_exif            - camera_front_mtf_exif_show                   *****
 * front_xtalkcal            - camera_front_xtalkcal_show                   *****
 *                                                                          *****
 ********************************************************************************/

static ssize_t camera_front_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (specific->rom_data[SENSOR_POSITION_FRONT].rom_valid) {
		if (force_caldata_dump == false) {
			fimc_is_vender_check_hw_init_running();
			ret = fimc_is_sec_run_fw_sel(is_dev, SENSOR_POSITION_FRONT);
			if (ret) {
				err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			}
		}
	}

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", specific->front_sensor_id);
}

static ssize_t camera_front_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;

	read_from_firmware_version(position);

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[5],
			finfo[position]->rom_module_id[6], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9]);
}

static ssize_t camera_front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	char sensor_maker[50];
	char sensor_name[50];

	ret = fimc_is_get_sensor_data(sensor_maker, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (specific->rom_data[SENSOR_POSITION_FRONT].rom_valid) {
		char command_ack[20] = {0, };

		read_from_firmware_version(SENSOR_POSITION_FRONT);

		if (!fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
			err(" NG, invalid ROM version");
			strcpy(command_ack, "NG_CD3");
			return sprintf(buf, "%s %s\n", "NULL", command_ack);
		}

		if (crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s\n",
				finfo[SENSOR_POSITION_FRONT]->header_ver, finfo[SENSOR_POSITION_FRONT]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");

			if (!crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD3");
			if (finfo[SENSOR_POSITION_FRONT]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[SENSOR_POSITION_FRONT]->header_ver, command_ack);
		}
	} else {
		int ret;
		char sensor_name[50];

		ret = fimc_is_get_sensor_data(NULL, sensor_name, SENSOR_POSITION_FRONT);

		if (ret < 0)
			return sprintf(buf, "UNKNOWN UNKNOWN\n");
		else
			return sprintf(buf, "%s N\n", sensor_name);
	}
}

static ssize_t camera_front_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_CD3");
		return sprintf(buf, "%s %s %s\n", "NULL", "N", command_ack);
	}

	if (crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY]) {
		return sprintf(buf, "%s %s %s\n",
			finfo[SENSOR_POSITION_FRONT]->header_ver, "N", finfo[SENSOR_POSITION_FRONT]->header_ver);
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_");

		if (!crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY])
			strcat(command_ack, "CD3");
		if (finfo[SENSOR_POSITION_FRONT]->header_ver[3] != 'L')
			strcat(command_ack, "_Q");

		return sprintf(buf, "%s %s %s\n", finfo[SENSOR_POSITION_FRONT]->header_ver, "N", command_ack);
	}
}

static ssize_t camera_front_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[120] = {0, };

#ifdef CONFIG_OF
	struct fimc_is_cam_info *front_cam_info = &(cam_infos[CAM_INFO_FRONT]);

	if(!front_cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch(front_cam_info->isp) {
			case CAM_INFO_ISP_TYPE_INTERNAL :
				strncat(camera_info, "INT;", strlen("INT;"));
				break;
			case CAM_INFO_ISP_TYPE_EXTERNAL :
				strncat(camera_info, "EXT;", strlen("EXT;"));
				break;
			case CAM_INFO_ISP_TYPE_SOC :
				strncat(camera_info, "SOC;", strlen("SOC;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch(front_cam_info->cal_memory) {
			case CAM_INFO_CAL_MEM_TYPE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CAL_MEM_TYPE_FROM :
			case CAM_INFO_CAL_MEM_TYPE_EEPROM :
			case CAM_INFO_CAL_MEM_TYPE_OTP :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch(front_cam_info->read_version) {
			case CAM_INFO_READ_VER_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_READ_VER_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch(front_cam_info->core_voltage) {
			case CAM_INFO_CORE_VOLT_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CORE_VOLT_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch(front_cam_info->upgrade) {
			case CAM_INFO_FW_UPGRADE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_UPGRADE_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_FW_UPGRADE_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch(front_cam_info->fw_write) {
			case CAM_INFO_FW_WRITE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_WRITE_OS :
				strncat(camera_info, "OS;", strlen("OS;"));
				break;
			case CAM_INFO_FW_WRITE_SD :
				strncat(camera_info, "SD;", strlen("SD;"));
				break;
			case CAM_INFO_FW_WRITE_ALL :
				strncat(camera_info, "ALL;", strlen("ALL;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch(front_cam_info->fw_dump) {
			case CAM_INFO_FW_DUMP_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_DUMP_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch(front_cam_info->companion) {
			case CAM_INFO_COMPANION_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_COMPANION_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch(front_cam_info->ois) {
			case CAM_INFO_OIS_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_OIS_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch(front_cam_info->valid) {
			case CAM_INFO_INVALID :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_VALID :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}
#endif

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_front_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {

		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_latest_cam_module[position]) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_front_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if (crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY]) {
		if (!check_final_cam_module[SENSOR_POSITION_FRONT]) {
			err(" NG, not final cam module");
			strcpy(command_ack, "NG_VER\n");
		} else {
			strcpy(command_ack, "OK\n");
		}
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_CRC\n");
	}
	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_front_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_FRONT);

	return sprintf(buf, "%s\n", finfo[SENSOR_POSITION_FRONT]->rom_sensor_id);
}

static ssize_t camera_front_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(SENSOR_POSITION_FRONT);
	fimc_is_sec_get_cal_buf(SENSOR_POSITION_FRONT, &cal_buf);

	memcpy(buf, &cal_buf[finfo[SENSOR_POSITION_FRONT]->mtf_data_addr], FIMC_IS_RESOLUTION_DATA_SIZE);
	return FIMC_IS_RESOLUTION_DATA_SIZE;
}

#ifdef EEP_XTALK_CAL_DATA_SIZE_FRONT
static ssize_t camera_front_xtalkcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(SENSOR_POSITION_FRONT);
	fimc_is_sec_get_front_cal_buf(&cal_buf);

	memcpy(buf, &cal_buf[EEP_XTALK_CAL_START_ADDR_FRONT], EEP_XTALK_CAL_DATA_SIZE_FRONT);
	return EEP_XTALK_CAL_DATA_SIZE_FRONT;
}
#endif

static DEVICE_ATTR(front_sensorid,        S_IRUGO, camera_front_sensorid_show,        NULL);
static DEVICE_ATTR(front_moduleid,        S_IRUGO, camera_front_moduleid_show,        NULL);
static DEVICE_ATTR(front_camtype,         S_IRUGO, camera_front_camtype_show,         NULL);
static DEVICE_ATTR(front_camfw,           S_IRUGO, camera_front_camfw_show,           NULL);
static DEVICE_ATTR(front_camfw_full,      S_IRUGO, camera_front_camfw_full_show,      NULL);
static DEVICE_ATTR(front_caminfo,         S_IRUGO, camera_front_info_show,            NULL);
static DEVICE_ATTR(front_checkfw_user,    S_IRUGO, camera_front_checkfw_user_show,    NULL);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO, camera_front_checkfw_factory_show, NULL);
static DEVICE_ATTR(front_sensorid_exif,   S_IRUGO, camera_front_sensorid_exif_show,   NULL);
static DEVICE_ATTR(front_mtf_exif,        S_IRUGO, camera_front_mtf_exif_show,        NULL);
#ifdef EEP_XTALK_CAL_DATA_SIZE_FRONT
static DEVICE_ATTR(front_xtalkcal,        S_IRUGO, camera_front_xtalkcal_show,        NULL);
#endif

static DEVICE_ATTR(SVC_front_module,      S_IRUGO, camera_front_moduleid_show,        NULL);


/*****                    [ REAR2 SYSFS LSIT ]                                     *****
 *                                                                                 *****
 * rear2_sensorid            - camera_rear2_sensorid_show                          *****
 * rear2_moduleid            - camera_rear2_moduleid_show                          *****
 * rear2_camfw               - camera_rear2_camfw_show                             *****
 * rear2_camfw_full          - camera_rear2_camfw_full_show                        *****
 * rear2_caminfo             - camera_rear2_info_show                              *****
 * rear2_checkfw_user        - camera_rear2_checkfw_user_show                      *****
 * rear2_checkfw_factory     - camera_rear2_checkfw_factory_show                   *****
 * rear2_sensorid_exif       - camera_rear2_sensorid_exif_show                     *****
 * rear2_mtf_exif            - camera_rear2_mtf_exif_show                          *****
 *                                                                                 *****
 ***************************************************************************************/

#ifdef CAMERA_REAR2
static ssize_t camera_rear2_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	dev_info(dev, "%s: E", __func__);
	return sprintf(buf, "%d\n", specific->rear3_sensor_id);
}

static ssize_t camera_rear2_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;

	read_from_firmware_version(position);

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[5],
			finfo[position]->rom_module_id[6], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9]);
}

static ssize_t camera_rear2_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	char command_ack[20] = {0, };

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCD4");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_check_list[position][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD4");
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_CD4");
		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_rear2_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	char command_ack[20] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCDCD4");
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s %s\n",
				finfo[position]->header_ver, "N", finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CD4");
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
		}
	} else {
		strcpy(command_ack, "NG_CD4");
		return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
	}
}

static ssize_t camera_rear2_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	char camera_info[130] = {0, };

#ifdef CONFIG_OF
	struct fimc_is_cam_info *cam_info = &(cam_infos[position]);

	if(!cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch (cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL:
			strncat(camera_info, "INT;", strlen("INT;"));
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL:
			strncat(camera_info, "EXT;", strlen("EXT;"));
			break;
		case CAM_INFO_ISP_TYPE_SOC:
			strncat(camera_info, "SOC;", strlen("SOC;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch (cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM:
		case CAM_INFO_CAL_MEM_TYPE_EEPROM:
		case CAM_INFO_CAL_MEM_TYPE_OTP:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch (cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_READ_VER_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch (cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CORE_VOLT_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch (cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_FW_UPGRADE_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch (cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_WRITE_OS:
			strncat(camera_info, "OS;", strlen("OS;"));
			break;
		case CAM_INFO_FW_WRITE_SD:
			strncat(camera_info, "SD;", strlen("SD;"));
			break;
		case CAM_INFO_FW_WRITE_ALL:
			strncat(camera_info, "ALL;", strlen("ALL;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch (cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_DUMP_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch (cam_info->companion) {
		case CAM_INFO_COMPANION_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_COMPANION_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch (cam_info->ois) {
		case CAM_INFO_OIS_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_OIS_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch (cam_info->valid) {
		case CAM_INFO_INVALID:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_VALID:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "DUALOPEN=", strlen("DUALOPEN="));
		switch (cam_info->dual_open) {
		case CAM_INFO_SINGLE_OPEN:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_DUAL_OPEN:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}
#endif

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_rear2_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {

		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_latest_cam_module[position]) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear2_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	char command_ack[10] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid FROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_final_cam_module[position]) {
				err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
				strcpy(command_ack, "OK\n");
			}
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_rear2_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	read_from_firmware_version(position);

	return sprintf(buf, "%s", finfo[position]->rom_sensor_id);
}

static ssize_t camera_rear2_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR2;
	char *cal_buf;

	read_from_firmware_version(position);
	fimc_is_sec_get_cal_buf(position, &cal_buf);

	memcpy(buf, &cal_buf[finfo[position]->mtf_data_addr], FIMC_IS_RESOLUTION_DATA_SIZE);
	return FIMC_IS_RESOLUTION_DATA_SIZE;
}

static DEVICE_ATTR(rear2_sensorid,        S_IRUGO, camera_rear2_sensorid_show,        NULL);
static DEVICE_ATTR(rear2_moduleid,        S_IRUGO, camera_rear2_moduleid_show,        NULL);
static DEVICE_ATTR(rear2_camfw,           S_IRUGO, camera_rear2_camfw_show,           NULL);
static DEVICE_ATTR(rear2_camfw_full,      S_IRUGO, camera_rear2_camfw_full_show,      NULL);
static DEVICE_ATTR(rear2_caminfo,         S_IRUGO, camera_rear2_info_show,            NULL);
static DEVICE_ATTR(rear2_checkfw_user,    S_IRUGO, camera_rear2_checkfw_user_show,    NULL);
static DEVICE_ATTR(rear2_checkfw_factory, S_IRUGO, camera_rear2_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear2_sensorid_exif,   S_IRUGO, camera_rear2_sensorid_exif_show,   NULL);
static DEVICE_ATTR(rear2_mtf_exif,        S_IRUGO, camera_rear2_mtf_exif_show,        NULL);
static DEVICE_ATTR(SVC_rear_module2,      S_IRUGO, camera_rear2_moduleid_show,        NULL);

#endif //CAMERA_REAR2


/*****                    [ FRONT2 SYSFS LSIT ]                            *****
 *                                                                         *****
 * front2_sensorid            - camera_front2_sensorid_show                *****
 * front2_camtype             - camera_front2_camtype_show                 *****
 * front2_camfw               - camera_front2_camfw_show                   *****
 * front2_camfw_full          - camera_front2_camfw_full_show              *****
 * front2_caminfo             - camera_front2_info_show                    *****
 * front2_checkfw_user        - camera_front2_checkfw_user_show            *****
 * front2_checkfw_factory     - camera_front2_checkfw_factory_show         *****
 * front2_sensorid_exif       - camera_front2_sensorid_exif_show           *****
 * front2_mtf_exif            - camera_front2_mtf_exif_show                *****
 * front_dualcal              - camera_front_dualcal_show                  *****
 * front_dualcal_size         - camera_front_dualcal_size_show             *****
 * front2_tilt                - camera_front2_tilt_show                    *****
 * front2_shift_x             - camera_front2_shift_x_show                 *****
 * front2_shift_y             - camera_front2_shift_y_show                 *****
 *                                                                         *****
 *******************************************************************************/

#ifdef CAMERA_FRONT2
static ssize_t camera_front2_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (specific->rom_data[CAMERA_FRONT2].rom_valid ||
		specific->rom_share[CAMERA_FRONT2].check_rom_share) {
		int ret = 0;
		struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

		if (force_caldata_dump == false) {
			fimc_is_vender_check_hw_init_running();
			ret = fimc_is_sec_run_fw_sel(is_dev, CAMERA_FRONT2);
			if (ret) {
				err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			}
		}
	}

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", specific->front2_sensor_id);
}

static ssize_t camera_front2_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	char sensor_maker[50];
	char sensor_name[50];

	ret = fimc_is_get_sensor_data(sensor_maker, sensor_name, CAMERA_FRONT2);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_front2_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_FRONT2;
	char command_ack[20] = {0, };

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCD5");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");

			strcpy(command_ack, "NG_");
			if (!crc32_check_list[position][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD5");
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_CD5");
		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_front2_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_FRONT2;
	char command_ack[20] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCD5");
		return sprintf(buf, "%s %s %s\n", "NULL", "N", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_check_list[position][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD5");
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_CD5");
		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_front2_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_FRONT2;
	char camera_info[125] = {0, };

#ifdef CONFIG_OF
	struct fimc_is_cam_info *front_cam_info = &(cam_infos[position]);

	if(!front_cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch(front_cam_info->isp) {
			case CAM_INFO_ISP_TYPE_INTERNAL :
				strncat(camera_info, "INT;", strlen("INT;"));
				break;
			case CAM_INFO_ISP_TYPE_EXTERNAL :
				strncat(camera_info, "EXT;", strlen("EXT;"));
				break;
			case CAM_INFO_ISP_TYPE_SOC :
				strncat(camera_info, "SOC;", strlen("SOC;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch(front_cam_info->cal_memory) {
			case CAM_INFO_CAL_MEM_TYPE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CAL_MEM_TYPE_FROM :
			case CAM_INFO_CAL_MEM_TYPE_EEPROM :
			case CAM_INFO_CAL_MEM_TYPE_OTP :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch(front_cam_info->read_version) {
			case CAM_INFO_READ_VER_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_READ_VER_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch(front_cam_info->core_voltage) {
			case CAM_INFO_CORE_VOLT_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_CORE_VOLT_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch(front_cam_info->upgrade) {
			case CAM_INFO_FW_UPGRADE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_UPGRADE_SYSFS :
				strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
				break;
			case CAM_INFO_FW_UPGRADE_CAMON :
				strncat(camera_info, "CAMON;", strlen("CAMON;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch(front_cam_info->fw_write) {
			case CAM_INFO_FW_WRITE_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_WRITE_OS :
				strncat(camera_info, "OS;", strlen("OS;"));
				break;
			case CAM_INFO_FW_WRITE_SD :
				strncat(camera_info, "SD;", strlen("SD;"));
				break;
			case CAM_INFO_FW_WRITE_ALL :
				strncat(camera_info, "ALL;", strlen("ALL;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch(front_cam_info->fw_dump) {
			case CAM_INFO_FW_DUMP_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_FW_DUMP_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch(front_cam_info->companion) {
			case CAM_INFO_COMPANION_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_COMPANION_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch(front_cam_info->ois) {
			case CAM_INFO_OIS_NONE :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_OIS_USE :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch(front_cam_info->valid) {
			case CAM_INFO_INVALID :
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_VALID :
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default :
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		strncat(camera_info, "DUALOPEN=", strlen("DUALOPEN="));
		switch (front_cam_info->dual_open) {
			case CAM_INFO_SINGLE_OPEN:
				strncat(camera_info, "N;", strlen("N;"));
				break;
			case CAM_INFO_DUAL_OPEN:
				strncat(camera_info, "Y;", strlen("Y;"));
				break;
			default:
				strncat(camera_info, "NULL;", strlen("NULL;"));
				break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}
#endif

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_front2_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_FRONT2;

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {

		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_latest_cam_module[position]) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_front2_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_FRONT2;
	char command_ack[10] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_final_cam_module[position]) {
				err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
				strcpy(command_ack, "OK\n");
			}
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_front2_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(CAMERA_FRONT2);

	return sprintf(buf, "%s\n", finfo[CAMERA_FRONT2]->rom_sensor_id);
}

static ssize_t camera_front2_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(CAMERA_FRONT2);
	fimc_is_sec_get_cal_buf(CAMERA_FRONT2, &cal_buf);

	memcpy(buf, &cal_buf[finfo[CAMERA_FRONT2]->mtf_data_addr], FIMC_IS_RESOLUTION_DATA_SIZE);
	return FIMC_IS_RESOLUTION_DATA_SIZE;
}

#if defined(FRONT_SUB_CAMERA)
static ssize_t camera_front_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	int32_t dual_cal_data2_addr, dual_cal_data2_size;
	char *cal_buf;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	read_from_firmware_version(position);
	fimc_is_sec_get_front_cal_buf(&cal_buf);

	dual_cal_data2_addr = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_start_addr;
	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_addr > 0 && dual_cal_data2_size > 0) {
		memcpy(buf, &cal_buf[dual_cal_data2_addr], dual_cal_data2_size);
		return dual_cal_data2_size;
	}

	return 0;
}

static ssize_t camera_front_dualcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	int32_t dual_cal_data2_size;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_size > 0)
		return sprintf(buf, "%d\n", dual_cal_data2_size);
	else
		return 0;
}

static ssize_t camera_front2_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, SENSOR_POSITION_FRONT);
}

static ssize_t camera_front2_shift_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	char *cal_buf;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	read_from_firmware_version(position);
	fimc_is_sec_get_front_cal_buf(&cal_buf);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size > 0) {
		int32_t dual_shift_x = specific->rom_cal_map_addr[position]->rom_dual_shift_x_addr;

		if (crc32_check_list[position][CRC32_CHECK_FW_VER] &&
			crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			u32 *x = NULL;
			x = (u32 *)&cal_buf[dual_shift_x];
			return sprintf(buf, "%d\n", *x);
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_front2_shift_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	char *cal_buf;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	read_from_firmware_version(position);
	fimc_is_sec_get_front_cal_buf(&cal_buf);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size > 0) {
		int32_t dual_shift_y = specific->rom_cal_map_addr[position]->rom_dual_shift_y_addr;

		if (crc32_check_list[position][CRC32_CHECK_FW_VER] &&
			crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			u32 *y = NULL;
			y = (u32 *)&cal_buf[dual_shift_y];
			return sprintf(buf, "%d\n", *y);
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}
#endif

static DEVICE_ATTR(front2_sensorid,        S_IRUGO, camera_front2_sensorid_show,        NULL);
static DEVICE_ATTR(front2_camtype,         S_IRUGO, camera_front2_camtype_show,         NULL);
static DEVICE_ATTR(front2_camfw,           S_IRUGO, camera_front2_camfw_show,           NULL);
static DEVICE_ATTR(front2_camfw_full,      S_IRUGO, camera_front2_camfw_full_show,      NULL);
static DEVICE_ATTR(front2_caminfo,         S_IRUGO, camera_front2_info_show,            NULL);
static DEVICE_ATTR(front2_checkfw_user,    S_IRUGO, camera_front2_checkfw_user_show,    NULL);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO, camera_front2_checkfw_factory_show, NULL);
static DEVICE_ATTR(front2_sensorid_exif,   S_IRUGO, camera_front2_sensorid_exif_show,   NULL);
static DEVICE_ATTR(front2_mtf_exif,        S_IRUGO, camera_front2_mtf_exif_show,        NULL);
#if defined(FRONT_SUB_CAMERA)
static DEVICE_ATTR(front_dualcal,          S_IRUGO, camera_front_dualcal_show,          NULL);
static DEVICE_ATTR(front_dualcal_size,     S_IRUGO, camera_front_dualcal_size_show,     NULL);
static DEVICE_ATTR(front2_tilt,            S_IRUGO, camera_front2_tilt_show,            NULL);
static DEVICE_ATTR(front2_shift_x,         S_IRUGO, camera_front2_shift_x_show,         NULL);
static DEVICE_ATTR(front2_shift_y,         S_IRUGO, camera_front2_shift_y_show,         NULL);
#endif

#endif //CAMERA_FRONT2



/*****                    [ REAR3 SYSFS LSIT ]                                     *****
 *                                                                                 *****
 * rear3_sensorid            - camera_rear3_sensorid_show                          *****
 * rear3_moduleid            - camera_rear3_moduleid_show                          *****
 * rear3_camfw               - camera_rear3_camfw_show                             *****
 * rear3_camfw_full          - camera_rear3_camfw_full_show                        *****
 * rear3_caminfo             - camera_rear3_info_show                              *****
 * rear3_checkfw_user        - camera_rear3_checkfw_user_show                      *****
 * rear3_checkfw_factory     - camera_rear3_checkfw_factory_show                   *****
 * rear3_sensorid_exif       - camera_rear3_sensorid_exif_show                     *****
 * rear3_mtf_exif            - camera_rear3_mtf_exif_show                          *****
 * rear3_dualcal             - camera_rear3_dualcal_show                          *****
 * rear3_dualcal_size        - camera_rear3_dualcal_size_show                          *****
 *                                                                                 *****
 ***************************************************************************************/


#ifdef CAMERA_REAR3
static ssize_t camera_rear3_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	info("%s: E", __func__);

	return sprintf(buf, "%d\n", specific->rear2_sensor_id);
}

#ifndef USE_SHARED_ROM_REAR3
static ssize_t camera_rear3_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	read_from_firmware_version(position);

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[5],
			finfo[position]->rom_module_id[6], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9]);
}
#endif
static ssize_t camera_rear3_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	char command_ack[20] = {0, };

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCD6");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_check_list[position][CRC32_CHECK_FACTORY])
				strcat(command_ack, "CD6");
			if (finfo[position]->header_ver[3] != 'L')
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_CD6");
		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_rear3_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	char command_ack[20] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FWCD6");
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s %s\n",
				finfo[position]->header_ver, "N", finfo[position]->header_ver);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CD6");
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
		}
	} else {
		strcpy(command_ack, "NG_CD6");
		return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
	}
}

static ssize_t camera_rear3_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	char camera_info[130] = {0, };

	struct fimc_is_cam_info *cam_info = &(cam_infos[position]);

	if(!cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch (cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL:
			strncat(camera_info, "INT;", strlen("INT;"));
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL:
			strncat(camera_info, "EXT;", strlen("EXT;"));
			break;
		case CAM_INFO_ISP_TYPE_SOC:
			strncat(camera_info, "SOC;", strlen("SOC;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch (cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM:
		case CAM_INFO_CAL_MEM_TYPE_EEPROM:
		case CAM_INFO_CAL_MEM_TYPE_OTP:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch (cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_READ_VER_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch (cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CORE_VOLT_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch (cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_FW_UPGRADE_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch (cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_WRITE_OS:
			strncat(camera_info, "OS;", strlen("OS;"));
			break;
		case CAM_INFO_FW_WRITE_SD:
			strncat(camera_info, "SD;", strlen("SD;"));
			break;
		case CAM_INFO_FW_WRITE_ALL:
			strncat(camera_info, "ALL;", strlen("ALL;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch (cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_DUMP_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch (cam_info->companion) {
		case CAM_INFO_COMPANION_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_COMPANION_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch (cam_info->ois) {
		case CAM_INFO_OIS_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_OIS_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch (cam_info->valid) {
		case CAM_INFO_INVALID:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_VALID:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "DUALOPEN=", strlen("DUALOPEN="));
		switch (cam_info->dual_open) {
		case CAM_INFO_SINGLE_OPEN:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_DUAL_OPEN:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;DUALOPEN=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_rear3_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_latest_cam_module[position]) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear3_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	char command_ack[10] = {0, };

	read_from_firmware_version(position);

	if (!fimc_is_sec_check_rom_ver(sysfs_core, position)) {
		err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_final_cam_module[position]) {
				err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
				strcpy(command_ack, "OK\n");
			}
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_rear3_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;

	read_from_firmware_version(position);

	return sprintf(buf, "%s", finfo[position]->rom_sensor_id);
}

static ssize_t camera_rear3_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = CAMERA_REAR3;
	char *cal_buf;

	read_from_firmware_version(position);
	fimc_is_sec_get_cal_buf(position, &cal_buf);

	memcpy(buf, &cal_buf[finfo[position]->mtf_data_addr], FIMC_IS_RESOLUTION_DATA_SIZE);
	return FIMC_IS_RESOLUTION_DATA_SIZE;
}

#if defined(REAR_SUB_CAMERA)
static ssize_t camera_rear3_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_addr, dual_cal_data2_size;
	char *cal_buf;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	read_from_firmware_version(position);
	fimc_is_sec_get_cal_buf(position, &cal_buf);

	if (specific->rom_cal_map_addr[position] == NULL)
		return 0;

	dual_cal_data2_addr = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_start_addr;
	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;


	if (dual_cal_data2_addr > 0 && dual_cal_data2_size > 0) {
		memcpy(buf, &cal_buf[dual_cal_data2_addr], dual_cal_data2_size);
	  	return dual_cal_data2_size;
	}

	return 0;
}

static ssize_t camera_rear3_dualcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_size;

	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	dual_cal_data2_size = specific->rom_cal_map_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_size > 0)
		return sprintf(buf, "%d\n", dual_cal_data2_size);
	else
		return 0;
}

static ssize_t camera_rear3_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAMERA_REAR3);
}
#endif

static DEVICE_ATTR(rear3_sensorid,        S_IRUGO, camera_rear3_sensorid_show,        NULL);
static DEVICE_ATTR(rear3_camfw,           S_IRUGO, camera_rear3_camfw_show,           NULL);
static DEVICE_ATTR(rear3_camfw_full,      S_IRUGO, camera_rear3_camfw_full_show,      NULL);
static DEVICE_ATTR(rear3_caminfo,         S_IRUGO, camera_rear3_info_show,            NULL);
static DEVICE_ATTR(rear3_checkfw_user,    S_IRUGO, camera_rear3_checkfw_user_show,    NULL);
static DEVICE_ATTR(rear3_checkfw_factory, S_IRUGO, camera_rear3_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear3_sensorid_exif,   S_IRUGO, camera_rear3_sensorid_exif_show,   NULL);
static DEVICE_ATTR(rear3_mtf_exif,        S_IRUGO, camera_rear3_mtf_exif_show,        NULL);
#if defined(REAR_SUB_CAMERA)
static DEVICE_ATTR(rear3_dualcal,         S_IRUGO, camera_rear3_dualcal_show,          NULL);
static DEVICE_ATTR(rear3_dualcal_size,    S_IRUGO, camera_rear3_dualcal_size_show,     NULL);
static DEVICE_ATTR(rear3_tilt,            S_IRUGO, camera_rear3_tilt_show,            NULL);
#endif
#ifndef USE_SHARED_ROM_REAR3
static DEVICE_ATTR(rear3_moduleid,        S_IRUGO, camera_rear3_moduleid_show,        NULL);
static DEVICE_ATTR(SVC_rear_module3,      S_IRUGO, camera_rear3_moduleid_show,        NULL);
#endif

#endif //CAMERA_REAR2



/*****                    [ BIGDATA SYSFS LSIT ]                                   *****
 *                                                                                 *****
 * rear_hwparam    - rear_camera_hw_param_show   / rear_camera_hw_param_store      *****
 * front_hwparam   - front_camera_hw_param_show  / front_camera_hw_param_store     *****
 * rear2_hwparam   - rear2_camera_hw_param_show  / rear2_camera_hw_param_store     *****
 * front2_hwparam  - front2_camera_hw_param_show / front2_camera_hw_param_store    *****
 * rear3_hwparam   - rear3_camera_hw_param_show  / rear3_camera_hw_param_store     *****
 *                                                                                 *****
 ***************************************************************************************/

#ifdef USE_CAMERA_HW_BIG_DATA
static ssize_t rear_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("failed to get hw param");
		return 0;
	}

	if (fimc_is_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
		return sprintf(buf, "\"CAMIR_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\","
			"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIR_ID\":\"MIR_ERR\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\","
			"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_hw_param(&ec_param, SENSOR_POSITION_REAR);

		if (ec_param)
			fimc_is_sec_init_err_cnt(ec_param);
	}

	return count;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("failed to get hw param");
		return 0;
	}

	if (fimc_is_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
		return sprintf(buf, "\"CAMIF_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\","
			"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIF_ID\":\"MIR_ERR\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\","
			"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t front_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_hw_param(&ec_param, SENSOR_POSITION_FRONT);

		if (ec_param)
			fimc_is_sec_init_err_cnt(ec_param);
	}

	return count;
}

#ifdef CAMERA_REAR2
static ssize_t rear2_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("failed to get hw param");
		return 0;
	}

	if (fimc_is_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
		return sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_hw_param(&ec_param, CAMERA_REAR2);

		if (ec_param)
			fimc_is_sec_init_err_cnt(ec_param);
	}

	return count;
}
#endif //CAMERA_REAR2

#ifdef CAMERA_FRONT2
static ssize_t front2_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("failed to get hw param");
		return 0;
	}

	if (fimc_is_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
		return sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t front2_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_hw_param(&ec_param, SENSOR_POSITION_FRONT2);

		if (ec_param)
			fimc_is_sec_init_err_cnt(ec_param);
	}

	return count;
}
#endif //CAMERA_FRONT2

#ifdef CAMERA_REAR3
static ssize_t rear3_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("failed to get hw param");
		return 0;
	}

	if (fimc_is_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
		return sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_AF\":\"%d\","
			"\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\",\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear3_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_hw_param(&ec_param, CAMERA_REAR3);

		if (ec_param)
			fimc_is_sec_init_err_cnt(ec_param);
	}

	return count;
}
#endif //CAMERA_REAR3


static DEVICE_ATTR(rear_hwparam,   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
								rear_camera_hw_param_show,   rear_camera_hw_param_store);

static DEVICE_ATTR(front_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
								front_camera_hw_param_show,  front_camera_hw_param_store);

#ifdef CAMERA_REAR2
static DEVICE_ATTR(rear2_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
								rear2_camera_hw_param_show,  rear2_camera_hw_param_store);
#endif

#ifdef CAMERA_FRONT2
static DEVICE_ATTR(front2_hwparam, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
								front2_camera_hw_param_show, front2_camera_hw_param_store);
#endif

#ifdef CAMERA_REAR3
static DEVICE_ATTR(rear3_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
								rear3_camera_hw_param_show, rear3_camera_hw_param_store);
#endif

#endif //USE_CAMERA_HW_BIG_DATA



/*****                    [ OIS SYSFS LSIT ]                             *****
 *                                                                       *****
 * selftest     - camera_ois_selftest_show                               *****
 * ois_power    - camera_ois_power_store                                 *****
 * autotest     - camera_ois_autotest_show / camera_ois_autotest_store   *****
 * ois_rawdata  - camera_ois_rawdata_show                                *****
 * oisfw        - camera_ois_version_show                                *****
 * ois_diff     - camera_ois_diff_show                                   *****
 * ois_exif     - camera_ois_exif_show                                   *****
 *                                                                       *****
 *****************************************************************************/

#ifdef CONFIG_OIS_USE
static ssize_t camera_ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	fimc_is_vender_check_hw_init_running();

	switch (buf[0]) {
	case '0':
		fimc_is_ois_gpio_off(sysfs_core);
		check_ois_power = false;
		break;
	case '1':
		fimc_is_ois_gpio_on(sysfs_core);
		check_ois_power = true;
		msleep(150);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_ois_autotest_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)

{
	int value = 0;

	if (kstrtoint(buf, 10, &value)) {
		err("convert fail");
	}

	ois_threshold = value;

	return count;
}

static ssize_t camera_ois_autotest_show(struct device *dev,
				    struct device_attribute *attr, char *buf)

{
	bool x_result = false, y_result = false;
	int sin_x = 0, sin_y = 0;

	if (check_ois_power) {
		fimc_is_ois_auto_test(sysfs_core, ois_threshold,
			&x_result, &y_result, &sin_x, &sin_y);

		if (x_result && y_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "pass", 0, "pass", 0);
		} else if (x_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "pass", 0, "fail", sin_y);
		} else if (y_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "pass", 0);
		} else {
			return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "fail", sin_y);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "fail", sin_y);
	}
}

static ssize_t camera_ois_selftest_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;

	if (check_ois_power) {
		fimc_is_ois_offset_test(sysfs_core, &raw_data_x, &raw_data_y);
		msleep(50);
		selftest_ret = fimc_is_ois_self_test(sysfs_core);

		if (selftest_ret == 0x0) {
			result_selftest = true;
		} else {
			result_selftest = false;
		}

		if (abs(raw_data_x) > CAMERA_OIS_GYRO_OFFSET_SPEC || abs(raw_data_y) > CAMERA_OIS_GYRO_OFFSET_SPEC)  {
			result_offset = false;
		} else {
			result_offset = true;
		}

		if (result_offset && result_selftest) {
			result_total = 0;
		} else if (!result_offset && !result_selftest) {
			result_total = 3;
		} else if (!result_offset) {
			result_total = 1;
		} else if (!result_selftest) {
			result_total = 2;
		}

		if (raw_data_x < 0 && raw_data_y < 0) {
			return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else if (raw_data_x < 0) {
			return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
				raw_data_y /1000, raw_data_y % 1000);
		} else if (raw_data_y < 0) {
			return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else {
			return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
				raw_data_y /1000, raw_data_y % 1000);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0;

	if (check_ois_power) {
		fimc_is_ois_get_offset_data(sysfs_core, &raw_data_x, &raw_data_y);

		if (raw_data_x < 0 && raw_data_y < 0) {
			return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else if (raw_data_x < 0) {
			return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
				raw_data_y /1000, raw_data_y % 1000);
		} else if (raw_data_y < 0) {
			return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else {
			return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
				raw_data_y /1000, raw_data_y % 1000);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	bool ret = false;

	fimc_is_vender_check_hw_init_running();
	ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);

	if (ois_minfo->checksum != 0x00 || !ret) {
		return sprintf(buf, "%s %s\n", "NG_FW2", "NULL");
	} else if (ois_minfo->caldata != 0x00) {
		return sprintf(buf, "%s %s\n", "NG_CD2", ois_pinfo->header_ver);
	} else {
		return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
	}
}

static ssize_t camera_ois_diff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int x_diff = 0, y_diff = 0;

	if (check_ois_power) {
		result = fimc_is_ois_diff_test(sysfs_core, &x_diff, &y_diff);

		return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%d,%d\n", 0, x_diff, y_diff);
	}
}

static ssize_t camera_ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	struct fimc_is_ois_info *ois_uinfo = NULL;
	struct fimc_is_ois_exif *ois_exif = NULL;

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);
	fimc_is_ois_get_user_version(&ois_uinfo);
	fimc_is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}

static DEVICE_ATTR(selftest,    S_IRUGO, camera_ois_selftest_show, NULL);
static DEVICE_ATTR(ois_power,   S_IWUSR, NULL,                     camera_ois_power_store);
static DEVICE_ATTR(autotest,    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
									camera_ois_autotest_show, camera_ois_autotest_store);
static DEVICE_ATTR(ois_rawdata, S_IRUGO, camera_ois_rawdata_show,  NULL);
static DEVICE_ATTR(oisfw,       S_IRUGO, camera_ois_version_show,  NULL);
static DEVICE_ATTR(ois_diff,    S_IRUGO, camera_ois_diff_show,     NULL);
static DEVICE_ATTR(ois_exif,    S_IRUGO, camera_ois_exif_show,     NULL);

#endif //CONFIG_OIS_USE



/*******************************************
 *
 * creat sysfs / destory sysfs
 *
 *******************************************/

int fimc_is_create_rear_sysfs(struct class *camera)
{
	struct class *cameraClass = camera;

	camera_rear_dev = device_create(cameraClass, NULL, 1, NULL, "rear");

	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
		return 0;
	}

	if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_moduleid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camtype.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_calcheck) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_calcheck.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_fw_update) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_fw_update.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_supported_cameraIds) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_supported_cameraIds.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_sensor_standby) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_sensor_standby.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_mtf_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_afcal) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_afcal.attr.name);
	}
#if defined(REAR_SUB_CAMERA)
	if (device_create_file(camera_rear_dev, &dev_attr_rear_dualcal) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_dualcal.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_dualcal_size) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_dualcal_size.attr.name);
	}
#endif
#ifdef FORCE_CAL_LOAD
	if (device_create_file(camera_rear_dev, &dev_attr_rear_force_cal_load) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_force_cal_load.attr.name);
	}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	if (device_create_file(camera_rear_dev, &dev_attr_rear_hwparam) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_hwparam.attr.name);
	}
#endif
#ifdef USE_SSRM_CAMERA_INFO
	if (device_create_file(camera_rear_dev, &dev_attr_ssrm_camera_info) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_ssrm_camera_info.attr.name);
	}
#endif
	return 0;
}

int fimc_is_create_front_sysfs(struct class *camera)
{
	struct class *cameraClass = camera;

	camera_front_dev = device_create(cameraClass, NULL, 0, NULL, "front");

	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
		return 0;
	}

	if (device_create_file(camera_front_dev, &dev_attr_front_sensorid) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_sensorid.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camtype) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camtype.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_moduleid) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_moduleid.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camfw) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camfw.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camfw_full) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camfw_full.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_caminfo) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_caminfo.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_checkfw_user.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_mtf_exif.attr.name);
	}
#ifdef EEP_XTALK_CAL_DATA_SIZE_FRONT
	if (device_create_file(camera_front_dev, &dev_attr_front_xtalkcal) < 0) {
		printk(KERN_ERR "failed to create front device fiel, %s\n",
			dev_attr_front_xtalkcal.attr.name);
	}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	if (device_create_file(camera_front_dev, &dev_attr_front_hwparam) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_hwparam.attr.name);
	}
#endif

	return 0;
}

#ifdef CAMERA_REAR2
int fimc_is_create_rear2_sysfs(void)
{
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
		return 0;
	}

	if (device_create_file(camera_rear_dev, &dev_attr_rear2_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_moduleid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_camfw) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_mtf_exif.attr.name);
	}
#ifdef USE_CAMERA_HW_BIG_DATA
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_hwparam) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_hwparam.attr.name);
	}
#endif

	return 0;
}
#endif

#ifdef CAMERA_FRONT2
int fimc_is_create_front2_sysfs(void)
{
	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
		return 0;
	}

	if (device_create_file(camera_front_dev, &dev_attr_front2_sensorid) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front2_sensorid.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_camtype) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_camtype.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_camfw) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_camfw.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_camfw_full) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_camfw_full.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_caminfo) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_caminfo.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_checkfw_user.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front2_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front2_mtf_exif.attr.name);
	}
#if defined(FRONT_SUB_CAMERA)
	if (device_create_file(camera_front_dev, &dev_attr_front_dualcal) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front_dualcal.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_dualcal_size) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front_dualcal_size.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_tilt) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
				dev_attr_front2_tilt.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_shift_x) < 0) {
		printk(KERN_ERR "failed to create front2 shitft_x device file, %s\n",
				dev_attr_front2_shift_x.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front2_shift_y) < 0) {
		printk(KERN_ERR "failed to create front2 shitft_y device file, %s\n",
				dev_attr_front2_shift_y.attr.name);
	}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	if (device_create_file(camera_front_dev, &dev_attr_front2_hwparam) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front2_hwparam.attr.name);
	}
#endif

	return 0;
}
#endif

#ifdef CAMERA_REAR3
int fimc_is_create_rear3_sysfs(void)
{
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
		return 0;
	}

	if (device_create_file(camera_rear_dev, &dev_attr_rear3_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_camfw) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_mtf_exif.attr.name);
	}
#ifndef USE_SHARED_ROM_REAR3
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_moduleid.attr.name);
	}
#endif
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_caminfo.attr.name);
	}
#if defined(REAR_SUB_CAMERA)
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_dualcal.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal_size) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_dualcal_size.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_tilt) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear3_tilt.attr.name);
	}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_hwparam) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_hwparam.attr.name);
	}
#endif

	return 0;
}
#endif

#ifdef CONFIG_OIS_USE
int fimc_is_create_ois_sysfs(struct class *ois)
{
	struct class *oisClass = ois;

	camera_ois_dev = device_create(oisClass, NULL, 2, NULL, "ois");

	if (IS_ERR(camera_ois_dev)) {
		printk(KERN_ERR "failed to create ois device!\n");
		return 0;
	}

	if (device_create_file(camera_ois_dev, &dev_attr_selftest) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_selftest.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_ois_power) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_ois_power.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_autotest) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_autotest.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_ois_rawdata) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_ois_rawdata.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_oisfw) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_oisfw.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_ois_diff) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_ois_diff.attr.name);
	}
	if (device_create_file(camera_ois_dev, &dev_attr_ois_exif) < 0) {
		printk(KERN_ERR "failed to create ois device file, %s\n",
			dev_attr_ois_exif.attr.name);
	}

	return 0;
}
#endif

int fimc_is_create_sysfs(struct fimc_is_core *core)
{
	struct kobject *svc = 0;
	int ret = 0;

	if (!core) {
		err("fimc_is_core is null");
		return -EINVAL;
	}

	svc_cheating_prevent_device_file_create(&svc);

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	fimc_is_create_rear_sysfs(camera_class);

	fimc_is_create_front_sysfs(camera_class);

#ifdef CAMERA_REAR2
	fimc_is_create_rear2_sysfs();
#endif

#ifdef CAMERA_FRONT2
	fimc_is_create_front2_sysfs();
#endif

#ifdef CAMERA_REAR3
	fimc_is_create_rear3_sysfs();
#endif

#ifdef CONFIG_OIS_USE
	int fimc_is_create_ois_sysfs(camera_class);
#endif

	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module.attr) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_SVC_rear_module.attr.name);
	}

	if (sysfs_create_file(svc, &dev_attr_SVC_front_module.attr) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_SVC_front_module.attr.name);
	}
#ifdef CAMERA_REAR2
	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module2.attr) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_SVC_rear_module2.attr.name);
	}
#endif
#ifdef CAMERA_REAR3
#ifndef USE_SHARED_ROM_REAR3

	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module3.attr) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_SVC_rear_module3.attr.name);
	}
#endif
#endif
	sysfs_core = core;

	return ret;
}

void fimc_is_destroy_rear_sysfs(void)
{
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
		device_remove_file(camera_rear_dev, &dev_attr_rear_moduleid);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
		device_remove_file(camera_rear_dev, &dev_attr_rear_caminfo);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
		device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
		device_remove_file(camera_rear_dev, &dev_attr_fw_update);
		device_remove_file(camera_rear_dev, &dev_attr_supported_cameraIds);
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensor_standby);
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid_exif);
		device_remove_file(camera_rear_dev, &dev_attr_rear_mtf_exif);
		device_remove_file(camera_rear_dev, &dev_attr_rear_afcal);
#if defined(REAR_SUB_CAMERA)
		device_remove_file(camera_rear_dev, &dev_attr_rear_dualcal);
		device_remove_file(camera_rear_dev, &dev_attr_rear_dualcal_size);
#endif
#ifdef FORCE_CAL_LOAD
		device_remove_file(camera_rear_dev, &dev_attr_rear_force_cal_load);
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
		device_remove_file(camera_rear_dev, &dev_attr_rear_hwparam);
#endif
#ifdef USE_SSRM_CAMERA_INFO
		device_remove_file(camera_rear_dev, &dev_attr_ssrm_camera_info);
#endif
}

void fimc_is_destroy_front_sysfs(void)
{
	device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
	device_remove_file(camera_front_dev, &dev_attr_front_moduleid);
	device_remove_file(camera_front_dev, &dev_attr_front_camtype);
	device_remove_file(camera_front_dev, &dev_attr_front_camfw);
	device_remove_file(camera_front_dev, &dev_attr_front_camfw_full);
	device_remove_file(camera_front_dev, &dev_attr_front_caminfo);
	device_remove_file(camera_front_dev, &dev_attr_front_checkfw_user);
	device_remove_file(camera_front_dev, &dev_attr_front_checkfw_factory);
	device_remove_file(camera_front_dev, &dev_attr_front_sensorid_exif);
	device_remove_file(camera_front_dev, &dev_attr_front_mtf_exif);
#ifdef EEP_XTALK_CAL_DATA_SIZE_FRONT
	device_remove_file(camera_front_dev, &dev_attr_front_xtalkcal);
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	device_remove_file(camera_front_dev, &dev_attr_front_hwparam);
#endif
}

#ifdef CAMERA_REAR2
void fimc_is_destroy_rear2_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear2_sensorid);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_moduleid);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_mtf_exif);
#ifdef USE_CAMERA_HW_BIG_DATA
	device_remove_file(camera_rear_dev, &dev_attr_rear2_hwparam);
#endif
}
#endif

#ifdef CAMERA_FRONT2
void fimc_is_destroy_front2_sysfs(void)
{
	device_remove_file(camera_front_dev, &dev_attr_front2_sensorid);
	device_remove_file(camera_front_dev, &dev_attr_front2_camtype);
	device_remove_file(camera_front_dev, &dev_attr_front2_camfw);
	device_remove_file(camera_front_dev, &dev_attr_front2_camfw_full);
	device_remove_file(camera_front_dev, &dev_attr_front2_caminfo);
	device_remove_file(camera_front_dev, &dev_attr_front2_checkfw_user);
	device_remove_file(camera_front_dev, &dev_attr_front2_checkfw_factory);
	device_remove_file(camera_front_dev, &dev_attr_front2_sensorid_exif);
	device_remove_file(camera_front_dev, &dev_attr_front2_mtf_exif);
	device_remove_file(camera_front_dev, &dev_attr_front_dualcal);
	device_remove_file(camera_front_dev, &dev_attr_front_dualcal_size);
	device_remove_file(camera_front_dev, &dev_attr_front2_tilt);
	device_remove_file(camera_front_dev, &dev_attr_front2_shift_x);
	device_remove_file(camera_front_dev, &dev_attr_front2_shift_y);
#ifdef USE_CAMERA_HW_BIG_DATA
	device_remove_file(camera_front_dev, &dev_attr_front2_hwparam);
#endif
}
#endif

#ifdef CAMERA_REAR3
void fimc_is_destroy_rear3_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear3_sensorid);
#ifndef USE_SHARED_ROM_REAR3
	device_remove_file(camera_rear_dev, &dev_attr_rear3_moduleid);
#endif
	device_remove_file(camera_rear_dev, &dev_attr_rear3_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_mtf_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_dualcal);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_dualcal_size);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_tilt);
#ifdef USE_CAMERA_HW_BIG_DATA
	device_remove_file(camera_rear_dev, &dev_attr_rear3_hwparam);
#endif
}
#endif

int fimc_is_destroy_sysfs(struct fimc_is_core *core)
{
	if (camera_rear_dev) {
		fimc_is_destroy_rear_sysfs();

#ifdef CAMERA_REAR2
		fimc_is_destroy_rear2_sysfs();
#endif
#ifdef CAMERA_REAR3
		fimc_is_destroy_rear3_sysfs();
#endif
	}

	if (camera_front_dev) {
		fimc_is_destroy_front_sysfs();

#ifdef CAMERA_FRONT2
		fimc_is_destroy_front2_sysfs();
#endif
	}

#if defined (CONFIG_OIS_USE)
	if (camera_ois_dev) {
		device_remove_file(camera_ois_dev, &dev_attr_selftest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_power);
		device_remove_file(camera_ois_dev, &dev_attr_autotest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
		device_remove_file(camera_ois_dev, &dev_attr_oisfw);
		device_remove_file(camera_ois_dev, &dev_attr_ois_diff);
		device_remove_file(camera_ois_dev, &dev_attr_ois_exif);
	}
#endif

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);

#if defined (CONFIG_OIS_USE)
		if (camera_ois_dev)
			device_destroy(camera_class, camera_ois_dev->devt);
#endif
	}

	class_destroy(camera_class);

	return 0;
}
