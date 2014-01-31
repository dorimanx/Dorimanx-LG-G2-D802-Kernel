/*
 * LP5521 LED chip driver.
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contact: Samu Onkalo <samu.p.onkalo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/leds.h>
#include <linux/leds-lp5521.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
#include <mach/board_lge.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#endif

#define LP5521_PROGRAM_LENGTH		32	/* in bytes */
#define LP5521_ENG_MASK_BASE		0x30	/* 00110000 */
#define LP5521_ENG_STATUS_MASK		0x07	/* 00000111 */

#define LP5521_CMD_LOAD			0x15	/* 00010101 */
#define LP5521_CMD_RUN			0x2a	/* 00101010 */
#define LP5521_CMD_DIRECT		0x3f	/* 00111111 */
#define LP5521_CMD_DISABLED		0x00	/* 00000000 */

/* Registers */
#define LP5521_REG_ENABLE		0x00
#define LP5521_REG_OP_MODE		0x01
#define LP5521_REG_R_PWM		0x02
#define LP5521_REG_R_CURRENT		0x05
#define LP5521_REG_CONFIG		0x08
#define LP5521_REG_R_CHANNEL_PC		0x09
#define LP5521_REG_STATUS		0x0C
#define LP5521_REG_RESET		0x0D
#define LP5521_REG_GPO			0x0E
#define LP5521_REG_R_PROG_MEM		0x10
#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
#define LP5521_REG_G_PWM		0x04
#define LP5521_REG_B_PWM		0x03
#define LP5521_REG_G_CURRENT		0x07
#define LP5521_REG_B_CURRENT		0x06
#define LP5521_REG_G_CHANNEL_PC		0x0B
#define LP5521_REG_B_CHANNEL_PC		0x0A
#define LP5521_REG_G_PROG_MEM		0x50
#define LP5521_REG_B_PROG_MEM		0x30
#else
#define LP5521_REG_G_PWM		0x03
#define LP5521_REG_B_PWM		0x04
#define LP5521_REG_G_CURRENT		0x06
#define LP5521_REG_B_CURRENT		0x07
#define LP5521_REG_G_CHANNEL_PC		0x0A
#define LP5521_REG_B_CHANNEL_PC		0x0B
#define LP5521_REG_G_PROG_MEM		0x30
#define LP5521_REG_B_PROG_MEM		0x50
#endif
#define LP5521_PROG_MEM_BASE		LP5521_REG_R_PROG_MEM
#define LP5521_PROG_MEM_SIZE		0x20

/* Base register to set LED current */
#define LP5521_REG_LED_CURRENT_BASE	LP5521_REG_R_CURRENT

/* Base register to set the brightness */
#define LP5521_REG_LED_PWM_BASE		LP5521_REG_R_PWM

/* Bits in ENABLE register */
#define LP5521_MASTER_ENABLE		0x40	/* Chip master enable */
#define LP5521_LOGARITHMIC_PWM		0x80	/* Logarithmic PWM adjustment */
#define LP5521_EXEC_RUN			0x2A
#define LP5521_ENABLE_DEFAULT	\
	(LP5521_MASTER_ENABLE | LP5521_LOGARITHMIC_PWM)
#define LP5521_ENABLE_RUN_PROGRAM	\
	(LP5521_ENABLE_DEFAULT | LP5521_EXEC_RUN)

/* Status */
#define LP5521_EXT_CLK_USED		0x08

/* default R channel current register value */
#define LP5521_REG_R_CURR_DEFAULT	0xAF

/* Current index max step */
#define PATTERN_CURRENT_INDEX_STEP_HAL 255

/* Pattern Mode */
#define PATTERN_OFF	0
#define PATTERN_BLINK_ON	-1

/*GV DCM Felica Pattern Mode*/
#define PATTERN_FELICA_ON 101
#define PATTERN_GPS_ON 102

/*GK Favorite MissedNoit Pattern Mode*/
#define PATTERN_FAVORITE_MISSED_NOTI 14

/*GK ATT Power off charged Mode (charge complete brightness 50%)*/
#define PATTERN_CHARGING_COMPLETE_50 15
#define PATTERN_CHARGING_50 16

/* Program Commands */
#define CMD_SET_PWM			0x40
#define CMD_WAIT_LSB			0x00

#define MAX_BLINK_TIME			60000	/* 60 sec */

enum lp5521_wait_type {
	LP5521_CYCLE_INVALID,
	LP5521_CYCLE_50ms,
	LP5521_CYCLE_100ms,
	LP5521_CYCLE_200ms,
	LP5521_CYCLE_500ms,
	LP5521_CYCLE_700ms,
	LP5521_CYCLE_920ms,
	LP5521_CYCLE_982ms,
	LP5521_CYCLE_MAX,
};

struct lp5521_pattern_cmd {
	u8 r[LP5521_PROGRAM_LENGTH];
	u8 g[LP5521_PROGRAM_LENGTH];
	u8 b[LP5521_PROGRAM_LENGTH];
	unsigned pc_r;
	unsigned pc_g;
	unsigned pc_b;
};

struct lp5521_wait_param {
	unsigned cycle;
	unsigned limit;
	u8 cmd;
};

static const struct lp5521_wait_param lp5521_wait_params[LP5521_CYCLE_MAX] = {
	[LP5521_CYCLE_50ms] = {
		.cycle = 50,
		.limit = 3000,
		.cmd   = 0x43,
	},
	[LP5521_CYCLE_100ms] = {
		.cycle = 100,
		.limit = 6000,
		.cmd   = 0x46,
	},
	[LP5521_CYCLE_200ms] = {
		.cycle = 200,
		.limit = 10000,
		.cmd   = 0x4d,
	},
	[LP5521_CYCLE_500ms] = {
		.cycle = 500,
		.limit = 30000,
		.cmd   = 0x60,
	},
	[LP5521_CYCLE_700ms] = {
		.cycle = 700,
		.limit = 40000,
		.cmd   = 0x6d,
	},
	[LP5521_CYCLE_920ms] = {
		.cycle = 920,
		.limit = 50000,
		.cmd   = 0x7b,
	},
	[LP5521_CYCLE_982ms] = {
		.cycle = 982,
		.limit = 60000,
		.cmd   = 0x7f,
	},
};

#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
static struct lp5521_led_config lp5521_led_config_rev_a[] = {
	{
		.name = "R",
		.chan_nr	= 0,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "B",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "G",
		.chan_nr	= 2,
		.led_current	= 180,
		.max_current	= 180,
	},
};

static struct lp5521_led_pattern board_led_patterns_rev_a[] = {
	{
		/* ID_POWER_ON = 1 */
		.r = mode1_red,
		.g = mode1_green,
		.b = mode1_blue,
		.size_r = ARRAY_SIZE(mode1_red),
		.size_g = ARRAY_SIZE(mode1_green),
		.size_b = ARRAY_SIZE(mode1_blue),
	},
	{
		/* ID_LCD_ON = 2 */
		.r = mode2_red,
		.g = mode2_green,
		.b = mode2_blue,
		.size_r = ARRAY_SIZE(mode2_red),
		.size_g = ARRAY_SIZE(mode2_green),
		.size_b = ARRAY_SIZE(mode2_blue),
		},
	{
		/* ID_CHARGING = 3 */
		.r = mode3_red,
		.size_r = ARRAY_SIZE(mode3_red),
	},
	{
		/* ID_CHARGING_FULL = 4 */
		.g = mode4_green,
		.size_g = ARRAY_SIZE(mode4_green),
	},
	{
		/* ID_CALENDAR_REMIND = 5 */
		.r = mode5_red_rev_a,
		.g = mode5_green_rev_a,
		.size_r = ARRAY_SIZE(mode5_red_rev_a),
		.size_g = ARRAY_SIZE(mode5_green_rev_a),
	},
	{
		/* ID_POWER_OFF = 6 */
		.r = mode6_red,
		.g = mode6_green,
		.b = mode6_blue,
		.size_r = ARRAY_SIZE(mode6_red),
		.size_g = ARRAY_SIZE(mode6_green),
		.size_b = ARRAY_SIZE(mode6_blue),
	},
	{
		/* ID_MISSED_NOTI = 7 */
		.r = mode7_red,
		.g = mode7_green,
		.b = mode7_blue,
		.size_r = ARRAY_SIZE(mode7_red),
		.size_g = ARRAY_SIZE(mode7_green),
		.size_b = ARRAY_SIZE(mode7_blue),
	},

	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ALARM = 8 */
	},
	{
		/* ID_CALL_01 = 9 */
	},
	{
		/* ID_CALL_02 = 10 */
	},
	{
		/* ID_CALL_03 = 11 */
	},
	{
		/* ID_VOLUME_UP = 12 */
	},
	{
		/* ID_VOLUME_DOWN = 13 */
	},

	{
		/* ID_FAVORITE_MISSED_NOTI = 14 */
		.r = mode8_red,
		.g = mode8_green,
		.b = mode8_blue,
		.size_r = ARRAY_SIZE(mode8_red),
		.size_g = ARRAY_SIZE(mode8_green),
		.size_b = ARRAY_SIZE(mode8_blue),
	},
	{
		/* CHARGING_100_FOR_ATT = 15 (use chargerlogo, only AT&T) */
		.g = mode4_green_50,
		.size_g = ARRAY_SIZE(mode4_green_50),
	},
	{
		/* CHARGING_FOR_ATT = 16 (use chargerlogo, only AT&T) */
		.r = mode3_red_50,
		.size_r = ARRAY_SIZE(mode3_red_50),
	},
	{
		/* ID_MISSED_NOTI_PINK = 17 */
		.r = mode9_red_rev_a,
		.g = mode9_green_rev_a,
		.b = mode9_blue_rev_a,
		.size_r = ARRAY_SIZE(mode9_red_rev_a),
		.size_g = ARRAY_SIZE(mode9_green_rev_a),
		.size_b = ARRAY_SIZE(mode9_blue_rev_a),
	},
	{
		/* ID_MISSED_NOTI_BLUE = 18 */
		.r = mode10_red,
		.g = mode10_green,
		.b = mode10_blue,
		.size_r = ARRAY_SIZE(mode10_red),
		.size_g = ARRAY_SIZE(mode10_green),
		.size_b = ARRAY_SIZE(mode10_blue),
	},
	{
		/* ID_MISSED_NOTI_ORANGE = 19 */
		.r = mode11_red_rev_a,
		.g = mode11_green_rev_a,
		.b = mode11_blue_rev_a,
		.size_r = ARRAY_SIZE(mode11_red_rev_a),
		.size_g = ARRAY_SIZE(mode11_green_rev_a),
		.size_b = ARRAY_SIZE(mode11_blue_rev_a),
	},
	{
		/* ID_MISSED_NOTI_YELLOW = 20 */
		.r = mode12_red_rev_a,
		.g = mode12_green_rev_a,
		.b = mode12_blue_rev_a,
		.size_r = ARRAY_SIZE(mode12_red_rev_a),
		.size_g = ARRAY_SIZE(mode12_green_rev_a),
		.size_b = ARRAY_SIZE(mode12_blue_rev_a),
	},
};

static struct lp5521_led_config lp5521_led_config[] = {
	{
		.name = "R",
		.chan_nr	= 0,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "G",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "B",
		.chan_nr	= 2,
		.led_current	= 180,
		.max_current	= 180,
	},
};

static struct lp5521_led_pattern board_led_patterns[] = {
	{
		/* ID_POWER_ON = 1 */
		.r = mode1_red,
		.g = mode1_green,
		.b = mode1_blue,
		.size_r = ARRAY_SIZE(mode1_red),
		.size_g = ARRAY_SIZE(mode1_green),
		.size_b = ARRAY_SIZE(mode1_blue),
	},
	{
		/* ID_LCD_ON = 2 */
		.r = mode2_red,
		.g = mode2_green,
		.b = mode2_blue,
		.size_r = ARRAY_SIZE(mode2_red),
		.size_g = ARRAY_SIZE(mode2_green),
		.size_b = ARRAY_SIZE(mode2_blue),
		},
	{
		/* ID_CHARGING = 3 */
		.r = mode3_red,
		.size_r = ARRAY_SIZE(mode3_red),
	},
	{
		/* ID_CHARGING_FULL = 4 */
		.g = mode4_green,
		.size_g = ARRAY_SIZE(mode4_green),
	},
	{
		/* ID_CALENDAR_REMIND = 5 */
		.r = mode5_red,
		.g = mode5_green,
		.size_r = ARRAY_SIZE(mode5_red),
		.size_g = ARRAY_SIZE(mode5_green),
	},
	{
		/* ID_POWER_OFF = 6 */
		.r = mode6_red,
		.g = mode6_green,
		.b = mode6_blue,
		.size_r = ARRAY_SIZE(mode6_red),
		.size_g = ARRAY_SIZE(mode6_green),
		.size_b = ARRAY_SIZE(mode6_blue),
	},
	{
		/* ID_MISSED_NOTI = 7 */
		.r = mode7_red,
		.g = mode7_green,
		.b = mode7_blue,
		.size_r = ARRAY_SIZE(mode7_red),
		.size_g = ARRAY_SIZE(mode7_green),
		.size_b = ARRAY_SIZE(mode7_blue),
	},

	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ALARM = 8 */
	},
	{
		/* ID_CALL_01 = 9 */
	},
	{
		/* ID_CALL_02 = 10 */
	},
	{
		/* ID_CALL_03 = 11 */
	},
	{
		/* ID_VOLUME_UP = 12 */
	},
	{
		/* ID_VOLUME_DOWN = 13 */
	},

	{
		/* ID_FAVORITE_MISSED_NOTI = 14 */
		.r = mode8_red,
		.g = mode8_green,
		.b = mode8_blue,
		.size_r = ARRAY_SIZE(mode8_red),
		.size_g = ARRAY_SIZE(mode8_green),
		.size_b = ARRAY_SIZE(mode8_blue),
	},
	{
		/* CHARGING_100_FOR_ATT = 15 (use chargerlogo, only AT&T) */
		.g = mode4_green_50,
		.size_g = ARRAY_SIZE(mode4_green_50),
	},
	{
		/* CHARGING_FOR_ATT = 16 (use chargerlogo, only AT&T) */
		.r = mode3_red_50,
		.size_r = ARRAY_SIZE(mode3_red_50),
	},
	{
		/* ID_MISSED_NOTI_PINK = 17 */
		.r = mode9_red,
		.g = mode9_green,
		.b = mode9_blue,
		.size_r = ARRAY_SIZE(mode9_red),
		.size_g = ARRAY_SIZE(mode9_green),
		.size_b = ARRAY_SIZE(mode9_blue),
	},
	{
		/* ID_MISSED_NOTI_BLUE = 18 */
		.r = mode10_red,
		.g = mode10_green,
		.b = mode10_blue,
		.size_r = ARRAY_SIZE(mode10_red),
		.size_g = ARRAY_SIZE(mode10_green),
		.size_b = ARRAY_SIZE(mode10_blue),
	},
	{
		/* ID_MISSED_NOTI_ORANGE = 19 */
		.r = mode11_red,
		.g = mode11_green,
		.b = mode11_blue,
		.size_r = ARRAY_SIZE(mode11_red),
		.size_g = ARRAY_SIZE(mode11_green),
		.size_b = ARRAY_SIZE(mode11_blue),
	},
	{
		/* ID_MISSED_NOTI_YELLOW = 20 */
		.r = mode12_red,
		.g = mode12_green,
		.b = mode12_blue,
		.size_r = ARRAY_SIZE(mode12_red),
		.size_g = ARRAY_SIZE(mode12_green),
		.size_b = ARRAY_SIZE(mode12_blue),
	},
};

#else

static struct lp5521_led_config lp5521_led_config[] = {
	{
		.name = "R",
		.chan_nr	= 0,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "G",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "B",
		.chan_nr	= 2,
		.led_current	= 180,
		.max_current	= 180,
	},
};

static struct lp5521_led_pattern board_led_patterns[] = {
	{
		/* ID_POWER_ON = 1 */
		.r = mode1_red,
		.g = mode1_green,
		.b = mode1_blue,
		.size_r = ARRAY_SIZE(mode1_red),
		.size_g = ARRAY_SIZE(mode1_green),
		.size_b = ARRAY_SIZE(mode1_blue),
	},
	{
		/* ID_LCD_ON = 2 */
		.r = mode2_red,
		.g = mode2_green,
		.b = mode2_blue,
		.size_r = ARRAY_SIZE(mode2_red),
		.size_g = ARRAY_SIZE(mode2_green),
		.size_b = ARRAY_SIZE(mode2_blue),
		},
	{
		/* ID_CHARGING = 3 */
		.r = mode3_red,
		.size_r = ARRAY_SIZE(mode3_red),
	},
	{
		/* ID_CHARGING_FULL = 4 */
		.g = mode4_green,
		.size_g = ARRAY_SIZE(mode4_green),
	},
	{
		/* ID_CALENDAR_REMIND = 5 */
		.r = mode5_red,
		.g = mode5_green,
		.size_r = ARRAY_SIZE(mode5_red),
		.size_g = ARRAY_SIZE(mode5_green),
	},
	{
		/* ID_POWER_OFF = 6 */
		.r = mode6_red,
		.g = mode6_green,
		.b = mode6_blue,
		.size_r = ARRAY_SIZE(mode6_red),
		.size_g = ARRAY_SIZE(mode6_green),
		.size_b = ARRAY_SIZE(mode6_blue),
	},
	{
		/* ID_MISSED_NOTI = 7 */
		.r = mode7_red,
		.g = mode7_green,
		.b = mode7_blue,
		.size_r = ARRAY_SIZE(mode7_red),
		.size_g = ARRAY_SIZE(mode7_green),
		.size_b = ARRAY_SIZE(mode7_blue),
	},
#if defined(CONFIG_MACH_APQ8064_GK_KR) || defined(CONFIG_MACH_APQ8064_GKATT) || defined(CONFIG_MACH_APQ8064_GKOPENHK)  || defined(CONFIG_MACH_APQ8064_GV_KR) || defined(CONFIG_MACH_APQ8064_GKOPENTW) || defined(CONFIG_MACH_APQ8064_GKSHBSG) || defined(CONFIG_MACH_APQ8064_GKOPENEU)
	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ALARM = 8 */
	},
	{
		/* ID_CALL_01 = 9 */
	},
	{
		/* ID_CALL_02 = 10 */
	},
	{
		/* ID_CALL_03 = 11 */
	},
	{
		/* ID_VOLUME_UP = 12 */
	},
	{
		/* ID_VOLUME_DOWN = 13 */
	},
#endif
	{
		/* ID_FAVORITE_MISSED_NOTI = 14 */
		.r = mode8_red,
		.g = mode8_green,
		.b = mode8_blue,
		.size_r = ARRAY_SIZE(mode8_red),
		.size_g = ARRAY_SIZE(mode8_green),
		.size_b = ARRAY_SIZE(mode8_blue),
	},
	{
		/* CHARGING_100_FOR_ATT = 15 (use chargerlogo, only AT&T) */
		.g = mode4_green_50,
		.size_g = ARRAY_SIZE(mode4_green_50),
	},
	{
		/* CHARGING_FOR_ATT = 16 (use chargerlogo, only AT&T) */
		.r = mode3_red_50,
		.size_r = ARRAY_SIZE(mode3_red_50),
	},
	{
		/* ID_MISSED_NOTI_PINK = 17 */
		.r = mode9_red,
		.g = mode9_green,
		.b = mode9_blue,
		.size_r = ARRAY_SIZE(mode9_red),
		.size_g = ARRAY_SIZE(mode9_green),
		.size_b = ARRAY_SIZE(mode9_blue),
	},
	{
		/* ID_MISSED_NOTI_BLUE = 18 */
		.r = mode10_red,
		.g = mode10_green,
		.b = mode10_blue,
		.size_r = ARRAY_SIZE(mode10_red),
		.size_g = ARRAY_SIZE(mode10_green),
		.size_b = ARRAY_SIZE(mode10_blue),
	},
	{
		/* ID_MISSED_NOTI_ORANGE = 19 */
		.r = mode11_red,
		.g = mode11_green,
		.b = mode11_blue,
		.size_r = ARRAY_SIZE(mode11_red),
		.size_g = ARRAY_SIZE(mode11_green),
		.size_b = ARRAY_SIZE(mode11_blue),
	},
	{
		/* ID_MISSED_NOTI_YELLOW = 20 */
		.r = mode12_red,
		.g = mode12_green,
		.b = mode12_blue,
		.size_r = ARRAY_SIZE(mode12_red),
		.size_g = ARRAY_SIZE(mode12_green),
		.size_b = ARRAY_SIZE(mode12_blue),
	},
};
#endif

#define LP5521_CONFIGS	(LP5521_PWM_HF | LP5521_PWRSAVE_EN | \
			LP5521_CP_MODE_AUTO | \
			LP5521_CLOCK_INT)

struct lp5521_chip      *chip;

static void lp5521_enable(bool state)
{
	int ret = 0;
	LP5521_INFO_MSG("LP5521: [%s] state = %d\n", __func__, state);

	if (!gpio_is_valid(chip->rgb_led_en)) {
		pr_err("rgb_led_en gpio_request failed for %d ret=%d\n", chip->rgb_led_en, ret);
		return;
	}

    if(lge_get_board_revno()> HW_REV_EVB2)
    {
        if(state){
	        gpio_set_value(chip->rgb_led_en, 1);
	        LP5521_INFO_MSG("LP5521: [%s] RGB_EN(gpio #%d) set to HIGH\n", __func__, chip->rgb_led_en);
        }
        else{
	        gpio_set_value(chip->rgb_led_en, 0);
	        LP5521_INFO_MSG("LP5521: [%s] RGB_EN(gpio #%d) set to LOW\n", __func__, chip->rgb_led_en);
        }
    }
	return;
}

static struct lp5521_platform_data lp5521_pdata = {
	.led_config = lp5521_led_config,
	.num_channels = ARRAY_SIZE(lp5521_led_config),
	.clock_mode = LP5521_CLOCK_INT,
	.update_config = LP5521_CONFIGS,
	.patterns = board_led_patterns,
	.num_patterns = ARRAY_SIZE(board_led_patterns),
	.enable = lp5521_enable
};


static inline struct lp5521_led *cdev_to_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct lp5521_led, cdev);
}

static inline struct lp5521_chip *engine_to_lp5521(struct lp5521_engine *engine)
{
	return container_of(engine, struct lp5521_chip,
			    engines[engine->id - 1]);
}

static inline struct lp5521_chip *led_to_lp5521(struct lp5521_led *led)
{
	return container_of(led, struct lp5521_chip,
			    leds[led->id]);
}

static void lp5521_led_brightness_work(struct work_struct *work);

static inline int lp5521_write(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int lp5521_read(struct i2c_client *client, u8 reg, u8 *buf)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return -EIO;

	*buf = ret;
	return 0;
}

static int lp5521_set_engine_mode(struct lp5521_engine *engine, u8 mode)
{
	struct lp5521_chip *chip = engine_to_lp5521(engine);
	struct i2c_client *client = chip->client;
	int ret;
	u8 engine_state;

	/* Only transition between RUN and DIRECT mode are handled here */
	if (mode == LP5521_CMD_LOAD)
		return 0;

	if (mode == LP5521_CMD_DISABLED)
		mode = LP5521_CMD_DIRECT;

	ret = lp5521_read(client, LP5521_REG_OP_MODE, &engine_state);
	if (ret < 0)
		return ret;

	/* set mode only for this engine */
	engine_state &= ~(engine->engine_mask);
	mode &= engine->engine_mask;
	engine_state |= mode;
	return lp5521_write(client, LP5521_REG_OP_MODE, engine_state);
}

static int lp5521_load_program(struct lp5521_engine *eng, const u8 *pattern)
{
	struct lp5521_chip *chip = engine_to_lp5521(eng);
	struct i2c_client *client = chip->client;
	int ret;
	int addr;
	u8 mode = 0;

	/* move current engine to direct mode and remember the state */
	ret = lp5521_set_engine_mode(eng, LP5521_CMD_DIRECT);
	if (ret)
		return ret;

	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);
	ret = lp5521_read(client, LP5521_REG_OP_MODE, &mode);
	if (ret)
		return ret;

	/* For loading, all the engines to load mode */
	lp5521_write(client, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);
	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);
	lp5521_write(client, LP5521_REG_OP_MODE, LP5521_CMD_LOAD);
	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);

	addr = LP5521_PROG_MEM_BASE + eng->prog_page * LP5521_PROG_MEM_SIZE;
	i2c_smbus_write_i2c_block_data(client,
				addr,
				LP5521_PROG_MEM_SIZE,
				pattern);

	return lp5521_write(client, LP5521_REG_OP_MODE, mode);
}

static int lp5521_set_led_current(struct lp5521_chip *chip, int led, u8 curr)
{
	return lp5521_write(chip->client,
		    LP5521_REG_LED_CURRENT_BASE + chip->leds[led].chan_nr,
		    curr);
}

static void lp5521_init_engine(struct lp5521_chip *chip)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(chip->engines); i++) {
		chip->engines[i].id = i + 1;
		chip->engines[i].engine_mask = LP5521_ENG_MASK_BASE >> (i * 2);
		chip->engines[i].prog_page = i;
	}
}

static int lp5521_configure(struct i2c_client *client)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	int ret;
	u8 cfg;

	lp5521_init_engine(chip);

	/* Set all PWMs to direct control mode */
	ret = lp5521_write(client, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);

	cfg = chip->pdata->update_config ?
		: (LP5521_PWRSAVE_EN | LP5521_CP_MODE_AUTO | LP5521_R_TO_BATT);
	ret |= lp5521_write(client, LP5521_REG_CONFIG, cfg);

	/* Initialize all channels PWM to zero -> leds off */
	ret |= lp5521_write(client, LP5521_REG_R_PWM, 0);
	ret |= lp5521_write(client, LP5521_REG_G_PWM, 0);
	ret |= lp5521_write(client, LP5521_REG_B_PWM, 0);

	/* Set engines are set to run state when OP_MODE enables engines */
	ret |= lp5521_write(client, LP5521_REG_ENABLE,
			LP5521_ENABLE_RUN_PROGRAM);
	/* enable takes 500us. 1 - 2 ms leaves some margin */
	usleep_range(1000, 2000);

	return ret;
}

static int lp5521_run_selftest(struct lp5521_chip *chip, char *buf)
{
	int ret;
	u8 status;

	ret = lp5521_read(chip->client, LP5521_REG_STATUS, &status);
	if (ret < 0)
		return ret;

	/* Check that ext clock is really in use if requested */
	if (chip->pdata && chip->pdata->clock_mode == LP5521_CLOCK_EXT)
		if  ((status & LP5521_EXT_CLK_USED) == 0)
			return -EIO;
	return 0;
}

static void lp5521_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct lp5521_led *led = cdev_to_led(cdev);
	static unsigned long log_counter;
	led->brightness = (u8)brightness;
	if (log_counter++ % 100 == 0) {
		LP5521_INFO_MSG("[%s] brightness : %d", __func__, brightness);
	}
	schedule_work(&led->brightness_work);
}

static void lp5521_led_brightness_work(struct work_struct *work)
{
	struct lp5521_led *led = container_of(work,
					      struct lp5521_led,
					      brightness_work);
	struct lp5521_chip *chip = led_to_lp5521(led);
	struct i2c_client *client = chip->client;

	mutex_lock(&chip->lock);
	lp5521_write(client, LP5521_REG_LED_PWM_BASE + led->chan_nr,
		led->brightness);
	mutex_unlock(&chip->lock);
}

/* Detect the chip by setting its ENABLE register and reading it back. */
static int lp5521_detect(struct i2c_client *client)
{
	int ret;
	u8 buf = 0;

	ret = lp5521_write(client, LP5521_REG_ENABLE, LP5521_ENABLE_DEFAULT);
	if (ret)
		return ret;
	/* enable takes 500us. 1 - 2 ms leaves some margin */
	usleep_range(1000, 2000);
	ret = lp5521_read(client, LP5521_REG_ENABLE, &buf);
	if (ret)
		return ret;
	if (buf != LP5521_ENABLE_DEFAULT)
		return -ENODEV;

	return 0;
}

/* Set engine mode and create appropriate sysfs attributes, if required. */
static int lp5521_set_mode(struct lp5521_engine *engine, u8 mode)
{
	int ret = 0;

	/* if in that mode already do nothing, except for run */
	if (mode == engine->mode && mode != LP5521_CMD_RUN)
		return 0;

	if (mode == LP5521_CMD_RUN) {
		ret = lp5521_set_engine_mode(engine, LP5521_CMD_RUN);
	} else if (mode == LP5521_CMD_LOAD) {
		lp5521_set_engine_mode(engine, LP5521_CMD_DISABLED);
		lp5521_set_engine_mode(engine, LP5521_CMD_LOAD);
	} else if (mode == LP5521_CMD_DISABLED) {
		lp5521_set_engine_mode(engine, LP5521_CMD_DISABLED);
	}

	engine->mode = mode;

	return ret;
}

static int lp5521_do_store_load(struct lp5521_engine *engine,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip = engine_to_lp5521(engine);
	struct i2c_client *client = chip->client;
	int  ret, nrchars, offset = 0, i = 0;
	char c[3];
	unsigned cmd;
	u8 pattern[LP5521_PROGRAM_LENGTH] = {0};

	while ((offset < len - 1) && (i < LP5521_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(buf + offset, "%2s%n ", c, &nrchars);
		if (ret != 2)
			goto fail;
		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto fail;
		pattern[i] = (u8)cmd;

		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto fail;

	mutex_lock(&chip->lock);
	if (engine->mode == LP5521_CMD_LOAD)
		ret = lp5521_load_program(engine, pattern);
	else
		ret = -EINVAL;
	mutex_unlock(&chip->lock);

	if (ret) {
		dev_err(&client->dev, "failed loading pattern\n");
		return ret;
	}

	return len;
fail:
	dev_err(&client->dev, "wrong pattern format\n");
	return -EINVAL;
}

static ssize_t store_engine_load(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	return lp5521_do_store_load(&chip->engines[nr - 1], buf, len);
}

#define store_load(nr)							\
static ssize_t store_engine##nr##_load(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_load(dev, attr, buf, len, nr);		\
}
store_load(1)
store_load(2)
store_load(3)

static ssize_t show_engine_mode(struct device *dev,
				struct device_attribute *attr,
				char *buf, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	switch (chip->engines[nr - 1].mode) {
	case LP5521_CMD_RUN:
		return sprintf(buf, "run\n");
	case LP5521_CMD_LOAD:
		return sprintf(buf, "load\n");
	case LP5521_CMD_DISABLED:
		return sprintf(buf, "disabled\n");
	default:
		return sprintf(buf, "disabled\n");
	}
}

#define show_mode(nr)							\
static ssize_t show_engine##nr##_mode(struct device *dev,		\
				    struct device_attribute *attr,	\
				    char *buf)				\
{									\
	return show_engine_mode(dev, attr, buf, nr);			\
}
show_mode(1)
show_mode(2)
show_mode(3)

static ssize_t store_engine_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	struct lp5521_engine *engine = &chip->engines[nr - 1];
	mutex_lock(&chip->lock);

	if (!strncmp(buf, "run", 3))
		lp5521_set_mode(engine, LP5521_CMD_RUN);
	else if (!strncmp(buf, "load", 4))
		lp5521_set_mode(engine, LP5521_CMD_LOAD);
	else if (!strncmp(buf, "disabled", 8))
		lp5521_set_mode(engine, LP5521_CMD_DISABLED);

	mutex_unlock(&chip->lock);
	return len;
}

#define store_mode(nr)							\
static ssize_t store_engine##nr##_mode(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_mode(dev, attr, buf, len, nr);		\
}
store_mode(1)
store_mode(2)
store_mode(3)

static ssize_t show_max_current(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led = cdev_to_led(led_cdev);

	return sprintf(buf, "%d\n", led->max_current);
}

static ssize_t show_current(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led = cdev_to_led(led_cdev);

	return sprintf(buf, "%d\n", led->led_current);
}

static ssize_t store_current(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led = cdev_to_led(led_cdev);
	struct lp5521_chip *chip = led_to_lp5521(led);
	ssize_t ret;
	unsigned long curr;

	if (kstrtoul(buf, 0, &curr))
		return -EINVAL;

	if (curr > led->max_current)
		return -EINVAL;

	mutex_lock(&chip->lock);
	ret = lp5521_set_led_current(chip, led->id, curr);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		return ret;

	led->led_current = (u8)curr;
	LP5521_INFO_MSG("[%s] brightness : %d", __func__, (u8)curr);

	return len;
}

static ssize_t lp5521_selftest(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->lock);
	ret = lp5521_run_selftest(chip, buf);
	mutex_unlock(&chip->lock);
	return sprintf(buf, "%s\n", ret ? "FAIL" : "OK");
}

static void lp5521_clear_program_memory(struct i2c_client *cl)
{
	int i;
	u8 rgb_mem[] = {
		LP5521_REG_R_PROG_MEM,
		LP5521_REG_G_PROG_MEM,
		LP5521_REG_B_PROG_MEM,
	};

	for (i = 0; i < ARRAY_SIZE(rgb_mem); i++) {
		lp5521_write(cl, rgb_mem[i], 0);
		lp5521_write(cl, rgb_mem[i] + 1, 0);
	}
}

static void lp5521_write_program_memory(struct i2c_client *cl,
				u8 base, const u8 *rgb, int size)
{
	int i;

	if (!rgb || size <= 0)
		return;

	for (i = 0; i < size; i++)
		lp5521_write(cl, base + i, *(rgb + i));

	lp5521_write(cl, base + i, 0);
	lp5521_write(cl, base + i + 1, 0);
}

static inline struct lp5521_led_pattern *lp5521_get_pattern
					(struct lp5521_chip *chip, u8 offset)
{
	struct lp5521_led_pattern *ptn;
	ptn = chip->pdata->patterns + (offset - 1);
	return ptn;
}

static void _run_led_pattern(struct lp5521_chip *chip,
			struct lp5521_led_pattern *ptn)
{
	struct i2c_client *cl = chip->client;

	lp5521_write(cl, LP5521_REG_OP_MODE, LP5521_CMD_LOAD);
	usleep_range(1000, 2000);

	lp5521_clear_program_memory(cl);

	lp5521_write_program_memory(cl, LP5521_REG_R_PROG_MEM,
				ptn->r, ptn->size_r);
#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
    if(lge_get_board_revno() == HW_REV_B)
    {
	    lp5521_write_program_memory(cl, LP5521_REG_B_PROG_MEM,
				ptn->g, ptn->size_g);
	    lp5521_write_program_memory(cl, LP5521_REG_G_PROG_MEM,
				ptn->b, ptn->size_b);
    }
    else
    {
	    lp5521_write_program_memory(cl, LP5521_REG_G_PROG_MEM,
				ptn->g, ptn->size_g);
	    lp5521_write_program_memory(cl, LP5521_REG_B_PROG_MEM,
				ptn->b, ptn->size_b);
    }
#else
	lp5521_write_program_memory(cl, LP5521_REG_G_PROG_MEM,
				ptn->g, ptn->size_g);
	lp5521_write_program_memory(cl, LP5521_REG_B_PROG_MEM,
				ptn->b, ptn->size_b);
#endif

	lp5521_write(cl, LP5521_REG_OP_MODE, LP5521_CMD_RUN);
	usleep_range(1000, 2000);
	lp5521_write(cl, LP5521_REG_ENABLE, LP5521_ENABLE_RUN_PROGRAM);
}

static void lp5521_run_led_pattern(int mode, struct lp5521_chip *chip)
{
	struct lp5521_led_pattern *ptn;
	struct i2c_client *cl = chip->client;
	int num_patterns = chip->pdata->num_patterns;

	chip->id_pattern_play = mode;

    #ifdef CONFIG_MACH_APQ8064_GVDCM
	if(mode == PATTERN_FELICA_ON  || mode == PATTERN_GPS_ON)
	{
		mode = num_patterns - (PATTERN_GPS_ON - mode);
	}
	#endif

#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
#if 0
	/* this process is not need, because dummy pattern defined in board file */
	if (mode == PATTERN_FAVORITE_MISSED_NOTI || mode == PATTERN_CHARGING_COMPLETE_50 || mode == PATTERN_CHARGING_50) {
		mode = num_patterns - (PATTERN_CHARGING_50 - mode);
	}
#endif
#endif
	if (mode > num_patterns || !(chip->pdata->patterns)) {
		chip->id_pattern_play = PATTERN_OFF;
		LP5521_INFO_MSG("[%s] invalid pattern!", __func__);
		return;
	}

	if (mode == PATTERN_OFF) {
		lp5521_write(cl, LP5521_REG_ENABLE, LP5521_ENABLE_DEFAULT);
		usleep_range(1000, 2000);
		lp5521_write(cl, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);
		LP5521_INFO_MSG("[%s] PATTERN_PLAY_OFF", __func__);
	} else {
		ptn = lp5521_get_pattern(chip, mode);
		if (!ptn)
			return;

		_run_led_pattern(chip, ptn);
		LP5521_INFO_MSG("[%s] PATTERN_PLAY_ON", __func__);
	}
}

static u8 get_led_current_value(u8 current_index)
{
	return current_index_mapped_value[current_index];
}

static ssize_t show_led_pattern(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct lp5521_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%d\n", chip->id_pattern_play);
}

static ssize_t store_led_pattern(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long val;
	int ret;
	LP5521_INFO_MSG("[%s] pattern id : %s", __func__, buf);

	ret = strict_strtoul(buf, 10, &val);
	if (ret)
		return ret;

	lp5521_run_led_pattern(val, chip);

	return len;
}

static ssize_t show_led_current_index(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct lp5521_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	if (!chip)
		return 0;

	return sprintf(buf, "%d\n", chip->current_index);
}

static ssize_t store_led_current_index(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long val;
	int ret, i;
	u8 max_current, modify_current;

	LP5521_INFO_MSG("[%s] current index (0~255) : %s", __func__, buf);

	ret = strict_strtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val > PATTERN_CURRENT_INDEX_STEP_HAL || val < 0)
		return -EINVAL;

	if (!chip)
		return 0;

	chip->current_index = val;

	mutex_lock(&chip->lock);
	for (i = 0; i < LP5521_MAX_LEDS ; i++) {
		max_current = chip->leds[i].max_current;
		modify_current = get_led_current_value(val);
		if (modify_current > max_current)
			modify_current = max_current;
		LP5521_INFO_MSG("[%s] modify_current : %d", __func__,  modify_current);
		ret = lp5521_set_led_current(chip, i, modify_current);
		if (ret)
			break;
		chip->leds[i].led_current = modify_current;
	}
	mutex_unlock(&chip->lock);

	if (ret)
		return ret;
	return len;
}

static void _set_pwm_cmd(struct lp5521_pattern_cmd *cmd, unsigned int color)
{
	u8 r = (color >> 16) & 0xFF;
	u8 g = (color >> 8) & 0xFF;
	u8 b = color & 0xFF;

	cmd->r[cmd->pc_r++] = CMD_SET_PWM;
	cmd->r[cmd->pc_r++] = r;
	cmd->g[cmd->pc_g++] = CMD_SET_PWM;
	cmd->g[cmd->pc_g++] = g;
	cmd->b[cmd->pc_b++] = CMD_SET_PWM;
	cmd->b[cmd->pc_b++] = b;
}

static enum lp5521_wait_type _find_wait_cycle_type(unsigned int ms)
{
	int i;

	for (i = LP5521_CYCLE_50ms ; i < LP5521_CYCLE_MAX ; i++) {
		if (ms > lp5521_wait_params[i-1].limit &&
		    ms <= lp5521_wait_params[i].limit)
			return i;
	}

	return LP5521_CYCLE_INVALID;
}

static void _set_wait_cmd(struct lp5521_pattern_cmd *cmd,
			unsigned int ms, u8 jump)
{
	enum lp5521_wait_type type = _find_wait_cycle_type(ms);
	unsigned int loop = ms / lp5521_wait_params[type].cycle;
	u8 cmd_msb = lp5521_wait_params[type].cmd;
	u8 msb;
	u8 lsb;
	u16 branch;

	WARN_ON(!cmd_msb);
	WARN_ON(loop > 64);

	/* wait command */
	cmd->r[cmd->pc_r++] = cmd_msb;
	cmd->r[cmd->pc_r++] = CMD_WAIT_LSB;
	cmd->g[cmd->pc_g++] = cmd_msb;
	cmd->g[cmd->pc_g++] = CMD_WAIT_LSB;
	cmd->b[cmd->pc_b++] = cmd_msb;
	cmd->b[cmd->pc_b++] = CMD_WAIT_LSB;

	/* branch command : if wait time is bigger than cycle msec,
			branch is used for command looping */
	if (loop > 1) {
		branch = (5 << 13) | ((loop - 1) << 7) | jump;
		msb = (branch >> 8) & 0xFF;
		lsb = branch & 0xFF;

		cmd->r[cmd->pc_r++] = msb;
		cmd->r[cmd->pc_r++] = lsb;
		cmd->g[cmd->pc_g++] = msb;
		cmd->g[cmd->pc_g++] = lsb;
		cmd->b[cmd->pc_b++] = msb;
		cmd->b[cmd->pc_b++] = lsb;
	}
}

static inline bool _is_pc_overflow(struct lp5521_led_pattern *ptn)
{
	return (ptn->size_r >= LP5521_PROGRAM_LENGTH ||
		ptn->size_g >= LP5521_PROGRAM_LENGTH ||
		ptn->size_b >= LP5521_PROGRAM_LENGTH);
}

static ssize_t store_led_blink(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	unsigned int rgb = 0;
	unsigned int on = 0;
	unsigned int off = 0;
	struct lp5521_led_pattern ptn = { };
	struct lp5521_pattern_cmd cmd = { };
	u8 jump_pc = 0;

	sscanf(buf, "0x%06x %d %d", &rgb, &on, &off);
	LP5521_INFO_MSG("[%s] rgb=0x%06x, on=%d, off=%d\n",__func__, rgb, on, off);
	lp5521_run_led_pattern(PATTERN_OFF, chip);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on || !off) {
		chip->id_pattern_play = PATTERN_OFF;
		return len;
	} else {
		chip->id_pattern_play = PATTERN_BLINK_ON;
	}

	/* on */
	_set_pwm_cmd(&cmd, rgb);
	_set_wait_cmd(&cmd, on, jump_pc);
	jump_pc = cmd.pc_r / 2; /* 16bit size program counter */

	/* off */
	_set_pwm_cmd(&cmd, 0);
	_set_wait_cmd(&cmd, off, jump_pc);

	ptn.r = cmd.r;
	ptn.size_r = cmd.pc_r;
	ptn.g = cmd.g;
	ptn.size_g = cmd.pc_g;
	ptn.b = cmd.b;
	ptn.size_b = cmd.pc_b;

	WARN_ON(_is_pc_overflow(&ptn));

	_run_led_pattern(chip, &ptn);

	return len;
}

/* led class device attributes */
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR, show_current, store_current);
static DEVICE_ATTR(max_current, S_IRUGO , show_max_current, NULL);

static struct attribute *lp5521_led_attributes[] = {
	&dev_attr_led_current.attr,
	&dev_attr_max_current.attr,
	NULL,
};

static struct attribute_group lp5521_led_attribute_group = {
	.attrs = lp5521_led_attributes
};

/* device attributes */
static DEVICE_ATTR(engine1_mode, S_IRUGO | S_IWUSR,
		   show_engine1_mode, store_engine1_mode);
static DEVICE_ATTR(engine2_mode, S_IRUGO | S_IWUSR,
		   show_engine2_mode, store_engine2_mode);
static DEVICE_ATTR(engine3_mode, S_IRUGO | S_IWUSR,
		   show_engine3_mode, store_engine3_mode);
static DEVICE_ATTR(engine1_load, S_IWUSR, NULL, store_engine1_load);
static DEVICE_ATTR(engine2_load, S_IWUSR, NULL, store_engine2_load);
static DEVICE_ATTR(engine3_load, S_IWUSR, NULL, store_engine3_load);
static DEVICE_ATTR(selftest, S_IRUGO, lp5521_selftest, NULL);
static DEVICE_ATTR(led_pattern, S_IRUGO | S_IWUSR, show_led_pattern, store_led_pattern);
static DEVICE_ATTR(led_blink, S_IRUGO | S_IWUSR, NULL, store_led_blink);
static DEVICE_ATTR(led_current_index, S_IRUGO | S_IWUSR, show_led_current_index, store_led_current_index);

static struct attribute *lp5521_attributes[] = {
	&dev_attr_engine1_mode.attr,
	&dev_attr_engine2_mode.attr,
	&dev_attr_engine3_mode.attr,
	&dev_attr_selftest.attr,
	&dev_attr_engine1_load.attr,
	&dev_attr_engine2_load.attr,
	&dev_attr_engine3_load.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_current_index.attr,
	NULL
};

static const struct attribute_group lp5521_group = {
	.attrs = lp5521_attributes,
};

static int lp5521_register_sysfs(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	return sysfs_create_group(&dev->kobj, &lp5521_group);
}

static void lp5521_unregister_sysfs(struct i2c_client *client)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int i;

	sysfs_remove_group(&dev->kobj, &lp5521_group);

	for (i = 0; i < chip->num_leds; i++)
		sysfs_remove_group(&chip->leds[i].cdev.dev->kobj,
				&lp5521_led_attribute_group);
}

static int lp5521_init_led(struct lp5521_led *led,
				struct i2c_client *client,
				int chan, struct lp5521_platform_data pdata)
{
	struct device *dev = &client->dev;
	char name[32];
	int res;

	if (chan >= LP5521_MAX_LEDS)
		return -EINVAL;

	if (lp5521_pdata.led_config[chan].led_current == 0)
		return 0;

	led->led_current = lp5521_pdata.led_config[chan].led_current;
	led->max_current = lp5521_pdata.led_config[chan].max_current;
	led->chan_nr = lp5521_pdata.led_config[chan].chan_nr;

	if (led->chan_nr >= LP5521_MAX_LEDS) {
		dev_err(dev, "Use channel numbers between 0 and %d\n",
			LP5521_MAX_LEDS - 1);
		return -EINVAL;
	}

	led->cdev.brightness_set = lp5521_set_brightness;
	if (lp5521_pdata.led_config[chan].name) {
		led->cdev.name = lp5521_pdata.led_config[chan].name;
	} else {
		snprintf(name, sizeof(name), "%s:channel%d",
			lp5521_pdata.label ?: client->name, chan);
		led->cdev.name = name;
	}

	res = led_classdev_register(dev, &led->cdev);
	if (res < 0) {
		dev_err(dev, "couldn't register led on channel %d\n", chan);
		return res;
	}

	res = sysfs_create_group(&led->cdev.dev->kobj,
			&lp5521_led_attribute_group);
	if (res < 0) {
		dev_err(dev, "couldn't register current attribute\n");
		led_classdev_unregister(&led->cdev);
		return res;
	}
	return 0;
}

static int lp5521_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int ret =0, i, led;
	u8 buf = 0;

	LP5521_INFO_MSG("[%s] start\n", __func__);

#ifdef CONFIG_OF
    if (&client->dev.of_node)
    {
        chip = devm_kzalloc(&client->dev, sizeof(struct lp5521_chip), GFP_KERNEL);
        if (!chip)
        {
            pr_err("%s: Failed to allocate memory\n", __func__);
            return -ENOMEM;
        }
        if(lge_get_board_revno()> HW_REV_EVB2)
        {
            chip->rgb_led_en = of_get_named_gpio(client->dev.of_node, "ti,led_en", 0);
			LP5521_INFO_MSG("chip->rgb_led_en ==%d \n", chip->rgb_led_en);
            if (!gpio_is_valid(chip->rgb_led_en))
			{
				pr_err("Fail to get named gpio for rgb_led_en.\n");
                goto fail0;
			}
            else
            {
                ret = gpio_request(chip->rgb_led_en, "rgb_led_en");
                if(ret) {
                    pr_err("request reset gpio failed, rc=%d\n", ret);
                    gpio_free(chip->rgb_led_en);
                    goto fail0;
                }
            }
        }
    }
    else
    {
        dev_err(&client->dev, "lp5521 probe of_node fail\n");
        return -ENODEV;
    }
#else
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		LP5521_INFO_MSG("[%s] Can not allocate memory!\n", __func__);
		return -ENOMEM;
	}
#endif

	i2c_set_clientdata(client, chip);
	chip->client = client;

	mutex_init(&chip->lock);

#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_Z_KR)
    if(lge_get_board_revno() != HW_REV_B)
    {
        lp5521_pdata.led_config = lp5521_led_config_rev_a;
        lp5521_pdata.patterns = board_led_patterns_rev_a;
    }
#endif

	chip->pdata = &lp5521_pdata;

    if(lge_get_board_revno()> HW_REV_EVB2)
    {
        gpio_set_value((chip->rgb_led_en), 0);
        usleep_range(1000, 2000); /* Keep enable down at least 1ms */
#ifdef CONFIG_MACH_MSM8974_Z_KR		
        if(lge_get_board_revno() > HW_REV_D)
        {
            gpio_set_value((chip->rgb_led_en), 1);
            usleep_range(1000, 2000); /* Keep enable down at least 1ms */
        }
#else	  	
        gpio_set_value((chip->rgb_led_en), 1);
        usleep_range(1000, 2000); /* Keep enable down at least 1ms */
#endif		
    }

	lp5521_write(client, LP5521_REG_RESET, 0xff);
	usleep_range(10000, 20000); /*
				     * Exact value is not available. 10 - 20ms
				     * appears to be enough for reset.
				     */

	/*
	 * Make sure that the chip is reset by reading back the r channel
	 * current reg. This is dummy read is required on some platforms -
	 * otherwise further access to the R G B channels in the
	 * LP5521_REG_ENABLE register will not have any effect - strange!
	 */
	ret = lp5521_read(client, LP5521_REG_R_CURRENT, &buf);
	if (ret || buf != LP5521_REG_R_CURR_DEFAULT) {
		dev_err(&client->dev, "error in resetting chip\n");
		goto fail2;
	}
	usleep_range(10000, 20000);

	ret = lp5521_detect(client);

	if (ret) {
		dev_err(&client->dev, "Chip not found\n");
		goto fail2;
	}

	dev_info(&client->dev, "%s programmable led chip found\n", id->name);

	ret = lp5521_configure(client);
	if (ret < 0) {
		dev_err(&client->dev, "error configuring chip\n");
		goto fail1;
	}

	/* Initialize leds */
	chip->num_channels = lp5521_pdata.num_channels;
	chip->num_leds = 0;
	led = 0;
	for (i = 0; i < lp5521_pdata.num_channels; i++) {
		/* Do not initialize channels that are not connected */
		if (lp5521_pdata.led_config[i].led_current == 0)
			continue;

		ret = lp5521_init_led(&chip->leds[led], client, i, lp5521_pdata);
		if (ret) {
			dev_err(&client->dev, "error initializing leds\n");
			goto fail2;
		}
		chip->num_leds++;

		chip->leds[led].id = led;
		/* Set initial LED current */
		lp5521_set_led_current(chip, led,
				chip->leds[led].led_current);

		INIT_WORK(&(chip->leds[led].brightness_work),
			lp5521_led_brightness_work);

		led++;
	}

	/* Initialize current index for auto brightness (max step) */
	chip->current_index = PATTERN_CURRENT_INDEX_STEP_HAL;

	ret = lp5521_register_sysfs(client);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto fail2;
	}

	lp5521_run_led_pattern(1, chip); //1: Power On pattern number
	LP5521_INFO_MSG("[%s] pattern id : 1(Power on)", __func__);
	LP5521_INFO_MSG("[%s] complete\n", __func__);

	return ret;
fail2:
	for (i = 0; i < chip->num_leds; i++) {
		led_classdev_unregister(&chip->leds[i].cdev);
		cancel_work_sync(&chip->leds[i].brightness_work);
	}
fail1:
    if(lge_get_board_revno()> HW_REV_EVB2)
    {
        if (lp5521_pdata.enable)
	        lp5521_pdata.enable(0);
    }
fail0:
    gpio_free(chip->rgb_led_en);

	return ret;
}

static int lp5521_remove(struct i2c_client *client)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	int i;

	LP5521_INFO_MSG("[%s] start\n", __func__);
	lp5521_run_led_pattern(PATTERN_OFF, chip);
	lp5521_unregister_sysfs(client);

	for (i = 0; i < chip->num_leds; i++) {
		led_classdev_unregister(&chip->leds[i].cdev);
		cancel_work_sync(&chip->leds[i].brightness_work);
	}

    if(lge_get_board_revno()> HW_REV_EVB2)
    {
        if (chip->pdata->enable)
	        chip->pdata->enable(0);
    }
	if (chip->pdata->release_resources)
		chip->pdata->release_resources();

	devm_kfree(&client->dev, chip);
	LP5521_INFO_MSG("[%s] complete\n", __func__);
	return 0;
}

static void lp5521_shutdown(struct i2c_client *client)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	int i;

	if (!chip) {
		LP5521_INFO_MSG("[%s] null pointer check!\n", __func__);
		return;
	}

	LP5521_INFO_MSG("[%s] start\n", __func__);
	lp5521_set_led_current(chip, 0, 0);
	lp5521_set_led_current(chip, 1, 0);
	lp5521_set_led_current(chip, 2, 0);
	lp5521_run_led_pattern(PATTERN_OFF, chip);
	lp5521_unregister_sysfs(client);

	for (i = 0; i < chip->num_leds; i++) {
		led_classdev_unregister(&chip->leds[i].cdev);
		cancel_work_sync(&chip->leds[i].brightness_work);
	}

    if(lge_get_board_revno()> HW_REV_EVB2)
    {
	    if (chip->pdata->enable)
		    chip->pdata->enable(0);
    }
	if (chip->pdata->release_resources)
		chip->pdata->release_resources();

	devm_kfree(&client->dev, chip);
	LP5521_INFO_MSG("[%s] complete\n", __func__);
}

static int lp5521_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);

	if (!chip || !chip->pdata) {
		LP5521_INFO_MSG("[%s] null pointer check!\n", __func__);
	return 0;
}

	LP5521_INFO_MSG("[%s] id_pattern_play = %d\n", __func__, chip->id_pattern_play);
    if(lge_get_board_revno()> HW_REV_EVB2)
    {
	    if (chip->pdata->enable && chip->id_pattern_play == PATTERN_OFF) {
		    LP5521_INFO_MSG("[%s] RGB_EN set to LOW\n", __func__);
		    chip->pdata->enable(0);
	    }
    }
	return 0;
}

static int lp5521_resume(struct i2c_client *client)
{
	struct lp5521_chip *chip = i2c_get_clientdata(client);
	int ret = 0;

	if (!chip || !chip->pdata) {
		LP5521_INFO_MSG("[%s] null pointer check!\n", __func__);
		return 0;
	}

	LP5521_INFO_MSG("[%s] id_pattern_play = %d\n", __func__, chip->id_pattern_play);

    if(lge_get_board_revno()> HW_REV_EVB2)
    {
	    if (chip->pdata->enable && chip->id_pattern_play == PATTERN_OFF) {
		    LP5521_INFO_MSG("[%s] RGB_EN set to HIGH\n", __func__);
		    chip->pdata->enable(0);
		    usleep_range(1000, 2000); /* Keep enable down at least 1ms */
		    chip->pdata->enable(1);
		    usleep_range(1000, 2000); /* 500us abs min. */
		    lp5521_write(client, LP5521_REG_RESET, 0xff);
		    usleep_range(10000, 20000);
		    ret = lp5521_configure(client);
		    if (ret < 0) {
			    dev_err(&client->dev, "error configuring chip\n");
		    }
        }
    }
    else
    {
	    if (chip->id_pattern_play == PATTERN_OFF) {
		    LP5521_INFO_MSG("[%s] RGB_EN set to HIGH\n", __func__);
		    lp5521_write(client, LP5521_REG_RESET, 0xff);
		    usleep_range(10000, 20000);
		    ret = lp5521_configure(client);
		    if (ret < 0) {
			    dev_err(&client->dev, "error configuring chip\n");
            }
        }
    }

	return ret;
}

static const struct i2c_device_id lp5521_id[] = {
	{ "lp5521", 0 }, /* Three channel chip */
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id lp5521_match_table[] = {
	{ .compatible = "ti,lp5521",},
	{ },
};
#endif

static struct i2c_driver lp5521_driver = {
	.driver = {
        .owner  = THIS_MODULE,
		.name	= "lp5521",
#ifdef CONFIG_OF
		.of_match_table = lp5521_match_table,
#endif
	},
	.probe		= lp5521_probe,
	.remove		= lp5521_remove,
	.shutdown	= lp5521_shutdown,
	.suspend	= lp5521_suspend,
	.resume		= lp5521_resume,
	.id_table	= lp5521_id,
};

/*
 * module load/unload record keeping
 */

static int __init lp5521_dev_init(void)
{
	return i2c_add_driver(&lp5521_driver);
}
module_init(lp5521_dev_init);

static void __exit lp5521_dev_exit(void)
{
	i2c_del_driver(&lp5521_driver);
}
module_exit(lp5521_dev_exit);

MODULE_DEVICE_TABLE(i2c, lp5521_id);

MODULE_AUTHOR("Mathias Nyman, Yuri Zaporozhets, Samu Onkalo");
MODULE_DESCRIPTION("LP5521 LED engine");
MODULE_LICENSE("GPL v2");
