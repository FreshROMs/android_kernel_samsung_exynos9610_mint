/*****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_BA_H__
#define __SLSI_BA_H__

#include "dev.h"

void slsi_handle_blockack(struct net_device *dev, struct slsi_peer *peer,
			  u16 vif, u8 *peer_qsta_address, u16 parameter_set, u16 sequence_number,
			  u16 reason_code, u16 direction);

int slsi_ba_process_frame(struct net_device *dev, struct slsi_peer *peer,
			  struct sk_buff *skb, u16 sequence_number, u16 tid);

void slsi_ba_update_window(struct net_device *dev,
				  struct slsi_ba_session_rx *ba_session_rx, u16 sequence_number);

void slsi_ba_process_complete(struct net_device *dev, bool ctx_napi);

bool slsi_ba_check(struct slsi_peer *peer, u16 tid);

void slsi_rx_ba_stop_all(struct net_device *dev, struct slsi_peer *peer);

void slsi_rx_ba_init(struct slsi_dev *sdev);
#endif
