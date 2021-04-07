#
# Makefile for muic devices
#


obj-y					+= muic-core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o

obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic.o sm5713-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_CCIC), y)
obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic-ccic.o
endif
