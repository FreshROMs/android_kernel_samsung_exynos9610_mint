#ifndef TFA98XXPARAMETERS_H_
#define TFA98XXPARAMETERS_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

struct tfa_msg {
	uint8_t msg_size;
	unsigned char cmd_id[3];
	int data[9];
};

/*
 * profiles & volumesteps
 *
 */
#define TFA_MAX_PROFILES		(64)
#define TFA_MAX_MSGS			(10)

/*
 * the pack pragma is required to make that the size in memory
 * matches the actual variable lengths
 * This is to assure that the binary files can be transported between
 * different platforms.
 */
#pragma pack(push, 1)

/*
 * typedef for 24 bit value using 3 bytes
 */
struct uint24 {
	uint8_t b[3];
};

/*
 * the generic header
 *   all char types are in ASCII
 */
struct tfa_header {
	uint16_t id;
	char version[2];     /* "V_": V=version, vv=subversion */
	char subversion[2];  /* "vv": vv=subversion */
	uint16_t size;	     /* data size in bytes following CRC */
	uint32_t crc;	     /* 32-bits CRC for following data */
	char customer[8];    /* "name of customer" */
	char application[8]; /* "application name" */
	char type[8];        /* "application type name" */
};

enum tfa_sample_rate {
	FS_8K,     /* 8kHz */
	FS_11K025, /* 11.025kHz */
	FS_12K,    /* 12kHz */
	FS_16K,    /* 16kHz */
	FS_22K05,  /* 22.05kHz */
	FS_24K,    /* 24kHz */
	FS_32K,    /* 32kHz */
	FS_44K1,   /* 44.1kHz */
	FS_48K,    /* 48kHz */
	FS_96K,    /* 96kHz */
	FS_COUNT   /* Should always be last item */
};

/* Keep in sync with enum tfa_sample_rate! */
static const int tfa_samplerate_hz[FS_COUNT]
	= {8000, 11025, 12000, 16000, 22050,
		24000, 32000, 44100, 48000, 96000};

/*
 * coolflux direct memory access
 */
struct tfa_dsp_mem {
	uint8_t  type;		/* 0--3: p, x, y, iomem */
	uint16_t address;	/* target address */
	uint8_t size;		/* data size in words */
	int words[];		/* payload in signed 32bit int (two's complement) */
};

/*
 * the biquad coefficients for the API together with index in filter
 *  the biquad_index is the actual index in the equalizer +1
 */
#define BIQUAD_COEFF_SIZE	6

/*
 * Output fixed point coeffs structure
 */
struct tfa_biquad {
	int a2;
	int a1;
	int b2;
	int b1;
	int b0;
};

struct tfa_biquad_old {
	uint8_t bytes[BIQUAD_COEFF_SIZE*sizeof(struct uint24)];
};

struct tfa_biquad_float {
	float headroom;
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
};

/*
 * EQ filter definitions
 * Note: This is not in line with smartstudio (JV: 12/12/2016)
 */
enum tfa_filter_type {
	f_custom,	/* User defined biquad coefficients */
	f_flat,		/* Vary only gain */
	f_lowpass,	/* 2nd order Butterworth low pass */
	f_highpass,	/* 2nd order Butterworth high pass */
	f_lowshelf,
	f_highshelf,
	f_notch,
	f_peak,
	f_bandpass,
	f_1st_lp,
	f_1st_hp,
	f_elliptic
};

/*
 * filter parameters for biquad (re-)calculation
 */
struct tfa_filter {
	struct tfa_biquad_old biquad;
	uint8_t enabled;
	uint8_t type; /* (== enum FilterTypes, assure 8bits length) */
	float frequency;
	float Q;
	float gain;
}; /* 8 * float + int32 + byte == 37 */

/*
 * biquad params for calculation
 */

#define TFA_BQ_EQ_INDEX 0
#define TFA_BQ_ANTI_ALIAS_INDEX 10
#define TFA_BQ_INTEGRATOR_INDEX 13

/*
 * Loudspeaker Compensation filter definitions
 */
struct tfa_ls_compensation_filter {
	struct tfa_biquad biquad;
	uint8_t ls_comp_on;  /* Loudspeaker compensation on/off */
	uint8_t bw_ext_on;   /* Bandwidth extension on/off */
	float f_res;	  /* [Hz] speaker resonance frequency */
	float Qt;		 /* speaker resonance Q-factor */
	float f_bw_ext;	  /* [Hz] Band width extension frequency */
	float sampling_freq; /* [Hz] Sampling frequency */
};

/*
 * Anti Aliasing Elliptic filter definitions
 */
struct tfa_anti_alias_filter {
	struct tfa_biquad biquad; /* Output results fixed point coeffs */
	uint8_t enabled;
	float cut_off_freq;  /* cut off frequency */
	float sampling_freq; /* sampling frequency */
	float ripple_db;     /* range: [0.1 3.0] */
	float rolloff;       /* range: [-1.0 1.0] */
};

/*
 * Integrator filter input definitions
 */
struct tfa_integrator_filter {
	struct tfa_biquad biquad; /* Output results fixed point coeffs */
	uint8_t type;	/* Butterworth filter type: high or low pass */
	float  cut_off_freq;	/* cut off frequency in Hz: [100.0 4000.0] */
	float  sampling_freq;	/* sampling frequency in Hz */
	float  leakage;	/* leakage factor; range [0.0 1.0] */
};

struct tfa_eq_filter {
	struct tfa_biquad biquad;
	uint8_t enabled;
	uint8_t type;	/* (== enum FilterTypes, assure 8bits length) */
	float cut_off_freq; /* cut off frequency, range: [100.0 4000.0] */
	float sampling_freq; /* sampling frequency */
	float Q;		/* range: [0.5 5.0] */
	float gain_db;	/* range: [-10.0 10.0] */
}; /* 8 * float + int32 + byte == 37 */

struct tfa_cont_anti_alias {
	int8_t index;	/* index - destination type; anti-alias,integrator,eq */
	uint8_t type;
	float cut_off_freq;	/* cut off frequency */
	float sampling_freq;
	float ripple_db;	/* integrator leakage */
	float rolloff;
	uint8_t bytes[5 * 3];	/* payload 5 * 24 bits coeffs */
};

struct tfa_cont_integrator {
	int8_t index;	/* index - destination type; anti-alias,integrator,eq */
	uint8_t type;
	float cut_off_freq;	/* cut off frequency */
	float sampling_freq;
	float leakage;	/* integrator leakage */
	float reserved;
	uint8_t bytes[5 * 3];	/* payload 5 * 24 bits coeffs */
};

struct tfa_cont_eq {
	int8_t index;
	uint8_t type;	/* (== enum FilterTypes, assure 8bits length) */
	float cut_off_freq;	/* cut off frequency, range: [100.0 4000.0] */
	float sampling_freq;	/* sampling frequency */
	float Q;	/* range: [0.5 5.0] */
	float gain_db;	/* range: [-10.0 10.0] */
	uint8_t bytes[5 * 3];	/* payload 5 * 24 bits coeffs */
}; /* 8 * float + int32 + byte == 37 */

union tfa_cont_biquad {
	struct tfa_cont_eq eq;
	struct tfa_cont_anti_alias aa;
	struct tfa_cont_integrator in;
};

#define TFA_BQ_EQ_INDEX 0
#define TFA_BQ_ANTI_ALIAS_INDEX 10
#define TFA_BQ_INTEGRATOR_INDEX 13

#define TFA98XX_MAX_EQ 10
struct tfa_equalizer {
	struct tfa_filter filter[TFA98XX_MAX_EQ];
	/* note: API index counts from 1..10 */
};

/*
 * files
 */
#define HDR(c1, c2) (c2<<8|c1) /* little endian */
enum tfa_header_type {
	params_hdr   = HDR('P', 'M'), /* containter file */
	volstep_hdr   = HDR('V', 'P'),
	patch_hdr     = HDR('P', 'A'),
	speaker_hdr   = HDR('S', 'P'),
	preset_hdr    = HDR('P', 'R'),
	config_hdr    = HDR('C', 'O'),
	equalizer_hdr	= HDR('E', 'Q'),
	drc_hdr	      = HDR('D', 'R'),
	msg_hdr	      = HDR('M', 'G'),	/* generic message */
	info_hdr      = HDR('I', 'N')
};

/*
 * equalizer file
 */
#define NXPTFA_EQ_VERSION	'1'
#define NXPTFA_EQ_SUBVERSION "00"
struct tfa_equalizer_file {
	struct tfa_header hdr;
	uint8_t samplerate; /* ==enum samplerates, assure 8 bits */
	struct tfa_filter filter[TFA98XX_MAX_EQ];
	/* note: API index counts from 1..10 */
};

/*
 * patch file
 */
#define NXPTFA_PA_VERSION	'1'
#define NXPTFA_PA_SUBVERSION "00"
struct tfa_patch_file {
	struct tfa_header hdr;
	uint8_t data[];
};

/*
 * generic message file
 * the payload of this file includes opcode and is send straight to the DSP
 */
#define NXPTFA_MG_VERSION	'3'
#define NXPTFA_MG_SUBVERSION "00"
struct tfa_msg_file {
	struct tfa_header hdr;
	uint8_t data[];
};

/*
 * NOTE the tfa98xx API defines the enum tfa9xxx_config_type that defines
 * the subtypes as decribes below.
 * tfa98xx_dsp_config_parameter_type() can be used to get the
 * supported type for the active device..
 */
/*
 * config file V1 sub 1
 */
#define NXPTFA_CO_VERSION	'1'
#define NXPTFA_CO3_VERSION   '3'
#define NXPTFA_CO_SUBVERSION1 "01"
struct tfa_config_s1_file {
	struct tfa_header hdr;
	uint8_t data[55 * 3];
};

/*
 * config file V1 sub 2
 */
#define NXPTFA_CO_SUBVERSION2 "02"
struct tfa_config_s2_file {
	struct tfa_header hdr;
	uint8_t data[67 * 3];
};

/*
 * config file V1 sub 3
 */
#define NXPTFA_CO_SUBVERSION3 "03"
struct tfa_config_s3_file {
	struct tfa_header hdr;
	uint8_t data[67 * 3];
};

/*
 * config file V1.0
 */
#define NXPTFA_CO_SUBVERSION "00"
struct tfa_config_file {
	struct tfa_header hdr;
	uint8_t data[];
};

/*
 * preset file
 */
#define NXPTFA_PR_VERSION	'1'
#define NXPTFA_PR_SUBVERSION "00"
struct tfa_preset_file {
	struct tfa_header hdr;
	uint8_t data[];
};

/*
 * drc file
 */
#define NXPTFA_DR_VERSION	'1'
#define NXPTFA_DR_SUBVERSION "00"
struct tfa_drc_file {
	struct tfa_header hdr;
	uint8_t data[];
};

/*
 * drc file
 * for tfa 2 there is also a xml-version
 */
#define NXPTFA_DR3_VERSION	'3'
#define NXPTFA_DR3_SUBVERSION "00"
struct tfa_drc_file2 {
	struct tfa_header hdr;
	uint8_t version[3];
	uint8_t data[];
};

/*
 * speaker file header
 */
struct tfa_spk_header {
	struct tfa_header hdr;
	char name[8]; /* speaker nick name (e.g. ?œdumbo?? */
	char vendor[16];
	char type[8];
	/* dimensions (mm) */
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint16_t ohm;
};

/*
 * speaker file
 */
#define NXPTFA_SP_VERSION	'1'
#define NXPTFA_SP_SUBVERSION "00"
struct tfa_speaker_file {
	struct tfa_header hdr;
	char name[8];	/* speaker nick name (e.g. ?œdumbo?? */
	char vendor[16];
	char type[8];
	/* dimensions (mm) */
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	uint8_t data[]; /* payload TFA98XX_SPEAKERPARAMETER_LENGTH */
};

struct tfa_fw_ver {
	uint8_t major;
	uint8_t minor;
	uint8_t minor_update:6;
	uint8_t update:2;
};

struct tfa_fw_msg {
	struct tfa_fw_ver fw_version;
	struct tfa_msg payload;
};

struct tfa_livedata {
	char name[25];
	char addrs[25];
	int tracker;
	int scalefactor;
};

#define NXPTFA_SP3_VERSION  '3'
#define NXPTFA_SP3_SUBVERSION "00"
struct tfa_speaker_file_max2  {
	struct tfa_header hdr;
	char name[8];	/* speaker nick name (e.g. ?œdumbo?? */
	char vendor[16];
	char type[8];
	/* dimensions (mm) */
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	struct tfa_fw_msg fw_msg; /* payload including FW ver and Cmd ID */
};

/*
 * parameter container file
 */
/*
 * descriptors
 * Note 1: append new DescriptorType at the end
 * Note 2: add new descriptors to dsc_name[] in tfaContUtil.c
 */
enum tfa_descriptor_type {
	dsc_device,		/* device list */
	dsc_profile,	/* profile list */
	dsc_register,	/* register patch */
	dsc_string,		/* ascii, zero terminated string */
	dsc_file,		/* filename + file contents */
	dsc_patch,		/* patch file */
	dsc_marker,		/* marker to indicate end of a list */
	dsc_mode,
	dsc_set_input_select,
	dsc_set_output_select,
	dsc_set_program_config,
	dsc_set_lag_w,
	dsc_set_gains,
	dsc_set_vbat_factors,
	dsc_set_senses_cal,
	dsc_set_senses_delay,
	dsc_bit_field,
	dsc_default,  /* used to reset bitfields to there default values */
	dsc_livedata,
	dsc_livedata_string,
	dsc_group,
	dsc_cmd,
	dsc_set_mb_drc,
	dsc_filter,
	dsc_no_init,
	dsc_features,
	dsc_cf_mem, /* coolflux memory x,y,io */
	dsc_set_fwk_use_case,
	dsc_set_vddp_config,
	dsc_tfa_hal, 
	dsc_last,	/* trailer */
	dsc_listend = -1
};

#define TFA_BITFIELDDSCMSK 0x7fffffff

struct tfa_desc_ptr {
	uint32_t offset:24;
	uint32_t  type:8; /* (== enum tfa_desc_type, assure 8bits length) */
};

/*
 * generic file descriptor
 */
struct tfa_file_dsc {
	struct tfa_desc_ptr name;
	uint32_t size;	/* file data length in bytes */
	uint8_t data[]; /* payload */
};

/*
 * device descriptor list
 */
struct tfa_device_list {
	uint8_t length;			/* nr of items in the list */
	uint8_t bus;			/* bus */
	uint8_t dev;			/* device */
	uint8_t func;			/* subfunction or subdevice */
	uint32_t devid;			/* device hw fw id */
	struct tfa_desc_ptr name;		/* device name */
	struct tfa_desc_ptr list[];		/* items list */
};

/*
 * profile descriptor list
 */
struct tfa_profile_list {
	uint32_t length:8;	/* nr of items in the list + name */
	uint32_t group:8;	/* profile group number */
	uint32_t id:16;	/* profile ID */
	struct tfa_desc_ptr name;	/* profile name */
	struct tfa_desc_ptr list[];	/* items list (length-1 items) */
};
#define TFA_PROFID 0x1234

/*
 * livedata descriptor list
 */
struct tfa_livedata_list {
	uint32_t length:8;		/* nr of items in the list */
	uint32_t id:24;			/* profile ID */
	struct tfa_desc_ptr name;		/* livedata name */
	struct tfa_desc_ptr list[];		/* items list */
};
#define TFA_LIVEDATAID 0x5678

/*
 * Bitfield descriptor
 */
struct tfa_bitfield {
	uint16_t  value;
	uint16_t  field; /* ==datasheet defined, 16 bits */
};

/*
 * Bitfield enumuration bits descriptor
 */
struct tfa_bf_enum {
	unsigned int  len:4;		/* this is the actual length-1 */
	unsigned int  pos:4;
	unsigned int  address:8;
};

/*
 * Register patch descriptor
 */
struct tfa_reg_patch {
	uint8_t   address;	/* register address */
	uint16_t  value;	/* value to write */
	uint16_t  mask;		/* mask of bits to write */
};

/*
 * Mode descriptor
 */
struct tfa_mode {
	int value;	/* mode value, maps to enum tfa9xxx_mode */
};

/*
 * NoInit descriptor
 */
struct tfa_no_init {
	uint8_t value;	/* noInit value */
};

/*
 * Features descriptor
 */
struct tfa_features {
	uint16_t value[3];	/* features value */
};

/*
 * the container file
 *   - the size field is 32bits long (generic=16)
 *   - all char types are in ASCII
 */
#define NXPTFA_PM_VERSION  '1'
#define NXPTFA_PM3_VERSION '3'
#define NXPTFA_PM_SUBVERSION '1'
struct tfa_container {
	char id[2];	/* "XX": XX=type */
	char version[2];	/* "V_": V=version, vv=subversion */
	char subversion[2];	/* "vv": vv=subversion */
	uint32_t size;	/* data size in bytes following CRC */
	uint32_t crc;	/* 32-bits CRC for following data */
	uint16_t rev;	/* "extra chars for rev nr" */
	char customer[8];	/* "name of customer" */
	char application[8];	/* "application name" */
	char type[8];	/* "application type name" */
	uint16_t ndev;	/* "nr of device lists" */
	uint16_t nprof;	/* "nr of profile lists" */
	uint16_t nlivedata;	/* "nr of livedata lists" */
	struct tfa_desc_ptr index[];	/* start of item index table */
};

#pragma pack(pop)
#endif /* TFA98XXPARAMETERS_H_ */
