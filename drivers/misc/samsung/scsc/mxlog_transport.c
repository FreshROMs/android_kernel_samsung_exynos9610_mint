/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
/** Uses */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"
#include "mifintrbit.h"
/** Implements */
#include "mxlog_transport.h"

#define MXLOG_TRANSPORT_BUF_LENGTH (16 * 1024)
#define MXLOG_TRANSPORT_PACKET_SIZE (4)
#define MXLOG_TRANSPORT_MAX_BAD_SYNC_HEADER_CNT (250)

/* Flag that an error has occurred so the I/O thread processing should stop */
void mxlog_transport_set_error(struct mxlog_transport *mxlog_transport)
{
	SCSC_TAG_WARNING(MXLOG_TRANS, "I/O thread processing is suspended\n");

	mxlog_transport->mxlog_thread.block_thread = 1;
}

static void input_irq_handler(int irq, void *data)
{
	struct mxlog_transport *mxlog_transport = (struct mxlog_transport *)data;
	struct mxlog_thread    *th = &mxlog_transport->mxlog_thread;
	struct scsc_mif_abs     *mif_abs;

	SCSC_TAG_DEBUG(MXLOG_TRANS, "mxlog intr\n");
	/* Clear the interrupt first to ensure we can't possibly miss one */
	mif_abs = scsc_mx_get_mif_abs(mxlog_transport->mx);
	mif_abs->irq_bit_clear(mif_abs, irq);

	/* The the other side wrote some data to the input stream,
	 * wake up the thread that deals with this.
	 */
	if (th->task == NULL) {
		SCSC_TAG_ERR(MXLOG_TRANS, "mxlog_thread is NOT running\n");
		return;
	}
	/*
	 * If an error has occured, we discard silently all messages from
	 * the stream until the error has been processed and the system has
	 * been reinitialised.
	 */
	if (th->block_thread == 1) {
		SCSC_TAG_DEBUG(MXLOG_TRANS, "discard message.\n");
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

static void thread_wait_until_stopped(struct mxlog_transport *mxlog_transport)
{
	struct mxlog_thread *th = &mxlog_transport->mxlog_thread;

	/*
	 * kthread_stop() cannot handle the th exiting while
	 * kthread_should_stop() is false, so sleep until kthread_stop()
	 * wakes us up.
	 */
	SCSC_TAG_INFO(MXLOG_TRANS, "%s waiting for the stop signal.\n", th->name);
	set_current_state(TASK_INTERRUPTIBLE);
	if (!kthread_should_stop()) {
		SCSC_TAG_DEBUG(MXLOG_TRANS, "%s schedule....\n", th->name);
		schedule();
	}

	/**
	 * Caller that spawned the kthread did a get_task_struct()
	 * on task_struct, that will be released on stop...
	 * ...so we should NOT nullify th->task here.
	 */
	SCSC_TAG_DEBUG(MXLOG_TRANS, "%s exiting.\n", th->name);
}

/**
 * A thread that forwards messages sent across the transport to
 * the registered handlers for each channel.
 */
static int mxlog_thread_function(void *arg)
{
	struct mxlog_transport    *mxlog_transport = (struct mxlog_transport *)arg;
	struct mxlog_thread       *th = &mxlog_transport->mxlog_thread;
	int                        ret;
	u32                        header;
	char			   *buf = NULL;
	size_t			   buf_sz = 4096;
	/*
	 * Do not die on first invalid packet header check atleast for MXLOG_TRANSPORT_MAX_BAD_SYNC_HEADER_CNT
	 * consecutive invalid packet head
	 */
	u32			   invld_hdr_cnt = 0;

	buf = kmalloc(buf_sz, GFP_KERNEL);
	if (!buf) {
		SCSC_TAG_ERR(MXLOG_TRANS, "Failed to alloc %s local buffer...exiting.\n", th->name);
		return -ENOMEM;
	}
	/* completion is used only for startup thread-synchronization */
	complete(&th->completion);
	/* Thread ready...start ISR processing*/
	th->block_thread = 0;
	while (!kthread_should_stop()) {
		/* wait until an error occurs, or we need to process */
		ret = wait_event_interruptible(th->wakeup_q,
					       (th->wakeup_flag && !th->block_thread) ||
					       kthread_should_stop());

		if (kthread_should_stop()) {
			SCSC_TAG_DEBUG(MXLOG_TRANS, "signalled to exit\n");
			break;
		}
		if (ret < 0) {
			SCSC_TAG_DEBUG(MXLOG_TRANS,
				       "wait_event returned %d, thread will exit\n", ret);
			thread_wait_until_stopped(mxlog_transport);
			break;
		}
		th->wakeup_flag = 0;
		SCSC_TAG_DEBUG(MXLOG_TRANS, "wokeup: r=%d\n", ret);
		if (!mxlog_transport->header_handler_fn) {
			/* Invalid header handler:
			 * unrecoverable log and terminate
			 */
			SCSC_TAG_WARNING(MXLOG_TRANS,
					 "mxlog_transport->header_handler_fn_==NULL\n");
			break;
		}
		while (mif_stream_read(&mxlog_transport->mif_stream,
				       &header, sizeof(uint32_t))) {
			u8 level = 0;
			u8 phase = 0;
			u32 num_bytes = 0;

			mutex_lock(&mxlog_transport->lock);
			if (!mxlog_transport->header_handler_fn) {
				/* Invalid header handler:
				 * unrecoverable log and terminate
				 */
				SCSC_TAG_WARNING(MXLOG_TRANS,
						 "mxlog_transport->header_handler_fn_==NULL. Channel has been released\n");
				mutex_unlock(&mxlog_transport->lock);
				/* not recoverable, terminate straight away */
				goto mxlog_thread_exit;
			}
			/**
			 * A generic header processor will properly retrieve
			 * level and num_bytes as specifically implemented
			 * by the phase.
			 */
			if (mxlog_transport->header_handler_fn(header, &phase,
							       &level, &num_bytes)) {
				SCSC_TAG_ERR(MXLOG_TRANS,
					     "Bad sync in header: header=0x%08x\n", header);

				if (++invld_hdr_cnt > MXLOG_TRANSPORT_MAX_BAD_SYNC_HEADER_CNT) {
					SCSC_TAG_ERR(MXLOG_TRANS, "Received %d continuous bad sync headers. Terminating ...\n", MXLOG_TRANSPORT_MAX_BAD_SYNC_HEADER_CNT);
					mutex_unlock(&mxlog_transport->lock);
					/* not recoverable, terminate straight away */
					goto mxlog_thread_exit;
				}
				mutex_unlock(&mxlog_transport->lock);
				continue;
			}
			if (num_bytes > 0 &&
			    num_bytes < (MXLOG_TRANSPORT_BUF_LENGTH - sizeof(uint32_t))) {
				u32	ret_bytes = 0;

				/* We have received valid packets reset invalid header count */
				invld_hdr_cnt = 0;

				/* 2nd read - payload (msg) */
				ret_bytes = mif_stream_read(&mxlog_transport->mif_stream,
							    buf, num_bytes);
				mxlog_transport->channel_handler_fn(phase, buf,
								    ret_bytes,
								    level,
								    mxlog_transport->channel_handler_data);
			} else {
				SCSC_TAG_ERR(MXLOG_TRANS,
					     "Bad num_bytes(%d) in header: header=0x%08x\n",
					     num_bytes, header);
			}
			mutex_unlock(&mxlog_transport->lock);
		}
	}

mxlog_thread_exit:
	SCSC_TAG_INFO(MXLOG_TRANS, "exiting....\n");
	kfree(buf);
	return 0;
}

static int mxlog_thread_start(struct mxlog_transport *mxlog_transport)
{
	int                  err;
	struct mxlog_thread *th = &mxlog_transport->mxlog_thread;

	if (th->task != NULL) {
		SCSC_TAG_WARNING(MXLOG_TRANS, "%s thread already started\n", th->name);
		return 0;
	}

	/* Initialise thread structure */
	th->block_thread = 1;
	init_waitqueue_head(&th->wakeup_q);
	init_completion(&th->completion);
	th->wakeup_flag = 0;
	snprintf(th->name, MXLOG_THREAD_NAME_MAX_LENGTH, "mxlog_thread");

	/* Start the kernel thread */
	th->task = kthread_run(mxlog_thread_function, mxlog_transport, "%s", th->name);
	if (IS_ERR(th->task)) {
		int err = (int)PTR_ERR(th->task);

		th->task = NULL;
		return err;
	}

	/**
	 * Avoid this task_struct vanishes immediately
	 * if the kthread exits by its own.
	 */
	get_task_struct(th->task);

	/* wait until thread function is running */
#define LOG_THREAD_START_TMO_SEC   (3)
	err = wait_for_completion_timeout(&th->completion, msecs_to_jiffies(LOG_THREAD_START_TMO_SEC * 1000));
	if (err == 0) {
		SCSC_TAG_ERR(MXLOG_TRANS, "timeout starting %s\n", th->name);
		kthread_stop(th->task);
		put_task_struct(th->task);
		return -ETIMEDOUT;
	}
	SCSC_TAG_INFO(MXLOG_TRANS, "Started thread %s\n", th->name);

	return 0;
}

static void mxlog_thread_stop(struct mxlog_transport *mxlog_transport)
{
	struct  mxlog_thread *th = &mxlog_transport->mxlog_thread;

	if (!th->task) {
		SCSC_TAG_WARNING(MXLOG_TRANS, "%s is already stopped\n", th->name);
		return;
	}
	SCSC_TAG_INFO(MXLOG_TRANS, "Stopping thread %s [%d]\n", th->name, th->task->pid);
	/* kthread_stop() marks thread as KTHREAD_SHOULD_STOP
	 * and wait for it to terminate
	 */
	if (kthread_stop(th->task))
		SCSC_TAG_ERR(MXLOG_TRANS, "Failed to stop %s [%d]\n", th->name, th->task->pid);
	/* Finally release the task_struct we held on start */
	put_task_struct(th->task);
	th->task = NULL;
}

void mxlog_transport_release(struct mxlog_transport *mxlog_transport)
{
	mxlog_thread_stop(mxlog_transport);
	mif_stream_release(&mxlog_transport->mif_stream);
}

void mxlog_transport_config_serialise(struct mxlog_transport *mxlog_transport,
				    struct mxlogconf   *mxlogconf)
{
	mif_stream_config_serialise(&mxlog_transport->mif_stream, &mxlogconf->stream_conf);
}

/** Public functions */
int mxlog_transport_init(struct mxlog_transport *mxlog_transport, struct scsc_mx *mx)
{
	int                       r;
	uint32_t                  mem_length = MXLOG_TRANSPORT_BUF_LENGTH;
	uint32_t                  packet_size = MXLOG_TRANSPORT_PACKET_SIZE;
	uint32_t                  num_packets;

	/*
	 * Initialising a buffer of 1 byte is never legitimate, do not allow it.
	 * The memory buffer length must be a multiple of the packet size.
	 */

	memset(mxlog_transport, 0, sizeof(struct mxlog_transport));
	mutex_init(&mxlog_transport->lock);
	num_packets = mem_length / packet_size;
	mxlog_transport->mx = mx;
	r = mif_stream_init(&mxlog_transport->mif_stream, SCSC_MIF_ABS_TARGET_R4, MIF_STREAM_DIRECTION_IN, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, input_irq_handler, mxlog_transport);
	if (r)
		return r;
	r = mxlog_thread_start(mxlog_transport);
	if (r) {
		mif_stream_release(&mxlog_transport->mif_stream);
		return r;
	}

	return 0;
}

void mxlog_transport_register_channel_handler(struct mxlog_transport *mxlog_transport,
					      mxlog_header_handler parser,
					      mxlog_channel_handler handler,
					      void *data)
{
	mutex_lock(&mxlog_transport->lock);
	mxlog_transport->header_handler_fn = parser;
	mxlog_transport->channel_handler_fn = handler;
	mxlog_transport->channel_handler_data = (void *)data;
	mutex_unlock(&mxlog_transport->lock);
}
