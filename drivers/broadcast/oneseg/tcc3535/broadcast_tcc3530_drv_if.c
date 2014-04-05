#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

#include "tcpal_os.h"
#include "tcpal_spi.h"
#include "tcpal_i2c.h"
#include "tcc353x_common.h"
#include "tcc353x_api.h"
#include "tcc353x_monitoring.h"
#include "tcc353x_user_defines.h"

#include "broadcast_tcc353x.h"
#include "broadcast_dmb_typedef.h"
#include "broadcast_dmb_drv_ifdef.h"

/*#define _DISPLAY_MONITOR_DBG_LOG_*/
/*#define _USE_ONSEG_SIGINFO_MODIFIED_MODE_*/
#define _USE_MONITORING_TIME_GAP_

typedef enum {
	TMM_13SEG = 0,
	TMM_1SEG,
	UHF_1SEG,
	UHF_13SEG,
} EnumIsdbType;

typedef enum {
	ENUM_GET_ALL = 1,
	ENUM_GET_BER,
	ENUM_GET_PER,
	ENUM_GET_CN,
	ENUM_GET_CN_PER_LAYER,
	ENUM_GET_LAYER_INFO,
	ENUM_GET_RECEIVE_STATUS,
	ENUM_GET_RSSI,
	ENUM_GET_SCAN_STATUS,
	ENUM_GET_SYS_INFO,
	ENUM_GET_TMCC_INFO,
	ENUM_GET_ONESEG_SIG_INFO
} EnumSigInfo;

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

I32S OnAir = 0;

#if defined (_USE_MONITORING_TIME_GAP_)
#define _MON_TIME_INTERVAL_ 	500
TcpalTime_t CurrentMonitoringTime = 0;
#endif

/* Body of Internel function */
int	broadcast_drv_start(void)
{
	int rc;
	rc = OK;
	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_start\n");
	return rc;
}

int	broadcast_get_stop_mode(void)
{
	int rc;
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
	Tcc353xApiClose(0);
	Tcc353xI2cClose(0);
	broadcast_drv_if_power_off();
}

int	broadcast_drv_if_open(void)
{
	int rc = ERROR;
	
	Tcc353xStreamFormat_t streamFormat;
	int ret = 0;

	TcpalSemaphoreLock(&Tcc353xDrvSem);

	Tcc353xI2cOpen(0);
	ret = Tcc353xApiOpen(0, &Tcc353xOptionSingle, sizeof(Tcc353xOption_t));
	if (ret != TCC353X_RETURN_SUCCESS) {
		/* driver re-open routine */
		TcpalPrintErr((I08S *) "[1seg] TCC3530 Re-init (close & open)...\n");
		Tcc353xWrapperSafeClose ();

		/* re-open driver & power ctrl*/
		broadcast_drv_if_power_on();
		Tcc353xI2cOpen(0);
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
#if defined (_MODEL_TCC3535_) && defined (_TCC3535_ROM_MASK_VER_)
	ret = Tcc353xApiInit(0, NULL, 0, &streamFormat);
#else
	ret = Tcc353xApiInit(0, (I08U *)TCC353X_BOOT_DATA_ISDBT13SEG,
			     TCC353X_BOOT_SIZE_ISDBT13SEG, &streamFormat);
#endif
	if (ret != TCC353X_RETURN_SUCCESS) {
		TcpalPrintErr((I08S *) "[1seg] TCC3530 Init Fail!!!\n");
		Tcc353xWrapperSafeClose ();
		rc = ERROR;
	} else {
		TcpalPrintStatus((I08S *) "[1seg] TCC3530 Init Success!!!\n");
		rc = OK;
	}

	OnAir = 1;
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return rc;
}

int	broadcast_drv_if_close(void)
{
	int rc = ERROR;	
	int ret = 0;

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	OnAir = 0;
	currentSelectedChannel = -1;
	currentBroadCast = TMM_13SEG;

	ret = Tcc353xApiClose(0);
	Tcc353xI2cClose(0);
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

	TcpalSemaphoreLock(&Tcc353xDrvSem);

	if(OnAir == 0 || udata == NULL) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_set_channel error [!OnAir]\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	TcpalMemset (&tuneOption, 0x00, sizeof(tuneOption));

	/* uhf 1segment */
	currentSelectedChannel = udata->channel;

	if(udata->segment == 13) {
		currentBroadCast = UHF_13SEG;
		tuneOption.rfIfType = TCC353X_ZERO_IF;
		tuneOption.segmentType = TCC353X_ISDBT_13SEG;
	} else {
		currentBroadCast = UHF_1SEG;
		tuneOption.rfIfType = TCC353X_LOW_IF;
		tuneOption.segmentType = TCC353X_ISDBT_1_OF_13SEG;
	}
	
	tuneOption.userFifothr = 0;
	needLockCheck = 1;

	if(udata->channel<13 || udata->channel>62) {
		TcpalPrintErr((I08S *)"[1seg] channel information error\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}
	frequency = frequencyTable[udata->channel-13];

	if(needLockCheck && udata->mode == 1)	/* Scan mode & need lock check */
		ret = Tcc353xApiChannelSearch(0, frequency, &tuneOption);
	else				/* normal mode */
		ret = Tcc353xApiChannelSelect(0, frequency, &tuneOption);

	Tcc353xMonitoringApiInit(0, 0);
	CurrentMonitoringTime = 0;

	if(ret!=TCC353X_RETURN_SUCCESS) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return OK;
}

int	broadcast_drv_if_resync(void)
{
	int rc;
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

#if defined (_USE_MONITORING_TIME_GAP_)
Tcc353xStatus_t SignalInfo;
#endif

static void broadcast_drv_if_get_oneseg_sig_info(Tcc353xStatus_t *pst, struct broadcast_dmb_control_info *pInfo)
{
	pInfo->sig_info.info.oneseg_info.lock = pst->status.isdbLock.TMCC;
	pInfo->sig_info.info.oneseg_info.cn = pst->status.snr.currentValue;
	pInfo->sig_info.info.oneseg_info.ber = pst->status.viterbiber[0].currentValue;
	pInfo->sig_info.info.oneseg_info.per = pst->status.tsper[0].avgValue;
	pInfo->sig_info.info.oneseg_info.agc = pst->bbLoopGain;
	pInfo->sig_info.info.oneseg_info.rssi = pst->status.rssi.avgValue/100;
	pInfo->sig_info.info.oneseg_info.ErrTSP = pst->opstat.ARsErrorCnt;
	pInfo->sig_info.info.oneseg_info.TotalTSP = pst->opstat.ARsCnt;

	if(pst->antennaPercent[0]>=80)
		pInfo->sig_info.info.oneseg_info.antenna_level = 3;
	else if(pst->antennaPercent[0]>=60)
		pInfo->sig_info.info.oneseg_info.antenna_level = 3;
	else if(pst->antennaPercent[0]>=40)
		pInfo->sig_info.info.oneseg_info.antenna_level = 2;
	else if(pst->antennaPercent[0]>=20)
		pInfo->sig_info.info.oneseg_info.antenna_level = 1;
	else
		pInfo->sig_info.info.oneseg_info.antenna_level = 0;

#if defined (_USE_ONSEG_SIGINFO_MODIFIED_MODE_)
	/*
	Num : Modulation
	Exp : CR
	mode : GI
	*/
	if((pst->opstat.syncStatus>>8&0x0F)<0x0C) {
		pInfo->sig_info.info.oneseg_info.Num = 0xFF;
		pInfo->sig_info.info.oneseg_info.Exp = 0xFF;
		pInfo->sig_info.info.oneseg_info.mode = 0xFF;
	} else {
		pInfo->sig_info.info.oneseg_info.Num = (pst->opstat.AMod & 0x03);
		pInfo->sig_info.info.oneseg_info.Exp = (pst->opstat.ACr & 0x07);

		if(pst->opstat.gi==0)
			pInfo->sig_info.info.oneseg_info.mode = 3; /* GI - 1/4 */
		else if(pst->opstat.gi==1)
			pInfo->sig_info.info.oneseg_info.mode = 2; /* GI - 1/8 */
		else if(pst->opstat.gi==2)
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
}

int	broadcast_drv_if_get_sig_info(struct broadcast_dmb_control_info *pInfo)
{	
	int ret = 0;
	Tcc353xStatus_t st;
	int layer;
#if defined (_USE_MONITORING_TIME_GAP_)
	TcpalTime_t tempTime = 0;
	unsigned int timeGap = 0;
#endif

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if(OnAir == 0 || pInfo==NULL) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

#if defined (_USE_MONITORING_TIME_GAP_)
	tempTime = TcpalGetCurrentTimeCount_ms();
	timeGap = (unsigned int)(TcpalGetTimeIntervalCount_ms(CurrentMonitoringTime));

	if(timeGap > _MON_TIME_INTERVAL_) {
		/*
		TcpalPrintStatus((I08S *)"[TCC353X] Monitoring Function Access\n");
		*/
		CurrentMonitoringTime = tempTime;
		ret =Tcc353xMonitoringApiGetStatus(0, 0, &SignalInfo);
		if(ret != TCC353X_RETURN_SUCCESS) {
			TcpalSemaphoreUnLock(&Tcc353xDrvSem);
			return ERROR;
		}
		Tcc353xMonitoringApiAntennaPercentage (0, &SignalInfo, sizeof(Tcc353xStatus_t));
	}
	ret = 0;
	TcpalMemcpy(&st, &SignalInfo, sizeof(Tcc353xStatus_t));
#else
	ret =Tcc353xMonitoringApiGetStatus(0, 0, &st);
	if(ret != TCC353X_RETURN_SUCCESS) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}
	Tcc353xMonitoringApiAntennaPercentage (0, &st, sizeof(Tcc353xStatus_t));
#endif
	layer = pInfo->cmd_info.layer;

	switch(pInfo->cmd_info.cmd)
	{
	case ENUM_GET_ALL:
		pInfo->sig_info.info.mmb_info.cn = 
				st.status.snr.currentValue;

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

	case ENUM_GET_BER:
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

	case ENUM_GET_PER:
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

	case ENUM_GET_CN:
		pInfo->sig_info.info.mmb_info.cn = 
			st.status.snr.currentValue;
		TcpalPrintStatus((I08S *)"[mmbi][monitor] cn[%d]\n",
				pInfo->sig_info.info.mmb_info.cn);
	break;

	case ENUM_GET_CN_PER_LAYER:
		pInfo->sig_info.info.mmb_info.cn = 
			st.status.snr.currentValue;
		TcpalPrintStatus((I08S *)"[mmbi][monitor] cn[%d]\n",
				pInfo->sig_info.info.mmb_info.cn);
	break;

	case ENUM_GET_LAYER_INFO:
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

	case ENUM_GET_RECEIVE_STATUS:
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

	case ENUM_GET_RSSI:
		pInfo->sig_info.info.mmb_info.rssi = 
			(st.status.rssi.avgValue);
		TcpalPrintStatus((I08S *)"[mmbi][monitor]  rssi[%d]\n",
				pInfo->sig_info.info.mmb_info.rssi);
	break;

	case ENUM_GET_SCAN_STATUS:
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

	case ENUM_GET_SYS_INFO:
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

	case ENUM_GET_TMCC_INFO:
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

	case ENUM_GET_ONESEG_SIG_INFO:
		broadcast_drv_if_get_oneseg_sig_info(&st, pInfo);
	break;

	default:
		TcpalPrintStatus((I08S *)"[mmbi][monitor] sig_info unknown command[%d]\n",
				pInfo->cmd_info.cmd);
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return -1;
	break;
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

	pdmb_data->copied_size = 0;
	pdmb_data->packet_cnt = 0;
	return OK;
}

int	broadcast_drv_if_reset_ch(void)
{
	int ret = 0;
	int rc = ERROR;

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_reset_ch error [!OnAir]\n");
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return ERROR;
	}

	TcpalPrintStatus((I08S *)"[1seg]broadcast_drv_if_reset_ch\n");
	ret = Tcc353xApiStreamStop(0);

	if (ret!=TCC353X_RETURN_SUCCESS)
		rc = ERROR;
	else
		rc = OK;

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	return rc;
}

int	broadcast_drv_if_user_stop(int mode)
{
	int rc;
	rc = OK;
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg][no semaphore] broadcast_drv_if_user_stop error [!OnAir]\n");
		return ERROR;
	}

	TcpalPrintStatus((I08S *)"[1seg][no semaphore] broadcast_drv_if_user_stop\n");
	Tcc353xApiUserLoopStopCmd(0);
	return rc;
}

int	broadcast_drv_if_select_antenna(unsigned int sel)
{
	int rc;
	rc = OK;
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg][no semaphore] broadcast_drv_if_select_antenna error [!OnAir]\n");
		return ERROR;
	}
	return rc;
}

int	broadcast_drv_if_isr(void)
{
	int rc;
	rc = OK;
	if(OnAir == 0) {
		TcpalPrintErr((I08S *)"[1seg] broadcast_drv_if_isr error [!OnAir]\n");
		return ERROR;
	}
	return rc;
}

int	broadcast_drv_if_read_control(char *buf, unsigned int size)
{
	return 0;
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
		} else if (currentBroadCast == UHF_13SEG) {
			band = 0;
			rcvSegment = 0;
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
/*                                                                          */
/*--------------------------------------------------------------------------*/


/* optional part when we include driver code to build-on
it's just used when we make device driver to module(.ko)
so it doesn't work in build-on */
MODULE_DESCRIPTION("TCC3530 ISDB-Tmm device driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TCC");
