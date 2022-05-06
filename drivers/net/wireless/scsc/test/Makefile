 # ----------------------------------------------------------------------------
# FILE: Makefile
#
# PURPOSE:
#       Build instructions for SCSC Wlan driver.
#
#
# Copyright (C) 2016 by Samsung Electronics Co., Ltd
# ----------------------------------------------------------------------------

CONFIG_SCSC_WLAN_MODULE=scsc_wlan_unittest

# ----------------------------------------------------------------------------
# Common Driver Files
# ----------------------------------------------------------------------------
$(CONFIG_SCSC_WLAN_MODULE)-y += ../mib.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../mib_text_convert.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../debug.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../debug_frame.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../procfs.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../mgt.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../udi.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../log_clients.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../src_sink.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../fw_test.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../cac.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../ioctl.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../traffic_monitor.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../reg_info.o
# enable GSCAN
ifeq ($(CONFIG_SCSC_WLAN_GSCAN_ENABLE),y)
$(CONFIG_SCSC_WLAN_MODULE)-y += ../nl80211_vendor.o
ccflags-y += -DCONFIG_SCSC_WLAN_GSCAN_ENABLE
endif

ifeq ($(CONFIG_SCSC_WIFI_NAN_ENABLE),y)
$(CONFIG_SCSC_WLAN_MODULE)-y += ../nl80211_vendor_nan.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../mlme_nan.o
ccflags-y += -DCONFIG_SCSC_WIFI_NAN_ENABLE
endif

# enable roam offload
ifeq ($(CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD),y)
ccflags-y += -DCONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
endif

ifeq ($(CONFIG_SCSC_WLAN_SAE_CONFIG),y)
ccflags-y += -DCONFIG_SCSC_WLAN_SAE_CONFIG
endif
ifeq ($(CONFIG_SCSC_WLAN_AP_INFO_FILE),y)
ccflags-y += -DCONFIG_SCSC_WLAN_AP_INFO_FILE
endif
ifeq ($(CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA),y)
ccflags-y += -DCONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
endif
ifeq ($(CONFIG_SCSC_WLAN_AP_AUTO_RECOVERY),y)
ccflags-y += -DCONFIG_SCSC_WLAN_AP_AUTO_RECOVERY
endif
ifeq ($(CONFIG_SCSC_WLAN_DUAL_STATION),y)
ccflags-y += -DCONFIG_SCSC_WLAN_DUAL_STATION
endif
ifeq ($(CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC),y)
ccflags-y += -DCONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
endif
ifeq ($(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ENHANCED_BIGDATA
endif
ifeq ($(CONFIG_SCSC_WLAN_ENHANCED_LOGGING),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ENHANCED_LOGGING
endif
ifeq ($(CONFIG_SLSI_WLAN_STA_FWD_BEACON),y)
ccflags-y += -DCONFIG_SLSI_WLAN_STA_FWD_BEACON
endif
ifeq ($(CONFIG_SCSC_WLAN_DYNAMIC_ITO),y)
ccflags-y += -DCONFIG_SCSC_WLAN_DYNAMIC_ITO
endif


ccflags-y += -DCONFIG_SCSC_AP_INTERFACE_NAME=\"$(CONFIG_SCSC_AP_INTERFACE_NAME)\"
ifeq ($(CONFIG_SCSC_WLAN_WIFI_SHARING),y)
ccflags-y += -DCONFIG_SCSC_WLAN_WIFI_SHARING
endif
ifeq ($(CONFIG_SCSC_WLAN_MHS_STATIC_INTERFACE),y)
ccflags-y += -DCONFIG_SCSC_WLAN_MHS_STATIC_INTERFACE
endif
ifeq ($(CONFIG_SCSC_WLAN_SINGLE_ANTENNA),y)
ccflags-y += -DCONFIG_SCSC_WLAN_SINGLE_ANTENNA
endif
ifeq ($(CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
endif
ifeq ($(CONFIG_SCSC_WLAN_ACS_ENABLE),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ACS_ENABLE
endif
ifeq ($(CONFIG_SCSC_WLAN_SILENT_RECOVERY),y)
ccflags-y += -DCONFIG_SCSC_WLAN_SILENT_RECOVERY
endif

ifeq ($(CONFIG_SCSC_WLAN_MAX_LINK_SPEED),y)
ccflags-y += -DCONFIG_SCSC_WLAN_MAX_LINK_SPEED
endif
ifeq ($(CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
endif
ifeq ($(CONFIG_SCSC_WLAN_LOW_LATENCY_MODE),y)
ccflags-y += -DCONFIG_SCSC_WLAN_LOW_LATENCY_MODE
endif
ifeq ($(CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT),y)
ccflags-y += -DCONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
endif
ifeq ($(CONFIG_SCSC_WLAN_NUM_ANTENNAS),y)
ccflags-y += -DCONFIG_SCSC_WLAN_NUM_ANTENNAS
endif
ifeq ($(CONFIG_SCSC_ENHANCED_PACKET_STATS),y)
ccflags-y += -DCONFIG_SCSC_ENHANCED_PACKET_STATS
endif
ifeq ($(CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER),y)
ccflags-y += -DCONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
endif

# ----------------------------------------------------------------------------
# Building for cfg802.11
# ----------------------------------------------------------------------------
$(CONFIG_SCSC_WLAN_MODULE)-y += ../cfg80211_ops.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../mlme.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../netif.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../rx.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../tx.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../ba.o

# ----------------------------------------------------------------------------
# Building for SAP
# ----------------------------------------------------------------------------
$(CONFIG_SCSC_WLAN_MODULE)-y += ../sap_ma.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../sap_mlme.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../sap_dbg.o
$(CONFIG_SCSC_WLAN_MODULE)-y += ../sap_test.o

# ----------------------------------------------------------------------------
# unittest Driver
# ----------------------------------------------------------------------------
$(CONFIG_SCSC_WLAN_MODULE)-y += ../dev.o
$(CONFIG_SCSC_WLAN_MODULE)-y += dev_test.o
$(CONFIG_SCSC_WLAN_MODULE)-y += hip_test.o
$(CONFIG_SCSC_WLAN_MODULE)-y += hydra_test.o
$(CONFIG_SCSC_WLAN_MODULE)-y += udi_test.o

# Suppress -Wempty-body when not built with Debug enabled.
ifneq ($(CONFIG_SCSC_WLAN_DEBUG),y)
ccflags-y += -Wno-empty-body
else
ccflags-y += -g -ggdb
endif

ccflags-y += -Wno-unused-parameter
ccflags-y += -Wno-sign-compare

# ----------------------------------------------------------------------------
# wlan configuration
# ----------------------------------------------------------------------------
ccflags-$(CONFIG_SCSC_WLAN_DEBUG)				+= -DCONFIG_SCSC_WLAN_DEBUG
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TRACE)			+= -DCONFIG_SCSC_WLAN_OFFLINE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_SDIO_TRACE)			+= -DCONFIG_SCSC_WLAN_OFFLINE_SDIO_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE)	+= -DCONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TX_TRACE)			+= -DCONFIG_SCSC_WLAN_OFFLINE_TX_TRACE
ccflags-$(CONFIG_SCSC_WLAN_HIP_CLEAR_TH_SLOT_SUPPORT)		+= -DCONFIG_SCSC_WLAN_HIP_CLEAR_TH_SLOT_SUPPORT
ccflags-$(CONFIG_SCSC_WLAN_HIP_DEFER_SLOT_SUPPORT)		+= -DCONFIG_SCSC_WLAN_HIP_DEFER_SLOT_SUPPORT
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI)				+= -DCONFIG_SCSC_WLAN_RX_NAPI
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI_GRO)				+= -DCONFIG_SCSC_WLAN_RX_NAPI_GRO
ccflags-$(CONFIG_SCSC_WLAN_WES_NCHO)                            += -DCONFIG_SCSC_WLAN_WES_NCHO
ccflags-$(CONFIG_SCSC_WLAN_MUTEX_DEBUG)                         += -DCONFIG_SCSC_WLAN_MUTEX_DEBUG

ccflags-y += -DCONFIG_SCSC_WLAN_MAX_INTERFACES=$(CONFIG_SCSC_WLAN_MAX_INTERFACES)

# Android specific build options
ccflags-$(CONFIG_SCSC_WLAN_ANDROID) += -DCONFIG_SCSC_WLAN_ANDROID

ccflags-y += -I$(src) -I$(src)/..
ccflags-y += -I$(src)/../../../../../include -I$(src)/../../../../../include/scsc

ccflags-y += -DSLSI_TEST_DEV
ccflags-y += $(CONFIG_SCSC_WLAN_EXTRA)

# ----------------------------------------------------------------------------
# module
# ----------------------------------------------------------------------------
obj-$(CONFIG_SCSC_WLAN_UNITTEST) := $(CONFIG_SCSC_WLAN_MODULE).o
