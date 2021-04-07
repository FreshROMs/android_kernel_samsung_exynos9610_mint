/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * Shared Memory driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/
#ifndef SHMEM_IPC_H
#define SHMEM_IPC_H

struct shdmem_info {
	unsigned int base;
	unsigned int size;
};

unsigned long shm_get_phys_base(void);
unsigned shm_get_phys_size(void);
unsigned shm_get_boot_size(void);
unsigned shm_get_ipc_rgn_offset(void);
unsigned shm_get_ipc_rgn_size(void);
unsigned long shm_get_security_param2(unsigned long mode, u32 bl_size);
unsigned long shm_get_security_param3(unsigned long mode, u32 main_size);

void __iomem *shm_request_region(unsigned long sh_addr, unsigned size);
void __iomem *shm_get_boot_region(void);
void __iomem *shm_get_ipc_region(void);
void shm_release_region(void *v_addr);
#endif
