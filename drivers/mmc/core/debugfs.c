/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc high-speed SDR200";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_rpm_hold(host, &host->class_dev);
	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);
	mmc_rpm_release(host, &host->class_dev);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

static int mmc_max_clock_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	if (!host)
		return -EINVAL;

	*val = host->f_max;

	return 0;
}

static int mmc_max_clock_set(void *data, u64 val)
{
	struct mmc_host *host = data;
	int err = -EINVAL;
	unsigned long freq = val;
	unsigned int old_freq;

	if (!host || (val < host->f_min))
		goto out;

	mmc_rpm_hold(host, &host->class_dev);
	mmc_claim_host(host);
	if (host->bus_ops && host->bus_ops->change_bus_speed) {
		old_freq = host->f_max;
		host->f_max = freq;

		err = host->bus_ops->change_bus_speed(host, &freq);

		if (err)
			host->f_max = old_freq;
	}
	mmc_release_host(host);
	mmc_rpm_release(host, &host->class_dev);
out:
	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_max_clock_fops, mmc_max_clock_get,
		mmc_max_clock_set, "%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

	if (!debugfs_create_file("max_clock", S_IRUSR | S_IWUSR, root, host,
		&mmc_max_clock_fops))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_rpm_hold(card->host, &card->dev);
	mmc_claim_host(card->host);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_release_host(card->host);
	mmc_rpm_release(card->host, &card->dev);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

#ifdef CONFIG_MACH_LGE
/* LGE_CHANGE
* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
* 2013-06-28, G2-FS@lge.com
*/
static int mmc_ext_csd_read(struct seq_file *s, void *data)
#else
#define EXT_CSD_STR_LEN 1025

static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
#endif
{
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G2-FS@lge.com
	*/
	struct mmc_card *card = s->private;
#else
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
#endif
	u8 *ext_csd;
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G2-FS@lge.com
	*/
	u8 ext_csd_rev;
	int err;
	const char *str;
#else
	int err, i;

	buf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
#endif
	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free;
	}

	mmc_rpm_hold(card->host, &card->dev);
	mmc_claim_host(card->host);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_release_host(card->host);
	mmc_rpm_release(card->host, &card->dev);
	if (err)
		goto out_free;
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G2-FS@lge.com
	*/
	ext_csd_rev = ext_csd[192];
#else
	for (i = 511; i >= 0; i--)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");
	BUG_ON(n != EXT_CSD_STR_LEN);

	filp->private_data = buf;
	kfree(ext_csd);
	return 0;
#endif

#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	 2013-06-28, G2-FS@lge.com
	*/

	switch (ext_csd_rev) {
       case 6:
               str = "4.5";
               break;
       case 5:
               str = "4.41";
               break;
       case 3:
               str = "4.3";
               break;
       case 2:
               str = "4.2";
               break;
       case 1:
               str = "4.1";
               break;
       case 0:
               str = "4.0";
               break;
       default:
               goto out_free;
       }
       seq_printf(s, "Extended CSD rev 1.%d (MMC %s)\n", ext_csd_rev, str);

       if (ext_csd_rev < 3)
               goto out_free; /* No ext_csd */

       /* Parse the Extended CSD registers.
        * Reserved bit should be read as "0" in case of spec older
        * than A441.
        */
               /* B45: reserved [511:505] */
       seq_printf(s, "[504] Supported Command Sets, s_cmd_set: 0x%02x\n", ext_csd[504]);
       seq_printf(s, "[503] HPI features, hpi_features: 0x%02x\n", ext_csd[503]);
       seq_printf(s, "[502] Background operations support, bkops_support: 0x%02x\n", ext_csd[502]);

       if (ext_csd_rev >= 6) {
               seq_printf(s, "[501] Max packed read commands, max_packed_reads: 0x%02x\n", ext_csd[501]);
               seq_printf(s, "[500] Max packed write commands, max_packed_writes: 0x%02x\n", ext_csd[500]);
               seq_printf(s, "[499] Data Tag Support, data_tag_support: 0x%02x\n", ext_csd[499]);
               seq_printf(s, "[498] Tag Unit Size, tag_unit_size: 0x%02x\n", ext_csd[498]);
               seq_printf(s, "[497] Tag Resources Size, tag_res_size: 0x%02x\n", ext_csd[497]);
               seq_printf(s, "[496] Context management capabilities, context_capabilities: 0x%02x\n", ext_csd[496]);
               seq_printf(s, "[495] Large Unit size, large_unit_size_m1: 0x%02x\n", ext_csd[495]);
               seq_printf(s, "[494] Extended partitions attribute support, ext_support: 0x%02x\n", ext_csd[494]);
               /* B45: reserved [493:253] */
               seq_printf(s, "[252:249] Cache size, cache_size %d KiB\n", ext_csd[249] << 0 |
                          (ext_csd[250] << 8) | (ext_csd[251] << 16) |
                          (ext_csd[252] << 24));
               seq_printf(s, "[248] Generic CMD6 timeout, generic_cmd6_time: 0x%02x\n", ext_csd[248]);
               seq_printf(s, "[247] Power off notification timeout, power_off_long_time: 0x%02x\n", ext_csd[247]);
               seq_printf(s, "[246] Background operations status, bkops_status: 0x%02x\n", ext_csd[246]);
               seq_printf(s, "[245:242] Number of correctly programmed sectors, correctly_prg_sectors_num %d KiB\n", ext_csd[242] << 0 |
                          (ext_csd[243] << 8) | (ext_csd[244] << 16) |
                          (ext_csd[245] << 24));

       }
               /* B45: Reserved [493:253]
   	      A441: Reserved [501:247]
          A43: reserved [246:229] */
       if (ext_csd_rev >= 5) {
               seq_printf(s, "[241] 1st initialization time after partitioning, ini_timeout_ap: 0x%02x\n", ext_csd[241]);
               /* B45: reserved [240] */
               /* A441: reserved [240] */
               seq_printf(s, "[239] Power class for 52MHz, DDR at 3.6V, pwr_cl_ddr_52_360: 0x%02x\n", ext_csd[239]);
               seq_printf(s, "[238] POwer class for 52MHz, DDR at 1.95V, pwr_cl_ddr_52_195: 0x%02x\n", ext_csd[238]);

               /* A441: reserved [237-236] */

               if (ext_csd_rev >= 6) {
                       seq_printf(s, "[237] Power class for 200MHz, SDR at 3.6V, pwr_cl_200_360: 0x%02x\n", ext_csd[237]);
                       seq_printf(s, "[236] Power class for 200MHz, SDR at 1.95V, pwr_cl_200_195: 0x%02x\n", ext_csd[236]);
               }

               seq_printf(s, "[235] Minimun Write Performance for 8bit at 52MHz in DDR mode, min_perf_ddr_w_8_52: 0x%02x\n", ext_csd[235]);
               seq_printf(s, "[234] Minimun Read Performance for 8bit at 52MHz in DDR modemin_perf_ddr_r_8_52: 0x%02x\n", ext_csd[234]);
               /* B45: reserved [233] */
               /* A441: reserved [233] */
               seq_printf(s, "[232] TRIM Multiplier, trim_mult: 0x%02x\n", ext_csd[232]);
               seq_printf(s, "[231] Secure Feature support, sec_feature_support: 0x%02x\n", ext_csd[231]);
       }
       if (ext_csd_rev == 5) { /* Obsolete in 4.5 */
               seq_printf(s, "[230] Secure Erase Multiplier, sec_erase_mult: 0x%02x\n", ext_csd[230]);
               seq_printf(s, "[229] Secure TRIM Multiplier, sec_trim_mult:  0x%02x\n", ext_csd[229]);
       }
       seq_printf(s, "[228] Boot information, boot_info: 0x%02x\n", ext_csd[228]);
               /* B45: reserved [227] */
		       /* A441/A43: reserved [227] */
       seq_printf(s, "[226] Boot partition size, boot_size_mult : 0x%02x\n", ext_csd[226]);
       seq_printf(s, "[225] Access size, acc_size: 0x%02x\n", ext_csd[225]);
       seq_printf(s, "[224] High-capacity erase unit size, hc_erase_grp_size: 0x%02x\n", ext_csd[224]);
       seq_printf(s, "[223] High-capacity erase timeout, erase_timeout_mult: 0x%02x\n", ext_csd[223]);
       seq_printf(s, "[222] Reliable write sector count, rel_wr_sec_c: 0x%02x\n", ext_csd[222]);
       seq_printf(s, "[221] High-capacity write protect group size, hc_wp_grp_size: 0x%02x\n", ext_csd[221]);
       seq_printf(s, "[220] Sleep current(VCC), s_c_vcc: 0x%02x\n", ext_csd[220]);
       seq_printf(s, "[219] Sleep current(VCCQ), s_c_vccq: 0x%02x\n", ext_csd[219]);
               /* B45: reserved [218] */
		       /* A441/A43: reserved [218] */
       seq_printf(s, "[217] Sleep/awake timeout, s_a_timeout: 0x%02x\n", ext_csd[217]);
               /* B45: reserved [216] */
		       /* A441/A43: reserved [216] */
       seq_printf(s, "[215:212] Sector Count, sec_count: 0x%08x\n", (ext_csd[215] << 24) |(ext_csd[214] << 16) | (ext_csd[213] << 8)  | ext_csd[212]);
               /* B45: reserved [211] */
		       /* A441/A43: reserved [211] */
       seq_printf(s, "[210] Minimum Write Performance for 8bit at 52MHz, min_perf_w_8_52: 0x%02x\n", ext_csd[210]);
       seq_printf(s, "[209] Minimum Read Performance for 8bit at 52MHz, min_perf_r_8_52: 0x%02x\n", ext_csd[209]);
       seq_printf(s, "[208] Minimum Write Performance for 8bit at 26MHz, for 4bit at 52MHz, min_perf_w_8_26_4_52: 0x%02x\n", ext_csd[208]);
       seq_printf(s, "[207] Minimum Read Performance for 8bit at 26MHz, for 4bit at 52MHz, min_perf_r_8_26_4_52: 0x%02x\n", ext_csd[207]);
       seq_printf(s, "[206] Minimum Write Performance for 4bit at 26MHz, min_perf_w_4_26: 0x%02x\n", ext_csd[206]);
       seq_printf(s, "[205] Minimum Read Performance for 4bit at 26MHz, min_perf_r_4_26: 0x%02x\n", ext_csd[205]);
               /* B45: reserved [204] */
		       /* A441/A43: reserved [204] */
       seq_printf(s, "[203] Power class for 26MHz at 3.6V, pwr_cl_26_360: 0x%02x\n", ext_csd[203]);
       seq_printf(s, "[202] Power class for 52MHz at 3.6V, pwr_cl_52_360: 0x%02x\n", ext_csd[202]);
       seq_printf(s, "[201] Power class for 26MHz at 1.95V, pwr_cl_26_195: 0x%02x\n", ext_csd[201]);
       seq_printf(s, "[200] Power class for 52MHz at 1.95V, pwr_cl_52_195: 0x%02x\n", ext_csd[200]);

       /* A43: reserved [199:198] */
       if (ext_csd_rev >= 5) {
               seq_printf(s, "[199] Partition switching timing, partition_switch_time: 0x%02x\n", ext_csd[199]);
               seq_printf(s, "[198] Out-of-interrupt busy timing, out_of_interrupt_time: 0x%02x\n", ext_csd[198]);
       }

               /* B45: reserved [195] [193] [190] [188] [186] [184] [182] [180] [176] */
		       /* A441/A43: reserved   [197] [195] [193] [190] [188]
		        * [186] [184] [182] [180] [176] */

       if (ext_csd_rev >= 6)
               seq_printf(s, "[197] IO Driver Strength, driver_strength: 0x%02x\n", ext_csd[197]);

       seq_printf(s, "[196] Device type, device_type: 0x%02x\n", ext_csd[196]);
       seq_printf(s, "[194] CSD structure version, csd_structure: 0x%02x\n", ext_csd[194]);
       seq_printf(s, "[192] Extended CSD revision, ext_csd_rev: 0x%02x\n", ext_csd[192]);
       seq_printf(s, "[191] Command set, cmd_set: 0x%02x\n", ext_csd[191]);
       seq_printf(s, "[189] Command set revision, cmd_set_rev: 0x%02x\n", ext_csd[189]);
       seq_printf(s, "[187] Power class, power_class: 0x%02x\n", ext_csd[187]);
       seq_printf(s, "[185] High-speed interface timing, hs_timing: 0x%02x\n", ext_csd[185]);
       /* bus_width: ext_csd[183] not readable */
       seq_printf(s, "[181] Erased memory content, erased_mem_cont: 0x%02x\n", ext_csd[181]);
       seq_printf(s, "[179] Partition configuration, partition_config: 0x%02x\n", ext_csd[179]);
       seq_printf(s, "[178] Boot config protection, boot_config_prot: 0x%02x\n", ext_csd[178]);
       seq_printf(s, "[177] Boot bus Conditions, boot_bus_conditions: 0x%02x\n", ext_csd[177]);
       seq_printf(s, "[175] High-density erase group definition, erase_group_def: 0x%02x\n", ext_csd[175]);

       /* A43: reserved [174:0] */
       if (ext_csd_rev >= 5) {
               seq_printf(s, "[174] Boot write protection status registers, boot_wp_status: 0x%02x\n", ext_csd[174]);
               seq_printf(s, "[173] Boot area write protection register, boot_wp: 0x%02x\n", ext_csd[173]);
               /* B45, A441: reserved [172] */
               seq_printf(s, "[171] User area write protection register, user_wp: 0x%02x\n", ext_csd[171]);
               /* B45, A441: reserved [170] */
               seq_printf(s, "[169] FW configuration, fw_config: 0x%02x\n", ext_csd[169]);
               seq_printf(s, "[168] RPMB Size, rpmb_size_mult: 0x%02x\n", ext_csd[168]);
               seq_printf(s, "[167] Write reliability setting register, wr_rel_set: 0x%02x\n", ext_csd[167]);
               seq_printf(s, "[166] Write reliability parameter register, wr_rel_param: 0x%02x\n", ext_csd[166]);
               /* sanitize_start ext_csd[165]: not readable
                * bkops_start ext_csd[164]: only writable */
               seq_printf(s, "[163] Enable background operations handshake, bkops_en: 0x%02x\n", ext_csd[163]);
               seq_printf(s, "[162] H/W reset function, rst_n_function: 0x%02x\n", ext_csd[162]);
               seq_printf(s, "[161] HPI management, hpi_mgmt: 0x%02x\n", ext_csd[161]);
               seq_printf(s, "[160] Partitioning Support, partitioning_support: 0x%02x\n", ext_csd[160]);
               seq_printf(s, "[159:157] Max Enhanced Area Size, max_enh_size_mult: 0x%06x\n", (ext_csd[159] << 16) | (ext_csd[158] << 8) |ext_csd[157]);
               seq_printf(s, "[156] Partitions attribute, partitions_attribute: 0x%02x\n", ext_csd[156]);
               seq_printf(s, "[155] Partitioning Setting, partition_setting_completed: 0x%02x\n", ext_csd[155]);
               seq_printf(s, "[154:152] General Purpose Partition Size, gp_size_mult_4: 0x%06x\n", (ext_csd[154] << 16) |(ext_csd[153] << 8) | ext_csd[152]);
               seq_printf(s, "[151:149] General Purpose Partition Size, gp_size_mult_3: 0x%06x\n", (ext_csd[151] << 16) |(ext_csd[150] << 8) | ext_csd[149]);
               seq_printf(s, "[148:146] General Purpose Partition Size, gp_size_mult_2: 0x%06x\n", (ext_csd[148] << 16) |(ext_csd[147] << 8) | ext_csd[146]);
               seq_printf(s, "[145:143] General Purpose Partition Size, gp_size_mult_1: 0x%06x\n", (ext_csd[145] << 16) |(ext_csd[144] << 8) | ext_csd[143]);
               seq_printf(s, "[142:140] Enhanced User Data Area Size, enh_size_mult: 0x%06x\n", (ext_csd[142] << 16) |(ext_csd[141] << 8) | ext_csd[140]);
               seq_printf(s, "[139:136] Enhanced User Data Start Address, enh_start_addr: 0x%06x\n", (ext_csd[139] << 24 ) |(ext_csd[138] << 16) |(ext_csd[137] << 8) | ext_csd[136]);

               /* B45, A441: reserved [135] [133]  */
               seq_printf(s, "[134] Bad Block Management mode, sec_bad_blk_mgmnt: 0x%02x\n", ext_csd[134]);
               /* A441: reserved [133:0] */
       }
       /* B45 */
       if (ext_csd_rev >= 6) {
               int j;
               /* tcase_support ext_csd[132] not readable */
               seq_printf(s, "[131] Periodic Wake-up, periodic_wakeup: 0x%02x\n", ext_csd[131]);
               seq_printf(s, "[130] Program CID CSD in DDR mode support, program_cid_csd_ddr_support: 0x%02x\n",
                          ext_csd[130]);
               /* B45: reserved [129:128] */

               for (j = 127; j >= 64; j--)
                       seq_printf(s, "[127:64] Vendor Specific Fields, vendor_specific_field[%d]: 0x%02x\n",
                                  j, ext_csd[j]);

               seq_printf(s, "[63] Native sector size, native_sector_size: 0x%02x\n", ext_csd[63]);
               seq_printf(s, "[62] Sector size emulation, use_native_sector: 0x%02x\n", ext_csd[62]);
               seq_printf(s, "[61] Sector size, data_sector_size: 0x%02x\n", ext_csd[61]);
               seq_printf(s, "[60] 1st initialization after disabling sector size emulation, ini_timeout_emu: 0x%02x\n", ext_csd[60]);
               seq_printf(s, "[59] Class 6 commands control, class_6_ctrl: 0x%02x\n", ext_csd[59]);
               seq_printf(s, "[58] Number of addressed group to be Released, dyncap_needed: 0x%02x\n", ext_csd[58]);
               seq_printf(s, "[57:56] Exception events control, exception_events_ctrl: 0x%04x\n",
                          (ext_csd[57] << 8) | ext_csd[56]);
               seq_printf(s, "[55:54] Exception events status, exception_events_status: 0x%04x\n",
                          (ext_csd[55] << 8) | ext_csd[54]);
               seq_printf(s, "[53:52] Extended Partitions Attribute, ext_partitions_attribute: 0x%04x\n",
                          (ext_csd[53] << 8) | ext_csd[52]);
               for (j = 51; j >= 37; j--)
                       seq_printf(s, "[51:37]Context configuration, context_conf[%d]: 0x%02x\n", j,
                                  ext_csd[j]);

               seq_printf(s, "[36] Packed command status, packed_command_status: 0x%02x\n", ext_csd[36]);
               seq_printf(s, "[35] Packed command failure index, packed_failure_index: 0x%02x\n", ext_csd[35]);
               seq_printf(s, "[34] Power Off Notification, power_off_notification: 0x%02x\n", ext_csd[34]);
               seq_printf(s, "[33] Control to turn the Cache On Off, cache_ctrl: 0x%02x\n", ext_csd[33]);
               /* flush_cache ext_csd[32] not readable */
               /*Reserved [31:0] */
       }
#endif
out_free:
#ifndef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G2-FS@lge.com
	*/
	kfree(buf);
#endif
	kfree(ext_csd);
	return err;
}

#ifdef CONFIG_MACH_LGE
/* LGE_CHANGE
* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
* 2013-06-28, G2-FS@lge.com
*/
static int mmc_ext_csd_open(struct inode *inode, struct file *file)
#else
static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
#endif
{
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G2-FS@lge.com
	*/
	return single_open(file, mmc_ext_csd_read, inode->i_private);
#else
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
#endif
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE
	* http://www.mail-archive.com/linux-mmc@vger.kernel.org/msg10669.html
	* 2013-06-28, G1-FS@lge.com
	*/
	.read		   = seq_read,
	.llseek 		= seq_lseek,
	.release		= single_release,
#else
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
#endif
};

static int mmc_wr_pack_stats_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;

	filp->private_data = card;
	card->wr_pack_stats.print_in_read = 1;
	return 0;
}

#define TEMP_BUF_SIZE 256
static ssize_t mmc_wr_pack_stats_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	struct mmc_wr_pack_stats *pack_stats;
	int i;
	int max_num_of_packed_reqs = 0;
	char *temp_buf;

	if (!card)
		return cnt;

	if (!card->wr_pack_stats.print_in_read)
		return 0;

	if (!card->wr_pack_stats.enabled) {
		pr_info("%s: write packing statistics are disabled\n",
			 mmc_hostname(card->host));
		goto exit;
	}

	pack_stats = &card->wr_pack_stats;

	if (!pack_stats->packing_events) {
		pr_info("%s: NULL packing_events\n", mmc_hostname(card->host));
		goto exit;
	}

	max_num_of_packed_reqs = card->ext_csd.max_packed_writes;

	temp_buf = kmalloc(TEMP_BUF_SIZE, GFP_KERNEL);
	if (!temp_buf)
		goto exit;

	spin_lock(&pack_stats->lock);

	snprintf(temp_buf, TEMP_BUF_SIZE, "%s: write packing statistics:\n",
		mmc_hostname(card->host));
	strlcat(ubuf, temp_buf, cnt);

	for (i = 1 ; i <= max_num_of_packed_reqs ; ++i) {
		if (pack_stats->packing_events[i]) {
			snprintf(temp_buf, TEMP_BUF_SIZE,
				 "%s: Packed %d reqs - %d times\n",
				mmc_hostname(card->host), i,
				pack_stats->packing_events[i]);
			strlcat(ubuf, temp_buf, cnt);
		}
	}

	snprintf(temp_buf, TEMP_BUF_SIZE,
		 "%s: stopped packing due to the following reasons:\n",
		 mmc_hostname(card->host));
	strlcat(ubuf, temp_buf, cnt);

	if (pack_stats->pack_stop_reason[EXCEEDS_SEGMENTS]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: exceed max num of segments\n",
			 mmc_hostname(card->host),
			 pack_stats->pack_stop_reason[EXCEEDS_SEGMENTS]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[EXCEEDS_SECTORS]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: exceed max num of sectors\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[EXCEEDS_SECTORS]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[WRONG_DATA_DIR]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: wrong data direction\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[WRONG_DATA_DIR]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[FLUSH_OR_DISCARD]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: flush or discard\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[FLUSH_OR_DISCARD]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[EMPTY_QUEUE]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: empty queue\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[EMPTY_QUEUE]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[REL_WRITE]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: rel write\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[REL_WRITE]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[THRESHOLD]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: Threshold\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[THRESHOLD]);
		strlcat(ubuf, temp_buf, cnt);
	}

	if (pack_stats->pack_stop_reason[LARGE_SEC_ALIGN]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: Large sector alignment\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[LARGE_SEC_ALIGN]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[RANDOM]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: random request\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[RANDOM]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[FUA]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: fua request\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[FUA]);
		strlcat(ubuf, temp_buf, cnt);
	}

	spin_unlock(&pack_stats->lock);

	kfree(temp_buf);

	pr_info("%s", ubuf);

exit:
	if (card->wr_pack_stats.print_in_read == 1) {
		card->wr_pack_stats.print_in_read = 0;
		return strnlen(ubuf, cnt);
	}

	return 0;
}

static ssize_t mmc_wr_pack_stats_write(struct file *filp,
				       const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	int value;

	if (!card)
		return cnt;

	sscanf(ubuf, "%d", &value);
	if (value) {
		mmc_blk_init_packed_statistics(card);
	} else {
		spin_lock(&card->wr_pack_stats.lock);
		card->wr_pack_stats.enabled = false;
		spin_unlock(&card->wr_pack_stats.lock);
	}

	return cnt;
}

static const struct file_operations mmc_dbg_wr_pack_stats_fops = {
	.open		= mmc_wr_pack_stats_open,
	.read		= mmc_wr_pack_stats_read,
	.write		= mmc_wr_pack_stats_write,
};

static int mmc_bkops_stats_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;

	filp->private_data = card;

	card->bkops_info.bkops_stats.print_stats = 1;
	return 0;
}

static ssize_t mmc_bkops_stats_read(struct file *filp, char __user *ubuf,
				     size_t cnt, loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	struct mmc_bkops_stats *bkops_stats;
	int i;
	char *temp_buf;

	if (!card)
		return cnt;

	bkops_stats = &card->bkops_info.bkops_stats;

	if (!bkops_stats->print_stats)
		return 0;

	if (!bkops_stats->enabled) {
		pr_info("%s: bkops statistics are disabled\n",
			 mmc_hostname(card->host));
		goto exit;
	}

	temp_buf = kmalloc(TEMP_BUF_SIZE, GFP_KERNEL);
	if (!temp_buf)
		goto exit;

	spin_lock(&bkops_stats->lock);

	memset(ubuf, 0, cnt);

	snprintf(temp_buf, TEMP_BUF_SIZE, "%s: bkops statistics:\n",
		mmc_hostname(card->host));
	strlcat(ubuf, temp_buf, cnt);

	for (i = 0 ; i < BKOPS_NUM_OF_SEVERITY_LEVELS ; ++i) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: BKOPS: due to level %d: %u\n",
		 mmc_hostname(card->host), i, bkops_stats->bkops_level[i]);
		strlcat(ubuf, temp_buf, cnt);
	}

	snprintf(temp_buf, TEMP_BUF_SIZE,
		 "%s: BKOPS: stopped due to HPI: %u\n",
		 mmc_hostname(card->host), bkops_stats->hpi);
	strlcat(ubuf, temp_buf, cnt);

	snprintf(temp_buf, TEMP_BUF_SIZE,
		 "%s: BKOPS: how many time host was suspended: %u\n",
		 mmc_hostname(card->host), bkops_stats->suspend);
	strlcat(ubuf, temp_buf, cnt);

	spin_unlock(&bkops_stats->lock);

	kfree(temp_buf);

	pr_info("%s", ubuf);

exit:
	if (bkops_stats->print_stats == 1) {
		bkops_stats->print_stats = 0;
		return strnlen(ubuf, cnt);
	}

	return 0;
}

static ssize_t mmc_bkops_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				      loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	int value;
	struct mmc_bkops_stats *bkops_stats;

	if (!card)
		return cnt;

	bkops_stats = &card->bkops_info.bkops_stats;

	sscanf(ubuf, "%d", &value);
	if (value) {
		mmc_blk_init_bkops_statistics(card);
	} else {
		spin_lock(&bkops_stats->lock);
		bkops_stats->enabled = false;
		spin_unlock(&bkops_stats->lock);
	}

	return cnt;
}

static const struct file_operations mmc_dbg_bkops_stats_fops = {
	.open		= mmc_bkops_stats_open,
	.read		= mmc_bkops_stats_read,
	.write		= mmc_bkops_stats_write,
};

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

	if (mmc_card_mmc(card) && (card->ext_csd.rev >= 6) &&
	    (card->host->caps2 & MMC_CAP2_PACKED_WR))
		if (!debugfs_create_file("wr_pack_stats", S_IRUSR, root, card,
					 &mmc_dbg_wr_pack_stats_fops))
			goto err;

	if (mmc_card_mmc(card) && (card->ext_csd.rev >= 5) &&
	    card->ext_csd.bkops_en)
		if (!debugfs_create_file("bkops_stats", S_IRUSR, root, card,
					 &mmc_dbg_bkops_stats_fops))
			goto err;

	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
