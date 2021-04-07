/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350.h
 *
 *	Description : API header of ISDB-T baseband module
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	History :
 *	----------------------------------------------------------------------
 ******************************************************************************/
#ifndef __FCI_OAL_H__
#define __FCI_OAL_H__

#ifdef __cplusplus
extern "C" {
#endif

extern void print_log(HANDLE handle, s8 *fmt, ...);
extern void msWait(s32 ms);
/* extern void OAL_CREATE_SEMAPHORE(void); */
/* extern void OAL_DELETE_SEMAPHORE(void); */
/* extern void OAL_OBTAIN_SEMAPHORE(void); */
/* extern void OAL_RELEASE_SEMAPHORE(void);*/

#ifdef __cplusplus
}
#endif

#endif /* __FCI_OAL_H__ */

