/* Sensirion SHT21 humidity and temperature sensor driver
 *
 * Copyright (C) 2010 Urs Fleisch <urs.fleisch@sensirion.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Data sheet available (5/2010) at
 * http://www.sensirion.com/en/pdf/product_information/Datasheet-humidity-sensor-SHT21.pdf
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/i2c/sht21.h>
#ifdef CONFIG_OF
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#endif

#define ABS_HUMIDITY		0x2B	/* humidity sensor */
#define ABS_TEMPERATURE		0x2C	/* temperature sensor */

/* I2C command bytes */
#define SHT21_TRIG_T_MEASUREMENT_HM  0xe3
#define SHT21_TRIG_RH_MEASUREMENT_HM 0xe5

/**
 * struct sht21 - SHT21 device specific data
 * @hwmon_dev: device registered with hwmon
 * @lock: mutex to protect measurement values
 * @valid: only 0 before first measurement is taken
 * @last_update: time of last update (jiffies)
 * @temperature: cached temperature measurement value
 * @humidity: cached humidity measurement value
 */
struct sht21 {
	struct i2c_client *client;

	struct input_dev *input_dev_humi;
	struct input_dev *input_dev_temp;
	struct sht21_platform_data *platform_data;
	struct delayed_work input_work;

	/* control flag from HAL */
	unsigned int enable_humi_sensor;
	unsigned int enable_temp_sensor;
	unsigned int humi_poll_delay;
	unsigned int temp_poll_delay;

	struct device *hwmon_dev;
	struct mutex lock;
	char valid;
	unsigned long last_update;
	int temperature;
	int humidity;
};

static struct sht21 *pdev_data;

/**
 * sht21_temp_ticks_to_millicelsius() - convert raw temperature ticks to
 * milli celsius
 * @ticks: temperature ticks value received from sensor
 */
static inline int sht21_temp_ticks_to_millicelsius(int ticks)
{
	ticks &= ~0x0003; /* clear status bits */
	/*
	 * Formula T = -46.85 + 175.72 * ST / 2^16 from data sheet 6.2,
	 * optimized for integer fixed point (3 digits) arithmetic
	 */
	return ((21965 * ticks) >> 13) - 46850;
}

/**
 * sht21_rh_ticks_to_per_cent_mille() - convert raw humidity ticks to
 * one-thousandths of a percent relative humidity
 * @ticks: humidity ticks value received from sensor
 */
static inline int sht21_rh_ticks_to_per_cent_mille(int ticks)
{
	ticks &= ~0x0003; /* clear status bits */
	/*
	 * Formula RH = -6 + 125 * SRH / 2^16 from data sheet 6.1,
	 * optimized for integer fixed point (3 digits) arithmetic
	 */
	return ((15625 * ticks) >> 13) - 6000;
}

/**
 * sht21_update_measurements() - get updated measurements from device
 * @client: I2C client device
 *
 * Returns 0 on success, else negative errno.
 */
static int sht21_update_measurements(struct i2c_client *client)
{
	int ret = 0;
	struct sht21 *sht21 = i2c_get_clientdata(client);

	mutex_lock(&sht21->lock);
	/*
	 * Data sheet 2.4:
	 * SHT2x should not be active for more than 10% of the time - e.g.
	 * maximum two measurements per second at 12bit accuracy shall be made.
	 */
	if (time_after(jiffies, sht21->last_update + HZ / 2) || !sht21->valid) {
		ret = i2c_smbus_read_word_swapped(client,
						  SHT21_TRIG_T_MEASUREMENT_HM);
		if (ret < 0)
			goto out;
		sht21->temperature = sht21_temp_ticks_to_millicelsius(ret);
		ret = i2c_smbus_read_word_swapped(client,
						  SHT21_TRIG_RH_MEASUREMENT_HM);
		if (ret < 0)
			goto out;
		sht21->humidity = sht21_rh_ticks_to_per_cent_mille(ret);
		sht21->last_update = jiffies;
		sht21->valid = 1;
	}
out:
	mutex_unlock(&sht21->lock);

	return ret >= 0 ? 0 : ret;
}

/**
 * sht21_show_temperature() - show temperature measurement value in sysfs
 * @dev: device
 * @attr: device attribute
 * @buf: sysfs buffer (PAGE_SIZE) where measurement values are written to
 *
 * Will be called on read access to temp1_input sysfs attribute.
 * Returns number of bytes written into buffer, negative errno on error.
 */
static ssize_t sht21_show_temperature(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sht21 *sht21 = i2c_get_clientdata(client);
	int ret = sht21_update_measurements(client);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", sht21->temperature);
}

/**
 * sht21_show_humidity() - show humidity measurement value in sysfs
 * @dev: device
 * @attr: device attribute
 * @buf: sysfs buffer (PAGE_SIZE) where measurement values are written to
 *
 * Will be called on read access to humidity1_input sysfs attribute.
 * Returns number of bytes written into buffer, negative errno on error.
 */
static ssize_t sht21_show_humidity(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sht21 *sht21 = i2c_get_clientdata(client);
	int ret = sht21_update_measurements(client);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", sht21->humidity);
}

/* sysfs attributes */
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, sht21_show_temperature,
	NULL, 0);
static SENSOR_DEVICE_ATTR(humidity1_input, S_IRUGO, sht21_show_humidity,
	NULL, 0);

static ssize_t sht21_show_humidity_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sht21 *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable_humi_sensor);
}

static ssize_t sht21_store_humidity_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sht21 *data = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	pr_info("%s: enable humidity sensor ( %ld)\n", __func__, val);

	if (val) {
		if (!(data->enable_humi_sensor || data->enable_temp_sensor))
			schedule_delayed_work(&data->input_work,
				msecs_to_jiffies(HZ/2)); /* 0.5sec */

		data->enable_humi_sensor = val;
	} else {
		data->enable_humi_sensor = val;
		if (!(data->enable_humi_sensor || data->enable_temp_sensor))
			cancel_delayed_work_sync(&data->input_work);
	}
	return count;
}
static SENSOR_DEVICE_ATTR(humidity_enable, S_IWUGO | S_IRUGO,
		sht21_show_humidity_enable, sht21_store_humidity_enable, 0);

static ssize_t sht21_show_humidity_poll_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sht21 *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->humi_poll_delay * 1000); /* return in micro-second */
}

static ssize_t sht21_store_humidity_poll_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sht21 *data = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	data->humi_poll_delay = val;

	return count;
}
static SENSOR_DEVICE_ATTR(humidity_poll_delay, S_IWUGO | S_IRUGO,
		sht21_show_humidity_poll_delay, sht21_store_humidity_poll_delay, 0);


static ssize_t sht21_show_temperature_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sht21 *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->enable_temp_sensor);
}

static ssize_t sht21_store_temperature_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sht21 *data = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	pr_info("%s: enable temperature sensor ( %ld)\n", __func__, val);

	if (val) {
		if (!(data->enable_humi_sensor || data->enable_temp_sensor))
			schedule_delayed_work(&data->input_work,
				msecs_to_jiffies(HZ/2)); /* 0.5sec */

		data->enable_temp_sensor = val;
	} else {
		data->enable_temp_sensor = val;
		if (!(data->enable_humi_sensor || data->enable_temp_sensor))
			cancel_delayed_work_sync(&data->input_work);
	}
	return count;
}
static SENSOR_DEVICE_ATTR(temperature_enable, S_IWUGO | S_IRUGO,
		sht21_show_temperature_enable, sht21_store_temperature_enable, 0);


static ssize_t sht21_show_temperature_poll_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sht21 *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->temp_poll_delay * 1000);	/* return in micro-second */
}

static ssize_t sht21_store_temperature_poll_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sht21 *data = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	data->temp_poll_delay = val;

	return count;
}
static SENSOR_DEVICE_ATTR(temperature_poll_delay, S_IWUGO | S_IRUGO,
		sht21_show_temperature_poll_delay, sht21_store_temperature_poll_delay, 0);


static struct attribute *sht21_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_humidity1_input.dev_attr.attr,
	&sensor_dev_attr_humidity_enable.dev_attr.attr,
	&sensor_dev_attr_humidity_poll_delay.dev_attr.attr,
	&sensor_dev_attr_temperature_enable.dev_attr.attr,
	&sensor_dev_attr_temperature_poll_delay.dev_attr.attr,
	NULL
};

static const struct attribute_group sht21_attr_group = {
	.attrs = sht21_attributes,
};

static void sht21_polling_work_handler(struct work_struct *work)
{
	struct sht21 *data;
	int err;

	data = container_of(work, struct sht21, input_work.work);

	err = sht21_update_measurements(data->client);

	if (data->enable_humi_sensor) {
		input_report_abs(data->input_dev_humi, ABS_HUMIDITY, data->humidity);
		input_sync(data->input_dev_humi);
	}

	if (data->enable_temp_sensor) {
		input_report_abs(data->input_dev_temp, ABS_TEMPERATURE, data->temperature);
		input_sync(data->input_dev_temp);
	}

	schedule_delayed_work(&data->input_work,
		msecs_to_jiffies(HZ/2)); /* 0.5sec */
}


#ifdef CONFIG_PM
static int sht21_suspend(struct device *dev)
{
	struct sht21_platform_data *pdata = pdev_data->platform_data;

	pr_info("%s\n", __func__);

	if (pdata->power_on)
		pdata->power_on(false);

	return 0;
}

static int sht21_resume(struct device *dev)
{
	struct sht21_platform_data *pdata = pdev_data->platform_data;

	pr_info("%s\n", __func__);

	if (pdata->power_on)
		pdata->power_on(true);
	return 0;
}

#else

#define sht21_suspend	NULL
#define sht21_resume	NULL

#endif /* CONFIG_PM */


#ifdef CONFIG_OF
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
	       regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int sensor_regulator_configure(struct sht21 *data, bool on)
{
	struct i2c_client *client = data->client;
	struct sht21_platform_data *pdata = data->platform_data;
	int rc;

	if (on == false)
		goto hw_shutdown;

	pdata->vcc_ana = regulator_get(&client->dev, "sensirion,vdd_ana");
	if (IS_ERR(pdata->vcc_ana)) {
		rc = PTR_ERR(pdata->vcc_ana);
		dev_err(&client->dev,
			"Regulator get failed vcc_ana rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->vcc_ana) > 0) {
		rc = regulator_set_voltage(pdata->vcc_ana, AVDD_VTG_MIN_UV,
					   AVDD_VTG_MAX_UV);
		if (rc) {
			dev_err(&client->dev,
				"regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_vcc_ana;
		}
	}
	if (pdata->digital_pwr_regulator) {
		pdata->vcc_dig = regulator_get(&client->dev, "sensirion,vddio_dig");
		if (IS_ERR(pdata->vcc_dig)) {
			rc = PTR_ERR(pdata->vcc_dig);
			dev_err(&client->dev,
				"Regulator get dig failed rc=%d\n", rc);
			goto error_get_vtg_vcc_dig;
		}

		if (regulator_count_voltages(pdata->vcc_dig) > 0) {
			rc = regulator_set_voltage(pdata->vcc_dig,
						   VDDIO_VTG_DIG_MIN_UV, VDDIO_VTG_DIG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_vcc_dig;
			}
		}
	}
	if (pdata->i2c_pull_up) {
		pdata->vcc_i2c = regulator_get(&client->dev, "sensirion,vddio_i2c");
		if (IS_ERR(pdata->vcc_i2c)) {
			rc = PTR_ERR(pdata->vcc_i2c);
			dev_err(&client->dev,
				"Regulator get failed rc=%d\n", rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
			rc = regulator_set_voltage(pdata->vcc_i2c,
						   VDDIO_I2C_VTG_MIN_UV, VDDIO_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}
	}

	return 0;

error_set_vtg_i2c:
	regulator_put(pdata->vcc_i2c);
error_get_vtg_i2c:
	if (pdata->digital_pwr_regulator)
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      VDDIO_VTG_DIG_MAX_UV);
error_set_vtg_vcc_dig:
	if (pdata->digital_pwr_regulator)
		regulator_put(pdata->vcc_dig);
error_get_vtg_vcc_dig:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, AVDD_VTG_MAX_UV);
error_set_vtg_vcc_ana:
	regulator_put(pdata->vcc_ana);
	return rc;

hw_shutdown:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, AVDD_VTG_MAX_UV);
	regulator_put(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      VDDIO_VTG_DIG_MAX_UV);
		regulator_put(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		if (regulator_count_voltages(pdata->vcc_i2c) > 0)
			regulator_set_voltage(pdata->vcc_i2c, 0,
					      VDDIO_I2C_VTG_MAX_UV);
		regulator_put(pdata->vcc_i2c);
	}
	return 0;
}


static int sensor_regulator_power_on(struct sht21 *data, bool on)
{
	struct i2c_client *client = data->client;
	struct sht21_platform_data *pdata = data->platform_data;
	int rc;

	if (on == false)
		goto power_off;

	rc = reg_set_optimum_mode_check(pdata->vcc_ana, AVDD_ACTIVE_LOAD_UA);
	if (rc < 0) {
		dev_err(&client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(pdata->vcc_ana);
	if (rc) {
		dev_err(&client->dev,
			"Regulator vcc_ana enable failed rc=%d\n", rc);
		goto error_reg_en_vcc_ana;
	}

	if (pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(pdata->vcc_dig,
						VDDIO_ACTIVE_LOAD_DIG_UA);
		if (rc < 0) {
			dev_err(&client->dev,
				"Regulator vcc_dig set_opt failed rc=%d\n",
				rc);
			goto error_reg_opt_vcc_dig;
		}

		rc = regulator_enable(pdata->vcc_dig);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_dig enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_dig;
		}
	}

	if (pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(pdata->vcc_i2c, VDDIO_I2C_LOAD_UA);
		if (rc < 0) {
			dev_err(&client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(pdata->vcc_i2c);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}

	msleep(130);

	return 0;

error_reg_en_vcc_i2c:
	if (pdata->i2c_pull_up)
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
error_reg_opt_i2c:
	if (pdata->digital_pwr_regulator)
		regulator_disable(pdata->vcc_dig);
error_reg_en_vcc_dig:
	if (pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
error_reg_opt_vcc_dig:
	regulator_disable(pdata->vcc_ana);
error_reg_en_vcc_ana:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	return rc;

power_off:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	regulator_disable(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
		regulator_disable(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
		regulator_disable(pdata->vcc_i2c);
	}
	msleep(50);
	return 0;
}

static int sensor_platform_hw_power_on(bool on)
{
	sensor_regulator_power_on(pdev_data, on);

	return 0;
}

static int sensor_platform_hw_init(void)
{
	struct i2c_client *client = pdev_data->client;
	struct sht21 *data = pdev_data;
	int error;

	error = sensor_regulator_configure(data, true);

	if (gpio_is_valid(data->platform_data->enable_gpio)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(data->platform_data->enable_gpio, "sht21_enable_gpio");
		if (error) {
			dev_err(&client->dev, "unable to request gpio [%d]\n",
				data->platform_data->enable_gpio);
		}
		error = gpio_direction_output(data->platform_data->enable_gpio, 1);
		if (error) {
			dev_err(&client->dev,
				"unable to set direction for gpio [%d]\n",
				data->platform_data->enable_gpio);
		}
	} else {
		dev_err(&client->dev, "enable gpio not provided\n");
	}
	return 0;
}

static void sensor_platform_hw_exit(void)
{
	struct sht21 *data = pdev_data;

	sensor_regulator_configure(data, false);

	if (gpio_is_valid(data->platform_data->enable_gpio))
		gpio_free(data->platform_data->enable_gpio);
}

static int sensor_parse_dt(struct device *dev, struct sht21_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	/* regulator info */
	pdata->i2c_pull_up = of_property_read_bool(np, "sensirion,i2c-pull-up");
	pdata->digital_pwr_regulator = of_property_read_bool(np,
				       "sensirion,dig-reg-support");

	/* enable gpio info */
	pdata->enable_gpio = of_get_named_gpio_flags(np, "sensirion,enable-gpio",
			  0, &pdata->enable_gpio_flags);

	/* set functions of platform data */
	pdata->init = sensor_platform_hw_init;
	pdata->exit = sensor_platform_hw_exit;
	pdata->power_on = sensor_platform_hw_power_on;

	return 0;
}
#endif

/* sht21 input device functions */
static int sht21_input_init(struct sht21 *data)
{
	int err = 0;

	/* Declare input device */
	data->input_dev_humi = input_allocate_device();
	if (!data->input_dev_humi)
		return -ENOMEM;

	data->input_dev_temp = input_allocate_device();
	if (!data->input_dev_temp)
		return -ENOMEM;

	set_bit(EV_ABS, data->input_dev_humi->evbit);
	set_bit(EV_ABS, data->input_dev_temp->evbit);


	input_set_abs_params(data->input_dev_humi, ABS_HUMIDITY, 0, 100, 0, 0);	/* humidity */
	input_set_abs_params(data->input_dev_temp, ABS_TEMPERATURE, -40, 120, 0, 0); /* temperature */

	/* Set name */
	data->input_dev_humi->name = "humidity";
	data->input_dev_temp->name = "temperature";

	data->input_dev_humi->dev.parent = &data->client->dev;
	data->input_dev_temp->dev.parent = &data->client->dev;

	/* Set device data */
	input_set_drvdata(data->input_dev_humi, data);
	input_set_drvdata(data->input_dev_temp, data);

	dev_set_drvdata(&data->input_dev_humi->dev, data);
	dev_set_drvdata(&data->input_dev_temp->dev, data);

	/* Register */
	err = input_register_device(data->input_dev_humi);
	if (err) {
		input_free_device(data->input_dev_humi);
		dev_err(&data->client->dev,
			"Unable to register input device : %s\n",
			data->input_dev_humi->name);
		return err;
	}

	err = input_register_device(data->input_dev_temp);
	if (err) {
		input_free_device(data->input_dev_temp);
		dev_err(&data->client->dev,
			"Unable to register input device : %s\n",
			data->input_dev_temp->name);
		return err;
	}

	INIT_DELAYED_WORK(&data->input_work, sht21_polling_work_handler);

	return err;
}

/**
 * sht21_probe() - probe device
 * @client: I2C client device
 * @id: device ID
 *
 * Called by the I2C core when an entry in the ID table matches a
 * device's name.
 * Returns 0 on success.
 */
static int __devinit sht21_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct sht21 *sht21;
	int err;
	struct sht21_platform_data *platform_data;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev,
			"adapter does not support SMBus word transactions\n");
		return -ENODEV;
	}

	sht21 = kzalloc(sizeof(*sht21), GFP_KERNEL);
	if (!sht21) {
		dev_dbg(&client->dev, "kzalloc failed\n");
		return -ENOMEM;
	}

	pdev_data = sht21;
	pdev_data->client = client;

	/***** Set layout information *****/
	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
					     sizeof(struct sht21_platform_data), GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		pdev_data->platform_data = platform_data;
		client->dev.platform_data = platform_data;
		err = sensor_parse_dt(&client->dev, platform_data);
		if (err)
			return err;
	} else {
		platform_data = client->dev.platform_data;

	}

	/***** h/w initialization *****/
	if (platform_data->init)
		err = platform_data->init();

	if (platform_data->power_on)
		err = platform_data->power_on(true);

	/***** input *****/
	err = sht21_input_init(sht21);
	if (err) {
		dev_err(&client->dev,
			"%s: input_dev register failed", __func__);
		goto fail_free;
	}

	i2c_set_clientdata(client, sht21);

	mutex_init(&sht21->lock);

	err = sysfs_create_group(&client->dev.kobj, &sht21_attr_group);
	if (err) {
		dev_dbg(&client->dev, "could not create sysfs files\n");
		goto fail_free;
	}
	sht21->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(sht21->hwmon_dev)) {
		dev_dbg(&client->dev, "unable to register hwmon device\n");
		err = PTR_ERR(sht21->hwmon_dev);
		goto fail_remove_sysfs;
	}

	dev_info(&client->dev, "initialized\n");

	return 0;

fail_remove_sysfs:
	sysfs_remove_group(&client->dev.kobj, &sht21_attr_group);
fail_free:
	if (platform_data->power_on)
		platform_data->power_on(false);
	if (platform_data->exit)
		platform_data->exit();

	kfree(sht21);

	return err;
}

/**
 * sht21_remove() - remove device
 * @client: I2C client device
 */
static int __devexit sht21_remove(struct i2c_client *client)
{
	struct sht21 *sht21 = i2c_get_clientdata(client);
	struct sht21_platform_data *pdata = sht21->platform_data;

	hwmon_device_unregister(sht21->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &sht21_attr_group);

	cancel_delayed_work_sync(&sht21->input_work);

	if (pdata->power_on)
		pdata->power_on(false);
	if (pdata->exit)
		pdata->exit();

	kfree(sht21);

	return 0;
}

/* Device ID table */
static const struct i2c_device_id sht21_id[] = {
	{ "sht21", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sht21_id);

#ifdef CONFIG_OF
static struct of_device_id sht21_match_table[] = {
	{ .compatible = "sensirion,sht21",},
	{ },
};
#else
#define sht21_match_table NULL
#endif

static const struct dev_pm_ops sht21_pm_ops = {
#ifdef CONFIG_PM
	.suspend	= sht21_suspend,
	.resume 	= sht21_resume,
#endif
};

static struct i2c_driver sht21_driver = {
	.driver.name = "sht21",
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = sht21_match_table,
#ifdef CONFIG_PM
	.driver.pm   = &sht21_pm_ops,
#endif
	.probe       = sht21_probe,
	.remove      = __devexit_p(sht21_remove),
	.id_table    = sht21_id,
};

module_i2c_driver(sht21_driver);

MODULE_AUTHOR("Urs Fleisch <urs.fleisch@sensirion.com>");
MODULE_DESCRIPTION("Sensirion SHT21 humidity and temperature sensor driver");
MODULE_LICENSE("GPL");
