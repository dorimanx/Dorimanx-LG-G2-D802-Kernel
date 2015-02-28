#!/bin/bash
clear

# Initia script by @glewarne big thanks!

# What you need installed to compile
# gcc, gpp, cpp, c++, g++, lzma, lzop, ia32-libs flex

# What you need to make configuration easier by using xconfig
# qt4-dev, qmake-qt4, pkg-config

# Setting the toolchain
# the kernel/Makefile CROSS_COMPILE variable to match the download location of the
# bin/ folder of your toolchain
# toolchain already axist and set! in kernel git. android-toolchain/bin/

# Structure for building and using this script

#--project/				(progect container folder)
#------LG-G2-D802-Ramdisk/		(ramdisk files for boot.img)
#------ramdisk-tmp/			(ramdisk tmp store without .git)
#--------lib/modules/			(modules dir, will be added to system on boot)
#------dorimanx-LG-G2-D802-Kernel/	(kernel source goes here)
#--------READY-RELEASES/		(When using all selector, all models ready kernels will go to this folder)
#--------READY-KERNEL/			(output directory, where the final boot.img is placed)
#----------meta-inf/			(meta-inf folder for your flashable zip)
#----------system/

# location
KERNELDIR=$(readlink -f .);
export PATH=$PATH:tools/lz4demo

CLEANUP()
{
	# begin by ensuring the required directory structure is complete, and empty
	echo "Initialising................."
	rm -rf "$KERNELDIR"/READY-KERNEL/boot
	rm -f "$KERNELDIR"/READY-KERNEL/*.zip
	rm -f "$KERNELDIR"/READY-KERNEL/*.img
	mkdir -p "$KERNELDIR"/READY-KERNEL/boot

	if [ -d ../ramdisk-tmp ]; then
		rm -rf ../ramdisk-tmp/*
	else
		mkdir ../ramdisk-tmp
		chown root:root ../ramdisk-tmp
		chmod 777 ../ramdisk-tmp
	fi;

	# force regeneration of .dtb and zImage files for every compile
	rm -f arch/arm/boot/*.dtb
	rm -f arch/arm/boot/*.cmd
	rm -f arch/arm/boot/zImage
	rm -f arch/arm/boot/Image

	BUILD_800=0
	BUILD_801=0
	BUILD_802=0
	BUILD_803=0
	BUILD_320=0
	BUILD_LS_980=0
	BUILD_VS_980=0
	BUILD_L01F=0
}
CLEANUP;

BUILD_NOW()
{
	PYTHON_CHECK=$(ls -la /usr/bin/python | grep python3 | wc -l);
	PYTHON_WAS_3=0;

	if [ "$PYTHON_CHECK" -eq "1" ] && [ -e /usr/bin/python2 ]; then
		if [ -e /usr/bin/python2 ]; then
			rm /usr/bin/python
			ln -s /usr/bin/python2 /usr/bin/python
			echo "Switched to Python2 for building kernel will switch back when done";
			PYTHON_WAS_3=1;
		else
			echo "You need Python2 to build this kernel. install and come back."
			exit 1;
		fi;
	else
		echo "Python2 is used! all good, building!";
	fi;

	# move into the kernel directory and compile the main image
	echo "Compiling Kernel.............";
	if [ ! -f "$KERNELDIR"/.config ]; then
		if [ "$BUILD_800" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d800_defconfig .config
		elif [ "$BUILD_801" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d801_defconfig .config
		elif [ "$BUILD_802" -eq "1" ]; then
			sh load_config-802.sh
		elif [ "$BUILD_803" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d803_defconfig .config
		elif [ "$BUILD_320" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_f320_defconfig .config
		elif [ "$BUILD_LS_980" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_ls980_defconfig .config
		elif [ "$BUILD_VS_980" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_vs980_defconfig .config
		elif [ "$BUILD_L01F" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_l01f_defconfig .config
		fi;
	fi;

	if [ -f "$KERNELDIR"/.config ]; then
		BRANCH_800=$(grep -R "CONFIG_MACH_MSM8974_G2_ATT=y" .config | wc -l)
		BRANCH_801=$(grep -R "CONFIG_MACH_MSM8974_G2_TMO_US=y" .config | wc -l)
		BRANCH_802=$(grep -R "CONFIG_MACH_MSM8974_G2_OPEN_COM=y" .config | wc -l)
		BRANCH_803=$(grep -R "CONFIG_MACH_MSM8974_G2_CA=y" .config | wc -l)
		BRANCH_320=$(grep -R "CONFIG_MACH_MSM8974_G2_KR=y" .config | wc -l)
		BRANCH_LS_980=$(grep -R "CONFIG_MACH_MSM8974_G2_SPR=y" .config | wc -l)
		BRANCH_VS_980=$(grep -R "CONFIG_MACH_MSM8974_G2_VZW=y" .config | wc -l)
		BRANCH_L01F=$(grep -R "CONFIG_MACH_MSM8974_G2_DCM=y" .config | wc -l)
		if [ "$BRANCH_800" -eq "0" ] && [ "$BUILD_800" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d800_defconfig ./.config
		fi;
		if [ "$BRANCH_801" -eq "0" ] && [ "$BUILD_801" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d801_defconfig ./.config
		fi;
		if [ "$BRANCH_802" -eq "0" ] && [ "$BUILD_802" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d802_defconfig ./.config
		fi;
		if [ "$BRANCH_803" -eq "0" ] && [ "$BUILD_803" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_d803_defconfig ./.config
		fi;
		if [ "$BRANCH_320" -eq "0" ] && [ "$BUILD_320" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_f320_defconfig ./.config
		fi;
		if [ "$BRANCH_LS_980" -eq "0" ] && [ "$BUILD_LS_980" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_ls980_defconfig ./.config
		fi;
		if [ "$BRANCH_VS_980" -eq "0" ] && [ "$BUILD_VS_980" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_vs980_defconfig ./.config
		fi;
		if [ "$BRANCH_L01F" -eq "0" ] && ["$BUILD_L01F" -eq "1" ]; then
			cp arch/arm/configs/dorimanx_l01f_defconfig ./.config
		fi;
	fi;

	# get version from config
	GETVER=$(grep 'Kernel-.*-V' .config |sed 's/Kernel-//g' | sed 's/.*".//g' | sed 's/-L.*//g');
	GETBRANCH=$(grep '.*-LG' .config |sed 's/Kernel-Dorimanx-V//g' | sed 's/[1-9].*-LG-//g' | sed 's/.*".//g' | sed 's/-PWR.*//g');

	cp "$KERNELDIR"/.config "$KERNELDIR"/arch/arm/configs/"$KERNEL_CONFIG_FILE";

	# remove all old modules before compile
	for i in $(find "$KERNELDIR"/ -name "*.ko"); do
		rm -f "$i";
	done;

	# Copy needed dtc binary to system to finish the build.
	if [ ! -e /bin/dtc ]; then
		cp -a tools/dtc-binary/dtc /bin/;
	fi;

	# Idea by savoca
	NR_CPUS=$(grep -c ^processor /proc/cpuinfo)

	if [ "$NR_CPUS" -le "2" ]; then
		NR_CPUS=4;
		echo "Building kernel with 4 CPU threads";
	else
		echo "Building kernel with $NR_CPUS CPU threads";
	fi;

	# build zImage
	time make -j ${NR_CPUS}

	cp "$KERNELDIR"/.config "$KERNELDIR"/arch/arm/configs/"$KERNEL_CONFIG_FILE";

	stat "$KERNELDIR"/arch/arm/boot/zImage || exit 1;

	# compile the modules, and depmod to create the final zImage
	echo "Compiling Modules............"
	time make modules -j ${NR_CPUS} || exit 1

	# move the compiled zImage and modules into the READY-KERNEL working directory
	echo "Move compiled objects........"

	# Check that RAMDISK is for this Branch. and check if need to push changes before switch to needed branch.

	RAMDISK_BRANCH=$(git -C ../LG-G2-D802-Ramdisk/ commit | grep "origin/kitkat-ramdisk" | wc -l);
	RAMDISK_NOT_SAVED=$(git -C ../LG-G2-D802-Ramdisk/ commit | grep "Changes not staged for commit" | wc -l);

	if [ "$RAMDISK_BRANCH" == "1" ]; then
		echo "Ram Disk is in the right branch!";
	else
		if [ "$RAMDISK_NOT_SAVED" != "1" ]; then
			git -C ../LG-G2-D802-Ramdisk/ checkout kitkat-ramdisk;
			echo -e "\e[1;31mRamDisk Switched to kitkat-ramdisk Branch!\e[m"
		else
			echo -e "\e[1;31mRamDisk has to be SAVED (commited) before switching to kitkat-ramdisk Branch\e[m";
			echo -e "\e[1;31mKernel Build Script exit! please (commit/Reset changes) and build again.\e[m";
			exit 1;
		fi;
	fi;

	# copy all ROOT ramdisk files to ramdisk temp dir.
	cp -a ../LG-G2-D802-Ramdisk/ROOT-RAMDISK/* ../ramdisk-tmp/

	# copy needed branch files to ramdisk temp dir.
	if [ "$BUILD_800" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/D800-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_801" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/D801-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_802" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/D802-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_803" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/D803-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_320" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/F320-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_LS_980" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/LS980-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_VS_980" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/VS980-RAMDISK/* ../ramdisk-tmp/
	elif [ "$BUILD_L01F" == "1" ]; then
		cp -a ../LG-G2-D802-Ramdisk/L01F-RAMDISK/* ../ramdisk-tmp/
	fi;

	for i in $(find "$KERNELDIR" -name '*.ko'); do
		cp -av "$i" ../ramdisk-tmp/lib/modules/;
	done;

	chmod 755 ../ramdisk-tmp/lib/modules/*

	# remove empty directory placeholders from tmp-initramfs
	for i in $(find ../ramdisk-tmp/ -name EMPTY_DIRECTORY); do
		rm -f "$i";
	done;

	if [ -e "$KERNELDIR"/arch/arm/boot/zImage ]; then
		cp arch/arm/boot/zImage READY-KERNEL/boot

		# strip not needed debugs from modules.
		android-toolchain/bin/arm-LG-linux-gnueabi-strip --strip-unneeded ../ramdisk-tmp/lib/modules/* 2>/dev/null
		android-toolchain/bin/arm-LG-linux-gnueabi-strip --strip-debug ../ramdisk-tmp/lib/modules/* 2>/dev/null

		# create the ramdisk and move it to the output working directory
		echo "Create ramdisk..............."
		scripts/mkbootfs ../ramdisk-tmp | gzip > ramdisk.gz 2>/dev/null
		mv ramdisk.gz READY-KERNEL/boot

		# create the dt.img from the compiled device files, necessary for msm8974 boot images
		echo "Create dt.img................"
		./scripts/dtbTool -v -s 2048 -o READY-KERNEL/boot/dt.img arch/arm/boot/

		if [ "$PYTHON_WAS_3" -eq "1" ]; then
			rm /usr/bin/python
			ln -s /usr/bin/python3 /usr/bin/python
		fi;

		# add kernel config to kernle zip for other devs
		cp "$KERNELDIR"/.config READY-KERNEL/

		# build the final boot.img ready for inclusion in flashable zip
		echo "Build boot.img..............."
		cp scripts/mkbootimg READY-KERNEL/boot
		cd READY-KERNEL/boot
		base=0x00000000
		offset=0x05000000
		tags_addr=0x04800000
		cmd_line="console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd"
		./mkbootimg --kernel zImage --ramdisk ramdisk.gz --cmdline "$cmd_line" --base $base --offset $offset --tags-addr $tags_addr --pagesize 2048 --dt dt.img -o newboot.img
		mv newboot.img ../boot.img

		# cleanup all temporary working files
		echo "Post build cleanup..........."
		cd ..
		rm -rf boot

		# BUMP boot.img with magic key to install on JB/KK bootloader
		cd ..
		sh kernel_bump.sh
		mv READY-KERNEL/boot_bumped.img READY-KERNEL/boot.img
		echo "Kernel BUMP done!";
		cd READY-KERNEL/

		# create the flashable zip file from the contents of the output directory
		echo "Make flashable zip..........."
		zip -r Kernel-"${GETVER}"-KK-"$(date +"[%H-%M]-[%d-%m]-LG-${GETBRANCH}-PWR-CORE")".zip * >/dev/null
		stat boot.img
		rm -f ./*.img
		cd ..
	else
		if [ "$PYTHON_WAS_3" -eq "1" ]; then
			rm /usr/bin/python
			ln -s /usr/bin/python3 /usr/bin/python
		fi;

		# with red-color
		echo -e "\e[1;31mKernel STUCK in BUILD! no zImage exist\e[m"
	fi;
}

CLEAN_KERNEL()
{
	PYTHON_CHECK=$(ls -la /usr/bin/python | grep python3 | wc -l);
	CLEAN_PYTHON_WAS_3=0;

	if [ "$PYTHON_CHECK" -eq "1" ] && [ -e /usr/bin/python2 ]; then
		if [ -e /usr/bin/python2 ]; then
			rm /usr/bin/python
			ln -s /usr/bin/python2 /usr/bin/python
			echo "Switched to Python2 for building kernel will switch back when done";
			CLEAN_PYTHON_WAS_3=1;
		else
			echo "You need Python2 to build this kernel. install and come back."
			exit 1;
		fi;
	else
		echo "Python2 is used! all good, building!";
	fi;

	cp -pv .config .config.bkp;
	make ARCH=arm mrproper;
	make clean;
	cp -pv .config.bkp .config;

	if [ "$CLEAN_PYTHON_WAS_3" -eq "1" ]; then
		rm /usr/bin/python
		ln -s /usr/bin/python3 /usr/bin/python
	fi;
}

echo "What to cook for you?!";
select CHOICE in D800 D801 D802 D803 F320 LS980 VS980 L01F ALL; do
	case "$CHOICE" in
		"D800")
			export KERNEL_CONFIG=dorimanx_d800_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d800_defconfig
			BUILD_800=1;
			BUILD_NOW;
			break;;
		"D801")
			export KERNEL_CONFIG=dorimanx_d801_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d801_defconfig
			BUILD_801=1;
			BUILD_NOW;
			break;;
		"D802")
			export KERNEL_CONFIG=dorimanx_d802_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d802_defconfig
			BUILD_802=1;
			BUILD_NOW;
			break;;
		"D803")
			export KERNEL_CONFIG=dorimanx_d803_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d803_defconfig
			BUILD_803=1
			BUILD_NOW;
			break;;
		"F320")
			export KERNEL_CONFIG=dorimanx_f320_defconfig
			KERNEL_CONFIG_FILE=dorimanx_f320_defconfig
			BUILD_320=1;
			BUILD_NOW;
			break;;
		"LS980")
			export KERNEL_CONFIG=dorimanx_ls980_defconfig
			KERNEL_CONFIG_FILE=dorimanx_ls980_defconfig
			BUILD_LS_980=1;
			BUILD_NOW;
			break;;
		"VS980")
			export KERNEL_CONFIG=dorimanx_vs980_defconfig
			KERNEL_CONFIG_FILE=dorimanx_vs980_defconfig
			BUILD_VS_980=1;
			BUILD_NOW;
			break;;
		"L01F")
			export KERNEL_CONFIG=dorimanx_l01f_defconfig
			KERNEL_CONFIG_FILE=dorimanx_l01f_defconfig
			BUILD_L01F=1;
			BUILD_NOW;
			break;;
		"ALL")
			CLEAN_KERNEL;
			echo "starting build of D800 in 3"
			sleep 1;
			echo "starting build of D800 in 2"
			sleep 1;
			echo "starting build of D800 in 1"
			sleep 1;
			CLEANUP;
			cp arch/arm/configs/dorimanx_d800_defconfig ./.config 
			export KERNEL_CONFIG=dorimanx_d800_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d800_defconfig
			BUILD_800=1;
			BUILD_NOW;
			echo "D800 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of D801 in 3"
			sleep 1;
			echo "starting build of D801 in 2"
			sleep 1;
			echo "starting build of D801 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_d801_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d801_defconfig
			BUILD_801=1;
			BUILD_NOW;
			echo "D801 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of D802 in 3"
			sleep 1;
			echo "starting build of D802 in 2"
			sleep 1;
			echo "starting build of D802 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_d802_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d802_defconfig
			BUILD_802=1;
			BUILD_NOW;
			echo "D802 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of D803 in 3"
			sleep 1;
			echo "starting build of D803 in 2"
			sleep 1;
			echo "starting build of D803 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_d803_defconfig
			KERNEL_CONFIG_FILE=dorimanx_d803_defconfig
			BUILD_803=1;
			BUILD_NOW;
			echo "D803 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of F320 in 3"
			sleep 1;
			echo "starting build of F320 in 2"
			sleep 1;
			echo "starting build of F320 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_f320_defconfig
			KERNEL_CONFIG_FILE=dorimanx_f320_defconfig
			BUILD_320=1;
			BUILD_NOW;
			echo "F320 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of LS980 in 3"
			sleep 1;
			echo "starting build of LS980 in 2"
			sleep 1;
			echo "starting build of LS980 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_ls980_defconfig
			KERNEL_CONFIG_FILE=dorimanx_ls980_defconfig
			BUILD_LS_980=1;
			BUILD_NOW;
			echo "LS980 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			echo "starting build of VS980 in 3"
			sleep 1;
			echo "starting build of VS980 in 2"
			sleep 1;
			echo "starting build of VS980 in 1"
			sleep 1;
			CLEANUP;
			export KERNEL_CONFIG=dorimanx_vs980_defconfig
			KERNEL_CONFIG_FILE=dorimanx_vs980_defconfig
			BUILD_VS_980=1;
			BUILD_NOW;
			echo "VS980 is ready!"
			cp READY-KERNEL/*.zip READY-RELEASES/;
			break;;
	esac;
done;
