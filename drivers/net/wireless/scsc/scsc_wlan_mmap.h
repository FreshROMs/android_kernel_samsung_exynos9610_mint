/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef SCSC_WLAN_MMAP_H
#define SCSC_WLAN_MMAP_H

int scsc_wlan_mmap_create(void);
int scsc_wlan_mmap_destroy(void);
int scsc_wlan_mmap_set_buffer(void *buf, size_t sz);
#endif /* SCSC_WLAN_MMAP_H */

