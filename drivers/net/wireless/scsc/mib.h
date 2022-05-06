/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Note: this is an auto-generated file. */

#ifndef SLSI_MIB_H__
#define SLSI_MIB_H__

#ifdef __cplusplus
extern "C" {
#endif

struct slsi_mib_data {
	u32 dataLength;
	u8  *data;
};

#define SLSI_MIB_MAX_INDEXES 2U

#define SLSI_MIB_TYPE_BOOL    0
#define SLSI_MIB_TYPE_UINT    1
#define SLSI_MIB_TYPE_INT     2
#define SLSI_MIB_TYPE_OCTET   3U
#define SLSI_MIB_TYPE_NONE    4

struct slsi_mib_value {
	u8 type;
	union {
		bool                 boolValue;
		s32                  intValue;
		u32                  uintValue;
		struct slsi_mib_data octetValue;
	} u;
};

struct slsi_mib_entry {
	u16                   psid;
	u16                   index[SLSI_MIB_MAX_INDEXES]; /* 0 = no Index */
	struct slsi_mib_value value;
};

struct slsi_mib_get_entry {
	u16 psid;
	u16 index[SLSI_MIB_MAX_INDEXES]; /* 0 = no Index */
};

#define SLSI_MIB_STATUS_SUCCESS                     0x0000
#define SLSI_MIB_STATUS_UNKNOWN_PSID                0x0001
#define SLSI_MIB_STATUS_INVALID_INDEX               0x0002
#define SLSI_MIB_STATUS_OUT_OF_RANGE                0x0003
#define SLSI_MIB_STATUS_WRITE_ONLY                  0x0004
#define SLSI_MIB_STATUS_READ_ONLY                   0x0005
#define SLSI_MIB_STATUS_UNKNOWN_INTERFACE_TAG       0x0006
#define SLSI_MIB_STATUS_INVALID_NUMBER_OF_INDICES   0x0007
#define SLSI_MIB_STATUS_ERROR                       0x0008
#define SLSI_MIB_STATUS_UNSUPPORTED_ON_INTERFACE    0x0009
#define SLSI_MIB_STATUS_UNAVAILABLE                 0x000A
#define SLSI_MIB_STATUS_NOT_FOUND                   0x000B
#define SLSI_MIB_STATUS_INCOMPATIBLE                0x000C
#define SLSI_MIB_STATUS_OUT_OF_MEMORY               0x000D
#define SLSI_MIB_STATUS_TO_MANY_REQUESTED_VARIABLES 0x000E
#define SLSI_MIB_STATUS_NOT_TRIED                   0x000F
#define SLSI_MIB_STATUS_FAILURE                     0xFFFF

/*******************************************************************************
 *
 * NAME
 *  slsi_mib_encode_get Functions
 *
 * DESCRIPTION
 *  For use when getting data from the Wifi Stack.
 *  These functions append the encoded data to the "buffer".
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      static const struct slsi_mib_get_entry getValues[] = {
 *          { PSID1, { 0, 0 } },
 *          { PSID2, { 3, 0 } },
 *      };
 *      struct slsi_mib_data buffer;
 *      slsi_mib_encode_get_list(&buffer,
 *                              sizeof(getValues) / sizeof(struct slsi_mib_get_entry),
 *                              getValues);
 *  }
 *  or
 *  {
 *      struct slsi_mib_data buffer = {0, NULL};
 *      slsi_mib_encode_get(&buffer, PSID1, 0);
 *      slsi_mib_encode_get(&buffer, PSID2, 3);
 *  }
 * RETURN
 *  SlsiResult: See SLSI_MIB_STATUS_*
 *
 *******************************************************************************/
void slsi_mib_encode_get(struct slsi_mib_data *buffer, u16 psid, u16 index);
int slsi_mib_encode_get_list(struct slsi_mib_data *buffer, u16 psidsLength, const struct slsi_mib_get_entry *psids);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMibdEncode Functions
 *
 * DESCRIPTION
 *  For use when getting data from the Wifi Stack.
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      static const struct slsi_mib_get_entry getValues[] = {
 *          { PSID1, { 0, 0 } },
 *          { PSID2, { 3, 0 } },
 *      };
 *      struct slsi_mib_data buffer = rxMibData; # Buffer with encoded Mib Data
 *
 *      getValues = slsi_mib_decode_get_list(&buffer,
 *                                      sizeof(getValues) / sizeof(struct slsi_mib_get_entry),
 *                                      getValues);
 *
 *      print("PSID1 = %d\n", getValues[0].u.uintValue);
 *      print("PSID2.3 = %s\n", getValues[1].u.boolValue?"TRUE":"FALSE");
 *
 *      kfree(getValues);
 *
 *  }
 *  or
 *  {
 *      u8* buffer = rxMibData; # Buffer with encoded Mib Data
 *      size_t offset=0;
 *      struct slsi_mib_entry value;
 *
 *      offset += slsi_mib_decode(&buffer[offset], &value);
 *      print("PSID1 = %d\n", value.u.uintValue);
 *
 *      offset += slsi_mib_decode(&buffer[offset], &value);
 *      print("PSID2.3 = %s\n", value.u.boolValue?"TRUE":"FALSE");
 *
 *  }
 *
 *******************************************************************************/
size_t slsi_mib_decode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value);
struct slsi_mib_value *slsi_mib_decode_get_list(struct slsi_mib_data *buffer, u16 psidsLength, const struct slsi_mib_get_entry *psids);

/*******************************************************************************
 *
 * NAME
 *  slsi_mib_encode Functions
 *
 * DESCRIPTION
 *  For use when setting data in the Wifi Stack.
 *  These functions append the encoded data to the "buffer".
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      u8 octets[2] = {0x00, 0x01};
 *      struct slsi_mib_data buffer = {0, NULL};
 *      slsi_mib_encode_bool(&buffer, PSID1, TRUE, 0);                     # Boolean set with no index
 *      slsi_mib_encode_int(&buffer, PSID2, -1234, 1);                     # Signed Integer set with on index 1
 *      slsi_mib_encode_uint(&buffer, PSID2, 1234, 3);                     # Unsigned Integer set with on index 3
 *      slsi_mib_encode_octet(&buffer, PSID3, sizeof(octets), octets, 0);  # Octet set with no index
 *  }
 *  or
 *  {
 # Unsigned Integer set with on index 3
 #      struct slsi_mib_data buffer = {0, NULL};
 #      struct slsi_mib_entry value;
 #      value.psid = psid;
 #      value.index[0] = 3;
 #      value.index[1] = 0;
 #      value.value.type = SLSI_MIB_TYPE_UINT;
 #      value.value.u.uintValue = 1234;
 #      slsi_mib_encode(buffer, &value);
 #  }
 # RETURN
 #  See SLSI_MIB_STATUS_*
 #
 *******************************************************************************/
u16 slsi_mib_encode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value);
u16 slsi_mib_encode_bool(struct slsi_mib_data *buffer, u16 psid, bool value, u16 index);
u16 slsi_mib_encode_int(struct slsi_mib_data *buffer, u16 psid, s32 value, u16 index);
u16 slsi_mib_encode_uint(struct slsi_mib_data *buffer, u16 psid, u32 value, u16 index);
u16 slsi_mib_encode_octet(struct slsi_mib_data *buffer, u16 psid, size_t dataLength, const u8 *data, u16 index);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMib Low level Encode/Decode functions
 *
 *******************************************************************************/
size_t slsi_mib_encode_uint32(u8 *buffer, u32 value);
size_t slsi_mib_encode_int32(u8 *buffer, s32 signedValue);
size_t slsi_mib_encode_octet_str(u8 *buffer, struct slsi_mib_data *octetValue);

size_t slsi_mib_decodeUint32(u8 *buffer, u32 *value);
size_t slsi_mib_decodeInt32(u8 *buffer, s32 *value);
size_t slsi_mib_decodeUint64(u8 *buffer, u64 *value);
size_t slsi_mib_decodeInt64(u8 *buffer, s64 *value);
size_t slsi_mib_decode_octet_str(u8 *buffer, struct slsi_mib_data *octetValue);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMib Helper Functions
 *
 *******************************************************************************/

/* Find a the offset to psid data in an encoded buffer
 * {
 *      struct slsi_mib_data buffer = rxMibData;                 # Buffer with encoded Mib Data
 *      struct slsi_mib_get_entry value = {PSID1, {0x01, 0x00}};   # Find value for PSID1.1
 *      u8* mibdata = slsi_mib_find(&buffer, &value);
 *      if(mibdata) {print("Mib Data for PSID1.1 Found\n");
 *  }
 */
u8 *slsi_mib_find(struct slsi_mib_data *buffer, const struct slsi_mib_get_entry *entry);

/* Append data to a Buffer */
void slsi_mib_buf_append(struct slsi_mib_data *dst, size_t bufferLength, u8 *buffer);

/*******************************************************************************
 *
 * PSID Definitions
 *
 *******************************************************************************/

/*******************************************************************************
 * NAME          : Dot11TdlsPeerUapsdIndicationWindow
 * PSID          : 53 (0x0035)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  The minimum time after the last TPU SP, before a RAME_TPU_SP indication
 *  can be issued.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_PEER_UAPSD_INDICATION_WINDOW 0x0035

/*******************************************************************************
 * NAME          : Dot11AssociationSaQueryMaximumTimeout
 * PSID          : 100 (0x0064)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Timeout (in TUs) before giving up on a Peer that has not responded to a
 *  SA Query frame.
 *******************************************************************************/
#define SLSI_PSID_DOT11_ASSOCIATION_SA_QUERY_MAXIMUM_TIMEOUT 0x0064

/*******************************************************************************
 * NAME          : Dot11AssociationSaQueryRetryTimeout
 * PSID          : 101 (0x0065)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 201
 * DESCRIPTION   :
 *  Timeout (in TUs) before trying a Query Request frame.
 *******************************************************************************/
#define SLSI_PSID_DOT11_ASSOCIATION_SA_QUERY_RETRY_TIMEOUT 0x0065

/*******************************************************************************
 * NAME          : Dot11MinPowerCapabilityOverride
 * PSID          : 113 (0x0071)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -127
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Override for
 *  the advertised power in an Association Request.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MIN_POWER_CAPABILITY_OVERRIDE 0x0071

/*******************************************************************************
 * NAME          : Dot11MaxNetworkPowerCapabilityOverride
 * PSID          : 114 (0x0072)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -127
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Override for
 *  the regulatory domain max power used in calculating network power cap.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MAX_NETWORK_POWER_CAPABILITY_OVERRIDE 0x0072

/*******************************************************************************
 * NAME          : Dot11RtsThreshold
 * PSID          : 121 (0x0079)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : octet
 * MIN           : 0
 * MAX           : 65536
 * DEFAULT       : 65536
 * DESCRIPTION   :
 *  Size of an MPDU, below which an RTS/CTS handshake shall not be performed,
 *  except as RTS/CTS is used as a cross modulation protection mechanism as
 *  defined in 9.10. An RTS/CTS handshake shall be performed at the beginning
 *  of any frame exchange sequence where the MPDU is of type Data or
 *  Management, the MPDU has an individual address in the Address1 field, and
 *  the length of the MPDU is greater than this threshold. (For additional
 *  details, refer to Table 21 in 9.7.) Setting larger than the maximum MSDU
 *  size shall have the effect of turning off the RTS/CTS handshake for
 *  frames of Data or Management type transmitted by this STA. Setting to
 *  zero shall have the effect of turning on the RTS/CTS handshake for all
 *  frames of Data or Management type transmitted by this STA.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RTS_THRESHOLD 0x0079

/*******************************************************************************
 * NAME          : Dot11ShortRetryLimit
 * PSID          : 122 (0x007A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 32
 * DESCRIPTION   :
 *  Maximum number of transmission attempts of a frame, the length of which
 *  is less than or equal to dot11RTSThreshold, that shall be made before a
 *  failure condition is indicated.
 *******************************************************************************/
#define SLSI_PSID_DOT11_SHORT_RETRY_LIMIT 0x007A

/*******************************************************************************
 * NAME          : Dot11LongRetryLimit
 * PSID          : 123 (0x007B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Maximum number of transmission attempts of a frame, the length of which
 *  is greater than dot11RTSThreshold, that shall be made before a failure
 *  condition is indicated.
 *******************************************************************************/
#define SLSI_PSID_DOT11_LONG_RETRY_LIMIT 0x007B

/*******************************************************************************
 * NAME          : Dot11FragmentationThreshold
 * PSID          : 124 (0x007C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 256
 * MAX           : 11500
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  Current maximum size, in octets, of the MPDU that may be delivered to the
 *  security encapsulation. This maximum size does not apply when an MSDU is
 *  transmitted using an HT-immediate or HTdelayed Block Ack agreement, or
 *  when an MSDU or MMPDU is carried in an AMPDU that does not contain a VHT
 *  single MPDU. Fields added to the frame by security encapsulation are not
 *  counted against the limit specified. Except as described above, an MSDU
 *  or MMPDU is fragmented when the resulting frame has an individual address
 *  in the Address1 field, and the length of the frame is larger than this
 *  threshold, excluding security encapsulation fields. The default value is
 *  the lesser of 11500 or the aMPDUMaxLength or the aPSDUMaxLength of the
 *  attached PHY and the value never exceeds the lesser of 11500 or the
 *  aMPDUMaxLength or the aPSDUMaxLength of the attached PHY.
 *******************************************************************************/
#define SLSI_PSID_DOT11_FRAGMENTATION_THRESHOLD 0x007C

/*******************************************************************************
 * NAME          : Dot11TxopDurationRtsThreshold
 * PSID          : 125 (0x007D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : octet
 * MIN           : 0
 * MAX           : 1023
 * DEFAULT       : 1023
 * DESCRIPTION   :
 *  In an HE BSS, the use of RTS/CTS can be TXOP duration-based.Any Data PPDU
 *  transmitted from the STA whose TXOP duration is greater or equal to the
 *  duration calculated above should pre-pend with RTS. Value is in units of
 *  32us
 *******************************************************************************/
#define SLSI_PSID_DOT11_TXOP_DURATION_RTS_THRESHOLD 0x007D

/*******************************************************************************
 * NAME          : Dot11RtsSuccessCount
 * PSID          : 146 (0x0092)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a CTS is received in response to an
 *  RTS.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RTS_SUCCESS_COUNT 0x0092

/*******************************************************************************
 * NAME          : Dot11AckFailureCount
 * PSID          : 148 (0x0094)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when an ACK is not received when expected.
 *******************************************************************************/
#define SLSI_PSID_DOT11_ACK_FAILURE_COUNT 0x0094

/*******************************************************************************
 * NAME          : Dot11MulticastReceivedFrameCount
 * PSID          : 150 (0x0096)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a MSDU is received with the multicast
 *  bit set in the destination MAC address.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MULTICAST_RECEIVED_FRAME_COUNT 0x0096

/*******************************************************************************
 * NAME          : Dot11FcsErrorCount
 * PSID          : 151 (0x0097)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : -9223372036854775808
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when an FCS error is detected in a received
 *  MPDU.
 *******************************************************************************/
#define SLSI_PSID_DOT11_FCS_ERROR_COUNT 0x0097

/*******************************************************************************
 * NAME          : Dot11WepUndecryptableCount
 * PSID          : 153 (0x0099)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a frame is received with the WEP
 *  subfield of the Frame Control field set to one and the WEPOn value for
 *  the key mapped to the transmitter&apos;s MAC address indicates that the
 *  frame should not have been encrypted or that frame is discarded due to
 *  the receiving STA not implementing the privacy option.
 *******************************************************************************/
#define SLSI_PSID_DOT11_WEP_UNDECRYPTABLE_COUNT 0x0099

/*******************************************************************************
 * NAME          : Dot11ManufacturerProductVersion
 * PSID          : 183 (0x00B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 300
 * DEFAULT       :
 * DESCRIPTION   :
 *  Printable string used to identify the manufacturer&apos;s product version
 *  of the resource.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MANUFACTURER_PRODUCT_VERSION 0x00B7

/*******************************************************************************
 * NAME          : Dot11RsnaStatsStaAddress
 * PSID          : 430 (0x01AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The MAC address of the STA to which the statistics in this conceptual row
 *  belong.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_STA_ADDRESS 0x01AE

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipicvErrors
 * PSID          : 433 (0x01B1)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of TKIP ICV errors encountered when decrypting packets
 *  for the STA.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIPICV_ERRORS 0x01B1

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipLocalMicFailures
 * PSID          : 434 (0x01B2)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of MIC failures encountered when checking the integrity
 *  of packets received from the STA at this entity.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_LOCAL_MIC_FAILURES 0x01B2

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipRemoteMicFailures
 * PSID          : 435 (0x01B3)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of MIC failures encountered by the STA identified by
 *  dot11RSNAStatsSTAAddress and reported back to this entity.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_REMOTE_MIC_FAILURES 0x01B3

/*******************************************************************************
 * NAME          : Dot11RsnaStatsCcmpReplays
 * PSID          : 436 (0x01B4)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received CCMP MPDUs discarded by the replay mechanism.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_CCMP_REPLAYS 0x01B4

/*******************************************************************************
 * NAME          : Dot11RsnaStatsCcmpDecryptErrors
 * PSID          : 437 (0x01B5)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received MPDUs discarded by the CCMP decryption algorithm.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_CCMP_DECRYPT_ERRORS 0x01B5

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipReplays
 * PSID          : 438 (0x01B6)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of TKIP replay errors detected.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_REPLAYS 0x01B6

/*******************************************************************************
 * NAME          : Dot11RsnaStatsRobustMgmtCcmpReplays
 * PSID          : 441 (0x01B9)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received Robust Management frame MPDUs discarded due to
 *  CCMP replay errors
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_ROBUST_MGMT_CCMP_REPLAYS 0x01B9

/*******************************************************************************
 * NAME          : UnifiMlmeConnectionTimeout
 * PSID          : 2000 (0x07D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_CONNECTION_TIMEOUT 0x07D0

/*******************************************************************************
 * NAME          : UnifiMlmeScanChannelMaxScanTime
 * PSID          : 2001 (0x07D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 14
 * MAX           : 14
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Test only: overrides max_scan_time. 0 indicates not used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CHANNEL_MAX_SCAN_TIME 0x07D1

/*******************************************************************************
 * NAME          : UnifiMlmeScanChannelProbeInterval
 * PSID          : 2002 (0x07D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 14
 * MAX           : 14
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Test only: overrides probe interval. 0 indicates not used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CHANNEL_PROBE_INTERVAL 0x07D2

/*******************************************************************************
 * NAME          : UnifiMlmeScanChannelRule
 * PSID          : 2003 (0x07D3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       : { 0X00, 0X01, 0X00, 0X01 }
 * DESCRIPTION   :
 *  Rules for channel scanners.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CHANNEL_RULE 0x07D3

/*******************************************************************************
 * NAME          : UnifiMlmeDataReferenceTimeout
 * PSID          : 2005 (0x07D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65534
 * DEFAULT       :
 * DESCRIPTION   :
 *  Maximum time, in TU, allowed for the data in data references
 *  corresponding to MLME primitives to be made available to the firmware.
 *  The special value 0 specifies an infinite timeout.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_DATA_REFERENCE_TIMEOUT 0x07D5

/*******************************************************************************
 * NAME          : UnifiMlmeScanProbeInterval
 * PSID          : 2007 (0x07D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_PROBE_INTERVAL 0x07D7

/*******************************************************************************
 * NAME          : UnifiMlmeScanHighRssiThreshold
 * PSID          : 2008 (0x07D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -90
 * DESCRIPTION   :
 *  Minimum RSSI, in dB, for a scan indication to be kept.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_HIGH_RSSI_THRESHOLD 0x07D8

/*******************************************************************************
 * NAME          : UnifiMlmeScanDeltaRssiThreshold
 * PSID          : 2010 (0x07DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  Magnitude of the change in RSSI for which a scan result will be issued.
 *  In dB.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_DELTA_RSSI_THRESHOLD 0x07DA

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaximumAge
 * PSID          : 2014 (0x07DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAXIMUM_AGE 0x07DE

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaximumResults
 * PSID          : 2015 (0x07DF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Max number of scan results, per sps, which will be stored before the
 *  oldest result is discarded, irrespective of its age. The value 0
 *  specifies no maximum.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAXIMUM_RESULTS 0x07DF

/*******************************************************************************
 * NAME          : UnifiMlmeAutonomousScanNoisy
 * PSID          : 2016 (0x07E0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_AUTONOMOUS_SCAN_NOISY 0x07E0

/*******************************************************************************
 * NAME          : UnifiChannelBusyThreshold
 * PSID          : 2018 (0x07E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 100
 * DEFAULT       : 25
 * DESCRIPTION   :
 *  The threshold in percentage of CCA busy time when a channel would be
 *  considered busy
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHANNEL_BUSY_THRESHOLD 0x07E2

/*******************************************************************************
 * NAME          : UnifiMacSequenceNumberRandomisationActivated
 * PSID          : 2020 (0x07E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enabling Sequence Number Randomisation to be applied for Probe Requests
 *  when scanning. Note: Randomisation only happens, if mac address gets
 *  randomised.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_SEQUENCE_NUMBER_RANDOMISATION_ACTIVATED 0x07E4

/*******************************************************************************
 * NAME          : UnifiFirmwareBuildId
 * PSID          : 2021 (0x07E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric build identifier for this firmware build. This should normally be
 *  displayed in decimal. The textual build identifier is available via the
 *  standard dot11manufacturerProductVersion MIB attribute.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FIRMWARE_BUILD_ID 0x07E5

/*******************************************************************************
 * NAME          : UnifiChipVersion
 * PSID          : 2022 (0x07E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric identifier for the UniFi silicon revision (as returned by the
 *  GBL_CHIP_VERSION hardware register). Other than being different for each
 *  design variant (but not for alternative packaging options), the
 *  particular values returned do not have any significance.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHIP_VERSION 0x07E6

/*******************************************************************************
 * NAME          : UnifiFirmwarePatchBuildId
 * PSID          : 2023 (0x07E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric build identifier for the patch set that has been applied to this
 *  firmware image. This should normally be displayed in decimal. For a
 *  patched ROM build there will be two build identifiers, the first will
 *  correspond to the base ROM image, the second will correspond to the patch
 *  set that has been applied.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FIRMWARE_PATCH_BUILD_ID 0x07E7

/*******************************************************************************
 * NAME          : UnifiExtendedCapabilitiesSoftAp
 * PSID          : 2024 (0x07E8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 9
 * DEFAULT       : { 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X40, 0X00 }
 * DESCRIPTION   :
 *  Extended capabilities for SoftAp. Bit field definition and coding follows
 *  IEEE 802.11 Extended Capability Information Element, with spare subfields
 *  for capabilities that are independent from chip/firmware implementation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTENDED_CAPABILITIES_SOFT_AP 0x07E8

/*******************************************************************************
 * NAME          : UnifiMaxNumAntennaToUse
 * PSID          : 2025 (0x07E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0202
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Specify the
 *  maximum number of antenna that will be used for each band. Lower 8 bits =
 *  2GHz, Higher 8 bits = 5Ghz. Limited by maximum supported by underlying
 *  hardware. WARNING: Changing this value after system start-up will have no
 *  effect.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_NUM_ANTENNA_TO_USE 0x07E9

/*******************************************************************************
 * NAME          : UnifiHtCapabilities5g
 * PSID          : 2026 (0x07EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_CAPABILITIES5G 0x07EA

/*******************************************************************************
 * NAME          : UnifiVhtCapabilities5g
 * PSID          : 2027 (0x07EB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VHT_CAPABILITIES5G 0x07EB

/*******************************************************************************
 * NAME          : UnifiHtCapabilitiesSoftAp
 * PSID          : 2028 (0x07EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 21
 * MAX           : 21
 * DEFAULT       : { 0XEF, 0X0A, 0X17, 0XFF, 0XFF, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. HT
 *  capabilities for softAP to disallow MIMO when customer want to control
 *  MIMO/ SISO mode (see SC-508764-DD for further details). NOTE: Greenfield
 *  has been disabled due to interoperability issues with setting SGI.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_CAPABILITIES_SOFT_AP 0x07EC

/*******************************************************************************
 * NAME          : UnifiSoftAp40MhzOn24g
 * PSID          : 2029 (0x07ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables 40MHz operation on 2.4GHz band for SoftAP.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SOFT_AP40_MHZ_ON24G 0x07ED

/*******************************************************************************
 * NAME          : UnifiBasicCapabilities
 * PSID          : 2030 (0x07EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X1730
 * DESCRIPTION   :
 *  The 16-bit field follows the coding of IEEE 802.11 Capability
 *  Information.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BASIC_CAPABILITIES 0x07EE

/*******************************************************************************
 * NAME          : UnifiExtendedCapabilities
 * PSID          : 2031 (0x07EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 10
 * MAX           : 10
 * DEFAULT       : { 0X05, 0X00, 0X08, 0X00, 0X00, 0X00, 0X00, 0X40, 0X80, 0X20}
 * DESCRIPTION   :
 *  Extended capabilities. Bit field definition and coding follows IEEE
 *  802.11 Extended Capability Information Element, with spare subfields for
 *  capabilities that are independent from chip/firmware implementation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTENDED_CAPABILITIES 0x07EF

/*******************************************************************************
 * NAME          : UnifiHtCapabilities
 * PSID          : 2032 (0x07F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 21
 * MAX           : 21
 * DEFAULT       : { 0XEF, 0X0A, 0X17, 0XFF, 0XFF, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. HT
 *  capabilities of the chip. See SC-503520-SP for further details. NOTE:
 *  Greenfield has been disabled due to interoperability issues wuth SGI.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_CAPABILITIES 0x07F0

/*******************************************************************************
 * NAME          : UnifiRsnCapabilities
 * PSID          : 2034 (0x07F2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSN_CAPABILITIES 0x07F2

/*******************************************************************************
 * NAME          : Unifi24G40MhzChannels
 * PSID          : 2035 (0x07F3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables 40Mz wide channels in the 2.4G band for STA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI24_G40_MHZ_CHANNELS 0x07F3

/*******************************************************************************
 * NAME          : UnifiExtendedCapabilitiesDisabled
 * PSID          : 2036 (0x07F4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Suppress
 *  extended capabilities IE being sent in the association request. Please
 *  note that this may fix IOP issues with Aruba APs in WMMAC. Singed Decimal
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTENDED_CAPABILITIES_DISABLED 0x07F4

/*******************************************************************************
 * NAME          : UnifiSupportedDataRates
 * PSID          : 2041 (0x07F9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * UNITS         : 500 kbps
 * MIN           : 2
 * MAX           : 16
 * DEFAULT       : { 0X02, 0X04, 0X0B, 0X0C, 0X12, 0X16, 0X18, 0X24, 0X30, 0X48, 0X60, 0X6C }
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Defines the
 *  supported non-HT data rates. It is encoded as N+1 octets where the first
 *  octet is N and the subsequent octets each describe a single supported
 *  rate.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPORTED_DATA_RATES 0x07F9

/*******************************************************************************
 * NAME          : UnifiRadioMeasurementActivated
 * PSID          : 2043 (0x07FB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  When TRUE Radio Measurements are supported.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_MEASUREMENT_ACTIVATED 0x07FB

/*******************************************************************************
 * NAME          : UnifiRadioMeasurementCapabilities
 * PSID          : 2044 (0x07FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 5
 * MAX           : 5
 * DEFAULT       : { 0X73, 0X00, 0X01, 0X00, 0X04 }
 * DESCRIPTION   :
 *  RM Enabled capabilities of the chip. See SC-503520-SP for further
 *  details.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_MEASUREMENT_CAPABILITIES 0x07FC

/*******************************************************************************
 * NAME          : UnifiVhtActivated
 * PSID          : 2045 (0x07FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate VHT mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VHT_ACTIVATED 0x07FD

/*******************************************************************************
 * NAME          : UnifiHtActivated
 * PSID          : 2046 (0x07FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate HT mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_ACTIVATED 0x07FE

/*******************************************************************************
 * NAME          : UnifiEnableTwoSimultaneousPassiveScansSameBand
 * PSID          : 2047 (0x07FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable two passive scans to be simultaneously scheduled on two distinct
 *  channels at the same.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_TWO_SIMULTANEOUS_PASSIVE_SCANS_SAME_BAND 0x07FF

/*******************************************************************************
 * NAME          : UnifiPlM689WorkaroundTriggerArpCount
 * PSID          : 2048 (0x0800)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PL_M689_WORKAROUND_TRIGGER_ARP_COUNT 0x0800

/*******************************************************************************
 * NAME          : UnifiRoamingActivated
 * PSID          : 2049 (0x0801)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Roaming functionality
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_ACTIVATED 0x0801

/*******************************************************************************
 * NAME          : UnifiRoamRssiScanTrigger
 * PSID          : 2050 (0x0802)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dbm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  The RSSI value, in dBm, below which roaming scan shall start.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSI_SCAN_TRIGGER 0x0802

/*******************************************************************************
 * NAME          : UnifiRoamCuScanTriggerHysteresis
 * PSID          : 2051 (0x0803)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Hysteresis value that subtracted from unifiRoamCUScanTrigger will result
 *  in the CU value that will stop eventual CU scans. This is an SCSC
 *  addition to ensure scans are not started/ stopped in an excessive manner.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_SCAN_TRIGGER_HYSTERESIS 0x0803

/*******************************************************************************
 * NAME          : UnifiRoamBackgroundScanPeriod
 * PSID          : 2052 (0x0804)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 120000000
 * DESCRIPTION   :
 *  The background scan period for cached channels.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BACKGROUND_SCAN_PERIOD 0x0804

/*******************************************************************************
 * NAME          : UnifiRoamNchoFullScanPeriod
 * PSID          : 2053 (0x0805)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 120000000
 * DESCRIPTION   :
 *  NCHO: Specifies the period at which the full channel scan shall be run.
 *  For certification and Host use only.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_FULL_SCAN_PERIOD 0x0805

/*******************************************************************************
 * NAME          : UnifiRoamSoftRoamingEnabled
 * PSID          : 2054 (0x0806)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SOFT_ROAMING_ENABLED 0x0806

/*******************************************************************************
 * NAME          : UnifiRoamScanBand
 * PSID          : 2055 (0x0807)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 2
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Indicates whether only intra-band or all-band should be used for roaming
 *  scan. 2 - Roaming across band 1 - Roaming within band
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_BAND 0x0807

/*******************************************************************************
 * NAME          : UnifiRoamCuRssiScanTriggerHysteresis
 * PSID          : 2056 (0x0808)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dbm
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Hysteresis value that added to unifiRoamCURssiScanTrigger will result in
 *  the RSSI value that will stop eventual CU scans. This is an SCSC addition
 *  to ensure scans are not started/ stopped in an excessive manner.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_RSSI_SCAN_TRIGGER_HYSTERESIS 0x0808

/*******************************************************************************
 * NAME          : UnifiRoamNchoScanMaxActiveChannelTime
 * PSID          : 2057 (0x0809)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  NCHO: Specifies the maximum time spent scanning a channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_SCAN_MAX_ACTIVE_CHANNEL_TIME 0x0809

/*******************************************************************************
 * NAME          : UnifiRoamReScanPeriod
 * PSID          : 2058 (0x080A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 10000000
 * DESCRIPTION   :
 *  The scan period for re-scanning cached channels.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RE_SCAN_PERIOD 0x080A

/*******************************************************************************
 * NAME          : UnifiRoamInactiveScanPeriod
 * PSID          : 2059 (0x080B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 10000000
 * DESCRIPTION   :
 *  The scan period for the inactive scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_INACTIVE_SCAN_PERIOD 0x080B

/*******************************************************************************
 * NAME          : UnifiRoamMode
 * PSID          : 2060 (0x080C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Enable/Disable host resume when roaming. 0: Wake up the host all the
 *  time. 1: Only wakeup the host if the AP is not white-listed. 2: Don't
 *  wake up the host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_MODE 0x080C

/*******************************************************************************
 * NAME          : UnifiRoamRssiScanTriggerReset
 * PSID          : 2061 (0x080D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dbm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -70
 * DESCRIPTION   :
 *  The current channel Averaged RSSI value above which a RSSI triggered
 *  roaming scan shall stop, and the RSSI trigger shall be reset at its
 *  initial value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSI_SCAN_TRIGGER_RESET 0x080D

/*******************************************************************************
 * NAME          : UnifiRoamRssiScanTriggerStep
 * PSID          : 2062 (0x080E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dbm
 * MIN           : 0
 * MAX           : 10
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  The delta to apply to unifiRoamRssiScanTrigger when no eligible candidate
 *  is found.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSI_SCAN_TRIGGER_STEP 0x080E

/*******************************************************************************
 * NAME          : UnifiRoamIdleVariationRssi
 * PSID          : 2063 (0x080F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dBm
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Defines the maximum RSSI variation of the current link allowed during
 *  idle roam. This parameter ensures the device is stationary.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_VARIATION_RSSI 0x080F

/*******************************************************************************
 * NAME          : UnifiRoamIdleMinRssi
 * PSID          : 2064 (0x0810)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -127
 * MAX           : 0
 * DEFAULT       : -60
 * DESCRIPTION   :
 *  Defines the minimum RSSI of the current link required to perform an idle
 *  roam.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_MIN_RSSI 0x0810

/*******************************************************************************
 * NAME          : UnifiRoamEapTimeout
 * PSID          : 2065 (0x0811)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  Timeout, in ms, for receiving the first EAP/EAPOL frame from the AP
 *  during roaming
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_EAP_TIMEOUT 0x0811

/*******************************************************************************
 * NAME          : UnifiRoamIdleInactiveTime
 * PSID          : 2066 (0x0812)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Defines the time interval during which the link needs to remain idle to
 *  perform an idle roam.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_INACTIVE_TIME 0x0812

/*******************************************************************************
 * NAME          : UnifiRoamNchoScanControl
 * PSID          : 2067 (0x0813)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  NCHO: Indicates which control path shall be evaluated in order to
 *  determine which channels should be scanned for NCHO scans.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_SCAN_CONTROL 0x0813

/*******************************************************************************
 * NAME          : UnifiRoamNchoDfsScanMode
 * PSID          : 2068 (0x0814)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  NCHO: Specifies how DFS channels should be scanned for roaming. For
 *  certification and Host use only.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_DFS_SCAN_MODE 0x0814

/*******************************************************************************
 * NAME          : UnifiRoamNchoScanHomeTime
 * PSID          : 2069 (0x0815)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 40
 * MAX           : 65535
 * DEFAULT       : 45
 * DESCRIPTION   :
 *  NCHO: The time, in TU, to spend NOT scanning during a HomeAway scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_SCAN_HOME_TIME 0x0815

/*******************************************************************************
 * NAME          : UnifiRoamNchoScanHomeAwayTime
 * PSID          : 2070 (0x0816)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 40
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  NCHO: The time, in TU, to spend scanning during a HomeAway scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_SCAN_HOME_AWAY_TIME 0x0816

/*******************************************************************************
 * NAME          : UnifiRoamIdleInactiveCount
 * PSID          : 2071 (0x0817)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : number_of_packets
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  The number of packets over which the link is considered not idle.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_INACTIVE_COUNT 0x0817

/*******************************************************************************
 * NAME          : UnifiRoamNchoScanNProbe
 * PSID          : 2072 (0x0818)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  NCHO: The number of ProbeRequest frames per channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_SCAN_NPROBE 0x0818

/*******************************************************************************
 * NAME          : UnifiRoamIdleBand
 * PSID          : 2073 (0x0819)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 3
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Specifies when connected on which band to run the Idle Roaming algorithm.
 *  0 for None. 1 for 2.4GHz. 2 for 5GHz. 3 for All.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_BAND 0x0819

/*******************************************************************************
 * NAME          : UnifiRoamIdleApSelectDeltaFactor
 * PSID          : 2074 (0x081A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Delta value applied to the score of the currently connected AP to
 *  determine candidates' eligibility threshold for Idle period triggered
 *  roaming scans.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_IDLE_AP_SELECT_DELTA_FACTOR 0x081A

/*******************************************************************************
 * NAME          : UnifiRoamNchoRssiDelta
 * PSID          : 2075 (0x081B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dBm
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  NCHO: Specifies the RSSI delta at a potential candidate is deemed
 *  eligible.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_RSSI_DELTA 0x081B

/*******************************************************************************
 * NAME          : UnifiApOlbcDuration
 * PSID          : 2076 (0x081C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  How long, in milliseconds, the AP enables reception of BEACON frames to
 *  perform Overlapping Legacy BSS Condition(OLBC). If set to 0 then OLBC is
 *  disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_OLBC_DURATION 0x081C

/*******************************************************************************
 * NAME          : UnifiApOlbcInterval
 * PSID          : 2077 (0x081D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2000
 * DESCRIPTION   :
 *  How long, in milliseconds, between periods of receiving BEACON frames to
 *  perform Overlapping Legacy BSS Condition(OLBC). This value MUST exceed
 *  the OBLC duration MIB unifiApOlbcDuration. If set to 0 then OLBC is
 *  disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_OLBC_INTERVAL 0x081D

/*******************************************************************************
 * NAME          : UnifiDnsSupportActivated
 * PSID          : 2078 (0x081E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This MIB activates support for transmitting DNS frame via MLME.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DNS_SUPPORT_ACTIVATED 0x081E

/*******************************************************************************
 * NAME          : UnifiOffchannelProcedureTimeout
 * PSID          : 2079 (0x081F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Maximum timeout in ms the Offchannel FSM will wait until the procedure is
 *  completed
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OFFCHANNEL_PROCEDURE_TIMEOUT 0x081F

/*******************************************************************************
 * NAME          : UnifiFrameResponseTimeout
 * PSID          : 2080 (0x0820)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 500
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  Timeout, in TU, to wait for a frame(Auth, Assoc, ReAssoc) after TX Cfm
 *  trasnmission_status == Successful.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_TIMEOUT 0x0820

/*******************************************************************************
 * NAME          : UnifiConnectionFailureTimeout
 * PSID          : 2081 (0x0821)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 20000
 * DEFAULT       : 2000
 * DESCRIPTION   :
 *  Timeout, in TU, for a frame retry before giving up.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONNECTION_FAILURE_TIMEOUT 0x0821

/*******************************************************************************
 * NAME          : UnifiConnectingProbeTimeout
 * PSID          : 2082 (0x0822)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  How long, in TU, to wait for a ProbeRsp when syncronising before
 *  resending a ProbeReq
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONNECTING_PROBE_TIMEOUT 0x0822

/*******************************************************************************
 * NAME          : UnifiDisconnectTimeout
 * PSID          : 2083 (0x0823)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 3000
 * DEFAULT       : 1500
 * DESCRIPTION   :
 *  Timeout, in milliseconds, to perform a disconnect or disconnect all STAs
 *  (triggered by MLME_DISCONNECT-REQ or MLME_DISCONNECT-REQ
 *  00:00:00:00:00:00) before responding with MLME-DISCONNECT-IND and
 *  aborting the disconnection attempt. This is particulary important when a
 *  SoftAP is attempting to disconnect associated stations which might have
 *  "silently" left the ESS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISCONNECT_TIMEOUT 0x0823

/*******************************************************************************
 * NAME          : UnifiFrameResponseCfmTxLifetimeTimeout
 * PSID          : 2084 (0x0824)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_CFM_TX_LIFETIME_TIMEOUT 0x0824

/*******************************************************************************
 * NAME          : UnifiFrameResponseCfmFailureTimeout
 * PSID          : 2085 (0x0825)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  Timeout, in TU, to wait to retry a frame after TX Cfm trasnmission_status
 *  != Successful.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_CFM_FAILURE_TIMEOUT 0x0825

/*******************************************************************************
 * NAME          : UnifiForceActiveDuration
 * PSID          : 2086 (0x0826)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 1000
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  How long, in milliseconds, the firmware temporarily extends PowerSave for
 *  STA as a workaround for wonky APs such as D-link.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_ACTIVE_DURATION 0x0826

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaxNumberOfProbeSets
 * PSID          : 2087 (0x0827)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAX_NUMBER_OF_PROBE_SETS 0x0827

/*******************************************************************************
 * NAME          : UnifiMlmeScanStopIfLessThanXFrames
 * PSID          : 2088 (0x0828)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Stop scanning on a channel if less than X Beacons or Probe Responses are
 *  received.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_STOP_IF_LESS_THAN_XFRAMES 0x0828

/*******************************************************************************
 * NAME          : UnifiApAssociationTimeout
 * PSID          : 2089 (0x0829)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2000
 * DESCRIPTION   :
 *  SoftAP: Permitted time for a station to complete associatation with FW
 *  acting as AP in milliseconds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_ASSOCIATION_TIMEOUT 0x0829

/*******************************************************************************
 * NAME          : UnifiRoamingInP2pActivated
 * PSID          : 2090 (0x082A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Roaming in P2P.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_IN_P2P_ACTIVATED 0x082A

/*******************************************************************************
 * NAME          : UnifiHostNumAntennaControlActivated
 * PSID          : 2091 (0x082B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Host has a control of number of antenna to use
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HOST_NUM_ANTENNA_CONTROL_ACTIVATED 0x082B

/*******************************************************************************
 * NAME          : UnifiRoamNchoRssiTrigger
 * PSID          : 2092 (0x082C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  The current channel Averaged RSSI value below which a NCHO scan shall
 *  start.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_RSSI_TRIGGER 0x082C

/*******************************************************************************
 * NAME          : UnifiRoamNchoRssiTriggerHysteresis
 * PSID          : 2093 (0x082D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dBm
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Hysteresis value that added to unifiRoamNchoRssiTrigger will result in
 *  the RSSI value that will stop eventual NCHO scans.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_RSSI_TRIGGER_HYSTERESIS 0x082D

/*******************************************************************************
 * NAME          : UnifiPeerBandwidth
 * PSID          : 2094 (0x082E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The bandwidth used with peer station prior it disconnects
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_BANDWIDTH 0x082E

/*******************************************************************************
 * NAME          : UnifiCurrentPeerNss
 * PSID          : 2095 (0x082F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of spatial streams used with peer station prior it
 *  disconnects: BIG DATA
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_PEER_NSS 0x082F

/*******************************************************************************
 * NAME          : UnifiPeerTxDataRate
 * PSID          : 2096 (0x0830)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The tx rate that was used for transmissions prior disconnection
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_TX_DATA_RATE 0x0830

/*******************************************************************************
 * NAME          : UnifiPeerRssi
 * PSID          : 2097 (0x0831)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  The recorded RSSI from peer station prior it disconnects
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_RSSI 0x0831

/*******************************************************************************
 * NAME          : UnifiMlmeStationInactivityTimeout
 * PSID          : 2098 (0x0832)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Timeout, in seconds, for instigating ConnectonFailure procedures. Setting
 *  it to less than 3 seconds may result in frequent disconnection or roaming
 *  with the AP. Disable with Zero. Values lower than
 *  INACTIVITY_MINIMUM_TIMEOUT becomes INACTIVITY_MINIMUM_TIMEOUT. This value
 *  is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_STATION_INACTIVITY_TIMEOUT 0x0832

/*******************************************************************************
 * NAME          : UnifiMlmeCliInactivityTimeout
 * PSID          : 2099 (0x0833)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Timeout, in seconds, for instigating ConnectonFailure procedures. Zero
 *  value disables the feature. Any value written lower than
 *  INACTIVITY_MINIMUM_TIMEOUT becomes INACTIVITY_MINIMUM_TIMEOUT. This value
 *  is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_CLI_INACTIVITY_TIMEOUT 0x0833

/*******************************************************************************
 * NAME          : UnifiMlmeStationInitialKickTimeout
 * PSID          : 2100 (0x0834)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Timeout, in
 *  milliseconds, for sending the AP a NULL frame to kick off the EAPOL
 *  exchange. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_STATION_INITIAL_KICK_TIMEOUT 0x0834

/*******************************************************************************
 * NAME          : UnifiFrameResponseLongTimeout
 * PSID          : 2101 (0x0835)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 10000
 * DEFAULT       : 5000
 * DESCRIPTION   :
 *  Deprecated. Timeout, in TU, to wait for a frame(Auth, Assoc, ReAssoc)
 *  after TX Cfm trasnmission_status == Successful for SAE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_LONG_TIMEOUT 0x0835

/*******************************************************************************
 * NAME          : UnifiInitialConnectionFailureTimeout
 * PSID          : 2102 (0x0836)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 20000
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  Timeout, in TU, for a frame retry before giving up for initial
 *  connection.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_INITIAL_CONNECTION_FAILURE_TIMEOUT 0x0836

/*******************************************************************************
 * NAME          : unifRetryAuthForSpecialAps
 * PSID          : 2103 (0x0837)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 20000
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  How long, in milliseconds, before ConnectionFailureTimeout to send
 *  authentication frame for special aps.
 *******************************************************************************/
#define SLSI_PSID_UNIF_RETRY_AUTH_FOR_SPECIAL_APS 0x0837

/*******************************************************************************
 * NAME          : UnifiAssocComebackTimeout
 * PSID          : 2104 (0x0838)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 20000
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  Timeout, in TU, for a association comeback limit before giving up.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ASSOC_COMEBACK_TIMEOUT 0x0838

/*******************************************************************************
 * NAME          : UnifiUartConfigure
 * PSID          : 2110 (0x083E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  UART configuration using the values of the other unifiUart* attributes.
 *  The value supplied for this attribute is ignored.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UART_CONFIGURE 0x083E

/*******************************************************************************
 * NAME          : UnifiUartPios
 * PSID          : 2111 (0x083F)
 * PER INTERFACE?: NO
 * TYPE          : unifiUartPios
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specification of which PIOs should be connected to the UART. Currently
 *  defined values are: 1 - UART not used; all PIOs are available for other
 *  uses. 2 - Data transmit and receive connected to PIO[12] and PIO[14]
 *  respectively. No hardware handshaking lines. 3 - Data and handshaking
 *  lines connected to PIO[12:15].
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UART_PIOS 0x083F

/*******************************************************************************
 * NAME          : UnifiCrystalFrequencyTrim
 * PSID          : 2141 (0x085D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 63
 * DEFAULT       : 31
 * DESCRIPTION   :
 *  The IEEE 802.11 standard requires a frequency accuracy of either +/- 20
 *  ppm or +/- 25 ppm depending on the physical layer being used. If
 *  UniFi&apos;s frequency reference is a crystal then this attribute should
 *  be used to tweak the oscillating frequency to compensate for design- or
 *  device-specific variations. Each step change trims the frequency by
 *  approximately 2 ppm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CRYSTAL_FREQUENCY_TRIM 0x085D

/*******************************************************************************
 * NAME          : UnifiEnableDorm
 * PSID          : 2142 (0x085E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Enable Dorm
 *  (deep sleep). When disabled, WLAN will not switch the radio power domain
 *  on/off *and* it will always veto deep sleep. Setting the value to TRUE
 *  means dorm functionality will behave normally. The intention is *not* for
 *  this value to be changed at runtime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_DORM 0x085E

/*******************************************************************************
 * NAME          : UnifiExternalClockDetect
 * PSID          : 2146 (0x0862)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  If UniFi is running with an external fast clock source, i.e.
 *  unifiExternalFastClockRequest is set, it is common for this clock to be
 *  shared with other devices. Setting to true causes UniFi to detect when
 *  the clock is present (presumably in response to a request from another
 *  device), and to perform any pending activities at that time rather than
 *  requesting the clock again some time later. This is likely to reduce
 *  overall system power consumption by reducing the total time that the
 *  clock needs to be active.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_CLOCK_DETECT 0x0862

/*******************************************************************************
 * NAME          : UnifiExternalFastClockRequest
 * PSID          : 2149 (0x0865)
 * PER INTERFACE?: NO
 * TYPE          : unifiExternalFastClockRequest
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  It is possible to supply UniFi with an external fast reference clock, as
 *  an alternative to using a crystal. If such a clock is used then it is
 *  only required when UniFi is active. A signal can be output on PIO[2] or
 *  if the version of UniFi in use is the UF602x or later, any PIO may be
 *  used (see unifiExternalFastClockRequestPIO) to indicate when UniFi
 *  requires a fast clock. Setting makes this signal become active and
 *  determines the type of signal output. 0 - No clock request. 1 - Non
 *  inverted, totem pole. 2 - Inverted, totem pole. 3 - Open drain. 4 - Open
 *  source.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_FAST_CLOCK_REQUEST 0x0865

/*******************************************************************************
 * NAME          : UnifiWatchdogTimeout
 * PSID          : 2152 (0x0868)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 1500
 * DESCRIPTION   :
 *  Maximum time the background may be busy or locked out for. If this time
 *  is exceeded, UniFi will reset. If this key is set to 65535 then the
 *  watchdog will be disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WATCHDOG_TIMEOUT 0x0868

/*******************************************************************************
 * NAME          : UnifiScanParametersPublic
 * PSID          : 2153 (0x0869)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       :
 * DESCRIPTION   :
 *  Each row of the table contains 2 entries for a scan: first entry when
 *  there is 0 registered VIFs, second - when there is 1 or more registered
 *  VIFs. index2 = 1 octet 0 ~ 1 - Probe Interval in Time Units (uint16) /
 *  Zero VIF octet 2 ~ 3 - Probe Interval in Time Units (uint16) / Non-Zero
 *  VIF index2 = 2 octet 0 ~ 1 - Max Active Channel Time in Time Units
 *  (uint16) / Zero VIF octet 2 ~ 3 - Max Active Channel Time in Time Units
 *  (uint16) / Non-Zero VIF index2 = 3 octet 0 ~ 1 - Max Passive Channel Time
 *  in Time Units (uint16) / Zero VIF octet 2 ~ 3 - Max Passive Channel Time
 *  in Time Units (uint16) / Non-Zero VIF index2 = 4 octet 0 ~ 1 - Scan Flags
 *  (uint16) (see unifiScanFlags) / Zero VIF octet 2 ~ 3 - Scan Flags
 *  (uint16) (see unifiScanFlags) / Non-Zero VIF bit 0 - Enable Early Channel
 *  Exit (bool) bit 1 - Disable Scan (bool) bit 2 - Enable NCHO (bool) bit 3
 *  - Enable MAC Randomization (bool) bit 4 - Clear Scan Records cache
 *  between cycles (bool)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_PARAMETERS_PUBLIC 0x0869

/*******************************************************************************
 * NAME          : UnifiOverrideEdcaParamActivated
 * PSID          : 2155 (0x086B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Activate override of STA edca config parameters with
 *  unifiOverrideEDCAParam. default: True - for volcano, and False - for
 *  others
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_EDCA_PARAM_ACTIVATED 0x086B

/*******************************************************************************
 * NAME          : UnifiOverrideEdcaParam
 * PSID          : 2156 (0x086C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  EDCA Parameters to be used if unifiOverrideEDCAParamActivated is true,
 *  indexed by unifiAccessClassIndex octet 0 - AIFSN octet 1 - [7:4] ECW MAX
 *  [3:0] ECW MIN octet 2 ~ 3 - TXOP[7:0] TXOP[15:8] in 32 usec units for
 *  both non-HT and HT connections.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_EDCA_PARAM 0x086C

/*******************************************************************************
 * NAME          : UnifiScanParametersPrivate
 * PSID          : 2157 (0x086D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       :
 * DESCRIPTION   :
 *  Each row of the table contains 2 entries for a scan type: first entry
 *  when there is 0 registered VIFs, second - when there is 1 or more
 *  registered VIFs. index2 = 1 octet 0 - Scan Priority (uint8) / Zero VIF
 *  octet 1 - Scan Priority (uint8) / Non-Zero VIF index2 = 2 octet 0 - Scan
 *  Policy (uint8) / Zero VIF octet 1 - Scan Policy (uint8) / Non-Zero VIF
 *  index2 = 3 octet 0 - Scan Groups (uint8) (see unifiScanGroups) / Zero VIF
 *  octet 1 - Scan Groups (uint8) (see unifiScanGroups) / Non-Zero VIF bit 0
 *  - Roaming scan group (bool) bit 1 - Roaming scan on known channel group
 *  (bool) bit 2 - P2P scan group (bool) bit 3 - Wifi Sharing allowed group
 *  (bool) bit 4 - NAN scan group (bool)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_PARAMETERS_PRIVATE 0x086D

/*******************************************************************************
 * NAME          : UnifiExternalFastClockRequestPio
 * PSID          : 2158 (0x086E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 15
 * DEFAULT       : 9
 * DESCRIPTION   :
 *  If an external fast reference clock is being supplied to UniFi as an
 *  alternative to a crystal (see unifiExternalFastClockRequest) and the
 *  version of UniFi in use is the UF602x or later, any PIO may be used as
 *  the external fast clock request output from UniFi. key determines the PIO
 *  to use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_FAST_CLOCK_REQUEST_PIO 0x086E

/*******************************************************************************
 * NAME          : UnifiModemDesenseEnabled
 * PSID          : 2160 (0x0870)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable dynamic desensing of the CCK and OFDM modems based on RSSI for STA
 *  VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_ENABLED 0x0870

/*******************************************************************************
 * NAME          : UnifiModemDesenseOffset
 * PSID          : 2164 (0x0874)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  The offset value for the CCK and 2G4 OFDM modem desensing. When the CCK
 *  and/or 2G4 OFDM modems are desensed, the desense value will be set to the
 *  averaged STA-AP RSSI less this parameter value. Only used when
 *  unifiModemDesenseEnabled is set to true.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_OFFSET 0x0874

/*******************************************************************************
 * NAME          : UnifiModemDesenseCap
 * PSID          : 2165 (0x0875)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 0
 * DEFAULT       : -65
 * DESCRIPTION   :
 *  The upper limit for dynamic desensing of the CCK and 2G4 OFDM modem. The
 *  CCK and/or 2G4 OFDM modems will not be desensed to a value in excess of
 *  this parameter. Only used when unifiModemDesenseEnabled is set to true.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_CAP 0x0875

/*******************************************************************************
 * NAME          : UnifiModemDesenseOffset5g
 * PSID          : 2169 (0x0879)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  The offset value for the CCK and OFDM modem desensing. When the 5G OFDM
 *  modem is desensed, the desense value will be set to the averaged STA-AP
 *  RSSI less this parameter value. Only used when unifiModemDesenseEnabled
 *  is set to true.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_OFFSET5G 0x0879

/*******************************************************************************
 * NAME          : UnifiModemDesenseCap5g
 * PSID          : 2170 (0x087A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 0
 * DEFAULT       : -65
 * DESCRIPTION   :
 *  The upper limit for dynamic desensing of the CCK and OFDM modem. The 5G
 *  OFDM modem will not be desensed to a value in excess of this parameter.
 *  Only used when unifiModemDesenseEnabled is set to true.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_CAP5G 0x087A

/*******************************************************************************
 * NAME          : UnifiModemDesenseOption
 * PSID          : 2171 (0x087B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 7
 * DESCRIPTION   :
 *  Select which of the modems will be desensed. Only valid if
 *  unifiModemDesenseEnabled is set to true. Set the corresponding bit to
 *  enable 0: CCK desense 1: 2G4 OFDM desense 2: 5G OFDM desense Also see
 *  unifiDesenseSelection
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_DESENSE_OPTION 0x087B

/*******************************************************************************
 * NAME          : UnifiRssiThresholdForBbbSync
 * PSID          : 2172 (0x087C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 128
 * DESCRIPTION   :
 *  Below this (dBm) threshold, a different set of bbb_sync_ratio and
 *  bbb_num_syms values are used for modem desense by default it is disabled
 *  by keeping a very high value of 128dbm
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_THRESHOLD_FOR_BBB_SYNC 0x087C

/*******************************************************************************
 * NAME          : UnifiRssiTypeToUse
 * PSID          : 2193 (0x0891)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Define which type of RSSI averaging will be used internally, e.g. for
 *  determining roaming thresholds. If the value is set to 0, then the
 *  average RSSI for all frame types will be used. If the value is set to 1,
 *  then the maximum RSSI between the beacon-only and non-beacon-only
 *  averages will be used. If the value is set to 2, then the average RSSI
 *  for only beacon frame types will be used. Only used for VIFs of type STA.
 *  Also see unifiRSSIAvgType.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_TYPE_TO_USE 0x0891

/*******************************************************************************
 * NAME          : UnifiNumRssiAverageSample
 * PSID          : 2194 (0x0892)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 16
 * DESCRIPTION   :
 *  The amount of samples used for calculating the average RSSI value.
 *  WARNING: Changing this value after system start-up will have no effect.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NUM_RSSI_AVERAGE_SAMPLE 0x0892

/*******************************************************************************
 * NAME          : UnifiRxDataRate
 * PSID          : 2196 (0x0894)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The bit rate of the last received frame on this VIF.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_DATA_RATE 0x0894

/*******************************************************************************
 * NAME          : UnifiPeerRxRetryCount
 * PSID          : 2198 (0x0896)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of retry packets from peer station
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_RX_RETRY_COUNT 0x0896

/*******************************************************************************
 * NAME          : UnifiPeerRxMulticastCount
 * PSID          : 2199 (0x0897)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of multicast and broadcast packets received from peer station
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_RX_MULTICAST_COUNT 0x0897

/*******************************************************************************
 * NAME          : UnifiRssi
 * PSID          : 2200 (0x0898)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Running average of the Received Signal Strength Indication (RSSI) for
 *  packets received by UniFi&apos;s radio. The value should only be treated
 *  as an indication of the signal strength; it is not an accurate
 *  measurement. The result is only meaningful if the unifiRxExternalGain
 *  attribute is set to the correct calibration value. If UniFi is part of a
 *  BSS, only frames originating from devices in the BSS are reported (so far
 *  as this can be determined). The average is reset when UniFi joins or
 *  starts a BSS or is reset.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI 0x0898

/*******************************************************************************
 * NAME          : UnifiLastBssRssi
 * PSID          : 2201 (0x0899)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Last BSS RSSI. See unifiRSSI description.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAST_BSS_RSSI 0x0899

/*******************************************************************************
 * NAME          : UnifiSnr
 * PSID          : 2202 (0x089A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Provides a running average of the Signal to Noise Ratio (dB) for packets
 *  received by UniFi&apos;s radio.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SNR 0x089A

/*******************************************************************************
 * NAME          : UnifiLastBssSnr
 * PSID          : 2203 (0x089B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Last BSS SNR. See unifiSNR description.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAST_BSS_SNR 0x089B

/*******************************************************************************
 * NAME          : UnifiSwTxTimeout
 * PSID          : 2204 (0x089C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : second
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Maximum time in seconds for a frame to be queued in firmware, ready to be
 *  sent, but not yet actually pumped to hardware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SW_TX_TIMEOUT 0x089C

/*******************************************************************************
 * NAME          : UnifiHwTxTimeout
 * PSID          : 2205 (0x089D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 512
 * DESCRIPTION   :
 *  Maximum time in milliseconds for a frame to be queued in the
 *  hardware/DPIF.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HW_TX_TIMEOUT 0x089D

/*******************************************************************************
 * NAME          : UnifiRateStatsRxSuccessCount
 * PSID          : 2206 (0x089E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of successful receptions of complete management and data
 *  frames at the rate indexed by unifiRateStatsIndex.This number will wrap
 *  to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_RX_SUCCESS_COUNT 0x089E

/*******************************************************************************
 * NAME          : UnifiRateStatsTxSuccessCount
 * PSID          : 2207 (0x089F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of successful (acknowledged) unicast transmissions of complete
 *  data or management frames the rate indexed by unifiRateStatsIndex. This
 *  number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_TX_SUCCESS_COUNT 0x089F

/*******************************************************************************
 * NAME          : UnifiTxDataRate
 * PSID          : 2208 (0x08A0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The bit rate currently in use for transmissions of unicast data frames;
 *  On an infrastructure BSS, this is the data rate used in communicating
 *  with the associated access point, if there is none, an error is returned
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DATA_RATE 0x08A0

/*******************************************************************************
 * NAME          : UnifiSnrExtraOffsetCck
 * PSID          : 2209 (0x08A1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dB
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  This offset is added to SNR values received at 802.11b data rates. This
 *  accounts for differences in the RF pathway between 802.11b and 802.11g
 *  demodulators. The offset applies to values of unifiSNR as well as SNR
 *  values in scan indications. Not used in 5GHz mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SNR_EXTRA_OFFSET_CCK 0x08A1

/*******************************************************************************
 * NAME          : UnifiRssiMaxAveragingPeriod
 * PSID          : 2210 (0x08A2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  Limits the period over which the value of unifiRSSI is averaged. If no
 *  more than unifiRSSIMinReceivedFrames frames have been received in the
 *  period, then the value of unifiRSSI is reset to the value of the next
 *  measurement and the rolling average is restarted. This ensures that the
 *  value is timely (although possibly poorly averaged) when little data is
 *  being received.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_MAX_AVERAGING_PERIOD 0x08A2

/*******************************************************************************
 * NAME          : UnifiRssiMinReceivedFrames
 * PSID          : 2211 (0x08A3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  See the description of unifiRSSIMaxAveragingPeriod for how the
 *  combination of attributes is used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_MIN_RECEIVED_FRAMES 0x08A3

/*******************************************************************************
 * NAME          : UnifiRateStatsRate
 * PSID          : 2212 (0x08A4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : 500 kbps
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The rate corresponding to the current table entry. The value is rounded
 *  to the nearest number of units where necessary. Most rates do not require
 *  rounding, but when short guard interval is in effect the rates are no
 *  longer multiples of the base unit. Note that there may be two occurrences
 *  of the value 130: the first corresponds to MCS index 7, and the second,
 *  if present, to MCS index 6 with short guard interval.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_RATE 0x08A4

/*******************************************************************************
 * NAME          : UnifiLastBssTxDataRate
 * PSID          : 2213 (0x08A5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Last BSS Tx DataRate. See unifiTxDataRate description.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAST_BSS_TX_DATA_RATE 0x08A5

/*******************************************************************************
 * NAME          : UnifiDiscardedFrameCount
 * PSID          : 2214 (0x08A6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This is a counter that indicates the number of data and management frames
 *  that have been processed by the UniFi hardware but were discarded before
 *  being processed by the firmware. It does not include frames not processed
 *  by the hardware because they were not addressed to the local device, nor
 *  does it include frames discarded by the firmware in the course of normal
 *  MAC processing (which include, for example, frames in an appropriate
 *  encryption state and multicast frames not requested by the host).
 *  Typically this counter indicates lost data frames for which there was no
 *  buffer space; however, other cases may cause the counter to increment,
 *  such as receiving a retransmitted frame that was already successfully
 *  processed. Hence this counter should not be treated as a reliable guide
 *  to lost frames. The counter wraps to 0 after 65535.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISCARDED_FRAME_COUNT 0x08A6

/*******************************************************************************
 * NAME          : UnifiMacrameDebugStats
 * PSID          : 2215 (0x08A7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  MACRAME debug stats readout key. Use set to write a debug readout, then
 *  read the same key to get the actual readout.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MACRAME_DEBUG_STATS 0x08A7

/*******************************************************************************
 * NAME          : UnifiCurrentTsfTime
 * PSID          : 2218 (0x08AA)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : -9223372036854775808
 * MAX           : 9223372036854775807
 * DEFAULT       :
 * DESCRIPTION   :
 *  Get TSF time (last 32 bits) for the specified VIF. VIF index can't be 0
 *  as that is treated as global VIF For station VIF - Correct BSS TSF wil
 *  only be reported after MLME-CONNECT.indication(success) indication to
 *  host. Note that if MAC Hardware is switched off then TSF returned is
 *  estimated value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_TSF_TIME 0x08AA

/*******************************************************************************
 * NAME          : UnifiBaRxEnableTid
 * PSID          : 2219 (0x08AB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X1555
 * DESCRIPTION   :
 *  Configure Block Ack RX on a per-TID basis. Bit mask is two bits per TID
 *  (B1 = Not Used, B0 = enable).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_RX_ENABLE_TID 0x08AB

/*******************************************************************************
 * NAME          : UnifiBaTxEnableTid
 * PSID          : 2221 (0x08AD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X0557
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Configure
 *  Block Ack TX on a per-TID basis. Bit mask is two bits per TID (B1 =
 *  autosetup, B0 = enable).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_TX_ENABLE_TID 0x08AD

/*******************************************************************************
 * NAME          : UnifiTrafficThresholdToSetupBa
 * PSID          : 2222 (0x08AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Sets the default Threshold (as packet count) to setup BA agreement per
 *  TID.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TRAFFIC_THRESHOLD_TO_SETUP_BA 0x08AE

/*******************************************************************************
 * NAME          : UnifiDplaneTxAmsduHwCapability
 * PSID          : 2223 (0x08AF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Returns 0 if A-MSDU size limited to 4K. Returns 1 is A-MSDU size is
 *  limited to 8K. This value is chip specific and limited by HW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TX_AMSDU_HW_CAPABILITY 0x08AF

/*******************************************************************************
 * NAME          : UnifiDplaneTxAmsduSubframeCountMax
 * PSID          : 2224 (0x08B0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 4
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Defines the maximum number of A-MSDU sub-frames per A-MSDU. A value of 1
 *  indicates A-MSDU aggregation has been disabled
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TX_AMSDU_SUBFRAME_COUNT_MAX 0x08B0

/*******************************************************************************
 * NAME          : UnifiBaConfig
 * PSID          : 2225 (0x08B1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Block Ack Configuration. It is composed of A-MSDU supported, TX MPDU per
 *  A-MPDU, RX Buffer size, TX Buffer size and Block Ack Timeout. Indexed by
 *  unifiAccessClassIndex.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_CONFIG 0x08B1

/*******************************************************************************
 * NAME          : UnifiBaTxMaxNumber
 * PSID          : 2226 (0x08B2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X10
 * DESCRIPTION   :
 *  Block Ack Configuration. Maximum number of BAs. Limited by HW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_TX_MAX_NUMBER 0x08B2

/*******************************************************************************
 * NAME          : UnifiMoveBKtoBe
 * PSID          : 2227 (0x08B3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated. Golden Certification MIB don't delete, change PSID or name
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MOVE_BKTO_BE 0x08B3

/*******************************************************************************
 * NAME          : UnifiBeaconReceived
 * PSID          : 2228 (0x08B4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Access point beacon received count from connected AP
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACON_RECEIVED 0x08B4

/*******************************************************************************
 * NAME          : UnifiAcRetries
 * PSID          : 2229 (0x08B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  It represents the number of retransmitted frames under each ac priority
 *  (indexed by unifiAccessClassIndex). This number will wrap to zero after
 *  the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AC_RETRIES 0x08B5

/*******************************************************************************
 * NAME          : UnifiRadioOnTime
 * PSID          : 2230 (0x08B6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is awake (32 bits number accruing over time). On
 *  multi-radio platforms an index to the radio instance is required
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_ON_TIME 0x08B6

/*******************************************************************************
 * NAME          : UnifiRadioTxTime
 * PSID          : 2231 (0x08B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is transmitting (32 bits number accruing over time). On
 *  multi-radio platforms an index to the radio instance is required
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_TIME 0x08B7

/*******************************************************************************
 * NAME          : UnifiRadioRxTime
 * PSID          : 2232 (0x08B8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is in active receive (32 bits number accruing over time).
 *  On multi-radio platforms an index to the radio instance is required
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_RX_TIME 0x08B8

/*******************************************************************************
 * NAME          : UnifiRadioScanTime
 * PSID          : 2233 (0x08B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is awake due to all scan (32 bits number accruing over
 *  time). On multi-radio platforms an index to the radio instance is
 *  required
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_SCAN_TIME 0x08B9

/*******************************************************************************
 * NAME          : UnifiPsLeakyAp
 * PSID          : 2234 (0x08BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  indicate that this AP typically leaks packets beyond the guard time
 *  (5msecs).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PS_LEAKY_AP 0x08BA

/*******************************************************************************
 * NAME          : UnifiTqamActivated
 * PSID          : 2235 (0x08BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Activate Vendor VHT IE for 256-QAM mode on 2.4GHz.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TQAM_ACTIVATED 0x08BB

/*******************************************************************************
 * NAME          : UnifiRadioOnTimeNan
 * PSID          : 2236 (0x08BC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is awake due to NAN operations (32 bits number accruing
 *  over time). On multi-radio platforms an index to the radio instance is
 *  required
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_ON_TIME_NAN 0x08BC

/*******************************************************************************
 * NAME          : UnifiPhyEventLogEnable
 * PSID          : 2238 (0x08BE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Enable or Disable PHY Event Log - Disable: zero, Enable: None zero
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PHY_EVENT_LOG_ENABLE 0x08BE

/*******************************************************************************
 * NAME          : UnifiOutputRadioInfoToKernelLog
 * PSID          : 2239 (0x08BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Print messages about the radio status to the Android Kernel Log. See
 *  document SC-508266-TC.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OUTPUT_RADIO_INFO_TO_KERNEL_LOG 0x08BF

/*******************************************************************************
 * NAME          : UnifiNoAckActivationCount
 * PSID          : 2240 (0x08C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of frames that are discarded due to HW No-ack activated during
 *  test. This number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_ACK_ACTIVATION_COUNT 0x08C0

/*******************************************************************************
 * NAME          : UnifiRxFcsErrorCount
 * PSID          : 2241 (0x08C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received frames that are discarded due to bad FCS (CRC).
 *  This number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_FCS_ERROR_COUNT 0x08C1

/*******************************************************************************
 * NAME          : UnifiBeaconsReceivedPercentage
 * PSID          : 2245 (0x08C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Percentage of beacons received, calculated as received / expected. The
 *  percentage is scaled to an integer value between 0 (0%) and 1000 (100%).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACONS_RECEIVED_PERCENTAGE 0x08C5

/*******************************************************************************
 * NAME          : UnifiArpDetectActivated
 * PSID          : 2246 (0x08C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Activate feature support for Enhanced ARP Detect. This is required by
 *  Volcano.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ARP_DETECT_ACTIVATED 0x08C6

/*******************************************************************************
 * NAME          : UnifiArpDetectResponseCounter
 * PSID          : 2247 (0x08C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counter used to track ARP Response frame for Enhanced ARP Detect. This is
 *  required by Volcano.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ARP_DETECT_RESPONSE_COUNTER 0x08C7

/*******************************************************************************
 * NAME          : UnifiReadTxPacketStatsFromStaVif
 * PSID          : 2248 (0x08C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Read TX packet stats from VIF to avoid resetting of the counters during
 *  roaming - this is a Volcano requirement
 *******************************************************************************/
#define SLSI_PSID_UNIFI_READ_TX_PACKET_STATS_FROM_STA_VIF 0x08C8

/*******************************************************************************
 * NAME          : UnifiEnableMgmtTxPacketStats
 * PSID          : 2249 (0x08C9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Consider management packets for TX stats counters
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_MGMT_TX_PACKET_STATS 0x08C9

/*******************************************************************************
 * NAME          : UnifiSwToHwQueueStats
 * PSID          : 2250 (0x08CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The timing statistics of packets being queued between SW-HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SW_TO_HW_QUEUE_STATS 0x08CA

/*******************************************************************************
 * NAME          : UnifiHostToSwQueueStats
 * PSID          : 2251 (0x08CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The timing statistics of packets being queued between HOST-SW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HOST_TO_SW_QUEUE_STATS 0x08CB

/*******************************************************************************
 * NAME          : UnifiQueueStatsEnable
 * PSID          : 2252 (0x08CC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables recording timing statistics of packets being queued between
 *  HOST-SW-HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_QUEUE_STATS_ENABLE 0x08CC

/*******************************************************************************
 * NAME          : UnifiTxDataConfirm
 * PSID          : 2253 (0x08CD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Allows to request on a per access class basis that an MA_UNITDATA.confirm
 *  be generated after each packet transfer. The default value is applied for
 *  all ACs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DATA_CONFIRM 0x08CD

/*******************************************************************************
 * NAME          : UnifiThroughputDebug
 * PSID          : 2254 (0x08CE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  is used to access throughput related counters that can help diagnose
 *  throughput problems. The index of the MIB will access different counters,
 *  as described in SC-506328-DD. Setting any index for a VIF to any value,
 *  clears all DPLP debug stats for the MAC instance used by the VIF. This is
 *  useful mainly for debugging LAA or small scale throughput issues that
 *  require short term collection of the statistics.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_THROUGHPUT_DEBUG 0x08CE

/*******************************************************************************
 * NAME          : UnifiLoadDpdLut
 * PSID          : 2255 (0x08CF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 147
 * MAX           : 147
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a static DPD LUT to the FW, read DPD LUT from hardware
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOAD_DPD_LUT 0x08CF

/*******************************************************************************
 * NAME          : UnifiDpdMasterSwitch
 * PSID          : 2256 (0x08D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enables Digital Pre-Distortion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_MASTER_SWITCH 0x08D0

/*******************************************************************************
 * NAME          : UnifiDpdPredistortGains
 * PSID          : 2257 (0x08D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 14
 * MAX           : 14
 * DEFAULT       :
 * DESCRIPTION   :
 *  DPD pre-distort gains. Takes a range of frequencies, where f_min &lt;=
 *  f_channel &lt; f_max. The format is [freq_min_msb, freq_min_lsb,
 *  freq_max_msb, freq_max_lsb, DPD policy bitmap, bandwidth_bitmap,
 *  power_trim_enable, OFDM0_gain, OFDM1_gain, CCK_gain, TR_gain, CCK PSAT
 *  gain, OFDM PSAT gain].
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_PREDISTORT_GAINS 0x08D1

/*******************************************************************************
 * NAME          : UnifiOverrideDpdLut
 * PSID          : 2258 (0x08D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 147
 * MAX           : 147
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a DPD LUT directly to the HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DPD_LUT 0x08D2

/*******************************************************************************
 * NAME          : UnifiGoogleMaxNumberOfPeriodicScans
 * PSID          : 2260 (0x08D4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_NUMBER_OF_PERIODIC_SCANS 0x08D4

/*******************************************************************************
 * NAME          : UnifiGoogleMaxRssiSampleSize
 * PSID          : 2261 (0x08D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_RSSI_SAMPLE_SIZE 0x08D5

/*******************************************************************************
 * NAME          : UnifiGoogleMaxHotlistAPs
 * PSID          : 2262 (0x08D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_HOTLIST_APS 0x08D6

/*******************************************************************************
 * NAME          : UnifiGoogleMaxSignificantWifiChangeAPs
 * PSID          : 2263 (0x08D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_SIGNIFICANT_WIFI_CHANGE_APS 0x08D7

/*******************************************************************************
 * NAME          : UnifiGoogleMaxBssidHistoryEntries
 * PSID          : 2264 (0x08D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_BSSID_HISTORY_ENTRIES 0x08D8

/*******************************************************************************
 * NAME          : UnifiRestrictScoTxopSize
 * PSID          : 2265 (0x08D9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  If non 0 value, during an SCO/ESCO USBPO, restrict the TXOP Limit to
 *  value (in steps of 32us). 0 = Do not restrict.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RESTRICT_SCO_TXOP_SIZE 0x08D9

/*******************************************************************************
 * NAME          : UnifiUspboTxAggregateBufferSize
 * PSID          : 2266 (0x08DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Control BA TX agreements and Restrict the Tx BA Buffer size during
 *  non-LTE USPBO. 0 = Tx BA agreements will be suppressed during USPBO. 1 -
 *  256 = Tx BA agreements will be preserved during USPBO, buffer size
 *  limited to this value. 0xffff = Tx BA agreements will be preserved during
 *  USPBO, buffer size limited to default value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USPBO_TX_AGGREGATE_BUFFER_SIZE 0x08DA

/*******************************************************************************
 * NAME          : UnifiNonLteUspboConfig
 * PSID          : 2267 (0x08DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X00
 * DESCRIPTION   :
 *  Deprecated. Configuration bitmap for non-LTE BlockACK control. bit0: If
 *  the bit value is 1, then preserve RX BA agreements during USPBO. If the
 *  bit value is 0, then RX BA agreements will be suppressed during USPBO
 *  bit1: If the bit value is 1, then preserve TX BA agreements during USPBO.
 *  If the bit value is 0, then TX BA agreements will be suppressed during
 *  USPBO bit2-bit15: reserved
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NON_LTE_USPBO_CONFIG 0x08DB

/*******************************************************************************
 * NAME          : UnifiSetResponseRate
 * PSID          : 2268 (0x08DC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Forces the response rate to the one of the selected or lower than rx
 *  frame A bitmask of following rates MAC_BASIC_RATE11A_06MBPS_MASK =
 *  0x0001, MAC_BASIC_RATE11A_09MBPS_MASK = 0x0002,
 *  MAC_BASIC_RATE11A_12MBPS_MASK = 0x0004, MAC_BASIC_RATE11A_18MBPS_MASK =
 *  0x0008, MAC_BASIC_RATE11A_24MBPS_MASK = 0x0010,
 *  MAC_BASIC_RATE11A_36MBPS_MASK = 0x0020, MAC_BASIC_RATE11A_48MBPS_MASK =
 *  0x0040, MAC_BASIC_RATE11A_54MBPS_MASK = 0x0080,
 *  MAC_BASIC_RATE11B_01MBPS_MASK = 0x0100, MAC_BASIC_RATE11B_02MBPS_MASK =
 *  0x0200, MAC_BASIC_RATE11B_5M5BPS_CCK_MASK = 0x0400,
 *  MAC_BASIC_RATE11B_11MBPS_CCK_MASK = 0x0800
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SET_RESPONSE_RATE 0x08DC

/*******************************************************************************
 * NAME          : UnifiMacBeaconTimeout
 * PSID          : 2270 (0x08DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 128
 * DESCRIPTION   :
 *  The maximum time in microseconds we want to stall TX data when expecting
 *  a beacon at EBRT time as a station.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_BEACON_TIMEOUT 0x08DE

/*******************************************************************************
 * NAME          : UnifiMifOffAllowed
 * PSID          : 2271 (0x08DF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Allow MIF to be turned off. If set to TRUE, it will prevent MIF to be
 *  turned off while WLAN is running. Disabling the mib will prevent to enter
 *  idle mode lite or idle mode medium
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MIF_OFF_ALLOWED 0x08DF

/*******************************************************************************
 * NAME          : UnifiBlockScanAfterNumSchedVif
 * PSID          : 2272 (0x08E0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Block Scan requests from having medium time after a specified amount of
 *  sync VIFs are schedulable. A value of 0 disables the functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BLOCK_SCAN_AFTER_NUM_SCHED_VIF 0x08E0

/*******************************************************************************
 * NAME          : UnifiMiflessStaEnable
 * PSID          : 2273 (0x08E1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  This is flag for checking if MIFLESS for station vif is enabled or not.
 *  True means enabled. And false means disabled. This would be mainly
 *  depended on RF chip.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MIFLESS_STA_ENABLE 0x08E1

/*******************************************************************************
 * NAME          : UnifiStaUsesOneAntennaWhenIdle
 * PSID          : 2274 (0x08E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Allow the platform to downgrade antenna usage for STA VIFs to 1 if the
 *  VIF is idle. Only valid for multi-radio platforms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_USES_ONE_ANTENNA_WHEN_IDLE 0x08E2

/*******************************************************************************
 * NAME          : UnifiStaUsesMultiAntennasDuringConnect
 * PSID          : 2275 (0x08E3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Allow the platform to use multiple antennas for STA VIFs during the
 *  connect phase. Only valid for multi-radio platforms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_USES_MULTI_ANTENNAS_DURING_CONNECT 0x08E3

/*******************************************************************************
 * NAME          : UnifiApUsesOneAntennaWhenPeersIdle
 * PSID          : 2276 (0x08E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Allow the platform to downgrade antenna usage for AP VIFs when all
 *  connected peers are idle. Only valid for multi-radio platforms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_USES_ONE_ANTENNA_WHEN_PEERS_IDLE 0x08E4

/*******************************************************************************
 * NAME          : deprecated_unifiUpdateAntennaCapabilitiesWhenScanning
 * PSID          : 2277 (0x08E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Specify whether antenna scan activities will be allowed to cause an
 *  update of VIF capability. Only valid for multi-radio platforms. WARNING:
 *  Changing this value after system start-up will have no effect.
 *******************************************************************************/
#define SLSI_PSID_DEPRECATED_UNIFI_UPDATE_ANTENNA_CAPABILITIES_WHEN_SCANNING 0x08E5

/*******************************************************************************
 * NAME          : UnifiPreferredAntennaBitmap
 * PSID          : 2278 (0x08E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Specify the
 *  preferred antenna(s) to use. A value of 0 means that the FW will decide
 *  on the antenna(s) to use. Only valid for multi-radio platforms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PREFERRED_ANTENNA_BITMAP 0x08E6

/*******************************************************************************
 * NAME          : UnifiMaxConcurrentMaCs
 * PSID          : 2279 (0x08E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Specify the
 *  maximum number of MACs that may be used for the platform. For multi-MAC
 *  platforms that value *could* be greater than 1. WARNING: Changing this
 *  value after system start-up will have no effect.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_CONCURRENT_MA_CS 0x08E7

/*******************************************************************************
 * NAME          : UnifiLoadDpdLutPerRadio
 * PSID          : 2280 (0x08E8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 147
 * MAX           : 147
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a static DPD LUT to the FW, read DPD LUT from hardware (for devices
 *  that support multiple radios)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOAD_DPD_LUT_PER_RADIO 0x08E8

/*******************************************************************************
 * NAME          : UnifiOverrideDpdLutPerRadio
 * PSID          : 2281 (0x08E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 147
 * MAX           : 147
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a DPD LUT directly to the HW (for devices that support multiple
 *  radios)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DPD_LUT_PER_RADIO 0x08E9

/*******************************************************************************
 * NAME          : UnifiRoamingLowLatencySoftRoamingAllowed
 * PSID          : 2282 (0x08EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Allow Soft Roaming functionality for some chipsets when the low latency
 *  mode is enabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_LOW_LATENCY_SOFT_ROAMING_ALLOWED 0x08EA

/*******************************************************************************
 * NAME          : UnifiRoamingSoftRoamingActivatedWhenLcdIsOff
 * PSID          : 2284 (0x08EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Soft Roaming functionality when the LCD is off.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_SOFT_ROAMING_ACTIVATED_WHEN_LCD_IS_OFF 0x08EC

/*******************************************************************************
 * NAME          : UnifiRoamingSendDeauthWhenRoamingBack
 * PSID          : 2285 (0x08ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Send a deauthentication frame before sending an authentication frame when
 *  roaming back.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_SEND_DEAUTH_WHEN_ROAMING_BACK 0x08ED

/*******************************************************************************
 * NAME          : UnifiRoamBtmDisregardSelectionFactor
 * PSID          : 2286 (0x08EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Disregard the selection factor for BTM candidates.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BTM_DISREGARD_SELECTION_FACTOR 0x08EE

/*******************************************************************************
 * NAME          : UnifiTestSetMaxBandwidth
 * PSID          : 2287 (0x08EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. Set the
 *  maximum bandwidth for a vif Setting it to 0 uses the default bandwidth as
 *  selected by firmware. channel_bw_20_mhz = 20, channel_bw_40_mhz = 40,
 *  channel_bw_80_mhz = 80, channel_bw_160_mhz = 160
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TEST_SET_MAX_BANDWIDTH 0x08EF

/*******************************************************************************
 * NAME          : UnifiLowLatencyScanNProbe
 * PSID          : 2288 (0x08F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Low Latency Scan: The number of ProbeRequest frames per channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOW_LATENCY_SCAN_NPROBE 0x08F0

/*******************************************************************************
 * NAME          : UnifiRoamEteActivated
 * PSID          : 2289 (0x08F1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This MIB activates support for calculation of estimated throughput. This
 *  sorts all candidates by descending order of the throughput.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_ETE_ACTIVATED 0x08F1

/*******************************************************************************
 * NAME          : UnifiRoamMpduss
 * PSID          : 2290 (0x08F2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X08
 * DESCRIPTION   :
 *  minimum MPDU start spacing of the MPDUs receiver. Limited by HW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_MPDUSS 0x08F2

/*******************************************************************************
 * NAME          : UnifiRoamBaWinSize
 * PSID          : 2291 (0x08F3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X20
 * DESCRIPTION   :
 *  Block Ack Configuration. Maximum number of BAs for receiving a-mpdu
 *  frame. Limited by HW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BA_WIN_SIZE 0x08F3

/*******************************************************************************
 * NAME          : UnifiRoamNchoCachedScanPeriod
 * PSID          : 2292 (0x08F4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 10000000
 * DESCRIPTION   :
 *  NCHO: Specifies the period at which the cached channel scan shall be run.
 *  For certification and Host use only.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_NCHO_CACHED_SCAN_PERIOD 0x08F4

/*******************************************************************************
 * NAME          : UnifiRoamCuFactor
 * PSID          : 2295 (0x08F7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Bi dimensional octet string table for allocating CUfactor to CU values.
 *  First index is the radio band, and the second will be CU table entry. The
 *  tables define the maximum CU value to which the values do apply(MAX CU),
 *  an OFFSET and an A value for the equation: CUfactor = OFFSET - A*(CU)/10
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_FACTOR 0x08F7

/*******************************************************************************
 * NAME          : UnifiBeaconCu
 * PSID          : 2296 (0x08F8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Provides a channel utilization of peer, value 255=100% channel
 *  utilization.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACON_CU 0x08F8

/*******************************************************************************
 * NAME          : UnifiRoamBtmForceAbridged
 * PSID          : 2297 (0x08F9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Force abridged scan, no matter the abridged bit is set or not.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BTM_FORCE_ABRIDGED 0x08F9

/*******************************************************************************
 * NAME          : UnifiRoamRssiBoost
 * PSID          : 2298 (0x08FA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  The value in dBm of the RSSI boost for each band
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSI_BOOST 0x08FA

/*******************************************************************************
 * NAME          : UnifiRoamTargetRssiBeaconLost
 * PSID          : 2299 (0x08FB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -127
 * MAX           : 32767
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  RSSI threshold under which candidate APs are not deemed eligible for
 *  Beacon Lost scans.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_TARGET_RSSI_BEACON_LOST 0x08FB

/*******************************************************************************
 * NAME          : UnifiRoamCuLocal
 * PSID          : 2300 (0x08FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Channel utilisation for the STA VIF, value 255=100% channel utilisation.
 *  - used for roaming
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_LOCAL 0x08FC

/*******************************************************************************
 * NAME          : UnifiRoamTargetRssiEmergency
 * PSID          : 2301 (0x08FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -127
 * MAX           : 32767
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  RSSI threshold under which candidate APs are not deemed eligible for
 *  emergency roaming scans.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_TARGET_RSSI_EMERGENCY 0x08FD

/*******************************************************************************
 * NAME          : UnifiRoamApSelectDeltaFactor
 * PSID          : 2302 (0x08FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  How much higher, in percentage points, does a candidate's score needs to
 *  be in order be considered an eligible candidate? A "0" value renders all
 *  candidates eligible. Please note this applies only to soft roams.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_AP_SELECT_DELTA_FACTOR 0x08FE

/*******************************************************************************
 * NAME          : UnifiRoamCuWeight
 * PSID          : 2303 (0x08FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 35
 * DESCRIPTION   :
 *  Weight of CUfactor, in percentage points, in AP selection algorithm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_WEIGHT 0x08FF

/*******************************************************************************
 * NAME          : UnifiRoamBtmApSelectDeltaFactor
 * PSID          : 2304 (0x0900)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       :
 * DESCRIPTION   :
 *  Delta value applied to the score of the currently connected AP to
 *  determine candidates' eligibility threshold for BTM triggered roaming
 *  scans
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BTM_AP_SELECT_DELTA_FACTOR 0x0900

/*******************************************************************************
 * NAME          : UnifiRoamRssiweight
 * PSID          : 2305 (0x0901)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 65
 * DESCRIPTION   :
 *  Weight of RSSI factor, in percentage points, in AP selection algorithm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSIWEIGHT 0x0901

/*******************************************************************************
 * NAME          : UnifiRoamRssiFactor
 * PSID          : 2306 (0x0902)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Table allocating RSSIfactor to RSSI values range.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_RSSI_FACTOR 0x0902

/*******************************************************************************
 * NAME          : UnifiRoamCuRssiScanTrigger
 * PSID          : 2307 (0x0903)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  The current channel Averaged RSSI value below which a soft roaming scan
 *  shall initially start, providing high channel utilisation (see
 *  unifiRoamCUScanTrigger). This is a table indexed by frequency band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_RSSI_SCAN_TRIGGER 0x0903

/*******************************************************************************
 * NAME          : UnifiRoamCuScanTrigger
 * PSID          : 2308 (0x0904)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  BSS Load / Channel Utilisation trigger above which a soft roaming scan
 *  shall initially start. This is a table indexed by frequency band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CU_SCAN_TRIGGER 0x0904

/*******************************************************************************
 * NAME          : UnifiRoamBssLoadMonitoringFrequency
 * PSID          : 2309 (0x0905)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  How often, in seconds, should the BSS load be monitored? - used for
 *  roaming
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BSS_LOAD_MONITORING_FREQUENCY 0x0905

/*******************************************************************************
 * NAME          : UnifiRoamBlacklistSize
 * PSID          : 2310 (0x0906)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : entries
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Do not remove! Read by the host! And then passed up to the framework.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BLACKLIST_SIZE 0x0906

/*******************************************************************************
 * NAME          : UnifiCuMeasurementInterval
 * PSID          : 2311 (0x0907)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 1
 * MAX           : 1000
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  The interval in ms to perform the channel usage update
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CU_MEASUREMENT_INTERVAL 0x0907

/*******************************************************************************
 * NAME          : UnifiCurrentBssNss
 * PSID          : 2312 (0x0908)
 * PER INTERFACE?: NO
 * TYPE          : unifiAntennaMode
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specifies current AP antenna mode: BIG DATA 0 = SISO, 1 = MIMO (2x2), 2 =
 *  MIMO (3x3), 3 = MIMO (4x4)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_BSS_NSS 0x0908

/*******************************************************************************
 * NAME          : UnifiApMimoUsed
 * PSID          : 2313 (0x0909)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  AP uses MU-MIMO
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_MIMO_USED 0x0909

/*******************************************************************************
 * NAME          : UnifiRoamEapolTimeout
 * PSID          : 2314 (0x090A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Maximum time, in seconds, allowed for an offloaded Eapol (4 way
 *  handshake).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_EAPOL_TIMEOUT 0x090A

/*******************************************************************************
 * NAME          : UnifiRoamingCount
 * PSID          : 2315 (0x090B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Number of roams
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_COUNT 0x090B

/*******************************************************************************
 * NAME          : UnifiRoamingAkm
 * PSID          : 2316 (0x090C)
 * PER INTERFACE?: NO
 * TYPE          : unifiRoamingAKM
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  specifies current AKM 0 = None 1 = OKC 2 = FT (FT_1X) 3 = PSK 4 = FT_PSK
 *  5 = PMKSA Caching
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_AKM 0x090C

/*******************************************************************************
 * NAME          : UnifiCurrentBssBandwidth
 * PSID          : 2317 (0x090D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Current bandwidth the STA is operating on channel_bw_20_mhz = 20,
 *  channel_bw_40_mhz = 40, channel_bw_80_mhz = 80, channel_bw_160_mhz = 160
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_BSS_BANDWIDTH 0x090D

/*******************************************************************************
 * NAME          : UnifiCurrentBssChannelFrequency
 * PSID          : 2318 (0x090E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Centre frequency for the connected channel
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_BSS_CHANNEL_FREQUENCY 0x090E

/*******************************************************************************
 * NAME          : UnifiRoamInactiveCount
 * PSID          : 2319 (0x090F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : number_of_packets
 * MIN           : 0
 * MAX           : 1000
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  The number of packets over which the link is considered not idle.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_INACTIVE_COUNT 0x090F

/*******************************************************************************
 * NAME          : UnifiLoggerEnabled
 * PSID          : 2320 (0x0910)
 * PER INTERFACE?: NO
 * TYPE          : unifiWifiLogger
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Enable reporting of the following events for Android logging: - firmware
 *  connectivity events - fate of management frames sent by the host through
 *  the MLME SAP It can take the following values: - 0: reporting for non
 *  mandetory triggers disabled. EAPOL, security, btm frames and roam
 *  triggers are reported. - 1: partial reporting is enabled. Beacons frames
 *  will not be reported. - 2: full reporting is enabled. Beacons frames are
 *  included.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOGGER_ENABLED 0x0910

/*******************************************************************************
 * NAME          : UnifiMaPacketFateEnabled
 * PSID          : 2321 (0x0911)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable reporting of the fate of the TX packets sent by the host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MA_PACKET_FATE_ENABLED 0x0911

/*******************************************************************************
 * NAME          : UnifiRoamApSelectMinDeltaFactor
 * PSID          : 2322 (0x0912)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 10000
 * DEFAULT       : 1500
 * DESCRIPTION   :
 *  The minimum score delta with which a candidate needs to be better to be
 *  considered as an eligible candidate. This prevents ping pong roaming.
 *  Please note this applies only to soft roams.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_AP_SELECT_MIN_DELTA_FACTOR 0x0912

/*******************************************************************************
 * NAME          : UnifiStaVifLinkNss
 * PSID          : 2324 (0x0914)
 * PER INTERFACE?: NO
 * TYPE          : unifiAntennaMode
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  STA Vif (Not P2P) while connected to an AP and does not apply to TDLS
 *  links. Specifies the max number of NSS that the link can use: BIG DATA 0
 *  = SISO, 1 = MIMO (2x2), 2 = MIMO (3x3), 3 = MIMO (4x4)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_VIF_LINK_NSS 0x0914

/*******************************************************************************
 * NAME          : UnifiFrameRxCounters
 * PSID          : 2326 (0x0916)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Frame RX Counters used by the host. These are required by MCD.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RX_COUNTERS 0x0916

/*******************************************************************************
 * NAME          : UnifiFrameTxCounters
 * PSID          : 2327 (0x0917)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Frame TX Counters used by the host. These are required by MCD.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_TX_COUNTERS 0x0917

/*******************************************************************************
 * NAME          : UnifiLaaNssSpeculationIntervalSlotTime
 * PSID          : 2330 (0x091A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the repeatable amount of time,
 *  in ms, that firmware will start to send speculation frames for spatial
 *  streams.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_NSS_SPECULATION_INTERVAL_SLOT_TIME 0x091A

/*******************************************************************************
 * NAME          : UnifiLaaNssSpeculationIntervalSlotMaxNum
 * PSID          : 2331 (0x091B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the maximum number of
 *  speculation time slot for spatial stream.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_NSS_SPECULATION_INTERVAL_SLOT_MAX_NUM 0x091B

/*******************************************************************************
 * NAME          : UnifiLaaBwSpeculationIntervalSlotTime
 * PSID          : 2332 (0x091C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the repeatable amount of time,
 *  in ms, that firmware will start to send speculation frames for bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_BW_SPECULATION_INTERVAL_SLOT_TIME 0x091C

/*******************************************************************************
 * NAME          : UnifiLaaBwSpeculationIntervalSlotMaxNum
 * PSID          : 2333 (0x091D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the maximum number of
 *  speculation time slot for bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_BW_SPECULATION_INTERVAL_SLOT_MAX_NUM 0x091D

/*******************************************************************************
 * NAME          : UnifiLaaMcsSpeculationIntervalSlotTime
 * PSID          : 2334 (0x091E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the repeatable amount of time,
 *  in ms, that firmware will start to send speculation frames for MCS or
 *  rate index.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_MCS_SPECULATION_INTERVAL_SLOT_TIME 0x091E

/*******************************************************************************
 * NAME          : UnifiLaaMcsSpeculationIntervalSlotMaxNum
 * PSID          : 2335 (0x091F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the maximum number of
 *  speculation time slot for MCS or rate index.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_MCS_SPECULATION_INTERVAL_SLOT_MAX_NUM 0x091F

/*******************************************************************************
 * NAME          : UnifiLaaGiSpeculationIntervalSlotTime
 * PSID          : 2336 (0x0920)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the repeatable amount of time,
 *  in ms, that firmware will start to send speculation frames for guard
 *  interval.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_GI_SPECULATION_INTERVAL_SLOT_TIME 0x0920

/*******************************************************************************
 * NAME          : UnifiLaaGiSpeculationIntervalSlotMaxNum
 * PSID          : 2337 (0x0921)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the maximum number of
 *  speculation time slot for guard interval.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_GI_SPECULATION_INTERVAL_SLOT_MAX_NUM 0x0921

/*******************************************************************************
 * NAME          : UnifiLaaTxNssAllowedMode
 * PSID          : 2338 (0x0922)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  If non-zero,it constraites NSS mode used in LAA. Note that this MIB
 *  should be set before any connection is made. - 0 : This MIB is not used.
 *  LAA can use all possible NSS modes - 0x1 : Only 1SS mode can be used
 *  (only bit 0 is set) - 0x2 : Only 2SS mode can be used (only bit 1 is set)
 *  - 0x3 : Both 1SS and 2SS modes can be used (both bit 0 and 1 are set)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_TX_NSS_ALLOWED_MODE 0x0922

/*******************************************************************************
 * NAME          : UnifiLaaTxDiversityBeamformEnabled
 * PSID          : 2350 (0x092E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It is used to enable or disable TX
 *  beamformer functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_TX_DIVERSITY_BEAMFORM_ENABLED 0x092E

/*******************************************************************************
 * NAME          : UnifiLaaTxDiversityBeamformMinMcs
 * PSID          : 2351 (0x092F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. TX Beamform is applied when MCS is same or
 *  larger than this threshold value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_TX_DIVERSITY_BEAMFORM_MIN_MCS 0x092F

/*******************************************************************************
 * NAME          : UnifiLaaTxDiversityFixMode
 * PSID          : 2352 (0x0930)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It is used to fix TX diversity mode. With
 *  two antennas available and only one spatial stream used, then one of the
 *  following modes can be selected: - 0 : Not fixed. Tx diversity mode is
 *  automatically selected by LAA. - 1 : CDD fixed mode - 2 : Beamforming
 *  fixed mode - 3 : STBC fixed mode
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_TX_DIVERSITY_FIX_MODE 0x0930

/*******************************************************************************
 * NAME          : UnifiLaaAmpduInhibitTimeout
 * PSID          : 2353 (0x0931)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  When fallback rate for an A-MPDU retransmission is non-(V)HT, we need to
 *  disable A-MPDUs to transmit the MPDU(s) at that rate. LAA does this, and
 *  re-enabled A-MPDUs based on this timeout. Note that the timeout is
 *  examined only when the current rate changes, which signifies some change
 *  in the conditions or state machine. If set to 0, A-MPDUs will restored as
 *  soon as a rate with MCS>1 or BW>20 is installed.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_AMPDU_INHIBIT_TIMEOUT 0x0931

/*******************************************************************************
 * NAME          : UnifiLaaInhibitRtsCount
 * PSID          : 2354 (0x0932)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 32
 * DESCRIPTION   :
 *  When RTS protection for an A-MPDU transmission fails
 *  UnifiLaaInhibitRtsCount successive times, we disable RTS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_INHIBIT_RTS_COUNT 0x0932

/*******************************************************************************
 * NAME          : UnifiLaaRtsInhibitTimeout
 * PSID          : 2355 (0x0933)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  When RTS protection for an A-MPDU transmission fails
 *  UnifiLaaInhibitRtsCount successive times, we disable RTS. RTS is later
 *  re-enabled based on this timeout. Note that RTS may be controlled by
 *  other means, orthogonally to this feature, e.g. Coex may have disabled
 *  it. If set to 0, RTS will be restored only after a re-association.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_RTS_INHIBIT_TIMEOUT 0x0933

/*******************************************************************************
 * NAME          : UnifiLaaProtectionConfigOverride
 * PSID          : 2356 (0x0934)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 6
 * DESCRIPTION   :
 *  Overrides the default Protection configuration. Only valid flags are
 *  DPIF_PEER_INFO_PROTECTION_TXOP_AMPDU and
 *  DPIF_PEER_INFO_PROTECTION_ALLOWED. Default allows protection code to work
 *  out the rules based on VIF configuration. If
 *  DPIF_PEER_INFO_PROTECTION_ALLOWED is unset, all protection, for this vif,
 *  is disabled. If DPIF_PEER_INFO_PROTECTION_TXOP_AMPDU is unset then, for
 *  the specified vif, the first A-MPDU in the TxOp is no longer protected.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_PROTECTION_CONFIG_OVERRIDE 0x0934

/*******************************************************************************
 * NAME          : UnifiLaaSpeculationMaxTime
 * PSID          : 2357 (0x0935)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5000
 * DESCRIPTION   :
 *  For Link Adaptation Algorithm. It defines the maximum time interval (in
 *  milliseconds) between speculating. So if the interval is being scaled due
 *  to not being scheduled all the time, the speculation interval will not
 *  exceed this value, even when backing off.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_SPECULATION_MAX_TIME 0x0935

/*******************************************************************************
 * NAME          : UnifiRateStatsRtsErrorCount
 * PSID          : 2358 (0x0936)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of successive RTS failures.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_RTS_ERROR_COUNT 0x0936

/*******************************************************************************
 * NAME          : UnifiRtsDurationProtectsFullTxop
 * PSID          : 2359 (0x0937)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When set to true the RTS duration protects full TXOP. If false then it
 *  only covers from the RTS to the first BA, which may be followed by other
 *  bursted AMPDUs
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RTS_DURATION_PROTECTS_FULL_TXOP 0x0937

/*******************************************************************************
 * NAME          : UnifiLaaAmpduInhibitRtsAlso
 * PSID          : 2360 (0x0938)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When inhibiting A-MPDU also disable RTS
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_AMPDU_INHIBIT_RTS_ALSO 0x0938

/*******************************************************************************
 * NAME          : UnifiCsrOnlyEifsDuration
 * PSID          : 2362 (0x093A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 12
 * DESCRIPTION   :
 *  Specifies time that is used for EIFS. A value of 0 causes the build in
 *  value to be used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CSR_ONLY_EIFS_DURATION 0x093A

/*******************************************************************************
 * NAME          : UnifiOverrideDefaultBetxopForHt
 * PSID          : 2364 (0x093C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 171
 * DESCRIPTION   :
 *  When set to non-zero value then this will override the BE TXOP for 11n
 *  and higher modulations (in 32 usec units) to the value specified here.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DEFAULT_BETXOP_FOR_HT 0x093C

/*******************************************************************************
 * NAME          : UnifiOverrideDefaultBetxop
 * PSID          : 2365 (0x093D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 78
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: When set to
 *  non-zero value then this will override the BE TXOP for 11g (in 32 usec
 *  units) to the value specified here.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DEFAULT_BETXOP 0x093D

/*******************************************************************************
 * NAME          : UnifiRxabbTrimSettings
 * PSID          : 2366 (0x093E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Various settings to change RX ABB filter trim behavior.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RXABB_TRIM_SETTINGS 0x093E

/*******************************************************************************
 * NAME          : UnifiRadioTrimsEnable
 * PSID          : 2367 (0x093F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0XFFF5
 * DESCRIPTION   :
 *  A bitmap for enabling/disabling trims at runtime. Check unifiEnabledTrims
 *  enum for description of the possible values.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TRIMS_ENABLE 0x093F

/*******************************************************************************
 * NAME          : UnifiRadioCcaThresholds
 * PSID          : 2368 (0x0940)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  The wideband CCA ED thresholds so that the CCA-ED triggers at the
 *  regulatory value of -62 dBm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_CCA_THRESHOLDS 0x0940

/*******************************************************************************
 * NAME          : UnifiHardwarePlatform
 * PSID          : 2369 (0x0941)
 * PER INTERFACE?: NO
 * TYPE          : unifiHardwarePlatform
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware platform. This is necessary so we can apply tweaks to specific
 *  revisions, even though they might be running the same baseband and RF
 *  chip combination. Check unifiHardwarePlatform enum for description of the
 *  possible values.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HARDWARE_PLATFORM 0x0941

/*******************************************************************************
 * NAME          : UnifiForceChannelBw
 * PSID          : 2370 (0x0942)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Test only: Force channel bandwidth to specified value. This can also be
 *  used to allow emulator/silicon back to back connection to communicate at
 *  bandwidth other than default (20 MHz) Setting it to 0 uses the default
 *  bandwidth as selected by firmware. The change will be applied at next
 *  radio state change opportunity channel_bw_20_mhz = 20, channel_bw_40_mhz
 *  = 40, channel_bw_80_mhz = 80
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_CHANNEL_BW 0x0942

/*******************************************************************************
 * NAME          : UnifiDpdTrainingDuration
 * PSID          : 2371 (0x0943)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Duration of DPD training (in ms).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_TRAINING_DURATION 0x0943

/*******************************************************************************
 * NAME          : UnifiTxFtrimSettings
 * PSID          : 2372 (0x0944)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter frequency compensation settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_FTRIM_SETTINGS 0x0944

/*******************************************************************************
 * NAME          : UnifiDpdTrainPacketConfig
 * PSID          : 2373 (0x0945)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       :
 * DESCRIPTION   :
 *  This MIB allows the dummy packets training bandwidth and rates to be
 *  overriden. Tipically the bandwidth would be the same as the channel
 *  bandwidth (for example 80 MHz packets for an 80 Mhz channel) and rates
 *  MCS1 and MCS5. With this MIB you can set, for example, an 80 MHz channel
 *  to be trained using 20 MHz bandwidth (centered or not) with MCS2 and MCS7
 *  packets. The MIB index dictates what channel bandwidth the configuration
 *  is for (1 for 20 MHz, 2 for 40 MHz and so on). The format is: - octet 0:
 *  train bandwidth (this basically follows the rice_channel_bw_t enum). -
 *  octet 1: train primary channel position - octet 2-3: OFDM 0 rate - octet
 *  4-5: OFDM 1 rate - octet 6-7: CCK rate (unused) The rates are encoded in
 *  host(FAPI) format, see SC-506179, section 4.41.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_TRAIN_PACKET_CONFIG 0x0945

/*******************************************************************************
 * NAME          : UnifiTxPowerTrimCommonConfig
 * PSID          : 2374 (0x0946)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Common transmitter power trim settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_TRIM_COMMON_CONFIG 0x0946

/*******************************************************************************
 * NAME          : UnifiIqDebugEnabled
 * PSID          : 2375 (0x0947)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Send IQ capture data to host for IQ debug
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IQ_DEBUG_ENABLED 0x0947

/*******************************************************************************
 * NAME          : UnifiFtmOneCrossTwoSupport
 * PSID          : 2376 (0x0948)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable FTM 1*2 support
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_ONE_CROSS_TWO_SUPPORT 0x0948

/*******************************************************************************
 * NAME          : UnifiRoamFtOverDsEnabled
 * PSID          : 2378 (0x094A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable roaming by FT over DS if BSS support it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_FT_OVER_DS_ENABLED 0x094A

/*******************************************************************************
 * NAME          : UnifiRoamFtOverDsMinRssi
 * PSID          : 2379 (0x094B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -70
 * DESCRIPTION   :
 *  Defines the minimum RSSI of the roaming required to try 'FT over the DS'.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_FT_OVER_DS_MIN_RSSI 0x094B

/*******************************************************************************
 * NAME          : UnifiEnableCoexLowLatency
 * PSID          : 2401 (0x0961)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable requests to BT to modify behaviour to support WLAN Low Latency
 *  operation. If enabled, WLAN shall request Low Latency supporting
 *  operation from BT if Low Latency is requested by the host or if
 *  CTS-To-Self is being used to protect Leaky APs See also
 *  unifiUseCtsForLeakyAp.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_COEX_LOW_LATENCY 0x0961

/*******************************************************************************
 * NAME          : UnifiCoexDebugOverrideBt
 * PSID          : 2425 (0x0979)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables overriding of all BT activities by WLAN.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_COEX_DEBUG_OVERRIDE_BT 0x0979

/*******************************************************************************
 * NAME          : UnifiLteMailbox
 * PSID          : 2430 (0x097E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 36
 * MAX           : 40
 * DEFAULT       :
 * DESCRIPTION   :
 *  Set modem status to simulate lte status updates. See SC-505775-SP for API
 *  description. Defined as array of uint32 represented by the octet string
 *  FOR TEST PURPOSES ONLY
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_MAILBOX 0x097E

/*******************************************************************************
 * NAME          : UnifiLteMwsSignal
 * PSID          : 2431 (0x097F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Set modem status to simulate lte status updates. See SC-505775-SP for API
 *  description. See unifiLteSignalsBitField for enum bitmap. FOR TEST
 *  PURPOSES ONLY
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_MWS_SIGNAL 0x097F

/*******************************************************************************
 * NAME          : UnifiLteEnableChannelAvoidance
 * PSID          : 2432 (0x0980)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enables channel avoidance scheme for LTE Coex
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_ENABLE_CHANNEL_AVOIDANCE 0x0980

/*******************************************************************************
 * NAME          : UnifiLteEnablePowerBackoff
 * PSID          : 2433 (0x0981)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enables power backoff scheme for LTE Coex
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_ENABLE_POWER_BACKOFF 0x0981

/*******************************************************************************
 * NAME          : UnifiLteEnableTimeDomain
 * PSID          : 2434 (0x0982)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enables TDD scheme for LTE Coex
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_ENABLE_TIME_DOMAIN 0x0982

/*******************************************************************************
 * NAME          : UnifiLteEnableLteCoex
 * PSID          : 2435 (0x0983)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enables LTE Coex support
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_ENABLE_LTE_COEX 0x0983

/*******************************************************************************
 * NAME          : UnifiLteBand40PowerBackoffChannels
 * PSID          : 2436 (0x0984)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X01, 0X02 }
 * DESCRIPTION   :
 *  Defines channels to which power backoff shall be applied when LTE
 *  operating on Band40.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_POWER_BACKOFF_CHANNELS 0x0984

/*******************************************************************************
 * NAME          : UnifiLteBand40PowerBackoffRsrpLow
 * PSID          : 2437 (0x0985)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -140
 * MAX           : -77
 * DEFAULT       : -100
 * DESCRIPTION   :
 *  WLAN Power Reduction shall be applied when RSRP of LTE operating on band
 *  40 falls below this level
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_POWER_BACKOFF_RSRP_LOW 0x0985

/*******************************************************************************
 * NAME          : UnifiLteBand40PowerBackoffRsrpHigh
 * PSID          : 2438 (0x0986)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -140
 * MAX           : -77
 * DEFAULT       : -95
 * DESCRIPTION   :
 *  WLAN Power Reduction shall be restored when RSRP of LTE operating on band
 *  40 climbs above this level
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_POWER_BACKOFF_RSRP_HIGH 0x0986

/*******************************************************************************
 * NAME          : UnifiLteBand40PowerBackoffRsrpAveragingAlpha
 * PSID          : 2439 (0x0987)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : percentage
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Weighting applied when calculaing the average RSRP when considering Power
 *  Back Off Specifies the percentage weighting (alpha) to give to the most
 *  recent value when calculating the moving average. ma_new = alpha *
 *  new_sample + (1-alpha) * ma_old.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_POWER_BACKOFF_RSRP_AVERAGING_ALPHA 0x0987

/*******************************************************************************
 * NAME          : UnifiLteSetChannel
 * PSID          : 2440 (0x0988)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enables LTE Coex support
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_SET_CHANNEL 0x0988

/*******************************************************************************
 * NAME          : UnifiLteSetPowerBackoff
 * PSID          : 2441 (0x0989)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  MIB to force WLAN Power Backoff for LTE COEX testing purposes
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_SET_POWER_BACKOFF 0x0989

/*******************************************************************************
 * NAME          : UnifiLteSetTddDebugMode
 * PSID          : 2442 (0x098A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  MIB to enable LTE TDD COEX simulation for testing purposes
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_SET_TDD_DEBUG_MODE 0x098A

/*******************************************************************************
 * NAME          : UnifiLteBand40AvoidChannels
 * PSID          : 2443 (0x098B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X01, 0X06 }
 * DESCRIPTION   :
 *  MIB to define WLAN channels to avoid when LTE Band 40 in use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_AVOID_CHANNELS 0x098B

/*******************************************************************************
 * NAME          : UnifiLteBand41AvoidChannels
 * PSID          : 2444 (0x098C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X04, 0X0E }
 * DESCRIPTION   :
 *  MIB to define WLAN channels to avoid when LTE Band 41 in use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND41_AVOID_CHANNELS 0x098C

/*******************************************************************************
 * NAME          : UnifiLteBand7AvoidChannels
 * PSID          : 2445 (0x098D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X09, 0X0E }
 * DESCRIPTION   :
 *  MIB to define WLAN channels to avoid when LTE Band 7 in use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND7_AVOID_CHANNELS 0x098D

/*******************************************************************************
 * NAME          : UnifiLteBand40FreqThreshold
 * PSID          : 2446 (0x098E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 2350
 * MAX           : 2400
 * DEFAULT       : 2380
 * DESCRIPTION   :
 *  MIB describing the Band 40 Frequency Threshold
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND40_FREQ_THRESHOLD 0x098E

/*******************************************************************************
 * NAME          : UnifiLteBand41FreqThreshold
 * PSID          : 2447 (0x098F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 2496
 * MAX           : 2546
 * DEFAULT       : 2500
 * DESCRIPTION   :
 *  MIB describing the Band 41 Frequency Threshold
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND41_FREQ_THRESHOLD 0x098F

/*******************************************************************************
 * NAME          : UnifiLteBand79FreqThreshold
 * PSID          : 2448 (0x0990)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 4950
 * MAX           : 5000
 * DEFAULT       : 5000
 * DESCRIPTION   :
 *  MIB describing the Band 79 Frequency Threshold
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND79_FREQ_THRESHOLD 0x0990

/*******************************************************************************
 * NAME          : UnifiLteBand41PowerBackoffChannels
 * PSID          : 2449 (0x0991)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X0C, 0X0D }
 * DESCRIPTION   :
 *  Defines channels to which power backoff shall be applied when LTE
 *  operating on Band41.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND41_POWER_BACKOFF_CHANNELS 0x0991

/*******************************************************************************
 * NAME          : UnifiLteBand4041AvoidChannelsFallback
 * PSID          : 2450 (0x0992)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X06, 0X06 }
 * DESCRIPTION   :
 *  Fallback allowed channels when LTE/NR Band (N)40 and (N)41 in use
 *  simultaneously.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND4041_AVOID_CHANNELS_FALLBACK 0x0992

/*******************************************************************************
 * NAME          : UnifiLteBand79Enable
 * PSID          : 2451 (0x0993)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable cellular coexistence schemes for band (n)79
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND79_ENABLE 0x0993

/*******************************************************************************
 * NAME          : UnifiLteBand79PowerBackoffChannels
 * PSID          : 2452 (0x0994)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X00, 0X00 }
 * DESCRIPTION   :
 *  Defines channels to which power backoff shall be applied when NR
 *  operating on Band n79.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND79_POWER_BACKOFF_CHANNELS 0x0994

/*******************************************************************************
 * NAME          : UnifiLteBand79AvoidChannels
 * PSID          : 2453 (0x0995)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0X00, 0X00 }
 * DESCRIPTION   :
 *  MIB to define WLAN channels to avoid when NR Band 79 in use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_BAND79_AVOID_CHANNELS 0x0995

/*******************************************************************************
 * NAME          : UnifiLeakyApConfig
 * PSID          : 2454 (0x0996)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0001
 * DESCRIPTION   :
 *  Bitmap to configure leaky AP behavior bit 0: set dont send NULL with
 *  CTS2SELF when bad AP is detected
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LEAKY_AP_CONFIG 0x0996

/*******************************************************************************
 * NAME          : UnifiUseCtsForLeakyAp
 * PSID          : 2455 (0x0997)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable or disable the use of CTS-to-self frames by STA connected to AP
 *  that leaks frames after PM=1. If enabled, coex will also modify BT
 *  behaviour to ensure BT opportunities are short enough to be protected by
 *  CTS-To-Self. See also unifiEnableCoexLowLatency.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USE_CTS_FOR_LEAKY_AP 0x0997

/*******************************************************************************
 * NAME          : UnifiUspboRxAggregateBufferSize
 * PSID          : 2456 (0x0998)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Control BA RX agreements and Restrict the Rx BA Buffer size during
 *  non-LTE USPBO. 0 = Rx BA agreements will be suppressed during USPBO. 1 -
 *  256 = Rx BA agreements will be preserved during USPBO, buffer size
 *  limited to this value. 0xffff = Rx BA agreements will be preserved during
 *  USPBO, buffer size limited to default value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USPBO_RX_AGGREGATE_BUFFER_SIZE 0x0998

/*******************************************************************************
 * NAME          : UnifiMiflessApBitmap
 * PSID          : 2458 (0x099A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This is bitmap for checking if AP IDLE is MIFLESS of not BIT 0: Set means
 *  beacon is transmitted with MIF turned off.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MIFLESS_AP_BITMAP 0x099A

/*******************************************************************************
 * NAME          : UnifiAntennaSelectionOption
 * PSID          : 2459 (0x099B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Select how antennas will be allocated for SISO/RSDB scenarios. Only valid
 *  for multi-radio platforms. WARNING: Changing this value after system
 *  start-up will have no effect. If the value is set to 0, antenna0 is used
 *  for 2G4 and antenna1 is used for 5G. If the value is set to 1, antenna1
 *  is used for 2G4 and antenna0 is used for 5G. If the value is set to 2,
 *  the antenna with the strongest RSSI average will be preferred over the
 *  other antenna. Also see unifiAntennaSelection.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ANTENNA_SELECTION_OPTION 0x099B

/*******************************************************************************
 * NAME          : UnifiBeaconRxPeriodUpdate
 * PSID          : 2465 (0x09A1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables beacon rx period update, if AP is sending wrong beacon period.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACON_RX_PERIOD_UPDATE 0x09A1

/*******************************************************************************
 * NAME          : UnifiBlackoutSimulation
 * PSID          : 2470 (0x09A6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Test Purposes, create specific blackouts: 0 = Turn off all test
 *  blackouts. 1 = A2DP One shot blackout sent periodically 2 = ESCO Blackout
 *  Needs STA vif, and it must be connected before mib is changed. Clearing
 *  value should stop test.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BLACKOUT_SIMULATION 0x09A6

/*******************************************************************************
 * NAME          : UnifiBlackoutForceTxOnly
 * PSID          : 2471 (0x09A7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Setting individual bits will make any blackout requests of that type into
 *  TX only blackouts. Only applicable to coex blackouts. 0x00 = Do not force
 *  any blackouts to become TX only 0x01 = Force coex single shot blackouts
 *  to become TX only 0x02 = Force coex non-uspbo periodic blackouts to
 *  become TX only
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BLACKOUT_FORCE_TX_ONLY 0x09A7

/*******************************************************************************
 * NAME          : UnifiUseHostListenInterval
 * PSID          : 2476 (0x09AC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Listen interval of beacons when in single-vif as set by host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USE_HOST_LISTEN_INTERVAL 0x09AC

/*******************************************************************************
 * NAME          : UnifiMaxBeaconListenWindowTime
 * PSID          : 2479 (0x09AF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The maximum beacon listen window time we can limit on.This is the
 *  difference between listen end and listen start time.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_BEACON_LISTEN_WINDOW_TIME 0x09AF

/*******************************************************************************
 * NAME          : UnifiApScanAbsenceDuration
 * PSID          : 2480 (0x09B0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Duration of the Absence time to use when protecting AP VIFs from scan
 *  operations. A value of 0 disables the feature.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_SCAN_ABSENCE_DURATION 0x09B0

/*******************************************************************************
 * NAME          : UnifiApScanAbsencePeriod
 * PSID          : 2481 (0x09B1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Period of the Absence/Presence times cycles to use when protecting AP
 *  VIFs from scan operations.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_SCAN_ABSENCE_PERIOD 0x09B1

/*******************************************************************************
 * NAME          : UnifiMlmestaKeepAliveTimeoutCheck
 * PSID          : 2485 (0x09B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  DO NOT SET TO A VALUE HIGHER THAN THE TIMEOUT. How long before keepalive
 *  timeout to start polling, in seconds. This value is read only once when
 *  an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMESTA_KEEP_ALIVE_TIMEOUT_CHECK 0x09B5

/*******************************************************************************
 * NAME          : UnifiMlmeapKeepAliveTimeoutCheck
 * PSID          : 2486 (0x09B6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  DO NOT SET TO A VALUE HIGHER THAN THE TIMEOUT. How long before keepalive
 *  timeout to start polling, in seconds. This value is read only once when
 *  an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMEAP_KEEP_ALIVE_TIMEOUT_CHECK 0x09B6

/*******************************************************************************
 * NAME          : UnifiMlmegoKeepAliveTimeoutCheck
 * PSID          : 2487 (0x09B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  DO NOT SET TO A VALUE HIGHER THAN THE TIMEOUT. How long before keepalive
 *  timeout to start polling, in seconds. This value is read only once when
 *  an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMEGO_KEEP_ALIVE_TIMEOUT_CHECK 0x09B7

/*******************************************************************************
 * NAME          : UnifiBssMaxIdlePeriod
 * PSID          : 2488 (0x09B8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : second
 * MIN           : 0
 * MAX           : 300
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  BSS Idle MAX Period. Used to cap the value coming from BSS Max Idle
 *  Period IE, in seconds
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BSS_MAX_IDLE_PERIOD 0x09B8

/*******************************************************************************
 * NAME          : UnifiDualStaIdleModeEnabled
 * PSID          : 2490 (0x09BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables Dual STA Idle mode, if dual vif stations are active and both vifs
 *  are idle.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DUAL_STA_IDLE_MODE_ENABLED 0x09BA

/*******************************************************************************
 * NAME          : UnifiStaIdleModeControl
 * PSID          : 2491 (0x09BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0000
 * DESCRIPTION   :
 *  Control STA idle mode behaviour within firmware with bit flags. 1 defines
 *  enabled, 0 defines disabled. bit 0: STA idle mode with LCD on
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_IDLE_MODE_CONTROL 0x09BB

/*******************************************************************************
 * NAME          : UnifiGoctWindowDelay
 * PSID          : 2492 (0x09BC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Delay to apply Client Traffic Window in seconds for oppertunistic power
 *  save in P2PGO.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOCT_WINDOW_DELAY 0x09BC

/*******************************************************************************
 * NAME          : UnifiStaIdleModeEnabled
 * PSID          : 2493 (0x09BD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables STA Idle mode, if single vif station is active and there has been
 *  no activity for a time.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_IDLE_MODE_ENABLED 0x09BD

/*******************************************************************************
 * NAME          : UnifiFastPowerSaveTimeoutAggressive
 * PSID          : 2494 (0x09BE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 20000
 * DESCRIPTION   :
 *  UniFi implements a proprietary power management mode called Fast Power
 *  Save that balances network performance against power consumption. In this
 *  mode UniFi delays entering power save mode until it detects that there
 *  has been no exchange of data for the duration of time specified. The
 *  unifiFastPowerSaveTimeOutAggressive aims to improve the power consumption
 *  by setting a aggressive time when channel is not busy for the Fast Power
 *  Save Timeout. If set with a value above unifiFastPowerSaveTimeOut it will
 *  default to unifiFastPowerSaveTimeOut. Setting it to zero disables the
 *  feature
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAST_POWER_SAVE_TIMEOUT_AGGRESSIVE 0x09BE

/*******************************************************************************
 * NAME          : UnifiIdlemodeListenIntervalSkippingDtim
 * PSID          : 2495 (0x09BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00054645
 * DESCRIPTION   :
 *  Listen interval of beacons when in single-vif power saving mode,
 *  receiving DTIMs is enabled and idle mode enabled. No DTIMs are skipped
 *  during MVIF operation. A maximum of the listen interval beacons are
 *  skipped, which may be less than the number of DTIMs that can be skipped.
 *  The value is a lookup table for DTIM counts. Each 4bits, in LSB order,
 *  represent DTIM1, DTIM2, DTIM3, DTIM4, DTIM5, (unused). This key is only
 *  used for STA VIF, connected to an AP. For P2P group client intervals,
 *  refer to unifiIdlemodeP2PListenIntervalSkippingDTIM, PSID=2496.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IDLEMODE_LISTEN_INTERVAL_SKIPPING_DTIM 0x09BF

/*******************************************************************************
 * NAME          : UnifiIdlemodeP2PListenIntervalSkippingDtim
 * PSID          : 2496 (0x09C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00000002
 * DESCRIPTION   :
 *  Listen interval of beacons when in single-vif, P2P client power saving
 *  mode,receiving DTIMs and idle mode enabled. No DTIMs are skipped during
 *  MVIF operation. A maximum of (listen interval - 1) beacons are skipped,
 *  which may be less than the number of DTIMs that can be skipped. The value
 *  is a lookup table for DTIM counts. Each 4bits, in LSB order, represent
 *  DTIM1, DTIM2, DTIM3, DTIM4, DTIM5, (unused). This key is only used for
 *  P2P group client. For STA connected to an AP, refer to
 *  unifiIdlemodeListenIntervalSkippingDTIM, PSID=2495.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IDLEMODE_P2_PLISTEN_INTERVAL_SKIPPING_DTIM 0x09C0

/*******************************************************************************
 * NAME          : UnifiApIdleModeEnabled
 * PSID          : 2497 (0x09C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enables AP Idle mode if softAP is active, and there has been no activity
 *  for a time if unifiMiflessAPBitmap bit 0 is not set SOFTAP beacons will
 *  be transmitted with MIF turned on when IDLE otherwise beacon will be
 *  transmited in MIFLESS mode
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_IDLE_MODE_ENABLED 0x09C1

/*******************************************************************************
 * NAME          : UnifiFastPowerSaveTimeout
 * PSID          : 2500 (0x09C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 400000
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: UniFi
 *  implements a proprietary power management mode called Fast Power Save
 *  that balances network performance against power consumption. In this mode
 *  UniFi delays entering power save mode until it detects that there has
 *  been no exchange of data for the duration of time specified.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAST_POWER_SAVE_TIMEOUT 0x09C4

/*******************************************************************************
 * NAME          : UnifiFastPowerSaveTimeoutSmall
 * PSID          : 2501 (0x09C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 50000
 * DESCRIPTION   :
 *  UniFi implements a proprietary power management mode called Fast Power
 *  Save that balances network performance against power consumption. In this
 *  mode UniFi delays entering power save mode until it detects that there
 *  has been no exchange of data for the duration of time specified. The
 *  unifiFastPowerSaveTimeOutSmall aims to improve the power consumption by
 *  setting a lower bound for the Fast Power Save Timeout. If set with a
 *  value above unifiFastPowerSaveTimeOut it will default to
 *  unifiFastPowerSaveTimeOut.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAST_POWER_SAVE_TIMEOUT_SMALL 0x09C5

/*******************************************************************************
 * NAME          : UnifiMlmestaKeepAliveTimeout
 * PSID          : 2502 (0x09C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Timeout
 *  before disconnecting in seconds. 0 = Disabled. Capped to greater than 6
 *  seconds. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMESTA_KEEP_ALIVE_TIMEOUT 0x09C6

/*******************************************************************************
 * NAME          : UnifiMlmeapKeepAliveTimeout
 * PSID          : 2503 (0x09C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Timeout before disconnecting in seconds. 0 = Disabled. Capped to greater
 *  than 6 seconds. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMEAP_KEEP_ALIVE_TIMEOUT 0x09C7

/*******************************************************************************
 * NAME          : UnifiMlmegoKeepAliveTimeout
 * PSID          : 2504 (0x09C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Timeout before disconnecting in seconds. 0 = Disabled. Capped to greater
 *  than 6 seconds. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMEGO_KEEP_ALIVE_TIMEOUT 0x09C8

/*******************************************************************************
 * NAME          : UnifiStaRouterAdvertisementMinimumIntervalToForward
 * PSID          : 2505 (0x09C9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 60
 * MAX           : 4294967285
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  STA Mode: Minimum interval to forward Router Advertisement frames to
 *  Host. Minimum value = 60 secs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_ROUTER_ADVERTISEMENT_MINIMUM_INTERVAL_TO_FORWARD 0x09C9

/*******************************************************************************
 * NAME          : UnifiRoamConnectionQualityCheckWaitAfterConnect
 * PSID          : 2506 (0x09CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  The amount of time a STA will wait after connection before starting to
 *  check the MLME-installed connection quality trigger thresholds
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_CONNECTION_QUALITY_CHECK_WAIT_AFTER_CONNECT 0x09CA

/*******************************************************************************
 * NAME          : UnifiApBeaconMaxDrift
 * PSID          : 2507 (0x09CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0XFFFF
 * DESCRIPTION   :
 *  The maximum drift in microseconds we will allow for each beacon sent when
 *  we're trying to move it to get a 50% duty cycle between GO and STA in
 *  multiple VIF scenario. We'll delay our TX beacon by a maximum of this
 *  value until we reach our target TBTT. We have 3 possible cases for this
 *  value: a) ap_beacon_max_drift = 0x0000 - Feature disabled b)
 *  ap_beacon_max_drift between 0x0001 and 0xFFFE - Each time we transmit the
 *  beacon we'll move it a little bit forward but never more than this. (Not
 *  implemented yet) c) ap_beacon_max_drift = 0xFFFF - Move the beacon to the
 *  desired position in one shot.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_BEACON_MAX_DRIFT 0x09CB

/*******************************************************************************
 * NAME          : UnifiBssMaxIdlePeriodActivated
 * PSID          : 2508 (0x09CC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  If set STA will configure keep-alive with options specified in a received
 *  BSS max idle period IE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BSS_MAX_IDLE_PERIOD_ACTIVATED 0x09CC

/*******************************************************************************
 * NAME          : UnifiVifIdleMonitorTime
 * PSID          : 2509 (0x09CD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : second
 * MIN           : 0
 * MAX           : 1800
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  In Fast Power Save mode, the STA will decide whether it is idle based on
 *  monitoring its traffic class. If the traffic class is continuously
 *  "occasional" for equal or longer than the specified value (in seconds),
 *  then the VIF is marked as idle. Traffic class monitoring is based on the
 *  interval specified in the "unifiTrafficAnalysisPeriod" MIB
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VIF_IDLE_MONITOR_TIME 0x09CD

/*******************************************************************************
 * NAME          : UnifiDisableLegacyPowerSave
 * PSID          : 2510 (0x09CE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: This affects
 *  Station VIF power save behaviour. Setting it to true will disable legacy
 *  power save (i.e. we wil use fast power save to retrieve data) Note that
 *  actually disables full power save mode (i.e sending trigger to retrieve
 *  frames which will be PS-POLL for legacy and QOS-NULL for UAPSD)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISABLE_LEGACY_POWER_SAVE 0x09CE

/*******************************************************************************
 * NAME          : UnifiDebugForceActive
 * PSID          : 2511 (0x09CF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Force station
 *  power save mode to be active (when scheduled). VIF scheduling, coex and
 *  other non-VIF specific reasons could still force power save on the VIF.
 *  Applies to all VIFs of type station (includes P2P client). Changes to the
 *  mib will only get applied after next host/mlme power management request.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_FORCE_ACTIVE 0x09CF

/*******************************************************************************
 * NAME          : UnifiStationActivityIdleTime
 * PSID          : 2512 (0x09D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  Time since last station activity when it can be considered to be idle.
 *  Only used in SoftAP mode when determining if all connected stations are
 *  idle (not active).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STATION_ACTIVITY_IDLE_TIME 0x09D0

/*******************************************************************************
 * NAME          : UnifiDmsActivated
 * PSID          : 2513 (0x09D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Activate Directed Multicast Service (DMS)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DMS_ACTIVATED 0x09D1

/*******************************************************************************
 * NAME          : UnifiPowerManagementDelayTimeout
 * PSID          : 2514 (0x09D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 20000
 * DESCRIPTION   :
 *  When UniFi enters power save mode it signals the new state by setting the
 *  power management bit in the frame control field of a NULL frame. It then
 *  remains active for the period since the previous unicast reception, or
 *  since the transmission of the NULL frame, whichever is later. This entry
 *  controls the maximum time during which UniFi will continue to listen for
 *  data. This allows any buffered data on a remote device to be cleared.
 *  Specifies an upper limit on the timeout. UniFi internally implements a
 *  proprietary algorithm to adapt the timeout depending upon the
 *  situation.This is used by firmware when current station VIF is only
 *  station VIF which can be scheduled
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POWER_MANAGEMENT_DELAY_TIMEOUT 0x09D2

/*******************************************************************************
 * NAME          : UnifiApsdServicePeriodTimeout
 * PSID          : 2515 (0x09D3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 20000
 * DESCRIPTION   :
 *  During Unscheduled Automated Power Save Delivery (U-APSD), UniFi may
 *  trigger a service period in order to fetch data from the access point.
 *  The service period is normally terminated by a frame from the access
 *  point with the EOSP (End Of Service Period) flag set, at which point
 *  UniFi returns to sleep. However, if the access point is temporarily
 *  inaccessible, UniFi would stay awake indefinitely. Specifies a timeout
 *  starting from the point where the trigger frame has been sent. If the
 *  timeout expires and no data has been received from the access point,
 *  UniFi will behave as if the service period had been ended normally and
 *  return to sleep. This timeout takes precedence over
 *  unifiPowerSaveExtraListenTime if both would otherwise be applicable.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APSD_SERVICE_PERIOD_TIMEOUT 0x09D3

/*******************************************************************************
 * NAME          : UnifiConcurrentPowerManagementDelayTimeout
 * PSID          : 2516 (0x09D4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 10000
 * DESCRIPTION   :
 *  When UniFi enters power save mode it signals the new state by setting the
 *  power management bit in the frame control field of a NULL frame. It then
 *  remains active for the period since the previous unicast reception, or
 *  since the transmission of the NULL frame, whichever is later. This entry
 *  controls the maximum time during which UniFi will continue to listen for
 *  data. This allows any buffered data on a remote device to be cleared.
 *  This is same as unifiPowerManagementDelayTimeout but this value is
 *  considered only when we are doing multivif operations and other VIFs are
 *  waiting to be scheduled.Note that firmware automatically chooses one of
 *  unifiPowerManagementDelayTimeout and
 *  unifiConcurrentPowerManagementDelayTimeout depending upon the current
 *  situation.It is sensible to set unifiPowerManagementDelayTimeout to be
 *  always more thanunifiConcurrentPowerManagementDelayTimeout.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONCURRENT_POWER_MANAGEMENT_DELAY_TIMEOUT 0x09D4

/*******************************************************************************
 * NAME          : UnifiStationQosInfo
 * PSID          : 2517 (0x09D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  QoS capability for a non-AP Station, and is encoded as per IEEE 802.11
 *  QoS Capability.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STATION_QOS_INFO 0x09D5

/*******************************************************************************
 * NAME          : UnifiListenIntervalSkippingDtim
 * PSID          : 2518 (0x09D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000A89AA
 * DESCRIPTION   :
 *  Listen interval of beacons when in single-vif power saving mode,receiving
 *  DTIMs is enabled and idle mode disabled. No DTIMs are skipped during MVIF
 *  operation. A maximum of the listen interval beacons are skipped, which
 *  may be less than the number of DTIMs that can be skipped. The value is a
 *  lookup table for DTIM counts. Each 4bits, in LSB order, represent DTIM1,
 *  DTIM2, DTIM3, DTIM4, DTIM5, (unused). This key is only used for STA VIF,
 *  connected to an AP. For P2P group client intervals, refer to
 *  unifiP2PListenIntervalSkippingDTIM, PSID=2523.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL_SKIPPING_DTIM 0x09D6

/*******************************************************************************
 * NAME          : UnifiListenInterval
 * PSID          : 2519 (0x09D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Association request listen interval parameter in beacon intervals. Not
 *  used for any other purpose.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL 0x09D7

/*******************************************************************************
 * NAME          : UnifiLegacyPsPollTimeout
 * PSID          : 2520 (0x09D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 15000
 * DESCRIPTION   :
 *  Time we try to stay awake after sending a PS-POLL to receive data.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LEGACY_PS_POLL_TIMEOUT 0x09D8

/*******************************************************************************
 * NAME          : UnifiBeaconSkippingControl
 * PSID          : 2521 (0x09D9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00010101
 * DESCRIPTION   :
 *  Control beacon skipping behaviour within firmware with bit flags. 1
 *  defines enabled, with 0 showing the case disabled. If beacon skipping is
 *  enabled, further determine if DTIM beacons can be skipped, or only
 *  non-DTIM beacons. The following applies: bit 0: station skipping on host
 *  suspend bit 1: station skipping on host awake bit 2: station skipping on
 *  LCD on bit 3: station skipping with multivif bit 4: station skipping with
 *  BT active. bit 8: station skip dtim on host suspend bit 9: station skip
 *  dtim on host awake bit 10: station skip dtim on LCD on bit 11: station
 *  skip dtim on multivif bit 12: station skip dtim with BT active bit 16:
 *  p2p-gc skipping on host suspend bit 17: p2p-gc skipping on host awake bit
 *  18: p2p-gc skipping on LCD on bit 19: p2p-gc skipping with multivif bit
 *  20: p2p-gc skipping with BT active bit 24: p2p-gc skip dtim on host
 *  suspend bit 25: p2p-gc skip dtim on host awake bit 26: p2p-gc skip dtim
 *  on LCD on bit 27: p2p-gc skip dtim on multivif bit 28: p2p-gc skip dtim
 *  with BT active
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACON_SKIPPING_CONTROL 0x09D9

/*******************************************************************************
 * NAME          : UnifiTogglePowerDomain
 * PSID          : 2522 (0x09DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Toggle WLAN power domain when entering dorm mode (deep sleep). When
 *  entering deep sleep and this value it true, then the WLAN power domain is
 *  disabled for the deep sleep duration. When false, the power domain is
 *  left turned on. This is to work around issues with WLAN rx, and is
 *  considered temporary until the root cause is found and fixed.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TOGGLE_POWER_DOMAIN 0x09DA

/*******************************************************************************
 * NAME          : UnifiP2PListenIntervalSkippingDtim
 * PSID          : 2523 (0x09DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00000002
 * DESCRIPTION   :
 *  Listen interval of beacons when in single-vif, P2P client power saving
 *  mode,receiving DTIMs and idle mode disabled. No DTIMs are skipped during
 *  MVIF operation. A maximum of (listen interval - 1) beacons are skipped,
 *  which may be less than the number of DTIMs that can be skipped. The value
 *  is a lookup table for DTIM counts. Each 4bits, in LSB order, represent
 *  DTIM1, DTIM2, DTIM3, DTIM4, DTIM5, (unused). This key is only used for
 *  P2P group client. For STA connected to an AP, refer to
 *  unifiListenIntervalSkippingDTIM, PSID=2518.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_P2_PLISTEN_INTERVAL_SKIPPING_DTIM 0x09DB

/*******************************************************************************
 * NAME          : UnifiFragmentationDuration
 * PSID          : 2524 (0x09DC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  A limit on transmission time for a data frame. If the data payload would
 *  take longer than unifiFragmentationDuration to transmit, UniFi will
 *  attempt to fragment the frame to ensure that the data portion of each
 *  fragment is within the limit. The limit imposed by the fragmentation
 *  threshold is also respected, and no more than 16 fragments may be
 *  generated. If the value is zero no limit is imposed. The value may be
 *  changed dynamically during connections. Note that the limit is a
 *  guideline and may not always be respected. In particular, the data rate
 *  is finalised after fragmentation in order to ensure responsiveness to
 *  conditions, the calculation is not performed to high accuracy, and octets
 *  added during encryption are not included in the duration calculation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAGMENTATION_DURATION 0x09DC

/*******************************************************************************
 * NAME          : UnifiRoamSendInitFrameToGetNeighbors
 * PSID          : 2525 (0x09DD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: The firmware
 *  can send a Neighbor request/ BTM query to get AP's neighbors. This frame
 *  upsets certification setups. Setting this MIB to false inhibits sending a
 *  Neighbor request/ BTM query to get AP's neighbors.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SEND_INIT_FRAME_TO_GET_NEIGHBORS 0x09DD

/*******************************************************************************
 * NAME          : UnifiIdleModeLiteEnabled
 * PSID          : 2526 (0x09DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  DEPRECATED
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IDLE_MODE_LITE_ENABLED 0x09DE

/*******************************************************************************
 * NAME          : UnifiDtimWaitTimeout
 * PSID          : 2529 (0x09E1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50000
 * DESCRIPTION   :
 *  If UniFi is in power save and receives a Traffic Indication Map from its
 *  associated access point with a DTIM indication, it will wait a maximum
 *  time given by this attribute for succeeding broadcast or multicast
 *  traffic, or until it receives such traffic with the &apos;more data&apos;
 *  flag clear. Any reception of broadcast or multicast traffic with the
 *  &apos;more data&apos; flag set, or any reception of unicast data, resets
 *  the timeout. The timeout can be turned off by setting the value to zero;
 *  in that case UniFi will remain awake indefinitely waiting for broadcast
 *  or multicast data. Otherwise, the value should be larger than that of
 *  unifiPowerSaveExtraListenTime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DTIM_WAIT_TIMEOUT 0x09E1

/*******************************************************************************
 * NAME          : UnifiListenIntervalMaxTime
 * PSID          : 2530 (0x09E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Maximum number length of time, in Time Units (1TU = 1024us), that can be
 *  used as a beacon listen interval. This will limit how many beacons maybe
 *  skipped, and affects the DTIM beacon skipping count; DTIM skipping (if
 *  enabled) will be such that skipped count = (unifiListenIntervalMaxTime /
 *  DTIM_period).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL_MAX_TIME 0x09E2

/*******************************************************************************
 * NAME          : UnifiScanMaxProbeTransmitLifetime
 * PSID          : 2531 (0x09E3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 64
 * DESCRIPTION   :
 *  In TU. If non-zero, used during active scans as the maximum lifetime for
 *  probe requests. It is the elapsed time after the initial transmission at
 *  which further attempts to transmit the probe are terminated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_MAX_PROBE_TRANSMIT_LIFETIME 0x09E3

/*******************************************************************************
 * NAME          : UnifiPowerSaveTransitionPacketThreshold
 * PSID          : 2532 (0x09E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name:If VIF has
 *  this many packets queued/transmitted/received in last
 *  unifiFastPowerSaveTransitionPeriod then firmware may decide to come out
 *  of aggressive power save mode. This is applicable to STA/CLI and AP/GO
 *  VIFs. Note that this is only a guideline. Firmware internal factors may
 *  override this MIB. Also see unifiTrafficAnalysisPeriod and
 *  unifiAggressivePowerSaveTransitionPeriod.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POWER_SAVE_TRANSITION_PACKET_THRESHOLD 0x09E4

/*******************************************************************************
 * NAME          : UnifiProbeResponseLifetime
 * PSID          : 2533 (0x09E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Lifetime of proberesponse frame in unit of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PROBE_RESPONSE_LIFETIME 0x09E5

/*******************************************************************************
 * NAME          : UnifiProbeResponseMaxRetry
 * PSID          : 2534 (0x09E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Number of retries of probe response frame.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PROBE_RESPONSE_MAX_RETRY 0x09E6

/*******************************************************************************
 * NAME          : UnifiTrafficAnalysisPeriod
 * PSID          : 2535 (0x09E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 80
 * DESCRIPTION   :
 *  Period in TUs over which firmware counts number of packet
 *  transmitted/queued/received to make decisions like coming out of
 *  aggressive power save mode or setting up BlockAck. This is applicable to
 *  STA/CLI and AP/GO VIFs. Note that this is only a guideline. Firmware
 *  internal factors may override this MIB. Also see
 *  unifiPowerSaveTransitionPacketThreshold,
 *  unifiAggressivePowerSaveTransitionPeriod and
 *  unifiTrafficThresholdToSetupBA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TRAFFIC_ANALYSIS_PERIOD 0x09E7

/*******************************************************************************
 * NAME          : UnifiAggressivePowerSaveTransitionPeriod
 * PSID          : 2536 (0x09E8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Defines how many unifiTrafficAnalysisPeriod firmware should wait in which
 *  VIF had received/transmitted/queued less than
 *  unifiPowerSaveTransitionPacketThreshold packets - before entering
 *  aggressive power save mode (when not in aggressive power save mode) This
 *  is applicable to STA/CLI and AP/GO VIFs. Note that this is only a
 *  guideline. Firmware internal factors may override this MIB. Also see
 *  unifiPowerSaveTransitionPacketThreshold and unifiTrafficAnalysisPeriod.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AGGRESSIVE_POWER_SAVE_TRANSITION_PERIOD 0x09E8

/*******************************************************************************
 * NAME          : UnifiActiveTimeAfterMoreBit
 * PSID          : 2537 (0x09E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: After seeing
 *  the "more" bit set in a message from the AP, the STA will goto active
 *  mode for this duration of time. After this time, traffic information is
 *  evaluated to determine whether the STA should stay active or go to
 *  powersave. Setting this value to 0 means that the described functionality
 *  is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ACTIVE_TIME_AFTER_MORE_BIT 0x09E9

/*******************************************************************************
 * NAME          : UnifiDefaultDwellTime
 * PSID          : 2538 (0x09EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Dwell time, in TU, for frames that need a response but have no dwell time
 *  associated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEFAULT_DWELL_TIME 0x09EA

/*******************************************************************************
 * NAME          : UnifiVhtCapabilities
 * PSID          : 2540 (0x09EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 12
 * MAX           : 12
 * DEFAULT       : { 0XB1, 0X7A, 0X91, 0X03, 0XFA, 0XFF, 0X00, 0X00, 0XFA, 0XFF, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. VHT
 *  capabilities of the chip. see SC-503520-SP.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VHT_CAPABILITIES 0x09EC

/*******************************************************************************
 * NAME          : UnifiMaxVifScheduleDuration
 * PSID          : 2541 (0x09ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Default time for which a non-scan VIF can be scheduled. Applies to
 *  multiVIF scenario. Internal firmware logic or BSS state (e.g. NOA) may
 *  cut short the schedule.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_VIF_SCHEDULE_DURATION 0x09ED

/*******************************************************************************
 * NAME          : UnifiVifLongIntervalTime
 * PSID          : 2542 (0x09EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  When the scheduler expects a VIF to schedule for time longer than this
 *  parameter (specified in TUs), then the VIF may come out of powersave.
 *  Only valid for STA VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VIF_LONG_INTERVAL_TIME 0x09EE

/*******************************************************************************
 * NAME          : UnifiDisallowSchedRelinquish
 * PSID          : 2543 (0x09EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When enabled the VIFs will not relinquish their assigned schedules when
 *  they have nothing left to do.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISALLOW_SCHED_RELINQUISH 0x09EF

/*******************************************************************************
 * NAME          : UnifiRameDplaneOperationTimeout
 * PSID          : 2544 (0x09F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Timeout for requests sent from MACRAME to Data Plane. Any value below
 *  1000ms will be capped at 1000ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAME_DPLANE_OPERATION_TIMEOUT 0x09F0

/*******************************************************************************
 * NAME          : UnifiDebugKeepRadioOn
 * PSID          : 2545 (0x09F1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Keep the radio on. For debug purposes only. Setting the value to FALSE
 *  means radio on/off functionality will behave normally. Note that setting
 *  this value to TRUE will automatically disable dorm. The intention is
 * not* for this value to be changed at runtime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_KEEP_RADIO_ON 0x09F1

/*******************************************************************************
 * NAME          : UnifiForceFixedDurationSchedule
 * PSID          : 2546 (0x09F2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For schedules with fixed duration e.g. scan, unsync VIF, the schedule
 *  will be forced after this time to avoid VIF starving
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_FIXED_DURATION_SCHEDULE 0x09F2

/*******************************************************************************
 * NAME          : UnifiRameUpdateMibs
 * PSID          : 2547 (0x09F3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAME_UPDATE_MIBS 0x09F3

/*******************************************************************************
 * NAME          : UnifiGoScanAbsenceDuration
 * PSID          : 2548 (0x09F4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Duration of the Absence time to use when protecting P2PGO VIFs from scan
 *  operations. A value of 0 disables the feature.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GO_SCAN_ABSENCE_DURATION 0x09F4

/*******************************************************************************
 * NAME          : UnifiGoScanAbsencePeriod
 * PSID          : 2549 (0x09F5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Period of the Absence/Presence times cycles to use when protecting P2PGO
 *  VIFs from scan operations.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GO_SCAN_ABSENCE_PERIOD 0x09F5

/*******************************************************************************
 * NAME          : UnifiMaxClient
 * PSID          : 2550 (0x09F6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 10
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Restricts the maximum number of associated STAs for SoftAP.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_CLIENT 0x09F6

/*******************************************************************************
 * NAME          : UnifiGoctWindowDuration
 * PSID          : 2551 (0x09F7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Client Traffic Window in TU to be set for oppertunistic power save in
 *  P2PGO.This will help reduce the GO power consumption but may adversly
 *  affect P2PGO disconverability.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOCT_WINDOW_DURATION 0x09F7

/*******************************************************************************
 * NAME          : UnifiSuppressScanGoNoaAnnounce
 * PSID          : 2552 (0x09F8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Stop a p2p GO from sending NoA announcements when the GO needs to be
 *  absent for scanning purposes.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPRESS_SCAN_GO_NOA_ANNOUNCE 0x09F8

/*******************************************************************************
 * NAME          : UnifiTdlsInP2pActivated
 * PSID          : 2556 (0x09FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate TDLS in P2P.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_IN_P2P_ACTIVATED 0x09FC

/*******************************************************************************
 * NAME          : UnifiTdlsActivated
 * PSID          : 2558 (0x09FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Activate
 *  TDLS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_ACTIVATED 0x09FE

/*******************************************************************************
 * NAME          : UnifiTdlsTpThresholdPktSecs
 * PSID          : 2559 (0x09FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Used for "throughput_threshold_pktsecs" of
 *  RAME-MLME-ENABLE-PEER-TRAFFIC-REPORTING.request.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_TP_THRESHOLD_PKT_SECS 0x09FF

/*******************************************************************************
 * NAME          : UnifiTdlsRssiThreshold
 * PSID          : 2560 (0x0A00)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  FW initiated TDLS Discovery/Setup procedure. If the RSSI of a received
 *  TDLS Discovery Response frame is greater than this value, initiate the
 *  TDLS Setup procedure.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_RSSI_THRESHOLD 0x0A00

/*******************************************************************************
 * NAME          : UnifiTdlsMaximumRetry
 * PSID          : 2561 (0x0A01)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_MAXIMUM_RETRY 0x0A01

/*******************************************************************************
 * NAME          : UnifiTdlsTpMonitorSecs
 * PSID          : 2562 (0x0A02)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Measurement period for recording the number of packets sent to a peer
 *  over a TDLS link.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_TP_MONITOR_SECS 0x0A02

/*******************************************************************************
 * NAME          : UnifiTdlsBasicHtMcsSet
 * PSID          : 2563 (0x0A03)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_BASIC_HT_MCS_SET 0x0A03

/*******************************************************************************
 * NAME          : UnifiTdlsBasicVhtMcsSet
 * PSID          : 2564 (0x0A04)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_BASIC_VHT_MCS_SET 0x0A04

/*******************************************************************************
 * NAME          : Dot11TdlsDiscoveryRequestWindow
 * PSID          : 2565 (0x0A05)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Time to gate Discovery Request frame (in DTIM intervals) after
 *  transmitting a Discovery Request frame.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_DISCOVERY_REQUEST_WINDOW 0x0A05

/*******************************************************************************
 * NAME          : Dot11TdlsResponseTimeout
 * PSID          : 2566 (0x0A06)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  If a valid Setup Response frame is not received within (seconds), the
 *  initiator STA shall terminate the setup procedure and discard any Setup
 *  Response frames.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_RESPONSE_TIMEOUT 0x0A06

/*******************************************************************************
 * NAME          : Dot11TdlsChannelSwitchActivated
 * PSID          : 2567 (0x0A07)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_CHANNEL_SWITCH_ACTIVATED 0x0A07

/*******************************************************************************
 * NAME          : UnifiTdlsDesignForTestMode
 * PSID          : 2568 (0x0A08)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_DESIGN_FOR_TEST_MODE 0x0A08

/*******************************************************************************
 * NAME          : UnifiTdlsWiderBandwidthProhibited
 * PSID          : 2569 (0x0A09)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Wider bandwidth prohibited flag.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_WIDER_BANDWIDTH_PROHIBITED 0x0A09

/*******************************************************************************
 * NAME          : UnifiTdlsKeyLifeTimeInterval
 * PSID          : 2577 (0x0A11)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000FFFFF
 * DESCRIPTION   :
 *  Build the Key Lifetime Interval in the TDLS Setup Request frame.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_KEY_LIFE_TIME_INTERVAL 0x0A11

/*******************************************************************************
 * NAME          : UnifiTdlsTeardownFrameTxTimeout
 * PSID          : 2578 (0x0A12)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  Allowed time in milliseconds for a Teardown frame to be transmitted.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_TEARDOWN_FRAME_TX_TIMEOUT 0x0A12

/*******************************************************************************
 * NAME          : UnifiWifiSharingActivated
 * PSID          : 2580 (0x0A14)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate WiFi Sharing feature
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WIFI_SHARING_ACTIVATED 0x0A14

/*******************************************************************************
 * NAME          : UnifiWiFiSharingChannels
 * PSID          : 2581 (0x0A15)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0XFF, 0XDF, 0XFF, 0XFF, 0X7F, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Default host allowed channels during Wifi Sharing. Defined in a uint64
 *  represented by the octet string. First byte of the octet string maps to
 *  LSB. Bits 0-13 representing 2.4G channels. Mapping defined in
 *  ChannelisationRules.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WI_FI_SHARING_CHANNELS 0x0A15

/*******************************************************************************
 * NAME          : UnifiWiFiSharing5GHzChannel
 * PSID          : 2582 (0x0A16)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0X00, 0XC0, 0XFF, 0XFF, 0X7F, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Deprecated. Applicable 5GHz Primary Channels mask. Defined in a uint64
 *  represented by the octet string. First byte of the octet string maps to
 *  LSB. Bits 0-13 representing 2.4G channels are always set to 0. Mapping
 *  defined in ChannelisationRules; i.e. Bit 14 maps to channel 36.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WI_FI_SHARING5_GHZ_CHANNEL 0x0A16

/*******************************************************************************
 * NAME          : UnifiWifiSharingChannelSwitchCount
 * PSID          : 2583 (0x0A17)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3
 * MAX           : 10
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Channel switch announcement count which will be used in the Channel
 *  announcement IE when using wifi sharing
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WIFI_SHARING_CHANNEL_SWITCH_COUNT 0x0A17

/*******************************************************************************
 * NAME          : UnifiChannelAnnouncementCount
 * PSID          : 2584 (0x0A18)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Channel switch announcement count which will be used in the Channel
 *  announcement IE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHANNEL_ANNOUNCEMENT_COUNT 0x0A18

/*******************************************************************************
 * NAME          : UnifiRaTestStoredSa
 * PSID          : 2585 (0x0A19)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  Test only: Source address of router contained in virtural router
 *  advertisement packet, specified in chapter '6.2 Forward Received RA frame
 *  to Host' in SC-506393-TE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RA_TEST_STORED_SA 0x0A19

/*******************************************************************************
 * NAME          : UnifiRaTestStoreFrame
 * PSID          : 2586 (0x0A1A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  Test only: Virtual router advertisement packet. Specified in chapter '6.2
 *  Forward Received RA frame to Host' in SC-506393-TE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RA_TEST_STORE_FRAME 0x0A1A

/*******************************************************************************
 * NAME          : Dot11TdlsPeerUapsdBufferStaActivated
 * PSID          : 2587 (0x0A1B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate TDLS peer U-APSD.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_PEER_UAPSD_BUFFER_STA_ACTIVATED 0x0A1B

/*******************************************************************************
 * NAME          : UnifiWpA3Activated
 * PSID          : 2588 (0x0A1C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate WPA3 support.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WP_A3_ACTIVATED 0x0A1C

/*******************************************************************************
 * NAME          : UnifiArpOutstandingMax
 * PSID          : 2590 (0x0A1E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 32
 * DESCRIPTION   :
 *  Maximum number of outstanding ARP frame transmissions in the firmware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ARP_OUTSTANDING_MAX 0x0A1E

/*******************************************************************************
 * NAME          : UnifiP2PLegacyProbeResponseActivated
 * PSID          : 2599 (0x0A27)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete. When activated, P2P-GO responds to
 *  legacy Probe Request.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_P2_PLEGACY_PROBE_RESPONSE_ACTIVATED 0x0A27

/*******************************************************************************
 * NAME          : UnifiProbeResponseLifetimeP2p
 * PSID          : 2600 (0x0A28)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  Lifetime of proberesponse frame in unit of ms for P2P.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PROBE_RESPONSE_LIFETIME_P2P 0x0A28

/*******************************************************************************
 * NAME          : UnifiStaChannelSwitchSlowApActivated
 * PSID          : 2601 (0x0A29)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: ChanelSwitch:
 *  Activate waiting for a slow AP.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_CHANNEL_SWITCH_SLOW_AP_ACTIVATED 0x0A29

/*******************************************************************************
 * NAME          : UnifiStaChannelSwitchSlowApMaxTime
 * PSID          : 2604 (0x0A2C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 70
 * DESCRIPTION   :
 *  ChannelSwitch delay for Slow APs. In Seconds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_CHANNEL_SWITCH_SLOW_AP_MAX_TIME 0x0A2C

/*******************************************************************************
 * NAME          : UnifiStaChannelSwitchSlowApPollInterval
 * PSID          : 2605 (0x0A2D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  ChannelSwitch polling interval for Slow APs. In Seconds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_CHANNEL_SWITCH_SLOW_AP_POLL_INTERVAL 0x0A2D

/*******************************************************************************
 * NAME          : UnifiStaChannelSwitchSlowApProcedureTimeoutIncrement
 * PSID          : 2606 (0x0A2E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  ChannelSwitch procedure timeout increment for Slow APs. In Seconds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_CHANNEL_SWITCH_SLOW_AP_PROCEDURE_TIMEOUT_INCREMENT 0x0A2E

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaxAerials
 * PSID          : 2607 (0x0A2F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. Limit the
 *  number of Aerials that Scan will use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAX_AERIALS 0x0A2F

/*******************************************************************************
 * NAME          : UnifiIncludePmkidForPsk
 * PSID          : 2610 (0x0A32)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Include PMKID in frame if security type WPA2-PSK.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_INCLUDE_PMKID_FOR_PSK 0x0A32

/*******************************************************************************
 * NAME          : UnifiAmsduSubframeRealtekOverrideActivated
 * PSID          : 2615 (0x0A37)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When connected to Realtek chipset AP, inform Dataplane that actions need
 *  to be taken.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AMSDU_SUBFRAME_REALTEK_OVERRIDE_ACTIVATED 0x0A37

/*******************************************************************************
 * NAME          : UnifiSuppressStsUpdateBasedOnPeer
 * PSID          : 2616 (0x0A38)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Do not update
 *  STS in VHT caps based on Peer's sounding dimensions.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPRESS_STS_UPDATE_BASED_ON_PEER 0x0A38

/*******************************************************************************
 * NAME          : UnifiBaRxInternalDeleteDisabled
 * PSID          : 2618 (0x0A3A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Disable Rx Block Ack from being deleted internally. The BA delete is
 *  still advertised to the peer, but is not deleted internally. This is
 *  applicable only when the delete does not come from the peer.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_RX_INTERNAL_DELETE_DISABLED 0x0A3A

/*******************************************************************************
 * NAME          : UnifiHtApDowngradeByNssActivated
 * PSID          : 2619 (0x0A3B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable HT Ap downgrade when NSS is changed. This is customer specific.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_AP_DOWNGRADE_BY_NSS_ACTIVATED 0x0A3B

/*******************************************************************************
 * NAME          : UnifiBtmDisassociationScanDelay
 * PSID          : 2639 (0x0A4F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Delay roaming scan to the expiration of Dissassociation Timer when
 *  disassociation is imminent.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BTM_DISASSOCIATION_SCAN_DELAY 0x0A4F

/*******************************************************************************
 * NAME          : UnifiRoamBtmQueryTimeout
 * PSID          : 2640 (0x0A50)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : second
 * MIN           : 0
 * MAX           : 10
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Wait time in seconds for a response to BTM Query.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BTM_QUERY_TIMEOUT 0x0A50

/*******************************************************************************
 * NAME          : UnifiRoamBsscuTriggerFrequency
 * PSID          : 2641 (0x0A51)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  How long, in seconds, should the BSS load be high before triggering
 *  roaming?
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_BSSCU_TRIGGER_FREQUENCY 0x0A51

/*******************************************************************************
 * NAME          : UnifiApfActivated
 * PSID          : 2650 (0x0A5A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  It is used to enable or disable Android Packet Filter(APF).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APF_ACTIVATED 0x0A5A

/*******************************************************************************
 * NAME          : UnifiApfVersion
 * PSID          : 2651 (0x0A5B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  APF version currently supported by the FW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APF_VERSION 0x0A5B

/*******************************************************************************
 * NAME          : UnifiApfMaxSize
 * PSID          : 2652 (0x0A5C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1024
 * DESCRIPTION   :
 *  Max size in bytes supported by FW per VIF. Includes both program len and
 *  data len.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APF_MAX_SIZE 0x0A5C

/*******************************************************************************
 * NAME          : UnifiApfActiveModeEnabled
 * PSID          : 2653 (0x0A5D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Indicates if APF is supported in host active mode. Applicable to only
 *  group addressed frames.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APF_ACTIVE_MODE_ENABLED 0x0A5D

/*******************************************************************************
 * NAME          : UnifiFrameResponseNdmTimeout
 * PSID          : 2654 (0x0A5E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1024
 * DESCRIPTION   :
 *  Timeout, in TU, to wait to retry a frame after after TX Cfm
 *  trasnmission_status == Successful.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_NDM_TIMEOUT 0x0A5E

/*******************************************************************************
 * NAME          : UnifiHeActivated
 * PSID          : 2700 (0x0A8C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables HE mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HE_ACTIVATED 0x0A8C

/*******************************************************************************
 * NAME          : UnifiHeCapabilities
 * PSID          : 2701 (0x0A8D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 28
 * MAX           : 28
 * DEFAULT       : { 0X03, 0X08, 0X88, 0X00, 0X00, 0X00, 0X26, 0X70, 0X42, 0X09, 0X0D, 0X01, 0XA4, 0X0A, 0X00, 0XBD, 0X00, 0XFA, 0XFF, 0XFA, 0XFF, 0X79, 0X1C, 0XC7, 0X71, 0X1C, 0XC7, 0X71 }
 * DESCRIPTION   :
 *  HE capabilities of chip. This includes HE MAC capabilities information,
 *  HE PHY capabilities information, Supported HE-MCS and NSS set, PPE
 *  thresholds(optional) fields.see SC-XXXXXX-SP.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HE_CAPABILITIES 0x0A8D

/*******************************************************************************
 * NAME          : UnifiRaaTxGiValue
 * PSID          : 2702 (0x0A8E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  HE GI value set by Host".
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_GI_VALUE 0x0A8E

/*******************************************************************************
 * NAME          : UnifiRaaTxLtfValue
 * PSID          : 2703 (0x0A8F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  HE LTF value set by host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_LTF_VALUE 0x0A8F

/*******************************************************************************
 * NAME          : Unifi256MsduConfig
 * PSID          : 2704 (0x0A90)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  256 MSDU support.
 *******************************************************************************/
#define SLSI_PSID_UNIFI256_MSDU_CONFIG 0x0A90

/*******************************************************************************
 * NAME          : UnifiMbssidActivated
 * PSID          : 2705 (0x0A91)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Multiple Bssid.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MBSSID_ACTIVATED 0x0A91

/*******************************************************************************
 * NAME          : UnifiTxAmsduSupportOverride
 * PSID          : 2706 (0x0A92)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Param to override tx amsdu support
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_AMSDU_SUPPORT_OVERRIDE 0x0A92

/*******************************************************************************
 * NAME          : UnifiMbssidRoamingActivated
 * PSID          : 2707 (0x0A93)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Multiple Bssid roaming.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MBSSID_ROAMING_ACTIVATED 0x0A93

/*******************************************************************************
 * NAME          : UnifiDlMumimoNcWar
 * PSID          : 2708 (0x0A94)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable DL MUMIMO WAR for dynamic configuration of nc value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DL_MUMIMO_NC_WAR 0x0A94

/*******************************************************************************
 * NAME          : UnifiHe1xltf08giWorkaround
 * PSID          : 2709 (0x0A95)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Param to disable workaround logic for station support of HE 1x ltf and
 *  0.8us gi in its association request frame. Some APs send HE SU PPDUs at
 *  1x ltf and 0.8us gi even though they do not support 1x ltf and 0.8us gi
 *  if peer advertises that it supports HE SU PPDUs at 1x ltf and 0.8us gi.
 *  If this param is set to true and if ap advertises that it does not
 *  support 1x ltf and 0.8us gi, station will disable support for 1x ltf and
 *  0.8us gi in HE capabilities in its association request frame. If this
 *  param is set to false, station will advertise support for 1x ltf and
 *  0.8us gi in HE capabilities in its association request frame based on its
 *  local capability only.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HE1XLTF08GI_WORKAROUND 0x0A95

/*******************************************************************************
 * NAME          : UnifiOverrideMuedcaParamAcEnable
 * PSID          : 2710 (0x0A96)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 31
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  override STA - AC MU-EDCA using the values in unifiOverrideMUEDCAParamAC
 *  MIB, useful for internal testing. Bit 0 - 0 means disable M-EDCA feature
 *  completely and always use legacy EDCA parameters. Bit 1 - if set to 1
 *  means ignore MUEDCA parameters for BE that is being broadcasted by AP and
 *  overwrite it with MU EDCA parameters for BE specified in MIB variable
 *  unifiOverrideMUEDCAParamAC. Bit 2 - if set to 1 means ignore MUEDCA
 *  parameters for BK that is being broadcasted by AP and overwrite it with
 *  MU EDCA parameters for BK specified in MIB variable
 *  unifiOverrideMUEDCAParamAC. Bit 3 - if set to 1 means ignore MUEDCA
 *  parameters for VI that is being broadcasted by AP and overwrite it with
 *  MU EDCA parameters for VI specified in MIB variable
 *  unifiOverrideMUEDCAParamAC. Bit 4 - if set to 1 means ignore MUEDCA
 *  parameters for VO that is being broadcasted by AP and overwrite it with
 *  MU EDCA parameters for VO specified in MIB variable
 *  unifiOverrideMUEDCAParamAC.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_MUEDCA_PARAM_AC_ENABLE 0x0A96

/*******************************************************************************
 * NAME          : UnifiOverrideMuedcaParamAc
 * PSID          : 2711 (0x0A97)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 12
 * MAX           : 12
 * DEFAULT       : {0X02, 0X33, 0X06, 0X02, 0XA4, 0X06, 0X02, 0X44, 0X06, 0X02, 0X32, 0X06}
 * DESCRIPTION   :
 *  Override the MU EDCA parameters. octet 0 - BE AIFS octet 1 - [7:4] ECW
 *  MAX [3:0] ECW MIN octet 2 - MU EDCA timer for BE in 8 TU units. octet 3 -
 *  BK AIFS octet 4 - [7:4] ECW MAX [3:0] ECW MIN octet 5 - MU EDCA timer for
 *  BK in 8 TU units. octet 6 - VI AIFS octet 7 - [7:4] ECW MAX [3:0] ECW MIN
 *  octet 8 - MU EDCA timer for VI in 8 TU units. octet 9 - VO AIFS octet 10
 *  - [7:4] ECW MAX [3:0] ECW MIN octet 11 - MU EDCA timer for VO in 8 TU
 *  units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_MUEDCA_PARAM_AC 0x0A97

/*******************************************************************************
 * NAME          : UnifiMuedcaTimerUnit
 * PSID          : 2712 (0x0A98)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  MU EDCA Timer Unit
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MUEDCA_TIMER_UNIT 0x0A98

/*******************************************************************************
 * NAME          : UnifiUlmuDisableBitMap
 * PSID          : 2713 (0x0A99)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Disable UL MU in various modes. Input: 1 (i.e. BIT0) : Disable UL MU if
 *  Bluetooth is on 2 (i.e BIT1) : Disable UL MU if Same band multi-vif
 *  detected 4 (i.e. BIT2) : Disable UL MU if TDLS is active 0: Enable by
 *  default
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ULMU_DISABLE_BIT_MAP 0x0A99

/*******************************************************************************
 * NAME          : UnifiOmTxNstsValue
 * PSID          : 2715 (0x0A9B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Tx NSTS value set by Host
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OM_TX_NSTS_VALUE 0x0A9B

/*******************************************************************************
 * NAME          : UnifiOmRxNssValue
 * PSID          : 2716 (0x0A9C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Rx NSS value set by Host
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OM_RX_NSS_VALUE 0x0A9C

/*******************************************************************************
 * NAME          : UnifiOmChanBandwidthValue
 * PSID          : 2717 (0x0A9D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Operating channel bandwidth value set by Host
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OM_CHAN_BANDWIDTH_VALUE 0x0A9D

/*******************************************************************************
 * NAME          : UnifiOmUlMuEnable
 * PSID          : 2718 (0x0A9E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  UL MU/UL MU Data enable/disable
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OM_UL_MU_ENABLE 0x0A9E

/*******************************************************************************
 * NAME          : UnifiOpModeValue
 * PSID          : 2719 (0x0A9F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0XDEAD
 * DESCRIPTION   :
 *  Operating Mode control param to add in OMI frame
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OP_MODE_VALUE 0x0A9F

/*******************************************************************************
 * NAME          : UnifiOfdmaNarrowBwRuDisableTime
 * PSID          : 2720 (0x0AA0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2929
 * DEFAULT       : 2929
 * DESCRIPTION   :
 *  Minimum Time (3 Minute, 2929TU) that needs to pass since the reception of
 *  last beacon from OBSS AP that disable Narrow BW RU.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OFDMA_NARROW_BW_RU_DISABLE_TIME 0x0AA0

/*******************************************************************************
 * NAME          : UnifiUlMuMimoSupportOverride
 * PSID          : 2721 (0x0AA1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Param to override UL MU support
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UL_MU_MIMO_SUPPORT_OVERRIDE 0x0AA1

/*******************************************************************************
 * NAME          : UnifiTwtActivated
 * PSID          : 2731 (0x0AAB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable TWT feature
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_ACTIVATED 0x0AAB

/*******************************************************************************
 * NAME          : UnifiTwtControlFlags
 * PSID          : 2732 (0x0AAC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Control flags for TWT Input: BIT0 : Broadcast TWT Support BIT1 : Enable
 *  TWT Service period skip if LCD OFF BIT2 : Enable aggressive TWT Service
 *  period powerdown
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_CONTROL_FLAGS 0x0AAC

/*******************************************************************************
 * NAME          : UnifiTwtIndDefSessIdx
 * PSID          : 2733 (0x0AAD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Default individual TWT session index to be used from
 *  unifiTWTIndSessParamTable.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_IND_DEF_SESS_IDX 0x0AAD

/*******************************************************************************
 * NAME          : UnifiTwtStartSession
 * PSID          : 2734 (0x0AAE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  Start TWT session. Input: 0: Broadcast TWT 1 to 2: Individual TWT session
 *  255: Disable
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_START_SESSION 0x0AAE

/*******************************************************************************
 * NAME          : UnifiTwtTeardownSession
 * PSID          : 2735 (0x0AAF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       :
 * DESCRIPTION   :
 *  Teardown TWT session. Input: flow id
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_TEARDOWN_SESSION 0x0AAF

/*******************************************************************************
 * NAME          : UnifiTwtIndSessParam
 * PSID          : 2736 (0x0AB0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 7
 * MAX           : 7
 * DEFAULT       :
 * DESCRIPTION   :
 *  Individual TWT session parameters. Supports multiple sessions. Each row
 *  of the table contains session parameters for a individual TWT sessions.
 *  Entry has the following structure: Octet 0: Control field bit 0: NDP
 *  paging indicator bit 1: Responder PM mode bit 2-3: Negotiation type bit
 *  4: TWT information frame disabled. bit 5-7: Reserved Octet 1: bit 0: TWT
 *  request bit 1: Trigger bit 2: Implicit bit 3: Flowtype bit 4: TWT
 *  protection bit 5-7: TWT Setup command Octet 2: bit 0-4: TWT Wake Interval
 *  Exponent bit 5: TWT channel bit 6-7: Reserved Octet 3: Nominal Minimum
 *  TWT wake duration Octet 4-5: TWT Wake Interval Mantissa Octet 6: TWT wake
 *  time offset from next beacon tbtt in milliseconds. Size of each entry is
 *  7 Octets.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_IND_SESS_PARAM 0x0AB0

/*******************************************************************************
 * NAME          : UnifiTwtModeSupportBitMap
 * PSID          : 2737 (0x0AB1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  TWT support in various modes. Input: 1 (i.e. BIT0) : TWT Support in
 *  Active Mode 2 (i.e BIT1) : TWT Support in Host Suspend Mode 4 (i.e. BIT2)
 *  : TWT Support in BT Coex Mode 0: Disable TWT in Active Mode uses Row 1
 *  params in unifiTWTIndSessParamTable TWT in Host Suspend Mode uses Row 2
 *  params in unifiTWTIndSessParamTable and so on
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_MODE_SUPPORT_BIT_MAP 0x0AB1

/*******************************************************************************
 * NAME          : UnifiTwtHostSuspTwtTeardownDelay
 * PSID          : 2738 (0x0AB2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Teardown host suspend TWT session after this delay. Delay in msec
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_HOST_SUSP_TWT_TEARDOWN_DELAY 0x0AB2

/*******************************************************************************
 * NAME          : UnifiTwtspInactivityTime
 * PSID          : 2739 (0x0AB3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10000
 * DESCRIPTION   :
 *  Inactivity time for terminating current service period in usec
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWTSP_INACTIVITY_TIME 0x0AB3

/*******************************************************************************
 * NAME          : UnifiTwtResumeDuration
 * PSID          : 2740 (0x0AB4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This value is in microseconds and is used to compute TWT information
 *  frame Next TWT subfield value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_RESUME_DURATION 0x0AB4

/*******************************************************************************
 * NAME          : UnifiTwtOperation
 * PSID          : 2741 (0x0AB5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       :
 * DESCRIPTION   :
 *  Set to 1 for TWT suspend and set to 2 for TWT resume
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_OPERATION 0x0AB5

/*******************************************************************************
 * NAME          : UnifiTwtInfoFrameTx
 * PSID          : 2742 (0x0AB6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable/disable transmission of TWT information frames
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TWT_INFO_FRAME_TX 0x0AB6

/*******************************************************************************
 * NAME          : UnifiUserControlBaSession
 * PSID          : 2799 (0x0AEF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 9
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00}
 * DESCRIPTION   :
 *  User control block ack session parameters octet 0 - action to be
 *  performed, tid and block ack session role bit [2:0] : action to be
 *  performed. if set to 0: add a block ack session if set to 1: delete a
 *  block ack session if set to 2: force reject incoming block ack session if
 *  set to 3: can initiate a block ack session if set to 4: can accept an
 *  incoming block ack session bit [7:3] : unused octet 1 - tid bit [2:0] :
 *  tid if set to 0: tid 0 if set to 1: tid 1 if set to 2: tid 2 if set to 3:
 *  tid 3 if set to 4: tid 4 if set to 5: tid 5 if set to 6: tid 6 if set to
 *  7: tid 7 bit [7:3] : unused octet 2 - block ack session role bit 0 : if
 *  set to 0: block ack initiator if set to 1: block ack responder bit [7:1]
 *  : unused octet 3-8 - peer MAC address
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USER_CONTROL_BA_SESSION 0x0AEF

/*******************************************************************************
 * NAME          : UnifiMlmestaKeepAliveFailure
 * PSID          : 2842 (0x0B1A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Link lost by keep alive failure. 0 = Disabled. This is required by MCD.
 *  This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLMESTA_KEEP_ALIVE_FAILURE 0x0B1A

/*******************************************************************************
 * NAME          : UnifiCsrOnlyMibShield
 * PSID          : 4001 (0x0FA1)
 * PER INTERFACE?: NO
 * TYPE          : unifiCSROnlyMIBShield
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Each element of the MIB has a set of read/write access constraints that
 *  may be applied when the element is accessed by the host. For most
 *  elements the constants are derived from their MAX-ACCESS clauses.
 *  unifiCSROnlyMIBShield controls the access mechanism. If this entry is set
 *  to &apos;warn&apos;, when the host makes an inappropriate access to a MIB
 *  variable (e.g., writing to a &apos;read-only&apos; entry) then the
 *  firmware attempts to send a warning message to the host, but access is
 *  allowed to the MIB variable. If this entry is set to &apos;guard&apos;
 *  then inappropriate accesses from the host are prevented. If this entry is
 *  set to &apos;alarm&apos; then inappropriate accesses from the host are
 *  prevented and the firmware attempts to send warning messages to the host.
 *  If this entry is set to &apos;open&apos; then no access constraints are
 *  applied and now warnings issued. Note that certain MIB entries have
 *  further protection schemes. In particular, the MIB prevents the host from
 *  reading some security keys (WEP keys, etc.).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CSR_ONLY_MIB_SHIELD 0x0FA1

/*******************************************************************************
 * NAME          : UnifiPrivateBbbTxFilterConfig
 * PSID          : 4071 (0x0FE7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X17
 * DESCRIPTION   :
 *  entry is written directly to the BBB_TX_FILTER_CONFIG register. Only the
 *  lower eight bits of this register are implemented . Bits 0-3 are the
 *  &apos;Tx Gain&apos;, bits 6-8 are the &apos;Tx Delay&apos;. This register
 *  should only be changed by an expert.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_BBB_TX_FILTER_CONFIG 0x0FE7

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcFrontEndGain
 * PSID          : 4075 (0x0FEB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Gain of the path between chip and antenna when LNA is on.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_FRONT_END_GAIN 0x0FEB

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcFrontEndLoss
 * PSID          : 4076 (0x0FEC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Loss of the path between chip and antenna when LNA is off.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_FRONT_END_LOSS 0x0FEC

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcExtThresh
 * PSID          : 4077 (0x0FED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -25
 * DESCRIPTION   :
 *  Signal level at which external LNA will be used for AGC purposes.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_EXT_THRESH 0x0FED

/*******************************************************************************
 * NAME          : UnifiCsrOnlyPowerCalDelay
 * PSID          : 4078 (0x0FEE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Delay applied at each step of the power calibration routine used with an
 *  external PA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CSR_ONLY_POWER_CAL_DELAY 0x0FEE

/*******************************************************************************
 * NAME          : UnifiRxAgcControl
 * PSID          : 4079 (0x0FEF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 11
 * DEFAULT       :
 * DESCRIPTION   :
 *  Override the AGC by adjusting the Rx minimum and maximum gains of each
 *  stage. Set requests write the values to a static structure in firmware.
 *  The saved values are written to hardware, and also used to configure the
 *  AGC whenever halradio_agc_setup() is called. Get requests read the values
 *  from the static structure in firmware. AGC enables are not altered. Fixed
 *  gain may be tested by setting the minimums and maximums to the same
 *  value. Version 1 (for rfchip jar) octet 0 - Version number = 1. Gain
 *  values. Default in brackets. octet 1 - 5G LNA minimum gain (0). octet 2 -
 *  5G LNA maximum gain (4). octet 3 - 2G LNA minimum gain (0). octet 4 - 2G
 *  LNA maximum gain (5). octet 5 - Mixer minimum gain (0). octet 6 - Mixer
 *  maximum gain (2). octet 7 - ABB minimum gain (8). octet 8 - ABB maximum
 *  gain (27). octet 9 - Digital minimum gain (depends on bb chip). octet 10
 *  - Digital maximum gain (depends on bb chip). Version 2 (for rfchip
 *  hopper, lark) octet 0 - Version number = 2. Gain values. Default in
 *  brackets. octet 1 - 5G FE minimum gain (hopper 1, lark 0). octet 2 - 5G
 *  FE maximum gain (8). octet 3 - 2G FE minimum gain (0). octet 4 - 2G FE
 *  maximum gain (8). octet 5 - ABB minimum gain (1). octet 6 - ABB maximum
 *  gain (9). octet 7 - Digital minimum gain (depends on bb chip). octet 8 -
 *  Digital maximum gain (depends on bb chip).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_AGC_CONTROL 0x0FEF

/*******************************************************************************
 * NAME          : deprecated_unifiWapiQosMask
 * PSID          : 4130 (0x1022)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 15
 * DESCRIPTION   :
 *  Forces the WAPI encryption hardware use the QoS mask specified.
 *******************************************************************************/
#define SLSI_PSID_DEPRECATED_UNIFI_WAPI_QOS_MASK 0x1022

/*******************************************************************************
 * NAME          : UnifiWmmStallEnable
 * PSID          : 4139 (0x102B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Enable
 *  workaround stall WMM traffic if the admitted time has been used up, used
 *  for certtification.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WMM_STALL_ENABLE 0x102B

/*******************************************************************************
 * NAME          : UnifiDeactivateInessentialDebug
 * PSID          : 4140 (0x102C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Allow Deactivate Inessential debug in high TRx throughput condition -
 *  default value is false.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEACTIVATE_INESSENTIAL_DEBUG 0x102C

/*******************************************************************************
 * NAME          : UnifiRaaTxHostRate
 * PSID          : 4148 (0x1034)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Fixed TX rate set by Host. Ideally this should be done by the driver. 0
 *  means "host did not specified any rate".
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_HOST_RATE 0x1034

/*******************************************************************************
 * NAME          : UnifiFallbackShortFrameRetryDistribution
 * PSID          : 4149 (0x1035)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 5
 * DEFAULT       : {0X3, 0X2, 0X2, 0X2, 0X1, 0X0}
 * DESCRIPTION   :
 *  Configure the retry distribution for fallback for short frames octet 0 -
 *  Number of retries for starting rate. octet 1 - Number of retries for next
 *  rate. octet 2 - Number of retries for next rate. octet 3 - Number of
 *  retries for next rate. octet 4 - Number of retries for next rate. octet 5
 *  - Number of retries for last rate. If 0 is written to an entry then the
 *  retries for that rate will be the short retry limit minus the sum of the
 *  retries for each rate above that entry (e.g. 15 - 5). Therefore, this
 *  should always be the value for octet 4. Also, when the starting rate has
 *  short guard enabled, the number of retries in octet 1 will be used and
 *  for the next rate in the fallback table (same MCS value, but with sgi
 *  disabled) octet 0 number of retries will be used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FALLBACK_SHORT_FRAME_RETRY_DISTRIBUTION 0x1035

/*******************************************************************************
 * NAME          : UnifiThroughputLow
 * PSID          : 4150 (0x1036)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 37500000
 * DESCRIPTION   :
 *  Lower threshold for number of bytes received in a second - default value
 *  based on 300Mbps
 *******************************************************************************/
#define SLSI_PSID_UNIFI_THROUGHPUT_LOW 0x1036

/*******************************************************************************
 * NAME          : UnifiThroughputHigh
 * PSID          : 4151 (0x1037)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 50000000
 * DESCRIPTION   :
 *  Upper threshold for number of bytes received in a second - default value
 *  based on 400Mbps
 *******************************************************************************/
#define SLSI_PSID_UNIFI_THROUGHPUT_HIGH 0x1037

/*******************************************************************************
 * NAME          : UnifiSetFixedAmpduAggregationSize
 * PSID          : 4152 (0x1038)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  A non 0 value defines the max number of mpdus that a ampdu can have. A 0
 *  value tells FW to manage the aggregation size.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SET_FIXED_AMPDU_AGGREGATION_SIZE 0x1038

/*******************************************************************************
 * NAME          : UnifiThroughputDebugReportInterval
 * PSID          : 4153 (0x1039)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  dataplane reports throughput diag report every this interval in msec. 0
 *  means to disable this report.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_THROUGHPUT_DEBUG_REPORT_INTERVAL 0x1039

/*******************************************************************************
 * NAME          : UnifiDplaneTest1
 * PSID          : 4154 (0x103A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Dplane test mib to read and write uint32
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TEST1 0x103A

/*******************************************************************************
 * NAME          : UnifiDplaneTest2
 * PSID          : 4155 (0x103B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Dplane test mib to read and write uint32
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TEST2 0x103B

/*******************************************************************************
 * NAME          : UnifiDplaneTest3
 * PSID          : 4156 (0x103C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Dplane test mib to read and write uint32
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TEST3 0x103C

/*******************************************************************************
 * NAME          : UnifiDplaneTest4
 * PSID          : 4157 (0x103D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Dplane test mib to read and write uint32
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_TEST4 0x103D

/*******************************************************************************
 * NAME          : UnifiEnableCopyPhyLogToDram
 * PSID          : 4158 (0x103E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enable or disable copying phy event logs in SRAM to DRAM
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_COPY_PHY_LOG_TO_DRAM 0x103E

/*******************************************************************************
 * NAME          : UnifiLaaFixedMaxBandwidth
 * PSID          : 4159 (0x103F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Fix maximum allowed bandwidth for transmission. - 20: allowed up to 20MHz
 *  - 40: allowed up to 40MHz - 80: allowed up to 80MHz - others: maximum
 *  bandwidth is not fixed
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_FIXED_MAX_BANDWIDTH 0x103F

/*******************************************************************************
 * NAME          : UnifiDplaneDeafDetection
 * PSID          : 4160 (0x1040)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Configuration for Deaf detection algorithm. This feature only becomes
 *  enabled when it is FlexiMac and FM_DEAF_DETECTION flag is set. Usage:
 *  octet 0 - the number of consecutive cts without data frames to detect as
 *  deafness octet 1 - the number of consecutive rts without cts to detect as
 *  deafness octet 2 and 3 are not used 0 means disabling it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_DEAF_DETECTION 0x1040

/*******************************************************************************
 * NAME          : UnifiLaaLocalBwControlEnable
 * PSID          : 4161 (0x1041)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  On the platform which supports secondary channel monitoring such as
 *  FlexiMAC, LAA can automatically adjust maximum allowed bandwidth to avoid
 *  long-delayed transmission due to busy secondary channels.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_LOCAL_BW_CONTROL_ENABLE 0x1041

/*******************************************************************************
 * NAME          : UnifiLaaLocalBwBusyWeigh
 * PSID          : 4162 (0x1042)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Weigh for busy secondary channel in LAA local BW control algorithm. 0
 *  means this value is not applied to firmware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_LOCAL_BW_BUSY_WEIGH 0x1042

/*******************************************************************************
 * NAME          : UnifiLaaLocalBwBusyLevelThres
 * PSID          : 4163 (0x1043)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Busy level threshold by which LAA local BW control drops bandwidth. 0
 *  means this value is not applied to firmware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_LOCAL_BW_BUSY_LEVEL_THRES 0x1043

/*******************************************************************************
 * NAME          : UnifiLaaMinMcsForBwChange
 * PSID          : 4164 (0x1044)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  LAA considers to drop bandwidth when MCS drops and hits the configured
 *  value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LAA_MIN_MCS_FOR_BW_CHANGE 0x1044

/*******************************************************************************
 * NAME          : UnifiDphpHwMonitorLogEnable
 * PSID          : 4165 (0x1045)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enable/Disable DPHP HW Monitor Log
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPHP_HW_MONITOR_LOG_ENABLE 0x1045

/*******************************************************************************
 * NAME          : UnifiPhyEventToUdiMinInterval
 * PSID          : 4166 (0x1046)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Minimal gap in msec from the last PHY Event Logs to UDI. PHY logs are
 *  generated only when the gap from the last logging is larger than MIB
 *  value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PHY_EVENT_TO_UDI_MIN_INTERVAL 0x1046

/*******************************************************************************
 * NAME          : UnifiPhyEventToUdiMinBeaconMissInRow
 * PSID          : 4167 (0x1047)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Minimal beacon misses in a row to start PHY logging on beacon misses.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PHY_EVENT_TO_UDI_MIN_BEACON_MISS_IN_ROW 0x1047

/*******************************************************************************
 * NAME          : UnifiPhyEventToUdiConfig
 * PSID          : 4168 (0x1048)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X000E
 * DESCRIPTION   :
 *  Configure events that trigger sending PHY Event logs to UDI This feature
 *  is enabled when DPHP_DEBUG_PHY_ENABLE is defined. Bitmap for configuring
 *  the trigger condition: 0x0000 : disabled 0x0001 : periodically 0x0002 :
 *  tx_failure 0x0004 : ba_missed 0x0008 : live_restart
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PHY_EVENT_TO_UDI_CONFIG 0x1048

/*******************************************************************************
 * NAME          : UnifiPhyEventToUdiInterval
 * PSID          : 4169 (0x1049)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Interval (milliseconds) to send PHY Event Logs to UDI Trigger condition
 *  is controlled by unifiPhyEventToUDIControl This feature is enabled when
 *  DPHP_DEBUG_PHY_ENABLE is defined.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PHY_EVENT_TO_UDI_INTERVAL 0x1049

/*******************************************************************************
 * NAME          : UnifiMonitorStaAddress
 * PSID          : 4170 (0x104A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 6
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  MAC address of a target station to be captured in monitor (sniffer) mode.
 *  Monitor mode captures all received frames on a best-effort basis but
 *  there are certain frames which require pre-configuration before
 *  capturing. This feature will allow to capture HE-MU OFDMA frame by
 *  setting corresponding AID to the target station. Monitor vif uses this
 *  target station address to retrieve AID from AID-contained-frames
 *  automatically, e.g. assoc resp and NDPA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MONITOR_STA_ADDRESS 0x104A

/*******************************************************************************
 * NAME          : UnifiPreEbrtWindow
 * PSID          : 4171 (0x104B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  Latest time before the expected beacon reception time that UniFi will
 *  turn on its radio in order to receive the beacon. Reducing this value can
 *  reduce UniFi power consumption when using low power modes, however a
 *  value which is too small may cause beacons to be missed, requiring the
 *  radio to remain on for longer periods to ensure reception of the
 *  subsequent beacon.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRE_EBRT_WINDOW 0x104B

/*******************************************************************************
 * NAME          : UnifiPostEbrtWindow
 * PSID          : 4173 (0x104D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 5000
 * DESCRIPTION   :
 *  Minimum time after the expected normal mode beacon reception time that
 *  UniFi will continue to listen for the beacon in an infrastructure BSS
 *  before timing out. Reducing this value can reduce UniFi power consumption
 *  when using low power modes, however a value which is too small may cause
 *  beacons to be missed, requiring the radio to remain on for longer periods
 *  to ensure reception of the subsequent beacon.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POST_EBRT_WINDOW 0x104D

/*******************************************************************************
 * NAME          : UnifiLiveRestartRssiThreshold
 * PSID          : 4174 (0x104E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Minimum rssi threshold (dBm) to trigger live-restart If the RSSI is
 *  lower(poor) than this value, even if live-restart is requested by
 *  unifiDplaneDeafDetection, do not execute live-restart to avoid false
 *  alarm from environmental cause. This feature is disabled if set to 0 and
 *  only valid for STA VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LIVE_RESTART_RSSI_THRESHOLD 0x104E

/*******************************************************************************
 * NAME          : UnifiLiveRestartMinInterval
 * PSID          : 4175 (0x104F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Minimum duration(ms) to execute live-restart from the previous restart.
 *  If elapsed time from previous live-restart is smaller than this value,
 *  even if live-restart is requested by unifiDplaneDeafDetection, do not
 *  execute live-restart to avoid redundant chatty alarm. This feature is
 *  disabled if set to 0.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LIVE_RESTART_MIN_INTERVAL 0x104F

/*******************************************************************************
 * NAME          : UnifiPsPollThreshold
 * PSID          : 4179 (0x1053)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  PS Poll threshold. When Unifi chip is configured for normal power save
 *  mode and when access point does not respond to PS-Poll requests, then a
 *  fault will be generated on non-zero PS Poll threshold indicating mode has
 *  been switched from power save to fast power save. Ignored PS Poll count
 *  is given as the fault argument.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PS_POLL_THRESHOLD 0x1053

/*******************************************************************************
 * NAME          : UnifiPostEbrtWindowIdle
 * PSID          : 4180 (0x1054)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 2000
 * DESCRIPTION   :
 *  Minimum time after the expected idle mode beacon reception time that
 *  UniFi will continue to listen for the beacon in an infrastructure BSS
 *  before timing out. Reducing this value can reduce UniFi power consumption
 *  when using low power modes, however a value which is too small may cause
 *  beacons to be missed, requiring the radio to remain on for longer periods
 *  to ensure reception of the subsequent beacon.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POST_EBRT_WINDOW_IDLE 0x1054

/*******************************************************************************
 * NAME          : UnifiPreEbrtWindowIdle
 * PSID          : 4181 (0x1055)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       :
 * DESCRIPTION   :
 *  Latest time before the expected idle mode beacon reception time that
 *  UniFi will turn on its radio in order to receive the beacon. Reducing
 *  this value can reduce UniFi power consumption when using low power modes,
 *  however a value which is too small may cause beacons to be missed,
 *  requiring the radio to remain on for longer periods to ensure reception
 *  of the subsequent beacon.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRE_EBRT_WINDOW_IDLE 0x1055

/*******************************************************************************
 * NAME          : UnifiSableContainerSizeConfiguration
 * PSID          : 5000 (0x1388)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 3
 * DEFAULT       :  {0X64}
 * DESCRIPTION   :
 *  Sable Container Size Configuration Sable WLAN reserved memory size is
 *  determined by the host. Sable TLV containers are allocated from this WLAN
 *  reserved area. Each container has different requirement on its size. For
 *  example, frame logging or IQ capture would be very greedy, requesting
 *  most of available memroy. But some just need fixed size, but not large.
 *  To cope with such requirements, each container size is configured with
 *  the following rules: 1. To allocate a certain percentage of the whole
 *  wlan reserved area, put the percentage in hex format. For example,
 *  0x28(=40) means 40% of reserved area will be assigned. The number
 *  0x64(=100) is specially treated that all remaining space will be assigned
 *  after all the other containers are first served. 2. To request (n * 2048)
 *  bytes, put (100 + n) value in hex format. For example, 0x96 (= 150) means
 *  50 * 2048 = 102400 bytes. Here are the list of containers: - octet 0 -
 *  WTLV_CONTAINER_ID_DPLANE_FRAME_LOG
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SABLE_CONTAINER_SIZE_CONFIGURATION 0x1388

/*******************************************************************************
 * NAME          : UnifiSableFrameLogMode
 * PSID          : 5001 (0x1389)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Sable Frame Logging mode - 0: disable frame logging - 1: enable frame
 *  logging always, regardless of CPU resource state - 2: dynamically enable
 *  frame logging base on CPU resource. If CPU too busy, frame logging is
 *  disabled. Logging is enabled when CPU resource gets recovered.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SABLE_FRAME_LOG_MODE 0x1389

/*******************************************************************************
 * NAME          : UnifiSableFrameLogCpuThresPercent
 * PSID          : 5002 (0x138A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 95
 * DESCRIPTION   :
 *  CPU target in percent. When CPU usage is higher than this target, frame
 *  logging will be disabled by firmware. Firmware will check if CPU resource
 *  is recovered every 1 second. If CPU resource recovered, then frame
 *  logging is re-enabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SABLE_FRAME_LOG_CPU_THRES_PERCENT 0x138A

/*******************************************************************************
 * NAME          : UnifiSableFrameLogCpuOverheadPercent
 * PSID          : 5003 (0x138B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Expected CPU overhead introduced by frame logging.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SABLE_FRAME_LOG_CPU_OVERHEAD_PERCENT 0x138B

/*******************************************************************************
 * NAME          : UnifiDplaneSableTxDelayThresMs
 * PSID          : 5004 (0x138C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  TX Delay threshold in msec to take action.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_SABLE_TX_DELAY_THRES_MS 0x138C

/*******************************************************************************
 * NAME          : UnifiDplaneSableTxDelayAction
 * PSID          : 5005 (0x138D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Action to take when TX delay is larger than
 *  unifiDplaneSableTxDelayThresMs. 0: do nothing 1: print delay in mxlog 2:
 *  trigger a panic
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_SABLE_TX_DELAY_ACTION 0x138D

/*******************************************************************************
 * NAME          : UnifiDebugSvcModeStackHighWaterMark
 * PSID          : 5010 (0x1392)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read the SVC mode stack high water mark in bytes
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_SVC_MODE_STACK_HIGH_WATER_MARK 0x1392

/*******************************************************************************
 * NAME          : UnifiOverrideEdcaParamBe
 * PSID          : 5023 (0x139F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_EDCA_PARAM_BE 0x139F

/*******************************************************************************
 * NAME          : UnifiOverrideEdcaParamBeEnable
 * PSID          : 5024 (0x13A0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_EDCA_PARAM_BE_ENABLE 0x13A0

/*******************************************************************************
 * NAME          : UnifiPanicSubSystemControl
 * PSID          : 5026 (0x13A2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  PANIC levels for WLAN SubSystems. Panic level is used to filter Panic
 *  sent to the host. 4 levels of Panic per subsystem are available
 *  (FAILURE_LEVEL_T): a. 0 FATAL - Always reported to host b. 1 ERROR c. 2
 *  WARNING d. 3 DEBUG NOTE: If Panic level of a subsystem is configured to
 *  FATAL, all the Panics within that subsystem configured to FATAL will be
 *  effective, panics with ERROR, WARNING and Debug level will be converted
 *  to faults. If Panic level of a subsystem is configured to WARNING, all
 *  the panics within that subsystem configured to FATAL, ERROR and WARNING
 *  will be issued to host, panics with Debug level will be converted to
 *  faults.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PANIC_SUB_SYSTEM_CONTROL 0x13A2

/*******************************************************************************
 * NAME          : UnifiFaultEnable
 * PSID          : 5027 (0x13A3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Send Fault to host state.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAULT_ENABLE 0x13A3

/*******************************************************************************
 * NAME          : UnifiFaultSubSystemControl
 * PSID          : 5028 (0x13A4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Fault levels for WLAN SubSystems. Fault level is used to filter faults
 *  sent to the host. 4 levels of faults per subsystem are available
 *  (FAILURE_LEVEL_T): a. 0 ERROR b. 1 WARNING c. 2 INFO_1 d. 3 INFO_2
 *  Modifying Fault Levels at run time: 1. Set the fault level for the
 *  subsystems in unifiFaultConfigTable 2. Set unifiFaultEnable NOTE: If
 *  fault level of a subsystem is configured to ERROR, all the faults within
 *  that subsystem configured to ERROR will only be issued to host, faults
 *  with WARNING, INFO_1 and INFO_2 level will be converted to debug message
 *  If fault level of a subsystem is configured to WARNING, all the faults
 *  within that subsystem configured to ERROR and WARNING will be issued to
 *  host, faults with INFO_1 and INFO_2 level will be converted to debug
 *  message
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAULT_SUB_SYSTEM_CONTROL 0x13A4

/*******************************************************************************
 * NAME          : UnifiDebugModuleControl
 * PSID          : 5029 (0x13A5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Debug Module levels for all modules. Module debug level is used to filter
 *  debug messages sent to the host. Only 6 levels of debug per module are
 *  available: a. -1 No debug created. b. 0 Debug if compiled in. Should not
 *  cause Buffer Full in normal testing. c. 1 - 3 Levels to allow sensible
 *  setting of the .hcf file while running specific tests or debugging d. 4
 *  Debug will harm normal execution due to excessive levels or processing
 *  time required. Only used in emergency debugging. Additional control for
 *  FSM transition and FSM signals logging is provided. Debug module level
 *  and 2 boolean flags are encoded within a uint16: Function | Is sending
 *  FSM signals | Is sending FSM transitions | Is sending FSM Timers |
 *  Reserved | Module level (signed int)
 *  -_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_+-_-_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_- Bits | 15 | 14 | 13 | 12 - 8 | 7 - 0 Note: 0x00FF disables any debug for a module 0xE004 enables all debug for a module
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_MODULE_CONTROL 0x13A5

/*******************************************************************************
 * NAME          : UnifiTxUsingLdpcActivated
 * PSID          : 5030 (0x13A6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  LDPC will be used to code packets, for transmit only. If disabled, chip
 *  will not send LDPC coded packets even if peer supports it. To advertise
 *  reception of LDPC coded packets,enable bit 0 of unifiHtCapabilities, and
 *  bit 4 of unifiVhtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_USING_LDPC_ACTIVATED 0x13A6

/*******************************************************************************
 * NAME          : UnifiTxSettings
 * PSID          : 5031 (0x13A7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SETTINGS 0x13A7

/*******************************************************************************
 * NAME          : UnifiTxGainSettings
 * PSID          : 5032 (0x13A8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter gain settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_GAIN_SETTINGS 0x13A8

/*******************************************************************************
 * NAME          : UnifiTxAntennaConnectionLossFrequency
 * PSID          : 5033 (0x13A9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The corresponding set of frequency values for
 *  TxAntennaConnectionLossTable
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_CONNECTION_LOSS_FREQUENCY 0x13A9

/*******************************************************************************
 * NAME          : UnifiTxAntennaConnectionLoss
 * PSID          : 5034 (0x13AA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of Antenna Connection Loss value (qdB), which is used for
 *  TPO/EIRP conversion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_CONNECTION_LOSS 0x13AA

/*******************************************************************************
 * NAME          : UnifiTxAntennaMaxGainFrequency
 * PSID          : 5035 (0x13AB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The corresponding set of frequency values for TxAntennaMaxGain
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_MAX_GAIN_FREQUENCY 0x13AB

/*******************************************************************************
 * NAME          : UnifiTxAntennaMaxGain
 * PSID          : 5036 (0x13AC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of Antenna Max Gain value (qdB), which is used for TPO/EIRP
 *  conversion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_MAX_GAIN 0x13AC

/*******************************************************************************
 * NAME          : UnifiRxExternalGainFrequency
 * PSID          : 5037 (0x13AD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of RSSI offset value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_EXTERNAL_GAIN_FREQUENCY 0x13AD

/*******************************************************************************
 * NAME          : UnifiRxExternalGain
 * PSID          : 5038 (0x13AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The table giving frequency-dependent RSSI offset value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_EXTERNAL_GAIN 0x13AE

/*******************************************************************************
 * NAME          : UnifiTxSgI20Activated
 * PSID          : 5040 (0x13B0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  SGI 20MHz will be used to code packets for transmit only. If disabled,
 *  chip will not send SGI 20MHz packets even if peer supports it. To
 *  advertise reception of SGI 20MHz packets, enable bit 5 of
 *  unifiHtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SG_I20_ACTIVATED 0x13B0

/*******************************************************************************
 * NAME          : UnifiTxSgI40Activated
 * PSID          : 5041 (0x13B1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  SGI 40MHz will be used to code packets, for transmit only. If disabled,
 *  chip will not send SGI 40MHz packets even if peer supports it. To
 *  advertise reception of SGI 40MHz packets, enable bit 6 of
 *  unifiHtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SG_I40_ACTIVATED 0x13B1

/*******************************************************************************
 * NAME          : UnifiTxSgI80Activated
 * PSID          : 5042 (0x13B2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  SGI 80MHz will be used to code packets, for transmit only. If disabled,
 *  chip will not send SGI 80MHz packets even if peer supports it. To
 *  advertise reception of SGI 80MHz packets, enable bit 5 of
 *  unifiVhtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SG_I80_ACTIVATED 0x13B2

/*******************************************************************************
 * NAME          : UnifiTxSgI160Activated
 * PSID          : 5043 (0x13B3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  SGI 160/80+80MHz will be used to code packets, for transmit only. If
 *  disabled, chip will not send SGI 160/80+80MHz packets even if peer
 *  supports it. To advertise reception of SGI 160/80+80MHz packets, enable
 *  bit 6 of unifiVhtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SG_I160_ACTIVATED 0x13B3

/*******************************************************************************
 * NAME          : UnifiMacAddressRandomisation
 * PSID          : 5044 (0x13B4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name: Enabling Mac
 *  Address Randomisation to be applied for Probe Requests when scanning.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_ADDRESS_RANDOMISATION 0x13B4

/*******************************************************************************
 * NAME          : UnifiMacAddressRandomisationMask
 * PSID          : 5047 (0x13B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 6
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  FW randomises MAC Address bits that have a corresponding bit set to 0 in
 *  the MAC Mask for Probe Requests. This excludes U/L and I/G bits which
 *  will be set to Local and Individual respectively.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_ADDRESS_RANDOMISATION_MASK 0x13B7

/*******************************************************************************
 * NAME          : UnifiWipsActivated
 * PSID          : 5050 (0x13BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Wips.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WIPS_ACTIVATED 0x13BA

/*******************************************************************************
 * NAME          : UnifiRfTestModeActivated
 * PSID          : 5054 (0x13BE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Test only: Set to true when running in RF Test mode. Setting this MIB key
 *  to true prevents setting mandatory HT MCS Rates.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RF_TEST_MODE_ACTIVATED 0x13BE

/*******************************************************************************
 * NAME          : UnifiTxPowerDetectorResponse
 * PSID          : 5055 (0x13BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter detector response settings. 2G settings
 *  before 5G. Increasing order within band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_DETECTOR_RESPONSE 0x13BF

/*******************************************************************************
 * NAME          : UnifiTxDetectorTemperatureCompensation
 * PSID          : 5056 (0x13C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DETECTOR_TEMPERATURE_COMPENSATION 0x13C0

/*******************************************************************************
 * NAME          : UnifiTxDetectorFrequencyCompensation
 * PSID          : 5057 (0x13C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DETECTOR_FREQUENCY_COMPENSATION 0x13C1

/*******************************************************************************
 * NAME          : UnifiTxOpenLoopTemperatureCompensation
 * PSID          : 5058 (0x13C2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter open-loop temperature compensation
 *  settings. 2G settings before 5G. Increasing order within band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OPEN_LOOP_TEMPERATURE_COMPENSATION 0x13C2

/*******************************************************************************
 * NAME          : UnifiTxOpenLoopFrequencyCompensation
 * PSID          : 5059 (0x13C3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter open-loop frequency compensation settings.
 *  2G settings before 5G. Increasing order within band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OPEN_LOOP_FREQUENCY_COMPENSATION 0x13C3

/*******************************************************************************
 * NAME          : UnifiTxOfdmSelect
 * PSID          : 5060 (0x13C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 8
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter OFDM selection settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OFDM_SELECT 0x13C4

/*******************************************************************************
 * NAME          : UnifiTxDigGain
 * PSID          : 5061 (0x13C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 16
 * MAX           : 48
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specify gain specific modulation power optimisation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DIG_GAIN 0x13C5

/*******************************************************************************
 * NAME          : UnifiChipTemperature
 * PSID          : 5062 (0x13C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : celcius
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read the chip temperature as seen by WLAN radio firmware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHIP_TEMPERATURE 0x13C6

/*******************************************************************************
 * NAME          : UnifiBatteryVoltage
 * PSID          : 5063 (0x13C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : millivolt
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Battery voltage
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BATTERY_VOLTAGE 0x13C7

/*******************************************************************************
 * NAME          : UnifiTxOobConstraints
 * PSID          : 5064 (0x13C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  OOB constraints table. | octects | description |
 * |-_-_-_-_-+-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-| | 0 | DPD applicability bitmask: 0 = no DPD, 1 = dynamic DPD, 2 = static DPD, 3 = applies to both static and dynamic DPD | | 1-2 | Bitmask indicating which regulatory domains this rule applies to FCC=bit0, ETSI=bit1, JAPAN=bit2 | | 3-4 | Bitmask indicating which band edges this rule applies to RICE_BAND_EDGE_ISM_24G_LOWER = bit 0, RICE_BAND_EDGE_ISM_24G_UPPER = bit 1, RICE_BAND_EDGE_U_NII_1_LOWER = bit 2, RICE_BAND_EDGE_U_NII_1_UPPER = bit 3, RICE_BAND_EDGE_U_NII_2_LOWER = bit 4, RICE_BAND_EDGE_U_NII_2_UPPER = bit 5, RICE_BAND_EDGE_U_NII_2E_LOWER = bit 6, RICE_BAND_EDGE_U_NII_2E_UPPER = bit 7, RICE_BAND_EDGE_U_NII_3_LOWER = bit 8, RICE_BAND_EDGE_U_NII_3_UPPER = bit 9 | | 5 | Bitmask indicating which modulation types this rule applies to (LSB/b0=DSSS/CCK, b1= OFDM0 modulation group, b2= OFDM1 modulation group) | | 6 | Bitmask indicating which channel bandwidths this rule applies to (LSB/b0=20MHz, b1=40MHz, b2=80MHz) | | 7 | Minimum distance to nearest band edge in 500 kHz units for which this constraint becomes is applicable. | | 8 | Maximum power (EIRP) for this particular constraint - specified in units of quarter dBm. | | 9-32 | Spectral shaping configuration to be used for this particular constraint. The value is specific to the radio hardware and should only be altered under advice from the IC supplier. | | 33-56| Tx DPD Spectral shaping configuration to be used for this particular constraint. The value is specific to the radio hardware and should only be altered under advice from the IC supplier. | |
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OOB_CONSTRAINTS 0x13C8

/*******************************************************************************
 * NAME          : UnifiTxPaGainDpdTemperatureCompensation
 * PSID          : 5066 (0x13CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_PA_GAIN_DPD_TEMPERATURE_COMPENSATION 0x13CA

/*******************************************************************************
 * NAME          : UnifiTxPaGainDpdFrequencyCompensation
 * PSID          : 5067 (0x13CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_PA_GAIN_DPD_FREQUENCY_COMPENSATION 0x13CB

/*******************************************************************************
 * NAME          : UnifiTxPowerTrimConfig
 * PSID          : 5072 (0x13D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 26
 * MAX           : 26
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter power trim settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_TRIM_CONFIG 0x13D0

/*******************************************************************************
 * NAME          : UnifiForceShortSlotTime
 * PSID          : 5080 (0x13D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  If set to true, forces FW to use short slot times for all VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_SHORT_SLOT_TIME 0x13D8

/*******************************************************************************
 * NAME          : UnifiTxGainStepSettings
 * PSID          : 5081 (0x13D9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter gain step settings. 2G settings before 5G.
 *  Increasing order within band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_GAIN_STEP_SETTINGS 0x13D9

/*******************************************************************************
 * NAME          : UnifiDebugDisableRadioNannyActions
 * PSID          : 5082 (0x13DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Bitmap to disable the radio nanny actions. B0==radio 0, B1==radio 1
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_DISABLE_RADIO_NANNY_ACTIONS 0x13DA

/*******************************************************************************
 * NAME          : UnifiRxCckModemSensitivity
 * PSID          : 5083 (0x13DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 6
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specify values of CCK modem sensitivity for scan, normal and low
 *  sensitivity.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_CCK_MODEM_SENSITIVITY 0x13DB

/*******************************************************************************
 * NAME          : UnifiDpdPerBandwidth
 * PSID          : 5084 (0x13DC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 63
 * DESCRIPTION   :
 *  Bitmask to enable Digital Pre-Distortion per bandwidth
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_PER_BANDWIDTH 0x13DC

/*******************************************************************************
 * NAME          : UnifiBbVersion
 * PSID          : 5085 (0x13DD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Baseband chip version number determined by reading BBIC version
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BB_VERSION 0x13DD

/*******************************************************************************
 * NAME          : UnifiRfVersion
 * PSID          : 5086 (0x13DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  RF chip version number determined by reading RFIC version
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RF_VERSION 0x13DE

/*******************************************************************************
 * NAME          : UnifiReadHardwareCounter
 * PSID          : 5087 (0x13DF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read a value from a hardware packet counter for a specific radio_id and
 *  return it. The firmware will convert the radio_id to the associated
 *  mac_instance.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_READ_HARDWARE_COUNTER 0x13DF

/*******************************************************************************
 * NAME          : UnifiClearRadioTrimCache
 * PSID          : 5088 (0x13E0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Clears the radio trim cache. The parameter is ignored.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CLEAR_RADIO_TRIM_CACHE 0x13E0

/*******************************************************************************
 * NAME          : UnifiRadioTxSettingsRead
 * PSID          : 5089 (0x13E1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read value from Tx settings, rfchip up to and including S620.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_SETTINGS_READ 0x13E1

/*******************************************************************************
 * NAME          : UnifiModemSgiOffset
 * PSID          : 5090 (0x13E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Overwrite SGI sampling offset. Indexed by Band and Bandwidth. Defaults
 *  currently defined in fw.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MODEM_SGI_OFFSET 0x13E2

/*******************************************************************************
 * NAME          : UnifiRadioTxPowerOverride
 * PSID          : 5091 (0x13E3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Option in radio code to override the power requested by the upper layer
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_POWER_OVERRIDE 0x13E3

/*******************************************************************************
 * NAME          : UnifiRxRadioCsMode
 * PSID          : 5092 (0x13E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  OBSOLETE. Configures RX Radio CS detection for 80MHz bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_RADIO_CS_MODE 0x13E4

/*******************************************************************************
 * NAME          : UnifiRxPriEnergyDetThreshold
 * PSID          : 5093 (0x13E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  OBSOLETE. Energy detection threshold for primary 20MHz channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_PRI_ENERGY_DET_THRESHOLD 0x13E5

/*******************************************************************************
 * NAME          : UnifiRxSecEnergyDetThreshold
 * PSID          : 5094 (0x13E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  OBSOLETE. Energy detection threshold for secondary 20MHz channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_SEC_ENERGY_DET_THRESHOLD 0x13E6

/*******************************************************************************
 * NAME          : UnifiAgcThresholds
 * PSID          : 5095 (0x13E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  AGC Thresholds settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AGC_THRESHOLDS 0x13E7

/*******************************************************************************
 * NAME          : UnifiRadioRxSettingsRead
 * PSID          : 5096 (0x13E8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read value from Rx settings.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_RX_SETTINGS_READ 0x13E8

/*******************************************************************************
 * NAME          : UnifiStaticDpdGain
 * PSID          : 5097 (0x13E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 11
 * MAX           : 27
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specify modulation specifc gains for static dpd optimisation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STATIC_DPD_GAIN 0x13E9

/*******************************************************************************
 * NAME          : UnifiIqBufferSize
 * PSID          : 5098 (0x13EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Buffer Size for IQ capture to allow CATs to read it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IQ_BUFFER_SIZE 0x13EA

/*******************************************************************************
 * NAME          : UnifiNarrowbandCcaThresholds
 * PSID          : 5099 (0x13EB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  The narrowband CCA ED thresholds so that the CCA-ED triggers at the
 *  regulatory value of -62 dBm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NARROWBAND_CCA_THRESHOLDS 0x13EB

/*******************************************************************************
 * NAME          : UnifiRadioCcaDebug
 * PSID          : 5100 (0x13EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read values from Radio CCA settings.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_CCA_DEBUG 0x13EC

/*******************************************************************************
 * NAME          : UnifiCcacsThresh
 * PSID          : 5101 (0x13ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Configures CCA CS thresholds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCACS_THRESH 0x13ED

/*******************************************************************************
 * NAME          : UnifiCcaMasterSwitch
 * PSID          : 5102 (0x13EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00540050
 * DESCRIPTION   :
 *  Enables CCA
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCA_MASTER_SWITCH 0x13EE

/*******************************************************************************
 * NAME          : UnifiRxSyncCcaCfg
 * PSID          : 5103 (0x13EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Configures CCA per 20 MHz sub-band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_SYNC_CCA_CFG 0x13EF

/*******************************************************************************
 * NAME          : UnifiMacCcaBusyTime
 * PSID          : 5104 (0x13F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the time CCA indicates busy
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_CCA_BUSY_TIME 0x13F0

/*******************************************************************************
 * NAME          : UnifiMacSecChanClearTime
 * PSID          : 5105 (0x13F1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Configures PIFS
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_SEC_CHAN_CLEAR_TIME 0x13F1

/*******************************************************************************
 * NAME          : UnifiDpdDebug
 * PSID          : 5106 (0x13F2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Debug MIBs for DPD
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_DEBUG 0x13F2

/*******************************************************************************
 * NAME          : UnifiNarrowbandCcaDebug
 * PSID          : 5107 (0x13F3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read values from Radio CCA settings.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NARROWBAND_CCA_DEBUG 0x13F3

/*******************************************************************************
 * NAME          : UnifiNannyTemperatureReportDelta
 * PSID          : 5109 (0x13F5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  A temperature difference, in degrees Celsius, above which the nanny
 *  process will generate a temperature update debug word
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NANNY_TEMPERATURE_REPORT_DELTA 0x13F5

/*******************************************************************************
 * NAME          : UnifiNannyTemperatureReportInterval
 * PSID          : 5110 (0x13F6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  A report interval in milliseconds where temperature is checked
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NANNY_TEMPERATURE_REPORT_INTERVAL 0x13F6

/*******************************************************************************
 * NAME          : UnifiRadioRxDcocDebugIqValue
 * PSID          : 5111 (0x13F7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  RX DCOC debug testing. Allows user to override LUT index IQ values in
 *  combination with unifiRadioRxDcocDebug. This MIB sets IQ value that all
 *  LUT index Is and Qs get set to.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_RX_DCOC_DEBUG_IQ_VALUE 0x13F7

/*******************************************************************************
 * NAME          : UnifiRadioRxDcocDebug
 * PSID          : 5112 (0x13F8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  RX DCOC debug testing. Allows user to override LUT index IQ values in
 *  combination with unifiRadioRxDcocDebugIqValue. This MIB enables the
 *  feature.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_RX_DCOC_DEBUG 0x13F8

/*******************************************************************************
 * NAME          : UnifiNannyRetrimDpdMod
 * PSID          : 5113 (0x13F9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Bitmap to selectively enable nanny retrim of DPD per modulation.
 *  B0==OFDM0, B1==OFDM1, B2==CCK
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NANNY_RETRIM_DPD_MOD 0x13F9

/*******************************************************************************
 * NAME          : UnifiDisableDpdSubIteration
 * PSID          : 5114 (0x13FA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  For Engineering debug use only.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISABLE_DPD_SUB_ITERATION 0x13FA

/*******************************************************************************
 * NAME          : UnifiRxRssiAdjustments
 * PSID          : 5115 (0x13FB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       :
 * DESCRIPTION   :
 *  Provides platform dependent rssi adjustments. Octet string (length 4),
 *  each octet represents a signed 8 bit value in units of quarter dB.
 *  octet[0] = always_adjust (applied unconditionally in all cases) octet[1]
 *  = low_power_adjust (applied in low_power mode only) octet[2] =
 *  ext_lna_on_adjust (applied only if we have a FEM and the external LNA is
 *  enabled) octet[3] = ext_lna_off_adjust (applied only if we have a FEM and
 *  the external LNA is disabled)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_RSSI_ADJUSTMENTS 0x13FB

/*******************************************************************************
 * NAME          : UnifiFleximacCcaEdEnable
 * PSID          : 5116 (0x13FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enable/disable CCA-ED in Fleximac.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FLEXIMAC_CCA_ED_ENABLE 0x13FC

/*******************************************************************************
 * NAME          : UnifiRadioTxIqDelay
 * PSID          : 5117 (0x13FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  The differential delay applied between I and Q paths in Tx.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_IQ_DELAY 0x13FD

/*******************************************************************************
 * NAME          : UnifiDisableLnaBypass
 * PSID          : 5118 (0x13FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Prevents the use of the LNA bypass. Can be set at any time, but takes
 *  effect the next time the radio is turned on from off. Set a bit to 1 to
 *  disable the LNA bypass in that configuration. B0 2.4G Radio 0 B1 2.4G
 *  Radio 1 B2 2.4G Radio 2 B3 2.4G Radio 3 B4 5G Radio 0 B5 5G Radio 1 B6 5G
 *  Radio 2 B7 5G Radio 3
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISABLE_LNA_BYPASS 0x13FE

/*******************************************************************************
 * NAME          : UnifiInitialFrequencyOffset
 * PSID          : 5119 (0x13FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Allows carrier frequency and sample clock offset initial value in trim to
 *  be non-zero. Use to test Tx/Rx with a freq offset with significant freq
 *  offset or test convergence of freq tracking loop. Relevant to chips with
 *  11ax capability only. Provided for test and characterisation only. Should
 *  normally be zero.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_INITIAL_FREQUENCY_OFFSET 0x13FF

/*******************************************************************************
 * NAME          : UnifiRxRssiNbHwGain
 * PSID          : 5120 (0x1400)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 26
 * MAX           : 26
 * DEFAULT       :
 * DESCRIPTION   :
 *  Narrowband RSSI gain values for some combination of radio, band and
 *  bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_RSSI_NB_HW_GAIN 0x1400

/*******************************************************************************
 * NAME          : UnifiRxRssiNbFmOffset
 * PSID          : 5121 (0x1401)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 26
 * MAX           : 26
 * DEFAULT       :
 * DESCRIPTION   :
 *  Narrowband RSSI gain offset values for some combination of radio, band
 *  and bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_RSSI_NB_FM_OFFSET 0x1401

/*******************************************************************************
 * NAME          : UnifiRadioTxSettingsReadV2
 * PSID          : 5122 (0x1402)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read value from Tx settings, rfchip S621 onwards.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_SETTINGS_READ_V2 0x1402

/*******************************************************************************
 * NAME          : UnifiRxCckModemDesense
 * PSID          : 5123 (0x1403)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 3
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specify values of CCK modem desense.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_CCK_MODEM_DESENSE 0x1403

/*******************************************************************************
 * NAME          : UnifiRxCckModemDesenseOffset
 * PSID          : 5124 (0x1404)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Magic offset for unifiRxCckModemDesense feature.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_CCK_MODEM_DESENSE_OFFSET 0x1404

/*******************************************************************************
 * NAME          : UnifiRxOfdmModemDesense
 * PSID          : 5125 (0x1405)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specify values of OFDM modem desense.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_OFDM_MODEM_DESENSE 0x1405

/*******************************************************************************
 * NAME          : UnifiRxOfdmModemDesenseOffset
 * PSID          : 5126 (0x1406)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       :
 * DESCRIPTION   :
 *  Set ofdm magic offsets for desense feature.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_OFDM_MODEM_DESENSE_OFFSET 0x1406

/*******************************************************************************
 * NAME          : UnifiRadioFecConfig
 * PSID          : 5127 (0x1407)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 7
 * MAX           : 7
 * DEFAULT       :
 * DESCRIPTION   :
 *  Configuration for Front End Control
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_FEC_CONFIG 0x1407

/*******************************************************************************
 * NAME          : UnifiTxPowerDetectorResponseFrequency
 * PSID          : 5128 (0x1408)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter detector response settings per frequency.
 *  2G settings before 5G. Increasing order within band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_DETECTOR_RESPONSE_FREQUENCY 0x1408

/*******************************************************************************
 * NAME          : UnifiRadioMiscPerBand
 * PSID          : 5129 (0x1409)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 16
 * MAX           : 16
 * DEFAULT       :
 * DESCRIPTION   :
 *  Miscellaneous per band radio configuration values octet[0] = band
 *  octet[1] = mix_lo_bias octet[2] = vco_ictrl octet[3] = vco_ictrl_lp
 *  octet[4] = vco_ictrl_dpd octet[5] = vco_ictrl_dpd_lp octets[15:6] =
 *  reserved
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_MISC_PER_BAND 0x1409

/*******************************************************************************
 * NAME          : UnifiNannyTrimNow
 * PSID          : 5130 (0x140A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Trigger Nanny retrim with next timer tick
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NANNY_TRIM_NOW 0x140A

/*******************************************************************************
 * NAME          : UnifiMapRadioIdToMacId
 * PSID          : 5131 (0x140B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Convert radio_id to mac_id
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAP_RADIO_ID_TO_MAC_ID 0x140B

/*******************************************************************************
 * NAME          : UnifiReportNarrowbandRssi
 * PSID          : 5132 (0x140C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable reporting of Fleximac narrowband RSSI to higher layers.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_REPORT_NARROWBAND_RSSI 0x140C

/*******************************************************************************
 * NAME          : UnifiEnableFemRfLoopback
 * PSID          : 5133 (0x140D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enable RF loopback from a suitably equipped external PA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_FEM_RF_LOOPBACK 0x140D

/*******************************************************************************
 * NAME          : UnifiRadioTrimCount
 * PSID          : 5134 (0x140E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Radio trims since service start
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TRIM_COUNT 0x140E

/*******************************************************************************
 * NAME          : UnifiRxRssiBandCompensation
 * PSID          : 5135 (0x140F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 24
 * MAX           : 81
 * DEFAULT       :
 * DESCRIPTION   :
 *  RSSI band flatness compensation for combinations of radio, band and
 *  bandwidth.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_RSSI_BAND_COMPENSATION 0x140F

/*******************************************************************************
 * NAME          : UnifiEnableFrequencyTracking
 * PSID          : 5136 (0x1410)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Allow RadioFW to tune the PLL frequency to that of the BSS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_FREQUENCY_TRACKING 0x1410

/*******************************************************************************
 * NAME          : UnifiEnableFlexiMacWatchdog
 * PSID          : 5200 (0x1450)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0000
 * DESCRIPTION   :
 *  OBSOLETE. Bitmap controlling watchdog configuration for fleximac. Setting
 *  bit to 1 will enable watchdog for MAC represented by bit position
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_FLEXI_MAC_WATCHDOG 0x1450

/*******************************************************************************
 * NAME          : UnifiRttCapabilities
 * PSID          : 5300 (0x14B4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0X01, 0X01, 0X01, 0X01, 0X00, 0X07, 0X1C, 0X32 }
 * DESCRIPTION   :
 *  RTT capabilities of the chip. see SC-506960-SW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RTT_CAPABILITIES 0x14B4

/*******************************************************************************
 * NAME          : UnifiFtmMinDeltaFrames
 * PSID          : 5301 (0x14B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  Default minimum time between consecutive FTM frames in units of 100 us.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_MIN_DELTA_FRAMES 0x14B5

/*******************************************************************************
 * NAME          : UnifiFtmPerBurst
 * PSID          : 5302 (0x14B6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 31
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Requested FTM frames per burst.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_PER_BURST 0x14B6

/*******************************************************************************
 * NAME          : UnifiFtmBurstDuration
 * PSID          : 5303 (0x14B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 2
 * MAX           : 11
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  indicates the duration of a burst instance, values 0, 1, 12-14 are
 *  reserved, [2..11], the burst duration is defined as (250 x 2)^(N-2), and
 *  15 means "no preference".
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_BURST_DURATION 0x14B7

/*******************************************************************************
 * NAME          : UnifiFtmNumOfBurstsExponent
 * PSID          : 5304 (0x14B8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 14
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of burst instances is 2^(Number of Bursts Exponent), value 15
 *  means "no preference".
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_NUM_OF_BURSTS_EXPONENT 0x14B8

/*******************************************************************************
 * NAME          : UnifiFtmAsapModeActivated
 * PSID          : 5305 (0x14B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate support for ASAP mode in FTM
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_ASAP_MODE_ACTIVATED 0x14B9

/*******************************************************************************
 * NAME          : UnifiFtmResponderActivated
 * PSID          : 5306 (0x14BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Activate support for FTM Responder
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_RESPONDER_ACTIVATED 0x14BA

/*******************************************************************************
 * NAME          : UnifiFtmDefaultSessionEstablishmentTimeout
 * PSID          : 5307 (0x14BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 10
 * MAX           : 100
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Default timeout for session estabishmant in units of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_DEFAULT_SESSION_ESTABLISHMENT_TIMEOUT 0x14BB

/*******************************************************************************
 * NAME          : UnifiFtmDefaultGapBeforeFirstBurstPerResponder
 * PSID          : 5308 (0x14BC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_DEFAULT_GAP_BEFORE_FIRST_BURST_PER_RESPONDER 0x14BC

/*******************************************************************************
 * NAME          : UnifiFtmDefaultGapBetweenBursts
 * PSID          : 5309 (0x14BD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 5
 * MAX           : 50
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Interval between consecutive Bursts. In units of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_DEFAULT_GAP_BETWEEN_BURSTS 0x14BD

/*******************************************************************************
 * NAME          : UnifiFtmDefaultTriggerDelay
 * PSID          : 5310 (0x14BE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Delay to account for differences in time between Initiator and Responder
 *  at start of the Burst. In units of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_DEFAULT_TRIGGER_DELAY 0x14BE

/*******************************************************************************
 * NAME          : UnifiFtmDefaultEndBurstDelay
 * PSID          : 5311 (0x14BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Delay to account for differences in time between Initiator and Responder
 *  at the end of the Burst. In units of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_DEFAULT_END_BURST_DELAY 0x14BF

/*******************************************************************************
 * NAME          : UnifiFtmRequestValidationEnabled
 * PSID          : 5312 (0x14C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable Validation for FTM Add Range request RTT_Configs
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_REQUEST_VALIDATION_ENABLED 0x14C0

/*******************************************************************************
 * NAME          : UnifiFtmResponseValidationEnabled
 * PSID          : 5313 (0x14C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable Validation for FTM Response
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_RESPONSE_VALIDATION_ENABLED 0x14C1

/*******************************************************************************
 * NAME          : UnifiFtmUseResponseParameters
 * PSID          : 5314 (0x14C2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Use Response burst parameters for burst
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_USE_RESPONSE_PARAMETERS 0x14C2

/*******************************************************************************
 * NAME          : UnifiFtmInitialResponseTimeout
 * PSID          : 5315 (0x14C3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 10
 * MAX           : 100
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Default timeout for FtmInitialResponse in units of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_INITIAL_RESPONSE_TIMEOUT 0x14C3

/*******************************************************************************
 * NAME          : UnifiFtmMacAddressRandomisation
 * PSID          : 5316 (0x14C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable mac address randomisation for FTM initiator
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_MAC_ADDRESS_RANDOMISATION 0x14C4

/*******************************************************************************
 * NAME          : UnifiFtmUseRequestParameters
 * PSID          : 5321 (0x14C9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Use certain burst params from user while sending FTM req
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_USE_REQUEST_PARAMETERS 0x14C9

/*******************************************************************************
 * NAME          : UnifiFtmMeanAroundCluster
 * PSID          : 5322 (0x14CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Whether to get simple mean or mean around cluster
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_MEAN_AROUND_CLUSTER 0x14CA

/*******************************************************************************
 * NAME          : UnifiFtmLegacyCalibration
 * PSID          : 5333 (0x14D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Table for legacy mode FTM calibration values.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_LEGACY_CALIBRATION 0x14D5

/*******************************************************************************
 * NAME          : UnifiFtmHtVhtCalibration
 * PSID          : 5334 (0x14D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Table for HT/VHT mode FTM calibration values.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FTM_HT_VHT_CALIBRATION 0x14D6

/*******************************************************************************
 * NAME          : UnifiScanMaxCacheCount
 * PSID          : 5395 (0x1513)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 200
 * DEFAULT       : 150
 * DESCRIPTION   :
 *  Maximum count the Scan will maintain the scan result in cache. Zero to
 *  disable the caching.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_MAX_CACHE_COUNT 0x1513

/*******************************************************************************
 * NAME          : UnifiScanMaxCacheTime
 * PSID          : 5396 (0x1514)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 1
 * MAX           : 120
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  Maximum period, in seconds, the Scan will keep the scan result in cache.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_MAX_CACHE_TIME 0x1514

/*******************************************************************************
 * NAME          : UnifiRmMaxMeasurementDuration
 * PSID          : 5400 (0x1518)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 100
 * MAX           : 200
 * DEFAULT       : 150
 * DESCRIPTION   :
 *  Maximum measurement duration in milliseconds for operating channel
 *  measurements, i.e. maximum channel time duration for measurement scan
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RM_MAX_MEASUREMENT_DURATION 0x1518

/*******************************************************************************
 * NAME          : UnifiRmMaxScanCacheTime
 * PSID          : 5401 (0x1519)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 120
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  Maximum period, in seconds, the Mlme Measurement will keep the scan
 *  result for beacon table mode. When set to zero, result never expires
 *  acting as a Golden Certification MIB for WFA VE Certification.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RM_MAX_SCAN_CACHE_TIME 0x1519

/*******************************************************************************
 * NAME          : UnifiRmMaxScanCacheCount
 * PSID          : 5402 (0x151A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 10
 * MAX           : 150
 * DEFAULT       : 70
 * DESCRIPTION   :
 *  Maximum count the Mlme Measurement will maintain the scan result for
 *  beacon table mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RM_MAX_SCAN_CACHE_COUNT 0x151A

/*******************************************************************************
 * NAME          : UnifiObssScanStartWait
 * PSID          : 5405 (0x151D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 200
 * DEFAULT       : 150
 * DESCRIPTION   :
 *  Time in seconds to wait before starting a OBSS scan after channel switch.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OBSS_SCAN_START_WAIT 0x151D

/*******************************************************************************
 * NAME          : UnifiMlmeScanDefaultShortFullScanTime
 * PSID          : 5406 (0x151E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 20
 * MAX           : 30
 * DEFAULT       : 25
 * DESCRIPTION   :
 *  Scan channel duration when in short full scan mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_DEFAULT_SHORT_FULL_SCAN_TIME 0x151E

/*******************************************************************************
 * NAME          : UnifiMlmeScanShortFullScanNumOfProbes
 * PSID          : 5407 (0x151F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 3
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Number of probes for short full scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_SHORT_FULL_SCAN_NUM_OF_PROBES 0x151F

/*******************************************************************************
 * NAME          : UnifiMlmeScanContinueIfMoreThanXAps
 * PSID          : 5410 (0x1522)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Part of Scan Algorithm: Keep scanning on a channel with lots of APs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CONTINUE_IF_MORE_THAN_XAPS 0x1522

/*******************************************************************************
 * NAME          : UnifiMlmeScanStopIfLessThanXNewAps
 * PSID          : 5411 (0x1523)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Part of Scan Algorithm: Stop scanning on a channel if less than X NEW APs
 *  are seen.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_STOP_IF_LESS_THAN_XNEW_APS 0x1523

/*******************************************************************************
 * NAME          : UnifiScanMultiVifActivated
 * PSID          : 5412 (0x1524)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Part of Scan Algorithm: Activate support for Multi Vif channel times.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_MULTI_VIF_ACTIVATED 0x1524

/*******************************************************************************
 * NAME          : UnifiScanNewAlgorithmActivated
 * PSID          : 5413 (0x1525)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Part of Scan Algorithm: Activate support for the new algorithm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_NEW_ALGORITHM_ACTIVATED 0x1525

/*******************************************************************************
 * NAME          : UnifiScanRegisteredVifInfrastructureSta
 * PSID          : 5414 (0x1526)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Part of Scan Algorithm: Consider only Infrastructure STA vif as
 *  registered vif to choose VIF >= 1 scan parameters.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_REGISTERED_VIF_INFRASTRUCTURE_STA 0x1526

/*******************************************************************************
 * NAME          : UnifiLnaControlEvaluationInterval
 * PSID          : 6008 (0x1778)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  Interval in milliseconds, for evaluating LNA control.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LNA_CONTROL_EVALUATION_INTERVAL 0x1778

/*******************************************************************************
 * NAME          : UnifiLnaControlRssiThresholds
 * PSID          : 6009 (0x1779)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * UNITS         : dBm
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       :
 * DESCRIPTION   :
 *  RSSI threshold table for dynamic switching of the LNA.Default values set
 *  to{-30,-40}
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LNA_CONTROL_RSSI_THRESHOLDS 0x1779

/*******************************************************************************
 * NAME          : UnifiUnsyncVifLnaEnabled
 * PSID          : 6010 (0x177A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enable or disable use of the LNA for unsynchronised VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UNSYNC_VIF_LNA_ENABLED 0x177A

/*******************************************************************************
 * NAME          : UnifiTpcMaxPower2Gmimo
 * PSID          : 6011 (0x177B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 52
 * DESCRIPTION   :
 *  Maximum power for 2.4GHz MIMO interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold (quarter dbm). Should be greater than
 *  dot11PowerCapabilityMinImplemented. This value is read only once when an
 *  interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER2_GMIMO 0x177B

/*******************************************************************************
 * NAME          : UnifiTpcMaxPower5Gmimo
 * PSID          : 6012 (0x177C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 52
 * DESCRIPTION   :
 *  Maximum power for 5 GHz MIMO interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold (quarter dbm). Should be greater than
 *  dot11PowerCapabilityMinImplemented. This value is read only once when an
 *  interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER5_GMIMO 0x177C

/*******************************************************************************
 * NAME          : UnifiLnaControlEnabled
 * PSID          : 6013 (0x177D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enable dynamic switching of the LNA based on RSSI for synchronised VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LNA_CONTROL_ENABLED 0x177D

/*******************************************************************************
 * NAME          : UnifiPowerIsGrip
 * PSID          : 6016 (0x1780)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Is using Grip power cap instead of SAR cap.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POWER_IS_GRIP 0x1780

/*******************************************************************************
 * NAME          : UnifiLowPowerRxConfig
 * PSID          : 6018 (0x1782)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Enables low power radio RX for idle STA and AP VIFs respectively.
 *  Setting/clearing bit 0 enables/disabled LP RX for (all) STA/Cli VIFs.
 *  Setting/clearing bit 1 enables/disabled LP RX for AP/GO VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOW_POWER_RX_CONFIG 0x1782

/*******************************************************************************
 * NAME          : UnifiTpcEnabled
 * PSID          : 6019 (0x1783)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated. Golden Certification MIB don't delete, change PSID or name
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_ENABLED 0x1783

/*******************************************************************************
 * NAME          : UnifiCurrentTxpowerLevel
 * PSID          : 6020 (0x1784)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : qdBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Maximum air power for the VIF. Values are expressed in 0.25 dBm units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_TXPOWER_LEVEL 0x1784

/*******************************************************************************
 * NAME          : UnifiUserSetTxpowerLevel
 * PSID          : 6021 (0x1785)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 127
 * DESCRIPTION   :
 *  Test only: Maximum User Set Tx Power (quarter dBm). Enable it in
 *  unifiTestTxPowerEnable.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USER_SET_TXPOWER_LEVEL 0x1785

/*******************************************************************************
 * NAME          : UnifiTpcMaxPowerRssiThreshold
 * PSID          : 6022 (0x1786)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -55
 * DESCRIPTION   :
 *  Below the (dBm) threshold, switch to the max power allowed by regulatory,
 *  if it has been previously reduced due to unifiTPCMinPowerRSSIThreshold.
 *  This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER_RSSI_THRESHOLD 0x1786

/*******************************************************************************
 * NAME          : UnifiTpcMinPowerRssiThreshold
 * PSID          : 6023 (0x1787)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -45
 * DESCRIPTION   :
 *  Above the (dBm) threshold, limit the transmit power by
 *  unifiTPCMaxPower2G/unifiTPCMaxPower5G. A Zero value reverts the power to
 *  a default state. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MIN_POWER_RSSI_THRESHOLD 0x1787

/*******************************************************************************
 * NAME          : UnifiTpcMaxPower2g
 * PSID          : 6024 (0x1788)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 52
 * DESCRIPTION   :
 *  Maximum power for 2.4GHz SISO interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold (quarter dbm). Should be greater than
 *  dot11PowerCapabilityMinImplemented. This value is read only once when an
 *  interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER2G 0x1788

/*******************************************************************************
 * NAME          : UnifiTpcMaxPower5g
 * PSID          : 6025 (0x1789)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  Maximum power for 5 GHz SISO interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold (quarter dbm). Should be greater than
 *  dot11PowerCapabilityMinImplemented. This value is read only once when an
 *  interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER5G 0x1789

/*******************************************************************************
 * NAME          : UnifiSarBackoff
 * PSID          : 6026 (0x178A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Max power values per band per index(quarter dBm).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SAR_BACKOFF 0x178A

/*******************************************************************************
 * NAME          : UnifiTpcUseAfterConnectRsp
 * PSID          : 6027 (0x178B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Use TPC only after MlmeConnect_Rsp has been received from the Host i.e.
 *  not during initial connection exchanges (EAPOL/DHCP operation) as RSSI
 *  readings might be inaccurate. This value is read only once when an
 *  interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_USE_AFTER_CONNECT_RSP 0x178B

/*******************************************************************************
 * NAME          : UnifiRadioLpRxRssiThresholdLower
 * PSID          : 6028 (0x178C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  The lower RSSI threshold for switching between low power rx and normal
 *  rx. If the RSSI avg of received frames is lower than this value for a
 *  VIF, then that VIF will vote against using low-power radio RX. Low power
 *  rx could negatively influence the receiver sensitivity.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_LP_RX_RSSI_THRESHOLD_LOWER 0x178C

/*******************************************************************************
 * NAME          : UnifiRadioLpRxRssiThresholdUpper
 * PSID          : 6029 (0x178D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -65
 * DESCRIPTION   :
 *  The upper RSSI threshold for switching between low power rx and normal
 *  rx. If the RSSI avg of received frames is higher than this value for a
 *  VIF, then that VIF will vote in favour of using low-power radio RX. Low
 *  power RX could negatively influence the receiver sensitivity.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_LP_RX_RSSI_THRESHOLD_UPPER 0x178D

/*******************************************************************************
 * NAME          : UnifiTestTxPowerEnable
 * PSID          : 6032 (0x1790)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X03DD
 * DESCRIPTION   :
 *  Test only: Bitfield to enable Control Plane Tx Power processing.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TEST_TX_POWER_ENABLE 0x1790

/*******************************************************************************
 * NAME          : UnifiLteCoexMaxPowerRssiThreshold
 * PSID          : 6033 (0x1791)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -55
 * DESCRIPTION   :
 *  Below this (dBm) threshold, switch to max power allowed by regulatory, if
 *  it has been previously reduced due to unifiTPCMinPowerRSSIThreshold. This
 *  value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_COEX_MAX_POWER_RSSI_THRESHOLD 0x1791

/*******************************************************************************
 * NAME          : UnifiLteCoexMinPowerRssiThreshold
 * PSID          : 6034 (0x1792)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -45
 * DESCRIPTION   :
 *  Above this(dBm) threshold, switch to minimum hardware supported - capped
 *  by unifiTPCMaxPower2G/unifiTPCMaxPower5G. Zero reverts the power to its
 *  default state. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_COEX_MIN_POWER_RSSI_THRESHOLD 0x1792

/*******************************************************************************
 * NAME          : UnifiLteCoexPowerReduction
 * PSID          : 6035 (0x1793)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 24
 * DESCRIPTION   :
 *  When LTE Coex Power Reduction provisions are met, impose a power cap of
 *  the regulatory domain less the amount specified by this MIB (quarter dB).
 *  This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LTE_COEX_POWER_REDUCTION 0x1793

/*******************************************************************************
 * NAME          : UnifiPmfAssociationComebackTimeDelta
 * PSID          : 6050 (0x17A2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1100
 * DESCRIPTION   :
 *  Timeout interval, in TU, for the TimeOut IE in the SA Query request
 *  frame.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PMF_ASSOCIATION_COMEBACK_TIME_DELTA 0x17A2

/*******************************************************************************
 * NAME          : UnifiTestTspecHack
 * PSID          : 6060 (0x17AC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Test only: Hack to allow in-house tspec testing
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TEST_TSPEC_HACK 0x17AC

/*******************************************************************************
 * NAME          : UnifiTestTspecHackValue
 * PSID          : 6061 (0x17AD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Test only: Saved dialog number of tspec request action frame from the
 *  Host
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TEST_TSPEC_HACK_VALUE 0x17AD

/*******************************************************************************
 * NAME          : UnifiDebugInstantDelivery
 * PSID          : 6069 (0x17B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Instant delivery control of the debug messages when set to true. Note:
 *  will not allow the host to suspend when set to True.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_INSTANT_DELIVERY 0x17B5

/*******************************************************************************
 * NAME          : UnifiDebugEnable
 * PSID          : 6071 (0x17B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Debug to host state. Debug is either is sent to the host or it isn't.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_ENABLE 0x17B7

/*******************************************************************************
 * NAME          : UnifiDPlaneDebug
 * PSID          : 6073 (0x17B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X203
 * DESCRIPTION   :
 *  Bit mask for turning on individual debug entities in the data_plane that
 *  if enabled effect throughput. See DPLP_DEBUG_ENTITIES_T in
 *  dplane_dplp_debug.h for bits. Default of 0x203 means dplp, ampdu and
 *  metadata logs are enabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_DEBUG 0x17B9

/*******************************************************************************
 * NAME          : UnifiNanDefaultEdcaParam
 * PSID          : 6075 (0x17BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  EDCA Parameters to be used as the default in NAN, as there is no AP to
 *  specify them, indexed by unifiAccessClassIndex octet 0 - AIFSN octet 1 -
 *  [7:4] ECW MAX [3:0] ECW MIN octet 2 ~ 3 - TXOP[7:0] TXOP[15:8] in 32 usec
 *  units for both non-HT and HT connections.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DEFAULT_EDCA_PARAM 0x17BB

/*******************************************************************************
 * NAME          : UnifiNanAllowCipherSuiteDowngrade
 * PSID          : 6076 (0x17BC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  PRI-12398: Workaround for Framework not selecting correct Cipher Suite
 *  ID. FW would use Cipher Suite ID '1' if peer is using it even if
 *  Framework requested ID '2'.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_ALLOW_CIPHER_SUITE_DOWNGRADE 0x17BC

/*******************************************************************************
 * NAME          : UnifiNanMaxQueuedFollowups
 * PSID          : 6077 (0x17BD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Maximum number of NAN Followup instances which can be queued in FW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_QUEUED_FOLLOWUPS 0x17BD

/*******************************************************************************
 * NAME          : UnifiNanMaxSdaTxRetryCount
 * PSID          : 6078 (0x17BE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Maximum number of reties to send SDA to a peer.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_SDA_TX_RETRY_COUNT 0x17BE

/*******************************************************************************
 * NAME          : UnifiNanSupportedCipherSuites
 * PSID          : 6079 (0x17BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 3
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Set of supported cipher suites. NanCipherSuiteType bit field format.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_SUPPORTED_CIPHER_SUITES 0x17BF

/*******************************************************************************
 * NAME          : UnifiNanActivated
 * PSID          : 6080 (0x17C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Neighbour Aware Networking (NAN)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_ACTIVATED 0x17C0

/*******************************************************************************
 * NAME          : UnifiNanBeaconCapabilities
 * PSID          : 6081 (0x17C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0620
 * DESCRIPTION   :
 *  The 16-bit field follows the coding of IEEE 802.11 Capability
 *  Information.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_BEACON_CAPABILITIES 0x17C1

/*******************************************************************************
 * NAME          : UnifiNanMaxConcurrentClusters
 * PSID          : 6082 (0x17C2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 1
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Maximum number of concurrent NAN clusters supported.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CONCURRENT_CLUSTERS 0x17C2

/*******************************************************************************
 * NAME          : UnifiNanMaxConcurrentPublishes
 * PSID          : 6083 (0x17C3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Maximum number of concurrent NAN Publish instances supported.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CONCURRENT_PUBLISHES 0x17C3

/*******************************************************************************
 * NAME          : UnifiNanMaxConcurrentSubscribes
 * PSID          : 6084 (0x17C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Maximum number of concurrent NAN Subscribe instances supported.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CONCURRENT_SUBSCRIBES 0x17C4

/*******************************************************************************
 * NAME          : UnifiNanMaxServiceNameLength
 * PSID          : 6085 (0x17C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2048
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  Not used by FW. Maximum Service Name Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_SERVICE_NAME_LENGTH 0x17C5

/*******************************************************************************
 * NAME          : UnifiNanMaxMatchFilterLength
 * PSID          : 6086 (0x17C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  Not used by FW. Maximum Match Filter Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_MATCH_FILTER_LENGTH 0x17C6

/*******************************************************************************
 * NAME          : UnifiNanMaxTotalMatchFilterLength
 * PSID          : 6087 (0x17C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2048
 * DEFAULT       : 512
 * DESCRIPTION   :
 *  Not used by FW. Maximum Total Match Filter Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_TOTAL_MATCH_FILTER_LENGTH 0x17C7

/*******************************************************************************
 * NAME          : UnifiNanMaxServiceSpecificInfoLength
 * PSID          : 6088 (0x17C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  Not used by FW. Maximum Service Specific Info Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_SERVICE_SPECIFIC_INFO_LENGTH 0x17C8

/*******************************************************************************
 * NAME          : UnifiNanMaxExtendedServiceSpecificInfoLen
 * PSID          : 6089 (0x17C9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2048
 * DEFAULT       : 2048
 * DESCRIPTION   :
 *  Not used by FW. Maximum Extended Service Specific Info Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_EXTENDED_SERVICE_SPECIFIC_INFO_LEN 0x17C9

/*******************************************************************************
 * NAME          : UnifiNanMaxSubscribeInterfaceAddresses
 * PSID          : 6090 (0x17CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 256
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Not used by FW. Maximum number interface addresses in subscribe filter
 *  list.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_SUBSCRIBE_INTERFACE_ADDRESSES 0x17CA

/*******************************************************************************
 * NAME          : UnifiNanMaxNdiInterfaces
 * PSID          : 6091 (0x17CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Not used by FW. Maximum NDI Interfaces. Note: This does not affect number
 *  of NDL Vifs supported by FW as they are hard coded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_NDI_INTERFACES 0x17CB

/*******************************************************************************
 * NAME          : UnifiNanMaxNdpSessions
 * PSID          : 6092 (0x17CC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 16
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Not used by FW. Maximum NDP Sessions. Note: This does not affect number
 *  of NDP sessions supported by FW as they are hard coded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_NDP_SESSIONS 0x17CC

/*******************************************************************************
 * NAME          : UnifiNanMaxAppInfoLength
 * PSID          : 6093 (0x17CD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2048
 * DEFAULT       : 2048
 * DESCRIPTION   :
 *  Not used by FW. Maximum App Info Length.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_APP_INFO_LENGTH 0x17CD

/*******************************************************************************
 * NAME          : UnifiNanMatchExpirationTime
 * PSID          : 6094 (0x17CE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Time limit in which Mlme will expire a match for discovered service. DW0
 *  interval is 8s and all NAN devices shall be awake in DW0.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MATCH_EXPIRATION_TIME 0x17CE

/*******************************************************************************
 * NAME          : UnifiNanPermittedChannels
 * PSID          : 6095 (0x17CF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0XFF, 0XFF, 0XFF, 0XFF, 0X7F, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Applicable Primary Channels mask. Defined in a uint64 represented by the
 *  octet string. Mapping defined in ChannelisationRules; i.e. Bit 14 maps to
 *  channel 36.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_PERMITTED_CHANNELS 0x17CF

/*******************************************************************************
 * NAME          : UnifiNanDefaultScanPeriod
 * PSID          : 6096 (0x17D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  The default value of scan period in seconds.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DEFAULT_SCAN_PERIOD 0x17D0

/*******************************************************************************
 * NAME          : UnifiNanMaxChannelSwitchTime
 * PSID          : 6097 (0x17D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5000
 * DESCRIPTION   :
 *  Maximum Channel Switch Time.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CHANNEL_SWITCH_TIME 0x17D1

/*******************************************************************************
 * NAME          : UnifiNanMacRandomisationActivated
 * PSID          : 6098 (0x17D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Enabling Mac Address Randomisation for NMI address.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAC_RANDOMISATION_ACTIVATED 0x17D2

/*******************************************************************************
 * NAME          : UnifiNanDefaultSchedule
 * PSID          : 6099 (0x17D3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 11
 * MAX           : 11
 * DEFAULT       :
 * DESCRIPTION   :
 *  Default Schedule to be proposed for NDL. FW will remove impossible /
 *  overlapping slots from proposal. Values must match with NAN Spec Time
 *  BitMap Control field values. Octet 0 ~ 1 : Ordered ChannelList[2] from
 *  most to least prefered channel, stated as channel number. Zero means not
 *  in used. Octet 2 : Bit Duration in TUs. Octet 3 ~ 4 : Period in TUs.
 *  Octet 5 ~ 6 : Start Offset in TUs. Octet 7 ~ 10 : TimeBitMap[4]
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DEFAULT_SCHEDULE 0x17D3

/*******************************************************************************
 * NAME          : hutsReadWriteDataElementInt32
 * PSID          : 6100 (0x17D4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of int32 type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_DATA_ELEMENT_INT32 0x17D4

/*******************************************************************************
 * NAME          : hutsReadWriteDataElementBoolean
 * PSID          : 6101 (0x17D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of boolean type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_DATA_ELEMENT_BOOLEAN 0x17D5

/*******************************************************************************
 * NAME          : hutsReadWriteDataElementOctetString
 * PSID          : 6102 (0x17D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 9
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of octet string
 *  type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_DATA_ELEMENT_OCTET_STRING 0x17D6

/*******************************************************************************
 * NAME          : hutsReadWriteTableInt16Row
 * PSID          : 6103 (0x17D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry table of int16
 *  type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_TABLE_INT16_ROW 0x17D7

/*******************************************************************************
 * NAME          : hutsReadWriteTableOctetStringRow
 * PSID          : 6104 (0x17D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry table of octet
 *  string type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_TABLE_OCTET_STRING_ROW 0x17D8

/*******************************************************************************
 * NAME          : hutsReadWriteRemoteProcedureCallInt32
 * PSID          : 6105 (0x17D9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000A0001
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Remote Procedure call read/write entry of int32
 *  type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_REMOTE_PROCEDURE_CALL_INT32 0x17D9

/*******************************************************************************
 * NAME          : hutsReadWriteRemoteProcedureCallOctetString
 * PSID          : 6107 (0x17DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 144
 * MAX           : 144
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Remote Procedure call read/write entry of octet
 *  string type.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_REMOTE_PROCEDURE_CALL_OCTET_STRING 0x17DB

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiInt16
 * PSID          : 6108 (0x17DC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -55
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of int16 type via
 *  internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_INT16 0x17DC

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiUint16
 * PSID          : 6109 (0x17DD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0730
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of unsigned int16
 *  type via internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_UINT16 0x17DD

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiUint32
 * PSID          : 6110 (0x17DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 30000
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of unsigned int32
 *  type via internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_UINT32 0x17DE

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiInt64
 * PSID          : 6111 (0x17DF)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : -9223372036854775808
 * MAX           : 9223372036854775807
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of int64 type via
 *  internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_INT64 0x17DF

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiBoolean
 * PSID          : 6112 (0x17E0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of boolean type
 *  via internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_BOOLEAN 0x17E0

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiOctetString
 * PSID          : 6113 (0x17E1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0X00, 0X18, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Data element read/write entry of octet string
 *  type via internal API.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_OCTET_STRING 0x17E1

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixedSizeTableRow
 * PSID          : 6114 (0x17E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Fixed size table rows of int16 type via
 *  internal API
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIXED_SIZE_TABLE_ROW 0x17E2

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiVarSizeTableRow
 * PSID          : 6115 (0x17E3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Variable size table rows of octet string type
 *  via internal API
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_VAR_SIZE_TABLE_ROW 0x17E3

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixSizeTableKey1Row
 * PSID          : 6116 (0x17E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Fixed size table rows of int16 type via
 *  internal API
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIX_SIZE_TABLE_KEY1_ROW 0x17E4

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixSizeTableKey2Row
 * PSID          : 6117 (0x17E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Reserved for HUTS tests - Fixed size table rows of int16 type via
 *  internal API
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIX_SIZE_TABLE_KEY2_ROW 0x17E5

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixVarSizeTableKey1Row
 * PSID          : 6118 (0x17E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The values stored in hutsReadWriteInternalAPIFixVarSizeTableKeys
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIX_VAR_SIZE_TABLE_KEY1_ROW 0x17E6

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixVarSizeTableKey2Row
 * PSID          : 6119 (0x17E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The values stored in hutsReadWriteInternalAPIFixVarSizeTableKeys
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIX_VAR_SIZE_TABLE_KEY2_ROW 0x17E7

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiFixSizeTableKeyRow
 * PSID          : 6120 (0x17E8)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received MPDUs discarded by the CCMP decryption algorithm.
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_FIX_SIZE_TABLE_KEY_ROW 0x17E8

/*******************************************************************************
 * NAME          : hutsReadWriteInternalApiVarSizeTableKeyRow
 * PSID          : 6121 (0x17E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 144
 * MAX           : 144
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a DPD LUT entry
 *******************************************************************************/
#define SLSI_PSID_HUTS_READ_WRITE_INTERNAL_API_VAR_SIZE_TABLE_KEY_ROW 0x17E9

/*******************************************************************************
 * NAME          : UnifiTestScanNoMedium
 * PSID          : 6122 (0x17EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Test only: Stop Scan from using the Medium to allow thruput testing.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TEST_SCAN_NO_MEDIUM 0x17EA

/*******************************************************************************
 * NAME          : UnifiDualBandConcurrency
 * PSID          : 6123 (0x17EB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. Identify
 *  whether the chip supports dualband concurrency or not (RSDB vs. VSDB).
 *  Set in the respective platform htf file.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DUAL_BAND_CONCURRENCY 0x17EB

/*******************************************************************************
 * NAME          : UnifiLoggerMaxDelayedEvents
 * PSID          : 6124 (0x17EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Maximum number of events to keep when host is suspended.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOGGER_MAX_DELAYED_EVENTS 0x17EC

/*******************************************************************************
 * NAME          : UnifiNanSkipNanChannelsIfNdlExists
 * PSID          : 6125 (0x17ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Disable Nan Scan if an NDL exists.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_SKIP_NAN_CHANNELS_IF_NDL_EXISTS 0x17ED

/*******************************************************************************
 * NAME          : UnifiNanKeepAliveTimeout
 * PSID          : 6140 (0x17FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 14
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. Timeout
 *  before terminating in seconds. 0 = Disabled. Capped to greater than 6
 *  seconds. This value is read only once when an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_KEEP_ALIVE_TIMEOUT 0x17FC

/*******************************************************************************
 * NAME          : UnifiNanKeepAliveTimeoutCheck
 * PSID          : 6141 (0x17FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 1
 * MAX           : 100
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  DO NOT SET TO A VALUE HIGHER THAN THE TIMEOUT. How long before keepalive
 *  timeout to start polling, in seconds. This value is read only once when
 *  an interface is added.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_KEEP_ALIVE_TIMEOUT_CHECK 0x17FD

/*******************************************************************************
 * NAME          : UnifiNanDataPathTrafficTimeout
 * PSID          : 6142 (0x17FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. Timeout
 *  before terminating NDP for no data traffic in seconds. 0 = Disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DATA_PATH_TRAFFIC_TIMEOUT 0x17FE

/*******************************************************************************
 * NAME          : UnifiNanDataPathSetupTimeout
 * PSID          : 6143 (0x17FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 20
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Timeout for a NDP setup.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DATA_PATH_SETUP_TIMEOUT 0x17FF

/*******************************************************************************
 * NAME          : UnifiNanWarmupTimeout
 * PSID          : 6144 (0x1800)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 120
 * DESCRIPTION   :
 *  Test MIB Only. Warmup time which FW doesn't set it's master preference.
 *  It's defined as 120s in spec.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_WARMUP_TIMEOUT 0x1800

/*******************************************************************************
 * NAME          : UnifiNanMultiVifSharedSlots
 * PSID          : 6146 (0x1802)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 9
 * DEFAULT       :
 * DESCRIPTION   :
 *  In multi vif scenarios, e.g. NAN + STA, NAN will share these slots with
 *  other VIFs. DW and NDC slots are not affected and are not removed due to
 *  MultiViF scenarios. Octet 0 : Bit Duration in TUs. Octet 1 ~ 2 : Period
 *  in TUs. Octet 3 ~ 4 : Start Offset in TUs. Octet 5 ~ 8 : TimeBitMap[4].
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MULTI_VIF_SHARED_SLOTS 0x1802

/*******************************************************************************
 * NAME          : UnifiRoamingInNdlActivated
 * PSID          : 6147 (0x1803)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When Activated, allow Roaming scan whilst NDL is active.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_IN_NDL_ACTIVATED 0x1803

/*******************************************************************************
 * NAME          : UnifiNanMultiBandNdlPermitted
 * PSID          : 6148 (0x1804)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. When
 *  permitted, NDLs can use multi-band schedules.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MULTI_BAND_NDL_PERMITTED 0x1804

/*******************************************************************************
 * NAME          : UnifiLoggerNanTrafficReportPeriod
 * PSID          : 6149 (0x1805)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Period in ms for sending NAN traffic updates to the host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOGGER_NAN_TRAFFIC_REPORT_PERIOD 0x1805

/*******************************************************************************
 * NAME          : UnifiNanUlwTest
 * PSID          : 6150 (0x1806)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Deprecated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_ULW_TEST 0x1806

/*******************************************************************************
 * NAME          : UnifiNanDiscoveryBeaconConfig
 * PSID          : 6151 (0x1807)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 7
 * DESCRIPTION   :
 *  Enables NAN discovery beacon transmission for NMI vif. Setting/clearing
 *  bit 0 enables/disabled discovery beacons on 2GHz band. Setting/clearing
 *  bit 1 enables/disabled discovery beacons on 5GHz band. Setting/clearing
 *  bit 3 enables/disabled discovery beacons when theres an NDL vif on same
 *  band.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_DISCOVERY_BEACON_CONFIG 0x1807

/*******************************************************************************
 * NAME          : UnifiNanForceWidestBandwidth
 * PSID          : 6152 (0x1808)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  When Activated, schedules on NDL VIF with same primary channel will have
 *  same bandwidth equal to widest one.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_FORCE_WIDEST_BANDWIDTH 0x1808

/*******************************************************************************
 * NAME          : UnifiNanScanBoRepeatCount
 * PSID          : 6153 (0x1809)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  The repeat count value to be used for blackouts installed for scan on NAN
 *  VIFs
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_SCAN_BO_REPEAT_COUNT 0x1809

/*******************************************************************************
 * NAME          : UnifiNanBlackoutFawReductionActivated
 * PSID          : 6154 (0x180A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  When Activated, Blackouts aligned to FAWs with count == 255 are converted
 *  to FAW Reduction.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_BLACKOUT_FAW_REDUCTION_ACTIVATED 0x180A

/*******************************************************************************
 * NAME          : UnifiNanRangingActivated
 * PSID          : 6155 (0x180B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Activate Ranging Support.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_RANGING_ACTIVATED 0x180B

/*******************************************************************************
 * NAME          : UnifiNanMaxConcurrentRangingAsInitiator
 * PSID          : 6156 (0x180C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Maximum number of concurrent NAN Ranging Sessions supported as initiator.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CONCURRENT_RANGING_AS_INITIATOR 0x180C

/*******************************************************************************
 * NAME          : UnifiNanMaxConcurrentRangingAsResponder
 * PSID          : 6157 (0x180D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 8
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Maximum number of concurrent NAN Ranging Sessions supported as responder.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_CONCURRENT_RANGING_AS_RESPONDER 0x180D

/*******************************************************************************
 * NAME          : UnifiNanRangingSetupTimeout
 * PSID          : 6158 (0x180E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 20
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Timeout for a Ranging setup.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_RANGING_SETUP_TIMEOUT 0x180E

/*******************************************************************************
 * NAME          : UnifiNanMaxRangingRetryCount
 * PSID          : 6159 (0x180F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Maximum number of retries FW does to perform a ranging (FTM burst
 *  sessions) and fails before terminating the ranging session.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_MAX_RANGING_RETRY_COUNT 0x180F

/*******************************************************************************
 * NAME          : UnifiNanTestRangingDistance
 * PSID          : 6160 (0x1810)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : millimeters
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Test MIB for Distance which Ranging reports. When this MIB is set, FW
 *  doesn't use FTM for ranging anymore.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_TEST_RANGING_DISTANCE 0x1810

/*******************************************************************************
 * NAME          : UnifiNansdfTransmissionOutsideDwPermitted
 * PSID          : 6161 (0x1811)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Golden Certification MIB don't delete, change PSID or name. When this MIB
 *  is set FW is allowed to send SDF during remote CRB otherwise FW only
 *  sends SDF during DW.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NANSDF_TRANSMISSION_OUTSIDE_DW_PERMITTED 0x1811

/*******************************************************************************
 * NAME          : UnifiNanTestMasterPreference
 * PSID          : 6170 (0x181A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Test MIB for Master Preference value. This overwrites host value, and to
 *  take effect this MIB should be set before NAN start(FW reads this MIB on
 *  start and config request signals.).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NAN_TEST_MASTER_PREFERENCE 0x181A

/*******************************************************************************
 * NAME          : UnifiRegulatoryParameters
 * PSID          : 8011 (0x1F4B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Regulatory parameters. Each row of the table contains the regulatory
 *  rules for one country: octet 0 - first character of alpha2 code for
 *  country octet 1 - second character of alpha2 code for country octet 2 -
 *  regulatory domain for the country Followed by the rules for the country,
 *  numbered 0..n in this description octet 7n+3 - LSB start frequency octet
 *  7n+4 - MSB start frequency octet 7n+5 - LSB end frequency octet 7n+6 -
 *  MSB end frequency octet 7n+7 - maximum bandwidth octet 7n+8 - maximum
 *  power octet 7n+9 - rule flags
 *******************************************************************************/
#define SLSI_PSID_UNIFI_REGULATORY_PARAMETERS 0x1F4B

/*******************************************************************************
 * NAME          : UnifiSupportedChannels
 * PSID          : 8012 (0x1F4C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 20
 * DEFAULT       :  {0X01,0X0D,0X24,0X04,0X34,0X04,0X64,0X0C,0X95,0X05}
 * DESCRIPTION   :
 *  For Test Only. Supported 20MHz channel primary frequency grouped in
 *  sub-bands. For each sub-band: starting channel number, followed by number
 *  of channels.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPORTED_CHANNELS 0x1F4C

/*******************************************************************************
 * NAME          : UnifiDefaultCountry
 * PSID          : 8013 (0x1F4D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 3
 * DEFAULT       :
 * DESCRIPTION   :
 *  Host set Default Code.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEFAULT_COUNTRY 0x1F4D

/*******************************************************************************
 * NAME          : UnifiCountryList
 * PSID          : 8014 (0x1F4E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 270
 * DEFAULT       : (Too Large to display)
 * DESCRIPTION   :
 *  Defines the ordered list of countries present in unifiRegulatoryTable.
 *  Each country is coded as 2 ASCII characters. If unifiRegulatoryTable is
 *  modified, such as a country is either added, deleted or its relative
 *  location is modified, has to be updated as well.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_COUNTRY_LIST 0x1F4E

/*******************************************************************************
 * NAME          : UnifiOperatingClassParamters
 * PSID          : 8015 (0x1F4F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 1
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Supported Operating Class parameters.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OPERATING_CLASS_PARAMTERS 0x1F4F

/*******************************************************************************
 * NAME          : UnifiVifCountry
 * PSID          : 8016 (0x1F50)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Per VIf: Each VIF updates its Country Code for the Host to read
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VIF_COUNTRY 0x1F50

/*******************************************************************************
 * NAME          : UnifiNoCellMaxPower
 * PSID          : 8017 (0x1F51)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  Max power values for included channels (quarter dBm).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_CELL_MAX_POWER 0x1F51

/*******************************************************************************
 * NAME          : UnifiNoCellIncludedChannels
 * PSID          : 8018 (0x1F52)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0X00, 0X18, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Channels applicable. Defined in a uint64 represented by the octet string.
 *  First byte of the octet string maps to LSB. Bit 0 maps to channel 1.
 *  Mapping defined in ChannelisationRules.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_CELL_INCLUDED_CHANNELS 0x1F52

/*******************************************************************************
 * NAME          : UnifiRegDomVersion
 * PSID          : 8019 (0x1F53)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Regulatory domain version encoded into 2 bytes, major version as MSB and
 *  minor version as LSB
 *******************************************************************************/
#define SLSI_PSID_UNIFI_REG_DOM_VERSION 0x1F53

/*******************************************************************************
 * NAME          : UnifiDefaultCountryWithoutCH12CH13
 * PSID          : 8020 (0x1F54)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Update the default country code to ensure CH12 and CH13 are not used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEFAULT_COUNTRY_WITHOUT_CH12_CH13 0x1F54

/*******************************************************************************
 * NAME          : UnifiReadReg
 * PSID          : 8051 (0x1F73)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read value from a register and return it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_READ_REG 0x1F73

#ifdef __cplusplus
}
#endif
#endif /* SLSI_MIB_H__ */
