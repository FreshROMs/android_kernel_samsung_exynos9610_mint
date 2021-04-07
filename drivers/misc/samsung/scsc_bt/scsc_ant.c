/****************************************************************************
 *
 *       Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

/* MX BT shared memory interface */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <asm/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/api/bsmhcp.h>
#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"
#include "scsc_shm.h"
#include "scsc_bt_hci.h"

static u8   ant_write_buffer[ASMHCP_BUFFER_SIZE];
static u16  ant_irq_mask;

static void scsc_ant_shm_irq_handler(int irqbit, void *data)
{
	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(ant_service.service, irqbit);

	ant_service.interrupt_count++;

	/* Wake the reader operation */
	if (ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_write !=
	    ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_read ||
	    ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_write !=
	    ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_read ||
	    atomic_read(&ant_service.error_count) != 0 ||
	    ant_service.asmhcp_protocol->header.panic_deathbed_confession) {
		ant_service.interrupt_read_count++;

		wake_lock_timeout(&ant_service.read_wake_lock, HZ);
		wake_up(&ant_service.read_wait);
	}

	if (ant_service.asmhcp_protocol->header.mailbox_data_driv_ctr_write ==
	    ant_service.asmhcp_protocol->header.mailbox_data_driv_ctr_read &&
	    ant_service.asmhcp_protocol->header.mailbox_cmd_driv_ctr_write ==
	    ant_service.asmhcp_protocol->header.mailbox_cmd_driv_ctr_read) {
		ant_service.interrupt_write_count++;

		if (wake_lock_active(&ant_service.write_wake_lock)) {
			ant_service.write_wake_unlock_count++;
			wake_unlock(&ant_service.write_wake_lock);
		}
	}
}

/* Assign firmware/host interrupts */
static int scsc_ant_shm_init_interrupt(void)
{
	int irq_ret = 0;
	u16 irq_num = 0;

	/* To-host f/w IRQ allocations and ISR registrations */
	irq_ret = scsc_service_mifintrbit_register_tohost(
	    ant_service.service, scsc_ant_shm_irq_handler, NULL);
	if (irq_ret < 0)
		return irq_ret;

	ant_service.asmhcp_protocol->header.bg_to_ap_int_src = irq_ret;
	ant_irq_mask |= 1 << irq_num++;

	/* From-host f/w IRQ allocations */
	irq_ret = scsc_service_mifintrbit_alloc_fromhost(
	    ant_service.service, SCSC_MIFINTR_TARGET_R4);
	if (irq_ret < 0)
		return irq_ret;

	ant_service.asmhcp_protocol->header.ap_to_bg_int_src = irq_ret;
	ant_irq_mask |= 1 << irq_num++;

	SCSC_TAG_DEBUG(BT_COMMON, "Registered to-host IRQ bit %d, from-host IRQ bit %d\n",
		       ant_service.asmhcp_protocol->header.bg_to_ap_int_src,
		       ant_service.asmhcp_protocol->header.ap_to_bg_int_src);

	return 0;
}

static ssize_t scsc_shm_ant_cmd_write(const unsigned char *data, size_t count)
{
	/* Store the read/write pointer on the stack since both are placed in unbuffered/uncached memory */
	uint32_t tr_read = ant_service.asmhcp_protocol->header.mailbox_cmd_driv_ctr_read;
	uint32_t tr_write = ant_service.asmhcp_protocol->header.mailbox_cmd_driv_ctr_write;

	struct ASMHCP_TD_CONTROL *td = &ant_service.asmhcp_protocol->cmd_driver_controller_transfer_ring[tr_write];
	/* Temp vars */
	SCSC_TAG_DEBUG(BT_H4, "ANT_COMMAND_PKT (len=%zu, read=%u, write=%u)\n",
		count, tr_read, tr_write);

	/* Index out of bounds check */
	if (tr_read >= ASMHCP_TRANSFER_RING_CMD_SIZE || tr_write >= ASMHCP_TRANSFER_RING_CMD_SIZE) {
		SCSC_TAG_ERR(BT_H4,
			"ANT_COMMAND_PKT - Index out of bounds (tr_read=%u, tr_write=%u)\n",
			tr_read, tr_write);
		atomic_inc(&ant_service.error_count);
		return -EIO;
	}

	/* Does the transfer ring have room for an entry */
	if (BSMHCP_HAS_ROOM(tr_write, tr_read, ASMHCP_TRANSFER_RING_CMD_SIZE)) {
		/* Fill the transfer descriptor with the ANT command data */
		memcpy(td->data, data, count);
		td->length = (u16)count;

		/* Ensure the wake lock is acquired */
		if (!wake_lock_active(&ant_service.write_wake_lock)) {
			ant_service.write_wake_lock_count++;
			wake_lock(&ant_service.write_wake_lock);
		}

		/* Increase the write pointer */
		BSMHCP_INCREASE_INDEX(tr_write, ASMHCP_TRANSFER_RING_CMD_SIZE);
		ant_service.asmhcp_protocol->header.mailbox_cmd_driv_ctr_write = tr_write;

		/* Memory barrier to ensure out-of-order execution is completed */
		wmb();

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(
			ant_service.service,
			ant_service.asmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	} else {
		/* Transfer ring full. Only happens if the user attempt to send more ANT command packets than
		 * available credits
		 */
		count = 0;
	}

	return count;
}

static ssize_t scsc_shm_ant_data_write(const unsigned char *data, size_t count)
{
	/* Store the read/write pointer on the stack since both are placed in unbuffered/uncached memory */
	uint32_t tr_read = ant_service.asmhcp_protocol->header.mailbox_data_driv_ctr_read;
	uint32_t tr_write = ant_service.asmhcp_protocol->header.mailbox_data_driv_ctr_write;

	/* Temp vars */
	struct ASMHCP_TD_CONTROL *td = &ant_service.asmhcp_protocol->data_driver_controller_transfer_ring[tr_write];

	SCSC_TAG_DEBUG(BT_H4, "ANT_DATA_PKT (len=%zu, read=%u, write=%u)\n",
		count, tr_read, tr_write);

	/* Index out of bounds check */
	if (tr_read >= ASMHCP_TRANSFER_RING_DATA_SIZE || tr_write >= ASMHCP_TRANSFER_RING_DATA_SIZE) {
		SCSC_TAG_ERR(
			BT_H4,
			"ANT_DATA_PKT - Index out of bounds (tr_read=%u, tr_write=%u)\n",
			tr_read, tr_write);
		atomic_inc(&ant_service.error_count);
		return -EIO;
	}

	/* Does the transfer ring have room for an entry */
	if (BSMHCP_HAS_ROOM(tr_write, tr_read, ASMHCP_TRANSFER_RING_DATA_SIZE)) {
		/* Fill the transfer descriptor with the ANT command data */
		memcpy(td->data, data, count);
		td->length = (u16)count;

		/* Ensure the wake lock is acquired */
		if (!wake_lock_active(&ant_service.write_wake_lock)) {
			ant_service.write_wake_lock_count++;
			wake_lock(&ant_service.write_wake_lock);
		}

		/* Increase the write pointer */
		BSMHCP_INCREASE_INDEX(tr_write, ASMHCP_TRANSFER_RING_DATA_SIZE);
		ant_service.asmhcp_protocol->header.mailbox_data_driv_ctr_write = tr_write;

		/* Memory barrier to ensure out-of-order execution is completed */
		wmb();

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(
			ant_service.service,
			ant_service.asmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	}
		else
		/* Transfer ring full */
			count = 0;

	return count;
}

static ssize_t scsc_ant_copy_td_to_buffer(char __user *buf, size_t len, struct ASMHCP_TD_CONTROL *td)
{
	ssize_t                 ret = 0;
	ssize_t                 consumed = 0;
	size_t                  copy_len = 0;

	SCSC_TAG_DEBUG(BT_H4, "td (length=%u), len=%zu, read_offset=%zu\n",
		       td->length, len, ant_service.read_offset);

	/* Has the header been copied to userspace (aka is this the start of the copy operation) */
	if (ant_service.read_offset < ANT_HEADER_LENGTH) {
		/* Calculate the amount of data that can be transferred */
		copy_len = min(ANT_HEADER_LENGTH - ant_service.read_offset, len);

		if (td->data[1] + ANT_HEADER_LENGTH + 1 != td->length) {
			SCSC_TAG_ERR(BT_H4, "Firmware sent invalid ANT cmd/data\n");
			atomic_inc(&ant_service.error_count);
			ret = -EFAULT;
		}
		/* Copy the ANT header to the userspace buffer */
		ret = copy_to_user(buf, &td->data[ant_service.read_offset], copy_len);
		if (ret == 0) {
			/* All good - Update our consumed information */
			consumed = copy_len;
			ant_service.read_offset += copy_len;
			SCSC_TAG_DEBUG(BT_H4,
				       "copied header: read_offset=%zu, consumed=%zu, ret=%zd, len=%zu, copy_len=%zu\n",
				       ant_service.read_offset, consumed, ret, len, copy_len);
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	/* Can more data be put into the userspace buffer */
	if (ret == 0 && ant_service.read_offset >= ANT_HEADER_LENGTH && (len - consumed)) {
		/* Calculate the amount of data that can be transferred */
		copy_len = min((td->length - ant_service.read_offset), (len - consumed));

		/* Copy the data to the user buffer */
		ret = copy_to_user(&buf[consumed], &td->data[ant_service.read_offset], copy_len);
		if (ret == 0) {
			/* All good - Update our consumed information */
			ant_service.read_offset += copy_len;
			consumed += copy_len;

			/* Have all data been copied to the userspace buffer */
			if (ant_service.read_offset == td->length) {
				/* All good - read operation is completed */
				ant_service.read_offset = 0;
				ant_service.read_operation = ANT_READ_OP_NONE;
			}
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	SCSC_TAG_DEBUG(BT_H4, "read_offset=%zu, consumed=%zu, ret=%zd, len=%zu, copy_len=%zu\n",
		       ant_service.read_offset, consumed, ret, len, copy_len);

	return ret == 0 ? consumed : ret;
}

static ssize_t scsc_ant_cmd_read(char __user *buf, size_t len)
{
	ssize_t ret = 0;

	/* Temp vars */
	if (ant_service.mailbox_cmd_ctr_driv_read != ant_service.mailbox_cmd_ctr_driv_write) {
		struct ASMHCP_PROTOCOL *ap = ant_service.asmhcp_protocol;
		struct ASMHCP_TD_CONTROL *td = &ap->cmd_controller_driver_transfer_ring
		    [ant_service.mailbox_cmd_ctr_driv_read];

		ret = scsc_ant_copy_td_to_buffer(buf, len, td);
	}

	return ret;
}

static ssize_t scsc_ant_data_read(char __user *buf, size_t len)
{
	ssize_t ret = 0;

	if (ant_service.mailbox_data_ctr_driv_read != ant_service.mailbox_data_ctr_driv_write) {
		struct ASMHCP_PROTOCOL *ap = ant_service.asmhcp_protocol;
		struct ASMHCP_TD_CONTROL *td = &ap->data_controller_driver_transfer_ring
		    [ant_service.mailbox_data_ctr_driv_read];

		ret = scsc_ant_copy_td_to_buffer(buf, len, td);
	}

	return ret;
}

static ssize_t scsc_bt_shm_ant_read_data(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	while (ant_service.read_operation == ANT_READ_OP_NONE &&
	       ret == 0 &&
	       ant_service.mailbox_data_ctr_driv_read != ant_service.mailbox_data_ctr_driv_write) {
		/* Start a data copy to userspace */
		ant_service.read_operation = ANT_READ_OP_DATA;
		ant_service.read_index = ant_service.mailbox_data_ctr_driv_read;
		ret = scsc_ant_data_read(&buf[consumed], len - consumed);
		if (ret > 0) {
			/* All good - Update our consumed information */
			consumed += ret;
			ret = 0;

			/* Update the index if all the data could be copied to the userspace buffer
			 * otherwise stop processing the data
			 */
			if (ant_service.read_operation == ANT_READ_OP_NONE)
				BSMHCP_INCREASE_INDEX(ant_service.mailbox_data_ctr_driv_read,
						      ASMHCP_TRANSFER_RING_DATA_SIZE);
			else
				break;
		}
	}

	return ret == 0 ? consumed : ret;
}

static ssize_t scsc_bt_shm_ant_read_cmd(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	while (ant_service.read_operation == ANT_READ_OP_NONE &&
	       ret == 0 &&
	       ant_service.mailbox_cmd_ctr_driv_read != ant_service.mailbox_cmd_ctr_driv_write) {
		/* Start a cmd copy to userspace */
		ant_service.read_operation = ANT_READ_OP_CMD;
		ant_service.read_index = ant_service.mailbox_cmd_ctr_driv_read;
		ret = scsc_ant_cmd_read(&buf[consumed], len - consumed);
		if (ret > 0) {
			/* All good - Update our consumed information */
			consumed += ret;
			ret = 0;

			/* Update the index if all the data could be copied to the userspace buffer
			 * otherwise stop processing the cmds
			 */
			if (ant_service.read_operation == ANT_READ_OP_NONE)
				BSMHCP_INCREASE_INDEX(ant_service.mailbox_cmd_ctr_driv_read,
						      ASMHCP_TRANSFER_RING_CMD_SIZE);
			else
				break;
		}
	}

	return ret == 0 ? consumed : ret;
}

static ssize_t scsc_shm_ant_read_continue(char __user *buf, size_t len)
{
	ssize_t ret = 0;

	/* Is a cmd read operation ongoing */
	if (ant_service.read_operation == ANT_READ_OP_CMD) {
		SCSC_TAG_DEBUG(BT_H4, "ANT_READ_OP_CMD\n");

		/* Copy data into the userspace buffer */
		ret = scsc_ant_cmd_read(buf, len);
		if (ant_service.read_operation == ANT_READ_OP_NONE)
			/* All done - increase the read pointer and continue */
			if (ant_service.read_operation == ANT_READ_OP_NONE)
				BSMHCP_INCREASE_INDEX(ant_service.mailbox_cmd_ctr_driv_read,
						      ASMHCP_TRANSFER_RING_CMD_SIZE);
		/* Is a data read operation ongoing */
	} else if (ant_service.read_operation == ANT_READ_OP_DATA) {
		SCSC_TAG_DEBUG(BT_H4, "ANT_READ_OP_DATA\n");

		/* Copy data into the userspace buffer */
		ret = scsc_ant_data_read(buf, len);
		if (ant_service.read_operation == ANT_READ_OP_NONE)
			/* All done - increase the read pointer and continue */
			BSMHCP_INCREASE_INDEX(ant_service.mailbox_data_ctr_driv_read, ASMHCP_TRANSFER_RING_DATA_SIZE);
	}

	return ret;
}

ssize_t scsc_shm_ant_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t consumed = 0;
	ssize_t ret = 0;
	ssize_t res;
	bool    gen_bg_int = false;

	/* Special handling in case read is called after service has closed */
	if (!ant_service.service_started)
		return -EIO;

	/* Only 1 reader is allowed */
	if (atomic_inc_return(&ant_service.ant_readers) != 1) {
		atomic_dec(&ant_service.ant_readers);
		return -EIO;
	}

	/* Has en error been detect then just return with an error */
	if (atomic_read(&ant_service.error_count) != 0) {
		atomic_dec(&ant_service.ant_readers);
		return -EIO;
	}

	/* Update the cached variables with the non-cached variables */
	ant_service.mailbox_cmd_ctr_driv_write = ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_write;
	ant_service.mailbox_data_ctr_driv_write = ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_write;

	/* put the remaining data from the transfer ring into the available userspace buffer */
	if (ant_service.read_operation != ANT_READ_OP_NONE) {
		ret = scsc_shm_ant_read_continue(buf, len);
		/* Update the consumed variable in case a operation was ongoing */
		if (ret > 0) {
			consumed = ret;
			ret = 0;
		}
	}

	/* Main loop - Can only be entered when no operation is present on entering this function
	 * or no hardware error has been detected. It loops until data has been placed in the
	 * userspace buffer or an error has been detected
	 */
	while (atomic_read(&ant_service.error_count) == 0 && consumed == 0) {
		/* Does any of the read/write pairs differs */
		if (ant_service.mailbox_data_ctr_driv_read == ant_service.mailbox_data_ctr_driv_write &&
		    ant_service.mailbox_cmd_ctr_driv_read == ant_service.mailbox_cmd_ctr_driv_write &&
		    atomic_read(&ant_service.error_count) == 0 &&
		    ant_service.asmhcp_protocol->header.panic_deathbed_confession == 0) {
			/* Don't wait if in NONBLOCK mode */
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			/* All read/write pairs are identical - wait for the firmware. The conditional
			 * check is used to verify that a read/write pair has actually changed
			 */
			ret = wait_event_interruptible(bt_service.read_wait,
				(ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_write !=
					ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_read ||
				 ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_write !=
					ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_read ||
				 atomic_read(&ant_service.error_count) != 0 ||
				 ant_service.asmhcp_protocol->header.panic_deathbed_confession));

			/* Has an error been detected elsewhere in the driver then just return from this function */
			if (atomic_read(&ant_service.error_count) != 0)
				break;

			/* Any failures is handled by the userspace application */
			if (ret)
				break;

			/* Refresh our write indexes before starting to process the protocol */
			ant_service.mailbox_cmd_ctr_driv_write =
			    ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_write;
			ant_service.mailbox_data_ctr_driv_write =
			    ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_write;
		}

		/* First: process any pending cmd that needs to be sent to userspace */
		res = scsc_bt_shm_ant_read_cmd(&buf[consumed], len - consumed);
		if (res > 0)
			consumed += res;
		else
			ret = res;

		/* Second: process any pending data that needs to be sent to userspace */
		res = scsc_bt_shm_ant_read_data(&buf[consumed], len - consumed);
		if (res > 0)
			consumed += res;
		else
			ret = res;
	}

	/* If anything was read, generate the appropriate interrupt(s) */
	if (ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_read != ant_service.mailbox_cmd_ctr_driv_read ||
	    ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_read != ant_service.mailbox_data_ctr_driv_read)
		gen_bg_int = true;

	/* Update the read index for all transfer rings */
	ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_read = ant_service.mailbox_cmd_ctr_driv_read;
	ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_read = ant_service.mailbox_data_ctr_driv_read;

	/* Ensure the data is updating correctly in memory */
	wmb();

	if (gen_bg_int)
		scsc_service_mifintrbit_bit_set(ant_service.service,
						ant_service.asmhcp_protocol->header.ap_to_bg_int_src,
						SCSC_MIFINTR_TARGET_R4);

	/* Decrease the ant readers counter */
	atomic_dec(&ant_service.ant_readers);

	return ret == 0 ? consumed : ret;
}

ssize_t scsc_shm_ant_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	size_t  length;
	size_t  ant_pkt_len;
	ssize_t written = 0;
	ssize_t ret = 0;
	size_t  pkt_count = 0;

	SCSC_TAG_DEBUG(BT_H4, "enter\n");

	UNUSED(file);
	UNUSED(offset);

	/* Don't allow any writes after service has been closed */
	if (!ant_service.service_started)
		return -EIO;

	/* Only 1 writer is allowed */
	if (atomic_inc_return(&ant_service.ant_writers) != 1) {
		SCSC_TAG_DEBUG(BT_H4, "only one reader allowed\n");
		atomic_dec(&ant_service.ant_writers);
		return -EIO;
	}

	/* Has en error been detect then just return with an error */
	if (atomic_read(&ant_service.error_count) != 0) {
		SCSC_TAG_DEBUG(BT_H4, "error has occured\n");
		atomic_dec(&ant_service.ant_writers);
		return -EIO;
	}

	while (written != count && ret == 0) {
		length = min(count - written, sizeof(ant_write_buffer) - ant_service.ant_write_offset);
		SCSC_TAG_DEBUG(BT_H4, "count: %zu, length: %zu, ant_write_offset: %zu, written:%zu, size:%zu\n",
			       count, length, ant_service.ant_write_offset,
				   written - (pkt_count * 2), sizeof(ant_write_buffer));

		/* Is there room in the temp buffer */
		if (length == 0) {
			SCSC_TAG_ERR(BT_H4, "no room in the buffer\n");
			atomic_inc(&ant_service.error_count);
			ret = -EIO;
			break;
		}

		/* Copy the userspace data to the target buffer */
		ret = copy_from_user(&ant_write_buffer[ant_service.ant_write_offset], &buf[written], length);

		if (ret == 0) {
			/* Is the message a data message? */
			if (ant_write_buffer[0] == ANT_DATA_MSG) {
				/* Extract the data packet length */
				ant_pkt_len = ant_write_buffer[1] + ANT_HEADER_LENGTH + 1;

				/* Is it a complete packet available */
				if (ant_pkt_len <= (length + ant_service.ant_write_offset)) {
					/* Transfer the packet to the ANT data transfer ring */
					ret = scsc_shm_ant_data_write(&ant_write_buffer[2], ant_pkt_len - 2);
					if (ret >= 0) {
						written += (ant_pkt_len - ant_service.ant_write_offset);
						pkt_count += 1;
						ant_service.ant_write_offset = 0;
						ret = 0;
					}
				} else {
					/* Still needing data to have the complete packet */
					SCSC_TAG_WARNING(BT_H4,
						"missing data (need=%zu, got=%zu)\n",
						ant_pkt_len, (length - ant_service.ant_write_offset));
					written += length;
					ant_service.ant_write_offset += (u32) length;
				}
			/* Is the message a command message? */
			} else if (ant_write_buffer[0] == ANT_COMMAND_MSG) {
				/* Extract the ANT command packet length */
				ant_pkt_len = ant_write_buffer[1] + ANT_HEADER_LENGTH + 1;

				/* Is it a complete packet available */
				if ((ant_pkt_len) <= (length + ant_service.ant_write_offset)) {
					/* Transfer the packet to the ANT command transfer ring */
					ret = scsc_shm_ant_cmd_write(&ant_write_buffer[2], ant_pkt_len - 2);
					if (ret >= 0) {
						written += (ant_pkt_len - ant_service.ant_write_offset);
						pkt_count += 1;
						ant_service.ant_write_offset = 0;
						ret = 0;
					}
				} else {
					/* Still needing data to have the complete packet */
					SCSC_TAG_WARNING(BT_H4,
						"missing data (need=%zu, got=%zu)\n",
						(ant_pkt_len), (length + ant_service.ant_write_offset));
					written += length;
					ant_service.ant_write_offset += (u32) length;
				}
				/* Is there less data than a header then just wait for more */
			} else if (length <= ANT_HEADER_LENGTH) {
				ant_service.ant_write_offset += length;
				written += length;
				/* Header is unknown - unable to proceed */
			} else {
				atomic_inc(&ant_service.error_count);
				ret = -EIO;
			}
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_from_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	SCSC_TAG_DEBUG(BT_H4, "ant_write_offset=%zu, ret=%zu, written=%zu\n",
		       ant_service.ant_write_offset, ret, written - (pkt_count * 2));

	/* Decrease the ant readers counter */
	atomic_dec(&ant_service.ant_writers);

	return ret == 0 ? written : ret;
}

unsigned int scsc_shm_ant_poll(struct file *file, poll_table *wait)
{
	/* Add the wait queue to the polling queue */
	poll_wait(file, &ant_service.read_wait, wait);

	if (!ant_service.service_started ||
	    atomic_read(&ant_service.error_count) != 0)
		return POLLERR;

	/* Has en error been detect then just return with an error */
	if (ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_write !=
	    ant_service.asmhcp_protocol->header.mailbox_data_ctr_driv_read ||
	    ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_write !=
	    ant_service.asmhcp_protocol->header.mailbox_cmd_ctr_driv_read) {
		SCSC_TAG_DEBUG(BT_H4, "queue(s) changed\n");
		return POLLIN | POLLRDNORM; /* readeable */
	}

	SCSC_TAG_DEBUG(BT_H4, "no change\n");

	return (atomic_read(&ant_service.error_count) != 0) ? POLLERR : POLLOUT;
}

/* Initialise the shared memory interface for ANT */
int scsc_ant_shm_init(void)
{
	/* Get kmem pointer to the shared memory ref */
	ant_service.asmhcp_protocol = scsc_mx_service_mif_addr_to_ptr(ant_service.service, ant_service.asmhcp_ref);
	if (ant_service.asmhcp_protocol == NULL) {
		SCSC_TAG_ERR(BT_COMMON, "couldn't map kmem to shm_ref 0x%08x\n", (u32)ant_service.asmhcp_ref);
		return -ENOMEM;
	}

	/* Clear the protocol shared memory area */
	memset(ant_service.asmhcp_protocol, 0, sizeof(*ant_service.asmhcp_protocol));
	ant_service.asmhcp_protocol->header.magic_value = ASMHCP_PROTOCOL_MAGICVALUE;
	ant_service.mailbox_data_ctr_driv_read = 0;
	ant_service.mailbox_data_ctr_driv_write = 0;
	ant_service.mailbox_cmd_ctr_driv_read = 0;
	ant_service.mailbox_cmd_ctr_driv_write = 0;
	ant_service.read_index = 0;
	ant_irq_mask = 0;

	/* Initialise the interrupt handlers */
	if (scsc_ant_shm_init_interrupt() < 0) {
		SCSC_TAG_ERR(BT_COMMON, "Failed to register IRQ bits\n");
		return -EIO;
	}

	return 0;
}

/* Terminate the shared memory interface for ANT, stopping its thread.
 *
 * Note: The service must be stopped prior to calling this function.
 *       The shared memory can only be released after calling this function.
 */
void scsc_ant_shm_exit(void)
{
	u16 irq_num = 0;

	/* Release IRQs */
	if (ant_service.asmhcp_protocol != NULL) {
		if (ant_irq_mask & 1 << irq_num++) {
			scsc_service_mifintrbit_unregister_tohost(
				ant_service.service,
				ant_service.asmhcp_protocol->header.bg_to_ap_int_src);
		}

		if (ant_irq_mask & 1 << irq_num++) {
			scsc_service_mifintrbit_free_fromhost(
				ant_service.service,
				ant_service.asmhcp_protocol->header.ap_to_bg_int_src,
				SCSC_MIFINTR_TARGET_R4);
		}
	}

	/* Clear all control structures */
	ant_service.asmhcp_protocol = NULL;
}
