/*
 * internal functions for TFA layer (not shared with SRV and HAL layer!)
 */

#ifndef __TFA_INTERNAL_H__
#define __TFA_INTERNAL_H__

#include "tfa_dsp_fw.h"
#include <sound/tfa_ext.h>
#include "tfa_service.h"
#include "config.h"

#if __GNUC__ >= 4
#define TFA_INTERNAL __attribute__((visibility("hidden")))
#else
#define TFA_INTERNAL
#endif

#define TFA98XX_GENERIC_SLAVE_ADDRESS 0x1C

#define MAX_HANDLES 4

enum feature_support {
	SUPPORT_NOT_SET, /* the default is not set yet, so = 0 */
	SUPPORT_NO,
	SUPPORT_YES
};

enum instream_state {
	BIT_PSTREAM = 1, /* b0 */
	BIT_CSTREAM = 2, /* b1 */
	BIT_SAMSTREAM = 4 /* b2 */
};

#if defined(TFADSP_DSP_BUFFER_POOL)
enum pool_control {
	POOL_NOT_SUPPORT,
	POOL_ALLOC,
	POOL_FREE,
	POOL_GET,
	POOL_RETURN,
	POOL_MAX_CONTROL
};

#define POOL_MAX_INDEX 6

struct tfa98xx_buffer_pool {
	int size;
	unsigned char in_use;
	void *pool;
};
#endif /* TFADSP_DSP_BUFFER_POOL */

/*
 * tfa98xx control structure gathers data related to a single control
 * (a 'control' can be related to an interface file)
 * Some operations can be flagged as deferrable, meaning that they can be
 *   scheduled for later execution. This can be used for operations that
 *   require the i2s clock to run, and if it is not available while applying
 *   the control.
 * The Some operations can as well be cache-able (supposedly they are the same
 *   operations as the deferrable). Cache-able means that the status or
 *   register value may not be accesable while accessing the control. Caching
 *   allows to get the last programmed value.
 *
 * Fields:
 * deferrable:
 *   true: means the action or register accces can be run later (for example
 *   when an active i2s clock will be available).
 *   false: meams the operation can be applied immediately
 * triggered: true if the deferred operation was triggered and is scheduled
 *   to run later
 * wr_value: the value to write in the deferred action (if applicable)
 * rd_value: the cached value to report on a cached read (if applicable)
 * rd_valid: true if the rd_value was initialized (and can be reported)
 */

struct tfa98xx_control {
	bool deferrable;
	bool triggered;
	int wr_value;
	int rd_value;
	bool rd_valid;
};

struct tfa98xx_controls {
	struct tfa98xx_control otc;
	struct tfa98xx_control mtpex;
	struct tfa98xx_control calib;
	/* struct tfa98xx_control r; */
	/* struct tfa98xx_control temp; */
};

typedef enum tfa98xx_error (*dsp_msg_t)(tfa98xx_handle_t handle,
	int length, const char *buf);
typedef enum tfa98xx_error (*dsp_msg_read_t)(tfa98xx_handle_t handle,
	int length, unsigned char *bytes);
typedef enum tfa98xx_error (*reg_read_t)(tfa98xx_handle_t handle,
	unsigned char subaddress, unsigned short *value);
typedef enum tfa98xx_error (*reg_write_t)(tfa98xx_handle_t handle,
	unsigned char subaddress, unsigned short value);
typedef enum tfa98xx_error (*mem_read_t)(tfa98xx_handle_t handle,
	unsigned int start_offset, int num_words, int *p_values);
typedef enum tfa98xx_error (*mem_write_t)(tfa98xx_handle_t handle,
	unsigned short address, int value, int memtype);

struct tfa_device_ops {
	enum tfa98xx_error (*tfa_init)(tfa98xx_handle_t dev_idx);
	enum tfa98xx_error (*tfa_dsp_reset)
		(tfa98xx_handle_t dev_idx, int state);
	enum tfa98xx_error (*tfa_dsp_system_stable)
		(tfa98xx_handle_t handle, int *ready);
	enum tfa98xx_error (*tfa_dsp_write_tables)
		(tfa98xx_handle_t dev_idx, int sample_rate);
	enum tfa98xx_error (*tfa_set_boost_trip_level)
		(tfa98xx_handle_t handle, int Re25C);

	dsp_msg_t	dsp_msg;
	dsp_msg_read_t	dsp_msg_read;
	reg_read_t	reg_read;
	reg_write_t	reg_write;
	mem_read_t	mem_read;
	mem_write_t	mem_write;

	struct tfa98xx_controls controls;
};

struct tfa98xx_handle_private {
	int in_use;
	int buffer_size;
	unsigned char slave_address;
	unsigned short rev;
	unsigned char tfa_family; /* tfa1/tfa2 */
	enum feature_support support_drc;
	enum feature_support support_framework;
	enum feature_support support_saam;
	int sw_feature_bits[2]; /* cached feature bits data */
	int hw_feature_bits; /* cached feature bits data */
	int profile;	/* cached active profile */
	int vstep[2]; /* cached active vsteps */
	/* TODO add? unsigned char rev_major; */
	/* void tfa98xx_rev(int *major, int *minor, int *revision) */
	/* unsigned char rev_minor; */
	/* unsigned char rev_build; */
	unsigned char spkr_count;
	unsigned char spkr_select;
	unsigned char spkr_damaged;
	unsigned char support_tcoef;
	enum tfa98xx_dai_bitmap daimap;
	int mohm[3]; /* > calibration values in milli ohms -1 is error */
	int temp; /* > calibration temperature in degC */
	struct tfa_device_ops dev_ops;
	uint16_t interrupt_enable[3];
	uint16_t interrupt_status[3];
	int ext_dsp; /* respond to external DSP: 0:none, 1:cold, 2:warm  */
	int is_cold; /* respond to MANSTATE, before tfa_run_speaker_boost */
	int is_bypass; /* respond to vstep in profile, to check bypass */
	enum tfadsp_event_en tfadsp_event;
	int default_boost_trip_level;
	int saam_use_case;
	/* 0: not in use, 1: RaM / SaM only, 2: bidirectional */
	int stream_state;
	/* b0: pstream (Rx), b1: cstream (Tx), b2: samstream (SaaM) */
#if defined(TFA_BLACKBOX_LOGGING)
	configure_log_t log_set_cb;
	update_log_t log_get_cb;
#endif
#if defined(TFADSP_DSP_BUFFER_POOL)
	struct tfa98xx_buffer_pool buf_pool[POOL_MAX_INDEX];
#endif
};

/* tfa98xx: tfa_device_ops */
#if (defined(USE_TFA9872) || defined(TFA98XX_FULL))
void tfa9872_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9912) || defined(TFA98XX_FULL))
void tfa9912_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9888) || defined(TFA98XX_FULL))
void tfa9888_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9891) || defined(TFA98XX_FULL))
void tfa9891_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9897) || defined(TFA98XX_FULL))
void tfa9897_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9896) || defined(TFA98XX_FULL))
void tfa9896_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9890) || defined(TFA98XX_FULL))
void tfa9890_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9895) || defined(TFA98XX_FULL))
void tfa9895_ops(struct tfa_device_ops *ops);
#endif

/* tfa98xx.c */
extern TFA_INTERNAL struct tfa98xx_handle_private handles_local[];
TFA_INTERNAL int tfa98xx_handle_is_open(tfa98xx_handle_t h);
TFA_INTERNAL enum tfa98xx_error tfa98xx_check_rpc_status
(tfa98xx_handle_t handle, int *p_rpc_status);
TFA_INTERNAL enum tfa98xx_error tfa98xx_wait_result
(tfa98xx_handle_t handle, int waitRetryCount);
TFA_INTERNAL void tfa98xx_apply_deferred_calibration(tfa98xx_handle_t handle);
TFA_INTERNAL void tfa98xx_deferred_calibration_status
(tfa98xx_handle_t handle, int calibrate_done);
TFA_INTERNAL int print_calibration
(tfa98xx_handle_t handle, char *str, size_t size);

#if defined(TFADSP_DSP_BUFFER_POOL)
TFA_INTERNAL int tfa98xx_buffer_pool_access
(tfa98xx_handle_t handle, int r_index, size_t g_size, int control);
#endif
#endif /* __TFA_INTERNAL_H__ */
