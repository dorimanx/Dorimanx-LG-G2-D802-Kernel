obj-$(CONFIG_MACH_MSM8974_LGPS11) += board-8974-lgps11.o board-8974-lgps11-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2EVB) += board-8974-g2-evb.o board-8974-g2-evb-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_KR) += board-8974-g2-kr.o board-8974-g2-kr-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_ATT) += board-8974-g2-us.o board-8974-g2-att-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_VZW) += board-8974-g2-us.o board-8974-g2-vzw-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_SPR) += board-8974-g2-us.o board-8974-g2-spr-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_TMO_US) += board-8974-g2-us.o board-8974-g2-tmo_us-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_DCM) += board-8974-g2-dcm.o board-8974-g2-dcm-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_KDDI) += board-8974-g2-kddi.o board-8974-g2-kddi-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_OPEN_COM) += board-8974-g2-com.o board-8974-g2-open_com-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_OPEN_AME) += board-8974-g2-open_ame.o board-8974-g2-open_ame-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_OPT_AU) += board-8974-g2-au.o board-8974-g2-opt_au-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_TEL_AU) += board-8974-g2-tel_au.o board-8974-g2-tel_au-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_G2_CA) += board-8974-g2-ca.o board-8974-g2-ca-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_Z_KR) += board-8974-z-kr.o board-8974-z-kr-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_Z_SPR) += board-8974-z-us-spr.o board-8974-z-us-spr-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_Z_TMO_US) += board-8974-z-us-spr.o board-8974-z-us-spr-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_Z_KDDI) += board-8974-z-kddi.o board-8974-z-kddi-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_Z_ATT_US) += board-8974-z-us-att.o board-8974-z-us-att-gpiomux.o
obj-$(CONFIG_MACH_MSM8974_VU3_KR) += board-8974-vu3-kr.o board-8974-vu3-kr-gpiomux.o
ifneq ($(CONFIG_BCM4334),)
obj-$(CONFIG_WIFI_CONTROL_FUNC) += wifi_bcm4334_lge.o
else
obj-$(CONFIG_WIFI_CONTROL_FUNC) += wifi_bcm_lge.o
endif

obj-$(CONFIG_MACH_LGE) += devices_lge.o
obj-$(CONFIG_MACH_LGE) += lge_charging_scenario.o
obj-$(CONFIG_HAS_EARLYSUSPEND) += lge_suspend_autotest.o
obj-$(CONFIG_MACH_LGE) += lge_blocking_monitor.o
obj-$(CONFIG_LGE_BOOTLOADER_LOG) += lge_bootloader_log.o
obj-$(CONFIG_LGE_HANDLE_PANIC) += lge_handle_panic.o
obj-$(CONFIG_LGE_BOOTLOADER_TIME_CHECKER) += lge_bootloader_time_checker.o
obj-$(CONFIG_LGE_ECO_MODE) += lge_kernel_driver.o
obj-$(CONFIG_LGE_BLUETOOTH) += board-8974-g2-rfkill.o
obj-$(CONFIG_LGE_QFPROM_INTERFACE) += lge_qfprom_access.o
obj-$(CONFIG_LGE_PM) += lge_gpio_debug.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_VU3EVB) +=board-vu3evb-input.o
obj-$(CONFIG_MACH_LGE) += lge_kcal_ctrl.o

CFLAGS_devices_lge.o += -Idrivers/staging/android
