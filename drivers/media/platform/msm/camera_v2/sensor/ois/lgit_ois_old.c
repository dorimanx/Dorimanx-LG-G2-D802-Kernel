//==============================================================================
// ois+init.c Code START
//==============================================================================
//**************************
//	Include Header File		
//**************************

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <mach/camera2.h>
#include "msm_ois.h"
#include "msm_ois_i2c.h"


#define		OISINI
#include "lgit_ois_old.h"

#define LAST_UPDATE  "13-08-01, 0x0204B_Q2_3" 


//**************************
//	Global Variable
//**************************
unsigned char spigyrocheck=0x00;

#define	OIS_FW_POLLING_PASS		OIS_SUCCESS
#define	OIS_FW_POLLING_FAIL		OIS_INIT_TIMEOUT
#define	CLRGYR_POLLING_LIMIT_A	6
#define	ACCWIT_POLLING_LIMIT_A	6
#define	INIDGY_POLLING_LIMIT_A	12
#define INIDGY_POLLING_LIMIT_B	12
#define BSYWIT_POLLING_LIMIT_A  6

#define HALREGTAB	32
#define HALFILTAB	138
#define	GYRFILTAB	125

/*Filter Calculator Version 4.02*/
/*the time and date : 2013/3/15 16:46:01*/
/*the time and date : 2013/3/28 22:25:01*/
/*fs,23438Hz*/
/*LSI No.,LC898111_AS*/
/*Comment,*/

/* 8bit */
OISINI__ const struct STHALREG	CsHalReg[][HALREGTAB]	= {
	{	//VER 04 /*FC filename : Mitsumi_130501*/
		{ 0x000E, 0x00},	/*00,000E*/
		{ 0x000F, 0x00},	/*00,000F,0dB*/
		{ 0x0086, 0x00},	/*00,0086,0dB*/
		{ 0x0087, 0x00},	/*00,0087,0dB*/
		{ 0x0088, 0x15},	/*15,0088,30dB*/
		{ 0x0089, 0x00},	/*00,0089*/
		{ 0x008A, 0x40},	/*40,008A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x008B, 0x54},	/*54,008B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x008C, 0x00},	/*00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0*/
		{ 0x0090, 0x00},	/*00,0090,0dB*/
		{ 0x0091, 0x00},	/*00,0091,0dB*/
		{ 0x0092, 0x15},	/*15,0092,30dB*/
		{ 0x0093, 0x00},	/*00,0093*/
		{ 0x0094, 0x40},	/*40,0094,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x0095, 0x54},	/*54,0095,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x0096, 0x00},	/*00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0*/
		{ 0x00A0, 0x00},	/*00,00A0,0dB*/
		{ 0x00A1, 0x00},	/*00,00A1*/
		{ 0x00B4, 0x00},	/*00,00B4*/
		{ 0x00B5, 0x00},	/*00,00B5,0dB*/
		{ 0x00B6, 0x00},	/*00,00B6,Through,0dB,fs/1,invert=0*/
		{ 0x00BB, 0x00},	/*00,00BB*/
		{ 0x00BC, 0x00},	/*00,00BC,0dB*/
		{ 0x00BD, 0x00},	/*00,00BD,Through,0dB,fs/1,invert=0*/
		{ 0x00C1, 0x00},	/*00,00C1,Through,0dB,fs/1,invert=0*/
		{ 0x00C5, 0x00},	/*00,00C5,Through,0dB,fs/1,invert=0*/
		{ 0x00C8, 0x00},	/*00,00C8*/
		{ 0x0110, 0x01},	/*01,0110*/
		{ 0x0111, 0x00},	/*00,0111*/
		{ 0x0112, 0x00},	/*00,0112*/
		{ 0x017D, 0x01},	/*01,017D*/
		{ 0xFFFF , 0xFF }
	}
} ;

/* 16bit */
OISINI__ const struct STHALFIL	CsHalFil[][HALFILTAB]	= {
	{	//VER 04 /*FC filename : Mitsumi_130501*/
		{ 0x1128, 0x0000},	/*0000,1128,Cutoff,invert=0*/
		{ 0x1168, 0x0000},	/*0000,1168,Cutoff,invert=0*/
		{ 0x11E0, 0x7FFF},	/*7FFF,11E0,0dB,invert=0*/
		{ 0x11E1, 0x7FFF},	/*7FFF,11E1,0dB,invert=0*/
		{ 0x11E2, 0x7FFF},	/*7FFF,11E2,0dB,invert=0*/
		{ 0x11E3, 0x7FFF},	/*7FFF,11E3,0dB,invert=0*/
		{ 0x11E4, 0x7FFF},	/*7FFF,11E4,0dB,invert=0*/
		{ 0x11E5, 0x7FFF},	/*7FFF,11E5,0dB,invert=0*/
		{ 0x12E0, 0x7FFF},	/*7FFF,12E0,Through,0dB,fs/1,invert=0*/
		{ 0x12E1, 0x0000},	/*0000,12E1,Through,0dB,fs/1,invert=0*/
		{ 0x12E2, 0x0000},	/*0000,12E2,Through,0dB,fs/1,invert=0*/
		{ 0x12E3, 0x7FFF},	/*7FFF,12E3,Through,0dB,fs/1,invert=0*/
		{ 0x12E4, 0x0000},	/*0000,12E4,Through,0dB,fs/1,invert=0*/
		{ 0x12E5, 0x0000},	/*0000,12E5,Through,0dB,fs/1,invert=0*/
		{ 0x12E6, 0x7FFF},	/*7FFF,12E6,0dB,invert=0*/
		{ 0x1301, 0x7FFF},	/*7FFF,1301,0dB,invert=0*/
		{ 0x1302, 0x7FFF},	/*7FFF,1302,0dB,invert=0*/
		{ 0x1305, 0x7FFF},	/*7FFF,1305,Through,0dB,fs/1,invert=0*/
		{ 0x1306, 0x0000},	/*0000,1306,Through,0dB,fs/1,invert=0*/
		{ 0x1307, 0x0000},	/*0000,1307,Through,0dB,fs/1,invert=0*/
		{ 0x1308, 0x0000},	/*0000,1308,Cutoff,invert=0*/
		{ 0x1309, 0x5A9D},	/*5A9D,1309,-3dB,invert=0*/
		{ 0x130A, 0x0000},	/*0000,130A,Cutoff,invert=0*/
		{ 0x130B, 0x0000},	/*0000,130B,Cutoff,invert=0*/
		{ 0x130C, 0x7FFF},	/*7FFF,130C,0dB,invert=0*/
		{ 0x130D, 0x46EB},	/*46EB,130D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x130E, 0xBA1D},	/*BA1D,130E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x130F, 0x3679},	/*3679,130F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1310, 0x0000},	/*0000,1310,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1311, 0x0000},	/*0000,1311,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1312, 0x43B3},	/*43B3,1312,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1313, 0xBCD7},	/*BCD7,1313,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1314, 0x3F51},	/*3F51,1314,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1315, 0x0000},	/*0000,1315,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1316, 0x0000},	/*0000,1316,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1317, 0x7FFF},	/*7FFF,1317,0dB,invert=0*/
		{ 0x1318, 0x0000},	/*0000,1318,Cutoff,invert=0*/
		{ 0x1319, 0x0000},	/*0000,1319,Cutoff,invert=0*/
		{ 0x131A, 0x002B},	/*002B,131A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131B, 0x0055},	/*0055,131B,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131C, 0x72F9},	/*72F9,131C,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131D, 0x002B},	/*002B,131D,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131E, 0xCC5D},	/*CC5D,131E,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131F, 0x020D},	/*020D,131F,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1320, 0x020D},	/*020D,1320,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1321, 0x7FCB},	/*7FCB,1321,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1322, 0x0000},	/*0000,1322,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1323, 0x0000},	/*0000,1323,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1324, 0x334D},	/*334D,1324,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1325, 0xCD0A},	/*CD0A,1325,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1326, 0x7FA9},	/*7FA9,1326,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1327, 0x0000},	/*0000,1327,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1328, 0x0000},	/*0000,1328,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1329, 0x7FFF},	/*7FFF,1329,0dB,invert=0*/
		{ 0x132A, 0x7FFF},	/*7FFF,132A,0dB,invert=0*/
		{ 0x132B, 0x0000},	/*0000,132B,Cutoff,invert=0*/
		{ 0x132C, 0x0000},	/*0000,132C,Cutoff,invert=0*/
		{ 0x132D, 0x0000},	/*0000,132D,Cutoff,invert=0*/
		{ 0x132E, 0x0000},	/*0000,132E,Cutoff,invert=0*/
		{ 0x132F, 0x7485},	/*7485,132F,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1330, 0x8EDF},	/*8EDF,1330,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1331, 0x7C9D},	/*7C9D,1331,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1332, 0x3853},	/*3853,1332,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1333, 0x9CAB},	/*9CAB,1333,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1334, 0x6355},	/*6355,1334,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1335, 0x313B},	/*313B,1335,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1336, 0xD672},	/*D672,1336,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1337, 0x0D17},	/*0D17,1337,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1338, 0x0D17},	/*0D17,1338,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1339, 0x65D1},	/*65D1,1339,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133A, 0x0000},	/*0000,133A,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133B, 0x0000},	/*0000,133B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133C, 0x7FFF},	/*7FFF,133C,0dB,invert=0*/
		{ 0x133D, 0x0000},	/*0000,133D,Cutoff,invert=0*/
		{ 0x133E, 0x0000},	/*0000,133E,Cutoff,invert=0*/
		{ 0x133F, 0x7FFF},	/*7FFF,133F,0dB,invert=0*/
		{ 0x1341, 0x7FFF},	/*7FFF,1341,0dB,invert=0*/
		{ 0x1342, 0x7FFF},	/*7FFF,1342,0dB,invert=0*/
		{ 0x1345, 0x7FFF},	/*7FFF,1345,Through,0dB,fs/1,invert=0*/
		{ 0x1346, 0x0000},	/*0000,1346,Through,0dB,fs/1,invert=0*/
		{ 0x1347, 0x0000},	/*0000,1347,Through,0dB,fs/1,invert=0*/
		{ 0x1348, 0x0000},	/*0000,1348,Cutoff,invert=0*/
		{ 0x1349, 0x5A9D},	/*5A9D,1349,-3dB,invert=0*/
		{ 0x134A, 0x0000},	/*0000,134A,Cutoff,invert=0*/
		{ 0x134B, 0x0000},	/*0000,134B,Cutoff,invert=0*/
		{ 0x134C, 0x7FFF},	/*7FFF,134C,0dB,invert=0*/
		{ 0x134D, 0x46EB},	/*46EB,134D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x134E, 0xBA1D},	/*BA1D,134E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x134F, 0x3679},	/*3679,134F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1350, 0x0000},	/*0000,1350,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1351, 0x0000},	/*0000,1351,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1352, 0x43B3},	/*43B3,1352,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1353, 0xBCD7},	/*BCD7,1353,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1354, 0x3F51},	/*3F51,1354,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1355, 0x0000},	/*0000,1355,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1356, 0x0000},	/*0000,1356,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1357, 0x7FFF},	/*7FFF,1357,0dB,invert=0*/
		{ 0x1358, 0x0000},	/*0000,1358,Cutoff,invert=0*/
		{ 0x1359, 0x0000},	/*0000,1359,Cutoff,invert=0*/
		{ 0x135A, 0x002B},	/*002B,135A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135B, 0x0055},	/*0055,135B,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135C, 0x72F9},	/*72F9,135C,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135D, 0x002B},	/*002B,135D,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135E, 0xCC5D},	/*CC5D,135E,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135F, 0x020D},	/*020D,135F,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1360, 0x020D},	/*020D,1360,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1361, 0x7FCB},	/*7FCB,1361,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1362, 0x0000},	/*0000,1362,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1363, 0x0000},	/*0000,1363,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1364, 0x334D},	/*334D,1364,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1365, 0xCD0A},	/*CD0A,1365,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1366, 0x7FA9},	/*7FA9,1366,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1367, 0x0000},	/*0000,1367,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1368, 0x0000},	/*0000,1368,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1369, 0x7FFF},	/*7FFF,1369,0dB,invert=0*/
		{ 0x136A, 0x7FFF},	/*7FFF,136A,0dB,invert=0*/
		{ 0x136B, 0x0000},	/*0000,136B,Cutoff,invert=0*/
		{ 0x136C, 0x0000},	/*0000,136C,Cutoff,invert=0*/
		{ 0x136D, 0x0000},	/*0000,136D,Cutoff,invert=0*/
		{ 0x136E, 0x0000},	/*0000,136E,Cutoff,invert=0*/
		{ 0x136F, 0x7485},	/*7485,136F,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1370, 0x8EDF},	/*8EDF,1370,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1371, 0x7C9D},	/*7C9D,1371,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1372, 0x3853},	/*3853,1372,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1373, 0x9CAB},	/*9CAB,1373,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1374, 0x6355},	/*6355,1374,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1375, 0x313B},	/*313B,1375,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1376, 0xD672},	/*D672,1376,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1377, 0x0D17},	/*0D17,1377,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1378, 0x0D17},	/*0D17,1378,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1379, 0x65D1},	/*65D1,1379,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137A, 0x0000},	/*0000,137A,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137B, 0x0000},	/*0000,137B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137C, 0x7FFF},	/*7FFF,137C,0dB,invert=0*/
		{ 0x137D, 0x0000},	/*0000,137D,Cutoff,invert=0*/
		{ 0x137E, 0x0000},	/*0000,137E,Cutoff,invert=0*/
		{ 0x137F, 0x7FFF},	/*7FFF,137F,0dB,invert=0*/
		{ 0xFFFF , 0xFFFF }
	}
} ;
/* 32bit */
OISINI__ const struct STGYRFIL	CsGyrFil[][GYRFILTAB]	= {
	{	//VER 04
		{ 0x1800, 0x3F800000},	/*3F800000,1800,0dB,invert=0*/
		{ 0x1801, 0x3C3F00A9},	/*3C3F00A9,1801,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1802, 0x3F7A07FB},	/*3F7A07FB,1802,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1803, 0x38A8A554},	/*38A8A554,1803,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1804, 0x38A8A554},	/*38A8A554,1804,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1805, 0x3F7FF576},	/*3F7FF576,1805,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1806, 0x3C3F00A9},	/*3C3F00A9,1806,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1807, 0x00000000},	/*00000000,1807,Cutoff,invert=0*/
		{ 0x180A, 0x38A8A554},	/*38A8A554,180A,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180B, 0x38A8A554},	/*38A8A554,180B,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180C, 0x3F7FF576},	/*3F7FF576,180C,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180D, 0x3F800000},	/*3F800000,180D,0dB,invert=0*/
		{ 0x180E, 0xBF800000},	/*BF800000,180E,0dB,invert=1*/
		{ 0x180F, 0x3FFF64C1},	/*3FFF64C1,180F,6dB,invert=0*/
		{ 0x1810, 0x3F800000},	/*3F800000,1810,0dB,invert=0*/
		{ 0x1811, 0x3F800000},	/*3F800000,1811,0dB,invert=0*/
		{ 0x1812, 0x3B02C2F2},	/*3B02C2F2,1812,Free,fs/2,invert=0*/
		{ 0x1813, 0x00000000},	/*00000000,1813,Free,fs/2,invert=0*/
		{ 0x1814, 0x3F7FFD80},	/*3F7FFD80,1814,Free,fs/2,invert=0*/
		{ 0x1815, 0x428C7352},	/*428C7352,1815,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1816, 0xC28AA79E},	/*C28AA79E,1816,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1817, 0x3DDE3847},	/*3DDE3847,1817,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1818, 0x3F231C22},	/*3F231C22,1818,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x1819, 0xBF1ECB8E},	/*BF1ECB8E,1819,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x181A, 0x3F7A916B},	/*3F7A916B,181A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x181B, 0x00000000},	/*00000000,181B,Cutoff,invert=0*/
		{ 0x181C, 0x3F800000},	/*3F800000,181C,0dB,invert=0*/
		{ 0x181D, 0x3F800000},	/*3F800000,181D,Through,0dB,fs/2,invert=0*/
		{ 0x181E, 0x00000000},	/*00000000,181E,Through,0dB,fs/2,invert=0*/
		{ 0x181F, 0x00000000},	/*00000000,181F,Through,0dB,fs/2,invert=0*/
		{ 0x1820, 0x3F75C43F},	/*3F75C43F,1820,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1821, 0xBF756FF8},	/*BF756FF8,1821,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1822, 0x3F7FABB9},	/*3F7FABB9,1822,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1823, 0x3F800000},	/*3F800000,1823,Through,0dB,fs/2,invert=0*/
		{ 0x1824, 0x00000000},	/*00000000,1824,Through,0dB,fs/2,invert=0*/
		{ 0x1825, 0x00000000},	/*00000000,1825,Through,0dB,fs/2,invert=0*/
		{ 0x1826, 0x00000000},	/*00000000,1826,Cutoff,invert=0*/
		{ 0x1827, 0x3F800000},	/*3F800000,1827,0dB,invert=0*/
		{ 0x1828, 0x3F800000},	/*3F800000,1828,0dB,invert=0*/
		{ 0x1829, 0x41400000},	/*41400000,1829,21.5836dB,invert=0*/
		{ 0x182A, 0x3F800000},	/*3F800000,182A,0dB,invert=0*/
		{ 0x182B, 0x3F800000},	/*3F800000,182B,0dB,invert=0*/
		{ 0x182C, 0x00000000},	/*00000000,182C,Cutoff,invert=0*/
		{ 0x1830, 0x3D1E56E0},	/*3D1E56E0,1830,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1831, 0x3D1E56E0},	/*3D1E56E0,1831,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1832, 0x3F6C3524},	/*3F6C3524,1832,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1833, 0x00000000},	/*00000000,1833,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1834, 0x00000000},	/*00000000,1834,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1835, 0x00000000},	/*00000000,1835,Cutoff,invert=0*/
		{ 0x1838, 0x3F800000},	/*3F800000,1838,0dB,invert=0*/
		{ 0x1839, 0x3C58B55D},	/*3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183A, 0x3C58B55D},	/*3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183B, 0x3F793A55},	/*3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183C, 0x3C58B55D},	/*3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183D, 0x3C58B55D},	/*3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183E, 0x3F793A55},	/*3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x1840, 0x38A8A554},	/*38A8A554,1840,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1841, 0x38A8A554},	/*38A8A554,1841,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1842, 0x3F7FF576},	/*3F7FF576,1842,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1850, 0x38A8A554},	/*38A8A554,1850,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1851, 0x38A8A554},	/*38A8A554,1851,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1852, 0x3F7FF576},	/*3F7FF576,1852,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1900, 0x3F800000},	/*3F800000,1900,0dB,invert=0*/
		{ 0x1901, 0x3C3F00A9},	/*3C3F00A9,1901,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1902, 0x3F7A07FB},	/*3F7A07FB,1902,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1903, 0x38A8A554},	/*38A8A554,1903,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1904, 0x38A8A554},	/*38A8A554,1904,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1905, 0x3F7FF576},	/*3F7FF576,1905,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1906, 0x3C3F00A9},	/*3C3F00A9,1906,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1907, 0x00000000},	/*00000000,1907,Cutoff,invert=0*/
		{ 0x190A, 0x38A8A554},	/*38A8A554,190A,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190B, 0x38A8A554},	/*38A8A554,190B,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190C, 0x3F7FF576},	/*3F7FF576,190C,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190D, 0x3F800000},	/*3F800000,190D,0dB,invert=0*/
		{ 0x190E, 0xBF800000},	/*BF800000,190E,0dB,invert=1*/
		{ 0x190F, 0x3FFF64C1},	/*3FFF64C1,190F,6dB,invert=0*/
		{ 0x1910, 0x3F800000},	/*3F800000,1910,0dB,invert=0*/
		{ 0x1911, 0x3F800000},	/*3F800000,1911,0dB,invert=0*/
		{ 0x1912, 0x3B02C2F2},	/*3B02C2F2,1912,Free,fs/2,invert=0*/
		{ 0x1913, 0x00000000},	/*00000000,1913,Free,fs/2,invert=0*/
		{ 0x1914, 0x3F7FFD80},	/*3F7FFD80,1914,Free,fs/2,invert=0*/
		{ 0x1915, 0x428C7352},	/*428C7352,1915,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1916, 0xC28AA79E},	/*C28AA79E,1916,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1917, 0x3DDE3847},	/*3DDE3847,1917,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1918, 0x3F231C22},	/*3F231C22,1918,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x1919, 0xBF1ECB8E},	/*BF1ECB8E,1919,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x191A, 0x3F7A916B},	/*3F7A916B,191A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x191B, 0x00000000},	/*00000000,191B,Cutoff,invert=0*/
		{ 0x191C, 0x3F800000},	/*3F800000,191C,0dB,invert=0*/
		{ 0x191D, 0x3F800000},	/*3F800000,191D,Through,0dB,fs/2,invert=0*/
		{ 0x191E, 0x00000000},	/*00000000,191E,Through,0dB,fs/2,invert=0*/
		{ 0x191F, 0x00000000},	/*00000000,191F,Through,0dB,fs/2,invert=0*/
		{ 0x1920, 0x3F75C43F},	/*3F75C43F,1920,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1921, 0xBF756FF8},	/*BF756FF8,1921,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1922, 0x3F7FABB9},	/*3F7FABB9,1922,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1923, 0x3F800000},	/*3F800000,1923,Through,0dB,fs/2,invert=0*/
		{ 0x1924, 0x00000000},	/*00000000,1924,Through,0dB,fs/2,invert=0*/
		{ 0x1925, 0x00000000},	/*00000000,1925,Through,0dB,fs/2,invert=0*/
		{ 0x1926, 0x00000000},	/*00000000,1926,Cutoff,invert=0*/
		{ 0x1927, 0x3F800000},	/*3F800000,1927,0dB,invert=0*/
		{ 0x1928, 0x3F800000},	/*3F800000,1928,0dB,invert=0*/
		{ 0x1929, 0x41400000},	/*41400000,1929,21.5836dB,invert=0*/
		{ 0x192A, 0x3F800000},	/*3F800000,192A,0dB,invert=0*/
		{ 0x192B, 0x3F800000},	/*3F800000,192B,0dB,invert=0*/
		{ 0x192C, 0x00000000},	/*00000000,192C,Cutoff,invert=0*/
		{ 0x1930, 0x3D1E56E0},	/*3D1E56E0,1930,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1931, 0x3D1E56E0},	/*3D1E56E0,1931,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1932, 0x3F6C3524},	/*3F6C3524,1932,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1933, 0x00000000},	/*00000000,1933,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1934, 0x00000000},	/*00000000,1934,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1935, 0x00000000},	/*00000000,1935,Cutoff,invert=0*/
		{ 0x1938, 0x3F800000},	/*3F800000,1938,0dB,invert=0*/
		{ 0x1939, 0x3C58B55D},	/*3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193A, 0x3C58B55D},	/*3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193B, 0x3F793A55},	/*3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193C, 0x3C58B55D},	/*3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193D, 0x3C58B55D},	/*3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193E, 0x3F793A55},	/*3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x1940, 0x38A8A554},	/*38A8A554,1940,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1941, 0x38A8A554},	/*38A8A554,1941,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1942, 0x3F7FF576},	/*3F7FF576,1942,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1950, 0x38A8A554},	/*38A8A554,1950,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1951, 0x38A8A554},	/*38A8A554,1951,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1952, 0x3F7FF576},	/*3F7FF576,1952,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0xFFFF , 0xFFFFFFFF }
	}
} ;

//**************************
//	Global Variable			
//**************************
int OnsemiI2CCheck(void)
{
	unsigned char UcLsiVer;
	RegReadA( CVER, &UcLsiVer ); // 0x27E
	return (UcLsiVer == 0x43) ? 1 : 0;
}	

int	IniSet( void )
{
	
	WitTim(5);
	RegWriteA( SOFRES1	, 0x00 );	// Software Reset 
	WitTim(5);
	RegWriteA( SOFRES1	, 0x11 );
	
	// Read calibration data from E2PROM
	E2pDat() ;
	// Version Check
	VerInf() ;
	// Clock Setting
	IniClk() ;
	// I/O Port Initial Setting
	IniIop() ;
	// Monitor & Other Initial Setting
	IniMon() ;
	// Servo Initial Setting
	IniSrv() ;
	// Gyro Filter Initial Setting
	IniGyr() ; //Pan OFF
	
	//POLLING EXCEPTION Setting
	// Hall Filter Initial Setting
	if( IniHfl() != OIS_FW_POLLING_PASS ) return OIS_FW_POLLING_FAIL ;
	// Gyro Filter Initial Setting
	if( IniGfl() != OIS_FW_POLLING_PASS ) return OIS_FW_POLLING_FAIL ;
	// DigitalGyro Initial Setting
	if( IniDgy() != OIS_FW_POLLING_PASS ) return OIS_FW_POLLING_FAIL ;

	// Adjust Fix Value Setting
	IniAdj() ; //Pan ON

	return OIS_FW_POLLING_PASS ;
}

//********************************************************************************
// Function Name 	: E2pDat
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: E2PROM Calibration Data Read Function
// History			: First edition 						2013.06.21 Y.Kim
//********************************************************************************
void	E2pDat( void )
{
	
	MemClr( ( unsigned char * )&StCalDat, sizeof( stCalDat ) ) ;
	
	E2pRed( (unsigned short)ADJ_COMP_FLAG, 2,		( unsigned char * )&StCalDat.UsAdjCompF ) ;
	
	E2pRed( (unsigned short)HALL_OFFSET_X, 2,		( unsigned char * )&StCalDat.StHalAdj.UsHlxOff ) ;
	E2pRed( (unsigned short)HALL_BIAS_X, 2,			( unsigned char * )&StCalDat.StHalAdj.UsHlxGan ) ;
	E2pRed( (unsigned short)HALL_AD_OFFSET_X, 2,	( unsigned char * )&StCalDat.StHalAdj.UsAdxOff ) ;

	E2pRed( (unsigned short)HALL_OFFSET_Y, 2,		( unsigned char * )&StCalDat.StHalAdj.UsHlyOff ) ;
	E2pRed( (unsigned short)HALL_BIAS_Y, 2,			( unsigned char * )&StCalDat.StHalAdj.UsHlyGan ) ;
	E2pRed( (unsigned short)HALL_AD_OFFSET_Y, 2,	( unsigned char * )&StCalDat.StHalAdj.UsAdyOff ) ;

	E2pRed( (unsigned short)LOOP_GAIN_X, 2,			( unsigned char * )&StCalDat.StLopGan.UsLxgVal ) ;
	E2pRed( (unsigned short)LOOP_GAIN_Y, 2,			( unsigned char * )&StCalDat.StLopGan.UsLygVal ) ;

	E2pRed( (unsigned short)LENS_CENTER_FINAL_X, 2,	( unsigned char * )&StCalDat.StLenCen.UsLsxVal ) ;
	E2pRed( (unsigned short)LENS_CENTER_FINAL_Y, 2,	( unsigned char * )&StCalDat.StLenCen.UsLsyVal ) ;

	E2pRed( (unsigned short)GYRO_AD_OFFSET_X, 2,	( unsigned char * )&StCalDat.StGvcOff.UsGxoVal ) ;
	E2pRed( (unsigned short)GYRO_AD_OFFSET_Y, 2,	( unsigned char * )&StCalDat.StGvcOff.UsGyoVal ) ;

	E2pRed( (unsigned short)GYRO_GAIN_X, 4 ,		( unsigned char * )&StCalDat.UlGxgVal ) ;
	E2pRed( (unsigned short)GYRO_GAIN_Y, 4 ,		( unsigned char * )&StCalDat.UlGygVal ) ;

	E2pRed( (unsigned short)OSC_CLK_VAL, 2 ,		( unsigned char * )&StCalDat.UsOscVal ) ;
	
	E2pRed( (unsigned short)SYSTEM_VERSION_INFO, 2,	( unsigned char * )&StCalDat.UsVerDat ) ;

	return;
}

//********************************************************************************
// Function Name 	: VerInf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: F/W Version Check
// History			: First edition 						2013.03.21 Y.Kim
//********************************************************************************
void	VerInf( void )
{
	UcVerHig = (unsigned char)(StCalDat.UsVerDat >> 8 ) ;		// System Version
	UcVerLow = (unsigned char)(StCalDat.UsVerDat)  ;			// Filter Version

	CDBG("%s : %x, %x \n",__func__, UcVerHig, UcVerLow);

	if( UcVerHig == 0xFF  ){						// If System Version Info not exist
		UcVerHig = 0x00 ;
	}else if( UcVerHig <= (unsigned char)0x02 ){		// Not use old Version
		UcVerHig = 0x00 ;
	};

	if( UcVerLow == 0xFF ){							// If System Version Info not exist
		UcVerLow = 0x00 ;
	}else if( UcVerLow <= (unsigned char)0x04 ){		// Not use old Version
		UcVerLow = 0x00 ;
	};

	return;
}

//********************************************************************************
// Function Name 	: IniClk
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Clock Setting
// History			: First edition 						2009.07.30 Y.Tashita
//					  LC898111 changes						2011.04.08 d.yamagata
//********************************************************************************
void	IniClk( void )
{
	UcOscAdjFlg	= 0 ;					// Osc adj flag 
	
	RegWriteA( CLKON,	0x13 ) ;		// 0x020B	 [ - | - | CmCalClkOn  | CMGifClkOn  | CmPezClkOn  | CmEepClkOn  | CmSrvClkOn  | CmPwmClkOn  ]

	return;
}

//********************************************************************************
// Function Name 	: IniIop
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: I/O Port Initial Setting
// History			: First edition 						2009.07.30 Y.Tashita
//					  LC898111 changes						2011.04.08 d.yamagata
//********************************************************************************
void	IniIop( void )
{
	/*select IOP signal*/
	RegWriteA( IOP0SEL, 0x00 ); 	// 0x0230	[1:0] 00: DGMOSI, 01: HPS_CTL0, 1x: IOP0
	RegWriteA( IOP3SEL, 0x01 ); 	// 0x0233	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: DGINT, 01: MON, 1x: IOP3
	RegWriteA( IOP4SEL, 0x00 ); 	// 0x0234	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: BUSY1/EPSIIF, 01: MON, 1x: IOP4
	RegWriteA( IOP5SEL, 0x21 ); 	// 0x0235	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: BUSY2, 01: MON, 1x: IOP5
	RegWriteA( IOP6SEL, 0x11 ); 	// 0x0236	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: EPSIIF, 01: MON, 1x: IOP6
	RegWriteA( SRMODE, 0x02 );		// 0x0251	[1]    0: SRAM DL ON, 1: OFF
	//			[0]    0: USE SRAM OFF, 1: ON

	return;
}

int	IniDgy( void )
{
	unsigned char	UcRegBak ;
	unsigned char	UcRamIni0, UcRamIni1, UcCntPla, UcCntPlb ;

	UcCntPla = 0 ;
	UcCntPlb = 0 ;
	
	/*Gyro Filter Setting*/
	RegWriteA( GGADON	, 0x01 );		// 0x011C		[ - | - | - | CmSht2PanOff ][ - | - | CmGadOn(1:0) ]
	RegWriteA( GGADSMPT , 0x0E);		// 0x011F
	/*Set to Command Mode*/
	RegWriteA( GRSEL	, 0x01 );							// 0x0380	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]
	
	/*Digital Gyro Read settings*/
	RegWriteA( GRINI	, 0x80 );							// 0x0381	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]
	
	// IDG-2021 Register Write Max1MHz
	RegReadA( GIFDIV, &UcRegBak ) ;
	RegWriteA( GIFDIV, 0x04 ) ;				// 48MHz / 4 = 12MHz
	RegWriteA( GRINI, 0x84 ) ;				// SPI Clock = Slow Mode 12MHz / 4 / 4 = 750KHz
	
	// Gyro Clock Setting
	RegWriteA( GRADR0	, 0x6B ) ;					// 0x0383	Set USER CONTROL 
	RegWriteA( GSETDT	, 0x01 ) ;					// 0x038A	Set Write Data
	RegWriteA( GRACC	, 0x10 );					// 0x0382	[ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ]
	
	if( AccWit( 0x10 ) == OIS_FW_POLLING_FAIL ){ return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/
	
	UcRamIni0 = 0x02 ;
	do{
		RegWriteA( GRACC,	0x01 ) ;				/* 0x0382	Set 1Byte Read Trigger ON			*/						
		if( AccWit( 0x01 ) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }	/* Digital Gyro busy wait 				*/
		RegReadA( GRADT0H, &UcRamIni0 ) ;			/* 0x0390	*/
		UcCntPla++ ;
	} while( ( UcRamIni0 & 0x02 ) && ( UcCntPla < INIDGY_POLLING_LIMIT_A ) );
	if( UcCntPla == INIDGY_POLLING_LIMIT_A ) { return OIS_FW_POLLING_FAIL; }
	
	WitTim(2);
	spigyrocheck = UcRamIni0;
	
	RegWriteA( GRADR0,	0x1B ) ;					// 0x0383	Set GYRO_CONFIG
	RegWriteA( GSETDT,	( FS_SEL << 3) ) ;			// 0x038A	Set Write Data //FS_SEL 2 ; 0x10 :  
	RegWriteA( GRACC,	0x10 ) ;					/* 0x0382	Set Trigger ON				*/
	if( AccWit( 0x10 ) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/
	/* SIG_COND_RESET ---< */
	
	/* SIG_COND_RESET ---> */
	RegWriteA( GRADR0	, 0x6A ) ;					// 0x0383	Set USER CONTROL
	RegWriteA( GSETDT	, 0x11 ) ;					// 0x038A	Set Write Data
	RegWriteA( GRACC	, 0x10 );					// 0x0382	[ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ]
	
						
	if( AccWit( 0x10 ) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }			/* Digital Gyro busy wait 				*/
	
	UcRamIni1 = 0x01 ;
	do{
		RegWriteA( GRACC,	0x01 ) ;				/* 0x0382	Set 1Byte Read Trigger ON			*/
		if( AccWit( 0x01 ) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/
		RegReadA( GRADT0H, &UcRamIni1 ) ;			/* 0x0390	*/
		UcCntPlb++ ;
	} while( ( UcRamIni1 & 0x01 ) && ( UcCntPlb < INIDGY_POLLING_LIMIT_B ) ) ;
	if( UcCntPlb == INIDGY_POLLING_LIMIT_B ) { return OIS_FW_POLLING_FAIL; }
	
	RegWriteA( GIFDIV, UcRegBak ) ;			// 48MHz / 3 = 16MHz
	RegWriteA( GRINI, 0x80 ) ;				// SPI Clock = 16MHz / 4 = 4MHz
	
	// Gyro Signal Output Select
	RegWriteA( GRADR0,	0x43 ) ;			// 0x0383	Set Gyro XOUT H~L
	RegWriteA( GRADR1,	0x45 ) ;			// 0x0384	Set Gyro YOUT H~L
	
	/*Start OIS Reading*/
	RegWriteA( GRSEL	, 0x02 );							// 0x0380	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]
	
	return OIS_FW_POLLING_PASS;
}

void	IniMon( void )
{
	RegWriteA( PWMMONFC, 0x00 ) ;				// 0x00F4	
	RegWriteA( DAMONFC, 0x81 ) ;				// 0x00F5	
	
	RegWriteA( MONSELA, 0x57 ) ;				// 0x0270	
	RegWriteA( MONSELB, 0x58 ) ;				// 0x0271	
	RegWriteA( MONSELC, 0x56 ) ;				// 0x0272	
	RegWriteA( MONSELD, 0x63 ) ;				// 0x0273
	
	return;
}

//********************************************************************************
// Function Name 	: IniSrv
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Servo Initial Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniSrv( void )
{
	RegWriteA( LXEQEN 	, 0x00 );				// 0x0084
	RegWriteA( LYEQEN 	, 0x00 );				// 0x008E
	
	RamWriteA( ADHXOFF,   0x0000 ) ;			// 0x1102
	RamWriteA( ADSAD4OFF, 0x0000 ) ;			// 0x110E
	RamWriteA( HXINOD,    0x0000 ) ;			// 0x1127
	RamWriteA( HXDCIN,    0x0000 ) ;			// 0x1126
	RamWriteA( HXSEPT1,    0x0000 ) ;			// 0x1123
	RamWriteA( HXSEPT2,    0x0000 ) ;			// 0x1124
	RamWriteA( HXSEPT3,    0x0000 ) ;			// 0x1125
	RamWriteA( LXDOBZ,     0x0000 ) ;			// 0x114A
	RamWriteA( LXFZF,      0x0000 ) ;			// 0x114B
	RamWriteA( LXFZB,      0x0000 ) ;			// 0x1156
	RamWriteA( LXDX,      0x0000 ) ;			// 0x1148
	RamWriteA( LXLMT,     0x7FFF ) ;			// 0x1157
	RamWriteA( LXLMT2,    0x7FFF ) ;			// 0x1158
	RamWriteA( LXLMTSD,   0x0000 ) ;			// 0x1159
	RamWriteA( PLXOFF,    0x0000 ) ;			// 0x115B
	RamWriteA( LXDODAT,   0x0000 ) ;			// 0x115A
	
	RamWriteA( ADHYOFF,   0x0000 ) ;			// 0x1105
	RamWriteA( HYINOD,    0x0000 ) ;			// 0x1167
	RamWriteA( HYDCIN,    0x0000 ) ;			// 0x1166
	RamWriteA( HYSEPT1,    0x0000 ) ;			// 0x1163
	RamWriteA( HYSEPT2,    0x0000 ) ;			// 0x1164
	RamWriteA( HYSEPT3,    0x0000 ) ;			// 0x1165
	RamWriteA( LYDOBZ,     0x0000 ) ;			// 0x118A
	RamWriteA( LYFZF,      0x0000 ) ;			// 0x118B
	RamWriteA( LYFZB,      0x0000 ) ;			// 0x1196
	RamWriteA( LYDX,      0x0000 ) ;			// 0x1188
	RamWriteA( LYLMT,     0x7FFF ) ;			// 0x1197
	RamWriteA( LYLMT2,    0x7FFF ) ;			// 0x1198
	RamWriteA( LYLMTSD,   0x0000 ) ;			// 0x1199
	RamWriteA( PLYOFF,    0x0000 ) ;			// 0x119B
	RamWriteA( LYDODAT,   0x0000 ) ;			// 0x119A

	RamWriteA( LXOLMT,		0x2C00 );			// 0x121A		// 165mA Limiter for CVL Mode
	RamWriteA( LYOLMT,		0x2C00 );			// 0x121B		// 165mA Limiter for CVL Mode
	RegWriteA( LXEQFC2 , 0x09 ) ;				// 0x0083		Linear Mode ON
	RegWriteA( LYEQFC2 , 0x09 ) ;				// 0x008D		

	/* Calculation flow   X : Y1->X1    Y : X2->Y2 */
	RegWriteA( LCXFC, (unsigned char)0x00 ) ;			// 0x0001	High-order function X function setting
	RegWriteA( LCYFC, (unsigned char)0x00 ) ;			// 0x0006	High-order function Y function setting

	RegWriteA( LCY1INADD, (unsigned char)LXDOIN ) ;		// 0x0007	High-order function Y1 input selection
	RegWriteA( LCY1OUTADD, (unsigned char)DLY00 ) ;		// 0x0008	High-order function Y1 output selection
	RegWriteA( LCX1INADD, (unsigned char)DLY00 ) ;		// 0x0002	High-order function X1 input selection
	RegWriteA( LCX1OUTADD, (unsigned char)LXADOIN ) ;	// 0x0003	High-order function X1 output selection

	RegWriteA( LCX2INADD, (unsigned char)LYDOIN ) ;		// 0x0004	High-order function X2 input selection
	RegWriteA( LCX2OUTADD, (unsigned char)DLY01 ) ;		// 0x0005	High-order function X2 output selection
	RegWriteA( LCY2INADD, (unsigned char)DLY01 ) ;		// 0x0009	High-order function Y2 input selection
	RegWriteA( LCY2OUTADD, (unsigned char)LYADOIN ) ;	// 0x000A	High-order function Y2 output selection

	/* (0.5468917X^3+0.3750114X)*(0.5468917X^3+0.3750114X) 6.5ohm*/
	RamWriteA( LCY1A0, 0x0000 ) ;			// 0x12F2	0
	RamWriteA( LCY1A1, 0x4600 ) ;			// 0x12F3	1
	RamWriteA( LCY1A2, 0x0000 ) ;			// 0x12F4	2
	RamWriteA( LCY1A3, 0x3000 ) ;			// 0x12F5	3
	RamWriteA( LCY1A4, 0x0000 ) ;			// 0x12F6	4
	RamWriteA( LCY1A5, 0x0000 ) ;			// 0x12F7	5
	RamWriteA( LCY1A6, 0x0000 ) ;			// 0x12F8	6

	RamWriteA( LCX1A0, 0x0000 ) ;			// 0x12D2	0
	RamWriteA( LCX1A1, 0x4600 ) ;			// 0x12D3	1
	RamWriteA( LCX1A2, 0x0000 ) ;			// 0x12D4	2
	RamWriteA( LCX1A3, 0x3000 ) ;			// 0x12D5	3
	RamWriteA( LCX1A4, 0x0000 ) ;			// 0x12D6	4
	RamWriteA( LCX1A5, 0x0000 ) ;			// 0x12D7	5
	RamWriteA( LCX1A6, 0x0000 ) ;			// 0x12D8	6

	RamWriteA( LCX2A0, 0x0000 ) ;			// 0x12D9	0
	RamWriteA( LCX2A1, 0x4600 ) ;			// 0x12DA	1
	RamWriteA( LCX2A2, 0x0000 ) ;			// 0x12DB	2
	RamWriteA( LCX2A3, 0x3000 ) ;			// 0x12DC	3
	RamWriteA( LCX2A4, 0x0000 ) ;			// 0x12DD	4
	RamWriteA( LCX2A5, 0x0000 ) ;			// 0x12DE	5
	RamWriteA( LCX2A6, 0x0000 ) ;			// 0x12DF	6
	
	RamWriteA( LCY2A0, 0x0000 ) ;			// 0x12F9	0
	RamWriteA( LCY2A1, 0x4600 ) ;			// 0x12FA	1
	RamWriteA( LCY2A2, 0x0000 ) ;			// 0x12FB	2
	RamWriteA( LCY2A3, 0x3000 ) ;			// 0x12FC	3
	RamWriteA( LCY2A4, 0x0000 ) ;			// 0x12FD	4
	RamWriteA( LCY2A5, 0x0000 ) ;			// 0x12FE	5
	RamWriteA( LCY2A6, 0x0000 ) ;			// 0x12FF	6
	
	RegWriteA( GDPX1INADD,  0x00 ) ;		// 0x00A5	Default Setting
	RegWriteA( GDPX1OUTADD, 0x00 ) ;		// 0x00A6	General Data Pass Output Cut Off
	RegWriteA( GDPX2INADD,  0x00 ) ;		// 0x00A7	Default Setting
	RegWriteA( GDPX2OUTADD, 0x00 ) ;		// 0x00A8	General Data Pass Output Cut Off

	// Gyro Filter Interface
	RegWriteA( GYINFC, 0x00 ) ;				// 0x00DA		LXGZB,LYGZB Input Cut Off, 0 Sampling Delay, Down Sampling 1/1
	
	// Sin Wave Generater
	RegWriteA( SWEN, 0x08 ) ;				// 0x00DB		Sin Wave Generate OFF, Sin Wave Setting
	RegWriteA( SWFC2, 0x08 ) ;				// 0x00DE		SWC = 0
	
	// Delay RAM Monitor
	RegWriteA( MDLY1ADD, 0x10 ) ;			// 0x00E5		Delay Monitor1
	RegWriteA( MDLY2ADD, 0x11 ) ;			// 0x00E6		Delay Monitor2
	
	// Delay RAM Clear
	//BsyWit( DLYCLR, 0xFF ) ;				// 0x00EE	Delay RAM All Clear
	//BsyWit( DLYCLR2, 0xEC ) ;				// 0x00EF	Delay RAM All Clear
	RegWriteA( DLYCLR, 0xFF ) ;				// 0x00EE	Delay RAM All Clear
	WitTim(1);
	RegWriteA( DLYCLR2, 0xEC ) ;				// 0x00EF	Delay RAM All Clear
	WitTim(1);
	RegWriteA( DLYCLR	, 0x00 );			// 0x00EE	CLR disable
	
	// Hall Amp...
	RegWriteA( RTXADD, 0x00 ) ;				// 0x00CE	Cal OFF
	RegWriteA( RTYADD, 0x00 ) ;				// 0x00E8	Cal OFF
	
	// PWM Signal Generate
	DrvSw( OFF ) ;							/* 0x0070	Driver Block Ena=0 */
	RegWriteA( DRVFC2	, 0x40 );			// 0x0068	PriDriver:Slope, Driver:DeadTime 12.5ns
	
	RegWriteA( PWMFC,   0x5C ) ;			// 0x0075	VREF, PWMCLK/64, MODE1, 12Bit Accuracy
	RegWriteA( PWMPERIODX, 0x00 );			// 0x0062
	RegWriteA( PWMPERIODY, 0x00 );			// 0x0063
	
	RegWriteA( PWMA,    0x00 ) ;			// 0x0074	PWM X/Y standby
	RegWriteA( PWMDLY1,  0x03 ) ;			// 0x0076	X Phase Delay Setting
	RegWriteA( PWMDLY2,  0x03 ) ;			// 0x0077	Y Phase Delay Setting
	
	RegWriteA( LNA		, 0xC0 );			// 0x0078	Low Noise mode enable
	RegWriteA( LNFC 	, 0x02 );			// 0x0079
	RegWriteA( LNSMTHX	, 0x80 );			// 0x007A
	RegWriteA( LNSMTHY	, 0x80 );			// 0x007B
	
	// Flag Monitor
	RegWriteA( FLGM, 0xCC ) ;				// 0x00F8	BUSY2 Output ON
	RegWriteA( FLGIST, 0xCC ) ;				// 0x00F9	Interrupt Clear
	RegWriteA( FLGIM2, 0xF8 ) ;				// 0x00FA	BUSY2 Output ON
	RegWriteA( FLGIST2, 0xF8 ) ;			// 0x00FB	Interrupt Clear
	
	// Function Setting
	RegWriteA( FCSW, 0x00 ) ;				// 0x00F6	2Axis Input, PWM Mode, X,Y Axis Reverse OFF
	RegWriteA( FCSW2, 0x00 ) ;				// 0x00F7	X,Y Axis Invert OFF, PWM Synchronous, A/D Over Sampling ON
	
	/* Srv Smooth start */
	RamWriteA( HXSMSTP   , 0x0400 ) ;					/* 0x1120	*/
	RamWriteA( HYSMSTP   , 0x0400 ) ;					/* 0x1160	*/
	
	RegWriteA( SSSFC1, 0x43 ) ;				// 0x0098	0.68ms * 8times = 5.46ms
	RegWriteA( SSSFC2, 0x03 ) ;				// 0x0099	1.36ms * 3 = 4.08ms
	RegWriteA( SSSFC3, 0x50 ) ;				// 0x009A	1.36ms
	
	RegWriteA( STBB, 0x00 ) ;				// 0x0260	All standby

	return;
}

void	IniGyr( void )
{
	RegWriteA( GEQON	, 0x00 );				// 0x0100		[ - | - | - | - ][ - | - | - | CmEqOn ]
	
	/*Initialize Gyro RAM*/
	ClrGyr( 0x00 , 0x03 );
	
	/*Gyro Filter Setting*/
	RegWriteA( GGADON	, 0x01 );		// 0x011C		[ - | - | - | CmSht2PanOff ][ - | - | CmGadOn(1:0) ]
	RegWriteA( GGADSMPT , 0x0E);		// 0x011F
	
	// Limiter
	RamWrite32A( gxlmt1L, 0x00000000 ) ;	// 0x18B0
	RamWrite32A( gxlmt1H, 0x3F800000 ) ;	// 0x18B1		
	RamWrite32A( gxlmt2L, 0x3B16BB99 ) ;	// 0x18B2	0.0023
	RamWrite32A( gxlmt2H, 0x3F800000 ) ;	// 0x18B3
	
	RamWrite32A( gylmt1L, 0x00000000 ) ;	// 0x19B0
	RamWrite32A( gylmt1H, 0x3F800000 ) ;	// 0x19B1		
	RamWrite32A( gylmt2L, 0x3B16BB99 ) ;	// 0x19B2	0.0023
	RamWrite32A( gylmt2H, 0x3F800000 ) ;	// 0x19B3
	
	// Limiter3
	RamWrite32A( gxlmt3H0, 0x3E800000 ) ;	// 0x18B4
	RamWrite32A( gylmt3H0, 0x3E800000 ) ;	// 0x19B4	Limiter = 0.25, 0.7deg
	RamWrite32A( gxlmt3H1, 0x3E800000 ) ;	// 0x18B5
	RamWrite32A( gylmt3H1, 0x3E800000 ) ;	// 0x19B5

	RamWrite32A( gxlmt4H0, GYRLMT_S1 ) ;	//0x1808	X Axis Limiter4 High Threshold 0
	RamWrite32A( gylmt4H0, GYRLMT_S1 ) ;	//0x1908	Y Axis Limiter4 High Threshold 0
	
	RamWrite32A( gxlmt4H1, GYRLMT_S2 ) ;	//0x1809	X Axis Limiter4 High Threshold 1
	RamWrite32A( gylmt4H1, GYRLMT_S2 ) ;	//0x1909	Y Axis Limiter4 High Threshold 1
	
	// Monitor Circuit
	RegWriteA( GDLYMON10, 0xF5 ) ;			// 0x0184
	RegWriteA( GDLYMON11, 0x01 ) ;			// 0x0185
	RegWriteA( GDLYMON20, 0xF5 ) ;			// 0x0186
	RegWriteA( GDLYMON21, 0x00 ) ;			// 0x0187
	RamWrite32A( gdm1g, 0x3F800000 ) ;		// 0x18AC
	RamWrite32A( gdm2g, 0x3F800000 ) ;		// 0x19AC
	
	/* Pan/Tilt parameter */
	RegWriteA( GPANADDA, 0x14 ) ;			// 0x0130	GXH1Z2/GYH1Z2
	RegWriteA( GPANADDB, 0x0E ) ;			// 0x0131	GXI3Z/GYI3Z
	
	//Threshold
	RamWrite32A( SttxHis, 	0x00000000 );			// 0x183F
	RamWrite32A( SttyHis, 	0x00000000 );			// 0x193F
	RamWrite32A( SttxaL, 	0x00000000 );			// 0x18AE
	RamWrite32A( SttxbL, 	0x00000000 );			// 0x18BE
	RamWrite32A( Sttx12aM, 	GYRA12_MID_DEG );		// 0x184F
	RamWrite32A( Sttx12aH, 	GYRA12_HGH_DEG );		// 0x185F
	RamWrite32A( Sttx12bM, 	GYRB12_MID );			// 0x186F
	RamWrite32A( Sttx12bH, 	GYRB12_HGH );			// 0x187F
	RamWrite32A( Sttx34aM, 	GYRA34_MID_DEG );		// 0x188F
	RamWrite32A( Sttx34aH, 	GYRA34_HGH_DEG );		// 0x189F
	RamWrite32A( Sttx34bM, 	GYRB34_MID );			// 0x18AF
	RamWrite32A( Sttx34bH, 	GYRB34_HGH );			// 0x18BF
	RamWrite32A( SttyaL, 	0x00000000 );			// 0x19AE
	RamWrite32A( SttybL, 	0x00000000 );			// 0x19BE
	RamWrite32A( Stty12aM, 	GYRA12_MID_DEG );		// 0x194F
	RamWrite32A( Stty12aH, 	GYRA12_HGH_DEG );		// 0x195F
	RamWrite32A( Stty12bM, 	GYRB12_MID );			// 0x196F
	RamWrite32A( Stty12bH, 	GYRB12_HGH );			// 0x197F
	RamWrite32A( Stty34aM, 	GYRA34_MID_DEG );		// 0x198F
	RamWrite32A( Stty34aH, 	GYRA34_HGH_DEG );		// 0x199F
	RamWrite32A( Stty34bM, 	GYRB34_MID );			// 0x19AF
	RamWrite32A( Stty34bH, 	GYRB34_HGH );			// 0x19BF
	
	// Phase Transition Setting
	RegWriteA( GPANSTT31JUG0, 	0x01 );		// 0x0142
	RegWriteA( GPANSTT31JUG1, 	0x00 );		// 0x0143
	RegWriteA( GPANSTT13JUG0, 	0x00 );		// 0x0148
	RegWriteA( GPANSTT13JUG1, 	0x07 );		// 0x0149
	
	// State Timer
	//RegWriteA( GPANSTT1LEVTMR, 	0x01 );		// 0x0160
	RegWriteA( GPANSTT1LEVTMR, 	0x00 );		// 0x0160
	
	// Control filter
	RegWriteA( GPANTRSON0, 		0x01 );		// 0x0132		// gxgain/gygain, gxistp/gyistp
	RegWriteA( GPANTRSON1, 		0x1C );		// 0x0133		// I Filter Control
	
	// State Setting
	RegWriteA( GPANSTTSETGAIN, 	0x10 );		// 0x0155
	RegWriteA( GPANSTTSETISTP, 	0x10 );		// 0x0156
	RegWriteA( GPANSTTSETI1FTR,	0x10 );		// 0x0157
	RegWriteA( GPANSTTSETI2FTR,	0x10 );		// 0x0158
	
	// State2,4 Step Time Setting
	RegWriteA( GPANSTT2TMR0,	0xEA );		// 0x013C
	RegWriteA( GPANSTT2TMR1,	0x00 );		// 0x013D
	RegWriteA( GPANSTT4TMR0,	0x92 );		// 0x013E
	RegWriteA( GPANSTT4TMR1,	0x04 );		// 0x013F
	
	RegWriteA( GPANSTTXXXTH,	0x0F );		// 0x015D

	RegWriteA( GPANSTTSETILHLD,	0x00 );		// 0x0168
	
	RamWrite32A( gxlevmid, TRI_LEVEL );					// 0x182D	Low Th
	RamWrite32A( gxlevhgh, TRI_HIGH );					// 0x182E	Hgh Th
	RamWrite32A( gylevmid, TRI_LEVEL );					// 0x192D	Low Th
	RamWrite32A( gylevhgh, TRI_HIGH );					// 0x192E	Hgh Th
	RamWrite32A( gxadjmin, XMINGAIN );					// 0x18BA	Low gain
	RamWrite32A( gxadjmax, XMAXGAIN );					// 0x18BB	Hgh gain
	RamWrite32A( gxadjdn, XSTEPDN );					// 0x18BC	-step
	RamWrite32A( gxadjup, XSTEPUP );					// 0x18BD	+step
	RamWrite32A( gyadjmin, YMINGAIN );					// 0x19BA	Low gain
	RamWrite32A( gyadjmax, YMAXGAIN );					// 0x19BB	Hgh gain
	RamWrite32A( gyadjdn, YSTEPDN );					// 0x19BC	-step
	RamWrite32A( gyadjup, YSTEPUP );					// 0x19BD	+step
	
	RegWriteA( GLEVGXADD, (unsigned char)XMONADR );		// 0x0120	Input signal
	RegWriteA( GLEVGYADD, (unsigned char)YMONADR );		// 0x0124	Input signal
	RegWriteA( GLEVTMR, 		TIMEBSE );				// 0x0128	Base Time
	RegWriteA( GLEVTMRLOWGX, 	TIMELOW );				// 0x0121	X Low Time
	RegWriteA( GLEVTMRMIDGX, 	TIMEMID );				// 0x0122	X Mid Time
	RegWriteA( GLEVTMRHGHGX, 	TIMEHGH );				// 0x0123	X Hgh Time
	RegWriteA( GLEVTMRLOWGY, 	TIMELOW );				// 0x0125	Y Low Time
	RegWriteA( GLEVTMRMIDGY, 	TIMEMID );				// 0x0126	Y Mid Time
	RegWriteA( GLEVTMRHGHGY, 	TIMEHGH );				// 0x0127	Y Hgh Time
	RegWriteA( GADJGANADD, (unsigned char)GANADR );		// 0x012A	control address
	RegWriteA( GADJGANGO, 		0x00 );					// 0x0108	manual off
	
	SetPanTiltMode( OFF ) ;								/* Pan/Tilt OFF */

	return;
}

int	IniHfl( void )
{
	unsigned short	UsAryIda, UsAryIdb ;
	
	// Hall&Gyro Register Parameter Setting
	UsAryIda	= 0 ;
	while( CsHalReg[UcVerLow][ UsAryIda].UsRegAdd != 0xFFFF )
	{
		RegWriteA( CsHalReg[UcVerLow][ UsAryIda].UsRegAdd, CsHalReg[UcVerLow][ UsAryIda].UcRegDat ) ;
		UsAryIda++ ;
		if( UsAryIda > HALREGTAB ){ return OIS_FW_POLLING_FAIL ; }
	}
	
	// Hall Filter Parameter Setting
	UsAryIdb	= 0 ;
	while( CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd != 0xFFFF )
	{
		if( CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd < gag ) {
			RamWriteA( CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd, CsHalFil[UcVerLow][ UsAryIdb].UsRamDat ) ;
		}
		UsAryIdb++ ;
		if( UsAryIdb > HALFILTAB ){ return OIS_FW_POLLING_FAIL ; }
	}
	
	if( (unsigned char)(StCalDat.UsVerDat) <= (unsigned char)0x01 ){		// X Reverse 
		RamWriteA( plxg, 0x8001 );
	}
	
	return OIS_FW_POLLING_PASS ;
}

int	IniGfl( void )
{
	unsigned short	UsAryIda ;
	
	// Gyro Filter Parameter Setting
	UsAryIda	= 0 ;

	while( CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd != 0xFFFF )
	{
		if( ( CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd & 0xFEFF ) < gxi2a_a ) {
			RamWrite32A( CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd, CsGyrFil[UcVerLow][ UsAryIda].UlRamDat ) ;
		}
		UsAryIda++ ;
		if( UsAryIda > GYRFILTAB ){ return OIS_FW_POLLING_FAIL ; }
	}

	return OIS_FW_POLLING_PASS ;
}

void	IniAdj( void )
{
//	unsigned long	UlLpfCof ;
	
	RegWriteA( CMSDAC, BIAS_CUR ) ;				// 0x0261	Hall Dac Current
	RegWriteA( OPGSEL, AMP_GAIN ) ;				// 0x0262	Hall Amp Gain
	
	/* Hall Xaxis Bias,Offset */
	if( (StCalDat.UsAdjCompF == 0x0000 ) || (StCalDat.UsAdjCompF == 0xFFFF ) || (StCalDat.UsAdjCompF & ( EXE_HXADJ - EXE_END )) ){
		RamWriteA( DAHLXO, DAHLXO_INI ) ;	// 0x1114
		RamWriteA( DAHLXB, DAHLXB_INI ) ;	// 0x1115
		RamWriteA( ADHXOFF, 0x0000 ) ;		// 0x1102
	}else{
		RamWriteA( DAHLXO, StCalDat.StHalAdj.UsHlxOff ) ;	// 0x1114
		RamWriteA( DAHLXB, StCalDat.StHalAdj.UsHlxGan ) ;	// 0x1115
		RamWriteA( ADHXOFF, StCalDat.StHalAdj.UsAdxOff ) ;	// 0x1102
	}
	
	/* Hall Yaxis Bias,Offset */
	if( (StCalDat.UsAdjCompF == 0x0000 ) || (StCalDat.UsAdjCompF == 0xFFFF ) || (StCalDat.UsAdjCompF & ( EXE_HYADJ - EXE_END )) ){
		RamWriteA( DAHLYO, DAHLYO_INI ) ;	// 0x1116
		RamWriteA( DAHLYB, DAHLYB_INI ) ;	// 0x1117
		RamWriteA( ADHYOFF, 0x0000 ) ;		// 0x1105
	}else{
		RamWriteA( DAHLYO, StCalDat.StHalAdj.UsHlyOff ) ;	// 0x1116
		RamWriteA( DAHLYB, StCalDat.StHalAdj.UsHlyGan ) ;	// 0x1117
		RamWriteA( ADHYOFF, StCalDat.StHalAdj.UsAdyOff ) ;	// 0x1105
	}
	
	/* Hall Xaxis Loop Gain */
	if( (StCalDat.UsAdjCompF == 0x0000 ) || (StCalDat.UsAdjCompF == 0xFFFF ) || (StCalDat.UsAdjCompF & ( EXE_LXADJ - EXE_END )) ){
		RamWriteA( lxgain, LXGAIN_INI ) ;	// 0x132A
	}else{
		RamWriteA( lxgain, StCalDat.StLopGan.UsLxgVal ) ;	// 0x132A
	}
	
	/* Hall Yaxis Loop Gain */
	if( (StCalDat.UsAdjCompF == 0x0000 ) || (StCalDat.UsAdjCompF == 0xFFFF ) || (StCalDat.UsAdjCompF & ( EXE_LYADJ - EXE_END )) ){
		RamWriteA( lygain, LYGAIN_INI ) ;	// 0x136A
	}else{
		RamWriteA( lygain, StCalDat.StLopGan.UsLygVal ) ;	// 0x136A
	}
	
	/* Lens Center */
	// X axis Lens Center Offset Read & Setting
	if( ( StCalDat.StLenCen.UsLsxVal != 0x0000 ) && ( StCalDat.StLenCen.UsLsxVal != 0xffff )){
		UsCntXof = StCalDat.StLenCen.UsLsxVal ;					/* Set Lens center X value */
	} else {
		UsCntXof = MECCEN_X ;						/* Clear Lens center X value */
	}
	RamWriteA( HXINOD, UsCntXof ) ;				// 0x1127
	
	// Y axis Lens Center Offset Read & Setting
	if( ( StCalDat.StLenCen.UsLsyVal != 0x0000 ) && ( StCalDat.StLenCen.UsLsyVal != 0xffff )){
		UsCntYof = StCalDat.StLenCen.UsLsyVal ;					/* Set Lens center Y value */
	} else {
		UsCntYof = MECCEN_Y ;						/* Clear Lens center Y value */
	}
	RamWriteA( HYINOD, UsCntYof ) ;				// 0x1167
	
	/* Gyro Xaxis Offset */
	if( ( StCalDat.StGvcOff.UsGxoVal == 0x0000 ) || ( StCalDat.StGvcOff.UsGxoVal == 0xffff )){
		RamWriteA( ADGXOFF, 0x0000 ) ;							// 0x1108
		RegWriteA( IZAH, DGYRO_OFST_XH ) ;						// 0x03A0		Set Offset High byte
		RegWriteA( IZAL, DGYRO_OFST_XL ) ;						// 0x03A1		Set Offset Low byte
	}else{
		RamWriteA( ADGXOFF, 0x0000 ) ;							// 0x1108
		RegWriteA( IZAH, (unsigned char)(StCalDat.StGvcOff.UsGxoVal >> 8) ) ;	// 0x03A0		Set Offset High byte
		RegWriteA( IZAL, (unsigned char)(StCalDat.StGvcOff.UsGxoVal) ) ;			// 0x03A1		Set Offset Low byte
	}
	
	/* Gyro Yaxis Offset */
	if( ( StCalDat.StGvcOff.UsGyoVal == 0x0000 ) || ( StCalDat.StGvcOff.UsGyoVal == 0xffff )){
		RamWriteA( ADGYOFF, 0x0000 ) ;							// 0x110B
		RegWriteA( IZBH, DGYRO_OFST_YH ) ;						// 0x03A2		Set Offset High byte
		RegWriteA( IZBL, DGYRO_OFST_YL ) ;						// 0x03A3		Set Offset Low byte
	}else{
		RamWriteA( ADGYOFF, 0x0000 ) ;							// 0x110B
		RegWriteA( IZBH, (unsigned char)(StCalDat.StGvcOff.UsGyoVal >> 8) ) ;	// 0x03A2		Set Offset High byte
		RegWriteA( IZBL, (unsigned char)(StCalDat.StGvcOff.UsGyoVal) ) ;			// 0x03A3		Set Offset Low byte
	}
	
	/* Gyro Xaxis Gain */
	if( ( StCalDat.UlGxgVal != 0x00000000 ) && ( StCalDat.UlGxgVal != 0xffffffff )){
		RamWrite32A( gxzoom, StCalDat.UlGxgVal ) ;		// 0x1828 Gyro X axis Gain adjusted value
	}else{
		RamWrite32A( gxzoom, GXGAIN_INI ) ;		// 0x1828 Gyro X axis Gain adjusted initial value
	}
	
	/* Gyro Yaxis Gain */
	if( ( StCalDat.UlGygVal != 0x00000000 ) && ( StCalDat.UlGygVal != 0xffffffff )){
		RamWrite32A( gyzoom, StCalDat.UlGygVal ) ;		// 0x1928 Gyro Y axis Gain adjusted value
	}else{
		RamWrite32A( gyzoom, GYGAIN_INI ) ;		// 0x1928 Gyro Y axis Gain adjusted initial value
	}
	
	/* OSC Clock value */
	if( ((unsigned char)StCalDat.UsOscVal != 0x00) && ((unsigned char)StCalDat.UsOscVal != 0xff) ){
		RegWriteA( OSCSET, (unsigned char)( (unsigned char)StCalDat.UsOscVal | 0x01 ) ) ;		// 0x0264
	}else{
		RegWriteA( OSCSET, OSC_INI ) ;						// 0x0264
	}
	
	RamWriteA( hxinog, 0x8001 ) ;			// 0x1128	back up initial value
	RamWriteA( hyinog, 0x8001 ) ;			// 0x1168	back up initial value
	
	RegWriteA( STBB 	, 0x0F );							// 0x0260 	[ - | - | - | - ][ STBOPAY | STBOPAX | STBDAC | STBADC ]
	
	RegWriteA( LXEQEN 	, 0x45 );			// 0x0084
	RegWriteA( LYEQEN 	, 0x45 );			// 0x008E

	RamWrite32A( gxistp_1, 0x00000000 );
	RamWrite32A( gyistp_1, 0x00000000 );
	

	// I Filter X							// 1s
	RamWrite32A( gxi1a_1, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gxi1b_1, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gxi1c_1, 0x33E6A3C6 ) ;		// Up

	RamWrite32A( gxi1a_a, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gxi1b_a, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gxi1c_a, 0x33E6A3C6 ) ;		// Up

	RamWrite32A( gxi1a_b, 0x3AAF73A1 ) ;		// 5Hz
	RamWrite32A( gxi1b_b, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gxi1c_b, 0x3F800000 ) ;		// Up

	RamWrite32A( gxi1a_c, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gxi1b_c, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gxi1c_c, 0x33E6A3C6 ) ;		// Up

	// I Filter Y
	RamWrite32A( gyi1a_1, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gyi1b_1, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gyi1c_1, 0x33E6A3C6 ) ;		// Up

	RamWrite32A( gyi1a_a, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gyi1b_a, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gyi1c_a, 0x33E6A3C6 ) ;		// Up

	RamWrite32A( gyi1a_b, 0x3AAF73A1 ) ;		// 5Hz
	RamWrite32A( gyi1b_b, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gyi1c_b, 0x3F800000 ) ;		// Up

	RamWrite32A( gyi1a_c, 0x38A8A554 ) ;		// 0.3Hz
	RamWrite32A( gyi1b_c, 0xB3E6A3C6 ) ;		// Down
	RamWrite32A( gyi1c_c, 0x33E6A3C6 ) ;		// Up

	// gxgain								// 0.4s
	RamWrite32A( gxl4a_1, 0x3F800000 ) ;		// 1
	RamWrite32A( gxl4b_1, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gxl4c_1, 0x39DFB23C ) ;		// Up
	RamWrite32A( gxl4a_a, 0x3F800000 ) ;		// 1
	RamWrite32A( gxl4b_a, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gxl4c_a, 0x39DFB23C ) ;		// Up
	RamWrite32A( gxl4a_b, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gxl4b_b, 0xBF800000 ) ;		// Down
	RamWrite32A( gxl4c_b, 0x39DFB23C ) ;		// Up
	RamWrite32A( gxl4a_c, 0x3F800000 ) ;		// 1
	RamWrite32A( gxl4b_c, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gxl4c_c, 0x39DFB23C ) ;		// Up

	// gygain
	RamWrite32A( gyl4a_1, 0x3F800000 ) ;		// 1
	RamWrite32A( gyl4b_1, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gyl4c_1, 0x39DFB23C ) ;		// Up
	RamWrite32A( gyl4a_a, 0x3F800000 ) ;		// 1
	RamWrite32A( gyl4b_a, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gyl4c_a, 0x39DFB23C ) ;		// Up
	RamWrite32A( gyl4a_b, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gyl4b_b, 0xBF800000 ) ;		// Down
	RamWrite32A( gyl4c_b, 0x39DFB23C ) ;		// Up
	RamWrite32A( gyl4a_c, 0x3F800000 ) ;		// 1
	RamWrite32A( gyl4b_c, 0xB9DFB23C ) ;		// Down
	RamWrite32A( gyl4c_c, 0x39DFB23C ) ;		// Up

	// gxistp								// 0.2s
	RamWrite32A( gxgyro_1, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gxgain_1, 0xBF800000 ) ;		// Down
	RamWrite32A( gxistp_1, 0x3F800000 ) ;		// Up
	RamWrite32A( gxgyro_a, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gxgain_a, 0xBF800000 ) ;		// Down
	RamWrite32A( gxistp_a, 0x3F800000 ) ;		// Up
	RamWrite32A( gxgyro_b, 0x37EC6C50 ) ;		// -91dB
	RamWrite32A( gxgain_b, 0xBF800000 ) ;		// Down
	RamWrite32A( gxistp_b, 0x3F800000 ) ;		// Up
	RamWrite32A( gxgyro_c, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gxgain_c, 0xBF800000 ) ;		// Down
	RamWrite32A( gxistp_c, 0x3F800000 ) ;		// Up

	// gyistp
	RamWrite32A( gygyro_1, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gygain_1, 0xBF800000 ) ;		// Down
	RamWrite32A( gyistp_1, 0x3F800000 ) ;		// Up
	RamWrite32A( gygyro_a, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gygain_a, 0xBF800000 ) ;		// Down
	RamWrite32A( gyistp_a, 0x3F800000 ) ;		// Up
	RamWrite32A( gygyro_b, 0x37EC6C50 ) ;		// -91dB
	RamWrite32A( gygain_b, 0xBF800000 ) ;		// Down
	RamWrite32A( gyistp_b, 0x3F800000 ) ;		// Up
	RamWrite32A( gygyro_c, 0x00000000 ) ;		// Cut Off
	RamWrite32A( gygain_c, 0xBF800000 ) ;		// Down
	RamWrite32A( gyistp_c, 0x3F800000 ) ;		// Up

	/* exe function */
	AutoGainControlSw( OFF ) ;					/* Auto Gain Control Mode OFF */
	
	DrvSw( ON ) ;								/* 0x0070		Driver Mode setting */
	
//	RegWriteA( G2NDCEFON0, 0x03 ) ;				// 0x0106
	RegWriteA( GEQON	, 0x01 );				// 0x0100		[ - | - | - | - ][ - | - | - | CmEqOn ]
	RegWriteA( GPANFILMOD, 0x01 ) ;				// 0x0167
	SetPanTiltMode( ON ) ;						/* Pan/Tilt ON */
	RegWriteA( GPANSTTFRCE, 0x44 ) ;			// 0x010A
	RegWriteA( GLSEL, 0x04 ) ;					// 0x017A
	
	RegWriteA( GHCHR, 0x11 ) ;					// 0x017B
	
	RamWrite32A( gxl2a_2, 0x00000000 ) ;	// 0x1860	0
	RamWrite32A( gxl2b_2, 0x3D829952 ) ;	// 0x1861	0.063769
	RamWrite32A( gyl2a_2, 0x00000000 ) ;	// 0x1960	0
	RamWrite32A( gyl2b_2, 0x3D829952 ) ;	// 0x1961	0.063769

	RamWrite32A( gxl2c_2, 0xBA9CD414 ) ;	// 0x1862	-0.001196506	3F7FF800
	RamWrite32A( gyl2c_2, 0xBA9CD414 ) ;	// 0x1962	-0.001196506	3F7FF800
	
	//RamWrite32A( gxh1c_2, 0x3F7FFD00 ) ;	// 0x18A2
	//RamWrite32A( gyh1c_2, 0x3F7FFD00 ) ;	// 0x19A2
	RamWrite32A( gxh1c_2, 0x3F7FFD80 ) ;	// 0x18A2
	RamWrite32A( gyh1c_2, 0x3F7FFD80 ) ;	// 0x19A2

	RegWriteA( GPANSTTFRCE, 0x11 ) ;									// 0x010A

	return;
}

void	MemClr( unsigned char	*NcTgtPtr, unsigned short	UsClrSiz )
{
	unsigned short	UsClrIdx ;
	
	for ( UsClrIdx = 0 ; UsClrIdx < UsClrSiz ; UsClrIdx++ )
	{
		*NcTgtPtr	= 0 ;
		NcTgtPtr++ ;
	}

	return;
}

int	AccWit( unsigned char UcTrgDat )
{
	unsigned char	UcFlgVal ;
	unsigned char	UcCntPla ;
	UcFlgVal	= 1 ;
	UcCntPla	= 0 ;
	
	do{
		RegReadA( GRACC, &UcFlgVal ) ;
		UcFlgVal	&= UcTrgDat ;
		UcCntPla++ ;
	} while( UcFlgVal && ( UcCntPla < ACCWIT_POLLING_LIMIT_A ) ) ;
	if( UcCntPla == ACCWIT_POLLING_LIMIT_A ) { return OIS_FW_POLLING_FAIL; }
	
	return OIS_FW_POLLING_PASS ;
}

void	AutoGainControlSw( unsigned char UcModeSw )
{
	
	if( UcModeSw == OFF )
	{
		RegWriteA( GADJGANGXMOD, 0x00 ) ;				// 0x0108	Gain Down
		RegWriteA( GADJGANGYMOD, 0x00 ) ;				// 0x0108	Gain Down
		RegWriteA( GADJGANGO,	 0x11 ) ;				// 0x0108	Gain Down
	}
	else
	{
		RegWriteA( GADJGANGO,		0x00 ) ;				// 0x0108	Gain Up
		RegWriteA( GADJGANGXMOD, 0xA7 ) ;				// 0x0108	Gain Down
		RegWriteA( GADJGANGYMOD, 0xA7 ) ;				// 0x0108	Gain Down
	}
	
	return;
}

int	ClrGyr( unsigned char UcClrFil , unsigned char UcClrMod )
{
	unsigned char	UcRamClr;
	unsigned char	UcCntPla;
	
	UcCntPla = 0 ;
	
	/*Select Filter to clear*/
	RegWriteA( GRAMDLYMOD	, UcClrFil ) ;	// 0x011B	[ - | - | - | P ][ T | L | H | I ]
	
	/*Enable Clear*/
	RegWriteA( GRAMINITON	, UcClrMod ) ;	// 0x0103	[ - | - | - | - ][ - | - | ’x‰„Clr | ŒW”Clr ]
	
	/*Check RAM Clear complete*/
	do{
		RegReadA( GRAMINITON, &UcRamClr );
		UcRamClr &= 0x03;
		UcCntPla++;
	} while( ( UcRamClr != 0x00 ) && ( UcCntPla < CLRGYR_POLLING_LIMIT_A ) );
	if( UcCntPla == CLRGYR_POLLING_LIMIT_A ) { return OIS_FW_POLLING_FAIL; }
	
	return OIS_FW_POLLING_PASS ;
}

void	DrvSw( unsigned char UcDrvSw )
{
	if( UcDrvSw == ON )	{
		RegWriteA( DRVFC	, 0xE3 );					// 0x0070	MODE=2,Drvier Block Ena=1,FullMode=1
	} else {
		RegWriteA( DRVFC	, 0x00 );					// 0x0070	Drvier Block Ena=0
	}

	return;
}
//==============================================================================
// OisIni.c Code END (2012.11.20)
//==============================================================================

unsigned char	RtnCen( unsigned char	UcCmdPar )
{
	unsigned char	UcCmdSts ;
	
	
	UcCmdSts	= EXE_END ;
	
	if( !UcCmdPar ) {										// X,Y Centering
		StbOnn() ;
	} else if( UcCmdPar == 0x01 ) {							// X Centering Only
		
		SrvCon( X_DIR, ON ) ;								// X only Servo ON
		SrvCon( Y_DIR, OFF ) ;
	} else if( UcCmdPar == 0x02 ) {							// Y Centering Only
		
		SrvCon( X_DIR, OFF ) ;								// Y only Servo ON
		SrvCon( Y_DIR, ON ) ;
	}
	
	return( UcCmdSts ) ;
}

void	OisEna( void ) //Ois On
{
	// Servo ON
	GyrCon( ON ) ;

	return;
}

void	OisOff( void ) //Ois Off
{
	GyrCon( OFF ) ;
	RamWriteA( lxggf, 0x0000 ) ;			// 0x1308
	RamWriteA( lyggf, 0x0000 ) ;			// 0x1348

	return;
}

void	S2cPro( unsigned char uc_mode )
{
	RamWrite32A( gxh1c, 0x3F7FFD00 ) ;										// 0x1814
	RamWrite32A( gyh1c, 0x3F7FFD00 ) ;										// 0x1914
	
	if( uc_mode == 1 ) {														//Capture Mode
		// HPF Through Setting	
//		RegWriteA( GSHTON, 0x01 ) ;												// 0x0104
		RegWriteA( G2NDCEFON0, 0x03 ) ;											// 0x0106	I Filter Control
	} else {																	//Video Mode
		// HPF Setting
//		RegWriteA( GSHTON, 0x00 ) ;												// 0x0104
		RegWriteA( G2NDCEFON0, 0x00 ) ;											// 0x0106
	}
	
	return;
}

//********************************************************************************
// Function Name 	: StbOnn
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2010.09.15 Y.Shigeoka
//********************************************************************************
void StbOnn( void )
{
	unsigned char	UcRegValx,UcRegValy;					// Registor value 

	RegReadA( LXEQEN, &UcRegValx ) ;				/* 0x0084 */
	RegReadA( LYEQEN, &UcRegValy ) ;				/* 0x008E */
	if( (( UcRegValx & 0x80 ) != 0x80 ) && (( UcRegValy & 0x80 ) != 0x80 ))
	{
		RegWriteA( SSSEN,  0x88 ) ;				// 0x0097	Smooth Start enable
		
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
	}
	else
	{
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
	}

	return;
}

void	SrvCon( unsigned char	UcDirSel, unsigned char	UcSwcCon )
{
	if( UcSwcCon ) {
		if( !UcDirSel ) {									// X Direction
			RegWriteA( LXEQEN, 0xC5 ) ;				// 0x0084	LXSW = ON
		} else {											// Y Direction
			RegWriteA( LYEQEN, 0xC5 ) ;				// 0x008E	LYSW = ON
		}
	} else {
		if( !UcDirSel ) {									// X Direction
			RegWriteA( LXEQEN, 0x45 ) ;				// 0x0084	LXSW = OFF
			RamWriteA( LXDODAT, 0x0000 ) ;			// 0x115A
		} else {											// Y Direction
			RegWriteA( LYEQEN, 0x45 ) ;				// 0x008E	LYSW = OFF
			RamWriteA( LYDODAT, 0x0000 ) ;			// 0x119A
		}
	}

	return;
}

void	GyrCon( unsigned char	UcGyrCon )
{
	// Return HPF Setting
	RegWriteA( GSHTON, 0x00 ) ;												// 0x0104
	
	if( UcGyrCon ) {														// Gyro ON
		RamWriteA( lxggf, 0x7fff ) ;										// 0x1308
		RamWriteA( lyggf, 0x7fff ) ;										// 0x1348
		RamWrite32A( gxi1b_1, 0xBF800000 ) ;								// Down
		RamWrite32A( gyi1b_1, 0xBF800000 ) ;								// Down

//		RegWriteA( GPANFILMOD, 0x00 ) ;										// 0x0167
//		ClrGyr( 0x02, 0x02 ) ;
//		ClrGyr( 0x04, 0x02 ) ;
//		RegWriteA( GPANFILMOD, 0x01 ) ;										// 0x0167
		RamWrite32A( gxi1b_1, 0xB3E6A3C6 ) ;								// Down
		RamWrite32A( gyi1b_1, 0xB3E6A3C6 ) ;								// Down
		RegWriteA( GPANSTTFRCE, 0x00 ) ;									// 0x010A
		AutoGainControlSw( ON ) ;											/* Auto Gain Control Mode ON */
	} else {																// Gyro OFF
		/* Gain3 Register */
		AutoGainControlSw( OFF ) ;											/* Auto Gain Control Mode OFF */
	}
	
	//S2cPro( ON ) ; //Go to Cature Mode
	S2cPro( OFF ) ; //Go to Video Mode

	return;
}

void	SetPanTiltMode( unsigned char UcPnTmod )
{
	switch ( UcPnTmod ) {
		case OFF :
			RegWriteA( GPANON, 0x00 ) ;			// 0x0109	X,Y Pan/Tilt Function OFF
			break ;
		case ON :
			RegWriteA( GPANON, 0x11 ) ;			// 0x0109	X,Y Pan/Tilt Function ON
			break ;
	}

	return;
}

void	VSModeConvert( unsigned char UcVstmod )
{
	if( UcVstmod ) {								// Cam Mode Convert
		RamWrite32A( gxl2a_2, 0x00000000 ) ;	// 0x1860	0
		RamWrite32A( gxl2b_2, 0x3E06FD65 ) ;	// 0x1861	0.131826
		RamWrite32A( gyl2a_2, 0x00000000 ) ;	// 0x1960	0
		RamWrite32A( gyl2b_2, 0x3E06FD65 ) ;	// 0x1961	0.131826

		RamWrite32A( gxl2c_2, 0xBB894028 ) ;	// 0x1862	-0.004188556	3F7FEC00
		RamWrite32A( gyl2c_2, 0xBB894028 ) ;	// 0x1962	-0.004188556	3F7FEC00

		RegWriteA( GPANSTT31JUG0, 	0x00 );		// 0x0142
		RegWriteA( GPANSTT31JUG1, 	0x00 );		// 0x0143
		RegWriteA( GPANSTT41JUG0, 	0x07 );		// 0x0144
		RegWriteA( GPANSTT41JUG1, 	0x00 );		// 0x0145
		RegWriteA( GPANSTT13JUG0, 	0x00 );		// 0x0148
		RegWriteA( GPANSTT13JUG1, 	0x07 );		// 0x0149
		RegWriteA( GPANSTT43JUG0, 	0x00 );		// 0x014C
		RegWriteA( GPANSTT43JUG1, 	0x07 );		// 0x014D
		RegWriteA( GPANSTT34JUG0, 	0x01 );		// 0x014E
		RegWriteA( GPANSTT34JUG1, 	0x00 );		// 0x014F

		RegWriteA( GPANSTTXXXTH,	0xF0 );		// 0x015D
		RamWrite32A( gxi1a_1, 0x38A8A554 ) ;	// 0.3Hz
		RamWrite32A( gxi1b_1, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gxi1c_1, 0x33E6A3C6 ) ;	// Up
		RamWrite32A( gxi1a_b, 0x38A8A554 ) ;	// 0.3Hz
		RamWrite32A( gxi1b_b, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gxi1c_b, 0x33E6A3C6 ) ;	// Up
		RamWrite32A( gxi1a_c, 0x3AAF73A1 ) ;	// 5Hz
		RamWrite32A( gxi1b_c, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gxi1c_c, 0x3F800000 ) ;	// Up
		RamWrite32A( gyi1a_1, 0x38A8A554 ) ;	// 0.3Hz
		RamWrite32A( gyi1b_1, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gyi1c_1, 0x33E6A3C6 ) ;	// Up
		RamWrite32A( gyi1a_b, 0x38A8A554 ) ;	// 0.3Hz
		RamWrite32A( gyi1b_b, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gyi1c_b, 0x33E6A3C6 ) ;	// Up
		RamWrite32A( gyi1a_c, 0x3AAF73A1 ) ;	// 5Hz
		RamWrite32A( gyi1b_c, 0xB3E6A3C6 ) ;	// Down
		RamWrite32A( gyi1c_c, 0x3F800000 ) ;	// Up
	} else {										// Still Mode Convert
		RamWrite32A( gxl2a_2, 0x00000000 ) ;	// 0x1860	0
		RamWrite32A( gxl2b_2, 0x3D829952 ) ;	// 0x1861	0.063769
		RamWrite32A( gyl2a_2, 0x00000000 ) ;	// 0x1960	0
		RamWrite32A( gyl2b_2, 0x3D829952 ) ;	// 0x1961	0.063769

		RamWrite32A( gxl2c_2, 0xBA9CD414 ) ;	// 0x1862	-0.001196506	3F7FF800
		RamWrite32A( gyl2c_2, 0xBA9CD414 ) ;	// 0x1962	-0.001196506	3F7FF800

		RegWriteA( GPANSTT31JUG0, 	0x01 );		// 0x0142
		RegWriteA( GPANSTT31JUG1, 	0x00 );		// 0x0143
		RegWriteA( GPANSTT41JUG0, 	0x00 );		// 0x0144
		RegWriteA( GPANSTT41JUG1, 	0x00 );		// 0x0145
		RegWriteA( GPANSTT13JUG0, 	0x00 );		// 0x0148
		RegWriteA( GPANSTT13JUG1, 	0x07 );		// 0x0149
		RegWriteA( GPANSTT43JUG0, 	0x00 );		// 0x014C
		RegWriteA( GPANSTT43JUG1, 	0x00 );		// 0x014D
		RegWriteA( GPANSTT34JUG0, 	0x00 );		// 0x014E
		RegWriteA( GPANSTT34JUG1, 	0x00 );		// 0x014F
		RegWriteA( GPANSTTXXXTH,	0x0F );		// 0x015D
		RamWrite32A( gxi1a_1, 0x38A8A554 ) ;		// 0.3Hz
		RamWrite32A( gxi1b_1, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gxi1c_1, 0x33E6A3C6 ) ;		// Up
		RamWrite32A( gxi1a_b, 0x3AAF73A1 ) ;		// 5Hz
		RamWrite32A( gxi1b_b, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gxi1c_b, 0x3F800000 ) ;		// Up
		RamWrite32A( gxi1a_c, 0x38A8A554 ) ;		// 0.3Hz
		RamWrite32A( gxi1b_c, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gxi1c_c, 0x33E6A3C6 ) ;		// Up
		RamWrite32A( gyi1a_1, 0x38A8A554 ) ;		// 0.3Hz
		RamWrite32A( gyi1b_1, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gyi1c_1, 0x33E6A3C6 ) ;		// Up
		RamWrite32A( gyi1a_b, 0x3AAF73A1 ) ;		// 5Hz
		RamWrite32A( gyi1b_b, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gyi1c_b, 0x3F800000 ) ;		// Up
		RamWrite32A( gyi1a_c, 0x38A8A554 ) ;		// 0.3Hz
		RamWrite32A( gyi1b_c, 0xB3E6A3C6 ) ;		// Down
		RamWrite32A( gyi1c_c, 0x33E6A3C6 ) ;		// Up
	}
	return;
}

void WitTim( unsigned short UsWitTim )
{
   usleep(UsWitTim*1000);
}

//==============================================================================
// OisCmd.c Code END (2012. 11. 20)
//==============================================================================

#define		HALL_ADJ		0
#define		LOOPGAIN		1
#define		THROUGH			2
#define		NOISE			3

//********************************************************************************
// Function Name 	: TneGvc
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Tunes the Gyro VC offset
// History			: First edition 						2009.07.31  Y.Tashita
//********************************************************************************
#define	INITVAL		0x0000

int BsyWit(unsigned short UsTrgAdr, unsigned char UcTrgDat)
{
	unsigned char UcFlgVal;
	unsigned char UcCntPla;

	RegWriteA(UsTrgAdr, UcTrgDat);

	UcFlgVal = 1;
	UcCntPla = 0;

	do {
		RegReadA(FLGM, &UcFlgVal);
		UcFlgVal &= 0x40;
		UcCntPla ++ ;
	} while ( UcFlgVal && ( UcCntPla < BSYWIT_POLLING_LIMIT_A));
	if (UcCntPla == BSYWIT_POLLING_LIMIT_A) { return OIS_FW_POLLING_FAIL; }
	
	return OIS_FW_POLLING_PASS;
}



short	GenMes( unsigned short	UsRamAdd, unsigned char	UcMesMod )
{
	short	SsMesRlt = 0;

	RegWriteA( MS1INADD, ( unsigned char )( UsRamAdd & 0x00ff ) ) ;	// 0x00C2	Input Signal Select

	if( !UcMesMod ) {
		RegWriteA( MSMPLNSH, 0x03 ) ;								// 0x00CB
		RegWriteA( MSMPLNSL, 0xFF ) ;								// 0x00CA	1024 Times Measure
		BsyWit( MSMA, 0x01 ) ;											// 0x00C9		Average Measure
	} else {
		RegWriteA( MSMPLNSH, 0x00 ) ;								// 0x00CB
		RegWriteA( MSMPLNSL, 0x03 ) ;								// 0x00CA	4 Cycle Measure
		BsyWit( MSMA, 0x22 ) ;											// 0x00C9		Average Measure
	}

	RegWriteA( MSMA, 0x00 ) ;										// 0x00C9		Measure Stop

	RamReadA( MSAV1, ( unsigned short * )&SsMesRlt ) ;				// 0x1205

	return( SsMesRlt ) ;
}

void	MesFil( unsigned char	UcMesMod )
{
	if( !UcMesMod ) {								// Hall Bias&Offset Adjust
		// Measure Filter1 Setting
		RamWriteA( ms1aa, 0x0285 ) ;		// 0x13AF	LPF150Hz
		RamWriteA( ms1ab, 0x0285 ) ;		// 0x13B0
		RamWriteA( ms1ac, 0x7AF5 ) ;		// 0x13B1
		RamWriteA( ms1ad, 0x0000 ) ;		// 0x13B2
		RamWriteA( ms1ae, 0x0000 ) ;		// 0x13B3
		RamWriteA( ms1ba, 0x7FFF ) ;		// 0x13B4	Through
		RamWriteA( ms1bb, 0x0000 ) ;		// 0x13B5
		RamWriteA( ms1bc, 0x0000 ) ;		// 0x13B6
		RamWriteA( ms1bd, 0x0000 ) ;		// 0x13B7
		RamWriteA( ms1be, 0x0000 ) ;		// 0x13B8

		RegWriteA( MSF1SOF, 0x00 ) ;		// 0x00C1
		
		// Measure Filter2 Setting
		RamWriteA( ms2aa, 0x0285 ) ;		// 0x13B9	LPF150Hz
		RamWriteA( ms2ab, 0x0285 ) ;		// 0x13BA
		RamWriteA( ms2ac, 0x7AF5 ) ;		// 0x13BB
		RamWriteA( ms2ad, 0x0000 ) ;		// 0x13BC
		RamWriteA( ms2ae, 0x0000 ) ;		// 0x13BD
		RamWriteA( ms2ba, 0x7FFF ) ;		// 0x13BE	Through
		RamWriteA( ms2bb, 0x0000 ) ;		// 0x13BF
		RamWriteA( ms2bc, 0x0000 ) ;		// 0x13C0
		RamWriteA( ms2bd, 0x0000 ) ;		// 0x13C1
		RamWriteA( ms2be, 0x0000 ) ;		// 0x13C2

		RegWriteA( MSF2SOF, 0x00 ) ;		// 0x00C5
		
	} else if( UcMesMod == LOOPGAIN ) {				// Loop Gain Adjust
		// Measure Filter1 Setting
		RamWriteA( ms1aa, 0x0F21 ) ;		// 0x13AF	LPF1000Hz
		RamWriteA( ms1ab, 0x0F21 ) ;		// 0x13B0
		RamWriteA( ms1ac, 0x61BD ) ;		// 0x13B1
		RamWriteA( ms1ad, 0x0000 ) ;		// 0x13B2
		RamWriteA( ms1ae, 0x0000 ) ;		// 0x13B3
		RamWriteA( ms1ba, 0x7F7D ) ;		// 0x13B4	HPF30Hz
		RamWriteA( ms1bb, 0x8083 ) ;		// 0x13B5
		RamWriteA( ms1bc, 0x7EF9 ) ;		// 0x13B6
		RamWriteA( ms1bd, 0x0000 ) ;		// 0x13B7
		RamWriteA( ms1be, 0x0000 ) ;		// 0x13B8

		RegWriteA( MSF1SOF, 0x00 ) ;		// 0x00C1
		
		// Measure Filter2 Setting
		RamWriteA( ms2aa, 0x0F21 ) ;		// 0x13B9	LPF1000Hz
		RamWriteA( ms2ab, 0x0F21 ) ;		// 0x13BA
		RamWriteA( ms2ac, 0x61BD ) ;		// 0x13BB
		RamWriteA( ms2ad, 0x0000 ) ;		// 0x13BC
		RamWriteA( ms2ae, 0x0000 ) ;		// 0x13BD
		RamWriteA( ms2ba, 0x7F7D ) ;		// 0x13BE	HPF30Hz
		RamWriteA( ms2bb, 0x8083 ) ;		// 0x13BF
		RamWriteA( ms2bc, 0x7EF9 ) ;		// 0x13C0
		RamWriteA( ms2bd, 0x0000 ) ;		// 0x13C1
		RamWriteA( ms2be, 0x0000 ) ;		// 0x13C2

		RegWriteA( MSF2SOF, 0x00 ) ;		// 0x00C5
		
	} else if( UcMesMod == THROUGH ) {				// for Through
		// Measure Filter1 Setting
		RamWriteA( ms1aa, 0x7FFF ) ;		// 0x13AF	Through
		RamWriteA( ms1ab, 0x0000 ) ;		// 0x13B0
		RamWriteA( ms1ac, 0x0000 ) ;		// 0x13B1
		RamWriteA( ms1ad, 0x0000 ) ;		// 0x13B2
		RamWriteA( ms1ae, 0x0000 ) ;		// 0x13B3
		RamWriteA( ms1ba, 0x7FFF ) ;		// 0x13B4	Through
		RamWriteA( ms1bb, 0x0000 ) ;		// 0x13B5
		RamWriteA( ms1bc, 0x0000 ) ;		// 0x13B6
		RamWriteA( ms1bd, 0x0000 ) ;		// 0x13B7
		RamWriteA( ms1be, 0x0000 ) ;		// 0x13B8

		RegWriteA( MSF1SOF, 0x00 ) ;		// 0x00C1
		
		// Measure Filter2 Setting
		RamWriteA( ms2aa, 0x7FFF ) ;		// 0x13B9	Through
		RamWriteA( ms2ab, 0x0000 ) ;		// 0x13BA
		RamWriteA( ms2ac, 0x0000 ) ;		// 0x13BB
		RamWriteA( ms2ad, 0x0000 ) ;		// 0x13BC
		RamWriteA( ms2ae, 0x0000 ) ;		// 0x13BD
		RamWriteA( ms2ba, 0x7FFF ) ;		// 0x13BE	Through
		RamWriteA( ms2bb, 0x0000 ) ;		// 0x13BF
		RamWriteA( ms2bc, 0x0000 ) ;		// 0x13C0
		RamWriteA( ms2bd, 0x0000 ) ;		// 0x13C1
		RamWriteA( ms2be, 0x0000 ) ;		// 0x13C2

		RegWriteA( MSF2SOF, 0x00 ) ;		// 0x00C5
		
	} else if( UcMesMod == NOISE ) {				// SINE WAVE TEST for NOISE
	
		RamWriteA( ms1aa, 0x04F3 ) ;		// 0x13AF	LPF150Hz
		RamWriteA( ms1ab, 0x04F3 ) ;		// 0x13B0
		RamWriteA( ms1ac, 0x761B ) ;		// 0x13B1
		RamWriteA( ms1ad, 0x0000 ) ;		// 0x13B2
		RamWriteA( ms1ae, 0x0000 ) ;		// 0x13B3
		RamWriteA( ms1ba, 0x04F3 ) ;		// 0x13B4	LPF150Hz
		RamWriteA( ms1bb, 0x04F3 ) ;		// 0x13B5
		RamWriteA( ms1bc, 0x761B ) ;		// 0x13B6
		RamWriteA( ms1bd, 0x0000 ) ;		// 0x13B7
		RamWriteA( ms1be, 0x0000 ) ;		// 0x13B8
		
		RegWriteA( MSF1SOF, 0x00 ) ;		// 0x00C1

		RamWriteA( ms2aa, 0x04F3 ) ;		// 0x13B9	LPF150Hz
		RamWriteA( ms2ab, 0x04F3 ) ;		// 0x13BA
		RamWriteA( ms2ac, 0x761B ) ;		// 0x13BB
		RamWriteA( ms2ad, 0x0000 ) ;		// 0x13BC
		RamWriteA( ms2ae, 0x0000 ) ;		// 0x13BD
		RamWriteA( ms2ba, 0x04F3 ) ;		// 0x13BE	LPF150Hz
		RamWriteA( ms2bb, 0x04F3 ) ;		// 0x13BF
		RamWriteA( ms2bc, 0x761B ) ;		// 0x13C0
		RamWriteA( ms2bd, 0x0000 ) ;		// 0x13C1
		RamWriteA( ms2be, 0x0000 ) ;		// 0x13C2
		
		RegWriteA( MSF2SOF, 0x00 ) ;		// 0x00C5
	}
}

int16_t lgit_convert_float32(uint32_t float32, uint8_t len);


unsigned char	TneGvc( uint8_t flag )
{
	unsigned char	UcGvcSts ;
	unsigned short UsGxoVal, UsGyoVal;
	int16_t oldx, oldy;

	UcGvcSts	= EXE_END ;

	{
		uint32_t gyrox, gyroy = 0;
		RamRead32A(GXADZ, &gyrox);
		RamRead32A(GYADZ, &gyroy);
		oldx = abs(lgit_convert_float32(gyrox,18));
		oldy = abs(lgit_convert_float32(gyroy,18));
		CDBG("%s old gxadz %x, %x \n",__func__,oldx, oldy);
		
		if ( oldx < 262*2 && oldy < 262*2)
		{
			CDBG("%s no need to adjust \n",__func__);		
			return UcGvcSts;
		}
	}

	
	// for InvenSense Digital Gyro	ex.IDG-2000
	// A/D Offset Clear
	RamWriteA( ADGXOFF, 0x0000 ) ;	// 0x1108
	RamWriteA( ADGYOFF, 0x0000 ) ;	// 0x110B
	RegWriteA( IZAH,	(unsigned char)(INITVAL >> 8) ) ;	// 0x03A0		Set Offset High byte
	RegWriteA( IZAL,	(unsigned char)INITVAL ) ;			// 0x03A1		Set Offset Low byte
	RegWriteA( IZBH,	(unsigned char)(INITVAL >> 8) ) ;	// 0x03A2		Set Offset High byte
	RegWriteA( IZBL,	(unsigned char)INITVAL ) ;			// 0x03A3		Set Offset Low byte
	
	RegWriteA( GDLYMON10, 0xF5 ) ;		// 0x0184 <- GXADZ(0x19F5)
	RegWriteA( GDLYMON11, 0x01 ) ;		// 0x0185 <- GXADZ(0x19F5)
	RegWriteA( GDLYMON20, 0xF5 ) ;		// 0x0186 <- GYADZ(0x18F5)
	RegWriteA( GDLYMON21, 0x00 ) ;		// 0x0187 <- GYADZ(0x18F5)
	MesFil( THROUGH ) ;
	RegWriteA( MSF1EN, 0x01 ) ;			// 0x00C0		Measure Filter1 Equalizer ON
	//////////
	// X
	//////////
	if (BsyWit( DLYCLR2, 0x80 ) != OIS_FW_POLLING_PASS) {return OIS_FW_POLLING_FAIL ;}			// 0x00EF	Measure Filter1 Delay RAM Clear
	UsGxoVal = (unsigned short)GenMes( GYRMON1, 0 );		// GYRMON1(0x1110) <- GXADZ(0x19F5)
	RegWriteA( IZAH, (unsigned char)(UsGxoVal >> 8) ) ;	// 0x03A0		Set Offset High byte
	RegWriteA( IZAL, (unsigned char)(UsGxoVal) ) ;		// 0x03A1		Set Offset Low byte
	//////////
	// Y
	//////////
	if (BsyWit( DLYCLR2, 0x80 ) != OIS_FW_POLLING_PASS) {return OIS_FW_POLLING_FAIL;}			// 0x00EF	Measure Filter1 Delay RAM Clear
	UsGyoVal = (unsigned short)GenMes( GYRMON2, 0 );		// GYRMON2(0x1111) <- GYADZ(0x18F5)

	RegWriteA( IZBH, (unsigned char)(UsGyoVal >> 8) ) ;	// 0x03A2		Set Offset High byte
	RegWriteA( IZBL, (unsigned char)(UsGyoVal) ) ;		// 0x03A3		Set Offset Low byte

	
	RegWriteA( MSF1EN, 0x00 ) ;			// 0x00C0		Measure Filter1 Equalizer OFF

	if( ( ( short )UsGxoVal > ( short )GYROFF_HIGH ) ||
		( ( short )UsGxoVal < ( short )GYROFF_LOW ) ) {
		UcGvcSts	= EXE_GYRADJ ;
	}

	if( ( ( short )UsGyoVal > ( short )GYROFF_HIGH ) ||
		( ( short )UsGyoVal < ( short )GYROFF_LOW ) ) {
		UcGvcSts	= EXE_GYRADJ ;
	}

	{
	uint32_t gyrox, gyroy = 0;
	int16_t newx, newy;
	
	RamRead32A(GXADZ, &gyrox);
	RamRead32A(GYADZ, &gyroy);
	newx = abs(lgit_convert_float32(gyrox,18));
	newy = abs(lgit_convert_float32(gyroy,18));
	CDBG("%s new gxadz %x, %x \n",__func__,newx, newy);

	if ( newx > 262*2 || newy > 262*2 || newx > oldx || newy > oldy )
	{ UcGvcSts = EXE_GYRADJ; }
	}

	if (UcGvcSts != EXE_GYRADJ)
	{
		CDBG("%s : gyro original : %x, %x \n",__func__, StCalDat.StGvcOff.UsGxoVal, StCalDat.StGvcOff.UsGyoVal);
		CDBG("%s : gyro result : %x, %x \n",__func__, UsGxoVal, UsGyoVal);
		if (flag == OIS_VER_CALIBRATION)
		{
			ois_i2c_e2p_write(GYRO_AD_OFFSET_X, 0xFFFF & UsGxoVal, 2);
			usleep(10000);
			ois_i2c_e2p_write(GYRO_AD_OFFSET_Y, 0xFFFF & UsGyoVal, 2);
		}
	}
	
	return( UcGvcSts ) ;
}
//---------------------------------------------------------------------------------//
// 2013-08-10 added for gain-loop test 
//---------------------------------------------------------------------------------//
#define     __MEASURE_LOOPGAIN      0x00

//==============================================================================
//  Function    :   SetSineWave()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//  outputs     :   void
//  explanation :   Initializes sine wave settings:
//                      Sine Table, Amplitue, Offset, Frequency
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void SetSineWave( unsigned char UcJikuSel , unsigned char UcMeasMode )
{
	unsigned char   UcSWFC1[]   = { 0x7D/*139Hz*/ , 0x0F/*15Hz*/ } ,            // { Loop Gain setting , Bias/Offset setting}
                    UcSWFC2[]   = { 0x00/*150Hz*/ , 0x00/*20Hz*/ } ;            // { Loop Gain setting , Bias/Offset setting}

    unsigned char   UcSwSel[2][2] = { { 0x80 , 0x40 } ,                         // Loop gain setting
                                      { 0x00 , 0x00 }                           // Bias/Offset Setting
                                    };

    UcMeasMode &= 0x01;
    UcJikuSel  &= 0x01;

    /* Manually Set Offset */
    RamWriteA( WAVXO , 0x0000 );                                                // 0x11D5
    RamWriteA( WAVYO , 0x0000 );                                                // 0x11D6

    /* Manually Set Amplitude */
    //RamWriteA( wavxg , 0x7FFF );                                                // 0x13C3
    //RamWriteA( wavyg , 0x7FFF );                                                // 0x13C4
	RamWriteA( wavxg , 0x3000 );                                                // 0x13C3
    RamWriteA( wavyg , 0x3000 );                                                // 0x13C4

    /* Set Frequency */
    //****************************************************
    //  Žü”g” = (fs/96)*(SWB+1)/(SWA+1)*1/(2^SWC)  [ Hz ]
    //****************************************************
    RegWriteA( SWFC1 , UcSWFC1[UcMeasMode] );                                   // 0x00DD    [ SWB(7:4) ][ SWA(3:0) ]
    RegWriteA( SWFC2 , UcSWFC2[UcMeasMode] );                                   // 0x00DE    [ SWCIR | SWDIR | - | - ][ SWCOM | SWFC(2:0) ]
    RegWriteA( SWFC3 , 0x00 );                                                  // 0x00DF    [ SWINIP | SWBGNP(6:0) ]
    RegWriteA( SWFC4 , 0x00 );                                                  // 0x00E0    [ SWFINP | SWENDP(6:0) ]
    RegWriteA( SWFC5 , 0x00 );                                                  // 0x00E1    [ SWFT(7:0) ]

    /* Set Sine Wave Input RAM */
    RegWriteA( SWSEL , UcSwSel[UcMeasMode][UcJikuSel] );                        // 0x00E2    [ SINX | SINY | SING | SIN0END ][ SINGDPX1 | SINGDPX2 | SINGDPY1 | SINGDPY2 ]

    /* Clear Optional Sine wave input address */
    if( !UcMeasMode )       // Loop Gain mode
    {
        RegWriteA( SINXADD , 0x00 );                                            // 0x00E3  [ SINXADD(7:0) ]
        RegWriteA( SINYADD , 0x00 );                                            // 0x00E4  [ SINYADD(7:0) ]
    }
    else if( !UcJikuSel )   // Bias/Offset mode X-Axis
    {
        RegWriteA( SINXADD , (unsigned char)LXDODAT );                          // 0x00E3  [ SINXADD(7:0) ]
        RegWriteA( SINYADD , 0x00 );                                            // 0x00E4  [ SINYADD(7:0) ]
    }
    else                    // Bias/Offset mode Y-Axis
    {
        RegWriteA( SINXADD , 0x00 );                                            // 0x00E3  [ SINXADD(7:0) ]
        RegWriteA( SINYADD , (unsigned char)LYDODAT );                          // 0x00E4  [ SINYADD(7:0) ]
    }
}

//==============================================================================
//  Function    :   StartSineWave()
//  inputs      :   none
//  outputs     :   void
//  explanation :   Starts sine wave
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StartSineWave( void )
{
    /* Start Sine Wave */
    RegWriteA( SWEN , 0x80 );                                                   // 0x00DB     [ SINON | SINRST | - | - ][ SININISET | - | - | - ]
}

//==============================================================================
//  Function    :   StopSineWave()
//  inputs      :   void
//  outputs     :   void
//  explanation :   Stops sine wave
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StopSineWave( void )
{
    /* Set Sine Wave Input RAM */
    RegWriteA( SWSEL   , 0x00 );                                                // 0x00E2    [ SINX | SINY | SING | SIN0END ][ SINGDPX1 | SINGDPX2 | SINGDPY1 | SINGDPY2 ]
    RegWriteA( SINXADD , 0x00 );                                                // 0x00E3  [ SINXADD(7:0) ]
    RegWriteA( SINYADD , 0x00 );                                                // 0x00E4  [ SINYADD(7:0) ]

    /* Stop Sine Wave */
    RegWriteA( SWEN  , 0x00 );                                                  // 0x00DB     [ SINON | SINRST | - | - ][ SININISET | - | - | - ]
}


//==============================================================================
//  Function    :   SetMeaseFil_LoopGain()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//                  UcFilSel
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void SetMeasFil( unsigned char UcJikuSel , unsigned char UcMeasMode , unsigned char UcFilSel )
{
    unsigned short  UsIn1Add[2][2] = { { LXC1   , LYC1   } ,                    // Loop Gain Setting
                                       { ADHXI0 , ADHYI0 }                      // Bias/Offset Setting
                                     } ,
                    UsIn2Add[2][2] = { { LXC2   , LYC2   } ,                    // Loop Gain Setting
                                       { 0x0000 , 0x0000 }                      // Bias/Offset Setting
                                     } ;

    /* Set Limits on Input Parameters */
    UcJikuSel  &= 0x01;
    UcMeasMode &= 0x01;
    if( UcFilSel > NOISE ) UcFilSel = THROUGH;
	
	MesFil( UcFilSel ) ;					/* Set Measure filter */

    RegWriteA( MS1INADD , (unsigned char)UsIn1Add[UcMeasMode][UcJikuSel] );     // 0x00C2
    RegWriteA( MS1OUTADD, 0x00 );                                               // 0x00C3


    RegWriteA( MS2INADD , (unsigned char)UsIn2Add[UcMeasMode][UcJikuSel] );     // 0x00C6
    RegWriteA( MS2OUTADD, 0x00 );                                               // 0x00C7

    /* Set Measure Filter Down Sampling */
    //****************************************************
    //  Measure Filter Down Sampling = Fs/(MSFDS+1)
    //****************************************************
    RegWriteA( MSFDS , 0x00 );                                                  // 0x00C8
}

//==============================================================================
//  Function    :   StartMeasFil()
//  inputs      :   void
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StartMeasFil( void )
{
    /* Enable Measure Filters */
    RegWriteA( MSF1EN , 0x01 );                                                 // 0x00C0       [ MSF1SW | - | - | - ][ - | - | - | MSF1EN ]
    RegWriteA( MSF2EN , 0x01 );                                                 // 0x00C4       [ MSF2SW | - | - | - ][ - | - | - | MSF2EN ]
}

//==============================================================================
//  Function    :   StopMeasFil()
//  inputs      :   void
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StopMeasFil( void )
{
    /* Enable Measure Filters */
    RegWriteA( MSF1EN , 0x00 );                                                 // 0x00C0       [ MSF1SW | - | - | - ][ - | - | - | MSF1EN ]
    RegWriteA( MSF2EN , 0x00 );                                                 // 0x00C4       [ MSF2SW | - | - | - ][ - | - | - | MSF2EN ]
}

//********************************************************************************
// Function Name 	: IntegralMes
// Retun Value		: void
// Argment Value	: Sine wave Frequency Index
// Explanation		: Integration Value Measure Setting
// History			: First edition 						2013.07.31 Y.Tashita
//********************************************************************************
void	IntegralMes( unsigned char UcSinFrq )
{
	RegWriteA( MSMA, 0x03 ) ;							// 0x00C9	Cycle Wait, Sin Wave Measure

	if ( UcSinFrq == 0 ){
		WitTim( 200 ) ;
	}else if ( UcSinFrq == 1 ){
		WitTim( 100 ) ;
	}else{
		WitTim( 60 ) ;
	}

	RegWriteA( MSMA, 0x00 ) ;							// 0x00C9	Measure End

}

//********************************************************************************
// Function Name 	: GetIntegral
// Retun Value		: Measure Result
// Argment Value	: LXG1/LXG2 Select
// Explanation		: Integration Value Measure Result Read
// History			: First edition 						2013.07.31 Y.Tashita
//********************************************************************************
unsigned short	GetIntegral( unsigned char	UcDirSel )
{
	unsigned short	UsAmpVal ;

	if( !UcDirSel ) {
		RamReadA( MSCAP, &UsAmpVal ) ;				// 0x120A
	} else {
		RamReadA( MSCAP2, &UsAmpVal ) ;				// 0x12AC
	}

	return( UsAmpVal ) ;
}

//********************************************************************************
// Function Name 	: CalAmpRatio
// Retun Value		: Amplitude Ratio * 100
// Argment Value	: Ratio Calculation Source Value
// Explanation		: Sine Wave Amplitude Ratio Calculation Function
// History			: First edition 						2013.08.01 Y.Tashita
//********************************************************************************
short	CalAmpRatio( unsigned short	UsSource1, unsigned short	UsSource2 )
{
	short	SsAmpRatio ;

	SsAmpRatio	= ( short )( ( ( unsigned long )UsSource1 * 100 ) / UsSource2 ) ;

	return( SsAmpRatio ) ;
}

//==============================================================================
//  Function    :   SrvGainMes()
//  inputs      :   UcJikuSel   0: X-Axis, 1: Y-Axis
//  outputs     :   OK/NG Judge Value
//  explanation :
//  revisions   :   First Edition                          2013.07.31 Y.Tashita
//==============================================================================
unsigned char	 SrvGainMes( unsigned char	UcJikuSel )
{
	unsigned short  UsSineAdd[]	= { lxxg   , lyxg   } ;
	unsigned short  UsSineGan[]	= { 0x2AF5 , 0x2AF5 } ;
	unsigned char	UcSWFC1[]	= { 0x0B, 0x7D, 0x42 } ;	// 20Hz, 139Hz, 406Hz
	unsigned char	UcSWFC2[]	= { 0x00, 0x00, 0x00 } ;	// SWC = 0, 0, 0
	short			SsRatioSh[]	= { 345, 141, 50 } ;
	unsigned char	UcJudgeSts1, UcJudgeSts2, UcJudgeSts3 ;
	unsigned char	UcJudgeSts = SUCCESS ;
	unsigned char	UcSinFrq ;
	unsigned short	UsMSCAP1, UsMSCAP2 ;
	short			SsCalRatio ;

	UcJikuSel	&= 0x01 ;
	UcJudgeSts1	= 0x00 ;
	UcJudgeSts2	= 0x00 ;
	UcJudgeSts3	= 0x00 ;

	/* set sine wave */
	SetSineWave( UcJikuSel, __MEASURE_LOOPGAIN ) ;
	RamWriteA( wavxg , 0x7FFF );                                                // 0x13C3
    RamWriteA( wavyg , 0x7FFF );                                                // 0x13C4
    
	/* Set Servo Filter */
	RamWriteA( UsSineAdd[ UcJikuSel ], UsSineGan[ UcJikuSel ] ) ;				// Set Sine Wave input amplitude

	/* set Measure Filter */
	SetMeasFil( UcJikuSel, __MEASURE_LOOPGAIN, THROUGH ) ;

	/* Start Measure Filters */
	StartMeasFil() ;

	/* Start Sine Wave */
	StartSineWave() ;
	WitTim( 100 ) ;
	for( UcSinFrq = 0 ; UcSinFrq < 3 ; UcSinFrq++ )
	{
		RegWriteA( SWFC1, UcSWFC1[ UcSinFrq ] ) ;								// 0x00DD	[ SWB(7:4) ][ SWA(3:0) ]
		RegWriteA( SWFC2, UcSWFC2[ UcSinFrq ] ) ;								// 0x00DE

		RegWriteA( DLYCLR2, 0xC0 ) ;
		WitTim( 50 ) ;
		IntegralMes( UcSinFrq ) ;
		UsMSCAP1	= GetIntegral( 0 ) ;
		UsMSCAP2	= GetIntegral( 1 ) ;
		SsCalRatio	= CalAmpRatio( UsMSCAP1, UsMSCAP2 ) ;
		CDBG("%s, %d=> %x, %x, ratio=%x (%x) \n", 
			__func__, UcSinFrq, UsMSCAP1, UsMSCAP2, SsCalRatio, SsRatioSh[ UcSinFrq ]);
		
		if( !UcSinFrq ) {
			if( SsCalRatio < SsRatioSh[ UcSinFrq ] ) {
				if( UcJikuSel == X_DIR){
					UcJudgeSts1	= EXE_XFRQ1 ;
				}else if( UcJikuSel == Y_DIR ){
					UcJudgeSts1	= EXE_YFRQ1 ;
				}
			}
		} else if( UcSinFrq == 1 ) {
			SsCalRatio	&= 0x7FFF ;
			if( SsCalRatio > SsRatioSh[ UcSinFrq ] ) {
				if( UcJikuSel == X_DIR){
					UcJudgeSts2	= EXE_XFRQ2 ;
				}else if( UcJikuSel == Y_DIR ){
					UcJudgeSts2	= EXE_YFRQ2 ;
				}
			}
		} else {
			if( SsCalRatio > SsRatioSh[ UcSinFrq ] ) {
				if( UcJikuSel == X_DIR){
					UcJudgeSts3	= EXE_XFRQ3 ;
				}else if( UcJikuSel == Y_DIR ){
					UcJudgeSts3	= EXE_YFRQ3 ;
				}
			}
		}
	}

	/* Cut Sine input */
	RamWriteA( UsSineAdd[UcJikuSel] , 0x0000 );                                 // Set Sine Wave input amplitude

	/* Stop Sine Wave */
	StopSineWave();

	/* Stop Measure Filter */
	StopMeasFil();

	UcJudgeSts	= UcJudgeSts1 | UcJudgeSts2 | UcJudgeSts3 ;

	return( UcJudgeSts ) ;
}

//-------------------------------------------------------------------//

void EnsureWrite(uint16_t addr, int16_t data, int8_t trial)
{
	int i = 0;
	int16_t written;
	do
	{
		RamWriteA(addr, data & 0xFFFF);
		RamReadA(addr, &written);
	} while ((data !=  written) && ( i < trial ));
}

static struct msm_ois_fn_t lgit_ois_func_tbl;

int32_t	lgit_ois_on( enum ois_ver_t ver )
{
	int32_t rc = OIS_SUCCESS;
	CDBG("%s, enter %s\n", __func__,LAST_UPDATE);
	switch(ver)
	{
		case OIS_VER_RELEASE:
		{
			usleep(20000);
			rc = IniSet();
			if (rc < 0) { return rc;}
			usleep(5000);
			RtnCen(0);
			usleep(100000);
			
		}
		break;	
		case OIS_VER_DEBUG:
		case OIS_VER_CALIBRATION:
		{
			int i=0;
			usleep(20000);
			rc = IniSet();
			if (rc < 0) { return rc;}
			usleep(5000);
			rc = OIS_INIT_GYRO_ADJ_FAIL;	
			do
			{
				if (TneGvc(ver) == EXE_END) 
				{
					rc = OIS_SUCCESS;  
					break; 
				}
			} while (i++ < 5);
			
			usleep(5000);
			RtnCen(0);

			if ( (SrvGainMes(X_DIR) | SrvGainMes(Y_DIR)) != 0)
			{
				rc |= OIS_INIT_SRV_GAIN_FAIL;
			}
			
			OisOff();

			EnsureWrite(HXSEPT1, 0x0000, 5);
			EnsureWrite(HYSEPT1, 0x0000, 5);

			usleep(50000);
		}
		break;
	}	
	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	if (StCalDat.UsVerDat != 0x0204)
	{
		CDBG("%s, old module %x \n", __func__,StCalDat.UsVerDat);
		rc |= OIS_INIT_OLD_MODULE;
	}

	CDBG("%s, exit \n", __func__);
	return rc;
}

int32_t	lgit_ois_off( void )
{
	int16_t i;
	int16_t hallx,hally;

	CDBG("%s\n", __func__);
	
	OisOff();
	usleep(1000);

	RamWriteA( LXOLMT, 0x1200 );
	usleep(2000);
	RamReadA(HXTMP, &hallx);
	RamWriteA(HXSEPT1, -hallx);

	RamWriteA( LYOLMT, 0x1200 );	
	usleep(2000);
	RamReadA(HYTMP, &hally);
	RamWriteA(HYSEPT1, -hally);

	for (i=0x1100; i>=0x0000;  i-=0x100)
	{
		RamWriteA( LXOLMT,	i );	
		RamWriteA( LYOLMT,	i );
		usleep(2000);
	}

	SrvCon( X_DIR, OFF ) ;
	SrvCon( Y_DIR, OFF ) ;

	CDBG("%s, exit\n", __func__);

	return OIS_SUCCESS;
}

int32_t lgit_ois_mode(enum ois_mode_t data)
{
	int cur_mode = lgit_ois_func_tbl.ois_cur_mode;
	CDBG("%s:%d\n", __func__,data);

	if (cur_mode == data)
	{
		return OIS_SUCCESS;
	}
	
	switch(cur_mode)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_CAPTURE : 
		case OIS_MODE_VIDEO :
			OisOff();
			break;
		case OIS_MODE_CENTERING_ONLY :
			break;
		case OIS_MODE_CENTERING_OFF:
			RtnCen(0); 
			break;
	}
	
	switch(data)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_CAPTURE : 	
			VSModeConvert(OFF);
			OisEna() ;
			break;			
		case OIS_MODE_VIDEO :
			VSModeConvert(ON);
			OisEna() ;
			break;
		case OIS_MODE_CENTERING_ONLY :
			break;
		case OIS_MODE_CENTERING_OFF:
			SrvCon( X_DIR, OFF ) ;
    		SrvCon( Y_DIR, OFF ) ;
			break;
	}

	lgit_ois_func_tbl.ois_cur_mode = data;

	return OIS_SUCCESS;
}

int16_t lgit_convert_float32(uint32_t float32, uint8_t len)
{
	uint8_t sgn = float32 >> 31;
	uint16_t exp = 0xFF & (float32 >> 23);
	uint32_t frc = (0x7FFFFF & (float32)) | 0x800000;

	if (exp > 127) {
		frc = frc << (exp-127);
	} else {
		frc = frc >> (127-exp);
	}

	frc = frc >> (24-len);
	if (frc > 0x007FFF) { frc = 0x7FFF; }
	if (sgn) { frc = (~frc) + 1; }
	
	return 0xFFFF & frc;
}

int16_t lgit_convert_int32(int32_t in)
{
	if (in > 32767) return 32767;
	if (in < -32767) return -32767;
	return 0xFFFF & in;
}

/*
	[Integration note]
	(1) gyro scale match (output unit -> dps*256)
	from gyro IC & driver IC spec sheet, 
		gyro[dps] = gyro[int16] / 65.5
		gyro[int16] = gyro[float32]*0x7FFF

    ->	gyro[dps*256] = gyro[float32] * 0x7FFF * 4 * {256 / 262}
    (attention : lgit_convert_float32(gyro,18) returns gyro[flaot32]*0x7FFF*4)

	(2) hall scale match (output unit -> um*256)
	from measurement, pixel-code relation is averagely given by,
		hall[pixel] : hall[code] = 43 pixel : 8471	

	 -> hall[um*256] = hall[code]*256*1.12[um/pixels]*43[pixel]/8471 
					 = hall[code]*256/176		
*/

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 176
#define STABLE_THRESHOLD  600 // 3.04[pixel]*1.12[um/pixel]*HALL_SCALE_FACTOR

int32_t lgit_ois_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	uint32_t gyro = 0;
	int16_t hall = 0;
	int16_t target = 0;
	
	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "LGIT_ONSEMI");
	
	RamRead32A(GXADZ, &gyro);
	RamReadA(LXGZF, &target);	
	RamReadA(HXIN, &hall);

	ois_stat->gyro[0] =  lgit_convert_int32((int32_t)(lgit_convert_float32(gyro,18))*256/GYRO_SCALE_FACTOR);
	ois_stat->target[0] = lgit_convert_int32(-1*(((int32_t)target)*256/HALL_SCALE_FACTOR));
	ois_stat->hall[0] = lgit_convert_int32(((int32_t)hall)*256/HALL_SCALE_FACTOR);
	
	CDBG("%s gyrox %x, %x | targetx %x, %x | hallx  %x, %x \n",__func__, 
		gyro, ois_stat->gyro[0], target, ois_stat->target[0], hall, ois_stat->hall[0]);

	RamRead32A(GYADZ, &gyro);
	RamReadA(LYGZF, &target);
	RamReadA(HYIN, &hall);

	ois_stat->gyro[1] =  lgit_convert_int32((int32_t)(lgit_convert_float32(gyro,18))*256/GYRO_SCALE_FACTOR);
	ois_stat->target[1] = lgit_convert_int32(-1*(((int32_t)target)*256/HALL_SCALE_FACTOR));
	ois_stat->hall[1] = lgit_convert_int32(((int32_t)hall)*256/HALL_SCALE_FACTOR);

	CDBG("%s gyroy %x, %x | targety %x, %x | hally  %x, %x \n",__func__, 
		gyro, ois_stat->gyro[1], target, ois_stat->target[1], hall, ois_stat->hall[1]);

	ois_stat->is_stable = 1; 
	
	// if all value = 0, set is_stable = 0	
	if (ois_stat->hall[0] == 0 && ois_stat->hall[1] == 0 && ois_stat->gyro[0] == 0 && ois_stat->gyro[1] == 0)
	{
		ois_stat->is_stable = 0; 
	}

	// if target-hall difference is bigger than STABLE_THRESHOLD
	RamReadA(LXCFIN, &target);
	if (abs(target) > STABLE_THRESHOLD)
	{
		ois_stat->is_stable = 0;
	}

	RamReadA(LYCFIN, &hall);
	if (abs(hall) > STABLE_THRESHOLD)
	{
		ois_stat->is_stable = 0;	
	}	
	CDBG("%s stable %x, %x, %d", __func__, target,hall, ois_stat->is_stable);
	
	return OIS_SUCCESS;
}

#define LENS_MOVE_POLLING_LIMIT (10)
#define LENS_MOVE_THRESHOLD     (5) // um

int32_t lgit_ois_move_lens(int16_t target_x, int16_t target_y)
{
	int32_t rc = OIS_SUCCESS;
	int16_t offset_x, offset_y;
	int16_t oldx, oldy;
	int16_t hallx1, hally1;
	int16_t hallx2, hally2;

	//1. convert um*256 -> code
    // hall[code] = hall[pixel*256]*176/256
	offset_x = -1 * target_x * HALL_SCALE_FACTOR / 256;
	offset_y = -1 * target_y * HALL_SCALE_FACTOR / 256;

    //2. read previous condition.
	RamReadA(HXSEPT1, &oldx);	
	RamReadA(HYSEPT1, &oldy);	

	CDBG("%s offset %d,%d -> %d, %d \n",__func__, oldx, oldy, offset_x, offset_y);

	RamReadA(HXIN, &hallx1);	
	RamReadA(HYIN, &hally1);
	
	hallx1 -= oldx;
	hally1 -= oldy;
	
	CDBG("%s : hall %d, %d \n",__func__, hallx1, hally1);

	//3. perform move (ensure HXSEPT1 is set correctly)
	EnsureWrite(HXSEPT1, offset_x, 5);
	EnsureWrite(HYSEPT1, offset_y, 5);

	usleep(30000);

	//4. read new lens position
	RamReadA(HXIN, &hallx2); 
	RamReadA(HYIN, &hally2);

	hallx2 -= offset_x;
	hally2 -= offset_y;
	
	CDBG("%s : hall %d, %d \n",__func__, hallx2, hally2);

    //5. make decision.
	rc = OIS_FAIL;
	if (abs(hallx2+offset_x) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR ||
		abs(hally2+offset_y) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR )
	{
		rc = OIS_SUCCESS;
	}
#if 1	
	else if 
	(((abs(hallx1-hallx2) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR) && (oldx != offset_x))
	||((abs(hally1-hally2) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR) && (oldy != offset_y)))
	{
		rc = OIS_FAIL;	
		CDBG("%s : fail \n",__func__);
	}
	else
	{
		rc = OIS_SUCCESS;
	}
#endif

	return rc;

}


void lgit_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	lgit_ois_func_tbl.ois_on = lgit_ois_on;
	lgit_ois_func_tbl.ois_off = lgit_ois_off;
	lgit_ois_func_tbl.ois_mode = lgit_ois_mode;
	lgit_ois_func_tbl.ois_stat = lgit_ois_stat;
	lgit_ois_func_tbl.ois_move_lens = lgit_ois_move_lens;
	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	
	msm_ois_t->sid_ois = 0x48 >> 1;
	msm_ois_t->ois_func_tbl = &lgit_ois_func_tbl;
}
