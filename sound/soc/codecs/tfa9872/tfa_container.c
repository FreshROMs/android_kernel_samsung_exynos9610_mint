/*
 * Copyright 2014 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

#include "tfa_internal.h"
#include "tfa_service.h"
#include "tfa_container.h"
#include "config.h"
#include "tfa.h"
#include "tfa_dsp_fw.h"
#include "tfa98xx_tfafieldnames.h"

/* module globals */
static int tfa98xx_cnt_verbose;

static struct tfa_container *g_cont; /* container file */
static int g_devs = -1; /* nr of devices TODO use direct access to cont? */
static struct tfa_device_list *g_dev[TFACONT_MAXDEVS];
static int g_profs[TFACONT_MAXDEVS];
static int g_liveds[TFACONT_MAXDEVS];
static struct tfa_profile_list  *g_prof[TFACONT_MAXDEVS][TFACONT_MAXPROFS];
static struct tfa_livedata_list  *g_lived[TFACONT_MAXDEVS][TFACONT_MAXPROFS];
static int nxp_tfa_vstep[TFACONT_MAXDEVS];
#define ERROR_STRING "!ERROR!"
#define NONE_STRING "NONE"
#define UNDEF_STRING "Undefined string"
static int partial_enable;

/* defines */
#define MODULE_BIQUADFILTERBANK 2
#define BIQUAD_COEFF_SIZE       6

static void cont_get_devs(struct tfa_container *cont);

static int float_to_int(uint32_t x)
{
	unsigned int e, m;

	e = (0x7F + 31) - ((*(unsigned int *)&x & 0x7F800000) >> 23);
	m = 0x80000000 | (*(unsigned int *)&x << 8);

	return -(int)((m >> e) & -(e < 32));
}

void tfa_set_partial_update(int enp)
{
	partial_enable = enp;
}

/*
 * check the container file and set module global
 */
enum tfa_error tfa_load_cnt(void *cnt, int length)
{
	struct tfa_container *cntbuf = (struct tfa_container *)cnt;

	g_cont = NULL;

	if (length > TFA_MAX_CNT_LENGTH) {
		pr_err("incorrect length\n");
		return tfa_error_container;
	}

	if (HDR(cntbuf->id[0], cntbuf->id[1]) == 0) {
		pr_err("header is 0\n");
		return tfa_error_container;
	}

	if ((HDR(cntbuf->id[0], cntbuf->id[1])) != params_hdr) {
		pr_err("wrong header type: 0x%02x 0x%02x\n",
			cntbuf->id[0], cntbuf->id[1]);
		return tfa_error_container;
	}

	if (cntbuf->size == 0) {
		pr_err("data size is 0\n");
		return tfa_error_container;
	}

	/* check CRC */
	if (tfa_cont_crc_check_container(cntbuf)) {
		pr_err("CRC error\n");
		return tfa_error_container;
	}

	/* check sub version level */
	if ((cntbuf->subversion[1] == NXPTFA_PM_SUBVERSION) &&
		 (cntbuf->subversion[0] == '0')) {
		g_cont = cntbuf;
		cont_get_devs(g_cont);
	} else {
		pr_err("container sub-version not supported: %c%c\n",
				cntbuf->subversion[0], cntbuf->subversion[1]);
		return tfa_error_container;
	}

	return tfa_error_ok;
}

void tfa_deinit(void)
{
	g_cont = NULL;
	g_devs = -1;
}

/*
 * Set the debug option
 */
void tfa_cont_verbose(int level)
{
	tfa98xx_cnt_verbose = level;

	if (tfa98xx_cnt_verbose)
		pr_debug("%s: level:%d\n", __func__, level);
}

/* start count from 1, 0 is invalid */
void tfa_cont_set_current_vstep(int channel, int vstep_idx)
{
	if (channel < TFACONT_MAXDEVS)
		nxp_tfa_vstep[channel] = vstep_idx+1;
	else
		pr_err("channel nr %d>%d\n", channel, TFACONT_MAXDEVS-1);
}

/* start count from 1, 0 is invalid */
int tfa_cont_get_current_vstep(int channel)
{
	if (channel < TFACONT_MAXDEVS)
		return nxp_tfa_vstep[channel]-1;

	pr_err("channel nr %d>%d\n", channel, TFACONT_MAXDEVS-1);
	return TFA_ERROR;
}

struct tfa_container *tfa98xx_get_cnt(void)
{
	return g_cont;
}

/*
 * Dump the contents of the file header
 */
void tfa_cont_show_header(struct tfa_header *hdr)
{
	char _id[2];

	pr_debug("File header\n");

	_id[1] = hdr->id >> 8;
	_id[0] = hdr->id & 0xff;
	pr_debug("\tid:%.2s version:%.2s subversion:%.2s\n", _id,
		hdr->version, hdr->subversion);
	pr_debug("\tsize:%d CRC:0x%08x\n", hdr->size, hdr->crc);
	pr_debug("\tcustomer:%.8s application:%.8s type:%.8s\n",
		hdr->customer, hdr->application, hdr->type);
}

/*
 * return device list dsc from index
 */
struct tfa_device_list *
tfa_cont_get_dev_list(struct tfa_container *cont, int dev_idx)
{
	uint8_t *base = (uint8_t *) cont;

	if ((dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dsc_device)
		return NULL;

	base += cont->index[dev_idx].offset;
	return (struct tfa_device_list *) base;
}

/*
 * get the Nth profile for the Nth device
 */
struct tfa_profile_list *
tfa_cont_get_dev_prof_list(struct tfa_container *cont,
	int dev_idx, int prof_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;
	struct tfa_profile_list *prof_list = NULL;

	dev = tfa_cont_get_dev_list(cont, dev_idx);
	if (!dev)
		return NULL;

	for (idx = 0, hit = 0; idx < dev->length; idx++) {
		if (dev->list[idx].type != dsc_profile)
			continue;
		if (prof_idx != hit++)
			continue;
		prof_list = (struct tfa_profile_list *)
			(dev->list[idx].offset + base);
		break;
	}

	return prof_list;
}

/*
 * get the Nth lifedata for the Nth device
 */
struct tfa_livedata_list *
tfa_cont_get_dev_livedata_list(struct tfa_container *cont,
	int dev_idx, int lifedata_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;
	struct tfa_livedata_list *livedata_list = NULL;

	dev = tfa_cont_get_dev_list(cont, dev_idx);
	if (!dev)
		return NULL;

	for (idx = 0, hit = 0; idx < dev->length; idx++) {
		if (dev->list[idx].type != dsc_livedata)
			continue;
		if (lifedata_idx != hit++)
			continue;
		livedata_list = (struct tfa_livedata_list *)
			(dev->list[idx].offset + base);
		break;
	}

	return livedata_list;
}

/*
 * Get the max volume step associated with Nth profile for the Nth device
 */
int tfa_cont_get_max_vstep(int dev_idx, int prof_idx)
{
	struct tfa_volume_step2_file *vp;
	struct tfa_volume_step_max2_file *vp3;
	int vstep_count = 0;

	vp = (struct tfa_volume_step2_file *)
		tfa_cont_get_file_data(dev_idx, prof_idx, volstep_hdr);
	if (vp == NULL)
		return 0;
	/* check the header type to load different NrOfVStep appropriately */
	if (tfa98xx_dev_family(dev_idx) == 2) {
		/* this is actually tfa2, so re-read the buffer*/
		vp3 = (struct tfa_volume_step_max2_file *)
		tfa_cont_get_file_data(dev_idx, prof_idx, volstep_hdr);
		if (vp3)
			vstep_count = vp3->nr_of_vsteps;
	} else {
		/* this is max1*/
		if (vp)
			vstep_count = vp->vsteps;
	}
	return vstep_count;
}

/**
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 */
struct tfa_file_dsc *
tfa_cont_get_file_data(int dev_idx,
	int prof_idx, enum tfa_header_type type)
{
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	struct tfa_file_dsc *file;
	struct tfa_header *hdr;
	unsigned int i;

	if (g_cont == NULL) {
		pr_err("invalid pointer to container file\n");
		return NULL;
	}

	dev = tfa_cont_get_dev_list(g_cont, dev_idx);
	if (dev == NULL) {
		pr_err("invalid pointer to container file device list\n");
		return NULL;
	}

	/* process the device list until a file type is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			if (file != NULL) {
				hdr = (struct tfa_header *)file->data;
				/* check for file type */
				if (hdr->id == type) {
					/* pr_debug("%s: file found of type "
					 *  "%d in device %s\n",
					 *  __func__, type,
					 * tfa_cont_device_name(dev_idx));
					 */
					return (struct tfa_file_dsc *)
						&file->data;
				}
			}
		}
	}

	/* File not found in device tree.
	 * So, look in the profile list until the file type is encountered
	 */
	prof = tfa_cont_get_dev_prof_list(g_cont, dev_idx, prof_idx);
	if (prof == NULL) {
		pr_err("invalid pointer to container file profile list\n");
		return NULL;
	}

	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset + (uint8_t *)g_cont);
			if (file != NULL) {
				hdr = (struct tfa_header *)file->data;
				if (hdr != NULL) {
					/* check for file type */
					if (hdr->id == type) {
						/* pr_debug("%s: file found of "
						 *  "type %d in profile %s\n",
						 *  __func__, type,
						 * tfa_cont_profile_name
						 *  (dev_idx, prof_idx));
						 */
						return (struct tfa_file_dsc *)
							&file->data;
					}
				}
			}
		}
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("%s: no file found of type %d\n", __func__, type);

	return NULL;
}

/*
 * fill globals
 */
static void cont_get_devs(struct tfa_container *cont)
{
	struct tfa_profile_list *prof;
	struct tfa_livedata_list *lived;
	int i, j;
	int count;

	/* get nr of devlists+1 */
	for (i = 0; i < cont->ndev; i++)
		g_dev[i] = tfa_cont_get_dev_list(cont, i); /* cache it */

	g_devs = cont->ndev;
	/* walk through devices and get the profile lists */
	for (i = 0; i < g_devs; i++) {
		j = 0;
		count = 0;
		while ((prof = tfa_cont_get_dev_prof_list(cont, i, j))
				!= NULL) {
			count++;
			g_prof[i][j++] = prof;
		}
		g_profs[i] = count; /* count the nr of profiles per device */
	}

	g_devs = cont->ndev;
	/* walk through devices and get the livedata lists */
	for (i = 0; i < g_devs; i++) {
		j = 0;
		count = 0;
		while ((lived = tfa_cont_get_dev_livedata_list(cont, i, j))
				!= NULL) {
			count++;
			g_lived[i][j++] = lived;
		}
		g_liveds[i] = count; /* count the nr of livedata per device */
	}
}

/*
 * write a parameter file to the device
 */
static enum tfa98xx_error
tfa_cont_write_vstep(int dev_idx,
	struct tfa_volume_step2_file *vp, int vstep)
{
	enum tfa98xx_error err;
	unsigned short vol;

	if (vstep < vp->vsteps) {
		/* vol = (unsigned short)(voldB / (-0.5f)); */
		vol = (unsigned short)
			(-2 * float_to_int
			(*((uint32_t *)&vp->vstep[vstep].attenuation)));
		if (vol > 255)	/* restricted to 8 bits */
			vol = 255;

		err = tfa98xx_set_volume_level(dev_idx, vol);
		if (err != TFA98XX_ERROR_OK)
			return err;

		err = tfa98xx_dsp_write_preset
			(dev_idx, sizeof(vp->vstep[0].preset),
			vp->vstep[vstep].preset);
		if (err != TFA98XX_ERROR_OK)
			return err;
		err = tfa_cont_write_filterbank
			(dev_idx, vp->vstep[vstep].filter);

	} else {
		pr_err("Incorrect volume given. The value vstep[%d] >= %d\n",
			nxp_tfa_vstep[dev_idx], vp->vsteps);
		err = TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("vstep[%d][%d]\n", dev_idx, vstep);

	return err;
}

static struct tfa_volume_step_message_info *
tfa_cont_get_msg_info_from_reg(struct tfa_volume_step_register_info *reg_info)
{
	char *p = (char *)reg_info;

	p += sizeof(reg_info->nr_of_registers)
		+ (reg_info->nr_of_registers * sizeof(uint32_t));
	return (struct tfa_volume_step_message_info *)p;
}

static int
tfa_cont_get_msg_len(struct tfa_volume_step_message_info *msg_info)
{
	return (msg_info->message_length.b[0] << 16)
		+ (msg_info->message_length.b[1] << 8)
		+ msg_info->message_length.b[2];
}

static struct tfa_volume_step_message_info *
tfa_cont_get_next_msg_info(struct tfa_volume_step_message_info *msg_info)
{
	char *p = (char *)msg_info;
	int msgLen = tfa_cont_get_msg_len(msg_info);
	int type = msg_info->message_type;

	p += sizeof(msg_info->message_type) + sizeof(msg_info->message_length);
	if (type == 3)
		p += msgLen;
	else
		p += msgLen * 3;

	return (struct tfa_volume_step_message_info *)p;
}

static struct tfa_volume_step_register_info *
tfa_cont_get_next_reg_from_end_info(
	struct tfa_volume_step_message_info *msg_info)
{
	char *p = (char *)msg_info;

	p += sizeof(msg_info->nr_of_messages);
	return (struct tfa_volume_step_register_info *)p;

}

static struct tfa_volume_step_register_info*
tfa_cont_get_reg_for_vstep(struct tfa_volume_step_max2_file *vp, int idx)
{
	int i, j, nrMessage;

	struct tfa_volume_step_register_info *reg_info
		= (struct tfa_volume_step_register_info *)vp->vsteps_bin;
	struct tfa_volume_step_message_info *msg_info = NULL;

	for (i = 0; i < idx; i++) {
		msg_info = tfa_cont_get_msg_info_from_reg(reg_info);
		nrMessage = msg_info->nr_of_messages;

		for (j = 0; j < nrMessage; j++)
			msg_info = tfa_cont_get_next_msg_info(msg_info);
		reg_info = tfa_cont_get_next_reg_from_end_info(msg_info);
	}

	return reg_info;
}

struct tfa_partial_msg_block {
	uint8_t offset;
	uint16_t change;
	uint8_t update[16][3];
} __packed;

static enum tfa98xx_error
tfa_cont_write_vstepMax2_One(int dev_idx,
	struct tfa_volume_step_message_info *new_msg,
	struct tfa_volume_step_message_info *old_msg, int enable_partial_update)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int len = (tfa_cont_get_msg_len(new_msg) - 1) * 3;
	char *buf = (char *)new_msg->parameter_data;
	uint8_t *partial = NULL;
	int partial_size = 0;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buffer_p_index = -1, partial_p_index = -1;
#endif
	uint8_t cmdid[3];
	int use_partial_coeff = 0;

	if (enable_partial_update) {
		if (new_msg->message_type != old_msg->message_type) {
			pr_debug("Message type differ - Disable Partial Update\n");
			enable_partial_update = 0;
		} else if (tfa_cont_get_msg_len(new_msg)
			!= tfa_cont_get_msg_len(old_msg)) {
			pr_debug("Message Length differ - Disable Partial Update\n");
			enable_partial_update = 0;
		}
	}

	if ((enable_partial_update) && (new_msg->message_type == 1)) {
		/* No patial updates for message type 1 (Coefficients) */
		enable_partial_update = 0;
		if ((tfa98xx_dev_revision(dev_idx) & 0xff) == 0x88)
			use_partial_coeff = 1;
	}

	/* Change Message Len to the actual buffer len */
	memcpy(cmdid, new_msg->cmd_id, sizeof(cmdid));

	/* The algoparams and mbdrc msg id will be changed
	 * to the reset type when SBSL=0
	 * if SBSL=1 the msg will remain unchanged.
	 * It's up to the tuning engineer to choose the 'without_reset'
	 * types inside the vstep.
	 * In other words: the reset msg is applied during SBSL==0
	 * else it remains unchanged.
	 */
	pr_info("%s: is_cold %d\n", __func__, handles_local[dev_idx].is_cold);
	/* if (TFA_GET_BF(dev_idx, SBSL) == 0) { */
	if (handles_local[dev_idx].is_cold == 1) {
		uint8_t org_cmd = cmdid[2];

		if ((new_msg->message_type == 0) &&
			(cmdid[2] != SB_PARAM_SET_ALGO_PARAMS))
			/* SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET */
		{
			pr_debug("P-ID for SetAlgoParams modified! cmdid[2]=0x%2x (to 0x00)\n",
				cmdid[2]);
			cmdid[2] = SB_PARAM_SET_ALGO_PARAMS;
		} else if ((new_msg->message_type == 2) &&
			(cmdid[2] != SB_PARAM_SET_MBDRC))
			/* SB_PARAM_SET_MBDRC_WITHOUT_RESET */
		{
			pr_debug("P-ID for SetMBDrc modified! cmdid[2]=0x%2x (to 0x07)\n",
				cmdid[2]);
			cmdid[2] = SB_PARAM_SET_MBDRC;
		}

		if (org_cmd != cmdid[2])
			pr_info("P-ID: cmdid[2]=0x%02x to 0x%02x\n",
				org_cmd, cmdid[2]);
	}

	/*
	 * +sizeof(struct tfa_partial_msg_block) will allow to fit one
	 * additonnal partial block If the partial update goes over the len of
	 * a regular message, we can safely write our block and check afterward
	 * that we are over the size of a usual update
	 */
	if (enable_partial_update) {
		partial_size = (sizeof(uint8_t) * len)
			+ sizeof(struct tfa_partial_msg_block);
#if defined(TFADSP_DSP_BUFFER_POOL)
		partial_p_index = tfa98xx_buffer_pool_access
			(dev_idx, -1, partial_size, POOL_GET);
		if (partial_p_index != -1) {
			pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
				__func__, partial_p_index, partial_size);
			partial = (uint8_t *)
				(handles_local[dev_idx]
				.buf_pool[partial_p_index].pool);
		} else {
			partial = kmalloc(partial_size, GFP_KERNEL);
		}
#else
		partial = kmalloc(partial_size, GFP_KERNEL);
#endif /* TFADSP_DSP_BUFFER_POOL */
	}

	if (partial) {
		uint8_t offset = 0, i = 0;
		uint16_t *change;
		uint8_t *n = new_msg->parameter_data;
		uint8_t *o = old_msg->parameter_data;
		uint8_t *p = partial;
		uint8_t *trim = partial;

		/* set dspFiltersReset */
		*p++ = 0x02;
		*p++ = 0x00;
		*p++ = 0x00;

		while ((o < (old_msg->parameter_data + len)) &&
			(p < (partial + len - 3))) {
			if ((offset == 0xff) ||
				(memcmp(n, o, 3 * sizeof(uint8_t)))) {
				*p++ = offset;
				change = (uint16_t *)p;
				*change = 0;
				p += 2;

				for (i = 0; (i < 16)
				     && (o < (old_msg->parameter_data + len));
				     i++, n += 3, o += 3) {
					if (memcmp(n, o, 3 * sizeof(uint8_t))) {
						*change |= BIT(i);
						memcpy(p, n, 3);
						p += 3;
						trim = p;
					}
				}

				offset = 0;
				*change = cpu_to_be16(*change);
			} else {
				n += 3;
				o += 3;
				offset++;
			}
		}

		if (trim == partial) {
			pr_debug("No Change in message - discarding %d bytes\n",
				len);
			len = 0;
		} else if (trim < (partial + len - 3)) {
			pr_debug("Using partial update: %d -> %d bytes\n",
				len, (int)(trim-partial + 3));

			/* Add the termination marker */
			memset(trim, 0x00, 3);
			trim += 3;

			/* Signal This will be a partial update */
			cmdid[2] |= BIT(6);
			buf = (char *)partial;
			len = (int)(trim - partial);
		} else {
			pr_debug("Partial too big - use regular update\n");
		}
	} else {
		if (!enable_partial_update)
			pr_debug("Partial update - Not enabled\n");
		else /* partial == NULL */
			pr_err("Partial update memory error - Disabling\n");
	}

	if (use_partial_coeff) {
		err = dsp_partial_coefficients
			(dev_idx, old_msg->parameter_data,
			new_msg->parameter_data);
	} else if (len) {
		uint8_t *buffer;

		pr_debug("Command-ID used: 0x%02x%02x%02x\n",
			cmdid[0], cmdid[1], cmdid[2]);

#if defined(TFADSP_DSP_BUFFER_POOL)
		buffer_p_index = tfa98xx_buffer_pool_access
			(dev_idx, -1, 3 + len, POOL_GET);
		if (buffer_p_index != -1) {
			pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
				__func__, buffer_p_index, 3 + len);
			buffer = (char *)(handles_local[dev_idx]
				.buf_pool[buffer_p_index].pool);
		} else {
			buffer = kmalloc(3 + len, GFP_KERNEL);
			if (buffer == NULL)
				goto tfa_cont_write_vstepMax2_One_error_exit;
		}
#else
		buffer = kmalloc(3 + len, GFP_KERNEL);
		if (buffer == NULL)
			goto tfa_cont_write_vstepMax2_One_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

		memcpy(&buffer[0], cmdid, 3);
		memcpy(&buffer[3], buf, len);
		err = dsp_msg(dev_idx, 3 + len, (char *)buffer);

#if defined(TFADSP_DSP_BUFFER_POOL)
		if (buffer_p_index != -1) {
			tfa98xx_buffer_pool_access
				(dev_idx, buffer_p_index, 0, POOL_RETURN);
		} else {
			kfree(buffer);
		}
#else
		kfree(buffer);
#endif /* TFADSP_DSP_BUFFER_POOL */
	}

tfa_cont_write_vstepMax2_One_error_exit:
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (partial_p_index != -1) {
		tfa98xx_buffer_pool_access
			(dev_idx, partial_p_index, 0, POOL_RETURN);
	} else {
		kfree(partial);
	}
#else
	kfree(partial);
#endif /* TFADSP_DSP_BUFFER_POOL */

	return err;
}

static enum tfa98xx_error
tfa_cont_write_vstepMax2(int dev_idx,
	struct tfa_volume_step_max2_file *vp,
	int vstep_idx, int vstep_msg_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	static struct tfa_volume_step_register_info *p_reg_info;
	struct tfa_volume_step_register_info *reg_info = NULL;
	struct tfa_volume_step_message_info *msg_info = NULL,
		*p_msg_info = NULL;
	struct tfa_bitfield bit_f;
	int i, nr_messages, enp = partial_enable;

	if (vstep_idx >= vp->nr_of_vsteps) {
		pr_debug("Volumestep %d is not available\n", vstep_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (p_reg_info == NULL) {
		pr_debug("Initial vstep write\n");
		enp = 0;
	}

	reg_info = tfa_cont_get_reg_for_vstep(vp, vstep_idx);

	msg_info = tfa_cont_get_msg_info_from_reg(reg_info);
	nr_messages = msg_info->nr_of_messages;

	if (enp) {
		p_msg_info = tfa_cont_get_msg_info_from_reg(p_reg_info);
		if (nr_messages != p_msg_info->nr_of_messages) {
			pr_debug("Message different - Disable partial update\n");
			enp = 0;
		}
	}

	for (i = 0; i < nr_messages; i++) {
		/* Messagetype(3) is Smartstudio Info! Dont send this! */
		if (msg_info->message_type == 3) {
			pr_debug("Skipping Message Type 3\n");
			/* message_length is in bytes */
			msg_info = tfa_cont_get_next_msg_info(msg_info);
			if (enp)
				p_msg_info = tfa_cont_get_next_msg_info
					(p_msg_info);
			continue;
		}

		/* If no vstepMsgIndex is passed on,
		 * all message needs to be send
		 */
		if ((vstep_msg_idx >= TFA_MAX_VSTEP_MSG_MARKER)
			|| (vstep_msg_idx == i)) {
			err = tfa_cont_write_vstepMax2_One
				(dev_idx, msg_info, p_msg_info, enp);
			if (err != TFA98XX_ERROR_OK) {
				/*
				 * Force a full update for the next write
				 * As the current status of the DSP is unknown
				 */
				p_reg_info = NULL;
				return err;
			}
		}

		msg_info = tfa_cont_get_next_msg_info(msg_info);
		if (enp)
			p_msg_info = tfa_cont_get_next_msg_info(p_msg_info);
	}

	p_reg_info = reg_info;

	for (i = 0; i < reg_info->nr_of_registers * 2; i++) {
		/* Byte swap the datasheetname */
		bit_f.field = (uint16_t)(reg_info->register_info[i] >> 8)
			| (reg_info->register_info[i] << 8);
		i++;
		bit_f.value = (uint16_t)reg_info->register_info[i] >> 8;
		err = tfa_run_write_bitfield(dev_idx, bit_f);
		if (err != TFA98XX_ERROR_OK)
			return err;
	}

	/* Save the current vstep */
	tfa_set_swvstep(dev_idx, (unsigned short)vstep_idx);

	return err;
}

/*
 * Write DRC message to the dsp
 * If needed modify the cmd-id
 */

enum tfa98xx_error
	tfa_cont_write_drc_file(int dev_idx, int size, uint8_t data[])
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	uint8_t cmdid_changed[3], modified = 0;

	if (TFA_GET_BF(dev_idx, SBSL) == 0) {
		/* Only do this when not set already */
		if (data[2] != SB_PARAM_SET_MBDRC) {
			cmdid_changed[0] = data[0];
			cmdid_changed[1] = data[1];
			cmdid_changed[2] = SB_PARAM_SET_MBDRC;
			modified = 1;

			if (tfa98xx_cnt_verbose) {
				pr_debug("P-ID for SetMBDrc modified!: ");
				pr_debug("Command-ID used: 0x%02x%02x%02x\n",
					cmdid_changed[0],
					cmdid_changed[1],
					cmdid_changed[2]);
			}
		}
	}

	if (modified == 1) {
		/* Send payload to dsp (Remove 3 from the length for cmdid) */
		err = tfa_dsp_msg_id
			(dev_idx, size - 3, (const char *)data, cmdid_changed);
	} else {
		/* Send cmd_id + payload to dsp */
		err = dsp_msg(dev_idx, size, (const char *)data);
	}

	return err;
}


/*
 * write a parameter file to the device
 * The VstepIndex and VstepMsgIndex are only used to write
 * a specific msg from the vstep file.
 */
enum tfa98xx_error
tfa_cont_write_file(int dev_idx,
	struct tfa_file_dsc *file, int vstep_idx, int vstep_msg_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_header *hdr = (struct tfa_header *)file->data;
	char *data_buf;
	enum tfa_header_type type;
	int size, temp_index;

	if (tfa98xx_cnt_verbose)
		tfa_cont_show_header(hdr);

	type = (enum tfa_header_type) hdr->id;

	switch (type) {
	case msg_hdr: /* generic DSP message */
		size = hdr->size - sizeof(struct tfa_msg_file);

		/* write temp stored in driver */
		data_buf = (char *)((struct tfa_msg_file *)hdr)->data;
		if (data_buf[1] == 0x80
			&& data_buf[2] == 0x14
			&& handles_local[dev_idx].temp != 0xffff) {
			temp_index = (1 + 2 + dev_idx) * 3;
			pr_info("%s: temp in msg 0x%02x%02x%02x",
				__func__, data_buf[temp_index],
				data_buf[temp_index + 1],
				data_buf[temp_index + 2]);

			data_buf[temp_index]
				= (char)((handles_local[dev_idx].temp
					& 0xff0000) >> 16);
			data_buf[temp_index + 1]
				= (char)((handles_local[dev_idx].temp
					& 0x00ff00) >> 8);
			data_buf[temp_index + 2]
				= (char)(handles_local[dev_idx].temp
					& 0x0000ff);

			pr_info("%s: temp from driver 0x%02x%02x%02x",
				__func__, data_buf[temp_index],
				data_buf[temp_index + 1],
				data_buf[temp_index + 2]);
		}

		err = dsp_msg(dev_idx, size,
			      (const char *)((struct tfa_msg_file *)hdr)->data);
		break;
	case volstep_hdr:
		if (tfa98xx_dev_family(dev_idx) == 2)
			err = tfa_cont_write_vstepMax2
				(dev_idx,
				(struct tfa_volume_step_max2_file *)hdr,
				vstep_idx, vstep_msg_idx);
		else
			err = tfa_cont_write_vstep
				(dev_idx,
				(struct tfa_volume_step2_file *)hdr,
				vstep_idx);

		/* If writing the vstep was succesful, set new current vstep */
		if (err == TFA98XX_ERROR_OK) {
			handles_local[dev_idx].is_bypass = 0;
			tfa_cont_set_current_vstep(dev_idx, vstep_idx);
		}

		break;
	case speaker_hdr:
		if (tfa98xx_dev_family(dev_idx) == 2) {
			/* Remove header and xml_id */
			size = hdr->size - sizeof(struct tfa_spk_header)
				- sizeof(struct tfa_fw_ver);
			err = dsp_msg
				(dev_idx, size,
				(const char *)
				(((struct tfa_speaker_file *)hdr)->data
				+ (sizeof(struct tfa_fw_ver))));
		} else {
			size = hdr->size - sizeof(struct tfa_speaker_file);
			err = tfa98xx_dsp_write_speaker_parameters
				(dev_idx, size,
				(const unsigned char *)
				((struct tfa_speaker_file *)hdr)->data);
		}
		break;
	case preset_hdr:
		size = hdr->size - sizeof(struct tfa_preset_file);
		err = tfa98xx_dsp_write_preset
			(dev_idx, size,
			(const unsigned char *)
			((struct tfa_preset_file *)hdr)->data);
		break;
	case equalizer_hdr:
		err = tfa_cont_write_filterbank
			(dev_idx, ((struct tfa_equalizer_file *)hdr)->filter);
		break;
	case patch_hdr:
		size = hdr->size - sizeof(struct tfa_patch_file);
		/* total length */
		err = tfa_dsp_patch(dev_idx,  size,
			(const unsigned char *)
			((struct tfa_patch_file *)hdr)->data);
		break;
	case config_hdr:
		size = hdr->size - sizeof(struct tfa_config_file);
		err = tfa98xx_dsp_write_config
			(dev_idx, size,
			(const unsigned char *)
			((struct tfa_config_file *)hdr)->data);
		break;
	case drc_hdr:
		if (hdr->version[0] == NXPTFA_DR3_VERSION) {
			/* Size is total size - hdrsize(36) - xmlversion(3) */
			size = hdr->size - sizeof(struct tfa_drc_file2);
			err = tfa_cont_write_drc_file
				(dev_idx, size,
				((struct tfa_drc_file2 *)hdr)->data);
		} else {
			/*
			 * The DRC file is split as:
			 * 36 bytes for generic header
			 * (customer, application, and type)
			 * 127x3 (381) bytes first block contains
			 *             the device and sample rate
			 *             independent settings
			 * 127x3 (381) bytes block
			 *             the device and sample rate
			 *             specific values.
			 * The second block can always be recalculated
			 * from the first block,
			 * if vlsCal and the sample rate are known.
			 */
			/* = hdr->size - sizeof(struct tfa_drc_file); */
			size = 381; /* fixed size for first block */

			/* 381 is done to only send 2nd part of the drc block */
			err = tfa98xx_dsp_write_drc
				(dev_idx, size,
				((const unsigned char *)
				((struct tfa_drc_file *)hdr)->data+381));
		}
		break;
	case info_hdr:
		/* Ignore */
		break;
	default:
		pr_err("Header is of unknown type: 0x%x\n", type);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return err;
}

/**
 * get the 1st of this dsc type this devicelist
 */
struct tfa_desc_ptr *
tfa_cnt_get_dsc(struct tfa_container *cnt,
	enum tfa_descriptor_type type, int dev_idx)
{
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);
	struct tfa_desc_ptr *this;
	int i;

	if (!dev)
		return NULL;

	/* process the list until a the type is encountered */
	for (i = 0; i < dev->length; i++)
		if (dev->list[i].type == (uint32_t)type) {
			this = (struct tfa_desc_ptr *)
				(dev->list[i].offset+(uint8_t *)cnt);
			return this;
		}

	return NULL;
}

/**
 * get the device type from the patch in this devicelist
 *  - find the patch file for this devidx
 *  - return the devid from the patch or 0 if not found
 */
int tfa_cont_get_devid(struct tfa_container *cnt, int dev_idx)
{
	struct tfa_patch_file *patchfile;
	struct tfa_desc_ptr *patchdsc;
	uint8_t *patchheader;
	unsigned short devid, checkaddress;
	int checkvalue;

	patchdsc = tfa_cnt_get_dsc(cnt, dsc_patch, dev_idx);
	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (struct tfa_patch_file *)patchdsc;

	patchheader = patchfile->data;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue = (patchheader[3] << 16)
		+ (patchheader[4] << 8) + patchheader[5];

	devid = patchheader[0];

	if (checkaddress == 0xFFFF
		&& checkvalue != 0xFFFFFF && checkvalue != 0) {
		devid = patchheader[5] << 8 | patchheader[0]; /* full revid */
	}

	return devid;
}

/*
 * get the slave for the device if it exists
 */
enum tfa98xx_error tfa_cont_get_slave(int dev_idx, uint8_t *slave_addr)
{
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);

	if (dev == 0)
		return TFA98XX_ERROR_BAD_PARAMETER;

	*slave_addr = dev->dev;
	return TFA98XX_ERROR_OK;
}

/*
 * write a bit field
 */
enum tfa98xx_error
tfa_run_write_bitfield(tfa98xx_handle_t dev_idx,
	struct tfa_bitfield bf)
{
	enum tfa98xx_error error;
	uint16_t value;
	union {
		uint16_t field;
		struct tfa_bf_enum bf_enum;
	} bf_uni;

	value = bf.value;
	bf_uni.field = bf.field;
	/* print all the bitfield writing */
	if (tfa98xx_cnt_verbose)
		pr_debug("bitfield: %s=0x%x (0x%x[%d..%d]=0x%x)\n",
			tfa_cont_bf_name
			(bf_uni.field, tfa98xx_dev_revision(dev_idx)), value,
			bf_uni.bf_enum.address, bf_uni.bf_enum.pos,
			bf_uni.bf_enum.pos+bf_uni.bf_enum.len, value);
	error = tfa_set_bf(dev_idx, bf_uni.field, value);

	return error;
}

/*
 * read a bit field
 */
enum tfa98xx_error
tfa_run_read_bitfield(tfa98xx_handle_t dev_idx,
	struct tfa_bitfield *bf)
{
	enum tfa98xx_error error;
	union {
		uint16_t field;
		struct tfa_bf_enum bf_enum;
	} bf_uni;
	uint16_t regvalue, msk;

	bf_uni.field = bf->field;

	error = reg_read
		(dev_idx, (unsigned char)(bf_uni.bf_enum.address), &regvalue);
	if (error)
		return error;

	msk = ((1 << (bf_uni.bf_enum.len + 1)) - 1) << bf_uni.bf_enum.pos;

	regvalue &= msk;
	bf->value = regvalue >> bf_uni.bf_enum.pos;

	return error;
}

/*
 * dsp mem direct write
 */
enum tfa98xx_error
tfa_run_write_dsp_mem(tfa98xx_handle_t dev, struct tfa_dsp_mem *cfmem)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int i;

	for (i = 0; i < cfmem->size; i++) {
		if (tfa98xx_cnt_verbose)
			pr_debug("dsp mem (%d): 0x%02x=0x%04x\n",
				cfmem->type, cfmem->address, cfmem->words[i]);

		error = mem_write
			(dev, cfmem->address++, cfmem->words[i], cfmem->type);
		if (error)
			return error;
	}

	return error;
}

/*
 * write filter payload to DSP
 * note that the data is in an aligned union for all filter variants
 * the aa data is used but it's the same for all of them
 */
enum tfa98xx_error
tfa_run_write_filter(tfa98xx_handle_t dev, union tfa_cont_biquad *bq)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	enum tfa98xx_dmem dmem;
	uint16_t address;
	uint8_t data[3*3+sizeof(bq->aa.bytes)];
	int i, channel = 0, runs = 1;
	int8_t saved_index = bq->aa.index; /* This is used to set back index */

	/* Channel=1 is primary, Channel=2 is secondary*/
	if (bq->aa.index > 100) {
		bq->aa.index -= 100;
		channel = 2;
	} else {
		if (bq->aa.index > 50) {
			bq->aa.index -= 50;
			channel = 1;
		} else if (tfa98xx_dev_family(dev) == 2) {
			runs = 2;
		}
	}

	if (tfa98xx_cnt_verbose) {
		if (channel == 2)
			pr_debug("filter[%d,S]", bq->aa.index);
		else if (channel == 1)
			pr_debug("filter[%d,P]", bq->aa.index);
		else
			pr_debug("filter[%d]", bq->aa.index);
	}

	for (i = 0; i < runs; i++) {
		if (runs == 2)
			channel++;

		/* get the target address for the filter on this device */
		dmem = tfa98xx_filter_mem(dev, bq->aa.index, &address, channel);
		if (dmem == TFA98XX_DMEM_ERR) {
			pr_debug("Warning: No memory location found to write filter settings! Filter settings are skipped!\n");
			/* Don't exit with an error here,
			 * We could continue without problems
			 */
			return TFA98XX_ERROR_OK;
		}

		/* send a DSP memory message
		 * that targets the devices specific memory for the filter
		 * msg params: which_mem, start_offset, num_words
		 */
		memset(data, 0, 3*3);
		data[2] = dmem; /* output[0] = which_mem */
		data[4] = address >> 8; /* output[1] = start_offset */
		data[5] = address & 0xff;
		data[8] = sizeof(bq->aa.bytes)/3; /*output[2] = num_words */
		/* payload */
		memcpy(&data[9], bq->aa.bytes, sizeof(bq->aa.bytes));

		if (tfa98xx_dev_family(dev) == 2) {
			error = tfa_dsp_cmd_id_write
				(dev, MODULE_FRAMEWORK, FW_PAR_ID_SET_MEMORY,
				sizeof(data), data);
		} else {
			error = tfa_dsp_cmd_id_write
				(dev, MODULE_FRAMEWORK, 4, /* param */
				sizeof(data), data);
		}
	}

#if defined(FLOAT_COMPATIBLE)
/* floating-point error from cross-complier compatibility */
	if (tfa98xx_cnt_verbose) {
		char buf[50];

		if (bq->aa.index == 13) {
			snprintf(buf, 50, "%d,%.0f,%.2f",
				bq->in.type, bq->in.cut_off_freq,
				bq->in.leakage);
		} else if (bq->aa.index >= 10 && bq->aa.index <= 12) {
			snprintf(buf, 50, "%d,%.0f,%.1f,%.1f", bq->aa.type,
				bq->aa.cut_off_freq, bq->aa.ripple_db,
				bq->aa.rolloff);
		} else {
			strlcpy(buf, "unsupported filter index", 50);
		}

		pr_debug("=%s\n", buf);
	}
#endif

	/* Because we can load the same filters multiple times
	 * For example: When we switch profile we re-write in operating mode.
	 * We then need to remember the index (primary, secondary or both)
	 */
	bq->aa.index = saved_index;

	return error;
}

/*
 * write the register based on the input address, value and mask
 * only the part that is masked will be updated
 */
enum tfa98xx_error
tfa_run_write_register(tfa98xx_handle_t handle, struct tfa_reg_patch *reg)
{
	enum tfa98xx_error error;
	uint16_t value, newvalue;

	if (tfa98xx_cnt_verbose)
		pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n",
			reg->address, reg->value, reg->mask);

	error = reg_read(handle, reg->address, &value);
	if (error)
		return error;

	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;

	value |= newvalue;
	error = reg_write(handle, reg->address, value);

	return error;
}

/*
 * return the bitfield
 */
struct tfa_bitfield tfa_cont_dsc2bf(struct tfa_desc_ptr dsc)
{
	uint32_t *ptr = (uint32_t *) (&dsc);
	union {
	struct tfa_bitfield bf;
	uint32_t num;
	} num_bf;

	num_bf.num = *ptr; /* & TFA_BITFIELDDSCMSK; */

	return num_bf.bf;
}

/* write reg and bitfield items in the devicelist to the target */
enum tfa98xx_error tfa_cont_write_regs_dev(int dev_idx)
{
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);
	struct tfa_bitfield *bit_f;
	int i;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (!dev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bit_f = (struct tfa_bitfield *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			err = tfa_run_write_bitfield(dev_idx, *bit_f);
		}
		if (dev->list[i].type == dsc_register)
			err = tfa_run_write_register
				(dev_idx, (struct tfa_reg_patch *)
				(dev->list[i].offset+(char *)g_cont));
		if (err)
			break;
	}
	return err;
}

/* write reg and bitfield items in the profilelist the target */
enum tfa98xx_error
tfa_cont_write_regs_prof(int dev_idx, int prof_idx)
{
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	struct tfa_bitfield *bitf;
	unsigned int i;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa98xx_cnt_verbose)
		pr_debug("----- profile: %s (%d) -----\n",
			tfa_cont_get_string(&prof->name), prof_idx);

	/* process the list until the end of profile or the default section */
	for (i = 0; i < prof->length; i++) {
		/* write values before default section when we switch profile */
		if (prof->list[i].type == dsc_default)
			break;

		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset+(uint8_t *)g_cont);
			err = tfa_run_write_bitfield(dev_idx, *bitf);
		}
		if (prof->list[i].type == dsc_register)
			err = tfa_run_write_register
				(dev_idx, (struct tfa_reg_patch *)
				(prof->list[i].offset+(char *)g_cont));
		if (err)
			break;
	}
	return err;
}

/* write patchfile in the devicelist to the target */
enum tfa98xx_error tfa_cont_write_patch(int dev_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);
	struct tfa_file_dsc *file;
	struct tfa_patch_file *patchfile;
	int size, i;

	if (!dev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list until a patch is encountered */
	for (i = 0; i < dev->length; i++)
		if (dev->list[i].type == dsc_patch) {
			file = (struct tfa_file_dsc *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			patchfile = (struct tfa_patch_file *)&file->data;

			if (tfa98xx_cnt_verbose)
				tfa_cont_show_header(&patchfile->hdr);

			size = patchfile->hdr.size
				- sizeof(struct tfa_patch_file);
			/* size is total length */
			err = tfa_dsp_patch
				(dev_idx, size,
				(const unsigned char *)patchfile->data);
			if (err)
				return err;
		}

	return TFA98XX_ERROR_OK;
}

/* write all param files in the devicelist to the target */
enum tfa98xx_error tfa_cont_write_files(int dev_idx)
{
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);
	struct tfa_file_dsc *file;
	struct tfa_cmd *cmd;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 bytes, and 3 is the msg */
	int i, size = 0;

	if (!dev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list and write all files  */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			if (tfa_cont_write_file
				(dev_idx, file, 0, TFA_MAX_VSTEP_MSG_MARKER)) {
				return TFA98XX_ERROR_BAD_PARAMETER;
			}
		}

		if (dev->list[i].type == dsc_set_input_select ||
			dev->list[i].type == dsc_set_output_select ||
			dev->list[i].type == dsc_set_program_config ||
			dev->list[i].type == dsc_set_lag_w ||
			dev->list[i].type == dsc_set_gains ||
			dev->list[i].type == dsc_set_vbat_factors ||
			dev->list[i].type == dsc_set_senses_cal ||
			dev->list[i].type == dsc_set_senses_delay ||
			dev->list[i].type == dsc_set_mb_drc) {
			create_dsp_buffer_msg
				((struct tfa_msg *)
				(dev->list[i].offset + (char *)g_cont),
				buffer, &size);
			if (tfa98xx_cnt_verbose) {
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(dev->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);
			}

			err = dsp_msg(dev_idx, size, buffer);
		}

		if (dev->list[i].type == dsc_cmd) {
			size = *(uint16_t *)
				(dev->list[i].offset + (char *)g_cont);
			err = dsp_msg
				(dev_idx, size,
				dev->list[i].offset+2 + (char *)g_cont);
			if (tfa98xx_cnt_verbose) {
				cmd = (struct tfa_cmd *)
					(dev->list[i].offset+(uint8_t *)g_cont);
				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					cmd->value[0], cmd->value[1],
					cmd->value[2]);
			}
		}
		if (err != TFA98XX_ERROR_OK)
			break;

		if (dev->list[i].type == dsc_cf_mem)
			err = tfa_run_write_dsp_mem
				(dev_idx, (struct tfa_dsp_mem *)
				(dev->list[i].offset+(uint8_t *)g_cont));

		if (err != TFA98XX_ERROR_OK)
			break;
	}

	return err;
}

/*
 * write all param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 */
enum tfa98xx_error
tfa_cont_write_files_prof(int dev_idx, int prof_idx, int vstep_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 bytes, and 3 is the msg */
	unsigned int i;
	struct tfa_file_dsc *file;
	struct tfa_cmd *cmd;
	struct tfa_patch_file *patchfile;
	int size;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list and write all files  */
	for (i = 0; i < prof->length; i++) {
		switch (prof->list[i].type) {
		case dsc_file:
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset
				 + (uint8_t *)g_cont);
			err = tfa_cont_write_file
				(dev_idx, file,
				 vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dsc_patch:
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset
				 + (uint8_t *)g_cont);
			patchfile = (struct tfa_patch_file *)
				&file->data;
			if (tfa98xx_cnt_verbose)
				tfa_cont_show_header(&patchfile->hdr);
			size = patchfile->hdr.size
				- sizeof(struct tfa_patch_file);
			/* size is total length */
			err = tfa_dsp_patch
				(dev_idx, size,
				 (const unsigned char *)
				 patchfile->data);
			break;
		case dsc_cf_mem:
			err = tfa_run_write_dsp_mem
				(dev_idx,
				 (struct tfa_dsp_mem *)
				 (prof->list[i].offset
				  + (uint8_t *)g_cont));
			break;
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
			create_dsp_buffer_msg
				((struct tfa_msg *)
				(prof->list[i].offset
				+ (uint8_t *)g_cont),
				buffer, &size);
			if (tfa98xx_cnt_verbose) {
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(prof->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);
			}

			err = dsp_msg(dev_idx, size, buffer);
			break;
		case dsc_cmd:
			size = *(uint16_t *)
				(prof->list[i].offset
				+ (char *)g_cont);
			err = dsp_msg
				(dev_idx, size,
				prof->list[i].offset
				+ 2 + (char *)g_cont);

			if (tfa98xx_cnt_verbose) {
				cmd = (struct tfa_cmd *)
					(prof->list[i].offset
					+ (uint8_t *)g_cont);
				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					cmd->value[0],
					cmd->value[1],
					cmd->value[2]);
			}
			break;
		default:
			/* ignore any other type */
			break;
		}
	}

	return err;
}

enum tfa98xx_error
tfa_cont_write_item(int dev_idx, struct tfa_desc_ptr *dsc)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	/* struct tfa_file_dsc *file; */
	struct tfa_reg_patch *reg;
	struct tfa_mode *cas;
	struct tfa_bitfield *bitf;

	switch (dsc->type) {
	case dsc_default:
	case dsc_device: /* ignore */
	case dsc_profile: /* profile list */
		break;
	case dsc_register: /* register patch */
		reg = (struct tfa_reg_patch *)(dsc->offset+(uint8_t *)g_cont);
		return tfa_run_write_register(dev_idx, reg);
		/* pr_debug("$0x%2x=0x%02x,0x%02x\n",
		 * reg->address, reg->mask, reg->value);
		 */
		break;
	case dsc_string: /* ascii: zero terminated string */
		pr_debug(";string: %s\n", tfa_cont_get_string(dsc));
		break;
	case dsc_file: /* filename + file contents */
	case dsc_patch:
		break;
	case dsc_mode:
		cas = (struct tfa_mode *)(dsc->offset+(uint8_t *)g_cont);
		if (cas->value == TFA98XX_MODE_RCV)
			tfa98xx_select_mode(dev_idx, TFA98XX_MODE_RCV);
		else
			tfa98xx_select_mode(dev_idx, TFA98XX_MODE_NORMAL);
		break;
	case dsc_cf_mem:
		err = tfa_run_write_dsp_mem
			(dev_idx,
			(struct tfa_dsp_mem *)
			(dsc->offset+(uint8_t *)g_cont));
		break;
	case dsc_bit_field:
		bitf = (struct tfa_bitfield *)(dsc->offset+(uint8_t *)g_cont);
		return tfa_run_write_bitfield(dev_idx, *bitf);
	case dsc_filter:
		return tfa_run_write_filter
			(dev_idx,
			(union tfa_cont_biquad *)
			(dsc->offset+(uint8_t *)g_cont));
	}

	return err;
}

static unsigned int tfa98xx_sr_from_field(unsigned int field)
{
	switch (field) {
	case 0:
		return 8000;
	case 1:
		return 11025;
	case 2:
		return 12000;
	case 3:
		return 16000;
	case 4:
		return 22050;
	case 5:
		return 24000;
	case 6:
		return 32000;
	case 7:
		return 44100;
	case 8:
		return 48000;
	default:
		return 0;
	}
}

enum tfa98xx_error tfa_write_filters(int dev_idx, int prof_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	unsigned int i;
	int status;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa98xx_cnt_verbose) {
		pr_debug("----- profile: %s (%d) -----\n",
			tfa_cont_get_string(&prof->name), prof_idx);
		pr_debug("Waiting for CLKS...\n");
	}

	for (i = 10; i > 0; i--) {
		err = tfa98xx_dsp_system_stable(dev_idx, &status);
		if (status)
			break;
		msleep_interruptible(10);
	}

	if (i == 0) {
		if (tfa98xx_cnt_verbose)
			pr_err("Unable to write filters, CLKS=0\n");

		return TFA98XX_ERROR_STATE_TIMED_OUT;
	}

	/* process the list until the end of profile or default section */
	for (i = 0; i < prof->length; i++)
		if (prof->list[i].type == dsc_filter)
			if (tfa_cont_write_item(dev_idx, &prof->list[i])
				!= TFA98XX_ERROR_OK)
				return TFA98XX_ERROR_BAD_PARAMETER;

	return err;
}

unsigned int tfa98xx_get_profile_sr(int dev_idx, unsigned int prof_idx)
{
	struct tfa_bitfield *bitf;
	unsigned int i;
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	int fs_profile = -1;

	dev = tfa_cont_device(dev_idx);
	if (!dev)
		return 0;

	prof = tfa_cont_profile(dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_default)
			break;

		/* check for profile settingd (AUDFS) */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
	}

	pr_debug("%s - profile fs: 0x%x = %dHz (%d - %d)\n", __func__,
		fs_profile, tfa98xx_sr_from_field(fs_profile),
		dev_idx, prof_idx);
	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
		/* Ignore register case */
	}

	pr_debug("%s - default fs: 0x%x = %dHz (%d - %d)\n",
		__func__, fs_profile,
		tfa98xx_sr_from_field(fs_profile),
		dev_idx, prof_idx);
	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	return 48000;
}

unsigned int tfa98xx_get_profile_chsa(int dev_idx, unsigned int prof_idx)
{
	struct tfa_bitfield *bitf;
	unsigned int i;
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	int chsa_profile = -1;

	/* bypass case in Max1 */
	if (tfa98xx_dev_family(dev_idx) != 1)
		return 0;

	dev = tfa_cont_device(dev_idx);
	if (!dev)
		return 0;

	prof = tfa_cont_profile(dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_default)
			break;

		/* check for profile setting (CHSA) */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA1_BF_CHSA) {
				chsa_profile = bitf->value;
				break;
			}
		}
	}

	pr_debug("%s - profile chsa: 0x%x (%d - %d)\n", __func__,
		 chsa_profile, dev_idx, prof_idx);
	if (chsa_profile != -1)
		return chsa_profile;

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA1_BF_CHSA) {
				chsa_profile = bitf->value;
				break;
			}
		}
		/* Ignore register case */
	}

	pr_debug("%s - default chsa: 0x%x (%d - %d)\n", __func__,
		chsa_profile, dev_idx, prof_idx);
	if (chsa_profile != -1)
		return chsa_profile;

	chsa_profile = tfa_get_bf(dev_idx, TFA1_BF_CHSA);
	pr_debug("%s - current chsa: 0x%x\n", __func__,
		chsa_profile);

	return chsa_profile;
}

unsigned int tfa98xx_get_profile_tdmspks(int dev_idx, unsigned int prof_idx)
{
	struct tfa_bitfield *bitf;
	unsigned int i;
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	int tdmspks_profile = -1;

	/* bypass case in Max2 */
	if (tfa98xx_dev_family(dev_idx) != 2)
		return 0;

	dev = tfa_cont_device(dev_idx);
	if (!dev)
		return 0;

	prof = tfa_cont_profile(dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_default)
			break;

		/* check for profile setting (TDMSPKS) */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset+(uint8_t *)g_cont);
			switch (handles_local[dev_idx].rev & 0xff) {
			case 0x72:
				if (bitf->field == TFA9872_BF_TDMSPKS)
					tdmspks_profile = bitf->value;
				break;
			default:
				break;
			}
			if (tdmspks_profile != -1)
				break;
		}
	}

	pr_debug("%s - profile tdmspks: 0x%x (%d - %d)\n", __func__,
		 tdmspks_profile, dev_idx, prof_idx);
	if (tdmspks_profile != -1)
		return tdmspks_profile;

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			switch (handles_local[dev_idx].rev & 0xff) {
			case 0x72:
				if (bitf->field == TFA9872_BF_TDMSPKS)
					tdmspks_profile = bitf->value;
				break;
			default:
				break;
			}
			if (tdmspks_profile != -1)
				break;
		}
		/* Ignore register case */
	}

	pr_debug("%s - default tdmspks: 0x%x (%d - %d)\n", __func__,
		 tdmspks_profile, dev_idx, prof_idx);
	if (tdmspks_profile != -1)
		return tdmspks_profile;

	switch (handles_local[dev_idx].rev & 0xff) {
	case 0x72:
		tdmspks_profile = tfa_get_bf(dev_idx, TFA9872_BF_TDMSPKS);
		break;
	default:
		pr_info("%s - use default tdmspks\n", __func__);
		tdmspks_profile = 0;
		break;
	}
	pr_debug("%s - current tdmspks: 0x%x\n", __func__,
		tdmspks_profile);

	return tdmspks_profile;
}

enum tfa98xx_error
get_sample_rate_info(int dev_idx, struct tfa_profile_list *prof,
	struct tfa_profile_list *previous_prof, int fs_previous_profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_bitfield *bitf;
	unsigned int i;
	int fs_default_profile = 8;	/* default is 48kHz */
	int fs_next_profile = 8;		/* default is 48kHz */


	/* ---------- default settings previous profile ---------- */
	for (i = 0; i < previous_prof->length; i++) {
		/* Search for the default section */
		if (i == 0) {
			while ((previous_prof->list[i].type != dsc_default)
				&& (i < previous_prof->length)) {
				i++;
			}
			i++;
		}

		/* Only if we found the default section search for AUDFS */
		if (i < previous_prof->length) {
			if (previous_prof->list[i].type == dsc_bit_field) {
				bitf = (struct tfa_bitfield *)
					(previous_prof->list[i].offset
					+ (uint8_t *)g_cont);
				if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
					fs_default_profile = bitf->value;
					break;
				}
			}
		}
	}

	/* ---------- settings next profile ---------- */
	for (i = 0; i < prof->length; i++) {
		/* write the values before the default section */
		if (prof->list[i].type == dsc_default)
			break;
		/* search for AUDFS */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_next_profile = bitf->value;
				break;
			}
		}
	}

	/* Enable if needed for debugging!
	 * if (tfa98xx_cnt_verbose) {
	 *  pr_debug("sample rate from the previous profile: %d\n",
	 *   fs_previous_profile);
	 *  pr_debug("sample rate in the default section: %d\n",
	 *   fs_default_profile);
	 *  pr_debug("sample rate for the next profile: %d\n",
	 *   fs_next_profile);
	 * }
	 */

	if (fs_next_profile != fs_default_profile) {
		if (tfa98xx_cnt_verbose)
			pr_debug("Writing delay tables for AUDFS=%d\n",
				fs_next_profile);

		/* If the AUDFS from the next profile is not the same as
		 * the AUDFS from the default we need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(dev_idx, fs_next_profile);
	} else if (fs_default_profile != fs_previous_profile) {
		if (tfa98xx_cnt_verbose)
			pr_debug("Writing delay tables for AUDFS=%d\n",
				fs_default_profile);

		/* But if we do not have a new AUDFS in the next profile and
		 * the AUDFS from the default profile is not the same as AUDFS
		 * from the previous profile we need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(dev_idx, fs_default_profile);
	}

	return err;
}

/*
 * process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 */
enum tfa98xx_error
tfa_cont_write_profile(int dev_idx, int prof_idx, int vstep_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	struct tfa_profile_list *previous_prof = tfa_cont_profile
		(dev_idx, tfa_get_swprof(dev_idx));
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 bytes, and 3 is the msg */
	unsigned int i, k = 0, j = 0;
	struct tfa_file_dsc *file;
	struct tfa_cmd *cmd;
	uint8_t slave_address = 0;
	int size = 0, fs_previous_profile = 8; /* default fs is 48kHz*/
	int dev_family = tfa98xx_dev_family(dev_idx);

	if (!prof || !previous_prof) {
		pr_err("Error trying to get the (previous) swprofile\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (tfa98xx_cnt_verbose) {
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n",
			tfa_cont_device_name(dev_idx),
			tfa_cont_profile_name(dev_idx, prof_idx),
			vstep_idx);
	}

	/* Get the slave */
	err = tfa_cont_get_slave(dev_idx, &slave_address);

	/* We only make a power cycle when profiles are not in the same group */
	if (prof->group == previous_prof->group && prof->group != 0) {
		if (tfa98xx_cnt_verbose) {
			pr_debug("The new profile (%s) is in the same group as the current profile (%s)\n",
				tfa_cont_get_string(&prof->name),
				tfa_cont_get_string(&previous_prof->name));
		}
	} else {
		/* mute */
		tfa_run_mute(dev_idx);

		/* Get current sample rate before we start switching */
		fs_previous_profile = TFA_GET_BF(dev_idx, AUDFS);

		/* clear SBSL to make sure we stay in initCF state */
		if (tfa98xx_dev_family(dev_idx) == 2)
			TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);

		/* When we switch profile we first power down the subsystem
		 * This should only be done when we are in operating mode
		 */
		if (((dev_family == 2)
			&& (TFA_GET_BF(dev_idx, MANSTATE) == 9))
			|| (dev_family != 2)) {
			err = tfa98xx_powerdown(dev_idx, 1);
			if (err)
				return err;
		} else {
			pr_debug("No need to go to powerdown now\n");
		}
	}

	/* set all bitfield settings */
	/* First set all default settings */
	if (tfa98xx_cnt_verbose) {
		pr_debug("------ default settings profile: %s (%d) ------\n",
			tfa_cont_get_string(&previous_prof->name),
			tfa_get_swprof(dev_idx));

		if (tfa98xx_dev_family(dev_idx) == 2)
			err = show_current_state(dev_idx);
	}

	/* Loop profile length */
	for (i = 0; i < previous_prof->length; i++) {
		/* Search for the default section */
		if (i == 0) {
			while (previous_prof->list[i].type != dsc_default
			       && i < previous_prof->length)
				i++;
			i++;
		}

		/* Only if we found the default section try writing the items */
		if (i < previous_prof->length)
			if (tfa_cont_write_item
				(dev_idx,  &previous_prof->list[i])
				!= TFA98XX_ERROR_OK)
				return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("------ new settings profile: %s (%d) ------\n",
			tfa_cont_get_string(&prof->name), prof_idx);

	/* set new settings */
	for (i = 0; i < prof->length; i++) {
		/* Remember where we currently are with writing items*/
		j = i;

		/* write values before default section when we switch profile */
		/* process and write all non-file items */
		switch (prof->list[i].type) {
		case dsc_file:
		case dsc_patch:
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
		case dsc_cmd:
		case dsc_filter:
		case dsc_default:
			/* When one of these files are found, we exit */
			i = prof->length;
			break;
		default:
			err = tfa_cont_write_item
				(dev_idx,  &prof->list[i]);
			if (err != TFA98XX_ERROR_OK)
				return TFA98XX_ERROR_BAD_PARAMETER;
			break;
		}
	}

	if (prof->group != previous_prof->group || prof->group == 0) {
		if (tfa98xx_dev_family(dev_idx) == 2)
			TFA_SET_BF_VOLATILE(dev_idx, MANSCONF, 1);

		/* Leave powerdown state */
		err = tfa_cf_powerup(dev_idx);
		if (err)
			return err;
		if (tfa98xx_cnt_verbose && tfa98xx_dev_family(dev_idx) == 2)
			err = show_current_state(dev_idx);

		if (tfa98xx_dev_family(dev_idx) == 2) {
			/* Reset SBSL to 0 (workaround of enbl_powerswitch=0) */
			TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);
			/* Sending commands to DSP need to make sure RST is 0
			 * (otherwise we get no response)
			 */
			TFA_SET_BF(dev_idx, RST, 0);
		}
	}

	/* Check if there are sample rate changes */
	err = get_sample_rate_info
		(dev_idx, prof, previous_prof, fs_previous_profile);
	if (err)
		return err;

	/* Write files from previous profile (default section)
	 * Should only be used for the patch&trap patch (file)
	 */
	if (tfa98xx_dev_family(dev_idx) == 2) {
		for (i = 0; i < previous_prof->length; i++) {
			/* Search for the default section */
			if (i == 0) {
				while (previous_prof->list[i].type
					!= dsc_default
					&& i < previous_prof->length) {
					i++;
				}
				i++;
			}

			/* Only if we found default section try writing file */
			if (i < previous_prof->length) {
				char type = previous_prof->list[i].type;

				if (type == dsc_file || type == dsc_patch) {
					/* Only write this once */
					if (tfa98xx_cnt_verbose && k == 0) {
						pr_debug("------ files default profile: %s (%d) ----------\n",
							tfa_cont_get_string
							(&previous_prof->name),
							prof_idx);
						k++;
					}
					file = (struct tfa_file_dsc *)
						(previous_prof->list[i].offset
						+ (uint8_t *)g_cont);
					err = tfa_cont_write_file
						(dev_idx, file, vstep_idx,
						TFA_MAX_VSTEP_MSG_MARKER);
				}
			}
		}
	}

	if (tfa98xx_cnt_verbose) {
		pr_debug("------ files new profile: %s (%d) --------\n",
			tfa_cont_get_string(&prof->name), prof_idx);
	}

	/* write everything until end or the default section starts
	 * Start where we currenly left
	 */
	for (i = j; i < prof->length; i++) {
		/* write values before default section when we switch profile */
		if (prof->list[i].type == dsc_default)
			break;

		switch (prof->list[i].type) {
		case dsc_file:
		case dsc_patch:
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset
				 + (uint8_t *)g_cont);
			err = tfa_cont_write_file
				(dev_idx, file, vstep_idx,
				 TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
			create_dsp_buffer_msg
				((struct tfa_msg *)
				 (prof->list[i].offset
				  + (char *)g_cont), buffer, &size);
			err = dsp_msg(dev_idx, size, buffer);

			if (tfa98xx_cnt_verbose)
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(prof->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);
			break;
		case dsc_cmd:
			size = *(uint16_t *)
				(prof->list[i].offset
				 + (char *)g_cont);
			err = dsp_msg
				(dev_idx, size,
				 prof->list[i].offset + 2 + (char *)g_cont);

			if (tfa98xx_cnt_verbose) {
				cmd = (struct tfa_cmd *)
					(prof->list[i].offset
					 + (uint8_t *)g_cont);
				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					cmd->value[0],
					cmd->value[1],
					cmd->value[2]);
			}
			break;
		default:
			/* write bitfield, register, xmem after files */
			if (tfa_cont_write_item
			    (dev_idx,  &prof->list[i])
			   != TFA98XX_ERROR_OK)
				return TFA98XX_ERROR_BAD_PARAMETER;
			break;
		}

		if (err != TFA98XX_ERROR_OK)
			return err;
	}

	if (handles_local[0].ext_dsp) { /* check only master device */
		/* put SetRe25 message to indicate all messages are send */
		pr_info("%s: tfa_set_calibration_values\n", __func__);
		tfa_set_swprof(dev_idx, (unsigned short)prof_idx);
		err = tfa_set_calibration_values(dev_idx);
		if (err)
			pr_info("%s: set calibration values error = %d\n",
				__func__, err);
	}

	if ((prof->group != previous_prof->group || prof->group == 0)
		&& (tfa98xx_dev_family(dev_idx) == 2) && (slave_address > 10)) {
		if (TFA_GET_BF(dev_idx, REFCKSEL) == 0) {
			/* set SBSL to go to operation mode */
			TFA_SET_BF_VOLATILE(dev_idx, SBSL, 1);
		}
	}

	return err;
}

/*
 *  process only vstep in the profilelist
 *
 */
enum tfa98xx_error
tfa_cont_write_files_vstep(int dev_idx, int prof_idx, int vstep_idx)
{
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	unsigned int i;
	struct tfa_file_dsc *file;
	struct tfa_header *hdr;
	enum tfa_header_type type;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa98xx_cnt_verbose)
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n",
			tfa_cont_device_name(dev_idx),
			tfa_cont_profile_name(dev_idx, prof_idx),
			vstep_idx);

	/* write vstep file only! */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset + (uint8_t *)g_cont);
			hdr = (struct tfa_header *)file->data;
			type = (enum tfa_header_type) hdr->id;

			switch (type) {
			case volstep_hdr:
				if (tfa_cont_write_file
					(dev_idx, file, vstep_idx,
					TFA_MAX_VSTEP_MSG_MARKER))
					return TFA98XX_ERROR_BAD_PARAMETER;
				break;
			default:
				break;
			}
		}
	}

	return err;
}

char *tfa_cont_get_string(struct tfa_desc_ptr *dsc)
{
	if (dsc->type != dsc_string)
		return UNDEF_STRING;

	return dsc->offset + (char *)g_cont;
}

void individual_calibration_results(tfa98xx_handle_t handle)
{
	int value_p, value_s;

	/* Read the calibration result
	 * in xmem (529=primary channel) (530=secondary channel)
	 */
	mem_read(handle, 529, 1, &value_p);
	mem_read(handle, 530, 1, &value_s);

	if (value_p != 1 && value_s != 1)
		pr_debug("Calibration failed on both channels!\n");
	else if (value_p != 1) {
		pr_debug("Calibration failed on Primary (Left) channel!\n");
		TFA_SET_BF_VOLATILE(handle, SSLEFTE, 0);
		/* Disable the sound for the left speaker */
	} else if (value_s != 1) {
		pr_debug("Calibration failed on Secondary (Right) channel!\n");
		TFA_SET_BF_VOLATILE(handle, SSRIGHTE, 0);
		/* Disable the sound for the right speaker */
	}

	TFA_SET_BF_VOLATILE(handle, AMPINSEL, 0);
	/* Set amplifier input to TDM */
	TFA_SET_BF_VOLATILE(handle, SBSL, 1);
}

char *tfa_cont_get_command_string(uint32_t type)
{
	if (type == dsc_set_input_select)
		return "SetInputSelector";
	else if (type == dsc_set_output_select)
		return "SetOutputSelector";
	else if (type == dsc_set_program_config)
		return "SetProgramConfig";
	else if (type == dsc_set_lag_w)
		return "SetLagW";
	else if (type == dsc_set_gains)
		return "SetGains";
	else if (type == dsc_set_vbat_factors)
		return "SetvBatFactors";
	else if (type == dsc_set_senses_cal)
		return "SetSensesCal";
	else if (type == dsc_set_senses_delay)
		return "SetSensesDelay";
	else if (type == dsc_set_mb_drc)
		return "SetMBDrc";
	else if (type == dsc_filter)
		return "filter";
	else
		return UNDEF_STRING;
}

/*
 * Get the name of the device at a certain index in the container file
 * return device name
 */
char *tfa_cont_device_name(int dev_idx)
{
	struct tfa_device_list *dev;

	if (dev_idx >= tfa98xx_cnt_max_device())
		return ERROR_STRING;

	dev = tfa_cont_device(dev_idx);
	if (dev == NULL)
		return ERROR_STRING;

	return tfa_cont_get_string(&dev->name);
}

/*
 * Get the application name from the container file application field
 * note that the input stringbuffer should be sizeof(application field)+1
 */
int tfa_cont_get_app_name(char *name)
{
	int i, len = 0;

	for (i = 0; i < (int)sizeof(g_cont->application); i++) {
		if (isalnum(g_cont->application[i])) /* copy char if valid */
			name[len++] = g_cont->application[i];
		if (g_cont->application[i] == '\0')
			break;
	}
	name[len++] = '\0';

	return len;
}

/*
 * Get profile index of the calibration profile.
 * Returns: (profile index) if found, (-2) if no
 * calibration profile is found or (-1) on error
 */
int tfa_cont_get_cal_profile(int dev_idx)
{
	int prof, nprof, cal_idx = -2;

	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return TFA_ERROR;

	nprof = tfa_cont_max_profile(dev_idx);
	/* search for the calibration profile in the list of profiles */
	for (prof = 0; prof < nprof; prof++) {
		if (strnstr(tfa_cont_profile_name(dev_idx, prof),
			".cal", strlen(tfa_cont_profile_name(dev_idx, prof)))
			!= NULL) {
			cal_idx = prof;
			pr_debug("Using calibration profile: '%s'\n",
				tfa_cont_profile_name(dev_idx, prof));
			break;
		}
	}
	return cal_idx;
}

/**
 * Is the profile a tap profile ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfa_cont_is_tap_profile(int dev_idx, int prof_idx)
{
	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return TFA_ERROR;

	/* Check if next profile is tap profile */
	if (strnstr(tfa_cont_profile_name(dev_idx, prof_idx),
		".tap", strlen(tfa_cont_profile_name(dev_idx, prof_idx)))
		!= NULL) {
		pr_debug("Using Tap profile: '%s'\n",
			tfa_cont_profile_name(dev_idx, prof_idx));
		return 1;
	}

	return 0;
}

/**
 * Is the profile specific to device ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile belongs to device or 0 if not
 */
int tfa_cont_is_dev_specific_profile(int dev_idx, int prof_idx)
{
	char dev_substring[100] = {0};
	char *pch;
	int prof_name_len;

	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return 0;

	prof_name_len = strlen(tfa_cont_profile_name(dev_idx, prof_idx));
	pch = strnchr(tfa_cont_profile_name(dev_idx, prof_idx),
		strlen(tfa_cont_profile_name(dev_idx, prof_idx)), '.');
	if (!pch)
		return 0;

	snprintf(dev_substring, 100, ".%s", tfa_cont_device_name(dev_idx));
	if (prof_name_len < strlen(dev_substring))
		return 0;

	/* Check if next profile is tap profile */
	if (strnstr(tfa_cont_profile_name(dev_idx, prof_idx),
		dev_substring, prof_name_len) != NULL) {
		pr_debug("dev profile: '%s' of device '%s'\n",
			tfa_cont_profile_name(dev_idx, prof_idx),
			dev_substring);
		return 1;
	}

	return 0;
}

/*
 * Get the name of the profile at certain index for a device
 * in the container file
 *  return profile name
 */
char *tfa_cont_profile_name(int dev_idx, int prof_idx)
{
	struct tfa_profile_list *prof;

	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return ERROR_STRING;
	if ((prof_idx < 0) || (prof_idx >= tfa_cont_max_profile(dev_idx)))
		return NONE_STRING;

	/* the Nth profiles for this device */
	prof = tfa_cont_get_dev_prof_list(g_cont, dev_idx, prof_idx);
	return tfa_cont_get_string(&prof->name);
}

/*
 * return 1st profile list
 */
struct tfa_profile_list *
tfa_cont_get_1st_prof_list(struct tfa_container *cont)
{
	struct tfa_profile_list *prof;
	uint8_t *b = (uint8_t *) cont;

	int maxdev = 0;
	struct tfa_device_list *dev;

	/* get nr of devlists */
	maxdev = cont->ndev;
	/* get last devlist */
	dev = tfa_cont_get_dev_list(cont, maxdev - 1);
	if (dev == NULL)
		return NULL;
	/* the 1st profile starts after the last device list */
	b = (uint8_t *) dev + sizeof(struct tfa_device_list)
		+ dev->length * (sizeof(struct tfa_desc_ptr));
	prof = (struct tfa_profile_list *) b;
	return prof;
}

/*
 * return 1st livedata list
 */
struct tfa_livedata_list *
tfa_cont_get_1st_livedata_list(struct tfa_container *cont)
{
	struct tfa_livedata_list *ldata;
	struct tfa_profile_list *prof;
	struct tfa_device_list *dev;
	uint8_t *b = (uint8_t *) cont;
	int maxdev, maxprof;

	/* get nr of devlists+1 */
	maxdev = cont->ndev;
	/* get nr of proflists */
	maxprof = cont->nprof;

	/* get last devlist */
	dev = tfa_cont_get_dev_list(cont, maxdev - 1);
	if (dev == NULL)
		return NULL;
	/* the 1st livedata starts after the last device list */
	b = (uint8_t *) dev + sizeof(struct tfa_device_list) +
		dev->length * (sizeof(struct tfa_desc_ptr));

	while (maxprof != 0) {
		/* get last proflist */
		prof = (struct tfa_profile_list *) b;
		b += sizeof(struct tfa_profile_list) +
			((prof->length-1) * (sizeof(struct tfa_desc_ptr)));
		maxprof--;
	}

	/* Else the marker falls off */
	b += 4; /* bytes */

	ldata = (struct tfa_livedata_list *) b;
	return ldata;
}


enum tfa98xx_error tfa_cont_open(int dev_idx)
{
	return tfa98xx_open((tfa98xx_handle_t)dev_idx);
}

enum tfa98xx_error tfa_cont_close(int dev_idx)
{
	return tfa98xx_close(dev_idx);
}

/*
 * return the device count in the container file
 */
int tfa98xx_cnt_max_device(void)
{
	return (g_cont != NULL)
		? ((g_cont->ndev < TFACONT_MAXDEVS)
		? g_cont->ndev : TFACONT_MAXDEVS) : 0;
}
EXPORT_SYMBOL(tfa98xx_cnt_max_device);

/*
 * lookup slave and return device index
 */
int tfa98xx_cnt_slave2idx(int slave_addr)
{
	int idx;

	for (idx = 0; idx < g_devs; idx++) {
		if (g_dev[idx] == NULL)
			continue;
		if (g_dev[idx]->dev == slave_addr)
			return idx;
	}

	return TFA_ERROR;
}

/*
 * lookup slave and return device revid
 */
int tfa98xx_cnt_slave2revid(int slave_addr)
{
	int idx = tfa98xx_cnt_slave2idx(slave_addr);
	uint16_t revid;

	if (idx < 0)
		return idx;

	/* note that the device must have been opened before */
	revid = tfa98xx_get_device_revision(idx);

	/* quick check for valid contents */
	return (revid&0xFF) >= 0x12 ? revid : -1;
}

/*
 * return the device list pointer
 */
struct tfa_device_list *tfa_cont_device(int dev_idx)
{
	if (dev_idx < g_devs) {
		if (g_dev[dev_idx] == NULL)
			return NULL;
		return g_dev[dev_idx];
	}
	/* pr_err("Devlist index too high:%d!", idx); */
	return NULL;
}

/*
 * return the per device profile count
 */
int tfa_cont_max_profile(int dev_idx)
{
	if (dev_idx >= g_devs) {
		/* pr_err("Devlist index too high:%d!", ndev); */
		return 0;
	}
	return g_profs[dev_idx];
}

/*
 * return the next profile:
 *  - assume that all profiles are adjacent
 *  - calculate the total length of the input
 *  - the input profile + its length is the next profile
 */
struct tfa_profile_list *tfa_cont_next_profile(struct tfa_profile_list *prof)
{
	uint8_t *this, *next; /* byte pointers for byte pointer arithmetic */
	struct tfa_profile_list *nextprof;
	int listlength; /* total length of list in bytes */

	if (prof == NULL)
		return NULL;

	if (prof->id != TFA_PROFID)
		return NULL;	/* invalid input */

	this = (uint8_t *)prof;
	/* nr of items in the list, length includes name dsc so - 1*/
	listlength = (prof->length - 1)*sizeof(struct tfa_desc_ptr);
	/* the sizeof(struct tfa_profile_list) includes the list[0] length */
	next = this + listlength + sizeof(struct tfa_profile_list);
		/* - sizeof(struct tfa_desc_ptr); */
	nextprof = (struct tfa_profile_list *)next;

	if (nextprof->id != TFA_PROFID)
		return NULL;

	return nextprof;
}

/*
 * return the next livedata
 */
struct tfa_livedata_list *
tfa_cont_next_livedata(struct tfa_livedata_list *livedata)
{
	struct tfa_livedata_list *nextlivedata
		= (struct tfa_livedata_list *)
			((char *)livedata + (livedata->length * 4)
			+ sizeof(struct tfa_livedata_list) - 4);

	if (nextlivedata->id == TFA_LIVEDATAID)
		return nextlivedata;

	return NULL;
}

/*
 * return the device list pointer
 */
struct tfa_profile_list *tfa_cont_profile(int dev_idx, int prof_ipx)
{
	if (dev_idx >= g_devs) {
		/* pr_err("Devlist index too high:%d!", ndev); */
		return NULL;
	}
	if (prof_ipx >= g_profs[dev_idx]) {
		/* pr_err("Proflist index too high:%d!", nprof); */
		return NULL;
	}

	return g_prof[dev_idx][prof_ipx];
}

/*
 * check CRC for container
 * CRC is calculated over the bytes following the CRC field
 * return non zero value on error
 */
int tfa_cont_crc_check_container(struct tfa_container *cont)
{
	uint8_t *base;
	size_t size;
	uint32_t crc;

	base = (uint8_t *)&cont->crc + 4;
	/* ptr to bytes following the CRC field */
	size = (size_t)(cont->size - (base - (uint8_t *)cont));
	/* nr of bytes following the CRC field */
	crc = ~crc32_le(~0u, base, size);

	return crc != cont->crc;
}

/**
 * Create a buffer which can be used to send to the dsp.
 */
void create_dsp_buffer_msg(struct tfa_msg *msg, char *buffer, int *size)
{
	int i, j = 0;

	/* Copy cmd_id. Remember that the cmd_id is reversed */
	buffer[0] = msg->cmd_id[2];
	buffer[1] = msg->cmd_id[1];
	buffer[2] = msg->cmd_id[0];

	/* Copy the data to the buffer */
	for (i = 3; i < 3 + (msg->msg_size * 3); i++) {
		buffer[i] = (uint8_t) ((msg->data[j] >> 16) & 0xffff);
		i++;
		buffer[i] = (uint8_t) ((msg->data[j] >> 8) & 0xff);
		i++;
		buffer[i] = (uint8_t) (msg->data[j] & 0xff);
		j++;
	}

	*size = (3 + (msg->msg_size * 3)) * sizeof(char);
}

void get_all_features_from_cnt(tfa98xx_handle_t dev_idx,
	int *hw_feature_register, int sw_feature_register[2])
{
	struct tfa_features *features;
	int i;

	struct tfa_device_list *dev = tfa_cont_device(dev_idx);

	/* Init values in case no keyword is defined in cnt file: */
	*hw_feature_register = -1;
	sw_feature_register[0] = -1;
	sw_feature_register[1] = -1;

	if (dev == NULL)
		return;

	/* process the device list */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_features) {
			features = (struct tfa_features *)
				(dev->list[i].offset+(uint8_t *)g_cont);
			*hw_feature_register = features->value[0];
			sw_feature_register[0] = features->value[1];
			sw_feature_register[1] = features->value[2];
			break;
		}
	}
}

/* wrapper function */
void get_hw_features_from_cnt(tfa98xx_handle_t dev_idx,
	int *hw_feature_register)
{
	int sw_feature_register[2];

	get_all_features_from_cnt(dev_idx, hw_feature_register,
		sw_feature_register);
}

/* wrapper function */
void get_sw_features_from_cnt(tfa98xx_handle_t dev_idx,
	int sw_feature_register[2])
{
	int hw_feature_register;

	get_all_features_from_cnt(dev_idx, &hw_feature_register,
		sw_feature_register);
}

/* Factory trimming for the Boost converter */
void tfa_factory_trimmer(tfa98xx_handle_t dev_idx)
{
	unsigned short current_value, delta;
	int result;

	/* Factory trimming for the Boost converter */
	/* check if there is a correction needed */
	result = TFA_GET_BF(dev_idx, DCMCCAPI);
	if (result) {
		/* Get currentvalue of DCMCC and the Delta value */
		current_value = (unsigned short)TFA_GET_BF(dev_idx, DCMCC);
		delta = (unsigned short)TFA_GET_BF(dev_idx, USERDEF);

		/* check the sign bit (+/-) */
		result = TFA_GET_BF(dev_idx, DCMCCSB);
		if (result == 0) {
			/* Do not exceed the maximum value of 15 */
			if (current_value + delta < 15) {
				TFA_SET_BF_VOLATILE
					(dev_idx, DCMCC, current_value + delta);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: %d\n",
						current_value + delta);
			} else {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, 15);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: 15\n");
			}
		} else if (result == 1) {
			/* Do not exceed the minimum value of 0 */
			if (current_value - delta > 0) {
				TFA_SET_BF_VOLATILE
					(dev_idx, DCMCC, current_value - delta);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: %d\n",
						current_value - delta);
			} else {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, 0);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: 0\n");
			}
		}
	}
}

enum tfa98xx_error tfa_set_filters(int dev_idx, int prof_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	unsigned int i;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* If we are in powerdown there is no need to set filters */
	if (TFA_GET_BF(dev_idx, PWDN) == 1)
		return TFA98XX_ERROR_OK;

	/* loop the profile to find filter settings */
	for (i = 0; i < prof->length; i++) {
		/* write values before default section */
		if (prof->list[i].type == dsc_default)
			break;

		/* write all filter settings */
		if (prof->list[i].type == dsc_filter) {
			if (tfa_cont_write_item(dev_idx,
				&prof->list[i]) != TFA98XX_ERROR_OK)
				return err;
		}
	}

	return err;
}

int tfa_tib_dsp_msgblob(int devidx, int length, const char *buffer)
{
	uint8_t *buf = (uint8_t *)buffer;
	static uint8_t *blob = 0, *blobptr;
#if defined(TFADSP_DSP_BUFFER_POOL)
	static int blob_p_index = -1;
#endif
	static int total;

	/* No data found*/
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (length == -1 && blob == 0)
#else
	if (devidx == -1 && blob == 0)
#endif
	{
		return TFA_ERROR;
	}

#if defined(TFADSP_DSP_BUFFER_POOL)
	if (length == -1)
#else
	if (devidx == -1)
#endif
	{
		blob[2] = (uint8_t)(total >> 8); /* msb */
		blob[3] = (uint8_t)total; /* lsb */
		total += 4;
		memcpy(buf, blob, total); /* + header: 'mm' | size */
#if defined(TFADSP_DSP_BUFFER_POOL)
		if (blob_p_index != -1) {
			tfa98xx_buffer_pool_access
				(devidx, blob_p_index, 0, POOL_RETURN);
			blob_p_index = -1;
		} else {
			kfree(blob);
		}
#else
		kfree(blob);
#endif /* TFADSP_DSP_BUFFER_POOL */
		blob = 0; /* Set back to 0 otherwise no new malloc is done! */
		return total;
	}

	if (length == -2) {
#if defined(TFADSP_DSP_BUFFER_POOL)
		if (blob_p_index != -1) {
			tfa98xx_buffer_pool_access
				(devidx, blob_p_index, 0, POOL_RETURN);
			blob_p_index = -1;
		} else {
			kfree(blob);
		}
#else
		kfree(blob);
#endif /* TFADSP_DSP_BUFFER_POOL */
		blob = 0; /* Set back to 0 otherwise no new malloc is done! */
		return 0;
	}

	if (blob == 0) {
		if (tfa98xx_cnt_verbose)
			pr_debug("%s, Creating the multi-message\n", __func__);

#if defined(TFADSP_DSP_BUFFER_POOL)
		blob_p_index = tfa98xx_buffer_pool_access
			(devidx, -1, 64*1024, POOL_GET);
		if (blob_p_index != -1) {
			pr_debug("%s: allocated from buffer_pool[%d]\n",
				__func__, blob_p_index);
			blob = (uint8_t *)(handles_local[devidx]
				.buf_pool[blob_p_index].pool);
		} else {
			blob = kmalloc(64*1024, GFP_KERNEL);
			/* max length is 64k */
			if (blob == NULL)
				goto msgblob_error_exit;
		}
#else
		blob = kmalloc(64*1024, GFP_KERNEL);
		/* max length is 64k */
		if (blob == NULL)
			goto msgblob_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */
		blobptr = blob;
		*blobptr++ = 'm'; /* 'mm' = multi message */
		*blobptr++ = 'm';
		blobptr += 2; /* size comes here */
		total = 0;
		if (tfa98xx_cnt_verbose)
			pr_debug("\n");
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("%s, id:0x%02x%02x%02x, length:%d\n",
			__func__, buf[0], buf[1], buf[2], length);

	*blobptr++ = (uint8_t)(length >> 8); /* msb */
	*blobptr++ = (uint8_t)length; /* lsb */
	memcpy(blobptr, buf, length);
	blobptr += length;
	total += length+2; /* +counters */

	/* SetRe25 message is always the last message of the multi-msg */
	pr_debug("%s: length (%d), [0]=0x%x-[1]=0x%x-[2]=0x%x\n",
		__func__, length, buf[0], buf[1], buf[2]);
	/* if (buf[1] == 0x81 && buf[2] == SB_PARAM_SET_RE25C) { */
	if ((buf[0] == SB_PARAM_SET_RE25C && buf[1] == 0x81 && buf[2] == 0x00)
		|| (buf[0] == FW_PAR_ID_GET_MEMORY && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GLOBAL_GET_INFO && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GET_FEATURE_INFO && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GET_MEMTRACK && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GET_TAG && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GET_API_VERSION && buf[1] == 0x80)
		|| (buf[0] == FW_PAR_ID_GET_STATUS_CHANGE && buf[1] == 0x80)
		|| (buf[0] == BFB_PAR_ID_GET_COEFS && buf[1] == 0x82)
		|| (buf[0] == BFB_PAR_ID_GET_CONFIG && buf[1] == 0x82)) {
		pr_debug("%s: found last message - sending: buf[0]=%d\n",
			__func__, buf[0]);
		return 1; /* 1 means last message is done! */
	}

	if ((buf[0] == SB_PARAM_GET_ALGO_PARAMS && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_LAGW && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_RE25C && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_LSMODEL && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_MBDRC && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_MBDRC_DYNAMICS && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_EXCURSION_FILTERS && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_TAG && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_STATE && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_XMODEL && buf[1] == 0x81)
		|| (buf[0] == SB_PARAM_GET_XMODEL_COEFFS && buf[1] == 0x81)) {
		pr_debug("%s: found last message - sending: buf[0]=%d CC=%d\n",
			__func__, buf[0], buf[2]);
		return 1; /* 1 means last message is done! with CC check */
	}

	/* SB_PARAM_SET_DATA_LOGGER to be handled at initializing */
	if (buf[0] == SB_PARAM_GET_DATA_LOGGER && buf[1] == 0x81) {
		pr_debug("%s: found blackbox message - sending: buf[0]=%d\n",
			 __func__, buf[0]);
		return 1; /* 1 means last message is done! */
	}

	return 0;

msgblob_error_exit:
	pr_debug("%s: can not allocate memory\n", __func__);
	return TFA98XX_ERROR_FAIL;
}
