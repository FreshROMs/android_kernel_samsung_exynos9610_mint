
#include <linux/kernel.h>
#include <linux/slab.h>

#include "ssp.h"
#include "ssp_comm.h"
#include "ssp_data.h"
#include "ssp_cmd_define.h"
#include "ssp_debug.h"

#define SSP_CMD_SIZE 64
#define SSP_MSG_HEADER_SIZE 13

void handle_packet(struct ssp_data *data, char *packet, int packet_size)
{
	u16 msg_length = 0;
	u8 msg_cmd = 0, msg_subcmd = 0, msg_type = 0;
	char *buffer;

	if(packet_size < SSP_MSG_HEADER_SIZE) {
		ssp_infof("nanohub packet size is small/(%s)", packet);
		return;
	}

	msg_cmd = packet[0];
	msg_type = packet[1];
	msg_subcmd = packet[2];
	msg_length = MAKE_WORD(packet[4], packet[3]);

/*
	ssp_infof("cmd %d, type %d, sub_cmd %d, length %d(0x%x, 0x%x)",
		msg_cmd, msg_type, msg_subcmd, msg_length, packet[3], packet[4]);
*/
	if (msg_length == 0) {
		ssp_errf("lengh is zero %d %d %d", msg_cmd, msg_type, msg_subcmd);
		return;
	}

	if (msg_cmd <= CMD_GETVALUE) {
		bool found = false;
		struct ssp_msg *msg, *n;

		mutex_lock(&data->pending_mutex);
		if (!list_empty(&data->pending_list)) {
			list_for_each_entry_safe(msg, n,
			                         &data->pending_list, list) {

				if ((msg->cmd == msg_cmd) && (msg->type == msg_type) &&
				    (msg->subcmd == msg_subcmd)) {
					list_del(&msg->list);
					found = true;
					break;
				}
			}

			if (!found) {
				ssp_errf("%d %d %d - Not match error",  msg_cmd, msg_type, msg_subcmd);
				goto exit;
			}

			if (msg_cmd == CMD_GETVALUE) {
				msg->length = msg_length;
				if (msg->length != 0) {
					if (msg->buffer != NULL) {
						kfree(msg->buffer);
					}
					msg->buffer = kzalloc(msg->length, GFP_KERNEL);
					memcpy(msg->buffer, packet + SSP_MSG_HEADER_SIZE, msg->length);

				} else {
					msg->res = 0;
				}
			}

			if (msg->done != NULL && !completion_done(msg->done)) {
				complete(msg->done);
			}

		} else {
			ssp_errf("List empty error(%d %d %d)", msg_cmd, msg_type, msg_subcmd);
		}
exit:
		mutex_unlock(&data->pending_mutex);
	} else if (msg_cmd == CMD_REPORT) {

	    buffer = kzalloc(msg_length, GFP_KERNEL);
		memcpy(buffer, &packet[SSP_MSG_HEADER_SIZE], msg_length);
		parse_dataframe(data, buffer, msg_length);
		kfree(buffer);
	} else {
		ssp_infof("msg_cmd does not define. cmd is %d", msg_cmd);
	}
	return;
}

static char ssp_cmd_data[SSP_CMD_SIZE];

static int do_transfer(struct ssp_data *data, struct ssp_msg *msg, int timeout)
{
	int status = 0;
	int ret = 0;
	bool is_ssp_shutdown;

	mutex_lock(&data->comm_mutex);

	if (!is_sensorhub_working(data)) {
		ssp_errf("sensorhub is not working");
		mutex_unlock(&data->comm_mutex);
		return -EIO;
	}

	msg->timestamp = get_current_timestamp();
	memcpy(ssp_cmd_data, msg, SSP_MSG_HEADER_SIZE);
	if (msg->length > 0) {
		memcpy(&ssp_cmd_data[SSP_MSG_HEADER_SIZE], msg->buffer, msg->length);
	} else if (msg->length > (SSP_CMD_SIZE - SSP_MSG_HEADER_SIZE)) {
		ssp_errf("command size over !");
		mutex_unlock(&data->comm_mutex);
		return -EINVAL;
	}

	if (msg->done != NULL) {
		mutex_lock(&data->pending_mutex);
		list_add_tail(&msg->list, &data->pending_list);
		mutex_unlock(&data->pending_mutex);
	}

	status = sensorhub_comms_write(data, ssp_cmd_data, SSP_CMD_SIZE, timeout);

	if (status < 0 && msg->done != NULL) {
		ssp_errf("comm write fail!!");
		mutex_lock(&data->pending_mutex);
		list_del(&msg->list);
		mutex_unlock(&data->pending_mutex);

		goto exit;
	}

exit:
	mutex_unlock(&data->comm_mutex);
	if (status < 0) {
		is_ssp_shutdown = !is_sensorhub_working(data);
		data->cnt_com_fail += (is_ssp_shutdown)? 0 : 1;
		ssp_errf("cnt_com_fail %d , ssp_down %d ", data->cnt_com_fail, is_ssp_shutdown);
		return status;
	}

	if ((status >= 0) && (msg->done != NULL) && (timeout > 0)) {
		ret = wait_for_completion_timeout(msg->done,
		                                  msecs_to_jiffies(timeout));

		if (msg->clean_pending_list_flag) {
			ssp_errf("clean_pending_list_flag %d", msg->clean_pending_list_flag);
			msg->clean_pending_list_flag = 0;
			return -EINVAL;
		}

		/* when timeout happen */
		if (!ret) {
			msg->done = NULL;
			list_del(&msg->list);
			is_ssp_shutdown = !is_sensorhub_working(data);
			data->cnt_timeout += (is_ssp_shutdown)? 0 : 1;
			if (msg->done != NULL) {
				list_del(&msg->list);
			}

			ssp_errf("cnt_timeout %d, ssp_down %d !!",
			         data->cnt_timeout, is_ssp_shutdown);
			return -EINVAL;
		}
	}

	return status;
}



static void clean_msg(struct ssp_msg *msg)
{
	if (msg->buffer != NULL) {
		kfree(msg->buffer);
	}
	kfree(msg);
}

void clean_pending_list(struct ssp_data *data)
{
	struct ssp_msg *msg, *n;

	ssp_infof("");

	mutex_lock(&data->pending_mutex);
	list_for_each_entry_safe(msg, n, &data->pending_list, list) {

		list_del(&msg->list);
		if (msg->done != NULL && !completion_done(msg->done)) {
			msg->clean_pending_list_flag = 1;
			complete(msg->done);
		}
	}
	mutex_unlock(&data->pending_mutex);
}

int ssp_send_command(struct ssp_data *data, u8 cmd, u8 type, u8 subcmd,
                     int timeout, char *send_buf, int send_buf_len, char **receive_buf,
                     int *receive_buf_len)
{
	int status = 0;
	struct ssp_msg *msg;
	DECLARE_COMPLETION_ONSTACK(done);

	if ((type < SENSOR_TYPE_MAX) && !(data->sensor_probe_state& (1ULL << type))) {
		ssp_infof("Skip this function!, sensor is not connected(0x%llx)", data->sensor_probe_state);
		return -ENODEV;
	}
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = cmd;
	msg->type = type;
	msg->subcmd = subcmd;
	msg->length = send_buf_len;

	if (timeout > 0) {
		if (send_buf != NULL && send_buf_len != 0) {
			msg->buffer = kzalloc(send_buf_len, GFP_KERNEL);
			memcpy(msg->buffer, send_buf, send_buf_len);
		} else {
			msg->length = 0;
		}
		msg->done = &done;
	} else {
		if (send_buf != NULL && send_buf_len != 0) {
			msg->buffer = kzalloc(send_buf_len, GFP_KERNEL);
			memcpy(msg->buffer, send_buf, send_buf_len);
		} else {
			msg->length = 0;
		}
		msg->done = NULL;
	}

	ssp_infof("cmd %d type %d subcmd %d send_buf_len %d timeout %d", cmd, type,
	          subcmd, send_buf_len, timeout);

	if (do_transfer(data, msg, timeout) < 0) {
		ssp_errf("do_transfer error");
		status = ERROR;
	}

	//mutex_lock(&data->cmd_mutex);
	if (((msg->cmd == CMD_GETVALUE) && (receive_buf != NULL) &&
	     ((receive_buf_len != NULL) && (msg->length != 0))) &&
	    (status != ERROR)) {
		if (timeout > 0) {
			*receive_buf = kzalloc(msg->length, GFP_KERNEL);
			*receive_buf_len = msg->length;
			memcpy(*receive_buf, msg->buffer, msg->length);
		} else {
			ssp_errf("CMD_GETVALUE zero timeout");
			//mutex_unlock(&data->cmd_mutex);
			return -EINVAL;
		}
	}

	clean_msg(msg);
	//mutex_unlock(&data->cmd_mutex);

	if(status < 0) {
		reset_mcu(data, RESET_TYPE_KERNEL_COM_FAIL);
		ssp_errf("status=%d", status);
	}
	return status;
}

static void get_tm(struct rtc_time *tm)
{
	struct timespec ts;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, tm);
}

int enable_sensor(struct ssp_data *data, unsigned int type, u8* buf, int buf_len)
{
	int ret = 0;

	if (type != SENSOR_TYPE_SCONTEXT)
		ret = ssp_send_command(data, CMD_ADD, type, 0, 0, buf, buf_len, NULL, NULL);

	if (ret < 0) {
		ssp_errf("commnd error %d", ret);
	} else {
		data->en_info[type].enabled = true;
		data->en_info[type].regi_time.timestamp = get_current_timestamp();
		get_tm(&(data->en_info[type].regi_time.tm));
	}

	return ret;
}

int disable_sensor(struct ssp_data *data, unsigned int type, u8 *buf, int buf_len)
{
	int ret = 0;
	if (type != SENSOR_TYPE_SCONTEXT)
		ret = ssp_send_command(data, CMD_REMOVE, type, 0, 0, buf, buf_len, NULL, NULL);

	if (ret < 0) {
		ssp_errf("commnd error %d", ret);
	} else {
		data->en_info[type].enabled = false;
		data->en_info[type].unregi_time.timestamp = get_current_timestamp();
		get_tm(&(data->en_info[type].unregi_time.tm));
	}

	return ret;
}

static int convert_ap_status(int command)
{
	int ret = -1;
	switch (command) {
	case SCONTEXT_AP_STATUS_SHUTDOWN:
		ret = AP_SHUTDOWN;
		break;
	case SCONTEXT_AP_STATUS_WAKEUP:
		ret = LCD_ON;
		break;
	case SCONTEXT_AP_STATUS_SLEEP:
		ret = LCD_OFF;
		break;
	case SCONTEXT_AP_STATUS_RESUME:
		ret = AP_RESUME;
		break;
	case SCONTEXT_AP_STATUS_SUSPEND:
		ret = AP_SUSPEND;
		break;
#if 0
	case SCONTEXT_AP_STATUS_RESET:
		ret = AP_SHUTDOWN;
		break;
#endif
	case SCONTEXT_AP_STATUS_POW_CONNECTED:
		ret = POW_CONNECTED;
		break;
	case SCONTEXT_AP_STATUS_POW_DISCONNECTED:
		ret = POW_DISCONNECTED;
		break;
	case SCONTEXT_AP_STATUS_CALL_IDLE:
		ret = CALL_IDLE;
		break;
	case SCONTEXT_AP_STATUS_CALL_ACTIVE:
		ret = CALL_ACTIVE;
		break;
	}

	return ret;
}



int ssp_send_status(struct ssp_data *data, char command)
{
	int ret = 0;

	ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU, convert_ap_status(command),
	                       0, NULL, 0, NULL, NULL);
	if (ret != SUCCESS) {
		ssp_errf("command 0x%x failed %d", command, ret);
		return ERROR;
	}

	ssp_infof("command 0x%x", command);

	return SUCCESS;
}

