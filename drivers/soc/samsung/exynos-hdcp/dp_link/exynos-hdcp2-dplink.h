/* driver/soc/samsung/exynos-hdcp/dplink/exynos-hdcp2-dplink.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_HDCP2_DPLINK_H__
#define __EXYNOS_HDCP2_DPLINK_H__

#if defined(CONFIG_HDCP2_EMULATION_MODE)
int dplink_emul_handler(int cmd);
#endif

enum {
	HDCP_AUTH_PROCESS_ON   = 0x0,
	HDCP_AUTH_PROCESS_STOP = 0x1,
	HDCP_AUTH_PROCESS_DONE = 0x2
};

enum {
	DRM_OFF = 0x0,
	DRM_ON = 0x1,
	DRM_SAME_STREAM_TYPE = 0x2	/* If the previous contents and stream_type id are the same flag */
};

/* Do hdcp2.2 authentication with DP Receiver
 * and enable encryption if authentication is succeed.
 * @return
 *  - 0: Success
 *  - ENOMEM: hdcp context open fail
 *  - EACCES: authentication fail
 */
int hdcp_dplink_authenticate(void);

int do_dplink_auth(struct hdcp_link_info *lk_handle);
int hdcp_dplink_get_rxstatus(uint8_t *status);
int hdcp_dplink_set_paring_available(void);
int hdcp_dplink_set_hprime_available(void);
int hdcp_dplink_set_rp_ready(void);
int hdcp_dplink_set_reauth(void);
int hdcp_dplink_set_integrity_fail(void);
int hdcp_dplink_cancel_auth(void);
int hdcp_dplink_stream_manage(void);
int hdcp_dplink_is_auth_state(void);
int hdcp_dplink_auth_check(void);
int hdcp_dplink_drm_flag_check(int flag);
int hdcp_dplink_dp_link_flag_check(int flag);
void hdcp_clear_session(uint32_t id);
#endif
