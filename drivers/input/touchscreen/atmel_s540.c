/*
 * Atmel maXTouch Touchscreen driver
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/atmel_s540.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/board_lge.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/time.h>

#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/sysdev.h>
#ifdef TSP_PATCH
#include "mxts_patch.c"
static u8 patch_bin[] = {
	#include "mxts_patch_bin.h"
};

#define MXT_PATCH_BAT_MODE_EVENT	0	/* 0 : BAT mode */
#define MXT_PATCH_TA_MODE_EVENT	1	/* 1: TA mode */
#define MXT_PATCH_KNOCKON_BAT_MODE_EVENT	2	/* 2: Knock on BAT mode */
#define MXT_PATCH_WAKEUP_BAT_MODE_EVENT	3	/* 3: Wake Up  BAT mode */
#define MXT_PATCH_KNOCKON_TA_MODE_EVENT	4	/* 4: Knock On TA mode */
#define MXT_PATCH_WAKEUP_TA_MODE_EVENT	5	/* 5: Wakeup  TA mode */
#endif

#define DEBUG_ABS	1
#define FIRMUP_ON_PROBE

/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"
#ifdef FIRMUP_ON_PROBE
#ifdef MXT_GESTURE_RECOGNIZE
#define MXT_LATEST_CONFIG_CRC	0xA8EA22
u8 latest_firmware[] = {
	#include "mXT540S_V1.0.E2_.h"
};
#else
#ifdef MFTS_TOUCH
#define MXT_LATEST_CONFIG_CRC	0x5F4D1E
#else
#define MXT_LATEST_CONFIG_CRC	0x629BB2
#endif
u8 latest_firmware[] = {
	#include "mxt540s_V3.0.AA_.h"
};
#endif
#endif

#ifdef CUST_B_TOUCH
#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];
#endif

#define MXT_ANTI_ENABLE_MASK	 0x0F
#define MXT_ANTI_DISABLE_MASK	 0xF0
static bool is_probing;
static bool selftest_enable;
static bool selftest_show;
static bool update_cfg_force;
static bool update_fw_force;
static bool chargerlogo;
int quick_cover_status = 0;
bool mxt_mfts = false;
/* check TA status */
static bool wait_change_cfg = false;
static int ime_drumming_status = 0;

#ifdef MXT_GESTURE_RECOGNIZE
#define MXT_T100_REPORT_EN_MASK	0x02
#define MXT_T100_REPORT_DIS_MASK	0xFD
#define MXT_ENABLE_MASK	0x01
#define MXT_DISABLE_MASK 0xFE
/* Setting Gesture mode (Sleep Current) */
static u8 t47_ctrl_cfg = 0;
static u8 t65_ctrl_cfg = 0;
static u8 t72_ctrl_cfg = 0;
static u8 t100_ctrl_cfg = 0;
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* Early-suspend level */
#define MXT_SUSPEND_LEVEL 1
static void mxt_early_suspend(struct early_suspend *h);
static void mxt_late_resume(struct early_suspend *h);
#endif
struct workqueue_struct*	touch_wq;

struct lge_touch_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct mxt_data *ts, char *buf);
	ssize_t (*store)(struct mxt_data *ts, const char *buf, size_t count);
};

#define LGE_TOUCH_ATTR(_name, _mode, _show, _store)	\
struct lge_touch_attribute lge_touch_attr_##_name = __ATTR(_name, _mode, _show, _store)

/* Auto Test interface for some model */
struct mxt_data *touch_test_dev = NULL;
EXPORT_SYMBOL(touch_test_dev);

#ifdef MXT_GESTURE_RECOGNIZE
static struct wake_lock touch_wake_lock;
static struct mutex i2c_suspend_lock;
static bool touch_irq_wake = 0;

static int touch_enable_irq_wake(unsigned int irq){
	int ret = 0;

	printk("[lge_touch] enable touch irq wake(%d)\n", touch_irq_wake);
	if(!touch_irq_wake){
		touch_irq_wake = 1;
		ret= enable_irq_wake(irq);
	}
	return ret;
}
static int touch_disable_irq_wake(unsigned int irq){
	int ret = 0;

	printk("[lge_touch] disable touch irq wake(%d)\n", touch_irq_wake);
	if(touch_irq_wake){
		touch_irq_wake = 0;
		ret = disable_irq_wake(irq);
	}
	return ret;
}
#endif

static bool touch_enable = 1;
static void touch_enable_irq(unsigned int irq){
	printk("[lge_touch] enable touch irq(%d)\n", touch_enable);

	if(!touch_enable){
		touch_enable = 1;
		enable_irq(irq);
	}
}
static void touch_disable_irq(unsigned int irq){
	printk("[lge_touch] disable touch irq(%d)\n", touch_enable);

	if(touch_enable){
		touch_enable = 0;
		disable_irq(irq);
	}
}

inline size_t mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size_minus_one + 1;
}

static inline size_t mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances_minus_one + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_TOUCH_PROXIMITY_T23:
#ifdef MXT_GESTURE_RECOGNIZE
	case MXT_PROCI_ONETOUCH_T24:
#endif
	case MXT_SPT_SELFTEST_T25:
	case MXT_PROCI_GRIPSUPPRESSION_T40:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_SPT_CTECONFIG_T46:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCI_ADAPTIVETHRESHOLD_T55:
	case MXT_PROCI_SHIELDLESS_T56:
	/*case MXT_SPT_TIMER_T61:
	case MXT_PROCI_LENSBENDING_T65:
	case MXT_SPT_GOLDENREFERENCES_T66:
	case MXT_PROCI_PALMGESTUREPROCESSOR_T69:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTROLLER_T70:
	case MXT_PROCG_NOISESUPPRESSION_T72:
	case MXT_GLOVEDETECTION_T78:
	case MXT_RETRANSMISSIONCOMPENSATION_T80:
	case MXT_PROCI_GESTUREPROCESSOR_T84:
	case MXT_TOUCH_MULTITOUCHSCREEN_T100:
	case MXT_SPT_TOUCHSCREENHOVER_T101:
	case MXT_SPT_SELFCAPCBCRCONFIG_T102:
	case MXT_PROCI_SCHNOISESUPPRESSION_T103:
	case MXT_SPT_AUXTOUCHCONFIG_T104:
	case MXT_SPT_DRIVENPLATEHOVERCONFIG_T105:*/
		return true;
	default:
		return false;
	}
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
		       message, data->T5_msg_size, false);
}

static int mxt_wait_for_completion(struct mxt_data *data,
			struct completion *comp, unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		dev_err(dev, "Wait for completion interrupted.\n");
		return -EINTR;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion timed out.\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static int mxt_bootloader_read(struct mxt_data *data,
			       u8 *val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);

	if (ret == 1) {
		ret = 0;
	} else {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&data->client->dev, "i2c recv failed (%d)\n", ret);
	}

	return ret;
}

static int mxt_bootloader_write(struct mxt_data *data,
				const u8 * const val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (u8 *)val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret == 1) {
		ret = 0;
	} else {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&data->client->dev, "i2c send failed (%d)\n", ret);
	}

	return ret;
}

static int mxt_lookup_bootloader_address(struct mxt_data *data, u8 retry)
{
	u8 appmode = data->client->addr;
	u8 bootloader;
	u8 family_id = 0;

	if (data->info)
		family_id = data->info->family_id;

	switch (appmode) {
	case 0x4a:
	case 0x4b:
		/* Chips after 1664S use different scheme */
		if ((retry % 2) || family_id >= 0xa2) {
			bootloader = appmode - 0x24;
			break;
		}
		/* Fall through for normal case */
	case 0x4c:
	case 0x4d:
	case 0x5a:
	case 0x5b:
		bootloader = appmode - 0x26;
		break;
	default:
		dev_err(&data->client->dev,
			"Appmode i2c address 0x%02x not found\n",
			appmode);
		return -EINVAL;
	}

	data->bootloader_addr = bootloader;
	return 0;
}

static int mxt_probe_bootloader(struct mxt_data *data, u8 retry)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;
	dev_info(dev, "mxt probe bootloader\n");
	ret = mxt_lookup_bootloader_address(data, retry);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
			val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state)
{
	struct device *dev = &data->client->dev;
	u8 val;
	int ret;

recheck:
	if (state != MXT_WAITING_BOOTLOAD_CMD) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		ret = mxt_wait_for_completion(data, &data->bl_completion,
					      MXT_FW_CHG_TIMEOUT);
		if (ret) {
			/*
			 * TODO: handle -EINTR better by terminating fw update
			 * process before returning to userspace by writing
			 * length 0x000 to device (iff we are in
			 * WAITING_FRAME_DATA state).
			 */
			dev_err(dev, "Update wait error %d\n", ret);
			return ret;
		}
	}

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	if (state == MXT_WAITING_BOOTLOAD_CMD)
		val = mxt_get_bootloader_version(data, val);

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK) {
			goto recheck;
		} else if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader state %02X != %02X\n",
			val, state);
		return -EINVAL;
	}

	return 0;
}

static int mxt_send_bootloader_cmd(struct mxt_data *data, bool unlock)
{
	int ret;
	u8 buf[2];

	if (unlock) {
		buf[0] = MXT_UNLOCK_CMD_LSB;
		buf[1] = MXT_UNLOCK_CMD_MSB;
	} else {
		buf[0] = 0x01;
		buf[1] = 0x01;
	}

	ret = mxt_bootloader_write(data, buf, 2);
	if (ret)
		return ret;

	return 0;
}

static int __mxt_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
#ifdef MXT_GESTURE_RECOGNIZE
	int i=0;
#else
	int ret;
	bool retry = false;
#endif


	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;
#ifdef MXT_GESTURE_RECOGNIZE
	do {
		if (i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer))==2)
			return 0;
		dev_dbg(&client->dev, "%s: i2c retry %d\n", __func__, i+1);
		msleep(MXT_WAKEUP_TIME);
	} while (++i < 10);

	dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
	return -EIO;
#else
retry_read:
	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		if (!retry) {
			dev_err(&client->dev, "%s: i2c retry\n", __func__);
			msleep(MXT_WAKEUP_TIME);
			retry = true;
			goto retry_read;
		} else {
			dev_err(&client->dev, "%s: i2c transfer failed (%d)\n",
				__func__, ret);
			return -EIO;
		}
	}

	return 0;
#endif
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
			   const void *val)
{
	u8 *buf;
	size_t count;
#ifdef MXT_GESTURE_RECOGNIZE
	int i = 0;
#else
	int ret;
	bool retry = false;
#endif

	count = len + 2;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], val, len);
#ifdef MXT_GESTURE_RECOGNIZE
	do {
		if (i2c_master_send(client, buf, count)==count){
			kfree(buf);
			return 0;
		}
		dev_info(&client->dev, "%s: i2c retry %d\n", __func__, i+1);
		msleep(MXT_WAKEUP_TIME);
	} while (++i < 10);
		dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
		kfree(buf);
		return -EIO;
#else
retry_write:
	ret = i2c_master_send(client, buf, count);
	if (ret == count) {
		ret = 0;
	} else {
		if (!retry) {
			dev_err(&client->dev, "%s: i2c retry\n", __func__);
			msleep(MXT_WAKEUP_TIME);
			retry = true;
			goto retry_write;
		} else {
			dev_err(&client->dev, "%s: i2c send failed (%d)\n",
				__func__, ret);
			ret = -EIO;
		}
	}

	kfree(buf);
	return ret;
#endif
}

int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

#ifdef TSP_PATCH
int mxt_write_mem(struct mxt_data *data,
		u16 reg, u8 len, const u8 *buf)
{
	int ret = 0;
	ret = __mxt_write_reg(data->client, reg, len, buf);
	return ret;
}

int mxt_read_mem(struct mxt_data *data, u16 reg, u8 len, void *buf)
{
	int ret = 0;
	ret = __mxt_read_reg(data->client, reg, len, buf);
	return ret;
}

static void mxt_make_reportid_table(struct mxt_data *data)
{
	struct mxt_object *object = data->object_table;
	struct mxt_reportid *reportids = data->reportids;		
	int i, j;
	int id = 0;

	for (i = 0; i < data->info->object_num; i++) {
		for (j = 0; j < object[i].num_report_ids * (object[i].instances_minus_one+1); j++) {
			id++;

			reportids[id].type = object[i].type;
			reportids[id].index = j;

			dev_dbg(&data->client->dev, "Report_id[%d]:\tType=%d\tIndex[%d]\n",
					id, reportids[id].type, reportids[id].index);
		}
	}
}
#endif

struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
}

#ifdef FIRMUP_ON_PROBE
static int mxt_init_write_config(struct mxt_data *data,
		u8 type, const u8 *cfg)
{
	struct mxt_object *object;
	int ret;

	object = mxt_get_object(data, type);
	if (!object) {
        dev_err(&data->client->dev,
			"%s error Cannot get object_type T%d\n", __func__, type);
        return -EINVAL;
    }

	if ((mxt_obj_size(object) == 0) || (object->start_address == 0)) {
		dev_err(&data->client->dev,
			"%s error object_type T%d\n", __func__, type);
		return -ENODEV;
	}

	dev_dbg(&data->client->dev, "Write config T%d: start_addr=%d, size=%d * instance=%d\n",
		type, object->start_address, mxt_obj_size(object), mxt_obj_instances(object));

	ret = __mxt_write_reg(data->client, object->start_address,
				mxt_obj_size(object)*mxt_obj_instances(object), cfg);

	if (ret) {
		dev_err(&data->client->dev,
			"%s write error T%d address[0x%x]\n",
			__func__, type, object->start_address);
		return ret;
	}

	return ret;
}

static int mxt_write_configuration(struct mxt_data *data)
{
	int i = 0;
	int ret = 0;
	u8 ** tsp_config = (u8 **)data->pdata->config_array->config_t;

	dev_info(&data->client->dev, "Write configuration data\n");

	for (i = 0; i < MXT_TMAX; i++) {
		ret = mxt_init_write_config(data, tsp_config[i][0],
							tsp_config[i] + 1);
		if (ret) {
			dev_err(&data->client->dev, "Failed to write configuration\n");
			goto out;
		}
	}

out:
	return ret;
}
#endif

void trigger_baseline_state_machine(int plug_in, int type)
{
	if(!touch_test_dev){
		return;
	}

	if(plug_in == 0 || plug_in == 1){
		if(plug_in ==0){
			dev_info(&touch_test_dev->client->dev, " TA/USB NOT CONNECTED.\n");
			touch_test_dev->charging_mode = 0;
#ifdef TSP_PATCH
			if (!touch_test_dev->suspended) {
				if(touch_test_dev->power_status == MXT_POWER_OFF){
					dev_info(&touch_test_dev->client->dev, " WAIT BAT_MODE %d\n", MXT_PATCH_BAT_MODE_EVENT);
					wait_change_cfg = true;
					touch_test_dev->ta_status = MXT_PATCH_BAT_MODE_EVENT;
				}else{
					dev_info(&touch_test_dev->client->dev, " BAT_MODE %d\n", MXT_PATCH_BAT_MODE_EVENT);
					touch_test_dev->ta_status = MXT_PATCH_BAT_MODE_EVENT;
					mxt_patch_test_event(touch_test_dev, MXT_PATCH_BAT_MODE_EVENT);
				}
			} else {
				if(touch_test_dev->power_status == MXT_POWER_OFF){
					dev_info(&touch_test_dev->client->dev, " WAIT WAKEUP_BAT_MODE %d\n", MXT_PATCH_WAKEUP_BAT_MODE_EVENT);
					wait_change_cfg = true;
					touch_test_dev->ta_status = MXT_PATCH_WAKEUP_BAT_MODE_EVENT;
				}else{
					dev_info(&touch_test_dev->client->dev, " WAKEUP_BAT_MODE %d\n", MXT_PATCH_WAKEUP_BAT_MODE_EVENT);
					touch_test_dev->ta_status = MXT_PATCH_WAKEUP_BAT_MODE_EVENT;
					mxt_patch_test_event(touch_test_dev, MXT_PATCH_WAKEUP_BAT_MODE_EVENT);
				}
			}
#endif

		}else if(plug_in ==1){
			dev_info(&touch_test_dev->client->dev, " %s CONNECTED.\n", type ? "TA" : "USB");
			touch_test_dev->charging_mode = 1;
#ifdef TSP_PATCH
			if (!touch_test_dev->suspended) {
				if(touch_test_dev->power_status == MXT_POWER_OFF){
					dev_info(&touch_test_dev->client->dev, " WAIT TA_MODE %d\n", MXT_PATCH_TA_MODE_EVENT);
					wait_change_cfg = true;
					touch_test_dev->ta_status = MXT_PATCH_TA_MODE_EVENT;
				}else{
					dev_info(&touch_test_dev->client->dev, " TA_MODE %d\n", MXT_PATCH_TA_MODE_EVENT);
					touch_test_dev->ta_status = MXT_PATCH_TA_MODE_EVENT;
					mxt_patch_test_event(touch_test_dev, MXT_PATCH_TA_MODE_EVENT);
				}
			} else {
				if(touch_test_dev->power_status == MXT_POWER_OFF){
					dev_info(&touch_test_dev->client->dev, " WAIT WAKEUP_TA_MODE %d\n", MXT_PATCH_WAKEUP_TA_MODE_EVENT);
					wait_change_cfg = true;
					touch_test_dev->ta_status = MXT_PATCH_WAKEUP_TA_MODE_EVENT;
				}else{
					dev_info(&touch_test_dev->client->dev, " WAKEUP_TA_MODE %d\n", MXT_PATCH_WAKEUP_TA_MODE_EVENT);
					touch_test_dev->ta_status = MXT_PATCH_WAKEUP_TA_MODE_EVENT;
					mxt_patch_test_event(touch_test_dev, MXT_PATCH_WAKEUP_TA_MODE_EVENT);
				}
			}
#endif
		}
	}
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_info(dev, "T6 Config Checksum: 0x%06X\n", crc);
		complete(&data->crc_completion);
	}

	/* Detect transition out of reset */
	if ((data->t6_status & MXT_T6_STATUS_RESET) &&
	    !(status & MXT_T6_STATUS_RESET))
		complete(&data->reset_completion);

	/* Output debug if status has changed */
	if (status != data->t6_status)
		dev_info(dev, "T6 Status 0x%02X%s%s%s%s%s%s%s\n",
			status,
			(status == 0) ? " OK" : "",
			(status & MXT_T6_STATUS_RESET) ? " RESET" : "",
			(status & MXT_T6_STATUS_OFL) ? " OFL" : "",
			(status & MXT_T6_STATUS_SIGERR) ? " SIGERR" : "",
			(status & MXT_T6_STATUS_CAL) ? " CAL" : "",
			(status & MXT_T6_STATUS_CFGERR) ? " CFGERR" : "",
			(status & MXT_T6_STATUS_COMSERR) ? " COMSERR" : "");

	/* Save current status */
	data->t6_status = status;
}

static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct input_dev *input = data->input_dev;
	const struct mxt_platform_data *pdata = data->pdata;
	bool button;
	int i;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* Active-low switch */
	for (i = 0; i < pdata->t19_num_keys; i++) {
		if (pdata->t19_keymap[i] == KEY_RESERVED)
			continue;
		button = !(message[1] & (1 << i));
		input_report_key(input, pdata->t19_keymap[i], button);
	}
}

static void mxt_input_sync(struct input_dev *input_dev)
{
	input_mt_report_pointer_emulation(input_dev, false);
	input_sync(input_dev);
}

static void mxt_proc_t9_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int tool;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T9_reportid_min;
	status = message[1];
	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	/* Handle 10/12 bit switching */
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;

	area = message[5];

	amplitude = message[6];
	vector = message[7];

	dev_dbg(dev,
		"[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n",
		id,
		(status & MXT_T9_DETECT) ? 'D' : '.',
		(status & MXT_T9_PRESS) ? 'P' : '.',
		(status & MXT_T9_RELEASE) ? 'R' : '.',
		(status & MXT_T9_MOVE) ? 'M' : '.',
		(status & MXT_T9_VECTOR) ? 'V' : '.',
		(status & MXT_T9_AMP) ? 'A' : '.',
		(status & MXT_T9_SUPPRESS) ? 'S' : '.',
		(status & MXT_T9_UNGRIP) ? 'U' : '.',
		x, y, area, amplitude, vector);

	input_mt_slot(input_dev, id);

	if (status & MXT_T9_DETECT) {
		/* Multiple bits may be set if the host is slow to read the
		 * status messages, indicating all the events that have
		 * happened */
		if (status & MXT_T9_RELEASE) {
			input_mt_report_slot_state(input_dev,
						   MT_TOOL_FINGER, 0);
			mxt_input_sync(input_dev);
		}

		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if (area == 0) {
			area = MXT_TOUCH_MAJOR_T47_STYLUS;
			tool = MT_TOOL_PEN;
		} else {
			tool = MT_TOOL_FINGER;
		}

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	data->update_input = true;
}
static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset, u8 value, bool wait);
static void mxt_reset_slots(struct mxt_data *data);
static void mxt_proc_t100_anti_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	u8 scr_status;
	u8 num_rpt_touch;

	scr_status = message[1];
	num_rpt_touch = message[2];
	data->anti->inter_area = (message[8] << 8) | message[7];
	data->anti->anti_area = (message[6] << 8) | message[5];
	data->anti->touch_area = (message[4] << 8) | message[3];

	if (scr_status == 0 && num_rpt_touch == 0 && data->anti->anti_area > 0 && data->anti->inter_area > 0) {
		data->anti->fcnt0_msg_cnt++;
		dev_dbg(dev, "Anti #%d scr_status(%d) num_rpt_touch(%d) anti touch area(%d), touch area(%d), internal area(%d)\n",
				data->anti->fcnt0_msg_cnt, scr_status, num_rpt_touch, data->anti->anti_area, data->anti->touch_area, data->anti->inter_area);
		if (data->anti->fcnt0_msg_cnt >= 10) {
			dev_info(dev, "Anti touch fcnt=0 : run Calibration*****************\n");
			mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
			mxt_reset_slots(data);
			data->anti->fcnt0_msg_cnt = 0;
			return;
		}
	} else {
		if (data->anti->fcnt0_msg_cnt)
			dev_dbg(dev, "Anti scr_status(%d) num_rpt_touch(%d) anti touch area(%d), touch area(%d), internal area(%d)\n",
				scr_status, num_rpt_touch, data->anti->anti_area, data->anti->touch_area, data->anti->inter_area);
		data->anti->fcnt0_msg_cnt = 0;
	}
}

static void mxt_proc_t100_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
#ifndef CUST_B_TOUCH
	struct input_dev *input_dev = data->input_dev;
#endif
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting){
		dev_dbg(dev,"return event\n");
		return;
	}

	data->anti->fcnt0_msg_cnt = 0;

	id = message[0] - data->T100_reportid_min - 2;
#ifndef CUST_B_TOUCH
	/* ignore SCRSTATUS events */
	if (id < 0 || id >= data->pdata->numtouch) {
		dev_err(dev, "limited number of finger is %d\n", data->pdata->numtouch);
		return;
	}
	input_mt_slot(input_dev, id);
#endif

	status = message[1];
	x = (message[3] << 8) | message[2];
	y = (message[5] << 8) | message[4];

	area = message[data->t100_aux_area];
	amplitude = message[data->t100_aux_ampl];	/* message[6] */
	vector =  message[data->t100_aux_vect];

	if(status & (1 << 2))
		dev_dbg(dev, "[%u] %c%c %s%s%s (%02X) x:%u y:%u amp:%u area:%02X vec:%02X\n",
			id,
			((status & MXT_T100_STATUS_MASK) == MXT_T100_PRESS) ? 'P' : '.',
			((status & MXT_T100_STATUS_MASK) == MXT_T100_RELEASE) ? 'R' : '.',
			((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_FINGER) ? "FIN" : ".",
			((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_STYLUS) ? "PEN" : ".",
			((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_PALM) ? "PALM" : ".",
			status, x, y, amplitude, area, vector);

#ifdef CUST_B_TOUCH
	if (status & MXT_T100_DETECT) {
		/* Multiple bits may be set if the host is slow to read the
		* status messages, indicating all the events that have
	 	* happened */

		if ((status & MXT_T100_STATUS_MASK) == MXT_T100_RELEASE) {
			data->ts_data.curr_data[id].id = id;
			data->ts_data.curr_data[id].status = FINGER_RELEASED;
		}

		data->ts_data.curr_data[id].id = id;
		data->ts_data.curr_data[id].x_position = x;
		data->ts_data.curr_data[id].y_position = y;
		data->ts_data.curr_data[id].pressure = amplitude;
		data->ts_data.curr_data[id].orientation = vector;
		data->ts_data.curr_data[id].tool = MT_TOOL_FINGER;

		if (message[7] >= message[8]) {
			data->ts_data.curr_data[id].touch_major = message[7];
			data->ts_data.curr_data[id].touch_minor = message[8];
		} else {
			data->ts_data.curr_data[id].touch_major = message[8];
			data->ts_data.curr_data[id].touch_minor = message[7];
		}

		if ((status & MXT_T100_STATUS_MASK) == MXT_T100_PRESS) {
			data->ts_data.curr_data[id].status = FINGER_PRESSED;
		}else if((status & MXT_T100_STATUS_MASK) == MXT_T100_MOVE){
			data->ts_data.curr_data[id].status = FINGER_MOVED;
		}

		dev_dbg(dev, "%s : curr_data[%d] x(%d), y(%d), area(%d), amplitude(%d)\n",
				__func__, id, x, y, area, amplitude);
		if((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_STYLUS){
			data->ts_data.curr_data[id].is_pen= true;
			data->ts_data.curr_data[id].is_palm= false;
		}else if((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_PALM){
			data->ts_data.curr_data[id].is_pen= false;
			data->ts_data.curr_data[id].is_palm= true;
		}else{
			data->ts_data.curr_data[id].is_pen= false;
			data->ts_data.curr_data[id].is_palm= false;
		}
	} else {
		/* Touch Release */
		data->ts_data.curr_data[id].id = id;
		data->ts_data.curr_data[id].status = FINGER_RELEASED;
	}
#else
	if (status & MXT_T100_DETECT) {
		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if ((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_STYLUS)
			tool = MT_TOOL_PEN;
		else
			tool = MT_TOOL_FINGER;

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

		if (data->t100_aux_ampl)
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 message[data->t100_aux_ampl]);

		if (data->t100_aux_area) {
			if (tool == MT_TOOL_PEN)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 MXT_TOUCH_MAJOR_T47_STYLUS);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 message[data->t100_aux_area]);
		}

		if (data->t100_aux_vect)
			input_report_abs(input_dev, ABS_MT_ORIENTATION,
					 message[data->t100_aux_vect]);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	data->update_input = true;
}


static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	for (key = 0; key < data->pdata->t15_num_keys; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    data->pdata->t15_keymap[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    data->pdata->t15_keymap[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status & MXT_T42_MSG_TCHSUP)
		dev_info(dev, "T42 suppress\n");
	else
		dev_info(dev, "T42 normal\n");
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	dev_dbg(dev, "T48 state %d status %02X %s%s%s%s%s\n",
			state,
			status,
			(status & 0x01) ? "FREQCHG " : "",
			(status & 0x02) ? "APXCHG " : "",
			(status & 0x04) ? "ALGOERR " : "",
			(status & 0x10) ? "STATCHG " : "",
			(status & 0x20) ? "NLVLCHG " : "");

	return 0;
}

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_STYLUS_PRESSURE_MASK;

	dev_dbg(dev,
		"[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		(msg[1] & MXT_STYLUS_SUPPRESS) ? 'S' : '.',
		(msg[1] & MXT_STYLUS_MOVE)     ? 'M' : '.',
		(msg[1] & MXT_STYLUS_RELEASE)  ? 'R' : '.',
		(msg[1] & MXT_STYLUS_PRESS)    ? 'P' : '.',
		x, y, pressure,
		(msg[2] & MXT_STYLUS_BARREL) ? 'B' : '.',
		(msg[2] & MXT_STYLUS_ERASER) ? 'E' : '.',
		(msg[2] & MXT_STYLUS_TIP)    ? 'T' : '.',
		(msg[2] & MXT_STYLUS_DETECT) ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS, (msg[2] & MXT_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2, (msg[2] & MXT_STYLUS_BARREL));

	mxt_input_sync(input_dev);
}

static int mxt_proc_t25_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	u8 status = message[1];

	if(!selftest_enable)
		return 0;

	dev_info(dev, "T25 Self Test completed %u\n",status);

	if(selftest_show)
		data->self_test_status[0] = status;

	if ( status == 0xFE ) {
		dev_info(dev, "[SUCCESS] All tests passed\n");
		data->self_test_result = true;
	} else {
		if (status == 0xFD) {
			dev_err(dev, "[FAIL] Invalid test code\n");
		} else if (status == 0xFC)  {
			dev_err(dev, "[FAIL] Unrelated fault\n");
		} else if (status == 0x01) {
			dev_err(dev, "[FAIL] AVdd or XVdd is not present\n");
		} else if (status == 0x12) {
			dev_err(dev, "[FAIL] Pin fault (SEQ_NUM %u, X_PIN %u, Y_PIN %u)\n", message[2], message[3], message[4]);
			if(selftest_show){
				data->self_test_status[1] = message[2];
				data->self_test_status[2] = message[3];
				data->self_test_status[3] = message[4];
			}
		} else if (status == 0x17) {
			dev_err(dev, "[FAIL] Signal limit fault (TYPE_NUM %u, TYPE_INSTANCE %u)\n", message[2], message[3]);
			if(selftest_show){
				data->self_test_status[1] = message[2];
				data->self_test_status[2] = message[3];
			}
		} else;
		data->self_test_result = false;
	}

	selftest_enable = false;
	complete(&data->t25_completion);
	return 0;
}

static struct sys_device lge_touch_sys_device;
#ifdef MXT_GESTURE_RECOGNIZE
static void mxt_proc_t35_messages(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	u8 msg;

	if (data->in_bootloader)
		return;

	msg = message[1];

	if(data->mxt_character_enable && msg == 0x43){	/*C*/
		dev_info(dev, "character C recognize.\n");
	}else if(data->mxt_character_enable && msg == 0x4D){	/*M*/
		dev_info(dev, "character M recognize.\n");
	}else if(data->mxt_character_enable && msg == 0x57){	/*W*/
		dev_info(dev, "character W recognize.\n");
	}else if(data->mxt_volume_enable && msg == 0x83){	/*Vol Up*/
		dev_info(dev, "Volume Up recognize.\n");
	}else if(data->mxt_volume_enable && msg == 0x84){	/*Vol Down*/
		dev_info(dev, "Volume Down recognize.\n");
	}else{
		dev_info(dev, "Unknown pattern recognize %d 0x%x\n", msg, msg);
	}
	wake_unlock(&touch_wake_lock);
}

char *knockon_event[2] = { "TOUCH_GESTURE_WAKEUP=WAKEUP", NULL };
static void mxt_proc_t24_messages(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	u8 msg;
	int x;
	int y;

	if (data->in_bootloader)
		return;

	msg = message[1];


	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	if(msg == 0x04) {
		if(quick_cover_status == 1){
			if(!((x > 440 ) && (x < 1760) && (y > 80) && (y < 1780))){
				dev_info(dev, "Out Of Quick Window Boundary Double_Tap!!     %d     %d \n",x,y);
				wake_unlock(&touch_wake_lock);
				return;
			}
		}else{
			if(!((x > 220 ) && (x < 1940) && (y > 270) && (y < 3570))){
				dev_info(dev, "Out Of Boundary Double_Tap!!     %d     %d \n",x,y);
				wake_unlock(&touch_wake_lock);
				return;
			}
		}
		dev_info(dev, "Double_Tap!!     %d     %d \n",x,y);
		kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, knockon_event);
	}
	wake_unlock(&touch_wake_lock);
}
#endif
static int mxt_proc_message(struct mxt_data *data, u8 *message)
{
	u8 report_id = message[0];
	bool dump = data->debug_enabled;

	if (report_id == MXT_RPTID_NOMSG)
		return 0;
	do_gettimeofday(&t_ex_debug[TIME_CURR_TIME]);

	if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, message);
	} else if (report_id >= data->T9_reportid_min
	    && report_id <= data->T9_reportid_max) {
		mxt_proc_t9_message(data, message);
	} else if (report_id > data->T100_reportid_min
	    && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_message(data, message);
	} else if (report_id == data->T19_reportid) {
		mxt_input_button(data, message);
		data->update_input = true;
	} else if (report_id >= data->T63_reportid_min
		   && report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T42_reportid_min
		   && report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, message);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, message);
	} else if (report_id >= data->T15_reportid_min
		   && report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, message);
#ifdef MXT_GESTURE_RECOGNIZE
	} else if (report_id == data->T24_reportid) {
		if (data->mxt_knock_on_enable && data->suspended){
			wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(1000));
			mxt_proc_t24_messages(data, message);
		}
	} else if (report_id == data->T35_reportid) {
		if ((data->mxt_character_enable || data->mxt_volume_enable) && data->suspended){
			wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(1000));
			mxt_proc_t35_messages(data, message);
		}
#endif
	} else if (report_id == data->T25_reportid){
		mxt_proc_t25_message(data, message);
	} else if (report_id == data->T100_reportid_min) {
		/* don't print dump message if it's antitouch information.*/
		mxt_proc_t100_anti_message(data, message);
	} else {
		dump = true;
	}

	if (dump)
		mxt_dump_message(data, message);

#ifdef TSP_PATCH
	{
		struct mxt_message stMsg;	
		stMsg.reportid = report_id;
		memcpy(stMsg.message, &message[1], 8);	
		mxt_patch_message(data, &stMsg);
	}	
#endif
	return 1;
}

static int mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = __mxt_read_reg(data->client, data->T5_address,
				data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 1)
			num_valid++;
	}

	/* return number of messages read */
	return num_valid;
}

#ifdef CUST_B_TOUCH
//                                                                 
#if TOUCHEVENTFILTER
int set_minor_data(struct mxt_data *data, int area, u8 vector)
{
	struct device *dev = &data->client->dev;

	u8 tmp;
	int component1;
	int component2;
	int magnitude = 0;
	int minor = 0;

	int i;

	/* 1. get componet data */
	// component1
	tmp = ( (vector >> 4) & 0xf);
	if( tmp & 0x8 )
		component1 = (int)(((~(tmp) ) & 0xf) | 0x1 );
	else
		component1 = tmp;

	// component2
	tmp = (vector & 0xf);
	if( tmp & 0x8 )
		component2 = (int)(((~(tmp) ) & 0xf) | 0x1 );
	else
		component2 = tmp;

	/* 2. magnitude = SQRT(component1^2 + component2^2) */
	tmp = (component1 * component1) + (component2 * component2);

	/* 3. make minor date
	// when the magnitude is same, the larger area has the longer minor value.
	// that means, when the area is same, the longer magnitude has the shorter minor value.
	// 3-1. if shape is circle, minor = major.
	// when the shape is circle, vector = 0x00 / 0x01 / 0x10 / 0x11
	// then magnitude^2 = 0 /1 / 1 / 2
	*/
	if ( tmp < 3 )
	{
		minor = area;
		magnitude = 1;
	}
	else {
	/* 3-2. if shape is elipse, minor = area / magnitude.	*/
	// find SQRT(magnitude^2)
		for( i = 9 ; i > 1 ; i--) {
			if ( tmp > ((i*i) - 1) ){
				magnitude = i;
				break;
			}
		}
		minor = area / magnitude;
	}

	dev_dbg(dev,
		"%5u area: %5u minor: %5u magnitude: %5u vector: %5u component1: %5u component2: %5u \n",
			tmp, area, minor, magnitude, vector, component1, component2);

	return minor;
}
#endif
//                                                                 

static char* get_tool_type(struct mxt_data *data, struct t_data touch_data) {
	if (touch_data.tool == MT_TOOL_FINGER) {
		if (touch_data.is_pen) {
			return "PEN";
		} else if(touch_data.is_palm){
			return "PALM";
		} else {
			return "FINGER";
		}
	} else if (touch_data.tool == MT_TOOL_PALM) {
		return "PALM";
	} else if (touch_data.tool == MT_TOOL_PEN) {
		return "PEN";
	} else {
		dev_err(&data->client->dev, "Invalid tool type : %d", touch_data.tool);
	}
	return "Unknown";
}
#endif
static void safety_reset(struct mxt_data *data);
static void mxt_process_messages_t44(struct work_struct *work)
{
	struct mxt_data *data =
			container_of(work, struct mxt_data, work);
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;
#ifdef CUST_B_TOUCH
	int report_num = 0;
	char *tool_type;
	int i;
#endif

	/* Read T44 and T5 together */
	ret = __mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);

	if (ret) {
		dev_err(dev, "Failed to read T44 and T5 (%d)\n", ret);
		goto err_out_critical;
	}

	count = data->msg_buf[0];

	if (count == 0) {
		dev_dbg(dev, "Interrupt triggered but zero messages\n");
		goto out_ignore_interrupt;
	} else if (count > data->max_reportid) {
		dev_err(dev, "T44 count %d exceeded max report id\n", count);
		count = data->max_reportid;
	}
#ifdef CUST_B_TOUCH
	data->ts_data.total_num = 0;
#endif

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		dev_warn(dev, "Unexpected invalid message\n");
		goto out_ignore_interrupt;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0)
			goto end;
		else if (ret != num_left)
			dev_warn(dev, "Unexpected invalid message\n");
	}
#ifdef CUST_B_TOUCH
	for (i = 0; i < data->pdata->numtouch; i++) {

		if (data->ts_data.curr_data[i].status == FINGER_INACTIVE &&
			data->ts_data.prev_data[i].status != FINGER_INACTIVE &&
			data->ts_data.prev_data[i].status != FINGER_RELEASED) {
			memcpy(&data->ts_data.curr_data[i], &data->ts_data.prev_data[i], sizeof(data->ts_data.prev_data[i]));
			data->ts_data.curr_data[i].skip_report = true;
		}else if (data->ts_data.curr_data[i].status == FINGER_INACTIVE) {
			continue;
		}

		if (data->ts_data.curr_data[i].status == FINGER_PRESSED ||
			data->ts_data.curr_data[i].status == FINGER_MOVED) {
			data->ts_data.total_num++;
		}
		report_num++;
	}

	if (!data->enable_reporting || !report_num)
		goto out;

	for (i = 0; i < data->pdata->numtouch; i++) {
		if (data->ts_data.curr_data[i].status == FINGER_INACTIVE || data->ts_data.curr_data[i].skip_report) {
			continue;
		}

		if (data->ts_data.curr_data[i].status == FINGER_RELEASED) {
			input_mt_slot(data->input_dev, data->ts_data.curr_data[i].id);
			if (data->ts_data.prev_data[i].tool == MT_TOOL_FINGER) {
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
			} else if (data->ts_data.prev_data[i].tool == MT_TOOL_PALM) {
				input_mt_report_slot_state(data->input_dev, MT_TOOL_PALM, 0);
			} else if (data->ts_data.prev_data[i].tool == MT_TOOL_PEN) {
				input_mt_report_slot_state(data->input_dev, MT_TOOL_PEN, 0);
			} else {
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
			}
		} else {
			input_mt_slot(data->input_dev, data->ts_data.curr_data[i].id);
			input_mt_report_slot_state(data->input_dev,
				data->ts_data.curr_data[i].tool, 1);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID,
				data->ts_data.curr_data[i].id);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				data->ts_data.curr_data[i].x_position);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				data->ts_data.curr_data[i].y_position);

			/* Report Palm event : if touch event is palm, report pressure 255 to framework */
			if(data->ts_data.curr_data[i].is_palm)
				input_report_abs(data->input_dev, ABS_MT_PRESSURE, 255);
			else if(data->ts_data.curr_data[i].pressure == 255)
				input_report_abs(data->input_dev, ABS_MT_PRESSURE, 254);
			else
				input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					data->ts_data.curr_data[i].pressure);
			/* Report Palm event end */

			input_report_abs(data->input_dev, ABS_MT_ORIENTATION,
							data->ts_data.curr_data[i].orientation);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR,
				data->ts_data.curr_data[i].touch_major);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MINOR,
				data->ts_data.curr_data[i].touch_minor);

		//                                                                   
			#if TOUCHEVENTFILTER
			dev_dbg(dev,
				"report_data[%d] : x: %d y: %d, z: %d, M: %d, m: %d, orient: %d)\n",
					data->ts_data.curr_data[i].id,
					data->ts_data.curr_data[i].x_position,
					data->ts_data.curr_data[i].y_position,
					data->ts_data.curr_data[i].pressure,
					data->ts_data.curr_data[i].touch_major,
					data->ts_data.curr_data[i].touch_minor,
					data->ts_data.curr_data[i].orientation
			);
			#else
			dev_dbg(dev, "report_data[%d] : (x %d, y %d, presure %d, touch_major %d, orient %d)\n",
					i,
					data->ts_data.curr_data[i].x_position,
					data->ts_data.curr_data[i].y_position,
					data->ts_data.curr_data[i].pressure,
					data->ts_data.curr_data[i].touch_major,
					data->ts_data.curr_data[i].orientation
			);
			#endif
			//                                                                   
		}
#if DEBUG_ABS
		if (data->ts_data.curr_data[i].status == FINGER_PRESSED) {
			tool_type = get_tool_type(data, data->ts_data.curr_data[i]);
			dev_info(dev, "%d %s Pressed <%d> : x[%4d] y[%4d], z[%3d]\n",
					data->ts_data.total_num, tool_type,
					data->ts_data.curr_data[i].id,
					data->ts_data.curr_data[i].x_position,
					data->ts_data.curr_data[i].y_position,
					data->ts_data.curr_data[i].pressure);
		} else if (data->ts_data.curr_data[i].status == FINGER_RELEASED) {
			tool_type = get_tool_type(data, data->ts_data.prev_data[i]);
			dev_info(dev, "%s Released <%d> <%d P>\n",
					tool_type,
					data->ts_data.curr_data[i].id, data->ts_data.total_num);
		}
#endif
	}

	if(data->ts_data.total_num < data->ts_data.prev_total_num)
		dev_dbg(dev, "Total_num(move+press)= %d\n",data->ts_data.total_num);
	if (data->ts_data.total_num) {
		data->ts_data.prev_total_num = data->ts_data.total_num;
		memcpy(data->ts_data.prev_data, data->ts_data.curr_data, sizeof(data->ts_data.curr_data));
	} else{
		data->ts_data.prev_total_num = 0;
		memset(data->ts_data.prev_data, 0, sizeof(data->ts_data.prev_data));
	}
	memset(data->ts_data.curr_data, 0, sizeof(data->ts_data.curr_data));
#endif


end:
	if (data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

#ifdef CUST_B_TOUCH
out:
	return;
#endif
out_ignore_interrupt:
	return;
err_out_critical:
	safety_reset(data);
	return;
}

static int mxt_process_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int count, read;
	u8 tries = 2;

	count = data->max_reportid;

	/* Read messages until we force an invalid */
	do {
		read = mxt_read_and_process_messages(data, count);
		if (read < count)
			return 0;
	} while (--tries);

	if (data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	dev_err(dev, "CHG pin isn't cleared\n");
	return -EBUSY;
}

static irqreturn_t mxt_process_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;

	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_and_process_messages(data, count + 1);
	if (total_handled < 0)
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	else if (total_handled <= count)
		goto update_count;

	/* read two at a time until an invalid message or else we reach
	 * reportid limit */
	do {
		num_handled = mxt_read_and_process_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;

	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;

	if (data->in_bootloader) {
		/* bootloader state transition completion */
		complete(&data->bl_completion);
		return IRQ_HANDLED;
	}

	if (!data->object_table)
		return IRQ_NONE;

	if (data->T44_address) {
#ifdef I2C_SUSPEND_WORKAROUND
		queue_delayed_work(touch_wq, &data->check_suspended_work, 0);
#else
		queue_work(touch_wq, &data->work);
#endif
		return IRQ_HANDLED;
	} else {
		return mxt_process_messages(data);
	}
}

#ifdef I2C_SUSPEND_WORKAROUND
static void touch_check_suspended_worker(struct work_struct *check_suspended_work)
{
	struct mxt_data *data =
		container_of(to_delayed_work(check_suspended_work), struct mxt_data, check_suspended_work);

	if (i2c_suspended && data->suspended){
		dev_err(&data->client->dev, "lge_touch touch suspended. try i2c operation after 10ms.\n");
		queue_delayed_work(touch_wq, &data->check_suspended_work, msecs_to_jiffies(10));
		return;
	}else{
		dev_dbg(&data->client->dev, "lge_touch touch resume. do touch work.\n");
		queue_work(touch_wq, &data->work);
		return;
	}
}
#endif
static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset,
			  u8 value, bool wait)
{
	u16 reg;
	u8 command_register = 0;
	int timeout_counter = 0;
	int ret;

	reg = data->T6_address + cmd_offset;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(20);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while ((command_register != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static int mxt_t25_command(struct mxt_data *data, u8 value, bool wait)
{
	u16 reg;
	u8 command_register = 0;
	int timeout_counter = 0;
	int ret;

	if(!selftest_enable)
		return 0;

	reg = data->T25_address + 1 ;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret){
		dev_err(&data->client->dev, "Write Self Test Command fail!\n");
		return ret;
	}

	if (!wait)
		return 0;

	do {
		msleep(20);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while ((command_register != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static int mxt_soft_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;

	dev_info(dev, "Resetting chip\n");

	INIT_COMPLETION(data->reset_completion);

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret)
		return ret;

#ifdef CUST_B_TOUCH
	msleep(MXT_RESET_TIME);
	cancel_work_sync(&data->work);
#ifdef I2C_SUSPEND_WORKAROUND
	cancel_delayed_work_sync(&data->check_suspended_work);
#endif
	cancel_delayed_work_sync(&data->work_ime_drumming);
#else
	ret = mxt_wait_for_completion(data, &data->reset_completion,
				      MXT_RESET_TIMEOUT);
	if (ret)
		return ret;
#endif
	return 0;
}
#ifdef FIRMUP_ON_PROBE
static int mxt_backup(struct mxt_data *data)
{
    struct device *dev = &data->client->dev;
	int ret = 0;

	dev_info(dev, "Backup configuration data\n");

	ret = mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE, false);
	if (ret)
		return ret;

	msleep(MXT_BACKUP_TIME);
	return 0;
}

static void mxt_update_crc(struct mxt_data *data, u8 cmd, u8 value)
{
	/* on failure, CRC is set to 0 and config will always be downloaded */
	data->config_crc = 0;
	INIT_COMPLETION(data->crc_completion);

	mxt_t6_command(data, cmd, value, true);
	/* Wait for crc message. On failure, CRC is set to 0 and config will
	 * always be downloaded */
#ifdef CUST_B_TOUCH
	msleep(MXT_CRC_TIMEOUT);
#else
	mxt_wait_for_completion(data, &data->crc_completion, MXT_CRC_TIMEOUT);
#endif
}
#endif

static void mxt_calc_crc24(u32 *crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u32 data_word;

	data_word = (secondbyte << 8) | firstbyte;
	result = ((*crc << 1) ^ data_word);

	if (result & 0x1000000)
		result ^= crcpoly;

	*crc = result;
}

static u32 mxt_calculate_crc(u8 *base, off_t start_off, off_t end_off)
{
	u32 crc = 0;
	u8 *ptr = base + start_off;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	while (ptr < last_val) {
		mxt_calc_crc24(&crc, *ptr, *(ptr + 1));
		ptr += 2;
	}

	/* if len is odd, fill the last byte with 0 */
	if (ptr == last_val)
		mxt_calc_crc24(&crc, *ptr, 0);

	/* Mask to 24-bit */
	crc &= 0x00FFFFFF;

	return crc;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data);

static int mxt_check_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error = 0;
	int val = 0;

	if (data->pdata->irqflags & IRQF_TRIGGER_LOW)
		return 0;

	if (data->T18_address) {
		error = __mxt_read_reg(client,
				       data->T18_address + MXT_COMMS_CTRL,
				       1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}

/*
 * mxt_check_reg_init - download configuration to chip
 *
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *   <TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *   <TYPE> - 2-byte object type as hex
 *   <INSTANCE> - 2-byte object instance number as hex
 *   <SIZE> - 2-byte object size as hex
 *   <CONTENTS> - array of <SIZE> 1-byte hex values
 */

static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 0, .idle = 0 };
	struct t7_config knockon = { .active = 64, .idle = 15 };	/* need to sync patch bin */

	if (sleep == MXT_POWER_CFG_DEEPSLEEP){
		data->power_status = MXT_POWER_CFG_DEEPSLEEP;
		new_config = &deepsleep;
	} else if (sleep == MXT_POWER_CFG_KNOCKON) {
		data->power_status = MXT_POWER_CFG_KNOCKON;
		new_config = &knockon;
	} else {
		data->power_status = MXT_POWER_CFG_RUN;
		new_config = &data->t7_cfg;
	}

	error = __mxt_write_reg(data->client, data->T7_address,
			sizeof(data->t7_cfg),
			new_config);
	if (error)
		return error;

	dev_info(dev, "Set T7 ACTV:%d IDLE:%d\n",
		new_config->active, new_config->idle);

	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;
	bool retry = false;

recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;

	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {
			dev_info(dev, "T7 cfg zero, resetting\n");
			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		} else {
		    dev_dbg(dev, "T7 cfg zero after reset, overriding\n");
		    data->t7_cfg.active = 20;
		    data->t7_cfg.idle = 100;
		    return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
	} else {
		dev_info(dev, "Initialised power cfg: ACTV %d, IDLE %d\n",
				data->t7_cfg.active, data->t7_cfg.idle);
		return 0;
	}
}

static int mxt_acquire_irq(struct mxt_data *data)
{
	int error;

	touch_enable_irq(data->irq);

	if (data->use_retrigen_workaround) {
		error = mxt_process_messages_until_invalid(data);
		if (error)
			return error;
	}

	return 0;
}

static void mxt_free_input_device(struct mxt_data *data)
{
	if (data->input_dev) {
		dev_info(&data->client->dev, "mxt_free_input_device\n");
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}
}

static void mxt_free_object_table(struct mxt_data *data)
{
	dev_info(&data->client->dev, "mxt_free_object_table\n");

	if(!update_cfg_force && !update_fw_force && !mxt_mfts){
	kfree(data->raw_info_block);
	data->object_table = NULL;
	data->info = NULL;
	data->raw_info_block = NULL;
	kfree(data->msg_buf);
	data->msg_buf = NULL;

	mxt_free_input_device(data);
	data->enable_reporting = false;
	}

	data->T5_address = 0;
	data->T5_msg_size = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T8_address = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_reportid = 0;
#ifdef MXT_GESTURE_RECOGNIZE
	data->T24_reportid = 0;
	data->T35_reportid = 0;
#endif
	data->T25_reportid = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T46_address = 0;
	data->T47_address = 0;
	data->T48_reportid = 0;
	data->T56_address = 0;
	data->T57_reportid = 0;
	data->T61_reportid = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T65_address = 0;
	data->T72_address = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->T100_address = 0;
	data->max_reportid = 0;
}

static int mxt_parse_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int i;
	u8 reportid;
	u16 end_address;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	data->mem_size = 0;
	for (i = 0; i < data->info->object_num; i++) {
		struct mxt_object *object = data->object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		dev_dbg(&data->client->dev,
			"T%u Start:%u Size:%u Instances:%u Report IDs:%u-%u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

		switch (object->type) {
		case MXT_GEN_MESSAGE_T5:
			if (data->info->family_id == 0x80) {
				/* On mXT224 read and discard unused CRC byte
				 * otherwise DMA reads are misaligned */
				data->T5_msg_size = mxt_obj_size(object);
			} else {
				/* CRC not enabled, so skip last byte */
				data->T5_msg_size = mxt_obj_size(object) - 1;
			}
			data->T5_address = object->start_address;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = min_id;
			data->T6_address = object->start_address;
			break;
		case MXT_GEN_POWER_T7:
			data->T7_address = object->start_address;
			break;
		case MXT_GEN_ACQUIRE_T8:
			data->T8_address = object->start_address;
			break;
		case MXT_TOUCH_MULTI_T9:
			/* Only handle messages from first T9 instance */
			data->T9_reportid_min = min_id;
			data->T9_reportid_max = min_id +
						object->num_report_ids - 1;
			data->num_touchids = object->num_report_ids;
			break;
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_min = min_id;
			data->T15_reportid_max = max_id;
			break;
		case MXT_SPT_COMMSCONFIG_T18:
			data->T18_address = object->start_address;
			break;
#ifdef MXT_GESTURE_RECOGNIZE
		case MXT_PROCI_ONETOUCH_T24:
			data->T24_reportid = min_id;
			break;
		case MXT_SPT_PROTOTYPE_T35:
			data->T35_reportid = min_id;
			break;
#endif
		case MXT_SPT_SELFTEST_T25:
			data->T25_reportid = min_id;
			data->T25_address = object->start_address;
			break;
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_reportid_min = min_id;
			data->T42_reportid_max = max_id;
			break;
		case MXT_SPT_CTECONFIG_T46:
			data->T46_address = object->start_address;
			break;
		case MXT_PROCI_STYLUS_T47:
			data->T47_address = object->start_address;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_reportid = min_id;
			break;
		case MXT_SPT_NOISESUPPRESSION_T48:
			data->T48_reportid = min_id;
			break;
		case MXT_PROCI_SHIELDLESS_T56:
			data->T56_address = object->start_address;
			break;
		case MXT_PROCI_EXTRATOUCHSCREENDATA_T57:
			data->T57_reportid = min_id;
			break;
		case MXT_SPT_TIMER_T61:
			data->T61_reportid = min_id;
			break;
		case MXT_PROCI_ACTIVE_STYLUS_T63:
			/* Only handle messages from first T63 instance */
			data->T63_reportid_min = min_id;
			data->T63_reportid_max = min_id;
			data->num_stylusids = 1;
			break;
		case MXT_PROCI_LENSBENDING_T65:
			data->T65_address = object->start_address;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T72:
			data->T72_address = object->start_address;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T100:
			data->T100_reportid_min = min_id;
			data->T100_reportid_max = max_id;
			/* first two report IDs reserved */
			data->num_touchids = object->num_report_ids - 2;
			data->T100_address = object->start_address;
			break;
		}

		end_address = object->start_address
			+ mxt_obj_size(object) * mxt_obj_instances(object) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T5 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(&client->dev, "Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "Failed to allocate message buffer\n");
		return -ENOMEM;
	}

	return 0;
}

static int mxt_read_info_block(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	size_t size;
	void *buf;
	struct mxt_info *info;
	u32 calculated_crc;
	u8 *crc_ptr;
	dev_info(&data->client->dev, "mxt_read_info_block\n");

	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, 0, size, buf);
	if (error)
		goto err_free_mem;

	/* Resize buffer to give space for rest of info block */
	info = (struct mxt_info *)buf;
	size = size + (info->object_num * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = krealloc(buf, size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Read rest of info block */
	error = __mxt_read_reg(client, MXT_OBJECT_START,
			       size - MXT_OBJECT_START,
			       buf + MXT_OBJECT_START);
	if (error)
		goto err_free_mem;

	/* Extract & calculate checksum */
	crc_ptr = buf + size - MXT_INFO_CHECKSUM_SIZE;
	data->info_crc = crc_ptr[0] | (crc_ptr[1] << 8) | (crc_ptr[2] << 16);

	calculated_crc = mxt_calculate_crc(buf, 0,
					   size - MXT_INFO_CHECKSUM_SIZE);

	/* CRC mismatch can be caused by data corruption due to I2C comms
	 * issue or else device is not using Object Based Protocol */
	if (data->info_crc != calculated_crc) {
		dev_err(&client->dev,
			"Info Block CRC error calculated=0x%06X read=0x%06X\n",
			data->info_crc, calculated_crc);
		return -EIO;
	}

	/* Save pointers in device data structure */
	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;
	data->object_table = (struct mxt_object *)(buf + MXT_OBJECT_START);

	dev_info(&client->dev,
		 "Family: %02X Variant: %02X Firmware V%u.%u.%02X Objects: %u\n",
		 data->info->family_id, data->info->variant_id, data->info->version >> 4,
		 data->info->version & 0xf, data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
		mxt_free_object_table(data);
		return error;
	}

#ifdef TSP_PATCH
	data->reportids = kcalloc(data->max_reportid + 1,
			sizeof(struct mxt_reportid),
			GFP_KERNEL);
	if (!data->reportids) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Make report id table */
	mxt_make_reportid_table(data);
#endif

	return 0;

err_free_mem:
	kfree(buf);
	return error;
}

static int mxt_read_t9_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct t9_range range;
	unsigned char orient = 0;
	struct mxt_object *object;
	memset(&range, 0, sizeof(range));

	object = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T9_RANGE,
			       sizeof(range), &range);
	if (error)
		return error;

	le16_to_cpus(range.x);
	le16_to_cpus(range.y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T9_ORIENT,
				1, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (range.x == 0)
		range.x = 2159;

	if (range.y == 0)
		range.y = 3839;

	if (orient & MXT_T9_ORIENT_SWITCH) {
		data->max_x = range.y;
		data->max_y = range.x;
	} else {
		data->max_x = range.x;
		data->max_y = range.y;
	}

	dev_info(&client->dev,
		 "Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static void mxt_regulator_enable(struct mxt_data *data)
{
	int error = 0;
	dev_info(&data->client->dev, "regulator enable\n");

	data->power_status = MXT_POWER_ON;

	gpio_set_value(data->pdata->gpio_reset, 0);

	error = regulator_enable(data->vdd_ana);
	if (error < 0) {
		dev_err(&data->client->dev, "vdd_ana regulator enable fail\n");
		return;
	}
	error = regulator_enable(data->vcc_i2c);
	if (error < 0) {
		dev_err(&data->client->dev, "vcc_i2c regulator enable fail\n");
		return;
	}
	error = regulator_enable(data->vcc_dig);
	if (error < 0) {
		dev_err(&data->client->dev, "vcc_dig regulator enable fail\n");
		return;
	}
	msleep(MXT_REGULATOR_DELAY);

	INIT_COMPLETION(data->bl_completion);
	gpio_set_value(data->pdata->gpio_reset, 1);
#ifdef CUST_B_TOUCH
	msleep(MXT_POWERON_DELAY);
#else
	mxt_wait_for_completion(data, &data->bl_completion, MXT_POWERON_DELAY);
#endif
}

static void mxt_regulator_disable(struct mxt_data *data)
{
	int error = 0;
	dev_info(&data->client->dev, "regulator disable\n");

	data->power_status = MXT_POWER_OFF;

	error = regulator_disable(data->vdd_ana);
	if (error < 0) {
		dev_err(&data->client->dev, "vdd_ana regulator disable fail\n");
		return;
	}
	error = regulator_disable(data->vcc_i2c);
	if (error < 0) {
		dev_err(&data->client->dev, "vcc_i2c regulator disable fail\n");
		return;
	}
	error = regulator_disable(data->vcc_dig);
	if (error < 0) {
		dev_err(&data->client->dev, "vcc_dig regulator disable fail\n");
		return;
	}
}

static void mxt_probe_regulators(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	/* According to maXTouch power sequencing specification, RESET line
	 * must be kept low until some time after regulators come up to
	 * voltage */
	if (!data->pdata->gpio_reset) {
		dev_warn(dev, "Must have reset GPIO to use regulator support\n");
		goto fail;
	}

	data->vdd_ana = regulator_get(dev, "vdd_ana");
	if (IS_ERR(data->vdd_ana)) {
		error = PTR_ERR(data->vdd_ana);
		dev_err(dev, "Error %d getting ana regulator\n", error);
		goto fail;
	}

	data->vcc_i2c = regulator_get(dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		error = PTR_ERR(data->vcc_i2c);
		dev_err(dev, "Error %d getting i2c regulator\n", error);
		goto fail_release;
	}

	data->vcc_dig= regulator_get(dev, "vcc_dig");
	if (IS_ERR(data->vcc_dig)) {
		error = PTR_ERR(data->vcc_dig);
		dev_err(dev, "Error %d getting dig regulator\n", error);
		goto fail_release2;
	}

	error = regulator_set_voltage(data->vdd_ana, 3300000, 3300000);
	if (error < 0) {
		dev_err(dev, "Error %d cannot control ana regulator\n", error);
		goto fail;
	}

	error = regulator_set_voltage(data->vcc_dig, 2850000, 2850000);
	if (error < 0) {
		dev_err(dev, "Error %d cannot control dig regulator\n", error);
		goto fail_release2;
	}

	data->use_regulator = true;
	mxt_regulator_enable(data);

	dev_info(dev, "Initialized regulators\n");
	return;

fail_release2:
	regulator_put(data->vcc_i2c);
fail_release:
	regulator_put(data->vdd_ana);
fail:
	data->vdd_ana = NULL;
	data->vcc_i2c = NULL;
	data->vcc_dig = NULL;
	data->use_regulator = false;
}

static int mxt_read_t100_config(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct mxt_object *object;
	u16 range_x = 0, range_y = 0;
	u8 cfg = 0, tchaux = 0;
	u8 aux = 0;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_XRANGE,
			       sizeof(range_x), &range_x);
	if (error)
		return error;

	le16_to_cpus(range_x);

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_YRANGE,
			       sizeof(range_y), &range_y);
	if (error)
		return error;

	le16_to_cpus(range_y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_CFG1,
				1, &cfg);
	if (error)
		return error;

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_TCHAUX,
				1, &tchaux);
	if (error)
		return error;

	/* Handle default values */
	if (range_x == 0)
		range_x = 2159;

	/* Handle default values */
	if (range_x == 0)
		range_x = 2159;

	if (range_y == 0)
		range_y = 3839;

	if (cfg & MXT_T100_CFG_SWITCHXY) {
		data->max_x = range_y;
		data->max_y = range_x;
	} else {
		data->max_x = range_x;
		data->max_y = range_y;
	}

	/* allocate aux bytes */
	aux = 6;

	if (tchaux & MXT_T100_TCHAUX_VECT)
		data->t100_aux_vect = aux++;

	if (tchaux & MXT_T100_TCHAUX_AMPL)
		data->t100_aux_ampl = aux++;

	if (tchaux & MXT_T100_TCHAUX_AREA)
		data->t100_aux_area = aux++;

	dev_info(&client->dev,
		 "T100 Touchscreen size X%u Y%u amp%u area%u vec%u\n",
	 		data->max_x, data->max_y, data->t100_aux_ampl,
	 		data->t100_aux_area, data->t100_aux_vect);

	return 0;
}

static int mxt_input_open(struct input_dev *dev);
static void mxt_input_close(struct input_dev *dev);

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;

	error = mxt_read_t100_config(data);
	if (error)
		dev_warn(dev, "Failed to initialize T100 configuration\n");

	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	/* Auto Test interface */
	touch_test_dev = data;

	input_dev->name = "touch_dev"; /*must sync to idc file name*/
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	/* input_dev->dev.parent = &data->client->dev;	remove this line for sysfs path (virtual) */
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	set_bit(EV_ABS, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	/* For multi touch */
	num_mt_slots = data->num_touchids;
	error = input_mt_init_slots(input_dev, num_mt_slots);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,
			     0, data->pdata->numtouch, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
			     0, 255, 0, 0);

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data);
static int mxt_configure_objects(struct mxt_data *data);
static ssize_t mxt_firmware_update(struct mxt_data *data);

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	u8 retry_count = 0;
	dev_info(&data->client->dev, "mxt_initialize\n");

retry_probe:
	error = mxt_read_info_block(data);
	if (error) {
		error = mxt_probe_bootloader(data, retry_count);
		if (error) {
			if (++retry_count > 11)
				/* Chip is not in appmode or bootloader mode */
				return error;

			goto retry_probe;
		} else {
#ifdef FIRMUP_ON_PROBE
			dev_err(&client->dev, "IC stay in bootloader mode\n");
			data->in_bootloader = true;
			return 0;
#else
			if (++retry_count > 10) {
				dev_err(&client->dev,
						"Could not recover device from "
						"bootloader mode\n");
				/* this is not an error state, we can reflash
				 * from here */
				data->in_bootloader = true;
				return 0;
			}

			/* Attempt to exit bootloader into app mode */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FW_RESET_TIME);
			goto retry_probe;
#endif
		}
	}

	error = mxt_check_retrigen(data);
	if (error)
		return error;

	return 0;
}

static int mxt_rest_init(struct mxt_data *data)
{
	int error;

	error = mxt_acquire_irq(data);
	if (error)
		return error;

	error = mxt_configure_objects(data);
	if (error)
		return error;

#ifdef TSP_PATCH
	if(sizeof(patch_bin))
		data->patch.patch = patch_bin;
	else
		dev_info(&data->client->dev, "No patch file %p\n", data->patch.patch);

	if (data->patch.patch){
		dev_info(&data->client->dev, "mxt_patch_init on probe, size:%d patch:%p\n", sizeof(patch_bin), data->patch.patch);
		error = mxt_patch_init(data, data->patch.patch);
		if (error) {
			dev_err(&data->client->dev, "Failed to mxt_patch_init\n");
		}
		dev_info(&data->client->dev, " patch date = %d\n", data->patch.date);
	}
	else
		dev_info(&data->client->dev, "No patch on probe, size:%d\n", sizeof(patch_bin));
#endif

	return 0;
}

static int mxt_configure_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

#ifdef FIRMUP_ON_PROBE
	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	dev_dbg(&data->client->dev, "current config checksum is 0x%06X\n",data->config_crc);
	if (data->config_crc == 0 || data->config_crc != MXT_LATEST_CONFIG_CRC || update_cfg_force) {
		/* Disable T70 Dynamic configuration */
		mxt_t6_command(data, MXT_COMMAND_BACKUPNV, MXT_STOP_DYNAMIC_CONFIG, false);

		error = mxt_write_configuration(data);
		if (error) {
			dev_err(&data->client->dev, "Failed to write config\n");
			return error;
		}

		error = mxt_backup(data);
		if (error) {
			dev_err(&data->client->dev, "Failed to backup NV data\n");
			return error;
		}

		error = mxt_soft_reset(data);
		if (error) {
			dev_err(&data->client->dev, "Failed to reset IC\n");
			return error;
		}

		error = mxt_init_t7_power_cfg(data);
		if (error) {
			dev_err(&client->dev, "Failed to initialize power cfg after write configuration\n");
			return error;
		}
	}
	error = mxt_check_retrigen(data);
	if (error)
		return error;
#endif

	if (data->T9_reportid_min) {
		error = mxt_initialize_t9_input_device(data);
		if (error)
			return error;
	} else if (data->T100_reportid_min) {
		error = mxt_initialize_t100_input_device(data);
		if (error)
			return error;
	} else {
		dev_warn(&client->dev, "No touch object detected\n");
	}
	return 0;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct mxt_data *data, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 data->info->version >> 4, data->info->version & 0xf,
			 data->info->build);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct mxt_data *data, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
			data->info->family_id, data->info->variant_id);
}

/* Configuration Checksum */
static ssize_t mxt_mxt_info_show(struct mxt_data *data, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "\n====== Touch IC Info ======\n");
	ret += sprintf(buf+ret, "FW version           = %u.%u.%02X\n",
					 data->info->version >> 4, data->info->version & 0xf,
					 data->info->build);
	ret += sprintf(buf+ret, "config checksum      = 0x%06X\n", data->config_crc);
	ret += sprintf(buf+ret, "Object Num           = %d\n", data->info->object_num);
	ret += sprintf(buf+ret, "Faily Id             = %d\n", data->info->family_id);
	ret += sprintf(buf+ret, "Variant              = %d\n", data->info->variant_id);
	ret += sprintf(buf+ret, "Version              = %d\n", data->info->version);
	ret += sprintf(buf+ret, "Build                = %d\n", data->info->build);
#ifdef TSP_PATCH
	if (data->patch.patch)
		ret += sprintf(buf+ret, "Patch Date           = %d\n", data->patch.date);
#endif

	return ret;
}

static ssize_t mxt_selftest_store(struct mxt_data *data, const char *buf, size_t count)
{
	int command = 0;
	int ret = 0;

	ret = sscanf(buf, "%u", &command);
	printk(KERN_INFO "\n");

	selftest_enable = true;
	mxt_t25_command(data, command, false);

	return count;
}

static ssize_t mxt_selftest_show(struct mxt_data *data, char *buf)
{
	int ret = 0;
	int test_all_cmd = 254;

	selftest_enable = true;
	selftest_show = true;

	mxt_t25_command(data, test_all_cmd, false);
	msleep(MXT_SELFTEST_TIME);

	ret = sprintf(buf, "====== MXT Self Test Info ======\n");
	if(data->self_test_status[0] == 0){
		ret += sprintf(buf+ret, "Need more time. Try Again.\n");
		return ret;
	}

	if(data->self_test_status[0] == 0xFD){
		ret += sprintf(buf+ret, "Invalid Test Code. Try Again.");
	}else if(data->self_test_status[0] == 0xFC){
		ret += sprintf(buf+ret, "The test could not be completed due to an unrelated fault. Try again.");
	}else{
		ret += sprintf(buf+ret, "All Test Result: %s", (data->self_test_status[0] == 0xFE) ? "Pass\n" : "Fail\n");
		ret += sprintf(buf+ret, "AVdd power Test Result: %s", (data->self_test_status[0] != 0x01) ? "Pass\n" : "Fail\n");

		ret += sprintf(buf+ret, "Pin Falut Test Result: %s", (data->self_test_status[0] != 0x12) ? "Pass\n" : "Fail\n");
		if(data->self_test_status[0] == 0x12)
			ret += sprintf(buf+ret, "# Fail # seq_num(%u) x_pin(%u) y_pin(%u)",
										data->self_test_status[1], data->self_test_status[2], data->self_test_status[3]);

		ret += sprintf(buf+ret, "Signal Limit Test: %s", (data->self_test_status[0] != 0x17) ? "Pass\n" : "Fail\n");
		if(data->self_test_status[0] == 0x17)
			ret += sprintf(buf+ret, "# Fail # type_num(%u) type_instance(%u)", data->self_test_status[1], data->self_test_status[2]);
	}

	memset(&data->self_test_status, 0, sizeof(data->self_test_status));
	selftest_show = false;
	return ret;
}

static ssize_t mxt_show_instance(char *buf, int count,
				 struct mxt_object *object, int instance,
				 const u8 *val)
{
	int i;

	if (mxt_obj_instances(object) > 1)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "Instance %u\n", instance);

	for (i = 0; i < mxt_obj_size(object); i++)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\t[%2u]: %02x (%d)\n", i, val[i], val[i]);
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t mxt_object_show(struct mxt_data *data, char *buf)
{
	struct mxt_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 *obuf;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	error = 0;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"T%u:\n", object->type);

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				goto done;

			count = mxt_show_instance(buf, count, object, j, obuf);
		}
	}

done:
	kfree(obuf);
	return error ?: count;
}

/* mxt_object_control
 * Usage
 * - read : echo read object 0 0 > object_ctrl
 * - wirte : echo write object address_offset value > object_ctrl
 */
static ssize_t mxt_object_control(struct mxt_data *data, const char *buf, size_t count)
{
	struct mxt_object *object;
	unsigned char command[6];
	int type = 0;
	int addr_offset = 0;
	int value = 0;
	int error = 0;
	int i = 0,j = 0;
	u8 *obuf;

	sscanf(buf, "%s %d %d %d", command, &type, &addr_offset, &value);

	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	if(type == 25)
		selftest_enable = true;

	object = mxt_get_object(data, type);
	if (!object) {
        dev_err(&data->client->dev,
			"error Cannot get object_type T%d\n", type);
        return -EINVAL;
    }

	if ((mxt_obj_size(object) == 0) || (object->start_address == 0)) {
		dev_err(&data->client->dev,
			"error object_type T%d\n", type);
		return -ENODEV;
	}

	if (!strncmp(command, "read", 4)){	/*read*/
		dev_info(&data->client->dev, "Object Read T%d: start_addr=%d, size=%d * instance=%d\n",
		type, object->start_address, mxt_obj_size(object), mxt_obj_instances(object));

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				dev_err(&data->client->dev, "Object Read Fail\n");
		}

		for (i = 0; i < mxt_obj_size(object)*mxt_obj_instances(object); i++)
			dev_info(&data->client->dev, "T%d [%d] %d[0x%x]\n", type, i, obuf[i], obuf[i]);

	}else if (!strncmp(command, "write", 4)){	/*write*/
		dev_info(&data->client->dev, "Object Write T%d: start_addr=%d, size=%d * instance=%d\n",
		type, object->start_address, mxt_obj_size(object), mxt_obj_instances(object));

		error = mxt_write_reg(data->client, object->start_address+addr_offset, value);
		if (error)
			dev_err(&data->client->dev, "Object Write Fail\n");

		dev_info(&data->client->dev, "Object Write Success. Execute Read Object and Check Value.\n");
	}else{
		dev_err(&data->client->dev, "Command Fail. Usage: echo [read | write] object cmd_field value > object_ctrl\n");
	}
	return count;
}

int mxt_read_object(struct mxt_data *data, u8 type, u8 offset, u8 *value)
{
	struct mxt_object *object;
	int error = 0;

	object = mxt_get_object(data, type);
	if (!object) {
		dev_err(&data->client->dev,
			"error Cannot get object_type T%d\n", type);
		return -EINVAL;
	}

	if ((mxt_obj_size(object) == 0) || (object->start_address == 0)) {
		dev_err(&data->client->dev,
			"error object_type T%d\n", type);
		return -ENODEV;
	}
	error = __mxt_read_reg(data->client, object->start_address+offset, 1, value);
	if (error)
		dev_err(&data->client->dev, "Object Read Fail\n");

	return error;
}

int mxt_write_object(struct mxt_data *data, u8 type, u8 offset, int value)
{
	struct mxt_object *object;
	int error = 0;

	object = mxt_get_object(data, type);
	if (!object) {
		dev_err(&data->client->dev,
			"error Cannot get object_type T%d\n", type);
		return -EINVAL;
	}

	if ((mxt_obj_size(object) == 0) || (object->start_address == 0)) {
		dev_err(&data->client->dev,
			"error object_type T%d\n", type);
		return -ENODEV;
	}

	error = mxt_write_reg(data->client, object->start_address+offset, value);
	if (error)
		dev_err(&data->client->dev, "Object Write Fail\n");

	return 0;
}

static int mxt_check_firmware_format(struct device *dev,
				     const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/* To convert file try
	 * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw */
	dev_err(dev, "Aborting: firmware file must be in binary format\n");

	return -1;
}

static int mxt_load_fw(struct device *dev, bool from_header)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
#ifdef FIRMUP_ON_PROBE
	struct firmware *fw_from_header = NULL;
#endif
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;

#ifdef FIRMUP_ON_PROBE
	if (from_header) {
		fw_from_header = kzalloc(sizeof(struct firmware), GFP_KERNEL);

		fw_from_header->data = latest_firmware;
		fw_from_header->size = sizeof(latest_firmware);
		fw = fw_from_header;
	} else {
		ret = request_firmware(&fw, data->fw_name, dev);
		if (ret) {
			dev_err(dev, "Unable to open firmware %s  ret %d\n", data->fw_name, ret);
			return ret;
		}
	}
#else
	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s  ret %d\n", data->fw_name, ret);
		return ret;
	}
#endif

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret)
		goto release_firmware;

	if (data->suspended) {
		if (data->use_regulator)
			mxt_regulator_enable(data);

		touch_enable_irq(data->irq);
		data->suspended = false;
	}

	if (!data->in_bootloader) {
		/* Change to the bootloader mode */
		data->in_bootloader = true;

		ret = mxt_t6_command(data, MXT_COMMAND_RESET,
				     MXT_BOOT_VALUE, false);
		if (ret)
			goto release_firmware;

		msleep(MXT_RESET_TIME);

		/* At this stage, do not need to scan since we know
		 * family ID */
		ret = mxt_lookup_bootloader_address(data, 0);
		if (ret)
			goto release_firmware;
	}

	mxt_free_object_table(data);
	INIT_COMPLETION(data->bl_completion);

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret)
			goto disable_irq;
	} else {
		dev_info(dev, "Unlocking bootloader\n");

		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		if (ret)
			goto disable_irq;
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret)
			goto disable_irq;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, fw->data + pos, frame_size);
		if (ret)
			goto disable_irq;

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto disable_irq;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}

		if (frame % 50 == 0)
			dev_info(dev, "Sent %d frames, %d/%zd bytes\n",
				 frame, pos, fw->size);
	}

	/* Wait for flash */
	ret = mxt_wait_for_completion(data, &data->bl_completion,
				      MXT_FW_RESET_TIME);
	if (ret)
		goto disable_irq;
	dev_info(dev, "Sent %d frames, %zd bytes\n", frame, pos);

#if 0	/*To avoid reset timeout*/
	/* Wait for device to reset */
	mxt_wait_for_completion(data, &data->bl_completion, MXT_RESET_TIMEOUT);
#endif
	data->in_bootloader = false;

disable_irq:
	touch_disable_irq(data->irq);
release_firmware:
#ifdef FIRMUP_ON_PROBE
	if (from_header)
		kfree(fw_from_header);
	else
#endif
	release_firmware(fw);
	return ret;
}

static int mxt_update_file_name(struct device *dev, char **file_name,
				const char *buf, size_t count)
{
	char *file_name_tmp;

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	file_name_tmp = krealloc(*file_name, count + 1, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	return 0;
}

#ifdef FIRMUP_ON_PROBE
/*
*   Update firmware from header file
*/
static ssize_t mxt_firmware_update(struct mxt_data *data)
{
    struct device *dev = &data->client->dev;
    int error;

    error = mxt_load_fw(dev, true);
    if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
    } else {
		dev_info(dev, "The firmware update succeeded\n");
		if(!update_fw_force && !mxt_mfts)
			data->suspended = false;
		error = mxt_read_info_block(data);
    }

    return error;
}
#endif

static ssize_t mxt_update_fw_store(struct mxt_data *data, const char *buf, size_t count)
{
	int error = 0;
	int value = 0;

	sscanf(buf, "%d", &value);
	update_fw_force = true;

	if(value && !(data->suspended)){
		dev_info(&data->client->dev, "Execute firmware update func\n");
		error = mxt_initialize(data);
		if (error) {
		dev_err(&data->client->dev, "Failed to initialize mxt\n");
		data->in_bootloader = false;
			goto out;
		}

		error = mxt_firmware_update(data);
		if (error) {
		dev_err(&data->client->dev, "Failed to update firmware\n");
		data->in_bootloader = false;
			goto out;
		}

		error = mxt_rest_init(data);
		if (error) {
		dev_err(&data->client->dev, "Failed to rest init\n");
			goto out;
		}
	}else
		dev_info(&data->client->dev, "Can't Execute firmware update func\n");

out:
	update_fw_force = false;
	return count;
}

static ssize_t mxt_update_cfg_store(struct mxt_data *data, const char *buf, size_t count)
{
	int ret = 0;
	int value = 0;

	sscanf(buf, "%d", &value);
	dev_info(&data->client->dev, "Update mxt Configuration.\n");

	if (data->in_bootloader) {
		dev_err(&data->client->dev, "Not in appmode\n");
		return -EINVAL;
	}

	if(value && !(data->suspended)){
		dev_info(&data->client->dev, "Update mxt Configuration Start.\n");
		data->enable_reporting = false;
		update_cfg_force = true;
		mxt_free_object_table(data);
		mxt_read_info_block(data);
		ret = mxt_configure_objects(data);
		if (ret){
			dev_err(&data->client->dev, "Update mxt Configuration Fail!\n");
			goto out;
		}
	}else{
		dev_err(&data->client->dev, "Update isn't excuted!\n");
		ret = count;
		goto out;
	}

	ret = count;
	dev_info(&data->client->dev, "Update mxt Configuration Success.\n");
out:
	data->enable_reporting = true;
	update_cfg_force = false;
	return ret;
}

static ssize_t mxt_debug_enable_show(struct mxt_data *data, char *buf)
{
	int count;
	char c;

	c = data->debug_enabled ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_debug_enable_store(struct mxt_data *data, const char *buf, size_t count)
{
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(&data->client->dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(&data->client->dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	data->mem_size = 32768;

	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}
#ifdef MXT_GESTURE_RECOGNIZE
static void mxt_active_mode_start(struct mxt_data *data);
static void mxt_gesture_mode_start(struct mxt_data *data);
static void mxt_gesture_wakeup_enable(struct mxt_data *data)
{
	if(data->run_wakeup_enable || data->mxt_knock_on_enable){
		dev_err(&data->client->dev, "Already Run Gesture enable Routine.\n");
		return;
	}

	data->run_wakeup_enable = true;

	if(data->power_status == MXT_POWER_CFG_DEEPSLEEP)
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_KNOCKON);
	else if(data->power_status == MXT_POWER_OFF)
		mxt_regulator_enable(data);

	if (touch_enable_irq_wake(data->irq) != 0){
		dev_err(&data->client->dev, "Gesture wakeup irq enable fail.\n");
	}
	touch_enable_irq(data->irq);

	mxt_gesture_mode_start(data);
	data->run_wakeup_enable = false;
}

static void mxt_gesture_wakeup_disable(struct mxt_data *data)
{
	if(data->run_wakeup_disable){
		dev_err(&data->client->dev, "Already Run Gesture disable Routine.\n");
		return;
	}

	data->run_wakeup_disable = true;
	if (touch_disable_irq_wake(data->irq) != 0){
		dev_err(&data->client->dev, "Gesture wakeup irq disable fail.\n");
	}

	if(data->power_status == MXT_POWER_ON || data->power_status == MXT_POWER_CFG_RUN || data->power_status == MXT_POWER_CFG_KNOCKON){
		touch_disable_irq(data->irq);
		cancel_work_sync(&data->work);
#ifdef I2C_SUSPEND_WORKAROUND
		cancel_delayed_work_sync(&data->check_suspended_work);
#endif
		cancel_delayed_work_sync(&data->work_ime_drumming);
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
	}else{
		dev_err(&data->client->dev, "Power off already.\n");
	}

	data->run_wakeup_disable = false;
}

static ssize_t mxt_knock_on_store(struct mxt_data *data, const char *buf, size_t size)
{
	struct input_dev *input_dev = data->input_dev;
	int value;

	dev_info(&data->client->dev, "%s (%s)\n", __func__, data->suspended ? "SLEEP" : "WAKEUP");

	if (data->in_bootloader){
		dev_info(&data->client->dev, "%s : Fw upgrade mode.\n", __func__);
		return size;
	}
	sscanf(buf, "%d", &value);
	mutex_lock(&input_dev->mutex);

	if(value == data->mxt_knock_on_enable || !data->suspended)
		goto no_action;

	if(value == 1){
		mxt_gesture_wakeup_enable(data);
	}else{
		mxt_gesture_wakeup_disable(data);
	}

no_action:
	data->mxt_knock_on_enable = value;
	dev_info(&data->client->dev, "Knock On : %s\n", data->mxt_knock_on_enable ? "Enabled" : "Disabled");
	mutex_unlock(&input_dev->mutex);
	return size;
}

static ssize_t mxt_character_store(struct mxt_data *data, const char *buf, size_t size)
{
	struct input_dev *input_dev = data->input_dev;
	int value;

	dev_info(&data->client->dev, "%s (%s)\n", __func__, data->suspended ? "SLEEP" : "WAKEUP");

	if (data->in_bootloader){
		dev_info(&data->client->dev, "%s : Fw upgrade mode.\n", __func__);
		return size;
	}

	sscanf(buf, "%d", &value);
	mutex_lock(&input_dev->mutex);

	if(value == data->mxt_character_enable || !data->suspended)
		goto no_action;

	if(value == 1){
		mxt_gesture_wakeup_enable(data);
	}else{
		mxt_gesture_wakeup_disable(data);
	}

no_action:
	data->mxt_character_enable = value;
	dev_info(&data->client->dev, "Character Recognize : %s\n", data->mxt_character_enable ? "Enabled" : "Disabled");
	mutex_unlock(&input_dev->mutex);
	return size;
}

static ssize_t mxt_volume_store(struct mxt_data *data, const char *buf, size_t size)
{
	struct input_dev *input_dev = data->input_dev;
	int value;

	dev_info(&data->client->dev, "%s (%s)\n", __func__, data->suspended ? "SLEEP" : "WAKEUP");

	if (data->in_bootloader){
		dev_info(&data->client->dev, "%s : Fw upgrade mode.\n", __func__);
		return size;
	}

	sscanf(buf, "%d", &value);
	mutex_lock(&input_dev->mutex);

	if(value == data->mxt_volume_enable || !data->suspended)
		goto no_action;

	if(value == 1){
		mxt_gesture_wakeup_enable(data);
	}else{
		mxt_gesture_wakeup_disable(data);
	}

no_action:
	data->mxt_volume_enable = value;
	dev_info(&data->client->dev, "Volume Recognize : %s\n", data->mxt_volume_enable ? "Enabled" : "Disabled");
	mutex_unlock(&input_dev->mutex);
	return size;
}
#endif

static ssize_t store_quick_cover_status(struct mxt_data *data, const char *buf, size_t size)
{
	int value;

	dev_info(&data->client->dev, "%s\n", __func__);

	sscanf(buf, "%d", &value);

	if( (value == 1) && (quick_cover_status == 0) )
		quick_cover_status = 1;
	else if( (value == 0) && (quick_cover_status == 1) )
		quick_cover_status = 0;
	else
		return size;

	dev_info(&data->client->dev, "quick cover status = %s\n", (quick_cover_status == 1) ? "QUICK_COVER_ON" : "QUICK_COVER_OFF");
#ifdef TSP_PATCH
	mxt_patch_goto_stage(data, value);
#endif
	return size;
}

#ifdef TSP_PATCH
static ssize_t mxt_load_patch_from_ums(struct mxt_data *data, const char *buf, size_t size)
{
	struct device *dev = &data->client->dev;
	struct file *filp = NULL;
	struct firmware fw;
	mm_segment_t old_fs = {0};
	u8 *patch_data;
	const char *firmware_name = "patch.bin";
	char *fw_path;
	int ret = 0;
	int value;

	sscanf(buf, "%d", &value);
	if(!value){
		dev_err(dev, "Invaild command.\n");
		return size;
	}
	memset(&fw, 0, sizeof(struct firmware));

	fw_path = kzalloc(MXT_MAX_FW_PATH, GFP_KERNEL);
	if (fw_path == NULL) {
		dev_err(dev, "Failed to allocate firmware path.\n");
		return -ENOMEM;
	}

	snprintf(fw_path, MXT_MAX_FW_PATH, "/sdcard/%s", firmware_name);

	old_fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		dev_err(dev, "Could not open firmware: %s,%d\n",
			fw_path, (s32)filp);
		ret = -ENOENT;
		goto err_open;
	}

	fw.size = filp->f_path.dentry->d_inode->i_size;

	patch_data = kzalloc(fw.size, GFP_KERNEL);
	if (!patch_data) {
		dev_err(dev, "Failed to alloc buffer for fw\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	ret = vfs_read(filp, (char __user *)patch_data, fw.size, &filp->f_pos);
	if (ret != fw.size) {
		dev_err(dev, "Failed to read file %s (ret = %d)\n",
			fw_path, ret);
		ret = -EINVAL;
		goto err_alloc;
	}
	fw.data = patch_data;
	data->patch.patch = patch_data;

	dev_info(dev, "%s patch file size:%d\n", __func__, fw.size);
	
	dev_info(dev, "%s ppatch:%p %p\n", __func__, patch_data, data->patch.patch);
	ret = mxt_patch_init(data, data->patch.patch);
	if(ret)
		dev_err(dev, "mxt patch init error.\n");
	ret = 0;

err_alloc:
	filp_close(filp, current->files);
err_open:
	set_fs(old_fs);
	kfree(fw_path);

	return size;
}
#endif

static ssize_t show_mfts_fw_ver(struct mxt_data *data, char *buf)
{
	struct input_dev *input_dev = data->input_dev;

	int ret = 0;
	u8 buf_fw;
	u8 buf_fw1;
	mxt_mfts = true;
	mutex_lock(&input_dev->mutex);

	ret = __mxt_read_reg(data->client, 0x02, 1, &buf_fw);
	if(ret){
		sprintf(buf, "Fail read Version info\n");
		goto out;
	}

	ret = __mxt_read_reg(data->client, 0x03, 1, &buf_fw1);
	if(ret){
		sprintf(buf, "Fail read Buid info\n");
		goto out;
	}

	ret = sprintf(buf, "\n====== MFTS Info ======\n");
	ret += sprintf(buf+ret, "Ver : %d	 Build : %d\n", buf_fw, buf_fw1);

out:
	mutex_unlock(&input_dev->mutex);
	mxt_mfts = false;
	return ret;
}

static ssize_t show_mfts_fw(struct mxt_data *data, char *buf)
{
	int ret = 0;
	mxt_mfts = true;

	dev_info(&data->client->dev, "Execute MTFS firmware update func\n");
	ret = mxt_initialize(data);
	if (ret) {
	dev_err(&data->client->dev, "Failed to initialize mxt\n");
	data->in_bootloader = false;
		goto out;
	}

	ret = mxt_firmware_update(data);
	if (ret) {
	dev_err(&data->client->dev, "Failed to update firmware\n");
	data->in_bootloader = false;
		goto out;
	}

	ret = mxt_rest_init(data);
	if (ret) {
	dev_err(&data->client->dev, "Failed to rest init\n");
		goto out;
	}
	dev_info(&data->client->dev, "MTFS firmware update Success.\n");
out:
	mxt_mfts = false;
	return ret;
}

static void change_ime_drumming_func(struct work_struct *work_ime_drumming)
{
	struct mxt_data *data = container_of(to_delayed_work(work_ime_drumming), struct mxt_data, work_ime_drumming);
	int ret = 0;
	u8 value;

	if(data->power_status == MXT_POWER_OFF)
		return;

	if (ime_drumming_status)
		value = 10;
	else
		value = 45;

	ret = mxt_write_reg(data->client, data->T100_address+43, value);
	if (ret) {
		dev_err(&data->client->dev, "change_ime_drumming_func error. Jump Limit(T100_addr+43)\n");
	}
	dev_info(&data->client->dev, "change_ime_drumming_func. Jump Limit(T100_addr+43), value %d\n", value);
}

static ssize_t show_ime_drumming_status(struct mxt_data *data, char *buf)
{
	int ret = 0;

	ret += sprintf(buf+ret, "%s\n", (ime_drumming_status == 1) ? "IME_ON" : "IME_OFF");
	return ret;
}

static ssize_t store_ime_drumming_status(struct mxt_data *data, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if( (value == 1) && (ime_drumming_status == 0) ) {
		ime_drumming_status = 1;
		queue_delayed_work(touch_wq, &data->work_ime_drumming, msecs_to_jiffies(10));
	}
	else if( (value == 0) && (ime_drumming_status == 1) ) {
		ime_drumming_status = 0;
		queue_delayed_work(touch_wq, &data->work_ime_drumming, msecs_to_jiffies(10));
	}
	else {
		return count;
	}

	dev_info(&data->client->dev, "%s ime status = %s\n", __func__, (ime_drumming_status == 1) ? "IME_ON" : "IME_OFF");
	return count;
}

static ssize_t store_keyguard_info(struct mxt_data *data, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if(value == KEYGUARD_RESERVED) {
		if(ime_drumming_status) {
			queue_delayed_work(touch_wq, &data->work_ime_drumming, msecs_to_jiffies(10));
		}
	}

	dev_info(&data->client->dev, "%s KEYGUARD = %d\n", __func__, value);
	return count;
}

static LGE_TOUCH_ATTR(fw_version, S_IRUGO, mxt_fw_version_show, NULL);
static LGE_TOUCH_ATTR(hw_version, S_IRUGO, mxt_hw_version_show, NULL);
static LGE_TOUCH_ATTR(mxt_info, S_IRUGO, mxt_mxt_info_show, NULL);
static LGE_TOUCH_ATTR(self_test, S_IRUGO | S_IWUSR, mxt_selftest_show, mxt_selftest_store);
static LGE_TOUCH_ATTR(object, S_IRUGO, mxt_object_show, NULL);
static LGE_TOUCH_ATTR(object_ctrl, S_IWUSR, NULL, mxt_object_control);
static LGE_TOUCH_ATTR(update_fw, S_IWUSR, NULL, mxt_update_fw_store);
static LGE_TOUCH_ATTR(update_cfg, S_IWUSR, NULL, mxt_update_cfg_store);
static LGE_TOUCH_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
		   mxt_debug_enable_store);
#ifdef MXT_GESTURE_RECOGNIZE
static LGE_TOUCH_ATTR(touch_gesture,S_IRUGO | S_IWUSR, NULL, mxt_knock_on_store);
static LGE_TOUCH_ATTR(touch_gesture_character,S_IRUGO | S_IWUSR, NULL, mxt_character_store);
static LGE_TOUCH_ATTR(touch_gesture_volume,S_IRUGO | S_IWUSR, NULL, mxt_volume_store);
#endif
static LGE_TOUCH_ATTR(quick_cover_status,S_IRUGO | S_IWUSR, NULL, store_quick_cover_status);
#ifdef TSP_PATCH
static LGE_TOUCH_ATTR(update_patch, S_IWUSR, NULL, mxt_load_patch_from_ums);
#endif
static LGE_TOUCH_ATTR(mfts_fw_ver, S_IRUGO, show_mfts_fw_ver, NULL);
static LGE_TOUCH_ATTR(mfts_fw, S_IRUGO, show_mfts_fw, NULL);
static LGE_TOUCH_ATTR(ime_status, S_IRUGO | S_IWUSR, show_ime_drumming_status, store_ime_drumming_status);
static LGE_TOUCH_ATTR(keyguard, S_IRUGO | S_IWUSR, NULL, store_keyguard_info);

static struct attribute *lge_touch_attribute_list[] = {
	&lge_touch_attr_fw_version.attr,
	&lge_touch_attr_hw_version.attr,
	&lge_touch_attr_mxt_info.attr,
	&lge_touch_attr_self_test.attr,
	&lge_touch_attr_object.attr,
	&lge_touch_attr_object_ctrl.attr,
	&lge_touch_attr_update_fw.attr,
	&lge_touch_attr_update_cfg.attr,
	&lge_touch_attr_debug_enable.attr,
#ifdef MXT_GESTURE_RECOGNIZE
	&lge_touch_attr_touch_gesture.attr,
	&lge_touch_attr_touch_gesture_character.attr,
	&lge_touch_attr_touch_gesture_volume.attr,
#endif
	&lge_touch_attr_quick_cover_status.attr,
#ifdef TSP_PATCH
	&lge_touch_attr_update_patch.attr,
#endif
	&lge_touch_attr_mfts_fw_ver.attr,
	&lge_touch_attr_mfts_fw.attr,
	&lge_touch_attr_ime_status.attr,
	&lge_touch_attr_keyguard.attr,
	NULL
};

/* lge_touch_attr_show / lge_touch_attr_store
 *
 * sysfs bindings for lge_touch
 */
static ssize_t lge_touch_attr_show(struct kobject *lge_touch_kobj, struct attribute *attr,
			     char *buf)
{
	struct mxt_data *ts =
			container_of(lge_touch_kobj, struct mxt_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->show)
		ret = lge_touch_priv->show(ts, buf);

	return ret;
}

static ssize_t lge_touch_attr_store(struct kobject *lge_touch_kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct mxt_data *ts =
			container_of(lge_touch_kobj, struct mxt_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->store)
		ret = lge_touch_priv->store(ts, buf, count);

	return ret;
}

static const struct sysfs_ops lge_touch_sysfs_ops = {
	.show	= lge_touch_attr_show,
	.store	= lge_touch_attr_store,
};

static struct kobj_type lge_touch_kobj_type = {
	.sysfs_ops		= &lge_touch_sysfs_ops,
	.default_attrs 	= lge_touch_attribute_list,
};

static struct sysdev_class lge_touch_sys_class = {
	.name = MXT_DEVICE_NAME,
};

static struct sys_device lge_touch_sys_device = {
	.id		= 0,
	.cls	= &lge_touch_sys_class,
};

static void mxt_reset_slots(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	int id;

	for (id = 0; id < data->pdata->numtouch; id++) {
		input_mt_slot(input_dev, id);
		if (data->ts_data.prev_data[id].tool == MT_TOOL_FINGER) {
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		} else if (data->ts_data.prev_data[id].tool == MT_TOOL_PALM) {
			input_mt_report_slot_state(input_dev, MT_TOOL_PALM, 0);
		} else {
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		}
	}

	mxt_input_sync(input_dev);
	memset(&data->ts_data, 0, sizeof(data->ts_data));

	dev_info(&data->client->dev, "Release all touch event!\n");
}

static void safety_reset(struct mxt_data *data)
{
	touch_disable_irq(data->irq);
	mxt_reset_slots(data);
	mxt_regulator_disable(data);
	mxt_regulator_enable(data);
	touch_enable_irq(data->irq);
	return;
}

#ifdef MXT_GESTURE_RECOGNIZE
/* reduce sleep current */
#ifndef TSP_PATCH
static const u8 t7_gesture_mode[]={64, 15, 5, 64, 1};
static const u8 t7_active_mode[]={48, 255, 20, 67, 0};
static const u8 t8_gesture_mode[] = {255, 0};
static const u8 t8_active_mode[] = {5, 5};
static const u8 t46_gesture_mode[] = {0, 4, 16, 0, 0};
static const u8 t46_active_mode[] = {12, 8, 8, 1, 4};
static const u8 t100_gesture_mode[]={32};
static const u8 t100_active_mode[]={60};
static void mxt_change_cfg(struct mxt_data *data, u16 addr, u16 offset,  u8 value)
{
	int ret = 0;
	ret = mxt_write_reg(data->client, addr+offset, value);
	if (ret) {
		dev_err(&data->client->dev, "chang configuration error (%d)\n",addr+offset);
	}
	dev_dbg(&data->client->dev, "change configuration. addr %d, value %d\n", addr+offset, value);
}
#endif

static void mxt_gesture_mode_start(struct mxt_data *data)
{
#ifdef TSP_PATCH
	if (data->charging_mode) {
		if(data->ta_status != MXT_PATCH_KNOCKON_TA_MODE_EVENT) {
			dev_info(&data->client->dev, " KNOCKON_TA_MODE %d\n", MXT_PATCH_KNOCKON_TA_MODE_EVENT);
			data->ta_status = MXT_PATCH_KNOCKON_TA_MODE_EVENT;
			mxt_patch_test_event(data, MXT_PATCH_KNOCKON_TA_MODE_EVENT);
		}
	} else {
		if(data->ta_status != MXT_PATCH_KNOCKON_BAT_MODE_EVENT) {
			dev_info(&data->client->dev, " KNOCKON_BAT_MODE %d\n", MXT_PATCH_KNOCKON_BAT_MODE_EVENT);
			data->ta_status = MXT_PATCH_KNOCKON_BAT_MODE_EVENT;
			mxt_patch_test_event(data, MXT_PATCH_KNOCKON_BAT_MODE_EVENT);
		}
	}
#else
	/* T100 setting */
	mxt_change_cfg(data, data->T100_address, 0,  t100_ctrl_cfg & MXT_T100_REPORT_DIS_MASK);
	mxt_change_cfg(data, data->T100_address, 30,  t100_gesture_mode[0]);
	/* T47 setting */
	mxt_change_cfg(data, data->T47_address, 0,  t47_ctrl_cfg & MXT_DISABLE_MASK);
	/* T65 setting */
	mxt_change_cfg(data, data->T65_address, 0,	t65_ctrl_cfg & MXT_DISABLE_MASK);
	/* T72 setting */
	mxt_change_cfg(data, data->T72_address, 0,	t72_ctrl_cfg & MXT_DISABLE_MASK);
	/* T7 setting */
	mxt_change_cfg(data, data->T7_address, 0,  t7_gesture_mode[0]);
	mxt_change_cfg(data, data->T7_address, 1,  t7_gesture_mode[1]);
	mxt_change_cfg(data, data->T7_address, 2,  t7_gesture_mode[2]);
	mxt_change_cfg(data, data->T7_address, 3,  t7_gesture_mode[3]);
	mxt_change_cfg(data, data->T7_address, 4,  t7_gesture_mode[4]);
	/* T8 setting */
	mxt_change_cfg(data, data->T8_address, 2,  t8_gesture_mode[0]);
	mxt_change_cfg(data, data->T8_address, 4,  t8_gesture_mode[1]);
	data->anti->autocal = false;
	/* T46 setting */
	mxt_change_cfg(data, data->T46_address, 0,  t46_gesture_mode[0]);
	mxt_change_cfg(data, data->T46_address, 2,  t46_gesture_mode[1]);
	mxt_change_cfg(data, data->T46_address, 3,  t46_gesture_mode[2]);
	mxt_change_cfg(data, data->T46_address, 5,  t46_gesture_mode[3]);
	mxt_change_cfg(data, data->T46_address, 6,  t46_gesture_mode[4]);
#endif

	dev_info(&data->client->dev, "Change to Knock on Mode\n");
}

static void mxt_active_mode_start(struct mxt_data *data)
{
#ifdef TSP_PATCH
	if (data->charging_mode) {
		if(data->ta_status != MXT_PATCH_WAKEUP_TA_MODE_EVENT){
			dev_info(&data->client->dev, " WAKEUP_TA_MODE %d\n", MXT_PATCH_WAKEUP_TA_MODE_EVENT);
			data->ta_status = MXT_PATCH_WAKEUP_TA_MODE_EVENT;
			mxt_patch_test_event(data, MXT_PATCH_WAKEUP_TA_MODE_EVENT);
		}
	} else {
		if(data->ta_status != MXT_PATCH_WAKEUP_BAT_MODE_EVENT){
			dev_info(&data->client->dev, " WAKEUP_BAT_MODE %d\n", MXT_PATCH_WAKEUP_BAT_MODE_EVENT);
			data->ta_status = MXT_PATCH_WAKEUP_BAT_MODE_EVENT;
			mxt_patch_test_event(data, MXT_PATCH_WAKEUP_BAT_MODE_EVENT);
		}
	}
#else
	/* T100 setting */
	mxt_change_cfg(data, data->T100_address, 0,  t100_ctrl_cfg | MXT_T100_REPORT_EN_MASK);
	mxt_change_cfg(data, data->T100_address, 30,  t100_active_mode[0]);
	/* T47 setting */
	mxt_change_cfg(data, data->T47_address, 0,	t47_ctrl_cfg | MXT_ENABLE_MASK);
	/* T65 setting */
	mxt_change_cfg(data, data->T65_address, 0,	t65_ctrl_cfg | MXT_ENABLE_MASK);
	/* T72 setting */
	mxt_change_cfg(data, data->T72_address, 0,	t72_ctrl_cfg | MXT_ENABLE_MASK);
	/* T7 setting */
	mxt_change_cfg(data, data->T7_address, 0,  t7_active_mode[0]);
	mxt_change_cfg(data, data->T7_address, 1,  t7_active_mode[1]);
	mxt_change_cfg(data, data->T7_address, 2,  t7_active_mode[2]);
	mxt_change_cfg(data, data->T7_address, 3,  t7_active_mode[3]);
	mxt_change_cfg(data, data->T7_address, 4,  t7_active_mode[4]);
	/* T8 setting */
	mxt_change_cfg(data, data->T8_address, 2,  t8_active_mode[0]);
	mxt_change_cfg(data, data->T8_address, 4,  t8_active_mode[1]);
	data->anti->autocal = true; /*                                                        */
	/* T46 setting */
	mxt_change_cfg(data, data->T46_address, 0,  t46_active_mode[0]);
	mxt_change_cfg(data, data->T46_address, 2,  t46_active_mode[1]);
	mxt_change_cfg(data, data->T46_address, 3,  t46_active_mode[2]);
	mxt_change_cfg(data, data->T46_address, 5,  t46_active_mode[3]);
	mxt_change_cfg(data, data->T46_address, 6,  t46_active_mode[4]);
#endif

	dev_info(&data->client->dev, "Change to Active Mode\n");
}
#endif

static void mxt_start(struct mxt_data *data)
{
	if(chargerlogo){
		chargerlogo = false;
		return;
	}

	if (!data->suspended || data->in_bootloader)
		return;

	if(data->power_status == MXT_POWER_OFF){
		mxt_regulator_enable(data);
	}

	touch_disable_irq(data->irq);
	mxt_process_messages_until_invalid(data);
	if(data->power_status != MXT_POWER_ON || data->power_status != MXT_POWER_CFG_RUN)
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

	if(wait_change_cfg){
		dev_info(&data->client->dev, " %s : CHANGE TA MODE %d\n", __func__, data->ta_status);
#ifdef TSP_PATCH
		mxt_patch_test_event(data, data->ta_status);
#endif
		wait_change_cfg = false;
	} else {
#ifdef MXT_GESTURE_RECOGNIZE
		mxt_active_mode_start(data);
#endif
	}
	/* Recalibrate since touch doesn't power off when lcd on */
	mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);

	data->enable_reporting = true;
	data->suspended = false;
	touch_enable_irq(data->irq);
	do_gettimeofday(&t_ex_debug[TIME_START_TIME]);
}

static void mxt_stop(struct mxt_data *data)
{
	if (data->suspended || data->in_bootloader)
		return;

	dev_dbg(&data->client->dev, "%s\n", __func__);
	data->enable_reporting = false;
	touch_disable_irq(data->irq);
	cancel_work_sync(&data->work);
#ifdef I2C_SUSPEND_WORKAROUND
	cancel_delayed_work_sync(&data->check_suspended_work);
#endif
	cancel_delayed_work_sync(&data->work_ime_drumming);

#ifdef MXT_GESTURE_RECOGNIZE
	mxt_gesture_mode_start(data);

	if (data->mxt_knock_on_enable) {
		/* Recalibrate since touch doesn't power off when lcd off */
		mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
	} else
#else
	if (data->use_regulator)
#endif
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);

	mxt_reset_slots(data);
	data->suspended = true;
#ifdef MXT_GESTURE_RECOGNIZE
	if (data->mxt_knock_on_enable) {
		touch_enable_irq(data->irq);
	}
#endif

}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	mxt_start(data);
	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	mxt_stop(data);
}

static int mxt_parse_config(struct device *dev, struct device_node *np,
				struct mxt_config_info *info)
{
	struct property *prop;
	u8 *temp_cfg;
	u32 temp_length;
	int i = 0;

	const char *config_node_name[MXT_TMAX] = {
		"atmel,config_t6"
		,"atmel,config_t38"
		,"atmel,config_t71"
		,"atmel,config_t7"
		,"atmel,config_t8"
		,"atmel,config_t15"
		,"atmel,config_t18"
		,"atmel,config_t19"
		,"atmel,config_t23"
#ifdef MXT_GESTURE_RECOGNIZE
		,"atmel,config_t24"
#endif
		,"atmel,config_t25"
#ifdef MXT_GESTURE_RECOGNIZE
		,"atmel,config_t35"
#endif
		,"atmel,config_t40"
		,"atmel,config_t42"
		,"atmel,config_t46"
		,"atmel,config_t47"
		,"atmel,config_t55"
		,"atmel,config_t56"
		,"atmel,config_t61"
		,"atmel,config_t65"
		,"atmel,config_t66"
		,"atmel,config_t69"
		,"atmel,config_t70"
		,"atmel,config_t72"
		,"atmel,config_t78"
		,"atmel,config_t80"
		,"atmel,config_t84"
		,"atmel,config_t100"
		,"atmel,config_t101"
		,"atmel,config_t102"
		,"atmel,config_t103"
		,"atmel,config_t104"
		,"atmel,config_t105"
	};

	memset(&info->config_t, 0, sizeof(info->config_t));

	while(i < MXT_TMAX) {
		prop = of_find_property(np, config_node_name[i], &temp_length);
		if (!prop) {
			dev_err(dev, "Looking up %s property in node %s failed",
				config_node_name[i], np->full_name);
			return -ENODEV;
		} else if (!temp_length) {
			dev_err(dev, "Invalid length of %s configuration data\n", config_node_name[i]+13);
			return -EINVAL;
		}

		temp_cfg = devm_kzalloc(dev,
				temp_length * sizeof(u8), GFP_KERNEL);
		if (!temp_cfg) {
			dev_err(dev, "Unable to allocate memory to store %s cfg\n", config_node_name[i]+13);
			return -ENOMEM;
		}

		memcpy(temp_cfg, prop->value, temp_length);
		info->config_t[i] = temp_cfg;

#ifdef MXT_GESTURE_RECOGNIZE
		if(info->config_t[i][0] == 47){
			dev_dbg(dev, "%s's ctrl value is 0x%x",config_node_name[i],info->config_t[i][1]);
			t47_ctrl_cfg = info->config_t[i][1];
		}else if(info->config_t[i][0] == 65){
			dev_dbg(dev, "%s's ctrl value is 0x%x",config_node_name[i],info->config_t[i][1]);
			t65_ctrl_cfg = info->config_t[i][1];
		}else if(info->config_t[i][0] == 72){
			dev_dbg(dev, "%s's ctrl value is 0x%x",config_node_name[i],info->config_t[i][1]);
			t72_ctrl_cfg = info->config_t[i][1];
		}else if(info->config_t[i][0] == 100){
			dev_dbg(dev, "%s's ctrl value is 0x%x, scraux value is 0x%x",config_node_name[i],info->config_t[i][1],info->config_t[i][3]);
			t100_ctrl_cfg = info->config_t[i][1];
		}
#endif
		dev_dbg(dev, "%s config :%d\n", config_node_name[i]+13, info->config_t[i][0]);
		i++;
	}
	return 0;
}



static int mxt_parse_dt(struct device *dev, struct mxt_platform_data *pdata)
{
	struct mxt_config_info *info;
	struct device_node *temp;
	struct device_node *node = dev->of_node;
	int rc = 0;
	u32 temp_val;

	/* reset, irq gpio info */
	if (node == NULL)
		return -ENODEV;
	
	pdata->gpio_reset= of_get_named_gpio_flags(node, "atmel,reset-gpio", 0, NULL);
	pdata->gpio_int = of_get_named_gpio_flags(node, "atmel,irq-gpio", 0, NULL);
	
	rc = of_property_read_u32(node, "atmel,numtouch", &temp_val);
	if(rc){
		dev_err(dev, "Unable to read numtouch\n");
		return rc;
	}else
		pdata->numtouch = temp_val;
	
	rc = of_property_read_u32(node, "atmel,max_x", &temp_val);
	if(rc){
		dev_err(dev, "Unable to read max_x\n");
		return rc;
	}else
		pdata->max_x = temp_val;

	rc = of_property_read_u32(node, "atmel,max_y", &temp_val);
	if(rc){
		dev_err(dev, "Unable to read max_y\n");
		return rc;
	}else
		pdata->max_y = temp_val;
	
	rc = of_property_read_u32(node, "atmel,irqflags", &temp_val);
	if(rc){
		dev_err(dev, "Unable to read irqflags\n");
		return rc;
	}else
		pdata->irqflags = temp_val;

	rc = of_property_read_u32(node, "atmel,t19_num_keys", &temp_val);
	if(rc){
		dev_err(dev, "Unable to read t19_num_keys\n");
		return rc;
	}else
		pdata->t19_num_keys = temp_val;

	/* config array size */
	pdata->config_array_size = 0;
	temp = NULL;
	while ((temp = of_get_next_child(node, temp)))
		pdata->config_array_size++;

	dev_dbg(dev, "config array size is '%d'\n",pdata->config_array_size);

	if (!pdata->config_array_size){
		dev_err(dev, "config array size is '0'\n");
		return 0;
	}

	info = devm_kzalloc(dev, pdata->config_array_size *
				sizeof(struct mxt_config_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}
	
	pdata->config_array  = info;

	for_each_child_of_node(node, temp) {
		rc = mxt_parse_config(dev, temp, info);
		if (rc) {
			dev_err(dev, "Unable to parse config data\n");
			return rc;
		}
		info++;
	}
	return 0;

}


static int mxt_handle_pdata(struct mxt_data *data)
{
	data->pdata = dev_get_platdata(&data->client->dev);

	/* Use provided platform data if present */
	if (data->pdata) {
		if (data->pdata->cfg_name)
			mxt_update_file_name(&data->client->dev,
					     &data->cfg_name,
					     data->pdata->cfg_name,
					     strlen(data->pdata->cfg_name));

		return 0;
	}

	data->pdata = kzalloc(sizeof(*data->pdata), GFP_KERNEL);
	if (!data->pdata) {
		dev_err(&data->client->dev, "Failed to allocate pdata\n");
		return -ENOMEM;
	}

	/* Set default parameters */
	data->pdata->irqflags = IRQF_TRIGGER_FALLING;

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	int i;

	error = mxt_read_t9_resolution(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = MXT_DEVICE_NAME;
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	if (pdata->t19_num_keys) {
		__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);

		for (i = 0; i < pdata->t19_num_keys; i++)
			if (pdata->t19_keymap[i] != KEY_RESERVED)
				input_set_capability(input_dev, EV_KEY,
						     pdata->t19_keymap[i]);

		__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
		__set_bit(BTN_TOOL_DOUBLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_QUADTAP, input_dev->keybit);

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);

		input_dev->name = "Atmel maXTouch Touchpad";
	}

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			     0, 255, 0, 0);

	/* For multi touch */
	num_mt_slots = data->num_touchids + data->num_stylusids;
	error = input_mt_init_slots(input_dev, num_mt_slots);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
			     0, 255, 0, 0);

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS);
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS2);
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;

		for (i = 0; i < data->pdata->t15_num_keys; i++)
			input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int mxt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;
	u8 fw_version = 0;
	u8 fw_build = 0;

	is_probing = true;
	dev_info(&client->dev, "%s\n", __func__);

#ifdef	MXT_GESTURE_RECOGNIZE
	wake_lock_init(&touch_wake_lock, WAKE_LOCK_SUSPEND, "touch_irq");
	mutex_init(&i2c_suspend_lock);
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check error\n");
		return -ENOMEM;
	}
	
	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0",
		 client->adapter->nr, client->addr);
	dev_info(&client->dev, "i2c-%u-%04x/input0\n", client->adapter->nr, client->addr);
	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	/*read dtsi data*/
	if (client->dev.of_node) {
		data->pdata = devm_kzalloc(&client->dev,
			sizeof(struct mxt_platform_data), GFP_KERNEL);
		if (!data->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			error = -ENOMEM;
			goto err_free_mem;
		}

		error = mxt_parse_dt(&client->dev, data->pdata);
		if (error)
			goto err_free_mem;

	} else{
		error = mxt_handle_pdata(data);
		if (error)
			goto err_free_mem;
	}
	/*read dtsi data*/

	data->anti = devm_kzalloc(&client->dev,
		sizeof(struct mxt_anti_info), GFP_KERNEL);
	if (!data->anti) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);
	/* Self Test */
	init_completion(&data->t25_completion);
	/* workqueue */
	INIT_WORK(&data->work, mxt_process_messages_t44);
#ifdef I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&data->check_suspended_work, touch_check_suspended_worker);
#endif
	INIT_DELAYED_WORK(&data->work_ime_drumming, change_ime_drumming_func);
	/* request reset pin */
	if(data->pdata->gpio_reset> 0){
		error = gpio_request(data->pdata->gpio_reset, "touch_reset");
		if (error < 0) {
			dev_err(&client->dev, "FAIL: touch_reset gpio_request\n");
			goto err_interrupt_failed;
		}
		gpio_direction_output(data->pdata->gpio_reset, 1);
	}

	/* request interrupt pin */
	if(data->pdata->gpio_int > 0){
		error = gpio_request(data->pdata->gpio_int, "touch_int");
		if (error < 0) {
			dev_err(&client->dev, "FAIL: touch_int gpio_request\n");
			goto err_interrupt_failed;
		}
		gpio_direction_input(data->pdata->gpio_int);
	}
	
	error = request_threaded_irq(data->irq, NULL, mxt_interrupt,
				     data->pdata->irqflags | IRQF_ONESHOT,
				     client->name, data);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_pdata;
	}

	mxt_probe_regulators(data);

	touch_disable_irq(data->irq);

	error = mxt_initialize(data);
	if (error)
		goto err_free_irq;

#ifdef FIRMUP_ON_PROBE
	if(data->info != NULL)
	{
		fw_version = data->info->version;
		fw_build = data->info->build;
	} else {
		dev_info(&client->dev,"%s data->info = %s \n", __func__, data->info?"Exist":"NULL");
	}

	dev_dbg(&client->dev, "Need to firmware update? %d, %x!=%x, %x!=%x\n",
				data->in_bootloader, data->info->version, MXT_LATEST_FW_VERSION,
				data->info->build, MXT_LATEST_FW_BUILD);
	if (data->in_bootloader ||
			  fw_version != MXT_LATEST_FW_VERSION ||
			  fw_build != MXT_LATEST_FW_BUILD) {
		mxt_acquire_irq(data);

		dev_info(&client->dev, "Execute firmware update func\n");
		error = mxt_firmware_update(data);
		 if (error) {
			dev_err(&client->dev, "Failed to update firmware\n");
			return error;
		}
	}
#endif
	error = mxt_rest_init(data);
	if (error)
		goto err_free_irq;

#if defined(CONFIG_FB)
		data->fb_notif.notifier_call = fb_notifier_callback;

		error = fb_register_client(&data->fb_notif);
		if (error)
			dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
				error);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
							MXT_SUSPEND_LEVEL;
		data->early_suspend.suspend = mxt_early_suspend;
		data->early_suspend.resume = mxt_late_resume;
		register_early_suspend(&data->early_suspend);
#endif

	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO) {
		dev_info(&client->dev, "Boot chargerlogo mode\n");
		chargerlogo = true;
	}else{
		dev_info(&client->dev, "Boot normal mode\n");
	}
	/* disabled report touch event to prevent unnecessary event.
	* it will be enabled in open function
	*/
	mxt_stop(data);

	/* Register sysfs for making fixed communication path to framework layer */
	error = sysdev_class_register(&lge_touch_sys_class);
	if (error < 0) {
		dev_err(&client->dev, "sysdev_class_register is failed\n");
		goto err_lge_touch_sys_class_register;
	}

	error = sysdev_register(&lge_touch_sys_device);
	if (error < 0) {
		dev_err(&client->dev, "sysdev_register is failed\n");
		goto err_lge_touch_sys_dev_register;
	}

	error = kobject_init_and_add(&data->lge_touch_kobj, &lge_touch_kobj_type,
			data->input_dev->dev.kobj.parent,
			"%s", MXT_DEVICE_NAME);
	if (error < 0) {
		dev_err(&client->dev, "kobject_init_and_add is failed\n");
		goto err_lge_touch_sysfs_init_and_add;
	}

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_lge_touch_sysfs_init_and_add;
	}

	is_probing = false;
	data->anti->pen_id = 255;
	data->anti->curr_ths = 23;	/* don't setting 23 on first 1 finger release event after booting*/
	dev_info(&client->dev, "probe success\n");
	return 0;

err_lge_touch_sysfs_init_and_add:
	kobject_del(&data->lge_touch_kobj);
err_lge_touch_sys_dev_register:
	sysdev_unregister(&lge_touch_sys_device);
err_lge_touch_sys_class_register:
	sysdev_class_unregister(&lge_touch_sys_class);
	mxt_free_object_table(data);
err_free_irq:
	free_irq(data->irq, data);
err_interrupt_failed:
err_free_pdata:
err_free_mem:
	if(data)
		kfree(data);
	return error;
}

static int __devexit mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	kobject_del(&data->lge_touch_kobj);
	sysdev_unregister(&lge_touch_sys_device);
	sysdev_class_unregister(&lge_touch_sys_class);

	if (data->pdata->gpio_int > 0)
		gpio_free(data->pdata->gpio_int);
	free_irq(data->irq, data);
	regulator_put(data->vdd_ana);
	regulator_put(data->vcc_i2c);
	regulator_put(data->vcc_dig);
	mxt_free_object_table(data);
	if(data)
		kfree(data);
#ifdef MXT_GESTURE_RECOGNIZE
	wake_lock_destroy(&touch_wake_lock);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	dev_info(&client->dev, "%s\n", __func__);

	if (data->in_bootloader){
		dev_info(&data->client->dev, "%s : Fw upgrade mode.\n", __func__);
		return 0;
	}

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_stop(data);

#ifdef MXT_GESTURE_RECOGNIZE
	if (data->mxt_knock_on_enable) {
		touch_enable_irq_wake(data->irq);
		dev_dbg(&client->dev, "touch enable irq wake");
	}
	gpio_tlmm_config(GPIO_CFG(data->pdata->gpio_reset, 0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif
	mutex_unlock(&input_dev->mutex);
	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
#ifdef MFTS_TOUCH
	int ret = 0;
#endif

	dev_info(&client->dev, "%s\n", __func__);

	mutex_lock(&input_dev->mutex);
#ifdef MXT_GESTURE_RECOGNIZE
	if(!data->pdata->gpio_reset)
		gpio_direction_output(data->pdata->gpio_reset, 1);
	if (data->mxt_knock_on_enable) {
		touch_disable_irq_wake(data->irq);
		dev_dbg(&client->dev, "touch disable irq wake");
	}
#endif
	if (input_dev->users){
		mxt_start(data);
	}
#ifdef MFTS_TOUCH
	if(data->power_status == MXT_POWER_ON || data->power_status == MXT_POWER_CFG_RUN || data->power_status == MXT_POWER_CFG_KNOCKON){
		dev_info(&client->dev, "%s : Update mxt Configuration Start.\n", __func__);
		ret = mxt_configure_objects(data);
		if (ret){
			dev_err(&client->dev, "%s : Update mxt Configuration Fail!\n", __func__);
		}
		dev_info(&client->dev, "%s : Configuration check end.\n", __func__);
	}
#endif

	mutex_unlock(&input_dev->mutex);
	return 0;
}
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mxt_data *mxt_dev_data =
		container_of(self, struct mxt_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && mxt_dev_data &&
			mxt_dev_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			mxt_resume(&mxt_dev_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			mxt_suspend(&mxt_dev_data->client->dev);
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void mxt_early_suspend(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data,
						early_suspend);
	mxt_suspend(&data->client->dev);
}

static void mxt_late_resume(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data,
						early_suspend);
	mxt_resume(&data->client->dev);
}
#endif

#if defined(CONFIG_PM)
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
static int touch_suspend(struct device *device)
{
	return 0;
}

static int touch_resume(struct device *device)
{
	return 0;
}
#elif defined(MXT_GESTURE_RECOGNIZE)
static int touch_suspend(struct device *device)
{
#ifdef I2C_SUSPEND_WORKAROUND
	struct mxt_data *data = dev_get_drvdata(device);
	if (!data) {
		printk("lge_touch called before init\n");
		return 0;
	}
#endif
	mutex_lock(&i2c_suspend_lock);
#ifdef I2C_SUSPEND_WORKAROUND
	dev_dbg(&data->client->dev, " %s\n", __func__);
#endif
	return 0;
}

static int touch_resume(struct device *device)
{
#ifdef I2C_SUSPEND_WORKAROUND
	struct mxt_data *data = dev_get_drvdata(device);
	if (!data) {
		printk("lge_touch called before init\n");
		return 0;
	}
#endif

	mutex_unlock(&i2c_suspend_lock);
#ifdef I2C_SUSPEND_WORKAROUND
	dev_dbg(&data->client->dev, " %s\n", __func__);
#endif

	return 0;
}
#endif
#endif

#if defined(CONFIG_PM)
static const struct dev_pm_ops mxt_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))||defined(MXT_GESTURE_RECOGNIZE)
	.suspend	= touch_suspend,
	.resume		= touch_resume,
#endif
};
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mxt_pm_ops = {
	.suspend	= NULL,
	.resume		= NULL,
};
#else
static SIMPLE_DEV_PM_OPS(mxt_pm_ops, mxt_suspend, mxt_resume);
#endif

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	touch_disable_irq(data->irq);
}

static struct of_device_id mxt_match_table[] = {
	{ .compatible = "atmel,s540",},
	{ },
};

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "atmel_mxt_tp", 0 },
	{ "mXT224", 0 },
	{ MXT_DEVICE_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= MXT_DEVICE_NAME,
		.of_match_table = mxt_match_table,
		.owner	= THIS_MODULE,
		.pm	= &mxt_pm_ops,
	},
	.probe		= mxt_probe,
	.remove		= __devexit_p(mxt_remove),
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

static int __devinit mxt_init(void)
{
	int ret = 0;

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		printk("CANNOT create new workqueue\n");
		ret = -EMLINK;
		goto err_work_queue;
	}
	ret = i2c_add_driver(&mxt_driver);
	if (ret < 0) {
		printk("FAIL: i2c_add_driver\n");
		goto err_i2c_add_driver;
	}

	return 0;

err_i2c_add_driver:
	destroy_workqueue(touch_wq);
err_work_queue:
	return ret;
}

static void __exit mxt_exit(void)
{
	i2c_del_driver(&mxt_driver);

	if (touch_wq)
		destroy_workqueue(touch_wq);
}

module_init(mxt_init);
module_exit(mxt_exit);

/* Module information */
MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");

