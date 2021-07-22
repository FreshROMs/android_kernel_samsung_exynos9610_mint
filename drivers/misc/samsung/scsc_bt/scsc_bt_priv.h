/****************************************************************************
 *
 *       Internal BT driver definitions
 *
 *       Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

#ifndef __SCSC_BT_PRIV_H
#define __SCSC_BT_PRIV_H

#include <linux/pm_qos.h>
#include <linux/poll.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/cdev.h>

#include <scsc/scsc_mx.h>
#include <scsc/api/bsmhcp.h>
#include <scsc/api/bhcs.h>
#include <scsc/api/bt_audio.h>

#include "scsc_shm.h"

#ifndef UNUSED
#define UNUSED(x)       ((void)(x))
#endif

/**
 * Size of temporary buffer (on stack) for peeking at HCI/H4
 * packet header held in FIFO.
 *
 * Must be big enough to decode the
 * length of any HCI packet type.
 *
 * For ACL that is 1 h4 header + 2 ACL handle + 2 ACL data size
 */
#define H4DMUX_HEADER_HCI (1 + 3) /* CMD, SCO */
#define H4DMUX_HEADER_ACL (1 + 4) /* ACL */

#define HCI_COMMAND_PKT         (1)
#define HCI_ACLDATA_PKT         (2)
#define HCI_EVENT_PKT           (4)

#define ACLDATA_HEADER_SIZE     (4)
#define L2CAP_HEADER_SIZE       (4)

#define HCI_ACL_DATA_FLAGS(data)        ((*(data + 1)) & 0xf0)
#define HCI_ACL_DATA_CON_HDL(data)      ((u16)(*(data + 0) | ((*(data + 1)) & 0x0f) << 8))
#define HCI_ACL_DATA_LENGTH(data)       ((u16)(*(data + 2) | (*(data + 3)) << 8))
#define HCI_L2CAP_LENGTH(data)          ((u16)(*(data + 4) | (*(data + 5)) << 8))
#define HCI_L2CAP_CID(data)             ((u16)(*(data + 6) | (*(data + 7)) << 8))

#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS_EVENT     (0x13)
#define HCI_EVENT_HARDWARE_ERROR_EVENT                  (0x10)

#define SCSC_BT_CONF      "bt.hcf"
#ifdef CONFIG_SCSC_BT_BLUEZ
#define SCSC_BT_ADDR      "/csa/bluetooth/.bd_addr"
#define SCSC_BT_ADDR_LEN  (3)
#elif defined CONFIG_SCSC_BT_ADDRESS_IN_FILE
#define SCSC_BT_ADDR      CONFIG_SCSC_BT_ADDRESS_FILENAME
#define SCSC_BT_ADDR_LEN  (6)
#endif

#define SCSC_H4_DEVICE_NAME             "scsc_h4_0"
#define SCSC_ANT_DEVICE_NAME            "scsc_ant_0"

#define ANT_COMMAND_MSG 0x0C
#define ANT_DATA_MSG 0x0E
#define ANT_HEADER_LENGTH 1

#define SCSC_BT_CONNECTION_INFO_MAX     (0x1000)
#define SCSC_BT_ACL_RAW_MASK            (0xF000)
#define SCSC_BT_ACL_RAW                 (0x2000)
#define SCSC_BT_ACL_HANDLE_MASK         (0x0FFF)

#define SCSC_TTY_MINORS (8)

enum scsc_bt_shm_thread_flags;

enum scsc_bt_read_op {
	BT_READ_OP_NONE,
	BT_READ_OP_HCI_EVT,
	BT_READ_OP_HCI_EVT_ERROR,
	BT_READ_OP_ACL_DATA,
	BT_READ_OP_ACL_CREDIT,
	BT_READ_OP_IQ_REPORT,
	BT_READ_OP_STOP
};

enum scsc_ant_read_op {
	ANT_READ_OP_NONE,
	ANT_READ_OP_CMD,
	ANT_READ_OP_DATA
};

struct scsc_bt_connection_info {
	u8  state;
	u16 length;
	u16 l2cap_cid;
};

#define CONNECTION_NONE         (0)
#define CONNECTION_ACTIVE       (1)
#define CONNECTION_DISCONNECTED (2)

enum bt_link_type_enum {
	BT_LINK_TYPE_SCO		= 0,
	BT_LINK_TYPE_ACL		= 1,
	BT_LINK_TYPE_SETUP_ID		= 2,
	BT_LINK_TYPE_SETUP_FHS		= 3,
	BT_LINK_TYPE_ESCO		= 4,
	BT_LINK_TYPE_ACL_23		= 5,
	BT_LINK_TYPE_ESCO_23		= 6,
	BT_LINK_TYPE_ANTPLUS		= 7,
	MAX_BT_LINK_TYPE		= 7
};

enum scsc_bt_avdtp_detect_state_enum {
	BT_AVDTP_STATE_IDLE_SIGNALING,
	BT_AVDTP_STATE_PENDING_SIGNALING,
	BT_AVDTP_STATE_COMPLETE_SIGNALING,
	BT_AVDTP_STATE_IDLE_STREAMING,
	BT_AVDTP_STATE_PENDING_STREAMING,
	BT_AVDTP_STATE_COMPLETE_STREAMING,
};

enum scsc_bt_avdtp_detect_conn_req_direction_enum {
	BT_AVDTP_CONN_REQ_DIR_INCOMING,
	BT_AVDTP_CONN_REQ_DIR_OUTGOING,
};

enum scsc_bt_avdtp_detect_type {
	BT_AVDTP_CONN_TYPE_SIGNAL = 0,
	BT_AVDTP_CONN_TYPE_STREAM = 1,
};

struct scsc_bt_avdtp_detect_connection {
	enum scsc_bt_avdtp_detect_type                  type;
	enum scsc_bt_avdtp_detect_state_enum            state;
	u16                                             src_cid;
	u16                                             dst_cid;
};

struct scsc_bt_avdtp_detect_ongoing {
	struct scsc_bt_avdtp_detect_connection		incoming_signal;
	struct scsc_bt_avdtp_detect_connection		outgoing_signal;
	struct scsc_bt_avdtp_detect_connection		incoming_stream;
	struct scsc_bt_avdtp_detect_connection		outgoing_stream;
};

enum scsc_bt_avdtp_detect_tsep {
	BT_AVDTP_TSEP_SRC = 0,
	BT_AVDTP_TSEP_SNK = 1,
};

struct scsc_bt_avdtp_detect_src_snk {
	enum scsc_bt_avdtp_detect_tsep          tsep;
	struct scsc_bt_avdtp_detect_snk_seid    *local_snk_seids;
	struct scsc_bt_avdtp_detect_snk_seid    *remote_snk_seids;
	uint16_t                                local_snk_seid_candidate;
	uint16_t                                remote_snk_seid_candidate;
};

struct scsc_bt_avdtp_detect_snk_seid {
	uint8_t								seid;
	struct scsc_bt_avdtp_detect_snk_seid	*next;
};

struct scsc_bt_avdtp_detect_hci_connection {
	struct scsc_bt_avdtp_detect_ongoing             ongoing;
	u16                                             hci_connection_handle;
	struct scsc_bt_avdtp_detect_connection          signal;
	struct scsc_bt_avdtp_detect_connection          stream;
	struct scsc_bt_avdtp_detect_src_snk             tsep_detect;
	struct scsc_bt_avdtp_detect_hci_connection      *next;
	spinlock_t                                      lock;
};

struct scsc_bt_avdtp_detect {
	struct scsc_bt_avdtp_detect_hci_connection    *connections;
	spinlock_t                                    lock;
	spinlock_t                                    fw_write_lock;
};

struct scsc_common_service {
	struct scsc_mx                *maxwell_core;
	struct class                   *class;
};

extern struct scsc_common_service common_service;

#ifdef CONFIG_SCSC_LOG_COLLECTION
struct scsc_bt_hcf_collection {
	void                       *hcf;
	u32                        hcf_size;
};
#endif

struct scsc_bt_service {
	dev_t                          device;
	struct scsc_service            *service;
	struct device                  *dev;

	struct cdev                    h4_cdev;
	struct device                  *h4_device;
	struct file                    *h4_file;
	bool                           h4_users;
	atomic_t                       h4_readers;
	atomic_t                       h4_writers;
	size_t                         h4_write_offset;

	atomic_t                       error_count;
	atomic_t                       service_users;
	bool                           service_started;

	scsc_mifram_ref                bhcs_ref;                /* Bluetooth host configuration service reference */
	scsc_mifram_ref                bsmhcp_ref;              /* Bluetooth shared memory host controller protocol reference */
	scsc_mifram_ref                config_ref;              /* Bluetooth configuration reference */
	scsc_mifram_ref                abox_ref;                /* A-Box reference */
	struct BSMHCP_PROTOCOL         *bsmhcp_protocol;        /* Bluetooth shared memory host controller protocol pointer */
	size_t                         read_offset;
	enum scsc_bt_read_op           read_operation;
	u32                            read_index;
	wait_queue_head_t              read_wait;

	wait_queue_head_t              info_wait;

	int                            last_alloc;  /* Cached previous alloc index to aid search */
	u8                             allocated[BSMHCP_DATA_BUFFER_TX_ACL_SIZE];
	u32                            allocated_count;
	u32                            freed_count;
	bool                           processed[BSMHCP_TRANSFER_RING_EVT_SIZE];

	struct scsc_bt_connection_info connection_handle_list[SCSC_BT_CONNECTION_INFO_MAX];
	bool                           hci_event_paused;
	bool                           acldata_paused;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock	       read_wake_lock;
        struct scsc_wake_lock	       write_wake_lock;
        struct scsc_wake_lock	       service_wake_lock;
#else
	struct wake_lock               read_wake_lock;
	struct wake_lock               write_wake_lock;
	struct wake_lock               service_wake_lock;
#endif
	size_t                         write_wake_lock_count;
	size_t                         write_wake_unlock_count;

	size_t                         interrupt_count;
	size_t                         interrupt_read_count;
	size_t                         interrupt_write_count;
	size_t                         last_suspend_interrupt_count;

	u32                            mailbox_hci_evt_read;
	u32                            mailbox_hci_evt_write;
	u32                            mailbox_acl_rx_read;
	u32                            mailbox_acl_rx_write;
	u32                            mailbox_acl_free_read;
	u32                            mailbox_acl_free_read_scan;
	u32                            mailbox_acl_free_write;
	u32                            mailbox_iq_report_read;
	u32                            mailbox_iq_report_write;

	struct scsc_bt_avdtp_detect    avdtp_detect;
	struct completion              recovery_release_complete;
	struct completion              recovery_probe_complete;
	u8                             recovery_level;

	bool                           iq_reports_enabled;

#ifdef CONFIG_SCSC_LOG_COLLECTION
	struct scsc_bt_hcf_collection  hcf_collection;
#endif
};

extern struct scsc_bt_service bt_service;

/* IQ reporting */
#define HCI_IQ_REPORTING_MAX_NUM_SAMPLES               (82)
/**
 * The driver supports both reception of LE Connection IQ Report
 * and LE Connectionless IQ report event:
 *
 * The largest hci event is LE Connection IQ Report, which
 * constitutes of:
 *
 * - Hci packet type    : 1 octet
 * - Event code         : 1 octet
 * - Param total Length : 1 octet
 * - Subevent code      : 1 octet
 * - Connection hdl     : 2 octets
 * - RX_PHY             : 1 octet
 * - Data_channel_idx   : 1 octet
 * - RSSI               : 2 octets
 * - RSSI antenna id    : 1 octet
 * - cte type           : 1 octet
 * - Slot durations     : 1 octet
 * - packet_status      : 1 octet
 * - event_counter      : 2 octets
 * - Sample count       : 1 octet
 *********************************
 * Total                : 17 octets
 *
 * The maximum hci event size in bytes is:
 *     (17 + (number of samples * 2 (both I and Q)))
 *
 */
#define HCI_IQ_REPORT_MAX_LEN                          (17 + (2 * HCI_IQ_REPORTING_MAX_NUM_SAMPLES))
#define HCI_LE_CONNECTIONLESS_IQ_REPORT_EVENT_SUB_CODE (0x15)
#define HCI_LE_CONNECTION_IQ_REPORT_EVENT_SUB_CODE     (0x16)


struct scsc_ant_service {
	dev_t                          device;
	struct scsc_service            *service;
	struct device                  *dev;

	struct cdev                    ant_cdev;
	struct device                  *ant_device;
	struct file                    *ant_file;
	bool                           ant_users;
	atomic_t                       ant_readers;
	atomic_t                       ant_writers;
	size_t                         ant_write_offset;

	atomic_t                       error_count;
	atomic_t                       service_users;
	bool                           service_started;

	/* Bluetooth host configuration service reference */
	scsc_mifram_ref                bhcs_ref;
	/* Ant shared memory host controller protocol reference */
	scsc_mifram_ref                asmhcp_ref;
	/* Bluetooth configuration reference */
	scsc_mifram_ref                config_ref;

	/* Ant shared memory host controller protocol pointer */
	struct ASMHCP_PROTOCOL         *asmhcp_protocol;
	size_t                         read_offset;
	enum scsc_ant_read_op          read_operation;
	u32                            read_index;
	wait_queue_head_t              read_wait;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock               read_wake_lock;
        struct scsc_wake_lock               write_wake_lock;
        struct scsc_wake_lock               service_wake_lock;
#else
	struct wake_lock               read_wake_lock;
	struct wake_lock               write_wake_lock;
	struct wake_lock               service_wake_lock;
#endif
	size_t                         write_wake_lock_count;
	size_t                         write_wake_unlock_count;

	size_t                         interrupt_count;
	size_t                         interrupt_read_count;
	size_t                         interrupt_write_count;

	u32                            mailbox_data_ctr_driv_read;
	u32                            mailbox_data_ctr_driv_write;
	u32                            mailbox_cmd_ctr_driv_read;
	u32                            mailbox_cmd_ctr_driv_write;

	struct completion              recovery_release_complete;
	struct completion              recovery_probe_complete;
	struct completion              release_complete;
	u8                             recovery_level;
};

extern struct scsc_ant_service ant_service;
/* Coex avdtp detection */

/* The buffers passed for inspection begin at the L2CAP basic header, as does the length
 * passed in the function calls */
#define AVDTP_DETECT_MIN_DATA_LENGTH            (12) /* We always want to look for the SRC CID */
#define AVDTP_DETECT_MIN_DATA_LENGTH_CON_RSP    (16) /* For CON RSP, we want the result, too */
#define AVDTP_DETECT_MIN_AVDTP_LENGTH           (6)  /* Basic L2CAP header + 2 AVDTP octets as min */

#define HCI_ACL_PACKET_BOUNDARY_START_FLUSH     (2)

/* Can't use HCI_L2CAP_CID(data), since that assumes 4 bytes of HCI header, which has been stripped
 * for the calls to the avdtp detection functions */
#define HCI_L2CAP_RX_CID(data)                  ((u16)(*(data + 2) | (*(data + 3)) << 8))

#define HCI_L2CAP_CODE(data)                    ((u8)(*(data + 4)))
#define HCI_L2CAP_CON_REQ_PSM(data)             ((u16)(*(data + 8) | (*(data + 9)) << 8))
/* Valid for at least connection request/response and disconnection request */
#define HCI_L2CAP_SOURCE_CID(data)              ((u16)(*(data + 10) | (*(data + 11)) << 8))
/* Valid for at least connection and disconnection responses */
#define HCI_L2CAP_RSP_DEST_CID(data)            ((u16)(*(data + 8) | (*(data + 9)) << 8))
#define HCI_L2CAP_CON_RSP_RESULT(data)          ((u16)(*(data + 12) | (*(data + 13)) << 8))
#define HCI_L2CAP_CON_RSP_RESULT_SUCCESS        (0x0000)
#define HCI_L2CAP_CON_RSP_RESULT_REFUSED        (0x0002)


#define HCI_L2CAP_CONF_SEID_OFFSET              6
#define HCI_L2CAP_CONF_TSEP_OFFSET              7
#define HCI_L2CAP_CONF_SEID_INFO_SIZE           2
#define HCI_L2CAP_CONF_SEID(data, index)        (((u8)(*(data + index * HCI_L2CAP_CONF_SEID_INFO_SIZE + HCI_L2CAP_CONF_SEID_OFFSET)) >> 2) & 0x3F)
#define HCI_L2CAP_CONF_TSEP_SRC                 0
#define HCI_L2CAP_CONF_TSEP_SNK                 1
#define HCI_L2CAP_CONF_TSEP(data, index)        (((u8)(*(data + index * HCI_L2CAP_CONF_SEID_INFO_SIZE + HCI_L2CAP_CONF_TSEP_OFFSET)) >> 3) & 0x1)
#define HCI_L2CAP_SET_CONF_ACP_SEID_OFFSET      6
#define HCI_L2CAP_SET_CONF_ACP_SEID(data)       (((u8)(*(data + HCI_L2CAP_SET_CONF_ACP_SEID_OFFSET)) >> 2) & 0x3F)


#define L2CAP_AVDTP_PSM                 0x0019
#define L2CAP_SIGNALING_CID             0x0001
#define L2CAP_CODE_CONNECT_REQ          0x02
#define L2CAP_CODE_CONNECT_RSP          0x03
#define L2CAP_CODE_CONFIGURE_REQ        0x04
#define L2CAP_CODE_DISCONNECT_REQ       0x06
#define L2CAP_CODE_DISCONNECT_RSP       0x07

#define AVDTP_MESSAGE_TYPE_OFFSET          4 /* Assuming only single packet type */
#define AVDTP_MESSAGE_TYPE_MASK            0x03
#define AVDTP_MESSAGE_TYPE(data)           ((u8)(*(data + AVDTP_MESSAGE_TYPE_OFFSET)) & AVDTP_MESSAGE_TYPE_MASK)
#define AVDTP_MESSAGE_TYPE_CMD             0x00
#define AVDTP_MESSAGE_TYPE_GENERAL_REJECT  0x01
#define AVDTP_MESSAGE_TYPE_RSP_ACCEPT      0x02
#define AVDTP_MESSAGE_TYPE_RSP_REJECT      0x03

#define AVDTP_SIGNAL_ID_OFFSET          5 /* Assuming only single packet type */
#define AVDTP_SIGNAL_ID_MASK            0x1F
#define AVDTP_SIGNAL_ID(data)           ((u8)(*(data + AVDTP_SIGNAL_ID_OFFSET)) & AVDTP_SIGNAL_ID_MASK)

#define AVDTP_SIGNAL_ID_DISCOVER        0x01
#define AVDTP_SIGNAL_ID_SET_CONF        0x03
#define AVDTP_SIGNAL_ID_OPEN            0x06
#define AVDTP_SIGNAL_ID_START           0x07
#define AVDTP_SIGNAL_ID_CLOSE           0x08
#define AVDTP_SIGNAL_ID_SUSPEND         0x09
#define AVDTP_SIGNAL_ID_ABORT           0x0A

#define AVDTP_SIGNAL_FLAG_MASK          (0x80000000)
#define AVDTP_SNK_FLAG_MASK             (0x40000000)
#define AVDTP_MESSAGE_COUNT_MASK        (0x30000000)
#define AVDTP_GET_MESSAGE_COUNT(data)   ((data & AVDTP_MESSAGE_COUNT_MASK) >> 28)

#define AVDTP_SNK_FLAG_TD_MASK             (0x00000001)
#define AVDTP_OPEN_FLAG_TD_MASK            (0x00000002)

extern uint16_t avdtp_signaling_src_cid;
extern uint16_t avdtp_signaling_dst_cid;
extern uint16_t avdtp_streaming_src_cid;
extern uint16_t avdtp_streaming_dst_cid;
extern uint16_t avdtp_hci_connection_handle;

#define AVDTP_DETECT_SIGNALING_IGNORE   0
#define AVDTP_DETECT_SIGNALING_ACTIVE   1
#define AVDTP_DETECT_SIGNALING_INACTIVE 2
#define AVDTP_DETECT_SIGNALING_OPEN     3

void scsc_avdtp_detect_rxtx(u16 hci_connection_handle, const unsigned char *data, uint16_t length, bool is_tx);
bool scsc_avdtp_detect_reset_connection_handle(uint16_t hci_connection_handle);
bool scsc_bt_shm_h4_avdtp_detect_write(uint32_t flags,
									   uint16_t l2cap_cid,
									   uint16_t hci_connection_handle);
void scsc_avdtp_detect_exit(void);

#ifdef CONFIG_SCSC_BT_BLUEZ
void slsi_bt_notify_probe(struct device *dev,
			  const struct file_operations *fs,
			  atomic_t *error_count,
			  wait_queue_head_t *read_wait);
void slsi_bt_notify_remove(void);
#else
#define slsi_bt_notify_probe(dev, fs, error_count, read_wait)
#define slsi_bt_notify_remove()
#endif

#endif /* __SCSC_BT_PRIV_H */
