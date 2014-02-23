/**
	@file	radio-mb86a35-dev.h \n
	multimedia tuner module device driver header file. \n
	This file is a header file for multimedia tuner module device driver users.
*/
/* COPYRIGHT FUJITSU SEMICONDUCTOR LIMITED 2011 */

#ifndef	__RADIO_MB86A35_DEV_H__
#define	__RADIO_MB86A35_DEV_H__

#include <linux/module.h>	/* module */
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spi.h>
//#include <linux/spi/spidev.h>
#include <linux/fs.h>		/* inode, file, file_operations, ... */
#ifndef  DEBUG_I386
#include <mach/hardware.h>
#endif
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/kernel.h>	/* printk */
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/sched.h>	/* task_struct */
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/delay.h>	/* sleep */

#include <asm/io.h>
#include <asm/uaccess.h>	/* copy_from_user */

#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/errno.h>

#include <media/v4l2-dev.h>	/*20110209 */

#include <linux/dma-mapping.h>
#include <linux/gpio.h>

/*20110209 <media/v4l2-dev.h>  VIDEO_MAJOR 81 */
#define	NODE_MAJOR		VIDEO_MAJOR	/* Major No. */
#define	NODE_MINOR			64	/* Minor No. */
//LG Character Device/Driver used

#include "radio-mb86a35.h"

//#define FPGA //for FPGA, not define LSI

//#define	PRINT			printk
//#define	PRINT(format, arg...)
//#define	PRINT_INFO		KERN_INFO
//#define	PRINT_ERROR		KERN_ERR
#define	PRINT_BASENAME		"radio-mb86a35"
#define	PRINT_LINE		__LINE__
#define	PRINT_FUNCTION		__FUNCTION__
#define	PRINT_LHEADERFMT	"%s[%d] %s "
#define	PRINT_LHEADER		PRINT_BASENAME,PRINT_LINE,PRINT_FUNCTION
#define PRINT_FORMAT		"%s[%d] %s [%05d] %s | %s\n"

#undef DEBUG

#ifdef	DEBUG
unsigned char STOPLOG = 0;

#define PRINT_DEBUG             KERN_DEBUG

#undef	DEBUG_PRINT_MEM
#endif

#ifdef	DEBUG_PRINT_MEM

unsigned char *LOGMEM = NULL;
#define	DEBUG_PRINT_MEM_SIZE		4096 * 4 + 128
int LOGCOPYSIZE = 0;

#define	INFOPRINT(format, arg...)

#ifdef	PRINT_INFO
#undef	INFOPRINT
#define INFOPRINT(format, arg...)	{\
		if(( LOGMEM != NULL ) && ( strlen( LOGMEM ) < ( DEBUG_PRINT_MEM_SIZE - 128 )))	\
			sprintf( &LOGMEM[strlen(LOGMEM)], " I:" format , ## arg );	\
		else	PRINT( PRINT_INFO format , ## arg );	\
	}
#endif

#define	ERRPRINT(format, arg...)

#ifdef	PRINT_ERROR
#undef	ERRPRINT
#define ERRPRINT(format, arg...)	{\
		if(( LOGMEM != NULL ) && ( strlen( LOGMEM ) < ( DEBUG_PRINT_MEM_SIZE - 128 )))	\
			sprintf( &LOGMEM[strlen(LOGMEM)], " E:" format , ## arg );	\
		else	PRINT( PRINT_ERROR format , ## arg );	\
	}
#endif

#define	DBGPRINT(format, arg...)

#ifdef	PRINT_DEBUG
#undef	DBGPRINT
#define DBGPRINT(format, arg...)	{\
		if(( LOGMEM != NULL ) && ( strlen( LOGMEM ) < ( DEBUG_PRINT_MEM_SIZE - 128 )))	\
			sprintf( &LOGMEM[strlen(LOGMEM)], " D:" format , ## arg );	\
		else	PRINT( PRINT_DEBUG format , ## arg );	\
	}
#endif

#else
#define	INFOPRINT(format, arg...)

#ifdef	PRINT_INFO
#undef	INFOPRINT
#define INFOPRINT(format, arg...)	\
		PRINT( PRINT_INFO format , ## arg )
#endif

#define	ERRPRINT(format, arg...)

#ifdef	PRINT_ERROR
#undef	ERRPRINT
#define ERRPRINT(format, arg...)	\
		PRINT( PRINT_ERROR format , ## arg )
#endif

#define	DBGPRINT(format, arg...)

#ifdef	PRINT_DEBUG
#undef	DBGPRINT
#define DBGPRINT(format, arg...)	\
		PRINT( PRINT_DEBUG format , ## arg )
#endif
#endif

/***************************************************************************************************/
/*  MB86A35S add define                                                                            */
/***************************************************************************************************/
#define MB86A35S_SPI_WRITE_HEADER_1BYTE		0x10
#define MB86A35S_SPI_WRITE_HEADER_3BYTE		0x20
#define MB86A35S_SPI_READ_HEADER_1BYTE		0x40
#define MB86A35S_SPI_READ_HEADER_3BYTE		0x80
#define MB86A35S_SPI_WRITE_1BYTE		0x01
#define MB86A35S_SPI_WRITE_2BYTE		0x02
#define MB86A35S_SPI_WRITE_3BYTE		0x03
#define MB86A35S_SPI_READ_1BYTE			0x01
#define MB86A35S_SPI_READ_2BYTE			0x02
#define MB86A35S_SPI_READ_3BYTE			0x03
#define MB86A35S_REG_ADDR_SPIF_EN		0x90
#define MB86A35S_SET_OFDN_EN			0x01
#define MB86A35S_SET_OFDN_DIS			0x00
#define MB86A35S_SET_RF_EN			0x02
#define MB86A35S_SET_RF_DIS			0x00

#define MB86A35S_IRQ_WAIT			5000
#define MB86A35S_IRQ_DELAY			1
#define MB86A35S_SPI_WAITTIME			200	/* micro sec	*/

#define MB86A35S_MAP_SIZE			(500 * 1024) /* 500KB	*/
#define MB86A35S_SPIS_160PAC			0x7F80 + 4

#define MB86A35S_SPIRF_FREQ			0x14

/* register define	*/
#define MB86A35S_REG_ADDR_SPICTRL		0x92
#define MB86A35S_REG_ADDR_STREAMREAD		0x94
#define MB86A35S_REG_ADDR_SPIRF_FREQ		0x98
#define MB86A35S_REG_ADDR_SPI_SUBA		0x9E
#define MB86A35S_REG_ADDR_SPI_SUBD		0x9F
#define MB86A35S_REG_ADDR_RFA			0xB0
#define MB86A35S_REG_ADDR_RFD			0xB1
#define MB86A35S_REG_ADDR_IOSEL3		0xF9
#define MB86A35S_REG_ADDR_IOOUTEN		0xFC

/* 0x50-0x5E	sub address	*/
#define	MB86A35_REG_SUBR_NULLCTRL		0xD0

/* 0x9E-0x9F	sub address	*/
#define	MB86A35S_REG_SUBR_PACBYTE		0x00
#define	MB86A35S_REG_SUBR_ERRCTRL		0x01
#define	MB86A35S_REG_SUBR_SPIS0_IRQMASK		0x1D
#define	MB86A35S_REG_SUBR_RAMDATAH		0x23
#define	MB86A35S_REG_SUBR_RAMDATAL		0x24

#ifndef FPGA
#define	MB86A35S_READ_SIZE_MAX			(160*204+3)
#else
#define	MB86A35S_READ_SIZE_MAX			(120*204+3)
#endif

#define GPIO_SPI_CLKI				54
#define GPIO_SPI_DO				52  // SPI_MISO Master Input Slave Output
#define GPIO_SPI_DI				51  // SPI MOSI Master Output Slave Input
#define GPIO_SPI_CSI				53
/*For LG Device*/
//#define GPIO_SPI_XIRQ				139
// kichul.park for Rev.D test
#define GPIO_SPI_XIRQ				77 // 16 
// #define GPIO_SPI_XIRQ				62


#define GPIO_SPI_TEST				138
#define GPIO_SPI_TEST2				133



#define	MB86A35_I2C_ADDRESS			0x10
#define	MB86A35_I2C_ADAPTER_ID			0	/* connected with I2C ch0 */
#define	MB86A35_I2C_RFICADDRESS			( 0xC2 >> 1 )

#define	MB86A35_SELECT_UHF			1
#define	MB86A35_SELECT_VHF			0

#define MB86A35_CN_MONI_WAITTIME(n)		( 1260 * ( 2 << ( n + 1 )))/1000 + 10

#define MB86A35_MER_MONI_WAITTIME(n)		( 1260 * ( 2 << n ))/1000 + 10

#define	MB86A35_RF_CHANNEL_SET_TIMEOUTCOUNT	3

/* ioctl() System Call Parameters */

static inline char SEGCNT_FROM_CHNO(int chno)
{
	/* TABLE : convert to SEGCNT from no.[0 - 41] */
	char SEGCNT_T[42]
	    = { 0x90, 0x90,	/*  0,  1     : 0x90 */
		0xA0, 0xA0, 0xA0,	/*  2,  3,  4 : 0xA0 */
		0xB0, 0xB0, 0xB0,	/*  5,  6,  7 : 0xB0 */
		0xC0, 0xC0, 0xC0,	/*  8,  9, 10 : 0xC0 */
		0xD0, 0xD0, 0xD0,	/* 11, 12, 13 : 0xD0 */
		0xE0, 0xE0, 0xE0,	/* 14, 15, 16 : 0xE0 */
		0xF0, 0xF0, 0xF0,	/* 17, 18, 19 : 0xF0 */
		0x00, 0x00, 0x00,	/* 20, 21, 22 : 0x00 */
		0x10, 0x10, 0x10,	/* 23, 24, 25 : 0x10 */
		0x20, 0x20, 0x20,	/* 26, 27, 28 : 0x20 */
		0x30, 0x30, 0x30,	/* 29, 30, 31 : 0x30 */
		0x40, 0x40, 0x40,	/* 32, 33, 34 : 0x40 */
		0x50, 0x50, 0x50,	/* 35, 36, 37 : 0x50 */
		0x60, 0x60, 0x60,	/* 38, 39, 40 : 0x60 */
		0x90		/* 41         : 0X90 */
	};

	if (chno > 41)
		return 0;
	return SEGCNT_T[chno];
}

									/* SEGCNT */
									/* ISDB-Tmm / Tsb */
#define	PARAM_FTSEGCNT_SEGCNT_ISDB_Tmm(n)		SEGCNT_FROM_CHNO(n)

/***************************************************************************************************/
/* MB86A35 Registers & Sub Registers & MASK Pattern                                                */
/***************************************************************************************************/

/*  I2C Master register address */
#define	MB86A35_REG_ADDR_REVISION		0x00		/* REVISION[7:0]  */	/* b'01000011' */
#define	MB86A35_REG_ADDR_IFAGC			0x03	/* IFAGC[7:0]     */
#define	MB86A35_REG_ADDR_SUBADR			0x04	/* SUBADR[7:0]    */
#define	MB86A35_REG_ADDR_SUBDAT			0x05	/* SUBADR[7:0]    */
#define	MB86A35_REG_ADDR_MODED_CTRL		0x06		/* MODED_CTRL     */	/* b'01110011' */
#define	MB86A35_REG_ADDR_MODED_STAT		0x07		/* MODED_STAT     */	/* b'---0----' */
#define	MB86A35_REG_ADDR_CONT_CTRL		0x08		/* CONT_CTRL      */	/* b'------00' */
#define	MB86A35_REG_ADDR_STATE_INIT		0x08		/* STATE_INIT     */	/* b'------00' */
#define	MB86A35_REG_ADDR_IQINV			0x09		/* IQINV          */	/* b'--111110' */
#define	MB86A35_REG_ADDR_SYNC_STATE		0x0A		/* SYNC_STATE     */	/* b'--------' */
#define	MB86A35_REG_ADDR_CONT_CTRL3		0x1C		/* CONT_CTRL3     */	/* b'------10' */
#define	MB86A35_REG_ADDR_MODED_CTRL2		0x1F		/* MODED_CTRL2    */	/* b'----0000' */
#define	MB86A35_REG_ADDR_SPFFT_LAYERSEL		0x39		/* SPFFT_LAYERSEL */	/* b'------01' */
#define	MB86A35_REG_ADDR_FTSEGSFT		0x44		/* FTSEGSFT       */	/* b'00000000' */
#define	MB86A35_REG_ADDR_CNCNT			0x45		/* CNCNT          */	/* b'--000100' */
#define	MB86A35_REG_ADDR_CNDATHI		0x46		/* DATA[15:8]     */	/* b'--------' */
#define	MB86A35_REG_ADDR_CNDATLO		0x47		/* DATA[7:0]      */	/* b'--------' */
#define	MB86A35_REG_ADDR_CNCNT2			0x48		/* CNCNT2         */	/* b'-----0--' */
#define	MB86A35_REG_ADDR_FEC_SUBA		0x50		/* FEC_SUBA       */	/* b'00000000' */
#define	MB86A35_REG_ADDR_FEC_SUBD		0x51		/* FEC_SUBD       */	/* b'00000000' */
#define	MB86A35_REG_ADDR_VBERON			0x52		/* VBERON         */	/* b'-------0' */
#define	MB86A35_REG_ADDR_VBERXRST		0x53		/* VBERXRST       */	/* b'-----000' */
#define	MB86A35_REG_ADDR_VBERXRST_AL		0x53		/* VBERXRST_AL    */	/* b'-----000' */
#define	MB86A35_REG_ADDR_VBERFLG		0x54		/* VBERFLG        */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERFLG_AL		0x54		/* VBERFLG_AL     */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTA0		0x55		/* VBERDTA[23:16] */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTA1		0x56		/* VBERDTA[15:8]  */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTA2		0x57		/* VBERDTA[7:0]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTB0		0x58		/* VBERDTB[23:16] */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTB1		0x59		/* VBERDTB[15:8]  */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTB2		0x5A		/* VBERDTB[7:0]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTC0		0x5B		/* VBERDTC[23:16] */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTC1		0x5C		/* VBERDTC[15:8]  */	/* b'--------' */
#define	MB86A35_REG_ADDR_VBERDTC2		0x5D		/* VBERDTC[7:0]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERON		0x5E		/* RSBERON        */	/* b'---0-000' */
#define	MB86A35_REG_ADDR_RSBERRST		0x5F		/* RSBERRST       */	/* b'-----000' */
#define	MB86A35_REG_ADDR_RSBERXRST		0x5F		/* RSBERXRST      */	/* b'-----000' */
#define	MB86A35_REG_ADDR_RSBERCEFLG		0x60		/* RSBERCEFLG     */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERTHFLG		0x61		/* RSBERTHFLG     */	/* b'-000----' */
#define	MB86A35_REG_ADDR_RSERRFLG		0x62		/* RSERRFLG       */	/* b'-000----' */
#define	MB86A35_REG_ADDR_FECIRQ1		0x63		/* FECIRQ1        */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTA0		0x64		/* SBERA[19:16]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTA1		0x65		/* SBERA[15:8]    */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTA2		0x66		/* SBERA[7:0]     */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTB0		0x67		/* SBERB[19:16]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTB1		0x68		/* SBERB[15:8]    */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTB2		0x69		/* SBERB[7:0]     */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTC0		0x6A		/* SBERC[19:16]   */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTC1		0x6B		/* SBERC[15:8]    */	/* b'--------' */
#define	MB86A35_REG_ADDR_RSBERDTC2		0x6C		/* SBERC[7:0]     */	/* b'--------' */
#define	MB86A35_REG_ADDR_TMCC_SUBA		0x6D		/* TMCC_SUBA      */	/* b'00000000' */
#define	MB86A35_REG_ADDR_TMCC_SUBD		0x6E		/* TMCC_SUBD      */	/* b'00000000' */
#define	MB86A35_REG_ADDR_FECIRQ2		0x6F		/* FECIRQ2        */	/* b'--------' */
#define	MB86A35_REG_ADDR_RST			0x70		/* RST            */	/* b'11111111' */
#define	MB86A35_REG_ADDR_SEGMENT		0x71		/* SEGMENT        */	/* b'00000000' */
#define	MB86A35_REG_ADDR_FPWDNMODE		0x72		/* FPWDNMODE      */	/* b'---0---0' */
#define	MB86A35_REG_ADDR_MACRO_PWDN		0xD0		/* MACRO_PWDN     */	/* b'--00--00' */
#define	MB86A35_REG_ADDR_DACOUT			0xD5		/* DACOUT[7:0]    */	/* b'00000000' */
#define	MB86A35_REG_ADDR_FRMLOCK_SEL		0xDD		/* FRMLOCK_SEL    */	/* b'-----000' */
#define	MB86A35_REG_ADDR_XIRQINV		0xDD		/* XIRQINV        */	/* b'-----000' */
#define	MB86A35_REG_ADDR_SEARCH_CTRL		0xE6		/* SEARCH_CTRL    */	/* b'--------' */
#define	MB86A35_REG_ADDR_SEARCH_CHANNEL		0xE7		/* SEARCH_CHANNEL */	/* b'--------' */
#define	MB86A35_REG_ADDR_SEARCH_SUBA		0xE8		/* SEARCH_SUBA    */	/* b'--------' */
#define	MB86A35_REG_ADDR_SEARCH_SUBD		0xE9		/* SEARCH_SUBD    */	/* b'--------' */
#define	MB86A35_REG_ADDR_GPIO_DAT		0xEB		/* GPIO_DAT       */	/* b'10--1111' */
#define	MB86A35_REG_ADDR_GPIO_OUTSEL		0xEC		/* GPIO_OUTSEL    */	/* b'11111111' */
#define	MB86A35_REG_ADDR_TUNER_IRQ		0xED		/* TUNER_IRQ      */	/* b'--------' */
#define	MB86A35_REG_ADDR_TUNER_IRQCTL		0xEE		/* TUNER_IRQCTL   */	/* b'--00--11' */

#define	MB86A35_REG_RFADDR_CHIPID1		0x00	/* RF Access */	/* RF CHIPID1 */
#define	MB86A35_REG_RFADDR_CHIPID0		0x01	/* RF CHIPID0 */
#define	MB86A35_REG_RFADDR_SPLITID		0x02	/* RF SPLITID */
#define	MB86A35_REG_RFADDR_RFAGC		0x03	/* RF RFAGC */
#define	MB86A35_REG_RFADDR_GVBB			0x04	/* RF GVBB */
#define	MB86A35_REG_RFADDR_LNAGAIN		0x05	/* RF LNAGAIN */

/*  I2C Slave register address */
#define	MB86A35_REG_SUBR_IFAH			0x00	/* SUBADR : SUBDAT *//* IFA[11:8]      */	/* b'----0000' */
#define	MB86A35_REG_SUBR_IFAL			0x01		/* IFA[7:0]       */	/* b'00110101' */
#define	MB86A35_REG_SUBR_IFBH			0x02		/* IFB[11:8]      */	/* b'----1110' */
#define	MB86A35_REG_SUBR_IFBL			0x03		/* IFB[7:0]       */	/* b'11011000' */
#define	MB86A35_REG_SUBR_DTS			0x08		/* DTS[4:0]       */	/* b'---01111' */
#define	MB86A35_REG_SUBR_IFAGCO			0x09		/* IFAGCO[7:0]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_MAXIFAGC		0x0A		/* MAXIFAGC[7:0]  */	/* b'11111111' */
#define	MB86A35_REG_SUBR_AGAIN			0x0B		/* AGAIN[7:0]     */	/* b'01111000' */
#define	MB86A35_REG_SUBR_VIFREFH		0x0E		/* VIFREF[11:8]   */	/* b'----0000' */
#define	MB86A35_REG_SUBR_VIFREFL		0x0F		/* VIFREF[7:0]    */	/* b'00001010' */
#define	MB86A35_REG_SUBR_VIFREF2H		0x10		/* VIFREF2[11:8]  */	/* b'----0001' */
#define	MB86A35_REG_SUBR_VIFREF2L		0x11		/* VIFREF2[7:0]   */	/* b'01100010' */
#define	MB86A35_REG_SUBR_LEVELTH		0x19		/* LEVELTH        */	/* b'????????' */	/*ADD:20110311 */
#define	MB86A35_REG_SUBR_IFSAMPLE		0x29		/* IFSAMPLE[7:0]  */	/* b'00000011' */
#define	MB86A35_REG_SUBR_OUTSAMPLE		0x32		/* OUTSAMPLE[7:0] */	/* b'00001010' */
#define	MB86A35_REG_SUBR_IFAGCDAC		0x3A		/* IFAGCDAC[7:0]  */	/* b'--------' */

#define	MB86A35_REG_SUBR_MERCTRL		0x50	/* FEC_SUBA : FEC_SUBD *//* MERCTRL     */	/* b'-----000' */
#define	MB86A35_REG_SUBR_MERSTEP		0x51		/* MERSTEP     */	/* b'-----111' */
#define	MB86A35_REG_SUBR_MERA0			0x52		/* MERA[23:16] */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERA1			0x53		/* MERA[15:8]  */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERA2			0x54		/* MERA[7:0]   */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERB0			0x55		/* MERB[23:16] */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERB1			0x56		/* MERB[15:8]  */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERB2			0x57		/* MERB[7:0]   */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERC0			0x58		/* MERC[23:16] */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERC1			0x59		/* MERC[15:8]  */	/* b'--------' */
#define	MB86A35_REG_SUBR_MERC2			0x5A		/* MERC[7:0]   */	/* b'--------' */
#define	MB86A35_REG_SUBR_MEREND			0x5B		/* MEREND      */	/* b'--------' */
#define	MB86A35_REG_SUBR_VBERSETA0		0xA7		/* VBERSNUMA[23:16]  */	/* b'00001000' */
#define	MB86A35_REG_SUBR_VBERSETA1		0xA8		/* VBERSNUMA[15:8]   */	/* b'00000000' */
#define	MB86A35_REG_SUBR_VBERSETA2		0xA9		/* VBERSNUMA[7:0]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_VBERSETB0		0xAA		/* VBERSNUMB[23:16]  */	/* b'00100000' */
#define	MB86A35_REG_SUBR_VBERSETB1		0xAB		/* VBERSNUMB[15:8]   */	/* b'00000000' */
#define	MB86A35_REG_SUBR_VBERSETB2		0xAC		/* VBERSNUMB[7:0]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_VBERSETC0		0xAD		/* VBERSNUMC[23:16]  */	/* b'00100000' */
#define	MB86A35_REG_SUBR_VBERSETC1		0xAE		/* VBERSNUMC[15:8]   */	/* b'00000000' */
#define	MB86A35_REG_SUBR_VBERSETC2		0xAF		/* VBERSNUMC[7:0]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_PEREN			0xB0		/* PEREN       */	/* b'-----000' */
#define	MB86A35_REG_SUBR_PERRST			0xB1		/* PERRST      */	/* b'-----000' */
#define	MB86A35_REG_SUBR_PERSNUMA0		0xB2		/* PERSNUMA[15:8]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_PERSNUMA1		0xB3		/* PERSNUMA[7:0]     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_PERSNUMB0		0xB4		/* PERSNUMB[15:8]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_PERSNUMB1		0xB5		/* PERSNUMB[7:0]     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_PERSNUMC0		0xB6		/* PERSNUMC[15:8]    */	/* b'00000000' */
#define	MB86A35_REG_SUBR_PERSNUMC1		0xB7		/* PERSNUMC[7:0]     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_PERFLG			0xB8		/* PERFLG      */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRA0		0xB9		/* PERERRA[15:8]    */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRA1		0xBA		/* PERERRA[7:0]     */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRB0		0xBB		/* PERERRB[15:8]    */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRB1		0xBC		/* PERERRB[7:0]     */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRC0		0xBD		/* PERERRC[15:8]    */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRC1		0xBE		/* PERERRC[7:0]     */	/* b'--------' */
#define	MB86A35_REG_SUBR_PERERRFLG		0xB8		/* PERFLG      */	/* b'--------' */
#define	MB86A35_REG_SUBR_TSMASK1		0xCC		/* TSMASK2     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_TSOUT2			0xCD		/* TSOUT2      */	/* b'00000---' */
#define	MB86A35_REG_SUBR_PBER2			0xCE		/* PBER2       */	/* b'---11001' */
#define	MB86A35_REG_SUBR_SBER2			0xCF		/* SBER2       */	/* b'10111001' */
#define	MB86A35_REG_SUBR_RS0			0xD1		/* RS0         */	/* b'--100010' */
#define	MB86A35_REG_SUBR_TSMASK			0xD4		/* TSMASK      */	/* b'00000000' */
#define	MB86A35_REG_SUBR_TSOUT			0xD5		/* TSOUT       */	/* b'00000101' */
#define	MB86A35_REG_SUBR_PBER			0xD6		/* PBER        */	/* b'---1111' */
#define	MB86A35_REG_SUBR_SBER			0xD7		/* SBER        */	/* b'10111111' */
#define	MB86A35_REG_SUBR_STSSEL			0xD8		/* STSSEL      */	/* b'-------0' */
#define	MB86A35_REG_SUBR_TSERR			0xD9		/* TSERR       */	/* b'--------' */
#define	MB86A35_REG_SUBR_IRQMASK		0xDA		/* IRQMASK     */	/* b'1-111111' */
#define	MB86A35_REG_SUBR_IRQMASK2		0xDB		/* IRQMASK2    */	/* b'-----111' */
#define	MB86A35_REG_SUBR_SBERSETA0		0xDC		/* SBERSETA[15:8]    */	/* b'00011111' */
#define	MB86A35_REG_SUBR_SBERSETA1		0xDD		/* SBERSETA[7:0]     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_SBERSETB0		0xDE		/* SBERSETB[15:8]    */	/* b'00011111' */
#define	MB86A35_REG_SUBR_SBERSETB1		0xDF		/* SBERSETB[7:0]     */	/* b'11111111' */
#define	MB86A35_REG_SUBR_SBERSETC0		0xE0		/* SBERSETC[15:8]    */	/* b'00011111' */
#define	MB86A35_REG_SUBR_SBERSETC1		0xE1		/* SBERSETC[7:0]     */	/* b'11111111' */

#define	MB86A35_REG_SUBR_TMCC_IRQ_MASK		0x35	/* TMCC_SUBA : TMCC_SUBD *//* TMCC_IRQ_MASK  */	/* b'-----111' */
#define	MB86A35_REG_SUBR_TMCC_IRQ_RST		0x36		/* TMCC_IRQ_RST    */	/* b'-----111' */
#define	MB86A35_REG_SUBR_TMCC_IRQ_EMG_INV	0x38		/* EMG_INV    */	/* b'---00001' */
#define	MB86A35_REG_SUBR_S8WAIT			0x71		/* S8WAIT     */	/* b'01100101' */
#define	MB86A35_REG_SUBR_TMCC0			0x80	/* B[0]       */
#define	MB86A35_REG_SUBR_TMCC1			0x81	/* B[9:16]    */
#define	MB86A35_REG_SUBR_TMCC2			0x82	/* B[1:8]     */
#define	MB86A35_REG_SUBR_TMCC3			0x83	/* B[17:19]   */
#define	MB86A35_REG_SUBR_TMCC4			0x84	/* B[20:26]   */
#define	MB86A35_REG_SUBR_TMCC5			0x85	/* B[27] & B[67] */
#define	MB86A35_REG_SUBR_TMCC6			0x86	/* B[28:30] & B[68:70] */
#define	MB86A35_REG_SUBR_TMCC7			0x87	/* B[31:33] & B[71:73] */
#define	MB86A35_REG_SUBR_TMCC8			0x88	/* B[34:36] & B[74:76] */
#define	MB86A35_REG_SUBR_TMCC9			0x89	/* B[37:40] & B[77:80] */
#define	MB86A35_REG_SUBR_TMCC10			0x8A	/* B[41:43] & B[81:83] */
#define	MB86A35_REG_SUBR_TMCC11			0x8B	/* B[44:46] & B[84:86] */
#define	MB86A35_REG_SUBR_TMCC12			0x8C	/* B[47:49] & B[87:89] */
#define	MB86A35_REG_SUBR_TMCC13			0x8D	/* B[50:53] & B[90:93] */
#define	MB86A35_REG_SUBR_TMCC14			0x8E	/* B[54:56] & B[94:96] */
#define	MB86A35_REG_SUBR_TMCC15			0x8F	/* B[57:59] & B[97:99] */
#define	MB86A35_REG_SUBR_TMCC16			0x90	/* B[60:62] & B[100:102] */
#define	MB86A35_REG_SUBR_TMCC17			0x91	/* B[63:66] & B[103:106] */
#define	MB86A35_REG_SUBR_TMCC18			0x92	/* B[107:109] */
#define	MB86A35_REG_SUBR_TMCC19			0x93	/* B[110:117] */
#define	MB86A35_REG_SUBR_TMCC20			0x94	/* B[118:121] */
#define	MB86A35_REG_SUBR_TMCC21			0x95	/* B[122:129] */
#define	MB86A35_REG_SUBR_TMCC22			0x96	/* B[130:137] */
#define	MB86A35_REG_SUBR_TMCC23			0x97	/* B[138:145] */
#define	MB86A35_REG_SUBR_TMCC24			0x98	/* B[146:153] */
#define	MB86A35_REG_SUBR_TMCC25			0x99	/* B[154:161] */
#define	MB86A35_REG_SUBR_TMCC26			0x9A	/* B[162:169] */
#define	MB86A35_REG_SUBR_TMCC27			0x9B	/* B[170:177] */
#define	MB86A35_REG_SUBR_TMCC28			0x9C	/* B[178:185] */
#define	MB86A35_REG_SUBR_TMCC29			0x9D	/* B[186:193] */
#define	MB86A35_REG_SUBR_TMCC30			0x9E	/* B[194:201] */
#define	MB86A35_REG_SUBR_TMCC31			0x9F	/* B[202:203] */
#define	MB86A35_REG_SUBR_FEC_IN			0xA0		/* FEC_IN         */	/* b'--------' */
#define	MB86A35_REG_SUBR_TMCCREAD		0xA1		/* TMCCREAD       */	/* b'-------0' */
#define	MB86A35_REG_SUBR_SBER_IRQ_MASK		0xDA		/* SBER_IRQ_MASK  */	/* b'1-111111' */
#define	MB86A35_REG_SUBR_TSERR_IRQ_MASK		0xDB		/* TSERR_IRQ_MASK  */	/* b'-----111' */
#define	MB86A35_REG_SUBR_XIRQINV		0xDD		/* XIRQINV         */	/* b'-----100' */
#define	MB86A35_REG_SUBR_PCHKOUT0		0xF0		/* PCHKOUT0        */	/* b'-0000000' */
#define	MB86A35_REG_SUBR_PCHKOUT1		0xF1		/* PCHKOUT1        */	/* b'00000000' */
#define	MB86A35_REG_SUBR_TMCCCHK_HI		0xFA		/* TMCCCHK_HI      */	/* b'-0000000' */
#define	MB86A35_REG_SUBR_TMCCCHK_LO		0xFB		/* TMCCCHK_LO      */	/* b'00000000' */
#define	MB86A35_REG_SUBR_TMCCCHK2_HI		0xF8		/* TMCCCHK_HI      */	/* b'-0000000' */
#define	MB86A35_REG_SUBR_TMCCCHK2_LO		0xF9		/* TMCCCHK_LO      */	/* b'00000000' */

#define	MB86A35_REG_SUBR_CHANNEL0		0x00	/* SEARCH_SUBA : SEARCH_SUBD */	/* CH8  - CH1     */
#define	MB86A35_REG_SUBR_CHANNEL1		0x01	/* CH16 - CH9     */
#define	MB86A35_REG_SUBR_CHANNEL2		0x02	/* CH24 - CH17    */
#define	MB86A35_REG_SUBR_CHANNEL3		0x03	/* CH32 - CH25    */
#define	MB86A35_REG_SUBR_CHANNEL4		0x04	/* CH40 - CH33    */
#define	MB86A35_REG_SUBR_CHANNEL5		0x05	/* CH48 - CH41    */
#define	MB86A35_REG_SUBR_CHANNEL6		0x06	/* CH56 - CH49    */
#define	MB86A35_REG_SUBR_CHANNEL7		0x07	/* CH64 - CH57    */
#define	MB86A35_REG_SUBR_CHANNEL8		0x10	/* CH72 - CH65    */
#define	MB86A35_REG_SUBR_SEARCH_END		0x08	/* SEARCH_END     */

/* register constant data */
#define	MB86A35_REG_DATA_REVISION		0x43			/* REVISION[7:0] */	/* b'01000011' */

#define	MB86A35_MASK_MODED_CTRL_M3G32		0x80	/* MODED_CTRL */	/* bit 7 : M3G32 */
#define	MB86A35_MASK_MODED_CTRL_M3G16		0x40	/* MODED_CTRL */	/* bit 6 : M3G16 */
#define	MB86A35_MASK_MODED_CTRL_M3G8		0x20	/* MODED_CTRL */	/* bit 5 : M3G8  */
#define	MB86A35_MASK_MODED_CTRL_M3G4		0x10	/* MODED_CTRL */	/* bit 4 : M3G4  */
#define	MB86A35_MASK_MODED_CTRL_M2G32		0x08	/* MODED_CTRL */	/* bit 3 : M2G32 */
#define	MB86A35_MASK_MODED_CTRL_M2G16		0x04	/* MODED_CTRL */	/* bit 2 : M2G16 */
#define	MB86A35_MASK_MODED_CTRL_M2G8		0x02	/* MODED_CTRL */	/* bit 1 : M2G8  */
#define	MB86A35_MASK_MODED_CTRL_M2G4		0x01	/* MODED_CTRL */	/* bit 0 : M2G4  */

#define	MB86A35_MASK_MODED_STAT_MDFAIL		0x40	/* MODED_STAT */	/* bit 6 : MDFAIL      */
#define	MB86A35_MASK_MODED_STAT_MDDETECT	0x20	/* MODED_STAT */	/* bit 5 : MDDETECT    */
#define	MB86A35_MASK_MODED_STAT_MDFIX		0x10	/* MODED_STAT */	/* bit 4 : MDFIX       */
#define	MB86A35_MASK_MODED_STAT_MODE		0x0C	/* MODED_STAT */	/* bit 3-2 : MODE[1:0]   */
#define	MB86A35_MASK_MODED_STAT_GUARDE		0x03	/* MODED_STAT */	/* bit 1-0 : GUARDE[1:0] */

#define	MB86A35_MASK_CONT_CTRL_MDTEST		0x02	/* CONT_CTRL */	/* bit 1 : MDTEST    */
#define	MB86A35_MASK_CONT_CTRL_STATEINIT	0x01	/* CONT_CTRL */	/* bit 0 : STATEINIT */

#define	MB86A35_MASK_STATE_INIT_INIT		0x01	/* STATE_INIT */	/* bit 0 : INIT */

#define	MB86A35_MASK_IQINV			0x04	/* IQINV */	/* bit 2 : IQINV */

#define	MB86A35_MASK_STAT			0x07	/* SYNC_STATE */	/* bit 3-0 : STAT[3:0] */

#define	MB86A35_MASK_CONT_CTRL3_IQSEL		0x02	/* CONT_CTRL3 */	/* bit 1 : IQSEL     */
#define	MB86A35_MASK_CONT_CTRL3_IFSEL		0x01	/* CONT_CTRL3 */	/* bit 0 : IFSEL     */
#define	MB86A35_MASK_CONT_CTRL3			( MB86A35_MASK_CONT_CTRL3_IQSEL | MB86A35_MASK_CONT_CTRL3_IFSEL )

#define	MB86A35_MASK_MODED_CTRL2_M1G32		0x08	/* MODED_CTRL2 */	/* bit 3 : M1G32 */
#define	MB86A35_MASK_MODED_CTRL2_M1G16		0x04	/* MODED_CTRL2 */	/* bit 2 : M1G16 */
#define	MB86A35_MASK_MODED_CTRL2_M1G8		0x02	/* MODED_CTRL2 */	/* bit 1 : M1G8  */
#define	MB86A35_MASK_MODED_CTRL2_M1G4		0x01	/* MODED_CTRL2 */	/* bit 0 : M1G4  */

#define	MB86A35_MASK_LAYERSEL			0x03	/* SPFFT_LAYERSEL */	/* bit 1-0 : LAYERSEL[1:0] */

#define	MB86A35_MASK_SEGCNT			0xF0	/* FTSEGSFT */	/* bit 7-4 : SEGCNT[3:0]  */

#define	MB86A35_CNCNT_FLG			0x40	/* CNCNT */	/* bit 6 : FLG  */
#define	MB86A35_CNCNT_LOCK			0x20	/* CNCNT */	/* bit 5 : LOCK */
#define	MB86A35_CNCNT_RST			0x10	/* CNCNT */	/* bit 4 : RST  */
#define	MB86A35_MASK_CNCNT_SYMCOUNT		0x0F	/* CNCNT */	/* bit 3-0 : SYMCOUNT[3:0] */

#define	MB86A35_MODE_AUTO			0x00	/* CNCNT2 */	/* bit 2 : MODE */
#define	MB86A35_MODE_MUNL			0x04	/* CNCNT2 */	/* bit 2 : MODE */
#define	MB86A35_MASK_MODE			0x04	/* CNCNT2 */	/* bit 2 : MODE */

#define	MB86A35_MERCTRL_LOCK			0x04	/* MERCTRL */	/* bit 2 : LOCK  */
#define	MB86A35_MERCTRL_MODE			0x02	/* MERCTRL */	/* bit 1 : MODE  */
#define	MB86A35_MERCTRL_RST			0x01	/* MERCTRL */	/* bit 0 : RST   */

#define	MB86A35_MASK_MERSTEP_SYMCOUNT		0x07	/* MERSTEP */	/* bit 2-0 : MERSTEP  */

#define	MB86A35_MEREND_FLG			0x01	/* MEREND */	/* bit 0 : FLG    */

#define	MB86A35_MASK_PEREN_PERENC		0x04	/* PEREN */	/* bit 2 : PERENC  */
#define	MB86A35_MASK_PEREN_PERENB		0x02	/* PEREN */	/* bit 1 : PERENB  */
#define	MB86A35_MASK_PEREN_PERENA		0x01	/* PEREN */	/* bit 0 : PERENA  */

#define	MB86A35_MASK_PERRST_PERRSTC		0x04	/* PERRST */	/* bit 2 : PERRSTC  */
#define	MB86A35_MASK_PERRST_PERRSTB		0x02	/* PERRST */	/* bit 1 : PERRSTB  */
#define	MB86A35_MASK_PERRST_PERRSTA		0x01	/* PERRST */	/* bit 0 : PERRSTA  */

#define	MB86A35_MASK_PERFLG_PERFLGC		0x04	/* PERFLG */	/* bit 2 : PERFLGC  */
#define	MB86A35_MASK_PERFLG_PERFLGB		0x02	/* PERFLG */	/* bit 1 : PERFLGB  */
#define	MB86A35_MASK_PERFLG_PERFLGA		0x01	/* PERFLG */	/* bit 0 : PERFLGA  */

#define	MB86A35_MASK_TSMASK1_TSERRMASK		0x10	/* TSMASK2 */	/* bit 4 : TSERRMASK */
#define	MB86A35_MASK_TSMASK1_TSPACMASK		0x08	/* TSMASK2 */	/* bit 3 : TSPACMASK */
#define	MB86A35_MASK_TSMASK1_TSENMASK		0x04	/* TSMASK2 */	/* bit 2 : TSENMASK  */
#define	MB86A35_MASK_TSMASK1_TSDTMASK		0x02	/* TSMASK2 */	/* bit 1 : TSDTMASK  */
#define	MB86A35_MASK_TSMASK1_TSCLKMASK		0x01	/* TSMASK2 */	/* bit 0 : TSCLKMASK */

#define	MB86A35_MASK_TSOUT2_TSERRINV		0x80	/* TSOUT2 */	/* bit 7 : TSERRINV  */
#define	MB86A35_MASK_TSOUT2_TSENINV		0x40	/* TSOUT2 */	/* bit 6 : TSENINV   */
#define	MB86A35_MASK_TSOUT2_TSSINV		0x20	/* TSOUT2 */	/* bit 5 : TSSINV    */

#define	MB86A35_MASK_PBER2_PLAER		0x07	/* PBER2 */	/* bit 2-0 : PLAER[2:0] */

#define	MB86A35_MASK_SBER2_SCLKSEL		0x80	/* SBER2 */	/* bit 7 : SCLKSEL   */
#define	MB86A35_MASK_SBER2_TSCLKCTRL		0x40	/* SBER2 */	/* bit 6 : TSCLKCTRL */
#define	MB86A35_MASK_SBER2_SBERSEL		0x20	/* SBER2 */	/* bit 5 : SBERSEL   */
#define	MB86A35_MASK_SBER2_SPACON		0x10	/* SBER2 */	/* bit 4 : SPACON    */
#define	MB86A35_MASK_SBER2_SENON		0x08	/* SBER2 */	/* bit 3 : SENON     */
#define	MB86A35_MASK_SBER2_SLAYER		0x07	/* SBER2 */	/* bit 2-0 : SLAYER[2:0] */

#define	MB86A35_MASK_RS0_ERID			0x04	/* RS0 */	/* bit 2 : ERID      */
#define	MB86A35_MASK_RS0_RSEN			0x02	/* RS0 */	/* bit 1 : RSEN      */

#define	MB86A35_MASK_TSMASK_TSFRMMASK		0x80	/* TSMASK */	/* bit 7 : TSFRMMASK */
#define	MB86A35_MASK_TSMASK_TSRSMASK		0x40	/* TSMASK */	/* bit 6 : TSRSMASK  */
#define	MB86A35_MASK_TSMASK_TSLAMASK		0x20	/* TSMASK */	/* bit 5 : TSLAMASK  */
#define	MB86A35_MASK_TSMASK_TSERRMASK		0x10	/* TSMASK */	/* bit 4 : TSERRMASK */
#define	MB86A35_MASK_TSMASK_TSPACMASK		0x08	/* TSMASK */	/* bit 3 : TSPACMASK */
#define	MB86A35_MASK_TSMASK_TSENMASK		0x04	/* TSMASK */	/* bit 2 : TSENMASK  */
#define	MB86A35_MASK_TSMASK_TSDTMASK		0x02	/* TSMASK */	/* bit 1 : TSDTMASK  */
#define	MB86A35_MASK_TSMASK_TSCLKMASK		0x01	/* TSMASK */	/* bit 0 : TSCLKMASK */

#define	MB86A35_MASK_TSOUT_TSERRINV		0x80	/* TSOUT */	/* bit 7 : TSERRINV   */
#define	MB86A35_MASK_TSOUT_TSENINV		0x40	/* TSOUT */	/* bit 6 : TSENINV    */
#define	MB86A35_MASK_TSOUT_TSSINV		0x20	/* TSOUT */	/* bit 5 : TSSINV     */
#define	MB86A35_MASK_TSOUT_TSPINV		0x10	/* TSOUT */	/* bit 4 : TSPINV     */
#define	MB86A35_MASK_TSOUT_TSRSINV		0x08	/* TSOUT */	/* bit 3 : TSRSINV    */
#define	MB86A35_MASK_TSOUT_TSERRMASK2		0x04	/* TSOUT */	/* bit 2 : TSERRMASK2 */
#define	MB86A35_MASK_TSOUT_TSERRMASK		0x02	/* TSOUT */	/* bit 1 : TSERRMASK  */

#define	MB86A35_MASK_PBER_PPACON		0x10	/* PBER */	/* bit 4 : PPACON     */
#define	MB86A35_MASK_PBER_PENON			0x08	/* PBER */	/* bit 3 : PENON      */
#define	MB86A35_MASK_PBER_PLAER			0x07	/* PBER */	/* bit 2-0 : PLAER[2:0] */

#define	MB86A35_MASK_SBER_SCLKSEL		0x80	/* SBER */	/* bit 7 : SCLKSEL    */
#define	MB86A35_MASK_SBER_TSCLKCTRL		0x40	/* SBER */	/* bit 6 : TSCLKCTRL  */
#define	MB86A35_MASK_SBER_SBERSEL		0x20	/* SBER */	/* bit 5 : SBERSEL    */
#define	MB86A35_MASK_SBER_SPACON		0x10	/* SBER */	/* bit 4 : SPACON     */
#define	MB86A35_MASK_SBER_SENON			0x08	/* SBER */	/* bit 3 : SEON       */
#define	MB86A35_MASK_SBER_SLAYER		0x07	/* SBER */	/* bit 2-0 : SLAYER[2:0] */

#define	MB86A35_MASK_STSSEL_STSSEL		0x01	/* STSSEL */	/* bit 0 : STSSEL     */

#define	MB86A35_MASK_TSERR_TSERR2		0x02	/* TSERR  */	/* bit 1 : TSERR2     */
#define	MB86A35_MASK_TSERR_TSERR1		0x01	/* TSERR  */	/* bit 0 : TSERR1     */

#define	MB86A35_MASK_IRQMASK_DTERIRQ		0x80	/* IRQMASK */	/* bit 7 : DTERIRQ    */
#define	MB86A35_MASK_IRQMASK_THMASKC		0x20	/* IRQMASK */	/* bit 5 : THMASKC    */
#define	MB86A35_MASK_IRQMASK_THMASKB		0x10	/* IRQMASK */	/* bit 4 : THMASKB    */
#define	MB86A35_MASK_IRQMASK_THMASKA		0x08	/* IRQMASK */	/* bit 3 : THMASKA    */
#define	MB86A35_MASK_IRQMASK_CEMASKC		0x04	/* IRQMASK */	/* bit 2 : CEMASKC    */
#define	MB86A35_MASK_IRQMASK_CEMASKB		0x02	/* IRQMASK */	/* bit 1 : CEMASKB    */
#define	MB86A35_MASK_IRQMASK_CEMASKA		0x01	/* IRQMASK */	/* bit 0 : CEMASKA    */

#define	MB86A35_MASK_IRQMASK2_ERRMASKC		0x04	/* IRQMASK2 */	/* bit 2 : ERRMASKC   */
#define	MB86A35_MASK_IRQMASK2_ERRMASKB		0x02	/* IRQMASK2 */	/* bit 1 : ERRMASKB   */
#define	MB86A35_MASK_IRQMASK2_ERRMASKA		0x01	/* IRQMASK2 */	/* bit 0 : ERRMASKA   */

#define	MB86A35_MASK_VBERON_VBERON		0x01	/* VBERON */	/* bit 0 : VBERON     */

#define	MB86A35_MASK_VBERXRST_AL_VBERXRSTC	0x04	/* VBERXRST */	/* bit 2 : VBERXRSTC  */
#define	MB86A35_MASK_VBERXRST_AL_VBERXRSTB	0x02	/* VBERXRST */	/* bit 1 : VBERXRSTB  */
#define	MB86A35_MASK_VBERXRST_AL_VBERXRSTA	0x01	/* VBERXRST */	/* bit 0 : VBERXRSTA  */

#define	MB86A35_MASK_VBERFLG_AL_VBERXRSTC	0x04	/* VBERFLG_AL */	/* bit 2 : VBERFLGC   */
#define	MB86A35_MASK_VBERFLG_AL_VBERXRSTB	0x02	/* VBERFLG_AL */	/* bit 1 : VBERFLGB   */
#define	MB86A35_MASK_VBERFLG_AL_VBERXRSTA	0x01	/* VBERFLG_AL */	/* bit 0 : VBERFLGA   */

#define	MB86A35_MASK_RSBERON_RSBERAUTO		0x10	/* RSBERON */	/* bit 4 : RSBERAUTO  */
#define	MB86A35_MASK_RSBERON_RSBERC		0x04	/* RSBERON */	/* bit 2 : RSBERC     */
#define	MB86A35_MASK_RSBERON_RSBERB		0x02	/* RSBERON */	/* bit 1 : RSBERB     */
#define	MB86A35_MASK_RSBERON_RSBERA		0x01	/* RSBERON */	/* bit 0 : RSBERA     */

#define	MB86A35_MASK_RSBERXRST_SBERXRSTC	0x04	/* RSBERXRST */	/* bit 2 : SBERXRSTC  */
#define	MB86A35_MASK_RSBERXRST_SBERXRSTB	0x02	/* RSBERXRST */	/* bit 1 : SBERXRSTB  */
#define	MB86A35_MASK_RSBERXRST_SBERXRSTA	0x01	/* RSBERXRST */	/* bit 0 : SBERXRSTA  */

#define	MB86A35_MASK_RSBERCEFLG_SBERCEFC	0x04	/* RSBERCEFLG */	/* bit 2 : SBERCEFC   */
#define	MB86A35_MASK_RSBERCEFLG_SBERCEFB	0x02	/* RSBERCEFLG */	/* bit 1 : SBERCEFB   */
#define	MB86A35_MASK_RSBERCEFLG_SBERCEFA	0x01	/* RSBERCEFLG */	/* bit 0 : SBERCEFA   */

#define	MB86A35_MASK_RSBERTHFLG_SBERTHRC	0x40	/* RSBERTHFLG */	/* bit 6 : SBERTHRC   */
#define	MB86A35_MASK_RSBERTHFLG_SBERTHRB	0x20	/* RSBERTHFLG */	/* bit 5 : SBERTHRB   */
#define	MB86A35_MASK_RSBERTHFLG_SBERTHRA	0x10	/* RSBERTHFLG */	/* bit 4 : SBERTHRA   */
#define	MB86A35_MASK_RSBERTHFLG_SBERTHFC	0x04	/* RSBERTHFLG */	/* bit 2 : SBERTHFC   */
#define	MB86A35_MASK_RSBERTHFLG_SBERTHFB	0x02	/* RSBERTHFLG */	/* bit 1 : SBERTHFB   */
#define	MB86A35_MASK_RSBERTHFLG_SBERTHFA	0x01	/* RSBERTHFLG */	/* bit 0 : SBERTHFA   */

#define	MB86A35_MASK_RSERRFLG_RSERRSTC		0x40	/* RSERRFLG */	/* bit 6 : RSERRSTC   */
#define	MB86A35_MASK_RSERRFLG_RSERRSTB		0x20	/* RSERRFLG */	/* bit 5 : RSERRSTB   */
#define	MB86A35_MASK_RSERRFLG_RSERRSTA		0x10	/* RSERRFLG */	/* bit 4 : RSERRSTA   */
#define	MB86A35_MASK_RSERRFLG_RSERRC		0x04	/* RSERRFLG */	/* bit 2 : RSERRC     */
#define	MB86A35_MASK_RSERRFLG_RSERRB		0x02	/* RSERRFLG */	/* bit 1 : RSERRB     */
#define	MB86A35_MASK_RSERRFLG_RSERRA		0x01	/* RSERRFLG */	/* bit 0 : RSERRA     */

#define	MB86A35_MASK_FECIRQ1_TSERRC		0x40	/* FECIRQ1 */	/* bit 6 : TSERRC     */
#define	MB86A35_MASK_FECIRQ1_TSERRB		0x20	/* FECIRQ1 */	/* bit 5 : TSERRB     */
#define	MB86A35_MASK_FECIRQ1_TSERRA		0x10	/* FECIRQ1 */	/* bit 4 : TSERRA     */
//#define MB86A35_MASK_FECIRQ1_LOCK             0x08    /* FECIRQ1 *//* bit 3 : LOCK       */
#define	MB86A35_MASK_FECIRQ1_EMG		0x04	/* FECIRQ1 */	/* bit 2 : EMG        */
#define	MB86A35_MASK_FECIRQ1_CNT		0x02	/* FECIRQ1 */	/* bit 1 : CNT        */
#define	MB86A35_MASK_FECIRQ1_ILL		0x01	/* FECIRQ1 */	/* bit 0 : ILL        */

#define	MB86A35_MASK_S8WAIT_TSWAIT		0x80	/* S8WAIT */	/* bit 7 : TSWAIT     */
#define	MB86A35_MASK_S8WAIT_BERRST		0x40	/* S8WAIT */	/* bit 6 : BERRST     */
#define	MB86A35_MASK_S8WAIT			( MB86A35_MASK_S8WAIT_TSWAIT | MB86A35_MASK_S8WAIT_BERRST )

#define	MB86A35_MASK_FEC_IN_CORRECT		0x02	/* FEC_IN */	/* bit 1 : CORRECT    */
#define	MB86A35_MASK_FEC_IN_VALID		0x01	/* FEC_IN */	/* bit 0 : VALID      */

#define	MB86A35_MASK_TMCCREAD_TMCCLOCK		0x01	/* TMCCREAD */	/* bit 0 : TMCCLOCK   */

//#define MB86A35_MASK_FECIRQ2_AC_RENEW         0x80    /* FECIRQ2 *//* bit 7 : AC_RENEW   */
#define	MB86A35_MASK_FECIRQ2_SBERCEFC		0x40	/* FECIRQ2 */	/* bit 6 : SBERCEFC   */
#define	MB86A35_MASK_FECIRQ2_SBERCEFB		0x20	/* FECIRQ2 */	/* bit 5 : SBERCEFB   */
#define	MB86A35_MASK_FECIRQ2_SBERCEFA		0x10	/* FECIRQ2 */	/* bit 4 : SBERCEFA   */
//#define MB86A35_MASK_FECIRQ2_AC_EMG           0x08    /* FECIRQ2 *//* bit 3 : AC_EMG     */
#define	MB86A35_MASK_FECIRQ2_SBERTHFC		0x04	/* FECIRQ2 */	/* bit 2 : SBERTHFC   */
#define	MB86A35_MASK_FECIRQ2_SBERTHFB		0x02	/* FECIRQ2 */	/* bit 1 : SBERTHFB   */
#define	MB86A35_MASK_FECIRQ2_SBERTHFA		0x01	/* FECIRQ2 */	/* bit 0 : SBERTHFA   */

#define	MB86A35_MASK_RST_I2CREG_RESET		0xF0	/* RST */	/* bit 7-4 : I2CREG_RESET */
#define	MB86A35_MASK_RST_LOGIC_RESET		0x0F	/* RST */	/* bit 3-0 : LOGIC_RESET */

#define	MB86A35_MASK_SEGMENT_BANDSEL		0xE0	/* SEGMENT */	/* bit 7-5 : BANDSEL   */
#define	MB86A35_MASK_SEGMENT_TMMSEL		0x04	/* SEGMENT */	/* bit 2 : TMMSEL      */
#define	MB86A35_MASK_SEGMENT_MYTYPESEL		0x03	/* SEGMENT */	/* bit 1-0 : MYTYPESEL */

#define	MB86A35_MASK_FPWDNMODE_TSSEL_1S		0x01	/* FPWDNMODE */	/* bit 0 : TSSEL_1S    */

#define	MB86A35_MASK_MACRO_PWDN_DACPWDN		0x20	/* MACRO_PWDN */	/* bit 5 : DACPWDN     */

#define	MB86A35_MASK_FRMLOCK_SEL_XIROINV	0x04	/* XIRQINV */	/* bit 2 : XIROINV    */

#define	MB86A35_SEARCH_CTRL_SEARCH_AFT		0x20	/* SEARCH_CTRL */	/* bit 5 : SEARCH_AFT */
#define	MB86A35_SEARCH_CTRL_SEARCH_RST		0x10	/* SEARCH_CTRL */	/* bit 4 : SEARCH_RST */
#define	MB86A35_SEARCH_CTRL_SEARCH		0x01	/* SEARCH_CTRL */	/* bit 4 : SEARCH     */

#define	MB86A35_MASK_GPIO_DAT_AC_MODE		0x40	/* GPIO_DAT */	/* bit 6 : AC_MODE    */
#define	MB86A35_MASK_GPIO_DAT_GPIO_DAT		0x0F	/* GPIO_DAT */	/* bit 3-0 : GPIO_DAT */

#define	MB86A35_MASK_GPIO_OUTSEL_GPIO_IRQOUT	0xF0	/* GPIO_OUTSEL */	/* bit 7-4 : GPIO_IRQOUT */
#define	MB86A35_MASK_GPIO_OUTSEL_GPIO_OUTEN	0x0F	/* GPIO_OUTSEL */	/* bit 3-0 : GPIO_OUTEN  */

#define	MB86A35_TUNER_IRQ_GPIO			0x02	/* TUNER_IRQ */	/* bit 1 : GPIO    */
#define	MB86A35_TUNER_IRQ_CHEND			0x01	/* TUNER_IRQ */	/* bit 0 : CHEND   */

#define	MB86A35_TUNER_IRQCTL_GPIO_RST		0x20	/* TUNER_IRQCTL */	/* bit 5 : GPIO_RST   */
#define	MB86A35_TUNER_IRQCTL_CHEND_RST		0x10	/* TUNER_IRQCTL */	/* bit 4 : CHEND_RST  */
#define	MB86A35_TUNER_IRQCTL_GPIOMASK		0x02	/* TUNER_IRQCTL */	/* bit 1 : GPIOMASK   */
#define	MB86A35_TUNER_IRQCTL_CHENDMASK		0x01	/* TUNER_IRQCTL */	/* bit 0 : CHENDMASK  */

#define	MB86A35_REG_ADDR_SUBSELECT		0xFE	/* RF TUTHREW */	/* RF bypass mode */

#define	MB86A35_MASK_LNAGAIN_LNA		0x03	/* RF LNAGAIN */	/* RSSI LNA mask */

/***************************************************************************************************/

#define	MB86A35_I2CMASK_STATE_INIT		0x02
#define	MB86A35_I2CMASK_SEGMENT			0x00
#define	MB86A35_I2CMASK_LAYERSEL		0x00
#define	MB86A35_I2CMASK_IQINV			0x3B
#define	MB86A35_I2CMASK_CONT_CTRL3		0x00
#define	MB86A35_I2CMASK_DACPWDN			0x00
#define	MB86A35_I2CMASK_FTSEGCNT		0x00
#define	MB86A35_I2CMASK_SUB_IFAH		0x00
#define	MB86A35_I2CMASK_SUB_IFAL		0x00
#define	MB86A35_I2CMASK_SUB_IFBH		0x00
#define	MB86A35_I2CMASK_SUB_IFBL		0x00
#define	MB86A35_I2CMASK_SUB_DTS			0x1F
#define	MB86A35_I2CMASK_SUB_IFAGCO		0x00
#define	MB86A35_I2CMASK_SUB_MAXIFAGC		0xFF
#define	MB86A35_I2CMASK_SUB_AGAIN		0xFF
#define	MB86A35_I2CMASK_SUB_VIFREFH		0x0F
#define	MB86A35_I2CMASK_SUB_VIFREFL		0xFF
#define	MB86A35_I2CMASK_SUB_VIFREF2H		0x0F
#define	MB86A35_I2CMASK_SUB_VIFREF2L		0xFF
#define	MB86A35_I2CMASK_SUB_IFSAMPLE		0xFF
#define	MB86A35_I2CMASK_SUB_OUTSAMPLE		0xFF
#define	MB86A35_I2CMASK_GPIO_DAT		0x80
#define	MB86A35_I2CMASK_GPIO_DAT_ACMODE		0x8F
#define	MB86A35_I2CMASK_TMCC_SUB_S8WAIT		0x3F
#define	MB86A35_I2CMASK_RS0			0x3D
#define	MB86A35_I2CMASK_TSOUT			0x01
#define	MB86A35_I2CMASK_TSOUT2			0x00
#define	MB86A35_I2CMASK_PBER			0x18
#define	MB86A35_I2CMASK_TSMASK0			0x60
#define	MB86A35_I2CMASK_TSMASK1			0x60
#define	MB86A35_I2CMASK_TMCC_IRQ_MASK		0xF8
#define	MB86A35_I2CMASK_SBER_IRQ_MASK		0x40
#define	MB86A35_I2CMASK_TSERR_IRQ_MASK		0x00
#define	MB86A35_I2CMASK_TMCC_IRQ_RST		0x00
#define	MB86A35_I2CMASK_EMG_INV			0x01

/***************************************************************************************************/

struct MB86A35_setting_data {
	unsigned char address;	/* Address */
	unsigned char data;	/* Data */
};
typedef struct MB86A35_setting_data MB86A35_setting_data_t;

/* when UHF Received */
typedef struct MB86A35_setting_data MB86A35_RF_init_UHF_t;

// ES2.0 ES2.5
MB86A35_RF_init_UHF_t RF_INIT_UHF_DATA[] = {
	{0x22, 0xb0}, {0x23, 0x28}, {0x24, 0x88},
	{0x25, 0xbb}, {0x26, 0xcc}, {0x27, 0x2b},
	{0x28, 0xd0}, {0x29, 0xd7}, {0x2a, 0x92},
	{0x2b, 0x28}, {0x2e, 0x06}, {0x2f, 0x00},
	{0x30, 0x04}, {0x31, 0x3f}, {0x32, 0xff},
	{0x33, 0x30}, {0x34, 0x00}, {0x35, 0x00},
	{0x36, 0x40}, {0x37, 0x88}, {0x38, 0x88},
	{0x39, 0xf6}, {0x3a, 0x02}, {0x3b, 0x18},
	{0x3c, 0x11}, {0x3d, 0x38}, {0x3e, 0x1b},
	{0x3f, 0x44}, {0x40, 0x3a}, {0x41, 0x0f},
	{0x42, 0x1d}, {0x43, 0x1c}, {0x44, 0x38},
	{0x45, 0x19}, {0x46, 0x31}, {0x47, 0x62},
	{0x48, 0x38}, {0x49, 0x6f}, {0x4a, 0x62},
	{0x4b, 0x50}, {0x4c, 0x00}, {0x4d, 0x9b},
	{0x4e, 0x3b}, {0x4f, 0x3b}, {0x50, 0x3b},
	{0x51, 0x33}, {0x52, 0x40}, {0x53, 0xe7},
	{0x54, 0x01}, {0x55, 0x8b}, {0x56, 0x80},
	{0x57, 0x8f}, {0x58, 0x3f}, {0x59, 0x23},
	{0x5a, 0x08}, {0x5b, 0x30}, {0x5c, 0x55},
	{0x5d, 0x33}, {0x5e, 0x38}, {0x5f, 0x08},
	{0x60, 0xbb}, {0x61, 0x33}, {0x62, 0x0e},
	{0x63, 0x65}, {0x64, 0x07}, {0x65, 0x2F},
	{0x66, 0xcc}, {0x67, 0x80}, {0x68, 0x60},
	{0x69, 0x1f}, {0x6a, 0x11}, {0x6b, 0x5f},
	{0x6c, 0x00}, {0x6d, 0x2b}, {0x6e, 0x2b},
	{0x6f, 0x2b}, {0x70, 0x55}, {0x71, 0x59},
	{0x72, 0x5e}, {0x73, 0x40}, {0x74, 0x49},
	{0x75, 0x4a}, {0x76, 0x39}, {0x77, 0x39},
	{0x78, 0x39}, {0x79, 0x2f}, {0x7a, 0x28},
	{0x7b, 0x80}, {0x7c, 0x86}, {0x7d, 0x5e},
	{0x7e, 0x5e}, {0x7f, 0x00}, {0x80, 0x23},
	{0x81, 0x3c}, {0x82, 0x1e}, {0x83, 0xe4},
	{0x84, 0x88}, {0x85, 0x80}, {0x86, 0xff},
	{0x87, 0x7f}, {0x88, 0x00}, {0x89, 0x3c},
	{0x8a, 0x7f}, {0x8b, 0x20}, {0x8c, 0x0d},
	{0x8d, 0x00}, {0x8e, 0x60}, {0x8f, 0x56},
	{0x90, 0x50}, {0x91, 0x56}, {0x92, 0x12},
	{0x93, 0x40},
	{0xff, 0xff}
};

/* when VHF Received */
MB86A35_RF_init_UHF_t RF_INIT_VHF_DATA[] = {
	{0x22, 0x30}, {0x23, 0x28}, {0x24, 0x88},
	{0x25, 0xe8}, {0x26, 0xcc}, {0x27, 0x2b},
	{0x28, 0xd0}, {0x29, 0xd7}, {0x2a, 0x92},
	{0x2b, 0x28}, {0x2c, 0x00}, {0x2d, 0x00},
	{0x2e, 0x06}, {0x2f, 0x00}, {0x30, 0x04},
	{0x31, 0x3f}, {0x32, 0xff}, {0x33, 0x30},
	{0x34, 0x00}, {0x35, 0x00}, {0x36, 0x40},
	{0x37, 0x88}, {0x38, 0x88}, {0x39, 0xf6},
	{0x3a, 0x02}, {0x3b, 0x18}, {0x3c, 0x11},
	{0x3d, 0x38}, {0x3e, 0x1b}, {0x3f, 0x44},
	{0x40, 0x3a}, {0x41, 0x0f}, {0x42, 0x1d},
	{0x43, 0x1c}, {0x44, 0x38}, {0x45, 0x19},
	{0x46, 0x31}, {0x47, 0x62}, {0x48, 0x38},
	{0x49, 0x6f}, {0x4a, 0x62}, {0x4b, 0x50},
	{0x4c, 0x00}, {0x4d, 0xcb}, {0x4e, 0x3b},
	{0x4f, 0xbf}, {0x50, 0x3f}, {0x51, 0x33},
	{0x52, 0x40}, {0x53, 0xe7}, {0x54, 0x01},
	{0x55, 0x8b}, {0x56, 0x80}, {0x57, 0x8f},
	{0x58, 0x3f}, {0x59, 0x23}, {0x5a, 0x08},
	{0x5b, 0x30}, {0x5c, 0x55}, {0x5d, 0x33},
	{0x5e, 0x38}, {0x5f, 0x08}, {0x60, 0x88},
	{0x61, 0x03}, {0x62, 0x08}, {0x63, 0x85},
	{0x64, 0x07}, {0x65, 0x2f}, {0x66, 0xcc},
	{0x67, 0x80}, {0x68, 0x60}, {0x69, 0x1f},
	{0x6a, 0x11}, {0x6b, 0x5f}, {0x6c, 0x00},
	{0x6d, 0x2b}, {0x6e, 0x2b}, {0x6f, 0x2b},
	{0x70, 0x55}, {0x71, 0x59}, {0x72, 0x5e},
	{0x73, 0x40}, {0x74, 0x49}, {0x75, 0x4a},
	{0x76, 0x39}, {0x77, 0x39}, {0x78, 0x39},
	{0x79, 0x2f}, {0x7a, 0x28}, {0x7b, 0x80},
	{0x7c, 0x86}, {0x7d, 0x5e}, {0x7e, 0x5e},
	{0x7f, 0x00}, {0x80, 0x23}, {0x81, 0x3c},
	{0x82, 0x1e}, {0x83, 0xe4}, {0x84, 0x88},
	{0x85, 0x80}, {0x86, 0xff}, {0x87, 0x7f},
	{0x88, 0x00}, {0x89, 0x3c}, {0x8a, 0x7f},
	{0x8b, 0x20}, {0x8c, 0x0d}, {0x8d, 0x08},
	{0x8e, 0x60}, {0x8f, 0x56}, {0x90, 0x50},
	{0x91, 0x56}, {0x92, 0x12}, {0x93, 0x40},
	{0xff, 0xff}
};

// ES3.0
MB86A35_RF_init_UHF_t RF_INIT_UHF_DATA1[] = {
	{0x22, 0x80}, {0x23, 0x18}, 
	{0x25, 0xaa}, 
	
	{0x2b, 0x08},               {0x2f, 0x01},
	{0x30, 0x2e}, 
	              {0x34, 0x7f}, {0x35, 0x00},
	{0x36, 0x00}, 
	
	                            {0x3e, 0xff},
	{0x3f, 0x41}, {0x40, 0x41}, {0x41, 0x41},
	
	
	
	                            {0x4d, 0x8c},
	{0x4e, 0x08}, {0x4f, 0x8b}, {0x50, 0x09},
	{0x51, 0xc9}, {0x52, 0x97}, 
	{0x54, 0x41}, {0x55, 0x06}, {0x56, 0x00},
	              {0x58, 0xdf}, 
	{0x5a, 0x09},               {0x5c, 0x42},
	{0x5d, 0x1c}, {0x5e, 0x2a}, {0x5f, 0x22},
	{0x60, 0x49}, {0x61, 0x50}, {0x62, 0x96},
	{0x63, 0x96},               {0x65, 0xff},
	{0x66, 0xff},               {0x68, 0x90},
	{0x69, 0x90}, {0x6a, 0x81}, {0x6b, 0x42},
	{0x6c, 0x42}, 
	
	{0x72, 0xa0}, 
	{0x75, 0x07}, 
	{0x78, 0x20}, {0x79, 0xff}, {0x7a, 0xf0},
	{0x7b, 0xc0}, {0x7c, 0xf0}, 
	              {0x7f, 0xf6}, 
	
	
	{0x87, 0xc0}, {0x88, 0x20}, 
	              {0x8b, 0x13}, 
	{0x8d, 0xe8}, 
	              {0x91, 0x3a}, 
	
	{0xff, 0xff}
};

/* when VHF Received */
MB86A35_RF_init_UHF_t RF_INIT_VHF_DATA1[] = {
	{0x22, 0x00}, {0x23, 0x18}, 
	{0x25, 0xaa}, 
	
	{0x2b, 0x08}, 
	              {0x2f, 0x01}, {0x30, 0x2e},
	
	{0x34, 0x10}, {0x35, 0x3f}, {0x36, 0x00},
	                            
	
	              {0x3e, 0xe0}, {0x3f, 0x41},
	{0x40, 0x41}, {0x41, 0x41}, 
	
	
	
	              {0x4d, 0xcc}, {0x4e, 0x10},
	{0x4f, 0x8b}, {0x50, 0x00}, {0x51, 0x98},
	{0x52, 0x97},               {0x54, 0x00},
	{0x55, 0x07}, {0x56, 0x00}, 
	{0x58, 0xdf},               {0x5a, 0x06},
	              {0x5c, 0x42}, {0x5d, 0x1c},
	{0x5e, 0x2a}, {0x5f, 0x22}, {0x60, 0x49},
	{0x61, 0x63}, {0x62, 0x63}, {0x63, 0x63},
	              {0x65, 0xff}, {0x66, 0xff},
	              {0x68, 0x90}, {0x69, 0x90},
	{0x6a, 0x81}, {0x6b, 0x42}, {0x6c, 0x42},
	
	                            {0x72, 0x80},
	                            {0x75, 0x00},
	                            {0x78, 0x20},
	{0x79, 0xff}, {0x7a, 0xf0}, {0x7b, 0xc0},
	{0x7c, 0xf0}, 
	{0x7f, 0xff}, 
	
	                            {0x87, 0xc0},
	{0x88, 0x20}, 
	{0x8b, 0x13},               {0x8d, 0xe8},
	
	{0x91, 0x3a}, 
	{0xff, 0xff}
};


// ES3.0 CS2
MB86A35_RF_init_UHF_t RF_INIT_UHF_DATA2[] = {
	{0x22, 0x80}, {0x23, 0x18}, 
	{0x25, 0xaa}, 
	
	{0x2b, 0x08},               {0x2f, 0x01},
	{0x30, 0x2e}, 
	              {0x34, 0x7f}, {0x35, 0x00},
	{0x36, 0x00}, 
	
	                            {0x3e, 0xff},
        {0x3f, 0x41}, {0x40, 0x41}, {0x41, 0x41},
	
	
	
	
	                            {0x4d, 0x8c},
	{0x4e, 0x08}, {0x4f, 0x8b}, {0x50, 0x09},
	{0x51, 0xc9}, {0x52, 0x92}, 
	{0x54, 0x41}, {0x55, 0x08}, {0x56, 0x00},
	              {0x58, 0xdf}, 
	{0x5a, 0x04},               {0x5c, 0x02},
	{0x5d, 0x1c}, {0x5e, 0x2a}, {0x5f, 0x32},
	{0x60, 0x49}, {0x61, 0x50}, {0x62, 0x51},
	{0x63, 0xd4},               {0x65, 0xff},
	{0x66, 0xff},               {0x68, 0x90},
	{0x69, 0x88}, {0x6a, 0x78}, {0x6b, 0x01},
	{0x6c, 0x21}, 
	
	{0x72, 0xa0}, 
	{0x75, 0x07}, 
	{0x78, 0x20}, {0x79, 0xff}, {0x7a, 0xf0},
	{0x7b, 0xb0}, {0x7c, 0xf0}, {0x7d, 0x26},
	{0x7e, 0x26}, {0x7f, 0xf6}, 
	
	
	{0x87, 0xc0}, {0x88, 0x20}, 
	              {0x8b, 0x13}, 
	{0x8d, 0xe8}, 
	              {0x91, 0x32}, 
	
	{0xff, 0xff}
};

// when VHF Received
MB86A35_RF_init_UHF_t RF_INIT_VHF_DATA2[] = {
	{0x22, 0x00}, {0x23, 0x18}, 
	{0x25, 0xaa}, 
	
	{0x2b, 0x08}, 
	              {0x2f, 0x01}, {0x30, 0x2e},
	
	{0x34, 0x1a}, {0x35, 0xff}, {0x36, 0x5f},
	                            
	
	              {0x3e, 0xe0}, {0x3f, 0x41},
        {0x40, 0x41}, {0x41, 0x41},	
	
	
	
	              {0x4d, 0xcc}, {0x4e, 0x20},
	{0x4f, 0x8b}, {0x50, 0x00}, {0x51, 0x98},
	{0x52, 0x92},               {0x54, 0x60},
	{0x55, 0x08}, {0x56, 0x00}, 
	{0x58, 0xdf},               {0x5a, 0x04},
	              {0x5c, 0x02}, {0x5d, 0x1c},
	{0x5e, 0x2a}, {0x5f, 0x32}, {0x60, 0x49},
	{0x61, 0x63}, {0x62, 0x51}, {0x63, 0xd4},
	              {0x65, 0xff}, {0x66, 0xff},
	              {0x68, 0x90}, {0x69, 0x88},
	{0x6a, 0x78}, {0x6b, 0x01}, {0x6c, 0x20},
	
	                            {0x72, 0x80},
	                            {0x75, 0x00},
	                            {0x78, 0x20},
	{0x79, 0xff}, {0x7a, 0xf0}, {0x7b, 0xb0},
	{0x7c, 0xf0}, {0x7d, 0x26}, {0x7e, 0x26},
	{0x7f, 0xff}, 
	
	                            {0x87, 0xc0},
	{0x88, 0x20}, 
	{0x8b, 0x13},               {0x8d, 0xe8},
	
	{0x91, 0x32}, 
	{0xff, 0xff}
};





/* A35_FREQ table(ES2) */
struct mb86a35_freq {
	u8 CH;
	//float         FREQ(MHz);
	//float         PLLN;
	u8 PLLN;
	u32 PLLF;
	//u8            REG28_X;
	u8 REG28;
	//u8            REG29_X;
	u8 REG29;
	//u8            REG2A_X;
	u8 REG2A;
};
typedef struct mb86a35_freq mb86a35_freq_t;

mb86a35_freq_t mb86a35_freq_VHF[] = {
	/*          CH ,     FREQ(MHz),        PLLN,   PLLN,    PLLF,  REG28, REG28,  REG29, REG29, REG2A,  REG2A  */
	{ /*Cvh */ 1, /*207.8571429, 25.98214286, */ 25, 0xFB6DB, /*0xfb, */
	 251, /* 0x6d, */ 109, /* 0xb1, */ 177},
	{ /*Cvh */ 2, /*208.2857143, 26.03571429, */ 26, 0x9249, /*0x9 , */ 9,
	 /* 0x24, */ 36, /* 0x91, */ 145},
	{ /*Cvh */ 3, /*208.7142857, 26.08928571, */ 26, 0x16DB6, /*0x16, */ 22,
	 /* 0xdb, */ 219, /* 0x61, */ 97},
	{ /*Cvh */ 4, /*209.1428571, 26.14285714, */ 26, 0x24924, /*0x24, */ 36,
	 /* 0x92, */ 146, /* 0x41, */ 65},
	{ /*Cvh */ 5, /*209.5714286, 26.19642857, */ 26, 0x32492, /*0x32, */ 50,
	 /* 0x49, */ 73, /* 0x21, */ 33},
	{ /*Cvh */ 6, /*210        , 26.25      , */ 26, 0x40000, /*0x40, */ 64,
	 /* 0x0 , */ 0, /* 0x1 , */ 1},
	{ /*Cvh */ 7, /*210.4285714, 26.30357143, */ 26, 0x4DB6D, /*0x4d, */ 77,
	 /* 0xb6, */ 182, /* 0xd1, */ 209},
	{ /*Cvh */ 8, /*210.8571429, 26.35714286, */ 26, 0x5B6DB, /*0x5b, */ 91,
	 /* 0x6d, */ 109, /* 0xb1, */ 177},
	{ /*Cvh */ 9, /*211.2857143, 26.41071429, */ 26, 0x69249, /*0x69, */
	 105, /* 0x24, */ 36, /* 0x91, */ 145},
	{ /*Cvh */ 10, /*211.7142857, 26.46428571, */ 26, 0x76DB6, /*0x76, */
	 118, /* 0xdb, */ 219, /* 0x61, */ 97},
	{ /*Cvh */ 11, /*212.1428571, 26.51785714, */ 26, 0x84924, /*0x84, */
	 132, /* 0x92, */ 146, /* 0x41, */ 65},
	{ /*Cvh */ 12, /*212.5714286, 26.57142857, */ 26, 0x92492, /*0x92, */
	 146, /* 0x49, */ 73, /* 0x21, */ 33},
	{ /*Cvh */ 13, /*213        , 26.625     , */ 26, 0xA0000, /*0xa0, */
	 160, /* 0x0 , */ 0, /* 0x1 , */ 1},
	{ /*Cvh */ 14, /*213.4285714, 26.67857143, */ 26, 0xADB6D, /*0xad, */
	 173, /* 0xb6, */ 182, /* 0xd1, */ 209},
	{ /*Cvh */ 15, /*213.8571429, 26.73214286, */ 26, 0xBB6DB, /*0xbb, */
	 187, /* 0x6d, */ 109, /* 0xb1, */ 177},
	{ /*Cvh */ 16, /*214.2857143, 26.78571429, */ 26, 0xC9249, /*0xc9, */
	 201, /* 0x24, */ 36, /* 0x91, */ 145},
	{ /*Cvh */ 17, /*214.7142857, 26.83928571, */ 26, 0xD6DB6, /*0xd6, */
	 214, /* 0xdb, */ 219, /* 0x61, */ 97},
	{ /*Cvh */ 18, /*215.1428571, 26.89285714, */ 26, 0xE4924, /*0xe4, */
	 228, /* 0x92, */ 146, /* 0x41, */ 65},
	{ /*Cvh */ 19, /*215.5714286, 26.94642857, */ 26, 0xF2492, /*0xf2, */
	 242, /* 0x49, */ 73, /* 0x21, */ 33},
	{ /*Cvh */ 20, /*216        , 27         , */ 27, 0x0, /*0x0 , */ 0,
	 /* 0x0 , */ 0, /* 0x1 , */ 1},
	{ /*Cvh */ 21, /*216.4285714, 27.05357143, */ 27, 0xDB6D, /*0x0d, */ 13,
	 /* 0xb6, */ 182, /* 0xd1, */ 209},
	{ /*Cvh */ 22, /*216.8571429, 27.10714286, */ 27, 0x1B6DB, /*0x1b, */
	 27, /* 0x6d, */ 109, /* 0xb1, */ 177},
	{ /*Cvh */ 23, /*217.2857143, 27.16071429, */ 27, 0x29249, /*0x29, */
	 41, /* 0x24, */ 36, /* 0x91, */ 145},
	{ /*Cvh */ 24, /*217.7142857, 27.21428571, */ 27, 0x36DB6, /*0x36, */
	 54, /* 0xdb, */ 219, /* 0x61, */ 97},
	{ /*Cvh */ 25, /*218.1428571, 27.26785714, */ 27, 0x44924, /*0x44, */
	 68, /* 0x92, */ 146, /* 0x41, */ 65},
	{ /*Cvh */ 26, /*218.5714286, 27.32142857, */ 27, 0x52492, /*0x52, */
	 82, /* 0x49, */ 73, /* 0x21, */ 33},
	{ /*Cvh */ 27, /*219        , 27.375     , */ 27, 0x60000, /*0x60, */
	 96, /* 0x0 , */ 0, /* 0x1 , */ 1},
	{ /*Cvh */ 28, /*219.4285714, 27.42857143, */ 27, 0x6DB6D, /*0x6d, */
	 109, /* 0xb6, */ 182, /* 0xd1, */ 209},
	{ /*Cvh */ 29, /*219.8571429, 27.48214286, */ 27, 0x7B6DB, /*0x7b, */
	 123, /* 0x6d, */ 109, /* 0xb1, */ 177},
	{ /*Cvh */ 30, /*220.2857143, 27.53571429, */ 27, 0x89249, /*0x89, */
	 137, /* 0x24, */ 36, /* 0x91, */ 145},
	{ /*Cvh */ 31, /*220.7142857, 27.58928571, */ 27, 0x96DB6, /*0x96, */
	 150, /* 0xdb, */ 219, /* 0x61, */ 97},
	{ /*Cvh */ 32, /*221.1428571, 27.64285714, */ 27, 0xA4924, /*0xa4, */
	 164, /* 0x92, */ 146, /* 0x41, */ 65},
	{ /*Cvh */ 33, /*221.5714286, 27.69642857, */ 27, 0xB2492, /*0xb2, */
	 178, /* 0x49, */ 73, /* 0x21, */ 33},
	{ /*Cvh */ 0xFF, /*999.9999999, 99.99999999, */ 99, 0xFFFFF, /*0xFF, */
	 255, /* 0xFF, */ 255, /* 0xFF, */ 255}
};

mb86a35_freq_t mb86a35_freq_UHF[] = {
	/*           CH,     FREQ(MHz),        PLLN,   PLLN,    PLLF,  REG28, REG28,  REG29, REG29,  REG2A,  REG2A  */
	{13, /*    473.143, 29.57142857, */ 29, 0x92492, /*0x92, */ 146,
	 /*0x49, */ 73, /*0x22, */ 34},
	{14, /*    479.143, 29.94642857, */ 29, 0xF2492, /*0xF2, */ 242,
	 /*0x49, */ 73, /*0x22, */ 34},
	{15, /*    485.143 ,30.32142857, */ 30, 0x52492, /*0x52, */ 82,
	 /*0x49, */ 73, /*0x22, */ 34},
	{16, /*    491.143 ,30.69642857, */ 30, 0xB2492, /*0xB2, */ 178,
	 /*0x49, */ 73, /*0x22, */ 34},
	{17, /*    497.143 ,31.07142857, */ 31, 0x12492, /*0x12, */ 18,
	 /*0x49, */ 73, /*0x22, */ 34},
	{18, /*    503.143 ,31.44642857, */ 31, 0x72492, /*0x72, */ 114,
	 /*0x49, */ 73, /*0x22, */ 34},
	{19, /*    509.143 ,31.82142857, */ 31, 0xD2492, /*0xD2, */ 210,
	 /*0x49, */ 73, /*0x22, */ 34},
	{20, /*    515.143 ,32.19642857, */ 32, 0x32492, /*0x32, */ 50,
	 /*0x49, */ 73, /*0x22, */ 34},
	{21, /*    521.143 ,32.57142857, */ 32, 0x92492, /*0x92, */ 146,
	 /*0x49, */ 73, /*0x22, */ 34},
	{22, /*    527.143 ,32.94642857, */ 32, 0xF2492, /*0xF2, */ 242,
	 /*0x49, */ 73, /*0x22, */ 34},
	{23, /*    533.143 ,33.32142857, */ 33, 0x52492, /*0x52, */ 82,
	 /*0x49, */ 73, /*0x22, */ 34},
	{24, /*    539.143 ,33.69642857, */ 33, 0xB2492, /*0xB2, */ 178,
	 /*0x49, */ 73, /*0x22, */ 34},
	{25, /*    545.143 ,34.07142857, */ 34, 0x12492, /*0x12, */ 18,
	 /*0x49, */ 73, /*0x22, */ 34},
	{26, /*    551.143 ,34.44642857, */ 34, 0x72492, /*0x72, */ 114,
	 /*0x49, */ 73, /*0x22, */ 34},
	{27, /*    557.143 ,34.82142857, */ 34, 0xD2492, /*0xD2, */ 210,
	 /*0x49, */ 73, /*0x22, */ 34},
	{28, /*    563.143 ,35.19642857, */ 35, 0x32492, /*0x32, */ 50,
	 /*0x49, */ 73, /*0x22, */ 34},
	{29, /*    569.143 ,35.57142857, */ 35, 0x92492, /*0x92, */ 146,
	 /*0x49, */ 73, /*0x22, */ 34},
	{30, /*    575.143 ,35.94642857, */ 35, 0xF2492, /*0xF2, */ 242,
	 /*0x49, */ 73, /*0x22, */ 34},
	{31, /*    581.143 ,36.32142857, */ 36, 0x52492, /*0x52, */ 82,
	 /*0x49, */ 73, /*0x22, */ 34},
	{32, /*    587.143 ,36.69642857, */ 36, 0xB2492, /*0xB2, */ 178,
	 /*0x49, */ 73, /*0x22, */ 34},
	{33, /*    593.143 ,37.07142857, */ 37, 0x12492, /*0x12, */ 18,
	 /*0x49, */ 73, /*0x22, */ 34},
	{34, /*    599.143 ,37.44642857, */ 37, 0x72492, /*0x72, */ 114,
	 /*0x49, */ 73, /*0x22, */ 34},
	{35, /*    605.143 ,37.82142857, */ 37, 0xD2492, /*0xD2, */ 210,
	 /*0x49, */ 73, /*0x22, */ 34},
	{36, /*    611.143 ,38.19642857, */ 38, 0x32492, /*0x32, */ 50,
	 /*0x49, */ 73, /*0x22, */ 34},
	{37, /*    617.143 ,38.57142857, */ 38, 0x92492, /*0x92, */ 146,
	 /*0x49, */ 73, /*0x22, */ 34},
	{38, /*    623.143 ,38.94642857, */ 38, 0xF2492, /*0xF2, */ 242,
	 /*0x49, */ 73, /*0x22, */ 34},
	{39, /*    629.143 ,39.32142857, */ 39, 0x52492, /*0x52, */ 82,
	 /*0x49, */ 73, /*0x22, */ 34},
	{40, /*    635.143 ,39.69642857, */ 39, 0xB2492, /*0xB2, */ 178,
	 /*0x49, */ 73, /*0x22, */ 34},
	{41, /*    641.143 ,20.03571429, */ 20, 0x9249, /*0x9 , */ 9, /*0x24, */
	 36, /*0x91, */ 145},
	{42, /*    647.143 ,20.22321429, */ 20, 0x39249, /*0x39, */ 57,
	 /*0x24, */ 36, /*0x91, */ 145},
	{43, /*    653.143 ,20.41071429, */ 20, 0x69249, /*0x69, */ 105,
	 /*0x24, */ 36, /*0x91, */ 145},
	{44, /*    659.143 ,20.59821429, */ 20, 0x99249, /*0x99, */ 153,
	 /*0x24, */ 36, /*0x91, */ 145},
	{45, /*    665.143 ,20.78571429, */ 20, 0xC9249, /*0xC9, */ 201,
	 /*0x24, */ 36, /*0x91, */ 145},
	{46, /*    671.143 ,20.97321429, */ 20, 0xF9249, /*0xF9, */ 249,
	 /*0x24, */ 36, /*0x91, */ 145},
	{47, /*    677.143 ,21.16071429, */ 21, 0x29249, /*0x29, */ 41,
	 /*0x24, */ 36, /*0x91, */ 145},
	{48, /*    683.143 ,21.34821429, */ 21, 0x59249, /*0x59, */ 89,
	 /*0x24, */ 36, /*0x91, */ 145},
	{49, /*    689.143 ,21.53571429, */ 21, 0x89249, /*0x89, */ 137,
	 /*0x24, */ 36, /*0x91, */ 145},
	{50, /*    695.143 ,21.72321429, */ 21, 0xB9249, /*0xB9, */ 185,
	 /*0x24, */ 36, /*0x91, */ 145},
	{51, /*    701.143 ,21.91071429, */ 21, 0xE9249, /*0xE9, */ 233,
	 /*0x24, */ 36, /*0x91, */ 145},
	{52, /*    707.143 ,22.09821429, */ 22, 0x19249, /*0x19, */ 25,
	 /*0x24, */ 36, /*0x91, */ 145},
	{0xFF, /*999.9999999, 99.99999999, */ 99, 0xFFFFF, /*0xFF, */ 255,
	 /* 0xFF, */ 255, /* 0xFF, */ 255}
};

/* A35_FREQ table(ES3) */
struct mb86a35_freq1 {
	u8 CH;
	u32 FREQ;        //FREQ(KHz);

};
typedef struct mb86a35_freq1 mb86a35_freq1_t;

mb86a35_freq1_t mb86a35_freq1_VHF[] = {
	/*       CH , FREQ(KHz)        */
	{ /*Cvh */ 1, 207857},
	{ /*Cvh */ 2, 208285},
	{ /*Cvh */ 3, 208714},
	{ /*Cvh */ 4, 209142},
	{ /*Cvh */ 5, 209571},
	{ /*Cvh */ 6, 210000},
	{ /*Cvh */ 7, 210428},
	{ /*Cvh */ 8, 210857},
	{ /*Cvh */ 9, 211285},
	{ /*Cvh */ 10,211714},
	{ /*Cvh */ 11,212142},
	{ /*Cvh */ 12,212571},
	{ /*Cvh */ 13,213000},
	{ /*Cvh */ 14,213428},
	{ /*Cvh */ 15,213857},
	{ /*Cvh */ 16,214285},
	{ /*Cvh */ 17,214714},
	{ /*Cvh */ 18,215142},
	{ /*Cvh */ 19,215571},
	{ /*Cvh */ 20,216000},
	{ /*Cvh */ 21,216428},
	{ /*Cvh */ 22,216857},
	{ /*Cvh */ 23,217285},
	{ /*Cvh */ 24,217714},
	{ /*Cvh */ 25,218142},
	{ /*Cvh */ 26,218571},
	{ /*Cvh */ 27,219000},
	{ /*Cvh */ 28,219428},
	{ /*Cvh */ 29,219857},
	{ /*Cvh */ 30,220285},
	{ /*Cvh */ 31,220714},
	{ /*Cvh */ 32,221142},
	{ /*Cvh */ 33,221571}, 
	{ /*Cvh */ 0xFF,999999},
};

mb86a35_freq1_t mb86a35_freq1_UHF[] = {

	/*  CH , FREQ(KHz)        */
	{13,     473143},
	{14,     479143},
	{15,     485143},
	{16,     491143},
	{17,     497143},
	{18,     503143},
	{19,     509143},
	{20,     515143},
	{21,     521143},
	{22,     527143},
	{23,     533143},
	{24,     539143},
	{25,     545143},
	{26,     551143},
	{27,     557143},
	{28,     563143},
	{29,     569143},
	{30,     575143},
	{31,     581143},
	{32,     587143},
	{33,     593143},
	{34,     599143},
	{35,     605143},
	{36,     611143},
	{37,     617143},
	{38,     623143},
	{39,     629143},
	{40,     635143},
	{41,     641143},
	{42,     647143},
	{43,     653143},
	{44,     659143},
	{45,     665143},
	{46,     671143},
	{47,     677143},
	{48,     683143},
	{49,     689143},
	{50,     695143},
	{51,     701143},
	{52,     707143},
	{53,     713143},
	{54,     719143},
	{55,     725143},
	{56,     731143},
	{57,     737143},
	{58,     743143},
	{59,     749143},
	{60,     755143},
	{61,     761143},
	{62,     767143},
	{0xFF,   999999},
};

struct mb86a35_cmdcontrol {
	char mer_flg;

	ioctl_reset_t RESET;
	ioctl_init_t INIT;
	ioctl_agc_t AGC;
	ioctl_port_t GPIO;
	ioctl_seq_t SEQ;
	ioctl_ber_moni_t BER;
	ioctl_ts_t TS;
	ioctl_irq_t IRQ;
	ioctl_cn_moni_t CN;
	ioctl_mer_moni_t MER;
	ioctl_ch_search_t CHSRH;
	ioctl_rf_t RF;
	ioctl_i2c_t I2C;
	ioctl_hrm_t HRM;
	ioctl_ofdm_init_t OFDM_INIT;
	ioctl_low_up_if_t LOW_UP_IF;
	ioctl_spi_t SPI;
	ioctl_spi_config_t SPI_CONFIG;
	ioctl_stream_read_t STREAM_READ;
	ioctl_stream_read_ctrl_t STREAM_READ_CTRL;
	ioctl_ts_setup_t TS_SETUP;
	ioctl_select_antenna_t SELECT_ANTENNA;	// eric0.kim@lge.com[2012.07.29]
};
typedef struct mb86a35_cmdcontrol mb86a35_cmdcontrol_t;

#endif /* __RADIO_MB86A35_DRV_H__ */
