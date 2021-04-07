#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

subdir-ccflags-$(CONFIG_SEC_GPIO_DVS)	+= -DCONFIG_SMCDSD_LCD_DEBUG

obj-$(CONFIG_EXYNOS_DPP) += dpp.o
dpp-y := dpp_drv.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += dsim.o
dsim-y += dsim_drv.o
obj-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/dsim_reg.o ./cal_9610/dpp_reg.o ./cal_9610/decon_reg.o
ifdef CONFIG_EXYNOS_DISPLAYPORT
obj-$(CONFIG_EXYNOS_DISPLAYPORT) += displayport.o
displayport-y += displayport_drv.o displayport_edid.o displayport_hdcp13.o decon_displayport.o
obj-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/displayport_reg.o
endif
obj-$(CONFIG_EXYNOS_DPU20) += decon.o
obj-$(CONFIG_EXYNOS_BTS) += bts.o
decon-y += decon_core.o decon_dsi.o decon_wb.o helper.o win_update.o cursor.o fence.o event_log.o
obj-y += panels/
ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui