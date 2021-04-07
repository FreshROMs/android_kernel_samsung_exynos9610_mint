
ccflags-y := $(KBUILD_FP_SENSOR_CFLAGS)

obj-$(CONFIG_ARCH_EXYNOS)       += exynos-el3_mon.o
obj-$(CONFIG_ARCH_EXYNOS)       += exynos-smc.o

# CAL
obj-$(CONFIG_CAL_IF)		+= cal-if/

obj-$(CONFIG_ECT)		+= ect_parser.o

obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o

#FSYS0 TCXO
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-fsys0-tcxo.o

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o

obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o

# ACPM
obj-$(CONFIG_EXYNOS_ACPM)       += acpm/

obj-$(CONFIG_EXYNOS_RGT)        += exynos-rgt.o

obj-$(CONFIG_EXYNOS_REBOOT)     += exynos-reboot.o

# DEBUG
obj-$(CONFIG_EXYNOS_DEBUG)      += debug/
# DVFS
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)	+= exynos-dm.o

# PD
obj-$(CONFIG_EXYNOS_PD)        += exynos-pd.o exynos-pd-dbg.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
#CPUHOTPLUG
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-cpuhp.o
obj-$(CONFIG_EXYNOS_PSTATE_MODE_CHANGER)	+= exynos-emc.o

# CPU Topology
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-topology.o

# OCP
obj-$(CONFIG_EXYNOS_OCP)	+= exynos-ocp.o

#CPU Power Management
obj-$(CONFIG_ARCH_EXYNOS)      += exynos-cpupm.o

#PM
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o

# Exynos Secure Log
obj-$(CONFIG_EXYNOS_SECURE_LOG)	+= exynos-seclog.o

# HDCP
obj-$(CONFIG_EXYNOS_HDCP2)  += exynos-hdcp/

# SDM
obj-$(CONFIG_EXYNOS_SDM) += exynos-sdm.o

# USI_V2
obj-$(CONFIG_USI_V2) += usi_v2.o

# HIU
obj-$(CONFIG_EXYNOS_HIU)	+= exynos-hiu.o

# HAFM & HAFM-TB
obj-$(CONFIG_EXYNOS_PSTATE_HAFM)	+= exynos-hafm.o
obj-$(CONFIG_EXYNOS_PSTATE_HAFM_TB)	+= exynos-hafm-tb.o

# secmem
obj-$(CONFIG_ARCH_EXYNOS)       += secmem.o
