#ifndef _ISDBT_COMMON_H_
#define _ISDBT_COMMON_H_

#include "../../../../arch/arm/mach-msm/lge/j1/board-j1.h"

#define GPIO_MB86A35S_SPIC_XIRQ 		27	//rev.B
#define GPIO_ISDBT_IRQ 					33	
#define GPIO_MB86A35S_FRAME_LOCK		46	//rev.B - not need
// GJ_DCM 1SEG_ISR GPIO 77
// eric0.kim@lge.com [2012.07.20] for the oneseg power saving mode
//#define GPIO_MB86A35S_SPIS_XIRQ 		16	//rev.B - not need??? rev.C : 62 -> Rev.D :141
#define GPIO_MB86A35S_SPIS_XIRQ 		77	//rev.B - not need??? rev.C : 62 -> Rev.D :141
// GJ_DCM LDO_EN GPIO 85
#define GPIO_ISDBT_PWR_EN 				85	//1.2V, 1.8V (but 2.8V->PMIC)
#define GPIO_ISDBT_RST 					1
//#define GPIO_ISDBT_ANT_SELECT 		11	//rev.B

#define ISDBT_DEFAULT_NOTUSE_MODE -1
#define ISDBT_UHF_MODE 0
#define ISDBT_VHF_MODE 1

// eric0.kim@lge.com [2012.07.26]
#define DMB_ANT_SEL_P_EAR       	    PM8921_GPIO_PM_TO_SYS(11)
// eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
#define GPIO_ONESEG_INT		       	    PM8921_GPIO_PM_TO_SYS(16)

#endif //_ISDBT_COMMON_H_