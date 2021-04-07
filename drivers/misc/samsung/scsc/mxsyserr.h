/****************************************************************************
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MX_SYSERR_H
#define __MX_SYSERR_H


struct mx_syserr {
	u16	length;		/* Length of this structure for future extension */
	u32	slow_clock;
	u32	fast_clock;
	u32	string_index;
	u32	syserr_code;
	u32	param[2];
} __packed;

struct mx_syserr_msg {
	u8		 id;
	struct mx_syserr syserr;
} __packed;

void mx_syserr_init(void);
void mx_syserr_handler(struct mxman *mx, const void *message);

#endif
