#!/bin/bash
clear

# Initia script by @glewarne big thanks!

# What you need installed to compile
# gcc, gpp, cpp, c++, g++, lzma, lzop, ia32-libs

# What you need to make configuration easier by using xconfig
# qt4-dev, qmake-qt4, pkg-config

# Setting the toolchain
# the kernel/Makefile CROSS_COMPILE variable to match the download location of the
# bin/ folder of your toolchain
# toolchain already axist and set! in kernel git. android-toolchain/bin/

# Structure for building and using this script

#--project/				(progect container folder)
#------LG-G2-D802-Ramdisk/		(ramdisk files for boot.img)
#------dorimanx-LG-G2-D802-Kernel/	(kernel source goes here)
#--------READY-KERNEL/			(output directory, where the final boot.img and modules are placed)
#----------meta-inf/			(meta-inf folder for your flashable zip)
#----------system/

# location
KERNELDIR=`readlink -f .`;
export HOST=`uname -n`;

# begin by ensuring the required directory structure is complete, and empty
echo "Initialising................."
if [ -e ../LG-G2-D802-Ramdisk/lib/modules/ ]; then
	rm -rf ../LG-G2-D802-Ramdisk/lib/
fi;
rm -rf $KERNELDIR/READY-KERNEL/boot
rm -f $KERNELDIR/READY-KERNEL/*.zip
rm -f $KERNELDIR/READY-KERNEL/*.img
rm -f $KERNELDIR/READY-KERNEL/system/lib/modules/*.ko
mkdir -p $KERNELDIR/READY-KERNEL/boot
mkdir -p ../LG-G2-D802-Ramdisk/lib/modules/

#force regeneration of .dtb and zImage files for every compile
rm -f arch/arm/boot/*.dtb
rm -f arch/arm/boot/*.cmd
rm -f arch/arm/boot/zImage
rm -f arch/arm/boot/Image

export PATH=$PATH:tools/lz4demo
export KERNEL_CONFIG=dorimanx_defconfig

if [ -e /usr/bin/python3 ]; then
	rm /usr/bin/python
	ln -s /usr/bin/python2.7 /usr/bin/python
fi;

# move into the kernel directory and compile the main image
echo "Compiling Kernel............."
if [ ! -f $KERNELDIR/.config ]; then
	sh load_config.sh
fi;

# get version from config
GETVER=`grep 'Kernel-.*-V' .config |sed 's/Kernel-//g' | sed 's/.*".//g' | sed 's/-L.*//g'`;

cp $KERNELDIR/.config $KERNELDIR/arch/arm/configs/dorimanx_defconfig;

# remove all old modules before compile
for i in `find $KERNELDIR/ -name "*.ko"`; do
        rm -f $i;
done;

# dorimanx detection ;)
if [ $HOST == "dorimanx-virtual-machine" ] || [ $HOST == "dorimanx" ]; then
	NR_CPUS=16;
	echo "Dori power detected!";
else
	NR_CPUS=4
        echo "not dorimanx system detected, setting $NR_CPUS build threads"
fi;

# build zImage
time make -j ${NR_CPUS}

cp $KERNELDIR/.config $KERNELDIR/arch/arm/configs/dorimanx_defconfig;

stat $KERNELDIR/arch/arm/boot/zImage || exit 1;

# compile the modules, and depmod to create the final zImage
echo "Compiling Modules............"
time make modules -j ${NR_CPUS} || exit 1

# move the compiled zImage and modules into the READY-KERNEL working directory
echo "Move compiled objects........"

for i in `find $KERNELDIR -name '*.ko'`; do
	cp -av $i $KERNELDIR/READY-KERNEL/system/lib/modules/
done;

#for i in `find $KERNELDIR -name '*.ko'`; do
#        cp -av $i ../LG-G2-D802-Ramdisk/lib/modules/
#done;

chmod 755 $KERNELDIR/READY-KERNEL/system/lib/modules/*

if [ -e $KERNELDIR/arch/arm/boot/zImage ]; then

	cp arch/arm/boot/zImage READY-KERNEL/boot

	# create the ramdisk and move it to the output working directory
	echo "Create ramdisk..............."
	scripts/mkbootfs ../LG-G2-D802-Ramdisk | gzip > ramdisk.gz 2>/dev/null
	mv ramdisk.gz READY-KERNEL/boot

	# clean modules from ramdisk.
	#rm -rf ../LG-G2-D802-Ramdisk/lib/

	# create the dt.img from the compiled device files, necessary for msm8974 boot images
	echo "Create dt.img................"
	./scripts/dtbTool -v -s 2048 -o READY-KERNEL/boot/dt.img arch/arm/boot/

	if [ -e /usr/bin/python3 ]; then
		rm /usr/bin/python
		ln -s /usr/bin/python3 /usr/bin/python
	fi;

	# build the final boot.img ready for inclusion in flashable zip
	echo "Build boot.img..............."
	cp scripts/mkbootimg READY-KERNEL/boot
	cd READY-KERNEL/boot
	base=0x00000000
	offset=0x05000000
	tags_addr=0x04800000
	cmd_line="console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0"
	./mkbootimg --kernel zImage --ramdisk ramdisk.gz --cmdline "$cmd_line" --base $base --offset $offset --tags-addr $tags_addr --pagesize 2048 --dt dt.img -o newboot.img
	mv newboot.img ../boot.img

	# cleanup all temporary working files
	echo "Post build cleanup..........."
	cd ..
	rm -rf boot

	# create the flashable zip file from the contents of the output directory
	echo "Make flashable zip..........."
	zip -r Kernel-${GETVER}-`date +"[%H-%M]-[%d-%m]-LG-D802-PWR-CORE"`.zip * >/dev/null
	stat boot.img
	rm -f *.img
	cd ..
else
	if [ -e /usr/bin/python3 ]; then
		rm /usr/bin/python
		ln -s /usr/bin/python3 /usr/bin/python
	fi;

	# with red-color
	 echo -e "\e[1;31mKernel STUCK in BUILD! no zImage exist\e[m"
fi;

