#include "tfa2_dev.h" /* for regwrite */
#include "tfa2_container.h"

static int tfa2_cnt_write_item(struct tfa2_device *tfa,
	struct tfa_desc_ptr * dsc);
/* write all descripters from the dsc list which type is in the items list */
static int tfa2_cnt_write_items_list(struct tfa2_device *tfa,
	struct tfa_desc_ptr *dsc_list, int length,
	enum tfa_descriptor_type *items_list);

/* items that require clock to run */
static 	enum tfa_descriptor_type clockdep_items_list[]
	= {dsc_file, dsc_cmd, dsc_cf_mem,
		dsc_profile, dsc_listend};

/*
 dsp mem direct write
 */
static int tfa2_cnt_write_dspmem(struct tfa2_device *tfa,
	struct tfa_dsp_mem *cfmem)
{
	int rc = 0, i;
	uint16_t address = cfmem->address;

	for (i = 0; i < cfmem->size; i++) {
		dev_dbg(&tfa->i2c->dev, "dsp mem (%d): 0x%02x=0x%04x\n",
			cfmem->type, address, cfmem->words[i]);
		rc = tfa2_i2c_write_cf_mem32(tfa->i2c,
			address++, &cfmem->words[i], 1, cfmem->type);
		if (rc < 0)
			return rc;
	}

	return rc;
}

/*
 * check the container file
 */
int tfa2_load_cnt(void *cnt, int length)
{
	struct tfa_container *cntbuf = (struct tfa_container *)cnt;

	if (length > TFA_MAX_CNT_LENGTH) {
		pr_err("incorrect length\n");
		return -EINVAL;
	}

	if (HDR(cntbuf->id[0], cntbuf->id[1]) == 0) {
		pr_err("header is 0\n");
		return -EINVAL;
	}

	if ((HDR(cntbuf->id[0],cntbuf->id[1])) != params_hdr) {
		pr_err("wrong header type: 0x%02x 0x%02x\n",
			cntbuf->id[0],cntbuf->id[1]);
		return -EINVAL;
	}

	if (cntbuf->size == 0) {
		pr_err("data size is 0\n");
		return -EINVAL;
	}

	/* check CRC */
	if (tfa2_cnt_crc_check_container(cntbuf)) {
		pr_err("CRC error\n");
		return -EINVAL;
	}

	/* check sub version level */
	if ((cntbuf->subversion[1] != NXPTFA_PM_SUBVERSION) &&
		(cntbuf->subversion[0] != '0')) {
		pr_err("container sub-version not supported: %c%c\n",
			cntbuf->subversion[0], cntbuf->subversion[1]);
		return -EINVAL;
	}

	return 0;
}

/*
 * check CRC for container
 * CRC is calculated over the bytes following the CRC field
 * return non zero value on error
 */
int tfa2_cnt_crc_check_container(struct tfa_container *cont)
{
	uint8_t *base;
	size_t size;
	uint32_t crc;

	/* ptr to bytes following the CRC field */
	base = (uint8_t *)&cont->crc + 4;
	/* nr of bytes following the CRC field */
	size = (size_t)(cont->size - (base - (uint8_t *)cont));
	crc = ~crc32_le(~0u, base, size);

	return crc != cont->crc;
}

/*
 * return 1 if the item is in the list
 * list must end with dsc_listend
 */
static int hitlist(enum tfa_descriptor_type *items,
	enum tfa_descriptor_type item)
{
	enum tfa_descriptor_type * list_item = items;

	while (*list_item != dsc_listend && *list_item < dsc_last) {
		if (*list_item++ == item)
			return 1;
	}
	return 0;
}

/***************/
/* cnt getinfo */

char *tfa2_cnt_get_string(struct tfa_container *cnt, struct tfa_desc_ptr *dsc)
{
	if (dsc->type != dsc_string)
		return NULL;

	return dsc->offset+(char*)cnt;
}

int tfa2_cnt_get_cmd(struct tfa_container *cnt, int dev,
	int profidx, int offset, uint8_t **array, int *length)
{
	int i;
	char *pcnt = (char *)cnt;
	struct tfa_profile_list *prof
		= tfa2_cnt_get_dev_prof_list(cnt, dev, profidx);
	struct tfa_desc_ptr *dsc;

	/* process the list until the end of the profile */
	for (i = offset; i < prof->length - 1; i++) {
		dsc = &prof->list[i];
		if (dsc->type == dsc_cmd) {
			*length = *(uint16_t *)(dsc->offset + pcnt);
			*array = (uint8_t *)(dsc->offset+2+ pcnt);
			return i;
		}
	}

	return -EINVAL; /* not found */
}

/*
 * Get the name of the device at a certain index in the container file
 * return device name
 */
char *tfa2_cnt_device_name(struct tfa_container *cnt, int dev_idx)
{
	struct tfa_device_list *dev;

	dev = tfa2_cnt_device(cnt, dev_idx);
	if (dev == NULL)
		return NULL;

	return tfa2_cnt_get_string(cnt, &dev->name);
}

/*
 * Get the name of the profile at certain index for a device in the container file
 * return profile name
 */
char *tfa2_cnt_profile_name(struct tfa_container *cnt,
	int dev_idx, int prof_idx)
{
	struct tfa_profile_list *prof = NULL;

	/* the Nth profiles for this device */
	prof = tfa2_cnt_get_dev_prof_list(cnt, dev_idx, prof_idx);

	/* If the index is out of bound */
	if (prof == NULL)
		return NULL;

	return tfa2_cnt_get_string(cnt, &prof->name);
}

/*
 * Get the application name from the container file application field
 * note that the input stringbuffer should be sizeof(application field)+1
 *
 */
int tfa2_cnt_get_app_name(struct tfa2_device *tfa, char *name)
{
	unsigned int i;
	int len = 0;

	for (i = 0; i < sizeof(tfa->cnt->application); i++) {
		if (isalnum(tfa->cnt->application[i])) /* copy char if valid */
			name[len++] = tfa->cnt->application[i];
		if (tfa->cnt->application[i]=='\0')
			break;
	}
	name[len++] = '\0';

	return len;
}


/*
 * Dump the contents of the file header
 */
void tfa2_cnt_show_header(struct tfa_header *hdr)
{
	char _id[2];

	pr_debug("File header\n");

	_id[1] = hdr->id >> 8;
	_id[0] = hdr->id & 0xff;
	pr_debug("\tid:%.2s version:%.2s subversion:%.2s\n", _id,
		hdr->version, hdr->subversion);
	pr_debug("\tsize:%d CRC:0x%08x \n", hdr->size, hdr->crc);
	pr_debug("\tcustomer:%.8s application:%.8s type:%.8s\n",
		hdr->customer, hdr->application, hdr->type);
		/* TODO fix leading zeroes */
}

/*********************/
/* cnt infra lookups */

/*
 * get the slave for the device if it exists
 */
int tfa2_cnt_get_slave(struct tfa_container *cnt, int dev_idx)
{
	struct tfa_device_list *dev;

	/* Make sure the cnt file is loaded */
	if (!cnt)
		return -EINVAL;

	dev = tfa2_cnt_device(cnt, dev_idx);
	if (!dev)
		return -EINVAL;

	return dev->dev;
}

/*
 * return the device list pointer
 */
struct tfa_device_list *tfa2_cnt_device(struct tfa_container *cnt, int dev_idx)
{
	return tfa2_cnt_get_dev_list(cnt, dev_idx);
}

/*
 * return device list dsc from index
 */
struct tfa_device_list *tfa2_cnt_get_dev_list(struct tfa_container *cont,
	int dev_idx)
{
	uint8_t *base = (uint8_t *) cont;
	struct tfa_device_list *this_dev;

	if (cont == NULL)
		return NULL;

	if ((dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dsc_device)
		return NULL;

	base += cont->index[dev_idx].offset;
	this_dev = (struct tfa_device_list *) base;

	if (this_dev->name.type != dsc_string) {
		pr_err("fatal corruption: device[%d] has no name\n", dev_idx);
		return NULL;
	}

	return this_dev;
}

/*
 * get the Nth profile for the Nth device
 */
struct tfa_profile_list *tfa2_cnt_get_dev_prof_list(struct tfa_container *cont,
	int dev_idx, int prof_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;

	dev = tfa2_cnt_get_dev_list(cont, dev_idx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile) {
				if (prof_idx != hit++)
					continue;
				return (struct tfa_profile_list *)
					(dev->list[idx].offset+base);
			}
		}
	}

	return NULL;
}

/*
 * get the number of profiles for the Nth device of this tfa
 */
/* TODO call tfa2_cnt_get_dev_nprof */
int tfa2_dev_get_dev_nprof(struct tfa2_device *tfa)
{
	struct tfa_device_list *dev;
	int idx, nprof = 0;

	if (tfa->cnt == NULL)
		return 0;

	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return 0;

	dev = tfa2_cnt_get_dev_list(tfa->cnt, tfa->dev_idx);
	if (dev) {
		for (idx = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile) {
				nprof++;
			}
		}
	}

	return nprof;
}

/*
 * get the number of profiles for the Nth device
 */
int tfa2_cnt_get_dev_nprof(struct tfa_container * cnt, int dev_idx)
{
	struct tfa_device_list *dev;
	int idx, nprof = 0;

	if (cnt == NULL)
		return 0;

	if ((dev_idx < 0) || (dev_idx >= cnt->ndev))
		return 0;

	dev = tfa2_cnt_get_dev_list(cnt, dev_idx);
	if (dev) {
		for (idx = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile) {
				nprof++;
			}
		}
	}

	return nprof;
}

/*
 * get the n-th profilename match for this device
 */
int tfa2_cnt_grep_nth_profile_name(struct tfa_container * cnt,
	int dev_idx, int n, const char *string)
{
	int prof, count=0;

	/* compare string to the profile name in the list of profiles */
	for (prof = 0; prof < cnt->nprof; prof++) {
		char *profile = tfa2_cnt_profile_name(cnt, dev_idx, prof);
		if (profile && strstr(profile, string)) {
			if (n == count++)
				/* tfa2_cnt_get_dev_prof_list(cnt, dev_idx, prof); */
				return prof;
		}
	}

	return -EINVAL;
}
/*
 * get the 1st profilename match for this device
 */
int tfa2_cnt_grep_profile_name(struct tfa_container * cnt,
	int dev_idx, const char *string)
{
	return tfa2_cnt_grep_nth_profile_name(cnt, dev_idx, 0, string);
}

/*
 * get the index of the 1st item in the list that depends on clock
 * get the index of the default section marker
 * if not found return -1
 */
int tfa2_cnt_get_clockdep_idx(struct tfa2_device *tfa,
	struct tfa_desc_ptr *dsc_list, int length,
	int *clockdep_idx, int *default_section_idx)
{
	int i;

	if (clockdep_idx == NULL || default_section_idx == NULL
		|| tfa == NULL || dsc_list == NULL)
		return -EINVAL;

	*clockdep_idx = -1;
	*default_section_idx=-1;

	/* find the matches in the list */
	for (i = 0; i < length; i++) {
		if (*clockdep_idx == -1
			&& hitlist(clockdep_items_list, dsc_list[i].type))
			*clockdep_idx = i;
		if (dsc_list[i].type == dsc_default) {
			*default_section_idx= i;
			break; /* clock dep items cannot be in the default section */
		}
	}

	return 0;
}

/*
 * return the index of the default section marker of this profile
 * if not found return -1
 */
/*
int tfa2_cnt_get_default_idx(struct tfa2_device *tfa,
	struct tfa_profile_list *prof)
{
	int i;

	// find a match in the list
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_default)
			return i;
	}

	return TFA_ERROR;
}
*/

/*
 * write the register based on the input address, value and mask
 * only the part that is masked will be updated
 */
static int tfa2_dev_cnt_write_register(struct tfa2_device *tfa,
	struct tfa_reg_patch *reg)
{
	int rc;
	uint16_t value,newvalue;

	if (tfa->verbose)
		dev_dbg(&tfa->i2c->dev, "register: 0x%02x=0x%04x (msk=0x%04x)\n",
			reg->address, reg->value, reg->mask);

	rc = tfa2_i2c_read_reg(tfa->i2c, reg->address); /* will report error */
	if (rc < 0)
		return rc;
	value = (uint16_t)rc;

	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;

	value |= newvalue;

	rc = tfa2_i2c_write_reg(tfa->i2c,reg->address, value);

	return rc;

}

/* 
 * write reg and bitfield items in the devicelist
 * until a clock dependent item is encountered
 * this is called during called start init
 */
int tfa2_cnt_write_regs_dev(struct tfa2_device *tfa)
{
	int i, rc = 0;
	struct tfa_device_list *dev = tfa2_cnt_device(tfa->cnt, tfa->dev_idx);
	/* write these items until break */
	enum tfa_descriptor_type items_list[]
		= {dsc_bit_field, dsc_register, dsc_listend};

	if (!dev) {
		return -EINVAL;
	}

	/* process the list until a clock dependent is encountered */
	for (i = 0; i < dev->length; i++) {
		if (hitlist(clockdep_items_list, dev->list[i].type))
				break;
		if (hitlist(items_list, dev->list[i].type)) {
			rc = tfa2_cnt_write_item(tfa, &dev->list[i]);
			if (rc < 0)
				break;
		}

		if (rc < 0)
			break;
	}

	return rc;
}

/* write reg and bitfield items in the profilelist the target */
int tfa2_cnt_write_regs_profile(struct tfa2_device *tfa, int prof_idx)
{
	int i, rc = 0;
	char *profile = NULL;
	struct tfa_profile_list *prof
		= tfa2_cnt_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	enum tfa_descriptor_type items_list[]
		= {dsc_bit_field, dsc_register, dsc_listend};

	if (!prof) {
		return -EINVAL;
	}

	profile = tfa2_cnt_get_string(tfa->cnt, &prof->name);
	if (profile == NULL) {
		dev_dbg(&tfa->i2c->dev,
			"Invalid profile index given: %d\n", prof_idx);
		return -EINVAL;
	}
	else {
		dev_dbg(&tfa->i2c->dev,
			"----- profile: %s (%d) -----\n", profile, prof_idx);
	}

	/* process the list until the end of the profile or the default section */
	for (i = 0; i < prof->length - 1; i++) {
		/* 
		 * we only want to write the values
		 * before the default section when we switch profile
		 */
		if (prof->list[i].type == dsc_default)
			break;
		if (hitlist(items_list, prof->list[i].type)) {
			rc = tfa2_cnt_write_item(tfa, &prof->list[i]);
			if (rc < 0)
				break;
		}

	}
	return rc;
}

static int tfa2_cnt_write_patch(struct tfa2_device *tfa,
	struct tfa_patch_file *patchfile)
{
	int rc , size;

	if (tfa->verbose)
		tfa2_cnt_show_header(&patchfile->hdr);

	/* size is total length */
	size = patchfile->hdr.size - sizeof(struct tfa_patch_file);
	/* TODO fix for single patch header type */
	rc = tfa2_check_patch((const uint8_t *)patchfile,
		patchfile->hdr.size, tfa->rev);
	if (rc < 0)
		return rc;

	rc = tfa2_dev_dsp_patch(tfa, size,
		(const unsigned char *)patchfile->data);

	return rc;
}

/*
 * write the item from the dsc
 * ignore if not a content item
 */
static int tfa2_cnt_write_item(struct tfa2_device *tfa,
	struct tfa_desc_ptr * dsc)
{
	int rc = 0;
	struct tfa_reg_patch *reg;
	struct tfa_bitfield *bitf;
	struct tfa_file_dsc *file;
	void *cnt_offset;

	/* payload offset in cnt */
	cnt_offset = dsc->offset + (uint8_t *) tfa->cnt;

	switch (dsc->type) {
	case dsc_bit_field:
		bitf = (struct tfa_bitfield *)(cnt_offset);
		rc = tfa2_i2c_write_bf_volatile(tfa->i2c, bitf->field, bitf->value);
		break;
	case dsc_file:
		rc = tfa2_cnt_write_file(tfa, (struct tfa_file_dsc *)(cnt_offset));
		break;
	case dsc_cmd:
		rc = tfa2_cnt_write_msg_dsc(tfa, dsc);
		break;
	case dsc_cf_mem:
		rc = tfa2_cnt_write_dspmem(tfa, (struct tfa_dsp_mem *)(cnt_offset));
		break;
	case dsc_default: /* default listpoint marker */
	case dsc_device: /* device list */
	case dsc_profile: /* profile list */
	case dsc_group:
	case dsc_marker:
	case dsc_tfa_hal:
	case dsc_livedata_string:
	case dsc_livedata:
		break; /* ignore */
	case dsc_string: /* ascii: zero terminated string */
		dev_dbg(&tfa->i2c->dev, ";string: %s\n",
				tfa2_cnt_get_string(tfa->cnt, dsc));
		break;
	case dsc_register: /* register patch */
		reg = (struct tfa_reg_patch *) (cnt_offset);
		rc = tfa2_dev_cnt_write_register(tfa, reg);
		/* dev_dbg(&tfa->i2c->dev, "$0x%2x=0x%02x,0x%02x\n",
		 *	reg->address, reg->mask, reg->value);
		 */
		break;
/*	case dsc_file: // filename + file contents */
	case dsc_patch:
		/* get the payload from the file dsc */
		file = (struct tfa_file_dsc *) (cnt_offset);
		/* data is the patch header */
		rc = tfa2_cnt_write_patch(tfa, (struct tfa_patch_file *) &file->data);
		break;
	default:
		dev_err(&tfa->i2c->dev, "unsupported list item:%d\n", dsc->type);
		rc = -EINVAL;
	}

	return rc;
}

/*
 * all container originated RPC msgs goes through this
 *
 * the cmd_id is monitored here to insert
		{"SetAlgoParams", 0x48100},
		{"SetAlgoParamsWithoutReset", 0x48102},
		{"SetMBDrc", 0x48107},
		{"SetMBDrcWithoutReset", 0x48108},
 */
int tfa2_cnt_write_msg(struct tfa2_device *tfa , int wlength, char *wbuf) {
	int rc = 0;
	uint8_t *cmd_id24 = (uint8_t *)wbuf;
	int cmd_id = cmd_id24[0] <<16 | cmd_id24[1] << 8 | cmd_id24[2];
	uint8_t *cmd_lsb = &cmd_id24[2];
	int coldstarting = tfa2_dev_is_fw_cold(tfa); /* set if FW is cold */

	/*
	 * select the cmd_id variant based on the init state
	 */
	switch (cmd_id & 0x7fff) {
	default:
		break;
	case 0x100:	/* SetAlgoParams 0x48100*/
	case 0x102:	/* SetAlgoParamsWithoutReset 0x48102*/
		/* if cold cmd_id = coldstart variant */
		*cmd_lsb = coldstarting ? 0 : 2;
		break;
	case 0x107:	/* SetMBDrc 0x48107*/
	case 0x108:	/* SetMBDrcWithoutReset 0x48108*/
		/* if cold cmd_id = coldstart variant */
		*cmd_lsb = coldstarting ? 7 : 8;
		break;
	}

	if (tfa->verbose)
		dev_dbg(&tfa->i2c->dev, "Writing cmd=0x%06x,size=%d\n",
			cmd_id24[0] <<16 | cmd_id24[1] << 8 | cmd_id24[2], wlength);

	rc = tfa2_dsp_execute(tfa, wbuf, wlength, NULL, 0);

	return rc;
}

/* write all patchfiles in the devicelist to the target */
int tfa2_cnt_write_patches(struct tfa2_device *tfa)
{
	int rc = 0;
	struct tfa_device_list *dev
		= tfa2_cnt_get_dev_list(tfa->cnt, tfa->dev_idx);
	int i;

	if (!dev)
		return -EINVAL;

	/* process the list until a patch is encountered and load it */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch)
			rc = tfa2_cnt_write_item(tfa, &dev->list[i]);
	}

	return rc;
}

int tfa2_cnt_write_msg_dsc(struct tfa2_device *tfa, struct tfa_desc_ptr *dsc)
{
	int rc = 0;
	char *cnt = (char *)tfa->cnt;
	int size;

	size = *(uint16_t *)(dsc->offset + cnt);
	rc = tfa2_cnt_write_msg(tfa, size, dsc->offset+2+ cnt);

	return rc;
}

/*
 * write all descripters from the dsc list which type is in the items list
 */
static int tfa2_cnt_write_items_list(struct tfa2_device *tfa,
	struct tfa_desc_ptr *dsc_list, int length,
	enum tfa_descriptor_type *items_list)
{
	int i, rc = 0;

	/* process the list and write all files */
	for (i = 0; i < length; i++) {
		if (hitlist(items_list, dsc_list[i].type)) {
			rc = tfa2_cnt_write_item(tfa, &dsc_list[i]);
			if (rc < 0)
				break;
		}
	}

	return rc;
}

/* write all param files and cfmem from the devicelist to the target */
int tfa2_cnt_write_files(struct tfa2_device *tfa)
{
	int rc = 0;
	struct tfa_device_list *dev;
	enum tfa_descriptor_type items_list[]
		= {dsc_file, dsc_cmd, dsc_cf_mem, dsc_listend};

	dev = tfa2_cnt_device(tfa->cnt, tfa->dev_idx);
	if (!dev)
		return -EINVAL;

	/* process the list and write all files */
	rc = tfa2_cnt_write_items_list(tfa,
		dev->list, dev->length, items_list);

	return rc;
}

/* write all param files in the profile list to the target */
int tfa2_cnt_write_files_profile(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx)
{
	int rc = 0;
	struct tfa_profile_list *prof
		= tfa2_cnt_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	enum tfa_descriptor_type items_list[]
		= {dsc_file, dsc_cmd, dsc_cf_mem, dsc_patch, dsc_listend};

	if (!prof)
		return -EINVAL;

	/* process the list and write all files */
	rc = tfa2_cnt_write_items_list(tfa, prof->list,
		prof->length-1, items_list);

	return rc;
}

/*
 * write a parameter file to the device
 * only generic rpc msg and speaker/lsmodel files are supported
 */
int tfa2_cnt_write_file(struct tfa2_device *tfa, struct tfa_file_dsc *file)
{
	int rc = 0;
	struct tfa_header *hdr = (struct tfa_header *)file->data;
	enum tfa_header_type type;
	int size;

	if (tfa->verbose)
		tfa2_cnt_show_header(hdr);

	type = (enum tfa_header_type)hdr->id;

	switch (type) {
	case msg_hdr: /* generic DSP message */
		size = hdr->size - sizeof(struct tfa_msg_file);
		rc = tfa2_cnt_write_msg(tfa, size,
			(char *)((struct tfa_msg_file *)hdr)->data);
		break;
	case speaker_hdr:
		/* Remove header and xml_id */
		size = hdr->size - sizeof(struct tfa_spk_header)
			- sizeof(struct tfa_fw_ver);
		rc = tfa2_cnt_write_msg(tfa, size,
			(char *)(((struct tfa_speaker_file *)hdr)->data
			+ (sizeof(struct tfa_fw_ver))));
		break;
	case info_hdr:
		/* Ignore */
		break;
	case patch_hdr:
		rc = tfa2_cnt_write_patch(tfa,
			(struct tfa_patch_file *) &file->data);
		break;

	default:
		dev_err(&tfa->i2c->dev,
			"Header is of unknown type: %c%c (0x%x)\n",
			type & 0xff, (type>>8) & 0xff, type);
		return -EINVAL;
	}

	return rc;
}

/*
 * process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 */
int tfa2_cnt_write_profile(struct tfa2_device *tfa, int prof_idx, int vstep_idx) {
	int i, rc = 0;
	int previous_prof_idx = tfa2_dev_get_swprofile(tfa);
	struct tfa_profile_list *prof = tfa2_cnt_get_dev_prof_list(tfa->cnt,
		tfa->dev_idx, prof_idx);
	struct tfa_profile_list *previous_prof
		= tfa2_cnt_get_dev_prof_list(tfa->cnt,
		tfa->dev_idx, previous_prof_idx);
	int in_group;
	int prof_clockdep_idx, prof_default_section_idx;
	int previous_clockdep_idx, previous_default_section_idx;

	if (!prof || !previous_prof) {
		dev_err(&tfa->i2c->dev,
			"Error trying to get the (previous) swprofile \n");
		return -EINVAL;
	}

	/* grouping enabled ? */
	in_group = prof->group == previous_prof->group && prof->group != 0;

	/* get indexes of relevant points in the profiles:
	 * 1st clock dependent item and default settings marker
	 */
	rc = tfa2_cnt_get_clockdep_idx(tfa, previous_prof->list,
		previous_prof->length, &previous_clockdep_idx,
		&previous_default_section_idx);
	if (rc < 0)
		return rc;
	/* new profile */
	rc = tfa2_cnt_get_clockdep_idx(tfa, prof->list, prof->length,
		&prof_clockdep_idx, &prof_default_section_idx);
	if (rc < 0)
		return rc;

	dev_dbg(&tfa->i2c->dev,
		"profile switch device:%s, %s > %s (%s pwdn)\n",
		tfa2_cnt_device_name(tfa->cnt, tfa->dev_idx),
		tfa2_cnt_profile_name(tfa->cnt, tfa->dev_idx, previous_prof_idx),
		tfa2_cnt_profile_name(tfa->cnt, tfa->dev_idx, prof_idx),
		in_group ? "no" : "with");

	/* We only power cycle when the profiles are not in the same group */
	if (!in_group) {
		/* When we switch profile we first power down PLL */
		/* with mute */
		rc = tfa2_dev_set_state(tfa,
			TFA_STATE_POWERDOWN | TFA_STATE_MUTE);
		if (rc < 0)
			return rc;
	}

	if (tfa->verbose)
		tfa2_show_current_state(tfa);

	/*
	 * restore defaults/ non-clock dependent settings
	 * from the active (previous) profile
	 */
	for (i = previous_default_section_idx;
		i < previous_prof->length - 1; i++) {
		rc = tfa2_cnt_write_item(tfa, &previous_prof->list[i]);
		if (rc < 0)
			return rc;
	}
	/* apply all non-clock dependent settings from new profile */
	for (i = 0; i < prof_clockdep_idx; i++) {
		rc = tfa2_cnt_write_item(tfa, &prof->list[i]);
		if (rc < 0)
			return rc;
	}

	/* We only power cycle when the profiles are not in the same group */
	if (!in_group) {
		/* Leave powerdown state */
		/* clock will be there on return */
		rc = tfa2_dev_set_state(tfa, TFA_STATE_CLOCK);
		if (rc < 0)
			return rc;
	}

	/* write everything until end or the default section */
	for (i = prof_clockdep_idx; i < prof_default_section_idx; i++) {
		rc = tfa2_cnt_write_item(tfa, &prof->list[i]);
		if (rc < 0)
			return rc;
	}

	return rc;
}

/*
 * direct write of all items in the profile
 */
int tfa2_cnt_write_transient_profile(struct tfa2_device *tfa, int prof_idx)
{
	int i, rc = 0;
	struct tfa_profile_list *prof
		= tfa2_cnt_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	int prof_clockdep_idx, prof_default_section_idx;

	if (!prof) {
		dev_err(&tfa->i2c->dev,
			"Error trying to get the (previous) swprofile \n");
		return -EINVAL;
	}
	/* get indexes of relevant points in the profiles:
	 * 1st clock dependent item and default settings marker
	 */
	rc = tfa2_cnt_get_clockdep_idx(tfa, prof->list, prof->length,
		&prof_clockdep_idx, &prof_default_section_idx);
	if (rc < 0)
		return rc;

	/* write everything until end or the default section */
	for (i = 0; i < prof_default_section_idx; i++) {
		rc = tfa2_cnt_write_item(tfa, &prof->list[i]);
		if (rc < 0)
			break;
	}

	return rc;
}

/*
 * lookup slave and return device index
 */
int tfa2_cnt_get_idx(struct tfa2_device *tfa)
{
	struct tfa_device_list *dev = NULL;
	int i;

	for (i = 0; i < tfa->cnt->ndev; i++) {
		dev = tfa2_cnt_device(tfa->cnt, i);
		if (dev->dev == tfa->slave_address)
			break;
	}

	if (i == tfa->cnt->ndev)
		return TFA_ERROR;

	return i;
}
