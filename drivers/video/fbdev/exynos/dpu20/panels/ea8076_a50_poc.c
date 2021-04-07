#include <video/mipi_display.h>
#include "ea8076_a50_poc.h"
#include "dpui.h"
#include "../dsim.h"
#include "../decon.h"
#include "../decon_notify.h"
#include "../decon_board.h"

/* ONLY SUPPORT ERASE AND WRITE */

static u8 poc_wr_img[POC_IMG_SIZE] = { 0, };

const char * const poc_op_string[] = {
	[POC_OP_NONE] = "POC_OP_NONE",
	[POC_OP_ERASE] = "POC_OP_ERASE",
	[POC_OP_WRITE] = "POC_OP_WRITE",
	[POC_OP_CANCEL] = "POC_OP_CANCEL",
};

#define DSI_WRITE(cmd, size)		do {				\
	ret = dsim_write_hl_data(poc_dev, cmd, size);			\
	if (ret < 0) {							\
		dsim_err("%s: failed to write %s\n", __func__, #cmd);	\
		goto tx_fail;	\
	}	\
} while (0)

static int dsim_write_hl_data(struct panel_poc_device *poc_dev, const u8 *cmd, u32 cmdSize)
{
	struct panel_private *priv = &poc_dev->dsim->priv;
	int ret = 0;
	int retry = 2;

	if (!priv->lcdconnected)
		return ret;

try_write:
	if (cmdSize == 1)
		ret = dsim_write_data(poc_dev->dsim, MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (cmdSize == 2)
		ret = dsim_write_data(poc_dev->dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = dsim_write_data(poc_dev->dsim, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, cmdSize);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dsim_err("%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

static int do_poc_cmd_table(struct panel_poc_device *poc_dev, const u8 *table, u32 table_size)
{
	int wtite_cnt = 0;
	int pos = 0;
	u8 erase_cmd_buf[3] = {0, };

	for ( pos = 0 ; pos < table_size; )
	{
		wtite_cnt++;

		if (table[pos] == 0xB0) {
			erase_cmd_buf[0] = table[pos];
			erase_cmd_buf[1] = table[pos+1];
			dsim_write_hl_data(poc_dev, erase_cmd_buf, 2);
			pos = pos + 2;
		} else if (table[pos] == 0xE1) {
			erase_cmd_buf[0] = table[pos];
			erase_cmd_buf[1] = table[pos+1];
			erase_cmd_buf[2] = table[pos+2];
			dsim_write_hl_data(poc_dev, erase_cmd_buf, 3);
			pos = pos + 3;
		} else {
			dsim_err("%s: abnormal cmd[0x%02x]\n", __func__, table[pos]);
			BUG();
		}
	}

	return 0;
}

static int do_poc_tx_cmd(struct panel_poc_device *poc_dev, u32 cmd)
{
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	int ret = 0;
	u8 wr_addr_arr[3] = {POC_WR_ADDR_REG, 0x00, 0x00};

	struct decon_device *decon = get_decon_drvdata(0);

	switch (cmd) {
	case POC_WRITE_ENTER_SEQ:
	case POC_ERASE_ENTER_SEQ:
		decon->partial_force_disable = 1;
		decon->ignore_vsync = true;
		kthread_flush_worker(&decon->up.worker);
		msleep(33);

		DSI_WRITE(KEY1_ENABLE, ARRAY_SIZE(KEY1_ENABLE));
		DSI_WRITE(KEY2_ENABLE, ARRAY_SIZE(KEY2_ENABLE));
		DSI_WRITE(KEY3_ENABLE, ARRAY_SIZE(KEY3_ENABLE));

		do_poc_cmd_table(poc_dev, FLASH_UNLOCK_TABLE, ARRAY_SIZE(FLASH_UNLOCK_TABLE));
		msleep(33);
		break;

	case POC_ERASE_ALL_SEQ:
		do_poc_cmd_table(poc_dev, ERASE_TABLE_00, ARRAY_SIZE(ERASE_TABLE_00));
		msleep(1200);
		do_poc_cmd_table(poc_dev, ERASE_TABLE_01, ARRAY_SIZE(ERASE_TABLE_01));
		msleep(1200);
		do_poc_cmd_table(poc_dev, ERASE_TABLE_02, ARRAY_SIZE(ERASE_TABLE_02));
		msleep(1200);
		do_poc_cmd_table(poc_dev, ERASE_TABLE_03, ARRAY_SIZE(ERASE_TABLE_03));
		msleep(1200);
		do_poc_cmd_table(poc_dev, ERASE_TABLE_04, ARRAY_SIZE(ERASE_TABLE_04));
		msleep(1200);
		break;

	case POC_ERASE_EXIT_SEQ:
	case POC_WRITE_EXIT_SEQ:
		do_poc_cmd_table(poc_dev, FLASH_LOCK_TABLE, ARRAY_SIZE(FLASH_LOCK_TABLE));

		msleep(33);
		DSI_WRITE(KEY1_DISABLE, ARRAY_SIZE(KEY1_DISABLE));
		DSI_WRITE(KEY2_DISABLE, ARRAY_SIZE(KEY2_DISABLE));
		DSI_WRITE(KEY3_DISABLE, ARRAY_SIZE(KEY3_DISABLE));

		decon->partial_force_disable = 0;
		decon->ignore_vsync = false;
		break;

	case POC_WRITE_DAT_SEQ:
		wr_addr_arr[1] = (poc_info->waddr & 0x00FF0) >> 4;
		wr_addr_arr[2] = (poc_info->waddr & 0xFF000) >> 12;
		DSI_WRITE(poc_info->wdata, ARRAY_SIZE(poc_info->wdata));

		wr_addr_arr[1]  = wr_addr_arr[1]  | 0x01;
		DSI_WRITE(WRITE_GP, ARRAY_SIZE(WRITE_GP));
		DSI_WRITE(wr_addr_arr, ARRAY_SIZE(wr_addr_arr));
		msleep(10);

		wr_addr_arr[1] = wr_addr_arr[1] & ~0x01;
		DSI_WRITE(WRITE_GP, ARRAY_SIZE(WRITE_GP));
		DSI_WRITE(wr_addr_arr, ARRAY_SIZE(wr_addr_arr));
		break;
	}

tx_fail:
	return ret;
}


int poc_erase(struct panel_poc_device *poc_dev, int addr, int len)
{
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	int ret, sz_block = 0, erased_size = 0, erase_seq_index;
	struct decon_device *decon = get_decon_drvdata(0);

	poc_info->erase_trycount++;

	if (addr % POC_PAGE > 0) {
		dsim_err("%s, failed to start erase. invalid addr\n", __func__);
		poc_info->erase_failcount++;
		return -EINVAL;
	}

	if (len < 0 || (addr + len > POC_TOTAL_SIZE)) {
		dsim_err("%s, failed to start erase. range exceeded\n", __func__);
		poc_info->erase_failcount++;
		return -EINVAL;
	}
	len = ALIGN(len, SZ_4K);

	dsim_info("%s poc erase +++, 0x%x, %d\n", __func__, addr, len);

	mutex_lock(poc_dev->lock);

	ret = do_poc_tx_cmd(poc_dev, POC_ERASE_ENTER_SEQ);
	if (unlikely(ret < 0)) {
		dsim_err("%s, failed to poc-erase-enter-seq\n", __func__);
		goto out_poc_erase;
	}

	erase_seq_index = POC_ERASE_ALL_SEQ;
	ret = do_poc_tx_cmd(poc_dev, erase_seq_index);

	if (unlikely(ret < 0)) {
		dsim_err("%s, failed to poc-erase-seq\n", __func__);
		goto out_poc_erase;
	}

	dsim_info("%s erased addr %06X, sz_block %06X\n",
			__func__, addr + erased_size, sz_block);


	ret = do_poc_tx_cmd(poc_dev, POC_ERASE_EXIT_SEQ);
	if (unlikely(ret < 0)) {
		dsim_err("%s, failed to poc-erase-exit-seq\n", __func__);
	}

	mutex_unlock(poc_dev->lock);

	dsim_info("%s poc erase ---\n", __func__);
	return 0;

out_poc_erase:
	decon->partial_force_disable = 0;
	poc_info->erase_failcount++;
	mutex_unlock(poc_dev->lock);
	return ret;
}

static int poc_write_data(struct panel_poc_device *poc_dev)
{
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	int ret = 0;
	struct decon_device *decon = get_decon_drvdata(0);
	int i, cnt = 0 ;
	int wr_len = 0;

	decon->partial_force_disable = 1;

	kthread_flush_worker(&decon->up.worker);
	msleep(33);

	cnt = poc_info->wsize / 256 + ((poc_info->wsize % 256) ? 1 : 0);
	poc_info->waddr = poc_info->wpos;

	for ( i = 0 ; i < cnt; i++) {
		poc_info->waddr = poc_info->wpos + ( i * 256);
		memset(poc_info->wdata, 0xFF, ARRAY_SIZE(poc_info->wdata));
		// [0] addr
		poc_info->wdata[0] = POC_WR_DATA_REG;

		//data length
		if  ( i == (cnt - 1) &&  (poc_info->wsize % 256)) {
			wr_len = (poc_info->wsize % 256);
		} else
			wr_len = 256;

		//[1] [256] data
		memcpy(&poc_info->wdata[1], &poc_info->wbuf[poc_info->wpos + ( i * 256)], wr_len );

		ret = do_poc_tx_cmd(poc_dev, POC_WRITE_DAT_SEQ);
		if (unlikely(ret < 0)) {
			dsim_err("%s, failed to read poc-wr-enter-seq\n", __func__);
			goto out_poc_write;
		}
	}

	decon->partial_force_disable = 0;

	return 0;

out_poc_write:
	mutex_unlock(poc_dev->lock);
	decon->partial_force_disable = 0;

	return ret;
}

static long panel_poc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return 0;
}

static int panel_poc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct panel_poc_device *poc_dev = container_of(dev, struct panel_poc_device, dev);
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	int ret = 0;

	dsim_info("%s was called\n", __func__);

	if (poc_dev->opened) {
		dsim_err("POC:ERR:%s: already opend\n", __func__);
		return -EBUSY;
	}

	if (!poc_dev->enable) {
		dsim_err("POC:WARN:%s:panel is not active %d\n", __func__, poc_dev->enable);
		return -EAGAIN;
	}

	poc_info->state = 0;
	poc_info->wbuf = poc_wr_img;
	poc_info->wpos = 0;
	poc_info->wsize = 0;

	file->private_data = poc_dev;
	poc_dev->opened = 1;
	poc_dev->read = 0;

	atomic_set(&poc_dev->cancel, 0);

	mutex_lock(poc_dev->lock);

	ret = do_poc_tx_cmd(poc_dev, POC_WRITE_ENTER_SEQ);
	if (unlikely(ret < 0)) {
		dsim_err("%s, failed to read poc-wr-enter-seq\n", __func__);
	}

	return 0;
}

static int panel_poc_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;

	dsim_info("%s was called\n", __func__);

	ret = do_poc_tx_cmd(poc_dev, POC_WRITE_EXIT_SEQ);
	if (unlikely(ret < 0)) {
		dsim_err("%s, failed to write poc-wr-exit seq\n", __func__);
	}

	mutex_unlock(poc_dev->lock);

	poc_info->state = 0;

	poc_info->wbuf = NULL;
	poc_info->wpos = 0;
	poc_info->wsize = 0;
	poc_dev->opened = 0;
	poc_dev->read = 0;

	atomic_set(&poc_dev->cancel, 0);

	dsim_info("%s Erase_try: %d, Erase_fail: %d, Write_try: %d, Write_fail: %d\n",
		__func__,
		poc_info->erase_trycount, poc_info->erase_failcount,
		poc_info->write_trycount,  poc_info->write_failcount);

	return ret;
}

static ssize_t panel_poc_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	/* not supported */

	return 0;
}

static ssize_t panel_poc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	ssize_t res;

	dsim_info("%s : size : %d, ppos %d\n", __func__, (int)count, (int)*ppos);
	poc_info->write_trycount++;

	if (unlikely(!poc_dev->opened)) {
		dsim_err("POC:ERR:%s: poc device not opened\n", __func__);
		poc_info->write_failcount++;
		return -EIO;
	}

	if (!poc_dev->enable) {
		dsim_err("POC:WARN:%s:panel is not active\n", __func__);
		poc_info->write_failcount++;
		return -EAGAIN;
	}

	if (unlikely(!buf)) {
		dsim_err("POC:ERR:%s: invalid write buffer\n", __func__);
		poc_info->write_failcount++;
		return -EINVAL;
	}

	if (unlikely(*ppos + count > POC_IMG_SIZE)) {
		dsim_err("POC:ERR:%s: invalid write size pos %d, size %d\n",
				__func__, (int)*ppos, (int)count);
		poc_info->write_failcount++;
		return -EINVAL;
	}

	poc_info->wbuf = poc_wr_img;
	poc_info->wpos = *ppos;
	poc_info->wsize = count;
	res = simple_write_to_buffer(poc_info->wbuf, POC_IMG_SIZE, ppos, buf, count);

	dsim_info("%s write %ld bytes (count %ld)\n", __func__, res, count);

	res = poc_write_data(poc_dev);
	if (res < 0) {
		poc_info->write_failcount++;
		goto err_write;
	}

	return count;

err_write:
	poc_info->write_failcount++;
	return res;
}

static const struct file_operations panel_poc_fops = {
	.owner = THIS_MODULE,
	.read = panel_poc_read,
	.write = panel_poc_write,
	.unlocked_ioctl = panel_poc_ioctl,
	.open = panel_poc_open,
	.release = panel_poc_release,
};


#ifdef CONFIG_DISPLAY_USE_INFO
static int poc_dpui_callback(struct panel_poc_device *poc_dev)
{
	struct panel_poc_info *poc_info;

	poc_info = &poc_dev->poc_info;

	inc_dpui_u32_field(DPUI_KEY_PNPOC_ER_TRY, poc_info->erase_trycount);
	poc_info->erase_trycount = 0;
	inc_dpui_u32_field(DPUI_KEY_PNPOC_ER_FAIL, poc_info->erase_failcount);
	poc_info->erase_failcount = 0;

	inc_dpui_u32_field(DPUI_KEY_PNPOC_WR_TRY, poc_info->write_trycount);
	poc_info->write_trycount = 0;
	inc_dpui_u32_field(DPUI_KEY_PNPOC_WR_FAIL, poc_info->write_failcount);
	poc_info->write_failcount = 0;

	inc_dpui_u32_field(DPUI_KEY_PNPOC_RD_TRY, poc_info->read_trycount);
	poc_info->read_trycount = 0;
	inc_dpui_u32_field(DPUI_KEY_PNPOC_RD_FAIL, poc_info->read_failcount);
	poc_info->read_failcount = 0;

	return 0;
}

static int poc_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct panel_poc_device *poc_dev;
	struct dpui_info *dpui = data;

	if (dpui == NULL) {
		dsim_err("%s: dpui is null\n", __func__);
		return 0;
	}

	poc_dev = container_of(self, struct panel_poc_device, poc_notif);
	poc_dpui_callback(poc_dev);

	return 0;
}
#endif /* CONFIG_DISPLAY_USE_INFO */

static int poc_fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct panel_poc_device *poc_dev;
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	poc_dev = container_of(self, struct panel_poc_device, fb_notif);

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (event == FB_EVENT_BLANK && fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(poc_dev->lock);
		poc_dev->enable = 1;
		mutex_unlock(poc_dev->lock);
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(poc_dev->lock);
		poc_dev->enable = 0;
		mutex_unlock(poc_dev->lock);
	}

	return NOTIFY_DONE;
}

static int poc_register_fb(struct panel_poc_device *poc_dev)
{
	poc_dev->fb_notif.notifier_call = poc_fb_notifier_callback;
	return decon_register_notifier(&poc_dev->fb_notif);
}

int panel_poc_probe(struct panel_poc_device *poc_dev)
{
	struct panel_poc_info *poc_info;
	int ret = 0;

	if (poc_dev == NULL) {
		dsim_err("POC:ERR:%s: invalid poc_dev\n", __func__);
		return -EINVAL;
	}

	poc_info = &poc_dev->poc_info;
	poc_dev->dev.minor = MISC_DYNAMIC_MINOR;
	poc_dev->dev.name = "poc";
	poc_dev->dev.fops = &panel_poc_fops;
	poc_dev->dev.parent = NULL;

	ret = misc_register(&poc_dev->dev);
	if (ret < 0) {
		dsim_err("PANEL:ERR:%s: failed to register panel misc driver (ret %d)\n",
				__func__, ret);
		goto exit_probe;
	}

	poc_info->erased = false;
	poc_info->poc = 1;	/* default enabled */

	poc_dev->opened = 0;
	poc_dev->enable = 1;

	poc_register_fb(poc_dev);

#ifdef CONFIG_DISPLAY_USE_INFO
	poc_info->total_trycount = -1;
	poc_info->total_failcount = -1;

	poc_dev->poc_notif.notifier_call = poc_notifier_callback;
	ret = dpui_logging_register(&poc_dev->poc_notif, DPUI_TYPE_PANEL);
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:failed to register dpui notifier callback\n", __func__);
		goto exit_probe;
	}
#endif

exit_probe:
	return ret;
}
