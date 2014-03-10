/*
 * Customer HW 10 dependant file
 *
 * Copyright (C) 1999-2013, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_custom_lge.c 334946 2012-05-24 20:38:00Z $
 */
#ifdef CUSTOMER_HW10
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <proto/ethernet.h>
#include <dngl_stats.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>

#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/ctype.h>


/* Definitions */
#define strtoul(nptr, endptr, base) bcm_strtoul((nptr), (endptr), (base))

#ifndef CONFIG_BCMDHD_CONFIG_PATH
#define CONFIG_BCMDHD_CONFIG_PATH "/data/misc/wifi/config"
#endif

/* Global variables */
bool g_pm_control;

/* Functions */
int dhd_preinit_config(dhd_pub_t *dhd, int ifidx);

#if defined(CUSTOMER_HW4)
#ifdef CONFIG_WIFI_CONTROL_FUNC
void *wifi_get_country_code(char *ccode);
#else
void *wifi_get_country_code(char *ccode) { return NULL; }
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#endif /* CUSTOMER_HW4 */

struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ]; /* ISO 3166-1 country abbreviation */
	char custom_locale[WLC_CNTRY_BUF_SZ]; /* Custom firmware locale */
	int32 custom_locale_rev; /* Custom local revisin default -1 */
};

/* Locale table for sec */
const struct cntry_locales_custom translate_custom_table[] = {
#if defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP)
	/* 4330/4334/43241 */
	{"AR", "AR", 1},
	{"AT", "AT", 1},
	{"AU", "AU", 2},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"CH", "CH", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DE", "DE", 3},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"ES", "ES", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"GB", "GB", 1},
	{"GR", "GR", 1},
	{"HR", "HR", 1},
	{"HU", "HU", 1},
	{"IE", "IE", 1},
	{"IS", "IS", 1},
	{"IT", "IT", 1},
	{"JP", "JP", 5},
	{"KR", "KR", 24},
	{"KW", "KW", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"LV", "LV", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"PY", "PY", 1},
	{"RO", "RO", 1},
	{"RU", "RU", 13},
	{"SE", "SE", 1},
	{"SI", "SI", 1},
	{"SK", "SK", 1},
	{"TW", "TW", 2},
#ifdef BCM4330_CHIP
	{"",   "XZ", 1},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 1},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 1},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 1},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 1},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 1},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 1},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 1},	/* Universal if Country code is MARSHALL ISLANDS */
	{"JO", "XZ", 1},	/* Universal if Country code is Jordan */
	{"PG", "XZ", 1},	/* Universal if Country code is Papua New Guinea */
	{"SA", "XZ", 1},	/* Universal if Country code is Saudi Arabia */
	{"AF", "XZ", 1},	/* Universal if Country code is Afghanistan */
	{"US", "US", 5},
	{"UA", "UY", 0},
	{"AD", "AL", 0},
	{"CX", "AU", 2},
	{"GE", "GB", 1},
	{"ID", "MW", 0},
	{"KI", "AU", 2},
	{"NP", "SA", 0},
	{"WS", "SA", 0},
	{"LR", "BR", 0},
	{"ZM", "IN", 0},
	{"AN", "AG", 0},
	{"AI", "AS", 0},
	{"BM", "AS", 0},
	{"DZ", "GB", 1},
	{"LC", "AG", 0},
	{"MF", "BY", 0},
	{"GY", "CU", 0},
	{"LA", "GB", 1},
	{"LB", "BR", 0},
	{"MA", "IL", 0},
	{"MO", "BD", 0},
	{"MW", "BD", 0},
	{"QA", "BD", 0},
	{"TR", "GB", 1},
	{"TZ", "BF", 0},
	{"VN", "BR", 0},
	{"AE", "AZ", 0},
	{"IQ", "GB", 1},
	{"CN", "CL", 0},
	{"MX", "MX", 1},
#else
	/* 4334/43241 */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"SG", "SG", 4},
	{"US", "US", 46},
	{"UA", "UA", 8},
	{"CO", "CO", 4},
	{"ID", "ID", 1},
	{"LA", "LA", 1},
	{"LB", "LB", 2},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"TR", "TR", 7},
#endif /* defined(BCM4330_CHIP) */
#ifdef BCM4334_CHIP
	{"AE", "AE", 1},
	{"MX", "MX", 1},
#endif /* defined(BCM4334_CHIP) */
#ifdef BCM43241_CHIP
	{"AE", "AE", 6},
	{"BD", "BD", 2},
	{"CN", "CN", 38},
	{"MX", "MX", 20},
#endif /* defined(BCM43241_CHIP) */
#else  /* defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP) */
	/* default ccode/regrev */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"AL", "AL", 2},
	{"DZ", "DZ", 1},
	{"AS", "AS", 12},
	{"AI", "AI", 1},
	{"AG", "AG", 2},
	{"AR", "AR", 21},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},
	{"BD", "BD", 2},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 4},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"CA", "CA", 31},
	{"KY", "KY", 3},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 12},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 7},
	{"IT", "IT", 4},
	{"JP", "JP", 45},
	{"JO", "JO", 3},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "MO", 2},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 20},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NP", "NP", 3},
	{"NL", "NL", 4},
	{"AN", "AN", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "PG", 2},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 20},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "SN", 2},
	{"RS", "RS", 2},
	{"SG", "SG", 4},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
	{"UG", "UG", 2},
	{"GB", "GB", 6},
	{"UY", "UY", 1},
	{"VI", "VI", 13},
	{"VA", "VA", 2},
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"ZM", "ZM", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 25},
	{"KR", "KR", 48},
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"GT", "GT", 1},
	{"FR", "FR", 5},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
#endif /* default ccode/regrev */
};

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(char *country_iso_code, wl_country_t *cspec)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))

	struct cntry_locales_custom *cloc_ptr;

	if (!cspec)
		return;

	cloc_ptr = wifi_get_country_code(country_iso_code);
	if (cloc_ptr) {
		strlcpy(cspec->ccode, cloc_ptr->custom_locale, WLC_CNTRY_BUF_SZ);
		cspec->rev = cloc_ptr->custom_locale_rev;
	}
	return;
#else
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}
	return;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)) */
}

static int dhd_preinit_proc(dhd_pub_t *dhd, int ifidx, char *name, char *value)
{
	int var_int;
	wl_country_t cspec = {{0}, -1, {0}};
	char *revstr;
	char *endptr = NULL;
	int iolen;
	char smbuf[WLC_IOCTL_SMLEN*2];
	int roam_trigger[2] = {CUSTOM_ROAM_TRIGGER_SETTING, WLC_BAND_ALL};
#ifdef ROAM_AP_ENV_DETECTION
	int roam_env_mode = AP_ENV_INDETERMINATE;
#endif /* ROAM_AP_ENV_DETECTION */

	if (!strcmp(name, "country")) {
		revstr = strchr(value, '/');
		if (revstr) {
			cspec.rev = strtoul(revstr + 1, &endptr, 10);
			memcpy(cspec.country_abbrev, value, WLC_CNTRY_BUF_SZ);
			cspec.country_abbrev[2] = '\0';
			memcpy(cspec.ccode, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);
			memset(smbuf, 0, sizeof(smbuf));
			printf("config country code is country : %s, rev : %d !!\n",
				cspec.country_abbrev, cspec.rev);
			iolen = bcm_mkiovar("country", (char*)&cspec, sizeof(cspec),
				smbuf, sizeof(smbuf));
			return dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR,
				smbuf, iolen, TRUE, 0);
		}
		return dhd_wl_ioctl_cmd(dhd, WLC_SET_COUNTRY,
			value, WLC_CNTRY_BUF_SZ, TRUE, 0);
	} else if (!strcmp(name, "roam_scan_period")) {
		var_int = (int)simple_strtol(value, NULL, 0);
		return dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_SCAN_PERIOD,
			&var_int, sizeof(var_int), TRUE, 0);
	} else if (!strcmp(name, "roam_delta")) {
		struct {
			int val;
			int band;
		} x;
		x.val = (int)simple_strtol(value, NULL, 0);
		x.band = WLC_BAND_ALL;
		return dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_DELTA, &x, sizeof(x), TRUE, 0);
	} else if (!strcmp(name, "roam_trigger")) {
		int ret = 0;

		roam_trigger[0] = (int)simple_strtol(value, NULL, 0);
		roam_trigger[1] = WLC_BAND_ALL;
		ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_ROAM_TRIGGER,
			&roam_trigger, sizeof(roam_trigger), TRUE, 0);

#ifdef ROAM_AP_ENV_DETECTION
		if (roam_trigger[0] == WL_AUTO_ROAM_TRIGGER) {
			char iovbuf[128];
			bcm_mkiovar("roam_env_detection", (char *)&roam_env_mode,
				4, iovbuf, sizeof(iovbuf));
			if (dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0) == BCME_OK) {
				dhd->roam_env_detection = TRUE;
			} else {
				dhd->roam_env_detection = FALSE;
			}
		}
#endif /* ROAM_AP_ENV_DETECTION */
		return ret;
	} else if (!strcmp(name, "PM")) {
		int ret = 0;
		var_int = (int)simple_strtol(value, NULL, 0);

		ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_PM,
			&var_int, sizeof(var_int), TRUE, 0);

#if defined(CONFIG_PM_LOCK)
		if (var_int == 0) {
			g_pm_control = TRUE;
			printk("%s var_int=%d don't control PM\n", __func__, var_int);
		} else {
			g_pm_control = FALSE;
			printk("%s var_int=%d do control PM\n", __func__, var_int);
		}
#endif /* CONFIG_PM_LOCK */

		return ret;
	}
#ifdef WLBTAMP
	else if (!strcmp(name, "btamp_chan")) {
		int btamp_chan;
		int iov_len = 0;
		char iovbuf[128];
		int ret;

		btamp_chan = (int)simple_strtol(value, NULL, 0);
		iov_len = bcm_mkiovar("btamp_chan", (char *)&btamp_chan, 4, iovbuf, sizeof(iovbuf));
		if ((ret  = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, iov_len, TRUE, 0) < 0))
			DHD_ERROR(("%s btamp_chan=%d set failed code %d\n",
				__FUNCTION__, btamp_chan, ret));
		else
			DHD_ERROR(("%s btamp_chan %d set success\n", __FUNCTION__, btamp_chan));
	}
#endif /* WLBTAMP */
	 else if (!strcmp(name, "band")) {
		int ret;
		if (!strcmp(value, "auto"))
			var_int = WLC_BAND_AUTO;
		else if (!strcmp(value, "a"))
			var_int = WLC_BAND_5G;
		else if (!strcmp(value, "b"))
			var_int = WLC_BAND_2G;
		else if (!strcmp(value, "all"))
			var_int = WLC_BAND_ALL;
		else {
			printk("set band value should be one of the a or b or all\n");
			var_int = WLC_BAND_AUTO;
		}
		if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_BAND,
			&var_int, sizeof(var_int), TRUE, 0)) < 0)
			printk(" set band err=%d\n", ret);

		return ret;

	} else if (!strcmp(name, "cur_etheraddr")) {
		struct ether_addr ea;
		char buf[32];
		uint iovlen;
		int ret;

		bcm_ether_atoe(value, &ea);

		ret = memcmp(&ea.octet, dhd->mac.octet, ETHER_ADDR_LEN);
		if (ret == 0) {
			DHD_ERROR(("%s: Same Macaddr\n", __FUNCTION__));
			return 0;
		}

		DHD_ERROR(("%s: Change Macaddr = %02X:%02X:%02X:%02X:%02X:%02X\n", __FUNCTION__,
			ea.octet[0], ea.octet[1], ea.octet[2],
			ea.octet[3], ea.octet[4], ea.octet[5]));

		iovlen = bcm_mkiovar("cur_etheraddr", (char*)&ea, ETHER_ADDR_LEN, buf, 32);

		ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, buf, iovlen, TRUE, 0);
		if (ret < 0) {
			DHD_ERROR(("%s: can't set MAC address , error=%d\n", __FUNCTION__, ret));
			return ret;
		} else {
			memcpy(dhd->mac.octet, (void *)&ea, ETHER_ADDR_LEN);
			return ret;
		}
	}
	else if (!strcmp(name, "lpc")) {
		int ret = 0;
		char buf[32];
		uint iovlen;
		var_int = (int)simple_strtol(value, NULL, 0);
		if (dhd_wl_ioctl_cmd(dhd, WLC_DOWN, NULL, 0, TRUE, 0) < 0) {
			DHD_ERROR(("%s: wl down failed\n", __FUNCTION__));
		}
		iovlen=bcm_mkiovar("lpc", (char *)&var_int, 4, buf, sizeof(buf));
		if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, buf, iovlen, TRUE, 0)) < 0) {
			DHD_ERROR(("%s Set lpc failed  %d\n", __FUNCTION__, ret));
		}
		if (dhd_wl_ioctl_cmd(dhd, WLC_UP, NULL, 0, TRUE, 0) < 0) {
			DHD_ERROR(("%s: wl up failed\n", __FUNCTION__));
		}
		return ret;
	}
	else if (!strcmp(name, "vht_features")) {
		int ret = 0;
		char buf[32];
		uint iovlen;
		var_int = (int)simple_strtol(value, NULL, 0);

		if (dhd_wl_ioctl_cmd(dhd, WLC_DOWN, NULL, 0, TRUE, 0) < 0) {
			DHD_ERROR(("%s: wl down failed\n", __FUNCTION__));
		}
		iovlen=bcm_mkiovar("vht_features", (char *)&var_int, 4, buf, sizeof(buf));
		if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, buf, iovlen, TRUE, 0)) < 0) {
			DHD_ERROR(("%s Set vht_features failed  %d\n", __FUNCTION__, ret));
		}
		if (dhd_wl_ioctl_cmd(dhd, WLC_UP, NULL, 0, TRUE, 0) < 0) {
			DHD_ERROR(("%s: wl up failed\n", __FUNCTION__));
		}
		return ret;
	}
	else {
		uint iovlen;
		char iovbuf[WLC_IOCTL_SMLEN];

		/* wlu_iovar_setint */
		var_int = (int)simple_strtol(value, NULL, 0);

		/* Setup timeout bcn_timeout from dhd driver 4.217.48 */
		if (!strcmp(name, "roam_off")) {
			/* Setup timeout if Beacons are lost to report link down */
			if (var_int) {
				uint bcn_timeout = 2;
				bcm_mkiovar("bcn_timeout", (char *)&bcn_timeout, 4,
					iovbuf, sizeof(iovbuf));
				dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
			}
		}
		/* Setup timeout bcm_timeout from dhd driver 4.217.48 */

		DHD_INFO(("%s:[%s]=[%d]\n", __FUNCTION__, name, var_int));

		iovlen = bcm_mkiovar(name, (char *)&var_int, sizeof(var_int),
			iovbuf, sizeof(iovbuf));
		return dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR,
			iovbuf, iovlen, TRUE, 0);
	}

	return 0;
}

int dhd_preinit_config(dhd_pub_t *dhd, int ifidx)
{
	mm_segment_t old_fs;
	struct kstat stat;
	struct file *fp = NULL;
	unsigned int len;
	char *buf = NULL, *p, *name, *value;
	int ret = 0;
	char *config_path;

	config_path = CONFIG_BCMDHD_CONFIG_PATH;

	if (!config_path)
	{
		printk(KERN_ERR "config_path can't read. \n");
		return 0;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	if ((ret = vfs_stat(config_path, &stat))) {
		set_fs(old_fs);
		printk(KERN_ERR "%s: Failed to get information (%d)\n",
			config_path, ret);
		return ret;
	}
	set_fs(old_fs);

	if (!(buf = MALLOC(dhd->osh, stat.size + 1))) {
		printk(KERN_ERR "Failed to allocate memory %llu bytes\n", stat.size);
		return -ENOMEM;
	}

	printk("dhd_preinit_config : config path : %s \n", config_path);

	if (!(fp = dhd_os_open_image(config_path)) ||
		(len = dhd_os_get_image_block(buf, stat.size, fp)) < 0)
		goto err;

	buf[stat.size] = '\0';
	for (p = buf; *p; p++) {
		if (isspace(*p))
			continue;
		for (name = p++; *p && !isspace(*p); p++) {
			if (*p == '=') {
				*p = '\0';
				p++;
				for (value = p; *p && !isspace(*p); p++);
				*p = '\0';
				if ((ret = dhd_preinit_proc(dhd, ifidx, name, value)) < 0) {
					printk(KERN_ERR "%s: %s=%s\n",
						bcmerrorstr(ret), name, value);
				}
				break;
			}
		}
	}
	ret = 0;

out:
	if (fp)
		dhd_os_close_image(fp);
	if (buf)
		MFREE(dhd->osh, buf, stat.size+1);
	return ret;

err:
	ret = -1;
	goto out;
}

/* BRCM_UPDATE_E for KEEP_ALIVE */
int wl_keep_alive_set(struct net_device *dev, char* extra, int total_len)
{
	char 				buf[256];
	const char 			*str;
	wl_mkeep_alive_pkt_t	mkeep_alive_pkt;
	wl_mkeep_alive_pkt_t	*mkeep_alive_pktp;
	int					buf_len;
	int					str_len;
	int res 				= -1;
	uint period_msec = 0;

	if (extra == NULL)
	{
		 DHD_ERROR(("%s: extra is NULL\n", __FUNCTION__));
		 return -1;
	}
	if (sscanf(extra, "%d", &period_msec) != 1)
	{
		 DHD_ERROR(("%s: sscanf error. check period_msec value\n", __FUNCTION__));
		 return -EINVAL;
	}
	DHD_ERROR(("%s: period_msec is %d\n", __FUNCTION__, period_msec));

	memset(&mkeep_alive_pkt, 0, sizeof(wl_mkeep_alive_pkt_t));

	str = "mkeep_alive";
	str_len = strlen(str);
	strncpy(buf, str, str_len);
	buf[ str_len ] = '\0';
	mkeep_alive_pktp = (wl_mkeep_alive_pkt_t *) (buf + str_len + 1);
	mkeep_alive_pkt.period_msec = period_msec;
	buf_len = str_len + 1;
	mkeep_alive_pkt.version = htod16(WL_MKEEP_ALIVE_VERSION);
	mkeep_alive_pkt.length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);

	/* Setup keep alive zero for null packet generation */
	mkeep_alive_pkt.keep_alive_id = 0;
	mkeep_alive_pkt.len_bytes = 0;
	buf_len += WL_MKEEP_ALIVE_FIXED_LEN;
	/* Keep-alive attributes are set in local	variable (mkeep_alive_pkt), and
	 * then memcpy'ed into buffer (mkeep_alive_pktp) since there is no
	 * guarantee that the buffer is properly aligned.
	 */
	memcpy((char *)mkeep_alive_pktp, &mkeep_alive_pkt, WL_MKEEP_ALIVE_FIXED_LEN);

	if ((res = wldev_ioctl(dev, WLC_SET_VAR, buf, buf_len, TRUE)) < 0)
	{
		DHD_ERROR(("%s:keep_alive set failed. res[%d]\n", __FUNCTION__, res));
	}
	else
	{
		DHD_ERROR(("%s:keep_alive set ok. res[%d]\n", __FUNCTION__, res));
	}

	return res;
}
#endif /* CUSTOMER_HW10 */
