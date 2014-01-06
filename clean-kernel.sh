#!/bin/bash

if [ -e /usr/bin/python3 ]; then
	rm /usr/bin/python
	ln -s /usr/bin/python2.7 /usr/bin/python
fi;

cp -pv .config .config.bkp;
make ARCH=arm mrproper;
make clean;
cp -pv .config.bkp .config;

#resore python3
if [ -e /usr/bin/python3 ]; then
	rm /usr/bin/python
	ln -s /usr/bin/python3 /usr/bin/python
fi;

# clean ccache
read -t 5 -p "clean ccache, 5sec timeout (y/n)?";
if [ "$REPLY" == "y" ]; then
        ccache -C;
fi;
