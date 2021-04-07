/* regs-dsim.h
 *
 * Register definition file for Samsung MIPI-DSIM driver
 *
 * Copyright (c) 2015 Samsung Electronics
 * Seungbeom Park <sb1.park@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 * Seuni Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REGS_DSIM_H
#define _REGS_DSIM_H

#define DSIM_VERSION					(0x0)

#define M4S4_TOP_ISO_BYPASS				(0x1 << 0)
#define M4S4_MODULE_ISO_BYPASS				(0x1 << 0)
#define MIPI_PHY_M4S4_CON				0x1186070C

#define DSIM_SWRST					(0x4)
#define DSIM_DPHY_RST					(1 << 16)
#define DSIM_SWRST_FUNCRST				(1 << 8)
#define DSIM_SWRST_RESET				(1 << 0)

#define DSIM_LINK_STATUS0				(0x8)
#define DSIM_LINK_STATUS0_VIDEO_MODE_STATUS_GET(x)	((x >> 24) & 0x1)
#define DSIM_LINK_STATUS0_VM_LINE_CNT_GET(x)		((x >> 0) & 0x1fff)

#define DSIM_LINK_STATUS1				(0xc)
#define DSIM_LINK_STATUS1_CMD_MODE_STATUS_GET(x)	((x >> 26) & 0x1)

#define DSIM_LINK_STATUS3				(0x14)
#define DSIM_LINK_STATUS3_PLL_STABLE			(1 << 0)

#define DSIM_MIPI_STATUS				(0x18)
#define DSIM_MIPI_STATUS_FRM_PROCESSING			(1 << 29)
#define DSIM_MIPI_STATUS_FRM_DONE			(1 << 28)
#define DSIM_MIPI_STATUS_SHADOW_REG_UP_EN		(1 << 25)
#define DSIM_MIPI_STATUS_SHADOW_REG_UP_DONE		(1 << 24)
#define DSIM_MIPI_STATUS_INSTANT_OFF_REQ		(1 << 21)
#define DSIM_MIPI_STATUS_INSTANT_OFF_ACK		(1 << 20)
#define DSIM_MIPI_STATUS_FRM_MASK			(1 << 1)
#define DSIM_MIPI_STATUS_TE				(1 << 0)

#define DSIM_DPHY_STATUS				(0x1c)
#define DSIM_DPHY_STATUS_TX_READY_HS_CLK		(1 << 10)
#define DSIM_DPHY_STATUS_ULPS_CLK			(1 << 9)
#define DSIM_DPHY_STATUS_STOP_STATE_CLK			(1 << 8)
#define DSIM_DPHY_STATUS_ULPS_DATA_LANE_GET(x)		(((x) >> 4) & 0xf)
#define DSIM_DPHY_STATUS_ULPS_DAT(_x)			(((_x) & 0xf) << 4)
#define DSIM_DPHY_STATUS_STOP_STATE_DAT(_x)		(((_x) & 0xf) << 0)

#define DSIM_CLK_CTRL					(0x20)
#define DSIM_CLK_CTRL_CLOCK_SEL				(1 << 26)
#define DSIM_CLK_CTRL_NONCONT_CLOCK_LANE		(1 << 25)
#define DSIM_CLK_CTRL_CLKLANE_ONOFF			(1 << 24)
#define DSIM_CLK_CTRL_TX_REQUEST_HSCLK			(1 << 20)
#define DSIM_CLK_CTRL_WORDCLK_EN			(1 << 17)
#define DSIM_CLK_CTRL_ESCCLK_EN				(1 << 16)
#define DSIM_CLK_CTRL_LANE_ESCCLK_EN(_x)		((_x) << 8)
#define DSIM_CLK_CTRL_LANE_ESCCLK_EN_MASK		(0x1f << 8)
#define DSIM_CLK_CTRL_ESC_PRESCALER(_x)			((_x) << 0)
#define DSIM_CLK_CTRL_ESC_PRESCALER_MASK		(0xff << 0)

#define DSIM_DESKEW_CTRL				(0x24)
#define DSIM_DESKEW_CTRL_HW_EN				(1 << 15)
#define DSIM_DESKEW_CTRL_HW_POSITION			(1 << 14)
#define DSIM_DESKEW_CTRL_HW_INTERVAL(_x)		((_x) << 2)
#define DSIM_DESKEW_CTRL_HW_INTERVAL_MASK		(0xfff << 2)
#define DSIM_DESKEW_CTRL_HW_INIT			(1 << 1)
#define DSIM_DESKEW_CTRL_SW_SEND			(1 << 0)

/* Time out register */
#define DSIM_TIMEOUT					(0x28)
#define DSIM_TIMEOUT_BTA_TOUT(_x)			((_x) << 16)
#define DSIM_TIMEOUT_BTA_TOUT_MASK			(0xffff << 16)
#define DSIM_TIMEOUT_LPDR_TOUT(_x)			((_x) << 0)
#define DSIM_TIMEOUT_LPDR_TOUT_MASK			(0xffff << 0)

/* Escape mode register */
#define DSIM_ESCMODE					(0x2c)
#define DSIM_ESCMODE_STOP_STATE_CNT(_x)			((_x) << 21)
#define DSIM_ESCMODE_STOP_STATE_CNT_MASK		(0x7ff << 21)
#define DSIM_ESCMODE_FORCE_STOP_STATE			(1 << 20)
#define DSIM_ESCMODE_FORCE_BTA				(1 << 16)
#define DSIM_ESCMODE_CMD_LPDT				(1 << 7)
#define DSIM_ESCMODE_TRIGGER_RST			(1 << 4)
#define DSIM_ESCMODE_TX_ULPS_DATA			(1 << 3)
#define DSIM_ESCMODE_TX_ULPS_DATA_EXIT			(1 << 2)
#define DSIM_ESCMODE_TX_ULPS_CLK			(1 << 1)
#define DSIM_ESCMODE_TX_ULPS_CLK_EXIT			(1 << 0)

#define DSIM_NUM_OF_TRANSFER				(0x30)
#define DSIM_NUM_OF_TRANSFER_PER_FRAME(_x)		((_x) << 0)
#define DSIM_NUM_OF_TRANSFER_PER_FRAME_MASK		(0xfffff << 0)
#define DSIM_NUM_OF_TRANSFER_PER_FRAME_GET(x)		(((x) >> 0) & 0xfffff)

#define DSIM_UNDERRUN_CTRL				(0x34)
#define DSIM_UNDERRUN_CTRL_CM_UNDERRUN_LP_REF(_x)	((_x) << 0)
#define DSIM_UNDERRUN_CTRL_CM_UNDERRUN_LP_REF_MASK	(0xffff << 0)

#define DSIM_THRESHOLD					(0x38)
#define DSIM_THRESHOLD_LEVEL(_x)			((_x) << 0)
#define DSIM_THRESHOLD_LEVEL_MASK			(0xffff << 0)

/* Display image resolution register */
#define DSIM_RESOL					(0x3c)
#define DSIM_RESOL_VRESOL(x)				(((x) & 0x1fff) << 16)
#define DSIM_RESOL_VRESOL_MASK				(0x1fff << 16)
#define DSIM_RESOL_HRESOL(x)				(((x) & 0x1fff) << 0)
#define DSIM_RESOL_HRESOL_MASK				(0x1fff << 0)
#define DSIM_RESOL_LINEVAL_GET(_v)			(((_v) >> 16) & 0x1fff)
#define DSIM_RESOL_HOZVAL_GET(_v)			(((_v) >> 0) & 0x1fff)

/* Main display Vporch register */
#define DSIM_VPORCH					(0x40)
#define DSIM_VPORCH_VFP_CMD_ALLOW(_x)			((_x) << 24)
#define DSIM_VPORCH_VFP_CMD_ALLOW_MASK			(0xff << 24)
#define DSIM_VPORCH_STABLE_VFP(_x)			((_x) << 16)
#define DSIM_VPORCH_STABLE_VFP_MASK			(0xff << 16)
#define DSIM_VPORCH_VFP_TOTAL(_x)			((_x) << 8)
#define DSIM_VPORCH_VFP_TOTAL_MASK			(0xff << 8)
#define DSIM_VPORCH_VBP(_x)				((_x) << 0)
#define DSIM_VPORCH_VBP_MASK				(0xff << 0)

/* Main display Hporch register */
#define DSIM_HPORCH					(0x44)
#define DSIM_HPORCH_HFP(_x)				((_x) << 16)
#define DSIM_HPORCH_HFP_MASK				(0xffff << 16)
#define DSIM_HPORCH_HBP(_x)				((_x) << 0)
#define DSIM_HPORCH_HBP_MASK				(0xffff << 0)

/* Main display sync area register */
#define DSIM_SYNC					(0x48)
#define DSIM_SYNC_VSA(_x)				((_x) << 16)
#define DSIM_SYNC_VSA_MASK				(0xff << 16)
#define DSIM_SYNC_HSA(_x)				((_x) << 0)
#define DSIM_SYNC_HSA_MASK				(0xffff << 0)

/* Configuration register */
#define DSIM_CONFIG					(0x4c)
#define DSIM_CONFIG_SYNC_INFORM				(1 << 27)
#define DSIM_CONFIG_BURST_MODE				(1 << 26)
#define DSIM_CONFIG_LP_FORCE_EN				(1 << 24)
#define DSIM_CONFIG_HSE_DISABLE				(1 << 23)
#define DSIM_CONFIG_HFP_DISABLE				(1 << 22)
#define DSIM_CONFIG_HBP_DISABLE				(1 << 21)
#define DSIM_CONFIG_HSA_DISABLE				(1 << 20)
#define DSIM_CONFIG_CPRS_EN				(1 << 19)
#define DSIM_CONFIG_VIDEO_MODE				(1 << 18)
#define DSIM_CONFIG_VC_ID(_x)				((_x) << 15)
#define DSIM_CONFIG_VC_ID_MASK				(0x3 << 15)
#define DSIM_CONFIG_PIXEL_FORMAT(_x)			((_x) << 9)
#define DSIM_CONFIG_PIXEL_FORMAT_MASK			(0x3f << 9)
#define DSIM_CONFIG_PER_FRAME_READ_EN			(1 << 8)
#define DSIM_CONFIG_EOTP_EN				(1 << 7)
#define DSIM_CONFIG_NUM_OF_DATA_LANE(_x)		((_x) << 5)
#define DSIM_CONFIG_NUM_OF_DATA_LANE_MASK		(0x3 << 5)
#define DSIM_CONFIG_LANES_EN(_x)			(((_x) & 0x1f) << 0)

/* Interrupt source register */
#define DSIM_INTSRC					(0x50)
#define DSIM_INTSRC_PLL_STABLE				(1 << 31)
#define DSIM_INTSRC_SW_RST_RELEASE			(1 << 30)
#define DSIM_INTSRC_SFR_PL_FIFO_EMPTY			(1 << 29)
#define DSIM_INTSRC_SFR_PH_FIFO_EMPTY			(1 << 28)
#define DSIM_INTSRC_SFR_PH_FIFO_OVERFLOW		(1 << 27)
#define DSIM_INTSRC_SW_DESKEW_DONE			(1 << 26)
#define DSIM_INTSRC_BUS_TURN_OVER			(1 << 25)
#define DSIM_INTSRC_FRAME_DONE				(1 << 24)
#define DSIM_INTSRC_INVALID_SFR_VALUE			(1 << 23)
#define DSIM_INTSRC_ABNRMAL_CMD_ST			(1 << 22)
#define DSIM_INTSRC_LPRX_TOUT				(1 << 21)
#define DSIM_INTSRC_BTA_TOUT				(1 << 20)
#define DSIM_INTSRC_UNDER_RUN				(1 << 19)
#define DSIM_INTSRC_RX_DATA_DONE			(1 << 18)
#define DSIM_INTSRC_RX_TE				(1 << 17)
#define DSIM_INTSRC_RX_ACK				(1 << 16)
#define DSIM_INTSRC_ERR_RX_ECC				(1 << 15)
#define DSIM_INTSRC_RX_CRC				(1 << 14)
#define DSIM_INTSRC_VT_STATUS				(1 << 13)

/* Interrupt mask register */
#define DSIM_INTMSK					(0x54)
#define DSIM_INTMSK_PLL_STABLE				(1 << 31)
#define DSIM_INTMSK_SW_RST_RELEASE			(1 << 30)
#define DSIM_INTMSK_SFR_PL_FIFO_EMPTY			(1 << 29)
#define DSIM_INTMSK_SFR_PH_FIFO_EMPTY			(1 << 28)
#define DSIM_INTMSK_SFR_PH_FIFO_OVERFLOW		(1 << 27)
#define DSIM_INTMSK_SW_DESKEW_DONE			(1 << 26)
#define DSIM_INTMSK_BUS_TURN_OVER			(1 << 25)
#define DSIM_INTMSK_FRAME_DONE				(1 << 24)
#define DSIM_INTMSK_INVALID_SFR_VALUE			(1 << 23)
#define DSIM_INTMSK_ABNRMAL_CMD_ST			(1 << 22)
#define DSIM_INTMSK_LPRX_TOUT				(1 << 21)
#define DSIM_INTMSK_BTA_TOUT				(1 << 20)
#define DSIM_INTMSK_UNDER_RUN				(1 << 19)
#define DSIM_INTMSK_RX_DATA_DONE			(1 << 18)
#define DSIM_INTMSK_RX_TE				(1 << 17)
#define DSIM_INTMSK_RX_ACK				(1 << 16)
#define DSIM_INTMSK_ERR_RX_ECC				(1 << 15)
#define DSIM_INTMSK_RX_CRC				(1 << 14)
#define DSIM_INTMSK_VT_STATUS				(1 << 13)

/* Packet Header FIFO register */
#define DSIM_PKTHDR					(0x58)
#define DSIM_PKTHDR_BTA_TYPE(_x)			((_x) << 24)
#define DSIM_PKTHDR_DATA1(_x)				((_x) << 16)
#define DSIM_PKTHDR_DATA0(_x)				((_x) << 8)
#define DSIM_PKTHDR_ID(_x)				((_x) << 0)
#define DSIM_PKTHDR_DATA				(0x1ffffff << 0)

/* Payload FIFO register */
#define DSIM_PAYLOAD					(0x5c)

/* Read FIFO register */
#define DSIM_RXFIFO					(0x60)

/* SFR control Register for Stanby & Shadow*/
#define DSIM_SFR_CTRL					(0x64)
#define DSIM_SFR_CTRL_SHADOW_REG_READ_EN		(1 << 1)
#define DSIM_SFR_CTRL_SHADOW_EN				(1 << 0)

/* FIFO status and control register */
#define DSIM_FIFOCTRL					(0x68)
#define DSIM_FIFOCTRL_NUMBER_OF_PH_SFR(_x)		(((_x) & 0x3f) << 16)
#define DSIM_FIFOCTRL_NUMBER_OF_PH_SFR_GET(x)		(((x) >> 16) & 0x3f)
#define DSIM_FIFOCTRL_EMPTY_RX				(1 << 12)
#define DSIM_FIFOCTRL_FULL_PH_SFR			(1 << 11)
#define DSIM_FIFOCTRL_EMPTY_PH_SFR			(1 << 10)
#define DSIM_FIFOCTRL_FULL_PL_SFR			(1 << 9)
#define DSIM_FIFOCTRL_EMPTY_PL_SFR			(1 << 8)
#define DSIM_FIFOCTRL_INIT_RX				(1 << 2)
#define DSIM_FIFOCTRL_INIT_PL_SFR			(1 << 1)
#define DSIM_FIFOCTRL_INIT_PH_SFR			(1 << 0)

#define DSIM_LP_SCATTER					(0x6c)
#define DSIM_LP_SCATTER_PATTERN(_x)			((_x) << 16)
#define DSIM_LP_SCATTER_PATTERN_MASK			(0xffff << 16)
#define DSIM_LP_SCATTER_EN				(1 << 0)

#define DSIM_S3D_CTRL					(0x70)
#define DSIM_S3D_CTRL_3D_PRESENT			(1 << 11)
#define DSIM_S3D_CTRL_3D_ORDER				(1 << 5)
#define DSIM_S3D_CTRL_3D_VSYNC				(1 << 4)
#define DSIM_S3D_CTRL_3D_FORMAT(_x)			(((_x) & 0x3) << 2)
#define DSIM_S3D_CTRL_3D_FORMAT_GET(x)			(((x) >> 2) & 0x3)
#define DSIM_S3D_CTRL_3D_MODE(_x)			(((_x) & 0x3) << 0)
#define DSIM_S3D_CTRL_3D_MODE_GET(x)			(((x) >> 0) & 0x3)

/* Multi slice setting register*/
#define DSIM_CPRS_CTRL					(0x74)
#define DSIM_CPRS_CTRL_MULI_SLICE_PACKET		(1 << 3)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE(_x)			((_x) << 0)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE_MASK		(0x7 << 0)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE_GET(x)		(((x) >> 0) & 0x7)

/*Slice01 size register*/
#define DSIM_SLICE01					(0x78)
#define DSIM_SLICE01_SIZE_OF_SLICE1(_x)			((_x) << 16)
#define DSIM_SLICE01_SIZE_OF_SLICE1_MASK		(0x1fff << 16)
#define DSIM_SLICE01_SIZE_OF_SLICE1_GET(x)		(((x) >> 16) & 0x1fff)
#define DSIM_SLICE01_SIZE_OF_SLICE0(_x)			((_x) << 0)
#define DSIM_SLICE01_SIZE_OF_SLICE0_MASK		(0x1fff << 0)
#define DSIM_SLICE01_SIZE_OF_SLICE0_GET(x)		(((x) >> 0) & 0x1fff)

/*Slice23 size register*/
#define DSIM_SLICE23					(0x7c)
#define DSIM_SLICE23_SIZE_OF_SLICE3(_x)			((_x) << 16)
#define DSIM_SLICE23_SIZE_OF_SLICE3_MASK		(0x1fff << 16)
#define DSIM_SLICE23_SIZE_OF_SLICE3_GET(x)		(((x) >> 16) & 0x1fff)
#define DSIM_SLICE23_SIZE_OF_SLICE2(_x)			((_x) << 0)
#define DSIM_SLICE23_SIZE_OF_SLICE2_MASK		(0x1fff << 0)
#define DSIM_SLICE23_SIZE_OF_SLICE2_GET(x)		(((x) >> 0) & 0x1fff)

/* Command configuration register */
#define DSIM_CMD_CONFIG					(0x80)
#define DSIM_CMD_CONFIG_PKT_GO_RDY			(1 << 17)
#define DSIM_CMD_CONFIG_PKT_GO_EN			(1 << 16)
#define DSIM_CMD_CONFIG_MULTI_CMD_PKT_EN		(1 << 8)
#define DSIM_CMD_CONFIG_MULTI_PKT_CNT(_x)		((_x) << 0)
#define DSIM_CMD_CONFIG_MULTI_PKT_CNT_MASK		(0x3f << 0)

/* TE based command register*/
#define DSIM_CMD_TE_CTRL0				(0x84)
#define DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP(_x)		((_x) << 0)
#define DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP_MASK		(0xffff << 0)

/* TE based command register*/
#define DSIM_CMD_TE_CTRL1				(0x88)
#define DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON(_x)	((_x) << 16)
#define DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON_MASK	(0xffff << 16)
#define DSIM_CMD_TE_CTRL1_TIME_TE_TOUT(_x)		((_x) << 0)
#define DSIM_CMD_TE_CTRL1_TIME_TE_TOUT_MASK		(0xffff << 0)

/*Command Mode Status register*/
#define DSIM_CMD_STATUS					(0x8c)
#define	DSIM_CMD_STATUS_ABNORMAL_CAUSE_ST_GET(x)	(((x) >> 0) & 0xff)

#define DSIM_VIDEO_TIMER				(0x90)
#define DSIM_VIDEO_TIMER_COMPENSATE(_x)			((_x) << 8)
#define DSIM_VIDEO_TIMER_COMPENSATE_MASK		(0xffffff << 8)
#define DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL(_x)		((_x) << 1)
#define DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL_MASK		(0x3 << 1)
#define DSIM_VIDEO_TIMER_SYNC_MODE			(1 << 0)

/*BIST generation register*/
#define	DSIM_BIST_CTRL0					(0x94)
#define	DSIM_BIST_CTRL0_BIST_TE_INTERVAL(_x)		((_x) << 8)
#define	DSIM_BIST_CTRL0_BIST_TE_INTERVAL_MASK		(0xffffff << 8)
#define	DSIM_BIST_CTRL0_BIST_PTRN_MOVE_EN		(1 << 4)
#define	DSIM_BIST_CTRL0_BIST_PTRN_MODE(_x)		((_x) << 1)
#define	DSIM_BIST_CTRL0_BIST_PTRN_MODE_MASK		(0x7 << 1)
#define	DSIM_BIST_CTRL0_BIST_EN				(1 << 0)

/*BIST generation register*/
#define	DSIM_BIST_CTRL1					(0x98)
#define	DSIM_BIST_CTRL1_BIST_PTRN_PRBS7_SEED(_x)	((_x) << 24)
#define	DSIM_BIST_CTRL1_BIST_PTRN_PRBS7_SEED_MASK	(0x7f << 24)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_R(_x)		((_x) << 16)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_R_MASK		(0XFF << 16)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_G(_x)		((_x) << 8)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_G_MASK		(0xFF << 8)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_B(_x)		((_x) << 0)
#define	DSIM_BIST_CTRL1_BIST_PTRN_USER_B_MASK		(0xFF << 0)

/*DSIM to CSI loopback register*/
#define	DSIM_CSIS_LB					(0x9C)
#define DSIM_CSIS_LB_1BYTEPPI_MODE			(1 << 9)
#define	DSIM_CSIS_LB_CSIS_LB_EN				(1 << 8)
#define DSIM_CSIS_LB_CSIS_PH(_x)			((_x) << 0)
#define DSIM_CSIS_LB_CSIS_PH_MASK			(0xff << 0)

/* PLL control register */
#define DSIM_PLLCTRL					(0xa0)
#define DSIM_PLLCTRL_DPDN_SWAP_CLK			(1 << 25)
#define DSIM_PLLCTRL_DPDN_SWAP_DATA			(1 << 24)
#define DSIM_PLLCTRL_PLL_EN				(1 << 23)
#define DSIM_PLLCTRL_PMS_MASK				(0x7ffff << 0)

/* M_PLL CTR1 register*/
#define DSIM_PLL_CTRL1					(0xa4)
#define DSIM_PLL_CTRL1_M_PLL_CTRL1			(0xffffffff << 0)

/* M_PLL CTR1 register*/
#define DSIM_PLL_CTRL2					(0xa8)
#define DSIM_PLL_CTRL2_M_PLL_CTRL2			(0xffffffff << 0)

/* PLL timer register */
#define DSIM_PLLTMR					(0xac)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_B1					(0xb0)
#define DSIM_PHYCTRL_B1_B_DPHYCTL(_x)			((_x) << 0)
#define DSIM_PHYCTRL_B1_B_DPHYCTL_MASK			(0x3ff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_B2					(0xb4)
#define DSIM_PHYCTRL_B2_B_DPHY_CTRL2			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_B3					(0xb8)
#define DSIM_PHYCTRL_B3_B_DPHY_CTRL3			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_B4					(0xbc)
#define DSIM_PHYCTRL_B4_B_DPHY_CTRL4			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_M1					(0xc0)
#define DSIM_PHYCTRL_M1_M_DPHY_CTRL1			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_M2					(0xc4)
#define DSIM_PHYCTRL_M2_M_DPHY_CTRL2			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_M3					(0xc8)
#define DSIM_PHYCTRL_M3_M_DPHY_CTRL3			(0xffffffff << 0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL_M4					(0xcc)
#define DSIM_PHYCTRL_M4_M_DPHY_CTRL4			(0xffffffff << 0)

/* D-PHY Master global operating timing register */
#define DSIM_PHY_TIMING					(0xd0)
#define DSIM_PHY_TIMING_M_TLPX_CTL(_x)			((_x) << 8)
#define DSIM_PHY_TIMING_M_TLPX_CTL_MASK			(0xff << 8)
#define DSIM_PHY_TIMING_M_THSEXIT_CTL(_x)		((_x) << 0)
#define DSIM_PHY_TIMING_M_THSEXIT_CTL_MASK		(0xff << 0)

#define DSIM_PHY_TIMING1				(0xd4)
#define DSIM_PHY_TIMING1_M_TCLKPRPR_CTL(_x)		((_x) << 24)
#define DSIM_PHY_TIMING1_M_TCLKPRPR_CTL_MASK		(0xff << 24)
#define DSIM_PHY_TIMING1_M_TCLKZERO_CTL(_x)		((_x) << 16)
#define DSIM_PHY_TIMING1_M_TCLKZERO_CTL_MASK		(0xff << 16)
#define DSIM_PHY_TIMING1_M_TCLKPOST_CTL(_x)		((_x) << 8)
#define DSIM_PHY_TIMING1_M_TCLKPOST_CTL_MASK		(0xff << 8)
#define DSIM_PHY_TIMING1_M_TCLKTRAIL_CTL(_x)		((_x) << 0)
#define DSIM_PHY_TIMING1_M_TCLKTRAIL_CTL_MASK		(0xff << 0)

#define DSIM_PHY_TIMING2				(0xd8)
#define DSIM_PHY_TIMING2_M_THSPRPR_CTL(_x)		((_x) << 16)
#define DSIM_PHY_TIMING2_M_THSPRPR_CTL_MASK		(0xff << 16)
#define DSIM_PHY_TIMING2_M_THSZERO_CTL(_x)		((_x) << 8)
#define DSIM_PHY_TIMING2_M_THSZERO_CTL_MASK		(0xff << 8)
#define DSIM_PHY_TIMING2_M_THSTRAIL_CTL(_x)		((_x) << 0)
#define DSIM_PHY_TIMING2_M_THSTRAIL_CTL_MASK		(0xff << 0)

/* IF CRC registers */
#define DSIM_IF_CRC_CTRL0				(0xdc)
#define DSIM_IF_CRC_FAIL				(1 << 16)
#define DSIM_IF_CRC_PASS				(1 << 12)
#define DSIM_IF_CRC_VALID				(1 << 8)
#define DSIM_IF_CRC_CMP_MODE				(1 << 4)
#define DSIM_IF_CRC_CLEAR				(1 << 1)
#define DSIM_IF_CRC_EN					(1 << 0)

#define DSIM_IF_CRC_CTRL1				(0xe0)
#define DSIM_IF_CRC_REF_R(_x)				((_x) << 16)
#define	DSIM_IF_CRC_RESULT_R_MASK			(0xffff << 0)
#define DSIM_IF_CRC_RESULT_R_GET(x)			(((x) >> 0) & 0xffff)

#define DSIM_IF_CRC_CTRL2				(0xe4)
#define DSIM_IF_CRC_REF_G(_x)				((_x) << 16)
#define	DSIM_IF_CRC_RESULT_G_MASK			(0xffff << 0)
#define DSIM_IF_CRC_RESULT_G_GET(x)			(((x) >> 0) & 0xffff)

#define DSIM_IF_CRC_CTRL3				(0xe8)
#define DSIM_IF_CRC_REF_B(_x)				((_x) << 16)
#define	DSIM_IF_CRC_RESULT_B_MASK			(0xffff << 0)
#define DSIM_IF_CRC_RESULT_B_GET(x)			(((x) >> 0) & 0xffff)

/* SA CRC registers */
#define DSIM_SA_CRC_CTRL0				(0xec)
#define DSIM_SA_CRC_FAIL				(1 << 16)
#define DSIM_SA_CRC_PASS				(1 << 12)
#define DSIM_SA_CRC_VALID				(1 << 8)
#define DSIM_SA_CRC_CMP_MODE				(1 << 4)
#define DSIM_SA_CRC_CLEAR				(1 << 1)
#define DSIM_SA_CRC_EN					(1 << 0)

#define DSIM_SA_CRC_CTRL1				(0xf0)
#define DSIM_SA_CRC_REF_LN0(_x)				((_x) << 16)
#define	DSIM_SA_CRC_RESULT_LN0_MASK			(0xffff << 0)
#define DSIM_SA_CRC_RESULT_LN0_GET(x)			(((x) >> 0) & 0xffff)

#define DSIM_SA_CRC_CTRL2				(0xf4)
#define DSIM_SA_CRC_REF_LN1(_x)				((_x) << 16)
#define	DSIM_SA_CRC_RESULT_LN1_MASK			(0xffff << 0)
#define DSIM_SA_CRC_RESULT_LN1_GET(x)			(((x) >> 0) & 0xffff)

#define DSIM_SA_CRC_CTRL3				(0xf8)
#define DSIM_SA_CRC_REF_LN2(_x)				((_x) << 16)
#define	DSIM_SA_CRC_RESULT_LN2_MASK			(0xffff << 0)
#define DSIM_SA_CRC_RESULT_LN2_GET(x)			(((x) >> 0) & 0xffff)

#define DSIM_SA_CRC_CTRL4				(0xfc)
#define DSIM_SA_CRC_REF_LN3(_x)				((_x) << 16)
#define	DSIM_SA_CRC_RESULT_LN3_MASK			(0xffff << 0)
#define DSIM_SA_CRC_RESULT_LN3_GET(x)			(((x) >> 0) & 0xffff)

#endif /* _REGS_DSIM_H */
