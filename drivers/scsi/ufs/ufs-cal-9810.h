struct uic_pwr_mode {
	u8 lane;
	u8 gear;
	u8 mode;
	u8 hs_series;
};

struct ufs_cal_param {
	void *host;		/* Host adaptor */
	u8 available_lane;
	u8 target_lane;
	u32 mclk_rate;
	u8 board;
	struct uic_pwr_mode *pmd;
};

typedef enum {
	UFS_CAL_NO_ERROR = 0,
	UFS_CAL_TIMEOUT,
	UFS_CAL_ERROR,
	UFS_CAL_INV_ARG,
} ufs_cal_errno;

enum {
	__BRD_COMMON,
	__BRD_UNIV,
	__BRD_SMDK,
};
#define BRD_COMMON	(1U << __BRD_COMMON)
#define BRD_UNIV	(1U << __BRD_UNIV)
#define BRD_SMDK	(1U << __BRD_SMDK)

/* UFS CAL interface */
ufs_cal_errno ufs_cal_post_h8_enter(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_pre_h8_exit(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_post_pmc(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_pre_pmc(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_post_link(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_pre_link(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_init(struct ufs_cal_param *p, int idx);

/* Adaptor for UFS CAL */
void ufs_lld_dme_set(void *h, u32 addr, u32 val);
void ufs_lld_dme_get(void *h, u32 addr, u32 *val);
void ufs_lld_dme_peer_set(void *h, u32 addr, u32 val);
void ufs_lld_pma_write(void *h, u32 val, u32 addr);
u32 ufs_lld_pma_read(void *h, u32 addr);
void ufs_lld_unipro_write(void *h, u32 val, u32 addr);
void ufs_lld_udelay(u32 val);
void ufs_lld_usleep_delay(u32 min, u32 max);
unsigned long ufs_lld_get_time_count(unsigned long offset);
unsigned long ufs_lld_calc_timeout(const unsigned int ms);
