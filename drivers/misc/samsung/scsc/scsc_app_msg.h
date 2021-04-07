/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SCSC_APP_MSG_H__
#define __SCSC_APP_MSG_H__


#define BCSP_CSTOPB_MASK  0x0001
#define BCSP_PARENB_MASK  0x0002
#define BCSP_PAREVEN_MASK  0x0004
#define BCSP_CRTSCTS_MASK 0x0008

enum {
	SCSC_APP_MSG_TYPE_APP_STARTED_REPLY = 0,
	SCSC_APP_MSG_TYPE_GET_DB,
	SCSC_APP_MSG_TYPE_GET_DB_REPLY,
	SCSC_APP_MSG_TYPE_LD_REGISTER_LOW_RATE,
	SCSC_APP_MSG_TYPE_LD_REGISTER_HIGH_RATE,
	SCSC_APP_MSG_TYPE_LD_REGISTER_REPLY,
	SCSC_APP_MSG_TYPE_LD_UNREGISTER,
	SCSC_APP_MSG_TYPE_LD_UNREGISTER_BREAK,
	SCSC_APP_MSG_TYPE_LD_UNREGISTER_REPLY,
	SCSC_APP_MSG_TYPE_APP_EXIT,
	SCSC_APP_MSG_TYPE_APP_EXIT_REPLY,
	SCSC_APP_MSG_TYPE_SET_FAST_RATE,
	SCSC_APP_MSG_TYPE_SET_FAST_RATE_REPLY,
};

enum {
	SCSC_APP_MSG_STATUS_OK = 0,
	SCSC_APP_MSG_STATUS_FAILURE,
};


struct scsc_app_msg_req {
	__u16 type;
};

struct scsc_app_msg_resp {
	__u16 type;
	__u16 status;
	__u32 len;
	__u8  data[0];
};

#endif /* __SCSC_APP_MSG_H__ */
