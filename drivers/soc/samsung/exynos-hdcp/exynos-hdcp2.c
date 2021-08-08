/*
 * drivers/soc/samsung/exynos-hdcp/exynos-hdcp.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/smc.h>
#include <asm/cacheflush.h>
#include <linux/smc.h>
#include "iia_link/exynos-hdcp2-iia-auth.h"
#include "exynos-hdcp2-teeif.h"
#include "iia_link/exynos-hdcp2-iia-selftest.h"
#include "exynos-hdcp2-encrypt.h"
#include "exynos-hdcp2-log.h"
#include "dp_link/exynos-hdcp2-dplink-if.h"
#include "dp_link/exynos-hdcp2-dplink.h"
#include "dp_link/exynos-hdcp2-dplink-selftest.h"

#define EXYNOS_HDCP_DEV_NAME	"hdcp2"

struct miscdevice hdcp;
static DEFINE_MUTEX(hdcp_lock);
struct hdcp_session_list g_hdcp_session_list;
enum hdcp_result hdcp_link_ioc_authenticate(void);


static uint32_t inst_num;

static long hdcp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rval;

	switch (cmd) {
#if defined(CONFIG_HDCP2_IIA_ENABLE)
	case (uint32_t)HDCP_IOC_SESSION_OPEN:
	{
		struct hdcp_sess_info ss_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&ss_info, (void __user *)arg, sizeof(struct hdcp_sess_info));
		if (rval) {
			hdcp_err("Session open copy from user fail. (%x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_session_open(&ss_info);
		if (rval) {
			hdcp_err("Session open fail. (%x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return rval;
		}

		rval = copy_to_user((void __user *)arg, &ss_info, sizeof(struct hdcp_sess_info));
		if (rval) {
			hdcp_err("Session open copy to user fail. (%x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_SESSION_CLOSE:
	{
		/* todo: session close */
		struct hdcp_sess_info ss_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&ss_info, (void __user *)arg, sizeof(struct hdcp_sess_info));
		if (rval) {
			hdcp_err("Session close copy from user fail. (%x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_session_close(&ss_info);
		if (rval) {
			hdcp_err("hdcp_session close fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_SESSION_CLOSE_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_LINK_OPEN:
	{
		struct hdcp_link_info lk_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&lk_info, (void __user *)arg, sizeof(struct hdcp_link_info));
		if (rval) {
			hdcp_err("hdcp_link open copy from user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_link_open(&lk_info, HDCP_LINK_TYPE_IIA);
		if (rval) {
			hdcp_err("hdcp_link open fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_LINK_OPEN_FAILED;
		}

		rval = copy_to_user((void __user *)arg, &lk_info, sizeof(struct hdcp_link_info));
		if (rval) {
			hdcp_err("hdcp_link open copy to user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_LINK_CLOSE:
	{
		struct hdcp_link_info lk_info;

		mutex_lock(&hdcp_lock);
		/* find Session node which contain the Link */
		rval = copy_from_user(&lk_info, (void __user *)arg, sizeof(struct hdcp_link_info));
		if (rval) {
			hdcp_err("hdcp_link close copy from user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_link_close(&lk_info);
		if (rval) {
			hdcp_err("hdcp_link close fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_LINK_CLOSE_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_LINK_AUTH:
	{
		struct hdcp_msg_info msg_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&msg_info, (void __user *)arg, sizeof(struct hdcp_msg_info));
		if (rval) {
			hdcp_err("hdcp_authenticate fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_link_authenticate(&msg_info);
		if (rval) {
			hdcp_err("hdcp_authenticate fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_AUTHENTICATE_FAILED;
		}

		rval = copy_to_user((void __user *)arg, &msg_info, sizeof(struct hdcp_msg_info));
		if (rval) {
			hdcp_err("hdcp_authenticate fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_LINK_ENC:
	{
		/* todo: link close */
		struct hdcp_enc_info enc_info;
		size_t packet_len = 0;
		uint8_t pes_priv[HDCP_PRIVATE_DATA_LEN];
		unsigned long input_phys, output_phys;
		struct hdcp_session_node *ss_node;
		struct hdcp_link_node *lk_node;
		struct hdcp_link_data *lk_data;
		uint8_t *input_virt, *output_virt;
		int ret = HDCP_SUCCESS;

		mutex_lock(&hdcp_lock);
		/* find Session node which contain the Link */
		rval = copy_from_user(&enc_info, (void __user *)arg, sizeof(struct hdcp_enc_info));
		if (rval) {
			hdcp_err("hdcp_encrypt copy from user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		/* find Session node which contains the Link */
		ss_node = hdcp_session_list_find(enc_info.id, &g_hdcp_session_list);
		if (!ss_node) {
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_INVALID_INPUT;
		}
		lk_node = hdcp_link_list_find(enc_info.id, &ss_node->ss_data->ln);
		if (!lk_node) {
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_INVALID_INPUT;
		}
		lk_data = lk_node->lk_data;

		if (!lk_data) {
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_INVALID_INPUT;
		}

		input_phys = (unsigned long)enc_info.input_phys;
		output_phys = (unsigned long)enc_info.output_phys;

		/* set input counters */
		memset(&(lk_data->tx_ctx.input_ctr), 0x00, HDCP_INPUT_CTR_LEN);
		/* set output counters */
		memset(&(lk_data->tx_ctx.str_ctr), 0x00, HDCP_STR_CTR_LEN);

		packet_len = (size_t)enc_info.input_len;
		input_virt =  (uint8_t *)phys_to_virt(input_phys);
		output_virt =  (uint8_t *)phys_to_virt(output_phys);

		__flush_dcache_area(input_virt, packet_len);
		__flush_dcache_area(output_virt, packet_len);

		ret = encrypt_packet(pes_priv,
				input_phys, packet_len,
				output_phys, packet_len,
				&(lk_data->tx_ctx));

		if (ret) {
			hdcp_err("encrypt_packet() is failed with 0x%x\n", ret);
			mutex_unlock(&hdcp_lock);
			return -1;
		}

		rval = copy_to_user((void __user *)arg, &enc_info, sizeof(struct hdcp_enc_info));
		if (rval) {
			hdcp_err("hdcp_encrypt copy to user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
	case (uint32_t)HDCP_IOC_STREAM_MANAGE:
	{
		struct hdcp_stream_info stream_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&stream_info, (void __user *)arg, sizeof(struct hdcp_stream_info));
		if (rval) {
			hdcp_err("hdcp_link stream manage copy from user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		rval = hdcp_link_stream_manage(&stream_info);
		if (rval) {
			hdcp_err("hdcp_link stream manage fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_LINK_STREAM_FAILED;
		}

		rval = copy_to_user((void __user *)arg, &stream_info, sizeof(struct hdcp_stream_info));
		if (rval) {
			hdcp_err("hdcp_link stream manage copy to user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}
#endif
#if defined(CONFIG_HDCP2_EMULATION_MODE)
	case (uint32_t)HDCP_IOC_EMUL_DPLINK_TX:
	{
		uint32_t emul_cmd;

		if (copy_from_user(&emul_cmd, (void __user *)arg, sizeof(uint32_t)))
			return -EINVAL;

		return dplink_emul_handler(emul_cmd);
	}
#endif
	case (uint32_t)HDCP_IOC_DPLINK_TX_AUTH:
	{
#if defined(CONFIG_HDCP2_DP_ENABLE)
		rval = dp_hdcp_protocol_self_test();
		if (rval) {
			hdcp_err("DP self_test fail. errno(%d)\n", rval);
			return rval;
		}
		hdcp_err("DP self_test success!!\n");
#endif
#if defined(TEST_VECTOR_2)
		/* todo: support test vector 1 */
		rval = iia_hdcp_protocol_self_test();
		if (rval) {
			hdcp_err("IIA self_test failed. errno(%d)\n", rval);
			return rval;
		}
		hdcp_err("IIA self_test success!!\n");
#endif
		rval = 0;
		return rval;
	}
	case (uint32_t)HDCP_IOC_WRAP_KEY:
	{
		struct hdcp_wrapped_key key_info;

		mutex_lock(&hdcp_lock);
		rval = copy_from_user(&key_info, (void __user *)arg, sizeof(struct hdcp_wrapped_key));
		if (rval) {
			hdcp_err("hdcp_wrap_key copy from user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_FROM_USER_FAILED;
		}

		if (hdcp_wrap_key(&key_info)) {
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_WRAP_FAIL;
		}
		rval = copy_to_user((void __user *)arg, &key_info, sizeof(struct hdcp_wrapped_key));
		if (rval) {
			hdcp_err("hdcp_wrap_key copy to user fail (0x%08x)\n", rval);
			mutex_unlock(&hdcp_lock);
			return HDCP_ERROR_COPY_TO_USER_FAILED;
		}

		mutex_unlock(&hdcp_lock);
		break;
	}

	default:
		hdcp_err("HDCP: Invalid IOC num(%d)\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

static int hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct device *dev = miscdev->this_device;
	struct hdcp_info *info;

	info = kzalloc(sizeof(struct hdcp_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	file->private_data = info;

	mutex_lock(&hdcp_lock);
	inst_num++;
	/* todo: hdcp device initialize ? */
	mutex_unlock(&hdcp_lock);

	return 0;
}

static int hdcp_release(struct inode *inode, struct file *file)
{
	struct hdcp_info *info = file->private_data;

	/* disable drm if we were the one to turn it on */
	mutex_lock(&hdcp_lock);
	inst_num--;
	/* todo: hdcp device finalize ? */
	mutex_unlock(&hdcp_lock);

	kfree(info);
	return 0;
}

static int __init hdcp_init(void)
{
	int ret;

	hdcp_info("hdcp2 driver init\n");

	ret = misc_register(&hdcp);
	if (ret) {
		hdcp_err("hdcp can't register misc on minor=%d\n",
				MISC_DYNAMIC_MINOR);
		return ret;
	}

	/* todo: do initialize sequence */
	hdcp_session_list_init(&g_hdcp_session_list);
#if defined(CONFIG_HDCP2_DP_ENABLE)
	if (hdcp_dplink_init() < 0) {
		hdcp_err("hdcp_dplink_init fail\n");
		return -EINVAL;
	}
#endif
	ret = hdcp_tee_open();
	if (ret) {
		hdcp_err("hdcp_tee_open fail\n");
		return -EINVAL;
	}

	return 0;
}

static void __exit hdcp_exit(void)
{
	/* todo: do clear sequence */

	misc_deregister(&hdcp);
	hdcp_session_list_destroy(&g_hdcp_session_list);
	hdcp_tee_close();
}

static const struct file_operations hdcp_fops = {
	.owner		= THIS_MODULE,
	.open		= hdcp_open,
	.release	= hdcp_release,
	.compat_ioctl = hdcp_ioctl,
	.unlocked_ioctl = hdcp_ioctl,
};

struct miscdevice hdcp = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= EXYNOS_HDCP_DEV_NAME,
	.fops	= &hdcp_fops,
};

module_init(hdcp_init);
module_exit(hdcp_exit);
