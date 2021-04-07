# Makefile of modem_v1

EXTRA_CFLAGS += -Idrivers/misc/modem_v1 \
		-DCONFIG_OPTION_REGION=\"$(PROJECT_REGION)\"

obj-y += modem_main.o modem_variation.o modem_io_device.o modem_utils.o modem_dump.o

obj-$(CONFIG_LINK_DEVICE_MEMORY) += link_device_memory_main.o \
				    link_device_memory_snapshot.o \
				    link_device_memory_command.o \
				    link_device_memory_flow_control.o \
				    link_device_memory_debug.o

obj-$(CONFIG_LINK_DEVICE_SHMEM) += link_device_shmem.o \
				   link_device_memory_snapshot.o \
				   link_device_memory_flow_control.o \
				   link_device_memory_debug.o \
				   link_device_memory_sbd.o \
				   modem_notifier.o

obj-$(CONFIG_UMTS_MODEM_SS310AP) += modem_ctrl_ss310ap.o

obj-$(CONFIG_LINK_CONTROL_MSG_IOSM) += link_ctrlmsg_iosm.o
obj-$(CONFIG_LINK_CONTROL_MSG_COMMAND) += link_ctrlmsg_command.o

obj-$(CONFIG_DEBUG_PKTLOG) += modem_pktlog.o

obj-$(CONFIG_CP_ZEROCOPY) += link_rx_zerocopy.o

ifeq ($(CONFIG_RPS), y)
ifeq ($(CONFIG_ARGOS), y)
obj-y += modem_argos_notifier.o
endif
endif

obj-$(CONFIG_UART_SWITCH) += uart_switch.o

