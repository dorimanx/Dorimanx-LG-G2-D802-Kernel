#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

#include <linux/of_gpio.h>
#include <linux/i2c/smb349_charger.h>

#include "ftt_ctrl_comm.h"
#include "ftt_status.h"
#include "ftt_charger_v3.h"

extern bool wireless_online(int gpio);
extern bool on_change_level(int level);
extern int get_ftt_gpio(int gpio);

extern struct platform_device ftt_charger_device;
#ifdef CONFIG_FTT_SYSFS
extern struct attribute_group ftt_sysfs_attr_group;
#endif /* CONFIG_FTT_SYSFS */

#if FTT_UEVENT
extern void ftt_uevent_init(struct ftt_charger_device *ftt_pdev);
#endif /* FTT_UEVENT */

#if FTT_CHARACTER_DEVICE
extern bool ftt_send_cmd(struct ftt_charger_device *ftt_pdev, enum FTT_READ_COMMAND cmd, u32 frequency);
extern bool ftt_send_cmd_interrupt(struct ftt_charger_device *ftt_pdev, u32 frequency);
extern bool ftt_send_cmd_interrupt_ping(struct ftt_charger_device *ftt_pdev, u16 *frequency_table);
extern bool ftt_send_cmd_interrupt_online(struct ftt_charger_device *ftt_pdev, u16 frequency);
extern bool ftt_send_cmd_suspend(struct ftt_charger_device *ftt_pdev, u32 frequency);
extern bool ftt_send_cmd_init(struct ftt_charger_device *ftt_pdev);
extern const struct file_operations ftt_fops;
#endif /* FTT_CHARACTER_DEVICE */


static u32 ftt_status_interrupt_func_enter(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_interrupt_func(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_interrupt_func_leave(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_ping_detect_func_enter(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_ping_detect_func(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_ping_detect_func_leave(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_pre_charging_func_enter(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_pre_charging_func(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_pre_charging_func_leave(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_charging_func_enter(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_charging_func(struct ftt_charger_device *ftt_pdev);
static u32 ftt_status_charging_func_leave(struct ftt_charger_device *ftt_pdev);

struct ftt_status_struct ftt_status_fn[] = {
	/* FTT_CHARGER_STATUS_INTERRUPT */
	{ftt_status_interrupt_func_enter, ftt_status_interrupt_func, ftt_status_interrupt_func_leave},
	/* FTT_CHARGER_STATUS_PING_DETECT */
	{ftt_status_ping_detect_func_enter, ftt_status_ping_detect_func, ftt_status_ping_detect_func_leave},
	/* FTT_CHARGER_STATUS_PRE_CHARGING */
	{ftt_status_pre_charging_func_enter, ftt_status_pre_charging_func, ftt_status_pre_charging_func_leave},
	/* FTT_CHARGER_STATUS_CHARGING */
	{ftt_status_charging_func_enter, ftt_status_charging_func, ftt_status_charging_func_leave},
	{NULL, NULL, NULL},
};

/****   define variable  *****/
unsigned int ftt_is_debug = FTT_LOG_LEVEL_ALWAYS;


/****   define function  *****/
#ifdef	CONFIG_HAS_EARLYSUSPEND
static	void	ftt_suspend			(struct early_suspend *h);
static	void	ftt_resume			(struct early_suspend *h);
#endif
void set_ftt_charger_status_interrupt(struct ftt_charger_device *ftt_pdev);
bool is_ftt_online(struct ftt_charger_device *ftt_pdev);
enum ftt_charger_status get_ftt_charger_current_status(struct ftt_charger_device *ftt_pdev);


/****   code  *****/
#if FTT_STATISTICS_DEBUG
static void ftt_detect_pad_statistics(struct ftt_charger_device *ftt_pdev)
{
	u32 count;

	ftt_pdev->ftt_detect_pad_statistics_count[ftt_pdev->detect_pad]++;
	ftt_pdev->ftt_detect_pad_statistics_total++;
	DPRINT(FTT_LOG_PAD_DETECT_TEST, "####################### STATISTICS #######################\n");
	DPRINT(FTT_LOG_PAD_DETECT_TEST, "STATISTICS : ");
	for (count = 1;count < FTT_PAD_TYPE_MAX;count++) {
		if (ftt_pdev->ftt_detect_pad_statistics_count[count] != 0) {
			DPRINTC(FTT_LOG_PAD_DETECT_TEST, "%d=%u(%u%%), ", count,
					ftt_pdev->ftt_detect_pad_statistics_count[count],
					ftt_pdev->ftt_detect_pad_statistics_count[count]*100/ftt_pdev->ftt_detect_pad_statistics_total);
		}
	}
	DPRINTC(FTT_LOG_PAD_DETECT_TEST, "total=%u\n", ftt_pdev->ftt_detect_pad_statistics_total);
	DPRINT(FTT_LOG_PAD_DETECT_TEST, "##########################################################\n");
}
#endif /* FTT_STATISTICS_DEBUG */

static u32 get_ftt_freqb_to_freqk(u32 frequency)
{
#if FTT_FREQUENCY_ROUND
	return frequency/1000 + (((frequency%1000) >= 500)?1:0);
#else /* FTT_FREQUENCY_ROUND */
	return frequency/1000;
#endif /* FTT_FREQUENCY_ROUND */
}

#if FTT_FREQ_CORRECTION_TABLE
static s32 ftt_frequency_correction(struct ftt_charger_device *ftt_pdev, u32 frequency)
{
	u32 i;
	u32 table_size;
	u32 ftt_freq;
	u32 correction_freq = 0;

	table_size = ftt_pdev->ftt_freq_correction_table_size;
	for (i=0;i<table_size;i++) {
		ftt_freq = ftt_pdev->ftt_freq_correction[i].correction_freq;
		if (correction_freq <= frequency) {
			correction_freq = ftt_pdev->ant_level_type_table[i].ant_level;
			break;
		}
		else {
			correction_freq = 0;
		}
	}
	return correction_freq;

}
#endif /* FTT_FREQ_CORRECTION_TABLE */

static u32 get_ftt_frequency_poll(struct ftt_charger_device *ftt_pdev)
{
	register u32 count_pulse = 0;
	register u8 prev, curr = 0;
	register u32 count = 0;
	u32 freq=0;
	u32 rest=0;
	u64 s_time=0, e_time=0;
	u32 d_time=0;
#if FTT_FREQ_CORRECTION
	u32 sampling_per_freq = 0;
	u32 sampling_per_time = 0;
	u32 pre_pulse = 0;
	u32 post_pulse = 0;
	u32 resent_sampling = 0;
	u32 resent_time = 0;
#endif /* FTT_FREQ_CORRECTION */
	unsigned long ftt_frequency_lock_flags;
	u32 smp_cnt=0;

	spin_lock_irqsave(&ftt_pdev->ftt_frequency_lock, ftt_frequency_lock_flags);

	smp_cnt = ftt_pdev->fct.ftt_count[FTT_COUNT_FREQUENCY_SAMPLE_COUNT];
	s_time = ktime_to_ns(ktime_get());
	for (count=0;count<smp_cnt;count++) {
		prev = curr;
		curr = gpio_get_value(ftt_pdev->ftt_gpio);
#if FTT_FREQ_CORRECTION
			post_pulse++;
#endif /* FTT_FREQ_CORRECTION */
		if (curr  != prev) {
			count_pulse++;
#if FTT_FREQ_CORRECTION
			post_pulse = 0;
#endif /* FTT_FREQ_CORRECTION */
		}
#if FTT_FREQ_CORRECTION
		if (!count_pulse) pre_pulse++;
#endif /* FTT_FREQ_CORRECTION */
	}
	e_time = ktime_to_ns(ktime_get());
	d_time = e_time - s_time;

#if FTT_FREQ_CORRECTION
	if (count_pulse/2 >= 1) {
		sampling_per_freq = (smp_cnt - pre_pulse - post_pulse) / count_pulse;
		sampling_per_time = d_time / smp_cnt;
		resent_sampling = (count_pulse % 2) * sampling_per_freq + pre_pulse + post_pulse;
		resent_time = resent_sampling * sampling_per_time;
		d_time -= resent_time;
	}
#endif /* FTT_FREQ_CORRECTION */

	count_pulse = count_pulse / 2;

	if (d_time) {
		freq = (u32)(FTT_1_SEC / d_time * count_pulse);
		rest = (FTT_1_SEC % d_time) * count_pulse / d_time;
		freq += rest;
	}

#if FTT_FREQ_CORRECTION_TABLE
	freq += ftt_frequency_correction(ftt_pdev, freq);
#endif /* FTT_FREQ_CORRECTION_TABLE */

	DPRINT(FTT_LOG_SAMPLING, "FREQ smpl=%5uc/ms cnt= %3u delta=%4u.%03uus %3ukHz %6uHz rs=%2u cor=%d\n",(1000*1000*smp_cnt)/d_time, count_pulse, d_time/1000, d_time%1000, get_ftt_freqb_to_freqk(freq), freq, resent_sampling, ftt_frequency_correction(ftt_pdev, freq));

	spin_unlock_irqrestore(&ftt_pdev->ftt_frequency_lock, ftt_frequency_lock_flags);
	return freq;
}

u32 get_ftt_frequency_poll_ex(struct ftt_charger_device *ftt_pdev)
{
	u32 max_freq = 0;
	u32 ret_freq;
	u32 i;

	for(i=0;i<ftt_pdev->fct.ftt_count[FTT_COUNT_FREQUENCY_COMPARE_COUNT];i++) {
		ret_freq = get_ftt_frequency_poll(ftt_pdev);
		if (max_freq < ret_freq) max_freq = ret_freq;
	}

	return max_freq;
}

static u32 get_ftt_ping_frequency(struct ftt_charger_device *ftt_pdev)
{
	u32 ftt_ping_frequency = 0;
	u32 i;

	for(i=0;i<ftt_pdev->fct.ftt_count[FTT_COUNT_PING_POLLING_COUNT];i++) {
		ftt_ping_frequency = get_ftt_frequency_poll_ex(ftt_pdev);
		if (ftt_ping_frequency != 0) break;
	}

	ftt_pdev->ftt_frequency = ftt_ping_frequency;
	return ftt_ping_frequency;
}

bool is_ftt_online(struct ftt_charger_device *ftt_pdev)
{
	struct ftt_charger_pdata *pdata = ftt_pdev->pdev->dev.platform_data;

	if (pdata->online_fn) {
		ftt_pdev->ftt_online_prev = ftt_pdev->ftt_online;
		ftt_pdev->ftt_online = pdata->online_fn(ftt_pdev->active_n_gpio);
		if (ftt_pdev->ftt_online != ftt_pdev->ftt_online_prev) {
			ftt_pdev->ftt_online_prev = ftt_pdev->ftt_online;
#ifdef CONFIG_MACH_LGE
			set_wireless_power_supply_control((int)ftt_pdev->ftt_online);
			printk("[FTT] ftt_online : %d\n", ftt_pdev->ftt_online);
#endif/*               */
		}
		return ftt_pdev->ftt_online;
	}

	DPRINT(FTT_ERROR, "ERROR online_fn is NULL\n");
	return 0;
}

bool is_ftt_charging(struct ftt_charger_device *ftt_pdev)
{
	if ((is_ftt_online(ftt_pdev) == true) && ((get_ftt_charger_current_status(ftt_pdev) == FTT_CHARGER_STATUS_CHARGING)
		|| (get_ftt_charger_current_status(ftt_pdev) == FTT_CHARGER_STATUS_PRE_CHARGING)
		)) {
			return 1;
	}
	return 0;
}

static s32 get_ftt_saved_pad_type_num(struct ftt_charger_device *ftt_pdev)
{
	switch (get_ftt_charger_current_status(ftt_pdev)) {
	case	FTT_CHARGER_STATUS_PRE_CHARGING :
	case	FTT_CHARGER_STATUS_CHARGING :
		return ftt_pdev->detect_pad;
		break;
	default :
		return 0;
	}
	return 0;
}

const char *get_ftt_saved_pad_type_str(struct ftt_charger_device *ftt_pdev)
{
	switch (get_ftt_charger_current_status(ftt_pdev)) {
	case	FTT_CHARGER_STATUS_PRE_CHARGING :
	case	FTT_CHARGER_STATUS_CHARGING :
		return ftt_pdev->detect_pad_name;
		break;
	default :
		return 0;
	}
	return 0;
}

static int get_ftt_anntena_level_freq(struct ftt_charger_device *ftt_pdev, u32 frequency)
{
	struct ftt_charger_pdata *pdata = ftt_pdev->pdev->dev.platform_data;

	int level = FTT_BATT_CHARGING_WARRNING;
	s32 ftt_frequency_kb;
	s32 ftt_table_freq_kb;
	u32 table_size;
	u32 i;
	s32 pad_type;

	if (is_ftt_charging(ftt_pdev)) {
		ftt_frequency_kb = get_ftt_freqb_to_freqk(frequency);
		pad_type = get_ftt_saved_pad_type_num(ftt_pdev);

		if (pad_type > 0) {
			table_size = ftt_pdev->ant_level_type_table_size;
			for (i=0;i<table_size;i++) {
				if (ftt_pdev->ant_level_type_table[i].ant_level > ftt_pdev->prev_ant_level) {
					ftt_table_freq_kb = ftt_pdev->ant_level_type_table[i].ping_freq + ftt_pdev->ftt_hysteresis;
				}
				else {
					ftt_table_freq_kb = ftt_pdev->ant_level_type_table[i].ping_freq - ftt_pdev->ftt_hysteresis;
				}
				if (ftt_table_freq_kb <= ftt_frequency_kb) {
					level = ftt_pdev->ant_level_type_table[i].ant_level;
					break;
				}
				else {
					level = FTT_BATT_CHARGING_WARRNING;
				}
			}
		}
		else {
			level = FTT_BATT_CHARGING_NO_DEFINED_TYPE;
		}
		DPRINT(FTT_DEBUG, "FTT_Frequency=%7uHz(%4ukHz) PAD_type=%4s Level=%2d\n", frequency, ftt_frequency_kb, ftt_pdev->detect_pad_name, level);
		DPRINT(FTT_LOG_LEVEL, "FTT_Frequency=%7uHz(%4ukHz) PAD_type=%4s Level=%2d\n", frequency, ftt_frequency_kb, ftt_pdev->detect_pad_name, level);
	}
	else {
		level = FTT_BATT_CHARGING_NO_CHARGING;
	}

	ftt_pdev->ant_level = level;
	if (ftt_pdev->prev_ant_level != ftt_pdev->ant_level) {
		ftt_pdev->prev_ant_level = ftt_pdev->ant_level;
#if FTT_UEVENT
		if (ftt_pdev->ftt_supply.name) power_supply_changed(&ftt_pdev->ftt_supply);
#endif /* FTT_UEVENT */
		if (pdata->on_change_level_fn) {
			pdata->on_change_level_fn(level);
		}
	}
	return level;
}

static int get_ftt_anntena_level(struct ftt_charger_device *ftt_pdev)
{
	return get_ftt_anntena_level_freq(ftt_pdev, get_ftt_frequency_poll_ex(ftt_pdev));
}

static void ftt_interrupt_enable(struct ftt_charger_device *ftt_pdev)
{
	if (ftt_pdev->ftt_interrupt_enable_flag == false) {
		ftt_pdev->ftt_interrupt_enable_flag = true;
		enable_irq(ftt_pdev->ftt_irq);
	}

}

static void ftt_interrupt_disable(struct ftt_charger_device *ftt_pdev)
{
	if (ftt_pdev->ftt_interrupt_enable_flag) {
		ftt_pdev->ftt_interrupt_enable_flag = false;
		 disable_irq_nosync(ftt_pdev->ftt_irq);
	}

}

enum ftt_charger_status get_ftt_charger_current_status(struct ftt_charger_device *ftt_pdev)
{
	return ftt_pdev->ftt_status_d.ftt_status;
}

static void set_ftt_charger_status(struct ftt_charger_device *ftt_pdev, enum ftt_charger_status next_status)
{
	ftt_pdev->ftt_status_d.ftt_next_status = next_status;
	ftt_pdev->ftt_set_status = true;

	switch (ftt_pdev->ftt_status_d.ftt_next_status) {
	case FTT_CHARGER_STATUS_PING_DETECT :
		if (ftt_pdev->ftt_status_d.ftt_status == ftt_pdev->ftt_status_d.ftt_next_status) {
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET];
		}
		else {
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET_ENTER];
		}
		if (ftt_is_debug == FTT_LOG_PING_TEST) {
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET];
		}
		break;
	case FTT_CHARGER_STATUS_PRE_CHARGING :
		ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_PRECHG];
		break;
	case FTT_CHARGER_STATUS_CHARGING :
		if (ftt_pdev->ftt_status_d.ftt_status == ftt_pdev->ftt_status_d.ftt_next_status) {
#ifdef	CONFIG_HAS_EARLYSUSPEND
			if (ftt_pdev->ftt_earlysuspend) {
				ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_CHG_SUSPEND];
			}
			else {
				ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_CHG];
			}
#else /* CONFIG_HAS_EARLYSUSPEND */
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_CHG];
#endif /* CONFIG_HAS_EARLYSUSPEND */
		}
		else {
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_CHG_ENTER];
		}
		if (ftt_is_debug == FTT_LOG_PING_TEST) {
			ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET];
		}
		break;
	default :
		ftt_pdev->ftt_status_d.next_timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_INIT];
		break;
	}
	return;
}

static u32 get_ftt_charger_next_time(struct ftt_charger_device *ftt_pdev)
{
	return ftt_pdev->ftt_status_d.next_timer;
}

static void set_ftt_charger_reset_status(struct ftt_charger_device *ftt_pdev)
{
	ftt_pdev->ftt_status_d.ftt_prev_status = FTT_CHARGER_STATUS_PING_DETECT;
	ftt_pdev->ftt_status_d.ftt_status = FTT_CHARGER_STATUS_INTERRUPT;
	ftt_pdev->ftt_status_d.ftt_next_status = FTT_CHARGER_STATUS_PING_DETECT;
	ftt_pdev->ftt_status_d.next_timer = 0;
}

static void set_ftt_charger_reset_data(struct ftt_charger_device *ftt_pdev)
{
	ftt_pdev->ant_level = FTT_BATT_CHARGING_NO_CHARGING;
	ftt_pdev->ftt_frequency = 0;
}

static void set_ftt_charger_save_status(struct ftt_charger_device *ftt_pdev)
{
	ftt_pdev->ftt_status_d.ftt_prev_status = ftt_pdev->ftt_status_d.ftt_status;
	ftt_pdev->ftt_status_d.ftt_status = ftt_pdev->ftt_status_d.ftt_next_status;

	if (ftt_pdev->ftt_status_d.ftt_status == FTT_CHARGER_STATUS_INTERRUPT) {
		set_ftt_charger_status_interrupt(ftt_pdev);
	}
	else {
		if (get_ftt_charger_next_time(ftt_pdev) != 0) {
			mod_timer(&ftt_pdev->ftt_charger_status_timer, jiffies + msecs_to_jiffies(get_ftt_charger_next_time(ftt_pdev)));
		}
	}

	return;
}

/*======================== START STATUS FUCTION ===============================*/
static u32 ftt_status_interrupt_func_enter(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_interrupt_func_enter ###############\n");

	return 0;
}

static u32 ftt_status_interrupt_func(struct ftt_charger_device *ftt_pdev)
{

	DPRINT(FTT_VERBOSE3, "########## ftt_status_interrupt_func #####################\n");

	return 0;
}

static u32 ftt_status_interrupt_func_leave(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_interrupt_func_leave ###############\n");

	return 0;
}

static u32 ftt_status_ping_detect_func_enter(struct ftt_charger_device *ftt_pdev)
{
	int i;

	DPRINT(FTT_DEBUG, "########## ftt_status_ping_detect_func_enter #############\n");

	ftt_pdev->detect_count = 0;
	ftt_pdev->detect_pad = 0;
	ftt_pdev->ftt_hysteresis = 0;
	for (i=0;i<FTT_DETECT_SAMPLE_COUNT;i++) {
		ftt_pdev->ftt_ping_freq_tbl[i] = 0;
	}
	memset (ftt_pdev->detect_pad_name, 0, FTT_READ_CMD_PUT_PAD_STRING_SIZE + 1);
	memset(ftt_pdev->ant_level_type_table, 0, sizeof(ftt_pdev->ant_level_type_table));
	ftt_pdev->ant_level_type_table_size = 0;

	return 0;
}

static u32 ftt_status_ping_detect_func(struct ftt_charger_device *ftt_pdev)
{
	u32 ftt_freq;

	if (ftt_is_debug == FTT_LOG_PING_TEST) {
		if (is_ftt_online(ftt_pdev)) {
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_CHARGING);
		}
		else {
			ftt_freq = get_ftt_ping_frequency(ftt_pdev);
			if ((ftt_freq == 0) && (ftt_pdev->detect_count < 10)) {
				set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
			}
			else {
				DPRINT(FTT_LOG_PING_TEST, "FTT_Frequency=%6uHz(%3ukHz) online=%d\n", ftt_freq, get_ftt_freqb_to_freqk(ftt_freq), is_ftt_online(ftt_pdev));
				ftt_pdev->detect_count++;
				if (ftt_pdev->detect_count >= (3000 / ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET])) {
					set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_CHARGING);
				}
				else{
					set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PING_DETECT);
				}
			}
		}
		return 0;
	}
	DPRINT(FTT_VERBOSE3, "########## ftt_status_ping_detect_func ###################\n");
	if (!is_ftt_online(ftt_pdev)) {
		ftt_pdev->ftt_ping_freq_tbl[ftt_pdev->detect_count] = get_ftt_freqb_to_freqk(get_ftt_ping_frequency(ftt_pdev));

		if ((ftt_pdev->ftt_ping_freq_tbl[ftt_pdev->detect_count]) && (ftt_pdev->detect_count < FTT_DETECT_SAMPLE_COUNT)) {
			ftt_pdev->detect_count++;
			if (ftt_pdev->detect_count >= FTT_DETECT_SAMPLE_COUNT) {
				ftt_send_cmd_interrupt_ping(ftt_pdev, ftt_pdev->ftt_ping_freq_tbl);
				set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PRE_CHARGING);
			}
			else {
				set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PING_DETECT);
			}
		}
		else {
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
		}
	}
	else {
		ftt_send_cmd_interrupt_online(ftt_pdev, get_ftt_freqb_to_freqk(get_ftt_ping_frequency(ftt_pdev)));
		set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_CHARGING);
	}

	return 0;
}

static u32 ftt_status_ping_detect_func_leave(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_ping_detect_func_leave #############\n");

	return 0;
}

static u32 ftt_status_pre_charging_func_enter(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_pre_charging_func_enter ############\n");
	ftt_pdev->pre_charging_count = 0;

	return 0;
}

static u32 ftt_status_pre_charging_func(struct ftt_charger_device *ftt_pdev)
{
	u32 ftt_ping_freq;

	DPRINT(FTT_VERBOSE3, "########## ftt_status_pre_charging_func ##################\n");

	if (ftt_pdev->detect_pad > 0) {
		if (!is_ftt_online(ftt_pdev)) {
			ftt_ping_freq = get_ftt_ping_frequency(ftt_pdev);
			if (ftt_pdev->pre_charging_count >= ((ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_PRECHG_TOTAL]/ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_PRECHG]))) {
				set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
			}
			else {
				if (ftt_ping_freq != 0) {
					ftt_pdev->pre_charging_count++;
					set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PRE_CHARGING);
				}
				else {
					DPRINT(FTT_WARN, "########## ftt_status_pre_charging_func : no frequency ###\n");
					set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
				}
			}
		}
		else {
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_CHARGING);
		}
	}
	else {
		DPRINT(FTT_ASSERT, "ftt_status_pre_charging_func : Can not detect pad type - goto detect charging pad type\n");
		set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PING_DETECT);
	}

	return 0;
}

static u32 ftt_status_pre_charging_func_leave(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_pre_charging_func_leave ############\n");

	return 0;
}

static u32 ftt_status_charging_func_enter(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_charging_func_enter ################\n");
#if FTT_STATISTICS_DEBUG
	if (ftt_is_debug == FTT_LOG_PAD_DETECT_TEST) {
		ftt_detect_pad_statistics(ftt_pdev);
	}
#endif /* FTT_STATISTICS_DEBUG */

	ftt_pdev->ftt_frequency = 0;

	return 0;
}

static u32 ftt_status_charging_func(struct ftt_charger_device *ftt_pdev)
{
	u32 ftt_freq;
	s8 ftt_level;
	bool online;

	DPRINT(FTT_VERBOSE3, "########## ftt_status_charging_func ######################\n");

	if (ftt_is_debug == FTT_LOG_PING_TEST) {
		if (is_ftt_online(ftt_pdev)) {
			ftt_freq = get_ftt_ping_frequency(ftt_pdev);
			DPRINT(FTT_LOG_PING_TEST, "FTT_Frequency=%6uHz(%3ukHz) online=%d\n", ftt_freq, get_ftt_freqb_to_freqk(ftt_freq), is_ftt_online(ftt_pdev));
		}
		else {
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
		}
		return 0;
	}
	online = is_ftt_online(ftt_pdev);
	if (online) {
		if (ftt_pdev->detect_pad > 0) {
			ftt_pdev->ftt_frequency = ftt_freq = get_ftt_ping_frequency(ftt_pdev);
			ftt_level = get_ftt_anntena_level_freq(ftt_pdev, ftt_freq);
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_CHARGING);
		}
		else {
			DPRINT(FTT_ASSERT, "ftt_status_charging_func : Can not detect pad type - goto detect charging pad type\n");
			set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PING_DETECT);
		}
	}
	else {
		set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_INTERRUPT);
	}
	return 0;
}

static u32 ftt_status_charging_func_leave(struct ftt_charger_device *ftt_pdev)
{
	DPRINT(FTT_DEBUG, "########## ftt_status_charging_func_leave ################\n");

	ftt_pdev->ftt_frequency = 0;
	return 0;
}
/*======================== END STATUS FUCTION ===============================*/

static u32 ftt_status_func(struct ftt_charger_device *ftt_pdev, enum ftt_status_func_enum func_enum)
{
	u32 prev_status, status, next_status;

	prev_status = ftt_pdev->ftt_status_d.ftt_prev_status;
	status = ftt_pdev->ftt_status_d.ftt_status;
	next_status = ftt_pdev->ftt_status_d.ftt_next_status;

	if (status == FTT_CHARGER_STATUS_DISABLE) return -EIO;

	switch (func_enum) {
	case FTT_STATUS_FUNC_ENTER :
		ftt_pdev->ftt_set_status = false;
		if ((ftt_status_fn[status].ftt_status_func_enter) && (prev_status != status)) return ftt_status_fn[status].ftt_status_func_enter(ftt_pdev);
		break;
	case FTT_STATUS_FUNC :
		if (ftt_status_fn[status].ftt_status_func) return ftt_status_fn[status].ftt_status_func(ftt_pdev);
		break;
	case FTT_STATUS_FUNC_LEAVE :
		if (ftt_pdev->ftt_set_status == false) {
			DPRINT(FTT_ALWAYS, "##########################################################\n");
			DPRINT(FTT_ALWAYS, "########## <<<ERROR>>> INVALID STATUS : %d ################\n", get_ftt_charger_current_status(ftt_pdev));
			DPRINT(FTT_ALWAYS, "##########################################################\n");
		}
		ftt_pdev->ftt_set_status = false;
		if ((ftt_status_fn[status].ftt_status_func_leave) && (next_status != status)) {
			if ((next_status == FTT_CHARGER_STATUS_CHARGING) || (status == FTT_CHARGER_STATUS_CHARGING)) {
				get_ftt_anntena_level(ftt_pdev);
			}
		}
		if ((ftt_status_fn[status].ftt_status_func_leave) && (next_status != status)) return ftt_status_fn[status].ftt_status_func_leave(ftt_pdev);
		break;
	}

	return -EIO;

}

/*======================== START CALLBACK FUCTION ===============================*/

static void ftt_charger_status_timer_handler(unsigned long data)
{
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)data;
	unsigned long 			flags;

	local_irq_save(flags);
	ftt_status_func(ftt_pdev, FTT_STATUS_FUNC_ENTER);

	switch (get_ftt_charger_current_status(ftt_pdev)) {
	case FTT_CHARGER_STATUS_PING_DETECT :
	case FTT_CHARGER_STATUS_PRE_CHARGING :
	case FTT_CHARGER_STATUS_CHARGING :
		ftt_status_func(ftt_pdev, FTT_STATUS_FUNC);
		break;
	default :
		ftt_status_func(ftt_pdev, FTT_STATUS_FUNC);
		break;
	}

	ftt_status_func(ftt_pdev, FTT_STATUS_FUNC_LEAVE);
	set_ftt_charger_save_status(ftt_pdev);
	local_irq_restore(flags);
	return;

}

static irqreturn_t ftt_interrupt_pin_cb(int irq, void *pdev)
{
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)pdev;
	unsigned long 			flags;
	u32 timer;

	local_irq_save(flags);

	if (ftt_pdev->ftt_interrupt_booting_enable_flag == false) {
		local_irq_restore(flags);
		return IRQ_NONE;
	}

	ftt_status_func(ftt_pdev, FTT_STATUS_FUNC);

	set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_PING_DETECT);
	ftt_status_func(ftt_pdev, FTT_STATUS_FUNC_LEAVE);

	ftt_pdev->ftt_status_d.ftt_prev_status = FTT_CHARGER_STATUS_INTERRUPT;
	ftt_pdev->ftt_status_d.ftt_status = FTT_CHARGER_STATUS_PING_DETECT;
	ftt_interrupt_disable(ftt_pdev);

#if FTT_CHARGER_STATUS_TIMER
	if( timer_pending(&ftt_pdev->ftt_charger_status_timer))
		del_timer(&ftt_pdev->ftt_charger_status_timer);
	timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET_ENTER];
	if (ftt_is_debug == FTT_LOG_PING_TEST) {
		timer = ftt_pdev->tst.ftt_status_timer[FTT_STATUS_TIMER_DET];
	}
	mod_timer(&ftt_pdev->ftt_charger_status_timer, jiffies + msecs_to_jiffies(timer));
#endif
	DPRINT(FTT_LOG_PING_TEST, "########## start ping test ###############################\n");

	local_irq_restore(flags);

	return IRQ_HANDLED;
}

/*======================== END CALLBACK FUCTION ===============================*/

void set_ftt_charger_status_interrupt(struct ftt_charger_device *ftt_pdev)
{
	ftt_status_func(ftt_pdev, FTT_STATUS_FUNC_ENTER);
	if( timer_pending(&ftt_pdev->ftt_charger_status_timer)) {
		del_timer(&ftt_pdev->ftt_charger_status_timer);
	}

	ftt_interrupt_enable(ftt_pdev);

}

void ftt_enable(struct ftt_charger_device *ftt_pdev)
{
	if (ftt_pdev->ftt_enable == false) {
		ftt_pdev->ftt_enable = true;
		set_ftt_charger_reset_status(ftt_pdev);
		set_ftt_charger_status_interrupt(ftt_pdev);
	}
}

void ftt_disable(struct ftt_charger_device *ftt_pdev)
{
	if (ftt_pdev->ftt_enable == true) {
		ftt_pdev->ftt_enable = false;
		if( timer_pending(&ftt_pdev->ftt_charger_status_timer)) {
			del_timer(&ftt_pdev->ftt_charger_status_timer);
		}
		set_ftt_charger_status(ftt_pdev, FTT_CHARGER_STATUS_DISABLE);
		ftt_status_func(ftt_pdev, FTT_STATUS_FUNC_LEAVE);

		ftt_pdev->ftt_status_d.ftt_prev_status = ftt_pdev->ftt_status_d.ftt_status;
		ftt_pdev->ftt_status_d.ftt_status = ftt_pdev->ftt_status_d.ftt_next_status;

		ftt_interrupt_disable(ftt_pdev);
		set_ftt_charger_reset_data(ftt_pdev);
	}

}

#ifdef	CONFIG_HAS_EARLYSUSPEND
static void ftt_suspend	(struct early_suspend *h)
{
	struct ftt_charger_device *ftt_pdev = container_of(h,
			struct ftt_charger_device, power);

	ftt_pdev->ftt_earlysuspend = true;
	DPRINT(FTT_DEBUG, "ftt_suspned\n");
}

static void ftt_resume(struct early_suspend *h)
{
	struct ftt_charger_device *ftt_pdev = container_of(h,
			struct ftt_charger_device, power);

	ftt_pdev->ftt_earlysuspend = false;
}
#endif

static void set_ftt_charger_init_data(struct ftt_charger_device *ftt_pdev)
{
#ifdef	CONFIG_HAS_EARLYSUSPEND
	ftt_pdev->ftt_earlysuspend = false;
#endif /* CONFIG_HAS_EARLYSUSPEND */
	ftt_pdev->ftt_interrupt_enable_flag = true;
	ftt_pdev->ftt_interrupt_booting_enable_flag = true;
	ftt_pdev->ftt_online = false;
	ftt_pdev->ftt_online_prev = false;
	ftt_pdev->daemon_state = 0;
	ftt_pdev->ftt_enable = false;
	ftt_pdev->openfile_num = 0;
}

static int ftt_hw_init(struct ftt_charger_device *ftt_pdev)
{
	int ret=0;

	ret = gpio_request_one(ftt_pdev->chg_ctrl_gpio, GPIOF_OUT_INIT_HIGH,
					"chg_ctrl_gpio");
	if (ret<0) {
		printk("[FTT] chg_ctrl_gpio request faild \n");
		return ret;
	}
	ret = gpio_request_one(ftt_pdev->half_chg_ctrl_gpio, GPIOF_OUT_INIT_LOW,
					"half_chg_ctrl_gpio");
	if (ret<0) {
		printk("[FTT] half_chg_ctrl_gpio request faild \n");
		return ret;
	}

	ret = gpio_request_one(ftt_pdev->active_n_gpio, GPIOF_DIR_IN,
					"active_n_gpio");
	if (ret<0) {
		printk("[FTT] active_n_gpio request faild \n");
		return ret;
	}
	ret = gpio_request_one(ftt_pdev->otg_ctrl_gpio, GPIOF_OUT_INIT_LOW,
					"otg_ctrl_gpio");
	if (ret<0) {
		printk("[FTT] otg_ctrl_gpio request faild \n");
		return ret;
	}
	return 1;
}

static int ftt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ftt_charger_pdata *pdata = pdev->dev.platform_data;
	struct ftt_charger_device *ftt_pdev;
	int err;
	int retval = 0;

#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
#endif

#ifdef CONFIG_OF
	printk("[FTT] +ftt_probe()\n");
	if (pdev->dev.of_node) {

		pdata = devm_kzalloc(&pdev->dev,
						sizeof(struct ftt_charger_pdata),
						GFP_KERNEL);
		if (!pdata) {
			printk("ftt : cannot allocate\n");
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;

		pdata->chg_ctrl_gpio = of_get_named_gpio(np, "chg_ctrl_gpio", 0);
		pdata->half_chg_ctrl_gpio = of_get_named_gpio(np, "half_chg_ctrl_gpio", 0);
		pdata->active_n_gpio = of_get_named_gpio(np, "active_n_gpio", 0);
		pdata->otg_ctrl_gpio= of_get_named_gpio(np, "otg_ctrl_gpio", 0);
		pdata->wlc_ts_mpp = of_get_named_gpio(np, "wlc_ts_mpp", 0);
		pdata->ftt_gpio = of_get_named_gpio(np, "track_gpio", 0);

		pdata->get_ftt_gpio = get_ftt_gpio;
		pdata->online_fn = wireless_online;
		pdata->on_change_level_fn = on_change_level;
	}
#endif

	ftt_pdev = kzalloc(sizeof(struct ftt_charger_device), GFP_KERNEL);
	if (!ftt_pdev) {
		dev_dbg(&pdev->dev, "out of memory\n");
		retval = -ENOMEM;
		goto err_out;
	}

	ftt_pdev->pdev = pdev;
	pdata = ftt_pdev->pdev->dev.platform_data;

#ifdef CONFIG_OF
	ftt_pdev->chg_ctrl_gpio = pdata->chg_ctrl_gpio;
	printk("[FTT] - chg_ctrl_gpio : %d\n", ftt_pdev->chg_ctrl_gpio);
	ftt_pdev->half_chg_ctrl_gpio = pdata->half_chg_ctrl_gpio;
	printk("[FTT] - half_chg_ctrl_gpio : %d\n", ftt_pdev->half_chg_ctrl_gpio);
	ftt_pdev->active_n_gpio = pdata->active_n_gpio;
	printk("[FTT] - active_n_gpio : %d\n", ftt_pdev->active_n_gpio);
	ftt_pdev->otg_ctrl_gpio = pdata->otg_ctrl_gpio;
	printk("[FTT] - otg_ctrl_gpio : %d\n", ftt_pdev->otg_ctrl_gpio);
	ftt_pdev->wlc_ts_mpp = pdata->wlc_ts_mpp;
	printk("[FTT] - wlc_ts_mpp : %d\n", ftt_pdev->wlc_ts_mpp);
	ftt_pdev->ftt_gpio = pdata->ftt_gpio;
	printk("[FTT] - track_gpio : %d\n", ftt_pdev->ftt_gpio);

	ftt_hw_init(ftt_pdev);
#endif

/* initalize member start */
	set_ftt_charger_init_data(ftt_pdev);
	set_ftt_charger_reset_status(ftt_pdev);
	set_ftt_charger_reset_data(ftt_pdev);
/* initalize member end */

	if (!pdata)
	{
		dev_err(dev, "Cannot find platform data.\n");
		retval = -ENXIO;
		goto err_kzalloc;
	}

	DPRINT(FTT_INFO, "%s\n", __FUNCTION__);

#if (FTT_CHIP_ENABLE_PIN_USE == 1)
	err = gpio_request(pdata->en1, "FTT_CHIP_ENABLE");
	if (err)
		printk(KERN_ERR "#### failed to request FTT_CHIP_ENABLE ####\n");
	gpio_direction_output(pdata->en1, ftt_chip_enable_flag);
#endif

	if (pdata->get_ftt_gpio == NULL) {
		retval = -ENXIO;
		goto err_kzalloc;
	}

	err = gpio_request(ftt_pdev->ftt_gpio, "FTT_FREQUANCY");
	if (err) {
		printk(KERN_ERR "#### failed to request FTT_FREQUANCY ####\n");
		retval = ENODEV;
		goto err_kzalloc;
	}

	gpio_direction_input(ftt_pdev->ftt_gpio);

	ftt_pdev->ftt_irq = gpio_to_irq(ftt_pdev->ftt_gpio);

	ftt_pdev->ftt_charger_status_timer.function = ftt_charger_status_timer_handler;
	ftt_pdev->ftt_charger_status_timer.data = (unsigned long)ftt_pdev;
	init_timer(&ftt_pdev->ftt_charger_status_timer);

	err = request_irq(ftt_pdev->ftt_irq, ftt_interrupt_pin_cb,
			IRQF_TRIGGER_RISING,
			DEVICE_NAME, (void *)ftt_pdev);
	if (err)
	{
		printk(KERN_ERR "gpio-ftt-charger: Unable to claim irq %d; error %d\n", ftt_pdev->ftt_irq, err);
		retval = err;
		goto err_gpio;
	}

	ftt_interrupt_disable(ftt_pdev);
	ftt_pdev->ftt_interrupt_booting_enable_flag = true;
	set_ftt_charger_reset_status(ftt_pdev);
#ifndef FTT_FILE_OPEN_ENABLE
	ftt_enable(ftt_pdev);
#endif /* FTT_FILE_OPEN_ENABLE */

#if defined(CONFIG_HAS_EARLYSUSPEND)
	/**************************************************************
	EARLYSUSPEND/LATERESUME
	**************************************************************/
	ftt_pdev->power.suspend	= ftt_suspend;
	ftt_pdev->power.resume	= ftt_resume;

	ftt_pdev->power.level	= EARLY_SUSPEND_LEVEL_DISABLE_FB-1;

	register_early_suspend(&ftt_pdev->power);
#endif

#if FTT_UEVENT
	ftt_uevent_init(ftt_pdev);
	if ((err = power_supply_register(dev, &ftt_pdev->ftt_supply))) {
		dev_err(dev, "failed: power supply ftt wireless register.\n");
		retval = err;
		goto err_suspend;
	}
	else	{
		dev_info(dev, "power supply ftt wireless registerd.\n");
	}
#endif

#if FTT_CHARACTER_DEVICE
	ftt_pdev->ftt_misc.fops = &ftt_fops;
	ftt_pdev->ftt_misc.name	= FTT_CHAR_DEVICE_NAME;
	ftt_pdev->ftt_misc.minor	= MISC_DYNAMIC_MINOR;//FTT_MINOR;
	if ((err = misc_register(&ftt_pdev->ftt_misc))) {
		retval = err;
		goto err_power_supply;
	}
	else	{
		dev_info(dev, "misc device ftt wireless registerd.\n");
	}
#endif /* FTT_CHARACTER_DEVICE */

	init_waitqueue_head(&ftt_pdev->wait);
	INIT_LIST_HEAD(&ftt_pdev->read_cmd_queue);

#ifdef CONFIG_FTT_SYSFS
	err = sysfs_create_group(&pdev->dev.kobj, &ftt_sysfs_attr_group);
	if (err) {
		printk(KERN_ERR "#### failed to sysfs_create_group ####\n");
		retval = err;
		goto err_character_device;
	}
#endif /* CONFIG_FTT_SYSFS */

	platform_set_drvdata(pdev, ftt_pdev);

	printk(DEVICE_NAME " Initialized\n");
	return 0;

/*******************************************************************************************
	ERROR
*******************************************************************************************/
	err_character_device:
#if FTT_CHARACTER_DEVICE
    misc_deregister(&ftt_pdev->ftt_misc);
#endif /* FTT_CHARACTER_DEVICE */

	err_power_supply:
#if FTT_UEVENT
	power_supply_unregister(&ftt_pdev->ftt_supply);
#endif

	err_suspend:
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ftt_pdev->power);
#endif
	free_irq(ftt_pdev->ftt_irq, ftt_pdev);
#if FTT_CHARGER_STATUS_TIMER
	del_timer(&ftt_pdev->ftt_charger_status_timer);
#endif

	err_gpio:
	gpio_free(ftt_pdev->ftt_gpio);

	err_kzalloc:
	kfree(ftt_pdev);

	err_out:
	return retval;
}

static int __exit ftt_remove(struct platform_device *pdev)
{
	struct ftt_charger_device *ftt_pdev = (struct ftt_charger_device *)pdev;

	DPRINT(FTT_INFO, "%s\n", __FUNCTION__);

#ifdef CONFIG_FTT_SYSFS
    sysfs_remove_group(&pdev->dev.kobj, &ftt_sysfs_attr_group);
#endif /* CONFIG_FTT_SYSFS */
#if FTT_CHARACTER_DEVICE
    misc_deregister(&ftt_pdev->ftt_misc);
#endif /* FTT_CHARACTER_DEVICE */
#if FTT_UEVENT
	power_supply_unregister(&ftt_pdev->ftt_supply);
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ftt_pdev->power);
#endif
	free_irq(ftt_pdev->ftt_irq, ftt_pdev);
#if FTT_CHARGER_STATUS_TIMER
	del_timer(&ftt_pdev->ftt_charger_status_timer);
#endif
	gpio_free(ftt_pdev->ftt_gpio);
	kfree(ftt_pdev);

	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id ftt_of_match[] = {
	{	.compatible = "cidt,ftt_charger"	},
	{},
};
MODULE_DEVICE_TABLE(of, ftt_of_match);
#endif

static struct platform_driver ftt_charger_v3_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ftt_of_match),
#endif
	},
	.probe          = ftt_probe,
	.remove = __exit_p(ftt_remove),
};

static int ftt_init(void)
{
	int ret = platform_driver_register(&ftt_charger_v3_driver);
#ifndef CONFIG_OF
	if(!ret)        {
		ret = platform_device_register(&ftt_charger_device);
		if(ret) platform_driver_unregister(&ftt_charger_v3_driver);
	}
#endif
	return ret;
}
module_init(ftt_init);

static void __exit ftt_exit(void)
{
#ifndef CONFIG_OF
	platform_device_unregister(&ftt_charger_device);
#endif
	platform_driver_unregister(&ftt_charger_v3_driver);
}
module_exit(ftt_exit);

MODULE_AUTHOR("Uhm choon ho <query91@cidt.co.kr>");
MODULE_DESCRIPTION("Driver for FTT Charget module Ver 3.1.0");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:CIDT FTT");
