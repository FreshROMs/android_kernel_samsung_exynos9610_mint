/*
 * Copyright 2016 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

/*
 * tfa_ext_DUMMY.c
 *
 *	external DSP/RPC interface dummy for reference demo
 *
 *	The front-end is tfa_ext pre-fixed code which can be the same for all
 *	platforms.
 *	The remote_*() functions abstract the interface of the communication
 *	between tfa_ext and remote DSP platform. It's a raw buffer interchange.
 *	In dummy_dsp_*() the remote response is emulated.
 *	debugfs is available for generating remote events and kernel log will
 *	shows the behavior.
 *	Note that ftrace function trace could be used to trace the call flow.
 *
 *
 */
#define pr_fmt(fmt) "%s(): " fmt, __func__
#include "config.h"
#include "tfa_internal.h"
#include <sound/tfa_ext.h>

#include <linux/module.h>

#define TFA_EXT_DEBUGFS		/* enable the debugfs test interface */
#define RCV_BUFFER_SIZE 2048

/*
 * module globals
 */
static tfa_event_handler_t tfa_ext_handle_event;
static int remote_connect;

static struct mutex rcv_wait;	/* for blocking event read */
static struct work_struct event_work;	/* for event handler */
static struct workqueue_struct *tfa_ext_wq; /* work queue for event handler */

static u8 rcv_buffer[RCV_BUFFER_SIZE];
static int rcv_size;

/*
 * the trace_level parameter can be used to enabled full buffer payload prints
 */
static int trace_level;
module_param(trace_level, int, S_IRUGO);
MODULE_PARM_DESC(trace_level, "tfa_ext_dummy trace_level (0=off).");

#ifdef TFA_EXT_DEBUGFS
static int remote_rcv(u8 *buf, int length);
/*
 * debugfs for event testing
 *   /sys/kernel/debug/tfa_ext_dummy/event : event input in ascii
 *   refer to the tfa_ext_event_code() below
 *    1=dsp powered up
 *    2=cmd ready
 *    3=dsp powered down
 *   /sys/kernel/debug/tfa_ext_dummy/status: returns msg byte count
 *   in ascii since last event
 */
#include <linux/debugfs.h>
/* This directory entry will point to `/sys/kernel/debug/tfa_ext_dummy`.*/
static struct dentry *dir;
/* File `/sys/kernel/debug/tfa_ext_dummy/sum` points to this variable.*/
static u32 debugfs_status;

/*
 * This is called when `/sys/kernel/debug/tfa_ext_dummy/event` is written to.
 * Executing `echo 1 >> /sys/kernel/debug/tfa_ext_dummy/event` will call
 * `debugfs_event(NULL, 1)`.
*/

static int debugfs_event(void *data, u64 value)
{
	debugfs_status = 0;
	pr_info("%llx\n", value);

	memcpy(rcv_buffer, &value, sizeof(u64));
	rcv_size = sizeof(u64);
	if (trace_level > 0)
		print_hex_dump_bytes("debug event receive ", DUMP_PREFIX_NONE,
			rcv_buffer, rcv_size);
	/* emulated received event */
	if (mutex_is_locked(&rcv_wait))
		mutex_unlock(&rcv_wait);

	return queue_work(tfa_ext_wq, &event_work);

}

DEFINE_SIMPLE_ATTRIBUTE(add_fops, NULL, debugfs_event, "%llu\n");

/* This is called when the module loads.*/
static int debugfs_init_module(void)
{
	struct dentry *junk;

	/* Create directory `/sys/kernel/debug/tfa_ext_dummy`.*/
	dir = debugfs_create_dir("tfa_ext_dummy", 0);
	if (!dir) {
		/* Abort module load. */
		pr_err("debugfs_tfa_ext_dummy: failed to create /sys/kernel/debug/tfa_ext_dummy\n");
		return TFA_ERROR;
	}

	/* Create file `/sys/kernel/debug/tfa_ext_dummy/event`.*/
	junk = debugfs_create_file(
			"event",
			0222,
			dir,
			NULL,
			&add_fops);
	if (!junk) {
		/* Abort module load. */
		pr_err("debugfs_tfa_ext_dummy: failed to create /sys/kernel/debug/tfa_ext_dummy/event\n");
		return TFA_ERROR;
	}

	/* Create file `/sys/kernel/debug/tfa_ext_dummy/status`. */
	junk = debugfs_create_u32("status", 0444, dir, &debugfs_status);
	if (!junk) {
		pr_err("debugfs_tfa_ext_dummy: failed to create /sys/kernel/debug/tfa_ext_dummy/status\n");
		return TFA_ERROR;
	}

	return 0;
}

/* This is called when the module is removed.*/
static void debugfs_cleanup_module(void)
{
	debugfs_remove_recursive(dir);
}
/*
 * end debugfs
 */
#endif /*debugfs*/

/************************ start of the remote dummy tfadsp *******************/
/*
 * simple remote DSP behavior
 */
static int dummy_dsp_send(u8 *buf, int length)
{
	memcpy(rcv_buffer, buf, length);

	if (trace_level > 0)
		print_hex_dump_bytes("remote DSP sends: ", DUMP_PREFIX_NONE,
			rcv_buffer, rcv_size);
	/* emulated received event */
	if (mutex_is_locked(&rcv_wait))
		mutex_unlock(&rcv_wait);

	return queue_work(tfa_ext_wq, &event_work);

}

static int dummy_dsp_rcv(u8 *buf, int length)
{
	u8 ack_msg_buf[4] = {4/*TFADSP_CMD_ACK*/, 0, 0, 0};

	if (trace_level > 0)
		print_hex_dump_bytes("remote DSP received: ", DUMP_PREFIX_NONE,
				 buf, length);

	if (length == 2) /* ack */
		pr_info("ack:0x%02x%02x\n", buf[1], buf[0]); /* revid */
	else { /* assume msg */
		pr_info("cmd_id:0x%02x%02x%02x, length:%d\n",
					 buf[0], buf[1], buf[2], length);
		/* Check for the SetRe25 message as last message */
		if (buf[1] == 0x81 && buf[2] == 0x05)
			pr_info("----Last message received (SetRe25C)----\n");
		/* send ack */
		dummy_dsp_send(ack_msg_buf, sizeof(ack_msg_buf));
	}

	return length;
}
/************************ end of the remote dummy tfadsp *********************/

/*
 * remote interface wrappers
 */
static int remote_send(u8 *buf, int length)
{
	if (trace_level > 0)
		print_hex_dump_bytes("remote_send ", DUMP_PREFIX_NONE,
				 buf, length);
	debugfs_status += length;

	return dummy_dsp_rcv(buf, length); /* send to dsp */
}

static int remote_rcv(u8 *buf, int length)
{
	int size = rcv_size;

	memcpy(buf, rcv_buffer, rcv_size);
	if (trace_level > 0)
		print_hex_dump_bytes("remote_receive ", DUMP_PREFIX_NONE,
				 buf, length);

	return size;
}

static int remote_init(void)
{
	pr_info("\n");

	return 0;	/* 0 if ok */
}

static void remote_exit(void)
{
	pr_info("%s\n", __func__);
}

/*
 * remote tfa interfacing
 *
 *  TFADSP_EXT_PWRUP	: DSP starting
 *   set cold to receive messages
 *  TFADSP_CMD_READY	: Ready to receive commands
 *   call tfa_start to send msgs
 *	TFADSP_SPARSESIG_DETECTED :Sparse signal detected
 *   call sparse protection
 *	TFADSP_EXT_PWRDOWN	: DSP stopping
 *   disable DPS msg forwarding
 *
 */

static enum tfadsp_event_en tfa_ext_wait_event(void);
static int tfa_ext_event_ack(u16 err);

/*
 * return the event code from the raw input
 *   for this dummy case pick 1st byte
 */
static int tfa_ext_event_code(u8 *buf, int size)
{
	int code;

	switch (buf[0]) {
	case 1:
		pr_info("TFADSP_EXT_PWRUP\n");
		/**< DSP API has started, powered up */
		code = TFADSP_EXT_PWRUP;
		break;
	case 2:
		pr_info("TFADSP_CMD_READY\n");
		/**< Ready to receive commands */
		code = TFADSP_CMD_READY;
		break;
	case 3:
		pr_info("TFADSP_EXT_PWRDOWN\n");
		/**< DSP API stopped, power down */
		code = TFADSP_EXT_PWRDOWN;
		break;
	case 4:
		pr_info("TFADSP_CMD_ACK\n");
		/**< Sparse signal detected */
		code = TFADSP_CMD_ACK;
		break;
	case 5:
		pr_info("TFADSP_SPARSESIG_DETECTED\n");
		/**< Sparse signal detected */
		code = TFADSP_SPARSESIG_DETECTED;
		break;
	default:
		pr_info("not handled:%d\n", buf[0]);
		code = -1;
		break;
	}

	return code;
}

/*
 * return the response to an event
 *  the remote needs to handle this
 *  in this dummy a bare 16 byte value is used
 */
static int tfa_ext_event_ack(u16 response)
{
	u8 event_msg_buf[8];

	event_msg_buf[0] = response & 0xFF;
	event_msg_buf[1] = response >> 8;

	return remote_send(event_msg_buf, 2) == 2 ? 0 : 1;
}

/*
 * wait for an event from the remote controller
 */
static enum tfadsp_event_en tfa_ext_wait_event(void)
{
	u8 buf[16];
	int length;
	enum tfadsp_event_en this_dsp_event = 0;

	mutex_lock(&rcv_wait);

	length = remote_rcv(buf, sizeof(buf));
	if ((length) && (trace_level > 0)) {
		print_hex_dump_bytes("tfa_ext_wait_event ", DUMP_PREFIX_NONE,
					 buf, length);
	}

	if (length)	/* ignore if nothing */
		this_dsp_event = tfa_ext_event_code(buf, length);

	pr_debug("event:%d/0x%04x\n", this_dsp_event, this_dsp_event);

	return this_dsp_event;

}

/**
 * @brief DSP message interface that sends the RPC to the remote TFADSP
 *
 * This is the the function that get called by the tfa driver for each
 * RPC message that is to be send to the TFADSP.
 * The tfa_ext_registry() function will connect this.
 *
 * @param [in] devidx : tfa device index that owns the TFADSP
 * @param [in] length : length in bytes of the message in the buffer
 * @param [in] buffer : buffer pointer to the RPC message payload
 *
 * @return 0  success
 */
static int tfa_ext_dsp_msg(int devidx, int length, const char *buffer)
{

	int error = 0, real_length;
	u8 *buf = (u8 *) buffer;

	if (trace_level > 0)
		pr_debug("id:0x%02x%02x%02x, length:%d\n",
			 buf[0], buf[1], buf[2], length);

	pr_info("%s: send_pkt...\n", __func__);
	real_length = remote_send((u8 *) buffer, length);
	if (real_length != length) {
		pr_err("length mismatch: exp:%d, act:%d\n",
			   length, real_length);
		error = 6;	/* communication with the DSP failed */
	}
	if (tfa_ext_wait_event() == TFADSP_CMD_ACK) {
		tfa_ext_event_ack(TFADSP_CMD_ACK);
	} else {
		pr_err("no TFADSP_CMD_ACK event received\n");
		tfa_ext_event_ack(0);
		error = 6;	/* communication with the DSP failed */
	}
	return error;
}

/**
 * @brief Register at tfa driver and instantiate remote interface functions.
 *
 * This function must be called once at startup after tfa driver module is
 * loaded.
 * The tfa_ext_register() will be called to get event handler and dsp message
 * interface functions and remote TFADSP will be connected after successful
 * return.
 *
 * @param void
 *
 * @return 0 on success
 */
static int tfa_ext_registry(void)
{
	int ret = -1;

	pr_debug("%s\n", __func__);

	if (tfa_ext_register
		(NULL, tfa_ext_dsp_msg, NULL, &tfa_ext_handle_event)) {
		pr_err("Cannot register to tfa driver!\n");
		return 1;
	}
	if (tfa_ext_handle_event == NULL) {
		pr_err("even callback not registered by tfa driver!\n");
		return 1;
	}
	if (remote_connect == 0) {
		remote_connect = (remote_init() == 0);
		if (remote_connect == 0)
			pr_err("remote_init failed\n");
		else
			ret = 0;
	}

	return remote_connect == 0;
}

/**
 * @brief Un-register and close the remote interface.
 *
 * This function must be called once at shutdown of the remote device.
 *
 * @param void
 *
 * @return 0 on success
 */
static void tfa_ext_unregister(void)
{
	pr_debug("%s\n", __func__);

	/* in case remote did not do send shutdown */
	tfa_ext_handle_event(0, TFADSP_EXT_PWRDOWN);

	if (remote_connect)
		remote_exit();

	remote_connect = 0;

}

/****************************** event handler *****************************/
/*
 * This event handler will be on a work queue and is normally blocking on an
 * event read.
 * Whenever a remote event comes in it will retrieve the event code, handles
 * it and/or passes it on to the tfa98xx driver.
 *
 * Note that the ack needs to be returned in response of TFADSP_CMD_READY
 * before the messages ar started.
 */
static void tfa_ext_event_handler(struct work_struct *work)
{
	enum tfadsp_event_en event;
	int event_return = -1;

	pr_info("\n");

	if (tfa_ext_handle_event == NULL)
		return; /* -EFAULT; */

	event = tfa_ext_wait_event();
	if (event < 0) {
		pr_info("illegal event %d\n", event);
		return;
	}

	if (event == TFADSP_CMD_READY) {
		/* Ack this first, next will be the tfadsp messages loop */
		tfa_ext_event_ack(1);
	}

	/* call the tfa driver here to handle this event */
	event_return = tfa_ext_handle_event(0, event);
	if (event_return < 0)
		return;

	tfa_ext_event_ack(event_return);
}

/*
 * register at tfa98xx and create the work queue for the events
 */
static int __init tfa_ext_dummy_init(void)
{
	/* pr_info("build: %s %s\n", __DATE__, __TIME__); */
	pr_info("trace_level:%d\n", trace_level);

	debugfs_init_module();
	tfa_ext_registry();	/* TODO add error check */

	mutex_init(&rcv_wait);

	/* setup work queue  */
	tfa_ext_wq = create_singlethread_workqueue("tfa_ext");
	if (!tfa_ext_wq)
		return -ENOMEM;
	INIT_WORK(&event_work, tfa_ext_event_handler);

	pr_info("done\n");
	return 0;

}
/*
 * cleanup and disappear
 */
static void __exit tfa_ext_dummy_exit(void)
{
	mutex_unlock(&rcv_wait);

	cancel_work_sync(&event_work);
	if (tfa_ext_wq)
		destroy_workqueue(tfa_ext_wq);
	remote_exit();
	mutex_destroy(&rcv_wait);
	tfa_ext_unregister();
	debugfs_cleanup_module();
	pr_info("done\n");
}

module_init(tfa_ext_dummy_init);
module_exit(tfa_ext_dummy_exit);

MODULE_DESCRIPTION("TFA98xx remote dummy DSP driver");
MODULE_LICENSE("GPL");
