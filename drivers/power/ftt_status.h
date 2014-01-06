#ifndef FTT_STATUS_H_
#define FTT_STATUS_H_

#include <linux/miscdevice.h>
#include <linux/power_supply.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif
#include <linux/stat.h>

#include "ftt_ctrl_comm.h"


/***** device name *****/
#define DEVICE_NAME 				"ftt_charger"
#define SYSFS_NAME 					"ftt_charger"
#define FTT_CHAR_DEVICE_NAME		"ftt_ctrl"
#define FTT_POWER_SUPPLY			"wireless"

/***** general define *****/
#define FTT_PAD_TYPE_MAX			20

#define MAX_CORRECTION_FREQ_TABLE	20


/****   command constant  *****/

struct node_read_cmd {
	struct list_head link;
	char *cmd;
	u32 cmd_len;
};


enum ftt_charger_status {
	FTT_CHARGER_STATUS_INTERRUPT,
	FTT_CHARGER_STATUS_PING_DETECT,
	FTT_CHARGER_STATUS_PRE_CHARGING,
	FTT_CHARGER_STATUS_CHARGING,
	FTT_CHARGER_STATUS_DISABLE,
};

struct ftt_status_data {
	u32 next_timer;
	enum ftt_charger_status ftt_prev_status;
	enum ftt_charger_status ftt_status;
	enum ftt_charger_status ftt_next_status;
};

enum ftt_status_func_enum {
	FTT_STATUS_FUNC_ENTER,
	FTT_STATUS_FUNC,
	FTT_STATUS_FUNC_LEAVE,
};

struct ftt_charger_device;
struct ftt_status_struct {
	u32 (*ftt_status_func_enter)(struct ftt_charger_device *ftt_pdev);
	u32 (*ftt_status_func)(struct ftt_charger_device *ftt_pdev);
	u32 (*ftt_status_func_leave)(struct ftt_charger_device *ftt_pdev);
};

/****   define structure  *****/
struct ftt_charger_device {
	struct platform_device 		*pdev;
#ifdef	CONFIG_HAS_EARLYSUSPEND
	struct	early_suspend		power;
	u32 ftt_earlysuspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */
#if FTT_CHARGER_STATUS_TIMER
	struct timer_list ftt_charger_status_timer;
#endif
#if FTT_UEVENT
	struct power_supply ftt_supply;
#endif /* FTT_UEVENT */
#if FTT_CHARACTER_DEVICE
	struct miscdevice ftt_misc;
#endif /* FTT_CHARACTER_DEVICE */
	int ftt_gpio;
	int ftt_irq;
	bool ftt_interrupt_enable_flag;
	bool ftt_interrupt_booting_enable_flag;
	bool ftt_online;
	bool ftt_online_prev;
	bool ftt_set_status;
	bool ftt_enable;
	struct ftt_cmd_time_status_table tst;
	struct ftt_cmd_count_table fct;
	struct ftt_cmd_value_table fvt;
#if FTT_FREQ_CORRECTION_TABLE
	struct ftt_freq_correction_type ftt_freq_correction[MAX_CORRECTION_FREQ_TABLE];
	u32 ftt_freq_correction_table_size;
#endif /* FTT_FREQ_CORRECTION_TABLE */
	s32 openfile_num;
	/* Correlation data buffers */
	wait_queue_head_t wait;
	struct list_head read_cmd_queue;
	/* detect state */
	u16 ftt_ping_freq_tbl[FTT_DETECT_SAMPLE_COUNT];
	struct ant_level_type ant_level_type_table[MAX_ANT_LEVEL_TABLE];
	u32 ant_level_type_table_size;
	char detect_pad_name[FTT_READ_CMD_PUT_PAD_STRING_SIZE + 1];
	u32 detect_count;
	s32 detect_pad;
	/* performance */
	spinlock_t ftt_frequency_lock;
#if FTT_STATISTICS_DEBUG
	u32 ftt_detect_pad_statistics_count[FTT_PAD_TYPE_MAX];
	u32 ftt_detect_pad_statistics_total;
#endif /* FTT_STATISTICS_DEBUG */
	/* pre charging state */
	u32 pre_charging_count;
	/* charging state */
	u32 ftt_hysteresis;
	int prev_ant_level;
	int ant_level;
	u32 ftt_frequency;
	/* daemon */
	int daemon_state;
	/* general data buffers */
	struct ftt_status_data ftt_status_d;
	unsigned int chg_ctrl_gpio;
	unsigned int half_chg_ctrl_gpio;
	unsigned int active_n_gpio;
	unsigned int otg_ctrl_gpio;
	unsigned int wlc_ts_mpp;
	unsigned int track_gpio;
};

/****   sysfs constant  *****/
#define FTT_SYS_PERMISSION	(S_IRWXU | S_IRGRP | S_IROTH)

#endif /* FTT_STATUS_H_ */
