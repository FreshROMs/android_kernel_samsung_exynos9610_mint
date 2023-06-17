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
#  Minty - The kernel build script for Mint
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
	git clone https://gitlab.com/TenSeventy7/exynos9610_toolchains_fresh.git ${TOOLCHAIN_EXT} --single-branch -b ${BUILD_PREF_COMPILER_VERSION} --depth 1 2>&1 | sed 's/^/     /'
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
			if [[ ${BUILD_PREF_COMPILER_VERSION} == 'proton' ]]; then
				sudo mkdir -p /root/build/install/aarch64-linux-gnu
				sudo cp -r "${TOOLCHAIN_EXT}/lib" /root/build/install/aarch64-linux-gnu/

				sudo chown ${CURRENT_BUILD_USER} /root
				sudo chown ${CURRENT_BUILD_USER} /root/build
				sudo chown ${CURRENT_BUILD_USER} /root/build/install
				sudo chown ${CURRENT_BUILD_USER} /root/build/install/aarch64-linux-gnu
				sudo chown ${CURRENT_BUILD_USER} /root/build/install/aarch64-linux-gnu/lib
			fi
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

	if [[ "x${BUILD_KERNEL_MAGISK_BRANCH}" == "xcanary" ]]; then
		MAGISK_BRANCH="canary"
	elif [[ "x${BUILD_KERNEL_MAGISK_BRANCH}" == "xlocal" ]]; then
		MAGISK_BRANCH="local"
	else
		MAGISK_BRANCH=""
	fi

	${ORIGIN_DIR}/usr/magisk/update_magisk.sh ${MAGISK_BRANCH} 2>&1 | sed 's/^/     /'
}

fill_magisk_config() {
	MAGISK_USR_DIR="${ORIGIN_DIR}/usr/magisk/"

	script_echo " "
	script_echo "I: Configuring Magisk..."

	if [[ -f "$MAGISK_USR_DIR/backup_magisk" ]]; then
		rm "$MAGISK_USR_DIR/backup_magisk"
	fi

	echo "KEEPVERITY=true" >> "$MAGISK_USR_DIR/backup_magisk"
	echo "KEEPFORCEENCRYPT=true" >> "$MAGISK_USR_DIR/backup_magisk"
	echo "RECOVERYMODE=false" >> "$MAGISK_USR_DIR/backup_magisk"
	echo "PREINITDEVICE=userdata" >> "$MAGISK_USR_DIR/backup_magisk"

	# Create a unique random seed per-build
	script_echo "   - Generating a unique random seed for this build..."
	RANDOMSEED=$(tr -dc 'a-f0-9' < /dev/urandom | head -c 16)
	echo "RANDOMSEED=0x$RANDOMSEED" >> "$MAGISK_USR_DIR/backup_magisk"
}

show_usage() {
	script_echo "Usage: ./build.sh -d|--device <device> -v|--variant <variant> [main options]"
	script_echo " "
	script_echo "Main options:"
	script_echo "-d, --device <device>     Set build device to build the kernel for. Required."
	script_echo "-a, --android <version>   Set Android version to build the kernel for. (Default: 11)"
	script_echo "-v, --variant <variant>   Set build variant to build the kernel for. Required."
	script_echo " "
	script_echo "-n, --no-clean            Do not clean and update Magisk before build."
	script_echo "-m, --magisk [canary]     Pre-root the kernel with Magisk. Optional flag to use canary builds."
	script_echo "-k, --kernelsu            Pre-root the kernel with KernelSU."
	script_echo "                          Not available for 'recovery' variant."
	script_echo "-p, --permissive          Build kernel with SELinux fully permissive. NOT RECOMMENDED!"
	script_echo " "
	script_echo "-h, --help                Show this message."
	script_echo " "
	script_echo "Variant options:"
	script_echo "    oneui: Build Mint for use with stock and One UI-based ROMs."
	script_echo "     aosp: Build Mint for use with AOSP and AOSP-based Generic System Images (GSIs)."
	script_echo " recovery: Build Mint for use with recovery device trees. Doesn't build a ZIP."
	script_echo " "
	script_echo "Supported devices:"
	script_echo "  a50 (Samsung Galaxy A50)"
	script_echo " a50s (Samsung Galaxy A50s)"
	exit_script
}

merge_config() {
	if [[ ! -e "${SUB_CONFIGS_DIR}/mint_${1}.config" ]]; then
		script_echo "E: Subconfig not found on config DB!"
		script_echo "   ${SUB_CONFIGS_DIR}/mint_${1}.config"
		script_echo "   Make sure it is in the proper directory."
		script_echo " "
		exit_script
	else
		echo "$(cat "${SUB_CONFIGS_DIR}/mint_${1}.config")" >> "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
	fi
}

set_android_version() {
	echo "CONFIG_MINT_PLATFORM_VERSION=${BUILD_ANDROID_PLATFORM}" >> "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
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

	if [[ ${BUILD_PREF_COMPILER_VERSION} == 'rui' ]]; then
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	elif [[ ${BUILD_PREF_COMPILER_VERSION} == 'proton' ]]; then
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} HOSTCC=clang HOSTCXX=clang++ AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} HOSTCC=clang HOSTCXX=clang++ AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	elif [[ ${BUILD_PREF_COMPILER_VERSION} == 'google_snowcone' ]]; then
		# google_snowcone (aka Clang 12 for Android) uses an additional 'LLVM=1' flag
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	elif [[ ${BUILD_PREF_COMPILER} == 'clang' ]]; then
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) CC=${BUILD_PREF_COMPILER} LLVM=1 -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	else
		make -C $(pwd) ${BUILD_DEVICE_TMP_CONFIG} LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
		make -C $(pwd) -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
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
				  --cmdline "androidboot.selinux=permissive androidboot.selinux=permissive loop.max_part=7" --board "$DEVICE_KERNEL_BOARD" \
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

set_file_name() {
ZIP_ONEUI_VERSION=""

if [[ ${BUILD_KERNEL_CODE} == "oneui" ]]; then
	ZIP_ONEUI_VERSION="$((${BUILD_ANDROID_PLATFORM} - 8))"
fi

if [[ ! -z ${BUILD_KERNEL_BRANCH} ]]; then

	if [[ ${BUILD_KERNEL_BRANCH} == *"android-"* ]]; then
		BUILD_KERNEL_BRANCH='mainline'
	fi

	if [[ ${BUILD_KERNEL_PERMISSIVE} == 'true' ]]; then
		FILE_NAME_SELINUX="Permissive"
	else
		FILE_NAME_SELINUX="Enforcing"
	fi

	if [[ ${BUILD_KERNEL_BRANCH} == "mainline" ]]; then
		LOCALVERSION=" - Mint ${KERNEL_BUILD_VERSION}"
		export LOCALVERSION=" - Mint ${KERNEL_BUILD_VERSION}"

		if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
			FILE_OUTPUT=Mint-${KERNEL_BUILD_VERSION}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-Magisk_${BUILD_DEVICE_NAME^}.zip
        elif [[ ${BUILD_KERNEL_KERNELSU} == 'true' ]]; then
			FILE_OUTPUT=Mint-${KERNEL_BUILD_VERSION}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-KSU_${BUILD_DEVICE_NAME^}.zip
		else
			FILE_OUTPUT=Mint-${KERNEL_BUILD_VERSION}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-NoRoot_${BUILD_DEVICE_NAME^}.zip
		fi
	else
		LOCALVERSION=" - Mint Beta ${GITHUB_RUN_NUMBER}"
		export LOCALVERSION=" - Mint Beta ${GITHUB_RUN_NUMBER}"

		if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
			FILE_OUTPUT=MintBeta-${GITHUB_RUN_NUMBER}.A${BUILD_ANDROID_PLATFORM}.${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-${FILE_NAME_SELINUX}-Magisk_${BUILD_DEVICE_NAME^}.CI.zip
        elif [[ ${BUILD_KERNEL_KERNELSU} == 'true' ]]; then
			FILE_OUTPUT=MintBeta-${GITHUB_RUN_NUMBER}.A${BUILD_ANDROID_PLATFORM}.${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-${FILE_NAME_SELINUX}-KSU_${BUILD_DEVICE_NAME^}.CI.zip
		else
			FILE_OUTPUT=MintBeta-${GITHUB_RUN_NUMBER}.A${BUILD_ANDROID_PLATFORM}.${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}-${FILE_NAME_SELINUX}-NoRoot_${BUILD_DEVICE_NAME^}.CI.zip
		fi
	fi
else
	if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
		FILE_OUTPUT=Mint-${BUILD_DATE}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}_${BUILD_DEVICE_NAME^}_UB.zip
    elif [[ ${BUILD_KERNEL_KERNELSU} == 'true' ]]; then
		FILE_OUTPUT=Mint-${BUILD_DATE}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}_${BUILD_DEVICE_NAME^}_UB.zip
	else
		FILE_OUTPUT=Mint-${BUILD_DATE}.A${BUILD_ANDROID_PLATFORM}_${FILE_KERNEL_CODE}${ZIP_ONEUI_VERSION}_${BUILD_DEVICE_NAME^}_UB.zip
	fi

	BUILD_KERNEL_BRANCH='user'
	LOCALVERSION=" - Mint-user"
	export LOCALVERSION=" - Mint-user"
fi
}

build_package() {
	script_echo " "
	script_echo "I: Building kernel ZIP..."

	# Copy kernel image to package directory
	mv $(pwd)/arch/arm64/boot/Image $(pwd)/tools/make/package/Image -f

	# Copy DTB image to package directory
	mv $(pwd)/arch/arm64/boot/dtb_exynos.img $(pwd)/tools/make/package/dtb.img -f

	# Make the manifest
	touch $(pwd)/tools/make/package/mint.prop

	echo "ro.mint.build.date=${BUILD_DATE}" > $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.build.branch=${BUILD_KERNEL_BRANCH}" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.droid.device=${BUILD_DEVICE_NAME^}" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.droid.variant=${FILE_KERNEL_CODE^}" >> $(pwd)/tools/make/package/mint.prop

	if [[ ${BUILD_KERNEL_BRANCH} == "mainline" ]]; then
		echo "ro.mint.droid.beta=false" >> $(pwd)/tools/make/package/mint.prop
		echo "ro.mint.build.version=${KERNEL_BUILD_VERSION}" >> $(pwd)/tools/make/package/mint.prop
	else
		echo "ro.mint.droid.beta=true" >> $(pwd)/tools/make/package/mint.prop
		echo "ro.mint.build.version=${GITHUB_RUN_NUMBER}" >> $(pwd)/tools/make/package/mint.prop
	fi
	
	echo "ro.mint.droid.android=${BUILD_ANDROID_PLATFORM}" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.droid.platform=11-${BUILD_ANDROID_PLATFORM}" >> $(pwd)/tools/make/package/mint.prop

	# Device support
	echo "ro.mint.device.name1=${BUILD_DEVICE_NAME}" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.device.name2=${BUILD_DEVICE_NAME}xx" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.device.name3=${BUILD_DEVICE_NAME}dd" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.device.name4=${BUILD_DEVICE_NAME}ser" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.device.name5=${BUILD_DEVICE_NAME}ltn" >> $(pwd)/tools/make/package/mint.prop
	echo "ro.mint.device.name6=${BUILD_DEVICE_NAME}*" >> $(pwd)/tools/make/package/mint.prop

	cd $(pwd)/tools/make/package

	zip -9 -r ./${FILE_OUTPUT} ./* 2>&1 | sed 's/^/     /'
	mv ./${FILE_OUTPUT} ${BUILD_KERNEL_OUTPUT}
	cd ${ORIGIN_DIR}
}

script_echo ' '
script_echo '==============================================='
script_echo "                       _       _               "
script_echo "                 /\/\ (_)_ __ | |_             "
script_echo "                /    \| | '_ \| __|            "
script_echo "               / /\/\ \ | | | | |_             "
script_echo "               \/    \/_|_| |_|\__|            "
script_echo "                                               "
script_echo '==============================================='
script_echo '           Minty - Kernel Build Script         '
script_echo '            Part of The Fresh Project          '
script_echo '       by TenSeventy7 - Licensed in GPLv3      '
script_echo '                                               '
script_echo '       Originally built for Project ShadowX    '
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
    -a|--android)
      BUILD_ANDROID_PLATFORM=`echo ${2} | tr 'A-Z' 'a-z'`

		  if [[ ! ${BUILD_ANDROID_PLATFORM} =~ ^[0-9]+$ ]]; then
				script_echo "E: Wrong Android version syntax!"
				script_echo " "
				show_usage
		  fi
      
      shift; shift # past value
      ;;
    -v|--variant)
      BUILD_KERNEL_CODE=`echo ${2} | tr 'A-Z' 'a-z'`
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
	-k|--kernelsu)
      BUILD_KERNEL_KERNELSU='true'
      shift; shift # past value
      ;;
    -m|--magisk)
      BUILD_KERNEL_MAGISK='true'
      BUILD_KERNEL_MAGISK_BRANCH=`echo ${2} | tr 'A-Z' 'a-z'`

      # Shift twice if asking for canary or local builds. Otherwise, shift only once.
      if [[ "x${BUILD_KERNEL_MAGISK_BRANCH}" == "xcanary" ]]; then
      	shift
      elif [[ "x${BUILD_KERNEL_MAGISK_BRANCH}" == "xlocal" ]]; then
      	shift
      fi
      
      shift # past value
      ;;
    -p|--permissive)
      BUILD_KERNEL_PERMISSIVE='true'
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
SUB_CONFIGS_DIR=${ORIGIN_DIR}/kernel/configs
BUILD_OUTPUT_DIR=$(pwd)/output

# Retrofit: 'fresh' variant now points to 'oneui' 
if [[ ${BUILD_KERNEL_CODE} == "fresh" ]]; then
	BUILD_KERNEL_CODE='oneui'
fi

if [[ -z ${BUILD_KERNEL_CODE} ]]; then
	script_echo "E: No variant selected!"
	script_echo " "
	show_usage
else
	if [[ ! -e "${SUB_CONFIGS_DIR}/mint_variant_${BUILD_KERNEL_CODE}.config" ]]; then
		script_echo "E: Variant is not valid!"
		script_echo " "
		show_usage
	fi
fi

if [[ -z ${BUILD_KERNEL_MAGISK} ]]; then
	BUILD_KERNEL_MAGISK='false'
fi

if [[ -z ${BUILD_KERNEL_KERNELSU} ]]; then
	BUILD_KERNEL_KERNELSU='false'
fi

BUILD_DEVICE_CONFIG=exynos9610-${BUILD_DEVICE_NAME}_core_defconfig
BUILD_DEVICE_TMP_CONFIG=tmp_exynos9610-${BUILD_DEVICE_NAME}_${BUILD_KERNEL_CODE}_defconfig
export KCONFIG_BUILTINCONFIG=${BUILD_CONFIG_DIR}/exynos9610-${BUILD_DEVICE_NAME}_default_defconfig
BUILD_DEVICE_OUTPUT=${BUILD_OUTPUT_DIR}/${BUILD_DEVICE_NAME}

if [[ ! -e "${DEVICE_DB_DIR}/kernel_info.sh" ]]; then
	script_echo "E: Kernel info not found from DB!"
	script_echo "   ${DEVICE_DB_DIR}/kernel_info.sh"
	script_echo "   Make sure it is in the proper directory."
	script_echo " "
	exit_script
else
	source "${DEVICE_DB_DIR}/kernel_info.sh"
fi

if [[ -z ${BUILD_ANDROID_PLATFORM} ]]; then
	BUILD_ANDROID_PLATFORM=11
fi

if [[ ${BUILD_KERNEL_CODE} == "aosp" ]]; then
	FILE_KERNEL_CODE='AOSP'
elif [[ ${BUILD_KERNEL_CODE} == "oneui" ]]; then
	FILE_KERNEL_CODE='OneUI'
else
	FILE_KERNEL_CODE='Recovery'
fi

set_file_name

if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
	BUILD_KERNEL_OUTPUT="${ORIGIN_DIR}/Image"
else
	BUILD_KERNEL_OUTPUT="${ORIGIN_DIR}/${FILE_OUTPUT}"
fi

if [[ -z ${BUILD_DEVICE_NAME} ]]; then
	script_echo "E: No device selected to build kernel!"
	script_echo " "
	show_usage
else
	script_echo "I: Selected device:    ${BUILD_DEVICE_NAME}"
	script_echo "   Selected variant:   ${FILE_KERNEL_CODE}"
	script_echo "   Kernel version:     ${VERSION}.${PATCHLEVEL}.${SUBLEVEL}"
	script_echo "   Android version:    ${BUILD_ANDROID_PLATFORM}"
	script_echo "   Magisk-enabled:     ${BUILD_KERNEL_MAGISK}"
	script_echo "   KernelSU-enabled:   ${BUILD_KERNEL_KERNELSU}"
	script_echo "   Output ZIP file:    ${BUILD_KERNEL_OUTPUT}"
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

# All variants get semi-Knox while we're fixing compile issues
merge_config partial-deknox-${BUILD_ANDROID_PLATFORM}
merge_config mali-${BUILD_ANDROID_PLATFORM}
merge_config variant_${BUILD_KERNEL_CODE}

if [[ ${BUILD_KERNEL_PERMISSIVE} == 'true' ]]; then
	script_echo "WARNING! You're building this kernel in permissive mode!"
	script_echo "         This is insecure and may make your device vulnerable"
	script_echo "         This kernel has NO RESPONSIBILITY on whatever happens next."
	merge_config selinux-permissive
fi

if [[ "${BUILD_KERNEL_MAGISK}" == 'true' || "${BUILD_KERNEL_KERNELSU}" == 'true' ]]; then
	if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
		script_echo " "
		script_echo "I: Recovery variant selected."
		script_echo "   Magisk is not an available option to allow recovery to boot."
		script_echo "   Patch the image using Magisk manually to get root."
		merge_config non-root
		sleep 3
	else
		if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
			merge_config pre-root
		fi

		if [[ ! ${BUILD_KERNEL_DIRTY} == 'true' ]]; then
			if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
				update_magisk
		  	fi

			if [[ ${BUILD_KERNEL_KERNELSU} == 'true' ]]; then
				curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
				wget -q "https://github.com/rushiranpise/android_kernel_samsung_exynos9610_mint/raw/patch/kernelsu.patch" -O KernelSU.patch
				git apply ./KernelSU.patch
		  	fi
		fi

		if [[ ${BUILD_KERNEL_MAGISK} == 'true' ]]; then
			fill_magisk_config
		fi
	fi
else
	merge_config non-root
fi

# Use no-product Exynos DTB when building AOSP
if [[ ${BUILD_KERNEL_CODE} == "aosp" ]]; then
	script_echo "I: Copying no-product DTB file for use with AOSP ROMs."
	cp -f $(pwd)/arch/arm64/boot/dts/exynos/aosp/exynos9610.dts $(pwd)/arch/arm64/boot/dts/exynos/
fi

set_android_version
build_kernel

if [[ ${BUILD_KERNEL_CODE} == 'recovery' ]]; then
	export_image 
else
	build_image
	build_package
fi

TIME_NOW=$(date +%s)
BUILD_TIME=$((TIME_NOW-BUILD_DATE))
BUILD_TIME_STR=$(printf '%02dh:%02dm:%02ds\n' $((BUILD_TIME/3600)) $((BUILD_TIME%3600/60)) $((BUILD_TIME%60)))

# Inform user if built with KSU
if [[ ${BUILD_KERNEL_KERNELSU} == 'true' ]]; then
	script_echo " "
	script_echo "I: Kernel built with KernelSU additions."
	script_echo "   You will need to revert these changes yourself"
	script_echo "   once the build has finished."
	script_echo " "
fi

script_echo " "
script_echo "I: Yay! Kernel build is done!"
script_echo "   Kernel build took ${BUILD_TIME_STR}"
script_echo "   File can be found at:"
script_echo "   ${BUILD_KERNEL_OUTPUT}"
rm -f "${BUILD_CONFIG_DIR}/${BUILD_DEVICE_TMP_CONFIG}"
sleep 7
