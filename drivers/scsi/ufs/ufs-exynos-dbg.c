/*
 * UFS debugging functions for Exynos specific extensions
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */

#include <linux/clk.h>
#include <linux/smc.h>
#include <linux/debug-snapshot.h>

#include "ufshcd.h"
#include "unipro.h"
#include "mphy.h"
#include "ufs-exynos.h"

static struct exynos_ufs_sfr_log ufs_log_sfr[] = {
	{"STD HCI SFR"			,	LOG_STD_HCI_SFR,		0},

	{"INTERRUPT STATUS"		,	REG_INTERRUPT_STATUS,		0},
	{"INTERRUPT ENABLE"		,	REG_INTERRUPT_ENABLE,		0},
	{"CONTROLLER STATUS"		,	REG_CONTROLLER_STATUS,		0},
	{"CONTROLLER ENABLE"		,	REG_CONTROLLER_ENABLE,		0},
	{"UIC ERR PHY ADAPTER LAYER"	,	REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER,		0},
	{"UIC ERR DATA LINK LAYER"	,	REG_UIC_ERROR_CODE_DATA_LINK_LAYER,		0},
	{"UIC ERR NETWORK LATER"	,	REG_UIC_ERROR_CODE_NETWORK_LAYER,		0},
	{"UIC ERR TRANSPORT LAYER"	,	REG_UIC_ERROR_CODE_TRANSPORT_LAYER,		0},
	{"UIC ERR DME"			,	REG_UIC_ERROR_CODE_DME,		0},
	{"UTP TRANSF REQ INT AGG CNTRL"	,	REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL,		0},
	{"UTP TRANSF REQ LIST BASE L"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_L,		0},
	{"UTP TRANSF REQ LIST BASE H"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_H,		0},
	{"UTP TRANSF REQ DOOR BELL"	,	REG_UTP_TRANSFER_REQ_DOOR_BELL,		0},
	{"UTP TRANSF REQ LIST CLEAR"	,	REG_UTP_TRANSFER_REQ_LIST_CLEAR,		0},
	{"UTP TRANSF REQ LIST RUN STOP"	,	REG_UTP_TRANSFER_REQ_LIST_RUN_STOP,		0},
	{"UTP TRANSF REQ LIST CNR"	,	REG_UTP_TRANSFER_REQ_LIST_CNR,		0},
	{"UTP TASK REQ LIST BASE L"	,	REG_UTP_TASK_REQ_LIST_BASE_L,		0},
	{"UTP TASK REQ LIST BASE H"	,	REG_UTP_TASK_REQ_LIST_BASE_H,		0},
	{"UTP TASK REQ DOOR BELL"	,	REG_UTP_TASK_REQ_DOOR_BELL,		0},
	{"UTP TASK REQ LIST CLEAR"	,	REG_UTP_TASK_REQ_LIST_CLEAR,		0},
	{"UTP TASK REQ LIST RUN STOP"	,	REG_UTP_TASK_REQ_LIST_RUN_STOP,		0},
	{"UIC COMMAND"			,	REG_UIC_COMMAND,		0},
	{"UIC COMMAND ARG1"		,	REG_UIC_COMMAND_ARG_1,		0},
	{"UIC COMMAND ARG2"		,	REG_UIC_COMMAND_ARG_2,		0},
	{"UIC COMMAND ARG3"		,	REG_UIC_COMMAND_ARG_3,		0},
	{"CCAP"				,	REG_CRYPTO_CAPABILITY,		0},

	{"VS HCI SFR"			,	LOG_VS_HCI_SFR,			0},

	{"TXPRDT ENTRY SIZE"		,	HCI_TXPRDT_ENTRY_SIZE,		0},
	{"RXPRDT ENTRY SIZE"		,	HCI_RXPRDT_ENTRY_SIZE,		0},
	{"TO CNT DIV VAL"		,	HCI_TO_CNT_DIV_VAL,		0},
	{"1US TO CNT VAL"		,	HCI_1US_TO_CNT_VAL,		0},
	{"INVALID UPIU CTRL"		,	HCI_INVALID_UPIU_CTRL,		0},
	{"INVALID UPIU BADDR"		,	HCI_INVALID_UPIU_BADDR,		0},
	{"INVALID UPIU UBADDR"		,	HCI_INVALID_UPIU_UBADDR,		0},
	{"INVALID UTMR OFFSET ADDR"	,	HCI_INVALID_UTMR_OFFSET_ADDR,		0},
	{"INVALID UTR OFFSET ADDR"	,	HCI_INVALID_UTR_OFFSET_ADDR,		0},
	{"INVALID DIN OFFSET ADDR"	,	HCI_INVALID_DIN_OFFSET_ADDR,		0},
	{"VENDOR SPECIFIC IS"		,	HCI_VENDOR_SPECIFIC_IS,		0},
	{"VENDOR SPECIFIC IE"		,	HCI_VENDOR_SPECIFIC_IE,		0},
	{"UTRL NEXUS TYPE"		,	HCI_UTRL_NEXUS_TYPE,		0},
	{"UTMRL NEXUS TYPE"		,	HCI_UTMRL_NEXUS_TYPE,		0},
	{"SW RST"			,	HCI_SW_RST,		0},
	{"RX UPIU MATCH ERROR CODE"	,	HCI_RX_UPIU_MATCH_ERROR_CODE,		0},
	{"DATA REORDER"			,	HCI_DATA_REORDER,		0},
	{"AXIDMA RWDATA BURST LEN"	,	HCI_AXIDMA_RWDATA_BURST_LEN,		0},
	{"WRITE DMA CTRL"		,	HCI_WRITE_DMA_CTRL,		0},
	{"V2P1 CTRL"			,	HCI_UFSHCI_V2P1_CTRL,	 	0},
	{"CLKSTOP CTRL"			,	HCI_CLKSTOP_CTRL,		0},
	{"FORCE HCS"			,	HCI_FORCE_HCS,		0},
	{"FSM MONITOR"			,	HCI_FSM_MONITOR,		0},
	{"DMA0 MONITOR STATE"		,	HCI_DMA0_MONITOR_STATE,		0},
	{"DMA0 MONITOR CNT"		,	HCI_DMA0_MONITOR_CNT,		0},
	{"DMA1 MONITOR STATE"		,	HCI_DMA1_MONITOR_STATE,		0},
	{"DMA1 MONITOR CNT"		,	HCI_DMA1_MONITOR_CNT,		0},
	{"AXI DMA IF CTRL"		,	HCI_UFS_AXI_DMA_IF_CTRL,	0},
	{"UFS ACG DISABLE"	 	,	HCI_UFS_ACG_DISABLE,		0},
	{"MPHY REFCLK SEL"		,	HCI_MPHY_REFCLK_SEL,		0},
	{"SMU ABORT MATCH INFO"		,	HCI_SMU_ABORT_MATCH_INFO,	0},
	{"DBR DUPLICATION INFO"		,	HCI_DBR_DUPLICATION_INFO,	0},
	{"DBR TIMER CONFIG"		,	HCI_DBR_TIMER_CONFIG,		0},
	{"DBR TIMER ENABLE"		,	HCI_DBR_TIMER_ENABLE,		0},
	{"DBR TIMER STATUS"		,	HCI_DBR_TIMER_STATUS,		0},
	{"UTRL DBR 3 0 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_3_0_TIMER_EXPIRED_VALUE,		0},
	{"UTRL DBR 7 4 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_7_4_TIMER_EXPIRED_VALUE,	0},
	{"UTRL DBR 11 8 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_11_8_TIMER_EXPIRED_VALUE,	0},
	{"UTRL DBR 15 12 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_15_12_TIMER_EXPIRED_VALUE,		0},
	{"UTMRL DBR 3 0 TIMER EXPIRED VALUE"		,	HCI_UTMRL_DBR_3_0_TIMER_EXPIRED_VALUE,		0},

	{"FMP SFR"			,	LOG_FMP_SFR,			0},

	{"UFSPRCTRL"			,	UFSPRCTRL,			0},
	{"UFSPRSTAT"			,	UFSPRSTAT,			0},
	{"UFSPRSECURITY"		,	UFSPRSECURITY,			0},
	{"UFSPWCTRL"			,	UFSPWCTRL,			0},
	{"UFSPWSTAT"			,	UFSPWSTAT,			0},
	{"UFSPSBEGIN0"			,	UFSPSBEGIN0,			0},
	{"UFSPSEND0"			,	UFSPSEND0,			0},
	{"UFSPSLUN0"			,	UFSPSLUN0,			0},
	{"UFSPSCTRL0"			,	UFSPSCTRL0,			0},
	{"UFSPSBEGIN1"			,	UFSPSBEGIN1,			0},
	{"UFSPSEND1"			,	UFSPSEND1,			0},
	{"UFSPSLUN1"			,	UFSPSLUN1,			0},
	{"UFSPSCTRL1"			,	UFSPSCTRL1,			0},
	{"UFSPSBEGIN2"			,	UFSPSBEGIN2,			0},
	{"UFSPSEND2"			,	UFSPSEND2,			0},
	{"UFSPSLUN2"			,	UFSPSLUN2,			0},
	{"UFSPSCTRL2"			,	UFSPSCTRL2,			0},
	{"UFSPSBEGIN3"			,	UFSPSBEGIN3,			0},
	{"UFSPSEND3"			,	UFSPSEND3,			0},
	{"UFSPSLUN3"			,	UFSPSLUN3,			0},
	{"UFSPSCTRL3"			,	UFSPSCTRL3,			0},
	{"UFSPSBEGIN4"			,	UFSPSBEGIN4,			0},
	{"UFSPSLUN4"			,	UFSPSLUN4,			0},
	{"UFSPSCTRL4"			,	UFSPSCTRL4,			0},
	{"UFSPSBEGIN5"			,	UFSPSBEGIN5,			0},
	{"UFSPSEND5"			,	UFSPSEND5,			0},
	{"UFSPSLUN5"			,	UFSPSLUN5,			0},
	{"UFSPSCTRL5"			,	UFSPSCTRL5,			0},
	{"UFSPSBEGIN6"			,	UFSPSBEGIN6,			0},
	{"UFSPSEND6"			,	UFSPSEND6,			0},
	{"UFSPSLUN6"			,	UFSPSLUN6,			0},
	{"UFSPSCTRL6"			,	UFSPSCTRL6,			0},
	{"UFSPSBEGIN7"			,	UFSPSBEGIN7,			0},
	{"UFSPSEND7"			,	UFSPSEND7,			0},
	{"UFSPSLUN7"			,	UFSPSLUN7,			0},
	{"UFSPSCTRL7"			,	UFSPSCTRL7,			0},

	{"UNIPRO SFR"			,	LOG_UNIPRO_SFR,			0},

	{"DME_LINKSTARTUP_CNF_RESULT"	,	UNIP_DME_LINKSTARTUP_CNF_RESULT	,	0},
	{"DME_HIBERN8_ENTER_CNF_RESULT"	,	UNIP_DME_HIBERN8_ENTER_CNF_RESULT,	0},
	{"DME_HIBERN8_ENTER_IND_RESULT"	,	UNIP_DME_HIBERN8_ENTER_IND_RESULT,	0},
	{"DME_HIBERN8_EXIT_CNF_RESULT"	,	UNIP_DME_HIBERN8_EXIT_CNF_RESULT,	0},
	{"DME_HIBERN8_EXIT_IND_RESULT"	,	UNIP_DME_HIBERN8_EXIT_IND_RESULT,	0},
	{"DME_PWR_IND_RESULT"		,	UNIP_DME_PWR_IND_RESULT	,	0},
	{"DME_INTR_STATUS_LSB"		,	UNIP_DME_INTR_STATUS_LSB,	0},
	{"DME_INTR_STATUS_MSB"		,	UNIP_DME_INTR_STATUS_MSB,	0},
	{"DME_INTR_ERROR_CODE"		,	UNIP_DME_INTR_ERROR_CODE,	0},
	{"DME_DISCARD_PORT_ID"		,	UNIP_DME_DISCARD_PORT_ID,	0},
	{"DME_DBG_OPTION_SUITE"		,	UNIP_DME_DBG_OPTION_SUITE,	0},
	{"DME_DBG_CTRL_FSM"		,	UNIP_DME_DBG_CTRL_FSM,	0},
	{"DME_DBG_FLAG_STATUS"		,	UNIP_DME_DBG_FLAG_STATUS,	0},
	{"DME_DBG_LINKCFG_FSM"		,	UNIP_DME_DBG_LINKCFG_FSM,	0},

	{"PMA SFR"			,	LOG_PMA_SFR,			0},

	{"COMN 0x2f",		(0x00BC),		0},
	{"TRSV_L0 0x4b",	(0x01EC),		0},
	{"TRSV_L0 0x4f",	(0x01FC),		0},
	{"TRSV_L1 0x4b",	(0x032C),		0},
	{"TRSV_L1 0x4f",	(0x033C),		0},
	{"0x74", 	0x74, 		0},
	{"0x110",	0x110,		0},
	{"0x134",	0x134,		0},
	{"0x16C",	0x16C,		0},
	{"0x178",	0x178,		0},
	{"0x1B0",	0x1B0,		0},
	{"0xE0", 	0xE0, 		0},
	{"0x164",	0x164,		0},
	{"0x8C", 	0x8C, 		0},
	{"0xC8",	0xC8,		0},
	{"0xF0",	0xF0,		0},
	{"0x120",	0x120,		0},
	{},
};

static struct exynos_ufs_attr_log ufs_log_attr[] = {
	/* PA Standard */
	{UIC_ARG_MIB(0x1520),	0, 0},
	{UIC_ARG_MIB(0x1540),	0, 0},
	{UIC_ARG_MIB(0x1543),	0, 0},
	{UIC_ARG_MIB(0x155C),	0, 0},
	{UIC_ARG_MIB(0x155D),	0, 0},
	{UIC_ARG_MIB(0x155E),	0, 0},
	{UIC_ARG_MIB(0x155F),	0, 0},
	{UIC_ARG_MIB(0x1560),	0, 0},
	{UIC_ARG_MIB(0x1561),	0, 0},
	{UIC_ARG_MIB(0x1564),	0, 0},
	{UIC_ARG_MIB(0x1567),	0, 0},
	{UIC_ARG_MIB(0x1568),	0, 0},
	{UIC_ARG_MIB(0x1569),	0, 0},
	{UIC_ARG_MIB(0x156A),	0, 0},
	{UIC_ARG_MIB(0x1571),	0, 0},
	{UIC_ARG_MIB(0x1580),	0, 0},
	{UIC_ARG_MIB(0x1581),	0, 0},
	{UIC_ARG_MIB(0x1582),	0, 0},
	{UIC_ARG_MIB(0x1583),	0, 0},
	{UIC_ARG_MIB(0x1584),	0, 0},
	{UIC_ARG_MIB(0x1585),	0, 0},
	{UIC_ARG_MIB(0x1590),	0, 0},
	{UIC_ARG_MIB(0x1591),	0, 0},
	{UIC_ARG_MIB(0x15A1),	0, 0},
	{UIC_ARG_MIB(0x15A2),	0, 0},
	{UIC_ARG_MIB(0x15A3),	0, 0},
	{UIC_ARG_MIB(0x15A4),	0, 0},
	{UIC_ARG_MIB(0x15A7),	0, 0},
	{UIC_ARG_MIB(0x15A8),	0, 0},
	{UIC_ARG_MIB(0x15C0),	0, 0},
	{UIC_ARG_MIB(0x15C1),	0, 0},

	/* PA Debug */
	{UIC_ARG_MIB(0x9514),	0, 0},
	{UIC_ARG_MIB(0x9536),	0, 0},
	{UIC_ARG_MIB(0x9556),	0, 0},
	{UIC_ARG_MIB(0x9564),	0, 0},
	{UIC_ARG_MIB(0x9566),	0, 0},
	{UIC_ARG_MIB(0x9567),	0, 0},
	{UIC_ARG_MIB(0x9568),	0, 0},
	{UIC_ARG_MIB(0x956A),	0, 0},
	{UIC_ARG_MIB(0x9595),	0, 0},
	{UIC_ARG_MIB(0x9596),	0, 0},
	{UIC_ARG_MIB(0x9597),	0, 0},
	/* DL Standard */
	{UIC_ARG_MIB(0x2046),	0, 0},
	{UIC_ARG_MIB(0x2047),	0, 0},
	{UIC_ARG_MIB(0x2066),	0, 0},
	{UIC_ARG_MIB(0x2067),	0, 0},

	/* DL Debug */
	{UIC_ARG_MIB(0xA000),	0, 0},
	{UIC_ARG_MIB(0xA005),	0, 0},
	{UIC_ARG_MIB(0xA007),	0, 0},
	{UIC_ARG_MIB(0xA010),	0, 0},
	{UIC_ARG_MIB(0xA011),	0, 0},
	{UIC_ARG_MIB(0xA020),	0, 0},
	{UIC_ARG_MIB(0xA021),	0, 0},
	{UIC_ARG_MIB(0xA022),	0, 0},
	{UIC_ARG_MIB(0xA023),	0, 0},
	{UIC_ARG_MIB(0xA024),	0, 0},
	{UIC_ARG_MIB(0xA025),	0, 0},
	{UIC_ARG_MIB(0xA026),	0, 0},
	{UIC_ARG_MIB(0xA027),	0, 0},
	{UIC_ARG_MIB(0xA028),	0, 0},
	{UIC_ARG_MIB(0xA029),	0, 0},
	{UIC_ARG_MIB(0xA02A),	0, 0},
	{UIC_ARG_MIB(0xA02B),	0, 0},
	{UIC_ARG_MIB(0xA100),	0, 0},
	{UIC_ARG_MIB(0xA101),	0, 0},
	{UIC_ARG_MIB(0xA102),	0, 0},
	{UIC_ARG_MIB(0xA103),	0, 0},
	{UIC_ARG_MIB(0xA114),	0, 0},
	{UIC_ARG_MIB(0xA115),	0, 0},
	{UIC_ARG_MIB(0xA116),	0, 0},
	{UIC_ARG_MIB(0xA120),	0, 0},
	{UIC_ARG_MIB(0xA121),	0, 0},
	{UIC_ARG_MIB(0xA122),	0, 0},
	/* NL Standard */
	{UIC_ARG_MIB(0x3000),	0, 0},
	{UIC_ARG_MIB(0x3001),	0, 0},

	/* NL Debug */
	{UIC_ARG_MIB(0xB010),	0, 0},
	{UIC_ARG_MIB(0xB011),	0, 0},
	/* TL Standard */
	{UIC_ARG_MIB(0x4020),	0, 0},
	{UIC_ARG_MIB(0x4021),	0, 0},
	{UIC_ARG_MIB(0x4022),	0, 0},
	{UIC_ARG_MIB(0x4023),	0, 0},
	{UIC_ARG_MIB(0x4025),	0, 0},
	{UIC_ARG_MIB(0x402B),	0, 0},

	/* TL Debug */
	{UIC_ARG_MIB(0xC001),	0, 0},
	{UIC_ARG_MIB(0xC024),	0, 0},
	{UIC_ARG_MIB(0xC025),	0, 0},
	{UIC_ARG_MIB(0xC026),	0, 0},
	/* MPHY PCS Lane 0*/
	{UIC_ARG_MIB_SEL(0x0021, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0022, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0023, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0024, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0028, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0029, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002A, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002B, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002C, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002D, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0033, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0035, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0036, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0041, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB(0x8F),	0, 0},

	{UIC_ARG_MIB_SEL(0x0F, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x65, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x69, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x21, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x22, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x84, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x35, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x73, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x41, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x42, RX_LANE_0+0),	0, 0},

	{UIC_ARG_MIB_SEL(0x00A1, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A2, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A3, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A4, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A7, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00C1, RX_LANE_0+0),	0, 0},
	{},
};

static struct exynos_ufs_sfr_log ufs_show_sfr[] = {
	{"STD HCI SFR"			,	LOG_STD_HCI_SFR,		0},

	{"INTERRUPT STATUS"		,	REG_INTERRUPT_STATUS,		0},
	{"INTERRUPT ENABLE"		,	REG_INTERRUPT_ENABLE,		0},
	{"CONTROLLER STATUS"		,	REG_CONTROLLER_STATUS,		0},
	{"CONTROLLER ENABLE"		,	REG_CONTROLLER_ENABLE,		0},
	{"UIC ERR PHY ADAPTER LAYER"	,	REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER,		0},
	{"UIC ERR DATA LINK LAYER"	,	REG_UIC_ERROR_CODE_DATA_LINK_LAYER,		0},
	{"UIC ERR NETWORK LATER"	,	REG_UIC_ERROR_CODE_NETWORK_LAYER,		0},
	{"UIC ERR TRANSPORT LAYER"	,	REG_UIC_ERROR_CODE_TRANSPORT_LAYER,		0},
	{"UIC ERR DME"			,	REG_UIC_ERROR_CODE_DME,		0},
	{"UTP TRANSF REQ INT AGG CNTRL"	,	REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL,		0},
	{"UTP TRANSF REQ LIST BASE L"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_L,		0},
	{"UTP TRANSF REQ LIST BASE H"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_H,		0},
	{"UTP TRANSF REQ DOOR BELL"	,	REG_UTP_TRANSFER_REQ_DOOR_BELL,		0},
	{"UTP TRANSF REQ LIST CLEAR"	,	REG_UTP_TRANSFER_REQ_LIST_CLEAR,		0},
	{"UTP TRANSF REQ LIST RUN STOP"	,	REG_UTP_TRANSFER_REQ_LIST_RUN_STOP,		0},
	{"UTP TRANSF REQ LIST CNR"	,	REG_UTP_TRANSFER_REQ_LIST_CNR,		0},
	{"UTP TASK REQ LIST BASE L"	,	REG_UTP_TASK_REQ_LIST_BASE_L,		0},
	{"UTP TASK REQ LIST BASE H"	,	REG_UTP_TASK_REQ_LIST_BASE_H,		0},
	{"UTP TASK REQ DOOR BELL"	,	REG_UTP_TASK_REQ_DOOR_BELL,		0},
	{"UTP TASK REQ LIST CLEAR"	,	REG_UTP_TASK_REQ_LIST_CLEAR,		0},
	{"UTP TASK REQ LIST RUN STOP"	,	REG_UTP_TASK_REQ_LIST_RUN_STOP,		0},
	{"UIC COMMAND"			,	REG_UIC_COMMAND,		0},
	{"UIC COMMAND ARG1"		,	REG_UIC_COMMAND_ARG_1,		0},
	{"UIC COMMAND ARG2"		,	REG_UIC_COMMAND_ARG_2,		0},
	{"UIC COMMAND ARG3"		,	REG_UIC_COMMAND_ARG_3,		0},
	{"CCAP"				,	REG_CRYPTO_CAPABILITY,		0},

	{"VS HCI SFR"			,	LOG_VS_HCI_SFR,			0},

	{"TXPRDT ENTRY SIZE"		,	HCI_TXPRDT_ENTRY_SIZE,		0},
	{"RXPRDT ENTRY SIZE"		,	HCI_RXPRDT_ENTRY_SIZE,		0},
	{"TO CNT DIV VAL"		,	HCI_TO_CNT_DIV_VAL,		0},
	{"1US TO CNT VAL"		,	HCI_1US_TO_CNT_VAL,		0},
	{"INVALID UPIU CTRL"		,	HCI_INVALID_UPIU_CTRL,		0},
	{"INVALID UPIU BADDR"		,	HCI_INVALID_UPIU_BADDR,		0},
	{"INVALID UPIU UBADDR"		,	HCI_INVALID_UPIU_UBADDR,		0},
	{"INVALID UTMR OFFSET ADDR"	,	HCI_INVALID_UTMR_OFFSET_ADDR,		0},
	{"INVALID UTR OFFSET ADDR"	,	HCI_INVALID_UTR_OFFSET_ADDR,		0},
	{"INVALID DIN OFFSET ADDR"	,	HCI_INVALID_DIN_OFFSET_ADDR,		0},
	{"VENDOR SPECIFIC IS"		,	HCI_VENDOR_SPECIFIC_IS,		0},
	{"VENDOR SPECIFIC IE"		,	HCI_VENDOR_SPECIFIC_IE,		0},
	{"UTRL NEXUS TYPE"		,	HCI_UTRL_NEXUS_TYPE,		0},
	{"UTMRL NEXUS TYPE"		,	HCI_UTMRL_NEXUS_TYPE,		0},
	{"SW RST"			,	HCI_SW_RST,		0},
	{"RX UPIU MATCH ERROR CODE"	,	HCI_RX_UPIU_MATCH_ERROR_CODE,		0},
	{"DATA REORDER"			,	HCI_DATA_REORDER,		0},
	{"AXIDMA RWDATA BURST LEN"	,	HCI_AXIDMA_RWDATA_BURST_LEN,		0},
	{"WRITE DMA CTRL"		,	HCI_WRITE_DMA_CTRL,		0},
	{"V2P1 CTRL"			,	HCI_UFSHCI_V2P1_CTRL,	 	0},
	{"CLKSTOP CTRL"			,	HCI_CLKSTOP_CTRL,		0},
	{"FORCE HCS"			,	HCI_FORCE_HCS,		0},
	{"FSM MONITOR"			,	HCI_FSM_MONITOR,		0},
	{"DMA0 MONITOR STATE"		,	HCI_DMA0_MONITOR_STATE,		0},
	{"DMA0 MONITOR CNT"		,	HCI_DMA0_MONITOR_CNT,		0},
	{"DMA1 MONITOR STATE"		,	HCI_DMA1_MONITOR_STATE,		0},
	{"DMA1 MONITOR CNT"		,	HCI_DMA1_MONITOR_CNT,		0},
	{"AXI DMA IF CTRL"		,	HCI_UFS_AXI_DMA_IF_CTRL,	0},
	{"UFS ACG DISABLE"	 	,	HCI_UFS_ACG_DISABLE,		0},
	{"MPHY REFCLK SEL"		,	HCI_MPHY_REFCLK_SEL,		0},
	{"SMU ABORT MATCH INFO"		,	HCI_SMU_ABORT_MATCH_INFO,	0},
	{"DBR DUPLICATION INFO"		,	HCI_DBR_DUPLICATION_INFO,	0},
	{"DBR TIMER CONFIG"		,	HCI_DBR_TIMER_CONFIG,		0},
	{"DBR TIMER ENABLE"		,	HCI_DBR_TIMER_ENABLE,		0},
	{"DBR TIMER STATUS"		,	HCI_DBR_TIMER_STATUS,		0},
	{"UTRL DBR 3 0 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_3_0_TIMER_EXPIRED_VALUE,		0},
	{"UTRL DBR 7 4 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_7_4_TIMER_EXPIRED_VALUE,	0},
	{"UTRL DBR 11 8 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_11_8_TIMER_EXPIRED_VALUE,	0},
	{"UTRL DBR 15 12 TIMER EXPIRED VALUE"		,	HCI_UTRL_DBR_15_12_TIMER_EXPIRED_VALUE,		0},
	{"UTMRL DBR 3 0 TIMER EXPIRED VALUE"		,	HCI_UTMRL_DBR_3_0_TIMER_EXPIRED_VALUE,		0},

	{"FMP SFR"			,	LOG_FMP_SFR,			0},

	{"UFSPRCTRL"			,	UFSPRCTRL,			0},
	{"UFSPRSTAT"			,	UFSPRSTAT,			0},
	{"UFSPRSECURITY"		,	UFSPRSECURITY,			0},
	{"UFSPWCTRL"			,	UFSPWCTRL,			0},
	{"UFSPWSTAT"			,	UFSPWSTAT,			0},
	{"UFSPSBEGIN0"			,	UFSPSBEGIN0,			0},
	{"UFSPSEND0"			,	UFSPSEND0,			0},
	{"UFSPSLUN0"			,	UFSPSLUN0,			0},
	{"UFSPSCTRL0"			,	UFSPSCTRL0,			0},
	{"UFSPSBEGIN1"			,	UFSPSBEGIN1,			0},
	{"UFSPSEND1"			,	UFSPSEND1,			0},
	{"UFSPSLUN1"			,	UFSPSLUN1,			0},
	{"UFSPSCTRL1"			,	UFSPSCTRL1,			0},
	{"UFSPSBEGIN2"			,	UFSPSBEGIN2,			0},
	{"UFSPSEND2"			,	UFSPSEND2,			0},
	{"UFSPSLUN2"			,	UFSPSLUN2,			0},
	{"UFSPSCTRL2"			,	UFSPSCTRL2,			0},
	{"UFSPSBEGIN3"			,	UFSPSBEGIN3,			0},
	{"UFSPSEND3"			,	UFSPSEND3,			0},
	{"UFSPSLUN3"			,	UFSPSLUN3,			0},
	{"UFSPSCTRL3"			,	UFSPSCTRL3,			0},
	{"UFSPSBEGIN4"			,	UFSPSBEGIN4,			0},
	{"UFSPSLUN4"			,	UFSPSLUN4,			0},
	{"UFSPSCTRL4"			,	UFSPSCTRL4,			0},
	{"UFSPSBEGIN5"			,	UFSPSBEGIN5,			0},
	{"UFSPSEND5"			,	UFSPSEND5,			0},
	{"UFSPSLUN5"			,	UFSPSLUN5,			0},
	{"UFSPSCTRL5"			,	UFSPSCTRL5,			0},
	{"UFSPSBEGIN6"			,	UFSPSBEGIN6,			0},
	{"UFSPSEND6"			,	UFSPSEND6,			0},
	{"UFSPSLUN6"			,	UFSPSLUN6,			0},
	{"UFSPSCTRL6"			,	UFSPSCTRL6,			0},
	{"UFSPSBEGIN7"			,	UFSPSBEGIN7,			0},
	{"UFSPSEND7"			,	UFSPSEND7,			0},
	{"UFSPSLUN7"			,	UFSPSLUN7,			0},
	{"UFSPSCTRL7"			,	UFSPSCTRL7,			0},

	{"UNIPRO SFR"			,	LOG_UNIPRO_SFR,			0},

	{"DME_LINKSTARTUP_CNF_RESULT"	,	UNIP_DME_LINKSTARTUP_CNF_RESULT	,	0},
	{"DME_HIBERN8_ENTER_CNF_RESULT"	,	UNIP_DME_HIBERN8_ENTER_CNF_RESULT,	0},
	{"DME_HIBERN8_ENTER_IND_RESULT"	,	UNIP_DME_HIBERN8_ENTER_IND_RESULT,	0},
	{"DME_HIBERN8_EXIT_CNF_RESULT"	,	UNIP_DME_HIBERN8_EXIT_CNF_RESULT,	0},
	{"DME_HIBERN8_EXIT_IND_RESULT"	,	UNIP_DME_HIBERN8_EXIT_IND_RESULT,	0},
	{"DME_PWR_IND_RESULT"		,	UNIP_DME_PWR_IND_RESULT	,	0},
	{"DME_INTR_STATUS_LSB"		,	UNIP_DME_INTR_STATUS_LSB,	0},
	{"DME_INTR_STATUS_MSB"		,	UNIP_DME_INTR_STATUS_MSB,	0},
	{"DME_INTR_ERROR_CODE"		,	UNIP_DME_INTR_ERROR_CODE,	0},
	{"DME_DISCARD_PORT_ID"		,	UNIP_DME_DISCARD_PORT_ID,	0},
	{"DME_DBG_OPTION_SUITE"		,	UNIP_DME_DBG_OPTION_SUITE,	0},
	{"DME_DBG_CTRL_FSM"		,	UNIP_DME_DBG_CTRL_FSM,	0},
	{"DME_DBG_FLAG_STATUS"		,	UNIP_DME_DBG_FLAG_STATUS,	0},
	{"DME_DBG_LINKCFG_FSM"		,	UNIP_DME_DBG_LINKCFG_FSM,	0},

	{"PMA SFR"			,	LOG_PMA_SFR,			0},

	{"COMN 0x2f",		(0x00BC),		0},
	{"TRSV_L0 0x4b",	(0x01EC),		0},
	{"TRSV_L0 0x4f",	(0x01FC),		0},
	{"TRSV_L1 0x4b",	(0x032C),		0},
	{"TRSV_L1 0x4f",	(0x033C),		0},
	{"0x74", 	0x74, 		0},
	{"0x110",	0x110,		0},
	{"0x134",	0x134,		0},
	{"0x16C",	0x16C,		0},
	{"0x178",	0x178,		0},
	{"0x1B0",	0x1B0,		0},
	{"0xE0", 	0xE0, 		0},
	{"0x164",	0x164,		0},
	{"0x8C", 	0x8C, 		0},
	{"0xC8",	0xC8,		0},
	{"0xF0",	0xF0,		0},
	{"0x120",	0x120,		0},
	{},
};

static struct exynos_ufs_attr_log ufs_show_attr[] = {
	/* PA Standard */
	{UIC_ARG_MIB(0x1520),	0, 0},
	{UIC_ARG_MIB(0x1540),	0, 0},
	{UIC_ARG_MIB(0x1543),	0, 0},
	{UIC_ARG_MIB(0x155C),	0, 0},
	{UIC_ARG_MIB(0x155D),	0, 0},
	{UIC_ARG_MIB(0x155E),	0, 0},
	{UIC_ARG_MIB(0x155F),	0, 0},
	{UIC_ARG_MIB(0x1560),	0, 0},
	{UIC_ARG_MIB(0x1561),	0, 0},
	{UIC_ARG_MIB(0x1564),	0, 0},
	{UIC_ARG_MIB(0x1567),	0, 0},
	{UIC_ARG_MIB(0x1568),	0, 0},
	{UIC_ARG_MIB(0x1569),	0, 0},
	{UIC_ARG_MIB(0x156A),	0, 0},
	{UIC_ARG_MIB(0x1571),	0, 0},
	{UIC_ARG_MIB(0x1580),	0, 0},
	{UIC_ARG_MIB(0x1581),	0, 0},
	{UIC_ARG_MIB(0x1582),	0, 0},
	{UIC_ARG_MIB(0x1583),	0, 0},
	{UIC_ARG_MIB(0x1584),	0, 0},
	{UIC_ARG_MIB(0x1585),	0, 0},
	{UIC_ARG_MIB(0x1590),	0, 0},
	{UIC_ARG_MIB(0x1591),	0, 0},
	{UIC_ARG_MIB(0x15A1),	0, 0},
	{UIC_ARG_MIB(0x15A2),	0, 0},
	{UIC_ARG_MIB(0x15A3),	0, 0},
	{UIC_ARG_MIB(0x15A4),	0, 0},
	{UIC_ARG_MIB(0x15A7),	0, 0},
	{UIC_ARG_MIB(0x15A8),	0, 0},
	{UIC_ARG_MIB(0x15C0),	0, 0},
	{UIC_ARG_MIB(0x15C1),	0, 0},

	/* PA Debug */
	{UIC_ARG_MIB(0x9514),	0, 0},
	{UIC_ARG_MIB(0x9536),	0, 0},
	{UIC_ARG_MIB(0x9556),	0, 0},
	{UIC_ARG_MIB(0x9564),	0, 0},
	{UIC_ARG_MIB(0x9566),	0, 0},
	{UIC_ARG_MIB(0x9567),	0, 0},
	{UIC_ARG_MIB(0x9568),	0, 0},
	{UIC_ARG_MIB(0x956A),	0, 0},
	{UIC_ARG_MIB(0x9595),	0, 0},
	{UIC_ARG_MIB(0x9596),	0, 0},
	{UIC_ARG_MIB(0x9597),	0, 0},
	/* DL Standard */
	{UIC_ARG_MIB(0x2046),	0, 0},
	{UIC_ARG_MIB(0x2047),	0, 0},
	{UIC_ARG_MIB(0x2066),	0, 0},
	{UIC_ARG_MIB(0x2067),	0, 0},

	/* DL Debug */
	{UIC_ARG_MIB(0xA000),	0, 0},
	{UIC_ARG_MIB(0xA005),	0, 0},
	{UIC_ARG_MIB(0xA007),	0, 0},
	{UIC_ARG_MIB(0xA010),	0, 0},
	{UIC_ARG_MIB(0xA011),	0, 0},
	{UIC_ARG_MIB(0xA020),	0, 0},
	{UIC_ARG_MIB(0xA021),	0, 0},
	{UIC_ARG_MIB(0xA022),	0, 0},
	{UIC_ARG_MIB(0xA023),	0, 0},
	{UIC_ARG_MIB(0xA024),	0, 0},
	{UIC_ARG_MIB(0xA025),	0, 0},
	{UIC_ARG_MIB(0xA026),	0, 0},
	{UIC_ARG_MIB(0xA027),	0, 0},
	{UIC_ARG_MIB(0xA028),	0, 0},
	{UIC_ARG_MIB(0xA029),	0, 0},
	{UIC_ARG_MIB(0xA02A),	0, 0},
	{UIC_ARG_MIB(0xA02B),	0, 0},
	{UIC_ARG_MIB(0xA100),	0, 0},
	{UIC_ARG_MIB(0xA101),	0, 0},
	{UIC_ARG_MIB(0xA102),	0, 0},
	{UIC_ARG_MIB(0xA103),	0, 0},
	{UIC_ARG_MIB(0xA114),	0, 0},
	{UIC_ARG_MIB(0xA115),	0, 0},
	{UIC_ARG_MIB(0xA116),	0, 0},
	{UIC_ARG_MIB(0xA120),	0, 0},
	{UIC_ARG_MIB(0xA121),	0, 0},
	{UIC_ARG_MIB(0xA122),	0, 0},
	/* NL Standard */
	{UIC_ARG_MIB(0x3000),	0, 0},
	{UIC_ARG_MIB(0x3001),	0, 0},

	/* NL Debug */
	{UIC_ARG_MIB(0xB010),	0, 0},
	{UIC_ARG_MIB(0xB011),	0, 0},
	/* TL Standard */
	{UIC_ARG_MIB(0x4020),	0, 0},
	{UIC_ARG_MIB(0x4021),	0, 0},
	{UIC_ARG_MIB(0x4022),	0, 0},
	{UIC_ARG_MIB(0x4023),	0, 0},
	{UIC_ARG_MIB(0x4025),	0, 0},
	{UIC_ARG_MIB(0x402B),	0, 0},

	/* TL Debug */
	{UIC_ARG_MIB(0xC001),	0, 0},
	{UIC_ARG_MIB(0xC024),	0, 0},
	{UIC_ARG_MIB(0xC025),	0, 0},
	{UIC_ARG_MIB(0xC026),	0, 0},
	/* MPHY PCS Lane 0*/
	{UIC_ARG_MIB_SEL(0x0021, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0022, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0023, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0024, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0028, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0029, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002A, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002B, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002C, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x002D, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0033, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0035, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0036, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x0041, TX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB(0x8F),	0, 0},

	{UIC_ARG_MIB_SEL(0x0F, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x65, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x69, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x21, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x22, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x84, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x35, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x73, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x41, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x42, RX_LANE_0+0),	0, 0},

	{UIC_ARG_MIB_SEL(0x00A1, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A2, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A3, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A4, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00A7, RX_LANE_0+0),	0, 0},
	{UIC_ARG_MIB_SEL(0x00C1, RX_LANE_0+0),	0, 0},
	{},
};

static void exynos_ufs_get_misc(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_clk_info *clki;
	struct list_head *head = &ufs->debug.misc.clk_list_head;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk))
			clki->freq = clk_get_rate(clki->clk);
	}
//	ufs->debug.misc.isolation = readl(ufs->phy.reg_pmu);
}

static void exynos_ufs_get_sfr(struct ufs_hba *hba,
					struct exynos_ufs_sfr_log* cfg)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int sel_api = 0;

	while(cfg) {
		if (!cfg->name)
			break;

		if (cfg->offset >= LOG_STD_HCI_SFR) {
			/* Select an API to get SFRs */
			sel_api = cfg->offset;
		} else {
			/* Fetch value */
			if (sel_api == LOG_STD_HCI_SFR)
				cfg->val = ufshcd_readl(hba, cfg->offset);
			else if (sel_api == LOG_VS_HCI_SFR)
				cfg->val = hci_readl(ufs, cfg->offset);
#ifdef CONFIG_EXYNOS_SMC_LOGGING
			else if (sel_api == LOG_FMP_SFR)
				cfg->val = exynos_smc(SMC_CMD_FMP_SMU_DUMP, 0, 0, cfg->offset);
#endif
			else if (sel_api == LOG_UNIPRO_SFR)
				cfg->val = unipro_readl(ufs, cfg->offset);
			else if (sel_api == LOG_PMA_SFR)
				cfg->val = phy_pma_readl(ufs, cfg->offset);
			else
				cfg->val = 0xFFFFFFFF;
		}

		/* Next SFR */
		cfg++;
	}
}

static void exynos_ufs_get_attr(struct ufs_hba *hba,
					struct exynos_ufs_attr_log* cfg)
{
	u32 i;
	u32 intr_enable;

	/* Disable and backup interrupts */
	intr_enable = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);

	while(cfg) {
		if (cfg->offset == 0)
			break;

		/* Send DME_GET */
		ufshcd_writel(hba, cfg->offset, REG_UIC_COMMAND_ARG_1);
		ufshcd_writel(hba, UIC_CMD_DME_GET, REG_UIC_COMMAND);

		i = 0;
		while(!(ufshcd_readl(hba, REG_INTERRUPT_STATUS) &
					UIC_COMMAND_COMPL)) {
			if (i++ > 20000) {
				dev_err(hba->dev,
					"Failed to fetch a value of %x",
					cfg->offset);
				goto out;
			}
		}

		/* Clear UIC command completion */
		ufshcd_writel(hba, UIC_COMMAND_COMPL, REG_INTERRUPT_STATUS);

		/* Fetch result and value */
		cfg->res = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2 &
				MASK_UIC_COMMAND_RESULT);
		cfg->val = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);

		/* Next attribute */
		cfg++;
	}

out:
	/* Restore and enable interrupts */
	ufshcd_writel(hba, intr_enable, REG_INTERRUPT_ENABLE);
}

static void exynos_ufs_dump_misc(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_misc_log* cfg = &ufs->debug.misc;
	struct exynos_ufs_clk_info *clki;
	struct list_head *head = &cfg->clk_list_head;

	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tMISC DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			dev_err(hba->dev, "%s: %lu\n",
					clki->name, clki->freq);
		}
	}
	dev_err(hba->dev, "iso: %d\n", ufs->debug.misc.isolation);
}

static void exynos_ufs_dump_sfr(struct ufs_hba *hba,
					struct exynos_ufs_sfr_log* cfg)
{
	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tREGISTER DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	while(cfg) {
		if (!cfg->name)
			break;

		/* Dump */
		dev_err(hba->dev, ": %s(0x%04x):\t\t\t\t0x%08x\n",
				cfg->name, cfg->offset, cfg->val);

		/* Next SFR */
		cfg++;
	}
}

static void exynos_ufs_dump_attr(struct ufs_hba *hba,
					struct exynos_ufs_attr_log* cfg)
{
	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tATTRIBUTE DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	while(cfg) {
		if (!cfg->offset)
			break;

		/* Dump */
		dev_err(hba->dev, ": 0x%04x:\t\t0x%08x\t\t0x%08x\n",
				cfg->offset, cfg->val, cfg->res);

		/* Next SFR */
		cfg++;
	}
}

/*
 * Functions to be provied externally
 *
 * There are two classes that are to initialize data structures for debug
 * and to define actual behavior.
 */
void exynos_ufs_get_uic_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (!(ufs->misc_flags & EXYNOS_UFS_MISC_TOGGLE_LOG))
		return;

	exynos_ufs_get_sfr(hba, ufs->debug.sfr);
	exynos_ufs_get_attr(hba, ufs->debug.attr);
	exynos_ufs_get_misc(hba);

	ufs->misc_flags &= ~(EXYNOS_UFS_MISC_TOGGLE_LOG);
}

void exynos_ufs_dump_uic_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* secure log */
#ifdef CONFIG_EXYNOS_SMC_LOGGING
	exynos_smc(SMC_CMD_UFS_LOG, 1, 0, hba->secure_log.paddr);
#endif

	exynos_ufs_get_sfr(hba, ufs->debug.sfr);
	exynos_ufs_get_attr(hba, ufs->debug.attr);
	exynos_ufs_get_misc(hba);

	exynos_ufs_dump_sfr(hba, ufs->debug.sfr);
	exynos_ufs_dump_attr(hba, ufs->debug.attr);
	exynos_ufs_dump_misc(hba);
}

void exynos_ufs_show_uic_info(struct ufs_hba *hba)
{
	exynos_ufs_get_sfr(hba, ufs_show_sfr);
	exynos_ufs_get_attr(hba, ufs_show_attr);

	exynos_ufs_dump_sfr(hba, ufs_show_sfr);
	exynos_ufs_dump_attr(hba, ufs_show_attr);
}


int exynos_ufs_init_dbg(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct exynos_ufs_clk_info *exynos_clki;

	ufs->debug.sfr = ufs_log_sfr;
	ufs->debug.attr = ufs_log_attr;
	INIT_LIST_HEAD(&ufs->debug.misc.clk_list_head);

	if (!head || list_empty(head))
		return 0;

	list_for_each_entry(clki, head, list) {
		exynos_clki = devm_kzalloc(hba->dev, sizeof(*exynos_clki), GFP_KERNEL);
		if (!exynos_clki) {
			return -ENOMEM;
		}
		exynos_clki->clk = clki->clk;
		exynos_clki->name = clki->name;
		exynos_clki->freq = 0;
		list_add_tail(&exynos_clki->list, &ufs->debug.misc.clk_list_head);
	}
#ifdef CONFIG_DEBUG_SNAPSHOT
	hba->secure_log.paddr = dbg_snapshot_get_spare_paddr(0);
	hba->secure_log.vaddr = (u32 *)dbg_snapshot_get_spare_vaddr(0);
#endif

	return 0;
}

static	struct ufs_cmd_info   ufs_cmd_queue;
static	struct ufs_cmd_logging_category ufs_cmd_log;

static void exynos_ufs_putItem_start(struct ufs_cmd_info *cmdQueue, struct ufs_cmd_logging_category *cmdData)
{
	cmdQueue->addr_per_tag[cmdData->tag] = &cmdQueue->data[cmdQueue->last];
	cmdQueue->data[cmdQueue->last].cmd_opcode = cmdData->cmd_opcode;
	cmdQueue->data[cmdQueue->last].tag = cmdData->tag;
	cmdQueue->data[cmdQueue->last].lba = cmdData->lba;
	cmdQueue->data[cmdQueue->last].sct = cmdData->sct;
	cmdQueue->data[cmdQueue->last].retries = cmdData->retries;
	cmdQueue->data[cmdQueue->last].start_time = cmdData->start_time;
	cmdQueue->data[cmdQueue->last].outstanding_reqs = cmdData->outstanding_reqs;
	cmdQueue->last = (cmdQueue->last + 1) % MAX_CMD_LOGS;
}



void exynos_ufs_cmd_log_start(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	int cpu = raw_smp_processor_id();

	unsigned long lba = (cmd->cmnd[2] << 24) |
					(cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) |
					(cmd->cmnd[5] << 0);
	unsigned int sct = (cmd->cmnd[7] << 8) |
					(cmd->cmnd[8] << 0);

	ufs_cmd_log.start_time = cpu_clock(cpu);
	ufs_cmd_log.cmd_opcode = cmd->cmnd[0];
	ufs_cmd_log.tag = cmd->request->tag;
	ufs_cmd_log.outstanding_reqs = hba->outstanding_reqs;

	if(cmd->cmnd[0] != UNMAP)
		ufs_cmd_log.lba = lba;

	ufs_cmd_log.sct = sct;
	ufs_cmd_log.retries = cmd->allowed;

	exynos_ufs_putItem_start(&ufs_cmd_queue, &ufs_cmd_log);
}


void exynos_ufs_cmd_log_end(struct ufs_hba *hba, int tag)
{
	int cpu = raw_smp_processor_id();

	ufs_cmd_queue.addr_per_tag[tag]->end_time = cpu_clock(cpu);
}
