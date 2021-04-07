#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"

#include "cmucal-vclklut.h"

/*=================CMUCAL version: S5E9610================================*/

/*=================CLK in each VCLK================================*/


/* DVFS List */
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	PLL_CPUCL0,
};
enum clk_id cmucal_vclk_vdd_cpucl1[] = {
	DIV_CLK_CLUSTER1_ACLK,
	PLL_CPUCL1,
};
enum clk_id cmucal_vclk_vdd_g3d[] = {
	PLL_G3D,
};
enum clk_id cmucal_vclk_vdd_int[] = {
	CLKCMU_DISPAUD_DISP,
	CLKCMU_FSYS_BUS,
	CLKCMU_G2D_MSCL,
	CLKCMU_FSYS_MMC_CARD,
	CLKCMU_FSYS_MMC_EMBD,
	CLKCMU_CORE_BUS,
	CLKCMU_CORE_CCI,
	CLKCMU_CORE_G3D,
	CLKCMU_CAM_BUS,
	CLKCMU_VIPX1_BUS,
	CLKCMU_ISP_BUS,
	CLKCMU_ISP_VRA,
	CLKCMU_ISP_GDC,
	CLKCMU_G2D_G2D,
	CLKCMU_USB_BUS,
	CLKCMU_DISPAUD_AUD,
	CLKCMU_MFC_MFC,
	CLKCMU_MFC_WFD,
	CLKCMU_VIPX2_BUS,
	MUX_CLKCMU_G2D_MSCL,
	MUX_CLKCMU_DISPAUD_DISP,
	MUX_CLKCMU_FSYS_BUS,
	MUX_CLKCMU_CORE_CCI,
	MUX_CLKCMU_CORE_G3D,
	MUX_CLKCMU_CORE_BUS,
	MUX_CLKCMU_CAM_BUS,
	MUX_CLKCMU_VIPX1_BUS,
	MUX_CLKCMU_ISP_BUS,
	MUX_CLKCMU_ISP_VRA,
	MUX_CLKCMU_ISP_GDC,
	MUX_CLKCMU_G2D_G2D,
	MUX_CLKCMU_CPUCL0_DBG,
	MUX_CLKCMU_USB_BUS,
	MUX_CLKCMU_DISPAUD_AUD,
	MUX_CLKCMU_MFC_MFC,
	MUX_CLKCMU_MFC_WFD,
	MUX_CLKCMU_VIPX2_BUS,
};
enum clk_id cmucal_vclk_vdd_cam[] = {
	DIV_CLK_AUD_AUDIF,
};
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF,
};

/* SPECIAL List */
enum clk_id cmucal_vclk_clkcmu_shub_bus[] = {
	CLKCMU_SHUB_BUS,
	MUX_CLKCMU_SHUB_BUS,
};
enum clk_id cmucal_vclk_div_clk_cmgp_adc[] = {
	MUX_CLK_CMGP_ADC,
	DIV_CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi01[] = {
	DIV_CLK_CMGP_USI01,
	MUX_CLK_CMGP_USI01,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi03[] = {
	DIV_CLK_CMGP_USI03,
	MUX_CLK_CMGP_USI03,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi02[] = {
	DIV_CLK_CMGP_USI02,
	MUX_CLK_CMGP_USI02,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi00[] = {
	DIV_CLK_CMGP_USI00,
	MUX_CLK_CMGP_USI00,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi04[] = {
	DIV_CLK_CMGP_USI04,
	MUX_CLK_CMGP_USI04,
};
enum clk_id cmucal_vclk_clkcmu_fsys_ufs_embd[] = {
	CLKCMU_FSYS_UFS_EMBD,
	MUX_CLKCMU_FSYS_UFS_EMBD,
};
enum clk_id cmucal_vclk_div_clk_cmu_cmuref[] = {
	MUX_CMU_CMUREF,
	DIV_CLK_CMU_CMUREF,
	MUX_CLK_CMU_CMUREF,
};
enum clk_id cmucal_vclk_clkcmu_hpm[] = {
	CLKCMU_HPM,
	MUX_CLKCMU_HPM,
};
enum clk_id cmucal_vclk_clkcmu_peri_ip[] = {
	CLKCMU_PERI_IP,
	MUX_CLKCMU_PERI_IP,
};
enum clk_id cmucal_vclk_clkcmu_mif_busp[] = {
	CLKCMU_MIF_BUSP,
	MUX_CLKCMU_MIF_BUSP,
};
enum clk_id cmucal_vclk_clkcmu_apm_bus[] = {
	CLKCMU_APM_BUS,
	MUX_CLKCMU_APM_BUS,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk1[] = {
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk3[] = {
	CLKCMU_CIS_CLK3,
	MUX_CLKCMU_CIS_CLK3,
};
enum clk_id cmucal_vclk_clkcmu_usb_usb30drd[] = {
	CLKCMU_USB_USB30DRD,
	MUX_CLKCMU_USB_USB30DRD,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk0[] = {
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_clkcmu_usb_dpgtc[] = {
	CLKCMU_USB_DPGTC,
	MUX_CLKCMU_USB_DPGTC,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk2[] = {
	CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
};
enum clk_id cmucal_vclk_clkcmu_peri_uart[] = {
	CLKCMU_PERI_UART,
	MUX_CLKCMU_PERI_UART,
};
enum clk_id cmucal_vclk_div_clk_cluster0_pclkdbg[] = {
	DIV_CLK_CLUSTER0_PCLKDBG,
};
enum clk_id cmucal_vclk_div_clk_cluster0_aclk[] = {
	DIV_CLK_CLUSTER0_ACLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_cmuref[] = {
	DIV_CLK_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_cluster0_cntclk[] = {
	DIV_CLK_CLUSTER0_CNTCLK,
};
enum clk_id cmucal_vclk_div_clk_cluster1_cntclk[] = {
	DIV_CLK_CLUSTER1_CNTCLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_cmuref[] = {
	DIV_CLK_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_aud_dsif[] = {
	DIV_CLK_AUD_DSIF,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif0[] = {
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF0,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif2[] = {
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF2,
};
enum clk_id cmucal_vclk_div_clk_aud_cpu_pclkdbg[] = {
	DIV_CLK_AUD_CPU_PCLKDBG,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif1[] = {
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF1,
};
enum clk_id cmucal_vclk_div_clk_aud_fm[] = {
	DIV_CLK_AUD_FM,
	MUX_CLK_AUD_FM,
	DIV_CLK_AUD_FM_SPDY
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif1_cmuref[] = {
	MUX_MIF1_CMUREF,
};
enum clk_id cmucal_vclk_pll_mif1[] = {
	PLL_MIF1,
};
enum clk_id cmucal_vclk_div_clk_peri_spi0[] = {
	DIV_CLK_PERI_SPI0,
};
enum clk_id cmucal_vclk_div_clk_peri_spi2[] = {
	DIV_CLK_PERI_SPI2,
};
enum clk_id cmucal_vclk_div_clk_peri_usi_i2c[] = {
	DIV_CLK_PERI_USI_I2C,
};
enum clk_id cmucal_vclk_div_clk_peri_spi1[] = {
	DIV_CLK_PERI_SPI1,
};
enum clk_id cmucal_vclk_div_clk_peri_usi_usi[] = {
	DIV_CLK_PERI_USI_USI,
};
enum clk_id cmucal_vclk_div_clk_shub_i2c[] = {
	DIV_CLK_SHUB_I2C,
	MUX_CLK_SHUB_I2C,
};
enum clk_id cmucal_vclk_div_clk_shub_usi00[] = {
	DIV_CLK_SHUB_USI00,
	MUX_CLK_SHUB_USI00,
};

/* COMMON List */
enum clk_id cmucal_vclk_blk_apm[] = {
	DIV_CLK_APM_BUS,
	MUX_CLK_APM_BUS,
};
enum clk_id cmucal_vclk_blk_cam[] = {
	DIV_CLK_CAM_BUSP,
};
enum clk_id cmucal_vclk_blk_cmgp[] = {
	DIV_CLK_CMGP_I2C,
	MUX_CLK_CMGP_I2C,
};
enum clk_id cmucal_vclk_blk_cmu[] = {
	AP2CP_SHARED0_PLL_CLK,
	CLKCMU_PERI_BUS,
	AP2CP_SHARED1_PLL_CLK,
	CLKCMU_CPUCL0_DBG,
	MUX_CLKCMU_FSYS_MMC_EMBD,
	MUX_CLKCMU_PERI_BUS,
	MUX_CLKCMU_FSYS_MMC_CARD,
	PLL_SHARED0_DIV4,
	PLL_SHARED1_DIV4,
	PLL_SHARED0_DIV2,
	PLL_SHARED0_DIV3,
	PLL_SHARED1_DIV2,
	PLL_MMC_DIV2,
	PLL_SHARED0,
	PLL_SHARED1_DIV3,
	PLL_MMC,
	PLL_SHARED1,
};
enum clk_id cmucal_vclk_blk_core[] = {
	DIV_CLK_CORE_BUSP,
	MUX_CLK_CORE_GIC,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	DIV_CLK_CPUCL0_PCLK,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	DIV_CLK_CPUCL1_PCLK,
	DIV_CLK_CPUCL1_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_dispaud[] = {
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_DISPAUD_BUSP,
	MUX_CLK_AUD_BUS,
	DIV_CLK_AUD_BUS,
	PLL_AUD,
	MUX_CLK_AUD_CPU_HCH,
	DIV_CLK_AUD_CPU,
};
enum clk_id cmucal_vclk_blk_g2d[] = {
	DIV_CLK_G2D_BUSP,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
};
enum clk_id cmucal_vclk_blk_isp[] = {
	DIV_CLK_ISP_BUSP,
};
enum clk_id cmucal_vclk_blk_mfc[] = {
	DIV_CLK_MFC_BUSP,
};
enum clk_id cmucal_vclk_blk_peri[] = {
	DIV_CLK_PERI_I2C,
};
enum clk_id cmucal_vclk_blk_shub[] = {
	DIV_CLK_SHUB_USI01,
	MUX_CLK_SHUB_USI01,
};
enum clk_id cmucal_vclk_blk_vipx1[] = {
	DIV_CLK_VIPX1_BUSP,
};
enum clk_id cmucal_vclk_blk_vipx2[] = {
	DIV_CLK_VIPX2_BUSP,
};

/* GATING List */
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	GOUT_BLK_APM_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_APM_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apm_cmu_apm[] = {
	CLK_BLK_APM_UID_APM_CMU_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_APM_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm_gnss[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm_modem[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm_shub[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm_wlbt[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_apm[] = {
	GOUT_BLK_APM_UID_LHS_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_shub[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap2cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap2cp_s[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP2CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap2gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap2shub[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP2SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap2wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm2ap[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm2cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm2gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm2shub[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM2SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm2wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp2gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp2shub[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP2SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp2wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_shub2gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_SHUB2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_shub2wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_SHUB2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt2abox[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT2ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt2gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pem[] = {
	GOUT_BLK_APM_UID_PEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_lite_apm[] = {
	GOUT_BLK_APM_UID_PGEN_LITE_APM_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	GOUT_BLK_APM_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_speedy_apm[] = {
	GOUT_BLK_APM_UID_SPEEDY_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_apm[] = {
	GOUT_BLK_APM_UID_SYSREG_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_apm[] = {
	GOUT_BLK_APM_UID_WDT_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_apm[] = {
	GOUT_BLK_APM_UID_XIU_DP_APM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_blk_cam[] = {
	GOUT_BLK_CAM_UID_BLK_CAM_IPCLKPORT_CLK_CAM_BUSD,
};
enum clk_id cmucal_vclk_ip_btm_cam[] = {
	GOUT_BLK_CAM_UID_BTM_CAM_IPCLKPORT_I_ACLK,
	GOUT_BLK_CAM_UID_BTM_CAM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cam_cmu_cam[] = {
	CLK_BLK_CAM_UID_CAM_CMU_CAM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cam[] = {
	GOUT_BLK_CAM_UID_LHM_AXI_P_CAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_cam[] = {
	GOUT_BLK_CAM_UID_LHS_ACEL_D_CAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_camisp[] = {
	GOUT_BLK_CAM_UID_LHS_ATB_CAMISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cam[] = {
	GOUT_BLK_CAM_UID_SYSREG_CAM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_is6p10p0_cam[] = {
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_3AA,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_CSIS0,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_CSIS1,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_CSIS2,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_CSIS3,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_GLUE_CSIS0,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_GLUE_CSIS1,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_GLUE_CSIS2,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_GLUE_CSIS3,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_PAFSTAT_CORE,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_PPMU_CAM,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_RDMA,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_SMMU_CAM,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_ACLK_XIU_D_CAM,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_PCLK_PGEN_LITE_CAM0,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_PCLK_PGEN_LITE_CAM1,
	GOUT_BLK_CAM_UID_is6p10p0_CAM_IPCLKPORT_PCLK_PPMU_CAM,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp00[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP00_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp01[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP01_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP01_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp02[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP02_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP02_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp03[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP03_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP03_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp04[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP04_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP04_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_shub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2shub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2wlbt[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp00[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP00_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp01[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP01_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP01_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp02[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP02_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP02_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp03[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP03_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP03_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp04[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP04_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP04_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp[] = {
	CLK_BLK_CMU_UID_OTP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_cci_550[] = {
	GOUT_BLK_CORE_UID_AD_APB_CCI_550_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma0[] = {
	GOUT_BLK_CORE_UID_AD_APB_PDMA0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_pgen_pdma[] = {
	GOUT_BLK_CORE_UID_AD_APB_PGEN_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_ppfw_mem0[] = {
	GOUT_BLK_CORE_UID_AD_APB_PPFW_MEM0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_ppfw_mem1[] = {
	GOUT_BLK_CORE_UID_AD_APB_PPFW_MEM1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_ppfw_peri[] = {
	GOUT_BLK_CORE_UID_AD_APB_PPFW_PERI_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	GOUT_BLK_CORE_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gic[] = {
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_asyncsfr_wr_dmc0[] = {
	GOUT_BLK_CORE_UID_ASYNCSFR_WR_DMC0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_asyncsfr_wr_dmc1[] = {
	GOUT_BLK_CORE_UID_ASYNCSFR_WR_DMC1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_axi_us_a40_64to128_dit[] = {
	GOUT_BLK_CORE_UID_AXI_US_A40_64to128_DIT_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	GOUT_BLK_CORE_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_modem[] = {
	GOUT_BLK_CORE_UID_BAAW_P_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_shub[] = {
	GOUT_BLK_CORE_UID_BAAW_P_SHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_wlbt[] = {
	GOUT_BLK_CORE_UID_BAAW_P_WLBT_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cci_550[] = {
	GOUT_BLK_CORE_UID_CCI_550_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_CORE_UID_DIT_IPCLKPORT_iClkL2A,
};
enum clk_id cmucal_vclk_ip_gic400_aihwacg[] = {
	GOUT_BLK_CORE_UID_GIC400_AIHWACG_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_isp[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D0_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_mfc[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D0_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_isp[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D1_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_mfc[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D1_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_cam[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_CAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_dpu[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_fsys[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_g2d[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_usb[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_vipx1[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_vipx2[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d_cpucl1[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_abox[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_ABOX_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_gnss[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_shub[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_wlbt[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cam[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dispaud[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DISPAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_fsys[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g2d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_gnss[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_isp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfc[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_modem[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peri[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_shub[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_usb[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vipx1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vipx2[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_wlbt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_core[] = {
	GOUT_BLK_CORE_UID_PDMA_CORE_IPCLKPORT_ACLK_PDMA0,
};
enum clk_id cmucal_vclk_ip_pgen_lite_sirex[] = {
	GOUT_BLK_CORE_UID_PGEN_LITE_SIREX_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_pdma[] = {
	GOUT_BLK_CORE_UID_PGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppcfw_g3d[] = {
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppfw_core_mem0[] = {
	GOUT_BLK_CORE_UID_PPFW_CORE_MEM0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppfw_core_mem1[] = {
	GOUT_BLK_CORE_UID_PPFW_CORE_MEM1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppfw_core_peri[] = {
	GOUT_BLK_CORE_UID_PPFW_CORE_PERI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl0[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl1[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apbif_cmu_topc[] = {
	GOUT_BLK_CORE_UID_SFR_APBIF_CMU_TOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sirex[] = {
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_i_ACLK,
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_spdma_core[] = {
	GOUT_BLK_CORE_UID_SPDMA_CORE_IPCLKPORT_ACLK_PDMA1,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_CCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_GCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_trex_d_nrt[] = {
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_trex_p_core[] = {
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_ACLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_CCLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_xiu_d_core[] = {
	GOUT_BLK_CORE_UID_XIU_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cssys_core[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_CSSYS_CORE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ads_ahb_g_cssys_fsys[] = {
	GOUT_BLK_CPUCL0_UID_ADS_AHB_G_CSSYS_FSYS_IPCLKPORT_HCLKS,
};
enum clk_id cmucal_vclk_ip_ads_apb_g_cssys_cpucl1[] = {
	GOUT_BLK_CPUCL0_UID_ADS_APB_G_CSSYS_CPUCL1_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_ads_apb_g_p8q[] = {
	GOUT_BLK_CPUCL0_UID_ADS_APB_G_P8Q_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_AD_APB_P_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl1[] = {
	GOUT_BLK_CPUCL0_UID_AD_APB_P_DUMP_PC_CPUCL1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_busif_hpmcpucl0[] = {
	GOUT_BLK_CPUCL0_UID_BUSIF_HPMCPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cssys_dbg[] = {
	GOUT_BLK_CPUCL0_UID_CSSYS_DBG_IPCLKPORT_PCLKDBG,
};
enum clk_id cmucal_vclk_ip_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_DUMP_PC_CPUCL0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_dump_pc_cpucl1[] = {
	GOUT_BLK_CPUCL0_UID_DUMP_PC_CPUCL1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_HPM_CPUCL0_IPCLKPORT_hpm_targetclk_c,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_D_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_UID_SECJTAG_IPCLKPORT_i_clk,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cssys_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_ADM_APB_G_CSSYS_CPUCL1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_busif_hpmcpucl1[] = {
	GOUT_BLK_CPUCL1_UID_BUSIF_HPMCPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_HPM_CPUCL1_IPCLKPORT_hpm_targetclk_c,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_LHM_AXI_P_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_LHS_ACE_D_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_SYSREG_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_SPDY,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_CA7,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_DBG,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_OSC_SPDY,
};
enum clk_id cmucal_vclk_ip_axi_us_32to128[] = {
	GOUT_BLK_DISPAUD_UID_AXI_US_32to128_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_blk_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_BLK_DISPAUD_IPCLKPORT_CLK_DISPAUD_AUD,
	GOUT_BLK_DISPAUD_UID_BLK_DISPAUD_IPCLKPORT_CLK_DISPAUD_DISP,
};
enum clk_id cmucal_vclk_ip_btm_abox[] = {
	GOUT_BLK_DISPAUD_UID_BTM_ABOX_IPCLKPORT_I_ACLK,
	GOUT_BLK_DISPAUD_UID_BTM_ABOX_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_btm_dpu[] = {
	GOUT_BLK_DISPAUD_UID_BTM_DPU_IPCLKPORT_I_ACLK,
	GOUT_BLK_DISPAUD_UID_BTM_DPU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_dftmux_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_DFTMUX_DISPAUD_IPCLKPORT_AUD_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_dispaud_cmu_dispaud[] = {
	CLK_BLK_DISPAUD_UID_DISPAUD_CMU_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DECON,
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DPP,
};
enum clk_id cmucal_vclk_ip_gpio_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_GPIO_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_LHM_AXI_P_DISPAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_dpu[] = {
	GOUT_BLK_DISPAUD_UID_LHS_ACEL_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_abox[] = {
	GOUT_BLK_DISPAUD_UID_LHS_AXI_D_ABOX_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_DISPAUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
	GOUT_BLK_DISPAUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_abox[] = {
	GOUT_BLK_DISPAUD_UID_PPMU_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_PPMU_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpu[] = {
	GOUT_BLK_DISPAUD_UID_PPMU_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_PPMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_abox[] = {
	GOUT_BLK_DISPAUD_UID_SMMU_ABOX_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_smmu_dpu[] = {
	GOUT_BLK_DISPAUD_UID_SMMU_DPU_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_SYSREG_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_DISPAUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_ahb_sss[] = {
	GOUT_BLK_FSYS_UID_ADM_AHB_SSS_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_btm_fsys[] = {
	GOUT_BLK_FSYS_UID_BTM_FSYS_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_BTM_FSYS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_fsys_cmu_fsys[] = {
	CLK_BLK_FSYS_UID_FSYS_CMU_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_fsys[] = {
	GOUT_BLK_FSYS_UID_GPIO_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_fsys[] = {
	GOUT_BLK_FSYS_UID_LHM_AXI_P_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_fsys[] = {
	GOUT_BLK_FSYS_UID_LHS_ACEL_D_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_FSYS_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_mmc_embd[] = {
	GOUT_BLK_FSYS_UID_MMC_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_MMC_EMBD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_pgen_lite_fsys[] = {
	GOUT_BLK_FSYS_UID_PGEN_LITE_FSYS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_fsys[] = {
	GOUT_BLK_FSYS_UID_PPMU_FSYS_IPCLKPORT_ACLK,
	GOUT_BLK_FSYS_UID_PPMU_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_FSYS_UID_RTIC_IPCLKPORT_i_ACLK,
	GOUT_BLK_FSYS_UID_RTIC_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_FSYS_UID_SSS_IPCLKPORT_i_ACLK,
	GOUT_BLK_FSYS_UID_SSS_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_fsys[] = {
	GOUT_BLK_FSYS_UID_SYSREG_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	GOUT_BLK_FSYS_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
	GOUT_BLK_FSYS_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_fsys[] = {
	GOUT_BLK_FSYS_UID_XIU_D_FSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_axi_jpeg[] = {
	GOUT_BLK_G2D_UID_AS_AXI_JPEG_IPCLKPORT_ACLKM,
	GOUT_BLK_G2D_UID_AS_AXI_JPEG_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_as_axi_mscl[] = {
	GOUT_BLK_G2D_UID_AS_AXI_MSCL_IPCLKPORT_ACLKM,
	GOUT_BLK_G2D_UID_AS_AXI_MSCL_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_blk_g2d[] = {
	GOUT_BLK_G2D_UID_BLK_G2D_IPCLKPORT_CLK_G2D_G2D,
	GOUT_BLK_G2D_UID_BLK_G2D_IPCLKPORT_CLK_G2D_MSCL,
};
enum clk_id cmucal_vclk_ip_btm_g2d[] = {
	GOUT_BLK_G2D_UID_BTM_G2D_IPCLKPORT_I_ACLK,
	GOUT_BLK_G2D_UID_BTM_G2D_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_g2d[] = {
	GOUT_BLK_G2D_UID_G2D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_g2d_cmu_g2d[] = {
	CLK_BLK_G2D_UID_G2D_CMU_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_jpeg[] = {
	GOUT_BLK_G2D_UID_JPEG_IPCLKPORT_I_FIMP_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g2d[] = {
	GOUT_BLK_G2D_UID_LHM_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_g2d[] = {
	GOUT_BLK_G2D_UID_LHS_ACEL_D_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mscl[] = {
	GOUT_BLK_G2D_UID_MSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_pgen100_lite_g2d[] = {
	GOUT_BLK_G2D_UID_PGEN100_LITE_G2D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g2d[] = {
	GOUT_BLK_G2D_UID_PPMU_G2D_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_PPMU_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_g2d[] = {
	GOUT_BLK_G2D_UID_SYSMMU_G2D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_g2d[] = {
	GOUT_BLK_G2D_UID_SYSREG_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mscl[] = {
	GOUT_BLK_G2D_UID_XIU_D_MSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_btm_g3d[] = {
	GOUT_BLK_G3D_UID_BTM_G3D_IPCLKPORT_I_ACLK,
	GOUT_BLK_G3D_UID_BTM_G3D_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmg3d[] = {
	GOUT_BLK_G3D_UID_BUSIF_HPMG3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d[] = {
	CLK_BLK_G3D_UID_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_g3d[] = {
	CLK_BLK_G3D_UID_HPM_G3D_IPCLKPORT_hpm_targetclk_c,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g3dsfr[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_G3DSFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g3dsfr[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_G3DSFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_lite_g3d[] = {
	GOUT_BLK_G3D_UID_PGEN_LITE_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_blk_isp[] = {
	GOUT_BLK_ISP_UID_BLK_ISP_IPCLKPORT_CLK_ISP_BUSD,
	GOUT_BLK_ISP_UID_BLK_ISP_IPCLKPORT_CLK_ISP_GDC,
	GOUT_BLK_ISP_UID_BLK_ISP_IPCLKPORT_CLK_ISP_VRA,
};
enum clk_id cmucal_vclk_ip_btm_isp0[] = {
	GOUT_BLK_ISP_UID_BTM_ISP0_IPCLKPORT_I_ACLK,
	GOUT_BLK_ISP_UID_BTM_ISP0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_btm_isp1[] = {
	GOUT_BLK_ISP_UID_BTM_ISP1_IPCLKPORT_I_ACLK,
	GOUT_BLK_ISP_UID_BTM_ISP1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_isp_cmu_isp[] = {
	CLK_BLK_ISP_UID_ISP_CMU_ISP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_camisp[] = {
	GOUT_BLK_ISP_UID_LHM_ATB_CAMISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_isp[] = {
	GOUT_BLK_ISP_UID_LHM_AXI_P_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_isp[] = {
	GOUT_BLK_ISP_UID_LHS_ACEL_D0_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_isp[] = {
	GOUT_BLK_ISP_UID_LHS_ACEL_D1_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_isp[] = {
	GOUT_BLK_ISP_UID_SYSREG_ISP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_is6p10p0_isp[] = {
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_GDC,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_ISP,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_MCSC,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_PPMU_ISP0,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_PPMU_ISP1,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_SMMU_ISP0,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_SMMU_ISP1,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_VRA,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_XIU_ASYNCM_GDC,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_XIU_ASYNCM_VRA,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_XIU_ASYNCS_GDC,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_XIU_ASYNCS_VRA,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_ACLK_XIU_D_ISP,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_PCLK_PPMU_ISP0,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_PCLK_PPMU_ISP1,
	GOUT_BLK_ISP_UID_is6p10p0_ISP_IPCLKPORT_PGEN_LITE_ISP_PCLK,
};
enum clk_id cmucal_vclk_ip_as_axi_wfd[] = {
	GOUT_BLK_MFC_UID_AS_AXI_WFD_IPCLKPORT_ACLKM,
	GOUT_BLK_MFC_UID_AS_AXI_WFD_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_blk_mfc[] = {
	GOUT_BLK_MFC_UID_BLK_MFC_IPCLKPORT_CLK_MFC_MFC,
	GOUT_BLK_MFC_UID_BLK_MFC_IPCLKPORT_CLK_MFC_WFD,
};
enum clk_id cmucal_vclk_ip_btm_mfcd0[] = {
	GOUT_BLK_MFC_UID_BTM_MFCD0_IPCLKPORT_I_ACLK,
	GOUT_BLK_MFC_UID_BTM_MFCD0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_btm_mfcd1[] = {
	GOUT_BLK_MFC_UID_BTM_MFCD1_IPCLKPORT_I_ACLK,
	GOUT_BLK_MFC_UID_BTM_MFCD1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfc[] = {
	GOUT_BLK_MFC_UID_LHM_AXI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_mfc[] = {
	GOUT_BLK_MFC_UID_LHS_ACEL_D0_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_mfc[] = {
	GOUT_BLK_MFC_UID_LHS_ACEL_D1_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mfc[] = {
	GOUT_BLK_MFC_UID_LH_ATB_MFC_IPCLKPORT_I_CLK_MI,
	GOUT_BLK_MFC_UID_LH_ATB_MFC_IPCLKPORT_I_CLK_SI,
};
enum clk_id cmucal_vclk_ip_mfc[] = {
	GOUT_BLK_MFC_UID_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mfc_cmu_mfc[] = {
	CLK_BLK_MFC_UID_MFC_CMU_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pgen100_lite_mfc[] = {
	GOUT_BLK_MFC_UID_PGEN100_LITE_MFC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcd0[] = {
	GOUT_BLK_MFC_UID_PPMU_MFCD0_IPCLKPORT_ACLK,
	GOUT_BLK_MFC_UID_PPMU_MFCD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcd1[] = {
	GOUT_BLK_MFC_UID_PPMU_MFCD1_IPCLKPORT_ACLK,
	GOUT_BLK_MFC_UID_PPMU_MFCD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcd0[] = {
	GOUT_BLK_MFC_UID_SYSMMU_MFCD0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcd1[] = {
	GOUT_BLK_MFC_UID_SYSMMU_MFCD1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc[] = {
	GOUT_BLK_MFC_UID_SYSREG_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wfd[] = {
	GOUT_BLK_MFC_UID_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfc[] = {
	GOUT_BLK_MFC_UID_XIU_D_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmmif[] = {
	GOUT_BLK_MIF_UID_BUSIF_HPMMIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddr_phy[] = {
	GOUT_BLK_MIF_UID_DDR_PHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_mif[] = {
	CLK_BLK_MIF_UID_HPM_MIF_IPCLKPORT_hpm_targetclk_c,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cp[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cpu[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_nrt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_rt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif[] = {
	GOUT_BLK_MIF_UID_LHM_AXI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dmc_cpu[] = {
	CLK_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_ddr_phy[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DDR_PHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_pf[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PPMPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_secure[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_SECURE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmmif1[] = {
	GOUT_BLK_MIF1_UID_BUSIF_HPMMIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc1[] = {
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_ACLK,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_SECURE,
};
enum clk_id cmucal_vclk_ip_hpm_mif1[] = {
	GOUT_BLK_MIF1_UID_HPM_MIF1_IPCLKPORT_hpm_targetclk_c,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_cp[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_D_MIF1_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_cpu[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_D_MIF1_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_nrt[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_D_MIF1_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_rt[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_D_MIF1_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mif1_cmu_mif1[] = {
	CLK_BLK_MIF1_UID_MIF1_CMU_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2ahb_msd32_peri[] = {
	GOUT_BLK_PERI_UID_AXI2AHB_MSD32_PERI_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_busif_tmu[] = {
	GOUT_BLK_PERI_UID_BUSIF_TMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cami2c_0[] = {
	GOUT_BLK_PERI_UID_CAMI2C_0_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_CAMI2C_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cami2c_1[] = {
	GOUT_BLK_PERI_UID_CAMI2C_1_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_CAMI2C_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cami2c_2[] = {
	GOUT_BLK_PERI_UID_CAMI2C_2_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_CAMI2C_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cami2c_3[] = {
	GOUT_BLK_PERI_UID_CAMI2C_3_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_CAMI2C_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peri[] = {
	GOUT_BLK_PERI_UID_GPIO_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_0[] = {
	GOUT_BLK_PERI_UID_I2C_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_1[] = {
	GOUT_BLK_PERI_UID_I2C_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_2[] = {
	GOUT_BLK_PERI_UID_I2C_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_3[] = {
	GOUT_BLK_PERI_UID_I2C_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_4[] = {
	GOUT_BLK_PERI_UID_I2C_4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_5[] = {
	GOUT_BLK_PERI_UID_I2C_5_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_6[] = {
	GOUT_BLK_PERI_UID_I2C_6_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peri[] = {
	GOUT_BLK_PERI_UID_LHM_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERI_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peri_cmu_peri[] = {
	CLK_BLK_PERI_UID_PERI_CMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pwm_motor[] = {
	GOUT_BLK_PERI_UID_PWM_MOTOR_IPCLKPORT_i_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_spi_0[] = {
	GOUT_BLK_PERI_UID_SPI_0_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_SPI_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spi_1[] = {
	GOUT_BLK_PERI_UID_SPI_1_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_SPI_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spi_2[] = {
	GOUT_BLK_PERI_UID_SPI_2_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_SPI_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peri[] = {
	GOUT_BLK_PERI_UID_SYSREG_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_uart[] = {
	GOUT_BLK_PERI_UID_UART_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_UART_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_i2c[] = {
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_usi[] = {
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_cluster0[] = {
	GOUT_BLK_PERI_UID_WDT_CLUSTER0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_cluster1[] = {
	GOUT_BLK_PERI_UID_WDT_CLUSTER1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_shub[] = {
	GOUT_BLK_SHUB_UID_BAAW_D_SHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_apm_shub[] = {
	GOUT_BLK_SHUB_UID_BAAW_P_APM_SHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cm4_shub[] = {
	GOUT_BLK_SHUB_UID_CM4_SHUB_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_gpio_shub[] = {
	GOUT_BLK_SHUB_UID_GPIO_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_shub00[] = {
	GOUT_BLK_SHUB_UID_I2C_SHUB00_IPCLKPORT_IPCLK,
	GOUT_BLK_SHUB_UID_I2C_SHUB00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_shub[] = {
	GOUT_BLK_SHUB_UID_LHM_AXI_LP_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_shub[] = {
	GOUT_BLK_SHUB_UID_LHM_AXI_P_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_shub[] = {
	GOUT_BLK_SHUB_UID_LHS_AXI_D_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm_shub[] = {
	GOUT_BLK_SHUB_UID_LHS_AXI_P_APM_SHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_shub[] = {
	GOUT_BLK_SHUB_UID_PDMA_SHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_pwm_shub[] = {
	GOUT_BLK_SHUB_UID_PWM_SHUB_IPCLKPORT_i_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_shub_cmu_shub[] = {
	CLK_BLK_SHUB_UID_SHUB_CMU_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_d_shub[] = {
	GOUT_BLK_SHUB_UID_SWEEPER_D_SHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sweeper_p_apm_shub[] = {
	GOUT_BLK_SHUB_UID_SWEEPER_P_APM_SHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_shub[] = {
	GOUT_BLK_SHUB_UID_SYSREG_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_shub[] = {
	GOUT_BLK_SHUB_UID_TIMER_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_shub00[] = {
	GOUT_BLK_SHUB_UID_USI_SHUB00_IPCLKPORT_IPCLK,
	GOUT_BLK_SHUB_UID_USI_SHUB00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_shub[] = {
	GOUT_BLK_SHUB_UID_WDT_SHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_shub[] = {
	GOUT_BLK_SHUB_UID_XIU_DP_SHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_btm_usb[] = {
	GOUT_BLK_USB_UID_BTM_USB_IPCLKPORT_I_ACLK,
	GOUT_BLK_USB_UID_BTM_USB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_dp_link[] = {
	GOUT_BLK_USB_UID_DP_LINK_IPCLKPORT_DPTX_LINK_I_DP_GTC_CLK,
	GOUT_BLK_USB_UID_DP_LINK_IPCLKPORT_DPTX_LINK_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_usb[] = {
	GOUT_BLK_USB_UID_LHM_AXI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_usb[] = {
	GOUT_BLK_USB_UID_LHS_ACEL_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_lite_usb[] = {
	GOUT_BLK_USB_UID_PGEN_LITE_USB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_usb[] = {
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_ACLK,
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_usb[] = {
	GOUT_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb30drd[] = {
	GOUT_BLK_USB_UID_USB30DRD_IPCLKPORT_ACLK_PHYCTRL_20,
	GOUT_BLK_USB_UID_USB30DRD_IPCLKPORT_ACLK_PHYCTRL_30_0,
	GOUT_BLK_USB_UID_USB30DRD_IPCLKPORT_ACLK_PHYCTRL_30_1,
	GOUT_BLK_USB_UID_USB30DRD_IPCLKPORT_USB30DRD_ref_clk,
	GOUT_BLK_USB_UID_USB30DRD_IPCLKPORT_bus_clk_early,
};
enum clk_id cmucal_vclk_ip_usb_cmu_usb[] = {
	CLK_BLK_USB_UID_USB_CMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_us_d_usb[] = {
	GOUT_BLK_USB_UID_US_D_USB_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_blk_vipx1[] = {
	GOUT_BLK_VIPX1_UID_BLK_VIPX1_IPCLKPORT_CLK_VIPX1_BUSD,
};
enum clk_id cmucal_vclk_ip_btm_d_vipx1[] = {
	GOUT_BLK_VIPX1_UID_BTM_D_VIPX1_IPCLKPORT_I_ACLK,
	GOUT_BLK_VIPX1_UID_BTM_D_VIPX1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_vipx1[] = {
	GOUT_BLK_VIPX1_UID_LHM_ATB_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vipx1[] = {
	GOUT_BLK_VIPX1_UID_LHM_AXI_P_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_vipx1[] = {
	GOUT_BLK_VIPX1_UID_LHS_ACEL_D_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_vipx1[] = {
	GOUT_BLK_VIPX1_UID_LHS_ATB_VIPX1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vipx1_local[] = {
	GOUT_BLK_VIPX1_UID_LHS_AXI_P_VIPX1_LOCAL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_lite_vipx1[] = {
	GOUT_BLK_VIPX1_UID_PGEN_LITE_VIPX1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_vipx1[] = {
	GOUT_BLK_VIPX1_UID_PPMU_D_VIPX1_IPCLKPORT_ACLK,
	GOUT_BLK_VIPX1_UID_PPMU_D_VIPX1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_d_vipx1[] = {
	GOUT_BLK_VIPX1_UID_SMMU_D_VIPX1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vipx1[] = {
	GOUT_BLK_VIPX1_UID_SYSREG_VIPX1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vipx1[] = {
	GOUT_BLK_VIPX1_UID_VIPX1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vipx1_cmu_vipx1[] = {
	CLK_BLK_VIPX1_UID_VIPX1_CMU_VIPX1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_vipx1[] = {
	GOUT_BLK_VIPX1_UID_XIU_D_VIPX1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_blk_vipx2[] = {
	GOUT_BLK_VIPX2_UID_BLK_VIPX2_IPCLKPORT_CLK_VIPX2_BUSD,
};
enum clk_id cmucal_vclk_ip_btm_d_vipx2[] = {
	GOUT_BLK_VIPX2_UID_BTM_D_VIPX2_IPCLKPORT_I_ACLK,
	GOUT_BLK_VIPX2_UID_BTM_D_VIPX2_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_vipx2[] = {
	GOUT_BLK_VIPX2_UID_LHM_ATB_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vipx2[] = {
	GOUT_BLK_VIPX2_UID_LHM_AXI_P_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vipx2_local[] = {
	GOUT_BLK_VIPX2_UID_LHM_AXI_P_VIPX2_LOCAL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_vipx2[] = {
	GOUT_BLK_VIPX2_UID_LHS_ACEL_D_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_vipx2[] = {
	GOUT_BLK_VIPX2_UID_LHS_ATB_VIPX2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pgen_lite_vipx2[] = {
	GOUT_BLK_VIPX2_UID_PGEN_LITE_VIPX2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_vipx2[] = {
	GOUT_BLK_VIPX2_UID_PPMU_D_VIPX2_IPCLKPORT_ACLK,
	GOUT_BLK_VIPX2_UID_PPMU_D_VIPX2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_d_vipx2[] = {
	GOUT_BLK_VIPX2_UID_SMMU_D_VIPX2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vipx2[] = {
	GOUT_BLK_VIPX2_UID_SYSREG_VIPX2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vipx2[] = {
	GOUT_BLK_VIPX2_UID_VIPX2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vipx2_cmu_vipx2[] = {
	CLK_BLK_VIPX2_UID_VIPX2_CMU_VIPX2_IPCLKPORT_PCLK,
};

/* Switching LUT */
/* -1 is the Value of EMPTY_CAL_ID */
struct switch_lut tail_blk_cpucl0_lut[] = {
	{799500, 0, 0},
	{399750, 0, 1},
	{266500, 2, 1},
};
struct switch_lut tail_blk_cpucl1_lut[] = {
	{799500, 0, 0},
	{399750, 0, 1},
};
struct switch_lut tail_blk_dispaud_lut[] = {
	{1332500, 0, 0},
	{799500, 1, 0},
	{399750, 1, 1},
};
struct switch_lut tail_blk_g3d_lut[] = {
	{799500, 0, 0},
	{399750, 0, 1},
	{199875, 0, 3},
};
struct switch_lut tail_blk_mif_lut[] = {
	{1599000, 0, -1},
	{1332500, 1, -1},
};
struct switch_lut tail_blk_mif1_lut[] = {
	{1599000, 0, -1},
	{1332500, 1, -1},
};

/* DVFS LUT */
struct vclk_lut vdd_cpucl0_lut[] = {
	{1850333, vdd_cpucl0_sod_lut_params},
	{1449500, vdd_cpucl0_od_lut_params},
	{1049750, vdd_cpucl0_nm_lut_params},
	{600166, vdd_cpucl0_ud_lut_params},
	{300083, vdd_cpucl0_sud_lut_params},
};
struct vclk_lut vdd_cpucl1_lut[] = {
	{2400666, vdd_cpucl1_sod_lut_params},
	{1898000, vdd_cpucl1_od_lut_params},
	{1499333, vdd_cpucl1_nm_lut_params},
	{850200, vdd_cpucl1_ud_lut_params},
	{549899, vdd_cpucl1_sud_lut_params},
};
struct vclk_lut vdd_g3d_lut[] = {
	{1200000, vdd_g3d_sod_lut_params},
	{1000000, vdd_g3d_od_lut_params},
	{750000, vdd_g3d_nm_lut_params},
	{550000, vdd_g3d_ud_lut_params},
	{300000, vdd_g3d_sud_lut_params},
};
struct vclk_lut vdd_int_lut[] = {
	{400000, vdd_int_nm_lut_params},
	{300000, vdd_int_od_lut_params},
	{200000, vdd_int_sud_lut_params},
	{100000, vdd_int_ud_lut_params},
};
struct vclk_lut vdd_cam_lut[] = {
	{400000, vdd_cam_nm_lut_params},
	{300000, vdd_cam_od_lut_params},
	{200000, vdd_cam_sud_lut_params},
	{100000, vdd_cam_ud_lut_params},
};
struct vclk_lut vdd_mif_lut[] = {
	{4264000, vdd_mif_nm_lut_params},
	{1399666, vdd_mif_sud_lut_params},
	{1332500, vdd_mif_ud_lut_params},
};

/* SPECIAL LUT */
struct vclk_lut clkcmu_shub_bus_lut[] = {
	{400000, spl_clk_shub_i2c_blk_apm_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_adc_lut[] = {
	{33333, mux_clk_cmgp_adc_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi01_lut[] = {
	{200000, spl_clk_cmgp_usi01_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi03_lut[] = {
	{200000, spl_clk_cmgp_usi03_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi02_lut[] = {
	{200000, spl_clk_cmgp_usi02_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi00_lut[] = {
	{200000, spl_clk_cmgp_usi00_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi04_lut[] = {
	{200000, spl_clk_cmgp_usi04_blk_cmgp_nm_lut_params},
};
struct vclk_lut clkcmu_fsys_ufs_embd_lut[] = {
	{133250, spl_clk_fsys_ufs_embd_blk_cmu_nm_lut_params},
};
struct vclk_lut div_clk_cmu_cmuref_lut[] = {
	{199875, occ_cmu_cmuref_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_hpm_lut[] = {
	{799500, clkcmu_hpm_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_peri_ip_lut[] = {
	{399750, spl_clk_peri_spi0_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_mif_busp_lut[] = {
	{199875, occ_mif_cmuref_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_apm_bus_lut[] = {
	{399750, spl_clk_shub_i2c_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk1_lut[] = {
	{99937, clkcmu_cis_clk1_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk3_lut[] = {
	{99937, clkcmu_cis_clk3_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_usb_usb30drd_lut[] = {
	{49968, spl_clk_usb_usb30drd_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk0_lut[] = {
	{99937, clkcmu_cis_clk0_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_usb_dpgtc_lut[] = {
	{99937, spl_clk_usb_dpgtc_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk2_lut[] = {
	{99937, clkcmu_cis_clk2_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_peri_uart_lut[] = {
	{199875, spl_clk_peri_uart_blk_cmu_nm_lut_params},
};
struct vclk_lut div_clk_cluster0_pclkdbg_lut[] = {
	{231291, div_clk_cluster0_pclkdbg_blk_cpucl0_sod_lut_params},
	{181187, div_clk_cluster0_pclkdbg_blk_cpucl0_od_lut_params},
	{131218, div_clk_cluster0_pclkdbg_blk_cpucl0_nm_lut_params},
	{75020, div_clk_cluster0_pclkdbg_blk_cpucl0_ud_lut_params},
	{37510, div_clk_cluster0_pclkdbg_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_cluster0_aclk_lut[] = {
	{925166, div_clk_cluster0_aclk_blk_cpucl0_sod_lut_params},
	{724750, div_clk_cluster0_aclk_blk_cpucl0_od_lut_params},
	{524875, div_clk_cluster0_aclk_blk_cpucl0_nm_lut_params},
	{300083, div_clk_cluster0_aclk_blk_cpucl0_ud_lut_params},
	{150041, div_clk_cluster0_aclk_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_cpucl0_cmuref_lut[] = {
	{925166, spl_clk_cpucl0_cmuref_blk_cpucl0_sod_lut_params},
	{724750, spl_clk_cpucl0_cmuref_blk_cpucl0_od_lut_params},
	{524875, spl_clk_cpucl0_cmuref_blk_cpucl0_nm_lut_params},
	{300083, spl_clk_cpucl0_cmuref_blk_cpucl0_ud_lut_params},
	{150041, spl_clk_cpucl0_cmuref_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_cluster0_cntclk_lut[] = {
	{462583, div_clk_cluster0_cntclk_blk_cpucl0_sod_lut_params},
	{362375, div_clk_cluster0_cntclk_blk_cpucl0_od_lut_params},
	{262437, div_clk_cluster0_cntclk_blk_cpucl0_nm_lut_params},
	{150041, div_clk_cluster0_cntclk_blk_cpucl0_ud_lut_params},
	{75020, div_clk_cluster0_cntclk_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_cluster1_cntclk_lut[] = {
	{600166, div_clk_cluster1_cntclk_blk_cpucl1_sod_lut_params},
	{474500, div_clk_cluster1_cntclk_blk_cpucl1_od_lut_params},
	{374833, div_clk_cluster1_cntclk_blk_cpucl1_nm_lut_params},
	{212550, div_clk_cluster1_cntclk_blk_cpucl1_ud_lut_params},
	{137474, div_clk_cluster1_cntclk_blk_cpucl1_sud_lut_params},
};
struct vclk_lut div_clk_cpucl1_cmuref_lut[] = {
	{1200333, spl_clk_cpucl1_cmuref_blk_cpucl1_sod_lut_params},
	{949000, spl_clk_cpucl1_cmuref_blk_cpucl1_od_lut_params},
	{749666, spl_clk_cpucl1_cmuref_blk_cpucl1_nm_lut_params},
	{425100, spl_clk_cpucl1_cmuref_blk_cpucl1_ud_lut_params},
	{274949, spl_clk_cpucl1_cmuref_blk_cpucl1_sud_lut_params},
};
struct vclk_lut div_clk_aud_dsif_lut[] = {
	{58982, spl_clk_aud_dsif_blk_dispaud_sud_lut_params},
	{39321, spl_clk_aud_dsif_blk_dispaud_ud_lut_params},
	{24999, spl_clk_aud_dsif_blk_dispaud_od_lut_params},
	{24576, spl_clk_aud_dsif_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_uaif0_lut[] = {
	{26000, dft_clk_aud_uaif0_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_uaif2_lut[] = {
	{26000, dft_clk_aud_uaif2_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_cpu_pclkdbg_lut[] = {
	{147456, spl_clk_aud_cpu_pclkdbg_blk_dispaud_nm_lut_params},
	{99937, spl_clk_aud_cpu_pclkdbg_blk_dispaud_od_lut_params},
};
struct vclk_lut div_clk_aud_uaif1_lut[] = {
	{26000, dft_clk_aud_uaif1_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_fm_lut[] = {
	{30000, dft_clk_aud_fm_blk_dispaud_nm_lut_params},
};
struct vclk_lut mux_mif_cmuref_lut[] = {
	{199875, occ_mif_cmuref_blk_mif_nm_lut_params},
};
struct vclk_lut mux_mif1_cmuref_lut[] = {
	{100000, occ_mif1_cmuref_blk_mif1_nm_lut_params},
};
struct vclk_lut pll_mif1_lut[] = {
	{100000, clk_mif1_busd_blk_mif1_nm_lut_params},
};
struct vclk_lut div_clk_peri_spi0_lut[] = {
	{399750, spl_clk_peri_spi0_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_spi2_lut[] = {
	{399750, spl_clk_peri_spi2_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_usi_i2c_lut[] = {
	{199875, spl_clk_peri_usi_i2c_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_spi1_lut[] = {
	{399750, spl_clk_peri_spi1_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_usi_usi_lut[] = {
	{399750, spl_clk_peri_usi_usi_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_shub_i2c_lut[] = {
	{200000, spl_clk_shub_i2c_blk_shub_nm_lut_params},
};
struct vclk_lut div_clk_shub_usi00_lut[] = {
	{400000, spl_clk_shub_usi00_blk_shub_nm_lut_params},
};

/* COMMON LUT */
struct vclk_lut blk_apm_lut[] = {
	{400000, blk_apm_lut_params},
};
struct vclk_lut blk_cam_lut[] = {
	{266500, blk_cam_lut_params},
};
struct vclk_lut blk_cmgp_lut[] = {
	{200000, blk_cmgp_lut_params},
};
struct vclk_lut blk_cmu_lut[] = {
	{799999, blk_cmu_lut_params},
};
struct vclk_lut blk_core_lut[] = {
	{333125, blk_core_lut_params},
};
struct vclk_lut blk_cpucl0_lut[] = {
	{131218, blk_cpucl0_lut_params},
};
struct vclk_lut blk_cpucl1_lut[] = {
	{187416, blk_cpucl1_lut_params},
};
struct vclk_lut blk_dispaud_lut[] = {
	{1179648, blk_dispaud_lut_params},
};
struct vclk_lut blk_g2d_lut[] = {
	{266500, blk_g2d_lut_params},
};
struct vclk_lut blk_g3d_lut[] = {
	{187500, blk_g3d_lut_params},
};
struct vclk_lut blk_isp_lut[] = {
	{222083, blk_isp_lut_params},
};
struct vclk_lut blk_mfc_lut[] = {
	{333125, blk_mfc_lut_params},
};
struct vclk_lut blk_peri_lut[] = {
	{199875, blk_peri_lut_params},
};
struct vclk_lut blk_shub_lut[] = {
	{400000, blk_shub_lut_params},
};
struct vclk_lut blk_vipx1_lut[] = {
	{266500, blk_vipx1_lut_params},
};
struct vclk_lut blk_vipx2_lut[] = {
	{266500, blk_vipx2_lut_params},
};
/*=================VCLK Switch list================================*/

struct vclk_switch vclk_switch_blk_cpucl0[] = {
	{MUX_CLK_CPUCL0_PLL, MUX_CLKCMU_CPUCL0_SWITCH, CLKCMU_CPUCL0_SWITCH, GATE_CLKCMU_CPUCL0_SWITCH, MUX_CLKCMU_CPUCL0_SWITCH_USER, tail_blk_cpucl0_lut, 3},
};
struct vclk_switch vclk_switch_blk_cpucl1[] = {
	{MUX_CLK_CPUCL1_PLL, MUX_CLKCMU_CPUCL1_SWITCH, CLKCMU_CPUCL1_SWITCH, GATE_CLKCMU_CPUCL1_SWITCH, MUX_CLKCMU_CPUCL1_SWITCH_USER, tail_blk_cpucl1_lut, 2},
};
struct vclk_switch vclk_switch_blk_dispaud[] = {
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_DISPAUD_CPU, CLKCMU_DISPAUD_CPU, GATE_CLKCMU_DISPAUD_CPU, MUX_CLKCMU_DISPAUD_CPU_USER, tail_blk_dispaud_lut, 3},
};
struct vclk_switch vclk_switch_blk_g3d[] = {
	{MUX_CLK_G3D_BUSD, MUX_CLKCMU_G3D_SWITCH, CLKCMU_G3D_SWITCH, GATE_CLKCMU_G3D_SWITCH, MUX_CLKCMU_G3D_SWITCH_USER, tail_blk_g3d_lut, 3},
};
struct vclk_switch vclk_switch_blk_mif[] = {
	{MUX_CLK_MIF_DDRPHY_CLK2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, tail_blk_mif_lut, 2},
};
struct vclk_switch vclk_switch_blk_mif1[] = {
	{MUX_CLK_MIF1_DDRPHY_CLK2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, tail_blk_mif1_lut, 2},
};

/*=================VCLK list================================*/

struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK */
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, vclk_switch_blk_cpucl0),
	CMUCAL_VCLK(VCLK_VDD_CPUCL1, vdd_cpucl1_lut, cmucal_vclk_vdd_cpucl1, NULL, vclk_switch_blk_cpucl1),
	CMUCAL_VCLK(VCLK_VDD_G3D, vdd_g3d_lut, cmucal_vclk_vdd_g3d, NULL, vclk_switch_blk_g3d),
	CMUCAL_VCLK(VCLK_VDD_INT, vdd_int_lut, cmucal_vclk_vdd_int, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CAM, vdd_cam_lut, cmucal_vclk_vdd_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_MIF, vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, vclk_switch_blk_mif),

/* SPECIAL VCLK */
	CMUCAL_VCLK(VCLK_CLKCMU_SHUB_BUS, clkcmu_shub_bus_lut, cmucal_vclk_clkcmu_shub_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_ADC, div_clk_cmgp_adc_lut, cmucal_vclk_div_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI01, div_clk_cmgp_usi01_lut, cmucal_vclk_div_clk_cmgp_usi01, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI03, div_clk_cmgp_usi03_lut, cmucal_vclk_div_clk_cmgp_usi03, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI02, div_clk_cmgp_usi02_lut, cmucal_vclk_div_clk_cmgp_usi02, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI00, div_clk_cmgp_usi00_lut, cmucal_vclk_div_clk_cmgp_usi00, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI04, div_clk_cmgp_usi04_lut, cmucal_vclk_div_clk_cmgp_usi04, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_FSYS_UFS_EMBD, clkcmu_fsys_ufs_embd_lut, cmucal_vclk_clkcmu_fsys_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMU_CMUREF, div_clk_cmu_cmuref_lut, cmucal_vclk_div_clk_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HPM, clkcmu_hpm_lut, cmucal_vclk_clkcmu_hpm, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_IP, clkcmu_peri_ip_lut, cmucal_vclk_clkcmu_peri_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_MIF_BUSP, clkcmu_mif_busp_lut, cmucal_vclk_clkcmu_mif_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_APM_BUS, clkcmu_apm_bus_lut, cmucal_vclk_clkcmu_apm_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK3, clkcmu_cis_clk3_lut, cmucal_vclk_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_USB_USB30DRD, clkcmu_usb_usb30drd_lut, cmucal_vclk_clkcmu_usb_usb30drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_USB_DPGTC, clkcmu_usb_dpgtc_lut, cmucal_vclk_clkcmu_usb_dpgtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK2, clkcmu_cis_clk2_lut, cmucal_vclk_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_UART, clkcmu_peri_uart_lut, cmucal_vclk_clkcmu_peri_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_PCLKDBG, div_clk_cluster0_pclkdbg_lut, cmucal_vclk_div_clk_cluster0_pclkdbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_ACLK, div_clk_cluster0_aclk_lut, cmucal_vclk_div_clk_cluster0_aclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_CMUREF, div_clk_cpucl0_cmuref_lut, cmucal_vclk_div_clk_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_CNTCLK, div_clk_cluster0_cntclk_lut, cmucal_vclk_div_clk_cluster0_cntclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER1_CNTCLK, div_clk_cluster1_cntclk_lut, cmucal_vclk_div_clk_cluster1_cntclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_CMUREF, div_clk_cpucl1_cmuref_lut, cmucal_vclk_div_clk_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_DSIF, div_clk_aud_dsif_lut, cmucal_vclk_div_clk_aud_dsif, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF0, div_clk_aud_uaif0_lut, cmucal_vclk_div_clk_aud_uaif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF2, div_clk_aud_uaif2_lut, cmucal_vclk_div_clk_aud_uaif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_CPU_PCLKDBG, div_clk_aud_cpu_pclkdbg_lut, cmucal_vclk_div_clk_aud_cpu_pclkdbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF1, div_clk_aud_uaif1_lut, cmucal_vclk_div_clk_aud_uaif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_FM, div_clk_aud_fm_lut, cmucal_vclk_div_clk_aud_fm, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF1_CMUREF, mux_mif1_cmuref_lut, cmucal_vclk_mux_mif1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_PLL_MIF1, pll_mif1_lut, cmucal_vclk_pll_mif1, NULL, vclk_switch_blk_mif1),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_SPI0, div_clk_peri_spi0_lut, cmucal_vclk_div_clk_peri_spi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_SPI2, div_clk_peri_spi2_lut, cmucal_vclk_div_clk_peri_spi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI_I2C, div_clk_peri_usi_i2c_lut, cmucal_vclk_div_clk_peri_usi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_SPI1, div_clk_peri_spi1_lut, cmucal_vclk_div_clk_peri_spi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI_USI, div_clk_peri_usi_usi_lut, cmucal_vclk_div_clk_peri_usi_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_SHUB_I2C, div_clk_shub_i2c_lut, cmucal_vclk_div_clk_shub_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_SHUB_USI00, div_clk_shub_usi00_lut, cmucal_vclk_div_clk_shub_usi00, NULL, NULL),

/* COMMON VCLK */
	CMUCAL_VCLK(VCLK_BLK_APM, blk_apm_lut, cmucal_vclk_blk_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CAM, blk_cam_lut, cmucal_vclk_blk_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMGP, blk_cmgp_lut, cmucal_vclk_blk_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMU, blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DISPAUD, blk_dispaud_lut, cmucal_vclk_blk_dispaud, NULL, vclk_switch_blk_dispaud),
	CMUCAL_VCLK(VCLK_BLK_G2D, blk_g2d_lut, cmucal_vclk_blk_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ISP, blk_isp_lut, cmucal_vclk_blk_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC, blk_mfc_lut, cmucal_vclk_blk_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERI, blk_peri_lut, cmucal_vclk_blk_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_SHUB, blk_shub_lut, cmucal_vclk_blk_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VIPX1, blk_vipx1_lut, cmucal_vclk_blk_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VIPX2, blk_vipx2_lut, cmucal_vclk_blk_vipx2, NULL, NULL),

/* GATING VCLK */
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APM_CMU_APM, NULL, cmucal_vclk_ip_apm_cmu_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM, NULL, cmucal_vclk_ip_lhm_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM_GNSS, NULL, cmucal_vclk_ip_lhm_axi_p_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM_MODEM, NULL, cmucal_vclk_ip_lhm_axi_p_apm_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM_SHUB, NULL, cmucal_vclk_ip_lhm_axi_p_apm_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM_WLBT, NULL, cmucal_vclk_ip_lhm_axi_p_apm_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_APM, NULL, cmucal_vclk_ip_lhs_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_SHUB, NULL, cmucal_vclk_ip_lhs_axi_lp_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP2CP, NULL, cmucal_vclk_ip_mailbox_ap2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP2CP_S, NULL, cmucal_vclk_ip_mailbox_ap2cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP2GNSS, NULL, cmucal_vclk_ip_mailbox_ap2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP2SHUB, NULL, cmucal_vclk_ip_mailbox_ap2shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP2WLBT, NULL, cmucal_vclk_ip_mailbox_ap2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM2AP, NULL, cmucal_vclk_ip_mailbox_apm2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM2CP, NULL, cmucal_vclk_ip_mailbox_apm2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM2GNSS, NULL, cmucal_vclk_ip_mailbox_apm2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM2SHUB, NULL, cmucal_vclk_ip_mailbox_apm2shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM2WLBT, NULL, cmucal_vclk_ip_mailbox_apm2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP2GNSS, NULL, cmucal_vclk_ip_mailbox_cp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP2SHUB, NULL, cmucal_vclk_ip_mailbox_cp2shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP2WLBT, NULL, cmucal_vclk_ip_mailbox_cp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_SHUB2GNSS, NULL, cmucal_vclk_ip_mailbox_shub2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_SHUB2WLBT, NULL, cmucal_vclk_ip_mailbox_shub2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT2ABOX, NULL, cmucal_vclk_ip_mailbox_wlbt2abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT2GNSS, NULL, cmucal_vclk_ip_mailbox_wlbt2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PEM, NULL, cmucal_vclk_ip_pem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_APM, NULL, cmucal_vclk_ip_pgen_lite_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPEEDY_APM, NULL, cmucal_vclk_ip_speedy_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_APM, NULL, cmucal_vclk_ip_sysreg_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_APM, NULL, cmucal_vclk_ip_wdt_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_APM, NULL, cmucal_vclk_ip_xiu_dp_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_CAM, NULL, cmucal_vclk_ip_blk_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_CAM, NULL, cmucal_vclk_ip_btm_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CAM_CMU_CAM, NULL, cmucal_vclk_ip_cam_cmu_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CAM, NULL, cmucal_vclk_ip_lhm_axi_p_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_CAM, NULL, cmucal_vclk_ip_lhs_acel_d_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_CAMISP, NULL, cmucal_vclk_ip_lhs_atb_camisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CAM, NULL, cmucal_vclk_ip_sysreg_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_is6p10p0_CAM, NULL, cmucal_vclk_ip_is6p10p0_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADC_CMGP, NULL, cmucal_vclk_ip_adc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP00, NULL, cmucal_vclk_ip_i2c_cmgp00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP01, NULL, cmucal_vclk_ip_i2c_cmgp01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP02, NULL, cmucal_vclk_ip_i2c_cmgp02, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP03, NULL, cmucal_vclk_ip_i2c_cmgp03, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP04, NULL, cmucal_vclk_ip_i2c_cmgp04, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_SHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2SHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2WLBT, NULL, cmucal_vclk_ip_sysreg_cmgp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP00, NULL, cmucal_vclk_ip_usi_cmgp00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP01, NULL, cmucal_vclk_ip_usi_cmgp01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP02, NULL, cmucal_vclk_ip_usi_cmgp02, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP03, NULL, cmucal_vclk_ip_usi_cmgp03, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP04, NULL, cmucal_vclk_ip_usi_cmgp04, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP, NULL, cmucal_vclk_ip_otp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CCI_550, NULL, cmucal_vclk_ip_ad_apb_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA0, NULL, cmucal_vclk_ip_ad_apb_pdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PGEN_PDMA, NULL, cmucal_vclk_ip_ad_apb_pgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PPFW_MEM0, NULL, cmucal_vclk_ip_ad_apb_ppfw_mem0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PPFW_MEM1, NULL, cmucal_vclk_ip_ad_apb_ppfw_mem1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PPFW_PERI, NULL, cmucal_vclk_ip_ad_apb_ppfw_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GIC, NULL, cmucal_vclk_ip_ad_axi_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCSFR_WR_DMC0, NULL, cmucal_vclk_ip_asyncsfr_wr_dmc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCSFR_WR_DMC1, NULL, cmucal_vclk_ip_asyncsfr_wr_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_A40_64to128_DIT, NULL, cmucal_vclk_ip_axi_us_a40_64to128_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_MODEM, NULL, cmucal_vclk_ip_baaw_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_SHUB, NULL, cmucal_vclk_ip_baaw_p_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_WLBT, NULL, cmucal_vclk_ip_baaw_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI_550, NULL, cmucal_vclk_ip_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC400_AIHWACG, NULL, cmucal_vclk_ip_gic400_aihwacg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_ISP, NULL, cmucal_vclk_ip_lhm_acel_d0_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_MFC, NULL, cmucal_vclk_ip_lhm_acel_d0_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_ISP, NULL, cmucal_vclk_ip_lhm_acel_d1_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_MFC, NULL, cmucal_vclk_ip_lhm_acel_d1_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_CAM, NULL, cmucal_vclk_ip_lhm_acel_d_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_DPU, NULL, cmucal_vclk_ip_lhm_acel_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_FSYS, NULL, cmucal_vclk_ip_lhm_acel_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_G2D, NULL, cmucal_vclk_ip_lhm_acel_d_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_USB, NULL, cmucal_vclk_ip_lhm_acel_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_VIPX1, NULL, cmucal_vclk_ip_lhm_acel_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_VIPX2, NULL, cmucal_vclk_ip_lhm_acel_d_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D_CPUCL0, NULL, cmucal_vclk_ip_lhm_ace_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D_CPUCL1, NULL, cmucal_vclk_ip_lhm_ace_d_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_ABOX, NULL, cmucal_vclk_ip_lhm_axi_d_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_d_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_G3D, NULL, cmucal_vclk_ip_lhm_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_GNSS, NULL, cmucal_vclk_ip_lhm_axi_d_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SHUB, NULL, cmucal_vclk_ip_lhm_axi_d_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_WLBT, NULL, cmucal_vclk_ip_lhm_axi_d_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CAM, NULL, cmucal_vclk_ip_lhs_axi_p_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL1, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DISPAUD, NULL, cmucal_vclk_ip_lhs_axi_p_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_FSYS, NULL, cmucal_vclk_ip_lhs_axi_p_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G2D, NULL, cmucal_vclk_ip_lhs_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_GNSS, NULL, cmucal_vclk_ip_lhs_axi_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ISP, NULL, cmucal_vclk_ip_lhs_axi_p_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFC, NULL, cmucal_vclk_ip_lhs_axi_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF0, NULL, cmucal_vclk_ip_lhs_axi_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhs_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MODEM, NULL, cmucal_vclk_ip_lhs_axi_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERI, NULL, cmucal_vclk_ip_lhs_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_SHUB, NULL, cmucal_vclk_ip_lhs_axi_p_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_USB, NULL, cmucal_vclk_ip_lhs_axi_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VIPX1, NULL, cmucal_vclk_ip_lhs_axi_p_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VIPX2, NULL, cmucal_vclk_ip_lhs_axi_p_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_WLBT, NULL, cmucal_vclk_ip_lhs_axi_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_CORE, NULL, cmucal_vclk_ip_pdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_SIREX, NULL, cmucal_vclk_ip_pgen_lite_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_PDMA, NULL, cmucal_vclk_ip_pgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPCFW_G3D, NULL, cmucal_vclk_ip_ppcfw_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPFW_CORE_MEM0, NULL, cmucal_vclk_ip_ppfw_core_mem0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPFW_CORE_MEM1, NULL, cmucal_vclk_ip_ppfw_core_mem1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPFW_CORE_PERI, NULL, cmucal_vclk_ip_ppfw_core_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL0, NULL, cmucal_vclk_ip_ppmu_ace_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL1, NULL, cmucal_vclk_ip_ppmu_ace_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APBIF_CMU_TOPC, NULL, cmucal_vclk_ip_sfr_apbif_cmu_topc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIREX, NULL, cmucal_vclk_ip_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA_CORE, NULL, cmucal_vclk_ip_spdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NRT, NULL, cmucal_vclk_ip_trex_d_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_CORE, NULL, cmucal_vclk_ip_trex_p_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CORE, NULL, cmucal_vclk_ip_xiu_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CSSYS_CORE, NULL, cmucal_vclk_ip_adm_apb_g_cssys_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_AHB_G_CSSYS_FSYS, NULL, cmucal_vclk_ip_ads_ahb_g_cssys_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_G_CSSYS_CPUCL1, NULL, cmucal_vclk_ip_ads_apb_g_cssys_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_G_P8Q, NULL, cmucal_vclk_ip_ads_apb_g_p8q, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_P_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_P_DUMP_PC_CPUCL1, NULL, cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMCPUCL0, NULL, cmucal_vclk_ip_busif_hpmcpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS_DBG, NULL, cmucal_vclk_ip_cssys_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DUMP_PC_CPUCL1, NULL, cmucal_vclk_ip_dump_pc_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0, NULL, cmucal_vclk_ip_hpm_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_d_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CSSYS_CPUCL1, NULL, cmucal_vclk_ip_adm_apb_g_cssys_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMCPUCL1, NULL, cmucal_vclk_ip_busif_hpmcpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL1, NULL, cmucal_vclk_ip_hpm_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL1, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D_CPUCL1, NULL, cmucal_vclk_ip_lhs_ace_d_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL1, NULL, cmucal_vclk_ip_sysreg_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_32to128, NULL, cmucal_vclk_ip_axi_us_32to128, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_DISPAUD, NULL, cmucal_vclk_ip_blk_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_ABOX, NULL, cmucal_vclk_ip_btm_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_DPU, NULL, cmucal_vclk_ip_btm_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_DISPAUD, NULL, cmucal_vclk_ip_dftmux_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DISPAUD_CMU_DISPAUD, NULL, cmucal_vclk_ip_dispaud_cmu_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_DISPAUD, NULL, cmucal_vclk_ip_gpio_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DISPAUD, NULL, cmucal_vclk_ip_lhm_axi_p_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_DPU, NULL, cmucal_vclk_ip_lhs_acel_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_ABOX, NULL, cmucal_vclk_ip_lhs_axi_d_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ABOX, NULL, cmucal_vclk_ip_ppmu_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPU, NULL, cmucal_vclk_ip_ppmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_ABOX, NULL, cmucal_vclk_ip_smmu_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_DPU, NULL, cmucal_vclk_ip_smmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DISPAUD, NULL, cmucal_vclk_ip_sysreg_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_AHB_SSS, NULL, cmucal_vclk_ip_adm_ahb_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_FSYS, NULL, cmucal_vclk_ip_btm_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_FSYS_CMU_FSYS, NULL, cmucal_vclk_ip_fsys_cmu_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_FSYS, NULL, cmucal_vclk_ip_gpio_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_FSYS, NULL, cmucal_vclk_ip_lhm_axi_p_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_FSYS, NULL, cmucal_vclk_ip_lhs_acel_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_EMBD, NULL, cmucal_vclk_ip_mmc_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_FSYS, NULL, cmucal_vclk_ip_pgen_lite_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_FSYS, NULL, cmucal_vclk_ip_ppmu_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_FSYS, NULL, cmucal_vclk_ip_sysreg_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_FSYS, NULL, cmucal_vclk_ip_xiu_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_JPEG, NULL, cmucal_vclk_ip_as_axi_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_MSCL, NULL, cmucal_vclk_ip_as_axi_mscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_G2D, NULL, cmucal_vclk_ip_blk_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_G2D, NULL, cmucal_vclk_ip_btm_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D, NULL, cmucal_vclk_ip_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D_CMU_G2D, NULL, cmucal_vclk_ip_g2d_cmu_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG, NULL, cmucal_vclk_ip_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G2D, NULL, cmucal_vclk_ip_lhm_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_G2D, NULL, cmucal_vclk_ip_lhs_acel_d_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MSCL, NULL, cmucal_vclk_ip_mscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN100_LITE_G2D, NULL, cmucal_vclk_ip_pgen100_lite_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G2D, NULL, cmucal_vclk_ip_ppmu_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G2D, NULL, cmucal_vclk_ip_sysmmu_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G2D, NULL, cmucal_vclk_ip_sysreg_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MSCL, NULL, cmucal_vclk_ip_xiu_d_mscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_G3D, NULL, cmucal_vclk_ip_btm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMG3D, NULL, cmucal_vclk_ip_busif_hpmg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D, NULL, cmucal_vclk_ip_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_G3D, NULL, cmucal_vclk_ip_hpm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G3DSFR, NULL, cmucal_vclk_ip_lhm_axi_g3dsfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_G3D, NULL, cmucal_vclk_ip_lhs_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G3DSFR, NULL, cmucal_vclk_ip_lhs_axi_g3dsfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_G3D, NULL, cmucal_vclk_ip_pgen_lite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_ISP, NULL, cmucal_vclk_ip_blk_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_ISP0, NULL, cmucal_vclk_ip_btm_isp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_ISP1, NULL, cmucal_vclk_ip_btm_isp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ISP_CMU_ISP, NULL, cmucal_vclk_ip_isp_cmu_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_CAMISP, NULL, cmucal_vclk_ip_lhm_atb_camisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ISP, NULL, cmucal_vclk_ip_lhm_axi_p_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_ISP, NULL, cmucal_vclk_ip_lhs_acel_d0_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_ISP, NULL, cmucal_vclk_ip_lhs_acel_d1_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ISP, NULL, cmucal_vclk_ip_sysreg_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_is6p10p0_ISP, NULL, cmucal_vclk_ip_is6p10p0_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_WFD, NULL, cmucal_vclk_ip_as_axi_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_MFC, NULL, cmucal_vclk_ip_blk_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_MFCD0, NULL, cmucal_vclk_ip_btm_mfcd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_MFCD1, NULL, cmucal_vclk_ip_btm_mfcd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFC, NULL, cmucal_vclk_ip_lhm_axi_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_MFC, NULL, cmucal_vclk_ip_lhs_acel_d0_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_MFC, NULL, cmucal_vclk_ip_lhs_acel_d1_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MFC, NULL, cmucal_vclk_ip_lh_atb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC, NULL, cmucal_vclk_ip_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC_CMU_MFC, NULL, cmucal_vclk_ip_mfc_cmu_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN100_LITE_MFC, NULL, cmucal_vclk_ip_pgen100_lite_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCD0, NULL, cmucal_vclk_ip_ppmu_mfcd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCD1, NULL, cmucal_vclk_ip_ppmu_mfcd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCD0, NULL, cmucal_vclk_ip_sysmmu_mfcd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCD1, NULL, cmucal_vclk_ip_sysmmu_mfcd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC, NULL, cmucal_vclk_ip_sysreg_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WFD, NULL, cmucal_vclk_ip_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFC, NULL, cmucal_vclk_ip_xiu_d_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMMIF, NULL, cmucal_vclk_ip_busif_hpmmif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDR_PHY, NULL, cmucal_vclk_ip_ddr_phy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_MIF, NULL, cmucal_vclk_ip_hpm_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CP, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CPU, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_NRT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_RT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF, NULL, cmucal_vclk_ip_lhm_axi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DMC_CPU, NULL, cmucal_vclk_ip_ppmu_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_DMC_CPU, NULL, cmucal_vclk_ip_qe_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DDR_PHY, NULL, cmucal_vclk_ip_sfrapb_bridge_ddr_phy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PF, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_pf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PPMPU, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_SECURE, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMMIF1, NULL, cmucal_vclk_ip_busif_hpmmif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC1, NULL, cmucal_vclk_ip_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_MIF1, NULL, cmucal_vclk_ip_hpm_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_CP, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_CPU, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_NRT, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_RT, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF1_CMU_MIF1, NULL, cmucal_vclk_ip_mif1_cmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2AHB_MSD32_PERI, NULL, cmucal_vclk_ip_axi2ahb_msd32_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_TMU, NULL, cmucal_vclk_ip_busif_tmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CAMI2C_0, NULL, cmucal_vclk_ip_cami2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CAMI2C_1, NULL, cmucal_vclk_ip_cami2c_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CAMI2C_2, NULL, cmucal_vclk_ip_cami2c_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CAMI2C_3, NULL, cmucal_vclk_ip_cami2c_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERI, NULL, cmucal_vclk_ip_gpio_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_0, NULL, cmucal_vclk_ip_i2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_1, NULL, cmucal_vclk_ip_i2c_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_2, NULL, cmucal_vclk_ip_i2c_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_3, NULL, cmucal_vclk_ip_i2c_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_4, NULL, cmucal_vclk_ip_i2c_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_5, NULL, cmucal_vclk_ip_i2c_5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_6, NULL, cmucal_vclk_ip_i2c_6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERI, NULL, cmucal_vclk_ip_lhm_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_CMU_PERI, NULL, cmucal_vclk_ip_peri_cmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_MOTOR, NULL, cmucal_vclk_ip_pwm_motor, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_0, NULL, cmucal_vclk_ip_spi_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_1, NULL, cmucal_vclk_ip_spi_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_2, NULL, cmucal_vclk_ip_spi_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERI, NULL, cmucal_vclk_ip_sysreg_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UART, NULL, cmucal_vclk_ip_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_I2C, NULL, cmucal_vclk_ip_usi00_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_USI, NULL, cmucal_vclk_ip_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CLUSTER0, NULL, cmucal_vclk_ip_wdt_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CLUSTER1, NULL, cmucal_vclk_ip_wdt_cluster1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_SHUB, NULL, cmucal_vclk_ip_baaw_d_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_APM_SHUB, NULL, cmucal_vclk_ip_baaw_p_apm_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_SHUB, NULL, cmucal_vclk_ip_cm4_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_SHUB, NULL, cmucal_vclk_ip_gpio_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_SHUB00, NULL, cmucal_vclk_ip_i2c_shub00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_SHUB, NULL, cmucal_vclk_ip_lhm_axi_lp_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_SHUB, NULL, cmucal_vclk_ip_lhm_axi_p_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_SHUB, NULL, cmucal_vclk_ip_lhs_axi_d_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM_SHUB, NULL, cmucal_vclk_ip_lhs_axi_p_apm_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_SHUB, NULL, cmucal_vclk_ip_pdma_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_SHUB, NULL, cmucal_vclk_ip_pwm_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SHUB_CMU_SHUB, NULL, cmucal_vclk_ip_shub_cmu_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_SHUB, NULL, cmucal_vclk_ip_sweeper_d_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_P_APM_SHUB, NULL, cmucal_vclk_ip_sweeper_p_apm_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SHUB, NULL, cmucal_vclk_ip_sysreg_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_SHUB, NULL, cmucal_vclk_ip_timer_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_SHUB00, NULL, cmucal_vclk_ip_usi_shub00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_SHUB, NULL, cmucal_vclk_ip_wdt_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_SHUB, NULL, cmucal_vclk_ip_xiu_dp_shub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_USB, NULL, cmucal_vclk_ip_btm_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DP_LINK, NULL, cmucal_vclk_ip_dp_link, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_USB, NULL, cmucal_vclk_ip_lhm_axi_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_USB, NULL, cmucal_vclk_ip_lhs_acel_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_USB, NULL, cmucal_vclk_ip_pgen_lite_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_USB, NULL, cmucal_vclk_ip_ppmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_USB, NULL, cmucal_vclk_ip_sysreg_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB30DRD, NULL, cmucal_vclk_ip_usb30drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB_CMU_USB, NULL, cmucal_vclk_ip_usb_cmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_US_D_USB, NULL, cmucal_vclk_ip_us_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_VIPX1, NULL, cmucal_vclk_ip_blk_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_D_VIPX1, NULL, cmucal_vclk_ip_btm_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_VIPX1, NULL, cmucal_vclk_ip_lhm_atb_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VIPX1, NULL, cmucal_vclk_ip_lhm_axi_p_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_VIPX1, NULL, cmucal_vclk_ip_lhs_acel_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_VIPX1, NULL, cmucal_vclk_ip_lhs_atb_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VIPX1_LOCAL, NULL, cmucal_vclk_ip_lhs_axi_p_vipx1_local, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_VIPX1, NULL, cmucal_vclk_ip_pgen_lite_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_VIPX1, NULL, cmucal_vclk_ip_ppmu_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_D_VIPX1, NULL, cmucal_vclk_ip_smmu_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VIPX1, NULL, cmucal_vclk_ip_sysreg_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VIPX1, NULL, cmucal_vclk_ip_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VIPX1_CMU_VIPX1, NULL, cmucal_vclk_ip_vipx1_cmu_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_VIPX1, NULL, cmucal_vclk_ip_xiu_d_vipx1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BLK_VIPX2, NULL, cmucal_vclk_ip_blk_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BTM_D_VIPX2, NULL, cmucal_vclk_ip_btm_d_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_VIPX2, NULL, cmucal_vclk_ip_lhm_atb_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VIPX2, NULL, cmucal_vclk_ip_lhm_axi_p_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VIPX2_LOCAL, NULL, cmucal_vclk_ip_lhm_axi_p_vipx2_local, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_VIPX2, NULL, cmucal_vclk_ip_lhs_acel_d_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_VIPX2, NULL, cmucal_vclk_ip_lhs_atb_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PGEN_LITE_VIPX2, NULL, cmucal_vclk_ip_pgen_lite_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_VIPX2, NULL, cmucal_vclk_ip_ppmu_d_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_D_VIPX2, NULL, cmucal_vclk_ip_smmu_d_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VIPX2, NULL, cmucal_vclk_ip_sysreg_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VIPX2, NULL, cmucal_vclk_ip_vipx2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VIPX2_CMU_VIPX2, NULL, cmucal_vclk_ip_vipx2_cmu_vipx2, NULL, NULL),
};
unsigned int cmucal_vclk_size = 440;
