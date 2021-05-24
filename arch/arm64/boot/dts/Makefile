# SPDX-License-Identifier: GPL-2.0

ifdef CONFIG_SEC_A50_PROJECT
dtb-y += exynos/universal9610/exynos9610.dtb
dtbo-y += samsung/a50/exynos9610-a50_swa_open_00.dtbo
dtbo-y += samsung/a50/exynos9610-a50_swa_open_01.dtbo
dtbo-y += samsung/a50/exynos9610-a50_swa_open_02.dtbo
dtbo-y += samsung/a50/exynos9610-a50_swa_open_03.dtbo
dtbo-y += samsung/a50/exynos9610-a50_swa_open_04.dtbo
dtbo-y += samsung/a50/exynos9610-a50_swa_open_06.dtbo
endif

ifdef CONFIG_SEC_A50S_PROJECT
dtb-y += exynos/universal9611/exynos9610.dtb
dtbo-y += samsung/a50s/exynos9611-a50s_eur_open_00.dtbo
dtbo-y += samsung/a50s/exynos9611-a50s_eur_open_02.dtbo
dtbo-y += samsung/a50s/exynos9611-a50s_eur_open_01.dtbo
endif

ifdef CONFIG_SEC_M30S_PROJECT
dtb-y += exynos/universal9611/exynos9610.dtb
dtbo-y += samsung/m30s/exynos9611-m30s_swa_open_00.dtbo
dtbo-y += samsung/m30s/exynos9611-m30s_swa_open_04.dtbo
dtbo-y += samsung/m30s/exynos9611-m30s_swa_open_02.dtbo
dtbo-y += samsung/m30s/exynos9611-m30s_swa_open_03.dtbo
endif

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
