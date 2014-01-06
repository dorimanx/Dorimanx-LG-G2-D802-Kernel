Kernel Build  
  - Uncompress using following command at the android directory
        tar xvzf LG-D802(G2)_Android_JB_D802_10a_Kernel.tar.gz  
  - When you compile the kernel source code, you have to add google original prebuilt source(toolchain) into the android directory.
  - Run following scripts to build kernel
    a) cd kernel
	b) export PATH=$PATH:tools/lz4demo	
	c) make ARCH=arm CROSS_COMPILE=../prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi- g2-open_com-perf_defconfig zImage -j4
	
	* "-j4" : The number, 4, is the number of multiple jobs to be invoked simultaneously. 
	* lz4demo : More information can be found at "https://code.google.com/p/lz4/"
  - After build, you can find the build image(zImage) at arch/arm/boot/