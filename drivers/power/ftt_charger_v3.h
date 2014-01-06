#ifndef __FTT_CARGER_H__
#define __FTT_CARGER_H__ 	__FILE__

struct ftt_charger_pdata {
	int en1;
	int en2;
	int (*get_ftt_gpio)(int gpio);
	bool (*online_fn)(int gpio);
	bool (*on_change_level_fn)(int level);
	unsigned int chg_ctrl_gpio;
	unsigned int half_chg_ctrl_gpio;
	unsigned int active_n_gpio;
	unsigned int otg_ctrl_gpio;
	unsigned int wlc_ts_mpp;
	unsigned int track_gpio;
	int ftt_gpio;
};

#endif /* __FTT_CARGER_H__ */
