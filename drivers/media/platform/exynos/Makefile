
ifeq ($(CONFIG_MINT_PLATFORM_VERSION),11)
obj-$(CONFIG_VIDEO_EXYNOS_MFC)		+= mfc_11/
else
obj-$(CONFIG_VIDEO_EXYNOS_MFC)		+= mfc/
endif

obj-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS)	+= camera-pp/
EXTRA_CLAGS += -Idrivers/media/video
