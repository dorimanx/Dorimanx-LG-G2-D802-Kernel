/*
* Customer code to add GPIO control during WLAN start/stop
* Copyright (C) 1999-2012, Broadcom Corporation
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
* $Id: dhd_custom_gpio.c 353167 2012-08-24 22:11:30Z $
*/

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>

#include <dngl_stats.h>
#include <dhd.h>

#include <wlioctl.h>
#include <wl_iw.h>

#define WL_ERROR(x) printf x
#define WL_TRACE(x)

#ifdef CUSTOMER_HW
extern  void bcm_wlan_power_off(int);
extern  void bcm_wlan_power_on(int);
#endif /* CUSTOMER_HW */
#if defined(CUSTOMER_HW2) || defined(CUSTOMER_HW4)
#ifdef CONFIG_WIFI_CONTROL_FUNC
int wifi_set_power(int on, unsigned long msec);
int wifi_get_irq_number(unsigned long *irq_flags_ptr);
int wifi_get_mac_addr(unsigned char *buf);
void *wifi_get_country_code(char *ccode);
#else
int wifi_set_power(int on, unsigned long msec) { return -1; }
int wifi_get_irq_number(unsigned long *irq_flags_ptr) { return -1; }
int wifi_get_mac_addr(unsigned char *buf) { return -1; }
void *wifi_get_country_code(char *ccode) { return NULL; }
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#endif /* CUSTOMER_HW2 || CUSTOMER_HW4 */

#if defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID)

#if defined(BCMLXSDMMC)
extern int sdioh_mmc_irq(int irq);
#endif /* (BCMLXSDMMC)  */

#ifdef CUSTOMER_HW3
#include <mach/gpio.h>
#endif

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1;

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

/* This function will return:
 *  1) return :  Host gpio interrupt number per customer platform
 *  2) irq_flags_ptr : Type of Host interrupt as Level or Edge
 *
 *  NOTE :
 *  Customer should check his platform definitions
 *  and his Host Interrupt spec
 *  to figure out the proper setting for his platform.
 *  Broadcom provides just reference settings as example.
 *
 */
int dhd_customer_oob_irq_map(unsigned long *irq_flags_ptr)
{
	int  host_oob_irq = 0;

#if defined(CUSTOMER_HW2) || defined(CUSTOMER_HW4)
	host_oob_irq = wifi_get_irq_number(irq_flags_ptr);

#else
#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif /* CUSTOMER_OOB_GPIO_NUM */

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
		__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#if defined CUSTOMER_HW
	host_oob_irq = MSM_GPIO_TO_INT(dhd_oob_gpio_num);
#elif defined CUSTOMER_HW3
	gpio_request(dhd_oob_gpio_num, "oob irq");
	host_oob_irq = gpio_to_irq(dhd_oob_gpio_num);
	gpio_direction_input(dhd_oob_gpio_num);
#endif /* CUSTOMER_HW */
#endif /* CUSTOMER_HW2 || CUSTOMER_HW4 */

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID) */

/* Customer function to control hw specific wlan gpios */
void
dhd_customer_gpio_wlan_ctrl(int onoff)
{
	switch (onoff) {
		case WLAN_RESET_OFF:
			WL_TRACE(("%s: call customer specific GPIO to insert WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(2);
#endif /* CUSTOMER_HW */
#if defined(CUSTOMER_HW2) || defined(CUSTOMER_HW4)
			wifi_set_power(0, 0);
#endif
			WL_ERROR(("=========== WLAN placed in RESET ========\n"));
		break;

		case WLAN_RESET_ON:
			WL_TRACE(("%s: callc customer specific GPIO to remove WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(2);
#endif /* CUSTOMER_HW */
#if defined(CUSTOMER_HW2) || defined(CUSTOMER_HW4)
			wifi_set_power(1, 0);
#endif
			WL_ERROR(("=========== WLAN going back to live  ========\n"));
		break;

		case WLAN_POWER_OFF:
			WL_TRACE(("%s: call customer specific GPIO to turn off WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(1);
#endif /* CUSTOMER_HW */
		break;

		case WLAN_POWER_ON:
			WL_TRACE(("%s: call customer specific GPIO to turn on WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(1);
			/* Lets customer power to get stable */
			OSL_DELAY(200);
#endif /* CUSTOMER_HW */
		break;
	}
}

#ifdef GET_CUSTOM_MAC_ENABLE
/* Function to get custom MAC address */
int
dhd_custom_get_mac_address(unsigned char *buf)
{
	int ret = 0;

	WL_TRACE(("%s Enter\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;

	/* Customer access to MAC address stored outside of DHD driver */
#if (defined(CUSTOMER_HW2) || defined(CUSTOMER_HW10)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	ret = wifi_get_mac_addr(buf);
#endif

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return ret;
}
#endif /* GET_CUSTOM_MAC_ENABLE */

#if !defined(CUSTOMER_HW4) || defined(CUSTOMER_HW10)
#ifdef CUSTOMER_HW10
#define EXAMPLE_TABLE
#endif
/* Customized Locale table : OPTIONAL feature */
const struct cntry_locales_custom translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
#ifdef EXAMPLE_TABLE
	{"",   "XY", 4},  /* Universal if Country code is unknown or empty */
#if 0  //                                                      
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries to : EU regrev 05 */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3}, /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3},
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
#else

#if defined(CONFIG_MACH_APQ8064_GVAR_CMCC) //                                                                    
	{"AD", "IL", 10}, //Andorra
	{"AE", "IL", 10}, //UAE
	{"AF", "IL" , 10}, //Afghanistan
	{"AG", "US", 46}, //Antigua & Barbuda
	{"AI", "US", 46}, //Anguilla
	{"AL", "IL", 10}, //Albania
	{"AM", "IL", 10}, //Armenia
	{"AN", "IL", 10}, //Netherlands Antilles
	{"AO", "IL", 10}, //Angola
	{"AR", "IL", 10}, //Argentina
	{"AS", "US", 46}, //American Samoa (USA)
	{"AT", "IL", 10}, //Austria
	{"AU", "IL", 10}, //Australia
	{"AW", "IL", 10}, //Aruba
	{"AZ", "IL", 10}, //Azerbaijan
	{"BA", "IL", 10}, //Bosnia and Herzegovina
	{"BB", "IL", 10}, //Barbados
	{"BD", "IL", 10}, //Bangladesh
	{"BE", "IL" , 10}, //Belgium
	{"BF", "IL", 10}, //Burkina Faso
	{"BG", "IL", 10}, //Bulgaria
	{"BH", "IL", 10}, //Bahrain
	{"BI", "IL" , 10}, //Burundi
	{"BJ", "IL", 10}, //Benin
	{"BM", "US", 46}, //Bermuda
	{"BN", "IL", 10}, //Brunei
	{"BO", "IL", 10}, //Bolivia
	{"BR", "IL", 10}, //Brazil
	{"BS", "IL", 10}, //Bahamas
	{"BT", "IL", 10}, //Bhntan
	{"BW", "IL", 10}, //Botswana
	{"BY", "IL", 10}, //Belarus
	{"BZ", "IL" , 10}, //Belize
	{"CA", "US", 46}, //Canada
	{"CD", "IL", 10}, //Congo. Democratic Republic of the
	{"CF", "IL", 10}, //Central African Republic
	{"CG", "IL", 10}, //Congo. Republic of the
	{"CH", "IL", 10}, //Switzerland
	{"CI", "IL", 10}, //Cote d'lvoire
	{"CK", "IL", 10}, //Cook Island
	{"CL", "IL", 10}, //Chile
	{"CM", "IL", 10}, //Cameroon
	{"CN", "IL", 10}, //China
	{"CO", "IL", 10}, //Columbia
	{"CR", "IL", 10}, //Costa Rica
	{"CU", "IL", 10}, //Cuba
	{"CV", "IL", 10}, //Cape Verde
	{"CX", "IL", 10}, //Christmas Island (Australia)
	{"CY", "IL", 10}, //Cyprus
	{"CZ", "IL", 10}, //Czech
	{"DE", "IL", 10}, //Germany
	{"DJ", "IL", 10}, //Djibouti
	{"DK", "IL", 10}, //Denmark
	{"DM", "IL", 10}, //Dominica
	{"DO", "IL", 10}, //Dominican Republic
	{"DZ", "IL", 10}, //Algeria
	{"EC", "IL", 10}, //Ecuador
	{"EE", "IL", 10}, //Estonia
	{"EG", "IL", 10}, //Egypt
	{"ER", "IL", 10}, //Eritrea
	{"ES", "IL", 10}, //Spain
	{"ET", "IL", 10}, //Ethiopia
	{"FI", "IL", 10}, //Finland
	{"FJ", "IL", 10}, //Fiji
	{"FM", "US", 46}, //Federated States of Micronesia
	{"FO", "IL", 10}, //Faroe Island
	{"FR", "IL", 10}, //France
	{"GA", "IL", 10}, //Gabon
	{"GB", "IL", 10}, //United Kingdom
	{"GD", "IL", 10}, //Grenada
	{"GE", "IL", 10}, //Georgia
	{"GF", "IL", 10}, //French Guiana
	{"GH", "IL", 10}, //Ghana
	{"GI", "IL", 10}, //Gibraltar
	{"GM", "IL", 10}, //Gambia
	{"GN", "IL", 10}, //Guinea
	{"GP", "IL", 10}, //Guadeloupe
	{"GQ", "IL", 10}, //Equatorial Guinea
	{"GR", "IL", 10}, //Greece
	{"GT", "IL", 10}, //Guatemala
	{"GU", "US", 46}, //Guam
	{"GW", "IL", 10}, //Guinea-Bissau
	{"GY", "IL", 10}, //Guyana
	{"HK", "IL", 10}, //Hong Kong
	{"HN", "IL" , 10}, //Honduras
	{"HR", "IL", 10}, //Croatia
	{"HT", "IL", 10}, //Haiti
	{"HU", "IL", 10}, //Hungary
	{"ID", "IL", 10}, //Indonesia
	{"IE", "IL", 10}, //Ireland
	{"IL", "IL" , 10}, //Israel
	{"IM", "IL", 10}, //Isle of Man
	{"IN", "IL", 10}, //India
	{"IQ", "IL", 10}, //Iraq
	{"IR", "IL", 10}, //Iran
	{"IS", "IL", 10}, //Iceland
	{"IT", "IL", 10}, //Italy
	{"JE", "IL", 10}, //Jersey
	{"JM", "IL", 10}, //Jameica
	{"JO", "IL", 10}, //Jordan
	{"JP", "IL", 10}, //Japan
	{"KE", "IL", 10}, //Kenya
	{"KG", "IL", 10}, //Kyrgyzstan
	{"KH", "IL", 10}, //Cambodia
	{"KI", "IL", 10}, //Kiribati
	{"KM", "IL", 10}, //Comoros
	{"KP", "IL", 10}, //North Korea
	{"KR", "IL", 10}, //South Korea
	{"KW", "IL", 10}, //Kuwait
	{"KY", "US", 46}, //Cayman Islands
	{"KZ", "IL", 10}, //Kazakhstan
	{"LA", "IL", 10}, //Laos
	{"LB", "IL" , 10}, //Lebanon
	{"LC", "IL", 10}, //Saint Lucia
	{"LI", "IL", 10}, //Liechtenstein
	{"LK", "IL" , 10}, //Sri Lanka
	{"LR", "IL", 10}, //Liberia
	{"LS", "IL", 10}, //Lesotho
	{"LT", "IL", 10}, //Lithuania
	{"LU", "IL", 10}, //Luxemburg
	{"LV", "IL", 10}, //Latvia
	{"LY", "IL", 10}, //Libya
	{"MA", "IL", 1}, //Morocco
	{"MC", "IL", 10}, //Monaco
	{"MD", "IL", 10}, //Moldova
	{"ME", "IL", 10}, //Montenegro
	{"MF", "IL", 10}, //Saint Martin / Sint Marteen (Added on window's list)
	{"MG", "IL" , 10}, //Madagascar
	{"MH", "IL", 10}, //Marshall Islands
	{"MK", "IL", 10}, //Macedonia
	{"ML", "IL" , 10}, //Mali
	{"MM", "IL", 10}, //Burma (Myanmar)
	{"MN", "IL", 10}, //Mongolia
	{"MO", "IL", 10}, //Macau
	{"MP", "US", 46}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
	{"MQ", "IL", 10}, //Martinique (France)
	{"MR", "IL", 10}, //Mauritania
	{"MS", "IL", 10}, //Montserrat (UK)
	{"MT", "IL", 10}, //Malta
	{"MU", "IL", 10}, //Mauritius
	{"MV", "IL", 10}, //Maldives
	{"MW", "IL", 10}, //Malawi
	{"MX", "IL", 10}, //Mexico
	{"MY", "IL", 10}, //Malaysia
	{"MZ", "IL", 10}, //Mozambique
	{"NA", "IL", 10}, //Namibia
	{"NC", "IL", 10}, //New Caledonia
	{"NE", "IL", 10}, //Niger
	{"NF", "IL", 10}, //Norfolk Island
	{"NG", "IL", 10}, //Nigeria
	{"NI", "IL" , 10}, //Nicaragua
	{"NL", "IL", 10}, //Netherlands
	{"NO", "IL", 10}, //Norway
	{"NP", "IL", 10}, //Nepal
	{"NR", "IL", 10}, //Nauru
	{"NU", "IL", 10}, //Niue
	{"NZ", "IL", 10 }, //New Zealand
	{"OM", "IL", 10}, //Oman
	{"PA", "IL", 10}, //Panama
	{"PE", "IL", 10}, //Peru
	{"PF", "IL", 10}, //French Polynesia (France)
	{"PG", "IL", 10}, //Papua New Guinea
	{"PH", "IL", 10}, //Philippines
	{"PK", "IL", 10}, //Pakistan
	{"PL", "IL", 10}, //Poland
	{"PM", "IL", 10}, //Saint Pierre and Miquelon
	{"PN", "IL", 10}, //Pitcairn Islands
	{"PR", "US", 46}, //Puerto Rico (USA)
	{"PS", "IL", 10}, //Palestinian Authority
	{"PT", "IL", 10}, //Portugal
	{"PW", "IL", 10}, //Palau
	{"PY", "IL", 10}, //Paraguay
	{"QA", "IL", 10}, //Qatar
	{"RE", "IL", 10}, //Reunion (France)
	{"RKS", "IL", 10}, //Kosvo (Added on window's list)
	{"RO", "IL", 10}, //Romania
	{"RS", "IL", 10}, //Serbia
	{"RU", "IL", 10}, //Russia
	{"RW", "IL", 10}, //Rwanda
	{"SA", "IL", 10}, //Saudi Arabia
	{"SB", "IL", 10}, //Solomon Islands
	{"SC", "IL", 10}, //Seychelles
	{"SD", "IL", 10}, //Sudan
	{"SE", "IL", 10}, //Sweden
	{"SG", "IL", 10}, //Singapole
	{"SI", "IL", 10}, //Slovenia
	{"SK", "IL", 10}, //Slovakia
	{"SKN", "IL", 10}, //Saint Kitts and Nevis
	{"SL", "IL", 10}, //Sierra Leone
	{"SM", "IL", 10}, //San Marino
	{"SN", "IL", 10}, //Senegal
	{"SO", "IL", 10}, //Somalia
	{"SR", "IL", 10}, //Suriname
	{"SS", "IL", 10}, //South_Sudan
	{"ST", "IL", 10}, //Sao Tome and Principe
	{"SV", "IL", 10}, //El Salvador
	{"SY", "IL", 10}, //Syria
	{"SZ", "IL", 10}, //Swaziland
	{"TC", "IL", 10}, //Turks and Caicos Islands (UK)
	{"TD", "IL", 10}, //Chad
	{"TF", "IL", 10}, //French Southern and Antarctic Lands)
	{"TG", "IL", 10}, //Togo
	{"TH", "IL", 10}, //Thailand
	{"TJ", "IL", 10}, //Tajikistan
	{"TL", "IL", 10}, //East Timor
	{"TM", "IL", 10}, //Turkmenistan
	{"TN", "IL", 10}, //Tunisia
	{"TO", "IL", 10}, //Tonga
	{"TR", "IL", 10}, //Turkey
	{"TT", "IL", 10}, //Trinidad and Tobago
	{"TV", "IL", 10}, //Tuvalu
	{"TW", "US", 46}, //Taiwan
	{"TZ", "IL", 10}, //Tanzania
	{"UA", "IL", 10}, //Ukraine
	{"UG", "IL", 10}, //Ugnada
	{"US", "US", 46}, //US
	{"UY", "IL", 10}, //Uruguay
	{"UZ", "IL", 10}, //Uzbekistan
	{"VA", "IL", 10}, //Vatican (Holy See)
	{"VC", "IL", 10}, //Saint Vincent and the Grenadines
	{"VE", "IL", 10}, //Venezuela
	{"VG", "IL", 10}, //British Virgin Islands
	{"VI", "US", 46}, //US Virgin Islands
	{"VN", "IL", 10}, //Vietnam
	{"VU", "IL", 10}, //Vanuatu
	{"WS", "IL", 10}, //Samoa
	{"YE", "IL", 10}, //Yemen
	{"YT", "IL", 10}, //Mayotte (France)
	{"ZA", "IL", 10}, //South Africa
	{"ZM", "IL", 10}, //Zambia
	{"ZW", "IL", 10}, //Zimbabwe
#else
	{"AD", "GB", 0}, //Andorra
	{"AE", "KR", 24}, //UAE
	{"AF", "AF" , 0}, //Afghanistan
	{"AG", "US", 100}, //Antigua & Barbuda
	{"AI", "US", 100}, //Anguilla
	{"AL", "GB", 0}, //Albania
	{"AM", "IL", 10}, //Armenia
	{"AN", "BR", 0}, //Netherlands Antilles
	{"AO", "IL", 10}, //Angola
	{"AR", "BR", 0}, //Argentina
	{"AS", "US", 100}, //American Samoa (USA)
	{"AT", "GB", 0}, //Austria
	{"AU", "AU", 2}, //Australia
	{"AW", "KR", 24}, //Aruba
	{"AZ", "BR", 0}, //Azerbaijan
	{"BA", "GB", 0}, //Bosnia and Herzegovina
	{"BB", "RU", 1}, //Barbados
	{"BD", "CN", 0}, //Bangladesh
	{"BE", "GB" , 0}, //Belgium
	{"BF", "CN", 0}, //Burkina Faso
	{"BG", "GB", 0}, //Bulgaria
	{"BH", "RU", 1}, //Bahrain
	{"BI", "IL" , 10}, //Burundi
	{"BJ", "IL", 10}, //Benin
	{"BM", "US", 100}, //Bermuda
	{"BN", "RU", 1}, //Brunei
	{"BO", "IL", 10}, //Bolivia
	{"BR", "BR", 0}, //Brazil
	{"BS", "RU", 1}, //Bahamas
	{"BT", "IL", 10}, //Bhntan
	{"BW", "GB", 0}, //Botswana
	{"BY", "GB", 0}, //Belarus
	{"BZ", "IL" , 10}, //Belize
	{"CA", "US", 100}, //Canada
	{"CD", "IL", 10}, //Congo. Democratic Republic of the
	{"CF", "IL", 10}, //Central African Republic
	{"CG", "IL", 10}, //Congo. Republic of the
	{"CH", "GB", 0}, //Switzerland
	{"CI", "IL", 10}, //Cote d'lvoire
	{"CK", "BR", 0}, //Cook Island
	{"CL", "RU", 1}, //Chile
	{"CM", "IL", 10}, //Cameroon
	{"CN", "CN", 0}, //China
	{"CO", "BR", 0}, //Columbia
	{"CR", "BR", 0}, //Costa Rica
	{"CU", "BR", 0}, //Cuba
	{"CV", "GB", 0}, //Cape Verde
	{"CX", "AU", 2}, //Christmas Island (Australia)
	{"CY", "GB", 0}, //Cyprus
	{"CZ", "GB", 0}, //Czech
	{"DE", "GB", 0}, //Germany
	{"DJ", "IL", 10}, //Djibouti
	{"DK", "GB", 0}, //Denmark
	{"DM", "BR", 0}, //Dominica
	{"DO", "BR", 0}, //Dominican Republic
	{"DZ", "KW", 1}, //Algeria
	{"EC", "BR", 0}, //Ecuador
	{"EE", "GB", 0}, //Estonia
	{"EG", "RU", 1}, //Egypt
	{"ER", "IL", 10}, //Eritrea
	{"ES", "GB", 0}, //Spain
	{"ET", "GB", 0}, //Ethiopia
	{"FI", "GB", 0}, //Finland
	{"FJ", "IL", 10}, //Fiji
	{"FM", "US", 100}, //Federated States of Micronesia
	{"FO", "GB", 0}, //Faroe Island
	{"FR", "GB", 0}, //France
	{"GA", "IL", 10}, //Gabon
	{"GB", "GB", 0}, //United Kingdom
	{"GD", "BR", 0}, //Grenada
	{"GE", "GB", 0}, //Georgia
	{"GF", "GB", 0}, //French Guiana
	{"GH", "BR", 0}, //Ghana
	{"GI", "GB", 0}, //Gibraltar
	{"GM", "IL", 10}, //Gambia
	{"GN", "IL", 10}, //Guinea
	{"GP", "GB", 0}, //Guadeloupe
	{"GQ", "IL", 10}, //Equatorial Guinea
	{"GR", "GB", 0}, //Greece
	{"GT", "RU", 1}, //Guatemala
	{"GU", "US", 100}, //Guam
	{"GW", "IL", 10}, //Guinea-Bissau
	{"GY", "QA", 0}, //Guyana
	{"HK", "BR", 0}, //Hong Kong
	{"HN", "CN" , 0}, //Honduras
	{"HR", "GB", 0}, //Croatia
	{"HT", "RU", 1}, //Haiti
	{"HU", "GB", 0}, //Hungary
	{"ID", "QA", 0}, //Indonesia
	{"IE", "GB", 0}, //Ireland
	{"IL", "IL" , 10}, //Israel
	{"IM", "GB", 0}, //Isle of Man
	{"IN", "RU", 1}, //India
	{"IQ", "IL", 10}, //Iraq
	{"IR", "IL", 10}, //Iran
	{"IS", "GB", 0}, //Iceland
	{"IT", "GB", 0}, //Italy
	{"JE", "GB", 0}, //Jersey
	{"JM", "GB", 0}, //Jameica
	{"JO", "XY", 3}, //Jordan
	{"JP", "JP", 5}, //Japan
	{"KE", "GB", 0}, //Kenya
	{"KG", "IL", 10}, //Kyrgyzstan
	{"KH", "BR", 0}, //Cambodia
	{"KI", "AU", 2}, //Kiribati
	{"KM", "IL", 10}, //Comoros
	{"KP", "IL", 10}, //North Korea
	{"KR", "KR", 24}, //South Korea
	{"KW", "KW", 1}, //Kuwait
	{"KY", "US", 100}, //Cayman Islands
	{"KZ", "BR", 0}, //Kazakhstan
	{"LA", "KR", 24}, //Laos
	{"LB", "BR" , 0}, //Lebanon
	{"LC", "BR", 0}, //Saint Lucia
	{"LI", "GB", 0}, //Liechtenstein
	{"LK", "BR" , 0}, //Sri Lanka
	{"LR", "BR", 0}, //Liberia
	{"LS", "GB", 0}, //Lesotho
	{"LT", "GB", 0}, //Lithuania
	{"LU", "GB", 0}, //Luxemburg
	{"LV", "GB", 0}, //Latvia
	{"LY", "IL", 10}, //Libya
	{"MA", "KW", 1}, //Morocco
	{"MC", "GB", 0}, //Monaco
	{"MD", "GB", 0}, //Moldova
	{"ME", "GB", 0}, //Montenegro
	{"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
	{"MG", "IL" , 10}, //Madagascar
	{"MH", "BR", 0}, //Marshall Islands
	{"MK", "GB", 0}, //Macedonia
	{"ML", "IL" , 10}, //Mali
	{"MM", "IL", 10}, //Burma (Myanmar)
	{"MN", "IL", 10}, //Mongolia
	{"MO", "CN", 0}, //Macau
	{"MP", "US", 100}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
	{"MQ", "GB", 0}, //Martinique (France)
	{"MR", "GB", 0}, //Mauritania
	{"MS", "GB", 0}, //Montserrat (UK)
	{"MT", "GB", 0}, //Malta
	{"MU", "GB", 0}, //Mauritius
	{"MV", "RU", 1}, //Maldives
	{"MW", "CN", 0}, //Malawi
	{"MX", "RU", 1}, //Mexico
	{"MY", "RU", 1}, //Malaysia
	{"MZ", "BR", 0}, //Mozambique
	{"NA", "BR", 0}, //Namibia
	{"NC", "IL", 10}, //New Caledonia
	{"NE", "BR", 0}, //Niger
	{"NF", "BR", 0}, //Norfolk Island
	{"NG", "NG", 0}, //Nigeria
	{"NI", "BR" , 0}, //Nicaragua
	{"NL", "GB", 0}, //Netherlands
	{"NO", "GB", 0}, //Norway
	{"NP", "SA", 0}, //Nepal
	{"NR", "IL", 10}, //Nauru
	{"NU", "BR", 0}, //Niue
	{"NZ", "BR", 0 }, //New Zealand
	{"OM", "GB", 0}, //Oman
	{"PA", "RU", 1}, //Panama
	{"PE", "BR", 0}, //Peru
	{"PF", "GB", 0}, //French Polynesia (France)
	{"PG", "XY", 3}, //Papua New Guinea
	{"PH", "BR", 0}, //Philippines
	{"PK", "CN", 0}, //Pakistan
	{"PL", "GB", 0}, //Poland
	{"PM", "GB", 0}, //Saint Pierre and Miquelon
	{"PN", "GB", 0}, //Pitcairn Islands
	{"PR", "US", 100}, //Puerto Rico (USA)
	{"PS", "BR", 0}, //Palestinian Authority
	{"PT", "GB", 0}, //Portugal
	{"PW", "BR", 0}, //Palau
	{"PY", "BR", 0}, //Paraguay
	{"QA", "CN", 0}, //Qatar
	{"RE", "GB", 0}, //Reunion (France)
	{"RKS", "IL", 10}, //Kosvo (Added on window's list)
	{"RO", "GB", 0}, //Romania
	{"RS", "GB", 0}, //Serbia
	{"RU", "RU", 10}, //Russia
	{"RW", "CN", 0}, //Rwanda
	{"SA", "SA", 0}, //Saudi Arabia
	{"SB", "IL", 10}, //Solomon Islands
	{"SC", "IL", 10}, //Seychelles
	{"SD", "GB", 0}, //Sudan
	{"SE", "GB", 0}, //Sweden
	{"SG", "BR", 0}, //Singapole
	{"SI", "GB", 0}, //Slovenia
	{"SK", "GB", 0}, //Slovakia
	{"SKN", "CN", 0}, //Saint Kitts and Nevis
	{"SL", "IL", 10}, //Sierra Leone
	{"SM", "GB", 0}, //San Marino
	{"SN", "GB", 0}, //Senegal
	{"SO", "IL", 10}, //Somalia
	{"SR", "IL", 10}, //Suriname
	{"SS", "GB", 0}, //South_Sudan
	{"ST", "IL", 10}, //Sao Tome and Principe
	{"SV", "RU", 1}, //El Salvador
	{"SY", "BR", 0}, //Syria
	{"SZ", "IL", 10}, //Swaziland
	{"TC", "GB", 0}, //Turks and Caicos Islands (UK)
	{"TD", "IL", 10}, //Chad
	{"TF", "GB", 0}, //French Southern and Antarctic Lands)
	{"TG", "IL", 10}, //Togo
	{"TH", "BR", 0}, //Thailand
	{"TJ", "IL", 10}, //Tajikistan
	{"TL", "BR", 0}, //East Timor
	{"TM", "IL", 10}, //Turkmenistan
	{"TN", "KW", 1}, //Tunisia
	{"TO", "IL", 10}, //Tonga
	{"TR", "GB", 0}, //Turkey
	{"TT", "BR", 0}, //Trinidad and Tobago
	{"TV", "IL", 10}, //Tuvalu
	{"TW", "TW", 2}, //Taiwan
	{"TZ", "CN", 0}, //Tanzania
	{"UA", "RU", 1}, //Ukraine
	{"UG", "BR", 0}, //Ugnada
	{"US", "US", 100}, //US
	{"UY", "BR", 0}, //Uruguay
	{"UZ", "IL", 10}, //Uzbekistan
	{"VA", "GB", 0}, //Vatican (Holy See)
	{"VC", "BR", 0}, //Saint Vincent and the Grenadines
	{"VE", "RU", 1}, //Venezuela
	{"VG", "GB", 0}, //British Virgin Islands
	{"VI", "US", 100}, //US Virgin Islands
	{"VN", "BR", 0}, //Vietnam
	{"VU", "IL", 10}, //Vanuatu
	{"WS", "SA", 0}, //Samoa
	{"YE", "IL", 10}, //Yemen
	{"YT", "GB", 0}, //Mayotte (France)
	{"ZA", "GB", 0}, //South Africa
	{"ZM", "RU", 1}, //Zambia
	{"ZW", "BR", 0}, //Zimbabwe
#endif //                                                                 
#endif //                                                      
#endif /* EXMAPLE_TABLE */
};


/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(char *country_iso_code, wl_country_t *cspec)
{
//                                                    
#if defined(CUSTOMER_HW2) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) && !defined(CUSTOMER_HW10) 

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

			WL_TRACE(("%s: finally set country code : country_iso_code(%s), ccode(%s), rev(%d) \n"
				,__FUNCTION__, country_iso_code, cspec->ccode, cspec->rev));

			return;
		}
	}
//                                                                                     
#if 0
#ifdef EXAMPLE_TABLE
	/* if no country code matched return first universal code from translate_custom_table */
	memcpy(cspec->ccode, translate_custom_table[0].custom_locale, WLC_CNTRY_BUF_SZ);
	cspec->rev = translate_custom_table[0].custom_locale_rev;
#endif /* EXMAPLE_TABLE */
#endif
//                                                                                     
	return;
#endif /* defined(CUSTOMER_HW2) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)) */
}
#endif /* !CUSTOMER_HW4 || CUSTOMER_HW10 */
