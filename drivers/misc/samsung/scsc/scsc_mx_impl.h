/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _CORE_H_
#define _CORE_H_

#include <linux/firmware.h>
#include "scsc_mif_abs.h"

struct device;
struct scsc_mx;
struct mifintrbit;
struct miframman;
struct mifmboxman;
struct mxman;
struct srvman;
struct mxmgmt_transport;
struct mxproc;
struct mxfwconfig;

struct scsc_mx          *scsc_mx_create(struct scsc_mif_abs *mif);
void scsc_mx_destroy(struct scsc_mx *mx);
struct scsc_mif_abs     *scsc_mx_get_mif_abs(struct scsc_mx *mx);
struct mifintrbit       *scsc_mx_get_intrbit(struct scsc_mx *mx);
struct mifmuxman        *scsc_mx_get_muxman(struct scsc_mx *mx);
struct miframman        *scsc_mx_get_ramman(struct scsc_mx *mx);
struct miframman        *scsc_mx_get_ramman2(struct scsc_mx *mx);
struct mifabox          *scsc_mx_get_aboxram(struct scsc_mx *mx);
struct mifmboxman       *scsc_mx_get_mboxman(struct scsc_mx *mx);
#ifdef CONFIG_SCSC_SMAPPER
struct mifsmapper       *scsc_mx_get_smapper(struct scsc_mx *mx);
#endif
#ifdef CONFIG_SCSC_QOS
struct mifqos           *scsc_mx_get_qos(struct scsc_mx *mx);
#endif
struct device           *scsc_mx_get_device(struct scsc_mx *mx);
struct mxman            *scsc_mx_get_mxman(struct scsc_mx *mx);
struct srvman           *scsc_mx_get_srvman(struct scsc_mx *mx);
struct mxproc           *scsc_mx_get_mxproc(struct scsc_mx *mx);
struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport(struct scsc_mx *mx);
struct gdb_transport    *scsc_mx_get_gdb_transport_r4(struct scsc_mx *mx);
struct gdb_transport    *scsc_mx_get_gdb_transport_m4(struct scsc_mx *mx);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
struct gdb_transport    *scsc_mx_get_gdb_transport_m4_1(struct scsc_mx *mx);
#endif
struct mxlog            *scsc_mx_get_mxlog(struct scsc_mx *mx);
struct mxlog_transport  *scsc_mx_get_mxlog_transport(struct scsc_mx *mx);
struct mxlogger         *scsc_mx_get_mxlogger(struct scsc_mx *mx);
struct panicmon         *scsc_mx_get_panicmon(struct scsc_mx *mx);
struct suspendmon	*scsc_mx_get_suspendmon(struct scsc_mx *mx);
struct mxfwconfig	*scsc_mx_get_mxfwconfig(struct scsc_mx *mx);

int mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size);
int mx140_request_file(struct scsc_mx *mx, char *path, const struct firmware **firmp);
int mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp);
int mx140_basedir_file(struct scsc_mx *mx);
int mx140_exe_path(struct scsc_mx *mx, char *path, size_t len, const char *bin);
int mx140_file_select_fw(struct scsc_mx *mx, u32 suffix);
bool mx140_file_supported_hw(struct scsc_mx *mx, u32 hw_ver);
#endif
