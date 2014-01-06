#ifndef _UEI_IRRC_UART_H
#define _UEI_IRRC_UART_H

#define GPIO_IRRC_RESET_N 78
#define UEI_IRRC_NAME "uei_irrc"

struct uei_irrc_pdata_type {
    int reset_gpio;

#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	struct regulator *irrc_vdd_main;
	struct regulator *irrc_vdd_led;
#endif
};

#endif /* _UEI_IRRC_UART_H */
