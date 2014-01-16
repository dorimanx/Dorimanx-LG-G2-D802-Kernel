#!/sbin/sh
#
# This leverages the loki_patch utility created by djrbliss
# See here for more information on loki: https://github.com/djrbliss/loki
#

dd if=/dev/block/platform/msm_sdcc.1/by-name/aboot of=/tmp/loki/aboot.img
/tmp/loki/loki_patch boot /tmp/loki/aboot.img /tmp/loki/boot.img /tmp/loki/boot.lok || exit 1
/tmp/loki/loki_flash boot /tmp/loki/boot.lok || exit 1
rm -rf /tmp/loki
exit 0
