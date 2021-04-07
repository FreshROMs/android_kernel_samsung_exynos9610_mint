/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/****************************************************************************
 * FILE
 *      bhcs.h  -  Bluetooth Host Configuration Structure
 *
 * DESCRIPTION
 *      This file specifies the layout of the Bluetooth Host Configuration
 *      Structure. The structure is written by the host and passed to the
 *      firmware as an argument to the service start callback function in
 *      the form of an offset that must be converted to a local address.
 *
 * ASSUMPTIONS
 *      The host and the firmware has the same endiannes.
 *      The ABI on the host and the firmware results in the same memory
 *      layout of the defined structure.
 *
 */

#ifndef BHCS_H__
#define BHCS_H__

/* The version of the BHCS structure. Must be written to the version field
 * by the host and confirmed to match the define by the firmware. Increment
 * the version when changing the layout of the structure. This also serves
 * as a rudimentary endianess check. */
#define BHCS_VERSION 2

struct BHCS {
	uint32_t version;                /* BHCS_VERSION */
	uint32_t bsmhcp_protocol_offset; /* BSMHCP_PROTOCOL structure offset */
	uint32_t bsmhcp_protocol_length; /* BSMHCP_PROTOCOL structure length */
	uint32_t configuration_offset;   /* Binary configuration data offset */
	uint32_t configuration_length;   /* Binary configuration data length */
	uint32_t bluetooth_address_lap;  /* Lower Address Part 00..23 */
	uint8_t  bluetooth_address_uap;  /* Upper Address Part 24..31 */
	uint16_t bluetooth_address_nap;  /* Non-significant    32..47 */
};

#endif /* BHCS_H__ */
