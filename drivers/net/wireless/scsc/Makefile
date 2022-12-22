# ----------------------------------------------------------------------------
# FILE: Makefile
#
# PURPOSE:
#       Build instructions for SCSC Wlan driver.
#
# Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved
# ----------------------------------------------------------------------------

# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := *.o *.ko

obj-$(CONFIG_SCSC_WLAN) += scsc_wlan.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip4.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mbulk.o

# ----------------------------------------------------------------------------
# HIP4 sampler
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WLAN_HIP4_PROFILING),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip4_sampler.o
endif

scsc_wlan-$(CONFIG_SCSC_SMAPPER) += hip4_smapper.o

# Upper driver
scsc_wlan-$(CONFIG_SCSC_WLAN) += dev.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cfg80211_ops.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += netif.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += rx.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += tx.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += ba.o

# SAPs
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_mlme.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_ma.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_dbg.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_test.o

# ----------------------------------------------------------------------------
# Common Driver Files
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += mib.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mib_text_convert.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += debug.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += debug_frame.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += procfs.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mgt.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mlme.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += udi.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += log_clients.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += src_sink.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += fw_test.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cac.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += scsc_wifi_fcq.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += ioctl.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += traffic_monitor.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += reg_info.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += scsc_wlan_mmap.o

# ----------------------------------------------------------------------------
# Wifi Logger Files
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WIFILOGGER),m)
ccflags-y += -DCONFIG_SCSC_WIFILOGGER
endif
obj-$(CONFIG_SCSC_WIFILOGGER) +=  scsc_wifilogger.o
scsc_wifilogger-y += scsc_wifilogger_api.o \
                     scsc_wifilogger_internal.o \
                     scsc_wifilogger_core.o \
                     scsc_wifilogger_module.o \
                     scsc_wifilogger_ring_connectivity.o \
                     scsc_wifilogger_ring_wakelock.o \
                     scsc_wifilogger_ring_pktfate.o

scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_DEBUGFS) += scsc_wifilogger_debugfs.o
scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_TEST) += scsc_wifilogger_ring_test.o

# ----------------------------------------------------------------------------
# ACM
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WLAN_ACM_API),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += acm_api.o
endif

# ----------------------------------------------------------------------------
# Building for Hardware
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cm_if.o

ifeq ($(CONFIG_SCSC_MXLOGGER), m)
ccflags-y += -DCONFIG_SCSC_MXLOGGER
endif

ifeq ($(CONFIG_SCSC_LOG_COLLECTION), m)
ccflags-y += -DCONFIG_SCSC_LOG_COLLECTION
endif

ifeq ($(CONFIG_SCSC_WLAN_KIC_OPS),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += kic.o
endif
ccflags-$(CONFIG_SCSC_WLAN_KIC_OPS) += -DCONFIG_SCSC_WLAN_KIC_OPS

ifeq ($(CONFIG_SCSC_WLAN_GSCAN_ENABLE),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += nl80211_vendor.o
ccflags-y += -DCONFIG_SCSC_WLAN_GSCAN_ENABLE
endif
ifeq ($(CONFIG_SCSC_WIFI_NAN_ENABLE),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += nl80211_vendor_nan.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mlme_nan.o
endif

ifeq ($(CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD),y)
ccflags-y += -DCONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
endif

# ----------------------------------------------------------------------------
# Wlan configuration
# ----------------------------------------------------------------------------
ccflags-$(CONFIG_SCSC_WLAN_DEBUG)                                         += -DCONFIG_SCSC_WLAN_DEBUG
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TRACE)                                 += -DCONFIG_SCSC_WLAN_OFFLINE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE)              += -DCONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TX_TRACE)                              += -DCONFIG_SCSC_WLAN_OFFLINE_TX_TRACE
ccflags-$(CONFIG_SCSC_WLAN_HIP_PSCHED_AMSDU)                              += -DCONFIG_SCSC_WLAN_HIP_PSCHED_AMSDU
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI)                                       += -DCONFIG_SCSC_WLAN_RX_NAPI
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI_GRO)                                   += -DCONFIG_SCSC_WLAN_RX_NAPI_GRO
ccflags-$(CONFIG_SCSC_WLAN_HIP_SUPPORT_SCATTER_GATHER_API)                += -DCONFIG_SCSC_WLAN_HIP_SUPPORT_SCATTER_GATHER_API
ccflags-$(CONFIG_SCSC_WLAN_WES_NCHO)                                      += -DCONFIG_SCSC_WLAN_WES_NCHO
ccflags-$(CONFIG_SCSC_WLAN_MUTEX_DEBUG)                                   += -DCONFIG_SCSC_WLAN_MUTEX_DEBUG
ccflags-$(CONFIG_SCSC_WLAN_DISABLE_NAT_KA)				  += -DCONFIG_SCSC_WLAN_DISABLE_NAT_KA

ccflags-y += $(CONFIG_SAMSUNG_MAXWELL_EXTRA)
ccflags-y += -DCONFIG_SCSC_WLAN_MAX_INTERFACES=$(CONFIG_SCSC_WLAN_MAX_INTERFACES)

# Android specific build options
ccflags-$(CONFIG_SCSC_WLAN_ANDROID) += -DCONFIG_SCSC_WLAN_ANDROID

ccflags-y += -Idrivers/misc/samsung/scsc
# Android Platform version
ifneq ($(ANDROID_VERSION),)
ANDROID_PLATFORM_VERSION=$(shell expr $(ANDROID_VERSION) / 10000)
ccflags-y += -DSCSC_SEP_VERSION=$(ANDROID_PLATFORM_VERSION)
$(info ANDROID_VERSION is $(ANDROID_VERSION))
else
ifneq ($(PLATFORM_VERSION),)
ANDROID_PLATFORM_VERSION := $(basename $(PLATFORM_VERSION))
ccflags-y += -DSCSC_SEP_VERSION=$(ANDROID_PLATFORM_VERSION)
$(info PLATFORM_VERSION is $(PLATFORM_VERSION))
endif
endif
$(info ANDROID_PLATFORM_VERSION is $(ANDROID_PLATFORM_VERSION))

## See drivers/misc/samsung/scsc/ Makefile for an explanation of the reasons
## of the following ifeq/else
ifeq ($(CONFIG_SCSC_LOGRING), m)
ccflags-y		 						+= -DCONFIG_SCSC_PRINTK
else
ccflags-$(CONFIG_SCSC_LOGRING) 						+= -DCONFIG_SCSC_PRINTK
endif
