/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_DEBUG_H__
#define __SSP_DEBUG_H__
#include "ssp.h"



void reset_task(struct work_struct *work);
void enable_debug_timer(struct ssp_data *);
void disable_debug_timer(struct ssp_data *);
int initialize_debug_timer(struct ssp_data *);
int print_mcu_debug(char *, int *, int );
void print_dataframe(struct ssp_data *data, char *dataframe, int frame_len);
void reset_mcu(struct ssp_data *data, int reason);
int ssp_wait_event_timeout(struct ssp_waitevent *lock, int timeout);
void ssp_lock_wait_event(struct ssp_waitevent *lock);
void ssp_wake_up_wait_event(struct ssp_waitevent *lock);

#endif /*__SSP_DEBUG_H__*/
