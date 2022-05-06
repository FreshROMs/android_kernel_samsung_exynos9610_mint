
enum acpm_dvfs_id {
	dvfs_mif = ACPM_VCLK_TYPE,
	dvfs_int,
	dvfs_cpucl0,
	dvfs_cpucl1,
	dvfs_g3d,
	dvfs_intcam,
	dvfs_cam,
	dvfs_disp,
	dvfs_aud,
	dvs_cp,
};

struct vclk acpm_vclk_list[] = {
	CMUCAL_ACPM_VCLK2(dvfs_mif, NULL, NULL, NULL, NULL, MARGIN_MIF),
	CMUCAL_ACPM_VCLK2(dvfs_int, NULL, NULL, NULL, NULL, MARGIN_INT),
	CMUCAL_ACPM_VCLK2(dvfs_cpucl0, NULL, NULL, NULL, NULL, MARGIN_LIT),
	CMUCAL_ACPM_VCLK2(dvfs_cpucl1, NULL, NULL, NULL, NULL, MARGIN_BIG),
	CMUCAL_ACPM_VCLK2(dvfs_g3d, NULL, NULL, NULL, NULL, MARGIN_G3D),
	CMUCAL_ACPM_VCLK2(dvfs_intcam, NULL, NULL, NULL, NULL, MARGIN_INTCAM),
	CMUCAL_ACPM_VCLK2(dvfs_cam, NULL, NULL, NULL, NULL, MARGIN_CAM),
	CMUCAL_ACPM_VCLK2(dvfs_disp, NULL, NULL, NULL, NULL, MARGIN_DISP),
	CMUCAL_ACPM_VCLK2(dvfs_aud, NULL, NULL, NULL, NULL, MARGIN_AUD),
	CMUCAL_ACPM_VCLK2(dvs_cp, NULL, NULL, NULL, NULL, MARGIN_CP),
};

unsigned int acpm_vclk_size = ARRAY_SIZE(acpm_vclk_list);
