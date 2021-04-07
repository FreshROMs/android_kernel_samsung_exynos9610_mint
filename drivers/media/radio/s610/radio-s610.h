#ifndef RADIO_S610_H
#define RADIO_S610_H

#define DRIVER_NAME "s610-radio"
#define DRIVER_CARD "S610 FM Receiver"

#define	ENABLE_RDS_WORK_QUEUE
#undef	ENABLE_RDS_WORK_QUEUE

#define	ENABLE_IF_WORK_QUEUE
/*#undef	ENABLE_IF_WORK_QUEUE*/

#define	USE_FM_LNA_ENABLE
/*#undef	USE_FM_LNA_ENABLE*/

#define	RDS_POLLING_ENABLE

#define IDLE_POLLING_ENABLE

#define USE_AUDIO_PM

/* DEBUG :: Print debug for debug *******/
#define  SUPPORT_FM_DEBUG
#define  SUPPORT_API_DEBUG
#undef	SUPPORT_FM_DEBUG
#undef	SUPPORT_API_DEBUG

#ifdef	SUPPORT_FM_DEBUG
#define FDEBUG(fm, fmt, args...) dev_info(fm->dev, fmt, ##args)
#define FUNC_ENTRY(fm) dev_info(fm->dev, "+ %s(): entry\n", __func__)
#define FUNC_EXIT(fm) dev_info(fm->dev, "- %s(): exit\n", __func__)
#else
#define FDEBUG(fm, fmt, args...)
#define FUNC_ENTRY(fm)
#define FUNC_EXIT(fm)
#endif /*SUPPORT_FM_DEBUG*/

#ifdef	SUPPORT_API_DEBUG
#define API_ENTRY(fm) dev_info(fm->dev, "> API: %s(): entry\n", __func__)
#define API_EXIT(fm) dev_info(fm->dev, "< API: %s(): exit\n", __func__)
#define APIEBUG(fm, fmt, args...) dev_info(fm->dev, fmt, ##args)
#else
#define API_ENTRY(fm)
#define API_EXIT(fm)
#define APIEBUG(fm, fmt, args...)
#endif

#define  SUPPORT_RDS_DEBUG
#undef	SUPPORT_RDS_DEBUG

#ifdef	SUPPORT_RDS_DEBUG
#define RDSEBUG(fm, fmt, args...) dev_info(fm->dev, fmt, ##args)
#define RDS_ENTRY(fm) dev_info(fm->dev, "> RDS: %s(): entry\n", __func__)
#define RDS_EXIT(fm) dev_info(fm->dev, "< RDS: %s(): exit\n", __func__)
#else
#define RDSEBUG(fm, fmt, args...)
#define RDS_ENTRY(fm)
#define RDS_EXIT(fm)
#endif /*SUPPORT_RDS_DEBUG*/


#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include <linux/i2c.h>
#include "fm_low_struc.h"

#define V4L2_CID_USER_S610_BASE		(0x00980900 + 0x1070)
enum s610_ctrl_id {
	V4L2_CID_S610_CH_SPACING  = (V4L2_CID_USER_S610_BASE + 0x01),
	V4L2_CID_S610_CH_BAND    = (V4L2_CID_USER_S610_BASE + 0x02),
	V4L2_CID_S610_SOFT_STEREO_BLEND = (V4L2_CID_USER_S610_BASE + 0x03),
	V4L2_CID_S610_SOFT_STEREO_BLEND_COEFF = (V4L2_CID_USER_S610_BASE+0x04),
	V4L2_CID_S610_SOFT_MUTE_COEFF = (V4L2_CID_USER_S610_BASE + 0x5),
	V4L2_CID_S610_RSSI_CURR	= (V4L2_CID_USER_S610_BASE + 0x06),
	V4L2_CID_S610_SNR_CURR	= (V4L2_CID_USER_S610_BASE + 0x07),
	V4L2_CID_S610_SEEK_CANCEL	= (V4L2_CID_USER_S610_BASE + 0x08),
	V4L2_CID_S610_SEEK_MODE	= (V4L2_CID_USER_S610_BASE + 0x09),
	V4L2_CID_S610_RDS_ON = (V4L2_CID_USER_S610_BASE + 0x0A),
	V4L2_CID_S610_IF_COUNT1 = (V4L2_CID_USER_S610_BASE + 0x0B),
	V4L2_CID_S610_IF_COUNT2 = (V4L2_CID_USER_S610_BASE + 0x0C),
	V4L2_CID_S610_RSSI_TH = (V4L2_CID_USER_S610_BASE + 0x0D),
	V4L2_CID_S610_KERNEL_VER = (V4L2_CID_USER_S610_BASE + 0x0E),
	V4L2_CID_S610_SOFT_STEREO_BLEND_REF = (V4L2_CID_USER_S610_BASE+0x0F),
};

enum fm_flag_get {
	FM_EVENT_TUNED	    =	(1 << 0),
	FM_EVENT_BD_LMT	    =	(1 << 1),
	FM_EVENT_SYN_LOS  =	(1 << 2),
	FM_EVENT_BUF_FUL	    =	(1 << 3),
	FM_EVENT_AUD_PAU	    =	(1 << 4),
	FM_EVENT_CH_STAT	    =	(1 << 5)
};

/* Tunner modes */
enum fm_tuner_mode {
	FM_TUNER_STOP_SEARCH_MODE	= 0,
	FM_TUNER_PRESET_MODE		= 1,
	FM_TUNER_AUTONOMOUS_SEARCH_MODE	= 2,
	FM_TUNER_AUTONOMOUS_SEARCH_MODE_NEXT	= 10
};

/* channel spacing */
enum fm_channel_spacing {
	FM_CHANNEL_SPACING_50KHZ = 1,
	FM_CHANNEL_SPACING_100KHZ = 2,
	FM_CHANNEL_SPACING_200KHZ = 4
};

#define FM_FREQ_MUL 50

/* Mute modes */
enum fm_mute_mode {
	FM_MUTE_ON   =		0,
	FM_MUTE_OFF   =		1,
	FM_MUTE_ATTENUATE	= 2
};

/* Register set mute bits */
enum fm_reg_mute {
	FM_RX_UNMUTE_MODE		= 0x00,
	FM_RX_RF_DEP_MODE		= 0x01,
	FM_RX_AC_MUTE_MODE		= 0x02,
	FM_RX_HARD_MUTE_LEFT_MODE	= 0x04,
	FM_RX_HARD_MUTE_RIGHT_MODE	= 0x08,
	FM_RX_SOFT_MUTE_FORCE_MODE	= 0x10
};

/* FM RDS modes */
enum fm_rds_mode {
	FM_RDS_DISABLE	= 0,
	FM_RDS_ENABLE	= 1
};

/* FM RDS Parser */
#define MAX_PS  8
#define MAX_RT  64
#define MAX_RTP_TAG 2

struct rtp_tag_info {
	u32 content_type;
	u32 start_pos;
	u32 len;
};

struct rtp_info {
	u8 toggle;
	u8 running;
	u8 validated;
	struct rtp_tag_info tag[2];
};

struct fm_rds_parser_info {
	u32 pi_idx;
	u16 pi_buf[2];

	u32 ecc_idx;
	u8 ecc_buf[2];

	u32 af_idx;
	u16 af_buf[2];

	u8 ps_segment;
	u8 ps_len;
	u32 ps_idx;
	u8 ps_buf[3][MAX_PS + 1];
	u8 ps_err[3][MAX_PS / 2];
	u8 ps_candidate[MAX_PS + 1];

	u8 rt_segment;
	u8 rt_len;
	u32 rt_idx;
	u8 rt_buf[3][MAX_RT + 1];
	u8 rt_err[3][MAX_RT / 2];
	u8 rt_candidate[MAX_RT + 1];
	u8 rt_change;
	u8 rt_validated;

	u8 rtp_drop_blk;
	u8 rtp_code_group;
	u16 rtp_raw_data[3];
	struct rtp_info rtp_data;

	u8 grp;
	bool drop_blk;
	u8 rds_event;
};

/* RF dependent mute mode */
#define FM_RX_RF_DEPENDENT_MUTE_ON	1
#define FM_RX_RF_DEPENDENT_MUTE_OFF	0

#define FM_DRV_TURN_TIMEOUT		(5*HZ)	/* 5 seconds */
#define FM_DRV_SEEK_TIMEOUT		(20*HZ)	/* 10 seconds */

/* Min and Max volume */
#define FM_RX_VOLUME_MIN	0
#define FM_RX_VOLUME_MAX	70

/* Volume gain step */
#define FM_RX_VOLUME_GAIN_STEP	16

#define FM_SEARCH_DIRECTION_UP	0
#define FM_SEARCH_DIRECTION_DOWN	1

/* undefined freq */
#define FM_UNDEFINED_FREQ		   0xFFFFFFFF

/* RDS system type (RDS/RBDS) */
#define FM_RDS_SYSTEM_RDS		0
#define FM_RDS_SYSTEM_RBDS		1

/* AF on/off */
#define FM_RX_RDS_AF_SWITCH_MODE_ON	1
#define FM_RX_RDS_AF_SWITCH_MODE_OFF	0

/* RX RDS */
#define FM_RX_RDS_FLUSH_FIFO		0x1
#define FM_RX_RDS_FIFO_THRESHOLD	48	/* tuples */
#define FM_RX_RDS_FIFO_THRESHOLD_PARSER	100	/* tuples */
#define FM_RDS_BLK_SIZE		3	/* 3 bytes */

/* default RSSI value for init */
#define FM_DEFAULT_RSSI_THRESHOLD	0x8E

/* Reset pre-tune value */
#define RESET_PRETUNE_VALUE     103500

#define GPIO_LOW		0
#define GPIO_HIGH		1

struct s610_radio;

enum s610_power_state {
	S610_POWER_DOWN		= 0,
	S610_POWER_ON_FM		= 1,
	S610_POWER_ON_RDS	= 2
};

/* FM region (Europe/US, Japan) info */
struct region_info {
	u32 chanl_space;
	u32 bot_freq;
	u32 top_freq;
	u8 fm_band;
};

struct s610_core {
	int chip_id;
	struct mutex cmd_lock; /* for serializing fm radio operations */
/*	enum s610_power_state power_state;*/
	u8 power_mode;
	wait_queue_head_t  rds_read_queue;
};

/**
 * s610_core_lock() - lock the core device to get an exclusive access
 * to it.
 */
static inline void s610_core_lock_init(struct s610_core *core)
{
	mutex_init(&core->cmd_lock);
}

/**
 * s610_core_lock() - lock the core device to get an exclusive access
 * to it.
 */
static inline void s610_core_lock(struct s610_core *core)
{
	mutex_lock(&core->cmd_lock);
}

/**
 * s610_core_lock() - lock the core device to get an exclusive access
 * to it.
 */
static inline  int __must_check s610_core_lock_interruptible(
		struct s610_core *core)
{
	int ret;

	ret = mutex_lock_interruptible(&core->cmd_lock);
	return ret;
}

/**
 * s610_core_unlock() - unlock the core device to relinquish an
 * exclusive access to it.
 */
static inline void s610_core_unlock(struct s610_core *core)
{
	mutex_unlock(&core->cmd_lock);
}

static inline int api_to_real(int freq)
{
	return freq;
}

static inline int real_to_api(int freq)
{
	return freq;
}

#define FREQ_MUL (10000000 / 625)

enum s610_freq_bands {
	S610_BAND_FM,
	S610_BAND_AM,
};

struct ringbuf_t {
	u8 *buf;
	u8 *head, *tail;
	int size;
};

/**
 * struct s610_radio - radio device
 *
 * @core: Pointer to underlying core device
 * @videodev: Pointer to video device created by V4L2 subsystem
 * @ops: Vtable of functions. See struct s610_radio_ops for details
 * @kref: Reference counter
 * @core_lock: An r/w semaphore to brebvent the deletion of underlying
 * core structure is the radio device is being used
 */
struct s610_radio {
	struct v4l2_device v4l2dev;
	struct video_device videodev;
	struct v4l2_ctrl_handler ctrl_handler;

	struct s610_core  *core;
	struct s610_low  *low;

	u32 audmode;

	int radio_region;
	struct region_info region;	/* Current selected band */
	u8 mute_mode;	/* Current mute mode */
	u32 freq;	/* Current RX frquency */
	u8 deemphasis_mode; /* Current deemphasis mode */
	u8 rf_depend_mute;	/* RF dependent soft mute mode */
	u16 volume;	/* Current volume level */
	u16 rssi_threshold;	/* Current RSSI threshold level */
	u8 rds_mode;	/* RDS operation mode (RDS/RDBS) */
	u8 af_mode;	/* Alternate frequency on/off */

	u16 irq_flag;	/* FM interrupt flag */
	u16 irq_mask;	/* FM interrupt mask */
	unsigned int irq;    /* AP interrupt line */

	/* flags FR BL completion handler */
	struct completion flags_set_fr_comp;
	struct completion flags_seek_fr_comp;

	/* set if counter completion handler */
	struct completion set_if_cnt_comp;

	spinlock_t slock; /* To protect access to buffer */
	spinlock_t rds_lock; /* To protect access to buffer */

	struct wake_lock	wakelock;
	struct wake_lock	rdswakelock;
	atomic_t is_doing;
	atomic_t is_rds_new;
	atomic_t is_rds_doing;
	int	wait_atomic;
	int	wait_atomic_rds;

	spinlock_t rds_buff_lock; /* To protect access to RDS buffer */
	u8 rds_flag;	/* RX RDS on/off status */
	u8 rds_new;		/* RX RDS new data status */
	u8 rds_buf[480];
	struct ringbuf_t rds_rb;
	struct fm_rds_parser_info pi;
	struct mutex	lock;
	struct workqueue_struct *work_queue;
	struct workqueue_struct *if_work_queue;

#ifdef	ENABLE_RDS_WORK_QUEUE
	struct work_struct work;
#endif	/*ENABLE_RDS_WORK_QUEUE*/
#ifdef	ENABLE_IF_WORK_QUEUE
	struct work_struct if_work;
#endif	/*ENABLE_IF_WORK_QUEUE*/

	struct delayed_work dwork_sig2;
	struct delayed_work dwork_tune;
#ifdef	RDS_POLLING_ENABLE
	struct delayed_work dwork_rds_poll;
#endif	/*RDS_POLLING_ENABLE*/
#ifdef	IDLE_POLLING_ENABLE
	struct delayed_work dwork_idle_poll;
#endif	/*IDLE_POLLING_ENABLE*/
	u16 dwork_idle_counter;
	u16 dwork_rds_counter;
	u16 dwork_sig2_counter;
	u16 dwork_tune_counter;
	u16 switch_rssi;
	u16 idle_fniarg;
	u16 sig2_fniarg;
	u16 tune_fniarg;

	u16 freq_step;
	u8 speedy_error;
	u16 seek_mode;
	u32 seek_freq;	/* seek start frquency */
	u32 seek_status;
	int seek_weak_rssi;
	u32 wrap_around;
	u32 iclkaux;
	void __iomem *fmspeedy_base;
	void __iomem *disaud_cmu_base;
#ifdef USE_FM_LNA_ENABLE
	int elna_gpio;
#endif /* USE_FM_EXTERN_PLL */
	struct platform_device *pdev;
	struct device *dev;
#ifdef USE_AUDIO_PM
	struct device *a_dev;
#endif /* USE_AUDIO_PM */
	struct clk **clocks;
	const char **clk_ids;
	int tc_on;
	int trf_on;
	int dual_clk_on;
	int vol_num;
	u32 *vol_level_mod;
	u32 *vol_level_tmp;
	int vol_3db_att;
	int rssi_est_on;
	int sw_mute_weak;
	u32 rfchip_ver;
	int without_elna;
	u16 rssi_adjust;
	bool rssi_ref_enable;
	u32 agc_enable;
/*	debug print counter */
	int idle_cnt_mod;
	int rds_cnt_mod;
	fm_rds_block_type_enum block_seq;

/* Test RDS log */
	bool invalid_rssi;
	int rds_n_count;
	int rds_r_count;
	int rds_new_stat;
	int rds_read_cnt;
	int rds_fifo_err_cnt;
	int rds_fifo_rd_cnt;
	int rds_reset_cnt;
	int rds_sync_loss_cnt;
	int rb_used;
	bool rds_parser_enable;
	int rds_gcnt;
/* Test RDS log end */
};

extern bool fm_radio_on(struct s610_radio *radio);
extern void fm_radio_off(struct s610_radio *radio);
extern void fm_rds_on(struct s610_radio *radio);
extern void fm_rds_off(struct s610_radio *radio);
extern void fm_set_band(struct s610_radio *radio, u8 index);
extern int fm_boot(struct s610_radio *radio);
extern void fm_power_off(void);

extern int low_get_search_lvl(struct s610_radio *radio, u16 *value);
extern u16 fm_get_flags(struct s610_radio *radio);
extern int low_set_if_limit(struct s610_radio *radio, u16 value);
extern int low_set_search_lvl(struct s610_radio *radio, u16 value);
extern int low_set_freq(struct s610_radio *radio, u32 value);
extern int low_set_tuner_mode(struct s610_radio *radio, u16 value);
extern int low_set_mute_state(struct s610_radio *radio, u16 value);
extern int low_set_most_mode(struct s610_radio *radio, u16 value);
extern int low_set_most_blend(struct s610_radio *radio, u16 value);
extern int low_set_pause_lvl(struct s610_radio *radio, u16 value);
extern int low_set_pause_dur(struct s610_radio *radio, u16 value);
extern int low_set_demph_mode(struct s610_radio *radio, u16 value);
extern int low_set_rds_cntr(struct s610_radio *radio, u16 value);
extern int low_set_power(struct s610_radio *radio, u16 value);
extern u8 aggr_rssi_device_to_host(u16 val);
extern void fm_isr(struct s610_radio *radio);
extern void cancel_tuner_timer(struct s610_radio *radio);
extern int init_low_struc(struct s610_radio *radio);
extern void fm_speedy_m_int_enable(void);
extern void fm_speedy_m_int_disable(void);
#ifdef	ENABLE_RDS_WORK_QUEUE
extern void s610_rds_work(struct work_struct *work);
#endif	/*ENABLE_RDS_WORK_QUEUE*/
#ifdef	ENABLE_IF_WORK_QUEUE
extern void s610_if_work(struct work_struct *work);
#endif	/*ENABLE_IF_WORK_QUEUE*/

extern void s610_sig2_work(struct work_struct *work);
extern void s610_tune_work(struct work_struct *work);
#ifdef	RDS_POLLING_ENABLE
extern void s610_rds_poll_work(struct work_struct *work);
extern void fm_rds_periodic_update(unsigned long data);
extern void fm_rds_periodic_cancel(unsigned long data);
#endif	/*RDS_POLLING_ENABLE*/
#ifdef	IDLE_POLLING_ENABLE
extern void s610_idle_poll_work(struct work_struct *work);
extern void fm_idle_periodic_update(unsigned long data);
extern void fm_idle_periodic_cancel(unsigned long data);
#endif	/*IDLE_POLLING_ENABLE*/

extern void fm_set_blend_mute(struct s610_radio *radio);

extern u32 fmspeedy_get_reg(u32 addr);
extern int fmspeedy_set_reg(u32 addr, u32 data);
extern u32 fmspeedy_get_reg_field(u32 addr, u32 shift, u32 mask);

extern void fm_update_rssi(struct s610_radio *radio);
extern void fm_update_snr(struct s610_radio *radio);
extern void fm_set_audio_gain(struct s610_radio *radio, u16 gain);
extern void fm_aux_pll_off(void);
extern void fmspeedy_wakeup(void);
extern int fm_read_rds_data(struct s610_radio *radio, u8 *buffer, int size,
		u16 *blocks);
extern u32 fmspeedy_get_reg_work(u32 addr);
extern signed int exynos_get_fm_open_status(void);
extern void fm_ds_set(u32 data);
extern void fm_get_version_number(void);
extern int ringbuf_bytes_used(const struct ringbuf_t *rb);
extern void fm_rds_parser_reset(struct fm_rds_parser_info *pi);
#endif /* RADIO_S610_H */

