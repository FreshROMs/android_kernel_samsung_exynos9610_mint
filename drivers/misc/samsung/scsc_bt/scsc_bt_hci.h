/****************************************************************************
 *
 *       Internal BT driver HCI decoder
 *
 *       Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

#ifndef __SCSC_BT_HCI_H
#define __SCSC_BT_HCI_H

#define HCI_EVENT_HEADER_LENGTH                      (2)

#define HCI_EV_INQUIRY_COMPLETE                      ((u8)0x01)
#define HCI_EV_INQUIRY_RESULT                        ((u8)0x02)
#define HCI_EV_CONN_COMPLETE                         ((u8)0x03)
#define HCI_EV_CONN_REQUEST                          ((u8)0x04)
#define HCI_EV_DISCONNECT_COMPLETE                   ((u8)0x05)
#define HCI_EV_AUTH_COMPLETE                         ((u8)0x06)
#define HCI_EV_REMOTE_NAME_REQ_COMPLETE              ((u8)0x07)
#define HCI_EV_ENCRYPTION_CHANGE                     ((u8)0x08)
#define HCI_EV_CHANGE_CONN_LINK_KEY_COMPLETE         ((u8)0x09)
#define HCI_EV_MASTER_LINK_KEY_COMPLETE              ((u8)0x0A)
#define HCI_EV_READ_REM_SUPP_FEATURES_COMPLETE       ((u8)0x0B)
#define HCI_EV_READ_REMOTE_VER_INFO_COMPLETE         ((u8)0x0C)
#define HCI_EV_QOS_SETUP_COMPLETE                    ((u8)0x0D)
#define HCI_EV_COMMAND_COMPLETE                      ((u8)0x0E)
#define HCI_EV_COMMAND_STATUS                        ((u8)0x0F)
#define HCI_EV_HARDWARE_ERROR                        ((u8)0x10)
#define HCI_EV_FLUSH_OCCURRED                        ((u8)0x11)
#define HCI_EV_ROLE_CHANGE                           ((u8)0x12)
#define HCI_EV_NUMBER_COMPLETED_PKTS                 ((u8)0x13)
#define HCI_EV_MODE_CHANGE                           ((u8)0x14)
#define HCI_EV_RETURN_LINK_KEYS                      ((u8)0x15)
#define HCI_EV_PIN_CODE_REQ                          ((u8)0x16)
#define HCI_EV_LINK_KEY_REQ                          ((u8)0x17)
#define HCI_EV_LINK_KEY_NOTIFICATION                 ((u8)0x18)
#define HCI_EV_LOOPBACK_COMMAND                      ((u8)0x19)
#define HCI_EV_DATA_BUFFER_OVERFLOW                  ((u8)0x1A)
#define HCI_EV_MAX_SLOTS_CHANGE                      ((u8)0x1B)
#define HCI_EV_READ_CLOCK_OFFSET_COMPLETE            ((u8)0x1C)
#define HCI_EV_CONN_PACKET_TYPE_CHANGED              ((u8)0x1D)
#define HCI_EV_QOS_VIOLATION                         ((u8)0x1E)
#define HCI_EV_PAGE_SCAN_MODE_CHANGE                 ((u8)0x1F)
#define HCI_EV_PAGE_SCAN_REP_MODE_CHANGE             ((u8)0x20)
/* 1.2 Events */
#define HCI_EV_FLOW_SPEC_COMPLETE                    ((u8)0x21)
#define HCI_EV_INQUIRY_RESULT_WITH_RSSI              ((u8)0x22)
#define HCI_EV_READ_REM_EXT_FEATURES_COMPLETE        ((u8)0x23)
#define HCI_EV_FIXED_ADDRESS                         ((u8)0x24)
#define HCI_EV_ALIAS_ADDRESS                         ((u8)0x25)
#define HCI_EV_GENERATE_ALIAS_REQ                    ((u8)0x26)
#define HCI_EV_ACTIVE_ADDRESS                        ((u8)0x27)
#define HCI_EV_ALLOW_PRIVATE_PAIRING                 ((u8)0x28)
#define HCI_EV_ALIAS_ADDRESS_REQ                     ((u8)0x29)
#define HCI_EV_ALIAS_NOT_RECOGNISED                  ((u8)0x2A)
#define HCI_EV_FIXED_ADDRESS_ATTEMPT                 ((u8)0x2B)
#define HCI_EV_SYNC_CONN_COMPLETE                    ((u8)0x2C)
#define HCI_EV_SYNC_CONN_CHANGED                     ((u8)0x2D)

/* 2.1 Events */
#define HCI_EV_SNIFF_SUB_RATE                        ((u8)0x2E)
#define HCI_EV_EXTENDED_INQUIRY_RESULT               ((u8)0x2F)
#define HCI_EV_ENCRYPTION_KEY_REFRESH_COMPLETE       ((u8)0x30)
#define HCI_EV_IO_CAPABILITY_REQUEST                 ((u8)0x31)
#define HCI_EV_IO_CAPABILITY_RESPONSE                ((u8)0x32)
#define HCI_EV_USER_CONFIRMATION_REQUEST             ((u8)0x33)
#define HCI_EV_USER_PASSKEY_REQUEST                  ((u8)0x34)
#define HCI_EV_REMOTE_OOB_DATA_REQUEST               ((u8)0x35)
#define HCI_EV_SIMPLE_PAIRING_COMPLETE               ((u8)0x36)
#define HCI_EV_LST_CHANGE                            ((u8)0x38)
#define HCI_EV_ENHANCED_FLUSH_COMPLETE               ((u8)0x39)
#define HCI_EV_USER_PASSKEY_NOTIFICATION             ((u8)0x3B)
#define HCI_EV_KEYPRESS_NOTIFICATION                 ((u8)0x3C)
#define HCI_EV_REM_HOST_SUPPORTED_FEATURES           ((u8)0x3D)
#define HCI_EV_ULP                                   ((u8)0x3E)

/* TCC + CSB Events */
#define HCI_EV_TRIGGERED_CLOCK_CAPTURE               ((u8)0x4E)
#define HCI_EV_SYNCHRONIZATION_TRAIN_COMPLETE        ((u8)0x4F)
#define HCI_EV_SYNCHRONIZATION_TRAIN_RECEIVED        ((u8)0x50)
#define HCI_EV_CSB_RECEIVE                           ((u8)0x51)
#define HCI_EV_CSB_TIMEOUT                           ((u8)0x52)
#define HCI_EV_TRUNCATED_PAGE_COMPLETE               ((u8)0x53)
#define HCI_EV_SLAVE_PAGE_RESPONSE_TIMEOUT           ((u8)0x54)
#define HCI_EV_CSB_CHANNEL_MAP_CHANGE                ((u8)0x55)
#define HCI_EV_INQUIRY_RESPONSE_NOTIFICATION         ((u8)0x56)

/* 4.1 Events */
#define HCI_EV_AUTHENTICATED_PAYLOAD_TIMEOUT_EXPIRED ((u8)0x57)

/* ULP Sub-opcodes */
#define HCI_EV_ULP_CONNECTION_COMPLETE                 ((u8)0x01)
#define HCI_EV_ULP_ADVERTISING_REPORT                  ((u8)0x02)
#define HCI_EV_ULP_CONNECTION_UPDATE_COMPLETE          ((u8)0x03)
#define HCI_EV_ULP_READ_REMOTE_USED_FEATURES_COMPLETE  ((u8)0x04)
#define HCI_EV_ULP_LONG_TERM_KEY_REQUEST               ((u8)0x05)
#define HCI_EV_ULP_REMOTE_CONNECTION_PARAMETER_REQUEST ((u8)0x06)
#define HCI_EV_ULP_DATA_LENGTH_CHANGE                  ((u8)0x07)
#define HCI_EV_ULP_READ_LOCAL_P256_PUB_KEY_COMPLETE    ((u8)0x08)
#define HCI_EV_ULP_GENERATE_DHKEY_COMPLETE             ((u8)0x09)
#define HCI_EV_ULP_ENHANCED_CONNECTION_COMPLETE        ((u8)0x0A)
#define HCI_EV_ULP_DIRECT_ADVERTISING_REPORT           ((u8)0x0B)
#define HCI_EV_ULP_PHY_UPDATE_COMPLETE                 ((u8)0x0C)
/* The subevent code of ULP_USED_CHANNEL_SELECTION_EVENT shall be updated
   when it is defined in the spec.
   Assign it as 0x0D temporarily.                                     */
#define HCI_EV_ULP_USED_CHANNEL_SELECTION              ((u8)0x0D)

#define HCI_EV_DECODE(entry) case entry: ret = #entry; break

#endif /* __SCSC_BT_HCI_H */
