#if defined(__UFS_CAL_U_BOOT__)
#include <linux/types.h>
#include <ufs-cal-9610.h>
#elif defined(__UFS_CAL_FW__)
#include <types.h>
#include <include/ufs-cal.h>
#else
#include <linux/types.h>
#include "ufs-cal-9610.h"
#endif

#ifndef _UFS_CAL_
#define _UFS_CAL_

/* UFSHCI */
#define UIC_ARG_MIB_SEL(attr, sel)	((((attr) & 0xFFFF) << 16) |\
					 ((sel) & 0xFFFF))
#define UIC_ARG_MIB(attr)		UIC_ARG_MIB_SEL(attr, 0)

/* Unipro.h */
#define IS_PWR_MODE_HS(m)        (((m) == FAST_MODE) || ((m) == FASTAUTO_MODE))
#define IS_PWR_MODE_PWM(m)       (((m) == SLOW_MODE) || ((m) == SLOWAUTO_MODE))

enum {
	PA_HS_MODE_A	= 1,
	PA_HS_MODE_B	= 2,
};

enum {
	FAST_MODE	= 1,
	SLOW_MODE	= 2,
	FASTAUTO_MODE	= 4,
	SLOWAUTO_MODE	= 5,
	UNCHANGED	= 7,
};

/* User defined */
#define UNIPRO_MCLK_PERIOD(p) (1000000000L / p->mclk_rate)

#define PHY_PMA_COMN_ADDR(reg)		(reg)
#define PHY_PMA_TRSV_ADDR(reg, lane)	((reg) + (0x140 * (lane)))

#define NUM_OF_UFS_HOST	1

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
	PHY_PLL_WAIT,
	PHY_CDR_WAIT,
	PHY_CDR_AFC_WAIT,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,

	/* Since exynos8895 */
	PHY_PCS_RX,
	PHY_PCS_TX,
	PHY_PCS_RX_PRD,
	PHY_PCS_TX_PRD,
	UNIPRO_DBG_PRD,
	PHY_PMA_TRSV_LANE1_SQ_OFF,
	COMMON_WAIT,
};

enum {
	TX_LANE_0 = 0,
	TX_LANE_1 = 1,
	TX_LANE_2 = 2,
	TX_LANE_3 = 3,
	RX_LANE_0 = 4,
	RX_LANE_1 = 5,
	RX_LANE_2 = 6,
	RX_LANE_3 = 7,
};

enum {
	__PMD_PWM_G1_L1,
	__PMD_PWM_G1_L2,
	__PMD_PWM_G2_L1,
	__PMD_PWM_G2_L2,
	__PMD_PWM_G3_L1,
	__PMD_PWM_G3_L2,
	__PMD_PWM_G4_L1,
	__PMD_PWM_G4_L2,
	__PMD_PWM_G5_L1,
	__PMD_PWM_G5_L2,
	__PMD_HS_G1_L1,
	__PMD_HS_G1_L2,
	__PMD_HS_G2_L1,
	__PMD_HS_G2_L2,
	__PMD_HS_G3_L1,
	__PMD_HS_G3_L2,
};

#define PMD_PWM_G1_L1	(1U << __PMD_PWM_G1_L1)
#define PMD_PWM_G1_L2	(1U << __PMD_PWM_G1_L2)
#define PMD_PWM_G2_L1	(1U << __PMD_PWM_G2_L1)
#define PMD_PWM_G2_L2	(1U << __PMD_PWM_G2_L2)
#define PMD_PWM_G3_L1	(1U << __PMD_PWM_G3_L1)
#define PMD_PWM_G3_L2	(1U << __PMD_PWM_G3_L2)
#define PMD_PWM_G4_L1	(1U << __PMD_PWM_G4_L1)
#define PMD_PWM_G4_L2	(1U << __PMD_PWM_G4_L2)
#define PMD_PWM_G5_L1	(1U << __PMD_PWM_G5_L1)
#define PMD_PWM_G5_L2	(1U << __PMD_PWM_G5_L2)
#define PMD_HS_G1_L1	(1U << __PMD_HS_G1_L1)
#define PMD_HS_G1_L2	(1U << __PMD_HS_G1_L2)
#define PMD_HS_G2_L1	(1U << __PMD_HS_G2_L1)
#define PMD_HS_G2_L2	(1U << __PMD_HS_G2_L2)
#define PMD_HS_G3_L1	(1U << __PMD_HS_G3_L1)
#define PMD_HS_G3_L2	(1U << __PMD_HS_G3_L2)

#define PMD_ALL		(PMD_HS_G3_L2 - 1)
#define PMD_PWM		(PMD_PWM_G4_L2 - 1)
#define PMD_HS		(PMD_ALL ^ PMD_PWM)

struct ufs_cal_phy_cfg {
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
	u8 board;
};

#define for_each_phy_cfg(cfg) \
	for (; (cfg)->flg != PHY_CFG_NONE; (cfg)++)

#endif /*_UFS_CAL_ */

static struct ufs_cal_param *ufs_cal[NUM_OF_UFS_HOST];
static unsigned long ufs_cal_lock_timeout = 0xFFFFFFFF;

static struct ufs_cal_phy_cfg init_cfg[] = {
	{0x9514, 0x00, PMD_ALL, UNIPRO_DBG_PRD, __BRD_COMMON},
	{0x200, 0x40, PMD_ALL, PHY_PCS_COMN, __BRD_COMMON},
	{0x12, 0x00, PMD_ALL, PHY_PCS_RX_PRD, __BRD_COMMON},
	{0xAA, 0x00, PMD_ALL, PHY_PCS_TX_PRD, __BRD_COMMON},
	{0x5C, 0x38, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x0F, 0x0, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x65, 0x01, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x69, 0x1, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x21, 0x0, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x22, 0x0, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x84, 0x1, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x04, 0x1, PMD_ALL, PHY_PCS_TX, __BRD_COMMON},
	{0x8F, 0x3E, PMD_ALL, PHY_PCS_TX, __BRD_COMMON},
	{0x200, 0x0, PMD_ALL, PHY_PCS_COMN, __BRD_COMMON},
	{0x9536, 0x4E20, PMD_ALL, UNIPRO_DBG_MIB, __BRD_COMMON},
	{0x9564, 0x2e820183, PMD_ALL, UNIPRO_DBG_MIB, __BRD_COMMON},
	{0x155E, 0x0, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x3000, 0x0, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x3001, 0x1, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x4021, 0x1, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x4020, 0x1, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x8C, 0x80, PMD_ALL, PHY_PMA_COMN, __BRD_COMMON},
	{0x74, 0x10, PMD_ALL, PHY_PMA_COMN, __BRD_COMMON},
	{0x110, 0xB5, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x43, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x16C, 0x20, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x178, 0xC0, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x1B0, 0x94, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0xE0, 0x12, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x164, 0x58, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x8C, 0xC0, PMD_ALL, PHY_PMA_COMN, __BRD_COMMON},
	{0x8C, 0x00, PMD_ALL, PHY_PMA_COMN, __BRD_COMMON},
	{0x00, 0xC8, PMD_ALL, COMMON_WAIT, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg post_init_cfg[] = {
	{0x9529, 0x1, PMD_ALL, UNIPRO_DBG_MIB, __BRD_COMMON},
	{0x15A4, 0xFA, PMD_ALL, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x9529, 0x0, PMD_ALL, UNIPRO_DBG_MIB, __BRD_COMMON},
	{0x200, 0x40, PMD_ALL, PHY_PCS_COMN, __BRD_COMMON},
	{0x35, 0x05, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x73, 0x01, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x41, 0x02, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x42, 0xAC, PMD_ALL, PHY_PCS_RX, __BRD_COMMON},
	{0x200, 0x0, PMD_ALL, PHY_PCS_COMN, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg calib_of_pwm[] = {
	{0x2041, 8064, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2042, 28224, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2043, 20160, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B0, 12000, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B1, 32000, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B2, 16000, PMD_PWM, UNIPRO_STD_MIB, __BRD_COMMON},

	{0x7888, 8064, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x788C, 28224, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x7890, 20160, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78B8, 12000, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78BC, 32000, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78C0, 16000, PMD_PWM, UNIPRO_DBG_APB, __BRD_COMMON},

	//MPHY tuning value
	{0xC8, 0x40, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{0xF0, 0x77, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{0x120, 0x80, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x00, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{0x12C, 0x00, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x43, PMD_PWM, PHY_PMA_TRSV, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_pwm[] = {
	{},
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a[] = {
	{0x2041, 8064, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2042, 28224, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2043, 20160, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B0, 12000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B1, 32000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B2, 16000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},

	{0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},

	{0xC8, 0xBC, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0xF0, 0x7F, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x120, 0xC0, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x08, PMD_HS_G1_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x02, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x00, PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x12C, 0x10, PMD_HS_G1_L2|PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_SMDK},
	{0x12C, 0x00, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0xd3, PMD_HS_G1_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x73, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x63, PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_COMMON},
	//RX EQ
	//{0x108, 0x5D, PMD_HS, PHY_PMA_TRSV, __BRD_SMDK},
	//{0x108, 0x5D, PMD_HS, PHY_PMA_TRSV, __BRD_ASB},

	//{0x10C, 0xD1, PMD_HS, PHY_PMA_TRSV, __BRD_SMDK},
	//{0x10C, 0xD1, PMD_HS, PHY_PMA_TRSV, __BRD_ASB},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a[] = {
	{0x1fc, 0x40, PMD_HS, PHY_CDR_AFC_WAIT, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b[] = {
	{0x2041, 8064, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2042, 28224, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x2043, 20160, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B0, 12000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B1, 32000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},
	{0x15B2, 16000, PMD_HS, UNIPRO_STD_MIB, __BRD_COMMON},

	{0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},
	{0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, __BRD_COMMON},

	//MPHY tuning value
	{0xC8, 0xBC, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0xF0, 0x7F, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x120, 0xC0, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x08, PMD_HS_G1_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x02, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x128, 0x00, PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x12C, 0x10, PMD_HS_G1_L2|PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_SMDK},
	{0x12C, 0x00, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0xd3, PMD_HS_G1_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x73, PMD_HS_G2_L2, PHY_PMA_TRSV, __BRD_COMMON},
	{0x134, 0x63, PMD_HS_G3_L2, PHY_PMA_TRSV, __BRD_COMMON},
	//RX EQ
	//{0x108, 0x5F, PMD_HS, PHY_PMA_TRSV, __BRD_SMDK},
	//{0x108, 0x5F, PMD_HS, PHY_PMA_TRSV, __BRD_ASB},

	//{0x10C, 0xD0, PMD_HS, PHY_PMA_TRSV, __BRD_SMDK},
	//{0x10C, 0xD0, PMD_HS, PHY_PMA_TRSV, __BRD_ASB},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b[] = {
	{0x1fc, 0x40, PMD_HS, PHY_CDR_AFC_WAIT, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg post_h8_enter[] = {
	{0x0C4, 0x99, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x0E8, 0x7F, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x0F0, 0x7F, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x004, 0x02, PMD_ALL, PHY_PMA_COMN, __BRD_COMMON},
	{},
};

static struct ufs_cal_phy_cfg pre_h8_exit[] = {
	{0x004, 0x3F, PMD_HS, PHY_PMA_COMN, __BRD_COMMON},
	{0x004, 0x00, PMD_PWM, PHY_PMA_COMN, __BRD_COMMON},
	{0x0C4, 0xD9, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x0E8, 0x77, PMD_ALL, PHY_PMA_TRSV, __BRD_COMMON},
	{0x00,  0x0A, PMD_HS, COMMON_WAIT, __BRD_COMMON},
	{0x0F0, 0xFF, PMD_HS, PHY_PMA_TRSV, __BRD_COMMON},
	{0x1fc, 0x01, PMD_HS, PHY_CDR_AFC_WAIT, __BRD_COMMON},
	{},
};

static inline ufs_cal_errno ufs_cal_wait_cdr_afc_check(void *hba,
			u32 addr, u32 mask, int lane)
{
	u32 delay_us = 1;
	u32 delay2_us = 40;
	u32 reg = 0;
	u32 i;

	for (i = 0; i < 100; i++) {
		ufs_lld_usleep_delay(delay2_us, delay2_us);

		reg = ufs_lld_pma_read(hba, PHY_PMA_TRSV_ADDR(addr, lane));
		if (mask == (reg & mask))
			return UFS_CAL_NO_ERROR;

		ufs_lld_usleep_delay(delay_us, delay_us);
		ufs_lld_pma_write(hba, 0x7F, PHY_PMA_TRSV_ADDR(0xF0, lane));
		ufs_lld_pma_write(hba, 0xFF, PHY_PMA_TRSV_ADDR(0xF0, lane));
	}
	return UFS_CAL_ERROR;
}

static inline ufs_cal_errno __match_board_by_cfg(u8 board, u8 cfg_board)
{
	ufs_cal_errno match = UFS_CAL_ERROR;

	if (cfg_board == 0)
		match = UFS_CAL_NO_ERROR;
	else if (board == cfg_board)
		match = UFS_CAL_NO_ERROR;

	return match;
}

static inline ufs_cal_errno __match_mode_by_cfg(struct uic_pwr_mode *pmd,
								int mode)
{
	ufs_cal_errno match = UFS_CAL_ERROR;
	u8 _m, _l, _g;

	_m = pmd->mode;
	_g = pmd->gear;
	_l = pmd->lane;

	if (mode == PMD_ALL)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && mode == PMD_HS)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && mode == PMD_PWM)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 1
		&& (mode & (PMD_HS_G1_L1|PMD_HS_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 2
		&& (mode & (PMD_HS_G1_L1|PMD_HS_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 1
		&& (mode & (PMD_HS_G2_L1|PMD_HS_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 2
		&& (mode & (PMD_HS_G2_L1|PMD_HS_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 1
		&& (mode & (PMD_HS_G3_L1|PMD_HS_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 2
		&& (mode & (PMD_HS_G3_L1|PMD_HS_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 1
		&& (mode & (PMD_PWM_G1_L1|PMD_PWM_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 2
		&& (mode & (PMD_PWM_G1_L1|PMD_PWM_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 1
		&& (mode & (PMD_PWM_G2_L1|PMD_PWM_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 2
		&& (mode & (PMD_PWM_G2_L1|PMD_PWM_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 1
		&& (mode & (PMD_PWM_G3_L1|PMD_PWM_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 2
		&& (mode & (PMD_PWM_G3_L1|PMD_PWM_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 1
		&& (mode & (PMD_PWM_G4_L1|PMD_PWM_G4_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 2
		&& (mode & (PMD_PWM_G4_L1|PMD_PWM_G4_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 1
		&& (mode & (PMD_PWM_G5_L1|PMD_PWM_G5_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 2
		&& (mode & (PMD_PWM_G5_L1|PMD_PWM_G5_L2)))
		match = UFS_CAL_NO_ERROR;

	return match;
}

static inline ufs_cal_errno ufs_cal_wait_pll_lock(void *hba,
					u32 addr, u32 mask)
{
	u32 delay_us = 1;
	u32 period_ms = 1;
	u32 reg;
	unsigned long timeout = ufs_lld_get_time_count(0) + ufs_cal_lock_timeout;

	do {
		reg = ufs_lld_pma_read(hba, PHY_PMA_COMN_ADDR(addr));
		if (mask == (reg & mask))
			return UFS_CAL_NO_ERROR;
		ufs_lld_usleep_delay(delay_us, delay_us);
	} while ((long)(ufs_lld_get_time_count(period_ms) - timeout) >= 0);

	return UFS_CAL_ERROR;
}

static inline ufs_cal_errno ufs_cal_wait_cdr_lock(void *hba,
					u32 addr, u32 mask, int lane)
{
	u32 delay_us = 1;
	u32 period_ms = 1;
	u32 reg;
	unsigned long timeout = ufs_lld_get_time_count(0) + ufs_cal_lock_timeout;

	do {
		reg = ufs_lld_pma_read(hba, PHY_PMA_TRSV_ADDR(addr, lane));
		if (mask == (reg & mask))
			return UFS_CAL_NO_ERROR;
		ufs_lld_usleep_delay(delay_us, delay_us);
	} while ((long)(ufs_lld_get_time_count(period_ms) - timeout) >= 0);

	return UFS_CAL_ERROR;

}

static ufs_cal_errno ufs_cal_config_uic(struct ufs_cal_param *p,
				  const struct ufs_cal_phy_cfg *cfg,
				  struct uic_pwr_mode *pmd)
{
	void *hba = p->host;
	u8 i = 0;

	if (!cfg)
		return UFS_CAL_INV_ARG;

	for_each_phy_cfg(cfg) {
		for (i = 0; i < p->available_lane; i++) {
			if (p->board && UFS_CAL_ERROR ==
					__match_board_by_cfg(p->board, cfg->board))
				continue;
			if (pmd && UFS_CAL_ERROR ==
					__match_mode_by_cfg(pmd, cfg->flg))
				continue;

			switch (cfg->lyr) {
			case PHY_PCS_COMN:
			case UNIPRO_STD_MIB:
			case UNIPRO_DBG_MIB:
				if (i == 0)
					ufs_lld_dme_set(hba, UIC_ARG_MIB(cfg->addr),
						cfg->val);
				break;
			case PHY_PCS_RXTX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr, i),
						cfg->val);
				break;
			case UNIPRO_DBG_PRD:
				if (i == 0)
					ufs_lld_dme_set(hba, UIC_ARG_MIB(cfg->addr),
						UNIPRO_MCLK_PERIOD(p));
				break;
			case PHY_PCS_RX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					RX_LANE_0+i), cfg->val);
				break;
			case PHY_PCS_TX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					TX_LANE_0+i), cfg->val);
				break;
			case PHY_PCS_RX_PRD:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					RX_LANE_0+i), UNIPRO_MCLK_PERIOD(p));
				break;

			case PHY_PCS_TX_PRD:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					TX_LANE_0+i), UNIPRO_MCLK_PERIOD(p));
				break;
			case PHY_PMA_COMN:
				if (i == 0)
					ufs_lld_pma_write(hba, cfg->val,
						PHY_PMA_COMN_ADDR(cfg->addr));
				break;
			case PHY_PMA_TRSV:
				ufs_lld_pma_write(hba, cfg->val,
						PHY_PMA_TRSV_ADDR(cfg->addr, i));
				break;
			case PHY_PMA_TRSV_LANE1_SQ_OFF:
				if (i == 1) {
					if (p->connected_rx_lane < p->available_lane)
						ufs_lld_pma_write(hba, cfg->val,
							PHY_PMA_TRSV_ADDR(cfg->addr, i));
				}
				break;
			case UNIPRO_DBG_APB:
				if (i == 1)
					ufs_lld_unipro_write(hba, cfg->val, cfg->addr);
				break;
			case PHY_PLL_WAIT:
				if (i == 0) {
					if (ufs_cal_wait_pll_lock(hba,
						cfg->addr, cfg->val) ==
								UFS_CAL_ERROR)
						return UFS_CAL_TIMEOUT;
				}
				break;
			case PHY_CDR_WAIT:
				if (ufs_cal_wait_cdr_lock(hba,
						cfg->addr, cfg->val, i) ==
								UFS_CAL_ERROR)
					return UFS_CAL_TIMEOUT;
				break;
			case PHY_CDR_AFC_WAIT:
				if (ufs_cal_wait_cdr_afc_check(hba,
						cfg->addr, cfg->val, i) ==
						UFS_CAL_ERROR)
					return UFS_CAL_TIMEOUT;
				break;
			case COMMON_WAIT:
				if (i == 0)
					ufs_lld_udelay(cfg->val);
				break;
			default:
				break;
			}
		}
	}

	return UFS_CAL_NO_ERROR;
}

/*
 * This is a recommendation from Samsung UFS device vendor.
 *
 * Activate time: host < device
 * Hibern time: host > device
 */
static void ufs_cal_calib_hibern8_values(void *hba)
{
	u32 hw_cap_min_tactivate;
	u32 peer_rx_min_actv_time_cap;
	u32 max_rx_hibern8_time_cap;

	ufs_lld_dme_get(hba, UIC_ARG_MIB_SEL(0x8F, RX_LANE_0),
			&hw_cap_min_tactivate);	/* HW Capability of MIN_TACTIVATE */

	ufs_lld_dme_get(hba, UIC_ARG_MIB(0x15A8),
			&peer_rx_min_actv_time_cap);	/* PA_TActivate */
	ufs_lld_dme_get(hba, UIC_ARG_MIB(0x15A7),
			&max_rx_hibern8_time_cap);	/* PA_Hibern8Time */

	if (peer_rx_min_actv_time_cap >= hw_cap_min_tactivate)
		ufs_lld_dme_peer_set(hba, UIC_ARG_MIB(0x15A8),
				peer_rx_min_actv_time_cap + 1);
	ufs_lld_dme_set(hba, UIC_ARG_MIB(0x15A7), max_rx_hibern8_time_cap + 1);
}

ufs_cal_errno ufs_cal_post_h8_enter(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, post_h8_enter, p->pmd);

	return ret;
}

ufs_cal_errno ufs_cal_pre_h8_exit(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, pre_h8_exit, p->pmd);

	return ret;
}

/*
 * This currently uses only SLOW_MODE and FAST_MODE.
 * If you want others, you should modify this function.
 */
ufs_cal_errno ufs_cal_pre_pmc(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_phy_cfg *cfg;

	if ((p->pmd->mode == SLOW_MODE) || (p->pmd->mode == SLOWAUTO_MODE))
		cfg = calib_of_pwm;
	else if (p->pmd->hs_series == PA_HS_MODE_B)
		cfg = calib_of_hs_rate_b;
	else if (p->pmd->hs_series == PA_HS_MODE_A)
		cfg = calib_of_hs_rate_a;
	else
		return UFS_CAL_INV_ARG;

	ret = ufs_cal_config_uic(p, cfg, p->pmd);

	return ret;
}

/*
 * This currently uses only SLOW_MODE and FAST_MODE.
 * If you want others, you should modify this function.
 */
ufs_cal_errno ufs_cal_post_pmc(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_phy_cfg *cfg;

	if ((p->pmd->mode == SLOWAUTO_MODE) || (p->pmd->mode == SLOW_MODE))
		cfg = post_calib_of_pwm;
	else if (p->pmd->hs_series == PA_HS_MODE_B)
		cfg = post_calib_of_hs_rate_b;
	else if (p->pmd->hs_series == PA_HS_MODE_A)
		cfg = post_calib_of_hs_rate_a;
	else
		return UFS_CAL_INV_ARG;

	ret = ufs_cal_config_uic(p, cfg, p->pmd);

	return ret;
}

ufs_cal_errno ufs_cal_post_link(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ufs_cal_calib_hibern8_values(p->host);

	ret = ufs_cal_config_uic(p, post_init_cfg, NULL);

	return ret;
}

ufs_cal_errno ufs_cal_pre_link(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, init_cfg, NULL);

	return ret;
}

ufs_cal_errno ufs_cal_init(struct ufs_cal_param *p, int idx)
{
	/*
	 * Return if innput index is greater than
	 * the maximum that cal supports
	 */
	if (idx >= NUM_OF_UFS_HOST)
		return UFS_CAL_INV_ARG;

	ufs_cal[idx] = p;

	ufs_cal_lock_timeout = ufs_lld_calc_timeout(100);

	return UFS_CAL_NO_ERROR;
}
