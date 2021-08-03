/*
 * Rcihtek SPM Class Driver
 *
 * Copyright (C) 2019, Richtek Technology Corp.
 * Author: CY Hunag <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <sound/sec_synchronized_ipc_richtek.h>
#include "richtek_spm_cls.h"

static struct class *richtek_spm_class;
static int trig_status;
static int cali_status;

enum {
	RICHTEK_SPM_DEV_RSPK = 0,
	RICHTEK_SPM_DEV_INDEX,
	RICHTEK_SPM_DEV_SPKIDX,
	RICHTEK_SPM_DEV_PARAMS,
	RICHTEK_SPM_DEV_PARAM_ITEMS,
	RICHTEK_SPM_DEV_IPC_GO,
	RICHTEK_SPM_DEV_TMAX,
	RICHTEK_SPM_DEV_TMAXCNT,
	RICHTEK_SPM_DEV_XMAX,
	RICHTEK_SPM_DEV_XMAXCNT,
	RICHTEK_SPM_DEV_TMAXKEEP,
	RICHTEK_SPM_DEV_XMAXKEEP,
	RICHTEK_SPM_DEV_VVALIDATION_REAL_POWER,
	RICHTEK_SPM_DEV_MAX,
};

enum {
	CMD_READ = 0,
	CMD_WRITE,
	CMD_MAX,
};

struct spm_param_item {
	u32 index;
	s32 param_items;
	s32 offset;
};

static const struct spm_param_item ampon_wparam_items[] = {
	{ 56, 1, offsetof(struct richtek_spm_classdev, rspk)},
	{ 61, 1, offsetof(struct richtek_spm_classdev, t0)},
	{ 67, 1, offsetof(struct richtek_spm_classdev, monitor_on)},
	{ 4004, 1, offsetof(struct richtek_spm_classdev, tmaxcnt)},
	{ 4006, 1, offsetof(struct richtek_spm_classdev, xmaxcnt)},
};

static const struct spm_param_item ampoff_rparam_items[] = {
	{ 4003, 1, offsetof(struct richtek_spm_classdev, tmax)},
	{ 4004, 1, offsetof(struct richtek_spm_classdev, tmaxcnt)},
	{ 4005, 1, offsetof(struct richtek_spm_classdev, xmax)},
	{ 4006, 1, offsetof(struct richtek_spm_classdev, xmaxcnt)},
};

static const struct spm_param_item calib_wswitch_items[] = {
	{ 4001, 1, offsetof(struct richtek_spm_classdev, calib_enable)},
};

static const struct spm_param_item vali_wswitch_items[] = {
	{ 4008, 1, offsetof(struct richtek_spm_classdev, vali_enable)},
};

static const struct spm_param_item calib_rstatus_items[] = {
	{ 4000, 1, offsetof(struct richtek_spm_classdev, calib_status)},
	{ 56, 1, offsetof(struct richtek_spm_classdev, rspk)},
};

static const struct spm_param_item vali_rstatus_items[] = {
	{ 4009, 1, offsetof(struct richtek_spm_classdev, vali_status)},
	{ 108, 1, offsetof(struct richtek_spm_classdev, vali_real_power)},
};

static ssize_t richtek_spm_dev_attr_show(struct device *,
					 struct device_attribute *, char *);
static ssize_t richtek_spm_dev_attr_store(struct device *,
					  struct device_attribute *,
					  const char *, size_t);
static struct device_attribute richtek_spm_dev_attrs[] = {
	__ATTR(rspk, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(index, 0644,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(spkidx, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(params, 0644,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(param_items, 0644,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(ipc_go, 0220,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(tmax, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(tmaxcnt, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(xmax, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(xmaxcnt, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(tmaxkeep, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(xmaxkeep, 0444,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(real_power, 0444,
		richtek_spm_dev_attr_show, NULL),
	__ATTR_NULL,
};

static struct attribute * richtek_spm_classdev_attrs[] = {
	&richtek_spm_dev_attrs[0].attr,
	&richtek_spm_dev_attrs[1].attr,
	&richtek_spm_dev_attrs[2].attr,
	&richtek_spm_dev_attrs[3].attr,
	&richtek_spm_dev_attrs[4].attr,
	&richtek_spm_dev_attrs[5].attr,
	&richtek_spm_dev_attrs[6].attr,
	&richtek_spm_dev_attrs[7].attr,
	&richtek_spm_dev_attrs[8].attr,
	&richtek_spm_dev_attrs[9].attr,
	&richtek_spm_dev_attrs[10].attr,
	&richtek_spm_dev_attrs[11].attr,
	&richtek_spm_dev_attrs[12].attr,
	NULL,
};

static const struct attribute_group richtek_spm_classdev_group = {
	.attrs = richtek_spm_classdev_attrs,
};

static const struct attribute_group *richtek_spm_classdev_groups[] = {
	&richtek_spm_classdev_group,
	NULL,
};

static int rt_spm_monitor_convert(int id, char *buf, int size, s32 val)
{
	int ret;

	if (!buf || size == 0)
		return -EINVAL;
	switch (id) {
	case RICHTEK_SPM_DEV_TMAX:
	/* Fall Through */
	case RICHTEK_SPM_DEV_TMAXKEEP:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", val / 1000);
		break;
	case RICHTEK_SPM_DEV_TMAXCNT:
	case RICHTEK_SPM_DEV_XMAX:
	case RICHTEK_SPM_DEV_XMAXKEEP:
	/* Fall Through */
	case RICHTEK_SPM_DEV_XMAXCNT:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t richtek_spm_dev_attr_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - richtek_spm_dev_attrs;
	const struct spm_param_item *pitem;
	int i, ret = -EINVAL;

	mutex_lock(&rdc->var_lock);
	switch (offset) {
	case RICHTEK_SPM_DEV_RSPK:
		ret = scnprintf(buf, PAGE_SIZE, "%u\n", rdc->rspk);
		break;
	case RICHTEK_SPM_DEV_INDEX:
		ret = scnprintf(buf, PAGE_SIZE, "%u\n", rdc->ipc_msg.index);
		break;
	case RICHTEK_SPM_DEV_SPKIDX:
		ret = scnprintf(buf, PAGE_SIZE, "%u\n", rdc->spkidx);
		break;
	case RICHTEK_SPM_DEV_PARAMS:
		ret = 0;
		for (i = 0; i < rdc->ipc_msg.param_items; i++) {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					 (i > 0) ? ",%d" : "%d",
					 rdc->ipc_msg.params[i]);
			if (ret < 0)
				return ret;
		}
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
		break;
	case RICHTEK_SPM_DEV_PARAM_ITEMS:
		ret = scnprintf(buf,
				PAGE_SIZE, "%d\n", rdc->ipc_msg.param_items);
		break;
	case RICHTEK_SPM_DEV_TMAX:
	case RICHTEK_SPM_DEV_TMAXCNT:
	case RICHTEK_SPM_DEV_XMAX:
	/* Fall Through */
	case RICHTEK_SPM_DEV_XMAXCNT:
		pitem = ampoff_rparam_items + (offset - RICHTEK_SPM_DEV_TMAX);
			ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
					 *(s32 *)((void *)rdc + pitem->offset));
		/* after get, reset to 0 */
		*(s32 *)((void *)rdc + pitem->offset) = 0;
		break;
	case RICHTEK_SPM_DEV_TMAXKEEP:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
					     rdc->boot_on_tmax);
		break;
	case RICHTEK_SPM_DEV_XMAXKEEP:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
					     rdc->boot_on_xmax);
		break;
	case RICHTEK_SPM_DEV_VVALIDATION_REAL_POWER:
		ret = scnprintf(buf, PAGE_SIZE, "%d.%dW\n",
		      rdc->vali_real_power / 1000, rdc->vali_real_power % 1000);
		break;
	case RICHTEK_SPM_DEV_IPC_GO:
	default:
		break;
	}
	mutex_unlock(&rdc->var_lock);
	return ret;
}

static int richtek_spm_dev_parsing_params(const char *buf,
					  struct richtek_spm_classdev *rdc)
{
	const char *delim = ",/ \n";
	char *buf_cpy, *buf_cpy2, *tmp;
	int i = 0, ret = 0;

	buf_cpy = buf_cpy2 = kstrdup(buf, GFP_KERNEL);
	while ((tmp = strsep(&buf_cpy2, delim)) != NULL) {
		if (!strcmp(tmp, ""))
			continue;
		ret = kstrtos32(tmp, 0, rdc->ipc_msg.params + (i++));
		if (i >= RICHTEK_SPM_MAX_PARAM_ITEMS || ret < 0)
			break;
	}
	kfree(buf_cpy);
	return (ret < 0) ? ret : i;
}

static inline void richtek_spm_dev_erase_all(struct richtek_spm_classdev *rdc)
{
	dev_dbg(rdc->dev, "%s\n", __func__);
	memset(&rdc->ipc_msg, 0, sizeof(rdc->ipc_msg));
}

static int richtek_spm_dev_execute_ipc(struct richtek_spm_classdev *rdc)
{
	int ret = 0;

	if (!rdc->ipc_msg.param_items)
		return -EINVAL;
	/* xlate from kernel intf to fw_ipc */
	/* 1) items to size. 2) cmd index */
	rdc->ipc_msg.param_items *= 4;
	rdc->ipc_msg.cmd -= RICHTEK_SPM_MAGIC;
	rdc->ipc_msg.spkid = rdc->spkidx;
	dev_dbg(rdc->dev, "cmd = %u, index = %u, spkid = %u\n",
		rdc->ipc_msg.cmd, rdc->ipc_msg.index, rdc->ipc_msg.spkid);
	dev_dbg(rdc->dev, "param_size = %d, param[0] = %d\n",
		rdc->ipc_msg.param_items, rdc->ipc_msg.params[0]);
	switch (rdc->ipc_msg.cmd) {
	case CMD_READ:
		dev_info(rdc->dev, "will execute read\n");
		/* Add richtek_spm_read */
		ret = richtek_spm_read(&rdc->ipc_msg, sizeof(rdc->ipc_msg));
		if (ret < 0)
			dev_err(rdc->dev, "spm_read fail\n");
		break;
	case CMD_WRITE:
		dev_info(rdc->dev, "will execute write\n");
		/* Add richtek_spm_write */
		ret = richtek_spm_write(&rdc->ipc_msg, sizeof(rdc->ipc_msg));
		if (ret < 0)
			dev_err(rdc->dev, "spm_write fail\n");
		break;
	default:
		ret = -EINVAL;
		break;
	}
	/* xlate back from fw_ipc to kernel intf */
	/* 1) cmd index. 2) size to items */
	rdc->ipc_msg.cmd += RICHTEK_SPM_MAGIC;
	rdc->ipc_msg.param_items /= 4 ;
	return (ret < 0) ? ret : 0;
}

static ssize_t richtek_spm_dev_attr_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t cnt)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - richtek_spm_dev_attrs;
	int ret = 0;

	mutex_lock(&rdc->var_lock);
	switch (offset) {
	case RICHTEK_SPM_DEV_INDEX:
		ret = kstrtou32(buf, 0, &rdc->ipc_msg.index);
		if (ret < 0)
			goto store_fail;
		break;
	case RICHTEK_SPM_DEV_PARAMS:
		ret = richtek_spm_dev_parsing_params(buf, rdc);
		if (ret < 0)
			goto store_fail;
		if (ret != rdc->ipc_msg.param_items) {
			ret = -EINVAL;
			goto store_fail;
		}
		break;
	case RICHTEK_SPM_DEV_PARAM_ITEMS:
		ret = kstrtos32(buf, 0, &rdc->ipc_msg.param_items);
		if (ret < 0)
			goto store_fail;
		break;
	case RICHTEK_SPM_DEV_IPC_GO:
		ret = kstrtou32(buf, 0, &rdc->ipc_msg.cmd);
		if (ret < 0)
			goto store_fail;
		ret = richtek_spm_dev_execute_ipc(rdc);
		if (ret < 0)
			goto store_fail;
		break;
	case RICHTEK_SPM_DEV_RSPK:
	case RICHTEK_SPM_DEV_SPKIDX:
	case RICHTEK_SPM_DEV_TMAX:
	case RICHTEK_SPM_DEV_TMAXCNT:
	case RICHTEK_SPM_DEV_XMAX:
	case RICHTEK_SPM_DEV_XMAXCNT:
	default:
		break;
	}
	mutex_unlock(&rdc->var_lock);
	return cnt;
store_fail:
	richtek_spm_dev_erase_all(rdc);
	mutex_unlock(&rdc->var_lock);
	return ret;
}

static int rt_spm_classdev_rw_param(struct richtek_spm_classdev *rdc,
				    const struct spm_param_item *pitem,
				    bool read)
{
	struct richtek_spm_ipc_msg *pipc_msg = &rdc->ipc_msg;
	int ret;

	if (!rdc || !pitem)
		return -EINVAL;
	memset(pipc_msg, 0, sizeof(*pipc_msg));
	pipc_msg->cmd = RICHTEK_SPM_MAGIC + (read ? CMD_READ : CMD_WRITE);
	pipc_msg->index = pitem->index;
	pipc_msg->param_items = pitem->param_items;
	if (!read) {
		memcpy(pipc_msg->params, (void *)rdc + pitem->offset,
					      sizeof(s32) * pitem->param_items);
	}
	ret = richtek_spm_dev_execute_ipc(rdc);
	if (ret < 0) {
		dev_err(rdc->dev, "%s: execute ipc fail\n", __func__);
		goto out_rw_param;
	}
	if (read) {
		memcpy((void *)rdc + pitem->offset,
		       pipc_msg->params, sizeof(s32) * pitem->param_items);
	}
out_rw_param:
	richtek_spm_dev_erase_all(rdc);
	return (ret < 0) ? ret : 0;
}

static ssize_t richtek_spm_class_attr_show(struct class *,
					   struct class_attribute *, char *);
static ssize_t richtek_spm_class_attr_store(struct class *,
				struct class_attribute *, const char *, size_t);
static const struct class_attribute richtek_spm_class_attrs[] = {
	__ATTR(trigger, 0220, richtek_spm_class_attr_show,
	       richtek_spm_class_attr_store),
	__ATTR(status, 0664, richtek_spm_class_attr_show,
	       richtek_spm_class_attr_store),
	__ATTR(internal_status, 0444, richtek_spm_class_attr_show,
	       richtek_spm_class_attr_store),
	__ATTR_NULL,
};

enum {
	RICHTEK_SPM_CLASS_TRIGGER = 0,
	RICHTEK_SPM_CLASS_STATUS, /* this is for Samsung Calibration */
	RICHTEK_SPM_CLASS_INTERNAL_STATUS,
	RICHTEK_SPM_CLASS_MAX,
};

static ssize_t richtek_spm_class_attr_show(struct class *cls,
					struct class_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - richtek_spm_class_attrs;
	int ret = 0;

	switch (offset) {
	case RICHTEK_SPM_CLASS_INTERNAL_STATUS:
 		ret = scnprintf(buf, PAGE_SIZE, "%d\n", trig_status);
 		break;
	case RICHTEK_SPM_CLASS_STATUS:
		ret = scnprintf(buf, PAGE_SIZE,
			"%s\n", cali_status ? "Enabled" : "Disable");
		break;
	case RICHTEK_SPM_CLASS_TRIGGER:
	default:
		return -EINVAL;
	}
	return ret;
}

static int fs_read_file(char *filename, char *data, size_t size)
{
	struct file *filp;
	mm_segment_t old_fs = get_fs();
	int flags = O_RDONLY;
	int ret;

	set_fs(KERNEL_DS);
	filp = filp_open(filename, flags, 0444);
	if (IS_ERR(filp)) {
		pr_err("[rt_spm] there is no spm_cal file\n");
		set_fs(old_fs);
		return -EEXIST;
	}
	ret = vfs_read(filp, data, size, &filp->f_pos);
	if (ret < 0) {
		pr_err("[rt_spm] can't read spm calibration value to file\n");
		ret = -EIO;
	}
	filp_close(filp, current->files);
	set_fs(old_fs);
	return ret;
}

static int fs_write_file(char *filename, const char *data, size_t size)
{
	struct file *filp;
	mm_segment_t old_fs = get_fs();
	int flags = O_CREAT | O_TRUNC | O_WRONLY;
	int ret;

	set_fs(KERNEL_DS);
	filp = filp_open(filename, flags, 0444);
	if (IS_ERR(filp)) {
		pr_err("[rt_spm] Can't open calibration file\n");
		set_fs(old_fs);
		ret = PTR_ERR(filp);
		return ret;
	}
	ret = vfs_write(filp, data, size, &filp->f_pos);
	if (ret != size) {
		pr_err("%s: can't write spm calibration value to file\n",
				__func__);
		ret = -EIO;
	}
	filp_close(filp, current->files);
	set_fs(old_fs);

	return ret;
}

enum {
	TRIGGER_CMD_READ = 0,
	TRIGGER_CMD_WRITE,
	TRIGGER_CMD_CALIBRATION,
	TRIGGER_CMD_VVALIDATION,
	TRIGGER_CMD_S_CALIBRATION, /* only calibration but don't write to efs */
	TRIGGER_CMD_MAX,
};

static int rt_spm_vali_threadfn(void *data)
{
	struct richtek_spm_classdev *rdc = data;
	const struct spm_param_item *pitem;
	int i, max_try = 5, ret;

	dev_dbg(rdc->dev, "%s: start\n", __func__);
	mutex_lock(&rdc->var_lock);
	/* vali enable write */
	rdc->vali_enable = 1;
	for (i = 0; i < ARRAY_SIZE(vali_wswitch_items); i++) {
		pitem = vali_wswitch_items + i;
		ret = rt_spm_classdev_rw_param(rdc, pitem, false);
		if (ret < 0) {
			dev_err(rdc->dev, "vali start param [%d] fail\n", i);
			goto out_threadfn;
		}
	}
	msleep(300);
	while (max_try--) {
		for (i = 0; i < ARRAY_SIZE(vali_rstatus_items); i++) {
			pitem = vali_rstatus_items + i;
			ret = rt_spm_classdev_rw_param(rdc, pitem, true);
			if (ret < 0) {
				dev_err(rdc->dev, "state param [%d] fail\n", i);
				goto out_threadfn;
			}
		}
		if (rdc->vali_status < 0) {
			ret = -EINVAL;
			break;
		} else if (rdc->vali_status == 2) {
			dev_dbg(rdc->dev, "vvalidation ending\n");
			ret = 0;
			break;
		}
		if (max_try == 0) {
			ret = -EINVAL;
			break;
		}
		msleep(100);
	}
out_threadfn:
	rdc->vali_enable = 0;
	/* vali disable write */
	for (i = 0; i < ARRAY_SIZE(vali_wswitch_items); i++) {
		pitem = vali_wswitch_items + i;
		rt_spm_classdev_rw_param(rdc, pitem, false);
	}
	rdc->vali_status = ret;
	mutex_unlock(&rdc->var_lock);
	complete(&rdc->trig_complete);
	dev_dbg(rdc->dev, "%s: end\n", __func__);
	return 0;
}

static int rt_spm_calib_threadfn(void *data)
{
	struct richtek_spm_classdev *rdc = data;
	const struct spm_param_item *pitem;
	int i, max_try = 5, ret;

	dev_dbg(rdc->dev, "%s: start\n", __func__);
	mutex_lock(&rdc->var_lock);
	/* calib enable write */
	rdc->calib_enable = 1;
	for (i = 0; i < ARRAY_SIZE(calib_wswitch_items); i++) {
		pitem = calib_wswitch_items + i;
		ret = rt_spm_classdev_rw_param(rdc, pitem, false);
		if (ret < 0) {
			dev_err(rdc->dev, "calib start param [%d] fail\n", i);
			goto out_threadfn;
		}
	}
	msleep(300);
	while (max_try--) {
		for (i = 0; i < ARRAY_SIZE(calib_rstatus_items); i++) {
			pitem = calib_rstatus_items + i;
			ret = rt_spm_classdev_rw_param(rdc, pitem, true);
			if (ret < 0) {
				dev_err(rdc->dev, "state param [%d] fail\n", i);
				goto out_threadfn;
			}
		}
		if (rdc->calib_status < 0) {
			ret = -EINVAL;
			break;
		} else if (rdc->calib_status == 2) {
			dev_dbg(rdc->dev, "calib ending\n");
			ret = 0;
			break;
		}
		if (max_try == 0) {
			ret = -EINVAL;
			break;
		}
		msleep(100);
	}
out_threadfn:
	rdc->calib_enable = 0;
	/* calib disable write */
	for (i = 0; i < ARRAY_SIZE(calib_wswitch_items); i++) {
		pitem = calib_wswitch_items + i;
		rt_spm_classdev_rw_param(rdc, pitem, false);
	}
	rdc->calib_status = ret;
	mutex_unlock(&rdc->var_lock);
	complete(&rdc->trig_complete);
	dev_dbg(rdc->dev, "%s: end\n", __func__);
	return 0;
}

static int rt_spm_cls_trigger_write(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	char rspk_dev_path[256];
	char rspk_nstr[20];
	u32 cmd = *(u32 *)data;
	int ret = 0;

	memset(rspk_nstr, 0, sizeof(rspk_nstr));
	ret = scnprintf(rspk_dev_path, sizeof(rspk_dev_path),
			"%s%u/rdc_cal", RICHTEK_SPM_RSPK_PATH, rdc->spkidx);
	if (ret < 0)
		goto out_trigger_write;
	switch (cmd) {
	case TRIGGER_CMD_READ:
		ret = fs_read_file(rspk_dev_path, rspk_nstr, sizeof(rspk_nstr));
		if (ret < 0)
			goto out_trigger_write;
		mutex_lock(&rdc->var_lock);
		ret = kstrtou32(rspk_nstr, 0, &rdc->rspk);
		mutex_unlock(&rdc->var_lock);
		break;
	case TRIGGER_CMD_WRITE:
		mutex_lock(&rdc->var_lock);
		ret = scnprintf(rspk_nstr, sizeof(rspk_nstr), "%u\n", rdc->rspk);
		mutex_unlock(&rdc->var_lock);
		if (ret < 0)
			goto out_trigger_write;
		ret = fs_write_file(rspk_dev_path, rspk_nstr, ret);
		break;
	case TRIGGER_CMD_S_CALIBRATION:
		reinit_completion(&rdc->trig_complete);
		rdc->trig_task = kthread_run(rt_spm_calib_threadfn,
					      rdc, "calib_thread.%s",
					      dev_name(rdc->dev));
		if (IS_ERR(rdc->trig_task)) {
			ret = PTR_ERR(rdc->trig_task);
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "wait thread\n");
		ret = wait_for_completion_interruptible(&rdc->trig_complete);
		if (ret < 0) {
			kthread_stop(rdc->trig_task);
			goto out_trigger_write;
		}
		if (rdc->calib_status < 0) {
			ret = rdc->calib_status;
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "trigger s calib write\n");
		break;
	case TRIGGER_CMD_CALIBRATION:
		reinit_completion(&rdc->trig_complete);
		rdc->trig_task = kthread_run(rt_spm_calib_threadfn,
					      rdc, "calib_thread.%s",
					      dev_name(rdc->dev));
		if (IS_ERR(rdc->trig_task)) {
			ret = PTR_ERR(rdc->trig_task);
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "wait thread\n");
		ret = wait_for_completion_interruptible(&rdc->trig_complete);
		if (ret < 0) {
			kthread_stop(rdc->trig_task);
			goto out_trigger_write;
		}
		if (rdc->calib_status < 0) {
			ret = rdc->calib_status;
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "trigger calib write\n");
		cmd = TRIGGER_CMD_WRITE;
		ret = rt_spm_cls_trigger_write(rdc->dev, &cmd);
		break;
	case TRIGGER_CMD_VVALIDATION:
		reinit_completion(&rdc->trig_complete);
		rdc->trig_task = kthread_run(rt_spm_vali_threadfn,
					     rdc, "vali_thread.%s",
					     dev_name(rdc->dev));
		if (IS_ERR(rdc->trig_task)) {
			ret = PTR_ERR(rdc->trig_task);
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "wait thread\n");
		ret = wait_for_completion_interruptible(&rdc->trig_complete);
		if (ret < 0) {
			kthread_stop(rdc->trig_task);
			goto out_trigger_write;
		}
		if (rdc->vali_status < 0) {
			ret = rdc->vali_status;
			goto out_trigger_write;
		}
		dev_dbg(rdc->dev, "trigger vali wtire\n");
		break;
	default:
		ret = -EINVAL;
		break;
	}
out_trigger_write:
	return (ret < 0) ? ret : 0;
}

static ssize_t richtek_spm_class_attr_store(struct class *cls,
					    struct class_attribute *attr,
					    const char *buf, size_t cnt)
{
	const ptrdiff_t offset = attr - richtek_spm_class_attrs;
	u32 tmp = 0;
	int ret = 0;

	switch (offset) {
	case RICHTEK_SPM_CLASS_TRIGGER:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		tmp -= RICHTEK_SPM_MAGIC;
		pr_info("[richtek_spm] start trigger\n");
		ret = class_for_each_device(cls, NULL, &tmp,
					    rt_spm_cls_trigger_write);
		trig_status = ret;
		if (ret < 0)
			return ret;
		break;
	case RICHTEK_SPM_CLASS_STATUS:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		if (tmp == 1) {
			cali_status = 1;
			tmp = TRIGGER_CMD_S_CALIBRATION;
			ret = class_for_each_device(cls, NULL, &tmp,
					    rt_spm_cls_trigger_write);
			cali_status = 0;
			if (ret < 0)
				return ret;
		}
		break;
	case RICHTEK_SPM_CLASS_INTERNAL_STATUS:
	default:
		return -EINVAL;
	}
	return cnt;
}

#ifdef CONFIG_PM_SLEEP
static int richtek_spm_classdev_suspend(struct device *dev)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc->ops && rdc->ops->suspend) ? rdc->ops->suspend(rdc) : 0;
}

static int richtek_spm_classdev_resume(struct device *dev)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc->ops && rdc->ops->resume) ? rdc->ops->resume(rdc) : 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(richtek_spm_class_pm_ops,
		     richtek_spm_classdev_suspend, richtek_spm_classdev_resume);

int richtek_spm_classdev_register(struct device *parent,
				  struct richtek_spm_classdev *rdc)
{
	static u32 spk_index = 0;

	if (rdc == NULL)
		return -EINVAL;
	if (parent == NULL && rdc->name == NULL) {
		pr_err("[richtek_spm] no name can be speficied\n");
		return -EINVAL;
	}
	if (rdc->name == NULL) {
        rdc->name = devm_kasprintf(parent,
			GFP_KERNEL, "rt_amp%d", spk_index);
    }
	rdc->dev = device_create_with_groups(richtek_spm_class, parent, 0,
					     rdc, rdc->groups, "%s",
					     rdc->name);
	if (IS_ERR(rdc->dev))
		return PTR_ERR(rdc->dev);
	dev_set_drvdata(rdc->dev, rdc);
	/* init */
	init_completion(&rdc->trig_complete);
	mutex_init(&rdc->var_lock);
	memset(&rdc->ipc_msg, 0, sizeof(rdc->ipc_msg));
	rdc->spkidx = spk_index++;
	return 0;
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_register);

void richtek_spm_classdev_unregister(struct richtek_spm_classdev *rdc)
{
	mutex_destroy(&rdc->var_lock);
	device_unregister(rdc->dev);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_unregister);

static void devm_richtek_spm_classdev_release(struct device *dev, void *res)
{
	richtek_spm_classdev_unregister(*(struct richtek_spm_classdev **)res);
}

int devm_richtek_spm_classdev_register(struct device *parent,
				       struct richtek_spm_classdev *rdc)
{
	struct richtek_spm_classdev **prdc;
	int rc;

	prdc = devres_alloc(devm_richtek_spm_classdev_release,
			    sizeof(*prdc), GFP_KERNEL);
	if (!prdc)
		return -ENOMEM;
	rc = richtek_spm_classdev_register(parent, rdc);
	if (rc < 0) {
		devres_free(prdc);
		return rc;
	}
	*prdc = rdc;
	devres_add(parent, prdc);
	return 0;
}
EXPORT_SYMBOL_GPL(devm_richtek_spm_classdev_register);

static int rt_spm_classdev_ampon(struct richtek_spm_classdev *rdc)
{
	const struct spm_param_item *pitem;
	u32 cmd = TRIGGER_CMD_READ;
	int i, ret;

	if (!rdc)
		return -EINVAL;
	/* trigger fs rspk read */
	ret = rt_spm_cls_trigger_write(rdc->dev, &cmd);
	if (ret < 0)
		return ret;
	mutex_lock(&rdc->var_lock);
	rdc->monitor_on = 1;
	for (i = 0; i < ARRAY_SIZE(ampon_wparam_items); i++) {
		pitem = ampon_wparam_items + i;
		ret = rt_spm_classdev_rw_param(rdc, pitem, false);
		if (ret < 0) {
			dev_err(rdc->dev, "ampon write param [%d] fail\n", i);
			goto out_classdev_ampon;
		}
	}
out_classdev_ampon:
	mutex_unlock(&rdc->var_lock);
	return (ret < 0) ? ret : 0;
}

static int rt_spm_classdev_ampoff(struct richtek_spm_classdev *rdc)
{
	const struct spm_param_item *pitem;
	int i, ret;

	if (!rdc)
		return -EINVAL;
	mutex_lock(&rdc->var_lock);
	for (i = 0; i < ARRAY_SIZE(ampoff_rparam_items); i++) {
		pitem = ampoff_rparam_items + i;
		ret = rt_spm_classdev_rw_param(rdc, pitem, true);
		if (ret < 0) {
			dev_err(rdc->dev, "ampoff read param [%d]\n", i);
			break;
		}
	}
	rdc->boot_on_xmax = max(rdc->xmax, rdc->boot_on_xmax);
	rdc->boot_on_tmax = max(rdc->tmax, rdc->boot_on_tmax);
	mutex_unlock(&rdc->var_lock);
	return (ret < 0) ? ret : 0;
}

static int rt_spm_cls_trigger_ampon(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc == data) ? rt_spm_classdev_ampon(rdc) : 0;
}

static int rt_spm_cls_trigger_ampoff(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc == data) ? rt_spm_classdev_ampoff(rdc) : 0;
}

int richtek_spm_classdev_trigger_ampon(struct richtek_spm_classdev *rdc)
{
	return class_for_each_device(richtek_spm_class, NULL,
				     rdc, rt_spm_cls_trigger_ampon);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_trigger_ampon);

int richtek_spm_classdev_trigger_ampoff(struct richtek_spm_classdev *rdc)
{
	return class_for_each_device(richtek_spm_class, NULL,
				     rdc, rt_spm_cls_trigger_ampoff);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_trigger_ampoff);

static int __init richtek_spm_init(void)
{
	int i = 0, ret = 0;

	richtek_spm_class = class_create(THIS_MODULE, "richtek_spm");
	if (IS_ERR(richtek_spm_class))
		return PTR_ERR(richtek_spm_class);
	richtek_spm_class->pm = &richtek_spm_class_pm_ops;
	for (i = 0; richtek_spm_class_attrs[i].attr.name; i++) {
		ret = class_create_file(richtek_spm_class,
					richtek_spm_class_attrs + i);
		if (ret < 0)
			goto out_cls_attr;
	}
	richtek_spm_class->dev_groups = richtek_spm_classdev_groups;
	return 0;
out_cls_attr:
	while (--i >= 0) {
		class_remove_file(richtek_spm_class,
				  richtek_spm_class_attrs + i);
	}
	class_destroy(richtek_spm_class);
	return ret;
}
subsys_initcall(richtek_spm_init);

static void __exit richtek_spm_exit(void)
{
	int i = 0;

	for (i = 0; richtek_spm_class_attrs[i].attr.name; i++) {
		class_remove_file(richtek_spm_class,
				  richtek_spm_class_attrs + i);
	}
	class_destroy(richtek_spm_class);
}
module_exit(richtek_spm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Richtek SPM Class driver");
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_VERSION("1.0.5_S");
