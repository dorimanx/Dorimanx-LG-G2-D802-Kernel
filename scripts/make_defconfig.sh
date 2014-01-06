#!/bin/bash
# A script to modify defconfig files
#
# Copyright (C) 2013 LG Electronics.
# Author(s): Jaeseong GIM <jaeseong.gim@lge.com>
#

DEFCONFIG_FILE=$1

function usage()
{
	echo "Usage : $ ./scripts/make_defconfig.sh [#defconfig file name]"
}

if [ -z "$DEFCONFIG_FILE" ]; then
	echo "Need defconfig file!"
	usage;
	exit -1
fi

if [ ! -e arch/arm/configs/$DEFCONFIG_FILE ]; then
	echo "No such file : arch/arm/configs/$DEFCONFIG_FILE"
	usage;
	exit -1
fi

# make .config
env KCONFIG_NOTIMESTAMP=true \
make ARCH=arm CROSS_COMPILE=arm-eabi- ${DEFCONFIG_FILE}

# run menuconfig
env KCONFIG_NOTIMESTAMP=true \
make menuconfig ARCH=arm

# run savedefconfig
make savedefconfig ARCH=arm

# copy .config to defconfig
mv defconfig arch/arm/configs/${DEFCONFIG_FILE}

# clean kernel object
make mrproper
