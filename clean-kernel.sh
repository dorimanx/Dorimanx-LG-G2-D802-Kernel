#!/bin/bash

PYTHON_CHECK=$(ls -la /usr/bin/python | grep python3 | wc -l);
PYTHON_WAS_3=0;

if [ "$PYTHON_CHECK" -eq "1" ] && [ -e /usr/bin/python2 ]; then
	if [ -e /usr/bin/python2 ]; then
		rm /usr/bin/python
		ln -s /usr/bin/python2 /usr/bin/python
		echo "Switched to Python2 for cleaning kernel will switch back when done";
		PYTHON_WAS_3=1;
	else
		echo "You need Python2 to clean this kernel. install and come back."
		exit 1;
	fi;
else
	echo "Python2 is used! all good, cleaning!";
fi;

cp -pv .config .config.bkp;
make ARCH=arm mrproper;
make clean;
cp -pv .config.bkp .config;

if [ "$PYTHON_WAS_3" -eq "1" ]; then
	rm /usr/bin/python
	ln -s /usr/bin/python3 /usr/bin/python
fi;

# clean ccache
read -t 5 -p "clean ccache, 5sec timeout (y/n)?";
if [ "$REPLY" == "y" ]; then
        ccache -C;
fi;
