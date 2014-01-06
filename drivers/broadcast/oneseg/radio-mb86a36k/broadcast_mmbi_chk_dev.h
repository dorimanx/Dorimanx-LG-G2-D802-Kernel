#ifndef	__BROADCAST_MMBI_CHK_DEV_H__
#define	__BROADCAST_MMBI_CHK_DEV_H__

#ifndef	__BROADCAST_MMBI_CHK_DEV_H__
#include <string.h>
#endif

/********************************************************************************************************/
/* ioctl() System call command code                                                                     */
/********************************************************************************************************/

enum ioctl_command_no {
	IOCTL_DRV_OPEN 			= 0x1001,
	IOCTL_DRV_CLOSE			= 0x1002,
	IOCTL_SET_RF_BAND 		= 0x1003,
	IOCTL_SET_CHANNEL 		= 0x1004,
	IOCTL_SET_MONITOR 		= 0x1005,
	IOCTL_GET_MONITOR 		= 0x1006,	
	IOCTL_SET_MONITOR_APP	= 0x1007,
};

/********************************************************************************************************/
/* ioctl() System call argement layout & define                                                         */
/********************************************************************************************************/
struct ioctl_sig_info {
	int cn;
	int rssi;
	int berA;
	int berB;
	int berC;
	int perA;
	int perB;
	int perC;
};
typedef struct ioctl_sig_info ioctl_sig_info_t;

struct ioctl_monitor_info {
	int drv_openning; //open : 0, close : -1
	int rf_mode; //VHF : 1, UHF : 0 
	int channel;
	int monitor_app; //app on:1 app off :0
	ioctl_sig_info_t sig_info;
};
typedef struct ioctl_monitor_info ioctl_monitor_info_t;

struct mmbi_chk_dev_cmdcontrol {
	ioctl_monitor_info_t MONITOR_INFO;
};
typedef struct mmbi_chk_dev_cmdcontrol mmbi_chk_dev_cmdcontrol_t;


#endif /* __RADIO_MB86A35_H__ */

