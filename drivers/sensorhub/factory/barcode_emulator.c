#include "../ssp.h"
#include "../ssp_comm.h"
#include "../ssp_cmd_define.h"
#include "ssp_factory.h"
#include <linux/sec_class.h>

#define VENDOR          "STM"
#define CHIP_ID         "STM32F410TBY6TR"

#define BEAMING_ON      1
#define BEAMING_OFF     0
#define STOP_BEAMING    0

#define COUNT_TEST      48 /* 0 */
#define REGISTER_TEST   49 /* 1 */
#define DATA_TEST       50 /* 2 */

#define OFFSET_DATA_SET 2
#define DATA_MAX_LEN    128

u8 is_beaming = BEAMING_OFF;

struct reg_index_table {
	unsigned char reg;
	unsigned char index;
};

static struct reg_index_table reg_id_table[15] = {
	{0x81, 0}, {0x88, 1}, {0x8F, 2}, {0x96, 3}, {0x9D, 4},
	{0xA4, 5}, {0xAB, 6}, {0xB2, 7}, {0xB9, 8}, {0xC0, 9},
	{0xC7, 10}, {0xCE, 11}, {0xD5, 12}, {0xDC, 13}, {0xE3, 14}
};


enum {
	reg,
	index,
};

void mobeam_on_off(struct ssp_data *data, u8 on_off)
{
	int ret;
	char buf[8] = {0,};

	if (on_off == is_beaming) {
		ssp_errf("skip %d", on_off);
		return;
	}

	if (on_off == BEAMING_ON) {
		ret = ssp_send_command(data, CMD_ADD, SENSOR_TYPE_MOBEAM, 0, 0, buf, 8, NULL,
		                       NULL);
	} else {
		ret = ssp_send_command(data, CMD_REMOVE, SENSOR_TYPE_MOBEAM, 0, 0, NULL, 0,
		                       NULL, NULL);
	}

	if (ret != SUCCESS) {
		ssp_errf(" fail %d\n", ret);
	} else {
		is_beaming = on_off;
		ssp_infof(" success(%u)", is_beaming);
	}
	return;
}


void mobeam_write(struct ssp_data *data, u8 sub_cmd, u8 *u_buf, u16 len)
{
	int ret = 0;

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_MOBEAM))) {
		ssp_errf(" Skip this function!!!"\
		         ", mobeam sensor is not connected(0x%llx)",
		         data->sensor_probe_state);
		return;
	}

	if (is_beaming == BEAMING_ON) {
		ssp_errf(" skip subcmd %d, already beaming on", sub_cmd);
		return;
	}

	ssp_infof(" start, subcmd = %d", sub_cmd);

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_MOBEAM, sub_cmd, 0,
	                       u_buf, len, NULL, NULL);


	if (ret != SUCCESS) {
		ssp_errf(" CMD fail %d\n", ret);
		return;
	}

	return;
}

static ssize_t mobeam_vendor_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t mobeam_name_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t barcode_emul_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 send_buf[DATA_MAX_LEN] = { 0, };
	int i;
	u16 len;

	memset(send_buf, 0xFF, 128);
	if (buf[0] == 0xFF && buf[1] != STOP_BEAMING) {
		ssp_infof(" - START BEAMING(0x%X, 0x%X)", buf[0], buf[1]);
		send_buf[0] = buf[1];
		mobeam_write(data, MOBEAM_BIT_LEN, send_buf, 1);
		mobeam_on_off(data, BEAMING_ON);
	} else if (buf[0] == 0xFF && buf[1] == STOP_BEAMING) {
		ssp_infof(" - STOP BEAMING(0x%X, 0x%X)", buf[0], buf[1]);
		mobeam_on_off(data, BEAMING_OFF);
	} else if (buf[0] == 0x00) {
		ssp_infof(" - DATA SET(0x%X, 0x%X)", buf[0], buf[1]);
		len = (int)size - OFFSET_DATA_SET;
		if (len > DATA_MAX_LEN) {
			len = DATA_MAX_LEN;
		}

		memcpy(send_buf, &buf[2], len);
		ssp_infof(" - %u %u %u %u %u %u", send_buf[0], send_buf[1], send_buf[2],
		          send_buf[3], send_buf[4], send_buf[5]);

		mobeam_write(data, MOBEAM_DATA, send_buf, len);
	} else if (buf[0] == 0x80) {
		ssp_infof(" - HOP COUNT SET(0x%X, 0x%X)", buf[0], buf[1]);
		send_buf[0] = buf[1];
		mobeam_write(data, MOBEAM_HOP_COUNT, send_buf, 1);
	} else {
		ssp_infof(" - REGISTER SET(0x%X)", buf[0]);
		for (i = 0; i < 15; i++) {
			if (reg_id_table[i].reg == buf[0]) {
				send_buf[0] = reg_id_table[i].index;
			}
		}
		send_buf[1] = buf[1];
		send_buf[2] = buf[2];
		send_buf[3] = buf[4];
		send_buf[4] = buf[5];
		send_buf[5] = buf[7];
		mobeam_write(data, MOBEAM_HOP_TABLE, send_buf, 6);
	}
	return size;
}

static ssize_t barcode_emul_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
	return strlen(buf);
}

static ssize_t barcode_led_status_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", is_beaming);
}

static ssize_t barcode_ver_check_show(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", 15);
}

static ssize_t barcode_emul_test_store(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 barcode_data[14] = {0xFF, 0xAC, 0xDB, 0x36, 0x42, 0x85,
	                       0x0A, 0xA8, 0xD1, 0xA3, 0x46, 0xC5, 0xDA, 0xFF
	                      };
	u8 test_data[128] = { 0, };

	memset(test_data, 0xFF, 128);
	if (buf[0] == COUNT_TEST) {
		test_data[0] = 0x80;
		test_data[1] = 1;
		ssp_infof(" COUNT_TEST - 0x%X, %u", test_data[0], test_data[1]);
		mobeam_write(data, MOBEAM_HOP_COUNT, &test_data[1], 1);
	} else if (buf[0] == REGISTER_TEST) {
		test_data[0] = 0;
		test_data[1] = 10;
		test_data[2] = 20;
		test_data[3] = 30;
		test_data[4] = 40;
		test_data[5] = 50;
		ssp_infof(" REGISTER_TEST - %u: %u %u %u %u %u",
		          test_data[0], test_data[1], test_data[2],
		          test_data[3], test_data[4], test_data[5]);
		mobeam_write(data, MOBEAM_HOP_TABLE, test_data, 6);
	} else if (buf[0] == DATA_TEST) {
		memcpy(test_data, &barcode_data[1], 13);
		ssp_infof(" DATA_TEST - 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X",
		          test_data[0], test_data[1], test_data[2],
		          test_data[3], test_data[4], test_data[5]);
		mobeam_write(data, MOBEAM_DATA, test_data, 13);
	}
	return size;
}
static DEVICE_ATTR(vendor, S_IRUGO, mobeam_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, mobeam_name_show, NULL);
static DEVICE_ATTR(barcode_send, S_IRUGO | S_IWUSR | S_IWGRP,
                   barcode_emul_show, barcode_emul_store);
static DEVICE_ATTR(barcode_led_status, S_IRUGO, barcode_led_status_show, NULL);
static DEVICE_ATTR(barcode_ver_check, S_IRUGO, barcode_ver_check_show, NULL);
static DEVICE_ATTR(barcode_test_send, S_IWUSR | S_IWGRP,
                   NULL, barcode_emul_test_store);

void initialize_mobeam(struct ssp_data *data)
{
	pr_info("[SSP] %s\n", __func__);
	data->devices[SENSOR_TYPE_MOBEAM] = sec_device_create(data, "sec_barcode_emul");

	if (IS_ERR(data->devices[SENSOR_TYPE_MOBEAM])) {
		pr_err("[SSP] Failed to create mobeam_dev device\n");
	}

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_vendor) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_vendor.attr.name);

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_name) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_name.attr.name);

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_send) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_barcode_send.attr.name);

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_led_status) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_barcode_led_status.attr.name);

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_ver_check) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_barcode_ver_check.attr.name);

	if (device_create_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_test_send) < 0)
		pr_err("[SSP] Failed to create device file(%s)!\n",
		       dev_attr_barcode_test_send.attr.name);
	is_beaming = BEAMING_OFF;
}

void remove_mobeam(struct ssp_data *data)
{
	pr_info("[SSP] %s\n", __func__);

	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_test_send);
	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_ver_check);
	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_led_status);
	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_barcode_send);
	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_name);
	device_remove_file(data->devices[SENSOR_TYPE_MOBEAM], &dev_attr_vendor);
}
