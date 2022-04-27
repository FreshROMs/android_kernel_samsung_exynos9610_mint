/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Sensitive Data Protection
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef SDP_COMMON_H__
#define SDP_COMMON_H__

#define PER_USER_RANGE 100000

#define KNOX_PERSONA_BASE_ID 100
#define DEK_USER_ID_OFFSET	100

#define BASE_ID KNOX_PERSONA_BASE_ID
#define GET_ARR_IDX(__userid) (__userid - BASE_ID)

#define SDP_CACHE_CLEANUP_DEBUG   0

#endif
