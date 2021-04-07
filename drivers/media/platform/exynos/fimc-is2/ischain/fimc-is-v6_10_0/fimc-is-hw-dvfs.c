/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos fimc-is2 core functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

/* for DT parsing */
DECLARE_DVFS_DT(FIMC_IS_SN_END,
		{"default_"		           , FIMC_IS_SN_DEFAULT},
		{"secure_front_"                   , FIMC_IS_SN_SECURE_FRONT},
		{"front_preview_"                  , FIMC_IS_SN_FRONT_PREVIEW},
		{"front_preview_full_"             , FIMC_IS_SN_FRONT_PREVIEW_FULL},
		{"front_preview_remosaic_"         , FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC},
		{"front_capture_"                  , FIMC_IS_SN_FRONT_CAPTURE},
		{"front_capture_remosaic_"         , FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC},
		{"front_video_"                    , FIMC_IS_SN_FRONT_CAMCORDING},
		{"front_video_capture_"            , FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE},
		{"front_wide_selfie_" 	   , FIMC_IS_SN_FRONT_WIDE_SELFIE},
		{"front_vt1_"                      , FIMC_IS_SN_FRONT_VT1},
		{"front_vt2_"                      , FIMC_IS_SN_FRONT_VT2},
		{"front_vt4_"                      , FIMC_IS_SN_FRONT_VT4},
#ifdef SUPPORT_FRONT_SM_DVFS
		{"front_video_high_speed_120fps_"  , FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS},
#endif
		{"rear2_preview_fhd_"              , FIMC_IS_SN_REAR2_PREVIEW_FHD},
		{"rear2_capture_"                  , FIMC_IS_SN_REAR2_CAPTURE},
		{"rear2_video_fhd_"                , FIMC_IS_SN_REAR2_CAMCORDING_FHD},
		{"rear2_video_fhd_capture_"        , FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE},
		{"rear3_preview_"                  , FIMC_IS_SN_REAR3_PREVIEW},
		{"rear3_capture_"                  , FIMC_IS_SN_REAR3_CAPTURE},
		{"rear3_video_"                    , FIMC_IS_SN_REAR3_CAMCORDING},
		{"rear3_video_capture_"            , FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE},
		{"rear_preview_full_"              , FIMC_IS_SN_REAR_PREVIEW_FULL},
		{"rear_preview_fhd_"               , FIMC_IS_SN_REAR_PREVIEW_FHD},
		{"rear_preview_hd_"                , FIMC_IS_SN_REAR_PREVIEW_HD},
		{"rear_preview_uhd_"               , FIMC_IS_SN_REAR_PREVIEW_UHD},
		{"rear_preview_remosaic_"	  , FIMC_IS_SN_REAR_PREVIEW_REMOSAIC},
		{"rear_capture_"                   , FIMC_IS_SN_REAR_CAPTURE},
		{"rear_capture_remosaic_"          , FIMC_IS_SN_REAR_CAPTURE_REMOSAIC},
		{"rear_video_fhd_"                 , FIMC_IS_SN_REAR_CAMCORDING_FHD},
		{"rear_video_hd_"				   , FIMC_IS_SN_REAR_CAMCORDING_HD},
		{"rear_video_uhd_"                 , FIMC_IS_SN_REAR_CAMCORDING_UHD},
		{"rear_video_fhd_capture_"         , FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE},
		{"rear_video_hd_capture_"		   , FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE},
		{"rear_video_uhd_capture_"         , FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE},
		{"dual_preview_"			, FIMC_IS_SN_DUAL_PREVIEW},
		{"dual_capture_"			, FIMC_IS_SN_DUAL_CAPTURE},
		{"dual_video_fhd_"			, FIMC_IS_SN_DUAL_FHD_CAMCORDING},
		{"dual_video_fhd_capture_"		, FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE},
		{"dual_video_uhd_"			, FIMC_IS_SN_DUAL_UHD_CAMCORDING},
		{"dual_video_uhd_capture_"		, FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE},
		{"dual_sync_preview_"			, FIMC_IS_SN_DUAL_SYNC_PREVIEW},
		{"dual_sync_capture_"			, FIMC_IS_SN_DUAL_SYNC_CAPTURE},
		{"dual_sync_preview_whd_"		, FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD},
		{"dual_sync_whd_capture_"		, FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE},
		{"dual_sync_video_fhd_"			, FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING},
		{"dual_sync_video_fhd_capture_"		, FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE},
		{"dual_sync_video_uhd_"			, FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING},
		{"dual_sync_video_uhd_capture_"		, FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE},
		{"preview_high_speed_fps_"         , FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS},
		{"video_high_speed_60fps_"         , FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS},
		{"video_high_speed_120fps_"        , FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS},
		{"video_high_speed_240fps_"        , FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS},
		{"video_high_speed_480fps_" 	   , FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS},
		{"video_high_speed_60fps_capture_" , FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE},
		{"ext_rear_"		     , FIMC_IS_SN_EXT_REAR},
		{"ext_front_"		     , FIMC_IS_SN_EXT_FRONT},
		{"ext_secure_"			 , FIMC_IS_SN_EXT_SECURE},
		{"max_"                            , FIMC_IS_SN_MAX});

/* dvfs scenario check logic data */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_SECURE_FRONT);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_FULL);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_WIDE_SELFIE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT4);
#ifdef SUPPORT_FRONT_SM_DVFS
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS);
#endif

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_PREVIEW_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FULL);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_HD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_REMOSAIC);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE_REMOSAIC);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE);

/* external isp's dvfs */
DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_REAR);
DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_FRONT);
DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_SECURE);


#if defined(ENABLE_DVFS)
/*
 * Static Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'static_scenarios'
 */

struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_SECURE_FRONT,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_SECURE_FRONT),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_SECURE_FRONT),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_FHD_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_FHD_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_UHD_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_UHD_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_HD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_HD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_REMOSAIC,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_REMOSAIC),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_REMOSAIC),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_FULL,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_FULL),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FULL),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_HD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_HD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_HD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR2_CAMCORDING_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR2_CAMCORDING_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR2_PREVIEW_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR2_PREVIEW_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_PREVIEW_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR3_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR3_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR3_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR3_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT1,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT1),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT2,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT2),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT4,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT4),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT4),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW_FULL,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW_FULL),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_FULL),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC),
	}, 
#ifdef SUPPORT_FRONT_SM_DVFS
	{
		.scenario_id		= FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS),
	},
#endif
	{
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_WIDE_SELFIE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_WIDE_SELFIE),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_WIDE_SELFIE),
	}
};

/*
 * Dynamic Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'dynamic_scenarios'
 */
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_DUAL_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR2_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR2_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR3_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR3_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE_REMOSAIC,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE_REMOSAIC),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE_REMOSAIC),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAPTURE),
		.keep_frame_tick	= FIMC_IS_DVFS_CAPTURE_TICK,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE),
	},
};

/*
 * External Sensor/Vision Scenario Set
 * You should describe external scenario by priorities of scenario.
 * And you should name array 'external_scenarios'
 */
struct fimc_is_dvfs_scenario external_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_EXT_REAR,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_EXT_REAR),
		.ext_check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_REAR),
	}, {
		.scenario_id		= FIMC_IS_SN_EXT_FRONT,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_EXT_FRONT),
		.ext_check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_FRONT),
	}, {
		.scenario_id		= FIMC_IS_SN_EXT_SECURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_EXT_SECURE),
		.ext_check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_SECURE),
	},
};
#else
/*
 * Default Scenario can not be seleted, this declaration is for static variable.
 */
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};

static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};

static struct fimc_is_dvfs_scenario external_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.ext_check_func		= NULL,
	},
};
#endif

/* fastAE */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_FHD_60FPS) ||
		(mask == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED) ||
		(mask == ISS_SUB_SCENARIO_FHD_240FPS);

	if ((fps > 30) && !setfile_flag)
		return 1;
	else
		return 0;
}

/* secure front */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_SECURE_FRONT)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENARIO_COLOR_IRIS);

	if (scenario_flag && stream_cnt > 1)
		return 1;
	else
		return 0;
}

/* 60fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_FHD_60FPS);

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps >= 60) &&
			(fps < 120) && setfile_flag)
		return 1;
	else
		return 0;
}

/* 120fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED);

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps > 60) &&
			(fps <= 120) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

#ifdef SUPPORT_FRONT_SM_DVFS
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	/* It uses same setfile scenario index for every high speed recording mode. */
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED);

	if (position == SENSOR_POSITION_FRONT &&
			(fps > 60) &&
			(fps <= 120) &&
			setfile_flag)
		return 1;
	else
		return 0;
}
#endif

/* 240fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_FHD_240FPS);

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps > 120) &&
			(fps <= 240) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* 480fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_480FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_FHD_240FPS);

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps > 240) &&
			(fps <= 480) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear camcording FHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_SW_VDIS_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR) &&
			(fps <= 30) &&
			(resol <= SIZE_12MP_FHD_BDS) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear camcording HD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR) &&
			(fps <= 30) &&
			(resol <= SIZE_HD) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear camcording UHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_UHD_30FPS) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON));

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps <= 30) &&
			(resol > SIZE_12MP_QHD_BDS) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear full resolution preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FULL)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENARIO_FULL_SIZE);

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
		(scenario_flag))
		return 1;
	else
		return 0;
}

/* rear preview FHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR) &&
			(fps <= 30) &&
			(resol <= SIZE_12MP_FHD_BDS) &&
			(!setfile_flag))

		return 1;
	else
		return 0;
}

/* rear preview HD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_HD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR) &&
			(fps <= 30) &&
			(resol <= SIZE_HD) &&
			(!setfile_flag))

		return 1;
	else
		return 0;
}

/* rear preview UHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_UHD_30FPS) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON));

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
			(fps <= 30) &&
			(resol > SIZE_12MP_QHD_BDS) &&
			(!setfile_flag))
		return 1;
	else
		return 0;
}

/* rear preview remosaic */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_REMOSAIC)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENAIRO_REMOSAIC);

	if ((position == SENSOR_POSITION_REAR) &&
			(fps <= 30) &&
			(scenario_flag))
		return 1;
	else
		return 0;
}

/* dual fhd camcording sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		setfile_flag &&
		(stream_cnt > 1) &&
#ifdef BDS_IN_VIDEO
		(resol <= SIZE_12MP_FHD_BDS) &&
#endif
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual fhd camcording capture sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_SYNC_FHD_CAMCORDING) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual uhd camcording sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_UHD_30FPS) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_AUTO));

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		setfile_flag &&
		(stream_cnt > 1) &&
#ifdef BDS_IN_VIDEO
		(resol > SIZE_12MP_FHD_BDS) &&
#endif
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual uhd capcording capture sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_SYNC_UHD_CAMCORDING) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual preview sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW)
{
	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(stream_cnt > 1) && (resol <= SIZE_12MP_FHD_BDS) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual preview capture sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_SYNC_PREVIEW) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual whd preview sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD)
{
	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(stream_cnt > 1) && (resol > SIZE_12MP_FHD_BDS) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual whd preview capture sync */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_SYNC_WHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_SYNC_PREVIEW_WHD) &&
		(dual_info->mode == FIMC_IS_DUAL_MODE_SYNC))
		return 1;
	else
		return 0;
}

/* dual fhd camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		setfile_flag &&
#ifdef BDS_IN_VIDEO
		(resol <= SIZE_12MP_FHD_BDS) &&
#endif
		(stream_cnt > 1))
		return 1;
	else
		return 0;
}

/* dual fhd camcording capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_FHD_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_FHD_CAMCORDING))
		return 1;
	else
		return 0;
}

/* dual uhd camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_UHD_30FPS) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR_AUTO));

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		setfile_flag &&
#ifdef BDS_IN_VIDEO
		(resol <= SIZE_12MP_FHD_BDS) &&
#endif
		(stream_cnt > 1))
		return 1;
	else
		return 0;
}

/* dual uhd camcording capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_UHD_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_UHD_CAMCORDING))
		return 1;
	else
		return 0;
}

/* dual preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW)
{
	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		stream_cnt > 1)
		return 1;
	else
		return 0;
}

/* dual preview capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((test_bit(SENSOR_POSITION_REAR, &sensor_map)) &&
		(test_bit(SENSOR_POSITION_REAR2, &sensor_map)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_DUAL_PREVIEW))
		return 1;
	else
		return 0;
}


/* rear2 preview FHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_PREVIEW_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR2) &&
			(fps <= 30) &&
			(resol <= SIZE_12MP_FHD_BDS) &&
			(!setfile_flag))

		return 1;
	else
		return 0;
}

/* rear2 camcording FHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR2) &&
			(fps <= 30) &&
#ifdef BDS_IN_VIDEO
			(resol <= SIZE_12MP_FHD_BDS) &&
#endif
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear2 capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAPTURE)
{
	if ((position == SENSOR_POSITION_REAR2) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR2_CAMCORDING_FHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR2) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_REAR_CAMCORDING_FHD)
		)
		return 1;
	else
		return 0;
}

/* REAR3 preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_PREVIEW)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR3) &&
			(fps <= 30) &&
			(!setfile_flag))

		return 1;
	else
		return 0;
}

/* REAR3 capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAPTURE)
{
	if ((position == SENSOR_POSITION_REAR3) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)
		)
		return 1;
	else
		return 0;
}

/* REAR3 camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_SW_VDIS_WDR_AUTO));

	if ((position == SENSOR_POSITION_REAR3) &&
			(fps <= 30) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* REAR3 camcording capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR3_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR3) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_REAR3_CAMCORDING)
		)
		return 1;
	else
		return 0;
}

/* front vt1 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1)
{
	if ((position == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT1))
		return 1;
	else
		return 0;
}

/* front vt2 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2)
{
	if ((position == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT2))
		return 1;
	else
		return 0;
}

/* front vt4 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT4)
{
	if ((position == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT4))
		return 1;
	else
		return 0;
}

/* front recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = ((mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_ON) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO));

	if ((position == SENSOR_POSITION_FRONT) &&
		setfile_flag && (resol <= SIZE_8MP_FHD_BDS))
		return 1;
	else
		return 0;
}

/* front  full resolution preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_FULL)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENARIO_FULL_SIZE);

	if ((position == SENSOR_POSITION_FRONT) && (scenario_flag))
		return 1;
	else
		return 0;
}

/* front preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW)
{
	if (position == SENSOR_POSITION_FRONT)
		return 1;
	else
		return 0;
}

/* front preview remosaic */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW_REMOSAIC)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENAIRO_REMOSAIC);

	if ((position == SENSOR_POSITION_FRONT) && (scenario_flag))
		return 1;
	else
		return 0;
}

/* front capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE)
{
	if ((position == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)))
		return 1;
	else
		return 0;
}

/* front capture remosaic */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE_REMOSAIC)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENAIRO_REMOSAIC);

	if ((position == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		&& (scenario_flag))
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_FRONT_CAMCORDING)
		)
		return 1;
	else
		return 0;
}

/* rear capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE)
{
	if ((position == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)
		)
		return 1;
	else
		return 0;
}

/* rear capture remosaic */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE_REMOSAIC)
{
	u32 scen = (device->setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
	bool scenario_flag = (scen == FIMC_IS_SCENAIRO_REMOSAIC);

	if ((position == SENSOR_POSITION_REAR) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		&& (scenario_flag))
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_REAR_CAMCORDING_FHD)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_HD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_REAR_CAMCORDING_HD)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		 (static_ctrl->cur_scenario_id == FIMC_IS_SN_REAR_CAMCORDING_UHD)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((position == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(static_ctrl->cur_scenario_id == FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_WIDE_SELFIE)
{
	if (position == SENSOR_POSITION_FRONT)
		return 1;
	else
		return 0;

}

DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_REAR)
{
	if (position == SENSOR_POSITION_REAR || position == SENSOR_POSITION_REAR2)
		return 1;
	else
		return 0;
}

DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_FRONT)
{
	if (position == SENSOR_POSITION_FRONT)
		return 1;
	else
		return 0;
}

DECLARE_EXT_DVFS_CHK_FUNC(FIMC_IS_SN_EXT_SECURE)
{
	if (position == SENSOR_POSITION_SECURE)
		return 1;
	else
		return 0;
}

int fimc_is_hw_dvfs_init(void *dvfs_data)
{
	int ret = 0;
	ulong i;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	dvfs_ctrl = (struct fimc_is_dvfs_ctrl *)dvfs_data;

	FIMC_BUG(!dvfs_ctrl);

	/* set priority by order */
	for (i = 0; i < ARRAY_SIZE(static_scenarios); i++)
		static_scenarios[i].priority = i;
	for (i = 0; i < ARRAY_SIZE(dynamic_scenarios); i++)
		dynamic_scenarios[i].priority = i;
	for (i = 0; i < ARRAY_SIZE(external_scenarios); i++)
		external_scenarios[i].priority = i;

	dvfs_ctrl->static_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->static_ctrl->cur_scenario_idx	= -1;
	dvfs_ctrl->static_ctrl->scenarios		= static_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		dvfs_ctrl->static_ctrl->scenario_cnt	= 0;
	else
		dvfs_ctrl->static_ctrl->scenario_cnt	= ARRAY_SIZE(static_scenarios);

	dvfs_ctrl->dynamic_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->dynamic_ctrl->cur_scenario_idx	= -1;
	dvfs_ctrl->dynamic_ctrl->cur_frame_tick	= -1;
	dvfs_ctrl->dynamic_ctrl->scenarios		= dynamic_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		dvfs_ctrl->dynamic_ctrl->scenario_cnt	= 0;
	else
		dvfs_ctrl->dynamic_ctrl->scenario_cnt	= ARRAY_SIZE(dynamic_scenarios);

	dvfs_ctrl->external_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->external_ctrl->cur_scenario_idx	= -1;
	dvfs_ctrl->external_ctrl->scenarios		= external_scenarios;
	if (external_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		dvfs_ctrl->external_ctrl->scenario_cnt	= 0;
	else
		dvfs_ctrl->external_ctrl->scenario_cnt	= ARRAY_SIZE(external_scenarios);

	return ret;
}
