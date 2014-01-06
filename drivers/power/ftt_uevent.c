#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include "ftt_ctrl_comm.h"
#include "ftt_status.h"


#if FTT_UEVENT
static enum power_supply_property ftt_charger_props[] = {
	POWER_SUPPLY_PROP_FTT_ANNTENA_LEVEL,
	POWER_SUPPLY_PROP_ONLINE,
};

static int ftt_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct ftt_charger_device *ftt_pdev = container_of(psy,
			struct ftt_charger_device, ftt_supply);

	switch (psp)
	{
		case POWER_SUPPLY_PROP_FTT_ANNTENA_LEVEL:
			val->intval = ftt_pdev->ant_level;
			break;
		case POWER_SUPPLY_PROP_ONLINE :
			val->intval = (int)ftt_pdev->ftt_online;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

void ftt_uevent_init(struct ftt_charger_device *ftt_pdev)
{
	ftt_pdev->ftt_supply.name		= FTT_POWER_SUPPLY;
	ftt_pdev->ftt_supply.type		= POWER_SUPPLY_TYPE_WIRELESS;
	ftt_pdev->ftt_supply.get_property	= ftt_get_property;
	ftt_pdev->ftt_supply.properties		= ftt_charger_props;
	ftt_pdev->ftt_supply.num_properties	= ARRAY_SIZE(ftt_charger_props);

	return;
}

#endif

