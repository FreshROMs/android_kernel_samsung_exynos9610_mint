/****************************************************************************
 *
 *       Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

/* Before submitting new changes to this file please make sure to run the module tests to verify
 * that the change didn't break anything. Also, make sure to write new tests that captures the
 * change. The module tests can be found in "vendor/samsung_slsi/scsc_tools/kernel_unit_test/"
 * from where there are run with "make". If needed its git project nane is:
 * "Connectivity/Android/platform/vendor/samsung_slsi/scsc_tools/kernel_unit_test" */

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
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
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
/**
 * Coex AVDTP detection.
 *
 * Strategy:
 *
 * - On the L2CAP signaling CID, look for connect requests with the AVDTP PSM
 *
 * - Assume the first AVDTP connection is the signaling channel.
 *   (AVDTP 1.3, section 5.4.6 "Transport and Signaling Channel Establishment")
 *
 * - If a signaling channel exists, assume the next connection is the streaming channel
 *
 * - If a streaming channel exists, look for AVDTP start, suspend, abort and close signals
 * -- When one of these is found, signal the FW with updated acl_id and cid
 *
 * - If the ACL is torn down, make sure to clean up.
 *
 * */

#define IS_VALID_CID_CONN_RESP(is_tx, avdtp, data) ((is_tx && avdtp->dst_cid == HCI_L2CAP_SOURCE_CID(data)) || \
													(!is_tx && avdtp->src_cid == HCI_L2CAP_SOURCE_CID(data)))


#define IS_VALID_CID_DISCONNECT_REQ(is_tx, avdtp, data) ((is_tx && avdtp.src_cid == HCI_L2CAP_SOURCE_CID(data) && \
														  avdtp.dst_cid == HCI_L2CAP_RSP_DEST_CID(data)) || \
														 (!is_tx && avdtp.src_cid == HCI_L2CAP_RSP_DEST_CID(data) && \
														  avdtp.dst_cid == HCI_L2CAP_SOURCE_CID(data)))

#define STORE_DETECTED_CID_CONN_REQ(is_tx, avdtp, data)                 \
	do {                                                                \
		if (is_tx) {                                                    \
			avdtp->src_cid = HCI_L2CAP_SOURCE_CID(data);                \
		} else {                                                        \
			avdtp->dst_cid = HCI_L2CAP_SOURCE_CID(data);                \
		}                                                               \
	} while (0)

#define STORE_DETECTED_CID_CONN_RESP(is_tx, avdtp, data)                \
	do {                                                                \
		if (is_tx) {                                                    \
			avdtp->src_cid = HCI_L2CAP_RSP_DEST_CID(data);              \
		} else {                                                        \
			avdtp->dst_cid = HCI_L2CAP_RSP_DEST_CID(data);              \
		}                                                               \
	} while (0)

#ifdef ENABLE_MODULE_TESTS_DEBUG
#undef SCSC_TAG_DEBUG
#define SCSC_TAG_DEBUG(tag, fmt, ...) \
	fprintf(stdout, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

/* Forward declarations */
void scsc_avdtp_detect_reset(struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
							 bool reset_signal,
							 bool reset_signal_ongoing,
							 bool reset_stream,
							 bool reset_stream_ongoing,
							 bool reset_local_seids,
							 bool reset_remote_seids);

/* Simple list traversal to find an existing AVDTP detection from a hci connection handle. If
 * not found, the function returns NULL
 */
static struct scsc_bt_avdtp_detect_hci_connection *scsc_avdtp_detect_search_hci_connection(
	u16 hci_connection_handle)
{
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci;

	avdtp_hci = bt_service.avdtp_detect.connections;
	while (avdtp_hci) {
		if (avdtp_hci->hci_connection_handle == hci_connection_handle) {
			/* Found it */
			break;
		}
		avdtp_hci = avdtp_hci->next;
	}
	return avdtp_hci;
}

/* Find existing detection for a given connection handle. If provided as argument, a new detection
 * will be created if it doesn't exist. The returned element is locked and must be unlocked before
 * moving to another context.
 */
static struct scsc_bt_avdtp_detect_hci_connection *scsc_avdtp_detect_find_or_create_hci_connection(
	u16 hci_connection_handle,
	bool create)
{
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci;

	spin_lock(&bt_service.avdtp_detect.lock);

	avdtp_hci = scsc_avdtp_detect_search_hci_connection(hci_connection_handle);
	if (avdtp_hci)
		spin_lock(&avdtp_hci->lock);

	/* Unlock the list again and check if a new element must be created. If that is the case,
	 * malloc the new memory and lock the list afterwards such that others are not spinning for
	 * some undefined amount of time. The trade-off in this construction is that the memory
	 * potentially is allocated twice in rare situations, and the list must therefore be locked
	 * and searched again before inserting.
	 */
	spin_unlock(&bt_service.avdtp_detect.lock);

	/* Check if the existing detection was found. If not create it */
	if (!avdtp_hci && create) {
		avdtp_hci = kmalloc(sizeof(struct scsc_bt_avdtp_detect_hci_connection), GFP_KERNEL);
		if (avdtp_hci) {
			struct scsc_bt_avdtp_detect_hci_connection *head;
			struct scsc_bt_avdtp_detect_hci_connection *recheck_avdtp_hci;

			memset(avdtp_hci, 0, sizeof(struct scsc_bt_avdtp_detect_hci_connection));
			avdtp_hci->signal.type = BT_AVDTP_CONN_TYPE_SIGNAL;
			avdtp_hci->stream.type = BT_AVDTP_CONN_TYPE_STREAM;
			avdtp_hci->ongoing.incoming_signal.type = BT_AVDTP_CONN_TYPE_SIGNAL;
			avdtp_hci->ongoing.outgoing_signal.type = BT_AVDTP_CONN_TYPE_SIGNAL;
			avdtp_hci->ongoing.incoming_stream.type = BT_AVDTP_CONN_TYPE_STREAM;
			avdtp_hci->ongoing.outgoing_stream.type = BT_AVDTP_CONN_TYPE_STREAM;
			avdtp_hci->signal.state = BT_AVDTP_STATE_IDLE_SIGNALING;
			avdtp_hci->signal.src_cid = 0;
			avdtp_hci->signal.dst_cid = 0;
			avdtp_hci->hci_connection_handle = hci_connection_handle;
			scsc_avdtp_detect_reset(avdtp_hci, false, true, true, true, true, true);

			/* The element is ready for insertion into the list. Recheck the list to make sure that
			 * the hci handle hasn't been detected meanwhile.
			 */
			spin_lock(&bt_service.avdtp_detect.lock);

			recheck_avdtp_hci = scsc_avdtp_detect_search_hci_connection(hci_connection_handle);
			if (recheck_avdtp_hci == NULL) {
				/* Insert into list */
				spin_lock_init(&avdtp_hci->lock);
				spin_lock(&avdtp_hci->lock);
				head = bt_service.avdtp_detect.connections;
				bt_service.avdtp_detect.connections = avdtp_hci;
				avdtp_hci->next = head;
				spin_unlock(&bt_service.avdtp_detect.lock);
			} else {
				/* The element was already present. Free the allocated memory and return the found
				 * element.
				 */
				spin_lock(&recheck_avdtp_hci->lock);
				spin_unlock(&bt_service.avdtp_detect.lock);
				kfree(avdtp_hci);
				avdtp_hci = NULL;
				avdtp_hci = recheck_avdtp_hci;
			}
		}
	}
	return avdtp_hci;
}

/* Find an existing l2cap connection struct. Works for both signal and stream since their internal
 * structures are equal */
static struct scsc_bt_avdtp_detect_connection *scsc_avdtp_detect_find_l2cap_connection(struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
																					   enum scsc_bt_avdtp_detect_conn_req_direction_enum direction)
{
	struct scsc_bt_avdtp_detect_connection *avdtp_l2cap = NULL;

	/* Return either signal or stream l2cap connection */
	if (avdtp_hci) {
		/* Check if there is already a signal connection and return the ongoing stream */
		if (avdtp_hci->signal.state == BT_AVDTP_STATE_COMPLETE_SIGNALING) {
			if (direction == BT_AVDTP_CONN_REQ_DIR_OUTGOING)
				avdtp_l2cap = &avdtp_hci->ongoing.outgoing_stream;
			else
				avdtp_l2cap = &avdtp_hci->ongoing.incoming_stream;
		} else {
			/* If not, use the ongoing signal */
			if (direction == BT_AVDTP_CONN_REQ_DIR_OUTGOING)
				avdtp_l2cap = &avdtp_hci->ongoing.outgoing_signal;
			else
				avdtp_l2cap = &avdtp_hci->ongoing.incoming_signal;
		}
	}
	return avdtp_l2cap;
}

/* Handle CONNECTION_REQUEST and detect signal or stream connections. The function handles both RX and TX, where
 * connect requests in TX direction is mapped to "outgoing". RX direction is mapped to "incoming" */
static void scsc_bt_avdtp_detect_connection_conn_req_handling(uint16_t hci_connection_handle,
															  const unsigned char *data,
															  uint16_t length,
															  bool is_tx)
{
	struct scsc_bt_avdtp_detect_connection *avdtp_l2cap = NULL;
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci = NULL;

	/* Ignore everything else than the PSM */
	if (HCI_L2CAP_CON_REQ_PSM(data) == L2CAP_AVDTP_PSM) {
		// Check if there is already an detection for the given hci handle
		avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(
			hci_connection_handle,
			true);

		avdtp_l2cap = scsc_avdtp_detect_find_l2cap_connection(avdtp_hci,
															  is_tx ? BT_AVDTP_CONN_REQ_DIR_OUTGOING : BT_AVDTP_CONN_REQ_DIR_INCOMING);

		if (avdtp_hci && avdtp_l2cap) {
			if (avdtp_l2cap->state == BT_AVDTP_STATE_IDLE_SIGNALING) {

				/* AVDTP signal channel was detected - store dst_cid or src_cid depending on the transmit
				 * direction, and store the connection_handle. */
				STORE_DETECTED_CID_CONN_REQ(is_tx, avdtp_l2cap, data);
				avdtp_l2cap->state = BT_AVDTP_STATE_PENDING_SIGNALING;
				SCSC_TAG_DEBUG(BT_H4, "Signaling dst CID: 0x%04X, src CID: 0x%04X, aclid: 0x%04X (tx=%u)\n",
							   avdtp_l2cap->dst_cid,
							   avdtp_l2cap->src_cid,
							   avdtp_hci->hci_connection_handle,
							   is_tx);
			} else if (avdtp_l2cap->state == BT_AVDTP_STATE_IDLE_STREAMING &&
					   avdtp_hci->signal.state == BT_AVDTP_STATE_COMPLETE_SIGNALING) {

				/* AVDTP stream channel was detected - store dst_cid or src_cid depending on the transmit
				 * direction. */
				STORE_DETECTED_CID_CONN_REQ(is_tx, avdtp_l2cap, data);
				avdtp_l2cap->state = BT_AVDTP_STATE_PENDING_STREAMING;
				SCSC_TAG_DEBUG(BT_H4, "Streaming dst CID: 0x%04X, src CID: 0x%04X, aclid: 0x%04X (%u)\n",
							   avdtp_l2cap->dst_cid,
							   avdtp_l2cap->src_cid,
							   avdtp_hci->hci_connection_handle,
							   is_tx);
			}
		}
		if (avdtp_hci)
			spin_unlock(&avdtp_hci->lock);
	}
}

/* Handle CONNECTION_REPSONS and detect signal or stream connections. The function handles both RX and TX, where
 * connect requests in TX direction is mapped to "incoming". RX direction is mapped to "outgoing" */
static void scsc_bt_avdtp_detect_connection_conn_resp_handling(uint16_t hci_connection_handle,
															   const unsigned char *data,
															   uint16_t length,
															   bool is_tx)
{

	/* Check if there is already a signal connection */
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci =
		scsc_avdtp_detect_find_or_create_hci_connection(hci_connection_handle, false);
	struct scsc_bt_avdtp_detect_connection *avdtp_l2cap =
		scsc_avdtp_detect_find_l2cap_connection(avdtp_hci,
												is_tx ? BT_AVDTP_CONN_REQ_DIR_INCOMING : BT_AVDTP_CONN_REQ_DIR_OUTGOING);
	/* Only consider RSP on expected connection handle */
	if (avdtp_hci && avdtp_l2cap) {
		if (HCI_L2CAP_CON_RSP_RESULT(data) == HCI_L2CAP_CON_RSP_RESULT_SUCCESS) {
			if (IS_VALID_CID_CONN_RESP(is_tx, avdtp_l2cap, data) &&
				avdtp_l2cap->state == BT_AVDTP_STATE_PENDING_SIGNALING) {

				/* If we were waiting to complete an AVDTP signal detection - store the dst_cid or src_cid depending
				 * on the transmit direction */
				STORE_DETECTED_CID_CONN_RESP(is_tx, avdtp_l2cap, data);
				avdtp_l2cap->state = BT_AVDTP_STATE_COMPLETE_SIGNALING;

				/* Switch to use "signal" and delete "ongoing" since the AVDTP signaling has now been
				 * detected */
				avdtp_hci->signal = *avdtp_l2cap;
				scsc_avdtp_detect_reset(avdtp_hci, false, true, false, false, false, false);
				SCSC_TAG_DEBUG(BT_H4, "Signaling dst CID: 0x%04X, src CID: 0x%04X, aclid: 0x%04X (tx=%u)\n",
							   avdtp_hci->signal.dst_cid,
							   avdtp_hci->signal.src_cid,
							   avdtp_hci->hci_connection_handle,
							   is_tx);

			} else if (IS_VALID_CID_CONN_RESP(is_tx, avdtp_l2cap, data) &&
					   avdtp_l2cap->state == BT_AVDTP_STATE_PENDING_STREAMING) {

				/* If we were waiting to complete an AVDTP stream detection - store the dst_cid or src_cid depending
				 * on the transmit direction */
				STORE_DETECTED_CID_CONN_RESP(is_tx, avdtp_l2cap, data);
				avdtp_l2cap->state = BT_AVDTP_STATE_COMPLETE_STREAMING;

				/* Switch to use "stream". If both an incoming and outgoing connection response was "expected"
				 * the first one wins. */
				avdtp_hci->stream = *avdtp_l2cap;
				scsc_avdtp_detect_reset(avdtp_hci, false, false, false, true, false, false);

				SCSC_TAG_DEBUG(BT_H4, "Streaming dst CID: 0x%04X, src CID: 0x%04X, aclid: 0x%04X (tx=%u)\n",
							   avdtp_hci->stream.dst_cid,
							   avdtp_hci->stream.src_cid,
							   avdtp_hci->hci_connection_handle,
							   is_tx);
			}
		} else if (HCI_L2CAP_CON_RSP_RESULT(data) >= HCI_L2CAP_CON_RSP_RESULT_REFUSED) {
			/* In case of a CONN_REFUSED the existing CIDs must be cleaned up such that the detection is ready
			 * for a new connection request */
			if (IS_VALID_CID_CONN_RESP(is_tx, avdtp_l2cap, data) &&
				avdtp_l2cap->state == BT_AVDTP_STATE_PENDING_SIGNALING) {
				avdtp_l2cap->dst_cid = avdtp_l2cap->src_cid = 0;
				avdtp_l2cap->state = BT_AVDTP_STATE_IDLE_SIGNALING;

			} else if (IS_VALID_CID_CONN_RESP(is_tx, avdtp_l2cap, data) &&
					   avdtp_l2cap->state == BT_AVDTP_STATE_PENDING_STREAMING) {

				/* Connection refused on streaming connect request. Reset dst_cid and src_cid, and
				 * reset the state to IDLE such that new connection requests can be detected */
				avdtp_l2cap->dst_cid = avdtp_l2cap->src_cid = 0;
				avdtp_l2cap->state = BT_AVDTP_STATE_IDLE_STREAMING;

			}
		}
	}
	if (avdtp_hci)
		spin_unlock(&avdtp_hci->lock);
}

/* Handle DISCONNECT_REQUEST and remove all current detections on the specific CIDs */
static bool scsc_bt_avdtp_detect_connection_disconnect_req_handling(uint16_t hci_connection_handle,
																	const unsigned char *data,
																	uint16_t length,
																	bool is_tx)
{
	bool result = false;
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci =
		scsc_avdtp_detect_find_or_create_hci_connection(hci_connection_handle, false);

	if (avdtp_hci) {
		if (avdtp_hci->signal.state == BT_AVDTP_STATE_COMPLETE_SIGNALING &&
			IS_VALID_CID_DISCONNECT_REQ(is_tx, avdtp_hci->signal, data)) {

			/* Disconnect the current registered signaling and streaming AVDTP connection */
			scsc_avdtp_detect_reset(avdtp_hci, true, true, true, true, true, true);

			/* The detection was removed and it can therefore not be unlocked */
			avdtp_hci = NULL;

			SCSC_TAG_DEBUG(BT_H4, "Signaling src CID disconnected (aclid: 0x%04X) (TX=%u)\n",
						   hci_connection_handle,
						   is_tx);
		} else if (avdtp_hci->stream.state == BT_AVDTP_STATE_COMPLETE_STREAMING &&
				   IS_VALID_CID_DISCONNECT_REQ(is_tx, avdtp_hci->stream, data)) {

			/* Disconnect the current registered streaming AVDTP connection */
			scsc_avdtp_detect_reset(avdtp_hci, false, false, true, true, false, false);

			SCSC_TAG_DEBUG(BT_H4, "Streaming src CID disconnected (aclid: 0x%04X) (TX=%u)\n",
						   hci_connection_handle,
						   is_tx);
			result = true;
		}
		if (avdtp_hci)
			spin_unlock(&avdtp_hci->lock);
	}
	return result;
}

/* Detects if there is any of the L2CAP codes of interrest, and returns true of the FW should be signalled a change */
static bool scsc_bt_avdtp_detect_connection_rxtx(uint16_t hci_connection_handle, const unsigned char *data, uint16_t length, bool is_tx)
{
	uint8_t code = 0;
	if (length < AVDTP_DETECT_MIN_DATA_LENGTH) {
		SCSC_TAG_DEBUG(BT_H4, "Ignoring L2CAP signal, length %u)\n", length);
		return false;
	}

	code = HCI_L2CAP_CODE(data);
	switch (code) {

	case L2CAP_CODE_CONNECT_REQ:
	{
		/* Handle connection request */
		scsc_bt_avdtp_detect_connection_conn_req_handling(hci_connection_handle, data, length, is_tx);
		break;
	}
	case L2CAP_CODE_CONNECT_RSP:
	{
		if (length < AVDTP_DETECT_MIN_DATA_LENGTH_CON_RSP) {
			SCSC_TAG_WARNING(BT_H4, "Ignoring L2CAP CON RSP in short packet, length %u)\n", length);
			return false;
		}
		/* Handle connection response */
		scsc_bt_avdtp_detect_connection_conn_resp_handling(hci_connection_handle, data, length, is_tx);
		break;
	}
	case L2CAP_CODE_DISCONNECT_REQ:
	{
		/* Handle disconnect request */
		return scsc_bt_avdtp_detect_connection_disconnect_req_handling(hci_connection_handle, data, length, is_tx);
		break;
	}
	default:
		break;
	}
	return false;
}

/* Check if there are any SEIDs from the discover that are SINK, and store them as SINK candidates */
static void scsc_avdtp_detect_check_discover_for_snk_seids(uint16_t hci_connection_handle,
						struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
						const unsigned char *data,
						uint16_t length,
						bool is_tx)
{
	uint16_t i = 0;
	uint16_t n_seid_info = (length - HCI_L2CAP_CONF_SEID_OFFSET) / HCI_L2CAP_CONF_SEID_INFO_SIZE;
	struct scsc_bt_avdtp_detect_snk_seid *seid = NULL;

	/* Remove potential existing SEID infos on the either local or remote */
	scsc_avdtp_detect_reset(avdtp_hci, false, false, false, false, is_tx, !is_tx);
	for (i = 0; i < n_seid_info; i++) {
		/* Only consider SEID if it's type is SINK. This means that for TX, look for TSEP equal to SNK.
		 * For RX look for TSEP equal to SRC, since this would result in our side being SNK */
		if ((is_tx && HCI_L2CAP_CONF_TSEP(data, i) == HCI_L2CAP_CONF_TSEP_SNK) ||
			(!is_tx && HCI_L2CAP_CONF_TSEP(data, i) == HCI_L2CAP_CONF_TSEP_SRC)) {

			if (avdtp_hci)
				spin_unlock(&avdtp_hci->lock);

			seid = kmalloc(sizeof(struct scsc_bt_avdtp_detect_snk_seid), GFP_KERNEL);

			avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(hci_connection_handle,
											false);

			if (avdtp_hci && seid) {
				memset(seid, 0, sizeof(struct scsc_bt_avdtp_detect_snk_seid));
				seid->seid = HCI_L2CAP_CONF_SEID(data, i);
				SCSC_TAG_DEBUG(BT_H4, "Storing seid=%u as candidate for SINK, aclid: 0x%04X\n",
							   seid->seid,
							   avdtp_hci->hci_connection_handle);
				/* Store the information on either local or remote */
				if (is_tx) {
					if (avdtp_hci->tsep_detect.local_snk_seids)
						seid->next = avdtp_hci->tsep_detect.local_snk_seids;
					avdtp_hci->tsep_detect.local_snk_seids = seid;
				} else {
					if (avdtp_hci->tsep_detect.remote_snk_seids)
						seid->next = avdtp_hci->tsep_detect.remote_snk_seids;
					avdtp_hci->tsep_detect.remote_snk_seids = seid;
				}
			} else
				kfree(seid);
		}
	}
}

/* Check if the set configuration matches any of the SINK candidates */
static void scsc_avdtp_detect_match_set_conf_seid_with_discover(struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
								const unsigned char *data,
								bool is_tx)
{
	struct scsc_bt_avdtp_detect_snk_seid *seid_info;
	uint8_t candidate = 0;

	/* Default to SRC */
	avdtp_hci->tsep_detect.tsep = BT_AVDTP_TSEP_SRC;

	if (is_tx) {
		seid_info = avdtp_hci->tsep_detect.local_snk_seids;
		candidate = avdtp_hci->tsep_detect.local_snk_seid_candidate;
	} else {
		seid_info = avdtp_hci->tsep_detect.remote_snk_seids;
		candidate = avdtp_hci->tsep_detect.remote_snk_seid_candidate;
	}

	while (seid_info) {
		if (seid_info->seid == candidate) {
			/* SINK was detected */
			avdtp_hci->tsep_detect.tsep = BT_AVDTP_TSEP_SNK;
			break;
		}
		seid_info = seid_info->next;
	}
	/* Clear the canditate SEID since it has now been checked */
	avdtp_hci->tsep_detect.local_snk_seid_candidate = 0;
	avdtp_hci->tsep_detect.remote_snk_seid_candidate = 0;
	SCSC_TAG_DEBUG(BT_H4, "TSEP for active stream, snk=%d, aclid=0x%04X\n",
				   avdtp_hci->tsep_detect.tsep,
				   avdtp_hci->hci_connection_handle);
}

/* Detects if the AVDTP signal leads to a state change that the FW should know */
static uint8_t scsc_avdtp_detect_signaling_rxtx(uint16_t hci_connection_handle,
						struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
						const unsigned char *data,
						uint16_t length,
						bool is_tx)
{
	u8 signal_id = AVDTP_SIGNAL_ID(data);
	u8 message_type = AVDTP_MESSAGE_TYPE(data);

	SCSC_TAG_DEBUG(BT_H4, "id: 0x%02X, type: 0x%02X)\n", signal_id, message_type);

	if (message_type == AVDTP_MESSAGE_TYPE_RSP_ACCEPT) {
		if (signal_id == AVDTP_SIGNAL_ID_START)
			return AVDTP_DETECT_SIGNALING_ACTIVE;
		else if (signal_id == AVDTP_SIGNAL_ID_OPEN)
			return AVDTP_DETECT_SIGNALING_OPEN;
		else if (is_tx && (signal_id == AVDTP_SIGNAL_ID_CLOSE || signal_id == AVDTP_SIGNAL_ID_SUSPEND ||
				signal_id == AVDTP_SIGNAL_ID_ABORT))
			return AVDTP_DETECT_SIGNALING_INACTIVE;
		else if (signal_id == AVDTP_SIGNAL_ID_DISCOVER) {
			/* Check the discover signal for potential SNK candidate SEIDs */
			scsc_avdtp_detect_check_discover_for_snk_seids(hci_connection_handle,
									avdtp_hci, data, length, is_tx);
		} else if (signal_id == AVDTP_SIGNAL_ID_SET_CONF) {
			/* Check if the SEID from set config matches a SNK SEID */
			scsc_avdtp_detect_match_set_conf_seid_with_discover(avdtp_hci, data, is_tx);
		}
	} else if (message_type == AVDTP_MESSAGE_TYPE_CMD) {
		if (signal_id == AVDTP_SIGNAL_ID_SET_CONF) {
			if (is_tx)
				avdtp_hci->tsep_detect.remote_snk_seid_candidate = HCI_L2CAP_SET_CONF_ACP_SEID(data);
			else
				avdtp_hci->tsep_detect.local_snk_seid_candidate = HCI_L2CAP_SET_CONF_ACP_SEID(data);
			SCSC_TAG_DEBUG(BT_H4, "Set configuration was detected; local_seid_candidate=%u, remote_seid_candidate=%u (aclid: 0x%04X)\n",
						   avdtp_hci->tsep_detect.local_snk_seid_candidate,
						   avdtp_hci->tsep_detect.remote_snk_seid_candidate,
						   avdtp_hci->hci_connection_handle);
		} else if (is_tx && (signal_id == AVDTP_SIGNAL_ID_CLOSE || signal_id == AVDTP_SIGNAL_ID_SUSPEND ||
				     signal_id == AVDTP_SIGNAL_ID_ABORT))
			return AVDTP_DETECT_SIGNALING_INACTIVE;
	} else if (message_type == AVDTP_MESSAGE_TYPE_GENERAL_REJECT || message_type == AVDTP_MESSAGE_TYPE_RSP_REJECT) {
		if (signal_id == AVDTP_SIGNAL_ID_SET_CONF) {
			if (is_tx) {
				if (avdtp_hci->tsep_detect.local_snk_seid_candidate)
					avdtp_hci->tsep_detect.local_snk_seid_candidate = 0;
			} else {
				if (avdtp_hci->tsep_detect.remote_snk_seid_candidate)
					avdtp_hci->tsep_detect.remote_snk_seid_candidate = 0;
			}
		}
	}
	return AVDTP_DETECT_SIGNALING_IGNORE;
}

/* Public function that hooks into scsc_shm.c. It pass the provided data on the proper functions such that
 * the AVDTP can be detected. If any state change is detected, the FW is signalled */
void scsc_avdtp_detect_rxtx(u16 hci_connection_handle, const unsigned char *data, uint16_t length, bool is_tx)
{
	/* Look for AVDTP connections */
	bool avdtp_gen_bg_int = false;
	uint16_t cid_to_fw = 0;
	bool is_sink = false;
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci = NULL;
	bool avdtp_open = false;

	/* Look for AVDTP connections */
	if (HCI_L2CAP_RX_CID((const unsigned char *)(data)) == L2CAP_SIGNALING_CID) {
		if (scsc_bt_avdtp_detect_connection_rxtx(hci_connection_handle, data, length, is_tx)) {
			avdtp_gen_bg_int = true;

			avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(
				hci_connection_handle,
				false);

			if (avdtp_hci) {
				cid_to_fw = avdtp_hci->stream.dst_cid;
				is_sink = avdtp_hci->tsep_detect.tsep == BT_AVDTP_TSEP_SNK;
			}
		}
	} else {
		/* Check if we have detected any signal on the connection handle */
		avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(
			hci_connection_handle,
			false);

		if (avdtp_hci) {
			if (avdtp_hci->signal.state == BT_AVDTP_STATE_COMPLETE_SIGNALING &&
			   length >= AVDTP_DETECT_MIN_AVDTP_LENGTH &&
			   ((is_tx && avdtp_hci->signal.dst_cid != 0 &&
				 avdtp_hci->signal.dst_cid == HCI_L2CAP_RX_CID((const unsigned char *)(data))) ||
				(!is_tx && avdtp_hci->signal.src_cid != 0 &&
				 avdtp_hci->signal.src_cid == HCI_L2CAP_RX_CID((const unsigned char *)(data))))) {
				/* Signaling has been detected on the given CID and hci_connection_handle */
				uint8_t result = scsc_avdtp_detect_signaling_rxtx(hci_connection_handle,
									avdtp_hci, data, length, is_tx);

				if (result != AVDTP_DETECT_SIGNALING_IGNORE) {
					avdtp_gen_bg_int = true;
					if (result != AVDTP_DETECT_SIGNALING_INACTIVE)
						cid_to_fw = avdtp_hci->stream.dst_cid;
					if (result == AVDTP_DETECT_SIGNALING_OPEN)
						avdtp_open = true;
					is_sink = avdtp_hci->tsep_detect.tsep == BT_AVDTP_TSEP_SNK;
				}

			}
		}
	}

	if (avdtp_hci)
		spin_unlock(&avdtp_hci->lock);

	if (avdtp_gen_bg_int) {
		if (bt_service.bsmhcp_protocol->header.firmware_features & BSMHCP_FEATURE_AVDTP_TRANSFER_RING) {
			uint32_t flags = 0;

			if (avdtp_hci && avdtp_hci->tsep_detect.tsep == BT_AVDTP_TSEP_SNK)
				flags |= AVDTP_SNK_FLAG_TD_MASK;
			if (avdtp_open)
				flags |= AVDTP_OPEN_FLAG_TD_MASK;
			scsc_bt_shm_h4_avdtp_detect_write(flags, cid_to_fw, hci_connection_handle);
		} else {
			/* Legacy communication between driver and FW. This was replaced by a transfer ring but
			 * the implementation is kept for backward compability
			 */
			u8 msg_counter = AVDTP_GET_MESSAGE_COUNT(
				bt_service.bsmhcp_protocol->header.avdtp_detect_stream_id);
			msg_counter++;
			msg_counter &= 0x3;

			bt_service.bsmhcp_protocol->header.avdtp_detect_stream_id = cid_to_fw |
				(hci_connection_handle << 16) |
				(msg_counter << 28);
			bt_service.bsmhcp_protocol->header.avdtp_detect_stream_id |= AVDTP_SIGNAL_FLAG_MASK;
			if (is_sink)
				bt_service.bsmhcp_protocol->header.avdtp_detect_stream_id |= AVDTP_SNK_FLAG_MASK;
			SCSC_TAG_DEBUG(
				BT_H4,
				"Found AVDTP signal. msgid: 0x%02X, aclid: 0x%04X, cid: 0x%04X, streamid: 0x%08X\n",
				msg_counter,
				hci_connection_handle,
				cid_to_fw,
				bt_service.bsmhcp_protocol->header.avdtp_detect_stream_id);
			wmb();
			scsc_service_mifintrbit_bit_set(bt_service.service,
										bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
										SCSC_MIFINTR_TARGET_R4);
		}
	}
}

/* Used to reset the different AVDTP detections */
void scsc_avdtp_detect_reset(struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci,
							 bool reset_signal,
							 bool reset_signal_ongoing,
							 bool reset_stream,
							 bool reset_stream_ongoing,
							 bool reset_local_seids,
							 bool reset_remote_seids)
{
	if (reset_signal_ongoing) {
		avdtp_hci->ongoing.outgoing_signal.state = BT_AVDTP_STATE_IDLE_SIGNALING;
		avdtp_hci->ongoing.outgoing_signal.src_cid = 0;
		avdtp_hci->ongoing.outgoing_signal.dst_cid = 0;
		avdtp_hci->ongoing.incoming_signal.state = BT_AVDTP_STATE_IDLE_SIGNALING;
		avdtp_hci->ongoing.incoming_signal.src_cid = 0;
		avdtp_hci->ongoing.incoming_signal.dst_cid = 0;
	}
	if (reset_stream) {
		avdtp_hci->stream.state = BT_AVDTP_STATE_IDLE_STREAMING;
		avdtp_hci->stream.src_cid = 0;
		avdtp_hci->stream.dst_cid = 0;
	}
	if (reset_stream_ongoing) {
		avdtp_hci->ongoing.outgoing_stream.state = BT_AVDTP_STATE_IDLE_STREAMING;
		avdtp_hci->ongoing.outgoing_stream.src_cid = 0;
		avdtp_hci->ongoing.outgoing_stream.dst_cid = 0;
		avdtp_hci->ongoing.incoming_stream.state = BT_AVDTP_STATE_IDLE_STREAMING;
		avdtp_hci->ongoing.incoming_stream.src_cid = 0;
		avdtp_hci->ongoing.incoming_stream.dst_cid = 0;
	}
	if (reset_local_seids) {
		struct scsc_bt_avdtp_detect_snk_seid *seid = avdtp_hci->tsep_detect.local_snk_seids;

		while (seid) {
			struct scsc_bt_avdtp_detect_snk_seid *next = seid->next;

			kfree(seid);
			seid = next;
		}
		avdtp_hci->tsep_detect.local_snk_seids = NULL;
		avdtp_hci->tsep_detect.local_snk_seid_candidate = 0;
	}
	if (reset_remote_seids) {
		struct scsc_bt_avdtp_detect_snk_seid *seid = avdtp_hci->tsep_detect.remote_snk_seids;

		while (seid) {
			struct scsc_bt_avdtp_detect_snk_seid *next = seid->next;

			kfree(seid);
			seid = next;
		}
		avdtp_hci->tsep_detect.remote_snk_seids = NULL;
		avdtp_hci->tsep_detect.remote_snk_seid_candidate = 0;
	}
	if (reset_local_seids && reset_remote_seids)
		avdtp_hci->tsep_detect.tsep = BT_AVDTP_TSEP_SRC;

	if (reset_signal) {
		struct scsc_bt_avdtp_detect_hci_connection *prev;
		/* Unlock the mutex to keep the order of lock/unlock between the connection list
		 * and the individual elements
		 */
		spin_unlock(&avdtp_hci->lock);
		spin_lock(&bt_service.avdtp_detect.lock);
		/* The element could have been deleted at this point by another thread before the mutext
		 * on the list was taken. Therefore re-check.
		 */
		if (avdtp_hci) {
			prev = bt_service.avdtp_detect.connections;

			if (prev && prev != avdtp_hci) {
				/* The element was not the head of the list. Search for the previous element */
				while (prev) {
					if (prev->next == avdtp_hci) {
						/* Remove the element from the list */
						prev->next = avdtp_hci->next;
						break;
					}
					prev = prev->next;
				}
			} else {
				bt_service.avdtp_detect.connections = avdtp_hci->next;
			}
			/* Lock to make sure that no-one reads from it. Since it has been removed from the list
			 * unlocking it again will not make another thread read it since it cannot be found
			 */
			spin_lock(&avdtp_hci->lock);
			spin_unlock(&bt_service.avdtp_detect.lock);
			spin_unlock(&avdtp_hci->lock);
			kfree(avdtp_hci);
			avdtp_hci = NULL;
		} else
			spin_unlock(&bt_service.avdtp_detect.lock);
	}
}

/* Used to reset all current or ongoing detections for a given hci_connection_handle. This can e.g.
 * used if the link is lost */
bool scsc_avdtp_detect_reset_connection_handle(uint16_t hci_connection_handle)
{
	bool reset_anything = false;
	struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci =
		scsc_avdtp_detect_find_or_create_hci_connection(hci_connection_handle, false);

	/* Check already established connections */
	if (avdtp_hci) {
		scsc_avdtp_detect_reset(avdtp_hci, true, true, true, true, true, true);
		/* No need to unlock the detection since it has been removed */
		reset_anything = true;
	}
	return reset_anything;
}

void scsc_avdtp_detect_exit(void)
{
	struct scsc_bt_avdtp_detect_hci_connection *head;

	/* Lock the detection list and find the head */
	spin_lock(&bt_service.avdtp_detect.lock);
	head = bt_service.avdtp_detect.connections;

	while (head) {
		spin_lock(&head->lock);
		/* Clear the remote and local seids lists on head */
		scsc_avdtp_detect_reset(head, false, false, false, false, true, true);

		/* Update the head to bypass the current element */
		bt_service.avdtp_detect.connections = head->next;

		spin_unlock(&bt_service.avdtp_detect.lock);

		/* Free the used memory */
		spin_unlock(&head->lock);
		kfree(head);
		head = NULL;

		/* Update the head variable */
		spin_lock(&bt_service.avdtp_detect.lock);
		head = bt_service.avdtp_detect.connections;
	}

	spin_unlock(&bt_service.avdtp_detect.lock);

	/* The avdtp_detect has now been restored and doesn't contain other information
	 * than its two locks
	 */
}
