obj-$(CONFIG_CAMERA_PAFSTAT) += fimc-is-pafstat.o \
			fimc-is-hw-pafstat-v1_0.o

ifdef CONFIG_LTO_CLANG
ifdef CONFIG_LD_LLD			
ccflags-y += $(DISABLE_LTO)
endif
endif