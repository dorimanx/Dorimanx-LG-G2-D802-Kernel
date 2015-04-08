#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H

typedef enum {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_H,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
} hw_rev_type;

extern char *rev_str[];

hw_rev_type lge_get_board_revno(void);

#ifdef CONFIG_LGE_PM
typedef enum {
	NO_INIT_CABLE = 0,
	CABLE_MHL_1K,
	CABLE_U_28P7K,
	CABLE_28P7K,
	CABLE_56K,
	CABLE_100K,
	CABLE_130K,
	CABLE_180K,
	CABLE_200K,
	CABLE_220K,
	CABLE_270K,
	CABLE_330K,
	CABLE_620K,
	CABLE_910K,
	CABLE_NONE
} acc_cable_type;

struct chg_cable_info {
	acc_cable_type cable_type;
	unsigned ta_ma;
	unsigned usb_ma;
};

void get_cable_data_from_dt(void *of_node);
int lge_pm_get_cable_info(struct chg_cable_info *);
void lge_pm_read_cable_info(void);
acc_cable_type lge_pm_get_cable_type(void);
unsigned lge_pm_get_ta_current(void);
unsigned lge_pm_get_usb_current(void);
int lge_get_android_dlcomplete(void);

struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};
#endif

#ifdef CONFIG_LGE_SUPPORT_LCD_MAKER_ID
typedef enum {
	LCD_RENESAS_LGD = 0,
	LCD_RENESAS_JDI,
	LCD_MAKER_MAX,
} lcd_maker_id;

typedef struct {
	lcd_maker_id maker_id;
	int min_mvol;
	int max_mvol;
} lcd_vol_maker_tbl_type;

lcd_maker_id lge_get_panel_maker(void);
#endif

enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_FACTORY,
	LGE_BOOT_MODE_FACTORY2,
	LGE_BOOT_MODE_PIFBOOT,
	LGE_BOOT_MODE_PIFBOOT2,
	LGE_BOOT_MODE_MINIOS    /*                          */
};

int lge_get_factory_boot(void);
int get_lge_frst_status(void);

#ifdef CONFIG_MACH_MSM8974_G2_VZW
int lge_get_battery_low(void);
#endif

#if defined(CONFIG_LCD_KCAL)
/*             
                          
                                
*/
struct kcal_data {
		int red;
		int green;
		int blue;
};

struct kcal_platform_data {
	int (*set_values) (int r, int g, int b);
	int (*get_values) (int *r, int *g, int *b);
	int (*refresh_display) (void);
};
#endif /* CONFIG_LCD_KCAL */

enum lge_boot_mode_type lge_get_boot_mode(void);

enum lge_laf_mode_type {
	LGE_LAF_MODE_NORMAL = 0,
	LGE_LAF_MODE_LAF,
};

enum lge_laf_mode_type lge_get_laf_mode(void);

enum cn_prop_type {
	CELL_U32 = 0,
	CELL_U64,
	STRING,
};

int __init lge_init_dt_scan_chosen(unsigned long node, const char *uname,
								  int depth, void *data);

void get_dt_cn_prop(const char *name, void *value);
void get_dt_cn_prop_str(const char *name, char *value);
void get_dt_cn_prop_u64(const char *name, uint64_t *u64);
void get_dt_cn_prop_u32(const char *name, uint32_t *u32);

extern bool lge_get_cont_splash_enabled(void);

#define UART_MODE_ALWAYS_OFF_BMSK   BIT(0)
#define UART_MODE_ALWAYS_ON_BMSK    BIT(1)
#define UART_MODE_INIT_BMSK         BIT(2)
#define UART_MODE_EN_BMSK           BIT(3)

extern unsigned int lge_get_uart_mode(void);
extern void lge_set_uart_mode(unsigned int um);

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
#define LGE_PERSISTENT_RAM_SIZE (SZ_1M)
#endif

#if defined(CONFIG_ANDROID_RAM_CONSOLE)
#define LGE_RAM_CONSOLE_SIZE (128 * SZ_1K * 2)
#endif

void __init lge_reserve(void);
void __init lge_add_persistent_device(void);


#if defined(CONFIG_ANDROID_PERSISTENT_RAM)
void __init lge_add_persist_ram_devices(void);
#endif

#ifdef CONFIG_LGE_LCD_TUNING
void __init lge_add_lcd_misc_devices(void);
#endif

#if defined(CONFIG_LCD_KCAL)
/*             
                          
                                
*/
void __init lge_add_lcd_kcal_devices(void);
#endif
#ifdef CONFIG_LGE_QFPROM_INTERFACE
void __init lge_add_qfprom_devices(void);
#endif

#ifdef CONFIG_LGE_ECO_MODE
void __init lge_add_lge_kernel_devices(void);
#endif

#ifdef CONFIG_LGE_DIAG_ENABLE_SYSFS
void __init lge_add_diag_devices(void);
#endif

#if defined(CONFIG_LGE_PM) && defined(CONFIG_DEBUG_FS)
int gpio_debug_init(void);
void gpio_debug_print(void);
#else
static inline int gpio_debug_init(void) { return 0; }
static inline void gpio_debug_print(void) { return; }
#endif

extern int on_hidden_reset;

void xo_therm_logging(void);

#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
void __init lge_battery_id_devices(void);
#endif

#endif
