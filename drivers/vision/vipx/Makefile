#
# Makefile for the VIPx common driver
#

obj-y					+= vipx-binary.o
obj-y					+= vipx-compat-ioctl.o
obj-y					+= vipx-ioctl.o
obj-y					+= vipx-debug.o
obj-y					+= vipx-device.o
obj-y					+= vipx-graph.o
obj-y					+= vipx-graphmgr.o
obj-y					+= vipx-interface.o
obj-y					+= vipx-io.o
obj-y					+= vipx-mailbox.o
obj-y					+= vipx-memory.o
obj-y					+= vipx-queue.o
obj-y					+= vipx-system.o
obj-y					+= vipx-taskmgr.o
obj-y					+= vipx-time.o
obj-y					+= vipx-core.o
obj-y					+= vipx-context.o
obj-y					+= vipx-kernel-binary.o
obj-y					+= vipx-pm.o
obj-y					+= vipx-util.o

obj-$(CONFIG_EXYNOS_VIPX_PLATFORM)	+= platform/

ccflags-y += -Idrivers/vision/vipx
