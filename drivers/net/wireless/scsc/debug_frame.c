/****************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/ieee80211.h>
#include <linux/ratelimit.h>

#include "debug.h"
#include "fapi.h"
#include "const.h"
#include "mgt.h"

#ifdef CONFIG_SCSC_WLAN_DEBUG

/* frame decoding debug level */
static int slsi_debug_summary_frame = 3;
module_param(slsi_debug_summary_frame, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(slsi_debug_summary_frame, "Debug level (0: disable, 1: mgmt only (no scan), 2: mgmt and imp frames, 3: all");

struct slsi_decode_entry {
	const char *name;
	void       (*decode_fn)(u8 *frame, u16 frame_length, char *result, size_t result_length);
};

struct slsi_decode_snap {
	const u8   snap[8];
	const char *name;
	size_t     (*decode_fn)(u8 *frame, u16 frame_length, char *result, size_t result_length);
};

struct slsi_value_name_decode {
	const u16  value;
	const char *name;
	size_t     (*decode_fn)(u8 *frame, u16 frame_length, char *result, size_t result_length);
};

static size_t slsi_decode_basic_ie_info(u8 *ies, u16 ies_length, char *result, size_t result_length)
{
	size_t   size_written = 0;
	const u8 *ssid = cfg80211_find_ie(WLAN_EID_SSID, ies, ies_length);
	const u8 *rsn = cfg80211_find_ie(WLAN_EID_RSN, ies, ies_length);
	const u8 *wpa = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPA, ies, ies_length);
	const u8 *wps = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPS, ies, ies_length);
	const u8 *htop = cfg80211_find_ie(WLAN_EID_HT_OPERATION, ies, ies_length);
	const u8 *country = cfg80211_find_ie(WLAN_EID_COUNTRY, ies, ies_length);

	if (htop && htop[1]) {
		size_written += snprintf(result + size_written, result_length - size_written, " channel:%d", htop[2]);
	} else {
		const u8 *ds = cfg80211_find_ie(WLAN_EID_DS_PARAMS, ies, ies_length);

		if (ds)
			size_written += snprintf(result + size_written, result_length - size_written, " channel:%d", ds[2]);
	}

	if (ssid) {
		if (ssid[1])
			size_written += snprintf(result + size_written, result_length - size_written, " %.*s", ssid[1], (char *)&ssid[2]);
		else
			size_written += snprintf(result + size_written, result_length - size_written, " <HIDDEN>");
	}

	if (country)
		size_written += snprintf(result + size_written, result_length - size_written, " country:%c%c%c", country[2], country[3], country[4]);
	if (wpa)
		size_written += snprintf(result + size_written, result_length - size_written, " wpa");
	if (rsn)
		size_written += snprintf(result + size_written, result_length - size_written, " wpa2");
	if (wps)
		size_written += snprintf(result + size_written, result_length - size_written, " wps");
	return size_written;
}

static void slsi_decode_frame_ies_only(u8 offset, u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t str_len;

	result[0] = '(';
	str_len = slsi_decode_basic_ie_info(frame + offset,
					    frame_length - offset,
					    result + 1,
					    result_length - 1);
	result[1 + str_len] = ')';
	result[2 + str_len] = '\0';
}

static size_t slsi_decode_frame_leu16(u8 *frame, u16 frame_length, char *result, size_t result_length, const char *name)
{
	u16 value = frame[0] | frame[1] << 8;

	SLSI_UNUSED_PARAMETER(frame_length);

	return snprintf(result, result_length, "%s:%u", name, value);
}

#define SLSI_ASSOC_REQ_IE_OFFSET 4
static void slsi_decode_assoc_req(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	slsi_decode_frame_ies_only(SLSI_ASSOC_REQ_IE_OFFSET, frame, frame_length, result, result_length);
}

#define SLSI_ASSOC_RSP_STATUS_OFFSET 2
#define SLSI_ASSOC_RSP_IE_OFFSET 6
static void slsi_decode_assoc_rsp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t str_len = 0;

	result[str_len++] = '(';
	str_len += slsi_decode_frame_leu16(frame + SLSI_ASSOC_RSP_STATUS_OFFSET,
					   frame_length - SLSI_ASSOC_RSP_STATUS_OFFSET,
					   result + str_len,
					   result_length - str_len,
					   "status");
	str_len += slsi_decode_basic_ie_info(frame + SLSI_ASSOC_RSP_IE_OFFSET,
					     frame_length - SLSI_ASSOC_RSP_IE_OFFSET,
					     result + str_len,
					     result_length - str_len);
	result[str_len++] = ')';
	result[str_len] = '\0';
}

#define SLSI_DEAUTH_REASON_OFFSET 0
static void slsi_decode_deauth(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t str_len = 0;

	result[str_len++] = '(';
	str_len += slsi_decode_frame_leu16(frame + SLSI_DEAUTH_REASON_OFFSET,
					   frame_length - SLSI_DEAUTH_REASON_OFFSET,
					   result + str_len,
					   result_length - str_len,
					   "reason_code");
	result[str_len++] = ')';
	result[str_len] = '\0';
}

#define SLSI_AUTH_ALGO_OFFSET 0
#define SLSI_AUTH_SEQ_OFFSET 2
#define SLSI_AUTH_STATUS_OFFSET 4
static void slsi_decode_auth(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t str_len = 0;

	result[str_len++] = '(';
	str_len += slsi_decode_frame_leu16(frame + SLSI_AUTH_ALGO_OFFSET,
					   frame_length - SLSI_AUTH_ALGO_OFFSET,
					   result + str_len,
					   result_length - str_len,
					   "algo");
	result[str_len++] = ' ';
	str_len += slsi_decode_frame_leu16(frame + SLSI_AUTH_SEQ_OFFSET,
					   frame_length - SLSI_AUTH_SEQ_OFFSET,
					   result + str_len,
					   result_length - str_len,
					   "seq");
	result[str_len++] = ' ';
	str_len += slsi_decode_frame_leu16(frame + SLSI_AUTH_STATUS_OFFSET,
					   frame_length - SLSI_AUTH_STATUS_OFFSET,
					   result + str_len,
					   result_length - str_len,
					   "status");
	result[str_len++] = ' ';
	result[str_len++] = ')';
	result[str_len] = '\0';
}

#define SLSI_REASSOC_IE_OFFSET 10
static void slsi_decode_reassoc_req(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	slsi_decode_frame_ies_only(SLSI_REASSOC_IE_OFFSET, frame, frame_length, result, result_length);
}

#define SLSI_BEACON_IE_OFFSET 12
static void slsi_decode_beacon(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	slsi_decode_frame_ies_only(SLSI_BEACON_IE_OFFSET, frame, frame_length, result, result_length);
}

#define SLSI_PROBEREQ_IE_OFFSET 0
static void slsi_decode_probe_req(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	slsi_decode_frame_ies_only(SLSI_PROBEREQ_IE_OFFSET, frame, frame_length, result, result_length);
}

#define SLSI_ACTION_BLOCK_ACK_ADDBA_REQ 0
#define SLSI_ACTION_BLOCK_ACK_ADDBA_RSP 1
#define SLSI_ACTION_BLOCK_ACK_DELBA     2
static size_t slsi_decode_action_blockack(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u8 action = frame[1];

	SLSI_UNUSED_PARAMETER(frame_length);

	switch (action) {
	case SLSI_ACTION_BLOCK_ACK_ADDBA_REQ:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->ADDBAReq(token:%u)", token);
	}
	case SLSI_ACTION_BLOCK_ACK_ADDBA_RSP:
	{
		u8  token = frame[2];
		u16 status = frame[3] | frame[4] << 8;

		return snprintf(result, result_length, "->ADDBARsp(token:%u, status:%u)", token, status);
	}
	case SLSI_ACTION_BLOCK_ACK_DELBA:
	{
		u16 reason_code = frame[4] | frame[5] << 8;

		return snprintf(result, result_length, "->DELBA(reason_code:%u)", reason_code);
	}
	default:
		return snprintf(result, result_length, "->Action(%u)", action);
	}
}

#define SLSI_ACTION_PUBLIC_DISCOVERY_RSP 14

static size_t slsi_decode_action_public(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u8 action = frame[1];

	SLSI_UNUSED_PARAMETER(frame_length);

	switch (action) {
	case SLSI_ACTION_PUBLIC_DISCOVERY_RSP:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->DiscoveryRsp(token:%u)", token);
	}
	default:
		return snprintf(result, result_length, "->Action(%u)", action);
	}
}

#define SLSI_ACTION_TDLS_SETUP_REQ           0
#define SLSI_ACTION_TDLS_SETUP_RSP           1
#define SLSI_ACTION_TDLS_SETUP_CFM           2
#define SLSI_ACTION_TDLS_TEARDOWN            3
#define SLSI_ACTION_TDLS_PEER_TRAFFIC_IND    4
#define SLSI_ACTION_TDLS_CHANNEL_SWITCH_REQ  5
#define SLSI_ACTION_TDLS_CHANNEL_SWITCH_RSP  6
#define SLSI_ACTION_TDLS_PEER_PSM_REQ        7
#define SLSI_ACTION_TDLS_PEER_PSM_RSP        8
#define SLSI_ACTION_TDLS_PEER_TRAFFIC_RSP    9
#define SLSI_ACTION_TDLS_DISCOVERY_REQ      10

static size_t slsi_decode_action_tdls(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u8 action = frame[1];

	SLSI_UNUSED_PARAMETER(frame_length);

	switch (action) {
	case SLSI_ACTION_TDLS_SETUP_REQ:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->SetupReq(token:%u)", token);
	}
	case SLSI_ACTION_TDLS_SETUP_RSP:
	{
		u16 status = frame[2] | frame[3] << 8;
		u8  token = frame[4];

		return snprintf(result, result_length, "->SetupRsp(token:%u, status:%u)", token, status);
	}
	case SLSI_ACTION_TDLS_SETUP_CFM:
	{
		u16 status = frame[2] | frame[3] << 8;
		u8  token = frame[4];

		return snprintf(result, result_length, "->SetupCfm(token:%u, status:%u)", token, status);
	}
	case SLSI_ACTION_TDLS_TEARDOWN:
	{
		u16 reason = frame[2] | frame[3] << 8;

		return snprintf(result, result_length, "->SetupCfm(reason:%u)", reason);
	}
	case SLSI_ACTION_TDLS_PEER_TRAFFIC_IND:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->PeerTrafficInd(token:%u)", token);
	}
	case SLSI_ACTION_TDLS_CHANNEL_SWITCH_REQ:
	{
		u8 channel = frame[2];

		return snprintf(result, result_length, "->ChannelSwitchReq(channel:%u)", channel);
	}
	case SLSI_ACTION_TDLS_CHANNEL_SWITCH_RSP:
	{
		u16 status = frame[2] | frame[3] << 8;

		return snprintf(result, result_length, "->ChannelSwitchRsp(status:%u)", status);
	}
	case SLSI_ACTION_TDLS_PEER_PSM_REQ:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->PeerPSMReq(token:%u)", token);
	}
	case SLSI_ACTION_TDLS_PEER_PSM_RSP:
	{
		u8  token = frame[2];
		u16 status = frame[3] | frame[4] << 8;

		return snprintf(result, result_length, "->PeerPSMRsp(token:%u, status:%u)", token, status);
	}
	case SLSI_ACTION_TDLS_PEER_TRAFFIC_RSP:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->PeerTrafficRsp(token:%u)", token);
	}
	case SLSI_ACTION_TDLS_DISCOVERY_REQ:
	{
		u8 token = frame[2];

		return snprintf(result, result_length, "->DiscoveryReq(token:%u)", token);
	}
	default:
		return snprintf(result, result_length, "->Action(%u)", action);
	}
}

static const struct slsi_value_name_decode action_categories[] = {
	{ 3,   "BlockAck",                      slsi_decode_action_blockack },
	{ 0,   "SpectrumManagement",            NULL },
	{ 1,   "QoS",                           NULL },
	{ 2,   "DLS",                           NULL },
	{ 4,   "Public",                        slsi_decode_action_public },
	{ 5,   "RadioMeasurement",              NULL },
	{ 6,   "FastBSSTransition",             NULL },
	{ 7,   "HT",                            NULL },
	{ 8,   "SAQuery",                       NULL },
	{ 9,   "ProtectedDualOfPublicAction",   NULL },
	{ 12,  "TDLS",                          slsi_decode_action_tdls },
	{ 17,  "ReservedWFA",                   NULL },
	{ 126, "VendorSpecificProtected",       NULL },
	{ 127, "VendorSpecific",                NULL },
	{ 132, "Public(error)",                 slsi_decode_action_public },
};

#define SLSI_ACTION_CAT_BLOCK_ACK 3
static void slsi_decode_action(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u8  category = frame[0];
	u32 i;

	for (i = 0; i < ARRAY_SIZE(action_categories); i++)
		if (action_categories[i].value == category) {
			int size_written = snprintf(result, result_length, "->%s", action_categories[i].name);

			if (action_categories[i].decode_fn)
				action_categories[i].decode_fn(frame, frame_length, result + size_written, result_length - size_written);
			return;
		}
	snprintf(result, result_length, "->category:%u", category);
}

const char *slsi_arp_opcodes[] = {
	"Reserved",
	"REQUEST",
	"REPLY",
	"request Reverse",
	"reply Reverse",
	"DRARP-Request",
	"DRARP-Reply",
	"DRARP-Error",
	"InARP-Request",
	"InARP-Reply",
	"ARP-NAK",
	"MARS-Request",
	"MARS-Multi",
	"MARS-MServ",
	"MARS-Join",
	"MARS-Leave",
	"MARS-NAK",
	"MARS-Unserv",
	"MARS-SJoin",
	"MARS-SLeave",
	"MARS-Grouplist-Request",
	"MARS-Grouplist-Reply",
	"MARS-Redirect-Map",
	"MAPOS-UNARP",
	"OP_EXP1",
	"OP_EXP2",
};

static size_t slsi_decode_arp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	/* u16 htype  = frame[0] << 8 | frame[1];
	 * u16 proto  = frame[2] << 8 | frame[3];
	 * u8  hlen   = frame[4];
	 * u8  plen   = frame[5];
	 */
	u16 opcode = frame[6] << 8 | frame[7];
	u8  *sha    = &frame[8];
	u8  *spa    = &frame[14];
	u8  *tha    = &frame[18];
	u8  *tpa    = &frame[24];

	SLSI_UNUSED_PARAMETER(frame_length);

	if (opcode < ARRAY_SIZE(slsi_arp_opcodes))
		return snprintf(result, result_length, "->%s(sha:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X, spa:%u.%u.%u.%u, tha:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X, tpa:%u.%u.%u.%u)",
				slsi_arp_opcodes[opcode],
				sha[0], sha[1], sha[2], sha[3], sha[4], sha[5],
				spa[0], spa[1], spa[2], spa[3],
				tha[0], tha[1], tha[2], tha[3], tha[4], tha[5],
				tpa[0], tpa[1], tpa[2], tpa[3]);
	else
		return snprintf(result, result_length, "->(opcode:%u)", opcode);
}

static const struct slsi_value_name_decode slsi_decode_eapol_packet_types[] = {
	{ 1,   "Identity",                                      NULL },
	{ 2,   "Notification",                                  NULL },
	{ 3,   "Nak",                                           NULL },
	{ 4,   "MD5Challenge",                                  NULL },
	{ 5,   "OneTimePassword",                               NULL },
	{ 6,   "GenericTokenCard",                              NULL },
	{ 9,   "RSA Public Key Authentication",                 NULL },
	{ 10,  "DSS Unilateral",                                NULL },
	{ 11,  "KEA",                                           NULL },
	{ 12,  "KEA-VALIDATE",                                  NULL },
	{ 13,  "EAP-TLS",                                       NULL },
	{ 14,  "Defender Token (AXENT)",                        NULL },
	{ 15,  "RSA Security SecurID EAP",                      NULL },
	{ 16,  "Arcot Systems EAP",                             NULL },
	{ 17,  "EAP-Cisco Wireless",                            NULL },
	{ 18,  "EAP-SIM",                                       NULL },
	{ 19,  "SRP-SHA1 Part 1",                               NULL },
	{ 21,  "EAP-TTLS",                                      NULL },
	{ 22,  "Remote Access Service",                         NULL },
	{ 23,  "EAP-AKA",                                       NULL },
	{ 24,  "EAP-3Com Wireless",                             NULL },
	{ 25,  "PEAP",                                          NULL },
	{ 26,  "MS-EAP-Authentication",                         NULL },
	{ 27,  "MAKE",                                          NULL },
	{ 28,  "CRYPTOCard",                                    NULL },
	{ 29,  "EAP-MSCHAP-V2",                                 NULL },
	{ 30,  "DynamID",                                       NULL },
	{ 31,  "Rob EAP",                                       NULL },
	{ 32,  "EAP-POTP",                                      NULL },
	{ 33,  "MS-Authentication-TLV",                         NULL },
	{ 34,  "SentriNET",                                     NULL },
	{ 35,  "EAP-Actiontec Wireless",                        NULL },
	{ 36,  "Cogent Systems Biometrics Authentication EAP",  NULL },
	{ 37,  "AirFortress EAP",                               NULL },
	{ 38,  "EAP-HTTP Digest",                               NULL },
	{ 39,  "SecureSuite EAP",                               NULL },
	{ 40,  "DeviceConnect EAP",                             NULL },
	{ 41,  "EAP-SPEKE",                                     NULL },
	{ 42,  "EAP-MOBAC",                                     NULL },
	{ 43,  "EAP-FAST",                                      NULL },
	{ 44,  "ZLXEAP",                                        NULL },
	{ 45,  "EAP-Link",                                      NULL },
	{ 46,  "EAP-PAX",                                       NULL },
	{ 47,  "EAP-PSK",                                       NULL },
	{ 48,  "EAP-SAKE",                                      NULL },
	{ 49,  "EAP-IKEv2",                                     NULL },
	{ 50,  "EAP-AKA",                                       NULL },
	{ 51,  "EAP-GPSK",                                      NULL },
	{ 52,  "EAP-pwd",                                       NULL },
	{ 53,  "EAP-EKE V1",                                    NULL },
	{ 254, "WPS",                                           NULL }
};

static size_t slsi_decode_eapol_packet(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	static const char *const slsi_decode_eapol_packet_codes[] = {
		"",
		"Request",
		"Response",
		"Success",
		"Failure",
	};

	size_t                   size_written = 0;
	u32                      i;
	u8                       code    = frame[0];
	u8                       id      = frame[1];
	u16                      length = frame[2] << 8 | frame[3];
	const char               *code_str = "";

	SLSI_UNUSED_PARAMETER(frame_length);

	if (code >= 1 && code <= 4)
		code_str = slsi_decode_eapol_packet_codes[code];

	if (length > 4 && (code == 1 || code == 2)) {
		u8 type = frame[4];

		for (i = 0; i < ARRAY_SIZE(slsi_decode_eapol_packet_types); i++)
			if (slsi_decode_eapol_packet_types[i].value == type) {
				size_written += snprintf(result, result_length, ":%s:%s id:%u", slsi_decode_eapol_packet_types[i].name, code_str, id);
				return size_written;
			}
		size_written += snprintf(result, result_length, ":type:%u: %s id:%u", type, code_str, id);
	} else {
		size_written += snprintf(result, result_length, ":%s id:%u length:%u", code_str, id, length);
	}
	return size_written;
}

static const struct slsi_value_name_decode slsi_eapol_packet_type[] = {
	{ 0, "EapPacket",   slsi_decode_eapol_packet },
	{ 1, "EapolStart",  NULL },
	{ 2, "EapolLogoff", NULL },
	{ 3, "EapolKey",    NULL }
};

static size_t slsi_decode_eapol(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t size_written = 0;
	u8     packet_type = frame[1];
	u16    length = frame[2] << 8 | frame[3];

	SLSI_UNUSED_PARAMETER(frame_length);

	if (packet_type < ARRAY_SIZE(slsi_eapol_packet_type)) {
		size_written += snprintf(result, result_length, "->%s", slsi_eapol_packet_type[packet_type].name);
		if (slsi_eapol_packet_type[packet_type].decode_fn)
			size_written += slsi_eapol_packet_type[packet_type].decode_fn(frame + 4, length, result + size_written, result_length - size_written);
		return size_written;
	} else {
		return snprintf(result, result_length, "->packet_type:%u", packet_type);
	}
}

static size_t slsi_decode_tdls(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u8 payload_type  = frame[0];

	if (payload_type == 2) {
		slsi_decode_action(frame + 1, frame_length - 1, result, result_length);
		return 0;
	} else {
		return snprintf(result, result_length, "->Unknown(payload:%u", payload_type);
	}
}

static size_t slsi_decode_ipv4_icmp_echo(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u16 id  = frame[0] << 8 | frame[1];
	u16 seq = frame[2] << 8 | frame[3];

	SLSI_UNUSED_PARAMETER(frame_length);

	return snprintf(result, result_length, " id:%u seq:%u", id, seq);
}

static const struct slsi_value_name_decode slsi_ipv4_icmp_types[] = {
	{ 0,   "EchoReply",                     slsi_decode_ipv4_icmp_echo },
	{ 8,   "Echo     ",                     slsi_decode_ipv4_icmp_echo },
	{ 3,   "Destination Unreachable Ack",   NULL },
	{ 4,   "Source Quench",                 NULL },
	{ 5,   "Redirect",                      NULL },
	{ 6,   "Alternate Host Address",        NULL },
	{ 9,   "Router Advertisement",          NULL },
	{ 10,  "Router Selection",              NULL },
	{ 11,  "Time Exceeded",                 NULL },
	{ 12,  "Parameter Problem",             NULL },
	{ 13,  "Timestamp",                     NULL },
	{ 14,  "Timestamp Reply",               NULL },
	{ 15,  "Information Request",           NULL },
	{ 16,  "Information Reply",             NULL },
	{ 17,  "Address Mask Request",          NULL },
	{ 18,  "Address Mask Reply",            NULL },
	{ 19,  "Reserved (for Security)",       NULL },
	{ 30,  "Traceroute",                    NULL },
	{ 31,  "Datagram Conversion Error",     NULL },
	{ 32,  "Mobile Host Redirect",          NULL },
	{ 33,  "IPv6 Where-Are-You",            NULL },
	{ 34,  "IPv6 I-Am-Here",                NULL },
	{ 35,  "Mobile Registration Request",   NULL },
	{ 36,  "Mobile Registration Reply",     NULL },
	{ 39,  "SKIP",                          NULL },
	{ 40,  "Photuris",                      NULL },
	{ 253, "RFC3692-style Experiment 1",    NULL },
	{ 254, "RFC3692-style Experiment 2",    NULL }
};

static size_t slsi_decode_ipv4_icmp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t size_written = 0;
	u32    i;
	u8     type = frame[0];
	u8     code = frame[1];

	for (i = 0; i < ARRAY_SIZE(slsi_ipv4_icmp_types); i++)
		if (slsi_ipv4_icmp_types[i].value == type) {
			size_written += snprintf(result, result_length, "->%s(code:%u)", slsi_ipv4_icmp_types[i].name, code);
			if (slsi_ipv4_icmp_types[i].decode_fn)
				size_written += slsi_ipv4_icmp_types[i].decode_fn(frame + 4, frame_length - 4, result + size_written, result_length - size_written);
			return size_written;
		}
	return snprintf(result, result_length, "->type(%u)", type);
}

static const struct slsi_value_name_decode slsi_ipv4_udp_bootp_dhcp_option53[] = {
	{ 1, "DHCP_DISCOVER", NULL },
	{ 2, "DHCP_OFFER",    NULL },
	{ 3, "DHCP_REQUEST",  NULL },
	{ 4, "DHCP_DECLINE",  NULL },
	{ 5, "DHCP_ACK",      NULL },
	{ 6, "DHCP_NAK",      NULL },
	{ 7, "DHCP_RELEASE",  NULL },
	{ 8, "DHCP_INFORM",   NULL },
};

#define SLSI_IPV4_UDP_BOOTP_CIADDR_OFFSET 16
#define SLSI_IPV4_UDP_BOOTP_YIADDR_OFFSET 20
#define SLSI_IPV4_UDP_BOOTP_GIADDR_OFFSET 24
#define SLSI_IPV4_UDP_BOOTP_MAGIC_OFFSET 236
static size_t slsi_decode_ipv4_udp_bootp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u32 i;
	u8  *ciaddr = &frame[SLSI_IPV4_UDP_BOOTP_CIADDR_OFFSET];
	u8  *yiaddr = &frame[SLSI_IPV4_UDP_BOOTP_YIADDR_OFFSET];
	u8  *giaddr = &frame[SLSI_IPV4_UDP_BOOTP_GIADDR_OFFSET];
	u8  *magic  = &frame[SLSI_IPV4_UDP_BOOTP_MAGIC_OFFSET];

	if (magic[0] == 0x63 && magic[1] == 0x82 && magic[2] == 0x53 && magic[3] == 0x63) {
		u8 *p = &frame[SLSI_IPV4_UDP_BOOTP_MAGIC_OFFSET + 4];

		while (p < p + frame_length) {
			u8 option = p[0];
			u8 option_length = p[1];

			if (option == 53 && option_length == 1) {
				for (i = 0; i < ARRAY_SIZE(slsi_ipv4_udp_bootp_dhcp_option53); i++)
					if (slsi_ipv4_udp_bootp_dhcp_option53[i].value == p[2])
						return snprintf(result, result_length, "->%s(ci:%u.%u.%u.%u yi:%u.%u.%u.%u gi:%u.%u.%u.%u)",
								slsi_ipv4_udp_bootp_dhcp_option53[i].name,
								ciaddr[0], ciaddr[1], ciaddr[2], ciaddr[3],
								yiaddr[0], yiaddr[1], yiaddr[2], yiaddr[3],
								giaddr[0], giaddr[1], giaddr[2], giaddr[3]);
				return snprintf(result, result_length, "->option53(%u ci:%u.%u.%u.%u yi:%u.%u.%u.%u gi:%u.%u.%u.%u)",
						p[2],
						ciaddr[0], ciaddr[1], ciaddr[2], ciaddr[3],
						yiaddr[0], yiaddr[1], yiaddr[2], yiaddr[3],
						giaddr[0], giaddr[1], giaddr[2], giaddr[3]);
			}
			if (option == 0)
				break;
			p = p + 2 + option_length;
		}
	}
	return 0;
}

static const struct slsi_value_name_decode slsi_ipv4_udp_ports[] = {
	{ 53, "DNS",   NULL },
	{ 67, "Bootp", slsi_decode_ipv4_udp_bootp },
	{ 68, "Bootp", slsi_decode_ipv4_udp_bootp },
};

static size_t slsi_decode_ipv4_udp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	/*       0      7 8     15 16    23 24    31
	 *       +--------+--------+--------+--------+
	 *       |     Source      |   Destination   |
	 *       |      Port       |      Port       |
	 *       +--------+--------+--------+--------+
	 *       |                 |                 |
	 *       |     Length      |    Checksum     |
	 *       +--------+--------+--------+--------+
	 *       |
	 *       |          data octets ...
	 *       +--------------------- ...
	 */
	size_t size_written = 0;
	u32    i;
	u16    sport  = frame[0] << 8 | frame[1];
	u16    dport  = frame[2] << 8 | frame[3];
	u16    length = frame[4] << 8 | frame[5];

	/*u16 chksum = frame[6] << 8 | frame[7];*/

	for (i = 0; i < ARRAY_SIZE(slsi_ipv4_udp_ports); i++)
		if (slsi_ipv4_udp_ports[i].value == dport || slsi_ipv4_udp_ports[i].value == sport) {
			size_written += snprintf(result, result_length, "->%s", slsi_ipv4_udp_ports[i].name);
			if (slsi_ipv4_udp_ports[i].decode_fn)
				size_written += slsi_ipv4_udp_ports[i].decode_fn(frame + 8, length, result + size_written, result_length - size_written);
			else
				size_written += snprintf(result + size_written, result_length - size_written, "(dport:%u, size:%u)", dport, frame_length - 8);
			return size_written;
		}
	return snprintf(result, result_length, "(dport:%u, size:%u)", dport, frame_length - 8);
}

static size_t slsi_decode_ipv4_tcp(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	/*  TCP Header Format
	 * 0                   1                   2                   3
	 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |          Source Port          |       Destination Port        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                        Sequence Number                        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                    Acknowledgment Number                      |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |  Data |           |U|A|P|R|S|F|                               |
	 * | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
	 * |       |           |G|K|H|T|N|N|                               |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |           Checksum            |         Urgent Pointer        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                    Options                    |    Padding    |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                             data                              |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 */
	u16  dport  = frame[2] << 8 | frame[3];
	u8   flags  = frame[13];
	bool fin = flags & 0x01;
	bool syn = flags & 0x02;
	bool rst = flags & 0x04;
	bool psh = flags & 0x08;
	bool ack = flags & 0x10;
	bool urg = flags & 0x20;

	return snprintf(result, result_length, "(dport:%u%s%s%s%s%s%s size:%u)",
			dport,
			fin ? " FIN" : "",
			syn ? " SYN" : "",
			rst ? " RST" : "",
			psh ? " PSH" : "",
			ack ? " ACK" : "",
			urg ? " URG" : "",
			frame_length - 24);
}

#define SLSI_IPV4_PROTO_ICMP 1
#define SLSI_IPV4_PROTO_IGMP 2
#define SLSI_IPV4_PROTO_TCP  6
#define SLSI_IPV4_PROTO_UDP  17
static size_t slsi_decode_ipv4(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	size_t size_written = 0;
	u16    ip_data_offset = 20;
	/*u8  version         = frame[0] >> 4; */
	u8     hlen            = frame[0] & 0x0F;
	/*u8  tos             = frame[1]; */
	/*u16 len             = frame[2] << 8 | frame[3]; */
	/*u16 id              = frame[4] << 8 | frame[5]; */
	/*u16 flags_foff      = frame[6] << 8 | frame[7]; */
	/*u8  ttl             = frame[8]; */
	u8 proto           = frame[9];
	/*u16 cksum           = frame[10] << 8 | frame[11]; */
	u8 *src_ip         = &frame[12];
	u8 *dest_ip        = &frame[16];

	if (hlen > 5)
		ip_data_offset += (hlen - 5) * 4;

	size_written += snprintf(result + size_written, result_length - size_written, "(s:%u.%u.%u.%u d:%u.%u.%u.%u)",
				 src_ip[0], src_ip[1], src_ip[2], src_ip[3],
				 dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3]);

	switch (proto) {
	case SLSI_IPV4_PROTO_TCP:
		size_written += snprintf(result + size_written, result_length - size_written, "->TCP");
		size_written += slsi_decode_ipv4_tcp(frame + ip_data_offset,
						     frame_length - ip_data_offset,
						     result + size_written,
						     result_length - size_written);
		break;
	case SLSI_IPV4_PROTO_UDP:
		size_written += snprintf(result + size_written, result_length - size_written, "->UDP");
		size_written += slsi_decode_ipv4_udp(frame + ip_data_offset,
						     frame_length - ip_data_offset,
						     result + size_written,
						     result_length - size_written);
		break;
	case SLSI_IPV4_PROTO_ICMP:
		size_written += snprintf(result + size_written, result_length - size_written, "->ICMP");
		size_written += slsi_decode_ipv4_icmp(frame + ip_data_offset,
						      frame_length - ip_data_offset,
						      result + size_written,
						      result_length - size_written);
		break;
	case SLSI_IPV4_PROTO_IGMP:
		size_written += snprintf(result + size_written, result_length - size_written, "->IGMP");
		break;
	default:
		size_written += snprintf(result + size_written, result_length - size_written, "->proto:%u", proto);
		break;
	}
	return size_written;
}

static const struct slsi_decode_snap snap_types[] = {
	{ { 0x08, 0x00 }, "IpV4",  slsi_decode_ipv4 },
	{ { 0x08, 0x06 }, "Arp",   slsi_decode_arp },
	{ { 0x88, 0x8e }, "Eapol", slsi_decode_eapol },
	{ { 0x89, 0x0d }, NULL,    slsi_decode_tdls },
	{ { 0x86, 0xdd }, "IpV6",  NULL },
	{ { 0x88, 0xb4 }, "Wapi",  NULL },
};

static void slsi_decode_proto_data(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(snap_types); i++)
		if (memcmp(snap_types[i].snap, frame, 2) == 0) {
			int slen = 0;

			if (snap_types[i].name)
				slen = snprintf(result, result_length, "->%s", snap_types[i].name);
			if (snap_types[i].decode_fn)
				slen += snap_types[i].decode_fn(frame + 2, frame_length - 2, result + slen, result_length - slen);
			return;
		}

	snprintf(result, result_length, "(proto:0x%.2X%.2X)", frame[0], frame[1]);
}

static void slsi_decode_80211_data(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	return slsi_decode_proto_data(frame + 6, frame_length - 6, result, result_length);
}

static const struct slsi_decode_entry frame_types[4][16] = {
	{
		{ "AssocReq",            slsi_decode_assoc_req },
		{ "AssocRsp",            slsi_decode_assoc_rsp },
		{ "ReassocReq",          slsi_decode_reassoc_req },
		{ "ReassocRsp",          slsi_decode_assoc_rsp },       /* Same as Assoc Req Frame*/
		{ "ProbeReq",            slsi_decode_probe_req },
		{ "ProbeRsp",            slsi_decode_beacon },          /* Same as Beacon Frame */
		{ "TimingAdv",           NULL },
		{ "Reserved",            NULL },
		{ "Beacon  ",            slsi_decode_beacon },
		{ "Atim",                NULL },
		{ "Disassoc",            slsi_decode_deauth },  /* Same as Deauth Frame */
		{ "Auth",                slsi_decode_auth },
		{ "Deauth",              slsi_decode_deauth },
		{ "Action",              slsi_decode_action },
		{ "ActionNoAck",         slsi_decode_action },
		{ "Reserved",            NULL }
	},
	{
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "BlockAckReq",         NULL },
		{ "BlockAck",            NULL },
		{ "PsPoll",              NULL },
		{ "RTS",                 NULL },
		{ "CTS",                 NULL },
		{ "Ack",                 NULL },
		{ "CF-End",              NULL },
		{ "CF-End+Ack",          NULL }
	},
	{
		{ "Data",                slsi_decode_80211_data },
		{ "Data+CF-Ack",         slsi_decode_80211_data },
		{ "Data+CF-Poll",        slsi_decode_80211_data },
		{ "Data+CF-Ack+Poll",    slsi_decode_80211_data },
		{ "Null",                NULL },
		{ "CF-Ack",              NULL },
		{ "CF-Poll",             NULL },
		{ "CF-Ack+Poll",         NULL },
		{ "QosData",             slsi_decode_80211_data },
		{ "QosData+CF-Ack",      slsi_decode_80211_data },
		{ "QosData+CF-Poll",     slsi_decode_80211_data },
		{ "QosData+CF-Ack+Poll", slsi_decode_80211_data },
		{ "QosNull",             NULL },
		{ "Reserved",            NULL },
		{ "QosCF-Poll",          NULL },
		{ "QosCF-Ack+Poll",      NULL }
	},
	{
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
		{ "Reserved",            NULL },
	}
};

static bool slsi_decode_80211_frame(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	struct ieee80211_hdr           *hdr = (struct ieee80211_hdr *)frame;
	u16                            fc_cpu = cpu_to_le16(hdr->frame_control);
	int                            ftype_idx = (fc_cpu & 0xf) >> 2;
	const struct slsi_decode_entry *entry;
	int                            hdrlen;
	int                            slen;

	/* Only decode Management Frames at Level 1 */
	if (slsi_debug_summary_frame == 1 && ftype_idx != 0)
		return false;

	/* Filter Scanning at the debug level 3 and above as it can be noisy with large scan results */
	if (slsi_debug_summary_frame < 3 &&
	    (ieee80211_is_probe_req(fc_cpu) || ieee80211_is_probe_resp(fc_cpu) || ieee80211_is_beacon(fc_cpu)))
		return false;

	entry = &frame_types[ftype_idx][(fc_cpu >> 4) & 0xf];
	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	slen = snprintf(result, result_length, entry->name);

	if (entry->decode_fn)
		entry->decode_fn(frame + hdrlen, frame_length - hdrlen, result + slen, result_length - slen);
	return true;
}

static bool slsi_decode_l3_frame(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	int slen;

	/* Only decode Management Frames at Level 1 */
	if (slsi_debug_summary_frame == 1)
		return false;

	/* Only decode high important frames e.g. EAPOL, ARP, DHCP at Level 2 */
	if (slsi_debug_summary_frame == 2) {
		struct ethhdr *ehdr = (struct ethhdr *)frame;
		u16           eth_type = be16_to_cpu(ehdr->h_proto);

		switch (eth_type) {
		case ETH_P_IP:
			if (slsi_is_dhcp_packet(frame) == SLSI_TX_IS_NOT_DHCP)
				return false;
			break;
		/* Fall through; process EAPOL, WAPI and ARP frames */
		case ETH_P_PAE:
		case ETH_P_WAI:
		case ETH_P_ARP:
			break;
		default:
			/* return for all other frames */
			return false;
		}
	}
	slen = snprintf(result, result_length, "eth");
	slsi_decode_proto_data(frame + 12, frame_length - 12, result + slen, result_length - slen);
	return true;
}

static bool slsi_decode_amsdu_subframe(u8 *frame, u16 frame_length, char *result, size_t result_length)
{
	int slen;

	/* Only decode Management Frames at Level 1 */
	if (slsi_debug_summary_frame == 1)
		return false;

	/* Only decode high important frames e.g. EAPOL, ARP, DHCP at Level 2 */
	if (slsi_debug_summary_frame == 2) {
		struct msduhdr *msdu_hdr = (struct msduhdr *)frame;
		u16           eth_type = be16_to_cpu(msdu_hdr->type);

		switch (eth_type) {
		case ETH_P_IP:
			/* slsi_is_dhcp_packet() decodes the frame as Ethernet frame so
			 * pass a offset (difference between MSDU header and ethernet header)
			 * to frames so it reads at the right offset
			 */
			if (slsi_is_dhcp_packet(frame + 8) == SLSI_TX_IS_NOT_DHCP)
				return false;
			break;
		/* Fall through; process EAPOL, WAPI and ARP frames */
		case ETH_P_PAE:
		case ETH_P_WAI:
		case ETH_P_ARP:
			break;
		default:
			/* return for all other frames */
			return false;
		}
	}
	slen = snprintf(result, result_length, "eth");
	slsi_decode_proto_data(frame + 20, frame_length - 20, result + slen, result_length - slen);
	return true;
}

static inline bool slsi_debug_frame_ratelimited(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, (5 * HZ), 200);

	if (__ratelimit(&_rs))
		return true;
	return false;
}

/* NOTE: dev can be NULL */
void slsi_debug_frame(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, const char *prefix)
{
	char frame_info[384];
	u8   *frame = fapi_get_data(skb);
	u16  len = fapi_get_datalen(skb);
	u8   *dst = NULL;
	u8   *src = NULL;
	u16  frametype = 0xFFFF;
	bool print = false;
	u16  id = fapi_get_sigid(skb);
	u16  vif = fapi_get_vif(skb);
	s16  rssi = 0;

	if (!slsi_debug_summary_frame)
		return;

	if (!len)
		return;

	switch (id) {
	case MA_UNITDATA_REQ:
	case MA_UNITDATA_IND:
		if (!slsi_debug_frame_ratelimited())    /* Limit the Data output to stop too much spam at high data rates */
			return;
		break;
	default:
		break;
	}

	frame_info[0] = '\0';
	switch (id) {
	case MA_UNITDATA_REQ:
		frametype = fapi_get_u16(skb, u.ma_unitdata_req.data_unit_descriptor);
		break;
	case MA_UNITDATA_IND:
		if (fapi_get_u16(skb, u.ma_unitdata_ind.bulk_data_descriptor) == FAPI_BULKDATADESCRIPTOR_INLINE)
			frametype = fapi_get_u16(skb, u.ma_unitdata_ind.data_unit_descriptor);
		break;
	case MLME_SEND_FRAME_REQ:
		frametype = fapi_get_u16(skb, u.mlme_send_frame_req.data_unit_descriptor);
		break;
	case MLME_RECEIVED_FRAME_IND:
		frametype = fapi_get_u16(skb, u.mlme_received_frame_ind.data_unit_descriptor);
		break;
	case MLME_SCAN_IND:
		frametype = FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME;
		rssi = fapi_get_s16(skb, u.mlme_scan_ind.rssi);
		vif = fapi_get_u16(skb, u.mlme_scan_ind.scan_id) >> 8;
		break;
	case MLME_CONNECT_CFM:
	case MLME_CONNECT_IND:
	case MLME_PROCEDURE_STARTED_IND:
	case MLME_CONNECTED_IND:
	case MLME_REASSOCIATE_IND:
	case MLME_ROAMED_IND:
		frametype = FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME;
		break;
	default:
		return;
	}

	switch (frametype) {
	case FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME:
	{
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)frame;

		dst = hdr->addr1;
		src = hdr->addr2;
		print = slsi_decode_80211_frame(frame, len, frame_info, sizeof(frame_info));
		break;
	}
	case FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME:
	{
		struct ethhdr *ehdr = (struct ethhdr *)frame;

		dst = ehdr->h_dest;
		src = ehdr->h_source;
		print = slsi_decode_l3_frame(frame, len, frame_info, sizeof(frame_info));
		break;
	}
	case FAPI_DATAUNITDESCRIPTOR_AMSDU_SUBFRAME:
	{
		struct ethhdr *ehdr = (struct ethhdr *)frame;

		dst = ehdr->h_dest;
		src = ehdr->h_source;
		print = slsi_decode_amsdu_subframe(frame, len, frame_info, sizeof(frame_info));
		break;
	}
	default:
		return;
	}
	if (print) {
		SLSI_DBG4(sdev, SLSI_SUMMARY_FRAMES, "%-5s: %s(vif:%u rssi:%-3d, s:%pM d:%pM)->%s\n",
			 dev ? netdev_name(dev) : "", prefix, vif, rssi, src, dst, frame_info);
	}
}

#endif /* CONFIG_SCSC_WLAN_DEBUG */
