#!/bin/sh
PYTHON_CHECK=$(ls -la /usr/bin/python2 | wc -l);
BOOT_IMAGE_LOCATION=READY-KERNEL/boot.img;

if [ "$PYTHON_CHECK" -eq "1" ]; then

	/usr/bin/python2 open_bump.py $BOOT_IMAGE_LOCATION;
else
	echo "you dont have PYTHON2.x script will not work!!!";
	exit 1;
fi;
