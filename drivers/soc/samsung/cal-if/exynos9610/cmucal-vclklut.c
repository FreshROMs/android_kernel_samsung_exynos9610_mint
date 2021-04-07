#include "../cmucal.h"

#include "cmucal-vclklut.h"

/*=================CMUCAL version: S5E9610================================*/

/*=================LUT in each VCLK================================*/
unsigned int vdd_cpucl0_nm_lut_params[] = {
	 1049750,
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	 1449500,
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	 1850333,
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	 300083,
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	 600166,
};
unsigned int vdd_cpucl1_nm_lut_params[] = {
	 1, 1499333,
};
unsigned int vdd_cpucl1_od_lut_params[] = {
	 1, 1898000,
};
unsigned int vdd_cpucl1_sod_lut_params[] = {
	 1, 2400666,
};
unsigned int vdd_cpucl1_sud_lut_params[] = {
	 0, 549899,
};
unsigned int vdd_cpucl1_ud_lut_params[] = {
	 0, 850200,
};
unsigned int vdd_g3d_nm_lut_params[] = {
	 750000,
};
unsigned int vdd_g3d_od_lut_params[] = {
	 1000000,
};
unsigned int vdd_g3d_sod_lut_params[] = {
	 1200000,
};
unsigned int vdd_g3d_sud_lut_params[] = {
	 300000,
};
unsigned int vdd_g3d_ud_lut_params[] = {
	 550000,
};
unsigned int vdd_int_nm_lut_params[] = {
	 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 3, 1,
};
unsigned int vdd_int_od_lut_params[] = {
	 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1,
};
unsigned int vdd_int_sud_lut_params[] = {
	 1, 7, 1, 1, 1, 1, 2, 2, 3, 3, 1, 1, 1, 1, 3, 1, 1, 3, 3, 2, 1, 0, 0, 0, 3, 0, 0, 3, 3, 3, 2, 1, 2, 3, 3, 2, 0,
};
unsigned int vdd_int_ud_lut_params[] = {
	 0, 3, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 3, 3, 0, 2, 2, 3, 0, 0, 3, 3, 3, 3, 0, 2, 3, 3, 2, 0,
};
unsigned int vdd_cam_nm_lut_params[] = {
	 23,
};
unsigned int vdd_cam_od_lut_params[] = {
	 23,
};
unsigned int vdd_cam_sud_lut_params[] = {
	 9,
};
unsigned int vdd_cam_ud_lut_params[] = {
	 14,
};
unsigned int vdd_mif_nm_lut_params[] = {
	 4264000,
};
unsigned int vdd_mif_sud_lut_params[] = {
	 1399666,
};
unsigned int vdd_mif_ud_lut_params[] = {
	 1332500,
};
unsigned int spl_clk_shub_i2c_blk_apm_nm_lut_params[] = {
	 0, 1,
};
unsigned int mux_clk_cmgp_adc_blk_cmgp_nm_lut_params[] = {
	 1, 11,
};
unsigned int spl_clk_cmgp_usi01_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_usi03_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_usi02_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_usi00_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_usi04_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_fsys_ufs_embd_blk_cmu_nm_lut_params[] = {
	 2, 1,
};
unsigned int occ_cmu_cmuref_blk_cmu_nm_lut_params[] = {
	 1, 1, 0,
};
unsigned int clkcmu_hpm_blk_cmu_nm_lut_params[] = {
	 0, 1,
};
unsigned int spl_clk_peri_spi0_blk_cmu_nm_lut_params[] = {
	 0, 1,
};
unsigned int occ_mif_cmuref_blk_cmu_nm_lut_params[] = {
	 1, 0,
};
unsigned int spl_clk_shub_i2c_blk_cmu_nm_lut_params[] = {
	 0, 0,
};
unsigned int clkcmu_cis_clk1_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int clkcmu_cis_clk3_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int spl_clk_usb_usb30drd_blk_cmu_nm_lut_params[] = {
	 7, 1,
};
unsigned int clkcmu_cis_clk0_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int spl_clk_usb_dpgtc_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int clkcmu_cis_clk2_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int spl_clk_peri_uart_blk_cmu_nm_lut_params[] = {
	 1, 1,
};
unsigned int div_clk_cluster0_pclkdbg_blk_cpucl0_nm_lut_params[] = {
	 7,
};
unsigned int div_clk_cluster0_pclkdbg_blk_cpucl0_od_lut_params[] = {
	 7,
};
unsigned int div_clk_cluster0_pclkdbg_blk_cpucl0_sod_lut_params[] = {
	 7,
};
unsigned int div_clk_cluster0_pclkdbg_blk_cpucl0_sud_lut_params[] = {
	 7,
};
unsigned int div_clk_cluster0_pclkdbg_blk_cpucl0_ud_lut_params[] = {
	 7,
};
unsigned int div_clk_cluster0_aclk_blk_cpucl0_nm_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_aclk_blk_cpucl0_od_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_aclk_blk_cpucl0_sod_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_aclk_blk_cpucl0_sud_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_aclk_blk_cpucl0_ud_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_od_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_sod_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_sud_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_ud_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_nm_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_od_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_sod_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_sud_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_ud_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster1_cntclk_blk_cpucl1_nm_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster1_cntclk_blk_cpucl1_od_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster1_cntclk_blk_cpucl1_sod_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster1_cntclk_blk_cpucl1_sud_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster1_cntclk_blk_cpucl1_ud_lut_params[] = {
	 3,
};
unsigned int spl_clk_cpucl1_cmuref_blk_cpucl1_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl1_cmuref_blk_cpucl1_od_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl1_cmuref_blk_cpucl1_sod_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl1_cmuref_blk_cpucl1_sud_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl1_cmuref_blk_cpucl1_ud_lut_params[] = {
	 1,
};
unsigned int spl_clk_aud_dsif_blk_dispaud_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_aud_dsif_blk_dispaud_od_lut_params[] = {
	 1,
};
unsigned int spl_clk_aud_dsif_blk_dispaud_sud_lut_params[] = {
	 1,
};
unsigned int spl_clk_aud_dsif_blk_dispaud_ud_lut_params[] = {
	 1,
};
unsigned int dft_clk_aud_uaif0_blk_dispaud_nm_lut_params[] = {
	 0, 1,
};
unsigned int dft_clk_aud_uaif2_blk_dispaud_nm_lut_params[] = {
	 0, 1,
};
unsigned int spl_clk_aud_cpu_pclkdbg_blk_dispaud_nm_lut_params[] = {
	 7,
};
unsigned int spl_clk_aud_cpu_pclkdbg_blk_dispaud_od_lut_params[] = {
	 7,
};
unsigned int dft_clk_aud_uaif1_blk_dispaud_nm_lut_params[] = {
	 0, 1,
};
unsigned int dft_clk_aud_fm_blk_dispaud_nm_lut_params[] = {
	 0, 1, 1,
};
unsigned int occ_mif_cmuref_blk_mif_nm_lut_params[] = {
	 1,
};
unsigned int occ_mif1_cmuref_blk_mif1_nm_lut_params[] = {
	 0,
};
unsigned int clk_mif1_busd_blk_mif1_nm_lut_params[] = {
	 100000,
};
unsigned int spl_clk_peri_spi0_blk_peri_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_peri_spi2_blk_peri_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_peri_usi_i2c_blk_peri_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_peri_spi1_blk_peri_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_peri_usi_usi_blk_peri_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_shub_i2c_blk_shub_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_shub_usi00_blk_shub_nm_lut_params[] = {
	 0, 1,
};
unsigned int blk_apm_lut_params[] = {
	 0, 1,
};
unsigned int blk_cam_lut_params[] = {
	 1,
};
unsigned int blk_cmgp_lut_params[] = {
	 1, 1,
};
unsigned int blk_cmu_lut_params[] = {
	 0, 5, 0, 1, 5, 0, 5, 1, 1, 1, 2, 1, 1, 1599000, 2, 799999, 1332500,
};
unsigned int blk_core_lut_params[] = {
	 1, 0,
};
unsigned int blk_cpucl0_lut_params[] = {
	 7,
};
unsigned int blk_cpucl1_lut_params[] = {
	 7, 7,
};
unsigned int blk_dispaud_lut_params[] = {
	 1, 1, 0, 1, 1179648, 0, 0,
};
unsigned int blk_g2d_lut_params[] = {
	 1,
};
unsigned int blk_g3d_lut_params[] = {
	 3,
};
unsigned int blk_isp_lut_params[] = {
	 2,
};
unsigned int blk_mfc_lut_params[] = {
	 1,
};
unsigned int blk_peri_lut_params[] = {
	 1,
};
unsigned int blk_shub_lut_params[] = {
	 0, 1,
};
unsigned int blk_vipx1_lut_params[] = {
	 1,
};
unsigned int blk_vipx2_lut_params[] = {
	 1,
};
