/*
 * Copyright 2015 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

#ifndef TFA_SERVICE_H
#define TFA_SERVICE_H

/* #include "config.h"
 * workaround for Visual Studio:
 * fatal error C1083: Cannot open include file:
 * 'config.h': No such file or directory
 */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#include "NXP_I2C.h"
#endif

/* Linux kernel module defines TFA98XX_GIT_VERSIONS
 * in the linux_driver/Makefile
 */
#if !defined(TFA98XX_GIT_VERSIONS)
#include "versions.h"
#endif
#ifdef TFA98XX_GIT_VERSIONS
  #define TFA98XX_API_REV_STR TFA98XX_GIT_VERSIONS
#else
	/* #warning update TFA98XX_API_REV_STR manually */
  #define TFA98XX_API_REV_STR "v4.2.0-Sep.17,2017"
#endif

/* log */
extern int tfa98xx_log_revision;
extern int tfa98xx_log_subrevision;
extern int tfa98xx_log_i2c_devicenum;
extern int tfa98xx_log_i2c_slaveaddress;
extern int tfa98xx_log_start_cnt;
extern int tfa98xx_log_tfa_family;

/*
 * data previously defined in Tfa9888_dsp.h
 */
#define MEMTRACK_MAX_WORDS           150
#define LSMODEL_MAX_WORDS            150
#define TFA98XX_MAXTAG              (138)
#define FW_VAR_API_VERSION          (521)

/* Indexes and scaling factors of GetLSmodel */
#define tfa9888_FS_IDX				128
#define tfa9888_LEAKAGE_FACTOR_IDX	130
#define tfa9888_RE_CORRECTION_IDX	131
#define tfa9888_Bl_IDX				132
#define ReZ_IDX						147

#define tfa9872_LEAKAGE_FACTOR_IDX	128
#define tfa9872_RE_CORRECTION_IDX	129
#define tfa9872_Bl_IDX				130

#define FS_SCALE              (double)1
#define LEAKAGE_FACTOR_SCALE   (double)8388608
#define RE_CORRECTION_SCALE    (double)8388608
#define Bl_SCALE              (double)2097152
#define TCOEF_SCALE           (double)8388608

/* ---------------------------- Max1 ---------------------------- */
/* Headroom applied to the main input signal */
#define SPKRBST_HEADROOM		7
/* Exponent used for AGC Gain related variables */
#define SPKRBST_AGCGAIN_EXP		SPKRBST_HEADROOM
#define SPKRBST_TEMPERATURE_EXP		9
/* Exponent used for Gain Corection related variables */
#define SPKRBST_LIMGAIN_EXP		4
#define SPKRBST_TIMECTE_EXP		1
#define DSP_MAX_GAIN_EXP		7
/* -------------------------------------------------------------- */

/* speaker related parameters */
#define TFA2_SPEAKERPARAMETER_LENGTH		(3*151)	/* MAX2=450 */
#define TFA1_SPEAKERPARAMETER_LENGTH		(3*141)	/* MAX1=423 */

/* vstep related parameters */
#define TFA2_ALGOPARAMETER_LENGTH		(3*304)
/* N1B = (304) 305 is including the cmd-id */
#define TFA72_ALGOPARAMETER_LENGTH_MONO		(3*183)
#define TFA72_ALGOPARAMETER_LENGTH_STEREO	(3*356)
#define TFA2_MBDRCPARAMETER_LENGTH		(3*152)
/* 154 is including the cmd-id */
#define TFA72_MBDRCPARAMETER_LENGTH		(3*98)
#define TFA1_PRESET_LENGTH			87
#define TFA1_DRC_LENGTH				381	/* 127 words */
#define TFA2_FILTERCOEFSPARAMETER_LENGTH	(3*168)
/* 170 is including the cmd-id */
#define TFA72_FILTERCOEFSPARAMETER_LENGTH	(3*156)

/* Maximum number of retries for DSP result
 * Keep this value low!
 * If certain calls require longer wait conditions, the
 * application should poll, not the API
 * The total wait time depends on device settings. Those
 * are application specific.
 */
#define TFA98XX_WAITRESULT_NTRIES          40
#define TFA98XX_WAITRESULT_NTRIES_LONG   2000

/* following lengths are in bytes */
#define TFA98XX_PRESET_LENGTH              87

/* make full the default */
#if !(defined(USE_TFA9890) || defined(USE_TFA9895) \
	|| defined(USE_TFA9897) || defined(USE_TFA9896))
#ifndef TFA98XX_FULL
#define TFA98XX_FULL
#endif
#endif

#if (defined(USE_TFA9890) || defined(USE_TFA9897) \
	|| defined(USE_TFA9896))
#define TFA98XX_CONFIG_LENGTH           165
#else
#if (defined(USE_TFA9895) || defined(TFA98XX_FULL))
#define TFA98XX_CONFIG_LENGTH           201
#define TFA98XX_DRC_LENGTH              381	/* 127 words */
/* not used in current driver */
/* typedef unsigned char tfa98xx_drc_parameters_t[TFA98XX_DRC_LENGTH]; */
#endif
#endif

/*
 * MUST BE CONSISTENT: either one uses opaque arrays of bytes, or not!!!
 */
/* not used in current driver */
/* typedef unsigned char tfa98xx_config_t[TFA98XX_CONFIG_LENGTH]; */
/* typedef unsigned char tfa98xx_preset_t[TFA98XX_PRESET_LENGTH]; */

/*
 * Type containing all the possible errors that can occur
 */
enum tfa98xx_error {
	TFA_ERROR = -1,
	TFA98XX_ERROR_OK = 0,
	TFA98XX_ERROR_DEVICE,		/* 1. in sync with tfa_error */
	TFA98XX_ERROR_BAD_PARAMETER,	/* 2. */
	TFA98XX_ERROR_FAIL,             /* 3. generic failure, avoid mislead */
	TFA98XX_ERROR_NO_CLOCK,         /* 4. no clock detected */
	TFA98XX_ERROR_STATE_TIMED_OUT,	/* 5. */
	TFA98XX_ERROR_DSP_NOT_RUNNING,	/* 6. communication with DSP failed */
	TFA98XX_ERROR_AMPON,            /* 7. amp is still running */
	TFA98XX_ERROR_NOT_OPEN,	        /* 8. the given handle is not open */
	TFA98XX_ERROR_IN_USE,	        /* 9. too many handles */
	TFA98XX_ERROR_BUFFER_TOO_SMALL, /* 10. if a buffer is too small */
	/* the expected response did not occur within the expected time */
	TFA98XX_ERROR_BUFFER_RPC_BASE = 100,
	TFA98XX_ERROR_RPC_BUSY = 101,
	TFA98XX_ERROR_RPC_MOD_ID = 102,
	TFA98XX_ERROR_RPC_PARAM_ID = 103,
	TFA98XX_ERROR_RPC_INVALID_CC = 104,
	TFA98XX_ERROR_RPC_INVALID_SEQ = 105,
	TFA98XX_ERROR_RPC_INVALID_PARAM = 106,
	TFA98XX_ERROR_RPC_BUFFER_OVERFLOW = 107,
	TFA98XX_ERROR_RPC_CALIB_BUSY = 108,
	TFA98XX_ERROR_RPC_CALIB_FAILED = 109,
	TFA98XX_ERROR_NOT_IMPLEMENTED,
	TFA98XX_ERROR_NOT_SUPPORTED,
	TFA98XX_ERROR_I2C_FATAL,	/* Fatal I2C error occurred */
	/* Nonfatal I2C error, and retry count reached */
	TFA98XX_ERROR_I2C_NON_FATAL,
	TFA98XX_ERROR_OTHER = 1000
};

/*
 * Type containing all the possible msg returns DSP can give
 */
enum tfa98xx_status_id {
	TFA98XX_DSP_NOT_RUNNING = -1,
	/* No response from DSP */
	TFA98XX_I2C_REQ_DONE = 0,
	/* Request executed correctly and result,
	 * if any, is available for download
	 */
	TFA98XX_I2C_REQ_BUSY = 1,
	/* Request is being processed, just wait for result */
	TFA98XX_I2C_REQ_INVALID_M_ID = 2,
	/* Provided M-ID does not fit in valid rang [0..2] */
	TFA98XX_I2C_REQ_INVALID_P_ID = 3,
	/* Provided P-ID isn�t valid in the given M-ID context */
	TFA98XX_I2C_REQ_INVALID_CC = 4,
	/* Invalid channel configuration bits (SC|DS|DP|DC) combination */
	TFA98XX_I2C_REQ_INVALID_SEQ = 5,
	/* Invalid sequence of commands,
	 * in case the DSP expects some commands in a specific order
	 */
	TFA98XX_I2C_REQ_INVALID_PARAM = 6,
	/* Generic error */
	TFA98XX_I2C_REQ_BUFFER_OVERFLOW = 7,
	/* I2C buffer has overflowed:
	 * host has sent too many parameters,
	 * memory integrity is not guaranteed
	 */
	TFA98XX_I2C_REQ_CALIB_BUSY = 8,
	/* Calibration not finished */
	TFA98XX_I2C_REQ_CALIB_FAILED = 9
	/* Calibration failed */
};

/*
 * speaker as microphone
 */
enum tfa98xx_saam {
	TFA98XX_SAAM_NONE,	/*< SAAM feature not available */
	TFA98XX_SAAM			/*< SAAM feature available */
};

/*
 * possible Digital Audio Interfaces bitmap
 */
enum tfa98xx_dai_bitmap {
	TFA98XX_DAI_I2S  =  0x01,
	TFA98XX_DAI_TDM  =  0x02,
	TFA98XX_DAI_PDM  =  0x04,
};

/*
 * config file subtypes
 */
enum tfa98xx_config_type {
	TFA98XX_CONFIG_GENERIC,
	TFA98XX_CONFIG_SUB1,
	TFA98XX_CONFIG_SUB2,
	TFA98XX_CONFIG_SUB3,
};

enum tfa98xx_amp_input_sel {
	TFA98XX_AMP_INPUT_SEL_I2SLEFT,
	TFA98XX_AMP_INPUT_SEL_I2SRIGHT,
	TFA98XX_AMP_INPUT_SEL_DSP
};

enum tfa98xx_output_sel {
	TFA98XX_I2S_OUTPUT_SEL_CURRENT_SENSE,
	TFA98XX_I2S_OUTPUT_SEL_DSP_GAIN,
	TFA98XX_I2S_OUTPUT_SEL_DSP_AEC,
	TFA98XX_I2S_OUTPUT_SEL_AMP,
	TFA98XX_I2S_OUTPUT_SEL_DATA3R,
	TFA98XX_I2S_OUTPUT_SEL_DATA3L,
	TFA98XX_I2S_OUTPUT_SEL_DCDC_FFWD_CUR,
};

enum tfa98xx_stereo_gain_sel {
	TFA98XX_STEREO_GAIN_SEL_LEFT,
	TFA98XX_STEREO_GAIN_SEL_RIGHT
};

#define TFA98XX_MAXPATCH_LENGTH (3*1024)

/* the number of biquads supported */
#define TFA98XX_BIQUAD_NUM              10

enum tfa98xx_channel {
	TFA98XX_CHANNEL_L,
	TFA98XX_CHANNEL_R,
	TFA98XX_CHANNEL_L_R,
	TFA98XX_CHANNEL_STEREO
};

enum tfa98xx_mode {
	TFA98XX_MODE_NORMAL = 0,
	TFA98XX_MODE_RCV
};

enum tfa98xx_mute {
	TFA98XX_MUTE_OFF,
	TFA98XX_MUTE_DIGITAL,
	TFA98XX_MUTE_AMPLIFIER
};

enum tfa98xx_speaker_boost_status_flags {
	TFA98XX_SPEAKER_BOOST_ACTIVITY = 0, /* Input signal activity. */
	TFA98XX_SPEAKER_BOOST_S_CTRL,       /* S Control triggers the limiter */
	TFA98XX_SPEAKER_BOOST_MUTED,        /* 1 when signal is muted */
	TFA98XX_SPEAKER_BOOST_X_CTRL,       /* X Control triggers the limiter */
	TFA98XX_SPEAKER_BOOST_T_CTRL,       /* T Control triggers the limiter */
	TFA98XX_SPEAKER_BOOST_NEW_MODEL,    /* New model is available */
	TFA98XX_SPEAKER_BOOST_VOLUME_RDY,   /* 0:stable vol, 1:smoothing */
	TFA98XX_SPEAKER_BOOST_DAMAGED,      /* Speaker Damage detected  */
	TFA98XX_SPEAKER_BOOST_SIGNAL_CLIPPING /* input clipping detected */
};

struct tfa98xx_drc_state_info {
	float grhighdrc1[2];
	float grhighdrc2[2];
	float grmiddrc1[2];
	float grmiddrc2[2];
	float grlowdrc1[2];
	float grlowdrc2[2];
	float grpostdrc1[2];
	float grpostdrc2[2];
	float grbldrc[2];
};
struct tfa98xx_state_info {
	/* SpeakerBoost State */
	float agc_gain;	/* Current AGC Gain value */
	float lim_gain;	/* Current Limiter Gain value */
	float s_max;	/* Current Clip/Lim threshold */
	int T;		/* Current Speaker Temperature value */
	int status_flag;	/* Masked bit word */
	float X1;	/* estimated excursion caused by Spkrboost gain ctrl */
	float X2;	/* estimated excursion caused by manual gain setting */
	float Re;	/* Loudspeaker blocked resistance */
	/* Framework state */
	/* increments each time a MIPS problem is detected on the DSP */
	int short_on_mips;
	/* DRC state, when enabled */
	struct tfa98xx_drc_state_info drc_state;
};

struct tfa_msg {
	uint8_t msg_size;
	unsigned char cmd_id[3];
	int data[9];
};

struct nxp_vstep_msg {
	int			fw_version;
	uint8_t		no_of_vsteps;
	uint16_t	reg_no;
	uint8_t		*msg_reg;
	uint8_t		msg_no;
	uint32_t	algo_param_length;
	uint8_t		*msg_algo_param;
	uint32_t	filter_coef_length;
	uint8_t		*msg_filter_coef;
	uint32_t	mbdrc_length;
	uint8_t		*msg_mbdrc;
};

struct tfa_group {
	uint8_t msg_size;
	uint8_t profile_id[64];
};


struct tfa98xx_memtrack_data {
	int length;
	float m_values[MEMTRACK_MAX_WORDS];
	int m_addresses[MEMTRACK_MAX_WORDS];
	int trackers[MEMTRACK_MAX_WORDS];
	int scaling_factor[MEMTRACK_MAX_WORDS];
};

/* possible memory values for DMEM in CF_CONTROLs */
enum tfa98xx_dmem {
	TFA98XX_DMEM_ERR = -1,
	TFA98XX_DMEM_PMEM = 0,
	TFA98XX_DMEM_XMEM = 1,
	TFA98XX_DMEM_YMEM = 2,
	TFA98XX_DMEM_IOMEM = 3,
};

/**
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type);

/**
 *  register definition structure
 */
struct regdef {
	unsigned char offset; /**< subaddress offset */
	unsigned short pwron_default;
			      /**< register contents after poweron */
	unsigned short pwron_testmask;
			      /**< mask of bits not test */
	char *name;	      /**< short register name */
};

#define tfa98xx_handle_t int

/**
 * Open the instance handle
 */
enum tfa98xx_error tfa98xx_open(tfa98xx_handle_t handle);

/**
 * Load the default HW settings in the device
 */
enum tfa98xx_error tfa98xx_init(tfa98xx_handle_t handle);

/**
 * Return the tfa revision
 */
void tfa98xx_rev(int *major, int *minor, int *revision);

enum tfa98xx_dmem tfa98xx_filter_mem(tfa98xx_handle_t dev,
			int filter_index, unsigned short *address, int channel);

/**
 * Return the maximum nr of devices
 */
int tfa98xx_max_devices(void);

/**
 * If needed, this function can be used to get a text version
 * of the status ID code
 * @param the given status ID code
 * @return the I2C status ID string
 */
const char *tfa98xx_get_i2c_status_id_string(int status);

/**
 * Close the instance handle
 */
enum tfa98xx_error tfa98xx_close(tfa98xx_handle_t handle);

/* control the powerdown bit
 * @param powerdown must be 1 or 0
 */
enum tfa98xx_error tfa98xx_powerdown(tfa98xx_handle_t handle,
				  int powerdown);

/* indicates on which channel of DATAI2 the gain from the IC is set
 * @param gain_sel, see Tfa98xx_StereoGainSel_t
 */
enum tfa98xx_error tfa98xx_select_stereo_gain_channel(tfa98xx_handle_t handle,
						enum tfa98xx_stereo_gain_sel
						gain_sel);

/**
 * set the mtp with user controllable values
 * @param value to be written
 * @param mask to be applied toi the bits affected
 */
enum tfa98xx_error tfa98xx_set_mtp
(tfa98xx_handle_t handle, uint16_t value, uint16_t mask);
enum tfa98xx_error tfa98xx_get_mtp
(tfa98xx_handle_t handle, uint16_t *value);

/**
 * lock or unlock KEY2
 * lock = 1 will lock
 * lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(tfa98xx_handle_t handle, int lock);

enum tfa98xx_error tfa_run_calibrate
(tfa98xx_handle_t handle, uint16_t *value);
enum tfa98xx_error tfa_read_calibrate
(tfa98xx_handle_t handle, uint16_t *value);
enum tfa98xx_error tfa_read_cal_temp
(tfa98xx_handle_t handle, uint16_t *value);
enum tfa98xx_error tfa_calibrate(tfa98xx_handle_t handle);
enum tfa98xx_error tfa_tfadsp_calibrate(tfa98xx_handle_t handle);
enum tfa98xx_error tfa_calibration_range_check
(tfa98xx_handle_t handle, unsigned int channel, int mohm);
void tfa98xx_set_exttemp(tfa98xx_handle_t handle, short ext_temp);
short tfa98xx_get_exttemp(tfa98xx_handle_t handle);

/* control the volume of the DSP
 * @param vol volume in bit field. It must be between 0 and 255
 */
enum tfa98xx_error tfa98xx_set_volume_level(tfa98xx_handle_t handle,
				  unsigned short vol);

/* set the input channel to use
 * @param channel see TFA98XX_CHANNEL_t enumeration
 */
enum tfa98xx_error tfa98xx_select_channel(tfa98xx_handle_t handle,
				      enum tfa98xx_channel channel);

/* set the mode for normal or receiver mode
 * @param mode see Tfa98xx_Mode enumeration
 */
enum tfa98xx_error tfa98xx_select_mode
(tfa98xx_handle_t handle, enum tfa98xx_mode mode);

/* mute/unmute the audio
 * @param mute see Tfa98xx_Mute_t enumeration
 */
enum tfa98xx_error tfa98xx_set_mute(tfa98xx_handle_t handle,
				enum tfa98xx_mute mute);

/*
 * tfa98xx_supported_speakers - required for SmartStudio initialization
 *  returns the number of the supported speaker count
 */
enum tfa98xx_error tfa98xx_supported_speakers
(tfa98xx_handle_t handle, int *spkr_count);

/*
 * Return the feature bits from MTP and cnt file for comparison
 */
enum tfa98xx_error tfa98xx_compare_features
(tfa98xx_handle_t handle, int features_from_MTP[3], int features_from_cnt[3]);

/*
 * return feature bits
 */
enum tfa98xx_error
tfa98xx_dsp_get_sw_feature_bits(tfa98xx_handle_t handle, int features[2]);
enum tfa98xx_error
tfa98xx_dsp_get_hw_feature_bits(tfa98xx_handle_t handle, int *features);
/*
 * tfa98xx_supported_dai
 *  returns the bitmap of the supported Digital Audio Interfaces
 * @param dai bitmap enum pointer
 *  @return error code
 */
enum tfa98xx_error tfa98xx_supported_dai
(tfa98xx_handle_t handle, enum tfa98xx_dai_bitmap *daimap);

/*
 * tfa98xx_supported_saam
 *  returns the speaker as microphone feature
 * @param saam enum pointer
 *  @return error code
 */
enum tfa98xx_error tfa98xx_supported_saam
(tfa98xx_handle_t handle, enum tfa98xx_saam *saam);

/*
 * tfa98xx_set_saam_use_case
 *  sets use case of saam: 0: not in use, 1: RaM/SaM only, 2: bidirectional
 */
enum tfa98xx_error tfa98xx_set_saam_use_case(int samstream);

/*
 * tfa98xx_set_stream_state
 *  sets the stream: b0: pstream (Rx), b1: cstream (Tx), b2: samstream (SaaM)
 */
enum tfa98xx_error tfa98xx_set_stream_state(int stream_state);

/* load the tables to the DSP
 *   called after patch load is done
 *   @return error code
 */
enum tfa98xx_error tfa98xx_dsp_write_tables
(tfa98xx_handle_t handle, int sample_rate);

/* Set the boost trip level
 *   called every startup, profile switch or volume switch
 *   @return error code
 */
enum tfa98xx_error tfa98xx_set_boost_trip_level(void);


/* set or clear DSP reset signal
 * @param new state
 * @return error code
 */
enum tfa98xx_error tfa98xx_dsp_reset(tfa98xx_handle_t handle, int state);

/* check the state of the DSP subsystem
 * return ready = 1 when clocks are stable to allow safe DSP subsystem access
 * @param pointer to state flag, non-zero if clocks are not stable
 * @return error code
 */
enum tfa98xx_error tfa98xx_dsp_system_stable
(tfa98xx_handle_t handle, int *ready);

/**
 * check the state of the DSP coolflux
 * returns the value of CFE
 */
int tfa98xx_cf_enabled(tfa98xx_handle_t dev_idx);

/* The following functions can only be called when the DSP is running
 * - I2S clock must be active,
 * - IC must be in operating mode
 */

/**
 * patch the ROM code of the DSP
 * @param handle to opened instance
 * @param patch_length the number of bytes of patch_bytes
 * @param patch_bytes pointer to the bytes to patch
 */
enum tfa98xx_error tfa_dsp_patch(tfa98xx_handle_t handle,
				 int patch_length,
				 const unsigned char *patch_bytes);

/**
 * return the tfa device family id
 */
int tfa98xx_dev_family(tfa98xx_handle_t dev_idx);

/**
 * return the device revision id
 */
unsigned short tfa98xx_dev_revision(tfa98xx_handle_t dev_idx);

/**
 * load explicitly the speaker parameters in case of free speaker,
 * or when using a saved speaker model
 */
enum tfa98xx_error tfa98xx_dsp_write_speaker_parameters(
				tfa98xx_handle_t handle,
				int length,
				const unsigned char *pSpeakerBytes);

/**
 * read the speaker parameters as used by the SpeakerBoost processing
 */
enum tfa98xx_error tfa98xx_dsp_read_speaker_parameters(
				tfa98xx_handle_t handle,
				int length,
				unsigned char *pSpeakerBytes);

/**
 * read the current status of the DSP, typically used for development,
 * not essential to be used in a product
 */
enum tfa98xx_error tfa98xx_dsp_get_state_info(
				tfa98xx_handle_t handle,
				unsigned char bytes[],
				unsigned int *statesize);

/**
 * Check whether the DSP supports DRC
 * pb_support_drc=1 when DSP supports DRC,
 * pb_support_drc=0 when DSP doesn't support it
 */
enum tfa98xx_error tfa98xx_dsp_support_drc(tfa98xx_handle_t handle,
				      int *pb_support_drc);

enum tfa98xx_error
tfa98xx_dsp_support_framework
(tfa98xx_handle_t handle, int *pb_support_framework);

/**
 * read the speaker excursion model as used by SpeakerBoost processing
 */
enum tfa98xx_error tfa98xx_dsp_read_excursion_model(
				tfa98xx_handle_t handle,
				int length,
				unsigned char *pSpeakerBytes);

/**
 * load all the parameters for a preset from a file
 */
enum tfa98xx_error tfa98xx_dsp_write_preset(tfa98xx_handle_t handle,
				       int length, const unsigned char
				       *pPresetBytes);

/**
 * wrapper for dsp_msg that adds opcode and only writes
 */
enum tfa98xx_error tfa_dsp_cmd_id_write(tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   const unsigned char data[]);

/**
 * wrapper for dsp_msg that writes opcode and reads back the data
 */
enum tfa98xx_error tfa_dsp_cmd_id_write_read(tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for coefs
 */
enum tfa98xx_error tfa_dsp_cmd_id_coefs(tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics
 */
enum tfa98xx_error tfa_dsp_cmd_id_mbdrc_dynamics(tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int index_subband,
			   int num_bytes, unsigned char data[]);

/**
 * Disable a certain biquad.
 * @param handle to opened instance
 * @param biquad_index: 1-10 of the biquad that needs to be adressed
 */
enum tfa98xx_error tfa98xx_dsp_biquad_disable(tfa98xx_handle_t handle,
			   int biquad_index);

/**
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum tfa98xx_error
tfa_dsp_get_calibration_impedance(tfa98xx_handle_t handle);
enum tfa98xx_error
tfa_tfadsp_get_calibration_impedance(tfa98xx_handle_t handle);

/*
 * return the mohm value
 */
int tfa_get_calibration_info(tfa98xx_handle_t handle, int channel);

/*
 * return sign extended tap pattern
 */
int tfa_get_tap_pattern(tfa98xx_handle_t handle);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param p_value pointer to read data
 */
enum tfa98xx_error tfa98xx_read_register16(tfa98xx_handle_t handle,
				       unsigned char subaddress,
				       unsigned short *p_value);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param value value to write int the memory
 */
enum tfa98xx_error tfa98xx_write_register16(tfa98xx_handle_t handle,
					unsigned char subaddress,
					unsigned short value);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param start_offset offset from where to start reading
 * @param num_words number of words to read
 * @param p_values pointer to read data
 */
enum tfa98xx_error tfa98xx_dsp_read_mem(tfa98xx_handle_t handle,
				   unsigned int start_offset,
				   int num_words, int *p_values);
/**
 * Write a value to dsp memory
 * @param handle to opened instance
 * @param address write address to set in address register
 * @param value value to write int the memory
 * @param memtype type of memory to write to
 */
enum tfa98xx_error tfa98xx_dsp_write_mem_word
(tfa98xx_handle_t handle, unsigned short address, int value, int memtype);

/**
 * Read data from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param num_bytes number of bytes to read from dsp
 * @param data the unsigned char buffer to read data into
 */
enum tfa98xx_error tfa98xx_read_data(tfa98xx_handle_t handle,
				 unsigned char subaddress,
				 int num_bytes, unsigned char data[]);

/**
 * Write all the bytes specified by num_bytes and data to dsp memory
 * @param handle to opened instance
 * @param subaddress the subaddress to write to
 * @param num_bytes number of bytes to write
 * @param data actual data to write
 */
enum tfa98xx_error tfa98xx_write_data(tfa98xx_handle_t handle,
				  unsigned char subaddress,
				  int num_bytes,
				  const unsigned char data[]);

enum tfa98xx_error tfa98xx_write_raw(tfa98xx_handle_t handle,
				     int num_bytes,
				     const unsigned char data[]);

/* support for converting error codes into text */
const char *tfa98xx_get_error_string(enum tfa98xx_error error);

/**
 * convert signed 24 bit integers to 32bit aligned bytes
 * input:   data contains "num_bytes/3" int24 elements
 * output:  bytes contains "num_bytes" byte elements
 * @param num_data length of the input data array
 * @param data input data as integer array
 * @param bytes output data as unsigned char array
*/
void tfa98xx_convert_data2bytes(int num_data, const int data[],
			       unsigned char bytes[]);

/* return the device revision id
 */
unsigned short tfa98xx_get_device_revision(tfa98xx_handle_t handle);

/**
 * return the device digital audio interface (DAI) type bitmap
 */
enum tfa98xx_dai_bitmap tfa98xx_get_device_dai(tfa98xx_handle_t handle);

/**
 * convert memory bytes to signed 24 bit integers
 * input:  bytes contains "num_bytes" byte elements
 * output: data contains "num_bytes/3" int24 elements
 * @param num_bytes length of the input data array
 * @param bytes input data as unsigned char array
 * @param data output data as integer array
 */
void tfa98xx_convert_bytes2data(int num_bytes, const unsigned char bytes[],
			       int data[]);

/**
 * Read a part of the dsp memory
 * @param handle to opened instance
 * @param memory_type indicator to the memory type
 * @param offset from where to start reading
 * @param length the number of bytes to read
 * @param bytes output data as unsigned char array
 */
enum tfa98xx_error tfa98xx_dsp_get_memory
(tfa98xx_handle_t handle, int memory_type, int offset,
	int length, unsigned char bytes[]);

/**
 * Write a value to the dsp memory
 * @param handle to opened instance
 * @param memory_type indicator to the memory type
 * @param offset from where to start writing
 * @param length the number of bytes to write
 * @param value the value to write to the dsp
 */
enum tfa98xx_error tfa98xx_dsp_set_memory
(tfa98xx_handle_t handle, int memory_type, int offset, int length, int value);

enum tfa98xx_error tfa98xx_dsp_write_config
(tfa98xx_handle_t handle, int length, const unsigned char *p_config_bytes);
enum tfa98xx_error tfa98xx_dsp_write_drc
(tfa98xx_handle_t handle, int length, const unsigned char *p_drc_bytes);

/**
 * write/read raw msg functions :
 * the buffer is provided in little endian format, each word occupying
 * 3 bytes, length is in bytes.
 * functions will return immediately and do not not wait for DSP response.
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 */
enum tfa98xx_error tfa_dsp_msg
(tfa98xx_handle_t handle, int length, const char *buf);

/**
 * The wrapper functions to call the dsp msg, register and memory function
 * for tfa or probus
 */
enum tfa98xx_error dsp_msg
(tfa98xx_handle_t handle, int length, const char *buf);
#if defined(TFADSP_DSP_MSG_APR_PACKET_STRATEGY)
enum tfa98xx_error dsp_msg_packet
(tfa98xx_handle_t handle, uint8_t *blob, int tfadsp_buf_size);
#endif
enum tfa98xx_error dsp_msg_read
(tfa98xx_handle_t handle, int length, unsigned char *bytes);
enum tfa98xx_error reg_write
(tfa98xx_handle_t handle, unsigned char subaddress, unsigned short value);
enum tfa98xx_error reg_read
(tfa98xx_handle_t handle, unsigned char subaddress, unsigned short *value);
enum tfa98xx_error mem_write
(tfa98xx_handle_t handle, unsigned short address, int value, int memtype);
enum tfa98xx_error mem_read
(tfa98xx_handle_t handle, unsigned int start_offset,
	int num_words, int *p_values);

enum tfa98xx_error dsp_partial_coefficients
(tfa98xx_handle_t dev_idx, uint8_t *prev, uint8_t *next);

/**
 * write/read raw msg functions:
 * the buffer is provided in little endian format, each word occupying
 * 3 bytes, length is in bytes.
 * functions will return immediately and do not not wait for DSP response.
 * An ID is added to modify the command-ID
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 * @param cmdid command identifier
 */
enum tfa98xx_error tfa_dsp_msg_id
(tfa98xx_handle_t handle, int length, const char *buf, uint8_t cmdid[3]);

/**
 * write raw dsp msg functions
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buffer character buffer to write
 */
enum tfa98xx_error tfa_dsp_msg_write
(tfa98xx_handle_t handle, int length, const char *buffer);

/**
 * write raw dsp msg functions
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 * @param cmdid command identifier
 */
enum tfa98xx_error tfa_dsp_msg_write_id
(tfa98xx_handle_t handle, int length, const char *buffer, uint8_t cmdid[3]);

/**
 * status function used by tfa_dsp_msg() to retrieve command/msg status:
 * return a <0 status of the DSP did not ACK.
 * @param handle to opened instance
 * @param p_rpc_status status for remote processor communication
 */
enum tfa98xx_error tfa_dsp_msg_status
(tfa98xx_handle_t handle, int *p_rpc_status);

/**
 * Read a message from dsp
 * @param length number of bytes of the message
 * @param bytes pointer to unsigned char buffer
 */
enum tfa98xx_error tfa_dsp_msg_read
(tfa98xx_handle_t handle, int length, unsigned char *bytes);


void create_dsp_buffer_msg(struct tfa_msg *msg, char *buffer, int *size);

int tfa_set_bf
(tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t value);
int tfa_set_bf_volatile
(tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t value);

/**
 * Get the value of a given bitfield
 * @param dev_idx this is the device index
 * @param bf the value indicating which bitfield
 */
int tfa_get_bf(tfa98xx_handle_t dev_idx, const uint16_t bf);

/**
 * Set the value of a given bitfield
 * @param bf the value indicating which bitfield
 * @param bf_value the value of the bitfield
 * @param p_reg_value a pointer to register where to write the bitfield value
 */
int tfa_set_bf_value
(const uint16_t bf, const uint16_t bf_value, uint16_t *p_reg_value);

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value);
int tfa_write_reg
(tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t reg_value);
int tfa_read_reg(tfa98xx_handle_t dev_idx, const uint16_t bf);

/* bitfield */
/**
 * get the datasheet or bitfield name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfa_cont_bf_name(uint16_t num, unsigned short rev);

/**
 * get the datasheet name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfa_cont_ds_name(uint16_t num, unsigned short rev);

/**
 * get the bitfield name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfa_cont_bit_name(uint16_t num, unsigned short rev);

/**
 * get the bitfield number corresponding to the bitfield name
 * @param name is the bitfield name for which to get the bitfield number
 * @param rev is the device type
 */
uint16_t tfa_cont_bf_enum(const char *name, unsigned short rev);

/**
 * get the bitfield number corresponding to the bitfield name,
 * checks for all devices
 * @param name is the bitfield name for which to get the bitfield number
 */
uint16_t tfa_cont_bf_enum_any(const char *name);

#define TFA_FAM(dev_idx, fieldname) \
	((tfa98xx_dev_family(dev_idx) == 1) ? \
	 TFA1_BF_##fieldname :  TFA2_BF_##fieldname)
#define TFA_FAM_FW(dev_idx, fwname) \
	((tfa98xx_dev_family(dev_idx) == 1) ? \
	 TFA1_FW_##fwname :  TFA2_FW_##fwname)

/* set/get bit fields to HW register*/
#define TFA_SET_BF(dev_idx, fieldname, value) \
	tfa_set_bf(dev_idx, TFA_FAM(dev_idx, fieldname), value)
#define TFA_SET_BF_VOLATILE(dev_idx, fieldname, value) \
	tfa_set_bf_volatile(dev_idx, TFA_FAM(dev_idx, fieldname), value)
#define TFA_GET_BF(dev_idx, fieldname) \
	tfa_get_bf(dev_idx, TFA_FAM(dev_idx, fieldname))

/* set/get bit field in variable */
#define TFA_SET_BF_VALUE(dev_idx, fieldname, bf_value, p_reg_value) \
	tfa_set_bf_value(TFA_FAM(dev_idx, fieldname), bf_value, p_reg_value)
#define TFA_GET_BF_VALUE(dev_idx, fieldname, reg_value) \
	tfa_get_bf_value(TFA_FAM(dev_idx, fieldname), reg_value)

/* write/read registers using a bit field name
 * to determine the register address
 */
#define TFA_WRITE_REG(dev_idx, fieldname, value) \
	tfa_write_reg(dev_idx, TFA_FAM(dev_idx, fieldname), value)
#define TFA_READ_REG(dev_idx, fieldname) \
	tfa_read_reg(dev_idx, TFA_FAM(dev_idx, fieldname))

/* FOR CALIBRATION RETRIES */
#define TFA98XX_API_WAITRESULT_NTRIES 3000 /* defined in API */

/**
 * run the startup/init sequence and set ACS bit
 * @param state the cold start state that is requested
 */
enum tfa98xx_error tfa_run_coldboot(tfa98xx_handle_t handle, int state);
enum tfa98xx_error tfa_run_mute(tfa98xx_handle_t handle);
enum tfa98xx_error tfa_run_unmute(tfa98xx_handle_t handle);

/**
 * wait for calibrate_done
 * @param calibrate_done pointer to status of calibration
 */
enum tfa98xx_error tfa_run_wait_calibration
(tfa98xx_handle_t handle, int *calibrate_done);

/**
 * run the startup/init sequence and set ACS bit
 * @param profile the profile that should be loaded
 */
enum tfa98xx_error tfa_run_coldstartup(tfa98xx_handle_t handle, int profile);

/**
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 */
enum tfa98xx_error tfa_run_start_dsp(tfa98xx_handle_t handle);

/**
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 * @param profile the profile that should be loaded on startup
 */
enum tfa98xx_error tfa_run_startup(tfa98xx_handle_t handle, int profile);

/**
 * start the maximus speakerboost algorithm
 * this implies a full system startup when the system was not already started
 * @param force indicates whether a full system startup should be allowed
 * @param profile the profile that should be loaded
 */
enum tfa98xx_error tfa_run_speaker_boost
(tfa98xx_handle_t handle, int force, int profile);

/**
 * Startup the device and write all files from device and profile section
 * @param force indicates whether a full system startup should be allowed
 * @param profile the profile that should be loaded on speaker startup
 */
enum tfa98xx_error tfa_run_speaker_startup
(tfa98xx_handle_t handle, int force, int profile);

/**
 * Run calibration
 * @param profile the profile that should be loaded
 */
enum tfa98xx_error tfa_run_speaker_calibration
(tfa98xx_handle_t handle, int profile);
enum tfa98xx_error tfa_set_calibration_values(tfa98xx_handle_t handle);

enum tfa98xx_error tfa_mtp_get_calibration
(tfa98xx_handle_t handle, uint16_t *cal_data);
enum tfa98xx_error tfa_mtp_set_calibration
(tfa98xx_handle_t handle, uint16_t cal_data);

/**
 * startup all devices. all step until patch loading is handled
 */
int tfa_run_startup_all(tfa98xx_handle_t *handles);

/**
 * powerup the coolflux subsystem and wait for it
 */
enum tfa98xx_error tfa_cf_powerup(tfa98xx_handle_t handle);

/*
 * print the current device manager state
 */
enum tfa98xx_error show_current_state(tfa98xx_handle_t handle);

/**
 * set verbosity level
 */
void tfa_verbose(int level);
/**
 * get verbosity level
 */
int tfa_get_verbose(void);
/**
 * set verbosity level for dsp communication
 */
void tfa_dsp_verbose(int level);

/**
 * Init registers and coldboot dsp
 */
int tfa98xx_reset(tfa98xx_handle_t handle);

/**
 * @param dev_idx is the device index
 * @param revid is the revision id
 * @param slave_address is the slave address
 */
void tfa_mock_probe(int dev_idx, unsigned short revid, int slave_address);

/**
 * @param dev_idx is the device index
 * @param revid is the revision id
 */
enum tfa98xx_error tfa_soft_probe(int dev_idx, int revid);

/**
 * Get profile from a register
 */
int tfa_get_swprof(tfa98xx_handle_t handle);

/**
 * Save profile in a register
 */
int tfa_set_swprof(tfa98xx_handle_t handle, unsigned short new_value);

int tfa_get_swvstep(tfa98xx_handle_t handle);

int tfa_set_swvstep(tfa98xx_handle_t handle, unsigned short new_value);

int tfa_is_cold(tfa98xx_handle_t handle);

int tfa_is_cold_amp(tfa98xx_handle_t handle);

int tfa_cf_enabled(tfa98xx_handle_t handle);

/* tfa2 interrupt support */
/*
 * interrupt bit function to clear
 */
int tfa_irq_clear(tfa98xx_handle_t handle, int bit);
/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(tfa98xx_handle_t handle, int bit);
/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(tfa98xx_handle_t handle, int bit, int state);
/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(tfa98xx_handle_t handle, int bit, int state);
/*
 * update dirty cached regs to i2c registers
 */
void tfa_irq_update(tfa98xx_handle_t handle);
/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(tfa98xx_handle_t handle);
/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(tfa98xx_handle_t handle);

#if defined(TFA_BLACKBOX_LOGGING)
typedef int (*configure_log_t)(void);
typedef int (*update_log_t)(void);
int tfa_log_register(configure_log_t tfa_log_configure,
	update_log_t tfa_log_update);
#endif

#ifdef __cplusplus
}
#endif
#endif				/* TFA_SERVICE_H */
