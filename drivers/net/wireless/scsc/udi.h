/******************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef __SLSI_UDI_H__
#define __SLSI_UDI_H__

#include "dev.h"

#ifdef SLSI_TEST_DEV

/* Maximum number of nodes supported in UNIT TEST MODE
 * arbitrarily set to 20, could increase this if needed
 */
#define SLSI_UDI_MINOR_NODES 20

#else
#define SLSI_UDI_MINOR_NODES 2                 /* Maximum number of nodes supported. */
#endif

int slsi_udi_node_init(struct slsi_dev *sdev, struct device *parent);
int slsi_udi_node_deinit(struct slsi_dev *sdev);

int slsi_udi_init(void);
int slsi_udi_deinit(void);
int slsi_kernel_to_user_space_event(struct slsi_log_client *log_client, u16 event, u32 data_length, const u8 *data);
int slsi_check_cdev_refs(void);

#endif
