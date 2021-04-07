/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-fimc-is-module.h>
#include "fimc-is-vender.h"
#include "fimc-is-vender-specific.h"
#include "fimc-is-core.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-dt.h"
#include "fimc-is-sysfs.h"

#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include "fimc-is-binary.h"

#if defined (CONFIG_OIS_USE)
#include "fimc-is-device-ois.h"
#endif
#include "fimc-is-device-preprocessor.h"
#include "fimc-is-interface-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-interface-library.h"
#if defined(CONFIG_LEDS_S2MU005_FLASH)
#include <linux/leds-s2mu005.h>
#endif

extern int fimc_is_create_sysfs(struct fimc_is_core *core);
extern bool crc32_check;
extern bool crc32_header_check;
extern bool crc32_check_front;
extern bool crc32_check_front2;
extern bool crc32_header_check_front;
extern bool is_dumped_fw_loading_needed;
extern bool force_caldata_dump;
extern bool crc32_check_rear3;
extern bool crc32_header_check_rear3;

static u32  rear_sensor_id;
static u32  rear2_sensor_id;
static u32  rear3_sensor_id;
static u32  front_sensor_id;
static u32  front2_sensor_id;
static bool check_sensor_vendor;
static bool skip_cal_loading;
static bool use_ois_hsi2c;
static bool need_i2c_config;
static bool use_ois;
static bool use_module_check;
static bool is_hw_init_running = false;
static FRomPowersource f_rom_power;
#ifdef CONFIG_SECURE_CAMERA_USE
static u32  secure_sensor_id;
#endif

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
struct workqueue_struct *sensor_pwr_ctrl_wq = 0;
#define CAMERA_WORKQUEUE_MAX_WAITING	1000
#endif

#ifdef USE_CAMERA_HW_BIG_DATA
static struct cam_hw_param_collector cam_hwparam_collector;
static bool mipi_err_check;
static bool need_update_to_file;

bool fimc_is_sec_need_update_to_file(void)
{
	return need_update_to_file;
}

void fimc_is_sec_init_err_cnt(struct cam_hw_param *hw_param)
{
	if (hw_param) {
		memset(hw_param, 0, sizeof(struct cam_hw_param));
#ifdef CAMERA_HW_BIG_DATA_FILE_IO
		fimc_is_sec_copy_err_cnt_to_file();
#endif
	}
}

void fimc_is_sec_copy_err_cnt_to_file(void)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	bool ret = false;
	int old_mask = 0;

	if (current && current->fs) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		ret = sys_access(CAM_HW_ERR_CNT_FILE_PATH, 0);

		if (ret != 0) {
			old_mask = sys_umask(7);
			fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
			if (IS_ERR_OR_NULL(fp)) {
				warn("%s open failed", CAM_HW_ERR_CNT_FILE_PATH);
				sys_umask(old_mask);
				set_fs(old_fs);
				return;
			}

			filp_close(fp, current->files);
			sys_umask(old_mask);
		}

		fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_TRUNC | O_SYNC, 0660);
		if (IS_ERR_OR_NULL(fp)) {
			warn("%s open failed", CAM_HW_ERR_CNT_FILE_PATH);
			set_fs(old_fs);
			return;
		}

		nwrite = vfs_write(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

		filp_close(fp, current->files);
		set_fs(old_fs);
		need_update_to_file = false;
	}
}

void fimc_is_sec_copy_err_cnt_from_file(void)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread = 0;
	bool ret = false;

	ret = fimc_is_sec_file_exist(CAM_HW_ERR_CNT_FILE_PATH);

	if (ret) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_RDONLY, 0660);
		if (IS_ERR_OR_NULL(fp)) {
			warn("%s open failed", CAM_HW_ERR_CNT_FILE_PATH);
			set_fs(old_fs);
			return;
		}

		nread = vfs_read(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

		filp_close(fp, current->files);
		set_fs(old_fs);
	}
}

void fimc_is_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position)
{
	switch (position) {
		case SENSOR_POSITION_REAR:
			*hw_param = &cam_hwparam_collector.rear_hwparam;
			break;
		case SENSOR_POSITION_REAR2:
			*hw_param = &cam_hwparam_collector.rear2_hwparam;
			break;
		case SENSOR_POSITION_REAR3:
			*hw_param = &cam_hwparam_collector.rear3_hwparam;
			break;
		case SENSOR_POSITION_FRONT:
			*hw_param = &cam_hwparam_collector.front_hwparam;
			break;
		case SENSOR_POSITION_FRONT2:
			*hw_param = &cam_hwparam_collector.front2_hwparam;
			break;
		case SENSOR_POSITION_SECURE:
			*hw_param = &cam_hwparam_collector.iris_hwparam;
			break;
		default:
			need_update_to_file = false;
			return;
	}
	need_update_to_file = true;
}

bool fimc_is_sec_is_valid_moduleid(char* moduleid)
{
	int i = 0;

	if (moduleid == NULL || strlen(moduleid) < 5) {
		goto err;
	}

	for (i = 0; i < 5; i++)
	{
		if (!((moduleid[i] > 47 && moduleid[i] < 58) || // 0 to 9
			(moduleid[i] > 64 && moduleid[i] < 91))) {  // A to Z
			goto err;
		}
	}

	return true;

err:
	warn("invalid moduleid\n");
	return false;
}
#endif

void fimc_is_vendor_csi_stream_on(struct fimc_is_device_csi *csi)
{
#ifdef USE_CAMERA_HW_BIG_DATA
	mipi_err_check = false;
#endif
}

void fimc_is_vender_csi_err_handler(struct fimc_is_device_csi *csi)
{
#ifdef USE_CAMERA_HW_BIG_DATA
	struct fimc_is_device_sensor *device = NULL;
	struct cam_hw_param *hw_param = NULL;

	device = container_of(csi->subdev, struct fimc_is_device_sensor, subdev_csi);

	if (device && device->pdev && !mipi_err_check) {
		switch (device->pdev->id) {
#ifdef CSI_SCENARIO_COMP
			case CSI_SCENARIO_COMP:
				fimc_is_sec_get_hw_param(&hw_param, device->position);
				if (hw_param)
					hw_param->mipi_comp_err_cnt++;
				break;
#endif

#ifdef CSI_SCENARIO_SEN_REAR
			case CSI_SCENARIO_SEN_REAR:
				fimc_is_sec_get_hw_param(&hw_param, device->position);
				if (hw_param)
					hw_param->mipi_sensor_err_cnt++;
				break;
#endif

#ifdef CSI_SCENARIO_SEN_FRONT
			case CSI_SCENARIO_SEN_FRONT:
				fimc_is_sec_get_hw_param(&hw_param, device->position);
				if (hw_param)
					hw_param->mipi_sensor_err_cnt++;
				break;
#endif

#ifdef CSI_SCENARIO_TELE
			case CSI_SCENARIO_TELE:
				break;
#endif

#ifdef CSI_SCENARIO_SECURE
			case CSI_SCENARIO_SECURE:
				fimc_is_sec_get_iris_hw_param(&hw_param);

				if (hw_param)
					hw_param->mipi_sensor_err_cnt++;
				break;
#endif
			default:
				break;
		}
		mipi_err_check = true;
	}
#endif
}

int fimc_is_vender_probe(struct fimc_is_vender *vender)
{
	int i, ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	WARN_ON(!vender);

	core = container_of(vender, struct fimc_is_core, vender);
	snprintf(vender->fw_path, sizeof(vender->fw_path), "%s", FIMC_IS_FW_SDCARD);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s", FIMC_IS_FW);

	specific = (struct fimc_is_vender_specific *)kmalloc(sizeof(struct fimc_is_vender_specific), GFP_KERNEL);

	/* init mutex for spi read */
	mutex_init(&specific->spi_lock);
	specific->running_front_camera = false;
	specific->running_rear_camera = false;
	specific->running_rear_second_camera = false;
	specific->running_front_second_camera = false;

	specific->retention_data.firmware_size = 0;
	memset(&specific->retention_data.firmware_crc32, 0, FIMC_IS_COMPANION_CRC_SIZE);

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
	info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION));
#endif

#ifdef CONFIG_COMPANION_DCDC_USE
	/* Init companion dcdc */
	comp_pmic_init(&specific->companion_dcdc, NULL);
#endif

	if (fimc_is_create_sysfs(core)) {
		probe_err("fimc_is_create_sysfs is failed");
		ret = -EINVAL;
		goto p_err;
	}

	specific->rear_sensor_id = rear_sensor_id;
	specific->front_sensor_id = front_sensor_id;
	specific->rear2_sensor_id = rear2_sensor_id;
	specific->front2_sensor_id = front2_sensor_id;
	specific->rear3_sensor_id = rear3_sensor_id;
	specific->check_sensor_vendor = check_sensor_vendor;
	specific->use_ois = use_ois;
	specific->use_ois_hsi2c = use_ois_hsi2c;
	specific->need_i2c_config = need_i2c_config;
	specific->use_module_check = use_module_check;
	specific->skip_cal_loading = skip_cal_loading;
	specific->f_rom_power = f_rom_power;
	specific->suspend_resume_disable = false;
	specific->need_cold_reset = false;
#ifdef CONFIG_SENSOR_RETENTION_USE
	specific->need_retention_init = true;
#endif
#ifdef CONFIG_SECURE_CAMERA_USE
	specific->secure_sensor_id = secure_sensor_id;
#endif
	for (i = 0; i < SENSOR_POSITION_END; i++) {
		specific->eeprom_client[i] = NULL;
	}

	vender->private_data = specific;

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
	if (!sensor_pwr_ctrl_wq) {
		sensor_pwr_ctrl_wq = create_singlethread_workqueue("sensor_pwr_ctrl");
	}
#endif

p_err:
	return ret;
}

#ifdef CAMERA_SYSFS_V2
static int parse_sysfs_caminfo(struct device_node *np,
				struct fimc_is_cam_info *cam_infos, int camera_num)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "isp", cam_infos[camera_num].isp);
	DT_READ_U32(np, "cal_memory", cam_infos[camera_num].cal_memory);
	DT_READ_U32(np, "read_version", cam_infos[camera_num].read_version);
	DT_READ_U32(np, "core_voltage", cam_infos[camera_num].core_voltage);
	DT_READ_U32(np, "upgrade", cam_infos[camera_num].upgrade);
	DT_READ_U32(np, "fw_write", cam_infos[camera_num].fw_write);
	DT_READ_U32(np, "fw_dump", cam_infos[camera_num].fw_dump);
	DT_READ_U32(np, "companion", cam_infos[camera_num].companion);
	DT_READ_U32(np, "ois", cam_infos[camera_num].ois);
	DT_READ_U32(np, "valid", cam_infos[camera_num].valid);
	DT_READ_U32(np, "dual_open", cam_infos[camera_num].dual_open);

	return 0;
}
#endif

int fimc_is_vender_dt(struct device_node *np)
{
	int ret = 0;
#ifdef CAMERA_SYSFS_V2
	struct device_node *camInfo_np;
	struct fimc_is_cam_info *camera_infos;
	struct fimc_is_common_cam_info *common_camera_infos = NULL;
	char camInfo_string[15];
	int camera_num;
	int max_camera_num;
#endif

	ret = of_property_read_u32(np, "rear_sensor_id", &rear_sensor_id);
	if (ret) {
		probe_err("rear_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "front_sensor_id", &front_sensor_id);
	if (ret) {
		probe_err("front_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "rear2_sensor_id", &rear2_sensor_id);
	if (ret) {
		probe_err("rear2_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "front2_sensor_id", &front2_sensor_id);
	if (ret) {
		probe_err("front2_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "rear3_sensor_id", &rear3_sensor_id);
	if (ret) {
		probe_err("rear3_sensor_id read is fail(%d)", ret);
	}

#ifdef CONFIG_SECURE_CAMERA_USE
	ret = of_property_read_u32(np, "secure_sensor_id", &secure_sensor_id);
	if (ret) {
		probe_err("secure_sensor_id read is fail(%d)", ret);
		secure_sensor_id = 0;
	}
#endif

	check_sensor_vendor = of_property_read_bool(np, "check_sensor_vendor");
	if (!check_sensor_vendor) {
		probe_info("check_sensor_vendor not use(%d)\n", check_sensor_vendor);
	}
#ifdef CONFIG_OIS_USE
	use_ois = of_property_read_bool(np, "use_ois");
	if (!use_ois) {
		probe_err("use_ois not use(%d)", use_ois);
	}

	use_ois_hsi2c = of_property_read_bool(np, "use_ois_hsi2c");
	if (!use_ois_hsi2c) {
		probe_err("use_ois_hsi2c not use(%d)", use_ois_hsi2c);
	}
#endif

	need_i2c_config = of_property_read_bool(np, "need_i2c_config");
	if(!need_i2c_config) {
		probe_err("need_i2c_config not use(%d)", need_i2c_config);
	}

	use_module_check = of_property_read_bool(np, "use_module_check");
	if (!use_module_check) {
		probe_err("use_module_check not use(%d)", use_module_check);
	}

	skip_cal_loading = of_property_read_bool(np, "skip_cal_loading");
	if (!skip_cal_loading) {
		probe_info("skip_cal_loading not use(%d)\n", skip_cal_loading);
	}

	ret = of_property_read_u32(np, "f_rom_power", &f_rom_power);
	if (ret) {
		probe_err("f_rom_power read is fail(%d)", ret);
		f_rom_power = 0;
	}
	if (!f_rom_power) {
		probe_info("f_rom_power not use(%d)\n", f_rom_power);
	}

#ifdef CAMERA_SYSFS_V2
	ret = of_property_read_u32(np, "max_camera_num", &max_camera_num);
	if (ret) {
		err("max_camera_num read is fail(%d)", ret);
		max_camera_num = 0;
	}
	fimc_is_get_cam_info(&camera_infos);

	for (camera_num = 0; camera_num < max_camera_num; camera_num++) {
		sprintf(camInfo_string, "%s%d", "camera_info", camera_num);

		camInfo_np = of_find_node_by_name(np, camInfo_string);
		if (!camInfo_np) {
			info("%s: camera_num = %d can't find camInfo_string node\n", __func__, camera_num);
			continue;
		}
		parse_sysfs_caminfo(camInfo_np, camera_infos, camera_num);
	}

	fimc_is_get_common_cam_info(&common_camera_infos);

	ret = of_property_read_u32(np, "max_supported_camera", &common_camera_infos->max_supported_camera);
	if (ret) {
		probe_err("supported_cameraId read is fail(%d)", ret);
	}

	ret = of_property_read_u32_array(np, "supported_cameraId",
		common_camera_infos->supported_camera_ids, common_camera_infos->max_supported_camera);
	if (ret) {
		probe_err("supported_cameraId read is fail(%d)", ret);
	}
#endif

	return ret;
}

bool fimc_is_vender_check_sensor(struct fimc_is_core *core)
{
	int i = 0;
	bool ret = false;
	int retry_count = 20;

	do {
		ret = false;
		for (i = 0; i < FIMC_IS_HW_SENSOR_COUNT; i++) {
			if (!test_bit(FIMC_IS_SENSOR_PROBE, &core->sensor[i].state)) {
				ret = true;
				break;
			}
		}

		if (i == FIMC_IS_HW_SENSOR_COUNT && ret == false) {
			info("Retry count = %d\n", retry_count);
			break;
		}

		mdelay(100);
		if (retry_count > 0) {
			--retry_count;
		} else {
			err("Could not get sensor before start ois fw update routine.\n");
			break;
		}
	} while (ret);

	return ret;
}

void fimc_is_vender_check_hw_init_running(void)
{
	int retry = 50;

	do {
		if (!is_hw_init_running) {
			break;
		}
		--retry;
		msleep(100);
	} while (retry > 0);

	if (retry <= 0) {
		err("HW init is not completed.");
	}

	return;
}

int fimc_is_vender_hw_init(struct fimc_is_vender *vender)
{
	bool ret = false;
	struct device *dev  = NULL;
	struct fimc_is_core *core;

	core = container_of(vender, struct fimc_is_core, vender);
	dev = &core->ischain[0].pdev->dev;

	info("hw init start\n");

	is_hw_init_running = true;
#ifdef USE_CAMERA_HW_BIG_DATA
	need_update_to_file = false;
	fimc_is_sec_copy_err_cnt_from_file();
#endif
	fimc_is_load_ctrl_init();
	fimc_is_load_ctrl_lock();
	ret = fimc_is_vender_check_sensor(core);
	if (ret) {
		err("Do not init hw routine. Check sensor failed!\n");
		is_hw_init_running = false;
		fimc_is_load_ctrl_unlock();
		return -EINVAL;
	} else {
		info("Start hw init. Check sensor success!\n");
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_REAR);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for rear is fail(%d)", ret);
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_FRONT);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for front is fail(%d)", ret);
	}
#endif

#if defined(CAMERA_MODULE_ES_VERSION_REAR2)
	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_REAR2);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for rear2 is fail(%d)", ret);
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_REAR3);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for rear3 is fail(%d)", ret);
	}
#endif

#if defined (CONFIG_OIS_USE)
	fimc_is_ois_fw_update(core);
#endif

	ret = fimc_is_load_bin_on_boot();
	if (ret) {
		err("fimc_is_load_bin_on_boot is fail(%d)", ret);
	}
	fimc_is_load_ctrl_unlock();
	is_hw_init_running = false;

	info("hw init done\n");
	return 0;
}

int fimc_is_vender_fw_prepare(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_preproc *device;
	struct fimc_is_vender_specific *specific;

	WARN_ON(!vender);

	specific = vender->private_data;
	core = container_of(vender, struct fimc_is_core, vender);
	device = &core->preproc;

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_REAR);
	if (core->current_position == SENSOR_POSITION_REAR) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail1(%d)", ret);
			goto p_err;
		}
	}

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_FRONT);
	if (core->current_position == SENSOR_POSITION_FRONT) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail2(%d)", ret);
			goto p_err;
		}
	}

#ifndef CONFIG_USE_DIRECT_IS_CONTROL
	/* Set SPI function */
	fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_ISP_FW);
#endif

p_err:
	return ret;
}

#if defined(CAMERA_FW_LOADING_FROM)
/**
  * fimc_is_vender_request_binary: send loading request to the loader
  * @bin: pointer to fimc_is_binary structure
  * @path: path of binary file
  * @name: name of binary file
  * @device: device for which binary is being loaded
  **/
int fimc_is_vender_request_binary(struct fimc_is_binary *bin, const char *path1, const char *path2,
				const char *name, struct device *device)
{
	char *filename;
	unsigned int retry_cnt = 0;
	int retry_err = 0;
	int ret;

	bin->data = NULL;
	bin->size = 0;
	bin->fw = NULL;

	/* whether the loader is customized or not */
	if (bin->customized != (unsigned long)bin) {
		bin->alloc = &vmalloc;
		bin->free =  &vfree;
	} else {
		retry_cnt = bin->retry_cnt;
		retry_err = bin->retry_err;
	}

	/* read the requested binary from file system directly */
	if (path1) {
		filename = __getname();
		if (unlikely(!filename))
			return -ENOMEM;

		snprintf(filename, PATH_MAX, "%s%s", path1, name);
		ret = get_filesystem_binary(filename, bin);
		__putname(filename);
		/* read successfully or don't want to go further more */
		if (!ret || !device) {
			info("%s path1 load(%s) \n", __func__, name);
			return ret;
		}
	}

	/* read the requested binary from file system directly DUMP  */
	if (path2 && is_dumped_fw_loading_needed) {
		filename = __getname();
		if (unlikely(!filename))
			return -ENOMEM;

		snprintf(filename, PATH_MAX, "%s%s", path2, name);
		ret = get_filesystem_binary(filename, bin);
		__putname(filename);
		/* read successfully or don't want to go further more */
		if (!ret || !device) {
			info("%s path2 load(%s) \n", __func__, name);
			return ret;
		}
	}

	/* ask to 'request_firmware' */
	do {
		ret = request_firmware(&bin->fw, name, device);

		if (!ret && bin->fw) {
			bin->data = (void *)bin->fw->data;
			bin->size = bin->fw->size;

			info("%s path3 load(%s) \n", __func__, name);
			break;
		}

		/*
		 * if no specific error for retry is given;
		 * whatever any error is occurred, we should retry
		 */
		if (!bin->retry_err)
			retry_err = ret;
	} while (retry_cnt-- && (retry_err == ret));

	return ret;
}
#endif

int fimc_is_vender_fw_filp_open(struct fimc_is_vender *vender, struct file **fp, int bin_type)
{
	int ret = FW_SKIP;
	struct fimc_is_from_info *sysfs_finfo;
	char fw_path[FIMC_IS_PATH_LEN];
	struct fimc_is_core *core;

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	core = container_of(vender, struct fimc_is_core, vender);
	memset(fw_path, 0x00, sizeof(fw_path));

	if (bin_type == FIMC_IS_BIN_FW) {
		if (is_dumped_fw_loading_needed) {
			snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_fw_name);
			*fp = filp_open(fw_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(*fp)) {
				*fp = NULL;
				ret = FW_FAIL;
			} else {
				ret = FW_SUCCESS;
			}
		} else {
			ret = FW_SKIP;
		}
	} else if (bin_type == FIMC_IS_BIN_SETFILE) {
		if (is_dumped_fw_loading_needed) {
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
			if (core->current_position == SENSOR_POSITION_FRONT) {
				snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_front_setfile_name);
			} else
#endif
			{
				snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_setfile_name);
			}
			*fp = filp_open(fw_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(*fp)) {
				*fp = NULL;
				ret = FW_FAIL;
			} else {
				ret = FW_SUCCESS;
			}
		} else {
			ret = FW_SKIP;
		}
	}

	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) \
    	|| defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT) \
    	|| defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
static int fimc_is_ischain_loadcalb_eeprom(struct fimc_is_core *core,
	struct fimc_is_module_enum *active_sensor, int position)
{
	int ret = 0;
	char *cal_ptr;
	char *cal_buf = NULL;
	u32 start_addr = 0;
	int cal_size = 0;
	struct fimc_is_from_info *finfo;
	struct fimc_is_from_info *pinfo;
	char *loaded_fw_ver;

	info("%s\n", __func__);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		err("Camera : Did not load cal data.");
		return 0;
	}

	cal_size = fimc_is_sec_get_max_cal_size(position);
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &cal_buf);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		start_addr = CAL_OFFSET1;
#ifdef ENABLE_IS_CORE
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);
#else
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr_front_cal);
#endif
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3) \
	||defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		start_addr = CAL_OFFSET0;
#ifdef ENABLE_IS_CORE
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);
#else
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr_rear_cal);
#endif
#endif
	}

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_loaded_fw(&loaded_fw_ver);

	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		info("CAL DATA : MAP ver : %c%c%c%c\n",
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT + 1],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT + 2],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_FRONT + 3]);
#endif
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		info("CAL DATA : MAP ver : %c%c%c%c\n",
			cal_buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT],
			cal_buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT+1],
			cal_buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT+2],
			cal_buf[OTP_HEADER_CAL_MAP_VER_START_ADDR_FRONT+3]);
#endif
	} else if (position == SENSOR_POSITION_REAR3) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		info("CAL DATA : MAP ver : %c%c%c%c\n",
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_REAR3],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_REAR3 + 1],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_REAR3 + 2],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR_REAR3 + 3]);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		info("CAL DATA : MAP ver : %c%c%c%c\n",
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR + 1],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR + 2],
			cal_buf[EEP_I2C_HEADER_CAL_MAP_VER_START_ADDR + 3]);
#endif
	}

	if (position == SENSOR_POSITION_FRONT2) {
		info("eeprom_fw_version = %s, phone_fw_version = %s, loaded_fw_version = %s\n",
			finfo->header2_ver, pinfo->header_ver, loaded_fw_ver);
	} else {
		info("eeprom_fw_version = %s, phone_fw_version = %s, loaded_fw_version = %s\n",
			finfo->header_ver, pinfo->header_ver, loaded_fw_ver);
	}

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT) {
		if (crc32_check_front == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Front Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check_front == true) {
				err("Front Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Front Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	} else if (position == SENSOR_POSITION_FRONT2) {
		if (crc32_check_front2 == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Front2 Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check_front == true) {
				err("Front2 Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Front2 Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	} else if (position == SENSOR_POSITION_REAR3) {
		if (crc32_check_rear3 == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Rear3 Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check_rear3 == true) {
				err("Rear3 Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Rear3 Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	} else {
		if (crc32_check == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Rear Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check == true) {
				err("Rear Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Rear Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	}

#ifdef ENABLE_IS_CORE
	CALL_BUFOP(core->resourcemgr.minfo.pb_fw, sync_for_device,
		core->resourcemgr.minfo.pb_fw,
		start_addr, cal_size, DMA_TO_DEVICE);
#else
	if (position == SENSOR_POSITION_FRONT || position == SENSOR_POSITION_FRONT2) {
		CALL_BUFOP(core->resourcemgr.minfo.pb_front_cal, sync_for_device,
				core->resourcemgr.minfo.pb_front_cal,
				0, cal_size, DMA_TO_DEVICE);
	} else {
		CALL_BUFOP(core->resourcemgr.minfo.pb_rear_cal, sync_for_device,
				core->resourcemgr.minfo.pb_rear_cal,
				0, cal_size, DMA_TO_DEVICE);
	}
#endif

	if (ret)
		warn("calibration loading is fail");
	else
		info("calibration loading is success\n");

	return ret;
}
#endif

#if 0 //not used for mid-tier !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
static int fimc_is_ischain_loadcalb(struct fimc_is_core *core,
	struct fimc_is_module_enum *active_sensor, int position)
{
	int ret = 0;
	char *cal_ptr;
	u32 start_addr = 0;
	int cal_size = 0;
	struct fimc_is_from_info *sysfs_finfo;
	struct fimc_is_from_info *sysfs_pinfo;
	char *loaded_fw_ver;
	char *cal_buf;

	if (!fimc_is_sec_check_from_ver(core, position)) {
		err("Camera : Did not load cal data.");
		return 0;
	}

	if (position == SENSOR_POSITION_FRONT) {
		start_addr = CAL_OFFSET1;
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		fimc_is_sec_get_sysfs_finfo_front(&sysfs_finfo);
		fimc_is_sec_get_front_cal_buf(&cal_buf);
#ifdef ENABLE_IS_CORE
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);
#else
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr_front_cal);
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		start_addr = CAL_OFFSET0;
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
		fimc_is_sec_get_cal_buf(SENSOR_POSITION_REAR, &cal_buf);
#ifdef ENABLE_IS_CORE
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);
#else
		cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr_rear_cal);
#endif
#endif
	}

	fimc_is_sec_get_sysfs_pinfo(&sysfs_pinfo);
	fimc_is_sec_get_loaded_fw(&loaded_fw_ver);

	info("CAL DATA : MAP ver : %c%c%c%c\n", cal_buf[0x60], cal_buf[0x61],
		cal_buf[0x62], cal_buf[0x63]);

	info("from_fw_version = %s, phone_fw_version = %s, loaded_fw_version = %s\n",
		sysfs_finfo->header_ver, sysfs_pinfo->header_ver, loaded_fw_ver);

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT) {
		if (crc32_check_front  == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Camera : the dumped Cal. data was applied successfully.\n");
		} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
			if (crc32_header_check_front  == true) {
				err("Camera : CRC32 error but only header section is no problem.");
				memcpy((void *)(cal_ptr),
					(void *)cal_buf,
					EEP_HEADER_CAL_DATA_START_ADDR);
				memset((void *)(cal_ptr + EEP_HEADER_CAL_DATA_START_ADDR),
					0xFF,
					cal_size - EEP_HEADER_CAL_DATA_START_ADDR);
			} else {
				err("Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
#endif
		}
	} else {
		if (crc32_check == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Camera : the dumped Cal. data was applied successfully.\n");
		} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
			if (crc32_header_check == true) {
				err("Camera : CRC32 error but only header section is no problem.");
				memcpy((void *)(cal_ptr),
					(void *)cal_buf,
					FROM_HEADER_CAL_DATA_START_ADDR);
				memset((void *)(cal_ptr + FROM_HEADER_CAL_DATA_START_ADDR),
					0xFF,
					cal_size - FROM_HEADER_CAL_DATA_START_ADDR);
			} else {
				err("Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
#endif
		}
	}

#ifdef ENABLE_IS_CORE
	CALL_BUFOP(core->resourcemgr.minfo.pb_fw, sync_for_device,
		core->resourcemgr.minfo.pb_fw,
		start_addr, cal_size, DMA_TO_DEVICE);
#else
	if (position == SENSOR_POSITION_FRONT) {
		CALL_BUFOP(core->resourcemgr.minfo.pb_front_cal, sync_for_device,
				core->resourcemgr.minfo.pb_front_cal,
				0, cal_size, DMA_TO_DEVICE);
	} else {
		CALL_BUFOP(core->resourcemgr.minfo.pb_rear_cal, sync_for_device,
				core->resourcemgr.minfo.pb_rear_cal,
				0, cal_size, DMA_TO_DEVICE);
	}
#endif

	if (ret)
		warn("calibration loading is fail");
	else
		info("calibration loading is success\n");

	return ret;
}
#endif

int fimc_is_vender_cal_load(struct fimc_is_vender *vender,
	void *module_data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module = module_data;

	core = container_of(vender, struct fimc_is_core, vender);

	if(module->position == SENSOR_POSITION_FRONT || module->position == SENSOR_POSITION_FRONT2) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		module->ext.sensor_con.cal_address = CAL_OFFSET1;
		ret = fimc_is_ischain_loadcalb_eeprom(core, NULL, module->position);
		if (ret) {
			err("loadcalb fail, load default caldata\n");
		}
#else
		module->ext.sensor_con.cal_address = 0;
#endif
	} else {
		/* Load calibration data from sensor */
		module->ext.sensor_con.cal_address = CAL_OFFSET0;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR3)
		ret = fimc_is_ischain_loadcalb_eeprom(core, NULL, module->position);
#else
#if 0 //not used for mid-tier
		ret = fimc_is_ischain_loadcalb(core, NULL, SENSOR_POSITION_REAR);
#endif
#endif
		if (ret) {
			err("loadcalb fail, load default caldata\n");
		}
	}

	return ret;
}

int fimc_is_vender_module_sel(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_module_del(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_fw_sel(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct device *dev;
	struct fimc_is_from_info *sysfs_finfo;

	WARN_ON(!vender);

	core = container_of(vender, struct fimc_is_core, vender);
	dev = &core->pdev->dev;

	if (!test_bit(FIMC_IS_PREPROC_S_INPUT, &core->preproc.state)) {
		ret = fimc_is_sec_run_fw_sel(dev, core->current_position);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			goto p_err;
		}
	}

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s",
		sysfs_finfo->load_fw_name);

p_err:
	return ret;
}

int fimc_is_vender_setfile_sel(struct fimc_is_vender *vender,
	char *setfile_name,
	int position)
{
	int ret = 0;
	struct fimc_is_core *core;

	WARN_ON(!vender);
	WARN_ON(!setfile_name);

	core = container_of(vender, struct fimc_is_core, vender);

	snprintf(vender->setfile_path[position], sizeof(vender->setfile_path[position]),
		"%s%s", FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path[position],
		sizeof(vender->request_setfile_path[position]),
		"%s", setfile_name);

	return ret;
}

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
void sensor_pwr_ctrl(struct work_struct *work)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_enum *g_module = NULL;
	struct fimc_is_core *core;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return;
	}

	ret = fimc_is_preproc_g_module(&core->preproc, &g_module);
	if (ret) {
		err("fimc_is_sensor_g_module is fail(%d)", ret);
		return;
	}

	pdata = g_module->pdata;
	ret = pdata->gpio_cfg(g_module, SENSOR_SCENARIO_NORMAL,
		GPIO_SCENARIO_STANDBY_OFF_SENSOR);
	if (ret) {
		err("gpio_cfg(sensor) is fail(%d)", ret);
	}
}

static DECLARE_DELAYED_WORK(sensor_pwr_ctrl_work, sensor_pwr_ctrl);
#endif

int fimc_is_vender_preprocessor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int fimc_is_vender_sensor_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario)
{
	struct fimc_is_core *core;
	int ret = 0;

	core = container_of(vender, struct fimc_is_core, vender);

#if 0
	/* In dual camera scenario,
	while loading cal data to C3 with spi in rear camera, changing spi config in front camera is not valid.
	Due to this issue, disable spi config here. (C3 + spi0, spi1 use case in rear camera)
	Need to consider this on other project later depending on spi use cases.
	*/
	/* Set spi pin to out */
	fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);
	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int fimc_is_vender_preprocessor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario,
			void *module_data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;
#ifdef CONFIG_SENSOR_RETENTION_USE
	struct fimc_is_from_info *sysfs_finfo;
#if defined(CONFIG_OIS_USE)
	struct fimc_is_module_enum *module = module_data;
#endif
	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
#endif

	specific = vender->private_data;
	core = container_of(vender, struct fimc_is_core, vender);

	/* Set spi pin to out */
	fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);
	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	if (scenario == SENSOR_SCENARIO_NORMAL) {
		{
			if (GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_ON) {
#ifdef CONFIG_SENSOR_RETENTION_USE
				if (specific->rear_sensor_id == SENSOR_NAME_S5K2L2 && force_caldata_dump == false) {
					*gpio_scenario = GPIO_SCENARIO_SENSOR_RETENTION_ON;

#if defined(CONFIG_OIS_USE)
					/* Enable OIS gyro sleep */
					if (module->position == SENSOR_POSITION_REAR) {
						fimc_is_ois_gyro_sleep(core);
					}
#endif
					/* Set i2c pin to default */
					fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_DEFAULT);
					fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_DEFAULT);
				} else
#endif
				{
					*gpio_scenario = GPIO_SCENARIO_STANDBY_ON;
				}
			}
		}
	}
#endif

	return ret;
}

int fimc_is_vender_preprocessor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	struct fimc_is_vender_specific *specific;

	specific = vender->private_data;

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	if (scenario == SENSOR_SCENARIO_NORMAL) {
		if (gpio_scenario == GPIO_SCENARIO_STANDBY_ON
#ifdef CONFIG_SENSOR_RETENTION_USE
			|| gpio_scenario == GPIO_SCENARIO_SENSOR_RETENTION_ON
#endif
			) {
			SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_STANDBY);
		} else if (gpio_scenario == GPIO_SCENARIO_OFF) {
			SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
		}
		info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION));
	}
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario,
		void *module_data)
{
	int ret = 0;
#ifdef CONFIG_SENSOR_RETENTION_USE
	struct fimc_is_module_enum *module = module_data;
	struct sensor_open_extended *ext_info;
	struct fimc_is_core *core;

	core = container_of(vender, struct fimc_is_core, vender);
	ext_info = &(((struct fimc_is_module_enum *)module)->ext);

	if ((ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED)
		&& (scenario == SENSOR_SCENARIO_NORMAL)) {
		*gpio_scenario = GPIO_SCENARIO_SENSOR_RETENTION_ON;
#if defined(CONFIG_OIS_USE) && defined(USE_OIS_SLEEP_MODE)
		/* Enable OIS gyro sleep */
		if (module->position == SENSOR_POSITION_REAR)
			fimc_is_ois_gyro_sleep(core);
#endif
		info("%s: use_retention_mode\n", __func__);
	}
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;

	return ret;
}

#ifdef CONFIG_SENSOR_RETENTION_USE
void fimc_is_vender_check_retention(struct fimc_is_vender *vender, void *module_data)
{
	struct fimc_is_vender_specific *specific;
	struct fimc_is_rom_info *sysfs_finfo;
	struct fimc_is_module_enum *module = module_data;
	struct sensor_open_extended *ext_info;

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo, ROM_ID_REAR);
	specific = vender->private_data;
	ext_info = &(((struct fimc_is_module_enum *)module)->ext);

	if ((ext_info->use_retention_mode != SENSOR_RETENTION_UNSUPPORTED)
		&& (force_caldata_dump == false)) {
		info("Sensor[id = %d] use retention mode.\n", specific->rear_sensor_id);
	} else { /* force_caldata_dump == true */
		ext_info->use_retention_mode = SENSOR_RETENTION_UNSUPPORTED;
		info("Sensor[id = %d] does not support retention mode.\n", specific->rear_sensor_id);
	}

	return;
}
#endif

void fimc_is_vender_sensor_s_input(struct fimc_is_vender *vender, void *module)
{
	fimc_is_vender_fw_prepare(vender);

	return;
}

void fimc_is_vender_itf_open(struct fimc_is_vender *vender, struct sensor_open_extended *ext_info)
{
	struct fimc_is_vender_specific *specific;
	struct fimc_is_from_info *sysfs_finfo;
	struct fimc_is_core *core;

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	specific = vender->private_data;
	core = container_of(vender, struct fimc_is_core, vender);

#ifndef CONFIG_USE_DIRECT_IS_CONTROL
	fimc_is_vender_check_retention(vendor, ext_info);
#endif

/*****
 Exynos7885 don't use 'core->client0/1'
 core->client0/1 must be probed 'fimc_is_i2c0'
 But Exynos7885 don't use 'fimc_is_i2c0' and don't work the 'fimc_is_i2c0_probe'
*****/

#if 0

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	/* Exynos7885 don't use "I2C_PIN_STATE_OFF" in eeprom control */
	//fimc_is_i2c_s_pin(specific->eeprom_client0, I2C_PIN_STATE_OFF);
#endif

#ifdef CONFIG_USE_DIRECT_IS_CONTROL
	fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_ON);
#else
	fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_FW);
#endif

#if defined(CONFIG_OIS_USE)
#ifdef CONFIG_USE_DIRECT_IS_CONTROL
	fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_ON);
#else
	fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_OFF);
#endif
#endif

#endif //#if 0

	return;
}

/* Flash Mode Control */
#ifdef CONFIG_LEDS_LM3560
extern int lm3560_reg_update_export(u8 reg, u8 mask, u8 data);
#endif
#ifdef CONFIG_LEDS_SKY81296
extern int sky81296_torch_ctrl(int state);
#endif
#if defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
extern int s2mpb02_set_torch_current(bool movie);
#endif

#ifdef CONFIG_LEDS_SUPPORT_FRONT_FLASH_AUTO
int fimc_is_vender_set_torch(u32 aeflashMode, u32 frontFlashMode)
{
	info("%s : aeflashMode(%d), frontFlashMode(%d)", __func__, aeflashMode, frontFlashMode);
	switch (aeflashMode) {
	case AA_FLASHMODE_ON_ALWAYS: /*TORCH(MOVIE) mode*/
#if defined(CONFIG_LEDS_S2MU005_FLASH)
		s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_MOVIE);
#endif
		break;
	case AA_FLASHMODE_START: /*Pre flash mode*/
	case AA_FLASHMODE_ON: /* Main flash Mode */
#if defined(CONFIG_LEDS_S2MU005_FLASH) && defined(CONFIG_LEDS_SUPPORT_FRONT_FLASH)
		if(frontFlashMode == CAM2_FLASH_MODE_LCD)
			s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_FLASH);
#endif
		break;
	case AA_FLASHMODE_CAPTURE: /*Main flash mode*/
		break;
	case AA_FLASHMODE_OFF: /*OFF mode*/
#if defined(CONFIG_LEDS_S2MU005_FLASH)
		s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif
		break;
	default:
		break;
	}

	return 0;
}
#else
int fimc_is_vender_set_torch(u32 aeflashMode)
{
	switch (aeflashMode) {
	case AA_FLASHMODE_ON_ALWAYS: /*TORCH mode*/
#ifdef CONFIG_LEDS_LM3560
		lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
		sky81296_torch_ctrl(1);
#elif defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
		s2mpb02_set_torch_current(true);
#elif defined(CONFIG_LEDS_S2MU005_FLASH)
		s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_MOVIE);
#endif
		break;
	case AA_FLASHMODE_START: /*Pre flash mode*/
#ifdef CONFIG_LEDS_LM3560
		lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
		sky81296_torch_ctrl(1);
#endif
#if defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
		s2mpb02_set_torch_current(false);
#endif
		break;
	case AA_FLASHMODE_CAPTURE: /*Main flash mode*/
		break;
	case AA_FLASHMODE_OFF: /*OFF mode*/
#ifdef CONFIG_LEDS_SKY81296
		sky81296_torch_ctrl(0);
#elif defined(CONFIG_LEDS_S2MU005_FLASH)
		s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif
		break;
	default:
		break;
	}

	return 0;
}
#endif

int fimc_is_vender_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = (struct fimc_is_device_ischain *)device_data;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;
	unsigned int value = 0;
	unsigned int captureIntent = 0;
	unsigned int captureCount = 0;

	WARN_ON(!device);
	WARN_ON(!ctrl);

	core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);
	specific = core->vender.private_data;

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		ctrl->id = VENDER_S_CTRL;
		value = (unsigned int)ctrl->value;
		captureIntent = (value >> 16) & 0x0000FFFF;
		switch (captureIntent) {
		case AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_MFHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_VEHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_VENR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLS_FLASH:
			captureCount = value & 0x0000FFFF;
			break;
		default:
			captureIntent = ctrl->value;
			captureCount = 0;
			break;
		}

		device->group_3aa.intent_ctl.captureIntent = captureIntent;
		device->group_3aa.intent_ctl.vendor_captureCount = captureCount;
		minfo("[VENDER] s_ctrl intent(%d) count(%d)\n", device, captureIntent, captureCount);
		break;
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
		ctrl->id = VENDER_S_CTRL;
		device->group_3aa.intent_ctl.vendor_captureExposureTime = ctrl->value;
		minfo("[VENDER] s_ctrl vendor_captureExposureTime(%d)\n", device, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		ctrl->id = VENDER_S_CTRL;
		switch (ctrl->value) {
		case IS_COLD_BOOT:
			/* change value to X when !TWIZ | front */
			fimc_is_itf_fwboot_init(device->interface);
			break;
		case IS_WARM_BOOT:
			if (specific ->need_cold_reset) {
				minfo("[VENDER] FW first launching mode for reset\n", device);
				device->interface->fw_boot_mode = FIRST_LAUNCHING;
			} else {
				/* change value to X when TWIZ & back | frist time back camera */
				if (!test_bit(IS_IF_LAUNCH_FIRST, &device->interface->launch_state))
					device->interface->fw_boot_mode = FIRST_LAUNCHING;
				else
					device->interface->fw_boot_mode = WARM_BOOT;
			}
			break;
		case IS_COLD_RESET:
			specific ->need_cold_reset = true;
			minfo("[VENDER] need cold reset!!!\n", device);
			break;
		default:
			err("[VENDER]unsupported ioctl(0x%X)", ctrl->id);
			ret = -EINVAL;
			break;
		}
		break;
#ifdef CONFIG_SENSOR_RETENTION_USE
	case V4L2_CID_IS_PREVIEW_STATE:
		ctrl->id = VENDER_S_CTRL;
#if 0 /* Do not control error state at Host side. Controled by Firmware */
		specific->need_retention_init = true;
		err("[VENDER]  need_retention_init = %d\n", specific->need_retention_init);
#endif
		break;
#endif
	}

	return ret;
}

int fimc_is_vender_ssx_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

int fimc_is_vender_ssx_video_g_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

bool fimc_is_vender_wdr_mode_on(void *cis_data)
{
	bool ret = false;
#ifndef USE_WDR_INTERFACE
	ret = (((cis_shared_data *)cis_data)->companion_data.wdr_mode != COMPANION_WDR_OFF ? true : false);
#endif
	return ret;
}

bool fimc_is_vender_enable_wdr(void *cis_data)
{
	return false;
}

void fimc_is_vender_resource_get(struct fimc_is_vender *vender)
{

}

void fimc_is_vender_resource_put(struct fimc_is_vender *vender)
{

}

int fimc_is_vender_remove_dump_fw_file(void)
{
	info("%s\n", __func__);
	remove_dump_fw_file();

	return 0;
}

