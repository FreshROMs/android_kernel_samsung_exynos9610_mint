#ifndef FM_LOW_STRUC_H
#define FM_LOW_STRUC_H

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/io.h>

#define S620_REV_0	(0x100000)

#define USE_SPUR_CANCEL
#undef USE_SPUR_CANCEL

/* S612 Rev1 SPUR TRF Default define */
#define USE_SPUR_CANCEL_TRF
/*#undef USE_SPUR_CANCEL_TRF*/

/* S612 Rev1 Dual clocking Default define */
#define USE_S612_DUAL_CLOCKING
/*#undef USE_S612_DUAL_CLOCKING*/

#define USE_FILTER_SELECT_BY_FREQ
#define MAX_FILTER_FREQ_NUM	6

#define USE_NEW_SCAN
#define	USE_RDS_HW_DECODER
#undef	USE_RDS_HW_DECODER

#define USE_RINGBUFF_API
/*#undef USE_RINGBUFF_API*/

#define FM_LOW_DRV_DELAY_MS  1
#define AGGR_RSSI_OFFSET (-114)
#define RDS_VALID_THRESHOLD (140) /* -104dB */

#undef  TRUE
#define TRUE	(1)

#undef  FALSE
#define FALSE	(0)

typedef u32 TIME;

/* TIME constants. */

#define	SECOND	HZ
#define	IDLE_TIME_MS	(300)
#define	RDS_POLL_DELAY_MS	(150)
#define	TUNE_TIME_FAST_MS	(30)
#define	TUNE_TIME_SLOW_MS	(60)
#ifdef USE_NEW_SCAN
#define	SEARCH_DELAY_MS	(20)
#else
#define	SEARCH_DELAY_MS	(15)
#endif

#define RSSI_REF_ENABLE 0x01
#define FM_RDS_MEM_SIZE_PARSER	1000
#define FM_RDS_MEM_SIZE	480
#define RDS_PARSER_ENABLE 0x04
#define FM_RADIO_RDS_PARSER_VER_CHECK 0x400

#ifdef USE_SPUR_CANCEL
#define EN_SPUR_REMOVAL        (0x0001)
#define DIS_SPUR_REMOVAL_MONO      (0x0002)
#endif

#define RSSI_ADJUST_WITHOUT_ELNA_VALUE (68)
#define SNR_OFF_RSSI_VALUE	(174)
#define SNR_ON_RSSI_VALUE	(171)
#define TRF_OFF_RSSI_VALUE	(202)
#define TRF_ON_RSSI_VALUE	(199)
#define SCAN_WEAK_SIG_THRESHOLD (-103)

/******************************************************************************
 *	DEFINITIONS AND MACROS
 ******************************************************************************/
/* == FM SPEEDY registers == */
#define FM_SPEEDY_MA_BASE	(0x14840000)
#define FMSPDY_CTL	(0x00000000)
#define FMSPDY_STAT	(0x00000004)
#define FMSPDY_DISR	(0x00000008)
#define FMSPDY_INTR_MASK	(0x0000000C)
#define FMSPDY_DATA	(0x00000010)
#define FMSPDY_CMD	(0x00000014)
#define FMSPDY_ERR_CNT	(0x00000018)
#define FMSPDY_MISC_STAT	(0x0000001C)
#define FMSPDY_PRAMS	(0x00000020)
#define FM_SLV_INT	(0x00000040)
#define AUDIO_CTRL	(0x00000024)
#define AUDIO_FIFO	(0x00000028)
#define FM_SPEEDY_MA_SIZE	1024

/* FMSPDY INT Mask bits */
#define FM_SLV_INT_MASK_BIT	(5)

/* FMSPDY Control Register Flags */
#define SPDY_WAKEUP        (1<<23)

/* FMSPDY Status Register Flags*/
#define RX_FMT_ERR         (1<<4)
#define RX_NO_STOP         (1<<3)
#define RX_GLITCHED        (1<<2)
#define RX_TIMEOUT         (1<<1)
#define RX_ALL_ERR         (RX_FMT_ERR|RX_NO_STOP|RX_GLITCHED|RX_TIMEOUT)
#define STAT_DONE          (1<<0)

/* FMSPDY Command Register Flags*/
#define FMSPDY_READ            (0<<17)
#define FMSPDY_WRITE           (1<<17)
#define FMSPDY_RANDOM          (1<<16)

#define write32(addr, data) __raw_writel(data, addr)
#define read32(addr) __raw_readl((volatile void __iomem *)addr)
#define SetBits(uAddr, uBaseBit, uMaskValue, uSetValue) \
		write32(uAddr, (read32(uAddr) & ~((uMaskValue)<<(uBaseBit))) | \
				(((uMaskValue)&(uSetValue))<<(uBaseBit)))
#define GetBits(uAddr, uBaseBit, uMaskValue) \
		((read32(uAddr)>>(uBaseBit))&(uMaskValue))


enum flags_enum {
	FLAG_TUNED		= (1 << 0),
	FLAG_BD_LMT		= (1 << 1),
	FLAG_SYN_LOS	= (1 << 2),
	FLAG_BUF_FUL	= (1 << 3),
	FLAG_AUD_PAU	= (1 << 4),
	FLAG_CH_STAT	= (1 << 5)
};

enum fm_status_mask_enum {
	STATUS_MASK_RDS_AVA	= (1 << 0),
	STATUS_MASK_STEREO	= (1 << 1)
};

enum fm_search_dir_mask_enum {
	SEARCH_DIR_MASK = (1 << 0)
};

enum fm_tuner_mode_mask_enum {
	TUNER_MODE_MASK_TUN_MOD	= (7 << 0),
	TUNER_MODE_MASK_NEXT	= (1 << 3)
};

enum fm_tuner_mode_enum {
	TUNER_MODE_NONE			= 0,
	TUNER_MODE_PRESET		= 1,
	TUNER_MODE_SEARCH		= 2
};

enum fm_mute_state_mask_enum {
	MUTE_STATE_MASK_SOFT = (1 << 0),
	MUTE_STATE_MASK_HARD = (1 << 1)
};

enum fm_output_mode_mask_enum {
	MODE_MASK_MONO_STEREO = (1 << 0)
};

enum fm_blend_mode_mask_enum {
	MODE_MASK_BLEND = (1 << 0)
};

enum fm_rds_ctrl_mask_enum {
	RDS_CTRL_MASK_FLUSH		= (1 << 0),
	RDS_CTRL_MASK_RESYNC	= (1 << 1)
};

enum fm_rds_system_mask_enum {
	RDS_SYSTEM_MASK_RDS		= (1 << 0),
	RDS_SYSTEM_MASK_EBLK	= (1 << 1)
};

enum fm_power_mask_enum {
	PWR_MASK_FM		= (1 << 0),
	PWR_MASK_RDS	= (1 << 1)
};

enum fm_deemph_mode_mask_enum {
	MODE_MASK_DEEMPH = (1 << 0)
};

typedef enum {
	HOST_RDS_ERRS_NONE		= 0,
	HOST_RDS_ERRS_2CORR		= 1,
	HOST_RDS_ERRS_5CORR		= 2,
	HOST_RDS_ERRS_UNCORR	= 3
} fm_host_rds_errors_enum;

#define RDS_MEM_MAX_THRESH	(48)
#define RDS_MEM_MAX_THRESH_PARSER	(100)

enum fm_host_rds_data_enum {
	HOST_RDS_DATA_BLKTYPE_POSI	= 0,
	HOST_RDS_DATA_ERRORS_POSI	= 3,
	HOST_RDS_DATA_AVAIL_MASK	= (1 << 5)
};

#define HOST_RDS_BLOCK_SIZE			3
#define HOST_RDS_BLOCK_FMT_LSB		0
#define HOST_RDS_BLOCK_FMT_MSB		1
#define HOST_RDS_BLOCK_FMT_STATUS	2

enum fm_demod_stat_mask_enum {
	FM_DEMOD_BLEND_STEREO_MASK = (0x0001 << 4),
	FM_DEMOD_IF_OOR_MASK = (0x0001 << 7)
};

enum fm_int_src_mask_enum {
	INT_IFC_READY_MASK = (0x0001 << 0),
	INT_AUDIO_PAU_MASK = (0x0001 << 3),
	INT_RDS_BYTES_MASK = (0x0001 << 4),
};

enum fm_audio_gain_enum {
	AUDIO_ATTENUATION_Max		= 0,
	AUDIO_ATTENUATION_42dB		= 1,
	AUDIO_ATTENUATION_39dB		= 2,
	AUDIO_ATTENUATION_36dB		= 3,
	AUDIO_ATTENUATION_33dB		= 4,
	AUDIO_ATTENUATION_30dB		= 5,
	AUDIO_ATTENUATION_27dB		= 6,
	AUDIO_ATTENUATION_24dB		= 7,
	AUDIO_ATTENUATION_21dB		= 8,
	AUDIO_ATTENUATION_18dB		= 9,
	AUDIO_ATTENUATION_15dB		= 10,
	AUDIO_ATTENUATION_12dB		= 11,
	AUDIO_ATTENUATION_9dB		= 12,
	AUDIO_ATTENUATION_6dB		= 13,
	AUDIO_ATTENUATION_3dB		= 14,
	AUDIO_ATTENUATION_0dB		= 15
};

/***************************************************************************/

typedef struct {
	u16 muffle_coeffs;
	u16 lpf_en;
	u16 lpf_auto;
	u16 lpf_bw;
} soft_muffle_conf;

typedef struct {
#ifdef USE_NEW_SCAN
	u16 weak_ifca_l;
	u16 weak_ifca_m;
	u16 weak_ifca_h;
	u16 normal_ifca_l;
	u16 normal_ifca_m;
	u16 normal_ifca_h;
	bool weak_sig;
#else
	u16 ifca_l;
	u16 ifca_m;
	u16 ifca_h;
#endif
} search_config;

#ifdef MONO_SWITCH_INTERF
typedef struct {
	s16 lo;
	s16 hi;
} interf_rssi_thres;

typedef struct {
	u16 lo;
	u16 hi;
} interf_snr_thres;
#endif /* MONO_SWITCH_INTERF */

typedef struct {
	u32 demod_conf_ini;
	u16 rssi_adj_ini;
	soft_muffle_conf soft_muffle_conf_ini;
	u16 soft_mute_atten_max_ini;
	u16 stereo_thres_ini;
	u16 narrow_thres_ini;
	u16 snr_adj_ini;
	u16 snr_smooth_conf_ini;
	u16 mute_coeffs_soft;
	u16 mute_coeffs_dis;
	u16 blend_coeffs_soft;
	u16 blend_coeffs_switch;
	u16 blend_coeffs_dis;
	u16 rds_int_byte_count;
	search_config search_conf;
#ifdef MONO_SWITCH_INTERF
	interf_rssi_thres interf_rssi;
	interf_snr_thres interf_snr;
#endif
	u16 rds_error_limit;
} fm_conf_ini_values;

/***************************************************************************/

typedef struct {
	bool rds_rx_enabled;
	bool fm_pwr_on;
	bool rds_pwr_on;
	bool force_mono;
	bool use_switched_blend;
	bool use_soft_mute;
	bool mute_forced;
	bool mute_audio;
	bool search_down;
	bool use_rbds;
	bool save_eblks;
	bool last_status_blend_stereo;
	bool last_status_rds_sync;
#ifdef MONO_SWITCH_INTERF
	bool force_mono_interf;
	bool interf_checked;
	bool mono_switched_interf;
	TIME mono_interf_reset_time;
#endif /* MONO_SWITCH_INTERF */
	u8 tuner_mode;
	u8 status;
	u8 rds_mem_thresh;
	u8 rssi;
	u8 band;
	u16 last_ifc;
	u16 snr;
	u16 rssi_limit_normal;
	u16 rssi_limit_search;
	u32 freq;
	u16 flags;
	u8 rds_unsync_uncorr_weight;
	u8 rds_unsync_blk_cnt;
	u16 rds_unsync_bit_cnt;
} fm_state_s;

typedef enum {
	TUNER_OFF,
	TUNER_NOTTUNED,
	TUNER_IDLE,
	TUNER_PRESET,
	TUNER_SEARCH
} fm_tuner_state;

/***************************************************************************/

typedef struct {
	fm_tuner_state tuner_state;
	bool curr_search_down;
	bool hit_band_limit;
	bool tune_done;
	u16 freq_step;
	u32 band_limit_lo;
	u32 band_limit_hi;
} fm_tuner_state_s;

typedef enum {
	RDS_RM_ALIGN_0 = 0,
	RDS_RM_ALIGN_1 = 1,
	RDS_RM_ALIGN_2 = 2,
	RDS_RM_ALIGN_3 = 3,
	RDS_RM_ALIGN_NONE = 4
} fm_rds_rm_align_enum;

#ifdef	USE_RDS_HW_DECODER
typedef enum {
	RDS_BLKTYPE_A	= 0,
	RDS_BLKTYPE_B	= 1,
	RDS_BLKTYPE_C	= 2,
	RDS_BLKTYPE_C2	= 3,
	RDS_BLKTYPE_D	= 4,
	RDS_BLKTYPE_E	= 5,
	RDS_NUM_BLOCK_TYPES = 6
} fm_rds_block_type_enum;

typedef enum {
	RDS_STATE_INIT,
	RDS_STATE_HAVE_DATA,
	RDS_STATE_PRE_SYNC,
	RDS_STATE_FULL_SYNC
} fm_rds_state_enum;
#else
typedef enum {
	RDS_BLKTYPE_A   = 0,
	RDS_BLKTYPE_B   = 1,
	RDS_BLKTYPE_C   = 2,
	RDS_BLKTYPE_D   = 3,
	RDS_BLKTYPE_E   = 4,
	RDS_NUM_BLOCK_TYPES = 5
} fm_rds_block_type_enum;

typedef enum {
	RDS_STATE_FOUND_BL_A,
	RDS_STATE_FOUND_BL_B,
	RDS_STATE_FOUND_BL_C,
	RDS_STATE_FOUND_BL_D,
	RDS_STATE_HAVE_DATA,
	RDS_STATE_PRE_SYNC,
	RDS_STATE_FULL_SYNC,
	RDS_STATE_INIT,
} fm_rds_state_enum;
#endif	/*USE_RDS_HW_DECODER*/

typedef struct {
	unsigned current_state :3;
	u16 error_bits;
	u8 error_blks;
} fm_rds_state_s;

typedef struct rds_buf_conf {
	u8 *base;
	u16 index;
	u16 outdex;
	u16 size;
} rds_buf_conf;


/****************************************************************************/

typedef struct struct_fm_rx_setup {
	u32 fm_freq_hz;
	u32 fm_freq_khz;
	u16 demod_if;
	s16 lna_cdac;
	u16 spur_ctrl;
	s16 spur_freq;
} struct_fm_rx_setup;

typedef struct struct_fm_lo_setup {
	u32 rx_lo_req_freq;
	u32 n_mmdiv;
	u32 frac_b1;
	u32 frac_b0;
	u32 n_lodiv;
} struct_fm_lo_setup;

 typedef struct struct_fm_rx_tune_info {
	struct_fm_rx_setup rx_setup;
	struct_fm_lo_setup lo_setup;
} struct_fm_rx_tune_info;

/**********************************************/
/* FM low struct
 **********************************************/
typedef struct {
	u32 lo;
	u32 hi;
} fm_band_s;

struct s610_low {
	/* fm low level struct - start */
	fm_conf_ini_values fm_config;
	fm_state_s fm_state;
	fm_tuner_state_s fm_tuner_state;
	fm_rds_state_s fm_rds_state;

	u8 *rds_buffer_mem;
	rds_buf_conf rds_buffer_config;
	rds_buf_conf *rds_buffer;

	struct_fm_rx_tune_info fm_tune_info;

	fm_band_s fm_bands[2];

	u16 fm_freq_steps[3];

#ifdef USE_SPUR_CANCEL
	u32 fm_spur[256];
#endif
#ifdef USE_SPUR_CANCEL_TRF
	u32 fm_spur_trf[256];
#endif
#ifdef USE_S612_DUAL_CLOCKING
	u32 fm_dual_clk[256];
#endif
	/* fm low level struct  -  end */
};

typedef void fm_callback_t(unsigned long);
typedef struct timer_list fm_timer_t;
typedef void fm_linux_callback_t(u_long);

#define fm_set_flag_bits(radio, value) \
		fm_set_flags(radio, radio->low->fm_state.flags | (value))
#define fm_clear_flag_bits(radio, value) \
		fm_set_flags(radio, radio->low->fm_state.flags & ~(value))
#define fm_get_band(radio) (radio->low->fm_state.band)
#define fm_get_freq_step(radio) (radio->low->fm_tuner_state.freq_step)


#endif	/*FM_LOW_STRUC_H*/
