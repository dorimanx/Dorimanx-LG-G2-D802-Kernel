#include <linux/gpio.h>
#include <linux/platform_device.h>

#include "ftt_status.h"
#include "ftt_charger_v3.h"

/*
 * ****************************************************
 * bool wireless_online(void)
 * 
 * ****************************************************
 */
bool wireless_online(int gpio)
{
	return !gpio_get_value(gpio);
/* return !gpio_get_value(FTT_DETECT); */
	return 1;
}

/*
 * ****************************************************
 * bool on_change_level(int level)
 * 
 * 
 * ****************************************************
 */
bool on_change_level(int level)
{
	/* Todo */

	return true;
}

/*
 * ****************************************************
 * int get_ftt_gpio(void)
 * 
 * 
 * ****************************************************
 */
int get_ftt_gpio(int gpio)
{
	return gpio;
/* return FTT_FREQUANCY;*/
}

static struct ftt_charger_pdata ftt_charger_data = {
	.on_change_level_fn = on_change_level,
	.online_fn	= wireless_online,
	.get_ftt_gpio = get_ftt_gpio,
};

struct platform_device ftt_charger_device = {
	.name		= DEVICE_NAME,
	.id		= -1,
	.dev	= {
		.platform_data = &ftt_charger_data,
	},
};


