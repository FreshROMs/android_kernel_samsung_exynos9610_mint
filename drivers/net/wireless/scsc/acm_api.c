/*****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/netdevice.h>
#include "mlme.h"
#include "mib.h"
#include "debug.h"
/* PSID Hdr(4) + VLDATA Hdr(1) + TSF(8) * + padding(1)*/
#define TSF_RESP_SIZE (14)

unsigned long long SDA_getTsf(const unsigned char if_id)
{
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;
	struct slsi_dev *sdev = NULL;
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	/* Alloc buffer in stack to avoid possible scheduling when malloc.*/
	u8 res_payload[TSF_RESP_SIZE] = { 0 };
	int r = 0;
	int rx_length = 0;
	unsigned long long ret = 0;

	read_lock(&dev_base_lock);
	dev = first_net_device(&init_net);
	while (dev) {
		if (if_id == 0 && memcmp(dev->name, "wlan0", 5) == 0)
			break;
		else if (if_id == 1 && memcmp(dev->name, "p2p0", 4) == 0)
			break;
		dev = next_net_device(dev);
	}
	read_unlock(&dev_base_lock);

	if (!dev) {
		SLSI_ERR(sdev, "dev is null\n");
		return 0;
	}
	ndev_vif = netdev_priv(dev);
	if (!ndev_vif) {
		SLSI_ERR(sdev, "ndev_vif is null\n");
		return 0;
	}
	sdev = ndev_vif->sdev;
	if (!sdev) {
		SLSI_ERR(sdev, "sdev is null\n");
		return 0;
	}

	SLSI_DBG3(sdev, SLSI_MLME, "\n");
	slsi_mib_encode_get(&mibreq, SLSI_PSID_UNIFI_CURRENT_TSF_TIME, 0);
	mibrsp.dataLength = sizeof(res_payload);
	mibrsp.data = res_payload;

	r = slsi_mlme_get(sdev, dev, mibreq.data, mibreq.dataLength, mibrsp.data, mibrsp.dataLength, &rx_length);

	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_length;
		if (rx_length == 0) {
			SLSI_ERR(sdev, "Mib decode error\n");
			return 0;
		}
		slsi_mib_decodeInt64(&mibrsp.data[4], &ret);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
		return 0;
	}
	return ret;
}
