#
#  Makefile for Exynos-Debug
#

obj-$(CONFIG_EXYNOS_CORESIGHT)		+= exynos-coresight.o
ifeq ($(CONFIG_SOC_EXYNOS9610),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos9610-itmon.o
endif
ifeq ($(CONFIG_SOC_EXYNOS9810),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos9810-itmon.o
endif
ifeq ($(CONFIG_SOC_EXYNOS9820),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos9820-itmon.o
endif
ifeq ($(CONFIG_DEBUG_SNAPSHOT),y)
obj-$(CONFIG_ARCH_EXYNOS)		+= exynos-helper.o
obj-$(CONFIG_ARCH_EXYNOS)		+= exynos-handler.o
endif
obj-$(CONFIG_EXYNOS_CORESIGHT_ETM)	+= exynos-coresight-etm.o
