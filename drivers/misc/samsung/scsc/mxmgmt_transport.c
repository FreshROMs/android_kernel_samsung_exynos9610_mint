/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**
 * Maxwell management transport (implementation)
 */

/** Implements */
#include "mxmgmt_transport.h"

/** Uses */
#include <scsc/scsc_logring.h>
#include <linux/module.h>
#include "mxmgmt_transport_format.h"
#include "mifintrbit.h"

/* Flag that an error has occurred so the I/O thread processing should stop */
void mxmgmt_transport_set_error(struct mxmgmt_transport *mxmgmt_transport)
{
	SCSC_TAG_WARNING(MXMGT_TRANS, "I/O thread processing is suspended\n");

	mxmgmt_transport->mxmgmt_thread.block_thread = 1;
}

/** MIF Interrupt handler for writes made to the AP */
static void input_irq_handler(int irq, void *data)
{
	struct mxmgmt_transport *mxmgmt_transport = (struct mxmgmt_transport *)data;
	struct mxmgmt_thread    *th = &mxmgmt_transport->mxmgmt_thread;
	struct scsc_mif_abs     *mif_abs;

	SCSC_TAG_DEBUG(MXMGT_TRANS, "IN\n");
	/* Clear the interrupt first to ensure we can't possibly miss one */
	mif_abs = scsc_mx_get_mif_abs(mxmgmt_transport->mx);
	mif_abs->irq_bit_clear(mif_abs, irq);

	/* The the other side wrote some data to the input stream, wake up the thread
	 * that deals with this. */
	if (th->task == NULL) {
		SCSC_TAG_ERR(MXMGT_TRANS, "th is NOT running\n");
		return;
	}
	/*
	 * If an error has occured, we discard silently all messages from the stream
	 * until the error has been processed and the system has been reinitialised.
	 */
	if (th->block_thread == 1) {
		SCSC_TAG_DEBUG(MXMGT_TRANS, "discard message.\n");
		/*
		 * Do not try to acknowledge a pending interrupt here.
		 * This function is called by a function which in turn can be
		 * running in an atomic or 'disabled irq' level.
		 */
		return;
	}
	th->wakeup_flag = 1;

	/* wake up I/O thread */
	wake_up_interruptible(&th->wakeup_q);
}

/** MIF Interrupt handler for acknowledging writes made by the AP */
static void output_irq_handler(int irq, void *data)
{
	struct scsc_mif_abs     *mif_abs;
	struct mxmgmt_transport *mxmgmt_transport = (struct mxmgmt_transport *)data;

	SCSC_TAG_DEBUG(MXMGT_TRANS, "OUT\n");

	/* Clear the interrupt first to ensure we can't possibly miss one */
	/* The FW read some data from the output stream.
	 * Currently we do not care, so just clear the interrupt. */
	mif_abs = scsc_mx_get_mif_abs(mxmgmt_transport->mx);
	mif_abs->irq_bit_clear(mif_abs, irq);

	/* The driver doesn't use the ack IRQ, so mask it from now on,
	 * otherwise we may get spurious host-wakes.
	 */
	mif_abs->irq_bit_mask(mif_abs, irq);
}


static void thread_wait_until_stopped(struct mxmgmt_transport *mxmgmt_transport)
{
	struct mxmgmt_thread *th = &mxmgmt_transport->mxmgmt_thread;

	/*
	 * kthread_stop() cannot handle the th exiting while
	 * kthread_should_stop() is false, so sleep until kthread_stop()
	 * wakes us up.
	 */
	SCSC_TAG_DEBUG(MXMGT_TRANS, "%s waiting for the stop signal.\n", th->name);
	set_current_state(TASK_INTERRUPTIBLE);
	if (!kthread_should_stop()) {
		SCSC_TAG_DEBUG(MXMGT_TRANS, "%s schedule....\n", th->name);
		schedule();
	}

	th->task = NULL;
	SCSC_TAG_DEBUG(MXMGT_TRANS, "%s exiting....\n", th->name);
}

/**
 * A thread that forwards messages sent across the transport to
 * the registered handlers for each channel.
 */
static int mxmgmt_thread_function(void *arg)
{
	struct mxmgmt_transport    *mxmgmt_transport = (struct mxmgmt_transport *)arg;
	struct mxmgmt_thread       *th = &mxmgmt_transport->mxmgmt_thread;
	const struct mxmgr_message *current_message;
	int                        ret;

	complete(&th->completion);

	th->block_thread = 0;
	while (!kthread_should_stop()) {
		/* wait until an error occurs, or we need to process something. */

		ret = wait_event_interruptible(th->wakeup_q,
					       (th->wakeup_flag && !th->block_thread) ||
					       kthread_should_stop());

		if (kthread_should_stop()) {
			SCSC_TAG_DEBUG(MXMGT_TRANS, "signalled to exit\n");
			break;
		}
		if (ret < 0) {
			SCSC_TAG_DEBUG(MXMGT_TRANS, "wait_event returned %d, thread will exit\n", ret);
			thread_wait_until_stopped(mxmgmt_transport);
			break;
		}
		th->wakeup_flag = 0;
		SCSC_TAG_DEBUG(MXMGT_TRANS, "wokeup: r=%d\n", ret);
		/* Forward each pending message to the applicable channel handler */
		current_message = mif_stream_peek(&mxmgmt_transport->mif_istream, NULL);
		while (current_message != NULL) {
			mutex_lock(&mxmgmt_transport->channel_handler_mutex);
			if (current_message->channel_id < MMTRANS_NUM_CHANNELS &&
			    mxmgmt_transport->channel_handler_fns[current_message->channel_id]) {
				SCSC_TAG_DEBUG(MXMGT_TRANS, "Calling handler for channel_id: %d\n", current_message->channel_id);
				(*mxmgmt_transport->channel_handler_fns[current_message->channel_id])(current_message->payload,
												      mxmgmt_transport->channel_handler_data[current_message->channel_id]);
			} else
				/* HERE: Invalid channel or no handler, raise fault or log message */
				SCSC_TAG_WARNING(MXMGT_TRANS, "Invalid channel or no handler channel_id: %d\n", current_message->channel_id);
			mutex_unlock(&mxmgmt_transport->channel_handler_mutex);
			/* Remove the current message from the buffer before processing the next
			 * one in case it generated another message, otherwise it's possible we
			 * could run out of space in the stream before we get through all the messages. */
			mif_stream_peek_complete(&mxmgmt_transport->mif_istream, current_message);
			current_message = mif_stream_peek(&mxmgmt_transport->mif_istream, NULL);
		}
	}

	SCSC_TAG_DEBUG(MXMGT_TRANS, "exiting....\n");
	complete(&th->completion);
	return 0;
}


static int mxmgmt_thread_start(struct mxmgmt_transport *mxmgmt_transport)
{
	int                  err;
	struct mxmgmt_thread *th = &mxmgmt_transport->mxmgmt_thread;

	if (th->task != NULL) {
		SCSC_TAG_WARNING(MXMGT_TRANS, "%s thread already started\n", th->name);
		return 0;
	}

	/* Initialise thread structure */
	th->block_thread = 1;
	init_waitqueue_head(&th->wakeup_q);
	init_completion(&th->completion);
	th->wakeup_flag = 0;
	snprintf(th->name, MXMGMT_THREAD_NAME_MAX_LENGTH, "mxmgmt_thread");

	/* Start the kernel thread */
	th->task = kthread_run(mxmgmt_thread_function, mxmgmt_transport, "%s", th->name);
	if (IS_ERR(th->task)) {
		SCSC_TAG_ERR(MXMGT_TRANS, "error creating kthread\n");
		return (int)PTR_ERR(th->task);
	}

	SCSC_TAG_DEBUG(MXMGT_TRANS, "Started thread %s\n", th->name);

	/* wait until thread is started */
#define MGMT_THREAD_START_TMO_SEC   (3)
	err = wait_for_completion_timeout(&th->completion, msecs_to_jiffies(MGMT_THREAD_START_TMO_SEC*1000));
	if (err == 0) {
		SCSC_TAG_ERR(MXMGT_TRANS, "timeout in starting thread\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static void mgmt_thread_stop(struct mxmgmt_transport *mxmgmt_transport)
{
	unsigned long left_jiffies;
	struct  mxmgmt_thread *th = &mxmgmt_transport->mxmgmt_thread;

	if (!th->task) {
		SCSC_TAG_WARNING(MXMGT_TRANS, "%s mgmt_thread is already stopped\n", th->name);
		return;
	}
	SCSC_TAG_DEBUG(MXMGT_TRANS, "Stopping %s mgmt_thread\n", th->name);
	kthread_stop(th->task);
	/* wait until th stopped */
#define MGMT_THREAD_STOP_TMO_SEC   (3)
	left_jiffies =
		wait_for_completion_timeout(&th->completion, msecs_to_jiffies(MGMT_THREAD_STOP_TMO_SEC*1000));
	if (!left_jiffies)
		SCSC_TAG_ERR(MXMGT_TRANS, "Failed to stop mgmt_thread %s\n",
			     th->name);
	else
		th->task = NULL;
}

void mxmgmt_transport_release(struct mxmgmt_transport *mxmgmt_transport)
{
	mgmt_thread_stop(mxmgmt_transport);
	mif_stream_release(&mxmgmt_transport->mif_istream);
	mif_stream_release(&mxmgmt_transport->mif_ostream);
}

void mxmgmt_transport_config_serialise(struct mxmgmt_transport *mxmgmt_transport,
				       struct mxtransconf      *trans_conf)
{
	mif_stream_config_serialise(&mxmgmt_transport->mif_istream, &trans_conf->to_ap_stream_conf);
	mif_stream_config_serialise(&mxmgmt_transport->mif_ostream, &trans_conf->from_ap_stream_conf);
}


/** Public functions */
int mxmgmt_transport_init(struct mxmgmt_transport *mxmgmt_transport, struct scsc_mx *mx)
{
#define MEM_LENGTH 512
	int      r;
	uint32_t mem_length = MEM_LENGTH;
	uint32_t packet_size = sizeof(struct mxmgr_message);
	uint32_t num_packets;


	/*
	 * Initialising a buffer of 1 byte is never legitimate, do not allow it.
	 * The memory buffer length must be a multiple of the packet size.
	 */
	if (mem_length <= 1 || mem_length % packet_size != 0)
		return -EIO;
	memset(mxmgmt_transport, 0, sizeof(struct mxmgmt_transport));
	num_packets = mem_length / packet_size;
	mutex_init(&mxmgmt_transport->channel_handler_mutex);
	mxmgmt_transport->mx = mx;
	r = mif_stream_init(&mxmgmt_transport->mif_istream, SCSC_MIF_ABS_TARGET_R4, MIF_STREAM_DIRECTION_IN, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, input_irq_handler, mxmgmt_transport);
	if (r) {
		SCSC_TAG_ERR(MXMGT_TRANS, "mif_stream_init IN failed %d\n", r);
		return r;
	}
	r = mif_stream_init(&mxmgmt_transport->mif_ostream, SCSC_MIF_ABS_TARGET_R4, MIF_STREAM_DIRECTION_OUT, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, output_irq_handler, mxmgmt_transport);
	if (r) {
		SCSC_TAG_ERR(MXMGT_TRANS, "mif_stream_init OUT failed %d\n", r);
		mif_stream_release(&mxmgmt_transport->mif_istream);
		return r;
	}

	r = mxmgmt_thread_start(mxmgmt_transport);
	if (r) {
		SCSC_TAG_ERR(MXMGT_TRANS, "mxmgmt_thread_start failed %d\n", r);
		mif_stream_release(&mxmgmt_transport->mif_istream);
		mif_stream_release(&mxmgmt_transport->mif_ostream);
		return r;
	}
	return 0;
}

void mxmgmt_transport_register_channel_handler(struct mxmgmt_transport *mxmgmt_transport, enum mxmgr_channels channel_id,
					       mxmgmt_channel_handler handler, void *data)
{
	if (channel_id >= MMTRANS_NUM_CHANNELS) {
		SCSC_TAG_ERR(MXMGT_TRANS, "Invalid channel id: %d\n", channel_id);
		return;
	}
	mutex_lock(&mxmgmt_transport->channel_handler_mutex);
	mxmgmt_transport->channel_handler_fns[channel_id] = handler;
	mxmgmt_transport->channel_handler_data[channel_id] = data;
	mutex_unlock(&mxmgmt_transport->channel_handler_mutex);
}

void mxmgmt_transport_send(struct mxmgmt_transport *mxmgmt_transport, enum mxmgr_channels channel_id,
			   void *message, uint32_t message_length)
{
	struct mxmgr_message transport_msg = { .channel_id = channel_id };
	bool res;

	const void           *bufs[2] = { &transport_msg.channel_id, message };
	uint32_t             buf_lengths[2] = { sizeof(transport_msg.channel_id), message_length };

	res = mif_stream_write_gather(&mxmgmt_transport->mif_ostream, bufs, buf_lengths, 2);

	if (res == false)
		SCSC_TAG_ERR(MXMGT_TRANS, "mif_stream_write_gather message error. Channel %d message_len %u\n",
					  channel_id, message_length);
}
