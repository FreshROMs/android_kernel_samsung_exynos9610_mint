#!/bin/bash
#
#  =========================
#  
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

export ARCH=arm64
export SUBARCH=arm64
export ANDROID_MAJOR_VERSION=r
export PLATFORM_VERSION=11.0.0
export $ARCH

# export KBUILD_BUILD_USER=TenSeventy7
# export KBUILD_BUILD_HOST=Lumiose-Build

TOOLCHAIN=$HOME/toolchain
TOOLCHAIN_EXT=$(pwd)/toolchain

TOOLCHAIN_ARM32=$HOME/toolchain_arm32/gcc-arm-10.2-2020.11-x86_64-arm-none-eabi
TOOLCHAIN_EXT_ARM32=$(pwd)/toolchain_arm32/gcc-arm-10.2-2020.11-x86_64-arm-none-eabi

DEVICE_BUILD=`echo $1 | tr 'A-Z' 'a-z'`
ORIG_DIR=$(pwd)
OPTIONS=`echo ${2} ${3} ${4} ${5} ${6} ${7} ${8} | tr 'A-Z' 'a-z'`

CONFIG_DIR=$(pwd)/arch/arm64/configs
OUTPUT_DIR=$(pwd)/output
DEIVCE_OUTPUT_DIR=${OUTPUT_DIR}/${DEVICE_BUILD}
BUILDDATE=$(date +%s)

if [[ ! -z ${GITHUB_REF##*/} ]]; then
	FILE_OUTPUT=FRSH_CORE_${DEVICE_BUILD}_aosp_${GITHUB_REF##*/}_${BUILDDATE}.zip
	if [[ ${GITHUB_REF##*/} == "staging" ]]; then
		LOCALVERSION=' - Fresh Core'
		export LOCALVERSION=' - Fresh Core'
	else
		LOCALVERSION=" - Fresh Core-${GITHUB_REF##*/}"
		export LOCALVERSION=" - Fresh Core-${GITHUB_REF##*/}"
	fi
else
	FILE_OUTPUT=FRSH_CORE_${DEVICE_BUILD}_aosp_user_${BUILDDATE}.zip
	LOCALVERSION="-user"
	export LOCALVERSION="-user"
fi

script_echo() {
	echo "  $1"
}

exit_script() {
	kill -INT $$
}

download_toolchain() {
	git clone https://github.com/CruelKernel/aarch64-cruel-elf.git ${TOOLCHAIN_EXT} 2>&1 | sed 's/^/     /'
	verify_toolchain
}

download_toolchain32() {
	mkdir -p ${TOOLCHAIN_EXT_ARM32}
	cd ${TOOLCHAIN_EXT_ARM32}
	wget https://developer.arm.com/-/media/Files/downloads/gnu-a/10.2-2020.11/binrel/gcc-arm-10.2-2020.11-x86_64-arm-none-eabi.tar.xz --output-document=${TOOLCHAIN_EXT_ARM32}/gcc-arm-10.2-2020.11-x86_64-arm-none-eabi.tar.xz
	tar -xvf ${TOOLCHAIN_EXT_ARM32}/gcc-arm-10.2-2020.11-x86_64-arm-none-eabi.tar.xz
	verify_toolchain
}

verify_toolchain() {
	if [[ -d "${TOOLCHAIN}" ]]
	then
		sleep 2
		script_echo "I: Toolchain found at default location"

		export PATH="${TOOLCHAIN}/bin:$PATH"

		# Cruel Kernel GCC 11.1.0
		export CROSS_COMPILE=${TOOLCHAIN}/bin/aarch64-cruel-elf-

	elif [[ -d "${TOOLCHAIN_EXT}" ]]
	then
		sleep 2
		script_echo "I: Toolchain found at repository root"

		export PATH="${TOOLCHAIN_EXT}/bin:$PATH"

		# Cruel Kernel GCC 11.1.0
		export CROSS_COMPILE=${TOOLCHAIN_EXT}/bin/aarch64-cruel-elf-

	else
		script_echo "I: Toolchain not found at default location or repository root"
		script_echo "   Downloading recommended toolchain at ${TOOLCHAIN_EXT}..."
		download_toolchain
	fi

	if [[ -d "${TOOLCHAIN_ARM32}" ]]
	then
		sleep 2
		script_echo "I: 32-bit Toolchain found at default location"

		export PATH="${TOOLCHAIN_ARM32}/bin:$PATH"

		# Linaro/ARM GCC 10.2
		export CROSS_COMPILE_ARM32=${TOOLCHAIN_ARM32}/bin/arm-none-eabi-

	elif [[ -d "${TOOLCHAIN_EXT_ARM32}" ]]
	then
		sleep 2
		script_echo "I: 32-bit Toolchain found at repository root"

		export PATH="${TOOLCHAIN_EXT_ARM32}/bin:$PATH"

		# Linaro/ARM GCC 10.2
		export CROSS_COMPILE_ARM32=${TOOLCHAIN_EXT_ARM32}/bin/arm-none-eabi-

	else
		script_echo "I: 32-bit Toolchain not found at default location or repository root"
		script_echo "   Downloading recommended toolchain at ${TOOLCHAIN_EXT}..."
		download_toolchain32
	fi
}

update_magisk() {
	script_echo " "
	script_echo "I: Updating Magisk..."
	$(pwd)/usr/magisk/update_magisk.sh 2>&1 | sed 's/^/     /'
}

show_usage() {
	script_echo "USAGE: ./build.sh (device) [dirty]"
	script_echo " "
	script_echo "       Supported devices:"
	script_echo "        - a50"
	script_echo "        - m30s"
	script_echo "        - a50s"
	exit_script
}

check_defconfig() {
	if [[ ! -e "${CONFIG_DIR}/${CHECK_DEFCONFIG}" ]]
	then
		script_echo "E: Defconfig not found!"
		script_echo "   ${CONFIG_DIR}/${CHECK_DEFCONFIG}"
		script_echo "   Make sure it is in the proper directory."
		script_echo ""
		show_usage
	else
		sleep 2
	fi
}

build_kernel() {
	CHECK_DEFCONFIG=exynos9610-${DEVICE_BUILD}_aosp_defconfig
	export KCONFIG_BUILTINCONFIG=${CONFIG_DIR}/exynos9610-${DEVICE_BUILD}_default_defconfig

	check_defconfig

	VERSION=$(grep -m 1 VERSION "$(pwd)/Makefile" | sed 's/^.*= //g')
	PATCHLEVEL=$(grep -m 1 PATCHLEVEL "$(pwd)/Makefile" | sed 's/^.*= //g')
	SUBLEVEL=$(grep -m 1 SUBLEVEL "$(pwd)/Makefile" | sed 's/^.*= //g')

	script_echo " "
	script_echo "I: Building ${VERSION}.${PATCHLEVEL}.${SUBLEVEL} kernel..."
	script_echo "I: Output: $(pwd)/${FILE_OUTPUT}"
	sleep 3
	script_echo " "

	make -C $(pwd) $CHECK_DEFCONFIG LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
	make -C $(pwd) -j$(nproc --all) LOCALVERSION="${LOCALVERSION}" 2>&1 | sed 's/^/     /'
}

build_image() {
	if [[ -e "$(pwd)/arch/arm64/boot/Image" ]]; then
		script_echo " "
		script_echo "I: Building kernel image..."
		mv -f $(pwd)/arch/arm64/boot/Image $(pwd)/tools/aik/${DEVICE_BUILD}/split_img/boot.img-zImage

		if [[ ! -d "$(pwd)/tools/aik/${DEVICE_BUILD}/ramdisk" ]]; then
			mkdir -p $(pwd)/tools/aik/${DEVICE_BUILD}/ramdisk
		fi
		
		$(pwd)/tools/aik/${DEVICE_BUILD}/repackimg.sh 2>&1 | sed 's/^/     /'
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
			"$(pwd)/arch/arm64/boot/config/exynos9610-${DEVICE_BUILD}.dtb.config"
}

build_dtbo() {
	$(pwd)/tools/dtb/mkdtboimg cfg_create \
			--dtb-dir=$(pwd) \
			$(pwd)/tools/dtb/dtbo.img \
			"$(pwd)/arch/arm64/boot/config/exynos9610-${DEVICE_BUILD}.dtbo.config"
}

build_zip() {
	script_echo " "
	script_echo "I: Building kernel ZIP..."

	mv $(pwd)/tools/aik/${DEVICE_BUILD}/image-new.img $(pwd)/tools/package/others/boot.img -f

	if [[ ! -z ${GITHUB_REF##*/} ]]; then
		echo "fresh.addon.code=io.tns.shadowx.${GITHUB_REF##*/}" >> $(pwd)/tools/package/others/addon.prop
		echo "fresh.addon.build=${GITHUB_REF##*/}-${GITHUB_RUN_NUMBER}" >> $(pwd)/tools/package/others/addon.prop
		echo "fresh.addon.version=${GITHUB_RUN_NUMBER}" >> $(pwd)/tools/package/others/addon.prop
	else
		echo "fresh.addon.code=user.shadowx" >> $(pwd)/tools/package/others/addon.prop
		echo "fresh.addon.build=user-build" >> $(pwd)/tools/package/others/addon.prop
		echo "fresh.addon.version=1" >> $(pwd)/tools/package/others/addon.prop
	fi
	
	cd $(pwd)/tools/package/others
	zip -9 -r ./${FILE_OUTPUT} ./* 2>&1 | sed 's/^/     /'
	mv ./${FILE_OUTPUT} ${ORIG_DIR}/${FILE_OUTPUT}
	cd ${ORIG_DIR}
}

build_kernel_full() {
	build_kernel
	build_image
	build_zip

	script_echo " "
	script_echo "I: Build is done!"
	sleep 3
	script_echo " "
	script_echo "I: File can be found at:"
	script_echo "   ${ORIG_DIR}/${FILE_OUTPUT}"
	sleep 7
}


script_echo ''
script_echo '====================================================='
script_echo '             ArKernel Builder - Fresh Core           '
script_echo '          by TenSeventy7 - Licensed in GPLv3         '
script_echo '                                                     '
script_echo '           Originally built for Galahad Kernel       '
script_echo '        Updated Aug 10 2020 for Project ShadowX      '
script_echo '       Updated Apr 10 2021 for The Fresh Project     '
script_echo '====================================================='
script_echo ''

if [[ ! -z ${1} ]]; then
	verify_toolchain

	if [[ $2 == "dirty" ]]; then
		script_echo " "
		script_echo "I: Dirty build!"
	elif [[ $2 == "ci" ]]; then
		export KBUILD_BUILD_USER=Clembot
		export KBUILD_BUILD_HOST=Lumiose-CI
		script_echo " "
		script_echo "I: CI build!"
		make clean 2>&1 | sed 's/^/     /'
		make mrproper 2>&1 | sed 's/^/     /'
		update_magisk
	else
		script_echo " "
		script_echo "I: Clean build!"
		make clean 2>&1 | sed 's/^/     /'
		make mrproper 2>&1 | sed 's/^/     /'
		update_magisk
	fi

	build_kernel_full
else
	show_usage
fi
