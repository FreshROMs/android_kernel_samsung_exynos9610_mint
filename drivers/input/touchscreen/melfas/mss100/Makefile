#
# Makefile for MELFAS MSS100 Touchscreen driver
#

obj-$(CONFIG_TOUCHSCREEN_MELFAS_MSS100)	+= melfas_mss100.o melfas_mss100_mod.o melfas_mss100_test.o melfas_mss100_cmd.o melfas_mss100_fw_update.o

#CFLAGS_melfas_mss100.o += -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-label
#CFLAGS_melfas_mss100_test.o += -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-label
#CFLAGS_melfas_mss100_cmd.o += -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-label
#CFLAGS_melfas_mss100_fw_update.o += -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-label

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui