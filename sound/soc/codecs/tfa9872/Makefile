# NXP audio amplifier - TFA9872

TFA_VERSION              =tfa9872
TFADSP_32BITS            =1
#TFADSP_DSP_MSG_APR_PACKET_STRATEGY=0
TFADSP_DSP_BUFFER_POOL   =1
TFA_BLACKBOX_LOGGING     =1
#TFA_WRITE_CAL_TO_FILE   =0
TFA_NO_SND_FORMAT_CHECK  =1
TFA_USE_DEVICE_SPECIFIC_CONTROL=1
TFA_PROFILE_ON_DEVICE    =1
TFA_SRC_DIR              =sound/soc/codecs/$(TFA_VERSION)
TFA98XX_FLAGS           := -DUSE_TFA9872

# cc flags
#ccflags-y               := -DDEBUG
#ccflags-y               += -I$(TFA_SRC_DIR)/inc
ccflags-y               := -I$(TFA_SRC_DIR)/inc
ccflags-y               += -Werror
ifdef TFADSP_32BITS
ccflags-y               += -DTFADSP_32BITS
endif
ifdef TFADSP_DSP_MSG_APR_PACKET_STRATEGY
ccflags-y               += -DTFADSP_DSP_MSG_APR_PACKET_STRATEGY
endif
ifdef TFADSP_DSP_BUFFER_POOL
ccflags-y               += -DTFADSP_DSP_BUFFER_POOL
endif
ifdef TFA_BLACKBOX_LOGGING
ccflags-y               += -DTFA_BLACKBOX_LOGGING
endif
ifdef TFA_WRITE_CAL_TO_FILE
ccflags-y               += -DTFA_WRITE_CAL_TO_FILE
endif
ifdef TFA_NO_SND_FORMAT_CHECK
ccflags-y               += -DTFA_NO_SND_FORMAT_CHECK
endif
ifdef TFA_USE_DEVICE_SPECIFIC_CONTROL
ccflags-y               += -DTFA_USE_DEVICE_SPECIFIC_CONTROL
endif
ifdef TFA_PROFILE_ON_DEVICE
ccflags-y               += -DTFA_PROFILE_ON_DEVICE
endif

# objs
snd-soc-tfa98xx-objs    := tfa98xx.o
snd-soc-tfa98xx-objs    += tfa_container.o
snd-soc-tfa98xx-objs    += tfa_dsp.o
snd-soc-tfa98xx-objs    += tfa_init.o
snd-soc-tfa98xx-objs    += tfa_debug.o
snd-soc-tfa98xx-objs    += tfa_cal.o
snd-soc-tfa98xx-objs    += tfa_log.o

# CFLAGS
CFLAGS_tfa98xx.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_container.o += $(TFA98XX_FLAGS)
CFLAGS_tfa_dsp.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_init.o      += $(TFA98XX_FLAGS)
CFLAGS_tfa_debug.o     += $(TFA98XX_FLAGS)
CFLAGS_tfa_cal.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_log.o       += $(TFA98XX_FLAGS)

# built-in driver
obj-y	+= snd-soc-tfa98xx.o
