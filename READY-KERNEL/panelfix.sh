#!/sbin/sh

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

# Search for boot.img in /tmp/

lcdmaker=$(grep -c "lcd_maker_id=1" /proc/cmdline)
if [ "$lcdmaker" -eq "1" ]; then
	echo "JDI panel detected";
	find /tmp/boot.img -type f -exec sed -i 's/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_jdi_cmd/g' {} \;
	echo "JDI panel set in kernel config!";
else
	echo "LGD panel detected and set in config";
fi;


if [ -e /system/app/STweaks.apk ]; then
	rm /system/app/STweaks.apk;
fi;
if [ -e /system/priv-app/STweaks.apk ]; then
	rm /system/priv-app/STweaks.apk;
fi;
if [ -e /system/app/re.codefi.savoca.kcal-v1.1.apk ]; then
	rm /system/app/re.codefi.savoca.kcal-v1.1.apk;
fi;

dd if=/tmp/boot.img of=/dev/block/platform/msm_sdcc.1/by-name/boot || exit 1
exit 0
