#!/bin/bash
# =========================================
#         _____              _      
#        |  ___| __ ___  ___| |__   
#        | |_ | '__/ _ \/ __| '_ \  
#        |  _|| | |  __/\__ \ | | | 
#        |_|  |_|  \___||___/_| |_| 
#                              
# =========================================
#  
#  Build - Kernel Build Script
#  The Fresh Project
#  Copyright (C) 2019-2021 TenSeventy7
#  
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#  
#  =========================
#

# Utility directories
ORIGIN_DIR=$(pwd)
CURRENT_BUILD_USER=$(whoami)

# Set build user if building locally
if [[ ${CURRENT_BUILD_USER} == "tenseventy7" ]]; then
	export KBUILD_BUILD_USER=TenSeventy7
	export KBUILD_BUILD_HOST=Lumiose-Build
fi

# Toolchain options
BUILD_PREF_COMPILER='clang'
BUILD_PREF_COMPILER_VERSION='proton'

# Local toolchain directory
TOOLCHAIN=$HOME/toolchains/exynos9610_toolchains_fresh

# External toolchain directory
TOOLCHAIN_EXT=$(pwd)/toolchain

DEVICE_DB_DIR="${ORIGIN_DIR}/Documentation/device-db"

export ARCH=arm64
export SUBARCH=arm64
export ANDROID_MAJOR_VERSION=r
export PLATFORM_VERSION=11.0.0
export $ARCH

script_echo() {
	echo "  $1"
}

exit_script() {
	kill -INT $$
}

download_toolchain() {
	git clone https://github.com/TenSeventy7/exynos9610_toolchains_fresh.git ${TOOLCHAIN_EXT} --single-branch -b ${BUILD_PREF_COMPILER_VERSION} --depth 1 2>&1 | sed 's/^/     /'
	verify_toolchain
}

verify_toolchain() {
	sleep 2
	script_echo " "

	if [[ -d "${TOOLCHAIN}" ]]; then
		script_echo "I: Toolchain found at default location"
		export PATH="${TOOLCHAIN}/bin:$PATH"
		export LD_LIBRARY_PATH="${TOOLCHAIN}/lib:$LD_LIBRARY_PATH"
	elif [[ -d "${TOOLCHAIN_EXT}" ]]; then

		script_echo "I: Toolchain found at repository root"

		cd ${TOOLCHAIN_EXT}
		git pull
		cd ${ORIGIN_DIR}

		export PATH="${TOOLCHAIN_EXT}/bin:$PATH"
		export LD_LIBRARY_PATH="${TOOLCHAIN_EXT}/lib:$LD_LIBRARY_PATH"

		if [[ ${BUILD_KERNEL_CI} == 'true' ]]; then
			sudo mkdir -p /root/build/install/aarch64-linux-gnu
			sudo cp "${TOOLCHAIN_EXT}/lib" /root/build/install/aarch64-linux-gnu/

			sudo chown ${CURRENT_BUILD_USER} /root
			sudo chown ${CURRENT_BUILD_USER} /root/build
			sudo chown ${CURRENT_BUILD_USER} /root/build/install
			sudo chown ${CURRENT_BUILD_USER} /root/build/install/aarch64-linux-gnu
			sudo chown ${CURRENT_BUILD_USER} /root/build/install/aarch64-linux-gnu/lib
		fi
	else
		script_echo "I: Toolchain not found at default location or repository root"
		script_echo "   Downloading recommended toolchain at ${TOOLCHAIN_EXT}..."
		download_toolchain
	fi

	# Proton Clang 13
	# export CLANG_TRIPLE=aarch64-linux-gnu-
	export CROSS_COMPILE=aarch64-linux-gnu-
	export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
	export CC=${BUILD_PREF_COMPILER}
}

update_magisk() {
	script_echo " "
	script_echo "I: Updating Magisk..."
	${ORIGIN_DIR}/usr/magisk/update_magisk.sh 2>&1 | sed 's/^/     /'
}

show_usage() {
	script_echo "Usage: ./build.sh -d|--device <device> [main options] [variant options]"
	script_echo " "
	script_echo "Main options:"
	script_echo "-d, --device <device>: Set build device to build the kernel for. Required."
	script_echo "-n, --no-clean: Do not clean and update Magisk before build."
	script_echo "-m, --magisk: Pre-root the kernel with latest stable Magisk. Not available for 'recovery' variant."
	script_echo " "
	script_echo "Variant options (default: One UI/Fresh):"
	script_echo "-f, --fresh: Build One UI/Fresh variant of the kernel."
	script_echo "-a, --aosp: Build AOSP variant of the kernel."
	script_echo "-r, --recovery: Build kernel variant for recovery device trees."
	script_echo " "
	script_echo "Supported devices:"
	script_echo "- a50 (Samsung Galaxy A50)"
	script_echo "- a50s (Samsung Galaxy A50s)"
	exit_script
}

merge_config() {
	SUB_CONFIGS_DIR=${ORIGIN_DIR}/kernel/configs

	if [[ ! -e "${SUB_CONFIGS_DIR}/fresh_${1}.config" ]]; then
		script_echo "E: Subconfig not found on config DB!"
		script_echo "   ${SUB_CONFIGS_DIR}/fresh_${1}.config"
		script_echo "   Make sure it is in the proper directory."
		script_echo " "
		exit_script
	else
		echo "$(cat "${SUB_CONFIGS_DIR}/fresh_${1}.config")" >> "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
	fi
}

get_devicedb_info() {
	if [[ ! -e "${DEVICE_DB_DIR}/${BUILD_DEVICE_NAME}.sh" ]]; then
		script_echo "E: Device info not found from device DB!"
		script_echo "   ${DEVICE_DB_DIR}/${BUILD_DEVICE_NAME}.sh"
		script_echo "   Make sure it is in the proper directory."
		script_echo " "
		exit_script
	else
		source "${DEVICE_DB_DIR}/${BUILD_DEVICE_NAME}.sh"
	fi
}

check_defconfig() {
	if [[ ! -e "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_CONFIG}" ]]; then
		script_echo "E: Defconfig not found!"
		script_echo "   ${BUILD_CONFIG_DIR}/${BUILD_DEVICE_CONFIG}"
		script_echo "   Make sure it is in the proper directory."
		script_echo ""
		exit_script
	else
		echo "$(cat "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_CONFIG}")" > "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
	fi
}

build_kernel() {
	sleep 3
	script_echo " "

	if [[ ${BUILD_PREF_COMPILER_VERSION} == 'proton' ]]; then
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	elif [[ ${BUILD_PREF_COMPILER_VERSION} == 'google_snowcone' ]]; then
		# google_snowcone (aka Clang 12 for Android) uses an additional 'LLVM=1' flag
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	else
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	fi
}

build_image() {
	if [[ -e "$(pwd)/arch/arm64/boot/Image" ]]; then
		script_echo " "
		script_echo "I: Building kernel image..."
		script_echo "    Header/Page size: ${DEVICE_KERNEL_HEADER}/${DEVICE_KERNEL_PAGESIZE}"
		script_echo "      Board and base: ${DEVICE_KERNEL_BOARD}/${DEVICE_KERNEL_BASE}"
		script_echo " "
		script_echo "     Android Version: ${PLATFORM_VERSION}"
		script_echo "Security patch level: ${PLATFORM_PATCH_LEVEL}"

		${ORIGIN_DIR}/tools/make/bin/mkbootimg \
				  --kernel $(pwd)/arch/arm64/boot/Image \
				  --cmdline " " --board "$DEVICE_KERNEL_BOARD" \
				  --base $DEVICE_KERNEL_BASE --pagesize $DEVICE_KERNEL_PAGESIZE \
				  --kernel_offset $DEVICE_KERNEL_OFFSET --ramdisk_offset $DEVICE_RAMDISK_OFFSET \
				  --second_offset $DEVICE_SECOND_OFFSET --tags_offset $DEVICE_TAGS_OFFSET \
				  --os_version "$PLATFORM_VERSION" --os_patch_level "$PLATFORM_PATCH_LEVEL" \
				  --header_version $DEVICE_KERNEL_HEADER --hashtype $DEVICE_DTB_HASHTYPE \
				  -o ${ORIGIN_DIR}/tools/make/boot.img

		if [[ ! -f ${ORIGIN_DIR}/tools/make/boot.img ]]; then
			script_echo " "
			script_echo "E: Kernel image not built successfully!"
			script_echo "   Errors can be fround from above."
			sleep 3
			exit_script
		else
			rm -f $(pwd)/arch/arm64/boot/Image
		fi

	else
		script_echo "E: Image not built!"
		script_echo "   Errors can be fround from above."
		sleep 3
		exit_script
	fi
}

export_image() {
	if [[ -e "$(pwd)/arch/arm64/boot/Image" ]]; then
		script_echo " "
		script_echo "I: Exporting kernel image..."
		mv -f $(pwd)/arch/arm64/boot/Image ${BUILD_KERNEL_OUTPUT}
	else
		script_echo "E: Image not built!"
		script_echo "   Errors can be fround from above."
		sleep 3
		exit_script
	fi
}

build_dtb() {
	$(pwd)/tools/dtb/mkdtboimg cfg_create \
			--dtb-dir=$(pwd) \
			$(pwd)/tools/dtb/dtb.img \
			"$(pwd)/arch/arm64/boot/config/exynos9610-${BUILD_DEVICE_NAME}.dtb.config"
}

build_dtbo() {
	$(pwd)/tools/dtb/mkdtboimg cfg_create \
			--dtb-dir=$(pwd) \
			$(pwd)/tools/dtb/dtbo.img \
			"$(pwd)/arch/arm64/boot/config/exynos9610-${BUILD_DEVICE_NAME}.dtbo.config"
}

build_package() {
	script_echo " "
	script_echo "I: Building kernel ZIP..."

	mv ${ORIGIN_DIR}/tools/make/boot.img $(pwd)/tools/make/package/boot.img -f
	touch $(pwd)/tools/make/package/fresh_core.prop
	echo "fresh.core.build=${BUILD_DATE}" > $(pwd)/tools/make/package/fresh_core.prop
	echo "fresh.core.branch=${BUILD_KERNEL_BRANCH}" >> $(pwd)/tools/make/package/fresh_core.prop

	cd $(pwd)/tools/make/package

	zip -9 -r ./${FILE_OUTPUT} ./* 2>&1 | sed 's/^/     /'
	mv ./${FILE_OUTPUT} ${BUILD_KERNEL_OUTPUT}
	cd ${ORIGIN_DIR}
}

script_echo ' '
script_echo '==============================================='
script_echo '            _____              _               '
script_echo '           |  ___| __ ___  ___| |__            '
script_echo '           | |_ | .__/ _ \/ __| ._ \           '
script_echo '           |  _|| | |  __/\__ \ | | |          '
script_echo '           |_|  |_|  \___||___/_| |_|          '
script_echo '                                               '
script_echo '==============================================='
script_echo '         Fresh Core Kernel Build Script        '
script_echo '       by TenSeventy7 - Licensed in GPLv3      '
script_echo '                                               '
script_echo '       Originally built for Project ShadowX    '
script_echo '            Last Updated Aug 05 2021           '
script_echo '==============================================='
script_echo ' '

POSITIONAL=()
while [[ $# -gt 0 ]]; do
  key="$1"

  case $key in
    -d|--device)
      BUILD_DEVICE_NAME=`echo ${2} | tr 'A-Z' 'a-z'`
      shift; shift # past value
      ;;
    -c|--automated)
      BUILD_KERNEL_CI='true'
      shift
      ;;
    -n|--no-clean)
      BUILD_KERNEL_DIRTY='true'
      shift
      ;;
    -m|--magisk)
      BUILD_KERNEL_MAGISK='true'
      shift
      ;;
    -f|--fresh)
      BUILD_KERNEL_CODE='fresh'
      BUILD_FRESH='true'
      shift
      ;;
    -a|--aosp)
      BUILD_KERNEL_CODE='aosp'
      BUILD_AOSP='true'
      shift
      ;;
    -r|--recovery)
      BUILD_KERNEL_CODE='recovery'
      BUILD_RECOVERY='true'
      shift
      ;;
    -h|--help)
      SCRIPT_SHOW_HELP='true'
      shift
      ;;
    --default)
      DEFAULT=YES
      shift # past value
      ;;
    *)    # unknown option
      POSITIONAL+=("$1") # save it in an array for later
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ ${SCRIPT_SHOW_HELP} == 'true' ]]; then
	show_usage
fi

# Build variables - DO NOT CHANGE
VERSION=$(grep -m 1 VERSION "$(pwd)/Makefile" | sed 's/^.*= //g')
PATCHLEVEL=$(grep -m 1 PATCHLEVEL "$(pwd)/Makefile" | sed 's/^.*= //g')
SUBLEVEL=$(grep -m 1 SUBLEVEL "$(pwd)/Makefile" | sed 's/^.*= //g')

BUILD_KERNEL_BRANCH=${GITHUB_REF##*/}
BUILD_DATE=$(date +%s)
BUILD_CONFIG_DIR=$(pwd)/arch/arm64/configs
BUILD_OUTPUT_DIR=$(pwd)/output

if [[ -z ${BUILD_KERNEL_CODE} ]]; then
	BUILD_KERNEL_CODE='fresh'
fi

if [[ -z ${BUILD_KERNEL_MAGISK} ]]; then
	BUILD_KERNEL_MAGISK='false'
fi

BUILD_DEVICE_CONFIG=exynos9610-${BUILD_DEVICE_NAME}_core_defconfig
BUILD_DEVICE_TMP_CONFIG=tmp_exynos9610-${BUILD_DEVICE_NAME}_${BUILD_KERNEL_CODE}_defconfig
export KCONFIG_BUILTINCONFIG=${BUILD_CONFIG_DIR}/exynos9610-${BUILD_DEVICE_NAME}_default_defconfig
BUILD_DEVICE_OUTPUT=${BUILD_OUTPUT_DIR}/${BUILD_DEVICE_NAME}

if [[ ! -z ${BUILD_KERNEL_BRANCH} ]]; then

	if [[ ${BUILD_KERNEL_BRANCH} == *"android-"* ]]; then
		BUILD_KERNEL_BRANCH='mainline'
	fi

	if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
		FILE_OUTPUT=FreshCore-${BUILD_KERNEL_CODE}_${BUILD_DEVICE_NAME}_${BUILD_KERNEL_BRANCH}_${BUILD_DATE}.zip
	else
		FILE_OUTPUT=FreshCore-${BUILD_KERNEL_CODE}-noroot_${BUILD_DEVICE_NAME}_${BUILD_KERNEL_BRANCH}_${BUILD_DATE}.zip
	fi

	if [[ ${BUILD_KERNEL_BRANCH} == "mainline" ]]; then
		LOCALVERSION=' - Fresh Core'
		export LOCALVERSION=' - Fresh Core'
	else
		LOCALVERSION=" - Fresh Core-${BUILD_KERNEL_BRANCH}"
		export LOCALVERSION=" - Fresh Core-${BUILD_KERNEL_BRANCH}"
	fi
else
	if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
		FILE_OUTPUT=FreshCore-${BUILD_KERNEL_CODE}_${BUILD_DEVICE_NAME}_user_${BUILD_DATE}.zip
	else
		FILE_OUTPUT=FreshCore-${BUILD_KERNEL_CODE}-noroot_${BUILD_DEVICE_NAME}_user_${BUILD_DATE}.zip
	fi

	BUILD_KERNEL_BRANCH='user'
	LOCALVERSION=" - Fresh Core-user"
	export LOCALVERSION=" - Fresh Core-user"
fi

if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
	BUILD_KERNEL_OUTPUT="${ORIGIN_DIR}/Image"
else
	BUILD_KERNEL_OUTPUT="${ORIGIN_DIR}/${FILE_OUTPUT}"
fi

if [[ "${BUILD_RECOVERY}${BUILD_AOSP}${BUILD_FRESH}" == *"truetrue"* ]]; then
	script_echo "E: Multiple variants selected!"
	script_echo "   You can only build one kernel variant at a time."
	script_echo " "
	show_usage
fi

if [[ -z ${BUILD_DEVICE_NAME} ]]; then
	script_echo "E: No device selected to build kernel!"
	script_echo " "
	show_usage
else
	script_echo "I: Selected device: ${BUILD_DEVICE_NAME}"
	script_echo "   Selected variant: ${BUILD_KERNEL_CODE}"
	script_echo "   Kernel version: ${VERSION}.${PATCHLEVEL}.${SUBLEVEL}"
	script_echo "   Magisk: ${BUILD_KERNEL_MAGISK}"
	script_echo "   Output file: ${BUILD_KERNEL_OUTPUT}"
fi

verify_toolchain

if [[ ${BUILD_KERNEL_CI} == 'true' ]]; then
	export KBUILD_BUILD_USER=Clembot
	export KBUILD_BUILD_HOST=Lumiose-CI

	script_echo " "
	script_echo "I: Beep boop! CI build!"
fi

if [[ ${BUILD_KERNEL_DIRTY} == 'true' ]]; then
	script_echo " "
	script_echo "I: Dirty build!"
else
	script_echo " "
	script_echo "I: Clean build!"
	make CC=${BUILD_PREF_COMPILER} clean 2>&1 | sed 's/^/     /'
	make CC=${BUILD_PREF_COMPILER} mrproper 2>&1 | sed 's/^/     /'
fi

check_defconfig
get_devicedb_info

if [[ ${BUILD_KERNEL_CODE} == 'fresh' ]]; then
	# One UI needs a couple of Knox stuff so it won't spout errors on logs. Keep them enabled for One UI.
	merge_config partial-deknox
	merge_config samsung-mtp
	merge_config oneui
elif [[ ${BUILD_KERNEL_CODE} == 'aosp' ]]; then
	merge_config partial-deknox
	merge_config aosp
elif [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
	merge_config partial-deknox
	merge_config samsung-mtp
	merge_config oneui
	merge_config aosp
fi

if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
	if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
		script_echo " "
		script_echo "I: Recovery variant selected."
		script_echo "   Magisk is not an available option to allow recovery to boot."
		script_echo "   Patch the image using Magisk manually to get root."
		merge_config non-root
		sleep 3
	else
		merge_config pre-root

		if [[ ! ${BUILD_KERNEL_DIRTY} == 'true' ]]; then
			update_magisk
		fi
	fi
else
	merge_config non-root
fi

build_kernel

if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
	export_image 
else
	build_image
	build_package
fi

script_echo " "
script_echo "I: Yay! Kernel build is done!"
script_echo "   File can be found at:"
script_echo "   ${BUILD_KERNEL_OUTPUT}"
rm -f "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
sleep 7
