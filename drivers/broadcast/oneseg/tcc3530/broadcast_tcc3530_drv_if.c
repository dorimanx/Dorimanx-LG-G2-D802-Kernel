#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

#include "tcpal_os.h"
#include "tcpal_spi.h"
#include "tcc353x_common.h"
#include "tcc353x_api.h"
#include "tcc353x_monitoring.h"

#include "broadcast_tcc353x.h"
#include "tcc3530_boot_tmm.h"

#include "broadcast_dmb_typedef.h"
#include "broadcast_dmb_drv_ifdef.h"

/*#define _DISPLAY_MONITOR_DBG_LOG_*/
/*#define _USE_ONSEG_SIGINFO_MODIFIED_MODE_*/

#define _1_SEG_FIFO_THR_	(188*16) /* 1seg interupt cycle 57ms, 416kbps, QPSK, Conv.Code:2/3, GI:1/8 */
//#define _13_SEG_FIFO_THR_	(188*60) /* MMBI interupt cycle 12ms, 7302Kbps, 16QAM, Conv.code:1/2, GI:1/4 */
#define _13_SEG_FIFO_THR_	(188*80) /* MMBI interupt cycle 18ms, 7302Kbps, 16QAM, Conv.code:1/2, GI:1/4 */
//#define _13_SEG_FIFO_THR_	(188*120) /* MMBI interupt cycle 25ms, 7302Kbps, 16QAM, Conv.code:1/2, GI:1/4 */

#ifdef _MODEL_F9J_
#define MMB_EAR_ANTENNA
#ifdef MMB_EAR_ANTENNA
#define ISDBT_DEFAULT_NOTUSE_MODE -1
#define ISDBT_UHF_MODE 0
#define ISDBT_VHF_MODE 1

#define ISDBT_DEFAULT_RETRACTABLE_MODE 0
#define ISDBT_AUTO_ATENNA_SWITCHING_MODE 1
#endif
#endif

/*
FIFO Threshold - Interrupt Interval 
188*250 : 8Mbps about 44 ms
188*160 : 8Mbps about 28 ms
188*120 : 8Mbps about 21 ms
*/

typedef enum {
	TMM_13SEG = 0,
	TMM_1SEG,
	UHF_1SEG
} EnumIsdbType;

extern Tcc353xOption_t Tcc353xOptionSingle;
extern TcpalSemaphore_t Tcc353xDrvSem;

static unsigned int frequencyTable[50] = {
	473143,479143,485143,491143,497143,
	503143,509143,515143,521143,527143,
	533143,539143,545143,551143,557143,
	563143,569143,575143,581143,587143,
	593143,599143,605143,611143,617143,
	623143,629143,635143,641143,647143,
	653143,659143,665143,671143,677143,
	683143,689143,695143,701143,707143,
	713143,719143,725143,731143,737143,
	743143,749143,755143,761143,767143
};

#define MMBI_MAX_TABLE 33
I32S MMBI_FREQ_TABLE[MMBI_MAX_TABLE] = {
	207857, 208286, 208714, 209143, 209571,
	210000, 210429, 210857, 211286, 211714,
	212143, 212571, 213000, 213429, 213857,
	214286, 214714, 215143, 215571, 216000,
	216429, 216857, 217286, 217714, 218143,
	218571, 219000, 219429, 219857, 220286,
	220714, 221143, 221571
};

static int currentBroadCast = TMM_13SEG;
static int currentSelectedChannel = -1;

static unsigned char InterleavingLen[24] =  {
	0,0,0, 4,2,1, 
	8,4,2, 16,8,4,	
	32,16,8, 62,62,62,
	62,62,62, 63,63,63
};
static int OnAir = 0;
extern I32U gOverflowcnt;

#ifdef MMB_EAR_ANTENNA
static int gIsdbtRfMode = ISDBT_DEFAULT_NOTUSE_MODE;
static int gIsdbtAntennaMode = ISDBT_DEFAULT_RETRACTABLE_MODE;

void isdbt_hw_antenna_switch(int ear_state);
void isdbt_hw_set_rf_mode(int rf_mode);
void isdbt_hw_set_antenna_mode(int antenna_mode);

extern int check_ear_state(void);
#endif

void Tcc353xStreamBufferInit(I32S _moduleIndex);
void Tcc353xStreamBufferClose(I32S _moduleIndex);
void Tcc353xStreamBufferReset(I32S _moduleIndex);
I32U Tcc353xGetStreamBuffer(I32S _moduleIndex, I08U * _buff, I32U _size);

#define GPIO_MMBI_ELNA_EN		8
#define GPIO_MMBI_ANT_SW		9
#define GPIO_1SEG_EAR_ANT_SEL_P		10
#define GPIO_LNA_PON			11

/* Body of Internel function */
int	broadcast_drv_start(void)
{
	int rc = ERROR;
	
	rc = OK;
	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_start\n");
	return rc;
}

int	broadcast_get_stop_mode(void)
{
	int rc = ERROR;
	
	rc = OK;
	TcpalPrintStatus((I08S *)"[1seg]broadcast_get_stop_mode\n");
	return rc;
}

int	broadcast_drv_if_power_on(void)
{
	int rc = ERROR;
	
	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_if_power_on\n");
	if (!tcc353x_is_power_on())
		rc = tcc353x_power_on();
	return rc;
}

int	broadcast_drv_if_power_off(void)
{
	int rc = ERROR;
	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_if_power_off\n");
	if (tcc353x_is_power_on()) {
		rc = tcc353x_power_off();
	} else {
		TcpalPrintStatus((I08S *)"[1seg] warning-already power off\n");
	}
	return rc;
}

static void Tcc353xWrapperSafeClose (void)
{
	/* close driver & power ctrl*/
	OnAir = 0;
	currentSelectedChannel = -1;
	currentBroadCast = TMM_13SEG;
	TcpalIrqDisable();
	Tcc353xApiClose(0);
	Tcc353xStreamBufferClose(0);
	Tcc353xTccspiClose(0);
	broadcast_drv_if_power_off();

	#ifdef _MODEL_F9J_
	gIsdbtRfMode = ISDBT_DEFAULT_NOTUSE_MODE;
	#endif
}

int	broadcast_drv_if_open(void)
{
	int rc = ERROR;
	
	Tcc353xStreamFormat_t streamFormat;
	int ret = 0;

	TcpalSemaphoreLock(&Tcc353xDrvSem);

	Tcc353xTccspiOpen(0);
	ret = Tcc353xApiOpen(0, &Tcc353xOptionSingle, sizeof(Tcc353xOption_t));
	if (ret != TCC353X_RETURN_SUCCESS) {
		/* driver re-open routine */
		TcpalPrintErr((I08S *) "[1seg] TCC3530 Re-init (close & open)...\n");
		Tcc353xWrapperSafeClose ();

		/* re-open driver & power ctrl*/
		broadcast_drv_if_power_on();
		Tcc353xTccspiOpen(0);
		ret = Tcc353xApiOpen(0, &Tcc353xOptionSingle, sizeof(Tcc353xOption_t));
		if (ret != TCC353X_RETURN_SUCCESS) {
			TcpalPrintErr((I08S *) "[1seg] TCC3530 Init Fail!!!\n");
			Tcc353xWrapperSafeClose ();
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return ERROR;
		}
	}

	streamFormat.pidFilterEnable = 0;
	streamFormat.syncByteFilterEnable = 1;
	streamFormat.tsErrorFilterEnable = 1;
	streamFormat.tsErrorInsertEnable = 1;
	ret = Tcc353xApiInit(0, (I08U *)TCC353X_BOOT_DATA_ISDBT13SEG,
			     TCC353X_BOOT_SIZE_ISDBT13SEG, &streamFormat);
	if (ret != TCC353X_RETURN_SUCCESS) {
		TcpalPrintErr((I08S *) "[1seg] TCC3530 Init Fail!!!\n");
		Tcc353xWrapperSafeClose ();
		rc = ERROR;
	} else {
		Tcc353xStreamBufferInit(0);
		TcpalIrqEnable();
		TcpalPrintStatus((I08S *) "[1seg] TCC3530 Init Success!!!\n");
		rc = OK;
	}

	OnAir = 1;
	#ifdef _MODEL_F9J_
	gIsdbtRfMode = ISDBT_DEFAULT_NOTUSE_MODE;
	#endif
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return rc;
}

int	broadcast_drv_if_close(void)
{
	int rc = ERROR;	
	int ret = 0;

	
	#ifdef _MODEL_F9J_
	#ifdef MMB_EAR_ANTENNA
	isdbt_hw_set_rf_mode(ISDBT_DEFAULT_NOTUSE_MODE);
	#endif
	#endif

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	OnAir = 0;
	currentSelectedChannel = -1;
	currentBroadCast = TMM_13SEG;

	/* lna control - off */
	Tcc353xApiSetGpioControl(0, 0, GPIO_LNA_PON, 0);
	Tcc353xApiSetGpioControl(0, 0, GPIO_MMBI_ELNA_EN, 0);

	TcpalIrqDisable();
	ret = Tcc353xApiClose(0);
	Tcc353xStreamBufferClose(0);
	Tcc353xTccspiClose(0);
	if(ret == TCC353X_RETURN_SUCCESS)
		rc = OK;

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return rc;
}

int	broadcast_drv_if_set_channel(struct broadcast_dmb_set_ch_info *udata)
{	
	Tcc353xTuneOptions tuneOption;
	signed long frequency = 214714; /*tmm*/
	int ret;
	int needLockCheck = 0;

	TcpalPrintStatus((I08S *)"[1seg] h0 broadcast_drv_if_set_channel\n");
	/* do not move code (Tcc353xDrvSem)-------------------------*/
#ifdef _MODEL_F9J_
#ifdef MMB_EAR_ANTENNA
	if(udata->subchannel == 22 && udata->rf_band==0) {
		isdbt_hw_set_rf_mode(ISDBT_UHF_MODE);
	} else {
		isdbt_hw_set_rf_mode(ISDBT_VHF_MODE);
	}
#endif
#endif
	/* ---------------------------------------------------------*/

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	TcpalPrintStatus((I08S *)"[1seg] h1 broadcast_drv_if_set_channel\n");

	if(OnAir == 0 || udata == NULL) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_set_channel error [!OnAir]\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	TcpalMemset (&tuneOption, 0x00, sizeof(tuneOption));

	if(udata->subchannel == 22 && udata->rf_band==0) {
		/* uhf 1segment */
		currentBroadCast = UHF_1SEG;
		currentSelectedChannel = udata->channel;
		tuneOption.rfIfType = TCC353X_LOW_IF;
		tuneOption.segmentType = TCC353X_ISDBT_1_OF_13SEG;
		tuneOption.userFifothr = _1_SEG_FIFO_THR_;

		needLockCheck = 1;

		if(udata->channel<13 || udata->channel>62) {
			TcpalPrintErr((I08S *)"[1seg] channel information error\n");
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return ERROR;
		}
		frequency = frequencyTable[udata->channel-13];
	} else if (udata->subchannel == 22) {
		/* tmm 13seg */

		if(udata->channel==0 || udata->channel>33) {
			TcpalPrintErr((I08S *)"[1seg] channel information error [%d]\n", udata->channel);
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return ERROR;
		}

		currentBroadCast = TMM_13SEG;
		currentSelectedChannel = udata->channel;
		tuneOption.rfIfType = TCC353X_ZERO_IF;
		tuneOption.segmentType = TCC353X_ISDBTMM;
		tuneOption.userFifothr = _13_SEG_FIFO_THR_;

		if(udata->channel == 7)
			tuneOption.tmmSet = C_1st_13Seg;
		else if(udata->channel == 20)
			tuneOption.tmmSet = C_2nd_13Seg;
		else if(udata->channel == 27)
			tuneOption.tmmSet = B_2nd_13Seg;
		else if(udata->channel == 14)
			tuneOption.tmmSet = A_1st_13Seg;
		else {
			tuneOption.tmmSet = UserDefine_Tmm13Seg;
			frequency = MMBI_FREQ_TABLE[udata->channel-1];
		}
	} else {
		/* tmm 1seg */

		if(udata->channel==0 || udata->channel>33) {
			TcpalPrintErr((I08S *)"[1seg] channel information error [%d]\n", udata->channel);
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return ERROR;
		}

		currentBroadCast = TMM_1SEG;
		currentSelectedChannel = udata->channel;
		tuneOption.rfIfType = TCC353X_LOW_IF;
		tuneOption.segmentType = TCC353X_ISDBTMM;
		tuneOption.userFifothr = _1_SEG_FIFO_THR_;

		if(udata->channel < 8 && udata->channel > 0)
			tuneOption.tmmSet = A_1st_1Seg+udata->channel-1;
		else if(udata->channel < 21 && udata->channel > 13)
			tuneOption.tmmSet = B_1st_1Seg+udata->channel-14;
		else if(udata->channel < 34 && udata->channel > 26)
			tuneOption.tmmSet = C_1st_1Seg+udata->channel-27;
		else {
			tuneOption.tmmSet = UserDefine_Tmm1Seg;
			frequency = MMBI_FREQ_TABLE[udata->channel-1];
		}
	}

	TcpalPrintStatus((I08S *)"[1seg] h2 broadcast_drv_if_set_channel\n");
	TcpalIrqDisable();
	gOverflowcnt = 0;

	TcpalPrintStatus((I08S *)"[1seg] h3 broadcast_drv_if_set_channel\n");

	/* change antenna switching */
	if(currentBroadCast == UHF_1SEG)
		Tcc353xApiSetGpioControl(0, 0, GPIO_MMBI_ANT_SW, 1);
	else
		Tcc353xApiSetGpioControl(0, 0, GPIO_MMBI_ANT_SW, 0);
		
	/* lna control - high gain */
	/* high gain : PON 1, EN 0   low gain : PON 0, EN 1 */
	Tcc353xApiSetGpioControl(0, 0, GPIO_LNA_PON, 1);
	Tcc353xApiSetGpioControl(0, 0, GPIO_MMBI_ELNA_EN, 0);

	if(needLockCheck && udata->mode == 1)	/* Scan mode & need lock check */
		ret = Tcc353xApiChannelSearch(0, frequency, &tuneOption);
	else				/* normal mode */
		ret = Tcc353xApiChannelSelect(0, frequency, &tuneOption);
	Tcc353xStreamBufferReset(0);
	Tcc353xMonitoringApiInit(0, 0);
	TcpalIrqEnable();

	if(ret!=TCC353X_RETURN_SUCCESS) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		TcpalPrintStatus((I08S *)"[1seg] h4 broadcast_drv_if_set_channel\n");
		return ERROR;
	}

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	TcpalPrintStatus((I08S *)"[1seg] h5 broadcast_drv_if_set_channel\n");
	return OK;
}

int	broadcast_drv_if_resync(void)
{
	int rc = ERROR;
	/*
	TCC3530 use auto-resync
	*/
	rc = OK;
	return rc;
}

int	broadcast_drv_if_detect_sync(struct broadcast_dmb_sync_info *udata)
{
	IsdbLock_t lock;
	I08U reg;

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_detect_sync error [!OnAir]\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	Tcc353xApiRegisterRead(0, 0, 0x0B, &reg, 1);
	Tcc353xApiParseIsdbSyncStat(&lock, reg);

	if(lock.TMCC)
		udata->sync_status = 3;
	else if(lock.CFO)
		udata->sync_status = 1;
	else
		udata->sync_status = 0;

	udata->sync_ext_status = reg;
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return OK;
}

static int Tcc353xWrapperGetLayerInfo(int layer, Tcc353xStatus_t *st)
{
	int ret = 0;
	unsigned char modulation;
	unsigned char cr;
	unsigned char mode;
	unsigned int intLen,outIntLen;
	unsigned char segNo;
	unsigned int temp;
	
	if(layer==0) {
		modulation = (st->opstat.AMod & 0x03);
		cr = (st->opstat.ACr & 0x07);
		mode = st->opstat.mode;
		intLen = st->opstat.AIntLen;
		segNo = st->opstat.ASegNo;
	} else {
		modulation = (st->opstat.BMod & 0x03);
		cr = (st->opstat.BCr & 0x07);
		mode = st->opstat.mode;
		intLen = st->opstat.BIntLen;
		segNo = st->opstat.BSegNo;
	}

	if(((st->opstat.syncStatus>>8)&0x0F)<0x0C)
		return 0xFFFF;

	ret = (modulation << 13);
	ret |= (cr << 10);
	temp = intLen * 3 + mode;
	if(temp>23)
		outIntLen = 62; /*set others*/
	else
		outIntLen = InterleavingLen[temp];
	ret |= ((outIntLen & 0x3F)<<4);
	ret |=  (segNo & 0x0F);

	return ret;
}

static int Tcc353xWrapperGetTmccInfo(Tcc353xStatus_t *st)
{
	int ret = 0;
	
	if(!st->opstat.dataState)
		return 0xFF;

	ret = ((st->opstat.sysId & 0x03)<<6);
	ret |= ((st->opstat.tmccSwitchCnt & 0x0F)<<2);
	ret |= ((st->opstat.af & 0x01)<<1);
	ret |= (st->opstat.pr & 0x01);
	
	return ret;
}

static int Tcc353xWrapperGetReceiveStat(Tcc353xStatus_t *st)
{
	int ret = 0;
	
	if(st->opstat.dataState)
		ret = ((st->opstat.af & 0x01)<<7);

	if(st->opstat.dataState)
		ret |= 0x00;
	else if(st->status.isdbLock.TMCC)
		ret |= 0x01;
	else if(st->status.isdbLock.CFO)
		ret |= 0x02;
	else
		ret |= 0x03;

	return ret;
}

static int Tcc353xWrapperGetScanStat(Tcc353xStatus_t *st)
{
	int ret = 0;
	
	if(st->status.isdbLock.TMCC)
		return 2;
	else if(st->status.isdbLock.CFO)
		return 1;
	else
		return 3;

	return ret;
}

static int Tcc353xWrapperGetSysInfo(Tcc353xStatus_t *st)
{
	int ret = 0;
	int temp = 0;
	
	if((st->opstat.syncStatus>>8&0x0F)<0x0C)
		return 0xFF;

	if(st->opstat.gi==0)
		temp = 3;
	else if(st->opstat.gi==1)
		temp = 2;
	else if(st->opstat.gi==2)
		temp = 1;
	else
		temp = 0;

	ret = (temp<<4);
	ret |= ((st->opstat.mode&0x03)<<6);

	return ret;
}

#ifdef _DISPLAY_MONITOR_DBG_LOG_
char *cModulation[8] = {"DQPSK","QPSK","16QAM","64QAM","reserved","reserved","reserved","non hier"};
char *cCR[8] = {"1/2","2/3","3/4","5/6","7/8","reserved","reserved","non hier"};

char *cSysInd[8] = {"TV","Audio","reserved","reserved","reserved","reserved","reserved","reserved"};
char *cEmergency[8] = {"No Emergency","Emergency","reserved","reserved","reserved","reserved","reserved","reserved"};
char *cPartial[8] = {"No Partial reception","Partial reception","reserved","reserved","reserved","reserved","reserved","reserved"};

char *cEmergencyStart[8] = {"No Emergency Start","Emergency Start","reserved","reserved","reserved","reserved","reserved","reserved"};
char *cRcvStatus[8] = {"TS out","Frame Sync","Mode detect","mode searching","reserved","reserved","reserved","reserved"};

char *cScanStat[8] = {"Scan Fail","Scan decision...","Channel Exist","No channel","reserved","reserved","reserved","reserved"};
char *cMode[8] = {"Mode1","Mode2","Mode3","reserved","reserved","reserved","reserved","reserved"};
char *cGI[8] = {"1/32","1/16","1/8","1/4","reserved","reserved","reserved","reserved"};
#endif

int	broadcast_drv_if_get_sig_info(struct broadcast_dmb_control_info *pInfo)
{	
	int ret = 0;
	Tcc353xStatus_t st;

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if(OnAir == 0 || pInfo==NULL) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	ret =Tcc353xMonitoringApiGetStatus(0, 0, &st);
	if(ret != TCC353X_RETURN_SUCCESS) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	Tcc353xMonitoringApiAntennaPercentage (0, &st, sizeof(Tcc353xStatus_t));

	if(currentBroadCast == UHF_1SEG) {
		pInfo->sig_info.info.oneseg_info.lock = st.status.isdbLock.TMCC;
		pInfo->sig_info.info.oneseg_info.cn = st.status.snr.avgValue;
		pInfo->sig_info.info.oneseg_info.ber = st.status.viterbiber[0].avgValue;
		pInfo->sig_info.info.oneseg_info.per = st.status.tsper[0].avgValue;
		pInfo->sig_info.info.oneseg_info.agc = st.bbLoopGain;
		pInfo->sig_info.info.oneseg_info.rssi = st.status.rssi.avgValue/100;
		pInfo->sig_info.info.oneseg_info.ErrTSP = st.opstat.ARsErrorCnt;
		pInfo->sig_info.info.oneseg_info.TotalTSP = st.opstat.ARsCnt;

		if(st.antennaPercent[0]>=80)
			pInfo->sig_info.info.oneseg_info.antenna_level = 3;
		else if(st.antennaPercent[0]>=60)
			pInfo->sig_info.info.oneseg_info.antenna_level = 3;
		else if(st.antennaPercent[0]>=40)
			pInfo->sig_info.info.oneseg_info.antenna_level = 2;
		else if(st.antennaPercent[0]>=20)
			pInfo->sig_info.info.oneseg_info.antenna_level = 1;
		else
			pInfo->sig_info.info.oneseg_info.antenna_level = 0;

#if defined (_USE_ONSEG_SIGINFO_MODIFIED_MODE_)
		/*
		Num : Modulation
		Exp : CR
		mode : GI
		*/
		if((st.opstat.syncStatus>>8&0x0F)<0x0C) {
			pInfo->sig_info.info.oneseg_info.Num = 0xFF;
			pInfo->sig_info.info.oneseg_info.Exp = 0xFF;
			pInfo->sig_info.info.oneseg_info.mode = 0xFF;
		} else {
			pInfo->sig_info.info.oneseg_info.Num = (st.opstat.AMod & 0x03);
			pInfo->sig_info.info.oneseg_info.Exp = (st.opstat.ACr & 0x07);

			if(st.opstat.gi==0)
				pInfo->sig_info.info.oneseg_info.mode = 3; /* GI - 1/4 */
			else if(st.opstat.gi==1)
				pInfo->sig_info.info.oneseg_info.mode = 2; /* GI - 1/8 */
			else if(st.opstat.gi==2)
				pInfo->sig_info.info.oneseg_info.mode = 1; /* GI - 1/16 */
			else
				pInfo->sig_info.info.oneseg_info.mode = 0; /* GI - 1/32 */

#ifdef _DISPLAY_MONITOR_DBG_LOG_
			TcpalPrintStatus((I08S *)"[1seg][monitor] Modulation [%s] CR[%s] GI[%s]\n",
				cModulation[(pInfo->sig_info.info.oneseg_info.Num)&0x07],
				cCR[(pInfo->sig_info.info.oneseg_info.Exp)&0x07],
				cGI[(pInfo->sig_info.info.oneseg_info.mode)&0x03] );
#endif
		}
#else
		pInfo->sig_info.info.oneseg_info.Num = 0;
		pInfo->sig_info.info.oneseg_info.Exp = 0;
		pInfo->sig_info.info.oneseg_info.mode = 0;
#endif /* _USE_ONSEG_SIGINFO_MODIFIED_MODE_ */

#ifdef _DISPLAY_MONITOR_DBG_LOG_
		TcpalPrintStatus((I08S *)"[1seg][monitor] lock[%d] Antenna[%d] cn[%d] ber[%d] per[%d] rssi[%d] errTSP[%d/%d]\n", 
				pInfo->sig_info.info.oneseg_info.lock,
				pInfo->sig_info.info.oneseg_info.antenna_level, 
				pInfo->sig_info.info.oneseg_info.cn, 
				pInfo->sig_info.info.oneseg_info.ber, 
				pInfo->sig_info.info.oneseg_info.per, 
				pInfo->sig_info.info.oneseg_info.rssi,
				pInfo->sig_info.info.oneseg_info.ErrTSP,
				pInfo->sig_info.info.oneseg_info.TotalTSP);

#endif
	} else {
		int layer;
		layer = pInfo->cmd_info.layer;

		switch(pInfo->cmd_info.cmd)
		{
		case 1:
			pInfo->sig_info.info.mmb_info.cn = 
					st.status.snr.avgValue;

			/*
			pInfo->sig_info.info.mmb_info.ber = 
					st.status.viterbiber[layer].avgValue;
					st.opstat.BRsCnt1;
			pInfo->sig_info.info.mmb_info.per = 
					st.status.tsper[layer].avgValue;
			*/

			if(layer==0)
				pInfo->sig_info.info.mmb_info.ber = 
						st.opstat.ARsErrorCnt;
			else
				pInfo->sig_info.info.mmb_info.ber = 
						st.opstat.BRsErrorCnt;

			if(layer==0)
				pInfo->sig_info.info.mmb_info.per = 
						st.opstat.ARsOverCnt;
			else
				pInfo->sig_info.info.mmb_info.per = 
						st.opstat.BRsOverCnt;

			if(layer==0)
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.ARsCnt;
			else
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.BRsCnt;
			
			pInfo->sig_info.info.mmb_info.layerinfo =  
				Tcc353xWrapperGetLayerInfo(layer, &st);

			pInfo->sig_info.info.mmb_info.tmccinfo = 
				Tcc353xWrapperGetTmccInfo(&st);
			
			pInfo->sig_info.info.mmb_info.receive_status = 
					Tcc353xWrapperGetReceiveStat(&st);
					
			pInfo->sig_info.info.mmb_info.rssi = 
				(st.status.rssi.avgValue);
				
			pInfo->sig_info.info.mmb_info.scan_status =
				Tcc353xWrapperGetScanStat(&st);
				
			pInfo->sig_info.info.mmb_info.sysinfo =
				Tcc353xWrapperGetSysInfo(&st);

#ifdef _DISPLAY_MONITOR_DBG_LOG_
			TcpalPrintStatus((I08S *)"[mmbi][monitor] Layer[%d] cn[%d] ber[%d] per[%d] totalTSP[%d] layerinfo[%d]\n",
					layer, 
					pInfo->sig_info.info.mmb_info.cn, 
					pInfo->sig_info.info.mmb_info.ber,
					pInfo->sig_info.info.mmb_info.per, 
					pInfo->sig_info.info.mmb_info.TotalTSP, 
					pInfo->sig_info.info.mmb_info.layerinfo);
			TcpalPrintStatus((I08S *)"[mmbi][monitor] tmccinfo[%d] status[%d] rssi[%d] scanstat[%d] sysinfo[%d]\n",
				pInfo->sig_info.info.mmb_info.tmccinfo,
				pInfo->sig_info.info.mmb_info.receive_status,
				pInfo->sig_info.info.mmb_info.rssi,
				pInfo->sig_info.info.mmb_info.scan_status,
				pInfo->sig_info.info.mmb_info.sysinfo);
#endif

			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				if(pInfo->sig_info.info.mmb_info.layerinfo==0xFFFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] Layer info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] Modulation [%s] CR[%s] TimeInterleave[%d] Segment[%d]\n",
					cModulation[(pInfo->sig_info.info.mmb_info.layerinfo>>13)&0x07],
					cCR[(pInfo->sig_info.info.mmb_info.layerinfo>>10)&0x07],
					(pInfo->sig_info.info.mmb_info.layerinfo>>4)&0x1F,
					(pInfo->sig_info.info.mmb_info.layerinfo)&0x0F );
				}

				if(pInfo->sig_info.info.mmb_info.tmccinfo==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] tmcc info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] System [%s] transParam[%d] %s %s\n",
						cSysInd[(pInfo->sig_info.info.mmb_info.tmccinfo>>6)&0x03],
						(pInfo->sig_info.info.mmb_info.tmccinfo>>2)&0x0F,
						cEmergency[(pInfo->sig_info.info.mmb_info.tmccinfo>>1)&0x01],
						cPartial[(pInfo->sig_info.info.mmb_info.tmccinfo)&0x01] );
				}

				if(pInfo->sig_info.info.mmb_info.receive_status==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] receive status fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] %s %s\n",
						cEmergencyStart[(pInfo->sig_info.info.mmb_info.receive_status>>7)&0x01],
						cRcvStatus[(pInfo->sig_info.info.mmb_info.receive_status)&0x07] );
				}

				TcpalPrintStatus((I08S *)"[mmbi][monitor] scan status [%s]\n",
					cScanStat[(pInfo->sig_info.info.mmb_info.scan_status)&0x03] );
				
				if(pInfo->sig_info.info.mmb_info.sysinfo==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] system info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] %s GI[%s]\n",
						cMode[(pInfo->sig_info.info.mmb_info.sysinfo>>6)&0x03],
						cGI[(pInfo->sig_info.info.mmb_info.sysinfo>>4)&0x03] );
				}
			}
#endif
		break;

		case 2:
			/*
			pInfo->sig_info.info.mmb_info.ber = 
				st.status.viterbiber[layer].avgValue;
			*/

			if(layer==0)
				pInfo->sig_info.info.mmb_info.ber = 
						st.opstat.ARsErrorCnt;
			else
				pInfo->sig_info.info.mmb_info.ber = 
						st.opstat.BRsErrorCnt;

			if(layer==0)
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.ARsCnt;
			else
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.BRsCnt;

			TcpalPrintStatus((I08S *)"[mmbi][monitor] ber[%d] TotalTSP[%d]\n",
					pInfo->sig_info.info.mmb_info.ber,
					pInfo->sig_info.info.mmb_info.TotalTSP);
		break;

		case 3:
			/*
			pInfo->sig_info.info.mmb_info.per = 
				st.status.tsper[layer].avgValue;
			*/

			if(layer==0)
				pInfo->sig_info.info.mmb_info.per = 
						st.opstat.ARsOverCnt;
			else
				pInfo->sig_info.info.mmb_info.per = 
						st.opstat.BRsOverCnt;

			if(layer==0)
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.ARsCnt;
			else
				pInfo->sig_info.info.mmb_info.TotalTSP = 
						st.opstat.BRsCnt;
			
			TcpalPrintStatus((I08S *)"[mmbi][monitor] per[%d] TotalTSP[%d]\n",
					pInfo->sig_info.info.mmb_info.per,
					pInfo->sig_info.info.mmb_info.TotalTSP);
		break;

		case 4:
			pInfo->sig_info.info.mmb_info.cn = 
				st.status.snr.avgValue;
			TcpalPrintStatus((I08S *)"[mmbi][monitor] cn[%d]\n",
					pInfo->sig_info.info.mmb_info.cn);
		break;

		case 5:
			pInfo->sig_info.info.mmb_info.cn = 
				st.status.snr.avgValue;
			TcpalPrintStatus((I08S *)"[mmbi][monitor] cn[%d]\n",
					pInfo->sig_info.info.mmb_info.cn);
		break;

		case 6:
			pInfo->sig_info.info.mmb_info.layerinfo =  
				Tcc353xWrapperGetLayerInfo(layer, &st);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  layer info[%d]\n",
					pInfo->sig_info.info.mmb_info.layerinfo);

			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				if(pInfo->sig_info.info.mmb_info.layerinfo==0xFFFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] Layer info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] Modulation [%s] CR[%s] TimeInterleave[%d] Segment[%d]\n",
					cModulation[(pInfo->sig_info.info.mmb_info.layerinfo>>13)&0x07],
					cCR[(pInfo->sig_info.info.mmb_info.layerinfo>>10)&0x07],
					(pInfo->sig_info.info.mmb_info.layerinfo>>4)&0x1F,
					(pInfo->sig_info.info.mmb_info.layerinfo)&0x0F );
				}
			}
#endif
		break;

		case 7:
			pInfo->sig_info.info.mmb_info.receive_status = 
					Tcc353xWrapperGetReceiveStat(&st);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  rcv status[%d]\n",
					pInfo->sig_info.info.mmb_info.receive_status);
			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				if(pInfo->sig_info.info.mmb_info.receive_status==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] receive status fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] %s %s\n",
						cEmergencyStart[(pInfo->sig_info.info.mmb_info.receive_status>>7)&0x01],
						cRcvStatus[(pInfo->sig_info.info.mmb_info.receive_status)&0x07] );
				}
			}
#endif
		break;

		case 8:
			pInfo->sig_info.info.mmb_info.rssi = 
				(st.status.rssi.avgValue);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  rssi[%d]\n",
					pInfo->sig_info.info.mmb_info.rssi);
		break;

		case 9:
			pInfo->sig_info.info.mmb_info.scan_status =
				Tcc353xWrapperGetScanStat(&st);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  scan status[%d]\n",
					pInfo->sig_info.info.mmb_info.scan_status);
			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				char *cScanStat[8] = {"Scan Fail","Scan decision...","Channel Exist","No channel","reserved","reserved","reserved","reserved"};
				TcpalPrintStatus((I08S *)"[mmbi][monitor] scan status [%s]\n",
					cScanStat[(pInfo->sig_info.info.mmb_info.scan_status)&0x03] );
			}
#endif
		break;

		case 10:
			pInfo->sig_info.info.mmb_info.sysinfo =
				Tcc353xWrapperGetSysInfo(&st);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  sys info[%d]\n",
					pInfo->sig_info.info.mmb_info.sysinfo);
			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				if(pInfo->sig_info.info.mmb_info.sysinfo==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] system info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] %s GI[%s]\n",
						cMode[(pInfo->sig_info.info.mmb_info.sysinfo>>6)&0x03],
						cGI[(pInfo->sig_info.info.mmb_info.sysinfo>>4)&0x03] );
				}
			}
#endif
		break;

		case 11:
			pInfo->sig_info.info.mmb_info.tmccinfo = 
				Tcc353xWrapperGetTmccInfo(&st);
			TcpalPrintStatus((I08S *)"[mmbi][monitor]  tmcc info[%d]\n",
					pInfo->sig_info.info.mmb_info.tmccinfo);
			/* for debugging log */
#ifdef _DISPLAY_MONITOR_DBG_LOG_
			{
				if(pInfo->sig_info.info.mmb_info.tmccinfo==0xFF) {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] tmcc info fail\n");
				} else {
					TcpalPrintStatus((I08S *)"[mmbi][monitor] System [%s] transParam[%d] %s %s\n",
						cSysInd[(pInfo->sig_info.info.mmb_info.tmccinfo>>6)&0x03],
						(pInfo->sig_info.info.mmb_info.tmccinfo>>2)&0x0F,
						cEmergency[(pInfo->sig_info.info.mmb_info.tmccinfo>>1)&0x01],
						cPartial[(pInfo->sig_info.info.mmb_info.tmccinfo)&0x01] );
				}
			}
#endif
		break;

		default:
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return -1;
		break;
		}
	}

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return OK;
}

int	broadcast_drv_if_get_ch_info(struct broadcast_dmb_ch_info *ch_info)
{
	int rc = ERROR;

	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg][no semaphore] broadcast_drv_if_get_ch_info error [!OnAir]\n");
		return ERROR;
	}

	/*
	Unused function
	*/
	rc = OK;
	return rc;
}

int	broadcast_drv_if_get_dmb_data(struct broadcast_dmb_data_info *pdmb_data)
{
	if(OnAir == 0 || pdmb_data==NULL) {
		return ERROR;
	}

	if(pdmb_data->data_buf == NULL) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_get_dmb_data[ERR] data_buf is null\n");
		return ERROR;
	}

	if(pdmb_data->data_buf_size < 188) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_get_dmb_data[ERR] buffsize < 188\n");
		return ERROR;
	}

#if 1
	pdmb_data->copied_size = (unsigned int) (Tcc353xGetStreamBuffer(0, pdmb_data->data_buf, pdmb_data->data_buf_size));
	pdmb_data->packet_cnt = pdmb_data->copied_size / 188;
	return OK;
#else
	struct dataprotocol_
	{
		unsigned short reserved;
		unsigned char subch;
		unsigned short size;
		unsigned char datatype;
	};
	struct dataprotocol_ *datap;
	datap = (struct dataprotocol_ *) pdmb_data->data_buf;

	pdmb_data->copied_size = (unsigned int) (Tcc353xGetStreamBuffer(0, &pdmb_data->data_buf[6], pdmb_data->data_buf_size));
	if(pdmb_data->copied_size) {
		pdmb_data->packet_cnt = pdmb_data->copied_size / 188;
		datap->reserved = 0x00;
		datap->subch = (unsigned char)(currentSelectedChannel & 0xFF);
		datap->size = (unsigned short)(pdmb_data->copied_size & 0xFFFF);
		datap->datatype = 0x00;
	} else {
		pdmb_data->packet_cnt = 0;
	}
	return OK;
#endif
}

int	broadcast_drv_if_reset_ch(void)
{
	int ret = 0;
	int rc = ERROR;

	TcpalPrintStatus((I08S *)"[1seg] j1 broadcast_drv_if_reset_ch\n");
	TcpalSemaphoreLock(&Tcc353xDrvSem);
	TcpalPrintStatus((I08S *)"[1seg] j2 broadcast_drv_if_reset_ch\n");
	
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_reset_ch error [!OnAir]\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		TcpalPrintErr((I08S *)"[1seg] j0 broadcast_drv_if_reset_ch[!OnAir]\n");
		return ERROR;
	}

	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_if_reset_ch\n");
	ret = Tcc353xApiStreamStop(0);

	if (ret!=TCC353X_RETURN_SUCCESS)
		rc = ERROR;
	else
		rc = OK;

	TcpalPrintStatus((I08S *)"[1seg] j3 broadcast_drv_if_reset_ch\n");
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	TcpalPrintStatus((I08S *)"[1seg] j4 broadcast_drv_if_reset_ch\n");
	return rc;
}

int	broadcast_drv_if_user_stop(int mode)
{
	int ret = 0;
	int rc = ERROR;

	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg][no semaphore] broadcast_drv_if_user_stop error [!OnAir]\n");
		return ERROR;
	}

	TcpalPrintStatus((I08S *)"[1seg][no semaphore] broadcast_drv_if_user_stop\n");
	Tcc353xApiUserLoopStopCmd(0);
	ret = OK;
	return rc;
}

int	broadcast_drv_if_select_antenna(unsigned int sel)
{
	int rc = ERROR;

	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg][no semaphore] broadcast_drv_if_select_antenna error [!OnAir]\n");
		return ERROR;
	}
	
	rc = OK;
	return rc;
}

int	broadcast_drv_if_isr(void)
{
	int rc = ERROR;
	
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_isr error [!OnAir]\n");
		return ERROR;
	}
	
	rc = OK;
	return rc;
}

int	broadcast_drv_if_read_control(char *buf, unsigned int size)
{
	int ret = 0;

	if(OnAir == 0 || buf == NULL)
		return 0;

	ret = (int)(Tcc353xGetStreamBuffer(0, buf, size));
	return ret;
}

int	broadcast_drv_if_get_mode (unsigned short *mode)
{
	int rc = ERROR;

	if(mode == NULL) {
		return ERROR;
	}

	if(OnAir == 0 || currentSelectedChannel== -1) {
		mode[0] = 0xFFFF;
	} else {
		unsigned short channel_num;
		unsigned short band = 0;
		unsigned short rcvSegment = 0;

		channel_num = currentSelectedChannel;

		if(currentBroadCast == UHF_1SEG) {
			band = 0;
			rcvSegment = 1;
		} else if(currentBroadCast == TMM_1SEG) {
			band = 1;
			rcvSegment = 1;
		} else {
			band = 1;
			rcvSegment = 0;
		}

		mode[0] = ((channel_num&0xFF)<<8) | 
				    ((band&0x0F)<<4) | 
				    (rcvSegment&0x0F);
	}

	rc = OK;
	return rc;
}

int	broadcast_drv_control_1SegEarAntennaSel (unsigned int data)
{
	/* data 0 :low, else high */
	int rc = ERROR;

	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_control_1SegEarAntennaSel error [!OnAir]\n");
		return ERROR;
	}

	if(data==0)
		Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
	else
		Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 1);//EAR ANT

	rc = OK;
	return rc;
}


#ifdef MMB_EAR_ANTENNA
//ear antenna switch for ear fw
void isdbt_hw_antenna_switch(int ear_state)
{
	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if(gIsdbtAntennaMode == ISDBT_DEFAULT_RETRACTABLE_MODE)
	{
		printk("default retractable ant. so don't work");
	}
	else
	{
		if(gIsdbtRfMode == ISDBT_VHF_MODE)
		{
			//state ==> ear on : 1(ear antenna high), ear off : 0(retractable antenna low)
			if(ear_state == 1)
			{
				//now ear on for ear fw
				Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 1);//EAR ANT
				printk("mmbi ear on detect 1 \n");
			}
			else if(ear_state == 0)
			{
				//now ear off
				Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
				printk("mmbi ear off detect 0 \n");
			}
			else
			{
				//nothing to do
				printk("mmbi nothing to do \n");
			}
		}
		else
		{
			printk("UHF Mode not working \n");
			//nothing to do
		}
	}
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
}
EXPORT_SYMBOL(isdbt_hw_antenna_switch);

//rf mode setting -- for isdbt fw
void isdbt_hw_set_rf_mode(int rf_mode)
{
	//need to check ear state ==> 0:ear off, 1:ear on
	int check_ear = 0; 
	TcpalSemaphoreLock(&Tcc353xDrvSem);

	check_ear = check_ear_state(); //ear fw confirm

	if(gIsdbtAntennaMode == ISDBT_DEFAULT_RETRACTABLE_MODE)
	{//default
		Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
		printk("default retractable antenna mode!!! \n");
		gIsdbtRfMode = rf_mode;
	}
	else
	{//auto switching
		if(gIsdbtRfMode == ISDBT_DEFAULT_NOTUSE_MODE)
		{
			if(rf_mode == ISDBT_VHF_MODE)
			{
				if(check_ear == 1)
				{
					//state ear on for ear fw
					Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 1);//EAR ANT
					printk("auto switching mmbi start ear on \n");
				}
				else
				{
					//state ear off
					Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
					printk("auto switching mmbi start ear off \n");
				}
				
			}
			else
			{
				Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
				printk("auto switching UHF default retractable = %d \n", check_ear);
			}

			//UHF : 0, VHF : 1,  None : -1
			gIsdbtRfMode = rf_mode;
		}
		else
		{
			//only for close
			printk("auto switching mode, but already setting check rf mode = %d ear state = %d\n", gIsdbtRfMode, check_ear);
			gIsdbtRfMode = rf_mode;
		}
	}
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
}
EXPORT_SYMBOL(isdbt_hw_set_rf_mode);

//setting antenna mode -- for ui setting
void isdbt_hw_set_antenna_mode(int antenna_mode)
{
	//need to check ear state ==> 0:ear off, 1:ear on
	int check_ear = 0; 
	TcpalSemaphoreLock(&Tcc353xDrvSem);
	
	if(gIsdbtRfMode == ISDBT_DEFAULT_NOTUSE_MODE)
	{
		gIsdbtAntennaMode = antenna_mode;
	}
	else
	{
		if(gIsdbtRfMode == ISDBT_VHF_MODE)
		{
			check_ear = check_ear_state(); //ear fw confirm
			
			if(check_ear == 1)
			{
				//state ear on for ear fw
				Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 1);//EAR ANT
				printk("mmbi start ear on \n");
			}
			else
			{
				//state ear off
				Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
				printk("mmbi start ear off \n");
			}

		}
		else
		{
			//state ear off
			Tcc353xApiSetGpioControl(0, 0, GPIO_1SEG_EAR_ANT_SEL_P, 0);//Retractable ANT
			printk("mmbi start ear off \n");
		}

		gIsdbtAntennaMode = antenna_mode;
	}

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
}
EXPORT_SYMBOL(isdbt_hw_set_antenna_mode);
#endif

/*                                                                          */
/*--------------------------------------------------------------------------*/


/* optional part when we include driver code to build-on
it's just used when we make device driver to module(.ko)
so it doesn't work in build-on */
MODULE_DESCRIPTION("TCC3530 ISDB-Tmm device driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TCC");
