/*
 * based on sysfs interface from:
 *	Chad Froebel <chadfroebel@gmail.com> &
 *	Jean-Pierre Rasquin <yank555.lu@gmail.com>
 * for backwards compatibility
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Possible values for "force_fast_charge" are :
 *
 *   0 - disabled (default)
 *   1 - substitute AC to USB unconditional
 *   2 - custom
*/

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fastchg.h>

/* Credits / Changelog:
 * version 1.0 Initial build by Paul Reioux
 * version 1.1 Added 1800ma limit to table by Dorimanx
 * version 1.2 Added Fake AC interface by Mankindtw@xda and Dorimanx 
 * (update 22/10/14 mod deleted, it's bugged and useless)
 * version 1.3 Misc fixes to force AC and allowed real 1800mA max.
 *
 * Next versions depend on code for LG G2 Device!!! (Dorimanx)
 * version 1.4 Added usage of custom mA value for max charging power,
 * Now we can use Intelli Thermal and get full power charge, this was controlled by
 * default ROM thermal engine, not any more, code will check if battery if not above 50c
 * and allow max charge!
 * version 1.5/6/7/8 trying to perfect fast charge auto on/off and auto tune based on connection type
 * and battery heat.
 * version 1.9 Added Auto fast charge on/off based on battery %, if above 95% then fast charge is OFF
 * when battery is below 95% and fast charge was ON by user before, then it's enabled again.
 * version 2.0 Guard with mutex all functions that use values from other code to prevent race and bug.
 * version 2.1 Corect Mutex guards in code for fastcharge.
 * version 2.2 allow to charge on 900ma lock.
 * version 2.3 added more checks to thermal mitigation functions and corrected code style.
 * removed updating charging scenario when no charger connected. no point to do so.
 * version 2.4 allowed full 2000ma to be set in charger driver.
 * version 2.5 fixed broken mitigation set if USB is connected.
 */

#define FAST_CHARGE_VERSION	"Version 2.5"

int force_fast_charge;
int force_fast_charge_temp;
int fast_charge_level;
int force_fast_charge_on_off;

/* sysfs interface for "force_fast_charge" */
static ssize_t force_fast_charge_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", force_fast_charge);
}

static ssize_t force_fast_charge_store(struct kobject *kobj,
			struct kobj_attribute *attr, const char *buf,
			size_t count)
{

	int new_force_fast_charge;

	sscanf(buf, "%du", &new_force_fast_charge);

	switch(new_force_fast_charge) {
		case FAST_CHARGE_DISABLED:
		case FAST_CHARGE_FORCE_AC:
		case FAST_CHARGE_FORCE_CUSTOM_MA:
			force_fast_charge = new_force_fast_charge;
			force_fast_charge_temp = new_force_fast_charge;
			force_fast_charge_on_off = new_force_fast_charge;
			return count;
		default:
			return -EINVAL;
	}
}

static ssize_t charge_level_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fast_charge_level);
}

static ssize_t charge_level_store(struct kobject *kobj,
			struct kobj_attribute *attr, const char *buf,
			size_t count)
{

	int new_charge_level;

	sscanf(buf, "%du", &new_charge_level);

	switch (new_charge_level) {
		case FAST_CHARGE_300:
		case FAST_CHARGE_500:
		case FAST_CHARGE_900:
		case FAST_CHARGE_1200:
		case FAST_CHARGE_1600:
		case FAST_CHARGE_1800:
		case FAST_CHARGE_2000:
			fast_charge_level = new_charge_level;
			return count;
		default:
			return -EINVAL;
	}
	return -EINVAL;
}

/* sysfs interface for "fast_charge_levels" */
static ssize_t available_charge_levels_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", FAST_CHARGE_LEVELS);
}

/* sysfs interface for "version" */
static ssize_t version_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", FAST_CHARGE_VERSION);
}

static struct kobj_attribute version_attribute =
	__ATTR(version, 0444, version_show, NULL);

static struct kobj_attribute available_charge_levels_attribute =
	__ATTR(available_charge_levels, 0444,
		available_charge_levels_show, NULL);

static struct kobj_attribute fast_charge_level_attribute =
	__ATTR(fast_charge_level, 0666,
		charge_level_show,
		charge_level_store);

static struct kobj_attribute force_fast_charge_attribute =
	__ATTR(force_fast_charge, 0666,
		force_fast_charge_show,
		force_fast_charge_store);

static struct attribute *force_fast_charge_attrs[] = {
	&force_fast_charge_attribute.attr,
	&fast_charge_level_attribute.attr,
	&available_charge_levels_attribute.attr,
	&version_attribute.attr,
	NULL,
};

static struct attribute_group force_fast_charge_attr_group = {
	.attrs = force_fast_charge_attrs,
};

/* Initialize fast charge sysfs folder */
static struct kobject *force_fast_charge_kobj;

int force_fast_charge_init(void)
{
	int force_fast_charge_retval;

	 /* Forced fast charge disabled by default */
	force_fast_charge = FAST_CHARGE_DISABLED;
	force_fast_charge_temp = FAST_CHARGE_DISABLED;
	force_fast_charge_on_off = FAST_CHARGE_DISABLED;
	fast_charge_level = FAST_CHARGE_1600;

	force_fast_charge_kobj
		= kobject_create_and_add("fast_charge", kernel_kobj);

	if (!force_fast_charge_kobj) {
		return -ENOMEM;
	}

	force_fast_charge_retval
		= sysfs_create_group(force_fast_charge_kobj,
				&force_fast_charge_attr_group);

	if (force_fast_charge_retval)
		kobject_put(force_fast_charge_kobj);

	if (force_fast_charge_retval)
		kobject_put(force_fast_charge_kobj);

	return (force_fast_charge_retval);
}

void force_fast_charge_exit(void)
{
	kobject_put(force_fast_charge_kobj);
}

module_init(force_fast_charge_init);
module_exit(force_fast_charge_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jean-Pierre Rasquin <yank555.lu@gmail.com>");
MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_AUTHOR("Yuri Sh. <yuri@bynet.co.il>");
MODULE_DESCRIPTION("Fast Charge Hack for Android");
