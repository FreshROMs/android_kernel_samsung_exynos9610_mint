/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>

#include "dynamic_aid.h"

#ifdef DYNAMIC_AID_DEBUG
#define aid_dbg(format, arg...)	printk(format, ##arg)
#else
#define aid_dbg(format, arg...)
#endif

struct rgb64_t {
	s64 rgb[3];
};

struct dynamic_aid_info {
	struct dynamic_aid_param_t param;
	int			*iv_tbl;
	int			iv_max;
	int			iv_top;
	int			*ibr_tbl;
	int			ibr_max;
	int			 *mtp;
	int			vreg;
	int			vref_h;

	struct rgb64_t *point_voltages;
	struct rgb64_t *output_voltages;
	struct rgb64_t *m_voltage;
};

#define MUL_100(x)		(x*100)

static int calc_point_voltages(struct dynamic_aid_info d_aid)
{
	int ret, iv, c, iv_ref;
	struct rgb64_t *vt, *vp;
	int *vd, *mtp;
	int numerator, denominator;
	s64 vreg, vb;
	s64 t1, t2;
	struct rgb64_t *point_voltages;

	point_voltages = d_aid.point_voltages;
	vreg = d_aid.vreg;
	ret = 0;

	/* iv == 0; */
	{
		vd = (int *)&d_aid.param.gamma_default[0];
		mtp = &d_aid.mtp[0];

		numerator = d_aid.param.formular[0].numerator;
		denominator = d_aid.param.formular[0].denominator;

		for (c = 0; c < CI_MAX; c++) {
			t1 = vreg;
			t2 = vreg*d_aid.param.vt_voltage_value[vd[c] + mtp[c]];
			point_voltages[0].rgb[c] = t1 - div_s64(t2, denominator);
		}
	}

	iv = d_aid.iv_max - 1;

	/* iv == (IV_MAX-1) ~ 1; */
	for (; iv > 0; iv--) {
		vt = &point_voltages[0];
		vp = &point_voltages[iv+1];
		vd = (int *)&d_aid.param.gamma_default[iv*CI_MAX];
		mtp = &d_aid.mtp[iv*CI_MAX];
		iv_ref = d_aid.param.iv_ref[iv];

		numerator = d_aid.param.formular[iv].numerator;
		denominator = d_aid.param.formular[iv].denominator;

		for (c = 0; c < CI_MAX; c++) {
			vb = (iv_ref < d_aid.iv_max) ? point_voltages[iv_ref].rgb[c] : vreg;
			if (iv == d_aid.iv_max - 1) {
				t1 = vb;
				t2 = vb - d_aid.vref_h;
			} else {
				t1 = vb;
				t2 = vb - vp->rgb[c];
			}
			t2 *= vd[c] + mtp[c] + numerator;
			point_voltages[iv].rgb[c] = (t1 - div_s64(t2, denominator));
		}
	}

#ifdef DYNAMIC_AID_DEBUG
	for (iv = 0; iv < d_aid.iv_max; iv++) {
		aid_dbg("Point Voltage[%d] = ", iv);
		for (c = 0; c < CI_MAX; c++)
			aid_dbg("%lld ", point_voltages[iv].rgb[c]);
		aid_dbg("\n");
	}
#endif

	return ret;
}

static int calc_output_voltages(struct dynamic_aid_info d_aid)
{
	int iv, i, c;
	int v_idx, v_cnt;
	struct rgb_t v, v_diff;
	struct rgb64_t *output_voltages;
	struct rgb64_t *point_voltages;

	output_voltages = d_aid.output_voltages;
	point_voltages = d_aid.point_voltages;
	iv = d_aid.iv_max - 1;

	for (c = 0; c < CI_MAX; c++)
		output_voltages[0].rgb[c] = d_aid.vreg;

	/* iv == (IV_MAX-1) ~ 0; */
	for (; iv > 0; iv--) {
		v_cnt = d_aid.iv_tbl[iv] - d_aid.iv_tbl[iv-1];
		v_idx = d_aid.iv_tbl[iv];

		for (c = 0; c < CI_MAX; c++) {
			v.rgb[c] = point_voltages[iv].rgb[c];
			v_diff.rgb[c] = point_voltages[iv-1].rgb[c] - point_voltages[iv].rgb[c];
		}

		for (i = 0; i < v_cnt; i++, v_idx--) {
			for (c = 0; c < CI_MAX; c++)
				output_voltages[v_idx].rgb[c] = v.rgb[c] + v_diff.rgb[c]*i/v_cnt;
		}
	}

#ifdef DYNAMIC_AID_DEBUG
	for (iv = 0; iv <= d_aid.iv_top; iv++) {
		aid_dbg("Output Voltage[%d] = ", iv);
		for (c = 0; c < CI_MAX; c++)
			aid_dbg("%lld ", output_voltages[iv].rgb[c]);
		aid_dbg("\n");
	}
#endif

	return 0;
}

static int calc_voltage_table(struct dynamic_aid_info d_aid)
{
	int ret;

	ret = calc_point_voltages(d_aid);
	if (ret)
		return ret;

	ret = calc_output_voltages(d_aid);
	if (ret)
		return ret;

	return 0;
}

static int calc_m_rgb_voltages(struct dynamic_aid_info d_aid, int ibr)
{
	int iv, c;
	struct rgb64_t *output_voltages;
	struct rgb64_t *point_voltages;
	int *m_gray;
	struct rgb64_t *m_voltage;

	output_voltages = d_aid.output_voltages;
	point_voltages = d_aid.point_voltages;
	m_gray = &((int(*)[d_aid.iv_max])d_aid.param.m_gray)[ibr][0];
	m_voltage = d_aid.m_voltage;
	iv = d_aid.iv_max - 1;

	/* iv == (IV_MAX - 1) ~ 1; */
	for (; iv > 0; iv--) {
		for (c = 0; c < CI_MAX; c++)
			m_voltage[iv].rgb[c] = output_voltages[m_gray[iv]].rgb[c];
	}

	/* iv == 0; */
	for (c = 0; c < CI_MAX; c++)
		m_voltage[iv].rgb[c] = point_voltages[0].rgb[c];

#ifdef DYNAMIC_AID_DEBUG
	aid_dbg("M-RGB voltage (%d) =\n", d_aid.ibr_tbl[ibr]);
	for (iv = 0; iv < d_aid.iv_max; iv++) {
		aid_dbg("[%d] = ", iv);
		for (c = 0; c < CI_MAX; c++)
			aid_dbg("%lld ", d_aid.m_voltage[iv].rgb[c]);
		aid_dbg("\n");
	}
#endif

	return 0;
}

static int calc_gamma(struct dynamic_aid_info d_aid, int ibr, int *result)
{
	int ret, iv, c, iv_ref;
	int numerator, denominator;
	s64 t1, t2, vb;
	int *vd, *mtp, *res;
	struct rgb_t (*offset_color)[d_aid.iv_max];
	struct rgb64_t *m_voltage;

	offset_color = (struct rgb_t(*)[])d_aid.param.offset_color;
	m_voltage = d_aid.m_voltage;
	iv = d_aid.iv_max - 1;
	ret = 0;

	/* iv == (IV_MAX - 1) ~ 1; */
	for (; iv > 0; iv--) {
		mtp = &d_aid.mtp[iv*CI_MAX];
		res = &result[iv*CI_MAX];
		numerator = d_aid.param.formular[iv].numerator;
		denominator = d_aid.param.formular[iv].denominator;
		iv_ref = d_aid.param.iv_ref[iv];
		for (c = 0; c < CI_MAX; c++) {
			vb = (iv_ref < d_aid.iv_max) ? m_voltage[iv_ref].rgb[c] : d_aid.vreg;
			if (iv == d_aid.iv_max - 1) {
				t1 = vb - m_voltage[iv].rgb[c];
				t2 = vb - d_aid.vref_h;
			} else {
				t1 = vb - m_voltage[iv].rgb[c];
				t2 = vb - m_voltage[iv+1].rgb[c];
			}
			res[c] = div64_s64((t1 + 1) * denominator, t2) - numerator;
			res[c] -= mtp[c];

			if (offset_color)
				res[c] += offset_color[ibr][iv].rgb[c];

			res[c] = (res[c] < 0) ? 0 : res[c];

			if ((res[c] > 255) && (iv != d_aid.iv_max - 1))
				res[c] = 255;
		}
	}

	/* iv == 0; */
	vd = (int *)&d_aid.param.gamma_default[0];
	res = &result[0];
	for (c = 0; c < CI_MAX; c++)
		res[c] = vd[c];

#ifdef DYNAMIC_AID_DEBUG
	aid_dbg("Gamma (%d) =\n", d_aid.ibr_tbl[ibr]);
	for (iv = 0; iv < d_aid.iv_max; iv++) {
		aid_dbg("[%d] = ", iv);
		for (c = 0; c < CI_MAX; c++)
			aid_dbg("%X ", result[iv*CI_MAX+c]);
		aid_dbg("\n");
	}
#endif

	return ret;
}

static int calc_gamma_table(struct dynamic_aid_info d_aid, int **gamma)
{
	int ibr;
#ifdef DYNAMIC_AID_DEBUG
	int iv, c;
#endif

	/* ibr == 0 ~ (IBRIGHTNESS_MAX - 1); */
	for (ibr = 0; ibr < d_aid.ibr_max; ibr++) {
		calc_m_rgb_voltages(d_aid, ibr);
		calc_gamma(d_aid, ibr, gamma[ibr]);
	}

#ifdef DYNAMIC_AID_DEBUG
	for (ibr = 0; ibr < d_aid.ibr_max; ibr++) {
		aid_dbg("Gamma [%3d] = ", d_aid.ibr_tbl[ibr]);
		for (iv = 0; iv < d_aid.iv_max; iv++) {
			for (c = 0; c < CI_MAX; c++)
				aid_dbg("%4d ", gamma[ibr][iv*CI_MAX+c]);
		}
		aid_dbg("\n");
	}
#endif

	return 0;
}

int dynamic_aid(struct dynamic_aid_param_t param, int **gamma)
{
	struct dynamic_aid_info d_aid;
	int ret;

	d_aid.param = param;
	d_aid.iv_tbl = (int *)param.iv_tbl;
	d_aid.iv_max = param.iv_max;
	d_aid.iv_top = param.iv_tbl[param.iv_max-1];
	d_aid.mtp = param.mtp;
	d_aid.vreg = MUL_100(param.vreg);
	d_aid.vref_h = param.vref_h;

	d_aid.ibr_tbl = (int *)param.ibr_tbl;
	d_aid.ibr_max = param.ibr_max;

	d_aid.point_voltages = kcalloc(d_aid.iv_max, sizeof(struct rgb64_t), GFP_KERNEL);
	if (!d_aid.point_voltages) {
		pr_err("failed to allocate point_voltages\n");
		ret = -ENOMEM;
		goto error1;
	}
	d_aid.output_voltages = kcalloc(d_aid.iv_top + 1, sizeof(struct rgb64_t), GFP_KERNEL);
	if (!d_aid.output_voltages) {
		pr_err("failed to allocate output_voltages\n");
		ret = -ENOMEM;
		goto error2;
	}
	d_aid.m_voltage = kcalloc(d_aid.iv_max, sizeof(struct rgb64_t), GFP_KERNEL);
	if (!d_aid.m_voltage) {
		pr_err("failed to allocate m_voltage\n");
		ret = -ENOMEM;
		goto error3;
	}

	ret = calc_voltage_table(d_aid);
	if (ret)
		goto error4;

	ret = calc_gamma_table(d_aid, gamma);
	if (ret)
		goto error4;

	pr_info("Dynamic Aid Finished !\n");

error4:
	kfree(d_aid.m_voltage);
error3:
	kfree(d_aid.output_voltages);
error2:
	kfree(d_aid.point_voltages);
error1:
	return ret;

}
