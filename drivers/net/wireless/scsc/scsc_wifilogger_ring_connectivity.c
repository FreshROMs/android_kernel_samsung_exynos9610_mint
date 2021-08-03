/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 *****************************************************************************/
/* Implements */
#include "scsc_wifilogger_ring_connectivity.h"

/* Uses */
#include <stdarg.h>
#include "scsc_wifilogger_ring_connectivity_api.h"
#include "scsc_wifilogger_internal.h"

static struct scsc_wlog_ring *the_ring;

u32 cring_lev;
EXPORT_SYMBOL(cring_lev);

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
#include "scsc_wifilogger_debugfs.h"

static struct scsc_wlog_debugfs_info di;

#endif /* CONFIG_SCSC_WIFILOGGER_DEBUGFS */

bool scsc_wifilogger_ring_connectivity_init(void)
{
	struct scsc_wlog_ring *r = NULL;
#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	struct scsc_ring_test_object *rto;
#endif

	r = scsc_wlog_ring_create(WLOGGER_RCONNECT_NAME,
				  RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				  ENTRY_TYPE_CONNECT_EVENT, 32768 * 8,
				  WIFI_LOGGER_CONNECT_EVENT_SUPPORTED,
				  NULL, NULL, NULL);

	if (!r) {
		SCSC_TAG_ERR(WLOG, "Failed to CREATE WiFiLogger ring: %s\n",
			     WLOGGER_RCONNECT_NAME);
		return false;
	}
	scsc_wlog_register_verbosity_reference(r, &cring_lev);

	if (!scsc_wifilogger_register_ring(r)) {
		SCSC_TAG_ERR(WLOG, "Failed to REGISTER WiFiLogger ring: %s\n",
			     WLOGGER_RCONNECT_NAME);
		scsc_wlog_ring_destroy(r);
		return false;
	}
	the_ring = r;

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	rto = init_ring_test_object(the_ring);
	if (rto)
		scsc_register_common_debugfs_entries(the_ring->st.name, rto, &di);
#endif

	return true;
}

/**** Producer API ******/

int scsc_wifilogger_ring_connectivity_fw_event(wlog_verbose_level lev, u16 fw_event_id,
					       u64 fw_timestamp, void *fw_bulk_data, size_t fw_blen)
{
	struct scsc_wifi_ring_buffer_driver_connectivity_event	event_item;

	if (!the_ring)
		return 0;

	SCSC_TAG_DEBUG(WLOG, "EL -- RX MLME_EVENT_LOG_INFO - event_id[%d] @0x%x\n",
		       fw_event_id, fw_timestamp);
	event_item.event = fw_event_id;

	return scsc_wlog_write_record(the_ring, fw_bulk_data, fw_blen, &event_item,
				      sizeof(event_item), lev, fw_timestamp);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_connectivity_fw_event);

int scsc_wifilogger_ring_connectivity_driver_event(wlog_verbose_level lev,
						   u16 driver_event_id, unsigned int tag_count, ...)
{
	int							i;
	u64							timestamp;
	va_list							ap;
	u8							tlvs[MAX_TLVS_SZ];
	size_t							tlvs_sz = 0;
	struct scsc_tlv_log					*tlv = NULL;
	struct scsc_wifi_ring_buffer_driver_connectivity_event	event_item;

	if (!the_ring)
		return 0;

	timestamp = local_clock();

	SCSC_TAG_DEBUG(WLOG, "EL -- RX Driver CONNECTIVITY EVENT - event_id[%d] @0x%x\n",
		       driver_event_id, timestamp);

	event_item.event = driver_event_id;
	va_start(ap, tag_count);
	for (i = 0; i < tag_count &&
	     tlvs_sz + sizeof(*tlv) < MAX_TLVS_SZ; i++) {
		tlv = (struct scsc_tlv_log *)(tlvs + tlvs_sz);
		tlv->tag = (u16)va_arg(ap, int);
		tlv->length = (u16)va_arg(ap, int);
		if (tlvs_sz + sizeof(*tlv) + tlv->length >= MAX_TLVS_SZ) {
			WARN(true,
			     "TLVs container too small [%d]....truncating event's tags !\n",
			     MAX_TLVS_SZ);
			break;
		}
		memcpy(&tlv->value, va_arg(ap, u8 *), tlv->length);
		tlvs_sz += sizeof(*tlv) + tlv->length;
	}
	va_end(ap);

	return scsc_wlog_write_record(the_ring, tlvs, tlvs_sz, &event_item,
				      sizeof(event_item), lev, timestamp);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_connectivity_driver_event);
