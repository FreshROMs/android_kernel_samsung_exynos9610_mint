/*****************************************************************************
 *
 *          Copyright (c) 2015 Samsung Electronics Co., Ltd
 *          Confidential information of Samsung Electronics Co., Ltd
 *
 *          Refer to LICENSE.txt included with this source for details
 *          on the license terms.
 *
 *          CME3_PRIM_H defines data structures and macros for coex signalling
 *          between BT Host and BT FW
 *
 *****************************************************************************/
#ifndef CME3_PRIM_H__
#define CME3_PRIM_H__

#if defined(XAP)
#include "types.h"
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#define CME3_PRIM_ANY_SIZE 1

/*******************************************************************************
 *
 * NAME
 *  CME_Signal_Id
 *
 * DESCRIPTION
 *
 * VALUES
 *  profile_a2dp_start_ind     -
 *  profile_a2dp_stop_ind      -
 *  coex_stop_ind              -
 *  coex_start_ind             -
 *
 *******************************************************************************/
typedef enum {
	CME_SIGNAL_ID_PROFILE_A2DP_START_IND = 0,
	CME_SIGNAL_ID_PROFILE_A2DP_STOP_IND = 1,
	CME_SIGNAL_ID_COEX_STOP_IND = 2,
	CME_SIGNAL_ID_COEX_START_IND = 3,
} CME_SIGNAL_ID;

/*******************************************************************************
 *
 * NAME
 *  CME_CODEC_TYPE
 *
 * DESCRIPTION
 *  Codec types used for A2DP profile.
 *
 * VALUES
 *  CME_CODEC_TYPE_SBC  -
 *  CME_CODEC_TYPE_APTX -
 *  CME_CODEC_TYPE_SSHD -
 *
 *******************************************************************************/
typedef enum {
	CME_CODEC_TYPE_SBC = 0,
	CME_CODEC_TYPE_APTX = 1,
	CME_CODEC_TYPE_SSHD = 2
} CME_CODEC_TYPE;


/*******************************************************************************
 *
 * NAME
 *  CME_A2DP_ROLE
 *
 * DESCRIPTION
 *  A2DP Device role for A2DP profile.
 *
 * VALUES
 *  CME_A2DP_SOURCE -
 *  CME_A2DP_SINK   -
 *
 *******************************************************************************/
typedef enum {
	CME_A2DP_SOURCE = 0,
	CME_A2DP_SINK = 1
} CME_A2DP_ROLE;


/*******************************************************************************
 *
 * NAME
 *  Cme_Header
 *
 * DESCRIPTION
 *
 * MEMBERS
 *  Signal_Id -
 *  Length    - The length of the whole message in 16-bit words.
 *
 *******************************************************************************/
typedef struct {
	CME_SIGNAL_ID Signal_Id;
	uint8_t       Length;
} CME_HEADER;

/* The following macros take CME_HEADER *cme_header_ptr or uint16_t *addr */
#define CME_HEADER_SIGNAL_ID_WORD_OFFSET (0)

#define CME_HEADER_SIGNAL_ID_GET(addr) \
	((CME_SIGNAL_ID)((*(((volatile const uint16_t *)(addr))) & 0xff)))

#define CME_HEADER_SIGNAL_ID_SET(addr, signal_id) \
	(*(((volatile uint16_t *)(addr))) =  \
		 (uint16_t)((*(((volatile uint16_t *)(addr))) & ~0xff) | (((signal_id)) & 0xff)))

#define CME_HEADER_LENGTH_GET(addr) (((*(((volatile const uint16_t *)(addr))) & 0xff00) >> 8))

#define CME_HEADER_LENGTH_SET(addr, length) (*(((volatile uint16_t *)(addr))) =  \
						     (uint16_t)((*(((volatile uint16_t *)(addr))) & ~0xff00) | (((length) << 8) & 0xff00)))

#define CME_HEADER_WORD_SIZE (1)

/*lint -e(773) allow unparenthesized*/
#define CME_HEADER_CREATE(Signal_Id, Length) \
	(uint16_t)(((Signal_Id)) & 0xff) | \
	(uint16_t)(((Length) << 8) & 0xff00)

#define CME_HEADER_PACK(addr, Signal_Id, Length) \
	do { \
		*(((volatile uint16_t *)(addr))) = (uint16_t)((uint16_t)(((Signal_Id)) & 0xff) | \
							      (uint16_t)(((Length) << 8) & 0xff00)); \
	} while (0)

#define CME_HEADER_MARSHALL(addr, cme_header_ptr) \
	do { \
		*((addr)) = (uint16_t)((((cme_header_ptr)->Signal_Id)) & 0xff) | \
			    (uint16_t)((((cme_header_ptr)->Length) << 8) & 0xff00); \
	} while (0)

#define CME_HEADER_UNMARSHALL(addr, cme_header_ptr) \
	do { \
		(cme_header_ptr)->Signal_Id = CME_HEADER_SIGNAL_ID_GET(addr); \
		(cme_header_ptr)->Length = CME_HEADER_LENGTH_GET(addr); \
	} while (0)




/*******************************************************************************
 *
 * NAME
 *  CME_PROFILE_A2DP_START_IND
 *
 * DESCRIPTION
 *  Message from CME_BH to CME_BT to indicate that an A2DP profile has
 *  started a connection or that an existing connection has resumed with
 *  updated parameters.
 *
 * MEMBERS
 *  acl_handle          - Identifies the ACL Link used for the profile
 *                        connection.
 *  l2cap_connection_id - Identifies the remote L2CAP connection ID (as used on
 *                        the air).
 *  bit_rate            - Identifies the bit rate of the codec in kbps.
 *  codec_type          - Identifies the codec type (CME_CODEC_TYPE enum).
 *  sdu_size            - Identifies the maximum size of the A2DP SDU (MTU
 *                        negotiated for the L2CAP link) in octets.
 *  period              - Identifies the period in ms of codec data being
 *                        available for transmission.
 *  role                - Identifies the local device role, source or sink.
 *  spare               - Spare.
 *
 *******************************************************************************/
typedef struct {
	CME_HEADER     header;
	uint16_t       acl_handle;
	uint16_t       l2cap_connection_id;
	uint16_t       bit_rate;      /* Only 12 bits used */
	CME_CODEC_TYPE codec_type;
	uint16_t       sdu_size;
	uint8_t        period;
	CME_A2DP_ROLE  role;
} CME_PROFILE_A2DP_START_IND;

/* The following macros take
 * CME_PROFILE_A2DP_START_IND *cme_profile_a2dp_start_ind_ptr or uint16_t *addr */
#define CME_PROFILE_A2DP_START_IND_ACL_HANDLE_WORD_OFFSET (1)

#define CME_PROFILE_A2DP_START_IND_ACL_HANDLE_GET(addr) (*((addr) + 1))

#define CME_PROFILE_A2DP_START_IND_ACL_HANDLE_SET(addr, acl_handle) \
	(*((addr) + 1) = (uint16_t)(acl_handle))

#define CME_PROFILE_A2DP_START_IND_L2CAP_CONNECTION_ID_WORD_OFFSET (2)

#define CME_PROFILE_A2DP_START_IND_L2CAP_CONNECTION_ID_GET(addr) (*((addr) + 2))

#define CME_PROFILE_A2DP_START_IND_L2CAP_CONNECTION_ID_SET(addr, l2cap_connection_id) \
	(*((addr) + 2) = (uint16_t)(l2cap_connection_id))
#define CME_PROFILE_A2DP_START_IND_BIT_RATE_WORD_OFFSET (3)

#define CME_PROFILE_A2DP_START_IND_BIT_RATE_GET(addr) (((*((addr) + 3) & 0xfff)))

#define CME_PROFILE_A2DP_START_IND_BIT_RATE_SET(addr, bit_rate) (*((addr) + 3) =  \
									 (uint16_t)((*((addr) + 3) & ~0xfff) | (((bit_rate)) & 0xfff)))

#define CME_PROFILE_A2DP_START_IND_CODEC_TYPE_GET(addr) \
	((CME_CODEC_TYPE)((*((addr) + 3) & 0x7000) >> 12))

#define CME_PROFILE_A2DP_START_IND_CODEC_TYPE_SET(addr, codec_type) \
	(*((addr) + 3) =  \
		 (uint16_t)((*((addr) + 3) & ~0x7000) | (((codec_type) << 12) & 0x7000)))

#define CME_PROFILE_A2DP_START_IND_SDU_SIZE_WORD_OFFSET (4)

#define CME_PROFILE_A2DP_START_IND_SDU_SIZE_GET(addr) (*((addr) + 4))

#define CME_PROFILE_A2DP_START_IND_SDU_SIZE_SET(addr, sdu_size) (*((addr) + 4) = \
									 (uint16_t)(sdu_size))

#define CME_PROFILE_A2DP_START_IND_PERIOD_WORD_OFFSET (5)

#define CME_PROFILE_A2DP_START_IND_PERIOD_GET(addr) (((*((addr) + 5) & 0xff)))

#define CME_PROFILE_A2DP_START_IND_PERIOD_SET(addr, period) (*((addr) + 5) =  \
								     (uint16_t)((*((addr) + 5) & ~0xff) | (((period)) & 0xff)))

#define CME_PROFILE_A2DP_START_IND_ROLE_GET(addr) \
	((CME_A2DP_ROLE)((*((addr) + 5) & 0x100) >> 8))

#define CME_PROFILE_A2DP_START_IND_ROLE_SET(addr, role) (*((addr) + 5) =  \
								 (uint16_t)((*((addr) + 5) & ~0x100) | (((role) << 8) & 0x100)))

#define CME_PROFILE_A2DP_START_IND_WORD_SIZE (6)

#define CME_PROFILE_A2DP_START_IND_PACK(addr, acl_handle, l2cap_connection_id, \
					bit_rate, codec_type, sdu_size, period, role, sampling_freq) \
	do { \
		*((addr) + 1) = (uint16_t)((uint16_t)(acl_handle)); \
		*((addr) + 2) = (uint16_t)((uint16_t)(l2cap_connection_id)); \
		*((addr) + 3) = (uint16_t)((uint16_t)(((bit_rate)) & 0xfff) | \
					   (uint16_t)(((codec_type) << 12) & 0x7000); \
					   *((addr) + 4) = (uint16_t)((uint16_t)(sdu_size)); \
					   *((addr) + 5) = (uint16_t)((uint16_t)(((period)) & 0xff) | \
								      (uint16_t)(((role) << 8) & 0x100); \
								      } while (0)

#define CME_PROFILE_A2DP_START_IND_MARSHALL(addr, cme_profile_a2dp_start_ind_ptr) \
	do { \
		CME_HEADER_MARSHALL((addr), &((cme_profile_a2dp_start_ind_ptr)->header)); \
		*((addr) + 1) = (uint16_t)((cme_profile_a2dp_start_ind_ptr)->acl_handle); \
		*((addr) + 2) = (uint16_t)((cme_profile_a2dp_start_ind_ptr)->l2cap_connection_id); \
		*((addr) + 3) = (uint16_t)((((cme_profile_a2dp_start_ind_ptr)->bit_rate)) & 0xfff) | \
				(uint16_t)((((cme_profile_a2dp_start_ind_ptr)->codec_type) << 12) & 0x7000); \
		*((addr) + 4) = (uint16_t)((cme_profile_a2dp_start_ind_ptr)->sdu_size); \
		*((addr) + 5) = (uint16_t)((((cme_profile_a2dp_start_ind_ptr)->period)) & 0xff) | \
				(uint16_t)((((cme_profile_a2dp_start_ind_ptr)->role) << 8) & 0x100); \
	} while (0)

#define CME_PROFILE_A2DP_START_IND_UNMARSHALL(addr, cme_profile_a2dp_start_ind_ptr) \
	do { \
		CME_HEADER_UNMARSHALL((addr), &((cme_profile_a2dp_start_ind_ptr)->header)); \
		(cme_profile_a2dp_start_ind_ptr)->acl_handle = CME_PROFILE_A2DP_START_IND_ACL_HANDLE_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->l2cap_connection_id = CME_PROFILE_A2DP_START_IND_L2CAP_CONNECTION_ID_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->bit_rate = CME_PROFILE_A2DP_START_IND_BIT_RATE_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->codec_type = CME_PROFILE_A2DP_START_IND_CODEC_TYPE_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->sdu_size = CME_PROFILE_A2DP_START_IND_SDU_SIZE_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->period = CME_PROFILE_A2DP_START_IND_PERIOD_GET(addr); \
		(cme_profile_a2dp_start_ind_ptr)->role = CME_PROFILE_A2DP_START_IND_ROLE_GET(addr); \
	} while (0)


/*******************************************************************************
 *
 * NAME
 *  CME_PROFILE_A2DP_STOP_IND
 *
 * DESCRIPTION
 *  Message from CME_BH to CME_BT to indicate that an A2DP profile has
 *  stopped or paused.
 *
 * MEMBERS
 *  acl_handle          - Identifies the ACL Link used for the profile
 *                        connection.
 *  l2cap_connection_id - Identifies the remote L2CAP connection ID (as used on
 *                        the air).
 *
 *******************************************************************************/
typedef struct {
	CME_HEADER header;
	uint16_t   acl_handle;
	uint16_t   l2cap_connection_id;
} CME_PROFILE_A2DP_STOP_IND;

/* The following macros take
 * CME_PROFILE_A2DP_STOP_IND *cme_profile_a2dp_stop_ind_ptr or uint16_t *addr */
#define CME_PROFILE_A2DP_STOP_IND_ACL_HANDLE_WORD_OFFSET (1)

#define CME_PROFILE_A2DP_STOP_IND_ACL_HANDLE_GET(addr) (*((addr) + 1))

#define CME_PROFILE_A2DP_STOP_IND_ACL_HANDLE_SET(addr, acl_handle) \
	(*((addr) + 1) = (uint16_t)(acl_handle))

#define CME_PROFILE_A2DP_STOP_IND_L2CAP_CONNECTION_ID_WORD_OFFSET (2)

#define CME_PROFILE_A2DP_STOP_IND_L2CAP_CONNECTION_ID_GET(addr) (*((addr) + 2))

#define CME_PROFILE_A2DP_STOP_IND_L2CAP_CONNECTION_ID_SET(addr, l2cap_connection_id) \
	(*((addr) + 2) = (uint16_t)(l2cap_connection_id))

#define CME_PROFILE_A2DP_STOP_IND_WORD_SIZE (3)

#define CME_PROFILE_A2DP_STOP_IND_PACK(addr, acl_handle, l2cap_connection_id) \
	do { \
		*((addr) + 1) = (uint16_t)((uint16_t)(acl_handle)); \
		*((addr) + 2) = (uint16_t)((uint16_t)(l2cap_connection_id)); \
	} while (0)

#define CME_PROFILE_A2DP_STOP_IND_MARSHALL(addr, cme_profile_a2dp_stop_ind_ptr) \
	do { \
		CME_HEADER_MARSHALL((addr), &((cme_profile_a2dp_stop_ind_ptr)->header)); \
		*((addr) + 1) = (uint16_t)((cme_profile_a2dp_stop_ind_ptr)->acl_handle); \
		*((addr) + 2) = (uint16_t)((cme_profile_a2dp_stop_ind_ptr)->l2cap_connection_id); \
	} while (0)

#define CME_PROFILE_A2DP_STOP_IND_UNMARSHALL(addr, cme_profile_a2dp_stop_ind_ptr) \
	do { \
		CME_HEADER_UNMARSHALL((addr), &((cme_profile_a2dp_stop_ind_ptr)->header)); \
		(cme_profile_a2dp_stop_ind_ptr)->acl_handle = CME_PROFILE_A2DP_STOP_IND_ACL_HANDLE_GET(addr); \
		(cme_profile_a2dp_stop_ind_ptr)->l2cap_connection_id = CME_PROFILE_A2DP_STOP_IND_L2CAP_CONNECTION_ID_GET(addr); \
	} while (0)



/*******************************************************************************
 *
 * NAME
 *  CME_COEX_STOP_IND
 *
 * DESCRIPTION
 *  Message from CME_BT to CME_BH to indicate that the coex service is
 *  stopping.
 *
 * MEMBERS
 *
 *******************************************************************************/
typedef struct {
	CME_HEADER header;
} CME_COEX_STOP_IND;

/* The following macros take
 *  CME_COEX_STOP_IND *cme_coex_stop_ind_ptr or uint16_t *addr */
#define CME_COEX_STOP_IND_WORD_SIZE (1)


/*******************************************************************************
 *
 * NAME
 *  CME_COEX_START_IND
 *
 * DESCRIPTION
 *  Message from CME_BT to CME_BH to indicate that the coex service is
 *  starting.
 *
 * MEMBERS
 *
 *******************************************************************************/
typedef struct {
	CME_HEADER header;
} CME_COEX_START_IND;

/* The following macros take
 * CME_COEX_START_IND *cme_coex_start_ind_ptr or uint16_t *addr */
#define CME_COEX_START_IND_WORD_SIZE (1)



#endif /* CME3_PRIM_H__ */
