#include <linux/platform_device.h>

#include "ftt_ctrl_comm.h"
#include "ftt_status.h"
#include "ftt_charger_v3.h"

#ifdef CONFIG_FTT_SYSFS

extern const char *get_ftt_saved_pad_type_str(struct ftt_charger_device *ftt_pdev);
extern bool is_ftt_charging(struct ftt_charger_device *ftt_pdev);
extern enum ftt_charger_status get_ftt_charger_current_status(struct ftt_charger_device *ftt_pdev);

static	ssize_t show_ftt_total	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_total, FTT_SYS_PERMISSION, show_ftt_total, NULL);
static	ssize_t show_ftt_frequency	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_frequency, FTT_SYS_PERMISSION, show_ftt_frequency, NULL);
static	ssize_t show_ftt_is_charging	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_is_charging, FTT_SYS_PERMISSION, show_ftt_is_charging, NULL);
static	ssize_t show_ftt_ant_level	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_ant_level, FTT_SYS_PERMISSION, show_ftt_ant_level, NULL);
static	ssize_t show_ftt_pad_type	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_pad_type, FTT_SYS_PERMISSION, show_ftt_pad_type, NULL);
static	ssize_t show_ftt_status(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_status, FTT_SYS_PERMISSION, show_ftt_status, NULL);
static	ssize_t show_ftt_ver(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_ver, FTT_SYS_PERMISSION, show_ftt_ver, NULL);
static	ssize_t show_ftt_version(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_version, FTT_SYS_PERMISSION, show_ftt_version, NULL);

static struct attribute *ftt_sysfs_entries[] = {
		&dev_attr_ftt_total.attr,
		&dev_attr_ftt_frequency.attr,
		&dev_attr_ftt_is_charging.attr,
		&dev_attr_ftt_ant_level.attr,
		&dev_attr_ftt_pad_type.attr,
		&dev_attr_ftt_status.attr,
		&dev_attr_ftt_ver.attr,
		&dev_attr_ftt_version.attr,
		NULL
};

struct attribute_group ftt_sysfs_attr_group = {
	.name   = NULL,
	.attrs  = ftt_sysfs_entries,
};


static	ssize_t show_ftt_total	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);
	char *ppad_type;

	ppad_type = (char *)get_ftt_saved_pad_type_str(ftt_pdev);
	if (ppad_type == NULL)
		ppad_type = "";

	return sprintf(buf,
			"PAD TYPE : %s\n"
			"PAD NUMBER : %u\n"
			"FTT Frequency : %uHz\n"
			"LEVEL : %d\n"
			, ppad_type
			,ftt_pdev->detect_pad
			,ftt_pdev->ftt_frequency
			,ftt_pdev->ant_level
			);
}


static 	ssize_t show_ftt_frequency		(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", ftt_pdev->ftt_frequency);
}

static	ssize_t show_ftt_is_charging	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", is_ftt_charging(ftt_pdev));
}

static	ssize_t show_ftt_ant_level	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", ftt_pdev->ant_level);
}

static	ssize_t show_ftt_pad_type	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);
	char *ppad_type;

	ppad_type = (char *)get_ftt_saved_pad_type_str(ftt_pdev);
	if (ppad_type == NULL)
		ppad_type = "";
	return sprintf(buf, "%s\n", ppad_type);
}

static	ssize_t show_ftt_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)platform_get_drvdata(pdev);

	switch (get_ftt_charger_current_status(ftt_pdev)) {
	case	FTT_CHARGER_STATUS_PING_DETECT :
		return sprintf(buf, "%s\n", "DETECT");
		break;
	case	FTT_CHARGER_STATUS_PRE_CHARGING :
		return sprintf(buf, "%s\n", "PRE_CHARGING");
		break;
	case	FTT_CHARGER_STATUS_CHARGING :
		return sprintf(buf, "%s\n", "CHARGING");
		break;
	default :
		return sprintf(buf, "%s\n", "NULL");
		break;
	}

	return 0;
}

static	ssize_t show_ftt_ver(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "v%u.%u.%u\n", FTT_DD_MAJOR_VERSION, FTT_DD_MINOR_VERSION_A, FTT_DD_MINOR_VERSION_B);
}

static	ssize_t show_ftt_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s l%d : v%u.%u.%u ,compile : %s, %s\n", DEVICE_NAME, MAX_ANT_LEVEL, FTT_DD_MAJOR_VERSION, FTT_DD_MINOR_VERSION_A, FTT_DD_MINOR_VERSION_B, __DATE__, __TIME__);
}

#endif /* CONFIG_FTT_SYSFS */
