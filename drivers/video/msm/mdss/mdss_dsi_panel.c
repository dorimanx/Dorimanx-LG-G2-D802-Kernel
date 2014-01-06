/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#if defined(CONFIG_MACH_LGE)
#include <linux/syscore_ops.h>
#endif

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
#include <mach/board_lge.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/err.h>
#endif

#include "mdss_dsi.h"

#define DT_CMD_HDR 6
#ifdef CONFIG_LGE_LCD_TUNING
#define TUNING_REGSIZE 400
#endif

#if defined(CONFIG_MACH_LGE)
#if defined(CONFIG_OLED_SUPPORT)
#include "mdss_mdp.h"
#include "mdss_fb.h"
extern struct msm_fb_data_type *mfd_base;
#else
#include "mdss_mdp.h"
#include "mdss_fb.h"
extern struct msm_fb_data_type *mfd_base;
static struct dsi_panel_cmds lge_ief_on_cmds;
static struct dsi_panel_cmds lge_ief_off_cmds;
#endif
#endif

#ifdef CONFIG_LGE_LCD_TUNING
/* LGE_CHANGE_S
 * Add code to apply tuning method for LCD
 * 2012-12-03, minjong.gong@lge.com
*/
extern int num_cmds;
extern int tun_len;
extern char init_buf[TUNING_REGSIZE];
extern struct dsi_cmd_desc *tun_dsi_panel_on_cmds;
extern u32 tun_porch_value[6];
struct mipi_panel_info *mipi;
static struct dsi_cmd_desc *dsi_panel_on_cmds;
static struct dsi_cmd_desc *dsi_panel_off_cmds;
static int num_of_on_cmds;
static int num_of_off_cmds;
static char *on_cmds, *off_cmds;

#endif /* CONFIG_LGE_LCD_TUNING */

DEFINE_LED_TRIGGER(bl_led_trigger);

#if defined(CONFIG_BACKLIGHT_LM3630)
extern void lm3630_lcd_backlight_set_level(int level);
#endif /* CONFIG_BACKLIGHT_LMXXXX */

#if defined(CONFIG_MACH_LGE)
struct mdss_panel_data *pdata_base;
extern int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata, int enable);
#endif

#if defined(CONFIG_OLED_SUPPORT)
extern int mdss_dsi_off(struct mdss_panel_data *pdata);
#endif

static struct mdss_dsi_phy_ctrl phy_params;

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
int g_mvol_for_lcd;
static lcd_vol_maker_tbl_type lcd_maker_table[LCD_MAKER_MAX] = {
	{LCD_RENESAS_LGD, 0, 900},
	{LCD_RENESAS_JDI, 901, 1800},
};

static lcd_maker_id get_lcd_maker_by_voltage(int mvol)
{
	lcd_maker_id lcd_maker = LCD_MAKER_MAX;
	int i = 0;

	for(i = 0; i < LCD_MAKER_MAX; i++){
		if (lcd_maker_table[i].min_mvol <= mvol &&
			mvol <= lcd_maker_table[i].max_mvol) {
			lcd_maker = lcd_maker_table[i].maker_id;
			break;
		}
	}
	g_mvol_for_lcd = mvol;
	return lcd_maker;
}

lcd_maker_id get_panel_maker_id(void)
{
    struct qpnp_vadc_result result;
    lcd_maker_id maker_id = LCD_MAKER_MAX;
    int rc;
    int acc_read_value = 0;

	rc = qpnp_vadc_read(P_MUX5_1_1, &result);

	if (rc < 0) {
		if (rc == -ETIMEDOUT) {
		   acc_read_value = 0;
		}
		pr_err("%s : adc read "
					 "error - %d\n", __func__, rc);
	}
	acc_read_value = (int)result.physical / 1000;
	pr_debug("%s: acc_read_value - %d\n", __func__,
					 (int)result.physical);

    maker_id = get_lcd_maker_by_voltage(acc_read_value);

	return maker_id;
}
EXPORT_SYMBOL(get_panel_maker_id);

static ssize_t panel_maker_id_show(struct device *dev,
				                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", get_panel_maker_id());
}
static DEVICE_ATTR(panel_maker_id, 0444, panel_maker_id_show, NULL);
#endif

#if defined(CONFIG_OLED_SUPPORT) && defined(CONFIG_LGE_OLED_IMG_TUNING)
#include "mdss_mdp.h"
#define	IMG_TUNE_COUNT	6
static int img_tune_mode = 1;	// Default value : cm0(standard) + plc 60%
struct img_tune_cmds_desc {
	struct dsi_panel_cmds img_tune_cmds[IMG_TUNE_COUNT];
};
static char *img_tune_dt[] = {
	"lge,img-tune-cmds-cm0",
	"lge,img-tune-cmds-ce-cm0-plc60",
	"lge,img-tune-cmds-cm1",
	"lge,img-tune-cmds-ce-cm1-plc60",
	"lge,img-tune-cmds-cm2",
	"lge,img-tune-cmds-ce-cm2-plc60",
};
static struct img_tune_cmds_desc *img_tune_cmds_set;

static int bl_tune_mode = 1;	// Default value : use the blmap
#endif

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
	if (ret) {
		pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: lpg_chan=%d pwm request failed", __func__,
				ctrl->pwm_lpg_chan);
		gpio_free(ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
	}
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	ret = pwm_config(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(ctrl->pwm_bl);
	if (ret)
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

static void dcs_read_cb(u32 data)
{
	pr_info("%s: data=0x%x\n", __func__, ntohl(data));
}

u32 mdss_dsi_dcs_read(struct mdss_dsi_ctrl_pdata *ctrl,
			char cmd0, char cmd1)
{
	struct dcs_cmd_req cmdreq;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 1;
	cmdreq.cb = dcs_read_cb; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */

	return 0;
}

static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_pwm1)},
	led_pwm1
};

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;

	pr_debug("%s: level=%d\n", __func__, level);

#ifdef CONFIG_OLED_SUPPORT
       if (bl_tune_mode)
              led_pwm1[1] = (unsigned char)ctrl->panel_data.panel_info.blmap[level];
       else
              led_pwm1[1] = (unsigned char)level;

	pr_info("[Zee][OLED] %s: level=%d, blmap=%d\n", __func__, level, led_pwm1[1]);
#else
	led_pwm1[1] = (unsigned char)level;
#endif

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

void mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

#ifndef CONFIG_MACH_LGE
		/* LGE_CHANGE_S
		 * power sequence for LGD_FHD panel
		 * 2013-04-05, yeonjun.kim@lge.com
		 */
	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}
#endif

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
#ifdef CONFIG_OLED_SUPPORT
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
#else // CONFIG_OLED_SUPPORT
#ifdef CONFIG_MACH_LGE
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(5);
#else
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		udelay(200);
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
#endif
#endif

#ifndef CONFIG_MACH_LGE
		/* maintain qmc's original code */
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
#endif
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
#ifndef CONFIG_MACH_LGE
		/* LGE_CHANGE_S
		 * power sequence for LGD_FHD panel
		 * 2013-04-05, yeonjun.kim@lge.com
		 */
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
#endif
	}
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
#ifdef CONFIG_OLED_SUPPORT
	struct mipi_panel_info *mipi;
	mipi  = &pdata->panel_info.mipi;
#endif
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
#if defined(CONFIG_BACKLIGHT_LM3630)
		lm3630_lcd_backlight_set_level(bl_level);
#else
		led_trigger_event(bl_led_trigger, bl_level);
#endif
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
#ifdef CONFIG_OLED_SUPPORT
		if (mipi->mode == DSI_VIDEO_MODE) {
			if (bl_level > 0xFF) bl_level = 0xFF;
		} else {
			pr_err("%s:%d, CMD MODE NOT SUPPORTED", __func__, __LINE__);
			return;
		}
#endif
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
#if defined(CONFIG_MACH_LGE)
	if(pdata_base == NULL)
		pdata_base = pdata;
#endif
	mipi  = &pdata->panel_info.mipi;

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
	pr_info("%s: ctrl=%p ndx=%d maker=%d\n", __func__, ctrl, ctrl->ndx, get_panel_maker_id());
#else
	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);
#endif

	if (ctrl->on_cmds.cmd_cnt)
#ifdef CONFIG_LGE_LCD_TUNING
		/* LGE_CHANGE
		 * Implement for LCD init sequence tuning
		 * 2013-01-25, baryun.hwang@lge.com
		 */
		mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf,
				tun_dsi_panel_on_cmds,
				num_of_on_cmds);
#else
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

#ifdef CONFIG_OLED_SUPPORT
	mdss_dsi_panel_img_tune_apply(IMG_TUNE_COUNT);
	pr_info("[Zee][OLED] %s success\n", __func__);
#endif

#endif
	pr_debug("%s:-\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mipi  = &pdata->panel_info.mipi;

#if defined(CONFIG_MACH_LGE)
	if (!gpio_get_value(ctrl->disp_en_gpio))  // we should remove the semi-colon
		return 0;
#endif

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	pr_debug("%s:-\n", __func__);
#ifdef CONFIG_OLED_SUPPORT
       pr_info("[Zee][OLED] %s success\n", __func__);
#endif
	return 0;
}

#if defined(CONFIG_MACH_LGE) && !(defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI))
int mdss_dsi_panel_ief_off(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata_base == NULL || mfd_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (mfd_base->panel_power_on == 0) {
		pr_err("%s: Panel is off\n", __func__);
		return -EPERM;
	}

	ctrl = container_of(pdata_base, struct mdss_dsi_ctrl_pdata,	panel_data);

	if(lge_ief_off_cmds.cmd_cnt){
		pr_info("sending IEF_OFF code\n");
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		mdss_dsi_panel_cmds_send(ctrl, &lge_ief_off_cmds);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	}

	return 0;
}
EXPORT_SYMBOL(mdss_dsi_panel_ief_off);

int mdss_dsi_panel_ief_on(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata_base == NULL || mfd_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (mfd_base->panel_power_on == 0) {
		pr_err("%s: Panel is off\n", __func__);
		return -EPERM;
	}

	ctrl = container_of(pdata_base, struct mdss_dsi_ctrl_pdata,	panel_data);

	if(lge_ief_on_cmds.cmd_cnt){
		pr_info("sending IEF_ON code\n");
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		mdss_dsi_panel_cmds_send(ctrl, &lge_ief_on_cmds);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	}

	return 0;
}
EXPORT_SYMBOL(mdss_dsi_panel_ief_on);

static ssize_t ief_on_off(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int on_off;

	if (!count)
		return -EINVAL;

	on_off = simple_strtoul(buf, NULL, 10);

	pr_debug("[LCD][DEBUG] %s, %d", __func__, on_off);

	if (on_off == 0)
		mdss_dsi_panel_ief_off();
	else
		mdss_dsi_panel_ief_on();

	return count;
}
DEVICE_ATTR(ief_on_off, 0644, NULL, ief_on_off);
#endif

#if defined(CONFIG_OLED_SUPPORT) && defined(CONFIG_LGE_OLED_IMG_TUNING)
int mdss_dsi_panel_img_tune_apply(unsigned int screen_mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata_base == NULL) {
		pr_err("%s: Invalid input data.\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata_base, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (pdata_base->panel_info.panel_power_on == 0) {
		pr_err("%s: panel off state. This screen mode will not be applied.\n", __func__);
		return -EINVAL;
	}
	/*
		if (ctrl->ctrl_state != (CTRL_STATE_PANEL_INIT | CTRL_STATE_MDP_ACTIVE)) {
			pr_err("%s: panel is not 'on' state(cur=%d). The screen mode will be applied next time.\n", __func__, ctrl->ctrl_state);
			return -EINVAL;
		}
	*/
	if ((screen_mode < 0) || (screen_mode > IMG_TUNE_COUNT)) {
		pr_err("%s: invalid input data(%d). Retry(0-5).\n", __func__, screen_mode);
		return -EINVAL;
	}
	
	if (screen_mode == IMG_TUNE_COUNT) {
		pr_info("%s: send the screen mode on(%d) from kernel.\n", __func__, img_tune_mode);
	} else {
		img_tune_mode = screen_mode;
		pr_info("%s: send the screen mode on(%d) from user.\n", __func__, img_tune_mode);
	}

	if(img_tune_cmds_set->img_tune_cmds[img_tune_mode].cmd_cnt) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
//		0 : CM0 / 1 : CM0 + PLC 60% / 2 : CM1 / 3 : CM1 + PLC 60% / 4 : CM2 / 5 : CM2 + PLC 60%
		mdss_dsi_panel_cmds_send(ctrl, &img_tune_cmds_set->img_tune_cmds[img_tune_mode]);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	}
	return 0;
}
EXPORT_SYMBOL(mdss_dsi_panel_img_tune_apply);

static ssize_t img_tune_mode_get(struct device *dev, 
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", img_tune_mode);
}

static ssize_t img_tune_mode_set(struct device *dev, 
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int param;
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	sscanf(buf, "%x", &param);
	mdss_dsi_panel_img_tune_apply(param);

	return ret;
}

static ssize_t bl_tune_mode_get(struct device *dev, 
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bl_tune_mode);
}

static ssize_t bl_tune_mode_set(struct device *dev, 
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int param;
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	sscanf(buf, "%x", &param);
	bl_tune_mode = param;

	return ret;
}

static ssize_t mipi_dsi_read_set(struct device *dev, 
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_addr;
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	if (pdata_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata_base, struct mdss_dsi_ctrl_pdata,
				panel_data);
	sscanf(buf, "%x", &reg_addr);
	mdss_dsi_dcs_read(ctrl, (char)reg_addr, 0x00);

	return ret;
}

static struct device_attribute panel_tuning_device_attrs[] = {
	__ATTR(img_tune_mode, 0644, img_tune_mode_get, img_tune_mode_set),
	__ATTR(bl_tune_mode, 0644, bl_tune_mode_get, bl_tune_mode_set),
	__ATTR(mipi_dsi_read, 0644 , NULL, mipi_dsi_read_set),
};

#endif

static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			return -ENOMEM;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		return -ENOMEM;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	pcmds->link_state = DSI_LP_MODE; /* default */

	data = of_get_property(np, link_key, NULL);
	if (!strncmp(data, "DSI_HS_MODE", 11))
		pcmds->link_state = DSI_HS_MODE;

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;
}

#if defined(CONFIG_MACH_LGE)
/* G2-Task-LCD@lge.com
 * In the normal case, when the device turnned off, lcd off function doesn't work.
 * But, It's need to turn off the panel device in order to prevent flicking issue.
 * So, Add lcd turn off function when the device shut down.
 */
static void mdss_dsi_panel_shutdown(void)
{

	if (mfd_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

#ifdef CONFIG_OLED_SUPPORT
	mfd_base->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd_base->fbi);
	pr_info("[Zee][OLED] %s success\n", __func__);
#else
	pr_info("%s - start panel shutdown in order to prevent s/w flickering\n", __func__);
	lm3630_lcd_backlight_set_level(0);
	mfd_base->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd_base->fbi);
	pr_info("%s complete\n", __func__);
#endif
}
#endif
static int mdss_panel_parse_dt(struct platform_device *pdev,
			       struct mdss_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	u32 fbc_res[7];
	int rc, i, len;
	const char *data;
	static const char *bl_ctrl_type, *pdest;
	bool fbc_enabled = false;
#ifdef CONFIG_OLED_SUPPORT
	u32 *array;
#endif

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-active-res", res, 2);
	if (rc == 0) {
		panel_data->panel_info.lcdc.xres_pad =
			panel_data->panel_info.xres - res[0];
		panel_data->panel_info.lcdc.yres_pad =
			panel_data->panel_info.yres - res[1];
	}

#ifdef CONFIG_OLED_SUPPORT
	rc = of_property_read_u32_array(np, "qcom,mdss-pan-margin-res", res, 2);
	if (rc == 0) {
		panel_data->panel_info.lcdc.xres_margin=
			res[0] - panel_data->panel_info.xres;
		panel_data->panel_info.lcdc.yres_margin=
			res[1] - panel_data->panel_info.yres;
	}
#endif

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);

	pdest = of_get_property(pdev->dev.of_node,
				"qcom,mdss-pan-dest", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		panel_data->panel_info.pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		panel_data->panel_info.pdest = DISPLAY_2;
	else {
		pr_debug("%s: pdest not specified. Set Default\n",
							__func__);
		panel_data->panel_info.pdest = DISPLAY_1;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-porch-values", res, 6);

#ifdef CONFIG_LGE_LCD_TUNING
	memcpy(tun_porch_value, res, sizeof(tun_porch_value));
#endif
	panel_data->panel_info.lcdc.h_back_porch = (!rc ? res[0] : 6);
	panel_data->panel_info.lcdc.h_pulse_width = (!rc ? res[1] : 2);
	panel_data->panel_info.lcdc.h_front_porch = (!rc ? res[2] : 6);
	panel_data->panel_info.lcdc.v_back_porch = (!rc ? res[3] : 6);
	panel_data->panel_info.lcdc.v_pulse_width = (!rc ? res[4] : 2);
	panel_data->panel_info.lcdc.v_front_porch = (!rc ? res[5] : 6);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-underflow-clr", &tmp);
	panel_data->panel_info.lcdc.underflow_clr = (!rc ? tmp : 0xff);

	bl_ctrl_type = of_get_property(pdev->dev.of_node,
				  "qcom,mdss-pan-bl-ctrl", NULL);
	if ((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12))) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);

		panel_data->panel_info.bklt_ctrl = BL_WLED;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_pwm", 11)) {
		panel_data->panel_info.bklt_ctrl = BL_PWM;

		rc = of_property_read_u32(np, "qcom,pwm-period", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
			return -EINVAL;
		}
		panel_data->panel_info.pwm_period = tmp;

		rc = of_property_read_u32(np, "qcom,pwm-lpg-channel", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, dsi lpg channel\n",
						__func__, __LINE__);
			return -EINVAL;
		}
		panel_data->panel_info.pwm_lpg_chan = tmp;

		tmp = of_get_named_gpio(np, "qcom,pwm-pmic-gpio", 0);
		panel_data->panel_info.pwm_pmic_gpio =  tmp;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_dcs", 11)) {
		panel_data->panel_info.bklt_ctrl = BL_DCS_CMD;
#ifdef CONFIG_OLED_SUPPORT
	}else if((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_oled", 12))) {
		panel_data->panel_info.bklt_ctrl = BL_DCS_CMD;
#endif
	} else {
		pr_debug("%s: Unknown backlight control\n", __func__);
		panel_data->panel_info.bklt_ctrl = UNKNOWN_CTRL;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);

#ifdef CONFIG_OLED_SUPPORT
	rc = of_property_read_u32(np, "qcom,blmap_size",
			&panel_data->panel_info.blmap_size);

	if (panel_data->panel_info.blmap_size) {
		array = kzalloc(sizeof(u32) * panel_data->panel_info.blmap_size, GFP_KERNEL);
		if (!array)
			return -ENOMEM;

		rc = of_property_read_u32_array(np, "qcom,blmap", array, panel_data->panel_info.blmap_size);
		if (rc) {
			pr_err("%s:%d, uable to read backlight map\n",__func__, __LINE__);
			return -EINVAL;
		}
		panel_data->panel_info.blmap = kzalloc(sizeof(char) * panel_data->panel_info.blmap_size, GFP_KERNEL);

		if (!panel_data->panel_info.blmap)
			return -ENOMEM;

		for (i = 0; i < panel_data->panel_info.blmap_size; i++ )
			panel_data->panel_info.blmap[i] = (char)array[i];

		if (array)
			kfree(array);

	} else {
		panel_data->panel_info.blmap = NULL;
	}
#endif

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	panel_data->panel_info.mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

	rc = of_property_read_u32(np, "qcom,mdss-vsync-enable", &tmp);
	panel_data->panel_info.mipi.vsync_enable = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-hw-vsync-mode", &tmp);
	panel_data->panel_info.mipi.hw_vsync_mode = (!rc ? tmp : 0);


	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-h-pulse-mode", &tmp);
	panel_data->panel_info.mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-h-power-stop", res, 3);
	panel_data->panel_info.mipi.hbp_power_stop = (!rc ? res[0] : false);
	panel_data->panel_info.mipi.hsa_power_stop = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.hfp_power_stop = (!rc ? res[2] : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-bllp-power-stop", res, 2);
	panel_data->panel_info.mipi.bllp_power_stop =
					(!rc ? res[0] : false);
	panel_data->panel_info.mipi.eof_bllp_power_stop =
					(!rc ? res[1] : false);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-traffic-mode", &tmp);
	panel_data->panel_info.mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-insert-dcs-cmd", &tmp);
	panel_data->panel_info.mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-continue", &tmp);
	panel_data->panel_info.mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-start", &tmp);
	panel_data->panel_info.mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-te-sel", &tmp);
	panel_data->panel_info.mipi.te_sel =
			(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-dst-format", &tmp);
	panel_data->panel_info.mipi.dst_format =
			(!rc ? tmp : DSI_VIDEO_DST_FORMAT_RGB888);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-vc", &tmp);
	panel_data->panel_info.mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-rgb-swap", &tmp);
	panel_data->panel_info.mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-data-lanes", res, 4);
	panel_data->panel_info.mipi.data_lane0 = (!rc ? res[0] : true);
	panel_data->panel_info.mipi.data_lane1 = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.data_lane2 = (!rc ? res[2] : false);
	panel_data->panel_info.mipi.data_lane3 = (!rc ? res[3] : false);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dlane-swap", &tmp);
	panel_data->panel_info.mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-dsi-t-clk", res, 2);
	panel_data->panel_info.mipi.t_clk_pre = (!rc ? res[0] : 0x24);
	panel_data->panel_info.mipi.t_clk_post = (!rc ? res[1] : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-stream", &tmp);
	panel_data->panel_info.mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mdp-tr", &tmp);
	panel_data->panel_info.mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dma-tr", &tmp);
	panel_data->panel_info.mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-frame-rate", &tmp);
	panel_data->panel_info.mipi.frame_rate = (!rc ? tmp : 60);

	rc = of_property_read_u32(np, "qcom,mdss-pan-clk-rate", &tmp);
	panel_data->panel_info.clk_rate = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,panel-phy-regulatorSettings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.regulator[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-timingSettings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.timing[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-strengthCtrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
		       __func__, __LINE__);
		goto error;
	}
	phy_params.strength[0] = data[0];
	phy_params.strength[1] = data[1];

	data = of_get_property(np, "qcom,panel-phy-bistCtrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.bistCtrl[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-laneConfig", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.laneCfg[i] = data[i];

	panel_data->panel_info.mipi.dsi_phy_db = &phy_params;

	fbc_enabled = of_property_read_bool(np,
			"qcom,fbc-enabled");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_data->panel_info.fbc.enabled = 1;

		rc = of_property_read_u32_array(np,
				"qcom,fbc-mode", fbc_res, 7);
		panel_data->panel_info.fbc.target_bpp =
			(!rc ?	fbc_res[0] : panel_data->panel_info.bpp);
		panel_data->panel_info.fbc.comp_mode = (!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.qerr_enable =
			(!rc ? fbc_res[2] : 0);
		panel_data->panel_info.fbc.cd_bias = (!rc ? fbc_res[3] : 0);
		panel_data->panel_info.fbc.pat_enable = (!rc ? fbc_res[4] : 0);
		panel_data->panel_info.fbc.vlc_enable = (!rc ? fbc_res[5] : 0);
		panel_data->panel_info.fbc.bflc_enable =
			(!rc ? fbc_res[6] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-budget-ctl", fbc_res, 3);
		panel_data->panel_info.fbc.line_x_budget =
			(!rc ? fbc_res[0] : 0);
		panel_data->panel_info.fbc.block_x_budget =
			(!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.block_budget =
			(!rc ? fbc_res[2] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-lossy-mode", fbc_res, 4);
		panel_data->panel_info.fbc.lossless_mode_thd =
			(!rc ? fbc_res[0] : 0);
		panel_data->panel_info.fbc.lossy_mode_thd =
			(!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.lossy_rgb_thd =
			(!rc ? fbc_res[2] : 0);
		panel_data->panel_info.fbc.lossy_mode_idx =
			(!rc ? fbc_res[3] : 0);

	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_data->panel_info.fbc.enabled = 0;
		panel_data->panel_info.fbc.target_bpp =
			panel_data->panel_info.bpp;
	}

	mdss_dsi_parse_dcs_cmds(np, &panel_data->on_cmds,
		"qcom,panel-on-cmds", "qcom,on-cmds-dsi-state");

	mdss_dsi_parse_dcs_cmds(np, &panel_data->off_cmds,
		"qcom,panel-off-cmds", "qcom,off-cmds-dsi-state");

#if defined(CONFIG_MACH_LGE) && !(defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI))
	mdss_dsi_parse_dcs_cmds(np, &lge_ief_on_cmds, "qcom,panel-ief-on-cmds", "qcom,ief-on-dsi-state");

	mdss_dsi_parse_dcs_cmds(np, &lge_ief_off_cmds, "qcom,panel-ief-off-cmds", "qcom,ief-off-dsi-state");
#endif


#if defined(CONFIG_OLED_SUPPORT) && defined(CONFIG_LGE_OLED_IMG_TUNING)
	img_tune_cmds_set = kzalloc(sizeof(struct img_tune_cmds_desc), GFP_KERNEL);
	for(i = 0; i < IMG_TUNE_COUNT; i++)
		mdss_dsi_parse_dcs_cmds(np, &img_tune_cmds_set->img_tune_cmds[i],
			img_tune_dt[i], "qcom,on-cmds-dsi-state");
#endif
	return 0;

error:
	return -EINVAL;
}

#if defined(CONFIG_MACH_LGE)
struct syscore_ops panel_syscore_ops = {
	.shutdown = mdss_dsi_panel_shutdown,
};
#endif

static int __devinit mdss_dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct mdss_panel_common_pdata vendor_pdata;
	static const char *panel_name;

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
	struct class *panel;
	struct device *panel_sysfs_dev;
#endif
	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	if (!pdev->dev.of_node)
		return -ENODEV;

#if defined(CONFIG_MACH_LGE)
	register_syscore_ops(&panel_syscore_ops);
#endif
	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata);
	if (rc)
		return rc;

	vendor_pdata.on = mdss_dsi_panel_on;
	vendor_pdata.off = mdss_dsi_panel_off;
	vendor_pdata.bl_fnc = mdss_dsi_panel_bl_ctrl;

	rc = dsi_panel_device_register(pdev, &vendor_pdata);
	if (rc)
		return rc;

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
	panel = class_create(THIS_MODULE, "panel");
	if (IS_ERR(panel))
	    pr_err("%s : Failed to create class(panel)!", __func__);

	panel_sysfs_dev = device_create(panel, NULL, 0, NULL, "panel_info");
	if (IS_ERR(panel_sysfs_dev))
	{
	    pr_err("%s : Failed to create dev(panel_sysfs_dev)!", __func__);
	}
	else
	{
		if (device_create_file(panel_sysfs_dev, &dev_attr_panel_maker_id) < 0)
		    pr_err("%s : Failed to create device file(%s)!",
					   __func__, dev_attr_panel_maker_id.attr.name);
	}
#endif

#if defined(CONFIG_OLED_SUPPORT) && defined(CONFIG_LGE_OLED_IMG_TUNING)
    rc = device_create_file(&pdev->dev,&panel_tuning_device_attrs[0]);
	if(rc) pr_err("%s: device file(img_tune_mode) create fail!\n",__func__);
    rc = device_create_file(&pdev->dev,&panel_tuning_device_attrs[1]);
	if(rc) pr_err("%s: device file(bl_tune_mode) create fail!\n",__func__);
    rc = device_create_file(&pdev->dev,&panel_tuning_device_attrs[2]);
	if(rc) pr_err("%s: device file(mipi_dsi_read) create fail!\n",__func__);
#endif


#if defined(CONFIG_MACH_LGE) && !(defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI))
	rc = device_create_file(&pdev->dev, &dev_attr_ief_on_off);
#endif

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
	pr_info("panel maker ID is %d\n", lge_get_panel_maker());
#endif

	return 0;
}

static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "qcom,mdss-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
		.name   = "dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

static int __init mdss_dsi_panel_init(void)
{
	return platform_driver_register(&this_driver);
}
module_init(mdss_dsi_panel_init);
