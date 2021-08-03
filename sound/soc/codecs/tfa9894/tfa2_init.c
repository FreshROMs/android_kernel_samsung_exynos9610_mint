#include "tfa2_dev.h"

static int no_init(struct tfa2_device *tfa)
{
	return 0;
}

/*
 * TODO can factory_trimmer for tfa9892 done after optimal settings?
 */
static int tfa9894_init(struct tfa2_device *tfa)
{
	int rc = 0;

	if (tfa->in_use == 0)
		return -ENOENT;

	/* Unlock keys to write settings */
	tfa2_i2c_hap_key2(tfa->i2c, 0);

	/* The optimal settings */
	if (tfa->rev == 0x1a94) {
		/* V14 */
		/* ----- generated code start ----- */
		tfa2_i2c_write_reg(tfa->i2c, 0x00, 0xa245); /* POR=0x8245 */
		tfa2_i2c_write_reg(tfa->i2c, 0x01, 0x15da); /* POR=0x11ca */
		tfa2_i2c_write_reg(tfa->i2c, 0x02, 0x5288); /* POR=0x55c8 */
		tfa2_i2c_write_reg(tfa->i2c, 0x52, 0xbe17); /* POR=0xb617 */
		tfa2_i2c_write_reg(tfa->i2c, 0x53, 0x0dbe); /* POR=0x0d9e */
		tfa2_i2c_write_reg(tfa->i2c, 0x56, 0x05c3); /* POR=0x07c3 */
		tfa2_i2c_write_reg(tfa->i2c, 0x57, 0x0344); /* POR=0x0366 */
		tfa2_i2c_write_reg(tfa->i2c, 0x61, 0x0032); /* POR=0x0073 */
		tfa2_i2c_write_reg(tfa->i2c, 0x71, 0x00cf); /* POR=0x018d */
		tfa2_i2c_write_reg(tfa->i2c, 0x72, 0x34a9); /* POR=0x44e8 */
		tfa2_i2c_write_reg(tfa->i2c, 0x73, 0x38c8); /* POR=0x3806 */
		tfa2_i2c_write_reg(tfa->i2c, 0x76, 0x0067); /* POR=0x0065 */
		tfa2_i2c_write_reg(tfa->i2c, 0x80, 0x0000); /* POR=0x0003 */
		tfa2_i2c_write_reg(tfa->i2c, 0x81, 0x5799); /* POR=0x561a */
		tfa2_i2c_write_reg(tfa->i2c, 0x82, 0x0104); /* POR=0x0044 */
		/* ----- generated code end ----- */
	} else if (tfa->rev == 0x2a94) {
		tfa->bf_manstate = 0x1333;
		/* V10 */
		/* ----- generated code start ----- */
		tfa2_i2c_write_reg(tfa->i2c, 0x00, 0xa245); /* POR=0x8245 */
		tfa2_i2c_write_reg(tfa->i2c, 0x01, 0x15da); /* POR=0x11ca */
		tfa2_i2c_write_reg(tfa->i2c, 0x02, 0x51e8); /* POR=0x55c8 */
		tfa2_i2c_write_reg(tfa->i2c, 0x52, 0xbe17); /* POR=0xb617 */
		tfa2_i2c_write_reg(tfa->i2c, 0x53, 0x0dbe); /* POR=0x0d9e k2 */
		tfa2_i2c_write_reg(tfa->i2c, 0x57, 0x0344); /* POR=0x0366 */
		tfa2_i2c_write_reg(tfa->i2c, 0x61, 0x0033); /* POR=0x0073 */
		tfa2_i2c_write_reg(tfa->i2c, 0x71, 0x6ecf); /* POR=0x6f8d */
		tfa2_i2c_write_reg(tfa->i2c, 0x72, 0x34a9); /* POR=0x44e8 */
		tfa2_i2c_write_reg(tfa->i2c, 0x73, 0x38c8); /* POR=0x3806 */
		tfa2_i2c_write_reg(tfa->i2c, 0x76, 0x0067); /* POR=0x0065 k2 */
		tfa2_i2c_write_reg(tfa->i2c, 0x80, 0x0000); /* POR=0x0003 k2 */
		tfa2_i2c_write_reg(tfa->i2c, 0x81, 0x5799); /* POR=0x561a k2 */
		tfa2_i2c_write_reg(tfa->i2c, 0x82, 0x0104); /* POR=0x0044 k2 */
		/* ----- generated code end   ----- */
	}

	/* re-lock */
	tfa2_i2c_hap_key2(tfa->i2c, 1);

	return rc;
}

/* TODO use the getfeatures() for retrieving the features [artf103523]
 * tfa->support_drc = SUPPORT_NOT_SET;
 */
int tfa2_dev_specific(struct tfa2_device *tfa)
{
	switch (tfa->rev & 0xff) {
	case 0x94:
		/* tfa9894 */
		tfa->tfa_init = tfa9894_init;
		break;
	case 0x18:
		/* tfa9918 */
		tfa->tfa_init = no_init;
		break;
	default:
		dev_err(&tfa->i2c->dev, "%s unknown device type : 0x%02x\n",
			__func__, tfa->rev);
		return -EINVAL;
		break;
	}

	return 0;
}
