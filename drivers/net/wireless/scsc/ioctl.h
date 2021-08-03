/******************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef __MX_IOCTL_H__
#define __MX_IOCTL_H__

#include <linux/if.h>
#include <linux/netdevice.h>

struct android_wifi_priv_cmd {
	char *buf;
	int  used_len;
	int  total_len;
};

int slsi_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
int slsi_get_sta_info(struct net_device *dev, char *command, int buf_len);
struct slsi_ioctl_args *slsi_get_private_command_args(char *buffer, int buf_len, int max_arg_count);
#define SLSI_VERIFY_IOCTL_ARGS(sdev, ioctl_args) \
	do { \
		if (!ioctl_args) { \
			SLSI_ERR(sdev, "Malloc of ioctl_args failed.\n"); \
			return -ENOMEM; \
		} \
	\
		if (!ioctl_args->arg_count) { \
			kfree(ioctl_args); \
			return -EINVAL; \
		} \
	} while (0)

struct slsi_supported_channels {
	int start_chan_num;
	int channel_count;
	int increment;
	int band;
};
#endif /*  __MX_IOCTL_H__ */
