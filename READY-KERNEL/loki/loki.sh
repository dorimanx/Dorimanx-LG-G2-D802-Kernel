#!/sbin/sh
#
# This leverages the loki_patch utility created by djrbliss
# See here for more information on loki: https://github.com/djrbliss/loki
#

dd if=/dev/block/platform/msm_sdcc.1/by-name/aboot of=/tmp/loki/aboot.img
chmod 644 /tmp/loki/aboot.img
/tmp/loki/loki_tool patch boot /tmp/loki/aboot.img /tmp/loki/boot.img /tmp/loki/boot.lok || exit 1
/tmp/loki/loki_tool flash boot /tmp/loki/boot.lok || exit 1
rm -rf /tmp/loki
exit 0
