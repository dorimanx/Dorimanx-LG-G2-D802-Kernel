#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <asm/io.h>
#ifdef CONFIG_WIFI_CONTROL_FUNC
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif
#include <mach/board_lge.h> // add for hw revision check by hayun.kim
#include <linux/pm_qos.h>

#define WLAN_POWER    26
#define WLAN_HOSTWAKE 44

static int gpio_wlan_power = WLAN_POWER; // add for hw revision check by hayun.kim
static int gpio_wlan_hostwake = WLAN_HOSTWAKE; // add for hw revision check by hayun.kim

static unsigned wlan_wakes_msm[] = {
	    GPIO_CFG(WLAN_HOSTWAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };

/* for wifi power supply */
static unsigned wifi_config_power_on[] = {
	    GPIO_CFG(WLAN_POWER, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA) };

#if defined(CONFIG_BCM4335BT) 
extern int bcm_bt_lock(int cookie);
extern void bcm_bt_unlock(int cookie);
static int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24; /* cookie is "WiFi" */
#endif // defined(CONFIG_BCM4335BT) 

// For broadcom
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

#define LGE_BCM_WIFI_DMA_QOS_CONTROL

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
}
	wlan_static_scan_buf0 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf0)
		goto err_mem_alloc;
	wlan_static_scan_buf1 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf1)
		goto err_mem_alloc;

	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

static unsigned int g_wifi_detect;
static void *sdc2_dev;
void (*sdc2_status_cb)(int card_present, void *dev);

int sdc2_status_register(void (*cb)(int card_present, void *dev), void *dev)
{

	printk(KERN_ERR "%s: Dubugging Point 1\n", __func__);

	if(sdc2_status_cb) {
		return -EINVAL;
	}
	sdc2_status_cb = cb;
	sdc2_dev = dev;
	return 0;
}

unsigned int sdc2_status(struct device *dev)
{
	printk("J:%s> g_wifi_detect = %d\n", __func__, g_wifi_detect );
	return g_wifi_detect;
}

#ifdef LGE_BCM_WIFI_DMA_QOS_CONTROL
static int wifi_dma_state; // 0 : INATIVE, 1:INIT, 2:IDLE, 3:ACTIVE
static struct pm_qos_request wifi_dma_qos;
static struct delayed_work req_dma_work;
static uint32_t packet_transfer_cnt = 0;

static void bcm_wifi_req_dma_work(struct work_struct * work)
{
	switch ( wifi_dma_state ) {
		case 2: //IDLE State
			if ( packet_transfer_cnt < 100 ) {
				// IDLE -> INIT
				wifi_dma_state = 1;
				//printk(KERN_ERR "%s: schedule work : %d : (IDLE -> INIT)\n", __func__, packet_transfer_cnt);
			}
			else {
				// IDLE -> ACTIVE
				wifi_dma_state = 3;
				pm_qos_update_request(&wifi_dma_qos, 7);
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(50));
				//printk(KERN_ERR "%s: schedule work : %d : (IDLE -> ACTIVE)\n", __func__, packet_transfer_cnt);
			}
			break;

		case 3: //ACTIVE State
			if ( packet_transfer_cnt < 10 ) {
				// ACTIVE -> IDLE
				wifi_dma_state = 2;
				pm_qos_update_request(&wifi_dma_qos, PM_QOS_DEFAULT_VALUE);
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(1000));
				//printk(KERN_ERR "%s: schedule work : %d : (ACTIVE -> IDLE)\n", __func__, packet_transfer_cnt);
			}
			else {
				// Keep ACTIVE
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(50));
				//printk(KERN_ERR "%s: schedule work : %d :  (ACTIVE -> ACTIVE)\n", __func__, packet_transfer_cnt);
			}
			break;

		default:
			break;
		
	}

	packet_transfer_cnt = 0;
}

void bcm_wifi_req_dma_qos(int vote)
{
	if (vote) {
		packet_transfer_cnt++;
	}

	// INIT -> IDLE
	if ( wifi_dma_state == 1 && vote ) {
		wifi_dma_state = 2; // IDLE
		schedule_delayed_work(&req_dma_work, msecs_to_jiffies(1000));
		//printk(KERN_ERR "%s: schedule work (INIT -> IDLE)\n", __func__);
	}
}
#endif

int bcm_wifi_reinit_gpio( void )
{
	int rc=0;

	int hw_rev = HW_REV_A;

	// set gpio value
	hw_rev = lge_get_board_revno();		

	gpio_wlan_hostwake 	= 44;
	gpio_wlan_power 	= 26;

	printk(KERN_ERR "%s: rev=%d, gpio_power=%d, gpio_hostwakeup=%d \n", __func__, hw_rev, gpio_wlan_power, gpio_wlan_hostwake);

	// COMMON
	wlan_wakes_msm[0] = GPIO_CFG(gpio_wlan_hostwake, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
	wifi_config_power_on[0] = GPIO_CFG(gpio_wlan_power, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		
	//WLAN_POWER
	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	//HOST_WAKEUP
	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);

	if (gpio_direction_input(gpio_wlan_hostwake)) 
		printk(KERN_ERR "%s: WL_HOSTWAKE failed direction in\n", __func__);

	return 0;
}

int bcm_wifi_set_power(int enable)
{
	int ret = 0;
	static int is_initialized = 0;

	if (is_initialized == 0) {
		bcm_wifi_reinit_gpio();
		mdelay(10);
		is_initialized = 1;
	}
		
#if defined(CONFIG_BCM4335BT) 
	printk("%s: trying to acquire BT lock\n", __func__);
	if (bcm_bt_lock(lock_cookie_wifi) != 0)
		printk("%s:** WiFi: timeout in acquiring bt lock**\n", __func__);
	else 
		printk("%s: btlock acquired\n", __func__);
#endif // defined(CONFIG_BCM4335BT) 

	if (enable)
	{
		ret = gpio_direction_output(gpio_wlan_power, 1); 
		if (ret) 
		{
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}

		// WLAN chip to reset
		mdelay(150); //for booting time save
		printk("J:%s: applied delay. 150ms\n",__func__);
		printk(KERN_ERR "%s: wifi power successed to pull up\n",__func__);

	}
	else{
		ret = gpio_direction_output(gpio_wlan_power, 0); 
		if (ret) 
		{
			printk(KERN_ERR "%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}

		// WLAN chip down 
		mdelay(100);//for booting time save
		printk(KERN_ERR "%s: wifi power successed to pull down\n",__func__);
	}

#if defined(CONFIG_BCM4335BT) 
	bcm_bt_unlock(lock_cookie_wifi);
#endif // defined(CONFIG_BCM4335BT) 

	return ret;

out : 
#if defined(CONFIG_BCM4335BT) 
	/* For a exceptional case, release btlock */
	printk("%s: exceptional bt_unlock\n", __func__);
	bcm_bt_unlock(lock_cookie_wifi);
#endif // defined(CONFIG_BCM4335BT) 

	return ret;
}

int __init bcm_wifi_init_gpio_mem( struct platform_device* platdev )
{
	int rc=0;

// add for hw revision check by hayun.kim, START
	int hw_rev = HW_REV_A;

	hw_rev = lge_get_board_revno();		
	
	gpio_wlan_hostwake 	= 44;
	gpio_wlan_power 	= 26;

	printk(KERN_ERR "%s: rev=%d, gpio_power=%d, gpio_hostwakeup=%d \n", __func__, hw_rev, gpio_wlan_power, gpio_wlan_hostwake);

	// COMMON
	wlan_wakes_msm[0] = GPIO_CFG(gpio_wlan_hostwake, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
	wifi_config_power_on[0] = GPIO_CFG(gpio_wlan_power, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
// add for hw revision check by hayun.kim, END
		
	//WLAN_POWER
	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	if (gpio_request(gpio_wlan_power, "WL_REG_ON"))		
		printk("Failed to request gpio %d for WL_REG_ON\n", gpio_wlan_power);	

	if (gpio_direction_output(gpio_wlan_power, 0)) 
		printk(KERN_ERR "%s: WL_REG_ON  failed direction out\n", __func__);

	//HOST_WAKEUP
	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);
	if (gpio_request(gpio_wlan_hostwake, "wlan_wakes_msm"))		
		printk("Failed to request gpio %d for wlan_wakes_msm\n", gpio_wlan_hostwake);			
	if (gpio_direction_input(gpio_wlan_hostwake)) 
		printk(KERN_ERR "%s: WL_HOSTWAKE failed direction in\n", __func__);


	//For MSM8974_S
	if( platdev != NULL )
	{
		struct resource* resource = platdev->resource;
		if( resource != NULL )
		{
			resource->start = gpio_to_irq( gpio_wlan_hostwake );
			resource->end = gpio_to_irq( gpio_wlan_hostwake ); 

			printk("J:%s> resource->start = %d\n", __func__, resource->start );
		}
	}
	//For MSM8974_E

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	printk("bcm_wifi_init_gpio_mem successfully \n");

	return 0;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;
	if(sdc2_status_cb)
		sdc2_status_cb(val, sdc2_dev);
	else
		printk("%s:There is no callback for notify\n", __FUNCTION__);
	return 0;
}

static int bcm_wifi_get_mac_addr(unsigned char* buf)
{
	uint rand_mac;
	static unsigned char mymac[6] = {0,};
	const unsigned char nullmac[6] = {0,};
	pr_debug("%s: %p\n", __func__, buf);

	if( buf == NULL ) return -EAGAIN;

	if( memcmp( mymac, nullmac, 6 ) != 0 )
	{
		/* Mac displayed from UI are never updated..
		   So, mac obtained on initial time is used */
		memcpy( buf, mymac, 6 );
		return 0;
	}

	srandom32((uint)jiffies);
	rand_mac = random32();
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy( mymac, buf, 6 );

	printk("[%s] Exiting. MyMac :  %x : %x : %x : %x : %x : %x \n",__func__ , buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

	return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

#if 0
/* Customized Locale table */
const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
        {"",   "XY", 4}, /* Universal if Country code is unknown or empty */
        {"AD", "GB", 0}, //Andorra
        {"AE", "KR", 24}, //UAE
        {"AF", "AF", 0}, //Afghanistan
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
        {"BE", "GB", 0}, //Belgium
        {"BF", "CN", 0}, //Burkina Faso
        {"BG", "GB", 0}, //Bulgaria
        {"BH", "RU", 1}, //Bahrain
        {"BI", "IL", 10}, //Burundi
        {"BJ", "IL", 10}, //Benin
        {"BM", "US", 100}, //Bermuda
        {"BN", "RU", 1}, //Brunei
        {"BO", "IL", 10}, //Bolivia
        {"BR", "BR", 0}, //Brazil
        {"BS", "RU", 1}, //Bahamas
        {"BT", "IL", 10}, //Bhntan
        {"BW", "GB", 0}, //Botswana
        {"BY", "GB", 0}, //Belarus
        {"BZ", "IL", 10}, //Belize
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
        {"HN", "CN", 0}, //Honduras
        {"HR", "GB", 0}, //Croatia
        {"HT", "RU", 1}, //Haiti
        {"HU", "GB", 0}, //Hungary
        {"ID", "QA", 0}, //Indonesia
        {"IE", "GB", 0}, //Ireland
        {"IL", "IL", 10}, //Israel
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
        {"LB", "BR", 0}, //Lebanon
        {"LC", "BR", 0}, //Saint Lucia
        {"LI", "GB", 0}, //Liechtenstein
        {"LK", "BR", 0}, //Sri Lanka
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
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
        {"MM", "IL", 10}, //Burma (Myanmar)
        {"MN", "IL", 10}, //Mongolia
        {"MO", "CN", 0}, //Macau
        {"MP", "US", 100}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
        {"MQ", "GB", 0}, //Martinique (France)
        {"MR", "GB", 0}, //Mauritania
        {"MS", "GB", 0}, //Montserrat (UK)
        {"MT", "GB", 0}, //Malta
        {"MU", "GB", 0}, //Mauritius
        {"MD", "GB", 0}, //Moldova
        {"ME", "GB", 0}, //Montenegro
        {"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
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
        {"NI", "BR", 0}, //Nicaragua
        {"NL", "GB", 0}, //Netherlands
        {"NO", "GB", 0}, //Norway
        {"NP", "SA", 0}, //Nepal
        {"NR", "IL", 10}, //Nauru
        {"NU", "BR", 0}, //Niue
        {"NZ", "BR", 0}, //New Zealand
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
};

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;

	size = ARRAY_SIZE(bcm_wifi_translate_custom_table);

	if (size == 0)
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table[i].iso_abbrev) == 0) {
			return (void *)&bcm_wifi_translate_custom_table[i];
		}
	}   

	/* if no country code matched return first universal code from bcm_wifi_translate_custom_table */
	return (void *)&bcm_wifi_translate_custom_table[0];
}
#endif

static struct wifi_platform_data bcm_wifi_control = {
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, 
//	.get_country_code = bcm_wifi_get_country_code,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  //assigned later
		.end   = 0,  //assigned later
		//.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, // for HW_OOB
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_SHAREABLE, // for SW_OOB
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};

void __init init_bcm_wifi(void)
{
#ifdef CONFIG_WIFI_CONTROL_FUNC

#ifdef LGE_BCM_WIFI_DMA_QOS_CONTROL
	INIT_DELAYED_WORK(&req_dma_work, bcm_wifi_req_dma_work);
	pm_qos_add_request(&wifi_dma_qos, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	wifi_dma_state = 1; //INIT
	printk("%s: wifi_dma_qos is added\n", __func__);
#endif

	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);
#endif
}

