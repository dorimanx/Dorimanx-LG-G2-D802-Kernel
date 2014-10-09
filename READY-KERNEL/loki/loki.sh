#!/sbin/sh
#
# This leverages the loki_patch utility created by djrbliss
# See here for more information on loki: https://github.com/djrbliss/loki
#

# 4.4 Kernel - Panel Detection - by @dr87 / dr87@xda
#
# Detect panel and swap as necessary
# lcd_maker_id is determined by get_panel_maker_id on the hardware and is always accurate
# This searches directly in the boot.img and has no other requirements
# Do not shorten the search or you may change the actual kernel source
#
#	LCD_RENESAS_LGD = 0
#	LCD_RENESAS_JDI = 1
# Dorimanx Note:
# My kernel compiled for LGD panel by default! so it's will be changed to JDI panel only when needed.

# Search for boot.img in /tmp/loki

lcdmaker=$(grep -c "lcd_maker_id=1" /proc/cmdline)
if [ "$lcdmaker" -eq "1" ]; then
	echo "JDI panel detected";
	find /tmp/loki/boot.img -type f -exec sed -i 's/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_jdi_cmd/g' {} \;
	echo "JDI panel set in kernel config!";
else
	echo "LGD panel detected and set in config";
fi;

# Loki
dd if=/dev/block/platform/msm_sdcc.1/by-name/aboot of=/tmp/loki/aboot.img
chmod 644 /tmp/loki/aboot.img
/tmp/loki/loki_tool patch boot /tmp/loki/aboot.img /tmp/loki/boot.img /tmp/loki/boot.lok || exit 1
/tmp/loki/loki_tool flash boot /tmp/loki/boot.lok || exit 1
rm -rf /tmp/loki
exit 0
