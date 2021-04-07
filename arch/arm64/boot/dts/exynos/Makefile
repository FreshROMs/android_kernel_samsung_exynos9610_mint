# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_EXYNOS) += \
	exynos5433-tm2.dtb	\
	exynos5433-tm2e.dtb	\
	exynos7-espresso.dtb

dtb-$(CONFIG_SOC_EXYNOS9610) += exynos9610.dtb
dtb-$(CONFIG_SOC_EXYNOS9610) += \
	exynos9610-universal9610.dtbo \
	exynos9611-universal9611.dtbo

# always		:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
