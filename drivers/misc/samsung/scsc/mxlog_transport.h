/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**
 * Maxwell mxlog transport (Interface)
 *
 * Provides communication between the firmware and the host.
 *
 */

#ifndef MXLOG_TRANSPORT_H__
#define MXLOG_TRANSPORT_H__

/** Uses */
#include <linux/kthread.h>
#include "mifstream.h"

struct mxlog_transport;

typedef int (*mxlog_header_handler)(u32 header, u8 *phase,
				     u8 *level, u32 *num_bytes);
/**
 * Transport channel callback handler. This will be invoked each time a message on a channel is
 * received. Handlers may perform work within their callback implementation, but should not block.
 * The detected phase is passed as first parameter.
 *
 * Note that the message pointer passed is only valid for the duration of the function call.
 */
typedef void (*mxlog_channel_handler)(u8 phase, const void *message,
				      size_t length, u32 level, void *data);

/**
 * Initialises the maxwell management transport and configures the necessary
 * interrupt handlers.
 */
int mxlog_transport_init(struct mxlog_transport *mxlog_transport, struct scsc_mx *mx);
void mxlog_transport_release(struct mxlog_transport *mxlog_transport);
/*
 *  Initialises the configuration area incl. Maxwell Infrastructure Configuration,
 * MIF Management Transport Configuration and  MIF Management Stream Configuration.
 */
void mxlog_transport_config_serialise(struct mxlog_transport *mxlog_transport, struct mxlogconf *mxlogconf);
void mxlog_transport_register_channel_handler(struct mxlog_transport *mxlog_transport,
					      mxlog_header_handler parser,
					      mxlog_channel_handler handler,
					      void *data);
void mxlog_transport_set_error(struct mxlog_transport *mxlog_transport);

#define MXLOG_THREAD_NAME_MAX_LENGTH 32
struct mxlog_thread {
	struct task_struct *task;
	char               name[MXLOG_THREAD_NAME_MAX_LENGTH];
	int                prio;
	struct completion  completion;
	wait_queue_head_t  wakeup_q;
	unsigned int       wakeup_flag;
	/*
	 * Use it to block the I/O thread when
	 * an error occurs.
	 */
	int                block_thread;
};

struct mxlog_transport {
	struct scsc_mx          *mx;
	struct mxlog_thread     mxlog_thread;
	struct mif_stream       mif_stream;
	mxlog_header_handler    header_handler_fn;
	mxlog_channel_handler   channel_handler_fn;
	void                    *channel_handler_data;
	struct mutex            lock;
};

#endif /* MXLOG_TRANSPORT_H__ */
