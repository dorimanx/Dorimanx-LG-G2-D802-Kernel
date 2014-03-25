/*
	File Name	: radio-mb86a36s.h
	Version		: V4.1L04	
	Date		: 2013/03/15	
	Description	: Multimedia tuner module MB86A36S/K device driver header file
			  for TS I/F
		
	(c) 2012 FUJITSU SEMICONDUCTOR LIMITED. 
*/

#ifndef	__RADIO_MB86A35_H__
#define	__RADIO_MB86A35_H__

#ifndef __KERNEL__
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#endif

#ifndef	__RADIO_MB86A35_DEV_H__
#include <string.h>
#endif

#define	NODE_BASENAME		"/dev/mmbi_radio"	/* Node Name (BASE) */
#define	NODE_PATHNAME		NODE_BASENAME "0"	/* Node Name */

/* System Call Interface */
/*
 *	int	open( const char *pathname, int flag );
 *	int	close( int fd );
 *	int	ioctl( int fd, int cmd, void *arg );
 *	size_t	read( int fd, void *buf, size_t count );
 *	size_t	write( int fd, const char __user *buf, size_t count );
 */

/***************************************************************************************************/
/* ioctl() System call command code                                                                */
/***************************************************************************************************/

enum ioctl_command_no {
	IOCTL_RST_SOFT = 0x0102,	/* Reset : OFDM Soft-reset ON / OFF */
	IOCTL_RST_SYNC = 0x0104,	/* Reset : OFDM Sequence reset ON / OFF */

	IOCTL_SET_RECVM = 0x0201,	/* Initialize : Received mode set */
	IOCTL_SET_SPECT,	/* Initialize : Receive spectrum Invert set */
	IOCTL_SET_IQSET = 0x0204,	/* Initialize : I/Q Input set */
	IOCTL_SET_ALOG_PDOWN = 0x0208,	/* Initialize : Analog Power-down set (DAC Power-down) */
	IOCTL_SET_SCHANNEL = 0x0210,	/* Initialize : Sub-Channel set */

	IOCTL_AGC = 0x0300,	/* AGC : AGC(Auto Gain Control) register set */
	IOCTL_OFDM_INIT = 0x0400,	/* SYNC , FEC : SYNC set, set FEC */

	IOCTL_GPIO_SETUP = 0x0600,	/* GPIO : Digital I/O port control : SETUP */
	IOCTL_GPIO_READ,	/* GPIO : Digital I/O port control : READ */
	IOCTL_GPIO_WRITE,	/* GPIO : Digital I/O port control : WRITE */
	IOCTL_ANALOG,		/* GPIO : Analog port control */

	IOCTL_SEQ_GETSTAT = 0x0701,	/* Sequence control : Status get */
	IOCTL_SEQ_SETMODE,	/* Sequence control : Mode set */
	IOCTL_SEQ_GETMODE,	/* Sequence control : Mode get */
	IOCTL_SEQ_GETTMCC = 0x0708,	/* Sequence control : TMCC Information get */

	IOCTL_BER_MONISTAT = 0x1000,	/* BER Monitor : TS Output state / BER measurement state get */
	IOCTL_BER_MONICONFIG,	/* BER Monitor : BER Monitor config set */
	IOCTL_BER_MONISTART = 0x1010,	/* BER Monitor : BER Monitor start */
	IOCTL_BER_MONIGET = 0x1020,	/* BER Monitor : BER Monitor information get */
	IOCTL_BER_MONISTOP = 0x10F0,	/* BER Monitor : BER Monitor STOP */

	IOCTL_TS_START = 0x1101,	/* TS Output : Serial TS Output Start */
	IOCTL_TS_STOP,		/* TS Output : Serial TS Output OFF */
	IOCTL_TS_CONFIG,	/* TS Output : Serial TS Output Configuration */
	IOCTL_TS_PCLOCK = 0x1110,	/* TS Output : Serial TS Output Clock / Parallel TS Clock set */
	IOCTL_TS_OUTMASK,	/* TS Output : TS Output Mask OFF set */
	IOCTL_TS_INVERT,	/* TS Output : TSEN / TSERR Invert */

	IOCTL_IRQ_GETREASON = 0x9000,	/* IRQ : IRQ Get Reason Get */
	IOCTL_IRQ_SETMASK,	/* IRQ : Interrupt mask control */
	IOCTL_IRQ_TMCCPARAM_SET,	/* IRQ : TMCC parameter Interrupt setting */
	IOCTL_IRQ_TMCCPARAM_REASON,	/* IRQ : TMCC parameter Interrupt, get reason. */

//	IOCTL_CN_MONI 			= 0x2001,	/* C/N Monitor : C/N Monitoring			*/
	IOCTL_CN_MONI_CONFIG_START 	= 0x2010,	/* C/N Monitor : C/N Monitoring CONFIG and START*/
	IOCTL_CN_MONI_GET 		= 0x2100,	/* C/N Monitor : C/N Monitoring Get status	*/

//	IOCTL_MER_MONI 			= 0x4001,	/* MER Monitor : MER Monitoring 		*/
	IOCTL_MER_MONI_CONFIG_START 	= 0x4010,	/* MER Monitor : MER Monitoring Config and START*/
	IOCTL_MER_MONI_GET 		= 0x4100,	/* MER Monitor : MER Monitoring Get status	*/

	IOCTL_CH_SEARCH = 0x6000,	/* Hight speed Channel Search */

	IOCTL_RF_INIT = 0x0801,	/* RF : UHF/VHF RF-IC Register Initialize */
	IOCTL_RF_FUSEVALUE,	/* RF : FUSE VALUE */
	IOCTL_RF_CALIB_DCOFF = 0x0804,	/* RF : CALIB DCOFFSET */
	IOCTL_RF_CALIB_CTUNE = 0x0808,	/* RF : CALIB CTUNE */
	IOCTL_RF_CHANNEL = 0x0810,	/* RF : UHF/VHF Channel set */
	IOCTL_RF_PDOWN = 0x0811,	/* RF : RF POWER DOWN */
	IOCTL_RF_RSSIMONI = 0x0812,	/* RF : RSSI Monitor */

	IOCTL_POWERCTRL = 0xA000,	/* Power Control */
	IOCTL_SET_SOFT_PDOWN,				/* Soft Power Down                              */

	IOCTL_I2C_MAIN = 0xF000,	/* I2C : I2C main access */
	IOCTL_I2C_SUB,		/* I2C : I2C sub access */
	IOCTL_I2C_RF,		/* I2C : I2C RF-IC access */
	IOCTL_HRM,              /* HRM : ON OFF select */
	IOCTL_LOW_UP_IF,              /* LOW_UP_IF : LOWER IF or UPPER IF select */
	IOCTL_SPI_MAIN 			= 0xF100,	/* SPI : SPI main access			*/
	IOCTL_SPI_SUB,					/* SPI : SPI sub access				*/
	IOCTL_SPI_OFDM,					/* SPI : SPI OFDM main access			*/
	IOCTL_SPI_OFDM_SUB,				/* SPI : SPI OFDM sub access			*/
	IOCTL_SPI_RF,					/* SPI : SPI RF-IC access			*/
	IOCTL_STREAM_READ 		= 0xF200,	/* Stream Read config set			*/	
	IOCTL_STREAM_READ_CTRL,				/* Stream Read Control				*/
	IOCTL_SPI_CONFIG,				/* SPI config Setting				*/
	IOCTL_TS_SETUP,					/* TS Setting					*/
	IOCTL_SELECT_ANTENNA, 				/* Select Antenna Path			*/
};

/***************************************************************************************************/
/* ioctl() System call argement layout & define                                                    */
/***************************************************************************************************/

							/* mode */
							// -- long (2011/08/17) --
#define		PARAM_MODE_DETECT_ON	0x00
#define		PARAM_MODE_DETECT_OFF	0x01

#define		PARAM_MODE_ISDBT_13UHF		0x4D	/* ISDB-T   13seg / UHF */
#define		PARAM_MODE_ISDBT_1UHF		0x41	/* ISDB-T    1seg / UHF */
#define		PARAM_MODE_ISDBTMM_13VHF	0x8D	/* ISDB-Tmm 13seg / VHF */
#define		PARAM_MODE_ISDBTMM_1VHF		0x81	/* ISDB-Tmm  1seg / VHF */
#define		PARAM_MODE_ISDBTSB_1VHF		0xC1	/* ISDB-Tsb  1seg / VHF */
#define		PARAM_MODE_ISDBTSB_3VHF		0xC3	/* ISDB-Tsb  3seg / VHF */

							/* RESET */
#define		PARAM_RESET_ON			0x00	/* OFDM soft reset */
#define		PARAM_RESET_OFF			0xFF	/* OFDM soft reset cancel */

							/* INIT */
#define		PARAM_STATE_INIT_ON		0x01	/* OFDM SYNC sequence reset */
#define		PARAM_STATE_INIT_OFF		0x00	/* OFDM SYNC sequence reset cancel */

							/* BANDSEL */
#define		PARAM_SEG_BAND_DVBT7M		0x00	/* DVB-T / 7MHz */
#define		PARAM_SEG_BAND_DVBT8M		0x20	/* DVB-T / 8MHz */
#define		PARAM_SEG_BAND_DVBT6M		0x60	/* DVB-T / 6MHz */
#define		PARAM_SEG_BAND_DVBT5M		0x80	/* DVB-T / 5MHz */
#define		PARAM_SEG_BAND_ISDB6M		0x00	/* ISDB-T/Tmm / 6MHz */
#define		PARAM_SEG_BAND_ISDB7M		0x20	/* ISDB-T/Tmm / 7MHz */
#define		PARAM_SEG_BAND_ISDB8M		0x40	/* ISDB-T/Tmm / 8MHz */

							/* TMMSEL */
#define		PARAM_SEG_TMM_32		0x00	/* 32MHz */
#define		PARAM_SEG_TMM_16		0x04	/* 16MHz */

							/* MYTYPESEL */
#define		PARAM_SEG_MYT_ISDB13S		0x00	/* ISDB-T/Tmm 13seg */
#define		PARAM_SEG_MYT_DVBT		0x01	/* DVB-T */
#define		PARAM_SEG_MYT_ISDB1S		0x02	/* ISDB-T/Tmm/Tsb 1seg */
#define		PARAM_SEG_MYT_ISDB3S		0x03	/* ISDB-Tsb 3seg */

							/* LAYERSEL */
#define		PARAM_LAYERSEL_A		0x00	/* A Layer */
#define		PARAM_LAYERSEL_B		0x01	/* B Layer */
#define		PARAM_LAYERSEL_C		0x02	/* C Layer */

							/* IQINV */
#define		PARAM_IQINV_IQINV_NORM		0x00	/* not Invert */
#define		PARAM_IQINV_IQINV_INVT		0x04	/* Invert */

							/* IQSEL */
#define		PARAM_CONT_CTRL3_IQSEL_IF	0x00	/* IF Input */
#define		PARAM_CONT_CTRL3_IQSEL_IQ	0x02	/* IQ Input */

							/* IFSEL */
#define		PARAM_CONT_CTRL3_IFSEL_16	0x00	/* 16MHz */
#define		PARAM_CONT_CTRL3_IFSEL_32	0x01	/* 32MHz */

							/* DACPWDN */
#define		PARAM_MACRO_PDOWN_DACPWDN_OFF	0x00	/* Power-down OFF */
#define		PARAM_MACRO_PDOWN_DACPWDN_ON	0x10	/* Power-down					*/
#define		PARAM_MACRO_PDOWN_ADCPWDNI_ON	0x01	/* Power-down					*/
#define		PARAM_MACRO_PDOWN_ADCPWDNQ_ON	0x02	/* Power-down					*/

								    /* SEGCNT *//* 0 - 41 */
#define		PARAM_FTSEGCNT_SEGCNT_ISDB_T	0xFF	/* ISDB-T */

							/* GPIO_DAT */
#define		PARAM_GPIO_DAT_AC_MODE_ACCLK	0x00	/* AC_MODE : ACCLK */
#define		PARAM_GPIO_DAT_AC_MODE_ACDT	0x40	/* AC_MODE : ACDT */

							/* GPIO_IRQOUT */
#define		PARAM_GPIO_OUTSEL_IRQOUT_3	0x80	/* GPIO-3 IRQ */
#define		PARAM_GPIO_OUTSEL_IRQOUT_2	0x40	/* GPIO-2 IRQ */
#define		PARAM_GPIO_OUTSEL_IRQOUT_1	0x20	/* GPIO-1 IRQ */
#define		PARAM_GPIO_OUTSEL_IRQOUT_0	0x10	/* GPIO-0 IRQ */

							/* GPIO_OUTEN */
#define		PARAM_GPIO_OUTSEL_OUTEN_3	0x08	/* GPIO-3 OUTEN */
#define		PARAM_GPIO_OUTSEL_OUTEN_2	0x04	/* GPIO-2 OUTEN */
#define		PARAM_GPIO_OUTSEL_OUTEN_1	0x02	/* GPIO-1 OUTEN */
#define		PARAM_GPIO_OUTSEL_OUTEN_0	0x01	/* GPIO-0 OUTEN */

							/* TUNER_IRQ */
#define		PARAM_TUNER_IRQ_NONE		0x00	/* NONE Interrupt */
#define		PARAM_TUNER_IRQ_GPIO		0x02	/* GPIO Interrupt */
#define		PARAM_TUNER_IRQ_CHEND		0x01	/* Channel End Interrupt */
#define		PARAM_TUNER_IRQ_GPIO_CHEND	0x03	/* GPIO & Channel End Interrupt */

							/* TSWAIT */
#define		PARAM_S8WAIT_TSWAIT_S8		0x00	/* S8 TS Output Start */
#define		PARAM_S8WAIT_TSWAIT_S9		0x80	/* S9 TS Output Start */
							/* BERWAIT */
#define		PARAM_S8WAIT_BERWAIT_S8		0x00	/* S8 BER measurement Start */
#define		PARAM_S8WAIT_BERWAIT_S9		0x40	/* S9 BER measurement Start */
								     /* BERWAIT *//* TSWAIT */
#define		PARAM_S8WAIT_TS8_BER8		0x00	/* S8 TS / BER S8 Output Start */
#define		PARAM_S8WAIT_TS9_BER8		0x80	/* S9 TS / BER S8 Output Start */
#define		PARAM_S8WAIT_TS8_BER9		0x40	/* S8 TS / BER S9 Output Start */
#define		PARAM_S8WAIT_TS9_BER9		0xC0	/* S9 TS / BER S9 Output Start */

							/* STATE_INIT */
#define		PARAM_STATE_INIT_NOR		0x00	/* Normal State  */
#define		PARAM_STATE_INIT_INIT		0x01	/* Initial State */

							/* SYNC_STATE */
#define		PARAM_SYNC_STATE_INIT		0x00	/* Initial State */
#define		PARAM_SYNC_STATE_INITTMR	0x01	/* Initial Timer */
#define		PARAM_SYNC_STATE_DETCTMODE	0x02	/* Detect Mode Wait */
#define		PARAM_SYNC_STATE_FIXEDMODE	0x03	/* Fixed Mode */
#define		PARAM_SYNC_STATE_AFCMODE	0x04	/* AFC Mode moving */
#define		PARAM_SYNC_STATE_FFTSTART	0x05	/* FFC Start , WIDE Range Search */
#define		PARAM_SYNC_STATE_FRMSYNCSRH	0x06	/* Frame SYNC Search */
#define		PARAM_SYNC_STATE_FRMSYNC	0x07	/* Frame SYNC */
#define		PARAM_SYNC_STATE_TMCCERRC	0x08	/* TMCC Error Correcting */
#define		PARAM_SYNC_STATE_TSOUT		0x09	/* TS Output Start */

							/* MODED_CTRL */
#define		PARAM_MODED_CTRL_M3G32		0x80	/* M3G32 */
#define		PARAM_MODED_CTRL_M3G16		0x40	/* M3G16 */
#define		PARAM_MODED_CTRL_M3G08		0x20	/* M3G8  */
#define		PARAM_MODED_CTRL_M3G04		0x10	/* M3G4  */
#define		PARAM_MODED_CTRL_M2G32		0x08	/* M2G32 */
#define		PARAM_MODED_CTRL_M2G16		0x04	/* M2G16 */
#define		PARAM_MODED_CTRL_M2G08		0x02	/* M2G8  */
#define		PARAM_MODED_CTRL_M2G04		0x01	/* M2G4  */

							/* MODED_CTRL2 */
#define		PARAM_MODED_CTRL_M1G32		0x08	/* M1G32 */
#define		PARAM_MODED_CTRL_M1G16		0x04	/* M1G16 */
#define		PARAM_MODED_CTRL_M1G08		0x02	/* M1G8  */
#define		PARAM_MODED_CTRL_M1G04		0x01	/* M1G4  */

							/* MODED_STAT */
#define		PARAM_MODED_STAT_MDFAIL		0x40	/* MDFAIL */
#define		PARAM_MODED_STAT_MDDETECT	0x20	/* MDDETECT */
#define		PARAM_MODED_STAT_MDFIX		0x10	/* MDFIX */

							/* MODE */
#define		PARAM_MODED_STAT_MODE1		0x00	/* MODE1 */
#define		PARAM_MODED_STAT_MODE2		0x04	/* MODE2 */
#define		PARAM_MODED_STAT_MODE3		0x08	/* MODE3 */

							/* GUARDE */
#define		PARAM_MODED_STAT_GUARDE14	0x00	/* GUARDE 1/4 */
#define		PARAM_MODED_STAT_GUARDE18	0x01	/* GUARDE 1/8 */
#define		PARAM_MODED_STAT_GUARDE116	0x02	/* GUARDE 1/16 */
#define		PARAM_MODED_STAT_GUARDE132	0x03	/* GUARDE 1/32 */

							/* TMCCREAD */
#define		PARAM_TMCCREAD_TMCCLOCK_AUTO	0x00	/* Auto Update */
#define		PARAM_TMCCREAD_TMCCLOCK_NOAUTO	0x01	/* Not Update */

							/* FEC_IN : CORRECT , VALID */
#define		PARAM_TMCC_CORRECT_INV_NOW	0x00	/* TMCC CORRECT=0 : VALID=0 */
#define		PARAM_TMCC_CORRECT_VLD_NOW	0x02	/* TMCC CORRECT=1 : VALID=0 */
#define		PARAM_TMCC_CORRECT_INV_DONE	0x01	/* TMCC CORRECT=0 : VALID=1 */
#define		PARAM_TMCC_CORRECT_VLD_DONE	0x03	/* TMCC CORRECT=1 : VALID=1 */

							/* VBERON */
#define		PARAM_VBERON_BEFORE		0x01	/* BEFORE */

							/* VBERXRSTC */
#define		PARAM_VBERXRST_VBERXRSTC_C	0x00	/* C Layer BER Continue */
#define		PARAM_VBERXRST_VBERXRSTC_E	0x04	/* C Layer BER End */
							/* VBERXRSTB */
#define		PARAM_VBERXRST_VBERXRSTB_C	0x00	/* B Layer BER Continue */
#define		PARAM_VBERXRST_VBERXRSTB_E	0x02	/* B Layer BER End */
							/* VBERXRSTA */
#define		PARAM_VBERXRST_VBERXRSTA_C	0x00	/* A Layer BER Continue */
#define		PARAM_VBERXRST_VBERXRSTA_E	0x01	/* A Layer BER End */

							/* RSBERAUTO */
#define		PARAM_RSBERON_RSBERAUTO_M	0x00	/* Manual Update Mode */
#define		PARAM_RSBERON_RSBERAUTO_A	0x10	/* Auto Update Mode */
							/* RSBERC */
#define		PARAM_RSBERON_RSBERC_S		0x00	/* C Layer RSBER STOP */
#define		PARAM_RSBERON_RSBERC_B		0x04	/* C Layer RSBER BEGIN */
							/* RSBERB */
#define		PARAM_RSBERON_RSBERB_S		0x00	/* B Layer RSBER STOP */
#define		PARAM_RSBERON_RSBERB_B		0x02	/* B Layer RSBER BEGIN */
							/* RSBERA */
#define		PARAM_RSBERON_RSBERA_S		0x00	/* A Layer RSBER STOP */
#define		PARAM_RSBERON_RSBERA_B		0x01	/* A Layer RSBER BEGIN */

							/* RSBERXRSTC */
#define		PARAM_RSBERXRST_RSBERXRSTC_S	0x00	/* C Layer RSBER RESET */
#define		PARAM_RSBERXRST_RSBERXRSTC_B	0x04	/* C Layer RSBER NORM */
							/* RSBERXRSTB */
#define		PARAM_RSBERXRST_RSBERXRSTB_S	0x00	/* B Layer RSBER RESET */
#define		PARAM_RSBERXRST_RSBERXRSTB_B	0x02	/* B Layer RSBER NORM */
							/* RSBERXRSTA */
#define		PARAM_RSBERXRST_RSBERXRSTA_S	0x00	/* A Layer RSBER RESET */
#define		PARAM_RSBERXRST_RSBERXRSTA_B	0x01	/* A Layer RSBER NORM */

							/* SBERCEFC */
#define		PARAM_RSBERCEFLG_SBERCEFC_E	0x04	/* C Layer SBER END */
							/* SBERCEFB */
#define		PARAM_RSBERCEFLG_SBERCEFB_E	0x02	/* B Layer SBER END */
							/* SBERCEFA */
#define		PARAM_RSBERCEFLG_SBERCEFA_E	0x01	/* A Layer SBER END */

							/* RSBERTHRC */
#define		PARAM_RSBERTHFLG_RSBERTHRC_R	0x40	/* C Layer RSBER TH RESET */
							/* RSBERTHRB */
#define		PARAM_RSBERTHFLG_RSBERTHRB_R	0x20	/* B Layer RSBER TH RESET */
							/* RSBERTHRA */
#define		PARAM_RSBERTHFLG_RSBERTHRA_R	0x10	/* A Layer RSBER TH RESET */
							/* RSBERTHFC */
#define		PARAM_RSBERTHFLG_RSBERTHFC_F	0x04	/* C Layer RSBER TH FAIL */
							/* RSBERTHFB */
#define		PARAM_RSBERTHFLG_RSBERTHFB_F	0x02	/* B Layer RSBER TH FAIL */
							/* RSBERTHFA */
#define		PARAM_RSBERTHFLG_RSBERTHFA_F	0x01	/* A Layer RSBER TH FAIL */

							/* PERENC */
#define		PARAM_PEREN_PERENC_F		0x04	/* C Layer RS PER Flag */
							/* PERENB */
#define		PARAM_PEREN_PERENB_F		0x02	/* B Layer RS PER Flag */
							/* PERENA */
#define		PARAM_PEREN_PERENA_F		0x01	/* A Layer RS PER Flag */

							/* PERRSTC */
#define		PARAM_PERRST_PERRSTC		0x04	/* C Layer PER counter reset */
							/* PERRSTB */
#define		PARAM_PERRST_PERRSTB		0x02	/* B Layer PER counter reset */
							/* PERRSTA */
#define		PARAM_PERRST_PERRSTA		0x01	/* A Layer PER counter reset */

							/* VBERFLG */
#define		PARAM_VBERFLG_VBERFLGA		0x01	/* A Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGB		0x02	/* B Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGC		0x04	/* C Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGAB		0x03	/* A,B Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGAC		0x05	/* A,C Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGBC		0x06	/* B,C Layer BER Completion */
#define		PARAM_VBERFLG_VBERFLGALL	0x07	/* ALL Layer BER Completion */

							/* RSBERCEFLG */
#define		PARAM_RSBERCEFLG_SBERCEFA	0x01	/* A Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFB	0x02	/* B Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFC	0x04	/* C Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFAB	0x03	/* A,B Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFAC	0x05	/* A,C Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFBC	0x06	/* B,C Layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFALL	0x07	/* ALL Layer RSBER Completion */

							/* PERFLG */
#define		PARAM_PERFLG_PERFLGA		0x01	/* A Layer PER Completion */
#define		PARAM_PERFLG_PERFLGB		0x02	/* B Layer PER Completion */
#define		PARAM_PERFLG_PERFLGC		0x04	/* C Layer PER Completion */
#define		PARAM_PERFLG_PERFLGAB		0x03	/* A,B Layer PER Completion */
#define		PARAM_PERFLG_PERFLGAC		0x05	/* A,C Layer PER Completion */
#define		PARAM_PERFLG_PERFLGBC		0x06	/* B,C Layer PER Completion */
#define		PARAM_PERFLG_PERFLGALL		0x07	/* ALL Layer PER Completion */

//                                                      /* ERID */
//#define               PARAM_RS0_ERID_OFF              0x00    /* ------ */
//#define               PARAM_RS0_ERID_ON               0x04    /* ------ */
							/* RSEN */
#define		PARAM_RS0_RSEN_OFF		0x00	/* RS Enable Flag RS=OFF */
#define		PARAM_RS0_RSEN_ON		0x02	/* RS Enable Flag RS=ON  */

							/* SCLKSEL */
#define		PARAM_SBER_SCLKSEL_OFF		0x00	/* Serial Out TSCLK */
#define		PARAM_SBER_SCLKSEL_ON		0x80	/* Serial Out TSCLK (Inversion) */
							/* SBERSEL */
#define		PARAM_SBER_SBERSEL_OFF		0x00	/* BER measurement point */
#define		PARAM_SBER_SBERSEL_ON		0x20	/* BER measurement point (RS) */
							/* SPACON */
#define		PARAM_SBER_SPACON_OFF		0x00	/* Serial TS OUT SYNC-BYTE */
#define		PARAM_SBER_SPACON_ON		0x10	/* Serial TS OUT SYNC-BYTE (OUTPUT) */
							/* SENON */
#define		PARAM_SBER_SENON_OFF		0x00	/* Serial TS OUT RS Parity-BYTE */
#define		PARAM_SBER_SENON_ON		0x08	/* Serial TS OUT RS Parity-BYTE */
								    /* SLAYER *//* Serial TS OUT Layer */
#define		PARAM_SBER_SLAYER_OFF		0x00	/* OFF */
#define		PARAM_SBER_SLAYER_A		0x01	/* A Layer */
#define		PARAM_SBER_SLAYER_B		0x02	/* B Layer */
#define		PARAM_SBER_SLAYER_C		0x03	/* C Layer */
#define		PARAM_SBER_SLAYER_AB		0x04	/* A+B Layer */
#define		PARAM_SBER_SLAYER_AC		0x05	/* A+C Layer */
#define		PARAM_SBER_SLAYER_BC		0x06	/* B+C Layer */
#define		PARAM_SBER_SLAYER_ALL		0x07	/* All Layer */

							/* SCLKSEL */
#define		PARAM_SBER2_SCLKSEL_OFF		0x00	/* Serial Out TSCLK */
#define		PARAM_SBER2_SCLKSEL_ON		0x80	/* Serial Out TSCLK (Inversion) */
							/* SBERSEL */
#define		PARAM_SBER2_SBERSEL_OFF		0x00	/* BER measurement point */
#define		PARAM_SBER2_SBERSEL_ON		0x20	/* BER measurement point (RS) */
							/* SPACON */
#define		PARAM_SBER2_SPACON_OFF		0x00	/* Serial TS OUT SYNC-BYTE */
#define		PARAM_SBER2_SPACON_ON		0x10	/* Serial TS OUT SYNC-BYTE (OUTPUT) */
							/* SENON */
#define		PARAM_SBER2_SENON_OFF		0x00	/* Serial TS OUT RS Parity-BYTE */
#define		PARAM_SBER2_SENON_ON		0x08	/* Serial TS OUT RS Parity-BYTE */
								    /* SLAYER *//* Serial TS OUT Layer */
#define		PARAM_SBER2_SLAYER_OFF		0x00	/* OFF */
#define		PARAM_SBER2_SLAYER_A		0x01	/* A Layer */

							/* TSERRINV */
#define		PARAM_TSOUT_TSERRINV_OFF	0x00	/* TSERR NON Invert Out */
#define		PARAM_TSOUT_TSERRINV_ON		0x80	/* TSERR Invert Out */
							/* TSENINV */
#define		PARAM_TSOUT_TSENINV_OFF		0x00	/* TSEN NON Invert Out */
#define		PARAM_TSOUT_TSENINV_ON		0x40	/* TSEN Invert Out */
							/* TSSINV */
#define		PARAM_TSOUT_TSSINV_MSB		0x00	/* Serial TS Out 0: MSB BEGIN */
#define		PARAM_TSOUT_TSSINV_LSB		0x20	/* Serial TS Out 1: LSB BEGIN */
							/* TSERRMASK2 */
#define		PARAM_TSOUT_TSERRMASK2_OFF	0x00	/* OFF : */
#define		PARAM_TSOUT_TSERRMASK2_ON	0x04	/* ON  : TSERR=0 Only */
							/* TSERRMASK2 */
#define		PARAM_TSOUT_TSERRMASK_OFF	0x00	/* OFF : */
#define		PARAM_TSOUT_TSERRMASK_ON	0x02	/* ON  : TSERR Replaced NULL data */

							/* TSERRINV */
#define		PARAM_TSOUT2_TSERRINV_OFF	0x00	/* TSERR NON Invert Out */
#define		PARAM_TSOUT2_TSERRINV_ON	0x80	/* TSERR Invert Out */
							/* TSENINV */
#define		PARAM_TSOUT2_TSENINV_OFF	0x00	/* TSEN NON Invert Out */
#define		PARAM_TSOUT2_TSENINV_ON		0x40	/* TSEN Invert Out */
							/* TSSINV */
#define		PARAM_TSOUT2_TSSINV_MSB		0x00	/* Serial TS Out 0: MSB BEGIN */
#define		PARAM_TSOUT2_TSSINV_LSB		0x20	/* Serial TS Out 1: LSB BEGIN */

							/* PLAYER */
#define		PARAM_PBER_PLAYER_OFF		0x00	/* OFF */
#define		PARAM_PBER_PLAYER_A		0x01	/* A Layer */
#define		PARAM_PBER_PLAYER_B		0x02	/* B Layer */
#define		PARAM_PBER_PLAYER_C		0x03	/* C Layer */
#define		PARAM_PBER_PLAYER_AB		0x04	/* A+B Layer */
#define		PARAM_PBER_PLAYER_AC		0x05	/* A+C Layer */
#define		PARAM_PBER_PLAYER_BC		0x06	/* B+C Layer */
#define		PARAM_PBER_PLAYER_ALL		0x07	/* All Layer */

							/* PLAYER */
#define		PARAM_PBER2_PLAYER_OFF		0x00	/* OFF */
#define		PARAM_PBER2_PLAYER_A		0x01	/* A Layer */

							/* TSFRMMASK0 */
#define		PARAM_TSMASK0_TSFRMMASK_OFF	0x00	/* TS Clock Mask OFF */
#define		PARAM_TSMASK0_TSFRMMASK_ON	0x80	/* TS Clock Mask ON  */
							/* TSERRMASK0 */
#define		PARAM_TSMASK0_TSERRMASK_OFF	0x00	/* TS Error Flag Mask OFF */
#define		PARAM_TSMASK0_TSERRMASK_ON	0x10	/* TS Error Flag Mask ON  */
							/* TSPACMASK0 */
#define		PARAM_TSMASK0_TSPACMASK_OFF	0x00	/* TSPAC Mask OFF */
#define		PARAM_TSMASK0_TSPACMASK_ON	0x08	/* TSPAC Mask ON  */
							/* TSENMASK0 */
#define		PARAM_TSMASK0_TSENMASK_OFF	0x00	/* TS Enable Mask OFF */
#define		PARAM_TSMASK0_TSENMASK_ON	0x04	/* TS Enable Mask ON  */
							/* TSDTMASK0 */
#define		PARAM_TSMASK0_TSDTMASK_OFF	0x00	/* TS DATA Mask OFF */
#define		PARAM_TSMASK0_TSDTMASK_ON	0x02	/* TS DATA Mask ON  */
							/* TSCLKMASK0 */
#define		PARAM_TSMASK0_TSCLKMASK_OFF	0x00	/* TS Clock Mask OFF */
#define		PARAM_TSMASK0_TSCLKMASK_ON	0x01	/* TS Clock Mask ON  */

							/* TSFRMMASK1 */
#define		PARAM_TSMASK1_TSFRMMASK_OFF	0x00	/* TS Clock Mask OFF */
#define		PARAM_TSMASK1_TSFRMMASK_ON	0x80	/* TS Clock Mask ON  */
							/* TSERRMASK1 */
#define		PARAM_TSMASK1_TSERRMASK_OFF	0x00	/* TS Error Flag Mask OFF */
#define		PARAM_TSMASK1_TSERRMASK_ON	0x10	/* TS Error Flag Mask ON  */
							/* TSPACMASK1 */
#define		PARAM_TSMASK1_TSPACMASK_OFF	0x00	/* TSPAC Mask OFF */
#define		PARAM_TSMASK1_TSPACMASK_ON	0x08	/* TSPAC Mask ON  */
							/* TSENMASK1 */
#define		PARAM_TSMASK1_TSENMASK_OFF	0x00	/* TS Enable Mask OFF */
#define		PARAM_TSMASK1_TSENMASK_ON	0x04	/* TS Enable Mask ON  */
							/* TSDTMASK1 */
#define		PARAM_TSMASK1_TSDTMASK_OFF	0x00	/* TS DATA Mask OFF */
#define		PARAM_TSMASK1_TSDTMASK_ON	0x02	/* TS DATA Mask ON  */
							/* TSCLKMASK1 */
#define		PARAM_TSMASK1_TSCLKMASK_OFF	0x00	/* TS Clock Mask OFF */
#define		PARAM_TSMASK1_TSCLKMASK_ON	0x01	/* TS Clock Mask ON  */

							/* IRQ */
#define		PARAM_IRQ_NON			0x00	/* NON Interrupt */
#define		PARAM_IRQ_YES			0x01	/* Interrupt */

							/* IRQ 1 */
#define		PARAM_FECIRQ1_TSERRC		0x40	/* layer C TSERR Interrupt */
#define		PARAM_FECIRQ1_TSERRB		0x20	/* layer B TSERR Interrupt */
#define		PARAM_FECIRQ1_TSERRA		0x10	/* layer A TSERR Interrupt */
#define		PARAM_FECIRQ1_EMG		0x04	/* Emergency Interrupt */
#define		PARAM_FECIRQ1_CNT		0x02	/* TMCC count down Interrupt */
#define		PARAM_FECIRQ1_ILL		0x01	/* TMCC illegal parameter Interrupt */

							/* IRQ 2 */
#define		PARAM_FECIRQ2_SBERCEFCC		0x40	/* layer C RSBER count end Interrupt */
#define		PARAM_FECIRQ2_SBERCEFCB		0x20	/* layer B RSBER count end Interrupt */
#define		PARAM_FECIRQ2_SBERCEFCA		0x10	/* layer A RSBER count end Interrupt */
#define		PARAM_FECIRQ2_SBERTHFC		0x04	/* layer C RSBER threshold over Interrupt */
#define		PARAM_FECIRQ2_SBERTHFB		0x02	/* layer B RSBER threshold over Interrupt */
#define		PARAM_FECIRQ2_SBERTHFA		0x01	/* layer A RSBER threshold over Interrupt */

							/* TUNER_IRQ */
#define		PARAM_TUNER_IRQ_NONE		0x00	/* NONE interrupt */
#define		PARAM_TUNER_IRQ_GPIO		0x02	/* GPIO interrupt */
#define		PARAM_TUNER_IRQ_CHEND		0x01	/* CHEND interrupt */
#define		PARAM_TUNER_IRQ_GPIO_CHEND	0x03	/* GPIO & CHEND interrupt */

							/* TMCC_IRQ_MASK */
#define		PARAM_TMCC_IRQ_MASK_EMG		0x04	/* EMG mask */
#define		PARAM_TMCC_IRQ_MASK_CNTDWON	0x02	/* CNTDWON mask */
#define		PARAM_TMCC_IRQ_MASK_ILL		0x01	/* ILL mask */

							/* SBER_IRQ_MASK */
#define		PARAM_SBER_IRQ_MASK_DTERIRQ	0x80	/* ?? mask */
#define		PARAM_SBER_IRQ_MASK_THMASKC	0x20	/* THMASK C */
#define		PARAM_SBER_IRQ_MASK_THMASKB	0x10	/* THMASK B */
#define		PARAM_SBER_IRQ_MASK_THMASKA	0x08	/* THMASK A */
#define		PARAM_SBER_IRQ_MASK_CEMASKC	0x04	/* CEMASK C */
#define		PARAM_SBER_IRQ_MASK_CEMASKB	0x02	/* CEMASK B */
#define		PARAM_SBER_IRQ_MASK_CEMASKA	0x01	/* CEMASK A */

							/* TSERR_IRQ_MASK */
#define		PARAM_TSERR_IRQ_MASK_C		0x04	/* TSERR IRQ MASK C */
#define		PARAM_TSERR_IRQ_MASK_B		0x02	/* TSERR IRQ MASK B */
#define		PARAM_TSERR_IRQ_MASK_A		0x01	/* TSERR IRQ MASK A */

							/* TMCC_IRQ_RST */
#define		PARAM_TMCC_IRQ_RST_EMG		0x04	/* EMG reset */
#define		PARAM_TMCC_IRQ_RST_CNTDWON	0x02	/* CNTDWON reset */
#define		PARAM_TMCC_IRQ_RST_ILL		0x01	/* ILL reset */

							/* RSBERRST */
#define		PARAM_RSBERRST_SBERXRSTC_R	0x00	/* C layer RSBER reset */
#define		PARAM_RSBERRST_SBERXRSTC_N	0x04	/* C layer RSBER reset */
#define		PARAM_RSBERRST_SBERXRSTB_R	0x00	/* B layer RSBER reset */
#define		PARAM_RSBERRST_SBERXRSTB_N	0x02	/* B layer RSBER reset */
#define		PARAM_RSBERRST_SBERXRSTA_R	0x00	/* A layer RSBER reset */
#define		PARAM_RSBERRST_SBERXRSTA_N	0x01	/* A layer RSBER reset */

							/* RSBERCEFLG */
#define		PARAM_RSBERCEFLG_SBERCEFA	0x01	/* A layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFB	0x02	/* B layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFC	0x04	/* C layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFAB	0x03	/* A,B layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFAC	0x05	/* A,C layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFBC	0x06	/* B,C layer RSBER Completion */
#define		PARAM_RSBERCEFLG_SBERCEFALL	0x07	/* B,C layer RSBER Completion */

							/* RSBERTHFLG */
#define		PARAM_RSBERTHFLG_SBERTHRC_R	0x40	/* C layer RSBER threshold reset */
#define		PARAM_RSBERTHFLG_SBERTHRB_R	0x20	/* B layer RSBER threshold reset */
#define		PARAM_RSBERTHFLG_SBERTHRA_R	0x10	/* A layer RSBER threshold reset */
#define		PARAM_RSBERTHFLG_SBERTHFC_F	0x04	/* C layer RSBER threshold flag */
#define		PARAM_RSBERTHFLG_SBERTHFB_F	0x02	/* B layer RSBER threshold flag */
#define		PARAM_RSBERTHFLG_SBERTHFA_F	0x01	/* A layer RSBER threshold flag */

							/* RSERRFLG */
#define		PARAM_RSERRFLG_RSERRSTC_R	0x40	/* C layer TSERR IRQ flag reset */
#define		PARAM_RSERRFLG_RSERRSTB_R	0x20	/* B layer TSERR IRQ flag reset */
#define		PARAM_RSERRFLG_RSERRSTA_R	0x10	/* A layer TSERR IRQ flag reset */
#define		PARAM_RSERRFLG_RSERRC_F		0x04	/* C layer TSERR IRQ flag */
#define		PARAM_RSERRFLG_RSERRB_F		0x02	/* B layer TSERR IRQ flag */
#define		PARAM_RSERRFLG_RSERRA_F		0x01	/* A layer TSERR IRQ flag */

							/* FECIRQ1 */
#define		PARAM_FECIRQ1_RSERRC		0x40	/* C layer TSERR IRQ */
#define		PARAM_FECIRQ1_RSERRB		0x20	/* C layer TSERR IRQ */
#define		PARAM_FECIRQ1_RSERRA		0x10	/* C layer TSERR IRQ */
#define		PARAM_FECIRQ1_LOCK		0x08	/* LOCK IRQ */
#define		PARAM_FECIRQ1_EMG		0x04	/* EMG IRQ */
#define		PARAM_FECIRQ1_CNT		0x02	/* TMCC count down start IRQ */
#define		PARAM_FECIRQ1_ILL		0x01	/* parameter error IRQ */

							/* XIRQINV */
#define		PARAM_XIRQINV_LOW		0x00	/* Low active */
#define		PARAM_XIRQINV_HIGH		0x04	/* High active */

							/* TMCCCHK_HI,LO : TMCCCHK2_HI,LO */
#define		PARAM_TMCCCHK_14		0x40	/* 14. ISDB-T, A layer DQPSK */
#define		PARAM_TMCCCHK_13		0x20	/* 13. Tsb, DQPSK */
#define		PARAM_TMCCCHK_12		0x10	/* 12. Tsb, mark 3/4, 5/6, 7/8 */
#define		PARAM_TMCCCHK_11		0x08	/* 11. Tsb, B layer 64QAM */
#define		PARAM_TMCCCHK_10		0x04	/* 10. IL I=0 */
#define		PARAM_TMCCCHK_9			0x02	/*  9. IL MODE2 I=16, MODE3 I=8 */
#define		PARAM_TMCCCHK_8			0x01	/*  8. ISDB-T B/C layer DQPSK */
#define		PARAM_TMCCCHK_7			0x80	/*  7. Tsb C layer unused */
#define		PARAM_TMCCCHK_6			0x40	/*  6. A layer > B layer */
#define		PARAM_TMCCCHK_5			0x20	/*  5. parameter reserved */
#define		PARAM_TMCCCHK_4			0x10	/*  4. B layer parameter, C layer parameter unused */
#define		PARAM_TMCCCHK_3			0x08	/*  3. A layer unused */
#define		PARAM_TMCCCHK_2			0x04	/*  2. Number of the segments 0 */
#define		PARAM_TMCCCHK_1			0x02	/*  1. unused layer */
#define		PARAM_TMCCCHK_0			0x01	/*  0. Number of the segments is right ? */

							/* EMG_INV */
#define		PARAM_EMG_INV_RECV		0x00	/* EMG recv interrupt */
#define		PARAM_EMG_INV_NORECV		0x10	/* EMG not recv interrupt */

							/* MODE */
#define		PARAM_CNCNT2_MODE_AUTO		0x00	/* Auto operation MODE */
#define		PARAM_CNCNT2_MODE_MUNL		0x04	/* Manual operation MODE */

							/* MODE */
#define		PARAM_MERCTRL_MODE_AUTO		0x00	/* Auto operation MODE */
#define		PARAM_MERCTRL_MODE_MUNL		0x02	/* Manual operation MODE */

							/* GPIO_RST */
#define		PARAM_TUNER_IRQCTL_GPIO_RST	0x20	/* GPIO Flag Reset */
							/* CHEND_RST */
#define		PARAM_TUNER_IRQCTL_CHEND_RST	0x10	/* Channel-End Flag Reset */
							/* GPIO_MASK */
#define		PARAM_TUNER_IRQCTL_GPIOMASK	0x02	/* GPIO Mask */
							/* CHENDMASK */
#define		PARAM_TUNER_IRQCTL_CHENDMASK	0x01	/* Channel-End Mask */

							/* SEARCH_AFT */
#define		PARAM_SEARCH_CTRL_SEARCH_AFT_B	0x00	/* After go S0 state */
#define		PARAM_SEARCH_CTRL_SEARCH_AFT_N	0x20	/* After NEXT state Step */
							/* SEARCH_AFT */
#define		PARAM_SEARCH_CTRL_SEARCH_RST	0x10	/* Search result clear */
							/* SEARCH */
#define		PARAM_SEARCH_CTRL_SEARCH	0x01	/* Search Start Flag */

							/* END */
#define		PARAM_SEARCH_END_END		0x01	/* Channel search complete flag */

#define		PARAM_I2C_MODE_SEND		0x01	/* I2C send access */
#define		PARAM_I2C_MODE_SEND_8		0x01	/* I2C send access :  8 bits */
#define		PARAM_I2C_MODE_SEND_16		0x02	/* I2C send access : 16 bits */
#define		PARAM_I2C_MODE_SEND_24		0x03	/* I2C send access : 24 bits */
#define		PARAM_I2C_MODE_RECV		0x10	/* I2C recv access */
#define		PARAM_I2C_MODE_RECV_8		0x10	/* I2C recv access :  8 bits */
#define		PARAM_I2C_MODE_RECV_16		0x12	/* I2C recv access : 16 bits */
#define		PARAM_I2C_MODE_RECV_24		0x13	/* I2C recv access : 24 bits */

#define		PARAM_SPI_MODE_SEND		0x01	/* SPI send access 				*/
#define		PARAM_SPI_MODE_SEND_8		0x01	/* SPI send access :  8 bits			*/
#define		PARAM_SPI_MODE_SEND_16		0x02	/* SPI send access : 16 bits			*/
#define		PARAM_SPI_MODE_SEND_24		0x03	/* SPI send access : 24 bits			*/
#define		PARAM_SPI_MODE_RECV		0x11	/* SPI recv access 				*/
#define		PARAM_SPI_MODE_RECV_8		0x11	/* SPI recv access :  8 bits			*/
#define		PARAM_SPI_MODE_RECV_16		0x12	/* SPI recv access : 16 bits			*/
#define		PARAM_SPI_MODE_RECV_24		0x13	/* SPI recv access : 24 bits			*/

#define		PARAM_HRM_ON				0x01	/* HRM LOWER IF */
#define		PARAM_HRM_OFF				0x02	/* HRM UPPER IF */
#define		PARAM_MODE_LOWER_IF			0x01	/* IOCTL_LOW_UP_IF LOWER IF */
#define		PARAM_MODE_UPPER_IF			0x02	/* IOCTL_LOW_UP_IF UPPER IF */

#define		PARAM_SPICTRL_STMODE_MASTER	0x00	/* SPI Master					*/
#define		PARAM_SPICTRL_STMODE_SLAVE	0x10	/* SPI SLAVE					*/
#define		PARAM_SPICTRL_SPINUM_SINGLE	0x00	/* Single system				*/
#define		PARAM_SPICTRL_SPINUM_DUAL	0x01	/* Dual system					*/
#define		PARAM_IOSEL3_SPIC_OPENDRAIN	0x00	/* Open-drain mode				*/
#define		PARAM_IOSEL3_SPIC_CMOS		0x08	/* CMOS mode					*/
#define		PARAM_IO_OUTEN_HIZ_EN		0x01	/* Hi-z Enable					*/
#define		PARAM_IO_OUTEN_HIZ_DIS		0x00	/* Hi-z Disanable				*/

#define         PARAM_STREAMREAD_START          0x01    /* Stream read START				*/
#define         PARAM_STREAMREAD_STOP           0x02    /* Stream read STOP				*/

#define         PARAM_IRQMASK_STREAMREAD_ON     0x00    /* Stream read IRQ ENABLE			*/
#define         PARAM_IRQMASK_STREAMREAD_OFF    0x40    /* Stream read IRQ DISABLE			*/
#define		PARAM_SBER_TSCLKCTRL_ON		0x40	/* TS out not gated				*/
#define		PARAM_SBER_TSCLKCTRL_OFF	0x00	/* TS out gated					*/
#define		PARAM_NULLCTRL_PNULL_ON		0x00	/* Output Null packet PLAYER			*/
#define		PARAM_NULLCTRL_PNULL_OFF	0x02	/* Not Output Null packet PLAYER		*/
#define		PARAM_NULLCTRL_SNULL_ON		0x00	/* Output Null packet SLAYER			*/
#define		PARAM_NULLCTRL_SNULL_OFF	0x01	/* Not Output Null paket SLAYER			*/

#define		PARAM_PACBYTE_204P		0x00	/* 204byte with parity				*/
#define		PARAM_PACBYTE_188N		0x01	/* 188byte with parity				*/
#define		PARAM_ERRCTRL_ERROUT		0x00	/* ERROR out put				*/
#define		PARAM_ERRCTRL_ERRNOOUT		0x01	/* ERROR not out put				*/

/***************************************************************************************************/

struct ioctl_reset {
	u8 RESET;
	u8 STATE_INIT;
};
typedef struct ioctl_reset ioctl_reset_t;

struct ioctl_init {
	u8 SEGMENT;
	u8 LAYERSEL;
	u8 IQINV;
	u8 CONT_CTRL3;
	u8 MACRO_PDOWN;
	u8 FTSEGCNT;
};
typedef struct ioctl_init ioctl_init_t;

/* AGC */
struct ioctl_agc {
	u8 mode;
	u16 IFA;		/* IFAH , IFAL */
	u16 IFB;		/* IFBH , IFBL */
	u8 IFAGCO;		/* IFAGCO *//* 0 - 255 */
	u8 MAXIFAGC;		/* MAXIFAGC *//* 0 - 255 */
	u16 VIFREF2;		/* VIFREF2H , VIFREF2L */
	u8 IFSAMPLE;		/* IFSAMPLE */
	/* return information */
	u8 IFAGC;		/* IFAGC *//* 0 - 255 */
	u8 IFAGCDAC;		/* IFAGCDAC */
};
typedef struct ioctl_agc ioctl_agc_t;

/* GPIO */
struct ioctl_port {
	u8 GPIO_DAT;
	u8 GPIO_OUTSEL;
	u8 DACOUT;
};
typedef struct ioctl_port ioctl_port_t;

/* Sequence control */
struct ioctl_seq {
	u8 S8WAIT;
	u8 STATE_INIT;
	/* return information */
	u8 SYNC_STATE;
	u8 MODED_CTRL;
	u8 MODED_CTRL2;
	u8 MODE_DETECT; // -- long (2011/08/26) --
	u8 MODE; // -- long (2011/08/26) --
	u8 GUARD; // -- long (2011/08/26) --
	u8 MODED_STAT;
	u8 TMCCREAD;		/* TMCCREAD */
	/* return information */
	u8 TMCC[32];		/* TMCC *//* TMCC0 - TMCC31 */
	u8 FEC_IN;
};
typedef struct ioctl_seq ioctl_seq_t;

/* BER Monitor */
struct ioctl_ber_moni {
	u8 S8WAIT;
	u8 VBERON;
	u8 VBERXRST;
	u8 VBERXRST_SAVE;
	u32 VBERSETA;		/* VBERSETA0 , VBERSETA1 , VBERSETA2 */
	u32 VBERSETB;		/* VBERSETB0 , VBERSETB1 , VBERSETB2 */
	u32 VBERSETC;		/* VBERSETC0 , VBERSETC1 , VBERSETC2 */
	u8 RSBERON;
	u8 RSBERXRST;
	u8 RSBERTHFLG;

	u16 SBERSETA;		/* SBERSETA0 , SBERSETA1 */
	u16 SBERSETB;		/* SBERSETB0 , SBERSETB1 */
	u16 SBERSETC;		/* SBERSETC0 , SBERSETC1 */
	u8 PEREN;
	u8 PERRST;
	u16 PERSNUMA;		/* SBERSETA0 , SBERSETA1 */
	u16 PERSNUMB;		/* SBERSETB0 , SBERSETB1 */
	u16 PERSNUMC;		/* SBERSETC0 , SBERSETC1 */
	/* return information */
	u8 VBERFLG;
	u8 RSBERCEFLG;
	u8 PERFLG;

	u32 VBERDTA;
	u32 VBERDTB;
	u32 VBERDTC;
	u32 RSBERDTA;
	u32 RSBERDTB;
	u32 RSBERDTC;
	u16 PERERRA;
	u16 PERERRB;
	u16 PERERRC;
};
typedef struct ioctl_ber_moni ioctl_ber_moni_t;

/* TS Output  */
struct ioctl_ts {
	u8 RS0;
	u8 SBER;
	u8 SBER2;
	u8 TSOUT;
	u8 TSOUT2;
	u8 PBER;
	u8 PBER2;
	u8 TSMASK0;
	u8 TSMASK1;
};
typedef struct ioctl_ts ioctl_ts_t;

/* IRQ reason */
struct ioctl_irq {
	u8 irq;
	u8 FECIRQ1;
	u8 FECIRQ2;
	u8 TUNER_IRQ;
	u8 TMCC_IRQ_MASK;
	u8 SBER_IRQ_MASK;
	u8 TSERR_IRQ_MASK;
	u8 TMCC_IRQ_RST;
	u8 RSBERON;
	u8 RSBERRST;
	u8 RSBERCEFLG;
	u8 RSBERTHFLG;
	u8 RSERRFLG;
	u8 XIRQINV;
	u8 TMCCCHK_HI;
	u8 TMCCCHK_LO;
	u8 TMCCCHK2_HI;
	u8 TMCCCHK2_LO;
	u8 dummy;
	u8 PCHKOUT0;
	u8 PCHKOUT1;
	u8 EMG_INV;
};
typedef struct ioctl_irq ioctl_irq_t;

	/* C/N Monitor */
struct ioctl_cn_moni {
	u8 CNCNT2;
	u8 cn_count;		/* LOOP count */
	u8 CNCNT;		/* SYMBOLE count *//* max 15 */
	u16 CNDATA[1];		/* measurement result area */
};
typedef struct ioctl_cn_moni ioctl_cn_moni_t;

	/* MER Monitor */

struct mer_data {
	u32 A;			/* layer A : measurement result area */
	u32 B;			/* layer B : measurement result area */
	u32 C;			/* layer C : measurement result area */
};

struct ioctl_mer_moni {
	u8 MERCTRL;
	u8 mer_count;		/* LOOP count */
	u8 MERSTEP;		/* SYMBOLE count *//* max 7 */
	u8 mer_flag;		/* 0: 1st , 1: 2nd time call */

	u32 mer_dummy32;

	struct mer_data MER[1];
};
typedef struct ioctl_mer_moni ioctl_mer_moni_t;

/* Hight-Speed Channel Search control */
struct ioctl_ch_search {
	u8 search_flag;
	u8 mode;
	u8 TUNER_IRQCTL;
	u8 SEARCH_CHANNEL;	/* Search Channel No. *//* 0 - 127 */
	u8 SEARCH_CTRL;		/* Search Channel , AFT */
	u8 CHANNEL[9];		/* Search Channel */
	u8 SEARCH_END;
};
typedef struct ioctl_ch_search ioctl_ch_search_t;

/* MB86A35(RF Setting) */
/* RF Controled */
struct ioctl_rf {
	u8 mode;
	u8 segments;		/* No. of segment , 1 or 3 or 13 */
	u8 ch_no;		/* UHF / VHF : channel no. */
	u8 RFAGC;		/* RFAGC gain state value. (0 - 127) */
	u8 GVBB;		/* BBAGC gain control. (0 - 255) */
	u8 LNA;			/* LNA GAIN. (0 - 3, LNA & 0x03) */
};
typedef struct ioctl_rf ioctl_rf_t;

/* I2C Controled */
struct ioctl_i2c {
	u8 mode;		/* send / recv mode */
	u8 address_sub;		/* SUBA address */
	u8 data_sub;		/* SUBD address */
	u8 address;		/* send / recv address */
	u8 data[4];		/* send / recv data */
};
typedef struct ioctl_i2c ioctl_i2c_t;

/* for MB86A35S	 */
/* SPI Controled */
struct ioctl_spi {
	u8 mode;		/* send / recv mode			*/
	u8 address_sub;		/* SUBA address				*/
	u8 data_sub;		/* SUBD address				*/
	u8 address;		/* send / recv address			*/
	u8 data[4];		/* send / recv data			*/
};
typedef struct ioctl_spi ioctl_spi_t;

/* HRM controled */
struct ioctl_hrm {
	u8 hrm;         /* HRM FLAG  */
};
typedef struct ioctl_hrm ioctl_hrm_t;

/* Sync and FEC(OFDM_INIT) */
struct ioctl_ofdm_init {
	u8 mode;
};
typedef struct ioctl_ofdm_init ioctl_ofdm_init_t;

/* LOW_UP_IF controled */
struct ioctl_low_up_if {
	u8 mode;         /* low_up_if mode  */
};
typedef struct ioctl_low_up_if ioctl_low_up_if_t;

/* Stream Read  */
struct ioctl_stream_read {
	u8 SBER;
	u8 NULLCTRL;
};
typedef struct ioctl_stream_read ioctl_stream_read_t;

/* Stream Read Control */
struct ioctl_stream_read_ctrl {
	u8  STREAM_CTRL;
	u16 READ_SIZE;
	u8 *BUF;
	u32 BUF_SIZE;
};
typedef struct ioctl_stream_read_ctrl ioctl_stream_read_ctrl_t;

/* SPI Config  */
struct ioctl_spi_config {
	u8 SPICTRL;
	u8 IOSEL3;
	u8 IO_OUTEN;
};
typedef struct ioctl_spi_config ioctl_spi_config_t;

/* TS Setup  */
struct ioctl_ts_setup {
	u8 PACBYTE;
	u8 ERRCTRL;
};
typedef struct ioctl_ts_setup ioctl_ts_setup_t;

/* Select Antenna */
struct ioctl_select_antenna {
	u8 mode;         	/* select antenna	*/
};
typedef struct ioctl_select_antenna ioctl_select_antenna_t;

#if 0//              
u32 inline GET32(u32 * merdata)
{
	u32 rtncode = 0;
	memcpy(&rtncode, merdata, sizeof(u32));
	return rtncode;
}
#endif
#endif /* __RADIO_MB86A35_H__ */
