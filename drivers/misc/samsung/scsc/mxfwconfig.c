/*****************************************************************************
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>

#include "mxfwconfig.h"
#include "miframman.h"
#include "scsc_mx_impl.h"
#include "mxconf.h"

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#define MXFWCONFIG_CFG_SUBDIR	"common"
#define MXFWCONFIG_CFG_FILE_HW	"common.hcf"
#define MXFWCONFIG_CFG_FILE_SW	"common_sw.hcf"

static void mxfwconfig_get_dram_ref(struct scsc_mx *mx, struct mxmibref *cfg_ref);

/* CS-213274-SP */
struct __packed mxfwconfig_vldata {
	u8 data[6];      /* info + 1 or more data octets when it's a multi-octet entry */
};

/* "info" in data[0] of mxfwconfig_vldata*/
#define MX_VLDATA_INFO_INDEX(x)		((x) & 0xFF)	/* When keyval length == 2 */
/* "info" in data[0 or 1] */
#define MX_VLDATA_INFO_MORE(x)		((x) & BIT(7))	/* more octets follow */
#define MX_VLDATA_INFO_SIGN(x)		((x) & BIT(6))	/* sign */
#define MX_VLDATA_INFO_TYPE(x)		((x) & BIT(5))	/* if "more", 1: octet string, 0: integer. Else part of b0-6 of single octet value */
#define MX_VLDATA_INFO_LENGTH(x)	((x) & 0x7F)	/* if "more", length of data or string. Else part of b0-6 of single octet value */

struct __packed mxfwconfig_keyval {
	u16 psid;				/* key ID */
	u16 length;				/* length of vldata in octets */
	struct mxfwconfig_vldata vldata;	/* data */
	/* u8 pad[0|1];	*/			/* padding: 0/1 instances to make data even number of octets */
};

#define MXFWCONFIG_SYSTEM_ERROR_EXCLUDE_FROM_PROMOTION 41 /* lernaSystemErrorExcludeFromPromotion */

u32 mxfwconfig_syserr_no_promote[MXFWCONFIG_MAX_NO_PROMOTE];

static int __mxfwconfig_advance_keyval(struct mxfwconfig_keyval **pkeyval, const u8* conf_data, const size_t conf_len)
{
	if ((*pkeyval)->length == 0) {
		SCSC_TAG_ERR(MX_CFG, "Zero length\n");
		return -EINVAL;
	}

	SCSC_TAG_DEBUG(MX_CFG, "*pkeyval=%p, ->psid=0x%x, ->length=0x%x, sizeof(psid)=%zu, sizeof(length)=%zu, conf_data=%p, conf_len=%zu\n",
		       *pkeyval, (*pkeyval)->psid, (*pkeyval)->length, sizeof((*pkeyval)->psid), sizeof((*pkeyval)->length), conf_data, conf_len);
	/* Advance */
	*pkeyval = (struct mxfwconfig_keyval *)(((u8 *)*pkeyval) +
						(((*pkeyval)->length + 1) & 0xfffe) + /* pad to even length */
						sizeof((*pkeyval)->psid) +
						sizeof((*pkeyval)->length));
	/* Sanity: pointer should be even */
	if ((uintptr_t)*pkeyval & 1) {
		SCSC_TAG_ERR(MX_CFG, "Odd keyval pointer %p\n", *pkeyval);
		return -EINVAL;
	}

	/* Reached end of buffer without finding key? */
	if ((ptrdiff_t)*pkeyval >= (ptrdiff_t)conf_data + conf_len) {
		SCSC_TAG_DEBUG(MX_CFG, "End of buffer pointer %p >= %p\n", *pkeyval, conf_data + conf_len);
		return -ERANGE;
	}
	return 0;
}

/* Parse System Recovery no-promotion list */
static int mxfwconfig_parse_system_recovery(struct scsc_mx *mx, const u8 *conf_data, const size_t conf_len, u32 *syserr_no_promote, int max_promotes)
{
	int r;
	struct mxfwconfig_keyval *keyval = (struct mxfwconfig_keyval *)conf_data;
	u16 promotes = 0;

	/* Default assumption is that promotion list is empty */
	syserr_no_promote[0] = 0;

	/* Look for MXFWCONFIG_SYSTEM_ERROR_EXCLUDE_FROM_PROMOTION anywhere in file */
	while (keyval->psid != MXFWCONFIG_SYSTEM_ERROR_EXCLUDE_FROM_PROMOTION) {
		r = __mxfwconfig_advance_keyval(&keyval, conf_data, conf_len);
		if (r) {
			SCSC_TAG_DEBUG(MX_CFG, "Search for psid%d: %d\n", MXFWCONFIG_SYSTEM_ERROR_EXCLUDE_FROM_PROMOTION, r);
			return r;
		}
	}

	/* If list present, L7 SysErrs promote to L8, maintaining legacy panic/moredump behaviour */

	while (keyval->psid == MXFWCONFIG_SYSTEM_ERROR_EXCLUDE_FROM_PROMOTION) {
		SCSC_TAG_DEBUG(MX_CFG, "PSID=%d, ->length=%d...\n", keyval->psid, keyval->length);

		/* If single entry of 0xFFFFFFFF, only host induced L7 SysErrs promote to L8 */

		/* 32-bit syserr IDs */
		if (keyval->length == 6) { /* 2 len + 4 data */
			SCSC_TAG_DEBUG(MX_CFG, "PSID=%d, ->length=%d, vldata.info=0x%x 0x%x, vldata.more=%d, vldata.length=%d\n",
			      keyval->psid, keyval->length, keyval->vldata.data[0], keyval->vldata.data[1], !!MX_VLDATA_INFO_MORE(keyval->vldata.data[1]), MX_VLDATA_INFO_LENGTH(keyval->vldata.data[1]));

			if (MX_VLDATA_INFO_MORE(keyval->vldata.data[1]) && MX_VLDATA_INFO_LENGTH(keyval->vldata.data[1]) == 4) { /* 4 octets */
				u32 syserr = (keyval->vldata.data[2] << 24) |
					     (keyval->vldata.data[3] << 16) |
					     (keyval->vldata.data[4] << 8) |
					     (keyval->vldata.data[5] << 0);

				SCSC_TAG_DEBUG(MX_CFG, "index=%d: vldata[]=%02x %02x %02x %02x\n",
					      MX_VLDATA_INFO_INDEX(keyval->vldata.data[0]),
					      keyval->vldata.data[2], keyval->vldata.data[3], keyval->vldata.data[4], keyval->vldata.data[5]);

				if (syserr == 0xFFFFFFFF) {
					/* L7 Host Induced promotes to L8 */
					syserr_no_promote[0] = 0xFFFFFFFF;

					SCSC_TAG_INFO(MX_CFG, "syserr_no_promote[0] = 0xFFFFFFFF\n");
					return 0; /* Terminates list */
				} else {
					syserr_no_promote[promotes++] = syserr;
					SCSC_TAG_INFO(MX_CFG, "syserr_no_promote[%d] = 0x%x\n", promotes - 1, syserr);

					/* ID of zero terminates the promotion list
					 * (Tool should not generate 16-bit ID of zero)
					 */
					if (syserr == 0 || promotes == max_promotes) {
						SCSC_TAG_DEBUG(MX_CFG, "Terminated no-promote list (%d)\n", promotes);
						return 0;
					}
				}
			}
		}

		/* If any other entry found, do not promote that syserr to L8 (i.e. handle with fast recovery) */

		/* 16-bit syserr IDs */
		if (keyval->length == 4) { /* 2 len + 2 data */
			SCSC_TAG_DEBUG(MX_CFG, "PSID=%d, ->length=%d, vldata.info=0x%x 0x%x, vldata.more=%d, vldata.length=%d\n",
			      keyval->psid, keyval->length, keyval->vldata.data[0], keyval->vldata.data[1], !!MX_VLDATA_INFO_MORE(keyval->vldata.data[1]), MX_VLDATA_INFO_LENGTH(keyval->vldata.data[1]));

			if (MX_VLDATA_INFO_MORE(keyval->vldata.data[1]) && MX_VLDATA_INFO_LENGTH(keyval->vldata.data[1]) == 2) {
				u16 syserr = keyval->vldata.data[2] << 8 | keyval->vldata.data[3];
				SCSC_TAG_DEBUG(MX_CFG, "index=%d: vldata[]=%02x %02x\n", MX_VLDATA_INFO_INDEX(keyval->vldata.data[0]), keyval->vldata.data[2], keyval->vldata.data[3]);

				syserr_no_promote[promotes++] = syserr;
				SCSC_TAG_INFO(MX_CFG, "syserr_no_promote[%d] = 0x%x\n", promotes - 1, syserr);

				/* ID of zero terminates the promotion list.
				 * (Tool should not generate 16-bit ID of zero)
				 */
				if (syserr == 0 || promotes == max_promotes) {
					SCSC_TAG_DEBUG(MX_CFG, "Terminated no promote list (%d)\n", promotes);
					return 0;
				}
			}
		}

		/* 8-bit syserr IDs */
		if (keyval->length == 2) { /* 2 len, data */
			u8 syserr = keyval->vldata.data[1];
			SCSC_TAG_DEBUG(MX_CFG, "PSID=%d, ->length=%d, vldata.info=0x%x, 0x%0x\n",
			      keyval->psid, keyval->length, keyval->vldata.data[0], keyval->vldata.data[1]);

			SCSC_TAG_DEBUG(MX_CFG, "index=%d: vldata[]=%02x\n", MX_VLDATA_INFO_INDEX(keyval->vldata.data[0]), keyval->vldata.data[1]);

			syserr_no_promote[promotes++] = syserr;
			SCSC_TAG_INFO(MX_CFG, "syserr_no_promote[%d] = 0x%x\n", promotes - 1, syserr);

			/* ID of zero terminates the promotion list */
			if (syserr == 0 || promotes == max_promotes) {
				SCSC_TAG_DEBUG(MX_CFG, "Terminate no promote list (%d)\n", promotes);
				return 0;
			}
		}

		/* Advance */
		r = __mxfwconfig_advance_keyval(&keyval, conf_data, conf_len);
		if (r) {
			SCSC_TAG_DEBUG(MX_CFG, "advance: %d\n", r);
			return r;
		}
	}

	return r;
}

/* Load config into non-shared DRAM */
static int mxfwconfig_load_cfg(struct scsc_mx *mx, struct mxfwconfig *cfg, const char *filename)
{
	int r = 0;
	u32 i;

	if (cfg->configs >= SCSC_MX_MAX_COMMON_CFG) {
		SCSC_TAG_ERR(MX_CFG, "Too many common config files (%u)\n", cfg->configs);
		return -E2BIG;
	}

	i = cfg->configs++; /* Claim next config slot */

	/* Load config file from file system into DRAM */
	r = mx140_file_request_conf(mx, &cfg->config[i].fw, MXFWCONFIG_CFG_SUBDIR, filename);
	if (r)
		return r;

	/* Initial size of file */
	cfg->config[i].cfg_len = cfg->config[i].fw->size;
	cfg->config[i].cfg_data = cfg->config[i].fw->data;

	/* Validate file in DRAM */
	if (cfg->config[i].cfg_len >= MX_COMMON_HCF_HDR_SIZE && /* Room for header */
		/*(cfg->config[i].cfg[6] & 0xF0) == 0x10 && */	/* Curator subsystem */
		cfg->config[i].cfg_data[7] == 1) {		/* First file format */
		int j;

		cfg->config[i].cfg_hash = 0;

		/* Calculate hash */
		for (j = 0; j < MX_COMMON_HASH_SIZE_BYTES; j++) {
			cfg->config[i].cfg_hash =
				(cfg->config[i].cfg_hash << 8) | cfg->config[i].cfg_data[j + MX_COMMON_HASH_OFFSET];
		}

		SCSC_TAG_INFO(MX_CFG, "CFG hash: 0x%.04x\n", cfg->config[i].cfg_hash);

		/* All good - consume header and continue */
		cfg->config[i].cfg_len -= MX_COMMON_HCF_HDR_SIZE;
		cfg->config[i].cfg_data += MX_COMMON_HCF_HDR_SIZE;
	} else {
		SCSC_TAG_ERR(MX_CFG, "Invalid HCF header size %zu\n", cfg->config[i].cfg_len);

		/* Caller must call mxfwconfig_unload_cfg() to release the buffer */
		return -EINVAL;
	}

	/* Running shtotal payload */
	cfg->shtotal += cfg->config[i].cfg_len;

	SCSC_TAG_INFO(MX_CFG, "Loaded common config %s, size %zu, payload size %zu, shared dram total %zu\n",
		filename, cfg->config[i].fw->size, cfg->config[i].cfg_len, cfg->shtotal);

	/* Parse syserr promotion table */
	mxfwconfig_parse_system_recovery(mx,
					 cfg->config[i].cfg_data,
					 cfg->config[i].cfg_len,
					 mxfwconfig_syserr_no_promote,
					 ARRAY_SIZE(mxfwconfig_syserr_no_promote));
	return r;
}

/* Unload config from non-shared DRAM */
static int mxfwconfig_unload_cfg(struct scsc_mx *mx, struct mxfwconfig *cfg, u32 index)
{
	if (index >= SCSC_MX_MAX_COMMON_CFG) {
		SCSC_TAG_ERR(MX_CFG, "Out of range index (%u)\n", index);
		return -E2BIG;
	}

	if (cfg->config[index].fw) {
		SCSC_TAG_DBG3(MX_CFG, "Unload common config %u\n", index);

		mx140_file_release_conf(mx, cfg->config[index].fw);

		cfg->config[index].fw = NULL;
		cfg->config[index].cfg_data = NULL;
		cfg->config[index].cfg_len = 0;
	}

	return 0;
}

/*
 * Load Common config files
 */
int mxfwconfig_load(struct scsc_mx *mx, struct mxmibref *cfg_ref)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);
	struct miframman *miframman = scsc_mx_get_ramman(mx);
	int r;
	u32 i;
	u8 *dest;

	/* HW file is optional */
	r = mxfwconfig_load_cfg(mx, cfg, MXFWCONFIG_CFG_FILE_HW);
	if (r)
		goto done;

	/* SW file is optional, but not without HW file */
	r = mxfwconfig_load_cfg(mx, cfg, MXFWCONFIG_CFG_FILE_SW);
	if (r == -EINVAL) {
		/* If SW file is corrupt, abandon both HW and SW */
		goto done;
	}

	/* Allocate shared DRAM */
	cfg->shdram = miframman_alloc(miframman, cfg->shtotal, 4, MIFRAMMAN_OWNER_COMMON);
	if (!cfg->shdram) {
		SCSC_TAG_ERR(MX_CFG, "MIF alloc failed for %zu octets\n", cfg->shtotal);
		r = -ENOMEM;
		goto done;
	}

	/* Copy files into shared DRAM */
	for (i = 0, dest = (u8 *)cfg->shdram;
	     i < cfg->configs;
	     i++) {
		/* Add to shared DRAM block */
		memcpy(dest, cfg->config[i].cfg_data, cfg->config[i].cfg_len);
		dest += cfg->config[i].cfg_len;
	}

done:
	/* Release the files from non-shared DRAM */
	for (i = 0; i < cfg->configs; i++)
		mxfwconfig_unload_cfg(mx, cfg, i);

	/* Configs abandoned on error */
	if (r)
		cfg->configs = 0;

	/* Pass offset of common HCF data.
	 * FW must ignore if zero length, so set up even if we loaded nothing.
	 */
	mxfwconfig_get_dram_ref(mx, cfg_ref);

	return r;
}

/*
 * Unload Common config data
 */
void mxfwconfig_unload(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);
	struct miframman *miframman = scsc_mx_get_ramman(mx);

	/* Free config block in shared DRAM */
	if (cfg->shdram) {
		SCSC_TAG_INFO(MX_CFG, "Free common config %zu bytes shared DRAM\n", cfg->shtotal);

		miframman_free(miframman, cfg->shdram);

		cfg->configs = 0;
		cfg->shtotal = 0;
		cfg->shdram = NULL;
	}
}

/*
 * Get ref (offset) of config block in shared DRAM
 */
static void mxfwconfig_get_dram_ref(struct scsc_mx *mx, struct mxmibref *cfg_ref)
{
	struct mxfwconfig *mxfwconfig = scsc_mx_get_mxfwconfig(mx);
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mx);

	if (!mxfwconfig->shdram) {
		cfg_ref->offset = (scsc_mifram_ref)0;
		cfg_ref->size = 0;
	} else {
		mif->get_mifram_ref(mif, mxfwconfig->shdram, &cfg_ref->offset);
		cfg_ref->size = (uint32_t)mxfwconfig->shtotal;
	}

	SCSC_TAG_INFO(MX_CFG, "cfg_ref: 0x%x, size %u\n", cfg_ref->offset, cfg_ref->size);
}

/*
 * Init config file module
 */
int mxfwconfig_init(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);

	cfg->configs = 0;
	cfg->shtotal = 0;
	cfg->shdram = NULL;

	return 0;
}

/*
 * Exit config file module
 */
void mxfwconfig_deinit(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);

	/* Leaked memory? */
	WARN_ON(cfg->configs > 0);
	WARN_ON(cfg->shdram);
}

