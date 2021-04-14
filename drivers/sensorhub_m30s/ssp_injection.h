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

#ifndef __SSP_INJECTION_H__
#define __SSP_INJECTION_H__

#include "ssp.h"

int ssp_injection_initialize(struct ssp_data *ssp_data);
void ssp_injection_remove(struct ssp_data *ssp_data);
#endif /*__SSP_INJECTION_H__*/

