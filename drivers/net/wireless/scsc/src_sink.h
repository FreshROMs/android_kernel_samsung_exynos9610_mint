/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_SRC_SINK_H__
#define __SLSI_SRC_SINK_H__

#include "unifiio.h"

struct slsi_src_sink_params {
	struct unifiio_src_sink_report sink_report;
	struct unifiio_src_sink_report gen_report;
};

long slsi_src_sink_cdev_ioctl_cfg(struct slsi_dev *sdev, unsigned long arg);

void slsi_rx_sink_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_rx_gen_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

#endif /* __SLSI_SRC_SINK_H__ */
