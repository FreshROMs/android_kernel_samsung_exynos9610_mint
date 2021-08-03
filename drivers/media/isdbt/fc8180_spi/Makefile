ifeq ($(CONFIG_ISDBT_FC8180_SPI),y)

DRV_NAME = isdbt_fc8180

#EXTRA_CFLAGS += -DDLOG


obj-$(CONFIG_ISDBT_FC8180_SPI) += bbm.o \
								fc8180.o \
								fc8180_bb.o \
								fc8180_i2c.o \
								fc8180_isr.o \
								fc8180_ppi.o \
								fc8180_spi.o \
								fc8180_spib.o \
								fc8180_tun.o \
								fci_bypass.o \
								fci_hal.o \
								fci_hpi.o \
								fci_i2c.o \
								fci_oal.o \
								fci_ringbuffer.o \
								fci_tun.o

endif

#ccflags-y += -Idrivers/media/isdbt

