# NXP audio amplifier - TFA9894

TFA_VERSION              =tfa9894
#TFADSP_32BITS           =0
#TFADSP_DSP_MSG_APR_PACKET_STRATEGY=0
TFADSP_DSP_BUFFER_POOL   =1
#TFA_ACTIVATED_ASYNCHRONOUSLY=0
#TFA_WRITE_CAL_TO_FILE   =0
TFA_USE_PSTREAM_ONLY     =1
TFA_USE_DEVICE_SPECIFIC_CONTROL=1
#TFA_PROFILE_ON_DEVICE   =0
TFA_NO_SND_FORMAT_CHECK  =1
#TFA_USE_GPIO_FOR_MCLK    =1
TFA_CONTROL_MCLK         =1
TFA_SRC_DIR              =sound/soc/codecs/$(TFA_VERSION)
TFA98XX_FLAGS           := -DUSE_TFA9894

# cc flags
ccflags-y               := -DDEBUG
ccflags-y               += -I$(TFA_SRC_DIR)/inc
#ccflags-y               := -I$(TFA_SRC_DIR)/inc
ccflags-y               += -Werror
#ifdef TFADSP_32BITS
#ccflags-y               += -DTFADSP_32BITS
#endif
#ifdef TFADSP_DSP_MSG_APR_PACKET_STRATEGY
#ccflags-y               += -DTFADSP_DSP_MSG_APR_PACKET_STRATEGY
#endif
ifdef TFADSP_DSP_BUFFER_POOL
ccflags-y               += -DTFADSP_DSP_BUFFER_POOL
endif
ifdef TFA_ACTIVATED_ASYNCHRONOUSLY
ccflags-y               += -DTFA_ACTIVATED_ASYNCHRONOUSLY
endif
ifdef TFA_WRITE_CAL_TO_FILE
ccflags-y               += -DTFA_WRITE_CAL_TO_FILE
endif
ifdef TFA_USE_PSTREAM_ONLY
ccflags-y               += -DTFA_USE_PSTREAM_ONLY
endif
ifdef TFA_USE_DEVICE_SPECIFIC_CONTROL
ccflags-y               += -DTFA_USE_DEVICE_SPECIFIC_CONTROL
endif
ifdef TFA_PROFILE_ON_DEVICE
ccflags-y               += -DTFA_PROFILE_ON_DEVICE
endif
ifdef TFA_NO_SND_FORMAT_CHECK
ccflags-y               += -DTFA_NO_SND_FORMAT_CHECK
endif
ifdef TFA_USE_GPIO_FOR_MCLK
ccflags-y               += -DTFA_USE_GPIO_FOR_MCLK
endif
ifdef TFA_CONTROL_MCLK
ccflags-y               += -DTFA_CONTROL_MCLK
endif

# objs
snd-soc-tfa9xxx-objs += tfa9xxx.o
snd-soc-tfa9xxx-objs += tfa9xxx_haptic.o
snd-soc-tfa9xxx-objs += tfa2_init.o
snd-soc-tfa9xxx-objs += tfa2_dev.o
snd-soc-tfa9xxx-objs += tfa2_container.o
snd-soc-tfa9xxx-objs += tfa2_haptic.o

# CFLAGS
CFLAGS_tfa9xxx.o        += $(TFA9XXX_FLAGS)
CFLAGS_tfa9xxx_haptic.o += $(TFA9XXX_FLAGS)
CFLAGS_tfa2_init.o      += $(TFA9XXX_FLAGS)
CFLAGS_tfa2_dev.o       += $(TFA9XXX_FLAGS)
CFLAGS_tfa2_container.o += $(TFA9XXX_FLAGS)
CFLAGS_tfa2_haptic.o    += $(TFA9XXX_FLAGS)

# built-in driver
obj-y	+= snd-soc-tfa9xxx.o
