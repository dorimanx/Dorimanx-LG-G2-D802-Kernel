#!/sbin/sh

# 4.4 Kernel - Panel Detection - dr87 (Recovery Version)
#
# Detect panel and swap as necessary 
# lcd_maker_id is determined by get_panel_maker_id on the hardware and is always accurate
# This searches directly in the recovery.img and has no other requirements
# Do not shorten the search or you may change the actual kernel source
#
#	LCD_RENESAS_LGD = 0,
#	LCD_RENESAS_JDI = 1

# Search for recovery.img in /tmp/panelfix

lcdmaker=$(grep -c "lcd_maker_id=1" /proc/cmdline)
if [ $lcdmaker == 1 ]; then
	echo "JDI panel detected"
	find /tmp/panelfix/recovery.img -type f -exec sed -i 's/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_jdi_cmd/g' {} \;
elif [ $lcdmaker == "0" ]; then
	echo "LGD panel detected"
	find /tmp/panelfix/recovery.img -type f -exec sed -i 's/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_jdi_cmd/console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd/g' {} \;
else
	echo "lcd_maker_id doesn't exist. Something went wrong."
fi

# Install BUMPED Recovery
dd if=/tmp/panelfix/recovery.img of=/dev/block/platform/msm_sdcc.1/by-name/recovery || exit 1
exit 0
