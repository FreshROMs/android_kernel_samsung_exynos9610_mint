ifeq ($(CONFIG_ISDBT_FC8350_SPI),y)

DRV_NAME = isdbt_fc8350

#EXTRA_CFLAGS += -DDLOG
#EXTRA_CFLAGS += -DSEC_ENABLE_13SEG_BOOST

ccflags-y := -Idrivers/media/isdbt/fc8350_spi/inc
obj-$(CONFIG_ISDBT_FC8350_SPI) += bbm.o \
					fc8350.o \
					fc8350_bb.o \
					fc8350_i2c.o \
					fc8350_isr.o \
					fc8350_spi.o \
					fc8350_tun.o \
					fc8350_tun_table.o \
					fci_hal.o \
					fci_hpi.o \
					fci_oal.o \
					fci_ringbuffer.o \
					fci_tun.o
endif
