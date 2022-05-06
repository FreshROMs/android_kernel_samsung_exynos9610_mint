/*****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef _SLSI_CONST_H__
#define _SLSI_CONST_H__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ieee80211.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max number of peers */
#define SLSI_TDLS_PEER_CONNECTIONS_MAX 4
#define SLSI_AP_PEER_CONNECTIONS_MAX 10
/* When SLSI_ADHOC_PEER_CONNECTIONS_MAX is increased to 32,
 *  method of intiliazing peer_sta_record[] should be carefully changed
 *  from the present way of static allocation to a dynamic allocation or
 *  a VIF specific initialization or both.
 */
#define SLSI_ADHOC_PEER_CONNECTIONS_MAX 16

/* Max number of indexes */
#define SLSI_TDLS_PEER_INDEX_MIN 2
#define SLSI_TDLS_PEER_INDEX_MAX 15
#define SLSI_PEER_INDEX_MIN 1
#define SLSI_PEER_INDEX_MAX 16

#define SLSI_STA_PEER_QUEUESET 0

#define SLSI_BA_TRAFFIC_STREAM_MAX 8
#define SLSI_BA_BUFFER_SIZE_MAX 64


#ifdef __cplusplus
}
#endif

#endif /* _SLSI_CONST_H__ */
