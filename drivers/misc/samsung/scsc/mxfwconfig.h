/*****************************************************************************
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MXFWCONFIG_H
#define __MXFWCONFIG_H

#define SCSC_MX_MAX_COMMON_CFG		2
#define MX_COMMON_HCF_HDR_SIZE		8
#define MX_COMMON_HASH_SIZE_BYTES	2 /* Hash will be contained in a uint32 */
#define MX_COMMON_HASH_OFFSET		4

struct mxfwconfig {
	u32		configs;	/* Number of files */
	void		*shdram;	/* Combined payload in shared DRAM */
	size_t		shtotal;	/* Size of combined payload in shared DRAM */

	struct {
		const struct firmware	*fw;		/* File image in DRAM */
		const u8		*cfg_data;	/* Payload in DRAM */
		size_t			cfg_len;	/* Length of payload */
		u32			cfg_hash;	/* ID hash */
	} config[SCSC_MX_MAX_COMMON_CFG];
};

struct mxmibref;

/* Zero terminated table of L7 system errors to handle at L7 rather than
 * auto-promote to L8
 */
#define MXFWCONFIG_MAX_NO_PROMOTE 32
extern u32 mxfwconfig_syserr_no_promote[MXFWCONFIG_MAX_NO_PROMOTE];

int mxfwconfig_init(struct scsc_mx *mx);
void mxfwconfig_deinit(struct scsc_mx *mx);
int mxfwconfig_load(struct scsc_mx *mx, struct mxmibref *cfg_ref);
void mxfwconfig_unload(struct scsc_mx *mx);

#endif // __MXFWCONFIG_H
