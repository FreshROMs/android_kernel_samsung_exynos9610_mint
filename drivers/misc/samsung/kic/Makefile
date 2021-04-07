obj-$(CONFIG_SAMSUNG_KIC) += samsung_kic.o
samsung_kic-y += \
	slsi_kic_core.o \
	slsi_kic_filtering.o \
	slsi_kic_sap_wifi.o \
	slsi_kic_sap_cm.o \
	slsi_kic_sap_bt.o \
	slsi_kic_sap_ant.o

ccflags-y += $(CONFIG_SAMSUNG_KIC_EXTRA)

# ----------------------------------------------------------------------------
# KIC configuration
# ----------------------------------------------------------------------------

ccflags-$(CONFIG_SLSI_KIC_API_ENABLED)    += -DCONFIG_SLSI_KIC_API_ENABLED

ccflags-$(CONFIG_SCSC_LOGRING) += -DCONFIG_SCSC_PRINTK
