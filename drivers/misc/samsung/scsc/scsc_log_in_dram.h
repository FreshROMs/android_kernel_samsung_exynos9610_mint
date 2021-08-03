/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef SCSC_LOG_IN_DRAM_MMAP_H
#define SCSC_LOG_IN_DRAM_MMAP_H

#define MIFRAMMAN_LOG_DRAM_SZ	(16 * 1024 * 1024)

int scsc_log_in_dram_mmap_create(void);
int scsc_log_in_dram_mmap_destroy(void);
#endif /* SCSC_LOG_IN_DRAM_MMAP_H */
