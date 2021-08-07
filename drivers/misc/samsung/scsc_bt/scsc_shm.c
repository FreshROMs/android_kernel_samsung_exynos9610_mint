/****************************************************************************
 *
 *       Copyright (c) 2015 Samsung Electronics Co., Ltd
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
#include <asm/io.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/api/bsmhcp.h>
#include <scsc/scsc_logring.h>

#ifdef CONFIG_SCSC_LOG_COLLECTION
#include <scsc/scsc_log_collector.h>
#endif

#include "scsc_bt_priv.h"
#include "scsc_shm.h"
#include "scsc_bt_hci.h"

struct hci_credit_entry {
	u16 hci_connection_handle;
	u16 credits;
};

static u8   h4_write_buffer[BSMHCP_ACL_PACKET_SIZE + H4DMUX_HEADER_ACL];
static u8   h4_acl_header[5];
static u8   h4_hci_event_ncp_header[4 + BSMHCP_TRANSFER_RING_ACL_COUNT * sizeof(struct hci_credit_entry)];
static u32  h4_hci_event_ncp_header_len = 8;
static struct hci_credit_entry *h4_hci_credit_entries = (struct hci_credit_entry *) &h4_hci_event_ncp_header[4];
static u8   h4_hci_event_hardware_error[4] = { HCI_EVENT_PKT, HCI_EVENT_HARDWARE_ERROR_EVENT, 1, 0 };
static u8   h4_iq_report_evt[HCI_IQ_REPORT_MAX_LEN];
static u32  h4_iq_report_evt_len;
static u16  h4_irq_mask;

static void scsc_bt_shm_irq_handler(int irqbit, void *data)
{
	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(bt_service.service, irqbit);

	/* Ensure irq bit is cleared before reading the mailbox indexes */
	mb();

	bt_service.interrupt_count++;

	/* Wake the reader operation */
	if (bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write !=
	    bt_service.bsmhcp_protocol->header.mailbox_hci_evt_read ||
	    bt_service.bsmhcp_protocol->header.mailbox_acl_rx_write !=
	    bt_service.bsmhcp_protocol->header.mailbox_acl_rx_read ||
	    bt_service.bsmhcp_protocol->header.mailbox_acl_free_write !=
	    bt_service.bsmhcp_protocol->header.mailbox_acl_free_read ||
	    bt_service.bsmhcp_protocol->header.mailbox_iq_report_write !=
	    bt_service.bsmhcp_protocol->header.mailbox_iq_report_read ||
	    0 != atomic_read(&bt_service.error_count) ||
	    bt_service.bsmhcp_protocol->header.panic_deathbed_confession) {
		bt_service.interrupt_read_count++;

		wake_lock_timeout(&bt_service.read_wake_lock, HZ);
		wake_up(&bt_service.read_wait);
	}

	if (bt_service.bsmhcp_protocol->header.mailbox_hci_cmd_write ==
	    bt_service.bsmhcp_protocol->header.mailbox_hci_cmd_read &&
	    bt_service.bsmhcp_protocol->header.mailbox_acl_tx_write ==
	    bt_service.bsmhcp_protocol->header.mailbox_acl_tx_read) {
		bt_service.interrupt_write_count++;

		if (wake_lock_active(&bt_service.write_wake_lock)) {
			bt_service.write_wake_unlock_count++;
			wake_unlock(&bt_service.write_wake_lock);
		}
	}
}

/* Assign firmware/host interrupts */
static int scsc_bt_shm_init_interrupt(void)
{
	int irq_ret = 0;
	u16 irq_num = 0;

	/* To-host f/w IRQ allocations and ISR registrations */
	irq_ret = scsc_service_mifintrbit_register_tohost(
	    bt_service.service, scsc_bt_shm_irq_handler, NULL);
	if (irq_ret < 0)
		return irq_ret;

	bt_service.bsmhcp_protocol->header.bg_to_ap_int_src = irq_ret;
	h4_irq_mask |= 1 << irq_num++;

	/* From-host f/w IRQ allocations */
	irq_ret = scsc_service_mifintrbit_alloc_fromhost(
	    bt_service.service, SCSC_MIFINTR_TARGET_R4);
	if (irq_ret < 0)
		return irq_ret;

	bt_service.bsmhcp_protocol->header.ap_to_bg_int_src = irq_ret;
	h4_irq_mask |= 1 << irq_num++;

	irq_ret = scsc_service_mifintrbit_alloc_fromhost(
	    bt_service.service, SCSC_MIFINTR_TARGET_R4);
	if (irq_ret < 0)
		return irq_ret;

	bt_service.bsmhcp_protocol->header.ap_to_fg_int_src = irq_ret;
	h4_irq_mask |= 1 << irq_num++;

	SCSC_TAG_DEBUG(BT_COMMON, "Registered to-host IRQ bits %d, from-host IRQ bits %d:%d\n",
		       bt_service.bsmhcp_protocol->header.bg_to_ap_int_src,
		       bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
		       bt_service.bsmhcp_protocol->header.ap_to_fg_int_src);

	return 0;
}

bool scsc_bt_shm_h4_avdtp_detect_write(uint32_t flags,
									   uint16_t l2cap_cid,
									   uint16_t hci_connection_handle)
{
	uint32_t tr_read;
	uint32_t tr_write;
	struct BSMHCP_TD_AVDTP *td;

	spin_lock(&bt_service.avdtp_detect.fw_write_lock);

	/* Store the read/write pointer on the stack since both are placed in unbuffered/uncached memory */
	tr_read = bt_service.bsmhcp_protocol->header.mailbox_avdtp_read;
	tr_write = bt_service.bsmhcp_protocol->header.mailbox_avdtp_write;

	td = &bt_service.bsmhcp_protocol->avdtp_transfer_ring[tr_write];

	SCSC_TAG_DEBUG(BT_H4,
				   "AVDTP_DETECT_PKT (flags: 0x%08X, cid: 0x%04X, handle: 0x%04X, read=%u, write=%u)\n",
				   flags,
				   l2cap_cid,
				   hci_connection_handle,
				   tr_read,
				   tr_write);

	/* Index out of bounds check */
	if (tr_read >= BSMHCP_TRANSFER_RING_AVDTP_SIZE || tr_write >= BSMHCP_TRANSFER_RING_AVDTP_SIZE) {
		spin_unlock(&bt_service.avdtp_detect.fw_write_lock);
		SCSC_TAG_ERR(BT_H4,
					 "AVDTP_DETECT_PKT - Index out of bounds (tr_read=%u, tr_write=%u)\n",
					 tr_read,
					 tr_write);
		atomic_inc(&bt_service.error_count);
		return false;
	}

	/* Does the transfer ring have room for an entry */
	if (BSMHCP_HAS_ROOM(tr_write, tr_read, BSMHCP_TRANSFER_RING_AVDTP_SIZE)) {
		/* Fill the transfer descriptor with the AVDTP data */
		td->flags = flags;
		td->l2cap_cid = l2cap_cid;
		td->hci_connection_handle = hci_connection_handle;

		/* Ensure the wake lock is acquired */
		if (!wake_lock_active(&bt_service.write_wake_lock)) {
			bt_service.write_wake_lock_count++;
			wake_lock(&bt_service.write_wake_lock);
		}

		/* Increate the write pointer */
		BSMHCP_INCREASE_INDEX(tr_write, BSMHCP_TRANSFER_RING_AVDTP_SIZE);
		bt_service.bsmhcp_protocol->header.mailbox_avdtp_write = tr_write;

		spin_unlock(&bt_service.avdtp_detect.fw_write_lock);

		/* Memory barrier to ensure out-of-order execution is completed */
		wmb();

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(
			bt_service.service,
			bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	} else {
		/* Transfer ring full */
		spin_unlock(&bt_service.avdtp_detect.fw_write_lock);
		SCSC_TAG_ERR(BT_H4,
					 "AVDTP_DETECT_PKT - No more room for messages (tr_read=%u, tr_write=%u)\n",
					 tr_read,
					 tr_write);
		scsc_service_force_panic(bt_service.service);
		return false;
	}
	return true;
}


static ssize_t scsc_bt_shm_h4_hci_cmd_write(const unsigned char *data, size_t count)
{
	/* Store the read/write pointer on the stack since both are placed in unbuffered/uncached memory */
	uint32_t tr_read = bt_service.bsmhcp_protocol->header.mailbox_hci_cmd_read;
	uint32_t tr_write = bt_service.bsmhcp_protocol->header.mailbox_hci_cmd_write;
#ifdef CONFIG_SCSC_PRINTK
	uint16_t op_code = *(uint16_t *) data;
#endif

	/* Temp vars */
	struct BSMHCP_TD_CONTROL *td = &bt_service.bsmhcp_protocol->hci_cmd_transfer_ring[tr_write];

	SCSC_TAG_DEBUG(BT_H4, "HCI_COMMAND_PKT (op_code=0x%04x, len=%zu, read=%u, write=%u)\n", op_code, count, tr_read, tr_write);

	/* Index out of bounds check */
	if (tr_read >= BSMHCP_TRANSFER_RING_CMD_SIZE || tr_write >= BSMHCP_TRANSFER_RING_CMD_SIZE) {
		SCSC_TAG_ERR(BT_H4, "HCI_COMMAND_PKT - Index out of bounds (tr_read=%u, tr_write=%u)\n", tr_read, tr_write);
		atomic_inc(&bt_service.error_count);
		return -EIO;
	}

	/* Does the transfer ring have room for an entry */
	if (BSMHCP_HAS_ROOM(tr_write, tr_read, BSMHCP_TRANSFER_RING_CMD_SIZE)) {
		/* Fill the transfer descriptor with the HCI command data */
		memcpy(td->data, data, count);
		td->length = (u16)count;

		/* Ensure the wake lock is acquired */
		if (!wake_lock_active(&bt_service.write_wake_lock)) {
			bt_service.write_wake_lock_count++;
			wake_lock(&bt_service.write_wake_lock);
		}

		/* Increate the write pointer */
		BSMHCP_INCREASE_INDEX(tr_write, BSMHCP_TRANSFER_RING_CMD_SIZE);
		bt_service.bsmhcp_protocol->header.mailbox_hci_cmd_write = tr_write;

		/* Memory barrier to ensure out-of-order execution is completed */
		wmb();

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(bt_service.service, bt_service.bsmhcp_protocol->header.ap_to_bg_int_src, SCSC_MIFINTR_TARGET_R4);
	} else
		/* Transfer ring full. Only happens if the user attempt to send more HCI command packets than
		 * available credits */
		count = 0;

	return count;
}

static ssize_t scsc_bt_shm_h4_acl_write(const unsigned char *data, size_t count)
{
	/* Store the read/write pointer on the stack since both are placed in unbuffered/uncached memory */
	uint32_t                     tr_read = bt_service.bsmhcp_protocol->header.mailbox_acl_tx_read;
	uint32_t                     tr_write = bt_service.bsmhcp_protocol->header.mailbox_acl_tx_write;

	/* Temp vars */
	struct BSMHCP_TD_ACL_TX_DATA *td = &bt_service.bsmhcp_protocol->acl_tx_data_transfer_ring[tr_write];
	int                          acldata_buf_index = -1;
	u16                          l2cap_length;
	u32                          i;
	size_t                       payload_len = count - ACLDATA_HEADER_SIZE;

	/* Index out of bounds check */
	if (tr_read >= BSMHCP_TRANSFER_RING_ACL_SIZE || tr_write >= BSMHCP_TRANSFER_RING_ACL_SIZE) {
		SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - Index out of bounds (tr_read=%u, tr_write=%u)\n", tr_read, tr_write);
		atomic_inc(&bt_service.error_count);
		return -EIO;
	}

	/* Allocate a data slot */
	for (i = 0; i < BSMHCP_DATA_BUFFER_TX_ACL_SIZE; i++) {
		/* Wrap the offset index around the buffer max */
		if (++bt_service.last_alloc == BSMHCP_DATA_BUFFER_TX_ACL_SIZE)
			bt_service.last_alloc = 0;
		/* Claim a free slot */
		if (bt_service.allocated[bt_service.last_alloc] == 0) {
			bt_service.allocated[bt_service.last_alloc] = 1;
			acldata_buf_index = bt_service.last_alloc;
			bt_service.allocated_count++;
			break;
		}
	}

	/* Is a buffer available to hold the data */
	if (acldata_buf_index < 0) {
		SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - No buffers available\n");
		atomic_inc(&bt_service.error_count);
		return -EIO;
	}

	/* Does the transfer ring have room for an entry */
	if (BSMHCP_HAS_ROOM(tr_write, tr_read, BSMHCP_TRANSFER_RING_ACL_SIZE)) {
		/* Extract the ACL data header and L2CAP header and fill it into the transfer descriptor */
		td->buffer_index = (uint8_t)acldata_buf_index;
		td->flags = HCI_ACL_DATA_FLAGS(data);
		td->hci_connection_handle = HCI_ACL_DATA_CON_HDL(data);
		td->length = (u16)payload_len;

		if ((td->flags & BSMHCP_ACL_BC_FLAG_BCAST_ACTIVE) ||
		    (td->flags & BSMHCP_ACL_BC_FLAG_BCAST_ALL)) {
			SCSC_TAG_DEBUG(BT_H4,
				"Setting broadcast handle (hci_connection_handle=0x%03x)\n",
				td->hci_connection_handle);
			bt_service.connection_handle_list[td->hci_connection_handle].state = CONNECTION_ACTIVE;
		}

		/* Is this a packet marked with the start flag */
		if ((td->flags & BSMHCP_ACL_PB_FLAG_MASK) == BSMHCP_ACL_PB_FLAG_START_NONFLUSH ||
		    (td->flags & BSMHCP_ACL_PB_FLAG_MASK) == BSMHCP_ACL_PB_FLAG_START_FLUSH) {

			/* Extract the L2CAP payload length and connection identifier */
			td->l2cap_cid = HCI_L2CAP_CID(data);

			/* data+4 to skip the HCI header, to align offsets with the rx detection. The "true" argument is to tell
			 * the detection that this is TX */
			scsc_avdtp_detect_rxtx(td->hci_connection_handle, data+4, td->length, true);

			l2cap_length = HCI_L2CAP_LENGTH(data);

			SCSC_TAG_DEBUG(BT_TX, "ACL[START] (len=%u, buffer=%u, credits=%u, l2cap_cid=0x%04x, l2cap_length=%u)\n",
				td->length, acldata_buf_index,
				BSMHCP_DATA_BUFFER_TX_ACL_SIZE - (bt_service.allocated_count - bt_service.freed_count),
				HCI_L2CAP_CID(data), l2cap_length);

			if (l2cap_length == payload_len - L2CAP_HEADER_SIZE)
				/* Mark it with the END flag if packet length matches the L2CAP payload length */
				td->flags |= BSMHCP_ACL_L2CAP_FLAG_END;
			else if (l2cap_length < payload_len - L2CAP_HEADER_SIZE) {
				/* Mark it with the END flag if packet length is greater than the L2CAP payload length
				   and generate a warning notifying that this is incorrect according to the specification.
				   This is allowed to support the BITE tester. */
				SCSC_TAG_WARNING(BT_H4, "ACL_DATA_PKT - H4 ACL payload length > L2CAP Length (payload_len=%zu, l2cap_length=%u)\n",
						 payload_len - L2CAP_HEADER_SIZE, l2cap_length);
				td->flags |= BSMHCP_ACL_L2CAP_FLAG_END;
			} else if (l2cap_length > (payload_len - L2CAP_HEADER_SIZE)) {
				/* This is only a fragment of the packet. Save the remaining number of octets required
				 * to complete the packet */
				bt_service.connection_handle_list[td->hci_connection_handle].length = (u16)(l2cap_length - payload_len + L2CAP_HEADER_SIZE);
				bt_service.connection_handle_list[td->hci_connection_handle].l2cap_cid = HCI_L2CAP_CID(data);
			} else {
				/* The packet is larger than the L2CAP payload length - protocol error */
				SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - L2CAP Length Error (l2cap_length=%u, payload_len=%zu)\n",
					     l2cap_length, payload_len - L2CAP_HEADER_SIZE);
				atomic_inc(&bt_service.error_count);
				return -EIO;
			}
		} else if ((td->flags & BSMHCP_ACL_PB_FLAG_MASK) == BSMHCP_ACL_PB_FLAG_CONT) {
			/* Set the L2CAP connection identifer set by the start packet */
			td->l2cap_cid = bt_service.connection_handle_list[td->hci_connection_handle].l2cap_cid;

			SCSC_TAG_DEBUG(BT_TX, "ACL[CONT] (len=%u, buffer=%u, credits=%u, l2cap_cid=0x%04x, length=%u)\n",
				td->length, acldata_buf_index,
				BSMHCP_DATA_BUFFER_TX_ACL_SIZE - (bt_service.allocated_count - bt_service.freed_count),
				bt_service.connection_handle_list[td->hci_connection_handle].l2cap_cid,
				bt_service.connection_handle_list[td->hci_connection_handle].length);

			/* Does this packet complete the L2CAP frame */
			if (bt_service.connection_handle_list[td->hci_connection_handle].length == payload_len) {
				/* The L2CAP frame is complete. mark it with the END flag */
				td->flags |= BSMHCP_ACL_L2CAP_FLAG_END;

				/* Set the remaining length to zero */
				bt_service.connection_handle_list[td->hci_connection_handle].length = 0;
			} else if (bt_service.connection_handle_list[td->hci_connection_handle].length < payload_len) {
				/* Mark it with the END flag if packet length is greater than the L2CAP missing payload length
				   and generate a warning notifying that this is incorrect according to the specification.
				   This is allowed to support the BITE tester. */
				SCSC_TAG_WARNING(BT_H4, "ACL_DATA_PKT - H4 ACL payload length > L2CAP Missing Length (payload_len=%zu, missing=%u)\n",
						 payload_len, bt_service.connection_handle_list[td->hci_connection_handle].length);
				td->flags |= BSMHCP_ACL_L2CAP_FLAG_END;
				/* Set the remaining length to zero */
				bt_service.connection_handle_list[td->hci_connection_handle].length = 0;
			} else if (bt_service.connection_handle_list[td->hci_connection_handle].length > payload_len)
				/* This is another fragment of the packet. Save the remaining number of octets required
				 * to complete the packet */
				bt_service.connection_handle_list[td->hci_connection_handle].length -= (u16)payload_len;
			else if (bt_service.connection_handle_list[td->hci_connection_handle].length < payload_len) {
				/* The packet is larger than the L2CAP payload length - protocol error */
				SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - L2CAP Length Error (missing=%u, payload_len=%zu)\n",
					     bt_service.connection_handle_list[td->hci_connection_handle].length, payload_len);
				atomic_inc(&bt_service.error_count);
				return -EIO;
			}
		} else {
			/* Reserved flags set - report it as an error */
			SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - Flag set to reserved\n");
			atomic_inc(&bt_service.error_count);
			return -EIO;
		}

		SCSC_TAG_DEBUG(BT_H4, "ACL_DATA_PKT (len=%zu, read=%u, write=%u, slot=%u, flags=0x%04x, handle=0x%03x, l2cap_cid=0x%04x, missing=%u)\n",
			       payload_len, tr_read, tr_write, acldata_buf_index, td->flags >> 4, td->hci_connection_handle, td->l2cap_cid,
			       bt_service.connection_handle_list[td->hci_connection_handle].length);

		/* Ensure the wake lock is acquired */
		if (!wake_lock_active(&bt_service.write_wake_lock)) {
			bt_service.write_wake_lock_count++;
			wake_lock(&bt_service.write_wake_lock);
		}

		/* Copy the ACL packet into the targer buffer */
		memcpy(&bt_service.bsmhcp_protocol->acl_tx_buffer[acldata_buf_index][0], &data[ACLDATA_HEADER_SIZE], payload_len);
		/* Increate the write pointer */
		BSMHCP_INCREASE_INDEX(tr_write, BSMHCP_TRANSFER_RING_ACL_SIZE);
		bt_service.bsmhcp_protocol->header.mailbox_acl_tx_write = tr_write;

		/* Memory barrier to ensure out-of-order execution is completed */
		wmb();

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(bt_service.service,
						bt_service.bsmhcp_protocol->header.ap_to_fg_int_src, SCSC_MIFINTR_TARGET_R4);
	} else {
		/* Transfer ring full. Only happens if the user attempt to send more ACL data packets than
		 * available credits */
		SCSC_TAG_ERR(BT_H4, "ACL_DATA_PKT - No room in transfer ring (tr_write=%u, tr_read=%u)\n",
			     tr_write, tr_read);
		atomic_inc(&bt_service.error_count);
		count = -EIO;
	}

	return count;
}

#ifdef CONFIG_SCSC_PRINTK
static const char *scsc_hci_evt_decode_event_code(u8 hci_event_code, u8 hci_ulp_sub_code)
{
	const char *ret = "NA";

	switch (hci_event_code) {
	HCI_EV_DECODE(HCI_EV_INQUIRY_COMPLETE);
	HCI_EV_DECODE(HCI_EV_INQUIRY_RESULT);
	HCI_EV_DECODE(HCI_EV_CONN_COMPLETE);
	HCI_EV_DECODE(HCI_EV_CONN_REQUEST);
	HCI_EV_DECODE(HCI_EV_DISCONNECT_COMPLETE);
	HCI_EV_DECODE(HCI_EV_AUTH_COMPLETE);
	HCI_EV_DECODE(HCI_EV_REMOTE_NAME_REQ_COMPLETE);
	HCI_EV_DECODE(HCI_EV_ENCRYPTION_CHANGE);
	HCI_EV_DECODE(HCI_EV_CHANGE_CONN_LINK_KEY_COMPLETE);
	HCI_EV_DECODE(HCI_EV_MASTER_LINK_KEY_COMPLETE);
	HCI_EV_DECODE(HCI_EV_READ_REM_SUPP_FEATURES_COMPLETE);
	HCI_EV_DECODE(HCI_EV_READ_REMOTE_VER_INFO_COMPLETE);
	HCI_EV_DECODE(HCI_EV_QOS_SETUP_COMPLETE);
	HCI_EV_DECODE(HCI_EV_COMMAND_COMPLETE);
	HCI_EV_DECODE(HCI_EV_COMMAND_STATUS);
	HCI_EV_DECODE(HCI_EV_HARDWARE_ERROR);
	HCI_EV_DECODE(HCI_EV_FLUSH_OCCURRED);
	HCI_EV_DECODE(HCI_EV_ROLE_CHANGE);
	HCI_EV_DECODE(HCI_EV_NUMBER_COMPLETED_PKTS);
	HCI_EV_DECODE(HCI_EV_MODE_CHANGE);
	HCI_EV_DECODE(HCI_EV_RETURN_LINK_KEYS);
	HCI_EV_DECODE(HCI_EV_PIN_CODE_REQ);
	HCI_EV_DECODE(HCI_EV_LINK_KEY_REQ);
	HCI_EV_DECODE(HCI_EV_LINK_KEY_NOTIFICATION);
	HCI_EV_DECODE(HCI_EV_LOOPBACK_COMMAND);
	HCI_EV_DECODE(HCI_EV_DATA_BUFFER_OVERFLOW);
	HCI_EV_DECODE(HCI_EV_MAX_SLOTS_CHANGE);
	HCI_EV_DECODE(HCI_EV_READ_CLOCK_OFFSET_COMPLETE);
	HCI_EV_DECODE(HCI_EV_CONN_PACKET_TYPE_CHANGED);
	HCI_EV_DECODE(HCI_EV_QOS_VIOLATION);
	HCI_EV_DECODE(HCI_EV_PAGE_SCAN_MODE_CHANGE);
	HCI_EV_DECODE(HCI_EV_PAGE_SCAN_REP_MODE_CHANGE);
	HCI_EV_DECODE(HCI_EV_FLOW_SPEC_COMPLETE);
	HCI_EV_DECODE(HCI_EV_INQUIRY_RESULT_WITH_RSSI);
	HCI_EV_DECODE(HCI_EV_READ_REM_EXT_FEATURES_COMPLETE);
	HCI_EV_DECODE(HCI_EV_FIXED_ADDRESS);
	HCI_EV_DECODE(HCI_EV_ALIAS_ADDRESS);
	HCI_EV_DECODE(HCI_EV_GENERATE_ALIAS_REQ);
	HCI_EV_DECODE(HCI_EV_ACTIVE_ADDRESS);
	HCI_EV_DECODE(HCI_EV_ALLOW_PRIVATE_PAIRING);
	HCI_EV_DECODE(HCI_EV_ALIAS_ADDRESS_REQ);
	HCI_EV_DECODE(HCI_EV_ALIAS_NOT_RECOGNISED);
	HCI_EV_DECODE(HCI_EV_FIXED_ADDRESS_ATTEMPT);
	HCI_EV_DECODE(HCI_EV_SYNC_CONN_COMPLETE);
	HCI_EV_DECODE(HCI_EV_SYNC_CONN_CHANGED);
	HCI_EV_DECODE(HCI_EV_SNIFF_SUB_RATE);
	HCI_EV_DECODE(HCI_EV_EXTENDED_INQUIRY_RESULT);
	HCI_EV_DECODE(HCI_EV_ENCRYPTION_KEY_REFRESH_COMPLETE);
	HCI_EV_DECODE(HCI_EV_IO_CAPABILITY_REQUEST);
	HCI_EV_DECODE(HCI_EV_IO_CAPABILITY_RESPONSE);
	HCI_EV_DECODE(HCI_EV_USER_CONFIRMATION_REQUEST);
	HCI_EV_DECODE(HCI_EV_USER_PASSKEY_REQUEST);
	HCI_EV_DECODE(HCI_EV_REMOTE_OOB_DATA_REQUEST);
	HCI_EV_DECODE(HCI_EV_SIMPLE_PAIRING_COMPLETE);
	HCI_EV_DECODE(HCI_EV_LST_CHANGE);
	HCI_EV_DECODE(HCI_EV_ENHANCED_FLUSH_COMPLETE);
	HCI_EV_DECODE(HCI_EV_USER_PASSKEY_NOTIFICATION);
	HCI_EV_DECODE(HCI_EV_KEYPRESS_NOTIFICATION);
	HCI_EV_DECODE(HCI_EV_REM_HOST_SUPPORTED_FEATURES);
	HCI_EV_DECODE(HCI_EV_TRIGGERED_CLOCK_CAPTURE);
	HCI_EV_DECODE(HCI_EV_SYNCHRONIZATION_TRAIN_COMPLETE);
	HCI_EV_DECODE(HCI_EV_SYNCHRONIZATION_TRAIN_RECEIVED);
	HCI_EV_DECODE(HCI_EV_CSB_RECEIVE);
	HCI_EV_DECODE(HCI_EV_CSB_TIMEOUT);
	HCI_EV_DECODE(HCI_EV_TRUNCATED_PAGE_COMPLETE);
	HCI_EV_DECODE(HCI_EV_SLAVE_PAGE_RESPONSE_TIMEOUT);
	HCI_EV_DECODE(HCI_EV_CSB_CHANNEL_MAP_CHANGE);
	HCI_EV_DECODE(HCI_EV_INQUIRY_RESPONSE_NOTIFICATION);
	HCI_EV_DECODE(HCI_EV_AUTHENTICATED_PAYLOAD_TIMEOUT_EXPIRED);
	case HCI_EV_ULP:
	{
		switch (hci_ulp_sub_code) {
		HCI_EV_DECODE(HCI_EV_ULP_CONNECTION_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_ADVERTISING_REPORT);
		HCI_EV_DECODE(HCI_EV_ULP_CONNECTION_UPDATE_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_READ_REMOTE_USED_FEATURES_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_LONG_TERM_KEY_REQUEST);
		HCI_EV_DECODE(HCI_EV_ULP_REMOTE_CONNECTION_PARAMETER_REQUEST);
		HCI_EV_DECODE(HCI_EV_ULP_DATA_LENGTH_CHANGE);
		HCI_EV_DECODE(HCI_EV_ULP_READ_LOCAL_P256_PUB_KEY_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_GENERATE_DHKEY_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_ENHANCED_CONNECTION_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_DIRECT_ADVERTISING_REPORT);
		HCI_EV_DECODE(HCI_EV_ULP_PHY_UPDATE_COMPLETE);
		HCI_EV_DECODE(HCI_EV_ULP_USED_CHANNEL_SELECTION);
		}
		break;
	}
	}

	return ret;
}
#endif

static ssize_t scsc_iq_report_evt_read(char __user *buf, size_t len)
{
	ssize_t consumed = 0;
	ssize_t ret = 0;

	/* Calculate the amount of data that can be transferred */
	len = min(h4_iq_report_evt_len - bt_service.read_offset, len);

	SCSC_TAG_DEBUG(BT_H4, "SCSC_IQ_REPORT_EVT_READ: td(h4_iq_len=%u offset=%u)\n",
								h4_iq_report_evt_len,
								bt_service.read_offset);

	/* Copy the data to the user buffer */
	ret = copy_to_user(buf, &h4_iq_report_evt[bt_service.read_offset], len);
	if (ret == 0) {
		/* All good - Update our consumed information */
		bt_service.read_offset += len;
		consumed = len;

		SCSC_TAG_DEBUG(BT_H4, "SCSC_IQ_REPORT_EVT_READ: (offset=%u consumed: %u)\n",
									bt_service.read_offset,
									consumed);

		/* Have all data been copied to the userspace buffer */
		if (bt_service.read_offset == h4_iq_report_evt_len) {
			/* All good - read operation is completed */
			bt_service.read_offset = 0;
			bt_service.read_operation = BT_READ_OP_NONE;
		}
	} else {
		SCSC_TAG_ERR(BT_H4, "copy_to_user returned: %zu\n", ret);
		ret = -EACCES;
	}

	return ret == 0 ? consumed : ret;
}

static ssize_t scsc_hci_evt_read(char __user *buf, size_t len)
{
	struct BSMHCP_TD_HCI_EVT *td = &bt_service.bsmhcp_protocol->hci_evt_transfer_ring[bt_service.read_index];
	u8                       h4_hci_event_header = HCI_EVENT_PKT;
	ssize_t                  consumed = 0;
	ssize_t                  ret = 0;

	SCSC_TAG_DEBUG(BT_H4, "td (length=%u, hci_connection_handle=0x%03x, event_type=%u), len=%zu, read_offset=%zu\n",
		       td->length, td->hci_connection_handle, td->event_type, len, bt_service.read_offset);

	/* Is this the start of the copy operation */
	if (0 == bt_service.read_offset) {
		SCSC_TAG_DEBUG(BT_RX, "HCI Event [type=%s (0x%02x), length=%u]\n",
			scsc_hci_evt_decode_event_code(td->data[0], td->data[2]), td->data[0], td->data[1]);

		if (td->data[1] + HCI_EVENT_HEADER_LENGTH != td->length) {
			SCSC_TAG_ERR(BT_H4, "Firmware sent invalid HCI event\n");
			atomic_inc(&bt_service.error_count);
			ret = -EFAULT;
		}

		/* Store the H4 header in the user buffer */
		ret = copy_to_user(buf, &h4_hci_event_header, sizeof(h4_hci_event_header));
		if (0 == ret) {
			/* All good - Update our consumed information */
			consumed = sizeof(h4_hci_event_header);
			bt_service.read_offset = sizeof(h4_hci_event_header);
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	/* Can more data be put into the userspace buffer */
	if (0 == ret && (len - consumed)) {
		/* Calculate the amount of data that can be transferred */
		len = min((td->length - (bt_service.read_offset - sizeof(h4_hci_event_header))), (len - consumed));

		/* Copy the data to the user buffer */
		ret = copy_to_user(&buf[consumed], &td->data[bt_service.read_offset - sizeof(h4_hci_event_header)], len);
		if (0 == ret) {
			/* All good - Update our consumed information */
			bt_service.read_offset += len;
			consumed += len;

			/* Have all data been copied to the userspace buffer */
			if (bt_service.read_offset == (sizeof(h4_hci_event_header) + td->length)) {
				/* All good - read operation is completed */
				bt_service.read_offset = 0;
				bt_service.read_operation = BT_READ_OP_NONE;
			}
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	return 0 == ret ? consumed : ret;
}

static ssize_t scsc_hci_evt_error_read(char __user *buf, size_t len)
{
	ssize_t ret;
	ssize_t consumed = 0;

	/* Calculate the amount of data that can be transferred */
	len = min(sizeof(h4_hci_event_hardware_error) - bt_service.read_offset, len);

	/* Copy the data to the user buffer */
	ret = copy_to_user(buf, &h4_hci_event_hardware_error[bt_service.read_offset], len);
	if (0 == ret) {
		/* All good - Update our consumed information */
		bt_service.read_offset += len;
		consumed = len;

		/* Have all data been copied to the userspace buffer */
		if (bt_service.read_offset == sizeof(h4_hci_event_hardware_error)) {
			/* All good - read operation is completed */
			bt_service.read_offset = 0;
			bt_service.read_operation = BT_READ_OP_NONE;
#ifdef CONFIG_SCSC_LOG_COLLECTION
			if (bt_service.recovery_level == 0)
				scsc_log_collector_schedule_collection(
					SCSC_LOG_HOST_BT,
					SCSC_LOG_HOST_BT_REASON_HCI_ERROR);
#endif
		}
	} else {
		SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
		ret = -EACCES;
	}

	return 0 == ret ? consumed : ret;
}

static ssize_t scsc_acl_read(char __user *buf, size_t len)
{
	struct BSMHCP_TD_ACL_RX *td = &bt_service.bsmhcp_protocol->acl_rx_transfer_ring[bt_service.read_index];
	ssize_t                 consumed = 0;
	size_t                  copy_len = 0;
	ssize_t                 ret = 0;

	SCSC_TAG_DEBUG(BT_H4, "td (length=%u, hci_connection_handle=0x%03x, packet_boundary=%u, broadcast_flag=%u), len=%zu, read_offset=%zu\n",
		       td->length, td->hci_connection_handle, td->packet_boundary, td->broadcast_flag, len, bt_service.read_offset);

	/* Has the header been copied to userspace */
	if (bt_service.read_offset < sizeof(h4_acl_header)) {
		/* Calculate the amount of data that can be transferred */
		copy_len = min(sizeof(h4_acl_header) - bt_service.read_offset, len);

		/* Fully generate the H4 header + ACL data header regardless of the available amount of user memory */
		h4_acl_header[0] = HCI_ACLDATA_PKT;
		h4_acl_header[1] = td->hci_connection_handle & 0x00ff;
		h4_acl_header[2] = ((td->hci_connection_handle & 0x0f00) >> 8) | ((td->packet_boundary & 0x03) << 4) | ((td->broadcast_flag & 0x03) << 6);
		h4_acl_header[3] = td->length & 0x00ff;
		h4_acl_header[4] = (td->length & 0xff00) >> 8;

		/* Copy the H4 header + ACL data header to the userspace buffer */
		ret = copy_to_user(buf, &h4_acl_header[bt_service.read_offset], copy_len);
		if (0 == ret) {
			/* All good - Update our consumed information */
			consumed = copy_len;
			bt_service.read_offset += copy_len;
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	/* Can more data be put into the userspace buffer */
	if (0 == ret && bt_service.read_offset >= sizeof(h4_acl_header) && (len - consumed)) {
		/* Calculate the amount of data that can be transferred */
		copy_len = min((td->length - (bt_service.read_offset - sizeof(h4_acl_header))), (len - consumed));

		/* Copy the data to the user buffer */
		ret = copy_to_user(&buf[consumed], &td->data[bt_service.read_offset - sizeof(h4_acl_header)], copy_len);
		if (0 == ret) {
			/* All good - Update our consumed information */
			bt_service.read_offset += copy_len;
			consumed += copy_len;

			/* Have all data been copied to the userspace buffer */
			if (bt_service.read_offset == (sizeof(h4_acl_header) + td->length)) {
				/* All good - read operation is completed */
				bt_service.read_offset = 0;
				bt_service.read_operation = BT_READ_OP_NONE;

				/* Only supported on start packet*/
				if (td->packet_boundary == HCI_ACL_PACKET_BOUNDARY_START_FLUSH)
					/* The "false" argument is to tell the detection that this is RX */
					scsc_avdtp_detect_rxtx(td->hci_connection_handle, td->data, td->length, false);
			}
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	SCSC_TAG_DEBUG(BT_H4, "read_offset=%zu, consumed=%zu, ret=%zd, len=%zu, copy_len=%zu\n",
		       bt_service.read_offset, consumed, ret, len, copy_len);

	return 0 == ret ? consumed : ret;
}

static ssize_t scsc_acl_credit(char __user *buf, size_t len)
{
	ssize_t                      consumed = 0;
	ssize_t                      ret = 0;

	SCSC_TAG_DEBUG(BT_H4, "len=%zu, read_offset=%zu\n", len, bt_service.read_offset);

	/* Calculate the amount of data that can be transferred */
	len = min(h4_hci_event_ncp_header_len - bt_service.read_offset, len);

	/* Copy the data to the user buffer */
	ret = copy_to_user(buf, &h4_hci_event_ncp_header[bt_service.read_offset], len);
	if (0 == ret) {
		/* All good - Update our consumed information */
		bt_service.read_offset += len;
		consumed = len;

		/* Have all data been copied to the userspace buffer */
		if (bt_service.read_offset == h4_hci_event_ncp_header_len) {
			/* All good - read operation is completed */
			bt_service.read_offset = 0;
			bt_service.read_operation = BT_READ_OP_NONE;
		}
	} else {
		SCSC_TAG_WARNING(BT_H4, "copy_to_user returned: %zu\n", ret);
		ret = -EACCES;
	}

	return 0 == ret ? consumed : ret;
}

static ssize_t scsc_bt_shm_h4_read_continue(char __user *buf, size_t len)
{
	ssize_t ret = 0;

	/* Is a HCI event read operation ongoing */
	if (BT_READ_OP_HCI_EVT == bt_service.read_operation) {
		SCSC_TAG_DEBUG(BT_H4, "BT_READ_OP_HCI_EVT\n");

		/* Copy data into the userspace buffer */
		ret = scsc_hci_evt_read(buf, len);
		if (BT_READ_OP_NONE == bt_service.read_operation)
			/* All done - increase the read pointer and continue
			 * unless this was an out-of-order read for the queue
			 * sync helper */
			if (bt_service.read_index == bt_service.mailbox_hci_evt_read)
				BSMHCP_INCREASE_INDEX(bt_service.mailbox_hci_evt_read, BSMHCP_TRANSFER_RING_EVT_SIZE);
		/* Is a ACL data read operation ongoing */
	} else if (BT_READ_OP_ACL_DATA == bt_service.read_operation) {
		SCSC_TAG_DEBUG(BT_H4, "BT_READ_OP_ACL_DATA\n");

		/* Copy data into the userspace buffer */
		ret = scsc_acl_read(buf, len);
		if (BT_READ_OP_NONE == bt_service.read_operation)
			/* All done - increase the read pointer and continue */
			BSMHCP_INCREASE_INDEX(bt_service.mailbox_acl_rx_read, BSMHCP_TRANSFER_RING_ACL_SIZE);
		/* Is a ACL credit update operation ongoing */
	} else if (BT_READ_OP_ACL_CREDIT == bt_service.read_operation) {
		SCSC_TAG_DEBUG(BT_H4, "BT_READ_OP_ACL_CREDIT\n");

		/* Copy data into the userspace buffer */
		ret = scsc_acl_credit(buf, len);
	} else if (bt_service.read_operation == BT_READ_OP_IQ_REPORT) {
		SCSC_TAG_DEBUG(BT_H4, "BT_READ_OP_IQ_REPORT\n");

		/* Copy data into the userspace buffer */
		ret = scsc_iq_report_evt_read(buf, len);
		if (bt_service.read_operation == BT_READ_OP_NONE)
			/* All done - increase the read pointer and continue */
			BSMHCP_INCREASE_INDEX(bt_service.mailbox_iq_report_read, BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE);
	} else if (BT_READ_OP_HCI_EVT_ERROR == bt_service.read_operation) {
		SCSC_TAG_ERR(BT_H4, "BT_READ_OP_HCI_EVT_ERROR\n");

		/* Copy data into the userspace buffer */
		ret = scsc_hci_evt_error_read(buf, len);
		if (BT_READ_OP_NONE == bt_service.read_operation)
			/* All done - set the stop condition */
			bt_service.read_operation = BT_READ_OP_STOP;
	} else if (BT_READ_OP_STOP == bt_service.read_operation)
		ret = -EIO;

	return ret;
}

static ssize_t scsc_bt_shm_h4_read_iq_report_evt(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	if (bt_service.read_operation == BT_READ_OP_NONE &&
	    bt_service.mailbox_iq_report_read != bt_service.mailbox_iq_report_write) {
		struct BSMHCP_TD_IQ_REPORTING_EVT *td =
			&bt_service.bsmhcp_protocol->iq_reporting_transfer_ring[bt_service.mailbox_iq_report_read];
		u32 index = 0;
		u32 j = 0;
		u32 i;

		if (!bt_service.iq_reports_enabled)
		{
			BSMHCP_INCREASE_INDEX(bt_service.mailbox_iq_report_read,
			BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE);
		}
		else
		{
			memset(h4_iq_report_evt, 0, sizeof(h4_iq_report_evt));
			h4_iq_report_evt_len = 0;

			h4_iq_report_evt[index++] = HCI_EVENT_PKT;
			h4_iq_report_evt[index++] = 0x3E;
			index++; /* Leaving room for total length of params  */
			h4_iq_report_evt[index++] = td->subevent_code;

			if (td->subevent_code == HCI_LE_CONNECTIONLESS_IQ_REPORT_EVENT_SUB_CODE) {
				/* LE Connectionless IQ Report Event*/
				h4_iq_report_evt[index++] = td->sync_handle & 0xFF;
				h4_iq_report_evt[index++] = (td->sync_handle >> 8) & 0xFF;
			} else if (td->subevent_code == HCI_LE_CONNECTION_IQ_REPORT_EVENT_SUB_CODE) {
				/* LE connection IQ Report Event */
				h4_iq_report_evt[index++] = td->connection_handle & 0xFF;
				h4_iq_report_evt[index++] = (td->connection_handle >> 8) & 0xFF;
				h4_iq_report_evt[index++] = td->rx_phy;

			}
			h4_iq_report_evt[index++] = td->channel_index;
			h4_iq_report_evt[index++] = td->rssi & 0xFF;
			h4_iq_report_evt[index++] = (td->rssi >> 8) & 0xFF;
			h4_iq_report_evt[index++] = td->rssi_antenna_id;
			h4_iq_report_evt[index++] = td->cte_type;
			h4_iq_report_evt[index++] = td->slot_durations;
			h4_iq_report_evt[index++] = td->packet_status;
			h4_iq_report_evt[index++] = td->event_count & 0xFF;
			h4_iq_report_evt[index++] = (td->event_count >> 8) & 0xFF;
			h4_iq_report_evt[index++] = td->sample_count;

			/* Total length of hci event */
			h4_iq_report_evt_len = index + (2 * td->sample_count);

			/* Total length of hci event parameters */
			h4_iq_report_evt[2] = h4_iq_report_evt_len - 3;

			for (i = 0; i < td->sample_count; i++) {
				h4_iq_report_evt[index + i] = td->data[j++];
				h4_iq_report_evt[(index + td->sample_count) + i] = td->data[j++];
			}

			bt_service.read_operation = BT_READ_OP_IQ_REPORT;
			bt_service.read_index = bt_service.mailbox_iq_report_read;

			ret = scsc_iq_report_evt_read(&buf[consumed], len - consumed);
			if (ret > 0) {
				/* All good - Update our consumed information */
				consumed += ret;
				ret = 0;

				/**
				 * Update the index if all the data could be copied to the userspace
				 * buffer otherwise stop processing the HCI events
				 */
				if (bt_service.read_operation == BT_READ_OP_NONE)
					BSMHCP_INCREASE_INDEX(bt_service.mailbox_iq_report_read,
									  BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE);
			}
		}
	}

	return ret == 0 ? consumed : ret;
}

static ssize_t scsc_bt_shm_h4_read_hci_evt(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	while (BT_READ_OP_NONE == bt_service.read_operation && 0 == ret && !bt_service.hci_event_paused && bt_service.mailbox_hci_evt_read != bt_service.mailbox_hci_evt_write) {
		struct BSMHCP_TD_HCI_EVT *td = &bt_service.bsmhcp_protocol->hci_evt_transfer_ring[bt_service.mailbox_hci_evt_read];

		/* This event has already been processed - skip it */
		if (bt_service.processed[bt_service.mailbox_hci_evt_read]) {
			bt_service.processed[bt_service.mailbox_hci_evt_read] = false;
			BSMHCP_INCREASE_INDEX(bt_service.mailbox_hci_evt_read, BSMHCP_TRANSFER_RING_EVT_SIZE);
			continue;
		}

		/* A connection event has been detected by the firmware */
		if (td->event_type == BSMHCP_EVENT_TYPE_CONNECTED) {
			/* Sanity check of the HCI connection handle */
			if (td->hci_connection_handle >= SCSC_BT_CONNECTION_INFO_MAX) {
				SCSC_TAG_ERR(BT_H4, "connection handle is beyond max (hci_connection_handle=0x%03x)\n",
					     td->hci_connection_handle);
				atomic_inc(&bt_service.error_count);
				break;
			}

			SCSC_TAG_DEBUG(BT_H4, "connected (hci_connection_handle=0x%03x, state=%u)\n",
				       td->hci_connection_handle, bt_service.connection_handle_list[td->hci_connection_handle].state);

			/* Update the connection table to mark it as active */
			bt_service.connection_handle_list[td->hci_connection_handle].state = CONNECTION_ACTIVE;
			bt_service.connection_handle_list[td->hci_connection_handle].length = 0;

			/* ACL data processing can now continue */
			bt_service.acldata_paused = false;

			/* A disconnection event has been detected by the firmware */
		} else if (td->event_type == BSMHCP_EVENT_TYPE_DISCONNECTED) {
			SCSC_TAG_DEBUG(BT_H4, "disconnected (hci_connection_handle=0x%03x, state=%u)\n",
				       td->hci_connection_handle, bt_service.connection_handle_list[td->hci_connection_handle].state);

			/* If this ACL connection had an avdtp stream, mark it gone and interrupt the bg */
			if (scsc_avdtp_detect_reset_connection_handle(td->hci_connection_handle))
				wmb();

			/* If the connection is marked as active the ACL disconnect packet hasn't yet arrived */
			if (CONNECTION_ACTIVE == bt_service.connection_handle_list[td->hci_connection_handle].state) {
				/* Pause the HCI event procssing until the ACL disconnect packet arrives */
				bt_service.hci_event_paused = true;
				break;
			}

			/* Firmware does not have more ACL data - Mark the connection as inactive */
			bt_service.connection_handle_list[td->hci_connection_handle].state = CONNECTION_NONE;
		} else if (td->event_type == BSMHCP_EVENT_TYPE_IQ_REPORT_ENABLED) {
			bt_service.iq_reports_enabled = true;
		} else if (td->event_type == BSMHCP_EVENT_TYPE_IQ_REPORT_DISABLED) {
			bt_service.iq_reports_enabled = false;
		}

		/* Start a HCI event copy to userspace */
		bt_service.read_operation = BT_READ_OP_HCI_EVT;
		bt_service.read_index = bt_service.mailbox_hci_evt_read;
		ret = scsc_hci_evt_read(&buf[consumed], len - consumed);
		if (ret > 0) {
			/* All good - Update our consumed information */
			consumed += ret;
			ret = 0;

			/* Update the index if all the data could be copied to the userspace buffer otherwise stop processing the HCI events */
			if (BT_READ_OP_NONE == bt_service.read_operation)
				BSMHCP_INCREASE_INDEX(bt_service.mailbox_hci_evt_read, BSMHCP_TRANSFER_RING_EVT_SIZE);
			else
				break;
		}
	}

	return 0 == ret ? consumed : ret;
}

/**
 * Start the acl data to userspace copy
 *
 * Acl processing should be stopped if either unable to read a complete packet
 * or a complete packet is read and BlueZ is enabled
 *
 * @param[out]    ret      result of read operations written to here
 * @param[in,out] consumed read bytes added to this
 *
 * @return true if ACL data processing should stop
 */
static bool scsc_bt_shm_h4_acl_start_copy(char __user *buf,
	size_t len,
	ssize_t *ret,
	ssize_t *consumed)
{
	bt_service.read_operation = BT_READ_OP_ACL_DATA;
	bt_service.read_index = bt_service.mailbox_acl_rx_read;
	*ret = scsc_acl_read(&buf[*consumed], len - *consumed);
	if (*ret <= 0)
		return *ret < 0; /* Break the loop for errors */

	/* Update our consumed information */
	*consumed += *ret;
	*ret = 0;

	/* Stop processing if all the data could not be copied to userspace */
	if (bt_service.read_operation != BT_READ_OP_NONE)
		return true;

	BSMHCP_INCREASE_INDEX(bt_service.mailbox_acl_rx_read, BSMHCP_TRANSFER_RING_ACL_SIZE);

	return false;
}

static ssize_t scsc_bt_shm_h4_read_acl_data(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	while (bt_service.read_operation == BT_READ_OP_NONE &&
	       !bt_service.acldata_paused &&
	       bt_service.mailbox_acl_rx_read != bt_service.mailbox_acl_rx_write) {
		struct BSMHCP_TD_ACL_RX *td = &bt_service.bsmhcp_protocol->acl_rx_transfer_ring[bt_service.mailbox_acl_rx_read];

		/* Bypass packet inspection and connection handling for data dump */
		if (SCSC_BT_ACL_RAW == (td->hci_connection_handle & SCSC_BT_ACL_RAW_MASK)) {
			if (scsc_bt_shm_h4_acl_start_copy(buf, len, &ret, &consumed))
				break;
		}

		/* Sanity check of the HCI connection handle */
		if (td->hci_connection_handle >= SCSC_BT_CONNECTION_INFO_MAX) {
			SCSC_TAG_ERR(BT_H4, "connection handle is beyond max (hci_connection_handle=0x%03x)\n",
				     td->hci_connection_handle);
			atomic_inc(&bt_service.error_count);
			break;
		}

		/* Only process ACL data if the connection is marked active aka a HCI connection complete event has arrived */
		if (CONNECTION_ACTIVE == bt_service.connection_handle_list[td->hci_connection_handle].state) {
			/* Is this the final packet for the indicated ACL connection */
			if (td->disconnected) {
				SCSC_TAG_DEBUG(BT_H4, "ACL disconnected (hci_connection_handle=0x%03x, state=%u)\n",
					       td->hci_connection_handle, bt_service.connection_handle_list[td->hci_connection_handle].state);

				/* Update the connection table to mark it as disconnected */
				bt_service.connection_handle_list[td->hci_connection_handle].state = CONNECTION_DISCONNECTED;

				/* Clear the HCI event processing to allow for the HCI disconnect event to be transferred to userspace */
				bt_service.hci_event_paused = false;

				/* Update the read pointer */
				BSMHCP_INCREASE_INDEX(bt_service.mailbox_acl_rx_read, BSMHCP_TRANSFER_RING_ACL_SIZE);
			} else {
				if (scsc_bt_shm_h4_acl_start_copy(buf, len, &ret, &consumed))
					break;
			}
			/* If the connection state is inactive the HCI connection complete information hasn't yet arrived. Stop processing ACL data */
		} else if (CONNECTION_NONE == bt_service.connection_handle_list[td->hci_connection_handle].state) {
			SCSC_TAG_DEBUG(BT_H4, "ACL empty (hci_connection_handle=0x%03x, state=%u)\n",
				       td->hci_connection_handle, bt_service.connection_handle_list[td->hci_connection_handle].state);
			bt_service.acldata_paused = true;
			/* If the connection state is disconnection the firmware sent ACL after the ACL disconnect packet which is an FW error */
		} else {
			SCSC_TAG_ERR(BT_H4, "ACL data received after disconnected indication\n");
			atomic_inc(&bt_service.error_count);
			break;
		}
	}

	return 0 == ret ? consumed : ret;
}

static ssize_t scsc_bt_shm_h4_read_acl_credit(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	ssize_t consumed = 0;

	if (BT_READ_OP_NONE == bt_service.read_operation && 0 == ret &&
	    bt_service.mailbox_acl_free_read != bt_service.mailbox_acl_free_write) {
		u32 entries = 0;
		u32 index;

		memset(h4_hci_event_ncp_header, 0, sizeof(h4_hci_event_ncp_header));

		while (bt_service.mailbox_acl_free_read != bt_service.mailbox_acl_free_write) {
			struct BSMHCP_TD_ACL_TX_FREE *td =
			    &bt_service.bsmhcp_protocol->acl_tx_free_transfer_ring[bt_service.mailbox_acl_free_read];
			uint16_t sanitized_conn_handle = td->hci_connection_handle & SCSC_BT_ACL_HANDLE_MASK;

			if (bt_service.connection_handle_list[sanitized_conn_handle].state == CONNECTION_ACTIVE) {
				SCSC_TAG_DEBUG(BT_H4, "td (hci_connection_handle=0x%03x, buffer_index=%u)\n",
					td->hci_connection_handle & SCSC_BT_ACL_HANDLE_MASK, td->buffer_index);

				for (index = 0; index < BSMHCP_TRANSFER_RING_ACL_COUNT; index++) {
					if (0 == h4_hci_credit_entries[index].hci_connection_handle) {
						h4_hci_credit_entries[index].hci_connection_handle =
						    td->hci_connection_handle;
						h4_hci_credit_entries[index].credits = 1;
						entries++;
						break;
					} else if ((h4_hci_credit_entries[index].hci_connection_handle &
						    SCSC_BT_ACL_HANDLE_MASK) == sanitized_conn_handle) {
						h4_hci_credit_entries[index].hci_connection_handle =
						    td->hci_connection_handle;
						h4_hci_credit_entries[index].credits++;
						break;
					}
				}
			} else
				SCSC_TAG_WARNING(BT_H4,
					"No active connection ((hci_connection_handle=0x%03x)\n",
					sanitized_conn_handle);

			BSMHCP_INCREASE_INDEX(bt_service.mailbox_acl_free_read, BSMHCP_TRANSFER_RING_ACL_SIZE);
		}

		if (entries) {
			/* Fill the number of completed packets data into the temp buffer */
			h4_hci_event_ncp_header[0] = HCI_EVENT_PKT;
			h4_hci_event_ncp_header[1] = HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS_EVENT;
			h4_hci_event_ncp_header[2] = 1 + (4 * entries);                           /* Parameter length */
			h4_hci_event_ncp_header[3] = entries;                                     /* Number_of_Handles */
			h4_hci_event_ncp_header_len = 4 + (4 * entries);

			/* Start a ACL credit copy to userspace */
			bt_service.read_operation = BT_READ_OP_ACL_CREDIT;
			bt_service.read_index = bt_service.mailbox_acl_free_read;
			ret = scsc_acl_credit(&buf[consumed], len - consumed);
			if (ret > 0) {
				/* Update our consumed information */
				consumed += ret;
				ret = 0;
			}
		}
	}

	return 0 == ret ? consumed : ret;
}

ssize_t scsc_bt_shm_h4_queue_sync_helper(char __user *buf, size_t len)
{
	ssize_t ret = 0;
	bool    found = false;
	u32     mailbox_hci_evt_read = bt_service.mailbox_hci_evt_read;

	/* If both the HCI event transfer ring and ACL data transfer ring has been
	 * paused the entire HCI event transfer ring is scanned for the presence
	 * of the connected indication. Once present this is transferred to the host
	 * stack and marked as processed. This will unlock the hci event processing */
	while (bt_service.hci_event_paused && bt_service.acldata_paused) {
		struct BSMHCP_TD_ACL_RX *acl_td = &bt_service.bsmhcp_protocol->acl_rx_transfer_ring[bt_service.mailbox_acl_rx_read];

		while (mailbox_hci_evt_read != bt_service.mailbox_hci_evt_write) {
			struct BSMHCP_TD_HCI_EVT *td = &bt_service.bsmhcp_protocol->hci_evt_transfer_ring[mailbox_hci_evt_read];

			if (td->event_type & BSMHCP_EVENT_TYPE_CONNECTED && acl_td->hci_connection_handle == td->hci_connection_handle) {
				/* Update the connection table to mark it as active */
				bt_service.connection_handle_list[td->hci_connection_handle].state = CONNECTION_ACTIVE;
				bt_service.connection_handle_list[td->hci_connection_handle].length = 0;

				/* ACL data processing can now continue */
				bt_service.acldata_paused = false;

				/* Mark the event as processed */
				bt_service.processed[mailbox_hci_evt_read] = true;

				/* Indicate the event have been found */
				found = true;

				/* Start a HCI event copy to userspace */
				bt_service.read_operation = BT_READ_OP_HCI_EVT;
				bt_service.read_index = mailbox_hci_evt_read;
				ret = scsc_hci_evt_read(buf, len);
				break;
			}

			BSMHCP_INCREASE_INDEX(mailbox_hci_evt_read, BSMHCP_TRANSFER_RING_EVT_SIZE);
		}

		if (!found) {
			ret = wait_event_interruptible_timeout(bt_service.read_wait,
							       ((mailbox_hci_evt_read != bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write ||
								 0 != atomic_read(&bt_service.error_count) ||
								 bt_service.bsmhcp_protocol->header.panic_deathbed_confession)), HZ);
			if (0 == ret) {
				SCSC_TAG_ERR(BT_H4, "firmware didn't send the connected event within the given timeframe\n");
				atomic_inc(&bt_service.error_count);
				break;
			} else if (1 != ret) {
				SCSC_TAG_INFO(BT_H4, "user interrupt\n");
				break;
			}
		}
	}

	return ret;
}

ssize_t scsc_bt_shm_h4_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t consumed = 0;
	ssize_t ret = 0;
	ssize_t res;
	bool    gen_bg_int = false;
	bool    gen_fg_int = false;

	if (len == 0)
		return 0;

	/* Special handling in case read is called after service has closed */
	if (!bt_service.service_started)
		return -EIO;

	/* Only 1 reader is allowed */
	if (1 != atomic_inc_return(&bt_service.h4_readers)) {
		atomic_dec(&bt_service.h4_readers);
		return -EIO;
	}

	/* Update the cached variables with the non-cached variables */
	bt_service.mailbox_hci_evt_write  = bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write;
	bt_service.mailbox_acl_rx_write   = bt_service.bsmhcp_protocol->header.mailbox_acl_rx_write;
	bt_service.mailbox_acl_free_write = bt_service.bsmhcp_protocol->header.mailbox_acl_free_write;
	bt_service.mailbox_iq_report_write = bt_service.bsmhcp_protocol->header.mailbox_iq_report_write;

	/* Only generate the HCI hardware error event if any pending operation has been completed
	 * and the event hasn't already neen sent. This check assume the main while loop will exit
	 * on a completed operation in the next section */
	if (0 != atomic_read(&bt_service.error_count) && BT_READ_OP_NONE == bt_service.read_operation)
		bt_service.read_operation = BT_READ_OP_HCI_EVT_ERROR;

	/* put the remaining data from the transfer ring into the available userspace buffer */
	if (BT_READ_OP_NONE != bt_service.read_operation) {
		ret = scsc_bt_shm_h4_read_continue(buf, len);
		/* Update the consumed variable in case a operation was ongoing */
		if (0 < ret) {
			consumed = ret;
			ret = 0;
		}
	}

	/* Main loop - Can only be entered when no operation is present on entering this function
	 * or no hardware error has been detected. It loops until data has been placed in the
	 * userspace buffer or an error has been detected */
	while (0 == atomic_read(&bt_service.error_count) && 0 == consumed) {
		/* If both the HCI event processing and ACL data processing has been disabled this function
		 * helps exit this condition by scanning the HCI event queue for the connection established
		 * event and return it to userspace */
		ret = scsc_bt_shm_h4_queue_sync_helper(buf, len);
		if (ret > 0) {
			consumed = ret;
			break;
		}

		/* Does any of the read/write pairs differs */
		if ((bt_service.mailbox_hci_evt_read == bt_service.mailbox_hci_evt_write || bt_service.hci_event_paused) &&
		    (bt_service.mailbox_acl_rx_read == bt_service.mailbox_acl_rx_write || bt_service.acldata_paused) &&
		    bt_service.mailbox_acl_free_read == bt_service.mailbox_acl_free_write &&
		    bt_service.mailbox_iq_report_read == bt_service.mailbox_iq_report_write &&
		    0 == atomic_read(&bt_service.error_count) &&
		    0 == bt_service.bsmhcp_protocol->header.panic_deathbed_confession) {
			/* Don't wait if in NONBLOCK mode */
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			/* All read/write pairs are identical - wait for the firmware. The conditional
			 * check is used to verify that a read/write pair has actually changed */
			ret = wait_event_interruptible(
				bt_service.read_wait,
				((bt_service.mailbox_hci_evt_read !=
				  bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write &&
				  !bt_service.hci_event_paused) ||
				 (bt_service.mailbox_acl_rx_read !=
				  bt_service.bsmhcp_protocol->header.mailbox_acl_rx_write &&
				  !bt_service.acldata_paused) ||
				 (bt_service.mailbox_acl_free_read !=
				  bt_service.bsmhcp_protocol->header.mailbox_acl_free_write) ||
				 (bt_service.mailbox_iq_report_read !=
				  bt_service.bsmhcp_protocol->header.mailbox_iq_report_write) ||
				 atomic_read(&bt_service.error_count) != 0 ||
				 bt_service.bsmhcp_protocol->header.panic_deathbed_confession));

			/* Has an error been detected elsewhere in the driver then just return from this function */
			if (0 != atomic_read(&bt_service.error_count))
				break;

			/* Any failures is handled by the userspace application */
			if (ret)
				break;

			/* Refresh our write indexes before starting to process the protocol */
			bt_service.mailbox_hci_evt_write = bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write;
			bt_service.mailbox_acl_rx_write = bt_service.bsmhcp_protocol->header.mailbox_acl_rx_write;
			bt_service.mailbox_acl_free_write = bt_service.bsmhcp_protocol->header.mailbox_acl_free_write;
			bt_service.mailbox_iq_report_write = bt_service.bsmhcp_protocol->header.mailbox_iq_report_write;
		}

		SCSC_TAG_DEBUG(BT_H4, "hci_evt_read=%u, hci_evt_write=%u, acl_rx_read=%u,acl_rx_write=%u\n",
									bt_service.mailbox_hci_evt_read,
									bt_service.mailbox_hci_evt_write,
									bt_service.mailbox_acl_rx_read,
									bt_service.mailbox_acl_rx_write);

		SCSC_TAG_DEBUG(BT_H4, "acl_free_read=%u, acl_free_write=%u, iq_report_read=%u iq_report_write=%u\n",
									bt_service.mailbox_acl_free_read,
									bt_service.mailbox_acl_free_write,
									bt_service.mailbox_iq_report_read,
									bt_service.mailbox_iq_report_write);

		SCSC_TAG_DEBUG(BT_H4, "read_operation=%u, hci_event_paused=%u, acldata_paused=%u\n",
			       bt_service.read_operation, bt_service.hci_event_paused,
			       bt_service.acldata_paused);

		while (bt_service.mailbox_acl_free_read_scan != bt_service.mailbox_acl_free_write) {
			struct BSMHCP_TD_ACL_TX_FREE *td = &bt_service.bsmhcp_protocol->acl_tx_free_transfer_ring[bt_service.mailbox_acl_free_read_scan];

			/* Free the buffer in the allocation table */
			if (td->buffer_index < BSMHCP_DATA_BUFFER_TX_ACL_SIZE) {
				bt_service.allocated[td->buffer_index] = 0;
				bt_service.freed_count++;

				SCSC_TAG_DEBUG(BT_TX, "ACL[CREDIT] (index=%u, buffer=%u, credits=%u)\n",
					bt_service.mailbox_acl_free_read_scan,
					td->buffer_index,
					BSMHCP_DATA_BUFFER_TX_ACL_SIZE - (bt_service.allocated_count - bt_service.freed_count));
			}

			BSMHCP_INCREASE_INDEX(bt_service.mailbox_acl_free_read_scan, BSMHCP_TRANSFER_RING_ACL_SIZE);
		}

#ifdef CONFIG_SCSC_QOS
		/* Update the quality of service module with the number of used entries */
		scsc_bt_qos_update(BSMHCP_USED_ENTRIES(bt_service.mailbox_hci_evt_write,
		                                       bt_service.mailbox_hci_evt_read,
		                                       BSMHCP_TRANSFER_RING_EVT_SIZE),
		                   BSMHCP_USED_ENTRIES(bt_service.mailbox_acl_rx_write,
		                                       bt_service.mailbox_acl_rx_read,
		                                       BSMHCP_TRANSFER_RING_ACL_SIZE));
#endif

		/* First: process any pending HCI event that needs to be sent to userspace */
		res = scsc_bt_shm_h4_read_hci_evt(&buf[consumed], len - consumed);
		if (res < 0) {
			ret = res;
			break;
		}
		consumed += res;

		/* Second: process any pending ACL data that needs to be sent to userspace */
		res = scsc_bt_shm_h4_read_acl_data(&buf[consumed], len - consumed);
		if (res < 0) {
			ret = res;
			break;
		}
		consumed += res;

		/* Third: process any pending ACL data that needs to be sent to userspace */
		res = scsc_bt_shm_h4_read_acl_credit(&buf[consumed], len - consumed);
		if (res < 0) {
			ret = res;
			break;
		}
		consumed += res;

		res = scsc_bt_shm_h4_read_iq_report_evt(&buf[consumed], len - consumed);
		if (res < 0) {
			ret = res;
			break;
		}
		consumed += res;
	}

	if (0 == ret && 0 == consumed) {
		if (0 != atomic_read(&bt_service.error_count) && BT_READ_OP_NONE == bt_service.read_operation)
			bt_service.read_operation = BT_READ_OP_HCI_EVT_ERROR;

		if (BT_READ_OP_HCI_EVT_ERROR == bt_service.read_operation) {
			SCSC_TAG_ERR(BT_H4, "BT_READ_OP_HCI_EVT_ERROR\n");

			/* Copy data into the userspace buffer */
			ret = scsc_hci_evt_error_read(buf, len);
			if (ret > 0) {
				consumed += ret;
				ret = 0;
			}

			if (BT_READ_OP_NONE == bt_service.read_operation)
				/* All done - set the stop condition */
				bt_service.read_operation = BT_READ_OP_STOP;
		}
	}

	/* If anything was read, generate the appropriate interrupt(s) */
	if (bt_service.bsmhcp_protocol->header.mailbox_hci_evt_read !=
	    bt_service.mailbox_hci_evt_read)
		gen_bg_int = true;

	if (bt_service.bsmhcp_protocol->header.mailbox_acl_rx_read !=
	    bt_service.mailbox_acl_rx_read ||
	    bt_service.bsmhcp_protocol->header.mailbox_acl_free_read !=
	    bt_service.mailbox_acl_free_read)
		gen_fg_int = true;

	if (bt_service.bsmhcp_protocol->header.mailbox_iq_report_read !=
	    bt_service.mailbox_iq_report_read)
		gen_fg_int = true;


	/* Update the read index for all transfer rings */
	bt_service.bsmhcp_protocol->header.mailbox_hci_evt_read = bt_service.mailbox_hci_evt_read;
	bt_service.bsmhcp_protocol->header.mailbox_acl_rx_read = bt_service.mailbox_acl_rx_read;
	bt_service.bsmhcp_protocol->header.mailbox_acl_free_read = bt_service.mailbox_acl_free_read;
	bt_service.bsmhcp_protocol->header.mailbox_iq_report_read = bt_service.mailbox_iq_report_read;

	/* Ensure the data is updating correctly in memory */
	wmb();

	if (gen_bg_int)
		scsc_service_mifintrbit_bit_set(bt_service.service, bt_service.bsmhcp_protocol->header.ap_to_bg_int_src, SCSC_MIFINTR_TARGET_R4);

	if (gen_fg_int)
		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(bt_service.service,
						bt_service.bsmhcp_protocol->header.ap_to_fg_int_src, SCSC_MIFINTR_TARGET_R4);

	if (BT_READ_OP_STOP != bt_service.read_operation)
		SCSC_TAG_DEBUG(BT_H4, "hci_evt_read=%u, acl_rx_read=%u, acl_free_read=%u, read_operation=%u, consumed=%zd, ret=%zd\n",
			       bt_service.mailbox_hci_evt_read, bt_service.mailbox_acl_rx_read, bt_service.mailbox_acl_free_read, bt_service.read_operation, consumed, ret);

	/* Decrease the H4 readers counter */
	atomic_dec(&bt_service.h4_readers);

	return 0 == ret ? consumed : ret;
}

ssize_t scsc_bt_shm_h4_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	size_t  length;
	size_t  hci_pkt_len;
	ssize_t written = 0;
	ssize_t ret = 0;

	SCSC_TAG_DEBUG(BT_H4, "enter\n");

	UNUSED(file);
	UNUSED(offset);

	/* Don't allow any writes after service has been closed */
	if (!bt_service.service_started)
		return -EIO;

	/* Only 1 writer is allowed */
	if (1 != atomic_inc_return(&bt_service.h4_writers)) {
		atomic_dec(&bt_service.h4_writers);
		return -EIO;
	}

	/* Has en error been detect then just return with an error */
	if (0 != atomic_read(&bt_service.error_count)) {
		/* SCSC_TAG_WARNING(BT_H4, "firmware panicked or protocol error (error_count=%u)\n", atomic_read(&bt_service.error_count));*/
		atomic_dec(&bt_service.h4_writers);
		return -EIO;
	}

	while (written != count && 0 == ret) {
		length = min(count - written, sizeof(h4_write_buffer) - bt_service.h4_write_offset);
		SCSC_TAG_DEBUG(BT_H4, "count: %zu, length: %zu, h4_write_offset: %zu, written:%zu, size:%zu\n",
			       count, length, bt_service.h4_write_offset, written, sizeof(h4_write_buffer));

		/* Is there room in the temp buffer */
		if (0 == length) {
			SCSC_TAG_ERR(BT_H4, "no room in the buffer\n");
			atomic_inc(&bt_service.error_count);
			ret = -EIO;
			break;
		}

		/* Copy the userspace data to the target buffer */
		ret = copy_from_user(&h4_write_buffer[bt_service.h4_write_offset], &buf[written], length);
		if (0 == ret) {
			/* Is there enough data to include a HCI command header and is the type a HCI_COMMAND_PKT */
			if ((length + bt_service.h4_write_offset) >= H4DMUX_HEADER_HCI && HCI_COMMAND_PKT == h4_write_buffer[0]) {
				/* Extract the HCI command packet length */
				hci_pkt_len = h4_write_buffer[3] + 3;

				/* Is it a complete packet available */
				if ((hci_pkt_len + 1) <= (length + bt_service.h4_write_offset)) {
					/* Transfer the packet to the HCI command transfer ring */
					ret = scsc_bt_shm_h4_hci_cmd_write(&h4_write_buffer[1], hci_pkt_len);
					if (ret >= 0) {
						written += ((hci_pkt_len + 1) - bt_service.h4_write_offset);
						bt_service.h4_write_offset = 0;
						ret = 0;
					}
				} else {
					/* Still needing data to have the complete packet */
					SCSC_TAG_WARNING(BT_H4, "missing data (need=%zu, got=%zu)\n", (hci_pkt_len + 1), (length + bt_service.h4_write_offset));
					written += length;
					bt_service.h4_write_offset += (u32) length;
				}
				/* Is there enough data to include a ACL data header and is the type a HCI_ACLDATA_PKT */
			} else if ((length + bt_service.h4_write_offset) >= H4DMUX_HEADER_ACL && HCI_ACLDATA_PKT == h4_write_buffer[0]) {
				/* Extract the ACL data packet length */
				hci_pkt_len = (h4_write_buffer[3] | (h4_write_buffer[4] << 8));

				/* Sanity check on the packet length */
				if (hci_pkt_len > BSMHCP_ACL_PACKET_SIZE) {
					SCSC_TAG_ERR(BT_H4, "ACL packet length is larger than read buffer size specifies (%zu > %u)\n", hci_pkt_len, BSMHCP_ACL_PACKET_SIZE);
					atomic_inc(&bt_service.error_count);
					ret = -EIO;
					break;
				}

				/* Is it a complete packet available */
				if ((hci_pkt_len + 5) <= (length + bt_service.h4_write_offset)) {
					/* Transfer the packet to the ACL data transfer ring */
					ret = scsc_bt_shm_h4_acl_write(&h4_write_buffer[1], hci_pkt_len + 4);
					if (ret >= 0) {
						written += ((hci_pkt_len + 5) - bt_service.h4_write_offset);
						bt_service.h4_write_offset = 0;
						ret = 0;
					}
				} else {
					/* Still needing data to have the complete packet */
					SCSC_TAG_WARNING(BT_H4, "missing data (need=%zu, got=%zu)\n", (hci_pkt_len + 5), (length - bt_service.h4_write_offset));
					written += length;
					bt_service.h4_write_offset += (u32) length;
				}
				/* Is there less data than a header then just wait for more */
			} else if (length <= 5) {
				bt_service.h4_write_offset += length;
				written += length;
				/* Header is unknown - unable to proceed */
			} else {
				atomic_inc(&bt_service.error_count);
				ret = -EIO;
			}
		} else {
			SCSC_TAG_WARNING(BT_H4, "copy_from_user returned: %zu\n", ret);
			ret = -EACCES;
		}
	}

	SCSC_TAG_DEBUG(BT_H4, "h4_write_offset=%zu, ret=%zu, written=%zu\n",
		       bt_service.h4_write_offset, ret, written);

	/* Decrease the H4 readers counter */
	atomic_dec(&bt_service.h4_writers);

	return 0 == ret ? written : ret;
}

unsigned scsc_bt_shm_h4_poll(struct file *file, poll_table *wait)
{
	/* Add the wait queue to the polling queue */
	poll_wait(file, &bt_service.read_wait, wait);

	/* Return immediately if service has been closed */
	if (!bt_service.service_started)
		return POLLOUT;

	/* Has en error been detect then just return with an error */
	if (((bt_service.bsmhcp_protocol->header.mailbox_hci_evt_write !=
	      bt_service.bsmhcp_protocol->header.mailbox_hci_evt_read ||
	      bt_service.bsmhcp_protocol->header.mailbox_acl_rx_write !=
	      bt_service.bsmhcp_protocol->header.mailbox_acl_rx_read ||
	      bt_service.bsmhcp_protocol->header.mailbox_acl_free_write !=
	      bt_service.bsmhcp_protocol->header.mailbox_acl_free_read ||
	      bt_service.bsmhcp_protocol->header.mailbox_iq_report_write !=
	      bt_service.bsmhcp_protocol->header.mailbox_iq_report_read) &&
	     bt_service.read_operation != BT_READ_OP_STOP) ||
	    (bt_service.read_operation != BT_READ_OP_NONE &&
	     bt_service.read_operation != BT_READ_OP_STOP) ||
	    ((BT_READ_OP_STOP != bt_service.read_operation) &&
	     (0 != atomic_read(&bt_service.error_count) ||
	     bt_service.bsmhcp_protocol->header.panic_deathbed_confession))) {
		SCSC_TAG_DEBUG(BT_H4, "queue(s) changed\n");
		return POLLIN | POLLRDNORM; /* readeable */
	}

	SCSC_TAG_DEBUG(BT_H4, "no change\n");

	return POLLOUT; /* writeable */
}

/* Initialise the shared memory interface */
int scsc_bt_shm_init(void)
{
	/* Get kmem pointer to the shared memory ref */
	bt_service.bsmhcp_protocol = scsc_mx_service_mif_addr_to_ptr(bt_service.service, bt_service.bsmhcp_ref);
	if (bt_service.bsmhcp_protocol == NULL) {
		SCSC_TAG_ERR(BT_COMMON, "couldn't map kmem to shm_ref 0x%08x\n", (u32)bt_service.bsmhcp_ref);
		return -ENOMEM;
	}

	/* Clear the protocol shared memory area */
	memset(bt_service.bsmhcp_protocol, 0, sizeof(*bt_service.bsmhcp_protocol));
	bt_service.bsmhcp_protocol->header.magic_value = BSMHCP_PROTOCOL_MAGICVALUE;
	bt_service.mailbox_hci_evt_read = 0;
	bt_service.mailbox_acl_rx_read = 0;
	bt_service.mailbox_acl_free_read = 0;
	bt_service.mailbox_acl_free_read_scan = 0;
	bt_service.mailbox_iq_report_read = 0;
	bt_service.read_index = 0;
	bt_service.allocated_count = 0;
	bt_service.iq_reports_enabled = false;
	h4_irq_mask = 0;

	/* Initialise the interrupt handlers */
	if (scsc_bt_shm_init_interrupt() < 0) {
		SCSC_TAG_ERR(BT_COMMON, "Failed to register IRQ bits\n");
		return -EIO;
	}

	return 0;
}

/* Terminate the shared memory interface, stopping its thread.
 *
 * Note: The service must be stopped prior to calling this function.
 *       The shared memory can only be released after calling this function.
 */
void scsc_bt_shm_exit(void)
{
	u16 irq_num = 0;

	/* Release IRQs */
	if (bt_service.bsmhcp_protocol != NULL) {

		if (h4_irq_mask & 1 << irq_num++)
			scsc_service_mifintrbit_unregister_tohost(
			    bt_service.service, bt_service.bsmhcp_protocol->header.bg_to_ap_int_src);
		if (h4_irq_mask & 1 << irq_num++)
			scsc_service_mifintrbit_free_fromhost(
			    bt_service.service, bt_service.bsmhcp_protocol->header.ap_to_bg_int_src, SCSC_MIFINTR_TARGET_R4);
		if (h4_irq_mask & 1 << irq_num++)
			scsc_service_mifintrbit_free_fromhost(
			    bt_service.service, bt_service.bsmhcp_protocol->header.ap_to_fg_int_src, SCSC_MIFINTR_TARGET_R4);
	}

	/* Clear all control structures */
	bt_service.last_alloc = 0;
	bt_service.hci_event_paused = false;
	bt_service.acldata_paused = false;
	bt_service.bsmhcp_protocol = NULL;

	memset(bt_service.allocated, 0, sizeof(bt_service.allocated));
	memset(bt_service.connection_handle_list, 0, sizeof(bt_service.connection_handle_list));
}

