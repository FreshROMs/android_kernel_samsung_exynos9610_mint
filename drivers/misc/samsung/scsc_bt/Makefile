# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := *.o *.ko

#
# Maxwell BT
obj-$(CONFIG_SCSC_BT) += scsc_bt.o
scsc_bt-y += scsc_bt_module.o scsc_shm.o scsc_avdtp_detect.o
scsc_bt-$(CONFIG_SCSC_BT_BLUEZ) += scsc_bluez.o
scsc_bt-$(CONFIG_SCSC_ANT) += scsc_ant.o

ifeq ($(CONFIG_SCSC_LOG_COLLECTION), m)
ccflags-y += -DCONFIG_SCSC_LOG_COLLECTION
endif

ccflags-y += $(CONFIG_SAMSUNG_MAXWELL_EXTRA)
## See sibling scsc/ Makefile for an explanation of the reasons of
## the following ifeq/else
ifeq ($(CONFIG_SCSC_LOGRING), m)
ccflags-y += -DCONFIG_SCSC_PRINTK
else
ccflags-$(CONFIG_SCSC_LOGRING) += -DCONFIG_SCSC_PRINTK
endif

ifeq ($(CONFIG_SCSC_BT_BLUEZ), m)
ccflags-y += -DCONFIG_SCSC_BT_BLUEZ
else
ccflags-$(CONFIG_SCSC_BT_BLUEZ) += -DCONFIG_SCSC_BT_BLUEZ
endif

ifeq ($(CONFIG_SCSC_ANT), m)
ccflags-y += -DCONFIG_SCSC_ANT
else
ccflags-$(CONFIG_SCSC_ANT) += -DCONFIG_SCSC_ANT
endif
