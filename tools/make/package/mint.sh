# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

## AnyKernel setup
# begin properties
properties() { '
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
supported.patchlevels=2021-04 - 
'; } # end properties

# shell variables
block=auto;
dtbblock=/dev/block/platform/13520000.ufs/by-name/dtb;
is_slot_device=0;
ramdisk_compression=auto;
patch_vbmeta_flag=0;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;

## AnyKernel file attributes
# set permissions/ownership for included ramdisk files
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;

# TenSeventySeven 2021 - Enable Pageboost and RAM Plus
AK_FOLDER=/tmp/anykernel
mount /system/
mount /system_root/
mount /vendor/
mount -o rw,remount -t auto /system > /dev/null
mount -o rw,remount -t auto /vendor > /dev/null

fresh4=0
fresh=$(file_getprop "/system_root/system/system_ext/fresh.prop" "ro.fresh.maintainer")
oneui=$(file_getprop "/system_root/system/build.prop" "ro.build.PDA")

# Accomodate Exynos9611 devices' init.hardware.rc
if [ -f "/vendor/etc/init/init.exynos9611.rc" ]; then
	VENDOR_INIT_RC=/vendor/etc/init/init.exynos9611.rc
else
	VENDOR_INIT_RC=/vendor/etc/init/init.exynos9610.rc
fi

# Try for Fresh 4 series
if [ -z $fresh ]; then
	fresh=$(file_getprop "/system_root/system/system_ext/etc/fresh.prop" "ro.fresh.maintainer")
	if [ ! -z $fresh ]; then
		fresh4=1
	fi
fi

if [ ! -z $oneui ]; then
	if [ -z $fresh ]; then
  		sdk_ver=$(file_getprop /vendor/build.prop ro.vendor.build.version.sdk);
  		if [ "${sdk_ver}" == "30" ]; then
			ui_print "  - One UI detected!"
			ui_print "    - Enabling Pageboost and RAM Plus"
			patch_prop /vendor/build.prop 'ro.nandswap.level' '2'
			patch_prop /vendor/build.prop 'ro.nandswap.lru_ratio' '50'
			patch_prop /vendor/build.prop 'ro.sys.kernelmemory.nandswap.ux_support' 'true'
			patch_prop /vendor/build.prop 'ro.sys.kernelmemory.nandswap.daily_quota' '786432'
			patch_prop /vendor/build.prop 'ro.sys.kernelmemory.nandswap.daily_quota_limit' '2359296'
			patch_prop /vendor/build.prop 'ro.config.pageboost.vramdisk.minimize' "true"
			patch_prop /vendor/build.prop 'ro.config.pageboost.active_launch.enabled' "true"
			patch_prop /vendor/build.prop 'ro.config.pageboost.io_prefetch.enabled' "true"
			patch_prop /vendor/build.prop 'ro.config.pageboost.io_prefetch.level' "3"

			cp -rf $AK_FOLDER/files_oneui/system/etc/init/init.mint.rc /system/etc/init/init.mint.rc
			cp -rf $AK_FOLDER/files_oneui/system/etc/init/init.mint.rc /system_root/system/etc/init/init.mint.rc
			cp -rf $AK_FOLDER/files_oneui/vendor/etc/fstab.sqzr /vendor/etc/fstab.sqzr

			chmod 644 /system/etc/init/init.mint.rc
			chmod 644 /system_root/system/etc/init/init.mint.rc
			chmod 644 /vendor/etc/fstab.sqzr

			# Disable SSWAP for RAM Plus and Pageboost
			remove_section ${VENDOR_INIT_RC} 'service swapon /system/bin/sswap -s -z -f 2048' 'oneshot'
			replace_string ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.dummy' 'swapon_all /vendor/etc/fstab.exynos9610' 'swapon_all /vendor/etc/fstab.sqzr' global
			replace_string ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.dummy' 'swapon_all /vendor/etc/fstab.model' 'swapon_all /vendor/etc/fstab.sqzr' global
			replace_string ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.dummy' 'swapon_all /vendor/etc/fstab.zram' 'swapon_all /vendor/etc/fstab.sqzr' global
			append_file ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.sqzr' init.ramplus.rc
			append_file ${VENDOR_INIT_RC} 'start pageboostd' init.pageboost.rc
		else
			ui_print "  - One UI 4 detected! RAM Plus is already enabled!"
  		fi
	else
  		sdk_ver=$(file_getprop /system_root/system/build.prop ro.build.version.sdk);
		ui_print "  - FreshROMs detected! RAM Plus is already enabled!"

		fresh_codename=$(file_getprop "/system_root/system/system_ext/etc/fresh.prop" "ro.fresh.build.codename")

		if [ "${fresh4}" == "1" ]; then
			if [ "${fresh_codename}" == "axl" ]; then
				cp -rf $AK_FOLDER/files_fresh/system/etc/init/init.fresh.perf.rc /system/etc/init/init.fresh.perf.rc
				chmod 644 /system_root/system/etc/init/init.fresh.perf.rc
			fi
		fi
	fi
else
	ui_print "  - AOSP ROM detected!"
	ui_print "    - Enabling native ZRAM writeback"

	mkdir -p /vendor/overlay
	cp -rf $AK_FOLDER/files_aosp/vendor/overlay/MintZramWb.apk /vendor/overlay/MintZramWb.apk
	cp -rf $AK_FOLDER/files_aosp/vendor/etc/fstab.zram /vendor/etc/fstab.zram

	chmod 644 /vendor/overlay/MintZramWb.apk
	chmod 644 /vendor/etc/fstab.zram

	patch_prop /vendor/build.prop 'ro.zram.mark_idle_delay_mins' '60'
	patch_prop /vendor/build.prop 'ro.zram.first_wb_delay_mins' '1440'
	patch_prop /vendor/build.prop 'ro.zram.periodic_wb_delay_hours' '24'
	replace_string ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.dummy' 'swapon_all /vendor/etc/fstab.exynos9610' 'swapon_all /vendor/etc/fstab.zram' global
	replace_string ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.dummy' 'swapon_all /vendor/etc/fstab.sqzr' 'swapon_all /vendor/etc/fstab.zram' global
	append_file ${VENDOR_INIT_RC} 'swapon_all /vendor/etc/fstab.zram' init.zram.rc
fi

umount /system
umount /system_root
umount /vendor

## AnyKernel boot install
split_boot;

flash_boot;

# Flash dtb
ui_print "  - Installing Exynos device tree blob (DTB)...";
flash_generic dtb;
## end boot install
