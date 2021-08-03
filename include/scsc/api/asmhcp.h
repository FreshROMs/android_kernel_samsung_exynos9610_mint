/****************************************************************************
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/****************************************************************************
 * FILE
 *      asmhcp.h  -  ANT Shared Memory Host Controller Protocol
 *
 * DESCRIPTION
 *      This file specifies the layout of the ANT Shared Memory
 *      Host Controller Protocol
 */

#ifndef __ASMHCP_H__
#define __ASMHCP_H__

#define ASMHCP_TRANSFER_RING_DATA_SIZE          (16)
#define ASMHCP_TRANSFER_RING_CMD_SIZE           (16)

#define ASMHCP_BUFFER_SIZE                      (258)

struct ASMHCP_TD_CONTROL {
	uint16_t length;
	uint8_t  data[ASMHCP_BUFFER_SIZE];
};

struct ASMHCP_HEADER {
	/* AP RW - R4 RO - 64 octets */
	uint32_t                        magic_value;                    /* 0x00 */
	uint32_t                        mailbox_data_ctr_driv_read;     /* 0x04 */
	uint32_t                        mailbox_data_driv_ctr_write;    /* 0x08 */
	uint32_t                        mailbox_cmd_ctr_driv_read;      /* 0x0C */
	uint32_t                        mailbox_cmd_driv_ctr_write;     /* 0x10 */
	uint16_t                        ap_to_bg_int_src;               /* 0x14 */
	uint16_t                        bg_to_ap_int_src;               /* 0x16 */
	uint32_t                        btlog_enables0_low;             /* 0x18 */
	uint32_t                        firmware_control;               /* 0x1C */
	uint32_t                        btlog_enables0_high;            /* 0x20 */
	uint32_t                        btlog_enables1_low;             /* 0x24 */
	uint32_t                        btlog_enables1_high;            /* 0x28 */
	uint8_t                         reserved1[0x14];                /* 0x2C */

	/* AP RO - R4 RW - 64 octets */
	uint32_t                        mailbox_cmd_driv_ctr_read;      /* 0x40 */
	uint32_t                        mailbox_cmd_ctr_driv_write;     /* 0x44 */
	uint32_t                        mailbox_data_driv_ctr_read;     /* 0x48 */
	uint32_t                        mailbox_data_ctr_driv_write;    /* 0x4C */
	uint32_t                        firmware_features;              /* 0x50 */
	uint16_t                        panic_deathbed_confession;      /* 0x54 */
	uint8_t                         reserved2[0x2A];                /* 0x56 */
};

struct ASMHCP_PROTOCOL {
	/* header offset: 0x00000000 */
	volatile struct ASMHCP_HEADER header;
	/* from controller */
	struct ASMHCP_TD_CONTROL /* offset: 0x00000080 */
		cmd_controller_driver_transfer_ring[ASMHCP_TRANSFER_RING_CMD_SIZE];
	struct ASMHCP_TD_CONTROL /* offset: 0x000008A0 */
		data_controller_driver_transfer_ring[ASMHCP_TRANSFER_RING_DATA_SIZE];

	/* Padding used to ensure minimum 32 octets between sections */
	uint8_t reserved[0x20]; /* offset: 0x000010C0 */

	/* from driver */
	struct ASMHCP_TD_CONTROL /* offset: 0x000010E0 */
		cmd_driver_controller_transfer_ring[ASMHCP_TRANSFER_RING_CMD_SIZE];
	struct ASMHCP_TD_CONTROL /* offset: 0x00001900 */
		data_driver_controller_transfer_ring[ASMHCP_TRANSFER_RING_DATA_SIZE];
};

#define ASMHCP_PROTOCOL_MAGICVALUE \
		((ASMHCP_TRANSFER_RING_DATA_SIZE | (ASMHCP_TRANSFER_RING_CMD_SIZE << 4) | \
		(offsetof(struct ASMHCP_PROTOCOL, cmd_driver_controller_transfer_ring) << 15)) ^ \
		sizeof(struct ASMHCP_PROTOCOL))

#endif /* __ASMHCP_H__ */
