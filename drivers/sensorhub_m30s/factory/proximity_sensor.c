#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "../ssp_data.h"
#include "ssp_factory.h"


/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static ssize_t proximity_name_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_name == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_name(buf);
}

static ssize_t proximity_vendor_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_vendor == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_vendor(buf);
}

static ssize_t proximity_probe_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_probe_status == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_probe_status(data, buf);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_threshold_high == NULL)
		return -EINVAL;
	return data->proximity_ops->get_threshold_high(data, buf);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
					   struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;
	if (data->proximity_ops == NULL || data->proximity_ops->set_threshold_high == NULL)
		return -EINVAL;

	ret = data->proximity_ops->set_threshold_high(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_threshold_low == NULL)
		return -EINVAL;
	return data->proximity_ops->get_threshold_low(data, buf);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->set_threshold_low == NULL)
		return -EINVAL;
	ret = data->proximity_ops->set_threshold_low(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
static ssize_t proximity_thresh_detect_high_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->set_threshold_low == NULL)
		return -EINVAL;
	return data->proximity_ops->get_threshold_detect_high(data, buf);
}

static ssize_t proximity_thresh_detect_high_store(struct device *dev,
						  struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->set_threshold_detect_high == NULL)
		return -EINVAL;
	ret = data->proximity_ops->set_threshold_detect_high(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

static ssize_t proximity_thresh_detect_low_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_threshold_detect_low == NULL)
		return -EINVAL;
	return data->proximity_ops->get_threshold_detect_low(data, buf);
}

static ssize_t proximity_thresh_detect_low_store(struct device *dev,
						 struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->set_threshold_detect_low == NULL)
		return -EINVAL;
	ret = data->proximity_ops->set_threshold_detect_low(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);
	return size;
}
#endif

static ssize_t proximity_raw_data_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	unsigned short raw_data = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_raw_data == NULL)
		return -EINVAL;
	raw_data = data->proximity_ops->get_proximity_raw_data(data);

	return sprintf(buf, "%u\n", raw_data);
}

static ssize_t proximity_default_trim_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_trim_value == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_trim_value(data, buf);
}

#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
static ssize_t proximity_default_trim_check_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_trim_value == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_trim_value(data, buf);
}
#endif

static ssize_t proximity_avg_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_avg_raw_data == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_avg_raw_data(data, buf);
}

static ssize_t proximity_avg_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->set_proximity_avg_raw_data == NULL)
		return -EINVAL;
	ret = data->proximity_ops->set_proximity_avg_raw_data(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);
	return size;
}

static ssize_t barcode_emul_enable_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->is_barcode_enabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0)
		return ret;

	if (dEnable)
		data->is_barcode_enabled = true;
	else
		data->is_barcode_enabled = false;
	ssp_info("Proximity Barcode En : %u", data->is_barcode_enabled);

	return size;
}

#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL)
static ssize_t proximity_cal_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->proximity_ops == NULL || data->proximity_ops->set_proximity_calibration == NULL)
		return -EINVAL;
	ret = data->proximity_ops->set_proximity_calibration(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);
	return size;
}

static ssize_t proximity_offset_pass_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->proximity_ops == NULL || data->proximity_ops->get_proximity_calibration_result == NULL)
		return -EINVAL;
	return data->proximity_ops->get_proximity_calibration_result(buf);
}
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
static ssize_t proximity_modify_settings_show(struct device *dev,
					      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	open_proximity_setting_mode(data);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->prox_setting_mode);
}

static ssize_t proximity_modify_settings_store(struct device *dev,
					       struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 temp;

	pr_info("[SSP] %s - %s\n", __func__, buf);

	ret = kstrtou8(buf, 10, &temp);
	if (ret < 0)
		return ret;

	if (temp == 1)
		data->prox_setting_mode = 1;
	else if (temp == 2) {
		data->prox_setting_mode = 2;
		memcpy(data->prox_thresh, data->prox_mode_thresh, sizeof(data->prox_thresh));
	} else {
		ssp_errf("invalid value %d", temp);
		return -EINVAL;
	}

	ssp_infof("prox_setting %d", temp);

	save_proximity_setting_mode(data);
	return size;
}

static ssize_t proximity_settings_thresh_high_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->prox_setting_thresh[0]);
}

static ssize_t proximity_settings_thresh_high_store(struct device *dev,
						    struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret;
	u16 settings_thresh;

	ret = kstrtou16(buf, 10, &settings_thresh);
	if (ret < 0) {
		ssp_errf("kstrto16 failed.(%d)", ret);
		return -EINVAL;
	} else
		data->prox_setting_thresh[0] = settings_thresh;

	ssp_infof("new prox setting high threshold %u", data->prox_setting_thresh[0]);

	return size;
}

static ssize_t proximity_settings_thresh_low_show(struct device *dev,
						  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->prox_setting_thresh[1]);
}

static ssize_t proximity_settings_thresh_low_store(struct device *dev,
						   struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret;
	u16 settings_thresh;

	ret = kstrtou16(buf, 10, &settings_thresh);
	if (ret < 0) {
		ssp_errf("kstrto16 failed.(%d)", ret);
		return -EINVAL;
	} else
		data->prox_setting_thresh[1] = settings_thresh;

	ssp_infof("new prox setting low threshold %u", data->prox_setting_thresh[1]);

	return size;
}
#endif

static DEVICE_ATTR(name, S_IRUGO, proximity_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, proximity_vendor_show, NULL);
static DEVICE_ATTR(prox_probe, S_IRUGO, proximity_probe_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_thresh_high_show, proximity_thresh_high_store);

static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_thresh_low_show, proximity_thresh_low_store);

#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
static DEVICE_ATTR(thresh_detect_high, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_thresh_detect_high_show, proximity_thresh_detect_high_store);

static DEVICE_ATTR(thresh_detect_low, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_thresh_detect_low_show, proximity_thresh_detect_low_store);
#endif

static DEVICE_ATTR(prox_trim, S_IRUGO, proximity_default_trim_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, proximity_raw_data_show, NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_avg_show, proximity_avg_store);

static DEVICE_ATTR(barcode_emul_en, S_IRUGO | S_IWUSR | S_IWGRP,
		   barcode_emul_enable_show, barcode_emul_enable_store);

#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
static DEVICE_ATTR(prox_trim_check, S_IRUGO, proximity_default_trim_check_show, NULL);
#endif

#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL)
static DEVICE_ATTR(prox_cal, S_IWUSR | S_IWGRP,
		   NULL, proximity_cal_store);

static DEVICE_ATTR(prox_offset_pass, S_IRUGO,
		   proximity_offset_pass_show, NULL);
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
static DEVICE_ATTR(modify_settings, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_modify_settings_show, proximity_modify_settings_store);
static DEVICE_ATTR(settings_thd_high, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_settings_thresh_high_show, proximity_settings_thresh_high_store);
static DEVICE_ATTR(settings_thd_low, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_settings_thresh_low_show, proximity_settings_thresh_low_store);
#endif

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
#endif
	&dev_attr_prox_trim,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_barcode_emul_en,
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	&dev_attr_prox_trim_check,
#endif
#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL)
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
#endif
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS)
	&dev_attr_modify_settings,
	&dev_attr_settings_thd_high,
	&dev_attr_settings_thd_low,
#endif
	NULL,
};

void select_prox_ops(struct ssp_data *data, char *name)
{
	struct proximity_sensor_operations **prox_ops_ary;
	int count = 0, i;
	char temp_buffer[SENSORNAME_MAX_LEN] = {0,};


	ssp_infof("");

#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_STK3X3X)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_GP2AP110S)
	count++;
#endif

	if (count == 0) {
		ssp_infof("count is 0");
		return;
	}

	prox_ops_ary = (struct proximity_sensor_operations **)kzalloc(count * sizeof(struct proximity_sensor_operations *), GFP_KERNEL);

	i = 0;
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	prox_ops_ary[i++] = get_proximity_ams_auto_cal_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_STK3X3X)
	prox_ops_ary[i++] = get_proximity_stk3x3x_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_GP2AP110S)
	prox_ops_ary[i++] = get_proximity_gp2ap110s_function_pointer(data);
#endif

	if (count > 1) {
		for (i = 0; i < count ; i++) {
			int size = prox_ops_ary[i]->get_proximity_name(temp_buffer);

			temp_buffer[size - 1] = '\0';
			ssp_infof("%d name : %s", i, temp_buffer);

			if (strcmp(temp_buffer, name) == 0)
				break;
		}

		if (i == count)
			i = 0;
	} else
		i = 0;

	data->proximity_ops = prox_ops_ary[i];
	kfree(prox_ops_ary);
}

void initialize_prox_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_PROXIMITY], data,
			 prox_attrs, "proximity_sensor");
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_PROXIMITY], prox_attrs);
}

