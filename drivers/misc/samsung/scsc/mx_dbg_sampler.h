/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MX_DBG_SAMPLER_H__
#define __MX_DBG_SAMPLER_H__


/**
 * Debug Sampler DRAM Buffer descriptor.
 *
 * Initialised by Debug Sampler Driver on AP and passed by
 * reference to Debug Sampler (Proxy) on R4 (by reference in
 * WLAN config).
 *
 * Integer fields are LittleEndian.
 */
struct debug_sampler_buffer_info {
	/**
	 * Offset of circular octet buffer w.r.t. shared dram start
	 */
	uint32_t buf_offset;

	/**
	 * Circular buffer length (octets, 2^n)
	 *
	 * Default = 32KiB default
	 */
	uint32_t buf_len;

	/**
	 * Offset of 32bit write index (not wrapped, counts octets) w.r.t. shared dram start
	 */
	uint32_t write_index_offset;

	/**
	 * To AP interrupt number (0 – 15)
	 */
	uint32_t intr_num;
};

struct debug_sampler_sample_spec {
	/**
	 * -relative address of Location to sample (usually a register)
	 *
	 * Default = 0x00000000
	 */
	uint32_t source_addr;

	/**
	 * Number of significant octets (1,2 or 4) to log (lsbytes from source)
	 *
	 * Default = 4
	 */
	uint32_t num_bytes;

	/**
	 * Sampling period.
	 *
	 * 0 means as fast as possible (powers of 2 only)
	 *
	 * Default = 0
	 */
	uint32_t period_usecs;
};


/**
 * Debug Sampler Config Structure.
 *
 * This structure is allocated and initialised by the Debug Sampler driver
 * on the AP and passed via the service_start message.
 */
struct debug_sampler_config {
	/**
	 * Config Structure Version (= DBGSAMPLER_CONFIG_VERSION)
	 *
	 * Set by driver, checked by service.
	 */
	uint32_t                         version;

	/**
	 * To-host circular buffer desciptor.
	 */
	struct debug_sampler_buffer_info buffer_info;

	/**
	 * Init/default sampling specification.
	 *
	 * (There is also an API on R4 to allow dynamic specification
	 * change - e.g. by WLAN service)
	 */
	struct debug_sampler_sample_spec sample_spec;

	/**
	 * Start/stop sampling when service is started/stopped?
	 *
	 * (There is also an API on R4 to allow dynamic start/stop
	 * - e.g. by WLAN service)
	 *
	 * Default = 0
	 */
	uint32_t auto_start;
};

struct debug_sampler_align {

	struct debug_sampler_config config __aligned(4);

	u32                                index;

	void *mem                          __aligned(64);

};


#endif /* __MX_DBG_SAMPLER_H__ */


