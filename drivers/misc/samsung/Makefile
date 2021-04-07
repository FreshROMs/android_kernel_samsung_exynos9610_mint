# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := built-in.o

obj-$(CONFIG_GPIO_DEBUG)		+= gpio_debug/
obj-$(CONFIG_SAMSUNG_KIC)		+= kic/
obj-$(CONFIG_SCSC_CORE)			+= scsc/
obj-$(CONFIG_SCSC_BT)			+= scsc_bt/
