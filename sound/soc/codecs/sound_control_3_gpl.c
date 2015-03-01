/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * WCD93xx sound control module
 * Copyright 2013 Paul Reioux
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

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kallsyms.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9320_registers.h>

#define SOUND_CONTROL_MAJOR_VERSION	3
#define SOUND_CONTROL_MINOR_VERSION	6

#ifdef CONFIG_MACH_LGE
static int lge_snd_ctrl_locked = 1;
int lge_snd_pa_ctrl_locked = 0;
#endif

extern struct snd_soc_codec *fauxsound_codec_ptr;
#ifdef CONFIG_MACH_LGE
static int wcd9xxx_hw_revision = 1;
#else
extern int wcd9xxx_hw_revision;
#endif

static int snd_ctrl_locked = 0;
static int snd_rec_ctrl_locked = 0;

unsigned int taiko_read(struct snd_soc_codec *codec, unsigned int reg);
int taiko_write(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int value);

#define REG_SZ  25

#ifdef CONFIG_MACH_LGE
static int cached_regs[] = {6, 6, -1, -1, 0, 0, -1, -1, -1, -1,
			1, -1, -1, -1, -1, -1, 10, 10, -1, -1,
			-1, -1, -1, -1, -1};
#else
static int cached_regs[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1 -1, -1, -1, -1};
#endif

void snd_hax_cache_write(unsigned int reg, unsigned int value)
{
        switch (reg) {
                case TAIKO_A_RX_HPH_L_GAIN:
			cached_regs[0] = value;
			break;
                case TAIKO_A_RX_HPH_R_GAIN:
			cached_regs[1] = value;
			break;
		case TAIKO_A_RX_HPH_L_STATUS:
			cached_regs[2] = value;
			break;
		case TAIKO_A_RX_HPH_R_STATUS:
			cached_regs[3] = value;
			break;
                case TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL:
			cached_regs[4] = value;
			break;
                case TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL:
			cached_regs[5] = value;
			break;
                case TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL:
			cached_regs[6] = value;
			break;
                case TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL:
			cached_regs[7] = value;
			break;
                case TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL:
			cached_regs[8] = value;
			break;
                case TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL:
			cached_regs[9] = value;
			break;
                case TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL:
			cached_regs[10] = value;
			break;
                case TAIKO_A_CDC_TX1_VOL_CTL_GAIN:
			cached_regs[11] = value;
			break;
                case TAIKO_A_CDC_TX2_VOL_CTL_GAIN:
			cached_regs[12] = value;
			break;
                case TAIKO_A_CDC_TX3_VOL_CTL_GAIN:
			cached_regs[13] = value;
			break;
                case TAIKO_A_CDC_TX4_VOL_CTL_GAIN:
			cached_regs[14] = value;
			break;
                case TAIKO_A_CDC_TX5_VOL_CTL_GAIN:
			cached_regs[15] = value;
			break;
                case TAIKO_A_CDC_TX6_VOL_CTL_GAIN:
			cached_regs[16] = value;
			break;
                case TAIKO_A_CDC_TX7_VOL_CTL_GAIN:
			cached_regs[17] = value;
			break;
                case TAIKO_A_CDC_TX8_VOL_CTL_GAIN:
			cached_regs[18] = value;
			break;
                case TAIKO_A_CDC_TX9_VOL_CTL_GAIN:
			cached_regs[19] = value;
			break;
                case TAIKO_A_CDC_TX10_VOL_CTL_GAIN:
			cached_regs[20] = value;
			break;
		case TAIKO_A_RX_LINE_1_GAIN:
			cached_regs[21] = value;
			break;
		case TAIKO_A_RX_LINE_2_GAIN:
			cached_regs[22] = value;
			break;
		case TAIKO_A_RX_LINE_3_GAIN:
			cached_regs[23] = value;
			break;
		case TAIKO_A_RX_LINE_4_GAIN:
			cached_regs[24] = value;
			break;
		default:
			break;
        }
	return;
}
EXPORT_SYMBOL(snd_hax_cache_write);

int snd_hax_cache_read(unsigned int reg)
{
	int out = -1;

	switch (reg) {
		case TAIKO_A_RX_HPH_L_GAIN:
			out = cached_regs[0];
			break;
		case TAIKO_A_RX_HPH_R_GAIN:
			out = cached_regs[1];
			break;
		case TAIKO_A_RX_HPH_L_STATUS:
			out = cached_regs[2];
			break;
		case TAIKO_A_RX_HPH_R_STATUS:
			out = cached_regs[3];
			break;
		case TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL:
			out = cached_regs[4];
			break;
		case TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL:
			out = cached_regs[5];
			break;
		case TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL:
			out = cached_regs[6];
			break;
		case TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL:
			out = cached_regs[7];
			break;
		case TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL:
			out = cached_regs[8];
			break;
		case TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL:
			out = cached_regs[9];
			break;
		case TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL:
			out = cached_regs[10];
			break;
		case TAIKO_A_CDC_TX1_VOL_CTL_GAIN:
			out = cached_regs[11];
			break;
		case TAIKO_A_CDC_TX2_VOL_CTL_GAIN:
			out = cached_regs[12];
			break;
		case TAIKO_A_CDC_TX3_VOL_CTL_GAIN:
			out = cached_regs[13];
			break;
		case TAIKO_A_CDC_TX4_VOL_CTL_GAIN:
			out = cached_regs[14];
			break;
		case TAIKO_A_CDC_TX5_VOL_CTL_GAIN:
			out = cached_regs[15];
			break;
		case TAIKO_A_CDC_TX6_VOL_CTL_GAIN:
			out = cached_regs[16];
			break;
		case TAIKO_A_CDC_TX7_VOL_CTL_GAIN:
			out = cached_regs[17];
			break;
		case TAIKO_A_CDC_TX8_VOL_CTL_GAIN:
			out = cached_regs[18];
			break;
		case TAIKO_A_CDC_TX9_VOL_CTL_GAIN:
			out = cached_regs[19];
			break;
		case TAIKO_A_CDC_TX10_VOL_CTL_GAIN:
			out = cached_regs[20];
			break;
		case TAIKO_A_RX_LINE_1_GAIN:
			out = cached_regs[21];
			break;
		case TAIKO_A_RX_LINE_2_GAIN:
			out = cached_regs[22];
			break;
		case TAIKO_A_RX_LINE_3_GAIN:
			out = cached_regs[23];
			break;
		case TAIKO_A_RX_LINE_4_GAIN:
			out = cached_regs[24];
			break;
		default:
			break;
        }
	return out;
}
EXPORT_SYMBOL(snd_hax_cache_read);

int snd_hax_reg_access(unsigned int reg)
{
	int ret = 1;

	switch (reg) {
		/* Analog Power Amp (PA) */
		case TAIKO_A_RX_HPH_L_GAIN:
		case TAIKO_A_RX_HPH_R_GAIN:
		case TAIKO_A_RX_HPH_L_STATUS:
		case TAIKO_A_RX_HPH_R_STATUS:
#ifdef CONFIG_MACH_LGE
			if (lge_snd_ctrl_locked > 0)
				ret = 0;
#else
			if (snd_ctrl_locked > 0)
				ret = 0;
#endif
			break;
		/* Digital Headphones Gain */
		case TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL:
		case TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL:
#ifndef CONFIG_MACH_LGE
		case TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL:
		case TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL:
		case TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL:
		case TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL:
#endif
		/* Loud Speaker Gain */
		case TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL:
		/* Line out gain */
		case TAIKO_A_RX_LINE_1_GAIN:
		case TAIKO_A_RX_LINE_2_GAIN:
		case TAIKO_A_RX_LINE_3_GAIN:
		case TAIKO_A_RX_LINE_4_GAIN:
#ifdef CONFIG_MACH_LGE
			if (lge_snd_ctrl_locked > 0)
				ret = 0;
#else
			if (snd_ctrl_locked > 0)
				ret = 0;
#endif
			break;
#ifndef CONFIG_MACH_LGE
		case TAIKO_A_CDC_TX1_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX2_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX3_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX4_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX5_VOL_CTL_GAIN:
#endif
		/* Incall MIC Gain */
		case TAIKO_A_CDC_TX6_VOL_CTL_GAIN:
		/* Camera MIC Gain */
		case TAIKO_A_CDC_TX7_VOL_CTL_GAIN:
#ifndef CONFIG_MACH_LGE
		case TAIKO_A_CDC_TX8_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX9_VOL_CTL_GAIN:
		case TAIKO_A_CDC_TX10_VOL_CTL_GAIN:
#endif
#ifdef CONFIG_MACH_LGE
			if (lge_snd_ctrl_locked > 0)
				ret = 0;
#else
			if (snd_rec_ctrl_locked > 0)
				ret = 0;
#endif
			break;
		default:
			break;
	}
	return ret;
}
EXPORT_SYMBOL(snd_hax_reg_access);

static bool calc_checksum(unsigned int a, unsigned int b, unsigned int c)
{
	unsigned char chksum = 0;

	chksum = ~((a & 0xff) + (b & 0xff));

	if (chksum == (c & 0xff)) {
		return true;
	} else {
		return false;
	}
}

static ssize_t cam_mic_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n",
		taiko_read(fauxsound_codec_ptr,
			TAIKO_A_CDC_TX7_VOL_CTL_GAIN));

}

static ssize_t cam_mic_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, chksum;

	sscanf(buf, "%u %u", &lval, &chksum);

#ifndef CONFIG_MACH_LGE
	if (calc_checksum(lval, 0, chksum)) {
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_TX7_VOL_CTL_GAIN, lval);
	}
#endif
	return count;
}

#ifdef CONFIG_MACH_LGE
static ssize_t lge_cam_mic_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval;

	sscanf(buf, "%u", &lval);

	if (lval >= 255)
		lval = 255;
	if (lval < 0)
		lval = 0;

	lge_snd_ctrl_locked = 0;
	taiko_write(fauxsound_codec_ptr,
		TAIKO_A_CDC_TX7_VOL_CTL_GAIN, lval);
	lge_snd_ctrl_locked = 1;

	return count;
}
#endif

static ssize_t mic_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n",
		taiko_read(fauxsound_codec_ptr,
			TAIKO_A_CDC_TX6_VOL_CTL_GAIN));
}

static ssize_t mic_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, chksum;

	sscanf(buf, "%u %u", &lval, &chksum);

#ifndef CONFIG_MACH_LGE
	if (calc_checksum(lval, 0, chksum)) {
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_TX6_VOL_CTL_GAIN, lval);
	}
#endif
	return count;

}

#ifdef CONFIG_MACH_LGE
static ssize_t lge_mic_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval;

	sscanf(buf, "%u", &lval);

	if (lval >= 255)
		lval = 255;
	if (lval < 0)
		lval = 0;

	lge_snd_ctrl_locked = 0;
	taiko_write(fauxsound_codec_ptr,
		TAIKO_A_CDC_TX6_VOL_CTL_GAIN, lval);
	lge_snd_ctrl_locked = 1;

	return count;
}
#endif

static ssize_t speaker_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%u %u\n",
			taiko_read(fauxsound_codec_ptr,
				TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL),
			taiko_read(fauxsound_codec_ptr,
				TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL));

}

static ssize_t speaker_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, rval, chksum;

	sscanf(buf, "%u %u %u", &lval, &rval, &chksum);

#ifndef CONFIG_MACH_LGE
	if (calc_checksum(lval, rval, chksum)) {
		/* we have mono speaker! lval = rval */
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL, lval);
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL, rval);
	}
#endif
	return count;
}

#ifdef CONFIG_MACH_LGE
static ssize_t lge_speaker_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, rval;

	sscanf(buf, "%u %u", &lval, &rval);

	if (lval >= 255)
		lval = 255;
	if (rval >= 255)
		rval = 255;
	if (lval < 0)
		lval = 0;
	if (rval < 0)
		rval = 0;

	/* we have mono speaker! lval = rval */
	lge_snd_ctrl_locked = 0;
	taiko_write(fauxsound_codec_ptr,
		TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL, lval);
	taiko_write(fauxsound_codec_ptr,
		TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL, rval);
	lge_snd_ctrl_locked = 1;

	return count;
}
#endif

static ssize_t headphone_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u %u\n",
			taiko_read(fauxsound_codec_ptr,
				TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL),
			taiko_read(fauxsound_codec_ptr,
				TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL));
}

static ssize_t headphone_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, rval, chksum;

	sscanf(buf, "%u %u %u", &lval, &rval, &chksum);

	if (calc_checksum(lval, rval, chksum)) {
#ifdef CONFIG_MACH_LGE
		lge_snd_ctrl_locked = 0;
#endif
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL, lval);
		taiko_write(fauxsound_codec_ptr,
			TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL, rval);
#ifdef CONFIG_MACH_LGE
		lge_snd_ctrl_locked = 1;
#endif
	}
	return count;
}

static ssize_t headphone_pa_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u %u\n",
		taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN),
		taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN));
}

static ssize_t headphone_pa_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, rval, chksum;
#ifndef CONFIG_MACH_LGE
	unsigned int gain, status;
	unsigned int out;
#endif

	sscanf(buf, "%u %u %u", &lval, &rval, &chksum);

#ifdef CONFIG_MACH_LGE
		return count;
#else
	if (calc_checksum(lval, rval, chksum)) {
		gain = taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN);
		out = (gain & 0xf0) | lval;
		taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN, out);

		status = taiko_read(fauxsound_codec_ptr,
				TAIKO_A_RX_HPH_L_STATUS);
		out = (status & 0x0f) | (lval << 4);
		taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_STATUS, out);

		gain = taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN);
		out = (gain & 0xf0) | rval;
		taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN, out);

		status = taiko_read(fauxsound_codec_ptr,
				TAIKO_A_RX_HPH_R_STATUS);
		out = (status & 0x0f) | (rval << 4);
		taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_STATUS, out);
	}
	return count;
#endif
}

#ifdef CONFIG_MACH_LGE
static ssize_t lge_headphone_pa_gain_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u %u\n",
		taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN),
		taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN));
}

static ssize_t lge_headphone_pa_gain_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int lval, rval;
	unsigned int gain, status;
	unsigned int out;

	sscanf(buf, "%u %u", &lval, &rval);

	lge_snd_ctrl_locked = 0;
	gain = taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN);
	out = (gain & 0xf0) | lval;
	taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_GAIN, out);

	status = taiko_read(fauxsound_codec_ptr,
			TAIKO_A_RX_HPH_L_STATUS);
	out = (status & 0x0f) | (lval << 4);
	taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_L_STATUS, out);

	gain = taiko_read(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN);
	out = (gain & 0xf0) | rval;
	taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_GAIN, out);

	status = taiko_read(fauxsound_codec_ptr,
			TAIKO_A_RX_HPH_R_STATUS);
	out = (status & 0x0f) | (rval << 4);
	taiko_write(fauxsound_codec_ptr, TAIKO_A_RX_HPH_R_STATUS, out);
	lge_snd_ctrl_locked = 1;

	return count;
}
#endif

static unsigned int selected_reg = 0xdeadbeef;

static ssize_t sound_reg_read_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (selected_reg == 0xdeadbeef)
		return -1;
	else
		return sprintf(buf, "%u\n",
			taiko_read(fauxsound_codec_ptr, selected_reg));
}

static ssize_t sound_reg_select_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
        sscanf(buf, "%u", &selected_reg);

	return count;
}

static ssize_t sound_reg_write_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
        unsigned int out, chksum;

	sscanf(buf, "%u %u", &out, &chksum);
	if (calc_checksum(out, 0, chksum)) {
		if (selected_reg != 0xdeadbeef)
			taiko_write(fauxsound_codec_ptr, selected_reg, out);
	}
	return count;
}

static ssize_t sound_control_hw_revision_show (struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "hw_revision: %i\n", wcd9xxx_hw_revision);
}

static ssize_t sound_control_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u\n",
			SOUND_CONTROL_MAJOR_VERSION,
			SOUND_CONTROL_MINOR_VERSION);
}

#ifdef CONFIG_MACH_LGE
static ssize_t lge_sound_pa_control_locked_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", lge_snd_pa_ctrl_locked);
}

static ssize_t lge_sound_pa_control_locked_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int inp;

	sscanf(buf, "%d", &inp);

	lge_snd_pa_ctrl_locked = inp;

	return count;
}
#endif

static ssize_t sound_control_locked_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", snd_ctrl_locked);
}

static ssize_t sound_control_locked_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int inp;

	sscanf(buf, "%d", &inp);

	snd_ctrl_locked = inp;

	return count;
}

static ssize_t sound_control_rec_locked_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", snd_rec_ctrl_locked);
}

static ssize_t sound_control_rec_locked_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int inp;

	sscanf(buf, "%d", &inp);

	snd_rec_ctrl_locked = inp;

	return count;
}


static struct kobj_attribute sound_reg_sel_attribute =
	__ATTR(sound_reg_sel,
		0222,
		NULL,
		sound_reg_select_store);

static struct kobj_attribute sound_reg_read_attribute =
	__ATTR(sound_reg_read,
		0444,
		sound_reg_read_show,
		NULL);

static struct kobj_attribute sound_reg_write_attribute =
	__ATTR(sound_reg_write,
		0666,
		NULL,
		sound_reg_write_store);

static struct kobj_attribute cam_mic_gain_attribute =
	__ATTR(gpl_cam_mic_gain,
		0666,
		cam_mic_gain_show,
		cam_mic_gain_store);

static struct kobj_attribute mic_gain_attribute =
	__ATTR(gpl_mic_gain,
		0666,
		mic_gain_show,
		mic_gain_store);

static struct kobj_attribute speaker_gain_attribute =
	__ATTR(gpl_speaker_gain,
		0666,
		speaker_gain_show,
		speaker_gain_store);

#ifdef CONFIG_MACH_LGE
static struct kobj_attribute lge_cam_mic_gain_attribute =
	__ATTR(lge_cam_mic_gain,
		0666,
		cam_mic_gain_show,
		lge_cam_mic_gain_store);

static struct kobj_attribute lge_mic_gain_attribute =
	__ATTR(lge_mic_gain,
		0666,
		mic_gain_show,
		lge_mic_gain_store);

static struct kobj_attribute lge_speaker_gain_attribute =
	__ATTR(lge_speaker_gain,
		0666,
		speaker_gain_show,
		lge_speaker_gain_store);

static struct kobj_attribute lge_headphone_pa_gain_attribute =
	__ATTR(lge_headphone_pa_gain,
		0666,
		lge_headphone_pa_gain_show,
		lge_headphone_pa_gain_store);

static struct kobj_attribute lge_sound_pa_control_locked_attribute =
	__ATTR(lge_sound_pa_control_locked,
		0666,
		lge_sound_pa_control_locked_show,
		lge_sound_pa_control_locked_store);
#endif

static struct kobj_attribute headphone_gain_attribute =
	__ATTR(gpl_headphone_gain,
		0666,
		headphone_gain_show,
		headphone_gain_store);

static struct kobj_attribute headphone_pa_gain_attribute =
	__ATTR(gpl_headphone_pa_gain,
		0666,
		headphone_pa_gain_show,
		headphone_pa_gain_store);

static struct kobj_attribute sound_control_locked_attribute =
	__ATTR(gpl_sound_control_locked,
		0666,
		sound_control_locked_show,
		sound_control_locked_store);

static struct kobj_attribute sound_control_rec_locked_attribute =
	__ATTR(gpl_sound_control_rec_locked,
		0666,
		sound_control_rec_locked_show,
		sound_control_rec_locked_store);

static struct kobj_attribute sound_control_version_attribute =
	__ATTR(gpl_sound_control_version,
		0444,
		sound_control_version_show, NULL);

static struct kobj_attribute sound_hw_revision_attribute =
	__ATTR(gpl_sound_control_hw_revision,
		0444,
		sound_control_hw_revision_show, NULL);

static struct attribute *sound_control_attrs[] =
	{
		&cam_mic_gain_attribute.attr,
		&mic_gain_attribute.attr,
		&speaker_gain_attribute.attr,
#ifdef CONFIG_MACH_LGE
		&lge_cam_mic_gain_attribute.attr,
		&lge_mic_gain_attribute.attr,
		&lge_speaker_gain_attribute.attr,
		&lge_headphone_pa_gain_attribute.attr,
		&lge_sound_pa_control_locked_attribute.attr,
#endif
		&headphone_gain_attribute.attr,
		&headphone_pa_gain_attribute.attr,
		&sound_control_locked_attribute.attr,
		&sound_control_rec_locked_attribute.attr,
		&sound_reg_sel_attribute.attr,
		&sound_reg_read_attribute.attr,
		&sound_reg_write_attribute.attr,
		&sound_hw_revision_attribute.attr,
		&sound_control_version_attribute.attr,
		NULL,
	};

static struct attribute_group sound_control_attr_group =
	{
		.attrs = sound_control_attrs,
	};

static struct kobject *sound_control_kobj;

static int sound_control_init(void)
{
	int sysfs_result;

	sound_control_kobj =
		kobject_create_and_add("sound_control_3", kernel_kobj);

	if (!sound_control_kobj) {
		pr_err("%s sound_control_kobj create failed!\n",
			__FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(sound_control_kobj,
			&sound_control_attr_group);

	if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(sound_control_kobj);
	}
	return sysfs_result;
}

static void sound_control_exit(void)
{
	if (sound_control_kobj != NULL)
		kobject_put(sound_control_kobj);
}

module_init(sound_control_init);
module_exit(sound_control_exit);
MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_DESCRIPTION("Sound Control Module 3.x");
