/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * YooMi Tak <youmi.tak@samsung.com>
 *
*/

#ifndef __SSP_COMM_H__
#define __SSP_COMM_H__


#include "ssp.h"

struct ssp_msg {
	u8 cmd;
	u8 type;
	u8 subcmd;
	u16 length;
	u64 timestamp;
	char *buffer;
	u8 res;         /* success : 1 fail : 0 */
	bool clean_pending_list_flag;
	struct completion *done;
	struct list_head list;
} __attribute__((__packed__));

void handle_packet(struct ssp_data *, char *, int);

int make_command(struct ssp_data *data, u8 uInst,
                 u8 sensor_type, u8 *send_buf, u16 length);
int ssp_send_command(struct ssp_data *data, u8 cmd, u8 type, u8 subcmd,
                     int timeout, char *send_buf, int send_buf_len, char **receive_buf,
                     int *receive_buf_len);

void clean_pending_list(struct ssp_data *data);
int ssp_send_status(struct ssp_data *data, char command);


int enable_sensor(struct ssp_data *data, unsigned int type, u8 *buf, int buf_len);
int disable_sensor(struct ssp_data *data, unsigned int type, u8 *buf, int buf_len);

#endif /* __SSP_COMM_H__ */
