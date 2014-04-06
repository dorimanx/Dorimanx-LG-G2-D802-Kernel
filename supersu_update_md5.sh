#!/bin/sh
md5sum READY-KERNEL/system/chainfire/SuperSU.apk | awk '{print $1}' > READY-KERNEL/system/chainfire/SuperSU.apk.md5;
echo "Done!";
cat READY-KERNEL/system/chainfire/SuperSU.apk.md5;
