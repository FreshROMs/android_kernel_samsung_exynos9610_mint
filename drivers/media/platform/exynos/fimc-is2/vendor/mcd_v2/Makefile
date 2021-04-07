obj-$(CONFIG_VENDER_MCD_V2) :=	fimc-is-vender.o \
							fimc-is-sec-define.o \
							crc32.o \
							fimc-is-sysfs.o \
							fimc-is-device-eeprom.o \
							fimc-is-sec-util.o

obj-$(CONFIG_OIS_USE)	+= fimc-is-device-ois_common.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= fimc-is-device-af.o
obj-$(CONFIG_OIS_USE_RUMBA_S4) += fimc-is-device-ois_s4.o
obj-$(CONFIG_OIS_USE_RUMBA_SA) += fimc-is-device-ois_sa.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/sensor/module_framework
