/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Test Functions (Optional)
 *
 */

#include "melfas_mss100.h"

#if MMS_USE_DEV_MODE

/**
 * Dev node output to user
 */
static ssize_t mms_dev_fs_read(struct file *fp, char *rbuf, size_t cnt, loff_t *fpos)
{
	struct mms_ts_info *info = fp->private_data;
	int ret = 0;

	ret = copy_to_user(rbuf, info->dev_fs_buf, cnt);
	return ret;
}

/**
 * Dev node input from user
 */
static ssize_t mms_dev_fs_write(struct file *fp, const char *wbuf, size_t cnt, loff_t *fpos)
{
	struct mms_ts_info *info = fp->private_data;
	u8 *buf;
	int ret = 0;
	int cmd = 0;

	buf = kzalloc(cnt + 1, GFP_KERNEL);

	if ((buf == NULL) || copy_from_user(buf, wbuf, cnt)) {
		input_err(true, &info->client->dev, "%s [ERROR] copy_from_user\n", __func__);
		ret = -EIO;
		goto EXIT;
	}

	cmd = buf[cnt - 1];

	if (cmd == 1) {
		if (mms_i2c_read(info, buf, (cnt - 2), info->dev_fs_buf, buf[cnt - 2]))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
	} else if (cmd == 2) {
		if (mms_i2c_write(info, buf, (cnt - 1)))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		}

EXIT:
	kfree(buf);

	return ret;
}

/**
 * Open dev node
 */
static int mms_dev_fs_open(struct inode *node, struct file *fp)
{
	struct mms_ts_info *info = container_of(node->i_cdev, struct mms_ts_info, cdev);

	fp->private_data = info;
	info->dev_fs_buf = kzalloc(1024 * 4, GFP_KERNEL);
	return 0;
}

/**
 * Close dev node
 */
static int mms_dev_fs_release(struct inode *node, struct file *fp)
{
	struct mms_ts_info *info = fp->private_data;

	kfree(info->dev_fs_buf);
	return 0;
}

/**
 * Dev node info
 */
static struct file_operations mms_dev_fops = {
	.owner	= THIS_MODULE,
	.open	= mms_dev_fs_open,
	.release	= mms_dev_fs_release,
	.read	= mms_dev_fs_read,
	.write	= mms_dev_fs_write,
};

/**
 * Create dev node
 */
int mms_dev_create(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (alloc_chrdev_region(&info->mms_dev, 0, 1, MMS_DEVICE_NAME)) {
		input_err(true, &client->dev, "%s [ERROR] alloc_chrdev_region\n", __func__);
		ret = -ENOMEM;
		goto ERROR;
	}

	cdev_init(&info->cdev, &mms_dev_fops);
	info->cdev.owner = THIS_MODULE;

	if (cdev_add(&info->cdev, info->mms_dev, 1)) {
		input_err(true, &client->dev, "%s [ERROR] cdev_add\n", __func__);
		ret = -EIO;
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return 0;
}

#endif

void minority_report_calculate_cmdata(struct mms_ts_info *info)
{
	info->item_cmdata = (info->test_diff_max / 100) > 0xF ? 0xF : (info->test_diff_max / 100);
}

void minority_report_sync_latest_value(struct mms_ts_info *info)
{
	u32 temp = 0;

	temp |= (info->item_cmdata & 0xF);

	info->defect_probability = temp;

	input_info(true, &info->client->dev, "%s : defect_probability[%X]\n",
		__func__, info->defect_probability);
}

/*
* Process table data
*/
static int mms_proc_table_data(struct mms_ts_info *info, u8 data_type_size,
					u8 data_type_sign, u8 buf_addr_h, u8 buf_addr_l, u8 row_num,
					u8 col_num, u8 buf_col_num, u8 rotate)
{
	char data[16];
	int i_col, i_row, i_x, i_y;
	int max_x = 0;
	int max_y = 0;
	int sValue = 0;
	unsigned int uValue = 0;
	int value = 0;
	int size = 0;
	u8 wbuf[8];
	u8 rbuf[512];
	unsigned int buf_addr;
	int offset;
	int i, j, tmp;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	unsigned char *pStr = NULL;
	int lsize = (info->node_x + info->node_y) * CMD_RESULT_WORD_LEN;

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		goto error_alloc_mem;

	memset(data, 0, 16);
	memset(pStr, 0, lsize);
	memset(info->print_buf, 0, PAGE_SIZE);

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	/* set axis */
	if (rotate == 0) {
		max_x = col_num;
		max_y = row_num;
	} else if (rotate == 1) {
		max_x = row_num;
		max_y = col_num;
	} else {
		input_err(true,&info->client->dev, "%s [ERROR] rotate[%d]\n", __func__, rotate);
		goto error;
	}

	/* get table data */
	for (i_row = 0; i_row < row_num; i_row++) {
		/* get line data */
		offset = buf_col_num * data_type_size;
		size = col_num * data_type_size;
		buf_addr = (buf_addr_h << 8) | buf_addr_l | (offset * i_row);

		wbuf[0] = (buf_addr >> 8) & 0xFF;
		wbuf[1] = buf_addr & 0xFF;
		if (mms_i2c_read(info, wbuf, 2, rbuf, size)) {
			input_err(true,&info->client->dev, "%s [ERROR] Read data buffer\n", __func__);
			goto error;
		}

		/* save data */
		for (i_col = 0; i_col < col_num; i_col++) {
			if (data_type_sign == 0) {
				/* unsigned */
				switch (data_type_size) {
				case 1:
					uValue = (u8)rbuf[i_col];
					break;
				case 2:
					uValue = (u16)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8));
					break;
				case 4:
					uValue = (u32)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8) | (rbuf[data_type_size * i_col + 2] << 16) | (rbuf[data_type_size * i_col + 3] << 24));
					break;
				default:
					input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
					goto error;
				}
				value = (int)uValue;
			} else {
				/* signed */
				switch (data_type_size) {
				case 1:
					sValue = (s8)rbuf[i_col];
					break;
				case 2:
					sValue = (s16)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8));
					break;
				case 4:
					sValue = (s32)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8) | (rbuf[data_type_size * i_col + 2] << 16) | (rbuf[data_type_size * i_col + 3] << 24));
					break;
				default:
					input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
					goto error;
				}
				value = (int)sValue;
			}

			switch (rotate) {
			case 0:
				info->image_buf[i_row * col_num + i_col] = value;
				break;
			case 1:
				info->image_buf[i_col * row_num + (row_num - 1 - i_row)] = value;
				break;
			default:
				input_err(true,&info->client->dev, "%s [ERROR] rotate[%d]\n", __func__, rotate);
				goto error;
			}
		}
	}

	/* min, max */
	for (i = 0; i < (row_num * col_num); i++) {
		if (i == 0)
			info->test_min = info->test_max = info->image_buf[i];

		info->test_min = min(info->test_min, info->image_buf[i]);
		info->test_max = max(info->test_max, info->image_buf[i]);

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", info->image_buf[i]);
		strlcat(info->print_buf, temp, PAGE_SIZE);
		memset(temp, 0x00, SEC_CMD_STR_LEN);		
	}

	/* max_diff without border*/
	for (i = 1; i < row_num - 1; i++) {
		for (j = 1; j < col_num - 2; j++) {
			if (i == 1 && j == 1) {
				info->test_diff_max = 0;
			}
			tmp = info->image_buf[i * col_num + j] - info->image_buf[i * col_num + j + 1];
			info->test_diff_max = max(info->test_diff_max, abs(tmp));
			if (i < row_num - 2) {
				tmp = info->image_buf[i * col_num + j] - info->image_buf[(i + 1) * col_num + j];
				info->test_diff_max = max(info->test_diff_max, abs(tmp));
			}
		}
	}

	/* print table header */
	snprintf(data, sizeof(data), "    ");
	strlcat(pStr, data, lsize);
	memset(data, 0, 16);

	switch (data_type_size) {
	case 1:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%2d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	case 2:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%4d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	case 4:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%5d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	default:
		input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
		goto error;
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	memset(data, 0, 16);
	memset(pStr, 0, lsize);

	/* print table */
	for (i_y = 0; i_y < max_y; i_y++) {
		/* print line header */
		snprintf(data, sizeof(data), "[%2d]", i_y);
		strlcat(pStr, data, lsize);
		memset(data, 0, 16);

		/* print line */
		for (i_x = 0; i_x < max_x; i_x++) {
			switch (data_type_size) {
			case 1:
				snprintf(data, sizeof(data), " %3d", info->image_buf[i_y * max_x + i_x]);
				break;
			case 2:
				snprintf(data, sizeof(data), " %5d", info->image_buf[i_y * max_x + i_x]);
				break;
			case 4:
				snprintf(data, sizeof(data), " %6d", info->image_buf[i_y * max_x + i_x]);
				break;
			default:
				input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}

			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}

		input_raw_info(true, &info->client->dev, "%s\n", pStr);

		memset(data, 0, 16);
		memset(pStr, 0, lsize);

	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	kfree(pStr);

	return 0;

error:
	input_err(true,&info->client->dev, "%s [ERROR]\n", __func__);
	kfree(pStr);
error_alloc_mem:	
	return 1;
}

/*
* Process vector data
*/
static int mms_proc_vector_data(struct mms_ts_info *info, u8 data_type_size,
		u8 data_type_sign, u8 buf_addr_h, u8 buf_addr_l, u8 key_num, u8 vector_num, u16 *vector_id,
		u16 *vector_elem_num, int table_size)
{
	char data[16];
	int i, i_line, i_vector, i_elem;
	int sValue = 0;
	unsigned int uValue = 0;
	int value = 0;
	int size = 0;
	u8 wbuf[8];
	u8 rbuf[512];
	unsigned int buf_addr;
	int key_exist = 0;
	int total_len = 0;
	int elem_len = 0;
	int vector_total_len = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	unsigned char *pStr = NULL;
	int lsize = (info->node_x + info->node_y) * CMD_RESULT_WORD_LEN;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		goto error_alloc_mem;

	memset(data, 0, 16);
	memset(pStr, 0, lsize);
	memset(info->print_buf, 0, PAGE_SIZE);

	for (i = 0; i < vector_num; i++) {
		vector_total_len += vector_elem_num[i];
		input_dbg(true, &info->client->dev, "%s - vector_elem_num(%d)[%d]\n", __func__, i, vector_elem_num[i]);		
	}
	
	total_len = key_num + vector_total_len;
	input_dbg(true, &info->client->dev, "%s - key_num[%d] total_len[%d]\n", __func__, key_num, total_len);

	if (key_num > 0)
		key_exist = 1;
	else
		key_exist = 0;

	/* get line data */
	size = (key_num + vector_total_len) * data_type_size;
	buf_addr = (buf_addr_h << 8) | buf_addr_l;

	wbuf[0] = (buf_addr >> 8) & 0xFF;
	wbuf[1] = buf_addr & 0xFF;
	if (mms_i2c_read(info, wbuf, 2, rbuf, size)) {
		input_err(true,&info->client->dev, "%s [ERROR] Read data buffer\n", __func__);
		goto error;
	}

	/* save data */
	for (i = 0; i < total_len; i++) {
		if (data_type_sign == 0) {
			/* unsigned */
			switch (data_type_size) {
			case 1:
				uValue = (u8)rbuf[i];
				break;
			case 2:
				uValue = (u16)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8));
				break;
			case 4:
				uValue = (u32)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8) | (rbuf[data_type_size * i + 2] << 16) | (rbuf[data_type_size * i + 3] << 24));
				break;
			default:
				input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}
			value = (int)uValue;
		} else {
			/* signed */
			switch (data_type_size) {
			case 1:
				sValue = (s8)rbuf[i];
				break;
			case 2:
				sValue = (s16)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8));
				break;
			case 4:
				sValue = (s32)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8) | (rbuf[data_type_size * i + 2] << 16) | (rbuf[data_type_size * i + 3] << 24));
				break;
			default:
				input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}
			value = (int)sValue;
		}

		info->image_buf[table_size + i] = value;

		/* min, max */
		if (table_size + i == 0)
			info->test_min = info->test_max = info->image_buf[table_size + i];

		info->test_min = min(info->test_min, info->image_buf[table_size + i]);
		info->test_max = max(info->test_max, info->image_buf[table_size + i]);

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", info->image_buf[i]);
		strlcat(info->print_buf, temp, PAGE_SIZE);
		memset(temp, 0x00, SEC_CMD_STR_LEN);	
	}

	/* print header */
	i_vector = 0;
	i_elem = 0;
	for (i_line = 0; i_line < (key_exist + vector_num); i_line++) {
		if ((i_line == 0) && (key_exist == 1)) {
			elem_len = key_num;
			input_raw_info(true, &info->client->dev, "[Key]\n");
		} else {
			elem_len = vector_elem_num[i_vector];

			if (elem_len <= 0) {
				i_vector++;
				continue;
			}
			switch (vector_id[i_vector]) {
			case VECTOR_ID_SCREEN_RX:
				input_raw_info(true, &info->client->dev, "[Screen Rx]\n");
				break;
			case VECTOR_ID_SCREEN_TX:
				input_raw_info(true, &info->client->dev, "[Screen Tx]\n");
				break;
			case VECTOR_ID_KEY_RX:
				input_raw_info(true, &info->client->dev, "[Key Rx]\n");
				break;
			case VECTOR_ID_KEY_TX:
				input_raw_info(true, &info->client->dev, "[Key Tx]\n");
				break;
			case VECTOR_ID_PRESSURE:
				input_raw_info(true, &info->client->dev, "[Pressure]\n");
				break;
			case VECTOR_ID_OPEN_RESULT:
				input_raw_info(true, &info->client->dev, "[Open Result]\n");
				break;
			case VECTOR_ID_OPEN_RX:
				input_raw_info(true, &info->client->dev, "[Open Rx]\n");
				break;
			case VECTOR_ID_OPEN_TX:
				input_raw_info(true, &info->client->dev, "[Open Tx\n");
				break;
			case VECTOR_ID_SHORT_RESULT:
				input_raw_info(true, &info->client->dev, "[Short Result]\n");
				break;
			case VECTOR_ID_SHORT_RX:
				input_raw_info(true, &info->client->dev, "[Short Rx]\n");
				break;
			case VECTOR_ID_SHORT_TX:
				input_raw_info(true, &info->client->dev, "[Short Tx]\n");
				break;
			default:
				input_raw_info(true, &info->client->dev, "[%d]\n", i_vector);
				break;
			}
			i_vector++;
		}

		memset(data, 0, 16);
		memset(pStr, 0, lsize);

		/* print line */
		for (i = i_elem; i < (i_elem + elem_len); i++) {
			switch (data_type_size) {
			case 1:
				snprintf(data, sizeof(data), " %3d", info->image_buf[table_size + i]);
				break;
			case 2:
				snprintf(data, sizeof(data), " %5d", info->image_buf[table_size + i]);
				break;
			case 4:
				snprintf(data, sizeof(data), " %6d", info->image_buf[table_size + i]);
				break;
			default:
				input_err(true,&info->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}

			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}

		input_raw_info(true, &info->client->dev, "%s\n", pStr);

		memset(data, 0, 16);
		memset(pStr, 0x0, lsize);

		i_elem += elem_len;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	kfree(pStr);
	return 0;

error:
	input_err(true,&info->client->dev, "%s [ERROR]\n", __func__);
	kfree(pStr);
error_alloc_mem:		
	return 1;
}

/**
 * Run test
 */
int mms_run_test(struct mms_ts_info *info, u8 test_type)
{
	int busy_cnt = 100;
	int wait_cnt = 0;
	int wait_num = 200;
	u8 wbuf[8];
	u8 rbuf[512];
	char data[16];
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 vector_num = 0;
	u16 vector_id[16];
	u16 vector_elem_num[16];
	u8 buf_addr_h;
	u8 buf_addr_l;
	u16 buf_addr;
	int table_size;
	int i;
	int ret = 0;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);
	input_dbg(true, &info->client->dev, "%s - test_type[%d]\n", __func__, test_type);

	if (info->ic_status == PWR_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		return 1;
	}

	while (busy_cnt--) {
		if (info->test_busy == false)
			break;

		msleep(10);
	}
	mutex_lock(&info->lock);
	info->test_busy = true;
	mutex_unlock(&info->lock);

	memset(info->print_buf, 0, PAGE_SIZE);

	disable_irq(info->client->irq);
	mms_clear_input(info);

	/* disable touch event */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_TRIGGER_TYPE_NONE;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Disable event\n", __func__);
		goto ERROR;
	}

	/* check test type */
	switch (test_type) {
	case MIP_TEST_TYPE_CM:
		input_raw_info(true, &info->client->dev, "==== CM Test ===\n");
		break;
	case MIP_TEST_TYPE_CM_ABS:
		input_raw_info(true, &info->client->dev, "==== CM ABS Test ===\n");
		break;
	case MIP_TEST_TYPE_CM_JITTER:
		input_raw_info(true, &info->client->dev, "==== CM JITTER Test ===\n");
		break;
	case MIP_TEST_TYPE_SHORT:
		input_raw_info(true, &info->client->dev, "==== SHORT Test ===\n");
		break;
	case MIP_TEST_TYPE_GPIO_LOW:
		input_raw_info(true, &info->client->dev, "==== GPIO LOW Test ===\n");
		break;
	case MIP_TEST_TYPE_GPIO_HIGH:
		input_raw_info(true, &info->client->dev, "==== GPIO HIGH Test ===\n");
		break;
	case MIP_TEST_TYPE_CM_DIFF_HOR:
		input_raw_info(true, &info->client->dev, "==== CM DIFF HOR Test ===\n");
		break;
	case MIP_TEST_TYPE_CM_DIFF_VER:
		input_raw_info(true, &info->client->dev, "==== CM DIFF VER Test ===\n");
		break;
	case MIP_TEST_TYPE_CP:
		input_raw_info(true, &info->client->dev, "==== CP Test ===\n");
		break;
	case MIP_TEST_TYPE_CP_SHORT:
		input_raw_info(true, &info->client->dev, "==== CP SHORT Test ===\n");
		break;		
	case MIP_TEST_TYPE_CP_LPM:
		input_raw_info(true, &info->client->dev, "==== CP LPM Test ===\n");
		break;
	case MIP_TEST_TYPE_PANEL_CONN:
		input_raw_info(true, &info->client->dev, "==== CONNECTION Test ===\n");
		break;
	case MIP_TEST_TYPE_OPEN_SHORT:
		input_raw_info(true, &info->client->dev, "==== OPEN SHORT Test ===\n");
		break;
	case MIP_TEST_TYPE_VSYNC:
		input_raw_info(true, &info->client->dev, "==== V-SYNC Test ===\n");
		break;
	default:
		input_raw_info(true, &info->client->dev, "%s [ERROR] Unknown test type\n", __func__);
		sprintf(info->print_buf, "ERROR : Unknown test type");
		goto ERROR;
	}

	/* set test mode */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_MODE;
	wbuf[2] = MIP_CTRL_MODE_TEST_CM;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Write test mode\n", __func__);
		goto ERROR;
	}

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (mms_get_ready_status(info) == MIP_CTRL_STATUS_READY)
			break;

		msleep(10);

		input_dbg(false, &info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &info->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - set control mode retry[%d]\n", __func__, wait_cnt);

	/* set test type */
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_TYPE;
	wbuf[2] = test_type;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Write test type\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - set test type\n", __func__);

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (mms_get_ready_status(info) == MIP_CTRL_STATUS_READY)
			break;

		msleep(10);

		input_dbg(false, &info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &info->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - ready retry[%d]\n", __func__, wait_cnt);

	
	/*data format */
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_DATA_FORMAT;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 7)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read data format\n", __func__);
		goto ERROR;
	}

	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	vector_num = rbuf[6];

	input_dbg(true, &info->client->dev, "%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n", __func__, row_num, col_num, buffer_col_num, rotate, key_num);
	input_dbg(true, &info->client->dev, "%s - data_type[0x%02X] data_type_sign[%d] data_type_size[%d]\n", __func__, data_type, data_type_sign, data_type_size);
	input_dbg(true, &info->client->dev, "%s - vector_num[%d]\n", __func__, vector_num);

	if (vector_num > 0) {
		wbuf[0] = MIP_R0_TEST;
		wbuf[1] = MIP_R1_TEST_VECTOR_INFO;
		if (mms_i2c_read(info, wbuf, 2, rbuf, (vector_num * 4))) {
			input_err(true, &info->client->dev, "%s [ERROR] Read vector info\n", __func__);
			goto ERROR;
		}
		for (i = 0; i < vector_num; i++) {
			vector_id[i] = rbuf[i * 4 + 0] | (rbuf[i * 4 + 1] << 8);
			vector_elem_num[i] = rbuf[i * 4 + 2] | (rbuf[i * 4 + 3] << 8);
			input_info(true, &info->client->dev, "%s - vector[%d] : id[%d] elem_num[%d]\n", __func__, i, vector_id[i], vector_elem_num[i]);
		}
	}

	/* get buf addr */
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_BUF_ADDR;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 2)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read buf addr\n", __func__);
		goto ERROR;
	}

	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	buf_addr = (buf_addr_h << 8) | buf_addr_l;
	input_dbg(true, &info->client->dev, "%s - buf_addr[0x%02X 0x%02X] [0x%04X]\n",
		__func__, buf_addr_h, buf_addr_l, buf_addr);

	/* print data */
	table_size = row_num * col_num;
	if (table_size > 0) {
		if (mms_proc_table_data(info, data_type_size, data_type_sign, buf_addr_h, buf_addr_l,
							row_num, col_num, buffer_col_num, rotate)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_proc_table_data\n", __func__);
			goto ERROR;
		}
	}

	if (test_type == MIP_TEST_TYPE_CM) {
		input_info(true, &info->client->dev, "%s : CM TEST : max_diff[%d]\n",
					__func__, info->test_diff_max);
		minority_report_calculate_cmdata(info);
	}

	if ((key_num > 0) || (vector_num > 0)) {
		if (table_size > 0)
			buf_addr += (row_num * buffer_col_num * data_type_size);

		buf_addr_l = buf_addr & 0xFF;
		buf_addr_h = (buf_addr >> 8) & 0xFF;
		input_dbg(true, &info->client->dev, "%s - vector buf_addr[0x%02X 0x%02X][0x%04X]\n", __func__, buf_addr_h, buf_addr_l, buf_addr);

		if (mms_proc_vector_data(info, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, key_num, vector_num, vector_id, vector_elem_num, table_size)) {
			input_err(true,&info->client->dev, "%s [ERROR] mip4_ts_proc_vector_data\n", __func__);
			goto ERROR;
		}
	}

	/* open short test return */
	if (test_type == MIP_TEST_TYPE_OPEN_SHORT) {
		int i_vector = 0;
		int i_elem = 0;
		int i_line, elem_len, temp;
		info->open_short_result = 0;

		memset(info->print_buf, 0, PAGE_SIZE);

		for (i_line = 0; i_line < vector_num; i_line++) {
			elem_len = vector_elem_num[i_vector];
			temp = elem_len;

			if (elem_len <= 0) {
				i_vector++;
				continue;
			}

			memset(data, 0, 16);

			if (info->open_short_type == CHECK_ONLY_OPEN_TEST) {
				if (vector_id[i_vector] == VECTOR_ID_OPEN_RESULT) {
					if (info->image_buf[table_size + i_elem] == 1) {
						snprintf(data, sizeof(data), "OK");
						strlcat(info->print_buf, data, PAGE_SIZE);
						memset(data, 0, 16);
						info->open_short_result = 1;
						break;
					} else {
						snprintf(data, sizeof(data), "NG,OPEN:");
						temp = -1;
					}
				} else if (vector_id[i_vector] == VECTOR_ID_OPEN_RX) {
					snprintf(data, sizeof(data), "RX:");
				} else if (vector_id[i_vector] == VECTOR_ID_OPEN_TX) {
					snprintf(data, sizeof(data), "TX:");
				} else {
					temp = -1;
				}
			} else if (info->open_short_type == CHECK_ONLY_SHORT_TEST) {
				if (vector_id[i_vector] == VECTOR_ID_SHORT_RESULT) {
					if (info->image_buf[table_size + i_elem] == 1) {
						snprintf(data, sizeof(data), "OK");
						strlcat(info->print_buf, data, PAGE_SIZE);
						memset(data, 0, 16);
						info->open_short_result = 1;
						break;
					} else {
						snprintf(data, sizeof(data), "NG,SHORT:");
						temp = -1;
					}
				} else if (vector_id[i_vector] == VECTOR_ID_SHORT_RX) {
					snprintf(data, sizeof(data), "RX:");
				} else if (vector_id[i_vector] == VECTOR_ID_SHORT_TX) {
					snprintf(data, sizeof(data), "TX:");
				} else {
					temp = -1;
				}
			}

			strlcat(info->print_buf, data, PAGE_SIZE);
			memset(data, 0, 16);

			for (i = i_elem; i < (i_elem + temp); i++) {
				snprintf(data, sizeof(data), "%d,", info->image_buf[table_size + i]);
				strlcat(info->print_buf, data, PAGE_SIZE);
				memset(data, 0, 16);
			}
			i_vector++;
			i_elem += elem_len;				
		}
	} else if (test_type == MIP_TEST_TYPE_GPIO_LOW | test_type == MIP_TEST_TYPE_GPIO_HIGH) { 
		info->image_buf[0] = gpio_get_value(info->dtdata->gpio_intr);
		input_info(true, &info->client->dev, "%s gpio value %d\n", __func__, info->image_buf[0]);
	}

	/* set normal mode */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_MODE;
	wbuf[2] = MIP_CTRL_MODE_NORMAL;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto ERROR;
	}

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (mms_get_ready_status(info) == MIP_CTRL_STATUS_READY)
			break;

		msleep(10);

		input_dbg(false, &info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &info->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - set normal mode %d\n", __func__, wait_cnt);

	goto EXIT;

ERROR:
	ret = 1;
EXIT:
	//enable touch event
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_TRIGGER_TYPE_INTR;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}

	if (ret)
		mms_reboot(info);

	enable_irq(info->client->irq);

	mutex_lock(&info->lock);
	info->test_busy = false;
	mutex_unlock(&info->lock);

	if (ret)
		input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	else
		input_info(true, &info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}

/**
 * Read image data
 */
int mms_get_image(struct mms_ts_info *info, u8 image_type)
{
	int busy_cnt = 500;
	int wait_cnt = 200;
	u8 wbuf[8];
	u8 rbuf[512];
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 vector_num = 0;
	u16 vector_id[16];
	u16 vector_elem_num[16];
	u8 buf_addr_h;
	u8 buf_addr_l;
	u16 buf_addr = 0;
	int i;
	int table_size;
	int ret = 0;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);
	input_dbg(true, &info->client->dev, "%s - image_type[%d]\n", __func__, image_type);

	while (busy_cnt--) {
		if (info->test_busy == false)
			break;

		msleep(10);
	}

	memset(info->print_buf, 0, PAGE_SIZE);

	/* disable touch event */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_CTRL_TRIGGER_NONE;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Disable event\n", __func__);
		return 1;
	}

	mutex_lock(&info->lock);
	info->test_busy = true;
	disable_irq(info->irq);
	mutex_unlock(&info->lock);

	//check image type
	switch (image_type) {
	case MIP_IMG_TYPE_INTENSITY:
		input_info(true, &info->client->dev, "=== Intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_RAWDATA:
		input_info(true, &info->client->dev, "=== Rawdata Image ===\n");
		break;
	case MIP_IMG_TYPE_HSELF_RAWDATA:
		input_info(true, &info->client->dev, "=== self Rawdata Image ===\n");
		break;
	case MIP_IMG_TYPE_HSELF_INTENSITY:
		input_info(true, &info->client->dev, "=== self intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_PROX_INTENSITY:
		input_info(true, &info->client->dev, "=== PROX intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_5POINT_INTENSITY:
		input_info(true, &info->client->dev, "=== sensitivity Image ===\n");
		break;		
	default:
		input_err(true, &info->client->dev, "%s [ERROR] Unknown image type\n", __func__);
		goto ERROR;
	}

	//set image type
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = image_type;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Write image type\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - set image type\n", __func__);

	//wait ready status
	wait_cnt = 200;
	while (wait_cnt--) {
		if (mms_get_ready_status(info) == MIP_CTRL_STATUS_READY)
			break;

		msleep(10);

		input_dbg(true, &info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &info->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - ready\n", __func__);

	//data format
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_DATA_FORMAT;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 7)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read data format\n", __func__);
		goto ERROR;
	}

	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	vector_num = rbuf[6];

	input_dbg(true, &info->client->dev,
		"%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n",
		__func__, row_num, col_num, buffer_col_num, rotate, key_num);
	input_dbg(true, &info->client->dev,
		"%s - data_type[0x%02X] data_sign[%d] data_size[%d]\n",
		__func__, data_type, data_type_sign, data_type_size);

	if (vector_num > 0) {
		wbuf[0] = MIP_R0_IMAGE;
		wbuf[1] = MIP_R1_IMAGE_VECTOR_INFO;
		if (mms_i2c_read(info, wbuf, 2, rbuf, (vector_num * 4))) {
			input_err(true, &info->client->dev, "%s [ERROR] Read vector info\n", __func__);
			goto ERROR;
		}
		for (i = 0; i < vector_num; i++) {
			vector_id[i] = rbuf[i * 4 + 0] | (rbuf[i * 4 + 1] << 8);
			vector_elem_num[i] = rbuf[i * 4 + 2] | (rbuf[i * 4 + 3] << 8);
			input_dbg(true, &info->client->dev, "%s - vector[%d] : id[%d] elem_num[%d]\n", __func__, i, vector_id[i], vector_elem_num[i]);
		}
	}

	//get buf addr
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_BUF_ADDR;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 2)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read buf addr\n", __func__);
		goto ERROR;
	}

	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	input_dbg(true, &info->client->dev, "%s - buf_addr[0x%02X 0x%02X]\n",
		__func__, buf_addr_h, buf_addr_l);

	/* print data */
	table_size = row_num * col_num;
	if (table_size > 0) {
		if (mms_proc_table_data(info, data_type_size, data_type_sign, buf_addr_h, buf_addr_l,
							row_num, col_num, buffer_col_num, rotate)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_proc_table_data\n", __func__);
			goto ERROR;
		}
	}

	if ((key_num > 0) || (vector_num > 0)) {
		if (table_size > 0)
			buf_addr += (row_num * buffer_col_num * data_type_size);

		buf_addr_l = buf_addr & 0xFF;
		buf_addr_h = (buf_addr >> 8) & 0xFF;
		input_dbg(true, &info->client->dev, "%s - vector buf_addr[0x%02X 0x%02X][0x%04X]\n", __func__, buf_addr_h, buf_addr_l, buf_addr);

		if (mms_proc_vector_data(info, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, key_num, vector_num, vector_id, vector_elem_num, table_size)) {
			input_err(true,&info->client->dev, "%s [ERROR] mip4_ts_proc_vector_data\n", __func__);
			goto ERROR;
		}
	}
	goto EXIT;

ERROR:
	ret = 1;
EXIT:
	/* clear image type */
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = MIP_IMG_TYPE_NONE;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Clear image type\n", __func__);
		ret = 1;
	}

	/* enable touch event */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_CTRL_TRIGGER_INTR;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}

	if (ret)
		mms_reboot(info);

	//exit
	mutex_lock(&info->lock);
	info->test_busy = false;
	enable_irq(info->irq);
	mutex_unlock(&info->lock);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#if MMS_USE_TEST_MODE
/**
 * Print chip firmware version
 */
static ssize_t mms_sys_fw_version(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[16];

	memset(info->print_buf, 0, PAGE_SIZE);

	if (mms_get_fw_version(info, rbuf)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_get_fw_version\n", __func__);

		sprintf(data, "F/W Version : ERROR\n");
		goto ERROR;
	}

	input_info(true, &info->client->dev,
		"%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		__func__, rbuf[0], rbuf[1], rbuf[2], rbuf[3],
		rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
	sprintf(data, "F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

ERROR:
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Print channel info
 */
static ssize_t mms_sys_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[32];
	u8 wbuf[8];
	int res_x, res_y;

	memset(info->print_buf, 0, PAGE_SIZE);

	sprintf(data, "\n");
	strcat(info->print_buf, data);

	mms_get_fw_version(info, rbuf);
	sprintf(data, "F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
	strcat(info->print_buf, data);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mms_i2c_read(info, wbuf, 2, rbuf, 16);
	sprintf(data, "Product Name : %s\n", rbuf);
	strcat(info->print_buf, data);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mms_i2c_read(info, wbuf, 2, rbuf, 7);
	res_x = (rbuf[0]) | (rbuf[1] << 8);
	res_y = (rbuf[2]) | (rbuf[3] << 8);
	sprintf(data, "Resolution : X[%d] Y[%d]\n", res_x, res_y);
	strcat(info->print_buf, data);

	sprintf(data, "Node Num : X[%d] Y[%d] Key[%d]\n", rbuf[4], rbuf[5], rbuf[6]);
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;

}

/**
 * Device enable
 */
static ssize_t mms_sys_device_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	mms_enable(info);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Device : Enabled\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;

}

/**
 * Device disable
 */
static ssize_t mms_sys_device_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	mms_disable(info);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Device : Disabled\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Enable IRQ
 */
static ssize_t mms_sys_irq_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	enable_irq(info->irq);

	input_info(true, &client->dev, "%s\n", __func__);

	sprintf(data, "IRQ : Enabled\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Disable IRQ
 */
static ssize_t mms_sys_irq_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	disable_irq(info->irq);
	mms_clear_input(info);

	input_info(true, &client->dev, "%s\n", __func__);

	sprintf(data, "IRQ : Disabled\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Power on
 */
static ssize_t mms_sys_power_on(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	mms_power_control(info, 1);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Power : On\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Power off
 */
static ssize_t mms_sys_power_off(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	mms_power_control(info, 0);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Power : Off\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Reboot chip
 */
static ssize_t mms_sys_reboot(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	input_info(true, &client->dev, "%s", __func__);

	disable_irq(info->irq);
	mms_clear_input(info);
	mms_reboot(info);
	enable_irq(info->irq);

	sprintf(data, "Reboot\n");
	strcat(info->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;

}

/**
 * Set glove mode
 */
static ssize_t mms_sys_glove_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[8];

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GLOVE_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		if (mms_i2c_write(info, wbuf, 3))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get glove mode
 */
static ssize_t mms_sys_glove_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(info->print_buf, 0, PAGE_SIZE);

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GLOVE_MODE;

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		sprintf(data, "\nGlove Mode : ERROR\n");
	} else {
		input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nGlove Mode : %d\n", rbuf[0]);
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Set charger mode
 */
static ssize_t mms_sys_charger_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[8];

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		if (mms_i2c_write(info, wbuf, 3))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get charger mode
 */
static ssize_t mms_sys_charger_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(info->print_buf, 0, PAGE_SIZE);

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		sprintf(data, "\nCharger Mode : ERROR\n");
	} else {
		input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nCharger Mode : %d\n", rbuf[0]);
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Set cover window mode
 */
static ssize_t mms_sys_window_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[8];

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		if (mms_i2c_write(info, wbuf, 3))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get cover window mode
 */
static ssize_t mms_sys_window_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(info->print_buf, 0, PAGE_SIZE);

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		sprintf(data, "\nWindow Mode : ERROR\n");
	} else {
		input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nWindow Mode : %d\n", rbuf[0]);
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Set palm rejection mode
 */
static ssize_t mms_sys_palm_rejection_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[8];

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_PALM_REJECTION;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		if (mms_i2c_write(info, wbuf, 3))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get palm rejection mode
 */
static ssize_t mms_sys_palm_rejection_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(info->print_buf, 0, PAGE_SIZE);

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_PALM_REJECTION;

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		sprintf(data, "\nPalm Rejection Mode : ERROR\n");
	} else {
		input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nPalm Rejection Mode : %d\n", rbuf[0]);
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs print intensity image
 */
static ssize_t mms_sys_intensity(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_get_image\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs print rawdata image
 */
static ssize_t mms_sys_rawdata(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_get_image(info, MIP_IMG_TYPE_RAWDATA)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_get_image\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs run cm delta test
 */
static ssize_t mms_sys_test_cm_delta(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_run_test(info, MIP_TEST_TYPE_CM)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_run_test\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs run cm abs test
 */
static ssize_t mms_sys_test_cm_abs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_run_test(info, MIP_TEST_TYPE_CM_ABS)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_run_test\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs run cm jitter test
 */
static ssize_t mms_sys_test_cm_jitter(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_run_test(info, MIP_TEST_TYPE_CM_JITTER)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_run_test\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
 * Sysfs run short test
 */
static ssize_t mms_sys_test_short(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (mms_run_test(info, MIP_TEST_TYPE_SHORT)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_run_test\n", __func__);
		return -1;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

static ssize_t mip4_ts_sys_proximity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = 0x23;

	if (!strcmp(attr->attr.name, "mode_proximity_on")) {
		wbuf[2] = 1;
	} else if (!strcmp(attr->attr.name, "mode_proximity_off")) {
		wbuf[2] = 0;
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] Unknown mode[%s]\n", __func__, attr->attr.name);
		snprintf(data, sizeof(data), "%s : Unknown Mode\n", attr->attr.name);
		goto exit;
	}

	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		snprintf(data, sizeof(data), "%s : ERROR\n", attr->attr.name);
	} else {
		input_info(true, &info->client->dev, "%s - addr[0x%02X%02X] value[%d]\n", __func__, wbuf[0], wbuf[1], wbuf[2]);
		snprintf(data, sizeof(data), "%s : %d\n", attr->attr.name, wbuf[2]);
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

exit:
	ret = snprintf(buf, 255, "%s\n", data);
	return ret;
}

/**
 * Sysfs functions
 */
static DEVICE_ATTR(fw_version, S_IWUSR | S_IWGRP, mms_sys_fw_version, NULL);
static DEVICE_ATTR(info, S_IWUSR | S_IWGRP, mms_sys_info, NULL);
static DEVICE_ATTR(device_enable, S_IWUSR | S_IWGRP, mms_sys_device_enable, NULL);
static DEVICE_ATTR(device_disable, S_IWUSR | S_IWGRP, mms_sys_device_disable, NULL);
static DEVICE_ATTR(irq_enable, S_IWUSR | S_IWGRP, mms_sys_irq_enable, NULL);
static DEVICE_ATTR(irq_disable, S_IWUSR | S_IWGRP, mms_sys_irq_disable, NULL);
static DEVICE_ATTR(power_on, S_IWUSR | S_IWGRP, mms_sys_power_on, NULL);
static DEVICE_ATTR(power_off, S_IWUSR | S_IWGRP, mms_sys_power_off, NULL);
static DEVICE_ATTR(reboot, S_IWUSR | S_IWGRP, mms_sys_reboot, NULL);
static DEVICE_ATTR(mode_glove, S_IWUSR | S_IWGRP, mms_sys_glove_mode_show, mms_sys_glove_mode_store);
static DEVICE_ATTR(mode_charger, S_IWUSR | S_IWGRP,
			mms_sys_charger_mode_show, mms_sys_charger_mode_store);
static DEVICE_ATTR(mode_cover_window, S_IWUSR | S_IWGRP,
			mms_sys_window_mode_show, mms_sys_window_mode_store);
static DEVICE_ATTR(mode_palm_rejection, S_IWUSR | S_IWGRP,
			mms_sys_palm_rejection_mode_show, mms_sys_palm_rejection_mode_store);
static DEVICE_ATTR(image_intensity, S_IWUSR | S_IWGRP, mms_sys_intensity, NULL);
static DEVICE_ATTR(image_rawdata, S_IWUSR | S_IWGRP, mms_sys_rawdata, NULL);
static DEVICE_ATTR(test_cm_delta, S_IWUSR | S_IWGRP, mms_sys_test_cm_delta, NULL);
static DEVICE_ATTR(test_cm_abs, S_IWUSR | S_IWGRP, mms_sys_test_cm_abs, NULL);
static DEVICE_ATTR(test_cm_jitter, S_IWUSR | S_IWGRP, mms_sys_test_cm_jitter, NULL);
static DEVICE_ATTR(test_short, S_IWUSR | S_IWGRP, mms_sys_test_short, NULL);
static DEVICE_ATTR(mode_proximity_on, S_IRUGO, mip4_ts_sys_proximity, NULL);
static DEVICE_ATTR(mode_proximity_off, S_IRUGO, mip4_ts_sys_proximity, NULL);

/**
 * Sysfs attr list info
 */
static struct attribute *mms_test_attr[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_info.attr,
	&dev_attr_device_enable.attr,
	&dev_attr_device_disable.attr,
	&dev_attr_irq_enable.attr,
	&dev_attr_irq_disable.attr,
	&dev_attr_power_on.attr,
	&dev_attr_power_off.attr,
	&dev_attr_reboot.attr,
	&dev_attr_mode_glove.attr,
	&dev_attr_mode_charger.attr,
	&dev_attr_mode_cover_window.attr,
	&dev_attr_mode_palm_rejection.attr,
	&dev_attr_image_intensity.attr,
	&dev_attr_image_rawdata.attr,
	&dev_attr_test_cm_delta.attr,
	&dev_attr_test_cm_abs.attr,
	&dev_attr_test_cm_jitter.attr,
	&dev_attr_test_short.attr,
	&dev_attr_mode_proximity_off.attr,
	&dev_attr_mode_proximity_on.attr,
	NULL,
};

/**
 * Sysfs attr group info
 */
static const struct attribute_group mms_test_attr_group = {
	.attrs = mms_test_attr,
};

/**
 * Create sysfs test functions
 */
int mms_sysfs_create(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (sysfs_create_group(&client->dev.kobj, &mms_test_attr_group)) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

#if !(MMS_USE_CMD_MODE)
	info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);
	info->image_buf =
		kzalloc(sizeof(int) * ((info->node_x * info->node_y) + info->node_key),
			GFP_KERNEL);
#endif
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return 0;
}

/**
 * Remove sysfs test functions
 */
void mms_sysfs_remove(struct mms_ts_info *info)
{
	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	sysfs_remove_group(&info->client->dev.kobj, &mms_test_attr_group);

#if !(MMS_USE_CMD_MODE)
	kfree(info->print_buf);
	kfree(info->image_buf);
#endif
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
}

#endif
