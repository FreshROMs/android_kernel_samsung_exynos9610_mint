#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

ver="$(cat "$DIR/magisk_version" 2>/dev/null || echo -n 'none')"

if [ "x$1" = "xcanary" ]
then
	nver="canary"
	magisk_link="https://cdn.jsdelivr.net/gh/huskydg/magisk-files@301b1865c7d47bbed1e375541987aee0cd1b753d/app-release.apk"
elif [ "x$1" = "xlocal" ]
then
	nver="local"
	magisk_link="https://cdn.jsdelivr.net/gh/huskydg/magisk-files@301b1865c7d47bbed1e375541987aee0cd1b753d/app-release.apk"
else
	if [ "x$1" = "x" ]; then
		nver="$(curl -s https://raw.githubusercontent.com/topjohnwu/magisk-files/master/stable.json | jq '.magisk.version' | cut -d '"' -f 2)"
	else
		nver="$1"
	fi
	magisk_link="https://cdn.jsdelivr.net/gh/huskydg/magisk-files@301b1865c7d47bbed1e375541987aee0cd1b753d/app-release.apk"
fi

if [ \( -n "$nver" \) -a \( "$nver" != "$ver" \) -o ! \( -f "$DIR/arm/magiskinit64" \) -o \( "$nver" = "canary" \) -o \( "$nver" = "local" \) ]
then
	echo "Updating Magisk from $ver to $nver"
	curl -s --output "$DIR/magisk.zip" -L "$magisk_link"
	if fgrep 'Not Found' "$DIR/magisk.zip"; then
		curl -s --output "$DIR/magisk.zip" -L "${magisk_link%.apk}.zip"
	fi

	7z e "$DIR/magisk.zip" lib/arm64-v8a/libmagiskinit.so lib/armeabi-v7a/libmagisk32.so lib/arm64-v8a/libmagisk64.so -o"$DIR" -y
	mv -f "$DIR/libmagiskinit.so" "$DIR/magiskinit"
	mv -f "$DIR/libmagisk32.so" "$DIR/magisk32"
	mv -f "$DIR/libmagisk64.so" "$DIR/magisk64"
	xz --force --check=crc32 "$DIR/magisk32" "$DIR/magisk64"

	echo -n "$nver" > "$DIR/magisk_version"
	rm "$DIR/magisk.zip"
	touch "$DIR/initramfs_list"
else
	echo "Nothing to be done: Magisk version $nver"
fi
