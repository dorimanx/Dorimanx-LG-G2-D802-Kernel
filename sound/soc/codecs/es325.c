/*
 * es325.c  --  Audience es325 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
#define CONFIG_HRTIMER_SLEEP_DELAYED

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/regulator/machine.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_slimbus.h>
#include <linux/clk.h>

#include <sound/esxxx.h>
#include <sound/es325.h>
#include <sound/es325-export.h>
#include <sound/es325-access.h>

#include <mach/msm_xo.h>
#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
#include <linux/hrtimer.h>
#include "../../../drivers/staging/android/timed_output.h"
#endif /* CONFIG_HRTIMER_SLEEP_DELAYED */

#include <linux/vmalloc.h>
#include <linux/fs.h>

#define BUS_TRANSACTIONS
//#define FIXED_CONFIG
#define ES325_TEXT_ENUMS
#define CONFIG_SLIMBUS_DEVICE_UP_COND // Qualcomm, Fusion3

#define WAKEUP_PIN_DETECT_TIME 30000
#define WAKEUP_PIN_DETECT_MAX_CNT 10

#define ES325_SLIM_CH_RX_OFFSET		152
#define ES325_SLIM_CH_TX_OFFSET		156
// #define ES325_SLIM_RX_PORTS		10
#define ES325_SLIM_RX_PORTS		6
#define ES325_SLIM_TX_PORTS		6

#define ES325_SLIM_1_PB		0
#define ES325_SLIM_1_CAP	1
#define ES325_SLIM_2_PB		2
#define ES325_SLIM_2_CAP	3
#define ES325_SLIM_3_PB		4
#define ES325_SLIM_3_CAP	5

#define ES325_SLIM_1_PB_MAX_CHANS	2
#define ES325_SLIM_1_CAP_MAX_CHANS	2
#define ES325_SLIM_2_PB_MAX_CHANS	2
#define ES325_SLIM_2_CAP_MAX_CHANS	2
#define ES325_SLIM_3_PB_MAX_CHANS	2
#define ES325_SLIM_3_CAP_MAX_CHANS	2

#define ES325_SLIM_1_PB_OFFSET	0
#define ES325_SLIM_2_PB_OFFSET	2
#define ES325_SLIM_3_PB_OFFSET	4
#define ES325_SLIM_1_CAP_OFFSET	0
#define ES325_SLIM_2_CAP_OFFSET	2
#define ES325_SLIM_3_CAP_OFFSET	4

#define ES325_NUM_CODEC_SLIM_DAIS	6

#define MAX_I2C_WR_ATTEMPT 3

/* Time in milliseconds to delay before executing a sleep request. */
#define ES325_SLEEP_DELAY 350

#define ES325_FW_LOAD_BUF_SZ 4

struct class *es325_class;
EXPORT_SYMBOL(es325_class);

enum es325_power_state {
	ES325_POWER_BOOT,		/* no firmware loaded */
	ES325_POWER_SLEEP,		/* chip is sleeping */
	ES325_POWER_SLEEP_PENDING,	/* sleep requested */
	ES325_POWER_AWAKE		/* chip is powered */
};

struct device *e325_codec_dev;

struct es325_slim_dai_data {
	unsigned int rate;
	unsigned int *ch_num;
	unsigned int ch_act;
	unsigned int ch_tot;
};

struct es325_slim_ch {
	u32	sph;
	u32	ch_num;
	u16	ch_h;
	u16	grph;
};

/* codec private data */
struct es325_priv {
	struct snd_soc_codec *codec;
	struct firmware *fw;

	struct esxxx_platform_data *pdata;

	struct i2c_client *this_client;
	struct slim_device *intf_client;
	struct slim_device *gen0_client;
	struct es325_slim_dai_data dai[ES325_NUM_CODEC_SLIM_DAIS];
	struct es325_slim_ch slim_rx[ES325_SLIM_RX_PORTS];
	struct es325_slim_ch slim_tx[ES325_SLIM_TX_PORTS];

	enum es325_power_state power_state;
#if !defined(CONFIG_HRTIMER_SLEEP_DELAYED)
	struct delayed_work sleep_work;
#endif /* CONFIG_HRTIMER_SLEEP_DELAYED */
	struct mutex power_lock;

	char fw_version[256];
} es325_priv = {
	.codec = NULL,
	.fw = NULL,
	.pdata = NULL,
	.this_client = NULL,
	.intf_client = NULL,
	.gen0_client = NULL,
	.power_state = ES325_POWER_BOOT
};

/* WCD9320
  * SLIM_TX1(128) ~ SLIM_TX16(143), SLIM_RX1(144) ~ SLIM_RX13(156)
  */
static int es325_slim_rx_port_to_ch[ES325_SLIM_RX_PORTS] = {
		152, 153, 154, 155, 134, 135
};
static int es325_slim_tx_port_to_ch[ES325_SLIM_TX_PORTS] = {
		156, 157, 144, 145, 144, 145
};
static int es325_slim_be_id[ES325_NUM_CODEC_SLIM_DAIS] = {
	ES325_SLIM_2_CAP, /* for ES325_SLIM_1_PB tx from es325 */
	ES325_SLIM_3_PB, /* for ES325_SLIM_1_CAP rx to es325 */
	ES325_SLIM_3_CAP, /* for ES325_SLIM_2_PB tx from es325 */
	-1, /* for ES325_SLIM_2_CAP */
	-1, /* for ES325_SLIM_3_PB */
	-1, /* for ES325_SLIM_3_CAP */
};

char* es325_fw_version = NULL;

// 1 is WCD9310 --> es325 --> MDM ro APQ, 0 is WCD9310 --> MDM or APQ
static unsigned int es325_tx1_route_ena = 0;

// 1 is APQ --> es325 --> WCD9310, 0 is APQ --> WCD9310
static unsigned int es325_rx1_route_ena = 0;

// 1 is MDM --> es325 --> WCD9310, 0 is MDM --> WCD9310
static unsigned int es325_rx2_route_ena = 0;

#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
static struct hrtimer sleep_delay_timer;
static struct work_struct work_es325_sleep;

static unsigned int sleep_delay_time = 200;
static int is_fw_download = 0;

static int es325_codec_sleep_delay_get_time(struct timed_output_dev *dev);
static void es325_codec_sleep_delay_enable(struct timed_output_dev *timed_dev, int value);
static enum hrtimer_restart es325_codec_sleep_delay_timer_func(struct hrtimer *timer);

static struct timed_output_dev sleep_delay = {
	.name = "es325_sleep_delay",
	.get_time = es325_codec_sleep_delay_get_time,
	.enable = es325_codec_sleep_delay_enable,
};
#endif

enum {
	ES325_ONE_MIC_CT = 0,
	ES325_ONE_MIC_DV,
	ES325_TWO_MIC_CT,
	ES325_TWO_MIC_FT,
	ES325_TWO_MIC_CT_ANC,
	ES325_TWO_MIC_CT_WB,
	ES325_TWO_MIC_FT_WB,
	ES325_TWO_MIC_CT_VOLTE,
	ES325_TWO_MIC_FT_VOLTE,
	ES325_TWO_MIC_CT_VOIP,
	ES325_TWO_MIC_FT_VOIP,
	ES325_ONE_MIC_HEADSET,
	ES325_ONE_CH_MUSIC_PLAYBACK,
	ES325_TWO_CH_MUSIC_PLAYBACK,
	ES325_ONE_MIC_VOICE_REC,
	ES325_PRI_MIC_LOOPBACK,
	ES325_SEC_MIC_LOOPBACK,
	ES325_ONE_MIC_VIDEO_REC,
	ES325_ONE_MIC_ASR,
	ES325_TWO_MIC_CT_ASR,
	ES325_TWO_MIC_CT_ASR_WB,
	ES325_TWO_MIC_FT_ASR,
	ES325_TWO_MIC_FT_ASR_WB,
	ES325_ONE_MIC_DESKTOP_ASR,
	ES325_ONE_MIC_DESKTOP_ASR_WB,
	ES325_TWO_MIC_CT_LOW_NS,
	ES325_ONE_MIC_DV_LOW_NS,
	ES325_BOTTOM_MIC_SEALED,
	ES325_TOP_MIC_SEALED,
	ES325_DUMMY,
	ES325_NB,
	ES325_WB,
	ES325_STOP
};

static bool is_wideband = false;
static long route_num_old = ES325_STOP;
static long es325_internal_route_num = ES325_STOP+1;
static u8 es325_internal_route_configs[ES325_STOP+1][60] = {
	{ // ES325_ONE_MIC_CT 1-Mic Close Talk
	0x90, 0x31, 0x00, 0x00, /* Route Preset */
	0x90, 0x31, 0x00, 0x14, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_DV 1-Mic DV
	0x90, 0x31, 0x00, 0x02, /* Route Preset */
	0x90, 0x31, 0x00, 0x16, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT 2-Mic Close Talk
	0x90, 0x31, 0x00, 0x01, /* Route Preset */
	0x90, 0x31, 0x00, 0x15, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT 2-Mic Far Talk
	0x90, 0x31, 0x00, 0x03, /* Route Preset */
	0x90, 0x31, 0x00, 0x17, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_ANC
	0x90, 0x31, 0x00, 0x10, /* Route Preset */
	0x90, 0x31, 0x00, 0x19, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_WB
	0x90, 0x31, 0x00, 0x01, /* Route Preset */
	0x90, 0x31, 0x00, 0x16, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT_WB
	0x90, 0x31, 0x00, 0x03, /* Route Preset */
	0x90, 0x31, 0x00, 0x18, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_VOLTE
	0x90, 0x31, 0x00, 0x01, /* Route Preset */
	0x90, 0x31, 0x00, 0x2A, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT_VOLTE
	0x90, 0x31, 0x00, 0x03, /* Route Preset */
	0x90, 0x31, 0x00, 0x2B, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_VOIP
	0x90, 0x31, 0x00, 0x01, /* Route Preset */
	0x90, 0x31, 0x00, 0x28, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT_VOIP
	0x90, 0x31, 0x00, 0x03, /* Route Preset */
	0x90, 0x31, 0x00, 0x29, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_HEADSET 1-Mic Headset
	0x90, 0x31, 0x00, 0x04, /* Route Preset */
	0x90, 0x31, 0x00, 0x1E, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_CH_MUSIC_PLAYBACK 1ch Multimedia Playback
	0x90, 0x31, 0x00, 0x05, /* Route Preset */
	0x90, 0x31, 0x00, 0x1F, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_CH_MUSIC_PLAYBACK 2ch Multimedia Playback
	0x90, 0x31, 0x00, 0x06, /* Route Preset */
	0x90, 0x31, 0x00, 0x20, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_VOICE_REC 1-Mic Voice Record
	0x90, 0x31, 0x00, 0x07, /* Route Preset */
	0x90, 0x31, 0x00, 0x21, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_PRI_MIC_LOOPBACK Loopback(1st Mic -> Receiver)
	0x90, 0x31, 0x00, 0x08, /* Route Preset */
	0x90, 0x31, 0x00, 0x22, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_SEC_MIC_LOOPBACK Loopback(2st Mic -> Speaker)
	0x90, 0x31, 0x00, 0x09, /* Route Preset */
	0x90, 0x31, 0x00, 0x23, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_VIDEO_REC 1-Mic Video Record
	0x90, 0x31, 0x00, 0x0A, /* Route Preset */
	0x90, 0x31, 0x00, 0x24, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_ASR ASR(Voice recognition)
	0x90, 0x31, 0x00, 0x0B, /* Route Preset */
	0x90, 0x31, 0x00, 0x25, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_ASR
	0x90, 0x31, 0x00, 0x3C, /* Route Preset */
	0x90, 0x31, 0x00, 0x3D, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_ASR_WB
	0x90, 0x31, 0x00, 0x3C, /* Route Preset */
	0x90, 0x31, 0x00, 0x3E, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT_ASR
	0x90, 0x31, 0x00, 0x3F, /* Route Preset */
	0x90, 0x31, 0x00, 0x40, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_FT_ASR_WB
	0x90, 0x31, 0x00, 0x3F, /* Route Preset */
	0x90, 0x31, 0x00, 0x41, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_DESKTOP_ASR
	0x90, 0x31, 0x00, 0x42, /* Route Preset */
	0x90, 0x31, 0x00, 0x43, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_DESKTOP_ASR_WB
	0x90, 0x31, 0x00, 0x42, /* Route Preset */
	0x90, 0x31, 0x00, 0x44, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_TWO_MIC_CT_LOW_NS 2-mic CT Low NS (TX VQOS:1, RX VQOS:2)
	0x90, 0x31, 0x00, 0x01, /* Route Preset */
	0x90, 0x31, 0x00, 0x2A, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_ONE_MIC_DV_LOW_NS 1-mic DV Low NS (TX VQOS:4, RX VQOS:2)
	0x90, 0x31, 0x00, 0x02, /* Route Preset */
	0x90, 0x31, 0x00, 0x2B, /* Algo Preset */
	0xff					/* End of Command */
	},
	{ // ES325_BOTTOM_MIC_SEALED Bottom MIC Sealed Test
	0xb0, 0x5c, 0x00, 0x01,
	0xb0, 0x5a, 0x28, 0xa0,
	0xb0, 0x5a, 0x14, 0xa2,
	0xb0, 0x5a, 0x04, 0xa4,
	0xb0, 0x5a, 0x40, 0xaa,
	0xb0, 0x5a, 0x44, 0xae,
	0xb0, 0x17, 0x00, 0x02,
	0xb0, 0x18, 0x00, 0x02,
	0x90, 0x1c, 0x00, 0x00, /* algo processing = off */
	0xff					/* End of Command */
	},
	{ // ES325_TOP_MIC_SEALED Top MIC Sealed Test
	0xb0, 0x5c, 0x00, 0x01,
	0xb0, 0x5a, 0x28, 0xa0,
	0xb0, 0x5a, 0x14, 0xa2,
	0xb0, 0x5a, 0x04, 0xa5,
	0xb0, 0x5a, 0x40, 0xaa,
	0xb0, 0x5a, 0x44, 0xae,
	0xb0, 0x17, 0x00, 0x02,
	0xb0, 0x18, 0x00, 0x02,
	0x90, 0x1c, 0x00, 0x00, /* algo processing = off */
	0xff					/* End of Command */
	},
	{ // ES325_DUMMY Dummy
	0x90, 0x31, 0x00, 0x0E, /* Route Preset */
	0xff					/* End of Command */
	},
	{ // ES325_NB
	0x90, 0x31, 0x00, 0x0E, /* Route Preset */
	0xff					/* End of Command */
	},
	{ // ES325_WB
	0x90, 0x31, 0x00, 0x0E, /* Route Preset */
	0xff					/* End of Command */
	},
	{ // ES325_STOP Clearing Route(STOP)
	0x90, 0x31, 0x00, 0x0F, /* Route Preset */
	0xff					/* End of Command */
	},
};

static const char *es325_internal_route_configs_text[ES325_STOP+1];

static struct snd_soc_dai_driver es325_dai[];

static struct clk *xo_handle_a2;

#ifdef FIXED_CONFIG
static void es325_fixed_config(struct es325_priv *es325);
#endif
static int es325_alloc_slim_rx_chan(struct slim_device *sbdev);
static int es325_alloc_slim_tx_chan(struct slim_device *sbdev);
static int es325_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt, unsigned int rate);
static int es325_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt, unsigned int rate);
static int es325_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt);
static int es325_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt);
static int es325_rx_ch_num_to_idx(int ch_num);
static int es325_tx_ch_num_to_idx(int ch_num);

static int es325_rx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	pr_info("%s(ch_num = %d)\n", __func__, ch_num);
	// for (i = 0; i < ES325_SLIM_RX_PORTS; i++) {
	for (i = 0; i < 6; i++) {
		if (ch_num == es325_slim_rx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int es325_tx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	pr_info("%s(ch_num = %d)\n", __func__, ch_num);
	for (i = 0; i < ES325_SLIM_TX_PORTS; i++) {
		if (ch_num == es325_slim_tx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

/* es325 -> wcd9310 - alsa playback function */
static int es325_wcd9310_cfg_slim_tx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	/* start slim channels associated with id */
	rc = es325_cfg_slim_tx(es325->gen0_client,
				es325->dai[dai_id].ch_num,
				es325->dai[dai_id].ch_tot,
				es325->dai[dai_id].rate);

	return rc;
}

/* es325 <- wcd9310 - alsa capture function */
static int es325_wcd9310_cfg_slim_rx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	/* start slim channels associated with id */
	rc = es325_cfg_slim_rx(es325->gen0_client,
				es325->dai[dai_id].ch_num,
				es325->dai[dai_id].ch_tot,
				es325->dai[dai_id].rate);

	return rc;
}

/* es325 -> wcd9310 - alsa playback function */
static int es325_wcd9310_close_slim_tx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	/* close slim channels associated with id */
	rc = es325_close_slim_tx(es325->gen0_client,
				es325->dai[dai_id].ch_num,
				es325->dai[dai_id].ch_tot);

	return rc;
}

/* es325 <- wcd9310 - alsa capture function */
static int es325_wcd9310_close_slim_rx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	/* close slim channels associated with id */
	rc = es325_close_slim_rx(es325->gen0_client,
				es325->dai[dai_id].ch_num,
				es325->dai[dai_id].ch_tot);

	return rc;
}

static int es325_alloc_slim_rx_chan(struct slim_device *sbdev)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	int i;
	int port_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	// for (i = 0; i < ES325_SLIM_RX_PORTS; i++) {
	for (i = 0; i < 6; i++) {
		port_id = i;
		rx[i].ch_num = es325_slim_rx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &rx[i].sph,
					SLIM_SINK);
		slim_query_ch(sbdev, rx[i].ch_num, &rx[i].ch_h);
		dev_dbg(&sbdev->dev, "%s(): port_id = %d\n", __func__, port_id);
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__, rx[i].ch_num);
		dev_dbg(&sbdev->dev, "%s(): sph = 0x%08x\n", __func__, rx[i].sph);
	}

	return rc;
}

static int es325_alloc_slim_tx_chan(struct slim_device *sbdev)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	int i;
	int port_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	for (i = 0; i < ES325_SLIM_TX_PORTS; i++) {
		port_id = i + 10; // ES325_SLIM_RX_PORTS;
		tx[i].ch_num = es325_slim_tx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &tx[i].sph,
					SLIM_SRC);
		slim_query_ch(sbdev, tx[i].ch_num, &tx[i].ch_h);
		dev_dbg(&sbdev->dev, "%s(): port_id = %d\n", __func__, port_id);
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__, tx[i].ch_num);
		dev_dbg(&sbdev->dev, "%s(): sph = 0x%08x\n", __func__, tx[i].sph);
	}

	return rc;
}

static int es325_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt, unsigned int rate)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_RX_PORTS] = {0};
	u16 ch_h[ES325_SLIM_RX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx = 0;
	int ret = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = rx[idx].ch_h;
		sph[i] = rx[idx].sph;

		pr_info("%s(): idx = %d\n", __func__, idx);
		dev_dbg(&sbdev->dev, "%s(): ch_num[i] = %d\n", __func__, ch_num[i]);
		dev_dbg(&sbdev->dev, "%s(): ch_h[i] = %d\n", __func__, ch_h[i]);
		dev_dbg(&sbdev->dev, "%s(): sph[i] = 0x%08x\n", __func__, sph[i]);
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	ret = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (ret < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, ret);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		ret = slim_connect_sink(sbdev, &sph[i], 1, ch_h[i]);
		if (ret < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_sink() failed: %d\n",
				__func__, ret);
			goto slim_connect_sink_error;
		}
	}
	ret = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, ret);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		rx[idx].grph = grph;
	}
	return 0;
slim_control_ch_error:
slim_connect_sink_error:
	es325_close_slim_rx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return ret;
}

static int es325_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt, unsigned int rate)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_TX_PORTS] = {0};
	u16 ch_h[ES325_SLIM_TX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx = 0;
	int ret = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);
	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d\n", __func__, ch_cnt);
	dev_dbg(&sbdev->dev, "%s(): rate = %d\n", __func__, rate);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = tx[idx].ch_h;
		sph[i] = tx[idx].sph;
		pr_info("%s(): idx = %d\n", __func__, idx);
		dev_dbg(&sbdev->dev, "%s(): ch_num[i] = %d\n", __func__, ch_num[i]);
		dev_dbg(&sbdev->dev, "%s(): ch_h[i] = %d\n", __func__, ch_h[i]);
		dev_dbg(&sbdev->dev, "%s(): sph[i] = 0x%08x\n", __func__, sph[i]);
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	ret = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (ret < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, ret);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		ret = slim_connect_src(sbdev, sph[i], ch_h[i]);
		if (ret < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_src() failed: %d\n",
				__func__, ret);
			dev_err(&sbdev->dev,
				"%s(): ch_num[0] = %d\n",
				__func__, ch_num[0]);
			goto slim_connect_src_error;
		}
	}
	ret = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, ret);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		tx[idx].grph = grph;
	}
	return 0;
slim_control_ch_error:
slim_connect_src_error:
	es325_close_slim_tx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return ret;
}

static int es325_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_RX_PORTS] = {0};
	int i;
	int idx = 0;
	int ret = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		sph[i] = rx[idx].sph;
		grph = rx[idx].grph;
	}

	ret = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, ret);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		rx[idx].grph = 0;
	}
	ret = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, ret);
	}
slim_control_ch_error:
	return ret;
}

static int es325_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
	unsigned int ch_cnt)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_TX_PORTS] = {0};
	int i;
	int idx = 0;
	int ret = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		sph[i] = tx[idx].sph;
		grph = tx[idx].grph;
	}

	ret = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_connect_sink() failed: %d\n",
			__func__, ret);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		pr_info("%s(): idx = %d\n", __func__, idx);
		tx[idx].grph = 0;
	}
	ret = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (ret < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, ret);
	}
slim_control_ch_error:
	return ret;
}

int es325_remote_cfg_slim_rx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	if (es325->power_state != ES325_POWER_AWAKE)
		return 0;
	dev_info(&sbdev->dev, "%s(dai_id = %d): entry\n", __func__, dai_id);
	dev_info(&sbdev->dev, "%s(): ch_tot = %d\n", __func__,
			es325->dai[dai_id].ch_tot);
	if (dai_id != ES325_SLIM_1_PB
		&& dai_id != ES325_SLIM_2_PB)
		return 0;

	if (es325->dai[dai_id].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = es325_cfg_slim_rx(es325->gen0_client,
					es325->dai[dai_id].ch_num,
					es325->dai[dai_id].ch_tot,
					es325->dai[dai_id].rate);

		be_id = es325_slim_be_id[dai_id];
		es325->dai[be_id].ch_tot = es325->dai[dai_id].ch_tot;
		es325->dai[be_id].rate = es325->dai[dai_id].rate;
		if (be_id == ES325_SLIM_2_CAP) {
			es325->dai[be_id].ch_num[0] = 144;
			es325->dai[be_id].ch_num[1] = 145;
		}
		else if (be_id == ES325_SLIM_3_CAP) {
			es325->dai[be_id].ch_num[0] = 144;
			es325->dai[be_id].ch_num[1] = 145;
		}
		rc = es325_wcd9310_cfg_slim_tx(es325, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_remote_cfg_slim_rx);

int es325_remote_cfg_slim_tx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	if (es325->power_state != ES325_POWER_AWAKE)
		return 0;
	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	dev_info(&sbdev->dev, "%s(): ch_tot = %d\n", __func__,
			es325->dai[dai_id].ch_tot);
	if (dai_id != ES325_SLIM_1_CAP)
		return 0;

	if (es325->dai[dai_id].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = es325_cfg_slim_tx(es325->gen0_client,
					es325->dai[dai_id].ch_num,
					es325->dai[dai_id].ch_tot,
					es325->dai[dai_id].rate);

		be_id = es325_slim_be_id[dai_id];
		es325->dai[be_id].ch_tot = es325->dai[dai_id].ch_tot;
		es325->dai[be_id].rate = es325->dai[dai_id].rate;
		if (be_id == ES325_SLIM_3_PB) {
			es325->dai[be_id].ch_num[0] = 134;
			es325->dai[be_id].ch_num[1] = 135;
		}
		rc = es325_wcd9310_cfg_slim_rx(es325, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_remote_cfg_slim_tx);

int es325_remote_close_slim_rx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	if (es325->power_state != ES325_POWER_AWAKE)
		return 0;
	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	dev_info(&sbdev->dev, "%s(): ch_tot = %d\n", __func__,
			es325->dai[dai_id].ch_tot);
	if (dai_id != ES325_SLIM_1_PB
		&& dai_id != ES325_SLIM_2_PB)
		return 0;

	if (es325->dai[dai_id].ch_tot != 0) {
                es325_close_slim_rx(es325->gen0_client,
                                    es325->dai[dai_id].ch_num,
                                    es325->dai[dai_id].ch_tot);

		be_id = es325_slim_be_id[dai_id];
		rc = es325_wcd9310_close_slim_tx(es325, be_id);

		es325->dai[dai_id].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_remote_close_slim_rx);

int es325_remote_close_slim_tx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	if (es325->power_state != ES325_POWER_AWAKE)
		return 0;
	dev_info(&sbdev->dev, "%s(); entry\n", __func__);
	dev_info(&sbdev->dev, "%s(): ch_tot = %d\n", __func__,
			es325->dai[dai_id].ch_tot);
	if (dai_id != ES325_SLIM_1_CAP)
		return 0;

	if (es325->dai[dai_id].ch_tot != 0) {
                es325_close_slim_tx(es325->gen0_client,
                                    es325->dai[dai_id].ch_num,
                                    es325->dai[dai_id].ch_tot);

		be_id = es325_slim_be_id[dai_id];
		rc = es325_wcd9310_close_slim_rx(es325, be_id);

		es325->dai[dai_id].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_remote_close_slim_tx);

static int es325_init_slim_slave(struct slim_device *sbdev)
{
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	rc = es325_alloc_slim_rx_chan(sbdev);
	rc = es325_alloc_slim_tx_chan(sbdev);

	return rc;
}

static void msg_to_bus_order(char *msg, int msg_len)
{
	char tmp;

	for (; msg_len > 0; msg_len -= 4, msg += 4)
	{
		tmp = *(msg + 3);
		*(msg + 3) = *(msg);
		*(msg) = tmp;
		tmp = *(msg + 2);
		*(msg + 2) = *(msg + 1);
		*(msg + 1) = tmp;
	}
}

/* Maximum number of attempts to read a VE in the presence of a "not
 * ready" response. */
#define ES325_RD_POLL_MAX 5
/* Interval between attemps to read a VE in milliseconds. */
#define ES325_RD_POLL_INTV 29

#ifdef BUS_TRANSACTIONS
#if defined(CONFIG_SND_SOC_ES325_I2C)
static int es325_i2c_read(struct es325_priv *es325, char *buf, int len);
static int es325_i2c_write(struct es325_priv *es325, char *buf, int len);
#define ES325_BUS_READ(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order) \
	es325_i2c_read(x_es325, x_buf, x_len)
#define ES325_BUS_WRITE(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order) \
	es325_i2c_write(x_es325, x_buf, x_len)
#elif defined(CONFIG_SND_SOC_ES325_SLIM)
static int es325_slim_read(struct es325_priv *es325, unsigned int offset,
	unsigned int width, char *buf, int len, int bus_order);
static int es325_slim_write(struct es325_priv *es325, unsigned int offset,
	unsigned int width, const char *buf, int len, int bus_order);
#define ES325_BUS_READ(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order) \
	es325_slim_read(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order)
#define ES325_BUS_WRITE(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order) \
	es325_slim_write(x_es325, x_offset, x_width, x_buf, x_len, x_bus_order)
#else
#error "es325.c - bus infrastructure not defined"
#endif
#else
/* Pretend all read and write operations on the bus are successful -
 * when no bus is available. */
#define ES325_BUS_READ(e, o, w, b, l, r) (0)
#define ES325_BUS_WRITE(e, o, w, b, l, r) (0)
#endif

#ifdef FIXED_CONFIG
static void es325_fixed_config(struct es325_priv *es325)
{
	u8 config_macro[] = {
#if 0
		0xb0, 0x5c, 0x00, 0x04,
		0xb0, 0x4c, 0x00, 0x01, /* sample rate 16khz */
		0xb0, 0x5a, 0x18, 0xa0, /* slim rx0 -> audin1 */
		0xb0, 0x5a, 0x4c, 0xac, /* audout1 -> slim tx2 */
		0x90, 0x1c, 0x00, 0x00,
#else
	  /* 2 mic voice call with no processing */
	  /* "rx1" is FEIN */
	  /* "tx1" is FEOUT */
		0xb0, 0x5c, 0x00, 0x01, // Set: Voice processing
		0xb0, 0x4c, 0x00, 0x01, // Algo rate = 16 kHz
		0xb0, 0x5e, 0x00, 0x01, // Mix rate = 16 kHz
		0xb0, 0x4e, 0x00, 0x00, // smooth rate = 0
		0xb0, 0x5a, 0x14, 0xa0, // Set: SLIMRx0 -> FEIN
		0xb0, 0x5a, 0x04, 0xa4, // Set: SLIMRx4 -> PRI(9310 to 325)
		0xb0, 0x5a, 0x08, 0xa5, // Set: SLIMRx4 -> SEC(9310 to 325)
		0xb0, 0x5a, 0x40, 0xaa, // Set: SLIMTx0 <- CSOUT(325 to MDM)
		0xb0, 0x5a, 0x44, 0xac, // Set: SLIMTx2 <- FEOUT1(325 to 9310)
		0x90, 0x1c, 0x00, 0x00, // Algo processing = off (COMMIT)
#endif
	};
	u8 *msg_ptr = config_macro;
	int i;

	for (i = 0; i < ARRAY_SIZE(config_macro); i += 4) {
		es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
				ES325_WRITE_VE_WIDTH, msg_ptr, 4, 1);
		msg_ptr += 4;
		mdelay(20);
	}
}
#endif

#if 0
static void es325_ping(struct es325_priv *es325)
{
	u8 req_msg[] = {
		0x80, 0x4f, 0x00, 0x00,
	};
	u8 ack_msg[4] = {
		0x00, 0x00, 0x00, 0x00,
	};

	es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
			ES325_WRITE_VE_WIDTH, req_msg, 4, 1);
	mdelay(20);
	es325_slim_read(es325, ES325_READ_VE_OFFSET,
			ES325_READ_VE_WIDTH, ack_msg, 4, 1);
	pr_info("%s(): ping ack = %02x%02x%02x%02x\n", __func__,
		ack_msg[0], ack_msg[1], ack_msg[2], ack_msg[3]);
}
#endif


#if defined(CONFIG_SND_SOC_ES325_I2C)
static int es325_i2c_read(struct es325_priv *es325, char *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es325->this_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int rc = 0;

#if 0
	pr_info("%s(): entry\n", __func__);

	rc = i2c_transfer(es325->this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s(): i2c_transfer() failed, rc = %d", __func__, rc);
		return rc;
	}

	{
		int i;
		pr_info("%s(): i2c msg:\n", __func__);
		for (i = 0; i < len; ++i) {
			pr_info("\t[%d] = 0x%02x\n", i, buf[i]);
		}
	}

	pr_info("%s(): exit\n", __func__);
#endif
	return rc;
}

static int es325_i2c_write(struct es325_priv *es325, char *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es325->this_client->addr,
			.flags = 0,
			.len = len,
			.buf = buf,
		},
	};
	int rc = 0;

#if 0
	pr_info("%s(): entry\n", __func__);

	{
		int i;

		pr_info("%s(): i2c msg:\n", __func__);
		for (i = 0; i < len; ++i) {
			pr_info("\t[%d] = 0x%02x\n", i, buf[i]);
		}
	}

	rc = i2c_transfer(es325->this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s(): i2c_transfer() failed, rc = %d", __func__, rc);
		return rc;
	}

	pr_info("%s(): exit\n", __func__);
#endif
	return rc;
}
#else
static int es325_slim_read(struct es325_priv *es325, unsigned int offset,
		unsigned int width, char *buf, int len, int bus_order)
{
	char notready[] = { 0, 0, 0, 0 };
	struct slim_device *sbdev = es325->gen0_client;
	/* DECLARE_COMPLETION_ONSTACK(read_done); */
	struct slim_ele_access msg = {
		.start_offset = offset,
		.num_bytes = width,
		/* .comp = &read_done, */
		.comp = NULL,
	};
	int rc = 0;
	int try = 0;

	for (try = 0; try < ES325_RD_POLL_MAX; try++) {
		rc = slim_request_val_element(sbdev, &msg, buf, len);
		if (rc != 0)
			break;
		if (memcmp(buf, notready, 4) != 0)
			break;
		mdelay(ES325_RD_POLL_INTV);
	}
	if (try >= ES325_RD_POLL_MAX && memcmp(buf, notready, 4) == 0) {
		pr_err("%s: failed not ready after %d tries\n", __func__, try);
		rc = -EIO;
	}

	if (bus_order)
		msg_to_bus_order(buf, len);
	if (rc != 0)
		pr_err("%s: read failed rc=%d\n", __func__, rc);

	return rc;
}

static int es325_slim_write(struct es325_priv *es325, unsigned int offset,
			    unsigned int width, const char *buf, int len, int bus_order)
{
	struct slim_device *sbdev = es325->gen0_client;
	struct slim_ele_access msg = {
		.start_offset = offset,
		.num_bytes = width,
		.comp = NULL,
	};
	int rc = 0;

	if (bus_order)
		msg_to_bus_order((char *)buf, len);
	rc = slim_change_val_element(sbdev, &msg, buf, len);

	return rc;
}
#endif

static int es325_build_algo_read_msg(char *msg, int *msg_len,
	unsigned int reg)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es325_algo_paramid))
		return -EINVAL;

	paramid = es325_algo_paramid[index];

	/* ES325_GET_ALGO_PARAM */
	*msg++ = (ES325_GET_ALGO_PARAM >> 8) & 0x00ff;
	*msg++ = ES325_GET_ALGO_PARAM & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;
	*msg_len = 4;

	return 0;
}

static int es325_build_algo_write_msg(char *msg, int *msg_len,
	unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	unsigned int cmd;
	unsigned int paramid;

	if (index >= ARRAY_SIZE(es325_algo_paramid))
		return -EINVAL;

	paramid = es325_algo_paramid[index];

	/* ES325_SET_ALGO_PARAMID */
	cmd = ES325_SET_ALGO_PARAMID;
	if (reg & ES325_STAGED_CMD)
		cmd |= ES325_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;

	/* ES325_SET_ALGO_PARAM */
	cmd = ES325_SET_ALGO_PARAM;
	if (reg & ES325_STAGED_CMD)
		cmd |= ES325_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* value */
	*msg++ = (value >> 8) & 0x00ff;
	*msg++ = value & 0x00ff;
	*msg_len = 8;

	return 0;
}

static int es325_build_dev_read_msg(char *msg, int *msg_len,
	unsigned int reg)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	unsigned int paramid;

	if (index > ARRAY_SIZE(es325_dev_paramid))
		return -EINVAL;

	paramid = es325_dev_paramid[index];

	/* ES325_GET_DEV_PARAM */
	*msg++ = (ES325_GET_DEV_PARAM >> 8) & 0x00ff;
	*msg++ = ES325_GET_DEV_PARAM & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;
	*msg_len = 4;

	return 0;
}

static int es325_build_dev_write_msg(char *msg, int *msg_len,
	unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	unsigned int cmd;
	unsigned int paramid;

	if (index > ARRAY_SIZE(es325_dev_paramid))
		return -EINVAL;

	paramid = es325_dev_paramid[index];

	/* ES325_SET_DEV_PARAMID */
	cmd = ES325_SET_DEV_PARAMID;
	if (reg & ES325_STAGED_CMD)
		cmd |= ES325_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* PARAM ID */
	*msg++ = (paramid >> 8) & 0x00ff;
	*msg++ = paramid & 0x00ff;

	/* ES325_SET_DEV_PARAM */
	cmd = ES325_SET_DEV_PARAM;
	if (reg & ES325_STAGED_CMD)
		cmd |= ES325_STAGED_MSG_BIT;
	*msg++ = (cmd >> 8) & 0x00ff;
	*msg++ = cmd & 0x00ff;

	/* value */
	*msg++ = (value >> 8) & 0x00ff;
	*msg++ = value & 0x00ff;
	*msg_len = 8;

	return 0;
}

static int es325_build_cmd_read_msg(char *msg, int *msg_len,
	unsigned int reg)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	struct es325_cmd_access *cmd_access;

	if (index > ARRAY_SIZE(es325_cmd_access))
		return -EINVAL;
	cmd_access = es325_cmd_access + index;

	*msg_len = cmd_access->read_msg_len;
	memcpy(msg, &cmd_access->read_msg, *msg_len);

	return 0;
}

static int es325_build_cmd_write_msg(char *msg, int *msg_len,
	unsigned int reg, unsigned int value)
{
	unsigned int index = reg & ES325_ADDR_MASK;
	struct es325_cmd_access *cmd_access;

	if (index > ARRAY_SIZE(es325_cmd_access))
		return -EINVAL;
	cmd_access = es325_cmd_access + index;

	*msg_len = cmd_access->write_msg_len;
	memcpy(msg, &cmd_access->write_msg, *msg_len);
	if (reg & ES325_STAGED_CMD)
		*msg |= (1 << 5);
	if (cmd_access->val_max > 255) {
		*(msg + cmd_access->write_msg_len - 2) = (value & 0xff00) >> 8;
		*(msg + cmd_access->write_msg_len - 1) = value & 0x00ff;
	} else {
		*(msg + cmd_access->write_msg_len - 1) = value & 0x00ff;
	}

	return 0;
}

static unsigned int es325_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
	struct es325_priv *es325 = &es325_priv;
	unsigned int access = reg & ES325_ACCESS_MASK;
	char req_msg[16];
	char ack_msg[16];
	unsigned int msg_len;
	unsigned int value;
	int rc;

/* GAC */
/* dead code */
#if 0
	/* rewrite SND_SOC_NOPM controls to be ext */
	if (reg == SND_SOC_NOPM)
		return 0;
#endif
/* GAC */
	switch (access) {
	case ES325_ALGO_ACCESS:
		rc = es325_build_algo_read_msg(req_msg, &msg_len, reg);
		break;
	case ES325_DEV_ACCESS:
		rc = es325_build_dev_read_msg(req_msg, &msg_len, reg);
		break;
	case ES325_CMD_ACCESS:
		rc = es325_build_cmd_read_msg(req_msg, &msg_len, reg);
		break;
	case ES325_OTHER_ACCESS:
		return 0;
	default:
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("%s(): failed to build read message for address = 0x%04x\n",
			__func__, reg);
		return rc;
	}

	rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
		ES325_WRITE_VE_WIDTH, req_msg, msg_len, 1);
	if (rc < 0) {
		pr_err("%s(): es325_xxxx_write()", __func__);
		return rc;
	}
	mdelay(20);
	rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET,
		ES325_READ_VE_WIDTH, ack_msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): es325_xxxx_read()", __func__);
		return rc;
	}
	value = ack_msg[2] << 8 | ack_msg[3];

	return value;
}

static int es325_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct es325_priv *es325 = &es325_priv;
	unsigned int access = reg & ES325_ACCESS_MASK;
	char msg[16];
	char *msg_ptr;
	int msg_len;
	int i;
	int rc;

	switch (access) {
	case ES325_ALGO_ACCESS:
		rc = es325_build_algo_write_msg(msg, &msg_len, reg, value);
		break;
	case ES325_DEV_ACCESS:
		rc = es325_build_dev_write_msg(msg, &msg_len, reg, value);
		break;
	case ES325_CMD_ACCESS:
		rc = es325_build_cmd_write_msg(msg, &msg_len, reg, value);
		break;
	case ES325_OTHER_ACCESS:
		return 0;
	default:
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("%s(): failed to build write message for address = 0x%04x\n",
			__func__, reg);
		return rc;
	}

	msg_ptr = msg;
	for (i = msg_len; i > 0; i -= 4) {
		rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
			ES325_WRITE_VE_WIDTH, msg_ptr, 4, 1);
		if (rc < 0) {
			pr_err("%s(): es325_xxxx_write()", __func__);
			return rc;
		}
		mdelay(1);
		rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET,
			ES325_READ_VE_WIDTH, msg_ptr, 4, 1);
		if (rc < 0) {
			pr_err("%s(): es325_xxxx_read()", __func__);
			return rc;
		}
		msg_ptr += 4;
	}

	return rc;
}

static ssize_t es325_route_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int rc;
	struct es325_priv *es325 = &es325_priv;
	u8 req_msg[] = {0x80, 0x4f, 0x00, 0x00};
	u8 ack_msg[4] = {0x00, 0x00, 0x00, 0x00};

	if(es325->power_state != ES325_POWER_AWAKE){
		pr_err("%s(): Can not get route_status when power_state is %d\n", __func__, es325->power_state);
		return sprintf(buf, "Can not get route_status when power_state is %d\n", es325->power_state);
	}
	else {
		rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
								ES325_WRITE_VE_WIDTH, req_msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			return sprintf(buf, "Failed to SLIMBus Write\n");
		}

		mdelay(20);
		rc = es325_slim_read(es325, ES325_READ_VE_OFFSET,
								ES325_READ_VE_WIDTH, ack_msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): Failed to SLIMBus Read\n", __func__);
			return sprintf(buf, "Failed to SLIMBus Read\n");
		}
		else {
			pr_info("%s(): ping ack = %02x%02x%02x%02x\n", __func__,
					ack_msg[0], ack_msg[1], ack_msg[2], ack_msg[3]);
			return sprintf(buf, "status=0x%02x%02x%02x%02x\n",
					ack_msg[0], ack_msg[1], ack_msg[2], ack_msg[3]);
		}
	}
}

static ssize_t es325_route_config_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	pr_info("%s(): route=%s\n", __func__,
		es325_internal_route_configs_text[es325_internal_route_num]);
	return sprintf(buf, "route=%s\n",
		es325_internal_route_configs_text[es325_internal_route_num]);
}

static ssize_t es325_route_config_set(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct es325_priv *es325 = &es325_priv;
	int i;
	long route_num;
	u8 msg[4];
	u8 *msg_ptr;
	int rc;

	pr_info("%s():buf = %s\n", __func__, buf);
	rc = kstrtol(buf, 10, &route_num);
	if (route_num > ES325_STOP)
		return count;
	es325_internal_route_num = route_num;
	pr_info("%s():es325_internal_route_num = %ld\n",
		__func__, es325_internal_route_num);
	msg_ptr = &es325_internal_route_configs[es325_internal_route_num][0];
	for (i = 0; ; msg_ptr +=4) {
		if (*msg_ptr == 0xff)
			break;
		memcpy(msg, msg_ptr, 4);
		es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
				ES325_WRITE_VE_WIDTH, msg, 4, 1);
		pr_info("%s(): msg = %02x%02x%02x%02x\n", __func__,
			msg[0], msg[1], msg[2], msg[3]);
	}

	return count;
}

static ssize_t es325_power_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	/* Don't bother locking. Not important here. */
	int state = es325_priv.power_state;
	return sprintf(buf, "power status=%d\n", state);
}

#define SIZE_OF_VERBUF 256

static unsigned char first_char_msg[4] = {0x80, 0x20, 0x00, 0x00};
static unsigned char next_char_msg[4] = {0x80, 0x21, 0x00, 0x00};

static int es325_get_fw_version(void)
{

	struct es325_priv *es325 = &es325_priv;
	int rc, idx = 0;
	unsigned int imsg;
	unsigned char bmsg[4];
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;
	unsigned char cmd[4];

	memset(verbuf,0,SIZE_OF_VERBUF);

	idx = 0;
	imsg = 1; // force first loop
	rc = 0;
	memcpy(cmd, first_char_msg, 4);

	while ((rc >= 0) &&
		(idx < (SIZE_OF_VERBUF-1)) &&
		(imsg & 0xFF))
	{
	    rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
				ES325_WRITE_VE_WIDTH, cmd, 4, 1);
	    if (rc < 0)
	    {
		pr_err("GetFW Ver: Audience Write/Read error.\n");
	    }
	    else
	    {
		mdelay(2);
		rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET,
				    ES325_READ_VE_WIDTH, bmsg, 4, 1);
//		pr_info("%s() : get fw read %x %x %x %x\n", __func__, bmsg[0], bmsg[1], bmsg[2], bmsg[3]);

		if (rc < 0) {
			pr_err("%s: ES325_BUS_READ()=%d\n", __func__, rc);
		}

		imsg = bmsg[3];
		if ((bmsg[0] == 0xFF) &&  (bmsg[1] == 0xFF))
		{
		    pr_err("No version API on Audience\n");
		    rc = -1;
		}
		else
		{
		    verbuf[idx++] = (char) (bmsg[3]);
		}
	    }
	    memcpy(cmd, next_char_msg, 4);
	}

	// Null terminate the string
	verbuf[idx] = '\0';

	strcpy(es325->fw_version, verbuf);

	pr_info("Audience fw ver %s\n", verbuf);

	return rc;
}

static ssize_t es325_fw_version_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct es325_priv *es325 = &es325_priv;

	pr_info("Audience fw ver %s\n", es325->fw_version);

	return sprintf(buf, "FW Version is [%s]\n", es325->fw_version);

}

static ssize_t es325_sleep_test_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	pr_info("%s(): debug_val=%d %d %d\n", __func__,
		es325_rx1_route_ena, es325_rx2_route_ena, es325_tx1_route_ena);
	return sprintf(buf, "debug_val=%d %d %d\n",
		es325_rx1_route_ena, es325_rx2_route_ena, es325_tx1_route_ena);
}

static ssize_t es325_sleep_test_set(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	long debug_val;
	int rc;

	pr_info("%s():buf = %s\n", __func__, buf);
	rc = kstrtol(buf, 10, &debug_val);
	switch(debug_val)
		{
		case 0:
			es325_codec_sleep();
			break;
		case 1:
			es325_codec_wakeup();
			break;
		default:
			break;
		}

	return count;
}

static ssize_t es325_mic_sealed_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int rc;
	struct es325_priv *es325 = &es325_priv;
	u8 req_msg[4] = {0x80, 0x43, 0x00, 0x00};
	u8 ack_msg[4] = {0x00, 0x00, 0x00, 0x00};

	if(es325->power_state != ES325_POWER_AWAKE){
		pr_err("%s(): Can not get mic sealed when power_state is %d\n", __func__, es325->power_state);
		return sprintf(buf, "Can not get mic sealed when power_state is %d\n", es325->power_state);
	}
	else {
		rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
								ES325_WRITE_VE_WIDTH, req_msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			return sprintf(buf, "Failed to SLIMBus Write\n");
		}

		mdelay(20);
		rc = es325_slim_read(es325, ES325_READ_VE_OFFSET,
								ES325_READ_VE_WIDTH, ack_msg, 4, 1);
		if (rc < 0) {
			pr_err("%s(): Failed to SLIMBus Read\n", __func__);
			return sprintf(buf, "Failed to SLIMBus Read\n");
		}
		else {
			pr_info("%s(): ping ack = %02x%02x%02x%02x\n", __func__,
					ack_msg[0], ack_msg[1], ack_msg[2], ack_msg[3]);
			return sprintf(buf, "status=0x%02x%02x%02x%02x\n",
					ack_msg[0], ack_msg[1], ack_msg[2], ack_msg[3]);
		}
	}
}


static ssize_t es325_mic_sealed_set(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	long debug_val;
	int rc;
	struct es325_priv *es325 = &es325_priv;
	u8 msg[4];
	u8 *msg_ptr;
	int i;

	rc = kstrtol(buf, 10, &debug_val);
	switch(debug_val)
		{
		case 0:
			msg_ptr = &es325_internal_route_configs[es325_internal_route_num][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		case 1:
			msg_ptr = &es325_internal_route_configs[ES325_BOTTOM_MIC_SEALED][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		case 2:
			msg_ptr = &es325_internal_route_configs[ES325_TOP_MIC_SEALED][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		default:
			break;
		}

	return count;
}

static int es325_put_mic_sealed(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	long debug_val;
	int rc = 0;
	struct es325_priv *es325 = &es325_priv;
	u8 msg[4];
	u8 *msg_ptr;
	int i;

	debug_val = ucontrol->value.integer.value[0];

	switch(debug_val)
		{
		case 0:
			msg_ptr = &es325_internal_route_configs[es325_internal_route_num][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		case 1:
			msg_ptr = &es325_internal_route_configs[ES325_BOTTOM_MIC_SEALED][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		case 2:
			msg_ptr = &es325_internal_route_configs[ES325_TOP_MIC_SEALED][0];
			for (i = 0; ; msg_ptr +=4) {
				if (*msg_ptr == 0xff)
					break;
				memcpy(msg, msg_ptr, 4);
				rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
						ES325_WRITE_VE_WIDTH, msg, 4, 1);
			}
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		default:
			break;
		}

	return debug_val;
}

static int es325_put_vp_onoff(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	long debug_val;
	int rc = 0;
	struct es325_priv *es325 = &es325_priv;
	u8 vp_on_req_msg[4] 	= {0x90, 0x1c, 0x00, 0x01}; // VP On
	u8 vp_off_req_msg[4]	= {0x90, 0x1c, 0x00, 0x00}; // VP Off

	debug_val = ucontrol->value.integer.value[0];

	switch(debug_val)
		{
		case 0:
			rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
									ES325_WRITE_VE_WIDTH, vp_off_req_msg, 4, 1);
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		case 1:
			rc = es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
									ES325_WRITE_VE_WIDTH, vp_on_req_msg, 4, 1);
			if (rc < 0) {
				pr_err("%s(): Failed to SLIMBus Write\n", __func__);
			}
			break;
		default:
			break;
		}

	return debug_val;
}

/*
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/route_status
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/route_config
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/power_status
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/fw_version
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/sleep_test
	/sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/mic_sealed
*/

static struct device_attribute es325_device_attrs[] = {
	__ATTR(route_status,	S_IRUGO, \
			es325_route_status_show,	NULL),
	__ATTR(route_config,	S_IRUGO|S_IWUSR, \
			es325_route_config_show,	es325_route_config_set),
	__ATTR(power_status,	S_IRUGO, \
			es325_power_status_show,	NULL),
	__ATTR(fw_version,		S_IRUGO, \
			es325_fw_version_show,		NULL),
	__ATTR(sleep_test,		S_IRUGO|S_IWUSR, \
			es325_sleep_test_show,	es325_sleep_test_set),
	__ATTR(mic_sealed,	S_IRUGO|S_IWUSR, \
			es325_mic_sealed_show,	es325_mic_sealed_set),

};


static int do_fs_firmware_load(const char *fn, char **fp)
{
	struct file* filp;
	long l;
	char *dp;
	loff_t pos;

	filp = filp_open(fn, O_RDONLY, 0);
	if (IS_ERR(filp))
	{
		pr_err("%s(): Unable to load '%s'.\n", __func__, fn);
		return 0;
	}
	l = i_size_read(filp->f_path.dentry->d_inode);
	if (l <= 0)
	{
		pr_err("%s(): Invalid firmware size\n", __func__);
		filp_close(filp, current->files);
		return 0;
	}
	dp = vmalloc(l);
	if (dp == NULL)
	{
		pr_err("%s(): Out of memory loading\n", __func__);
		filp_close(filp, current->files);
		return 0;
	}
	pos = 0;
	if (vfs_read(filp, dp, l, &pos) != l)
	{
		pr_err("%s(): Failed to read\n", __func__);
		vfree(dp);
		filp_close(filp, current->files);
		return 0;
	}
	filp_close(filp, current->files);
	*fp = dp;
	return (int) l;
}

/**
 *	mod_firmware_load - load sound driver firmware
 *	@fn: filename
 *	@fp: return for the buffer.
 *
 *	Load the firmware for a sound module (up to 128K) into a buffer.
 *	The buffer is returned in *fp. It is allocated with vmalloc so is
 *	virtually linear and not DMAable. The caller should free it with
 *	vfree when finished.
 *
 *	The length of the buffer is returned on a successful load, the
 *	value zero on a failure.
 *
 *	Caution: This API is not recommended. Firmware should be loaded via
 *	request_firmware.
 */

int fs_firmware_load(const char *fn, char **fp)
{
	int r;
	mm_segment_t fs = get_fs();

	set_fs(get_ds());
	r = do_fs_firmware_load(fn, fp);
	set_fs(fs);
	return r;
}

static int es325_firmware_reload(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct es325_priv *es325 = &es325_priv;
	int rc;
	const char *filename_strawberry = "/system/etc/snd_soc_msm/strawberry.bin";
	static unsigned char * fp = NULL;
	static int sizeof_strawberry = 0;
	static char sync_ok[] = { 0x80, 0, 0, 0 };
	char msg[16];
	unsigned int buf_frames;
	const char *buf_ptr;

	pr_info("%s(): entry\n", __func__);

	for(rc = 0; rc < 10; rc++) {
		if (es325->power_state == ES325_POWER_BOOT)
			msleep(1000);
		else
			break;
	}

	if(es325_codec_wakeup() == 0) {
		sizeof_strawberry = fs_firmware_load(filename_strawberry, (void *) &fp);
		if(sizeof_strawberry) {
			gpio_direction_output(es325->pdata->reset_gpio, 0);
			gpio_direction_output(es325->pdata->wakeup_gpio, 0);
			msleep(10);

			gpio_direction_output(es325->pdata->reset_gpio, 1);
			gpio_direction_output(es325->pdata->wakeup_gpio, 1);
			mdelay(30);

			pr_info("%s(): write ES325_BOOT_CMD\n", __func__);
			memset(msg, 0, 16);
			msg[0] = ES325_BOOT_CMD & 0x00ff;
			msg[1] = ES325_BOOT_CMD >> 8;
			rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
									ES325_WRITE_VE_WIDTH, msg, 4, 0);
			if (rc < 0) {
				pr_err("%s(): firmware load failed boot write\n", __func__);
				return  0; // rc;
			}
			mdelay(100);
			pr_info("%s(): read boot cmd ack\n", __func__);
			memset(msg, 0, 16);
			rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET, ES325_READ_VE_WIDTH,
								msg, 4, 0);
			if (rc < 0) {
				pr_err("%s(): firmware load failed boot ack\n", __func__);
				return  rc;
			}
			if ((msg[0] != (ES325_BOOT_ACK >> 8)) || (msg[1] != (ES325_BOOT_ACK & 0x00ff))) {
				pr_err("%s(): firmware load failed boot ack pattern", __func__);
				return  -EIO;
			}
			mdelay(100);

			pr_info("%s(): write firmware image\n", __func__);
			/* send image */
			buf_frames = sizeof_strawberry / ES325_FW_LOAD_BUF_SZ;
			pr_info("%s(): buf_frames = %d fw size=%d\n", __func__, buf_frames, sizeof_strawberry);
			buf_ptr = fp;
			for ( ; buf_frames; --buf_frames, buf_ptr += ES325_FW_LOAD_BUF_SZ) {
				rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
					ES325_WRITE_VE_WIDTH, buf_ptr, ES325_FW_LOAD_BUF_SZ, 0);
				if (rc < 0) {
					pr_err("%s(): firmware load failed\n", __func__);
					return -EIO;
				}
			}
			if (sizeof_strawberry % ES325_FW_LOAD_BUF_SZ) {
				rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
					ES325_WRITE_VE_WIDTH, buf_ptr,
					sizeof_strawberry % ES325_FW_LOAD_BUF_SZ, 0);
				if (rc < 0) {
					pr_err("%s(): firmware load failed\n", __func__);
					return -EIO;
				}
			}

			/* Give the chip some time to become ready after firmware
			 * download. */
			mdelay(100);

			pr_info("%s(): write ES325_SYNC_CMD\n", __func__);
			memset(msg, 0, 16);
			msg[0] = ES325_SYNC_CMD >> 8;
			msg[1] = ES325_SYNC_CMD & 0x00ff;
			msg[2] = ES325_SYNC_POLLING >> 8;
			msg[3] = ES325_SYNC_POLLING & 0x00ff;
			rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
				ES325_WRITE_VE_WIDTH, msg, 4, 1);
			if (rc < 0) {
				pr_err("%s(): firmware load failed sync write\n", __func__);
				return rc;
			}
			mdelay(100);
			pr_info("%s(): read sync cmd ack\n", __func__);
			memset(msg, 0, 16);
			rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET, ES325_READ_VE_WIDTH,
				msg, 4, 1);
			if (rc < 0) {
				pr_err("%s(): error reading firmware sync ack rc=%d\n",
					__func__, rc);
				return rc;
			}
			if (memcmp(msg, sync_ok, 4) == 0) {
				pr_info("%s(): firmware sync ack good=0x%02x%02x%02x%02x\n",
					__func__, msg[0], msg[1], msg[2], msg[3]);
			}
			else {
				pr_err("%s(): firmware sync ack failed=0x%02x%02x%02x%02x\n",
					__func__, msg[0], msg[1], msg[2], msg[3]);
				return -EIO;
			}

			es325_get_fw_version();

			es325_codec_sleep();

			pr_info("%s(): exit rc=%d\n", __func__, rc);

			return rc;
		} else {
			pr_err("%s(): Unable to load '%s'\n", __func__, filename_strawberry);
			return -EMFILE;
		}
	} else {
		pr_err("%s(): Failed wake up\n", __func__);
		return -EBUSY;
	}
}

static int es325_bootup(struct es325_priv *es325)
{
	static char sync_ok[] = { 0x80, 0, 0, 0 };
	char msg[16];
	unsigned int buf_frames;
	const char *buf_ptr;
	int rc;
	int retry =0;

	pr_info("%s(): entry\n", __func__);

reload_firmware:
	mdelay(100);
	pr_info("%s(): write ES325_BOOT_CMD\n", __func__);
	memset(msg, 0, 16);
	msg[0] = ES325_BOOT_CMD & 0x00ff;
	msg[1] = ES325_BOOT_CMD >> 8;
	rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
							ES325_WRITE_VE_WIDTH, msg, 4, 0);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n", __func__);
		return  0; // rc;
	}
	mdelay(100);
	pr_info("%s(): read boot cmd ack\n", __func__);
	memset(msg, 0, 16);
	rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET, ES325_READ_VE_WIDTH,
						msg, 4, 0);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack\n", __func__);
		return  rc;
	}
	if ((msg[0] != (ES325_BOOT_ACK >> 8)) || (msg[1] != (ES325_BOOT_ACK & 0x00ff))) {
		pr_err("%s(): firmware load failed boot ack pattern", __func__);
		return  -EIO;
	}
	mdelay(100);

	pr_info("%s(): write firmware image\n", __func__);

	/* send image */
	buf_frames = es325->fw->size / ES325_FW_LOAD_BUF_SZ;
	pr_info("%s(): buf_frames = %d fw size=%d\n", __func__, buf_frames,es325->fw->size);
	buf_ptr = es325->fw->data;
	for ( ; buf_frames; --buf_frames, buf_ptr += ES325_FW_LOAD_BUF_SZ) {
		rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
			ES325_WRITE_VE_WIDTH, buf_ptr, ES325_FW_LOAD_BUF_SZ, 0);
		if (rc < 0) {
			pr_err("%s(): firmware load failed\n", __func__);
			if (++retry < 3)
				goto reload_firmware;
			else
				return -EIO;
		}
	}
	if (es325->fw->size % ES325_FW_LOAD_BUF_SZ) {
		rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
			ES325_WRITE_VE_WIDTH, buf_ptr,
			es325->fw->size % ES325_FW_LOAD_BUF_SZ, 0);
		if (rc < 0) {
			pr_err("%s(): firmware load failed\n", __func__);
			if (++retry < 3)
				goto reload_firmware;
			else
				return -EIO;
		}
	}

	/* Give the chip some time to become ready after firmware
	 * download. */
	mdelay(100);

	pr_info("%s(): write ES325_SYNC_CMD\n", __func__);
	memset(msg, 0, 16);
	msg[0] = ES325_SYNC_CMD >> 8;
	msg[1] = ES325_SYNC_CMD & 0x00ff;
	msg[2] = ES325_SYNC_POLLING >> 8;
	msg[3] = ES325_SYNC_POLLING & 0x00ff;
	rc = ES325_BUS_WRITE(es325, ES325_WRITE_VE_OFFSET,
		ES325_WRITE_VE_WIDTH, msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write\n", __func__);
		return rc;
	}
	mdelay(200);
	pr_info("%s(): read sync cmd ack\n", __func__);
	memset(msg, 0, 16);
	rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET, ES325_READ_VE_WIDTH,
		msg, 4, 1);
	if (rc < 0) {
		pr_err("%s(): error reading firmware sync ack rc=%d\n",
			__func__, rc);
		return rc;
	}
	if (memcmp(msg, sync_ok, 4) == 0) {
		pr_info("%s(): firmware sync ack good=0x%02x%02x%02x%02x\n",
			__func__, msg[0], msg[1], msg[2], msg[3]);
	}
	else {
		pr_err("%s(): firmware sync ack failed=0x%02x%02x%02x%02x\n",
			__func__, msg[0], msg[1], msg[2], msg[3]);
		return -EIO;
	}

	es325_get_fw_version();

	pr_info("%s(): exit\n", __func__);
	return 0;
}

static int register_snd_soc(struct es325_priv *priv);
static int fw_download(void *arg)
{
	struct es325_priv *priv = (struct es325_priv *)arg;
	int rc;

	pr_info("%s(): entry\n", __func__);
#ifdef BUS_TRANSACTIONS
	rc = es325_bootup(priv);
#endif
	if (rc < 0) {
		pr_err("%s(): error bootup rc=%d\n",
			__func__, rc);
	}

	if(is_fw_download){
		pr_info("%s():already exist fw\n",__func__);
		return 0;
	}
	release_firmware(priv->fw);

	rc = register_snd_soc(priv);
	if (rc < 0) {
		pr_err("%s(): error register_snd_soc rc=%d\n",
			__func__, rc);
	}

#ifdef FIXED_CONFIG
	es325_fixed_config(priv);
#endif

	/* Sleep only _after_ the ALSA controls and widgets have been
	 * registered.
	 */
	es325_codec_sleep();
	module_put(THIS_MODULE);
	pr_info("%s(): exit\n", __func__);
	is_fw_download = 1;
	return 0;
}

#ifndef CONFIG_SLIMBUS_DEVICE_UP_COND
static int es325_defer_fw_download(struct es325_priv *priv)
{
	struct task_struct *task;

	pr_info("%s(): called\n", __func__);
	if (!try_module_get(THIS_MODULE)) {
		pr_info("%s(): failed to increment module ref count\n", __func__);
		return -EFAULT;
	}
	task = kthread_run(fw_download, priv, "es325-firmware-dload");
	if (IS_ERR(task)) {
		module_put(THIS_MODULE);
		pr_info("%s(): failed to create kernel task\n", __func__);
		return PTR_ERR(task);
	}
	return 0;
}
#endif

static void delayed_sleep(struct work_struct *w)
{
	int ch_tot;

	/* If there are active streams we do not sleep.
	 * Count the front end (FE) streams ONLY.
	 */
	ch_tot = 0;
	ch_tot += es325_priv.dai[ES325_SLIM_1_PB].ch_tot;
	ch_tot += es325_priv.dai[ES325_SLIM_2_PB].ch_tot;
	ch_tot += es325_priv.dai[ES325_SLIM_1_CAP].ch_tot;

	pr_info("%s %d active channels.\n", __func__, ch_tot);
	if ((ch_tot <= 0) && (es325_rx1_route_ena == 0) && (es325_tx1_route_ena == 0) && (es325_rx2_route_ena == 0)) {
		pr_info("%s : call es325_codec_sleep()\n", __func__);
		es325_codec_sleep();
	} else {
		pr_info("%s : restart hrtimer\n", __func__);
		es325_codec_sleep_delay_enable(&sleep_delay, sleep_delay_time);
	}
}

int es325_codec_sleep(void)
{
	unsigned int offset = ES325_WRITE_VE_OFFSET;
	unsigned int width = ES325_WRITE_VE_WIDTH;
	struct es325_priv *es325 = &es325_priv;
	struct device *dev = &es325->gen0_client->dev;
	char sleep_cmds[] = {
		0x00, 0x00, 0x4e, 0x90,	/* smooth mute 0 delay */
		0x01, 0x00, 0x10, 0x80	/* sleep command */
	};
	int rc = -1;
	pr_info("%s(): entry", __func__);

	mutex_lock(&es325->power_lock);

	if (es325->power_state == ES325_POWER_SLEEP) {
		dev_warn(dev,
			"%s() Sleep called while sleeping/trying to sleep. power_state=%d\n",
			__func__, es325->power_state);
		goto CODEC_SLEEP_EXIT;
	}

	rc = ES325_BUS_WRITE(es325, offset, width, sleep_cmds, 4, 0);
	if (rc < 0)
		goto CODEC_SLEEP_EXIT;
	rc = ES325_BUS_WRITE(es325, offset, width, sleep_cmds + 4, 4, 0);
	if (rc < 0)
		goto CODEC_SLEEP_EXIT;

	/* Give the device time to process and act on the commands. */
	msleep(25);
	clk_disable_unprepare(xo_handle_a2);

CODEC_SLEEP_EXIT:
	if (rc == 0) {
		es325->power_state = ES325_POWER_SLEEP;
		dev_info(dev, "%s() Entered sleep state. power_state=%d\n",
			__func__, es325->power_state);
		is_wideband = false;
	} else
		dev_warn(dev, "%s() Failed to enter sleep state. power_state=%d  rc=%d\n",
			__func__, es325->power_state, rc);

	mutex_unlock(&es325->power_lock);

	return rc;
}

#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
static int es325_codec_sleep_delay_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&sleep_delay_timer)) {
		ktime_t r = hrtimer_get_remaining(&sleep_delay_timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}

	pr_info("%s(): hrtimer_active does not activated\n", __func__);
	return 0;
}

static void es325_codec_sleep_delay_enable(struct timed_output_dev *timed_dev, int value)
{
	int rc;

	rc = hrtimer_cancel(&sleep_delay_timer);

	rc = hrtimer_start(&sleep_delay_timer,
			  ktime_set(value / 1000, (value % 1000) * 1000000),
			  HRTIMER_MODE_REL);
	pr_info("%s(): hrtimer_start returns = %d\n", __func__, rc);
}

static void es325_sleep_work(struct work_struct *work)
{
	pr_info("%s()\n", __func__);

	delayed_sleep(work);
}

static void timed_es325_sleep(struct timed_output_dev *sdev)
{
	pr_info("%s()\n", __func__);

	schedule_work(&work_es325_sleep);
}

static enum hrtimer_restart es325_codec_sleep_delay_timer_func(struct hrtimer *timer)
{
	pr_info("%s()\n", __func__);

	timed_es325_sleep(NULL);

	return HRTIMER_NORESTART;
}
#endif

int es325_codec_wakeup(void)
{
	unsigned int offset = ES325_WRITE_VE_OFFSET;
	unsigned int width = ES325_WRITE_VE_WIDTH;
	char msg[16];
	static char sync_ok[] = { 0x80, 0x00, 0x00, 0x01 };
	static char sync_cmd[] = { 0x01, 0x00, 0x00, 0x80 };
	struct es325_priv *es325 = &es325_priv;
	struct device *dev = &es325->gen0_client->dev;
	int rc = -ENXIO;
	int ret, cnt;
	pr_info("%s(): entry", __func__);

	mutex_lock(&es325->power_lock);

	if (es325->power_state == ES325_POWER_AWAKE) {
		dev_err(dev,
			"Chip is already awake. power_state=%d\n",
			es325->power_state);
		rc = 0;
		goto EXIT_WAKEUP_NOGPIO;
	}

	/* Turn on reference clock and give it ample time to stabilize. */
	rc = clk_prepare_enable(xo_handle_a2);
	if (IS_ERR_VALUE(rc)) {
		rc = PTR_ERR(xo_handle_a2);
		pr_err("%s: Failed to enable the msm_xo_mode_vote(%d)\n", __func__, rc);
		goto EXIT_WAKEUP;
	}
	usleep_range(800, 1000);
	/* Wakeup signal H -> L. */
	ret = gpio_direction_output(es325->pdata->wakeup_gpio, 0);
	if (ret < 0)
		dev_err(dev, "%s(): es325_wakeup %d direction failed", __func__, es325->pdata->wakeup_gpio);
	usleep_range(WAKEUP_PIN_DETECT_TIME, WAKEUP_PIN_DETECT_TIME+1000);

	/* Poll until the es325 is ready to accept a command.
	* Wait for 5ms, return error if it's not responding.
	* If this returns an error, we need to resend sync command
	* max. 100ms
	*/
	for (cnt=0; cnt<WAKEUP_PIN_DETECT_MAX_CNT; cnt++) {
		 rc = ES325_BUS_WRITE(es325, offset, width, sync_cmd, 4, 0);
	 if (rc < 0) {
	        dev_err(dev, "Sync write failed. retry cnt=%d\n", cnt+1);
	        /* wait 5ms for stable */
	        usleep_range(WAKEUP_PIN_DETECT_TIME, WAKEUP_PIN_DETECT_TIME+1000);
	 } else
	            cnt = WAKEUP_PIN_DETECT_MAX_CNT;
	}
	if (rc < 0) {
		dev_err(dev, "Sync write failed. goto EXIT_WAKEUP\n");
		goto EXIT_WAKEUP;
	}

	usleep_range(WAKEUP_PIN_DETECT_TIME, WAKEUP_PIN_DETECT_TIME+1000);
	memset(msg, 0, sizeof (msg));

	for (cnt=0; cnt<WAKEUP_PIN_DETECT_MAX_CNT; cnt++) {
		rc = ES325_BUS_READ(es325, ES325_READ_VE_OFFSET, ES325_READ_VE_WIDTH,
					msg, 4, 1);
		if (rc == 0)
			goto EXIT_WAKEUP;
		usleep_range(WAKEUP_PIN_DETECT_TIME, WAKEUP_PIN_DETECT_TIME+1000);
	}
	dev_warn(dev, "First sync attempt after sleep failed. rc=%d\n", rc);

	if (memcmp(msg, sync_ok, 4) != 0) {
		dev_err(dev, "Wakeup ACK=0x%X.\n", *(u32*)msg);
		rc = -ENXIO;
	}

EXIT_WAKEUP:
	/* wakeup signal L -> H */
	gpio_direction_output(es325->pdata->wakeup_gpio, 1);

EXIT_WAKEUP_NOGPIO:
	if (rc == 0) {
		dev_info(dev, "Wakeup successful.\n");
		es325->power_state = ES325_POWER_AWAKE;
	} else
		dev_err(dev, "Wakeup FAILED.\n");

	mutex_unlock(&es325->power_lock);

	return rc;
}

static int es325_put_control_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value;
	int rc = 0;

	pr_debug("%s(): reg = %d\n", __func__, reg);
	pr_debug("%s(): shift = %d\n", __func__, shift);
	pr_debug("%s(): max = %d\n", __func__, max);
	pr_debug("%s(): invert = %d\n", __func__, invert);
	pr_debug("%s(): value = %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	value = ucontrol->value.integer.value[0];
	rc = es325_write(NULL, reg, value);

	return 0;
}

static int es325_get_control_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value;

	pr_debug("%s(): reg = %d\n", __func__, reg);
	pr_debug("%s(): shift = %d\n", __func__, shift);
	pr_debug("%s(): max = %d\n", __func__, max);
	pr_debug("%s(): invert = %d\n", __func__, invert);
	value = es325_read(NULL, reg);
	pr_debug("%s(): value = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	pr_debug("%s(): value = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int es325_put_control_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int max = e->max;
	unsigned int value;
	int rc = 0;

	pr_debug("%s(): reg = 0x%x\n", __func__, reg);
	pr_debug("%s(): max = %d\n", __func__, max);
	pr_debug("%s(): value.integer.value[0] = %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	value = ucontrol->value.integer.value[0];
	rc = es325_write(NULL, reg, value);

	return 0;
}

static int es325_get_control_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	pr_debug("%s(): reg = 0x%x\n", __func__, reg);
	value = es325_read(NULL, reg);
	ucontrol->value.enumerated.item[0] = value;
	pr_debug("%s(): value = %d\n", __func__, value);

	return 0;
}

/* GAC */
static int es325_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es325_rx1_route_ena;
	pr_info("%s(): is = %d\n", __func__,
		es325_rx1_route_ena);

	return 0;
}

static int es325_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	if(!is_fw_download)
		return 0;
	es325_rx1_route_ena = ucontrol->value.integer.value[0];
	pr_info("%s(): is = %d\n", __func__,
		es325_rx1_route_ena);

	return 0;
}

static int es325_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es325_tx1_route_ena;
	pr_info("%s(): is = %d\n", __func__,
		es325_tx1_route_ena);

	return 0;
}

static int es325_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	if(!is_fw_download)
		return 0;
	es325_tx1_route_ena = ucontrol->value.integer.value[0];
	pr_info("%s(): is = %d\n", __func__,
		es325_tx1_route_ena);

	return 0;
}

static int es325_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es325_rx2_route_ena;
	pr_info("%s(): is = %d\n", __func__,
		es325_rx2_route_ena);

	return 0;
}

static int es325_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	if(!is_fw_download)
		return 0;
	es325_rx2_route_ena = ucontrol->value.integer.value[0];
	pr_info("%s(): is = %d\n", __func__,
		es325_rx2_route_ena);

	return 0;
}

int es325_remote_route_enable(struct snd_soc_dai *dai)
{
	unsigned int ret = 0;
	switch (dai->id) {
	case ES325_SLIM_1_PB:
		ret = es325_rx1_route_ena;
		break;
	case ES325_SLIM_1_CAP:
		ret = es325_tx1_route_ena;
		break;
	case ES325_SLIM_2_PB:
		ret = es325_rx2_route_ena;
		break;
	default:
		return 0;
	}
	pr_info("%s:(dai->id = %d)(rx1_ena = %d)(tx1_ena = %d)(rx2_ena = %d)\n",
			__func__, dai->id,
			es325_rx1_route_ena, es325_tx1_route_ena, es325_rx2_route_ena);
	return ret;
}
EXPORT_SYMBOL_GPL(es325_remote_route_enable);
/* GAC */

int es325_get_tx1_enabled(void)
{
	pr_info("%s:(tx1_ena = %d)\n",
			__func__, es325_tx1_route_ena);

	return es325_tx1_route_ena;
}
EXPORT_SYMBOL_GPL(es325_get_tx1_enabled);

static int es325_put_internal_route_config(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct es325_priv *es325 = &es325_priv;
	int i;
	long route_num;
	u8 msg[4];
	u8 *msg_ptr;

	if(!is_fw_download)
		return 0;

	route_num = ucontrol->value.integer.value[0];

	pr_info("%s(): route_num = %s es325_internal_route_num = %s\n",
			__func__, es325_internal_route_configs_text[route_num],
			es325_internal_route_configs_text[es325_internal_route_num]);

	if(route_num > ES325_STOP)
		return 0;

	if (route_num == ES325_NB) {
		is_wideband = false;
		if (es325_rx1_route_ena == 1 && es325_tx1_route_ena == 1) {
			if (route_num_old == ES325_TWO_MIC_CT_WB) {
				pr_info("%s : 2-mic CT Handover WB -> NB\n", __func__);
				route_num = ES325_TWO_MIC_CT;
			} else if (route_num_old == ES325_TWO_MIC_FT_WB) {
				pr_info("%s : 2-mic FT Handover WB -> NB\n", __func__);
				route_num = ES325_TWO_MIC_FT;
			} else {
				pr_info("%s : Narrowband to Narrowband case -> No Change :: sungmin test\n", __func__);
				route_num = route_num_old;
			}
		} else {
			pr_info("%s() : sungmin : AMR Codec type is changed but Voice call is NOT activated.", __func__);
			return 0;
		}
	} else if (route_num == ES325_WB) {
		is_wideband = true;
		if (es325_rx1_route_ena == 1 && es325_tx1_route_ena == 1) {
			if (route_num_old == ES325_TWO_MIC_CT) {
				pr_info("%s : 2-mic CT Handover NB -> WB\n", __func__);
				route_num = ES325_TWO_MIC_CT_WB;
			} else if (route_num_old == ES325_TWO_MIC_FT) {
				pr_info("%s : 2-mic FT Handover NB -> WB\n", __func__);
				route_num = ES325_TWO_MIC_FT_WB;
			} else {
				pr_info("%s : Wideband to Wideband case -> No Change :: sungmin test\n", __func__);
				route_num = route_num_old;
			}
		} else {
			pr_info("%s() : sungmin : AMR Codec type is changed but Voice call is NOT activated.", __func__);
			return 0;
		}
	}

	if (is_wideband) {
		if (route_num == ES325_TWO_MIC_CT) {
			route_num = ES325_TWO_MIC_CT_WB;
		} else if (route_num == ES325_TWO_MIC_FT) {
			route_num = ES325_TWO_MIC_FT_WB;
		}
	}
	if(es325_internal_route_num != route_num) {
		es325_internal_route_num = route_num_old = route_num;
		/* Flag to setup slimbus channel for es325 */
		/* jeremy.pi@lge.com
		*   blocked ch mapping flag
		*   controled by UCM
		*/
//		es325_rx1_route_ena = 1;
//		es325_tx1_route_ena = 1;
//		es325_rx2_route_ena = 1;

		if(es325_internal_route_num < ES325_STOP) {
#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
			hrtimer_cancel(&sleep_delay_timer);
#endif
			if(es325_codec_wakeup() == 0) {
				msg_ptr = &es325_internal_route_configs[es325_internal_route_num][0];
				for (i = 0; ; msg_ptr +=4) {
					if (*msg_ptr == 0xff)
						break;
					memcpy(msg, msg_ptr, 4);
					es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
										ES325_WRITE_VE_WIDTH, msg, 4, 1);
					pr_info("%s(): msg = %02x%02x%02x%02x\n", __func__,
							msg[0], msg[1], msg[2], msg[3]);
				}
			}
		} else if (es325_internal_route_num == ES325_STOP) {
			if (es325->power_state == ES325_POWER_AWAKE) {
				msg_ptr = &es325_internal_route_configs[ES325_STOP][0];
				for (i = 0; ; msg_ptr +=4) {
					if (*msg_ptr == 0xff)
						break;
					memcpy(msg, msg_ptr, 4);
					es325_slim_write(es325, ES325_WRITE_VE_OFFSET,
										ES325_WRITE_VE_WIDTH, msg, 4, 1);
					pr_info("%s(): msg = %02x%02x%02x%02x\n", __func__,
							msg[0], msg[1], msg[2], msg[3]);
				}
			}
			/* Flag to setup slimbus channel for without es325 */
			// lock power lock
			es325_rx1_route_ena = 0;
			es325_tx1_route_ena = 0;
			es325_rx2_route_ena = 0;

#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
			es325_codec_sleep_delay_enable(&sleep_delay, sleep_delay_time);
#else
			INIT_DELAYED_WORK(&es325->sleep_work, delayed_sleep);
			schedule_delayed_work(&es325->sleep_work, msecs_to_jiffies(ES325_SLEEP_DELAY));
			// set power state to be waiting to sleep
			// unlock power lock
#endif
		}
	}

	return 0;
}

static int es325_get_internal_route_config(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es325_internal_route_num;
	pr_info("%s(): internal_route_num = %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	pr_info("%s(): internal_route_num = %ld\n", __func__,
		es325_internal_route_num);

	return 0;
}


/* generic gain translation */
static int es325_index_to_gain(int min, int step, int index)
{
	return  (min + (step * index));
}
static int es325_gain_to_index(int min, int step, int gain)
{
	return  ((gain - min) / step);
}

/* dereverb gain */
static int es325_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 12) {
		pr_info("%s() ucontrol = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
		value = es325_index_to_gain(-12, 1, ucontrol->value.integer.value[0]);
		pr_info("%s() value = %d\n", __func__, value);
		rc = es325_write(NULL, reg, value);
	}

	return 0;
}

static int es325_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es325_read(NULL, reg);
	pr_info("%s() value = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = es325_gain_to_index(-12, 1, value);
	pr_info("%s() ucontrol = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

/* bwe high band gain */
static int es325_put_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 30) {
		pr_info("%s() ucontrol = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
		value = es325_index_to_gain(-10, 1, ucontrol->value.integer.value[0]);
		pr_info("%s() value = %d\n", __func__,
			value);
		rc = es325_write(NULL, reg, value);
	}

	return 0;
}

static int es325_get_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es325_read(NULL, reg);
	pr_info("%s() value = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = es325_gain_to_index(-10, 1, value);
	pr_info("%s() ucontrol = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

/* bwe max snr */
static int es325_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 70) {
		pr_info("%s() ucontrol = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
		value = es325_index_to_gain(-20, 1, ucontrol->value.integer.value[0]);
		pr_info("%s() value = %d\n", __func__,
			value);
		rc = es325_write(NULL, reg, value);
	}

	return 0;
}

static int es325_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es325_read(NULL, reg);
	pr_info("%s() value = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = es325_gain_to_index(-20, 1, value);
	pr_info("%s() ucontrol = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

#ifdef ES325_TEXT_ENUMS
static const char *es325_mic_config_texts[] = {
	"CT Two Mic", "FT Two Mic", "DV One Mic", "EXT One Mic", "BT One Mic",
	"CT ASR Two Mic", "FT ASR Two Mic", "EXT ASR One Mic", "FT ASR One Mic",
};
static const struct soc_enum es325_mic_config_enum =
	SOC_ENUM_SINGLE(ES325_MIC_CONFIG, 0,
			ARRAY_SIZE(es325_mic_config_texts),
			es325_mic_config_texts);

static const char *es325_aec_mode_texts[] = {
	"Off", "On", "RSVRD2", "RSVRD3", "RSVRD4", "On Half Duplex"
};
static const struct soc_enum es325_aec_mode_enum =
	SOC_ENUM_SINGLE(ES325_AEC_MODE, 0,
			ARRAY_SIZE(es325_aec_mode_texts),
			es325_aec_mode_texts);

static const char *es325_algo_rates_text[] = {
	"Rate 8khz", "Rate 16khz", "Rate 24khz", "Rate 48khz", "Rate 96khz", "Rate 192khz"
};
static const struct soc_enum es325_algo_sample_rate_enum =
	SOC_ENUM_SINGLE(ES325_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es325_algo_rates_text),
			es325_algo_rates_text);
static const struct soc_enum es325_algo_mix_rate_enum =
	SOC_ENUM_SINGLE(ES325_MIX_SAMPLE_RATE, 0,
			ARRAY_SIZE(es325_algo_rates_text),
			es325_algo_rates_text);

static const char *es325_algorithms_text[] = {
	"None", "VP", "2CHREC", "AUDIO", "4CHPASS"
};
static const struct soc_enum es325_algorithms_enum =
	SOC_ENUM_SINGLE(ES325_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es325_algorithms_text),
			es325_algorithms_text);

static const char *es325_off_on_texts[] = {
	"Off", "On"
};
static const struct soc_enum es325_veq_enable_enum =
	SOC_ENUM_SINGLE(ES325_VEQ_ENABLE, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);
static const struct soc_enum es325_dereverb_enable_enum =
	SOC_ENUM_SINGLE(ES325_DEREVERB_ENABLE, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);
static const struct soc_enum es325_bwe_enable_enum =
	SOC_ENUM_SINGLE(ES325_BWE_ENABLE, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);
static const struct soc_enum es325_bwe_post_eq_enable_enum =
	SOC_ENUM_SINGLE(ES325_BWE_POST_EQ_ENABLE, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);
static const struct soc_enum es325_algo_processing_enable_enum =
	SOC_ENUM_SINGLE(ES325_ALGO_PROCESSING, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);


static const char *es325_internal_route_configs_text[ES325_STOP+1] = {
	"ONE MIC CT",			/* ES325_ONE_MIC_CT */
	"ONE MIC DV",			/* ES325_ONE_MIC_DV */
	"TWO MIC CT",			/* ES325_TWO_MIC_CT */
	"TWO MIC FT",			/* ES325_TWO_MIC_FT */
	"TWO MIC CT ANC",		/* ES325_TWO_MIC_CT_ANC */
	"TWO MIC CT WB",		/* ES325_TWO_MIC_CT_WB */
	"TWO MIC FT WB",		/* ES325_TWO_MIC_FT_WB */
	"TWO MIC CT VOLTE",		/* ES325_TWO_MIC_CT_VOLTE */
	"TWO MIC FT VOLTE",		/* ES325_TWO_MIC_FT_VOLTE */
	"TWO MIC CT VOIP",		/* ES325_TWO_MIC_CT_VOIP */
	"TWO MIC FT VOIP",		/* ES325_TWO_MIC_FT_VOIP */
	"ONE MIC HEADSET",		/* ES325_ONE_MIC_HEADSET */
	"ONE CH MUSIC PLAYBACK",		/* ES325_ONE_CH_MUSIC_PLAYBACK */
	"TWO CH MUSIC PLAYBACK",	/* ES325_TWO_CH_MUSIC_PLAYBACK */
	"ONE MIC VOICE REC",		/* ES325_ONE_MIC_VOICE_REC */
	"PRI MIC LOOPBACK",		/* ES325_PRI_MIC_LOOPBACK */
	"SEC MIC LOOPBACK",		/* ES325_SEC_MIC_LOOPBACK */
	"ONE MIC VIDEO REC",		/* ES325_ONE_MIC_VIDEO_REC */
	"ONE MIC ASR",			/* ES325_ONE_MIC_ASR */
	"TWO MIC CT ASR",		/* ES325_TWO_MIC_CT_ASR */
	"TWO MIC CT ASR WB",		/* ES325_TWO_MIC_CT_ASR_WB */
	"TWO MIC FT ASR",		/* ES325_TWO_MIC_FT_ASR */
	"TWO MIC FT ASR WB",		/* ES325_TWO_MIC_FT_ASR_WB */
	"ONE MIC DESKTOP ASR",		/* ES325_ONE_MIC_DESKTOP_ASR */
	"ONE MIC DESKTOP ASR WB",	/* ES325_ONE_MIC_DESKTOP_ASR_WB */
	"TWO MIC CT LOW NS",		/* ES325_TWO_MIC_CT_LOW_NS */
	"ONE MIC DV LOW NS",		/* ES325_ONE_MIC_DV_LOW_NS */
	"BOTTOM MIC SEALED",		/* ES325_BOTTOM_MIC_SEALED */
	"TOP MIC SEALED",		/* ES325_TOP_MIC_SEALED */
	"DUMMY",			/* ES325_DUMMY */
	"NB",				/* ES325_NB */
	"WB",				/* ES325_WB */
	"STOP"				/* ES325_STOP */
};


static const struct soc_enum es325_internal_route_config_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es325_internal_route_configs_text),
			es325_internal_route_configs_text);
#endif

/* digital gain */
static int es325_put_digital_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	pr_info("%s() ucontrol = %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	value = ucontrol->value.integer.value[0];
	rc = es325_write(NULL, reg, value);

	return 0;
}

static int es325_get_digital_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es325_read(NULL, reg);
	pr_info("%s() value = %d\n", __func__, value);
	pr_info("%s() ucontrol = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static struct snd_kcontrol_new es325_digital_ext_snd_controls[] = {
	/* commit controls */
/* GAC */
	SOC_SINGLE_EXT("ES325 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_rx1_route_enable_value,
		       es325_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES325 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_tx1_route_enable_value,
		       es325_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES325 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_rx2_route_enable_value,
		       es325_put_rx2_route_enable_value),
/* GAC */
#ifdef ES325_TEXT_ENUMS
	SOC_ENUM_EXT("ES325 Mic Config", es325_mic_config_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 AEC Mode", es325_aec_mode_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 VEQ Enable", es325_veq_enable_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 Dereverb Enable", es325_dereverb_enable_enum,
		es325_get_control_enum, es325_put_control_enum),
#else
	SOC_SINGLE_EXT("ES325 Mic Config",
		ES325_MIC_CONFIG, 0, 8, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 AEC Mode",
		ES325_AEC_MODE, 0, 6, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 VEQ Enable",
		ES325_VEQ_ENABLE, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 Dereverb Enable",
		ES325_DEREVERB_ENABLE, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
#endif
	SOC_SINGLE_EXT("ES325 Dereverb Gain",
		ES325_DEREVERB_GAIN, 0, 100, 0,
		es325_get_dereverb_gain_value, es325_put_dereverb_gain_value),
#ifdef ES325_TEXT_ENUMS
	SOC_ENUM_EXT("ES325 BWE Enable", es325_bwe_enable_enum,
		es325_get_control_enum, es325_put_control_enum),
#else
	SOC_SINGLE_EXT("ES325 BWE Enable",
		ES325_BWE_ENABLE, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
#endif
	SOC_SINGLE_EXT("ES325 BWE High Band Gain",
		ES325_BWE_HIGH_BAND_GAIN, 0, 100, 0,
		es325_get_bwe_high_band_gain_value,
		es325_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("ES325 BWE Max SNR",
		ES325_BWE_MAX_SNR, 0, 100, 0,
		es325_get_bwe_max_snr_value, es325_put_bwe_max_snr_value),
#ifdef ES325_TEXT_ENUMS
	SOC_ENUM_EXT("ES325 BWE Post EQ Enable", es325_bwe_post_eq_enable_enum,
		es325_get_control_enum, es325_put_control_enum),
#else
	SOC_SINGLE_EXT("ES325 BWE Post EQ Enable",
		ES325_BWE_MAX_SNR, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
#endif
	SOC_SINGLE_EXT("ES325 SLIMbus Link Multi Channel",
		ES325_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	/* SOC_SINGLE_EXT("ES325 Set Power State", */
	/*	ES325_POWER_STATE, 0, 1, 0, */
	/*	es325_get_control_value, es325_put_control_value), */
#ifdef ES325_TEXT_ENUMS
	SOC_ENUM_EXT("ES325 Algorithm Processing", es325_algo_processing_enable_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 Algorithm Sample Rate", es325_algo_sample_rate_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 Algorithm", es325_algorithms_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 Mix Sample Rate", es325_algo_mix_rate_enum,
		es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("ES325 INTERNAL ROUTE CONFIG", es325_internal_route_config_enum,
		es325_get_internal_route_config, es325_put_internal_route_config),
#else
	SOC_SINGLE_EXT("ES325 Algorithm Processing",
		ES325_ALGO_PROCESSING, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 Algorithm Sample Rate",
		ES325_ALGO_SAMPLE_RATE, 0, 5, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 Algorithm",
		ES325_ALGORITHM, 0, 4, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("ES325 Mix Sample Rate",
		ES325_MIX_SAMPLE_RATE, 0, 5, 0,
		es325_get_control_value, es325_put_control_value),
#endif
	SOC_SINGLE_EXT("ES325 INTERNAL ROUTE CONFIG NUM",
		SND_SOC_NOPM, 0, 100, 0,
		es325_get_internal_route_config, es325_put_internal_route_config),

	SOC_SINGLE_EXT("ES325 FW Reload",
		SND_SOC_NOPM, 0, 1, 0,
		NULL, es325_firmware_reload),

	SOC_SINGLE_EXT("ES325 VP ONOFF",
		SND_SOC_NOPM, 0, 100, 0,
		NULL, es325_put_vp_onoff),

	SOC_SINGLE_EXT("ES325 MIC SEALED",
		SND_SOC_NOPM, 0, 100, 0,
		NULL, es325_put_mic_sealed),

	SOC_SINGLE_EXT("Dgain PRI",
		       ES325_DIGITAL_GAIN_PRIMARY, 0, 255, 0,
		       es325_get_digital_gain_value,
		       es325_put_digital_gain_value),
	SOC_SINGLE_EXT("Dgain SEC",
		       ES325_DIGITAL_GAIN_SECONDARY, 0, 255, 0,
		       es325_get_digital_gain_value,
		       es325_put_digital_gain_value),
	SOC_SINGLE_EXT("Dgain FEIN",
		       ES325_DIGITAL_GAIN_FEIN, 0, 255, 0,
		       es325_get_digital_gain_value,
		       es325_put_digital_gain_value),
	SOC_SINGLE_EXT("Dgain CSOUT",
		       ES325_DIGITAL_GAIN_CSOUT, 0, 255, 0,
		       es325_get_digital_gain_value,
		       es325_put_digital_gain_value),
	SOC_SINGLE_EXT("Dgain FEOUT1",
		       ES325_DIGITAL_GAIN_FEOUT1, 0, 255, 0,
		       es325_get_digital_gain_value,
		       es325_put_digital_gain_value),

	SOC_SINGLE_EXT("TX Noise Suppress Level",
		       ES325_TX_NOISE_SUPPRESS_LEVEL, 0, 15, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("RX Noise Suppress Level",
		       ES325_RX_NOISE_SUPPRESS_LEVEL, 0, 15, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("TX Out Limiter Max Level",
		       ES325_TX_OUT_LIMITER_MAX_LEVEL, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("TX In Limiter Max Level",
		       ES325_TX_IN_LIMITER_MAX_LEVEL, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("RX Out Limiter Max Level",
		       ES325_RX_OUT_LIMITER_MAX_LEVEL, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("RX Post EQ",
		       ES325_RX_POST_EQ, 0, 1, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("TX Post EQ",
		       ES325_TX_POST_EQ, 0, 1, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("TX MBC",
		       ES325_TX_MBC, 0, 1, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("RX MBC",
		       ES325_RX_MBC, 0, 1, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("AEC CNG",
		       ES325_AEC_CNG, 0, 1, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("AEC ESE",
		       ES325_AEC_ESE, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("AEC Speaker Vol",
		       ES325_AEC_SPEAKER_VOLUME, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("AEC CNG Gain",
		       ES325_AEC_CNG_GAIN, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("TX Interrupt Level",
		       ES325_TX_INTR_LEVEL, 0, 65535, 0,
		       es325_get_control_value,
		       es325_put_control_value),
	SOC_SINGLE_EXT("Position Supp Tradeoff",
		       ES325_POS_SUPP_TRADEOFF, 0, 7, 0,
		       es325_get_control_value,
		       es325_put_control_value),

};

/* GAC */
/* staged commands have been removed, staged commands are only used for
 * the route
 */

#if 0
static struct snd_kcontrol_new es325_digital_staged_ext_snd_controls[] = {
	/* staged controls */
	SOC_SINGLE_EXT("es325 Mic Config (S)",
		ES325_MIC_CONFIG_STAGED, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 AEC Mode (S)",
		ES325_AEC_MODE_STAGED, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 VEQ Enable (S)",
		ES325_VEQ_ENABLE_STAGED, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 Dereverb Enable (S)",
		ES325_DEREVERB_ENABLE_STAGED, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 Dereverb Gain (S)",
		ES325_DEREVERB_GAIN_STAGED, 0, 100, 0,
		es325_get_dereverb_gain_value, es325_put_dereverb_gain_value),
	SOC_SINGLE_EXT("es325 BWE Enable (S)",
		ES325_BWE_ENABLE_STAGED, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 BWE High Band Gain (S)",
		ES325_BWE_HIGH_BAND_GAIN_STAGED, 0, 100, 0,
		es325_get_bwe_high_band_gain_value,
		es325_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("es325 BWE Max SNR (S)",
		ES325_BWE_MAX_SNR_STAGED, 0, 100, 0,
		es325_get_bwe_max_snr_value, es325_put_bwe_max_snr_value),
	SOC_SINGLE_EXT("es325 BWE Post EQ Enable (S)",
		ES325_BWE_MAX_SNR_STAGED, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 SLIMbus Link Multi Channel (S)",
		ES325_SLIMBUS_LINK_MULTI_CHANNEL_STAGED, 0, 65535, 0,
		es325_get_control_value, es325_put_control_value),
	/* SOC_SINGLE_EXT("es325 Set Power State (S)", */
	/*	ES325_POWER_STATE_STAGED, 0, 1, 0, */
	/*	es325_get_control_value, es325_put_control_value), */
	SOC_SINGLE_EXT("es325 Algorithm Processing (S)",
		ES325_ALGO_PROCESSING_STAGED, 0, 1, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 Algorithm Sample Rate (S)",
		ES325_ALGO_SAMPLE_RATE_STAGED, 0, 5, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 Algorithm (S)",
		ES325_ALGORITHM_STAGED, 0, 4, 0,
		es325_get_control_value, es325_put_control_value),
	SOC_SINGLE_EXT("es325 Mix Sample Rate (S)",
		ES325_MIX_SAMPLE_RATE_STAGED, 0, 5, 0,
		es325_get_control_value, es325_put_control_value),
};
#endif
/* GAC */

#if defined(CONFIG_SND_SOC_ES325_I2S)
static int es325_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
	unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
	int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
	int div)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_channel_map(struct snd_soc_dai *dai, unsigned int tx_num,
	unsigned int *tx_slot, unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	switch (dai->id) {
	case 0:
		break;
	case 1:
		break;
	default:
		return -EINVAL;
	}

	if (tristate)
		val = 0x0001;
	else
		val = 0x0000;

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return (snd_soc_write(codec, paramid, val));
}

static int es325_i2s_port_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	/* Is this valid since DACs are not statically mapped to DAIs? */
	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);
	switch (dai->id) {
	case 0:
		break;
	case 1:
		break;
	default:
		return -EINVAL;
	}

	if (mute)
		val = 0x0000;
	else
		val = 0x0001;

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return (snd_soc_write(codec, paramid, val));
}

static int es325_i2s_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);


	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static void es325_i2s_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
}

static int es325_i2s_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	// struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);
	int bits_per_sample = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);
	switch (dai->id) {
	case 0:
		dev_dbg(codec->dev, "%s(): ES325_PORTA_PARAMID\n", __func__);
		break;
	case 1:
		dev_dbg(codec->dev, "%s(): ES325_PORTB_PARAMID\n", __func__);
		break;
	default:
		dev_dbg(codec->dev, "%s(): unknown port\n", __func__);
		dev_dbg(codec->dev, "%s(): exit\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_channels(params) = %d\n", __func__,
		params_channels(params));
	switch (params_channels(params)) {
	case 1:
		dev_dbg(codec->dev, "%s(): 1 channel\n", __func__);
		break;
	case 2:
		dev_dbg(codec->dev, "%s(): 2 channels\n", __func__);
		break;
	case 4:
		dev_dbg(codec->dev, "%s(): 4 channels\n", __func__);
		break;
	default:
		dev_dbg(codec->dev, "%s(): unsupported number of channels\n",
			__func__);
		dev_dbg(codec->dev, "%s(): exit\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_rate(params) = %d\n", __func__,
		params_rate(params));
	switch (params_rate(params)) {
	case 8000:
		dev_dbg(codec->dev, "%s(): 8000Hz\n", __func__);
		break;
	case 11025:
		dev_dbg(codec->dev, "%s(): 11025\n", __func__);
		break;
	case 16000:
		dev_dbg(codec->dev, "%s(): 16000\n", __func__);
		break;
	case 22050:
		dev_dbg(codec->dev, "%s(): 22050\n", __func__);
		break;
	case 32000:
		dev_dbg(codec->dev, "%s(): 32000\n", __func__);
		break;
	case 48000:
		dev_dbg(codec->dev, "%s(): 48000\n", __func__);
		break;
	case 96000:
		dev_dbg(codec->dev, "%s(): 96000\n", __func__);
		break;
	case 192000:
		dev_dbg(codec->dev, "%s(): 96000\n", __func__);
		break;
	default:
		dev_dbg(codec->dev, "%s(): unsupported rate = %d\n", __func__,
			params_rate(params));
		dev_dbg(codec->dev, "%s(): exit\n", __func__);
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(codec->dev, "%s(): S16_LE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		dev_dbg(codec->dev, "%s(): S16_BE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		dev_dbg(codec->dev, "%s(): S20_3LE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S20_3BE:
		dev_dbg(codec->dev, "%s(): S20_3BE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dev_dbg(codec->dev, "%s(): S24_LE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S24_BE:
		dev_dbg(codec->dev, "%s(): S24_BE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(codec->dev, "%s(): S32_LE\n", __func__);
		bits_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
		dev_dbg(codec->dev, "%s(): S32_BE\n", __func__);
		bits_per_sample = 32;
		break;
	default:
		dev_dbg(codec->dev, "%s(): unknown format\n", __func__);
		dev_dbg(codec->dev, "%s(): exit\n", __func__);
		return -EINVAL;
	}
	if (ret) {
		dev_dbg(codec->dev, "%s(): snd_soc_update_bits() failed\n", __func__);
		dev_dbg(codec->dev, "%s(): exit\n", __func__);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_hw_free(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}

static int es325_i2s_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);
	dev_dbg(codec->dev, "%s(): cmd = %d\n", __func__, cmd);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}
#endif

static int es325_slim_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
#else
	return 0;
#endif
}

int es325_slim_set_channel_map(struct snd_soc_dai *dai, unsigned int tx_num,
	unsigned int *tx_slot, unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	int id = dai->id;
	int i;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	if (id == ES325_SLIM_1_PB ||
			id == ES325_SLIM_2_PB ||
			id == ES325_SLIM_3_PB) {
		es325->dai[id].ch_tot = rx_num;
		es325->dai[id].ch_act = 0;
		for (i = 0; i < rx_num; i++) {
			es325->dai[id].ch_num[i] = rx_slot[i];
			pr_info("%s() rx_slot[] = %d\n", __func__, rx_slot[i]);
		}
	} else if (id == ES325_SLIM_1_CAP ||
			id == ES325_SLIM_2_CAP ||
			id == ES325_SLIM_3_CAP) {
		es325->dai[id].ch_tot = tx_num;
		es325->dai[id].ch_act = 0;
		pr_info("%s() id = %d\n", __func__, id);
		pr_info("%s() ch_tot = %d\n", __func__, tx_num);
		for (i = 0; i < tx_num; i++) {
			es325->dai[id].ch_num[i] = tx_slot[i];
			pr_info("%s() tx_slot[] = %d\n", __func__, tx_slot[i]);
		}
	}

	// dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(es325_slim_set_channel_map);

int es325_slim_get_channel_map(struct snd_soc_dai *dai,
	unsigned int *tx_num, unsigned int *tx_slot,
	unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	struct es325_slim_ch *rx = es325->slim_rx;
	struct es325_slim_ch *tx = es325->slim_tx;
	int id = dai->id;
	int i;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);
	dev_dbg(codec->dev, "%s(): rx[0].ch_num = %d\n", __func__, rx[0].ch_num);
	dev_dbg(codec->dev, "%s(): rx[1].ch_num = %d\n", __func__, rx[1].ch_num);
	dev_dbg(codec->dev, "%s(): tx[0].ch_num = %d\n", __func__, tx[0].ch_num);
	dev_dbg(codec->dev, "%s(): tx[1].ch_num = %d\n", __func__, tx[1].ch_num);

	if (id == ES325_SLIM_1_PB) {
		*rx_num = es325_dai[id].playback.channels_max;
		dev_info(codec->dev, "%s(): *rx_num = %d\n", __func__, *rx_num);
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_1_PB_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): rx_slot[] = %d\n", __func__, rx_slot[i]);
		}
	}
	else if (id == ES325_SLIM_2_PB) {
		*rx_num = es325_dai[id].playback.channels_max;
		dev_info(codec->dev, "%s(): *rx_num = %d\n", __func__, *rx_num);
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_2_PB_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): rx_slot[] = %d\n", __func__, rx_slot[i]);
		}
	}
	else if (id == ES325_SLIM_3_PB) {
		*rx_num = es325_dai[id].playback.channels_max;
		dev_info(codec->dev, "%s(): *rx_num = %d\n", __func__, *rx_num);
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_3_PB_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): rx_slot[] = %d\n", __func__, rx_slot[i]);
		}
	}
	else if (id == ES325_SLIM_1_CAP) {
		*tx_num = es325_dai[id].capture.channels_max;
		dev_info(codec->dev, "%s(): *tx_num = %d\n", __func__, *tx_num);
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_1_CAP_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): tx_slot[] = %d\n", __func__, tx_slot[i]);
			rx_slot[i] = rx[ES325_SLIM_1_PB_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): rx_slot[] = %d\n", __func__, rx_slot[i]);
		}
	}
	else if (id == ES325_SLIM_2_CAP) {
		*tx_num = es325_dai[id].capture.channels_max;
		dev_info(codec->dev, "%s(): *tx_num = %d\n", __func__, *tx_num);
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_2_CAP_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): tx_slot[] = %d\n", __func__, tx_slot[i]);
		}
	}
	else if (id == ES325_SLIM_3_CAP) {
		*tx_num = es325_dai[id].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_3_CAP_OFFSET + i].ch_num;
			dev_info(codec->dev, "%s(): tx_slot[] = %d\n", __func__, tx_slot[i]);
		}
	}

	// dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(es325_slim_get_channel_map);

static int es325_slim_set_tristate(struct snd_soc_dai *dai, int tristate)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
#else
	return 0;
#endif
}

static int es325_slim_port_mute(struct snd_soc_dai *dai, int mute)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	/* Is this valid since DACs are not statically mapped to DAIs? */
	dev_dbg(codec->dev, "%s(): entry\n", __func__);
	dev_dbg(codec->dev, "%s(): dai->id = %d\n", __func__, dai->id);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
#else
	return 0;
#endif
}

static int es325_slim_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
#else
	return 0;
#endif
}

static void es325_slim_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s(): entry\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
#endif
}

int es325_slim_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	int id = dai->id;
	int channels;
	int rate;
	int ret = 0;

	dev_info(codec->dev, "%s(): entry\n", __func__);
	dev_info(codec->dev, "%s(): stream_name = %s\n", __func__,
			es325_dai[id].playback.stream_name);
	dev_info(codec->dev, "%s(): id = %d\n", __func__,
			es325_dai[id].id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		es325->dai[id].ch_tot = (channels >2)? 2 : channels;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported number of channels, %d\n",
			__func__, channels);
		return -EINVAL;
	}
	rate = params_rate(params);
	switch (rate) {
	case 8000:
	case 16000:
	case 32000:
	case 48000:
		es325->dai[id].rate = rate;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported rate, %d\n",
			__func__, rate);
		return -EINVAL;
	}

	dev_info(codec->dev, "%s(): exit\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(es325_slim_hw_params);

static int es325_slim_hw_free(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return rc;
}

static int es325_slim_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s(): entry\n", __func__);

	dev_dbg(codec->dev, "%s(): exit\n", __func__);
	return ret;
#else
	return 0;
#endif
}

int es325_slim_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	/* struct es325_priv *es325 = &es325_priv; */
	int id = dai->id;
	int ret = 0;

	dev_info(codec->dev, "%s(): entry\n", __func__);
	dev_info(codec->dev, "%s(): stream_name = %s\n",
			__func__,
			es325_dai[id].playback.stream_name);
	dev_info(codec->dev, "%s(): id = %d\n",
			__func__, es325_dai[id].id);
	dev_info(codec->dev, "%s(): cmd = %d\n",
			__func__, cmd);

	return ret;
}
EXPORT_SYMBOL_GPL(es325_slim_trigger);

#define ES325_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define ES325_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES325_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES325_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE)

#if defined(CONFIG_SND_SOC_ES325_I2S)
static struct snd_soc_dai_ops es325_portx_dai_ops = {
	.set_sysclk	= es325_i2s_set_sysclk,
	.set_pll	= es325_i2s_set_pll,
	.set_clkdiv	= es325_i2s_set_clkdiv,
	.set_fmt	= es325_i2s_set_dai_fmt,
	.set_tdm_slot	= es325_i2s_set_tdm_slot,
	.set_channel_map	= es325_i2s_set_channel_map,
	.set_tristate	= es325_i2s_set_tristate,
	.digital_mute	= es325_i2s_port_mute,
	.startup	= es325_i2s_startup,
	.shutdown	= es325_i2s_shutdown,
	.hw_params	= es325_i2s_hw_params,
	.hw_free	= es325_i2s_hw_free,
	.prepare	= es325_i2s_prepare,
	.trigger	= es325_i2s_trigger,
};
#endif

#if defined(CONFIG_SND_SOC_ES325_SLIM)
static struct snd_soc_dai_ops es325_slim_port_dai_ops = {
	.set_fmt	= es325_slim_set_dai_fmt,
	.set_channel_map	= es325_slim_set_channel_map,
	.get_channel_map	= es325_slim_get_channel_map,
	.set_tristate	= es325_slim_set_tristate,
	.digital_mute	= es325_slim_port_mute,
	.startup	= es325_slim_startup,
	.shutdown	= es325_slim_shutdown,
	.hw_params	= es325_slim_hw_params,
	.hw_free	= es325_slim_hw_free,
	.prepare	= es325_slim_prepare,
	.trigger	= es325_slim_trigger,
};
#endif

static struct snd_soc_dai_driver es325_dai[] = {
#if defined(CONFIG_SND_SOC_ES325_I2S)
/* initial support is for slimbus, not i2s */
	{
		.name = "es325-porta",
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		 },
		.ops = &es325_portx_dai_ops,
	},
	{
		.name = "es325-portb",
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		},
		.ops = &es325_portx_dai_ops,
	},
	{
		.name = "es325-portc",
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES325_RATES,
			.formats = ES325_FORMATS,
		},
		.ops = &es325_portx_dai_ops,
	},
#endif
#if defined(CONFIG_SND_SOC_ES325_SLIM)
	{
		.name = "es325-slim-rx1",
		.id = ES325_SLIM_1_PB,
		.playback = {
			.stream_name = "SLIM_PORT-1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
	{
		.name = "es325-slim-tx1",
		.id = ES325_SLIM_1_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
	{
		.name = "es325-slim-rx2",
		.id = ES325_SLIM_2_PB,
		.playback = {
			.stream_name = "SLIM_PORT-2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
	{
		.name = "es325-slim-tx2",
		.id = ES325_SLIM_2_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
	{
		.name = "es325-slim-rx3",
		.id = ES325_SLIM_3_PB,
		.playback = {
			.stream_name = "SLIM_PORT-3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
	{
		.name = "es325-slim-tx3",
		.id = ES325_SLIM_3_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES325_SLIMBUS_RATES,
			.formats = ES325_SLIMBUS_FORMATS,
		},
		.ops = &es325_slim_port_dai_ops,
	},
#endif
};

int es325_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	dev_info(codec->dev, "%s(): entry\n", __func__);
	dev_info(codec->dev, "%s(): codec->name = %s\n", __func__, codec->name);
#if 1
	rc = snd_soc_add_codec_controls(codec, es325_digital_ext_snd_controls,
			     ARRAY_SIZE(es325_digital_ext_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es325_digital_snd_controls failed\n", __func__);
#endif

/* GAC */
/* staged commands have been removed, staged commands are only used for
 * the route
 */

#if 0
	rc = snd_soc_add_codec_controls(codec,
			es325_digital_staged_ext_snd_controls,
			ARRAY_SIZE(es325_digital_staged_ext_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es325_digital_staged_snd_controls failed\n", __func__);
#endif
/* GAC */

	return rc;
}

static int es325_codec_probe(struct snd_soc_codec *codec)
{
	struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);
	/* int rc = 0; */

	dev_info(codec->dev, "%s(): entry\n", __func__);
	dev_info(codec->dev, "%s(): codec->name = %s\n", __func__, codec->name);
	dev_info(codec->dev, "%s(): codec = 0x%08x\n", __func__,
		(unsigned int)codec);
	dev_info(codec->dev, "%s(): es325 = 0x%08x\n", __func__,
		(unsigned int)es325);
	es325->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);
	dev_info(codec->dev, "%s(): codec->control_data = 0x%08x\n", __func__, (unsigned int)codec->control_data);

#if 0
	rc = snd_soc_add_codec_controls(codec, es325_digital_snd_controls,
			     ARRAY_SIZE(es325_digital_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es325_digital_snd_controls failed\n", __func__);
#endif

#if 0
	rc = snd_soc_add_codec_controls(codec,
			es325_digital_staged_snd_controls,
			ARRAY_SIZE(es325_digital_staged_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es325_digital_staged_snd_controls failed\n", __func__);
#endif

	dev_info(codec->dev, "%s(): exit\n", __func__);
	return 0;
}

static int  es325_codec_remove(struct snd_soc_codec *codec)
{
	struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec);

	kfree(es325);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_es325 = {
	.probe =	es325_codec_probe,
	.remove =	es325_codec_remove,
	.read =		es325_read,
	.write =	es325_write,
};

static void es325_parse_dtsi(struct device *dev, struct esxxx_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->reset_gpio = of_get_named_gpio_flags(np, "qcom,cdc-reset-gpio", 0, NULL);
	pdata->wakeup_gpio = of_get_named_gpio_flags(np, "qcom,cdc-wakeup-gpio", 0, NULL);
	pdata->int_gpio = of_get_named_gpio_flags(np, "qcom,cdc-ldo-gpio", 0, NULL);
	dev_dbg(dev, "%s(): reset_gpio : %d , wakeup_gpio : %d , int_gpio : %d\n",
		__func__, pdata->reset_gpio, pdata->wakeup_gpio, pdata->int_gpio);
}

#if defined(CONFIG_SND_SOC_ES325_I2C)
static int es325_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	struct es325_priv *es325;
	const char *filename = "audience-es325-fw.aud";
	int rc;

	dev_dbg(&i2c->dev, "%s(): entry\n", __func__);

	if (pdata == NULL) {
		dev_err(&i2c->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	es325 = kzalloc(sizeof(struct es325_priv), GFP_KERNEL);
	if (es325 == NULL) {
		dev_err(&i2c->dev, "%s(): kzalloc failed", __func__);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, es325);
	es325->this_client = i2c;

	rc = gpio_request(pdata->reset_gpio, "es325_reset");
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es325_reset request failed",
			__func__);
		goto reset_gpio_request_error;
	}
	rc = gpio_direction_output(pdata->reset_gpio, 1);
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es325_reset direction failed",
			__func__);
		goto reset_gpio_direction_error;
	}

	rc = gpio_request(pdata->wakeup_gpio, "es325_wakeup");
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es325_wakeup request failed",
			__func__);
		goto wakeup_gpio_request_error;
	}
	rc = gpio_direction_output(pdata->wakeup_gpio, 1);
	if (rc < 0) {
		dev_err(&i2c->dev, "%s(): es325_wakeup direction failed",
			__func__);
		goto wakeup_gpio_direction_error;
	}

	dev_dbg(&i2c->dev, "%s(): initialize interrupt\n", __func__);
	dev_dbg(&i2c->dev, "%s(): TODO: interrupts\n", __func__);

	gpio_set_value(pdata->reset_gpio, 0);
	gpio_set_value(pdata->wakeup_gpio, 1);

	es325->pdata = pdata;

	rc = request_firmware(&es325->fw, filename, &i2c->dev);
	if (rc) {
		dev_err(&i2c->dev, "%s(): request_firmware(%s) failed %d\n", __func__, filename, rc);
		goto request_firmware_error;
	}
	rc = es325_bootup(es325);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es325_bootup failed %d\n", __func__, rc);
		goto bootup_error;
	}
	release_firmware(es325->fw);

	rc = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_es325, es325_dai,
		ARRAY_SIZE(es325_dai));
	dev_dbg(&i2c->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n", __func__, rc);

	dev_dbg(&i2c->dev, "%s(): exit\n", __func__);
	return rc;

bootup_error:
request_firmware_error:
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);
reset_gpio_request_error:
pdata_error:
	dev_dbg(&i2c->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es325_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

static const struct i2c_device_id es325_i2c_id[] = {
	{ "es325", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, es325_i2c_id);

static struct i2c_driver es325_i2c_driver = {
	.driver = {
		.name = "es325-codec",
		.owner = THIS_MODULE,
	},
	.probe = es325_i2c_probe,
	.remove = es325_i2c_remove,
	.id_table = es325_i2c_id,
};
#else
static int es325_slim_probe(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata;
	const char *filename = "audience-es325-fw.aud";
	int rc;
	int cnt = 0;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);

	pdata = devm_kzalloc(&sbdev->dev, sizeof(struct esxxx_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&sbdev->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}
	es325_parse_dtsi(&sbdev->dev, pdata);

	if(strcmp(sbdev->name, "es325-codec-intf") == 0) {
		xo_handle_a2 = clk_get(&sbdev->dev, "es325_clk");
		if (IS_ERR(xo_handle_a2)) {
			rc = PTR_ERR(xo_handle_a2);
			pr_err("%s: Failed to get the xo_handle_a2(%d)\n", __func__, rc);
		} else {
			clk_set_rate(xo_handle_a2, 19200000);
			rc = clk_prepare_enable(xo_handle_a2);
			if (IS_ERR_VALUE(rc)) {
				rc = PTR_ERR(xo_handle_a2);
				pr_err("%s: Failed to enable the msm_xo_mode_vote(%d)\n", __func__, rc);
				clk_put(xo_handle_a2);
			}
		}
		rc = gpio_request(pdata->int_gpio, "es325_ldo_gpio");
		if (rc) {
			pr_err("%s : int_gpio  is failed.\n", __func__);
			goto ldo_gpio_direction_error;
		}
		rc = gpio_direction_output(pdata->int_gpio, 1);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): ldo enable failed", __func__);
			goto ldo_gpio_direction_error;
		}
		rc = gpio_request(pdata->reset_gpio, "es325_reset_gpio");
		if (rc) {
			pr_err("%s : reset_gpio  is failed.\n", __func__);
			goto reset_gpio_direction_error;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 0);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): es325_reset direction failed", __func__);
			goto reset_gpio_direction_error;
		}
		usleep_range(1500, 2000);
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): es325_reset direction failed", __func__);
			goto reset_gpio_direction_error;
		}

		rc = gpio_request(pdata->wakeup_gpio, "es325_wakeup_gpio");
		if (rc) {
			pr_err("%s : wakeup_gpio  is failed.\n", __func__);
			goto wakeup_gpio_direction_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 1);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): es325_wakeup direction failed", __func__);
			goto wakeup_gpio_direction_error;
		}
		usleep_range(15000, 16000);
	}

	if(strcmp(sbdev->name, "es325-codec-gen0") == 0) {
		/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/xxx */
		for (cnt = 0; cnt < ARRAY_SIZE(es325_device_attrs); cnt++) {
			rc = device_create_file(&sbdev->dev, &es325_device_attrs[cnt]);
			if (rc){
				dev_err(&sbdev->dev, "%s(): failed to create debug  [%d]\n", __func__, cnt);
				device_remove_file(&sbdev->dev, &es325_device_attrs[cnt]);
			}
		}

		/* /sys/class/earsmart/es325/xxx */
		es325_class = class_create(THIS_MODULE, "earsmart");
		if (IS_ERR(es325_class))
			dev_err(&sbdev->dev, "%s(): failed to class create\n", __func__);

		e325_codec_dev = device_create(es325_class, NULL, 0, NULL, "es325-codec");
		if (IS_ERR(e325_codec_dev))
			dev_err(&sbdev->dev, "%s(): failed to create device\n", __func__);
		for (cnt = 0; cnt < ARRAY_SIZE(es325_device_attrs); cnt++) {
			rc = device_create_file(e325_codec_dev, &es325_device_attrs[cnt]);
			if (rc){
				dev_err(&sbdev->dev, "%s(): failed to create class debug[%d]\n", __func__, cnt);
				device_remove_file(e325_codec_dev, &es325_device_attrs[cnt]);
			}
		}
	}

	slim_set_clientdata(sbdev, &es325_priv);

	if (strcmp(sbdev->name, "es325-codec-intf") == 0) {
		dev_dbg(&sbdev->dev, "%s(): interface device probe\n", __func__);
		es325_priv.intf_client = sbdev;
	}
	if (strcmp(sbdev->name, "es325-codec-gen0") == 0) {
		dev_dbg(&sbdev->dev, "%s(): generic device probe\n", __func__);
		es325_priv.gen0_client = sbdev;
	}

	if (es325_priv.intf_client == NULL || es325_priv.gen0_client == NULL) {
		dev_dbg(&sbdev->dev, "%s() incomplete initialization\n", __func__);
		return 0;
	}

	pr_info("%s initalized mutex for power state management\n", __func__);

	mutex_init(&es325_priv.power_lock);

	es325_priv.pdata = pdata;

	rc = request_firmware((const struct firmware **)&es325_priv.fw,
				filename, &sbdev->dev);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): request_firmware(%s) failed %d\n",
				__func__, filename, rc);
		goto request_firmware_error;
	}

#ifndef CONFIG_SLIMBUS_DEVICE_UP_COND
	pr_info("%s(): deferring firmware download...\n", __func__);
	rc = es325_defer_fw_download(&es325_priv);
#endif

#if defined(CONFIG_HRTIMER_SLEEP_DELAYED)
	INIT_WORK(&work_es325_sleep, es325_sleep_work);

	hrtimer_init(&sleep_delay_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sleep_delay_timer.function = es325_codec_sleep_delay_timer_func;
	timed_output_dev_register(&sleep_delay);
#endif

	dev_dbg(&sbdev->dev, "%s(): exit\n", __func__);

	return rc;

request_firmware_error:
ldo_gpio_direction_error:
	gpio_free(pdata->int_gpio);
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);
pdata_error:
	dev_dbg(&sbdev->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int register_snd_soc(struct es325_priv *priv)
{
	int rc;
	int i;
	int ch_cnt;
	struct slim_device *sbdev = priv->gen0_client;

	rc = es325_init_slim_slave(sbdev);

	dev_dbg(&sbdev->dev, "%s(): name = %s\n", __func__, sbdev->name);
	rc = snd_soc_register_codec(&sbdev->dev, &soc_codec_dev_es325, es325_dai,
				    ARRAY_SIZE(es325_dai));
	dev_dbg(&sbdev->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n", __func__, rc);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es325_dai); i++) {
		switch (es325_dai[i].id) {
		case ES325_SLIM_1_PB:
		case ES325_SLIM_2_PB:
		case ES325_SLIM_3_PB:
			ch_cnt = es325_dai[i].playback.channels_max;
			break;
		case ES325_SLIM_1_CAP:
		case ES325_SLIM_2_CAP:
		case ES325_SLIM_3_CAP:
			ch_cnt = es325_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		es325_priv.dai[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}

	es325_priv.dai[ES325_SLIM_1_PB].ch_num[0] = 152;
	es325_priv.dai[ES325_SLIM_1_PB].ch_num[1] = 153;
	es325_priv.dai[ES325_SLIM_1_CAP].ch_num[0] = 156;
	es325_priv.dai[ES325_SLIM_1_CAP].ch_num[1] = 157;
	es325_priv.dai[ES325_SLIM_2_PB].ch_num[0] = 154;
	es325_priv.dai[ES325_SLIM_2_PB].ch_num[1] = 155;
	es325_priv.dai[ES325_SLIM_2_CAP].ch_num[0] = 144;
	es325_priv.dai[ES325_SLIM_2_CAP].ch_num[1] = 145;
	es325_priv.dai[ES325_SLIM_3_PB].ch_num[0] = 134;
	es325_priv.dai[ES325_SLIM_3_PB].ch_num[1] = 135;
	es325_priv.dai[ES325_SLIM_3_CAP].ch_num[0] = 144;
	es325_priv.dai[ES325_SLIM_3_CAP].ch_num[1] = 145;

	dev_dbg(&sbdev->dev, "%s(): exit\n", __func__);
	return rc;
}

static int es325_slim_remove(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata = sbdev->dev.platform_data;

	dev_dbg(&sbdev->dev, "%s(): entry\n", __func__);
	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);

	snd_soc_unregister_codec(&sbdev->dev);

	dev_dbg(&sbdev->dev, "%s(): exit\n", __func__);

	return 0;
}

#if defined(CONFIG_SLIMBUS_DEVICE_UP_COND)
static int es325_slim_device_up(struct slim_device *sbdev)
{
	struct es325_priv *priv;
	int rc;
	dev_info(&sbdev->dev, "%s: name=%s\n", __func__, sbdev->name);
	dev_info(&sbdev->dev, "%s: laddr=%d\n", __func__, sbdev->laddr);
	/* Start the firmware download in the workqueue context. */
	priv = slim_get_devicedata(sbdev);
	dev_info(&sbdev->dev, "%s: priv=%p\n", __func__, priv);
	if (strcmp(sbdev->name, "es325-codec-intf") == 0)
		return 0;
	rc = fw_download(priv);
	BUG_ON(rc != 0);
	return rc;
}
#endif

static const struct slim_device_id es325_slim_id[] = {
	{ "es325-codec", 0 },
	{ "es325-codec-intf", 0 },
	{ "es325-codec-gen0", 0 },
	{  }
};
MODULE_DEVICE_TABLE(slim, es325_slim_id);

static struct slim_driver es325_slim_driver = {
	.driver = {
		.name = "es325-codec",
		.owner = THIS_MODULE,
	},
	.probe = es325_slim_probe,
	.remove = es325_slim_remove,
#if defined(CONFIG_SLIMBUS_DEVICE_UP_COND)
	.device_up = es325_slim_device_up,
#endif
	.id_table = es325_slim_id,
};
#endif

static __init int es325_init(void)
{
	int ret = 0;

	pr_info("%s(): entry", __func__);
#if defined(CONFIG_SND_SOC_ES325_I2C)
	ret = i2c_add_driver(&es325_i2c_driver);
	if (ret) {
		pr_err("Failed to register Audience es325 I2C driver: %d\n", ret);
	}
#else
	pr_info("%s(): slim_driver_register()", __func__);
	ret = slim_driver_register(&es325_slim_driver);
	if (ret) {
		pr_err("Failed to register Audience es325 SLIMbus driver: %d\n", ret);
	}
#endif

	pr_info("%s(): exit\n", __func__);
	return ret;
}
module_init(es325_init);

static __exit void es325_exit(void)
{
	pr_info("%s(): entry\n", __func__);
#if defined(CONFIG_SND_SOC_ES325_I2C)
	i2c_del_driver(&es325_i2c_driver);
#else
	/* no support from QCOM to unregister
	 * slim_driver_unregister(&es325_slim_driver);
	 */
#endif
	pr_info("%s(): exit\n", __func__);
}
module_exit(es325_exit);


MODULE_DESCRIPTION("ASoC ES325 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es325-codec");
MODULE_FIRMWARE("audience-es325-fw.aud");
