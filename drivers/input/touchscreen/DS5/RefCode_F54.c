// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Copyright ?2012 Synaptics Incorporated. All rights reserved.
//
// The information in this file is confidential under the terms
// of a non-disclosure agreement with Synaptics and is provided
// AS IS.
//
// The information in this file shall remain the exclusive property
// of Synaptics and may be the subject of Synaptics?patents, in
// whole or part. Synaptics?intellectual property rights in the
// information in this file are not expressly or implicitly licensed
// or otherwise transferred to you as a result of such information
// being made available to you.
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FullRawCapacitance Support 0D button
//
#include "RefCode_F54.h"
#include "TestLimits.h"

#if 0
#ifdef _DEBUG
#define DEBUG_printk(format, ...) printk(format, __VA_ARGS__)
#else
#define DEBUG_printk(format, ...)
#endif

#ifdef INTERACTIVE_SHELL
#define EXIT(x) exit(x)
#else
void EXIT(int x)
{
  fprintk(stderr, "Press any key to exit.\n");
  _getch();
  exit(x);
}
#endif
#endif

#define TRX_max 32
#define TRX_mapping_max 54
#define LOWER_ABS_ADC_RANGE_LIMIT 60
#define UPPER_ABS_ADC_RANGE_LIMIT 190
#define LOWER_ABS_RAW_CAP_LIMIT 10000 //fF
#define UPPER_ABS_RAW_CAP_LIMIT 40000 //fF
#define REPORT_DATA_OFFEST 3
#define VERSION "1.0"

#if 0
void fatal(const char *format, ...)
{
  va_list ap;

  DEBUG_printk("Error in %s on line %d\n", __FILE__, __LINE__);

  va_start(ap, format);
  vfprintk(stderr, format, ap);
  va_end(ap);

  EXIT(EXIT_FAILURE);
}
#endif

unsigned int count;
const unsigned short DefaultTarget = 0;
const int DefaultTimeout = 10; // In counts

int pageNum = 0;
int scanMaxPageCount = 5;
//CCdciApi cdci;
int input;

bool bHaveF01 = false;
bool bHaveF11 = false;
bool bHaveF1A = false;
bool bHaveF12 = false;
bool bHaveF34 = false;
bool bHaveF54 = false;
bool bHaveF55 = false;
bool SignalClarityOn = false;
bool bHaveF54Ctrl07 = false;
bool bHaveF54Ctrl41 = false;
bool bHaveF54Ctrl57 = false;
bool bHavePixelTouchThresholdTuning = false;
bool bHaveInterferenceMetric = false;
bool bHaveCtrl11 = false;
bool bHaveRelaxationControl = false;
bool bHaveSensorAssignment = false;
bool bHaveSenseFrequencyControl = false;
bool bHaveFirmwareNoiseMitigation = false;
bool bHaveIIRFilter = false;
bool bHaveCmnRemoval = false;
bool bHaveCmnMaximum = false;
bool bHaveTouchHysteresis = false;
bool bHaveEdgeCompensation = false;
bool bHavePerFrequencyNoiseControl = false;
bool bHaveSignalClarity = false;
bool bHaveMultiMetricStateMachine = false;
bool bHaveVarianceMetric = false;
bool bHave0DRelaxationControl = false;
bool bHave0DAcquisitionControl = false;
bool bHaveSlewMetric = false;
bool bHaveHBlank = false;
bool bHaveVBlank = false;
bool bHaveLongHBlank = false;
bool bHaveNoiseMitigation2 = false;
bool bHaveSlewOption = false;
bool bHaveEnhancedStretch = false;
bool bHaveStartupFastRelaxation = false;
bool bHaveESDControl = false;
bool bHaveEnergyRatioRelaxation = false;
bool bHaveCtrl86 = false;
bool bHaveCtrl87 = false;
bool bHaveCtrl88 = false;
bool ButtonShared = false;

unsigned char F54DataBase;
unsigned char F54QueryBase;
unsigned char F54CommandBase;
unsigned char F54ControlBase;
unsigned char F55QueryBase;
unsigned char F55ControlBase;
unsigned char F01ControlBase;
unsigned char F01CommandBase;
unsigned char RxChannelCount;
unsigned char TxChannelCount;
unsigned char TouchControllerFamily;
unsigned char CurveCompensationMode;
unsigned char NumberOfSensingFrequencies;
unsigned char F54Ctrl07Offset;
unsigned char F54Ctrl41Offset;
unsigned char F54Ctrl57Offset;
unsigned char F54Ctrl88Offset;
unsigned char F1AControlBase;
unsigned char F12ControlBase;
unsigned char F12QueryBase;
unsigned char F12_2DTxCount;
unsigned char F12_2DRxCount;
unsigned char ButtonTx[8];
unsigned char ButtonRx[8];
unsigned char ButtonCount;
unsigned char F12Support;
unsigned char F12ControlRegisterPresence;
unsigned char mask;

// Assuming Tx = 32 & Rx = 32 to accommodate any configuration
short Image1[TRX_max][TRX_max];
int ImagepF[TRX_max][TRX_max];
int AbsSigned32Data[TRX_mapping_max];
unsigned char AbsADCRangeData[TRX_mapping_max];
unsigned char Data[TRX_max * TRX_max *4];
unsigned char TRxPhysical[TRX_mapping_max] ;

int MaxArrayLength;

unsigned char TREX_mapped[7] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3f};
unsigned char TRX_Open[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00} ;
unsigned char TRX_Gnd[7] = {0xff,0xff,0xff,0xff,0x3,0xff,0xfc} ;
unsigned char TRX_Short[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00} ;
int HighResistanceLowerLimit[3] = {-1000, -1000, -400};
int HighResistanceUpperLimit[3] = {450, 450, 200};


enum {
	STARTTIME,
	ENDTIME,
	TIME_PROFILE_MAX
};

#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b
struct timeval t_interval[TIME_PROFILE_MAX];
static int outbuf = 0;
char f54_wlog_buf[6000] = {0};


#if 0
// Read one or more sensor registers. The number of registers to read (starting
// at address regAddr) is supplied in length, and the data is returned in values[].
void Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
  unsigned int lengthRead;

  cdci.ReadRegister(DefaultTarget, (unsigned short) I2C_SLAVE_ADDRESS,
    (unsigned short)regAddr, data, length, lengthRead, DefaultTimeout);
}

// Write to one or more sensor registers. The number of registers to write (starting
// at address regAddr) is supplied in length, and the data is supplied in values[].
void Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
  unsigned int lengthWritten;

  cdci.WriteRegister(DefaultTarget, (unsigned short) I2C_SLAVE_ADDRESS,
	  (unsigned short)regAddr, data, length, lengthWritten, DefaultTimeout);
}
#endif

// Function to switch beteen register pages.
bool switchPage(int page) {
  unsigned char values[1];
  unsigned char data;

  pageNum = values[0] = page;

	count = 0;
	do {
		Write8BitRegisters(0xFF, values, 1);
		msleep(1);
		Read8BitRegisters(0xFF, &data, 1);
		count++;
	} while ((int)data != page && (count < DefaultTimeout));
	if(count >= DefaultTimeout){
		printk("Timeout -- Page switch fail ! \n");
		return false;
	}
	return true;
}

#if 0
void LoadTestLimits()
{
	xlsWorkBook* pWB;
	xlsWorkSheet* pWS;
	int SheetCount, r, c, i, j, temp;
	struct st_row::st_row_data* row ;
	int debug_externel_limits = 1;
	int index, offset;

	pWB=xls_open("..\\Function54RefCode\\TestLimits.xls","UTF-8");
	if (pWB!=NULL){

		for (i = 0 ; i<pWB->sheets.count ; i++) {

			if (debug_externel_limits){
				printf("Sheet N%i (%s) pos %i\n",	i,	pWB->sheets.sheet[i].name,
				pWB->sheets.sheet[i].filepos);
			}
			for (j=0; j < pWB->sheets.count; j++) {
				if (!strcmp(pWB->sheets.sheet[i].name, SheetName[j])) {
					//printf("Start to parse limits%s(%d)\n",	SheetName[j], j);

					pWS=xls_getWorkSheet(pWB,i);
			        xls_parseWorkSheet(pWS);

					switch (j) {
					case 0: //"FullRawCapacitance"
						for (r = 0; r < TxChannelCount; r++) {
								for (c = 0; c < RxChannelCount; c++) {
									row = &pWS->rows.row[r+1];
									if (debug_externel_limits) {
										//printf("FullRawCapacitance L [%d][%d]%f\n",r,c, pWS->rows.row[r+1].cells.cell[2*c+1].d);
										//printf("fullrawcapacitance l %f\n",	(&row->cells.cell[2*c+1])->d);
										//printf("fullrawcapacitance h %f\n", (&row->cells.cell[2*c+2])->d);
									}
									LowerImageLimit[r][c] = (&row->cells.cell[2*c+1])->d;
									UpperImageLimit[r][c] = (&row->cells.cell[2*c+2])->d;
							}
						}
						break;

					case 1: //AdcRange
						for (r = 0; r < TxChannelCount; r++) {
							for (c = 0; c < RxChannelCount; c++) {
								row = &pWS->rows.row[r+1];
								ADCLowerImageLimit[r][c] = (&row->cells.cell[2*c+1])->d;
								ADCUpperImageLimit[r][c] = (&row->cells.cell[2*c+2])->d;
							}
						}
						break;

					case 2: //SensorSpeed
						for (r = 0; r < TxChannelCount; r++) {
							for (c = 0; c < RxChannelCount; c++) {
								row = &pWS->rows.row[r+1];
								SensorSpeedLowerImageLimit[r][c] = (&row->cells.cell[2*c+1])->d;
								SensorSpeedUpperImageLimit[r][c] = (&row->cells.cell[2*c+2])->d;
							}
						}
						break;
					case 3: //TRxOpen
						row = &pWS->rows.row[1];

						for(r = 0; r < TRX_mapping_max ; r++){
							temp = TRxPhysical[r];
							if ((TRxPhysical[r] != 0xFF) && ((&row->cells.cell[temp + 1])->d > 0)){
									index = temp / 8;
									offset = 7- (temp % 8);
									TRX_Open[index] = TRX_Open[index] + pow(2.0,offset);

							}
						}

						break;
					case 4: //TRxGround
						row = &pWS->rows.row[1];
						for(r = 0; r < TRX_mapping_max ; r++){
							temp = TRxPhysical[r];
							if ((TRxPhysical[r] != 0xFF) && ((&row->cells.cell[temp + 1])->d > 0)){
									index = temp / 8;
									offset = 7- (temp % 8);
									TRX_Gnd[index] = TRX_Gnd[index] + pow(2.0,offset);
									//printf("trx, %d, row , %3.0f \n",TRxPhysical[r] , (&row->cells.cell[temp + 1])->d );
									//printf("TRX_G1[%d]\n", TRX_Gnd[2]);
							}
						}
						break;
					case 5: //TRxToTRxShort
						row = &pWS->rows.row[1];
						for(r = 0; r < TRX_mapping_max ; r++){
							temp = TRxPhysical[r];
							if ((TRxPhysical[r] != 0xFF) && ((&row->cells.cell[temp + 1])->d > 0)){
									index = temp / 8;
									offset = 7- (temp % 8);
									TRX_Short[index] = TRX_Short[index] + pow(2.0,offset);

							}
						}
						break;
					case 6:	//"HighResistance"
						r = 1;
						for (c = 0; c < 3 ; c++) {
							row=&pWS->rows.row[r];
							HighResistanceLowerLimit[c] = (float)(&row->cells.cell[2*c+1])->d;
							HighResistanceUpperLimit[c] = (float)(&row->cells.cell[2*c+2])->d;
						}
						break;
						case 7: //MaxMinTest
							break;
						case 8://AbsADCRange
							break;
						case 9: //AbsDelta
							break;
						case 10: //AbsRaw
							break;
					default:
						printf("[%d] Invalid value!\n", i);
						break;
					}
			    }
		    }
		}
	} else {
		//Load TRX_Gnd test limit according to Physical TRx mapping.

		for (int k = 0; k < TRX_mapping_max; k++){
			temp = TRxPhysical[k];
			if ( temp != 0xFF){
				index = temp / 8;
				offset = 7- (temp % 8);
				TRX_Gnd[index] = TRX_Gnd[index] + pow(2.0,offset);
				//printf (" TRxPhysical[%d],Index [%d], offset [%d], TRX_Gnd[%d], [%3.0f]\n", TRxPhysical[k],index,offset, TRX_Gnd[index], pow(2.0,offset) );
			}
		}
		printf("Open file failed, use default Test Limit!\n");
	}

	return;
}
#endif

void Reset(void)
{
	unsigned char data;

	switchPage(0x00);

	data = 0x01;
	Write8BitRegisters(F01CommandBase, &data, 1);

    msleep(10);
}

// Compare Report type #20 data against test limits
int CompareImageReport(void)
{
	bool result = true;
	int i,j,node_crack_count=0,rx_crack_count=0, row_crack_count = 0;

	//Compare 0D area
	if (ButtonCount > 0){
		for(i = 0; i < ButtonCount; i++){
			if ((ImagepF[TxChannelCount-1][F12_2DRxCount + i] < LowerImageLimit[TxChannelCount-1][F12_2DRxCount + i]) ||
				(ImagepF[TxChannelCount-1][F12_2DRxCount + i] > UpperImageLimit[TxChannelCount-1][F12_2DRxCount + i]))	{
				//printf("Failed: Button area: TxChannel [%d] RxChannel[%d]\n",TxChannelCount-1, F12_2DRxCount + i);
				result = false;
				break;
			}
		}

	}
	//Compare 2D area
	for (j = 0; j < (int)F12_2DRxCount; j++){
		extern int f54_window_crack;
		extern int f54_window_crack_check_mode;

		rx_crack_count = 0;

		for (i = 0; i < (int)F12_2DTxCount; i++){
			if ((ImagepF[i][j] < LowerImageLimit[i][j]) || (ImagepF[i][j] > UpperImageLimit[i][j]))	{
				if(f54_window_crack_check_mode) {
					if (ImagepF[i][j] < 300){
						rx_crack_count++;
						node_crack_count++;
					}
					else row_crack_count = 0;

					if (F12_2DTxCount<=rx_crack_count) row_crack_count++;

					if (2<row_crack_count){
						f54_window_crack = 1;
						break;
					}

					if((int)(F12_2DTxCount*F12_2DRxCount*20/100)<node_crack_count) {
						result = false;
						f54_window_crack = 1;
						break;
					}

					printk("[Touch] Tx [%d] Rx [%d] node_crack_count %d, row_crack_count %d, raw cap %d\n",i, j,node_crack_count,row_crack_count, ImagepF[i][j]);
				}
				else {
					//printf("Failed: 2D area: Tx [%d] Rx [%d]\n",i, j);
					outbuf += sprintf(f54_wlog_buf+outbuf, "FAIL, %d,%d,%d\n", i, j, ImagepF[i][j]);
					result = false;
					break;
				}
			}
		}
	}

	if (result)
		outbuf += sprintf(f54_wlog_buf+outbuf, "\nFull Raw Capacitance Image Test passed.\n\n");
	else outbuf += sprintf(f54_wlog_buf+outbuf, "\nFull Raw Capacitance Image Test failed.\n\n");

	return (result)?1:0;
}

// Compare Report type #4 data against test limits
int CompareHighResistance(int maxRxpF, int maxTxpF, int minpF )
{
	bool result = true;

	if ( maxRxpF > HighResistanceUpperLimit[0] || maxRxpF < HighResistanceLowerLimit[0])
		result = false;
	if ( maxTxpF > HighResistanceUpperLimit[1] || maxTxpF < HighResistanceLowerLimit[1])
		result = false;
	if ( minpF > HighResistanceUpperLimit[2] || minpF < HighResistanceLowerLimit[2])
		result = false;

	if (result == false)
		printk ("HighResistance Test failed.\n");
	else
		printk("HighResistance Test passed.\n");

	return (result)?1:0;
}


// Compare Report type #22 data against test limits
void CompareSensorSpeedReport(void)
{
	bool result = true;
	int i,j = 0;

	for (i = 0; i < (int)F12_2DTxCount; i++){
		for (j = 0; j < (int)F12_2DRxCount; j++){
			if ((ImagepF[i][j] < SensorSpeedLowerImageLimit[i][j]) || (ImagepF[i][j] > SensorSpeedUpperImageLimit[i][j])){
				result = false;
				printk("Failed : Tx[%d] Rx[%d]\n",i ,j );
				break;
			}
		}
	}

	if (result)
		printk("\nSensor Speed Test passed.\n");
	else printk("\nSensor Speed Test failed.\n");
}

// Compare Report type #23 data against test limits
void CompareADCReport(void)
{
	bool result = true;
	int i,j = 0;

	for (i = 0; i < (int)F12_2DTxCount; i++){
		for (j = 0; j < (int)F12_2DRxCount; j++){
			if ((Image1[i][j] < ADCLowerImageLimit[i][j]) || (Image1[i][j] > ADCUpperImageLimit[i][j])){
				printk("Failed : Tx[%d] Rx[%d].\n",i ,j);
				result = false;
				break;
			}
		}
	}

	if (result)
		printk("\nADC Range Test passed.\n");
	else printk("\nADC Range Test failed.\n");
}

void CompareAbsADCRangeReport(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < RxChannelCount + F12_2DTxCount; i++) {
		if (i == F12_2DRxCount ){
			i = RxChannelCount;
		}
		if ((AbsADCRangeData[i] < LOWER_ABS_ADC_RANGE_LIMIT) || (AbsADCRangeData[i] > UPPER_ABS_ADC_RANGE_LIMIT)) {
			result = false;
			break;
		}
	}

	if (result)
		printk("\nAbs Sensing ADC Range Test Passed.\n");
	else
		printk("\nAbs Sensing ADC Range Test Failed.\n");
}

void CompareAbsRawReport(void)
{
	bool result = true;
	int i = 0;

	for (i = 0; i < RxChannelCount + F12_2DTxCount; i++) {
		if (i == F12_2DRxCount ){
			i = RxChannelCount;
		}
		if ((AbsSigned32Data[i] < LOWER_ABS_RAW_CAP_LIMIT) || (AbsSigned32Data[i] > UPPER_ABS_RAW_CAP_LIMIT)) {
			result = false;
			break;
		}
	}

	if (result)
		printk("\nAbs Sensing Raw Capacitance Test Passed.\n");
	else
		printk("\nAbs Sensing Raw Capacitance Test Failed.\n");
}

// Compare Report type #24 data against test limits
void CompareTRexOpenTestReport(int i)
{
	int index;
	for(index = 0; index < 7; index++){
		if(Data[index] != TRX_Open[index]){
			printk("\nTRex Open Test failed.\n");
			return;
		}
	}

	printk("\nTRex Open Test passed.\n");
}

// Compare Report type #25 data against test limits
int CompareTRexGroundTestReport(int i)
{
	int index;//, offset, temp;

	for(index = 0; index < 7; index++){
		if(Data[index] !=  TRX_Gnd[index]){
			outbuf += sprintf(f54_wlog_buf+outbuf, "\nTRex Ground Test failed.\n\n");
		}
	}

	outbuf += sprintf(f54_wlog_buf+outbuf, "\nTRex Ground Test passed.\n\n");
	return 1;
}

// Compare Report type #26 data against test limits
int CompareTRexShortTestReport(int i)
{
	int index;//, offset;
	for(index = 0; index < 7; index++){
		if(Data[index] != TRX_Short[index]){
			outbuf += sprintf(f54_wlog_buf+outbuf, "\nTRex-TRex Short Test failed.\n\n");
			return 0;
			//printk("\nTRex Ground Test failed.\n");
			//return;
		}
	}

	outbuf += sprintf(f54_wlog_buf+outbuf, "\nTRex-TRex Short Test passed.\n\n");

	return 1;
}

// Construct data with Report Type #20 data
int ReadImageReport(void)
{
	int ret = 0;
	int i,j,k = 0;

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	outbuf += sprintf(f54_wlog_buf+outbuf, "Image Data : \n");
	for (i = 0; i < (int)TxChannelCount; i++){
		outbuf += sprintf(f54_wlog_buf+outbuf, "Tx[%2d]: ", i);
		for (j = 0; j < (int)RxChannelCount; j++){
			Image1[i][j] = ((short)Data[k] | (short)Data[k+1] << 8);
			ImagepF[i][j] = Image1[i][j];
			outbuf += sprintf(f54_wlog_buf+outbuf, "%4d,", ImagepF[i][j]);
			k = k + 2;
		}
		outbuf += sprintf(f54_wlog_buf+outbuf, "\n");
	}

	ret = CompareImageReport();
	write_log(f54_wlog_buf);
  //Reset Device
  Reset();

  return ret;
}

// Construct data with Report Type #4 data
int ReadHighResistanceReport(void)
{
	short maxRx, maxTx, min;
	int maxRxpF, maxTxpF, minpF;
	int ret = 0;

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, 6);

	maxRx = ((short)Data[0] | (short)Data[1] << 8);
	maxTx = ((short)Data[2] | (short)Data[3] << 8);
	min = ((short)Data[4] | (short)Data[5] << 8);
  maxRxpF = maxRx;
  maxTxpF = maxTx;
  minpF = min;

	outbuf += sprintf(f54_wlog_buf+outbuf, "\nHigh Resistance Test:\n");
	outbuf += sprintf(f54_wlog_buf+outbuf, "Max Rx Offset(pF) = %d\n", maxRxpF);
	outbuf += sprintf(f54_wlog_buf+outbuf, "Max Tx Offset(pF) = %d\n", maxTxpF);
	outbuf += sprintf(f54_wlog_buf+outbuf, "Min(pF) = %d\n", minpF);
	write_log(f54_wlog_buf);

	ret = CompareHighResistance(maxRxpF, maxTxpF, minpF);
  //Reset Device
  Reset();

	return ret;
}

// Construct data with Report Type #13 data
void ReadMaxMinReport(void)
{
	short max, min;
	int maxpF, minpF;

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, 4);

	max = ((short)Data[0] | (short)Data[1] << 8);
	min = ((short)Data[2] | (short)Data[3] << 8);
  maxpF = max;
  minpF = min;

	printk("\nRaw Capacitance Maximum and Minimum Test:\n");
	/*printk("Max = 0x%x\n", max);
	printk("Min = 0x%x\n", min);*/
  printk("Max(pF) = %d\n", maxpF);
	printk("Min(pF) = %d\n", minpF);

  //Reset Device
  Reset();
}

// Construct data with Report Type #23 data
void ReadADCRangeReport(void)
{
	int temp = TxChannelCount;
	int i, j, k = 0;

	if (SignalClarityOn){
		if ((TxChannelCount / 4) != 0)	{
			temp = (4 - (TxChannelCount % 4)) +  TxChannelCount;
		}
	}

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, (temp*RxChannelCount*2));

	k = 0;
	printk("ADC Data: \n");
	for (i = 0; i < (int)TxChannelCount; i++){
		printk("Tx[%d]: ", i);
		for (j = 0; j < (int)RxChannelCount; j++){
			//Image1[i][j] = ((unsigned short)Data[k]  | (unsigned short)Data[k+1] << 8);
			Image1[i][j] = ((unsigned short)Data[k]);
			//ImagepF[i][j] = Image1[i][j]/1000.0;
			printk("%u,", Image1[i][j]);
			k = k + 2;
		}
	printk("\n");
	}

	CompareADCReport();

	//Reset Device
	Reset();
}

void ReadAbsADCRangeReport(void)
{
	int i, k = 0;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], 2 * (RxChannelCount + TxChannelCount));

	printk("Abs Sensing ADC Range Data:\n");
	printk("Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsADCRangeData[k / 2] = (unsigned char)Data[k];
		printk("%d ", AbsADCRangeData[k / 2]);
		k = k + 2;
	}
	printk("\n");
	printk("Tx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsADCRangeData[k / 2] = (unsigned char)Data[k];
		printk("%d ", AbsADCRangeData[k / 2]);
		k = k + 2;
	}
	printk("\n");

	CompareAbsADCRangeReport();

	Reset();
}

void ReadAbsDeltaReport(void)
{
	int i, k = 0;
	int *p32data;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], 4 * (RxChannelCount + TxChannelCount));

	p32data = (int *)&Data[0];

	printk("Abs Sensing Delta Capacitance Data:\n");
	printk("Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		printk("%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	printk("\n");
	printk("Tx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		printk("%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	printk("\n");

	Reset();
}

void ReadAbsRawReport(void)
{
	int i, k = 0;
	int *p32data;

	Read8BitRegisters((F54DataBase + REPORT_DATA_OFFEST), &Data[0], 4 * (RxChannelCount + TxChannelCount));

	p32data = (int *)&Data[0];

	printk("Abs Sensing Raw Capacitance Data:\n");
	printk("Rx: ");
	for (i = 0; i < (int)RxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		printk("%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	printk("\n");
	printk("Tx: ");
	for (i = 0; i < (int)TxChannelCount; i++) {
		AbsSigned32Data[k] = (int)*p32data;
		printk("%d ", AbsSigned32Data[k]);
		k++;
		p32data++;
	}
	printk("\n");

	CompareAbsRawReport();

	Reset();
}

// Construct data with Report Type #22 data
void ReadSensorSpeedReport(void)
{
	int i,j,k = 0;

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), &Data[0], MaxArrayLength);

	printk("Sensor speed Test Data : \n");
	for (i = 0; i < (int)TxChannelCount; i++)
	{
    printk("Tx[%d]: ", i);
		for (j = 0; j < (int)RxChannelCount; j++)
		{
			Image1[i][j] = ((short)Data[k] | (short)Data[k+1] << 8);
      ImagepF[i][j] = Image1[i][j];
      printk("%d,", ImagepF[i][j]);
			k = k + 2;
		}
    printk("\n");
	}

	CompareSensorSpeedReport();

  //Reset Device
  Reset();
}

int pow_func(int x, int y)
{
	int result = 1;
	int i = 0;
	for(i = 0; i < y; i++)
		result *= x;
	return result;
}

// Construct data with Report Type #24 data
void ReadTRexOpenReport(void)
{
	int i, j = 0;
	//int k = ((int)TxChannelCount) / 8 + 1;
	int k = 7, mask = 0x01, value;// Hardcode for Waikiki Test and it support up to 54 Tx

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k ; i++){
		value = Data[i];
		Data[i] = 0;
		for (j = 0; j < 8; j++){
		if((value & mask) == 1) {
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));
			}
			value >>= 1;
		}
		printk("TRex-Open Test Data = %#x,", Data[i]);
	}
  printk("\n");

	CompareTRexOpenTestReport(k * 8);

  //Reset Device
  Reset();
}

// Construct data with Report Type #25 data
int ReadTRexGroundReport(void)
{
	int ret = 0;
	int i,j = 0;
	//int k = ((int)TxChannelCount) / 8 + 1;
	int k = 7, mask = 0x01, value;// Hardcode for Waikiki Test and it support up to 54 Tx
	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k; i++){
		value = Data[i];
		Data[i] = 0;
		for (j = 0; j < 8; j++){
			if((value & mask) == 1) {
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));
			}
			value >>= 1;
		}

		outbuf += sprintf(f54_wlog_buf+outbuf, "TRex-Ground Test Data = %#x\n", Data[i]);
	}
  outbuf += sprintf(f54_wlog_buf+outbuf, "\n");

	ret = CompareTRexGroundTestReport(k * 8);
	write_log(f54_wlog_buf);
  //Reset Device
  Reset();

  return ret;
}

// Construct data with Report Type #26 data
int ReadTRexShortReport(void)
{
	int ret = 0;
	int i, j = 0;
	//int k = ((int)TxChannelCount) / 8 + 1;
	int k = 7, mask = 0x01, value;// Hardcode for Waikiki Test and it support up to 54 Tx

	Read8BitRegisters((F54DataBase+REPORT_DATA_OFFEST), Data, k);

	for (i = 0; i < k; i++)	{
		value = Data[i];
		Data[i] = 0;
		for (j = 0; j < 8; j++){
			if((value & mask) == 1) {
				Data[i] = Data[i] + (unsigned char)pow_func(2, (7 - j));
			}
			value >>= 1;
		}
		outbuf += sprintf(f54_wlog_buf+outbuf, "TRex-TRex Short Test Data = %#x\n", Data[i]);
	}

	outbuf += sprintf(f54_wlog_buf+outbuf, "\n");

	ret = CompareTRexShortTestReport(k * 8);
	write_log(f54_wlog_buf);

  //Reset Device
  Reset();

	return ret;
}
// Function to handle report reads based on user input
int ReadReport(unsigned char input)
{
	int ret = 0;
  unsigned char data;

	data = 0x01;
	Write8BitRegisters(F54CommandBase, &data, 1);

	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		msleep(1);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));
	if(count >= DefaultTimeout) {
	outbuf += sprintf(f54_wlog_buf+outbuf, "Timeout -- Not supported Report Type in FW\n");
		Reset();
		return ret;
	}

	do_gettimeofday(&t_interval[ENDTIME]);

	printk("Takes %lu ticks\n", get_time_interval(t_interval[ENDTIME].tv_sec,t_interval[STARTTIME].tv_sec));

	switch (input){
		case 'a':
			ret = ReadImageReport();
			break;
		case 'b':
			ReadADCRangeReport();
			break;
		case 'c':
			ReadSensorSpeedReport();
			break;
		case 'd':
//			fprintk(stderr, "Press any key to continue after you have lowered the bar.\n");
//			_getch();
			ReadTRexOpenReport();
			break;
		case 'e':
			ret = ReadTRexGroundReport();
			break;
		case 'f':
			ret = ReadTRexShortReport();
			break;
		case 'g':
			ret = ReadHighResistanceReport();
			break;
		case 'h':
			ReadMaxMinReport();
			break;
		case 'i':
			ReadAbsADCRangeReport();
			break;
		case 'j':
			ReadAbsDeltaReport();
			break;
		case 'k':
			ReadAbsRawReport();
			break;
		default:
			break;
	}

	return ret;
}

#if 0
// Power up the sensor, configure the Control Bridge protocol converter to use the appropriate protocol.
EError PowerOnSensor()
{
	EError nRet;
	nRet = cdci.Connect();
	nRet = cdci.PowerOn((unsigned short)0, SENSOR_VDD_SUPPLY_MV, SENSOR_SUPPLY_MV, SENSOR_SUPPLY_MV, DefaultTimeout);
	if (nRet == 0){
		nRet = cdci.ConfigI2CReg((unsigned short)0, ERmiAddressEightBit,EPullupsYes, ELowSpeed, EAttnLow, DefaultTimeout);
	}
	return nRet;
}

// Remove power from the sensor.
EError PowerOffSensor()
{
  EError nRet;

  nRet = cdci.PowerOff(DefaultTarget, DefaultTimeout);
  cdci.Disconnect();
  return nRet;
}
#endif

// Examples of reading query registers. Real applications often do not need to read query registers at all.
void RunQueries(void)
{
  unsigned short cAddr = 0xEE;
  unsigned char cFunc;
  int rxCount = 0;
  int txCount = 0;
  int offset = 0;
	int i,j = 0;

	// Scan Page Description Table (PDT) to find all RMI functions presented by this device.
	// The Table starts at $00EE. This and every sixth register (decrementing) is a function number
	// except when this "function number" is $00, meaning end of PDT.
	// In an actual use case this scan might be done only once on first run or before compile.
	do{
		Read8BitRegisters(cAddr, &cFunc, 1);
		if (cFunc == 0){
			break;
		}

	switch (cFunc){
		case 0x01:
			if (!bHaveF01){
				Read8BitRegisters((cAddr-3), &F01ControlBase, 1);
				Read8BitRegisters((cAddr-4), &F01CommandBase, 1);
				bHaveF01 = true;
				break;
			}
#if 0
		case 0x1a:
			if(!bHaveF1A){
				k =0;
				Read8BitRegisters((cAddr-3), &F1AControlBase, 1);
				Read8BitRegisters(F1AControlBase+1, &ButtonCount, 1);
				ButtonCount = log((double)ButtonCount+1 )/ log(2.0);

				for (i = 0; i < ButtonCount; i++){
					Read8BitRegisters(F1AControlBase + 3 + k, &ButtonTx[i], 1);
					Read8BitRegisters(F1AControlBase + 3 + k + 1 , &ButtonRx[i], 1);
					k= k +2;
				}
				//printk("Have 1A:%d,%d\n",ButtonTx[1], ButtonRx[1]);
				bHaveF1A = true;
			}
			break;
#endif
		case 0x12:
			if(!bHaveF12){
				Read8BitRegisters((cAddr-3), &F12ControlBase, 1);
				Read8BitRegisters((cAddr-5), &F12QueryBase, 1);
				Read8BitRegisters((F12QueryBase), &F12Support, 1);

				if( (F12Support | 0x00) == 0  ){
					printk("Device not support F12.\n");
					break;
				}
				Read8BitRegisters((F12QueryBase+5), Data, 2);
				mask = 0x01;
				for (j = 0; j < 8; j++){
					if((Data[1] & mask) == 1) offset++;
					Data[1] >>= 1;
				}
				Read8BitRegisters((F12ControlBase + offset), Data, 14);
				F12_2DRxCount = Data[12];
				F12_2DTxCount = Data[13];

				if(TRX_max<=F12_2DRxCount)
					F12_2DRxCount = TRX_max;
				if(TRX_max<=F12_2DTxCount)
					F12_2DTxCount = 16;

				bHaveF12 = true;
				offset = 0;
				break;
			}
		case 0x54:
			if (!bHaveF54){
				Read8BitRegisters((cAddr-2), &F54DataBase, 1);
				Read8BitRegisters((cAddr-3), &F54ControlBase, 1);
				Read8BitRegisters((cAddr-4), &F54CommandBase, 1);
				Read8BitRegisters((cAddr-5), &F54QueryBase, 1);

			    Read8BitRegisters(F54QueryBase, &RxChannelCount, 1);
			    Read8BitRegisters((F54QueryBase+1), &TxChannelCount, 1);

				if(TRX_max<=RxChannelCount)
					RxChannelCount = TRX_max;
				if(TRX_max<=TxChannelCount)
					TxChannelCount = 16;

			    MaxArrayLength = (int)RxChannelCount * (int)TxChannelCount * 2;
			    bHaveF54 = true;

				Read8BitRegisters(F54QueryBase, Data, 15);
				TouchControllerFamily = Data[5];
				offset++; //Ctrl 00
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset++; //Ctrl 01
				offset+=2; //Ctrl 02
				bHavePixelTouchThresholdTuning = ((Data[6] & 0x01) == 0x01);
				if (bHavePixelTouchThresholdTuning) offset++; //Ctrl 03;
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01)
					offset+=3; //Ctrl 04/05/06
				if (TouchControllerFamily == 0x01){
					F54Ctrl07Offset = offset;
					offset++; //Ctrl 07;
					bHaveF54Ctrl07 = true;
				}
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01) offset+=2; //Ctrl 08
				if (TouchControllerFamily == 0x0 || TouchControllerFamily == 0x01) offset++; //Ctrl 09
				bHaveInterferenceMetric = ((Data[7] & 0x02) == 0x02);
				if (bHaveInterferenceMetric) offset++; // Ctrl 10
				bHaveCtrl11 = ((Data[7] & 0x10) == 0x10);
				if (bHaveCtrl11) offset+=2; //Ctrl 11
				bHaveRelaxationControl = ((Data[7] & 0x80) == 0x80);
				if (bHaveRelaxationControl) offset+=2; //Ctrl 12/13
				bHaveSensorAssignment = ((Data[7] & 0x01) == 0x01);
				if (bHaveSensorAssignment) offset++; //Ctrl 14
				if (bHaveSensorAssignment) offset+=RxChannelCount; //Ctrl 15
				if (bHaveSensorAssignment) offset+=TxChannelCount; //Ctrl 16
				bHaveSenseFrequencyControl = ((Data[7] & 0x04) == 0x04);
				NumberOfSensingFrequencies = (Data[13] & 0x0F);
				if (bHaveSenseFrequencyControl) offset+=(3*(int)NumberOfSensingFrequencies); //Ctrl 17/18/19
				offset++; //Ctrl 20
				if (bHaveSenseFrequencyControl) offset+=2; //Ctrl 21
				bHaveFirmwareNoiseMitigation = ((Data[7] & 0x08) == 0x08);
				if (bHaveFirmwareNoiseMitigation) offset++; //Ctrl 22
				if (bHaveFirmwareNoiseMitigation) offset+=2; //Ctrl 23
				if (bHaveFirmwareNoiseMitigation) offset+=2; //Ctrl 24
				if (bHaveFirmwareNoiseMitigation) offset++; //Ctrl 25
				if (bHaveFirmwareNoiseMitigation) offset++; //Ctrl 26
				bHaveIIRFilter = ((Data[9] & 0x02) == 0x02);
				if (bHaveIIRFilter) offset++; //Ctrl 27
				if (bHaveFirmwareNoiseMitigation) offset+=2; //Ctrl 28
				bHaveCmnRemoval = ((Data[9] & 0x04) == 0x04);
				bHaveCmnMaximum = ((Data[9] & 0x08) == 0x08);
				if (bHaveCmnRemoval) offset++; //Ctrl 29
				if (bHaveCmnMaximum) offset++; //Ctrl 30
				bHaveTouchHysteresis = ((Data[9] & 0x10) == 0x10);
				if (bHaveTouchHysteresis) offset++; //Ctrl 31
				bHaveEdgeCompensation = ((Data[9] & 0x20) == 0x20);
				if (bHaveEdgeCompensation) offset+=2; //Ctrl 32
				if (bHaveEdgeCompensation) offset+=2; //Ctrl 33
				if (bHaveEdgeCompensation) offset+=2; //Ctrl 34
				if (bHaveEdgeCompensation) offset+=2; //Ctrl 35
				CurveCompensationMode = (Data[8] & 0x03);
				if (CurveCompensationMode == 0x02) {
					offset += (int)RxChannelCount;
				} else if (CurveCompensationMode == 0x01) {
					offset += ((int)RxChannelCount > (int)TxChannelCount) ? (int)RxChannelCount: (int)TxChannelCount;
				} //Ctrl 36
				if (CurveCompensationMode == 0x02) offset += (int)TxChannelCount; //Ctrl 37
				bHavePerFrequencyNoiseControl = ((Data[9] & 0x40) == 0x40);
				if (bHavePerFrequencyNoiseControl) offset+=(3*(int)NumberOfSensingFrequencies); //Ctrl 38/39/40
				bHaveSignalClarity = ((Data[10] & 0x04) == 0x04);
				if (bHaveSignalClarity){
					F54Ctrl41Offset = offset;
					offset++; //Ctrl 41
					bHaveF54Ctrl41 = true;
				}
				bHaveMultiMetricStateMachine = ((Data[10] & 0x02) == 0x02);
				bHaveVarianceMetric = ((Data[10] & 0x08) == 0x08);
				if (bHaveVarianceMetric) offset+=2; //Ctr 42
				if (bHaveMultiMetricStateMachine) offset+=2; //Ctrl 43
				if (bHaveMultiMetricStateMachine) offset+=11 ; //Ctrl 44/45/46/47/48/49/50/51/52/53/54
				bHave0DRelaxationControl = ((Data[10] & 0x10) == 0x10);
				bHave0DAcquisitionControl = ((Data[10] & 0x20) == 0x20);
				if (bHave0DRelaxationControl) offset+=2; //Ctrl 55/56
				if (bHave0DAcquisitionControl){
					F54Ctrl57Offset = offset;
					offset++; //Ctrl 57;
					bHaveF54Ctrl57 = true;
				}
				if (bHave0DAcquisitionControl) offset += 1; //Ctrl 58
				bHaveSlewMetric = ((Data[10] & 0x80) == 0x80);
				bHaveHBlank = ((Data[11] & 0x01) == 0x01);
				bHaveVBlank = ((Data[11] & 0x02) == 0x02);
				bHaveLongHBlank = ((Data[11] & 0x04) == 0x04);
				bHaveNoiseMitigation2 = ((Data[11] & 0x20) == 0x20);
				bHaveSlewOption = ((Data[12] & 0x02) == 0x02);
				if (bHaveHBlank) offset += 1; //Ctrl 59
				if (bHaveHBlank || bHaveVBlank || bHaveLongHBlank) offset +=3; //Ctrl 60/61/62
				if (bHaveSlewMetric || bHaveHBlank || bHaveVBlank || bHaveLongHBlank || bHaveNoiseMitigation2 || bHaveSlewOption) offset += 1; //Ctrl 63
				if (bHaveHBlank) offset += 28; //Ctrl 64/65/66/67
				else if (bHaveVBlank || bHaveLongHBlank) offset += 4; //Ctrl 64/65/66/67
				if (bHaveHBlank || bHaveVBlank || bHaveLongHBlank) offset += 8; //Ctrl 68/69/70/71/72/73
				if (bHaveSlewMetric) offset += 2; //Ctrl 74
				bHaveEnhancedStretch = ((Data[9] & 0x80) == 0x80);
				if (bHaveEnhancedStretch) offset += (int)NumberOfSensingFrequencies; //Ctrl 75
				bHaveStartupFastRelaxation = ((Data[11] & 0x08) == 0x08);
				if (bHaveStartupFastRelaxation) offset += 1; //Ctrl 76
				bHaveESDControl = ((Data[11] & 0x10) == 0x10);
				if (bHaveESDControl) offset += 2; //Ctrl 77/78
				if (bHaveNoiseMitigation2) offset += 5; //Ctrl 79/80/81/82/83
				bHaveEnergyRatioRelaxation = ((Data[11] & 0x80) == 0x80);
				if (bHaveEnergyRatioRelaxation) offset += 2; //Ctrl 84/85
				bHaveCtrl86 = (((Data[12] & 0x08) == 0x08) && ((Data[13] & 0x01) == 0x01));
				bHaveCtrl87 = (((Data[12] & 0x08) == 0x08) && ((Data[13] & 0x02) == 0x02));
				bHaveCtrl88 = ((Data[12] & 0x40) == 0x40);
				if (bHaveCtrl86) offset += 1; //Ctrl 86
				if (bHaveCtrl87) offset += 1; //Ctrl 87
				if (bHaveCtrl88){
					F54Ctrl88Offset = offset;
					offset++; //Ctrl 88;
				}
				break;
			}
		case 0x55:
			if (!bHaveF55){
				Read8BitRegisters((cAddr-3), &F55ControlBase, 1);
				Read8BitRegisters((cAddr-5), &F55QueryBase, 1);

			    Read8BitRegisters(F55QueryBase, &RxChannelCount, 1);
			    Read8BitRegisters((F55QueryBase+1), &TxChannelCount, 1);

				rxCount = 0;
				txCount = 0;
				//Read Sensor Mapping
				Read8BitRegisters((F55ControlBase+1), Data, (int)RxChannelCount);
				for (i = 0; i < (int)RxChannelCount; i++){
					if (Data[i] != 0xFF){
						rxCount++;
						TRxPhysical[i] = Data[i];
					}
					else
						break;
				}
				Read8BitRegisters((F55ControlBase+2), Data, (int)TxChannelCount);
				for (i = 0; i < (int)TxChannelCount; i++){
					if (Data[i] != 0xFF ){
						TRxPhysical[rxCount + i] = Data[i];
						txCount++;
					}
					else
						break;
				}
				for (i = (rxCount + txCount); i < (TRX_mapping_max); i++){
						TRxPhysical[i] = 0xFF;
				}

				RxChannelCount = rxCount;
				TxChannelCount = txCount;
				if(TRX_max<=RxChannelCount)
					RxChannelCount = TRX_max;
				if(TRX_max<=TxChannelCount)
					TxChannelCount = 16;

				MaxArrayLength = (int)RxChannelCount * (int)TxChannelCount * 2;
				if(((int)TxChannelCount - F12_2DTxCount == 0) && ButtonCount > 0){
					ButtonShared = true;
				}

				bHaveF55 = true;
				break;
			}
			default: // Any other function
				break;
		}
	cAddr -= 6;
	} while (true);
}

// The following function is necessary to setup the Function $54 tests. The setup needs to be done once
// after entering into page 0x01. As long as the touch controller stays in page 1, the setup does not
// need to be repeated.
bool TestPreparation(void)
{
	unsigned char data;
	unsigned char addr;

	//if (!switchPage(0x01))
	//	return false;

	// Turn off CBC.
	if (bHaveF54Ctrl07) {
		addr = F54ControlBase + F54Ctrl07Offset;
		Read8BitRegisters(addr, &data, 1);
		// data = data & 0xEF;
		data = 0;
		Write8BitRegisters(addr, &data, 1);
	}
	else if (bHaveCtrl88){
		addr = F54ControlBase + F54Ctrl88Offset;
		Read8BitRegisters(addr, &data, 1);
		data = data & 0xDF;
		Write8BitRegisters(addr, &data, 1);
	}

	// Turn off 0D CBC.
	if (bHaveF54Ctrl57){
		addr = F54ControlBase + F54Ctrl57Offset;
		Read8BitRegisters(addr, &data, 1);
		//ata = data & 0xEF;
		data = 0;
		Write8BitRegisters(addr, &data, 1);
	}

	// Turn off SignalClarity. ForceUpdate is required for the change to be effective
	if (bHaveF54Ctrl41){
		addr = F54ControlBase + F54Ctrl41Offset;
		Read8BitRegisters(addr, &data, 1);
		data = data | 0x01;
		Write8BitRegisters(addr, &data, 1);
	}

	// Apply ForceUpdate.
  Read8BitRegisters(F54CommandBase, &data, 1);
	data = data | 0x04;
	Write8BitRegisters(F54CommandBase, &data, 1);
	// Wait complete
	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		msleep(1);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));

	if(count >= DefaultTimeout) {
		outbuf += sprintf(f54_wlog_buf+outbuf, "Timeout -- ForceUpdate can not complete\n");
		//printk("Timeout -- ForceUpdate can not complete\n");
		Reset();
		return false;
	}

	// Apply ForceCal.
  Read8BitRegisters(F54CommandBase, &data, 1);
	data = data | 0x02;
  Write8BitRegisters(F54CommandBase, &data, 1);

	// Wait complete
	count = 0;
	do {
		Read8BitRegisters(F54CommandBase, &data, 1);
		msleep(1);
		count++;
	} while (data != 0x00 && (count < DefaultTimeout));

	if(count >= DefaultTimeout){
		outbuf += sprintf(f54_wlog_buf+outbuf, "Timeout -- ForceCal can not complete\n");
		//printk("Timeout -- ForceCal can not complete\n");
		Reset();
		return false;
	}

	return true;
}

// The following funtion illustrates the steps in getting a full raw image report (report #20) by Function $54.
int ImageTest(void)
{
	unsigned char data;

	if (TestPreparation()){
		// Assign report type for Full Raw Image
		data = 20;
		Write8BitRegisters(F54DataBase, &data, 1);

	  do_gettimeofday(&t_interval[STARTTIME]);

		data = 'a';
	return ReadReport(data);
  } else {
	return -1;
  }
}

// The following funtion illustrates the steps in getting a sensor speed test report (report #22) by Function $54.
void SensorSpeed(void)
{
	unsigned char data;

	if (TestPreparation()){

		// Assign report type for Sensor Speed Test
		data = 22;
		Write8BitRegisters(F54DataBase, &data, 1);

	  do_gettimeofday(&t_interval[STARTTIME]);

		data = 'c';
		ReadReport(data);
	}
}

// The following funtion illustrates the steps in getting a ADC Range report (report #23) by Function $54.
void ADCRange(void)
{
	unsigned char data;

	if (TestPreparation()){
		Read8BitRegisters((F54ControlBase + F54Ctrl41Offset), &data, 1);
		if (data & 0x01)
			SignalClarityOn = false;
		else SignalClarityOn = true;

			// Assign report type for ADC Range report
			data = 23;
		Write8BitRegisters(F54DataBase, &data, 1);

		do_gettimeofday(&t_interval[STARTTIME]);
		//startTime = GetTickCount();

		data = 'b';
		ReadReport(data);
	}
}

void AbsADCRange(void)
{
	unsigned char data;

	if (TestPreparation()){
		// Assign report type for Abs Sensing ADC Range report
		data = 42;
		Write8BitRegisters(F54DataBase, &data, 1);

		do_gettimeofday(&t_interval[STARTTIME]);
		//startTime = GetTickCount();

		data = 'i';
		ReadReport(data);
	}
}
// report type 40
void AbsDelta(void)
{
	unsigned char data;

	//--switchPage(0x01);

	// Assign report type for Abs Sensing Delta Capacitance report
	data = 40;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);
	//startTime = GetTickCount();

	data = 'j';
	ReadReport(data);
}
// report type 38
void AbsRaw(void)
{
	unsigned char data;

	//switchPage(0x01);

	data = 38;
	Write8BitRegisters(F54DataBase, &data, 1);

	do_gettimeofday(&t_interval[STARTTIME]);
	//startTime = GetTickCount();

	data = 'k';
	ReadReport(data);
}

// The following funtion illustrates the steps in getting a TRex-Opens(No sensor) report (report #24) by Function $54.
void TRexOpenTest(void)
{
	unsigned char data;

	//fprintf(stderr, "Press any key to continue after you have lowered the bar.\n");
	//_getch();

	if (TestPreparation()){
		// Assign report type for TRex Open Test
		data = 24;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'd';
	  ReadReport(data);
  }
}

// The following funtion illustrates the steps in getting a TRex-to-GND(No sensor) report (report #25) by Function $54.
int TRexGroundTest(void)
{
  unsigned char data;

	if (TestPreparation()){
		// Assign report type for TRex Ground Test
		data = 25;
    Write8BitRegisters(F54DataBase, &data, 1);

		data = 'e';
	  return ReadReport(data);
  } else {
	return -1;
  }
}

// The following funtion illustrates the steps in getting a TRex-TRex short(No sensor) report (report #26) by Function $54.
int TRexShortTest(void)
{
	unsigned char data;

	if (TestPreparation()){
		// Assign report type for TRex Short Test
		data = 26;
		Write8BitRegisters(F54DataBase, &data, 1);
		data = 'f';
		return ReadReport(data);
	} else {
		return -1;
	}
}

// This test is to retreive the high resistance report, report type #4.
int HighResistanceTest(void)
{
	unsigned char data;

	if (TestPreparation()){

		// Assign report type for High Resistance report
		data = 4;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'g';
	  return ReadReport(data);
  } else {
	return -1;
  }
}

// This test is to retreive the maximum and minimum pixel report, report type #13.
void MaxMinTest(void)
{
  unsigned char data;

	if (TestPreparation()){
		// Assign report type for Max Min report
		data = 13;
		Write8BitRegisters(F54DataBase, &data, 1);

		data = 'h';
		ReadReport(data);
	}
}

void SCAN_PDT(void)
{
	int i;

	for (i = 0; i < scanMaxPageCount ;i++)
  {
	if (switchPage(i))
	RunQueries();
  }
}

// Main entry point for the application
int F54Test(int input)
//int _tmain(int argc, _TCHAR* argv[])
{

	int ret = 0;

  //if (PowerOnSensor())
  //{
  //  fatal("Error powering on sensor.\n");
  //}

  // These four function calls are to scan the Page Description Table (PDT)
  // Function $01, $11 and $34 are on page 0
  // Function $54 is on page 1
  // Function $55 is on Page ?
  // Scan up to Page 4
  //for (int i = 0; i < scanMaxPageCount ;i++)
  //{
  //  if (switchPage(i))
  //   RunQueries();
  //}

  // Application should exit with the absence of Function $54
  if (!bHaveF54)
		return -1;
		//exit(0);
	//LoadTestLimits();
	/*
	while (1){
		printf("\nPress these keys for tests:\n");
		printf(" a ) - Full Raw Capacitance Test\n");
		printf(" b ) - ADC Range Test\n");
		printf(" c ) - Sensor Speed Test\n");
		printf(" d ) - TRex Open Test\n");
		printf(" e ) - TRex Gnd Test\n");
		printf(" f ) - TRx-to-TRx and TRx-to-Vdd shorts\n");
		printf(" g ) - High Resistance Test\n");
		printf(" h ) - Full Raw Capacitance Max/Min Test\n");
		printf(" i ) - Abs Sensing ADC Range Test\n");
		printf(" j ) - Abs Sensing Delta Capacitance\n");
		printf(" k ) - Abs Sensing Raw Capcitance Test\n");
		printf("---------------------------------------------------------------");
		printf("\n z ) - Version\n");
		printf("\nPress any other key to exit.\n");
		input = _getch();
	*/
		if (!switchPage(0x01))
			return false;

		outbuf = 0;
		memset(f54_wlog_buf, 0, sizeof(f54_wlog_buf));

		switch(input){
			case 'a':
				outbuf = sprintf(f54_wlog_buf, "a - Full Raw Capacitance Test\n");
				ret = ImageTest();
				break;
			case 'b':
				outbuf = sprintf(f54_wlog_buf, "b - ADC Range Test\n");
				ADCRange();
				break;
			case 'c':
				outbuf = sprintf(f54_wlog_buf, "c - Sensor Speed Test\n");
				SensorSpeed();
				break;
			case 'd':
				outbuf = sprintf(f54_wlog_buf, "d - TRex Open Test\n");
				TRexOpenTest();
				break;
			case 'e':
				outbuf = sprintf(f54_wlog_buf, "e - TRex Gnd Test\n");
				ret = TRexGroundTest();
				break;
			case 'f':
				outbuf = sprintf(f54_wlog_buf, "f - TRex Short Test\n");
				ret = TRexShortTest();
				break;
			case 'g':
				outbuf = sprintf(f54_wlog_buf, "g - High Resistance Test\n");
				ret = HighResistanceTest();
				break;
			case 'h':
				outbuf = sprintf(f54_wlog_buf, "h - Full Raw Capacitance Max/Min Test\n");
				MaxMinTest();
				break;
			case 'i':
				outbuf = sprintf(f54_wlog_buf, "i - Abs Sensing ADC Range Test\n");
				AbsADCRange();
				break;
			case 'j':
				outbuf = sprintf(f54_wlog_buf, "j - Abs Sensing Delta Capacitance\n");
				AbsDelta();
				break;
			case 'k':
				outbuf = sprintf(f54_wlog_buf, "k - Abs Sensing Raw Capcitance Test\n");
				AbsRaw();
				break;
			case 'z':
				printk("Version: %s\n", VERSION);
				break;
			default:
				return -1;
				//PowerOffSensor();
				//exit(0);
		}
	return ret;
}
