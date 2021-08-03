#ifndef _BT_AUDIO_H
#define _BT_AUDIO_H

/* Version number */
#define SCSC_BT_AUDIO_ABOX_VERSION_MAJOR		(0x01)
#define SCSC_BT_AUDIO_ABOX_VERSION_MINOR		(0x01)

/* The A-Box uses a ARM Cortex-A7 with a 64 bytes cache line whereas
 * the WLBT uses a ARM Cortex-R4 with a 32 bytes cache line. The data
 * needs to be aligned to the largest cache line
 */
#define SCSC_BT_AUDIO_ABOX_DCACHE_LINE_WIDTH		(64)

/* kernel page size used for memory alignment */
#define SCSC_BT_AUDIO_PAGE_SIZE				(PAGE_SIZE)

/* Total size of the shared memory in one direction */
#define SCSC_BT_AUDIO_ABOX_DATA_SIZE			(128 * SCSC_BT_AUDIO_ABOX_DCACHE_LINE_WIDTH)

/* Size of the buffer for each interface */
#define SCSC_BT_AUDIO_ABOX_IF_0_SIZE			(10 * SCSC_BT_AUDIO_ABOX_DCACHE_LINE_WIDTH)
#define SCSC_BT_AUDIO_ABOX_IF_1_SIZE			(10 * SCSC_BT_AUDIO_ABOX_DCACHE_LINE_WIDTH)

/* Feature mask */
#define SCSC_BT_AUDIO_FEATURE_STREAMING_IF_0		(0x00000001)
#define SCSC_BT_AUDIO_FEATURE_STREAMING_IF_1		(0x00000002)
#define SCSC_BT_AUDIO_FEATURE_MESSAGING			(0x00000004)
#define SCSC_BT_AUDIO_FEATURE_A2DP_OFFLOAD		(0x00000008)

struct scsc_bt_audio_abox {
	/* AP RW - BT R4 RO - ABOX RO - 128 octets */

	/* header */
	uint32_t magic_value;
	uint16_t version_major;
	uint16_t version_minor;

	/* align to cache line (32 bytes) */
	uint8_t  reserved1[0x18];

	/* streaming interface 0 */
	uint32_t abox_to_bt_streaming_if_0_size;

	/* offset in abox_to_bt_streaming_if_data */
	uint32_t abox_to_bt_streaming_if_0_offset;

	uint32_t bt_to_abox_streaming_if_0_size;

	/* offset in bt_to_abox_streaming_if_data */
	uint32_t bt_to_abox_streaming_if_0_offset;

	/* streaming interface 1 */
	uint32_t abox_to_bt_streaming_if_1_size;

	/* offset in abox_to_bt_streaming_if_data */
	uint32_t abox_to_bt_streaming_if_1_offset;

	uint32_t bt_to_abox_streaming_if_1_size;

	/* offset in bt_to_abox_streaming_if_data */
	uint32_t bt_to_abox_streaming_if_1_offset;

	/* reserved room for additional AP information (64 bytes) */
	uint8_t  reserved2[0x40];

	/* AP RO - BT R4 RO - ABOX RW - 64 octets */
	uint32_t abox_fw_features;

	/* BTWLAN audio and ABOX firmware may start at different time
	 * and a number of interrupts may be already triggered by ABOX
	 * firmware before BTWLAN audio can process them causing
	 * misalignment between the two systems (e.g. both accessing
	 * the same buffer at the same time). The fields below provide
	 * information about which half of the double buffer the ABOX
	 * firmware is processing using 0/1.
	 * filled by ABOX firmware at each interrupt (read/write) and
	 * initialised to 0 by BT driver.
	 */
	uint32_t bt_to_abox_streaming_if_0_current_index;
	uint32_t abox_to_bt_streaming_if_0_current_index;
	uint32_t bt_to_abox_streaming_if_1_current_index;
	uint32_t abox_to_bt_streaming_if_1_current_index;

	/* align to cache line (64 bytes) */
	uint8_t  reserved3[0x2C];

	/* AP RO - BT R4 RW - ABOX RO - 64 octets */
	uint32_t bt_fw_features;

	/* sample rate (Hz) of the streaming interfaces */
	uint32_t streaming_if_0_sample_rate;
	uint32_t streaming_if_1_sample_rate;

	uint8_t  reserved4[0x34];

	/* payload */

	/* AP RO - BT R4 RO - ABOX RW - multiple of 64 octets */
	uint8_t  abox_to_bt_streaming_if_data[SCSC_BT_AUDIO_ABOX_DATA_SIZE];

	/* AP RO - BT R4 RW - ABOX RO - multiple of 64 octets */
	uint8_t  bt_to_abox_streaming_if_data[SCSC_BT_AUDIO_ABOX_DATA_SIZE];
};

/* Magic value */
#define SCSC_BT_AUDIO_ABOX_MAGIC_VALUE \
	(((offsetof(struct scsc_bt_audio_abox, abox_to_bt_streaming_if_0_size) << 20) | \
	(offsetof(struct scsc_bt_audio_abox, bt_to_abox_streaming_if_0_current_index) << 10) | \
	offsetof(struct scsc_bt_audio_abox, abox_to_bt_streaming_if_data)) ^ \
	0xBA12EF82)

#ifndef CONFIG_SOC_EXYNOS7885
struct scsc_bt_audio {
	struct device			*dev;
	struct scsc_bt_audio_abox	*abox_virtual;
	struct scsc_bt_audio_abox	*abox_physical;
	int (*dev_iommu_map)(struct device *, phys_addr_t, size_t);
	void (*dev_iommu_unmap)(struct device *, size_t);
};

struct scsc_bt_audio_driver {
	const char *name;
	void (*probe)(struct scsc_bt_audio_driver *driver, struct scsc_bt_audio *bt_audio);
	void (*remove)(struct scsc_bt_audio *bt_audio);
};

phys_addr_t scsc_bt_audio_get_paddr_buf(bool tx);
unsigned int scsc_bt_audio_get_rate(int id);
int scsc_bt_audio_register(struct device *dev,
		int (*dev_iommu_map)(struct device *, phys_addr_t, size_t),
		void (*dev_iommu_unmap)(struct device *, size_t));
int scsc_bt_audio_unregister(struct device *dev);
#else
struct scsc_bt_audio {
	struct device			*dev;
	struct scsc_bt_audio_abox	*abox_virtual;
	struct scsc_bt_audio_abox	*abox_physical;
};

struct scsc_bt_audio_driver {
	const char *name;
	void (*probe)(struct scsc_bt_audio_driver *driver, struct scsc_bt_audio *bt_audio);
	void (*remove)(struct scsc_bt_audio *bt_audio);
};

int scsc_bt_audio_register(struct scsc_bt_audio_driver *driver);
int scsc_bt_audio_unregister(struct scsc_bt_audio_driver *driver);
#endif

#endif /* _BT_AUDIO_H */
