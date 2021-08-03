/****************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/sysfs.h>
#include <linux/poll.h>
#include <linux/cdev.h>

#include "debug.h"
#include "procfs.h"
#include "utils.h"

#ifndef CONFIG_SCSC_DEBUG_COMPATIBILITY
const int   SLSI_INIT_DEINIT;
const int   SLSI_NETDEV             =  1;
const int   SLSI_CFG80211           =  2;
const int   SLSI_MLME               =  3;
const int   SLSI_SUMMARY_FRAMES     =  4;
const int   SLSI_HYDRA              =  5;
const int   SLSI_TX                 =  6;
const int   SLSI_RX                 =  7;
const int   SLSI_UDI                =  8;

const int   SLSI_WIFI_FCQ           =  9;

const int   SLSI_HIP                = 10;
const int   SLSI_HIP_INIT_DEINIT    = 11;
const int   SLSI_HIP_FW_DL          = 12;
const int   SLSI_HIP_SDIO_OP        = 13;
const int   SLSI_HIP_PS             = 14;
const int   SLSI_HIP_TH             = 15;
const int   SLSI_HIP_FH             = 16;
const int   SLSI_HIP_SIG            = 17;

const int   SLSI_FUNC_TRACE         = 18;
const int   SLSI_TEST               = 19;   /* Unit test logging */
const int   SLSI_SRC_SINK           = 20;
const int   SLSI_FW_TEST            = 21;
const int   SLSI_RX_BA              = 22;

const int   SLSI_TDLS               = 23;
const int   SLSI_GSCAN              = 24;
const int   SLSI_MBULK              = 25;
const int   SLSI_FLOWC              = 26;
const int   SLSI_SMAPPER            = 27;
#endif

static int slsi_dbg_set_param_cb(const char *val, const struct kernel_param *kp);
static int slsi_dbg_get_param_cb(char *buffer, const struct kernel_param *kp);

static struct kernel_param_ops param_ops_log = {
	.set = slsi_dbg_set_param_cb,
	.get = slsi_dbg_get_param_cb,
};

#define ADD_DEBUG_MODULE_PARAM(name, default_level, filter) \
	static int slsi_dbg_lvl_ ## name = default_level; \
	module_param_cb(slsi_dbg_lvl_ ## name, &param_ops_log, (void *)&filter, S_IRUGO | S_IWUSR); \
	MODULE_PARM_DESC(slsi_dbg_lvl_ ## name, " Debug levels (0~4) for the " # name " module (0 = off) default=" # default_level)

#ifndef CONFIG_SCSC_DEBUG_COMPATIBILITY
/*                     Name,             Default, Filter */
ADD_DEBUG_MODULE_PARAM(init_deinit,         3, SLSI_INIT_DEINIT);
ADD_DEBUG_MODULE_PARAM(netdev,              2, SLSI_NETDEV);
ADD_DEBUG_MODULE_PARAM(cfg80211,            1, SLSI_CFG80211);
ADD_DEBUG_MODULE_PARAM(mlme,                2, SLSI_MLME);
ADD_DEBUG_MODULE_PARAM(summary_frames,      0, SLSI_SUMMARY_FRAMES);
ADD_DEBUG_MODULE_PARAM(hydra,               0, SLSI_HYDRA);
ADD_DEBUG_MODULE_PARAM(tx,                  0, SLSI_TX);
ADD_DEBUG_MODULE_PARAM(rx,                  0, SLSI_RX);
ADD_DEBUG_MODULE_PARAM(udi,                 2, SLSI_UDI);

ADD_DEBUG_MODULE_PARAM(wifi_fcq,            0, SLSI_WIFI_FCQ);

ADD_DEBUG_MODULE_PARAM(hip,                 0, SLSI_HIP);
ADD_DEBUG_MODULE_PARAM(hip_init_deinit,     0, SLSI_HIP_INIT_DEINIT);
ADD_DEBUG_MODULE_PARAM(hip_fw_dl,           0, SLSI_HIP_FW_DL);
ADD_DEBUG_MODULE_PARAM(hip_sdio_op,         0, SLSI_HIP_SDIO_OP);
ADD_DEBUG_MODULE_PARAM(hip_ps,              0, SLSI_HIP_PS);
ADD_DEBUG_MODULE_PARAM(hip_th,              0, SLSI_HIP_TH);
ADD_DEBUG_MODULE_PARAM(hip_fh,              0, SLSI_HIP_FH);
ADD_DEBUG_MODULE_PARAM(hip_sig,             0, SLSI_HIP_SIG);

ADD_DEBUG_MODULE_PARAM(func_trace,          0, SLSI_FUNC_TRACE);
ADD_DEBUG_MODULE_PARAM(test,                0, SLSI_TEST);
ADD_DEBUG_MODULE_PARAM(src_sink,            0, SLSI_SRC_SINK);
ADD_DEBUG_MODULE_PARAM(fw_test,             0, SLSI_FW_TEST);
ADD_DEBUG_MODULE_PARAM(rx_ba,               0, SLSI_RX_BA);

ADD_DEBUG_MODULE_PARAM(tdls,                2, SLSI_TDLS);
ADD_DEBUG_MODULE_PARAM(gscan,               3, SLSI_GSCAN);
ADD_DEBUG_MODULE_PARAM(mbulk,               0, SLSI_MBULK);
ADD_DEBUG_MODULE_PARAM(flowc,               0, SLSI_FLOWC);
ADD_DEBUG_MODULE_PARAM(smapper,             0, SLSI_SMAPPER);

static int       slsi_dbg_lvl_all; /* Override all debug modules */

int       *slsi_dbg_filters[] = {
	&slsi_dbg_lvl_init_deinit,
	&slsi_dbg_lvl_netdev,
	&slsi_dbg_lvl_cfg80211,
	&slsi_dbg_lvl_mlme,
	&slsi_dbg_lvl_summary_frames,
	&slsi_dbg_lvl_hydra,
	&slsi_dbg_lvl_tx,
	&slsi_dbg_lvl_rx,
	&slsi_dbg_lvl_udi,

	&slsi_dbg_lvl_wifi_fcq,

	&slsi_dbg_lvl_hip,
	&slsi_dbg_lvl_hip_init_deinit,
	&slsi_dbg_lvl_hip_fw_dl,
	&slsi_dbg_lvl_hip_sdio_op,
	&slsi_dbg_lvl_hip_ps,
	&slsi_dbg_lvl_hip_th,
	&slsi_dbg_lvl_hip_fh,
	&slsi_dbg_lvl_hip_sig,

	&slsi_dbg_lvl_func_trace,
	&slsi_dbg_lvl_test,
	&slsi_dbg_lvl_src_sink,
	&slsi_dbg_lvl_fw_test,
	&slsi_dbg_lvl_rx_ba,

	&slsi_dbg_lvl_tdls,
	&slsi_dbg_lvl_gscan,
	&slsi_dbg_lvl_mbulk,
	&slsi_dbg_lvl_flowc,
	&slsi_dbg_lvl_smapper,
};
#else
static int slsi_dbg_lvl_compat_all;
module_param(slsi_dbg_lvl_compat_all, int, S_IRUGO | S_IWUSR);

int       *slsi_dbg_filters[] = {
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
	&slsi_dbg_lvl_compat_all,
};

static int       slsi_dbg_lvl_all; /* Override all debug modules */
#endif

const int SLSI_DF_MAX = (sizeof(slsi_dbg_filters) / sizeof(slsi_dbg_filters[0]));

const int SLSI_OVERRIDE_ALL_FILTER = -1; /* This is not a log module but merely a filter option */

/* Convert a string containing a decimal value to an integer */
static int slsi_decstr_to_int(const char *dec_str, int *res)
{
	int        tmp_res = 0;
	int        sign = 0;
	const char *tmp_char = dec_str;

	sign = (*tmp_char == '-') ? -1 : ((*tmp_char == '+') ? 1 : 0);
	if (sign != 0)
		tmp_char++;

	while (*tmp_char) {
		if (*tmp_char == '\n')
			break;
		if ((*tmp_char < '0') || (*tmp_char > '9'))
			return -1;
		tmp_res = tmp_res * 10 + (*tmp_char - '0');
		tmp_char++;
	}

	*res = (sign < 0) ? (-tmp_res) : tmp_res;
	return 0;
}

static int slsi_dbg_set_param_cb(const char *val, const struct kernel_param *kp)
{
	int new_val;
	int filter;

	if (slsi_decstr_to_int(val, &new_val) < 0) {
		pr_info("%s: failed to convert %s to int\n", __func__, val);
		return -1;
	}
	filter = *((int *)(kp->arg));

	if (filter < -1 || filter >= SLSI_DF_MAX) {
		pr_info("%s: filter %d out of range\n", __func__, filter);
		return -1;
	}

	if (filter == SLSI_OVERRIDE_ALL_FILTER) {
		if (new_val == -1) {
			pr_info("Override does not take effect because slsi_dbg_lvl_all=%d\n", new_val);
		} else {
			int i;

			pr_info("Setting all debug modules to level %d\n", new_val);
			for (i = 0; i < SLSI_DF_MAX; i++)
				*slsi_dbg_filters[i] = new_val;

			slsi_dbg_lvl_all = new_val;
		}
	} else {
		pr_info("Setting debug module %d to level %d\n", filter, new_val);
		*slsi_dbg_filters[filter] = new_val;
	}

	return 0;
}

static int slsi_dbg_get_param_cb(char *buffer, const struct kernel_param *kp)
{
#define KERN_PARAM_OPS_MAX_BUF_SIZE (4 * 1024)
	int filter;
	int val = 0;

	filter = *((int *)(kp->arg));

	if (filter == SLSI_OVERRIDE_ALL_FILTER)
		val = slsi_dbg_lvl_all;
	else if (filter < 0 || filter >= SLSI_DF_MAX)
		pr_info("%s: filter %d out of range\n", __func__, filter);
	else
		val = *slsi_dbg_filters[filter];

	return snprintf(buffer, KERN_PARAM_OPS_MAX_BUF_SIZE, "%i", val);
}

module_param_cb(slsi_dbg_lvl_all, &param_ops_log, (void *)&SLSI_OVERRIDE_ALL_FILTER, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(slsi_dbg_lvl_all, "Override debug level (0~4) for all the log modules (-1 = do not override) default=0");

