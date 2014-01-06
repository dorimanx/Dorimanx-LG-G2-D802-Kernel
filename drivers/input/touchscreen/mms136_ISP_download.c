/* Melfas MMS136 Series Download base v1.0 2011.10.11 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include "mms136_ISP_download.h"

#if defined(CONFIG_USING_INNOTEK_PANEL_3_5) || defined(CONFIG_USING_INNOTEK_PANEL_4_3)
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <mach/board_lge.h>
#endif
/*============================================================
//
//      Include MELFAS Binary code File ( ex> MELFAS_FIRM_bin.c)
//
//      Warning!!!!
//              Please, don't add binary.c file into project
//              Just #include here !!
//
============================================================*/

#if defined(CONFIG_USING_INNOTEK_PANEL_4_5) /*D1LA */
#include "MDH_LD1LF_R10_VA5.c" /* under rev.C */
#include "MDH_LD1LF_R10_V09.c" /* rev.D */
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_7)  /*D1LV */
/* #include "MTH_B_Va5_ESD_MODE_3_KEY.c"  rev.C ESD mode change */
/* #include "MTH-LD1LVF_B_Va7.c"	rev.C, rev.D camera deco ghost tuning, but it has key event issue */
/* #include "MTH-LD1LVF_RC10_VC01.c"  for rev.1.0 "C" tsp */
/* #include "MTH-LD1LVF_RC20_VC03.c"  for rev.1.0 "C" tsp - 20120402 */
/* #include "MTH-LD1LVF_RC20_VA2.c"   for rev.1.0 "C" tsp - 20120410 */
/*#include "MTH-LD1LVF_RC20_C_V07.c"  for rev.1.0 "C" tsp to add ghost pattern -20120615  */
/*#include "MTH-LD1LVF_RC20_D_V07.c"  for rev.1.0 "D" tsp to add ghost pattern -20120615  */
#include "MTH-LD1LVF_RC20_C_V12.c" /* for rev.1.0 "C" tsp to fix FW bug -20120620  */
#include "MTH-LD1LVF_RC20_D_V12.c" /* for rev.1.0 "D" tsp to fix FW bug -20120620  */

#elif defined(CONFIG_USING_TOVIS_PANEL) /*D1LK */
/*#include "MCH-LD1LKT_B_Vc8.c"   Tovis  GroupB sample (12.05.29) to fix IME issue */
#include "MCH-LD1LKT_B_Vcb.c"   /* Tovis  GroupB sample (12.06.04) to fix ghost finger issue */
/*#include "MDH-LD1LKT_RN12_VN01.c" LGIT Group N ITO sample (12.03.15)*/
/*#include "MDH-LD1LKF_RO13_VO05.c"  LGIT Group N ITO sample (12.05.26)*/
#include "MDH-LD1LKF_RO13_VO07.c" /* LGIT Group N ITO sample (12.06.01)*/
#elif defined(CONFIG_USING_INNOTEK_PANEL_3_5) /* L0 MPCS */
#include "L0-R00-V04_bin.c" /* rev.A */
#include "L0_R01_V15_02_bin.c" /* over rev.B */
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_3) /* L1 ATT */
#include "MTH_LL1_V0D_US.c"
#include "MTH_LL1_V51_US_Dummy.c"
#endif

extern UINT16 MELFAS_binary_nLength;
extern UINT8 *MELFAS_binary;

UINT8  ucVerifyBuffer[MELFAS_TRANSFER_LENGTH];          /* You may melloc *ucVerifyBuffer instead of this */

/*---------------------------------
//      Downloading functions
---------------------------------*/
/*static int  mcsdl_download(const UINT8 *pData, const UINT16 nLength, INT8 IdxNum);*/

static void mcsdl_set_ready(void);
static void mcsdl_reboot_mcs(void);

static int  mcsdl_erase_flash(INT8 IdxNum);
static int  mcsdl_program_flash(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum);
static void mcsdl_program_flash_part(UINT8 *pData);

static int  mcsdl_verify_flash(UINT8 *pData, UINT16 nLength, INT8 IdxNum);

static void mcsdl_read_flash(UINT8 *pBuffer);
static int  mcsdl_read_flash_from(UINT8 *pBuffer, UINT16 unStart_addr, UINT16 unLength, INT8 IdxNum);

static void mcsdl_select_isp_mode(UINT8 ucMode);
static void mcsdl_unselect_isp_mode(void);

static int  mcsdl_program_flash_sys(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum);
static int  mcsdl_verify_flash_sys(UINT8 *pData, UINT16 nLength, INT8 IdxNum);
static void  mcsdl_read_flash_sys(UINT8 *pBuffer);
static void mcsdl_select_isp_mode_sys(UINT8 ucMode);
static void mcsdl_unselect_isp_mode_sys(void);

static void mcsdl_read_32bits(UINT8 *pData);
static void mcsdl_write_bits(UINT32 wordData, int nBits);
/*static void mcsdl_scl_toggle_twice(void); */

/*---------------------------------
//      Delay functions
//---------------------------------*/
void mcsdl_delay(UINT32 nCount);

/*---------------------------------
//      For debugging display
//---------------------------------*/
#if MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet);
#endif


/*----------------------------------
// Download enable command
//----------------------------------*/
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
void melfas_send_download_enable_command(void)
{
    /* TO DO : Fill this up */
}
#endif

#if defined(CONFIG_USING_INNOTEK_PANEL_3_5)
void vdd_set_on_off(bool on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l17;
	if (lge_get_board_revno() >= HW_REV_B) {
		vreg_l17 = regulator_get(NULL, "8921_l17");	/* 3P0_L17_TOUCH */
		if (IS_ERR(vreg_l17)) {
			pr_err("%s: regulator get of touch_3p0 failed (%ld)\n"
				, __func__, PTR_ERR(vreg_l17));
			rc = PTR_ERR(vreg_l17);
			return ;
		}
		rc = regulator_set_voltage(vreg_l17,
			MELFAS_VD33_MIN_UV, MELFAS_VD33_MAX_UV);

		if (on) {
			rc = regulator_set_optimum_mode(vreg_l17, MELFAS_VD33_CURR_UA);
			rc = regulator_enable(vreg_l17);
		} else {
			rc = regulator_disable(vreg_l17);
		}
	} else {
		gpio_set_value(GPIO_TOUCH_EN, on);
	}
}
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_3)
void vdd_set_on_off(bool on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l17;
	vreg_l17 = regulator_get(NULL, "8921_l17");	/* 3P0_L17_TOUCH */
	if (IS_ERR(vreg_l17)) {
		pr_err("%s: regulator get of touch_3p0 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_l17));
		rc = PTR_ERR(vreg_l17);
		return ;
	}
	rc = regulator_set_voltage(vreg_l17,
		MELFAS_VD33_MIN_UV, MELFAS_VD33_MAX_UV);

	if (on) {
		rc = regulator_set_optimum_mode(vreg_l17, MELFAS_VD33_CURR_UA);
		rc = regulator_enable(vreg_l17);
	} else {
		rc = regulator_disable(vreg_l17);
	}
}
#endif

/*============================================================
//
//      Main Download furnction
//
//   1. Run mcsdl_download( pBinary[IdxNum], nBinary_length[IdxNum], IdxNum);
//       IdxNum : 0 (Master Chip Download)
//       IdxNum : 1 (2Chip Download)
//
//
============================================================*/

int mms100_ISP_download_binary_data(int dl_mode)
{
	int nRet = 0;
	int retry_cnt = 0;

#if 0
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR();			/* Disable Baseband touch interrupt ISR. */
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();		/* Disable Baseband watchdog timer */
#endif

	/*------------------------
	// Run Download
	//------------------------*/

	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		if (dl_mode == 0x01)/* MELFAS_ISP_DOWNLOAD */
			nRet = mcsdl_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength , 0);
		else /* MELFAS_ISC_DOWNLOAD */
			nRet = mcsdl_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength , 0);
#if MELFAS_2CHIP_DOWNLOAD_ENABLE
		if (!nRet) {
			if (dl_mode == 0x01) /* MELFAS_ISP_DOWNLOAD */
				nRet = mcsdl_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength, 1); /* Slave Binary data download */
			else /* MELFAS_ISC_DOWNLOAD */
				nRet = mcsdl_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength , 1);
		}
#endif
		if (!nRet)
			break;
	}

	MELFAS_ROLLBACK_BASEBAND_ISR();			/*Roll-back Baseband touch interrupt ISR.*/
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();		/*Roll-back Baseband watchdog timer*/
/*
fw_error:
	if (nRet) {
		mcsdl_erase_flash(0);
		mcsdl_erase_flash(1);
	}*/
	return nRet;
}

#if 0
int mms100_ISP_download_binary_file(void)
{
	int nRet;
	int i;

	UINT8  *pBinary[2] = {NULL, NULL};
	UINT16 nBinary_length[2] = {0, 0};
	UINT8 IdxNum = MELFAS_2CHIP_DOWNLOAD_ENABLE;
	/*==================================================
	//
	//      1. Read '.bin file'
	//      2. *pBinary[0]       : Binary data(Core + Private Custom)
	//       *pBinary[1]       : Binary data(Public Custom)
	//         nBinary_length[0] : Firmware size(Core + Private Custom)
	//         nBinary_length[1] : Firmware size(Public Custom)
	//      3. Run mcsdl_download( pBinary[IdxNum], nBinary_length[IdxNum], IdxNum);
	//       IdxNum : 0 (Master Chip Download)
	//       IdxNum : 1 (2Chip Download)
	//
	//==================================================*/


	/* TO DO : File Process & Get file Size(== Binary size)
	//                  This is just a simple sample*/

	FILE *fp;
	INT  nRead;

	/*------------------------------
	// Open a file
	//------------------------------*/

	if (fopen(fp, "MELFAS_FIRMWARE.bin", "rb") == NULL) {
		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	/*------------------------------
	// Get Binary Size
	//------------------------------*/

	fseek(fp, 0, SEEK_END);
	nBinary_length = (UINT16)ftell(fp);

	/*------------------------------
	// Memory allocation
	//------------------------------*/

	pBinary = (UINT8 *)malloc((INT)nBinary_length);
	if (pBinary == NULL) {
		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	/*------------------------------
	// Read binary file
	//------------------------------*/

	fseek(fp, 0, SEEK_SET);
	nRead = fread(pBinary, 1, (INT)nBinary_length, fp);             /* Read binary file */
	if (nRead != (INT)nBinary_length) {
		fclose(fp);                                             /* Close file */
		if (pBinary != NULL)                                    /* free memory alloced. */
			free(pBinary);
		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	/*------------------------------
	// Close file
	//------------------------------*/

	fclose(fp);

#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR();                  /* Disable Baseband touch interrupt ISR */
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();          /* Disable Baseband watchdog timer */

	for (i = 0; i <= IdxNum; i++) {
		if (pBinary[0] != NULL && nBinary_length[0] > 0 && nBinary_length[0] < MELFAS_FIRMWARE_MAX_SIZE)
		/*------------------------
		// Run Download
		//------------------------*/
			nRet = mcsdl_download((const UINT8 *)pBinary[0], (const UINT16)nBinary_length[0], i);
		else
			nRet = MCSDL_RET_WRONG_BINARY;
	}

	MELFAS_ROLLBACK_BASEBAND_ISR();                 /* Roll-back Baseband touch interrupt ISR.*/
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();         /* Roll-back Baseband watchdog timer */

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result(nRet);
#endif

	return (nRet == MCSDL_RET_SUCCESS);

}
#endif

/*------------------------------------------------------------------
//
//      Download function
//
//------------------------------------------------------------------*/

int mcsdl_download(const UINT8 *pBianry, const UINT16 unLength, INT8 IdxNum)
{
	int nRet;

	/*---------------------------------
	// Check Binary Size
	//---------------------------------*/
	if (unLength > MELFAS_FIRMWARE_MAX_SIZE) {
		nRet = MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
		goto MCSDL_DOWNLOAD_FINISH;
	}


#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" - Starting download...\n");
#endif

	/*---------------------------------
	// Make it ready
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Ready\n");
#endif
	mcsdl_set_ready();

	/*---------------------------------
	// Erase Flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Erase\n");
#endif
	nRet = mcsdl_erase_flash(IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/*---------------------------------
	// Program Flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n > Program   ");
#endif

	nRet = mcsdl_program_flash((UINT8 *)pBianry, (UINT16)unLength, IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/*---------------------------------
	// Verify flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n > Verify    ");
#endif
	nRet = mcsdl_verify_flash((UINT8 *)pBianry, (UINT16)unLength, IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	nRet = MCSDL_RET_SUCCESS;

MCSDL_DOWNLOAD_FINISH:

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result(nRet);       /*Show result*/
#endif
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Rebooting\n");
	printk(" - Fin.\n\n");
#endif
	mcsdl_reboot_mcs();

	return nRet;
}

/*------------------------------------------------------------------
//
//      Sub functions
//
//------------------------------------------------------------------*/
static int mcsdl_erase_flash(INT8 IdxNum)
{
	int         i;
	UINT8 readBuffer[32];
	int eraseCompareValue = 0xFF;
	/*----------------------------------------
	//      Do erase
	//----------------------------------------*/
	if (IdxNum > 0) {
		mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
		mcsdl_delay(MCSDL_DELAY_3US);
	}
	mcsdl_select_isp_mode(ISP_MODE_ERASE_FLASH);
	mcsdl_unselect_isp_mode();

	/*----------------------------------------
	//      Check 'erased well'
	//----------------------------------------*/

	mcsdl_read_flash_from(readBuffer, 0x0000, 16, IdxNum);
	mcsdl_read_flash_from(&readBuffer[16], 0x7FF0, 16, IdxNum);

	if (IdxNum > 0) {
		eraseCompareValue = 0x00;
	}
	for (i = 0; i < 32; i++) {
		if (readBuffer[i] != eraseCompareValue)
			return MCSDL_RET_ERASE_FLASH_VERIFY_FAILED;
	}

	return MCSDL_RET_SUCCESS;
}

static int mcsdl_program_flash(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum)
{
	int             i;

	UINT8   *pData;
	UINT8   ucLength;

	UINT16  addr;
	UINT32  header;

	addr   = 0;
	pData  = pDataOriginal;
	ucLength = MELFAS_TRANSFER_LENGTH;

	while ((addr*4) < (int)unLength) {
		if ((unLength - (addr*4)) < MELFAS_TRANSFER_LENGTH)
			ucLength  = (UINT8)(unLength - (addr * 4));

		/*--------------------------------------
		//      Select ISP Mode
		//--------------------------------------*/

		mcsdl_delay(MCSDL_DELAY_40US);

		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_WRITE);

		/*---------------------------------------------
		//      Header
		//      Address[13ibts] <<1
		//---------------------------------------------*/
		header = ((addr & 0x1FFF) << 1) | 0x0 ;
		header = header << 14;
		mcsdl_write_bits(header, 18); /*Write 18bits*/

		/*---------------------------------
		//      Writing
		//---------------------------------*/
		/*addr += (UINT16)ucLength;*/
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		/*printk("#");*/
#endif
		mcsdl_program_flash_part(pData);
		pData  += ucLength;

		/*---------------------------------------------
		//      Tail
		//---------------------------------------------*/
		MCSDL_GPIO_SDA_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_40US);

		for (i = 0; i < 6; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_20US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_40US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_10US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_10US);
		}
		MCSDL_GPIO_SDA_SET_LOW();

		mcsdl_unselect_isp_mode();
		mcsdl_delay(MCSDL_DELAY_300US);
	}

	return MCSDL_RET_SUCCESS;
}

static void mcsdl_program_flash_part(UINT8 *pData)
{
	UINT32  data;
	/*---------------------------------
	//      Body
	//---------------------------------*/
	data  = (UINT32)pData[0] <<  0;
	data |= (UINT32)pData[1] <<  8;
	data |= (UINT32)pData[2] << 16;
	data |= (UINT32)pData[3] << 24;
	mcsdl_write_bits(data, 32);
}

static int mcsdl_verify_flash(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum)
{
	int       j;
	int       nRet;

	UINT8 *pData;
	UINT8 ucLength;

	UINT16 addr;
	UINT32 wordData;

	addr  = 0;
	pData = (UINT8 *) pDataOriginal;
	ucLength  = MELFAS_TRANSFER_LENGTH;

	while ((addr*4) < (int)unLength) {
		if ((unLength - (addr*4)) < MELFAS_TRANSFER_LENGTH)
			ucLength = (UINT8)(unLength - (addr * 4));

		/* start ADD DELAY */
		mcsdl_delay(MCSDL_DELAY_40US);

		/*--------------------------------------
		//      Select ISP Mode
		//--------------------------------------*/
		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}

		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);

		/*---------------------------------------------
		//      Header
		//      Address[13ibts] <<1
		//---------------------------------------------*/
		wordData   = ((addr & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;
		mcsdl_write_bits(wordData, 18);
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		/* printk("#"); */
#endif
		/*--------------------
		// Read flash
		//--------------------*/
		mcsdl_read_flash(ucVerifyBuffer);

		MCSDL_GPIO_SDA_SET_LOW();
		MCSDL_GPIO_SDA_SET_OUTPUT(0);

		/*--------------------
		// Comparing
		//--------------------*/
		if (IdxNum == 0) {
			for (j = 0; j < (int)ucLength; j++) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				/* printk(" %02X", ucVerifyBuffer[j]); */
				udelay(2);
#endif
				if (ucVerifyBuffer[j] != pData[j]) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
					printk("\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", addr, pData[j], ucVerifyBuffer[j]);
#endif
					nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
					goto MCSDL_VERIFY_FLASH_FINISH;
				}
			}
		} else /* slave */ {
			for (j = 0; j < (int)ucLength; j++) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				printk(" %02X", ucVerifyBuffer[j]);
#endif
				if ((0xff - ucVerifyBuffer[j]) != pData[j]) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
					printk("\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", addr, pData[j], ucVerifyBuffer[j]);
#endif
					nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
					goto MCSDL_VERIFY_FLASH_FINISH;
				}
			}
		}
		pData += ucLength;
		mcsdl_unselect_isp_mode();
	}

	nRet = MCSDL_RET_SUCCESS;

MCSDL_VERIFY_FLASH_FINISH:

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n");
#endif

	mcsdl_unselect_isp_mode();
	return nRet;
}


static void mcsdl_read_flash(UINT8 *pBuffer)
{
	int i;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_40US);

	for (i = 0; i < 6; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_10US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_10US);
	}
	mcsdl_read_32bits(pBuffer);
}

static int mcsdl_read_flash_from(UINT8 *pBuffer, UINT16 unStart_addr, UINT16 unLength, INT8 IdxNum)
{
	int i;
	int j;

	UINT8  ucLength;

	UINT16 addr;
	UINT32 wordData;

	if (unLength >= MELFAS_FIRMWARE_MAX_SIZE)
		return MCSDL_RET_PROGRAM_SIZE_IS_WRONG;

	addr  = 0;
	ucLength  = MELFAS_TRANSFER_LENGTH;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" %04X : ", unStart_addr);
#endif
	for (i = 0; i < (int)unLength; i += (int)ucLength) {
		addr = (UINT16)i;
		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}

		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);
		wordData   = (((unStart_addr + addr)&0x1FFF) << 1) | 0x0;
		wordData <<= 14;

		mcsdl_write_bits(wordData, 18);

		if ((unLength - addr) < MELFAS_TRANSFER_LENGTH)
			ucLength = (UINT8)(unLength - addr);

		/*--------------------
		// Read flash
		//--------------------*/
		mcsdl_read_flash(&pBuffer[addr]);

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		for (j = 0; j < (int)ucLength; j++)
			printk("%02X ", pBuffer[j]);
#endif
		mcsdl_unselect_isp_mode();
	}

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	/*printk("\n");*/
#endif
	return MCSDL_RET_SUCCESS;
}


static void mcsdl_set_ready(void)
{
	/*--------------------------------------------
	// Tkey module reset
	//--------------------------------------------*/

	MCSDL_VDD_SET_LOW(); /*power*/

	MCSDL_CE_SET_LOW();
	MCSDL_CE_SET_OUTPUT(0);

	MCSDL_SET_GPIO_I2C();

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SCL_SET_OUTPUT(0);

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT(0);

	mcsdl_delay(MCSDL_DELAY_25MS);                                          /* Delay for Stable VDD */

	MCSDL_VDD_SET_HIGH();
	/*MCSDL_CE_SET_HIGH();*/

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_40MS);                                          /*  Delay '30 msec' */
}


static void mcsdl_reboot_mcs(void)
{
	MCSDL_VDD_SET_LOW();

	MCSDL_CE_SET_LOW();
	MCSDL_CE_SET_OUTPUT(0);

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT(0);

	mcsdl_delay(MCSDL_DELAY_25MS);                                          /* Delay for Stable VDD */

	MCSDL_VDD_SET_HIGH();
	/*MCSDL_CE_SET_HIGH();*/

	MCSDL_RESETB_SET_HIGH();
	MCSDL_RESETB_SET_INPUT();
	MCSDL_GPIO_SCL_SET_INPUT();
	MCSDL_GPIO_SDA_SET_INPUT();

	mcsdl_delay(MCSDL_DELAY_30MS);                                          /* Delay '25 msec' */
}


/*--------------------------------------------
//
//   Write ISP Mode entering signal
//
//--------------------------------------------*/
static void mcsdl_select_isp_mode(UINT8 ucMode)
{
	int    i;

	UINT8 enteringCodeMassErase[16]   = { 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1 };
	UINT8 enteringCodeSerialWrite[16] = { 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
	UINT8 enteringCodeSerialRead[16]  = { 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1 };
	UINT8 enteringCodeNextChipBypass[16]  = { 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 };

	UINT8 *pCode = 0;

	/*------------------------------------
	// Entering ISP mode : Part 1
	//------------------------------------*/
	if (ucMode == ISP_MODE_ERASE_FLASH)
		pCode = enteringCodeMassErase;
	else if (ucMode == ISP_MODE_SERIAL_WRITE)
		pCode = enteringCodeSerialWrite;
	else if (ucMode == ISP_MODE_SERIAL_READ)
		pCode = enteringCodeSerialRead;
	else if (ucMode == ISP_MODE_NEXT_CHIP_BYPASS)
		pCode = enteringCodeNextChipBypass;

	MCSDL_RESETB_SET_LOW();
	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();

	for (i = 0; i < 16; i++) {
		if (pCode[i] == 1)
			MCSDL_RESETB_SET_HIGH();
		else
			MCSDL_RESETB_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}

	MCSDL_RESETB_SET_LOW();

	/*---------------------------------------------------
	// Entering ISP mode : Part 2   - Only Mass Erase
	//---------------------------------------------------*/
	mcsdl_delay(MCSDL_DELAY_7US);

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();
	if (ucMode == ISP_MODE_ERASE_FLASH) {
		mcsdl_delay(MCSDL_DELAY_7US);
		for (i = 0; i < 4; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_25MS);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_150US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_7US);
		}
	}
	MCSDL_GPIO_SDA_SET_LOW();
}


static void mcsdl_unselect_isp_mode(void)
{
	int i;

	MCSDL_RESETB_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_3US);

	for (i = 0; i < 10; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}
}

static void mcsdl_read_32bits(UINT8 *pData)
{
	int i, j;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_INPUT();

	for (i = 3; i >= 0; i--) {
		pData[i] = 0;
		for (j = 0; j < 8; j++) {
			pData[i] <<= 1;
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_1US);
			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_1US);
			if (MCSDL_GPIO_SDA_IS_HIGH())
				pData[i] |= 0x01;
		}
	}

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);
}

static void mcsdl_write_bits(UINT32 wordData, int nBits)
{
	int i;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();

	for (i = 0; i < nBits; i++) {
		if (wordData & 0x80000000)
			MCSDL_GPIO_SDA_SET_HIGH();
		else
			MCSDL_GPIO_SDA_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);

		wordData <<= 1;
	}
}

/*
static void mcsdl_scl_toggle_twice(void)
{
	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();  mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();   mcsdl_delay(MCSDL_DELAY_20US);

	MCSDL_GPIO_SCL_SET_HIGH();  mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();   mcsdl_delay(MCSDL_DELAY_20US);
}
*/

/*============================================================
//
//      Sys Function
//
//============================================================*/
int mcsdl_download_sys(const UINT8 *pBianry, const UINT16 unLength, INT8 IdxNum)
{
	int nRet;

	/*---------------------------------
	// Check Binary Size
	//---------------------------------*/
	if (unLength > MELFAS_FIRMWARE_MAX_SIZE) {
		nRet = MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
		goto MCSDL_DOWNLOAD_FINISH;
	}


#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" - Starting download...\n");
#endif

	/*---------------------------------
	// Make it ready
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Ready\n");
#endif
	mcsdl_set_ready();

	/*---------------------------------
	// Erase Flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Erase\n");
#endif
	nRet = mcsdl_erase_flash(IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;


	/*---------------------------------
	// Program Flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n > Program Test  ");
#endif

	nRet = mcsdl_program_flash_sys((UINT8 *)pBianry, (UINT16)unLength, IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/*---------------------------------
	// Verify flash
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n > Verify    ");
#endif
	nRet = mcsdl_verify_flash_sys((UINT8 *)pBianry, (UINT16)unLength, IdxNum);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	nRet = MCSDL_RET_SUCCESS;

MCSDL_DOWNLOAD_FINISH:

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result(nRet);       /*Show result*/
#endif
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Rebooting\n");
	printk(" - Fin.\n\n");
#endif
	mcsdl_reboot_mcs();

	return nRet;
}

static int mcsdl_program_flash_sys(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum)
{
	int             i;

	UINT8   *pData;
	UINT8   ucLength;

	UINT16  addr;
	UINT32  header;

	addr   = 0;
	pData  = pDataOriginal;
	ucLength = MELFAS_TRANSFER_LENGTH;

	while ((addr*4) < (int)unLength) {
		if ((unLength - (addr*4)) < MELFAS_TRANSFER_LENGTH)
			ucLength  = (UINT8)(unLength - (addr * 4));

		/*--------------------------------------
		//      Select ISP Mode
		//--------------------------------------*/

		mcsdl_delay(MCSDL_DELAY_10US); /* 40 us*/

		if (IdxNum > 0) {
			mcsdl_select_isp_mode_sys(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}
		mcsdl_select_isp_mode_sys(ISP_MODE_SERIAL_WRITE);

		/*---------------------------------------------
		//      Header
		//      Address[13ibts] <<1
		//---------------------------------------------*/
		header = ((addr & 0x1FFF) << 1) | 0x0 ;
		header = header << 14;
		mcsdl_write_bits(header, 18); /*Write 18bits*/

		/*---------------------------------
		//      Writing
		//---------------------------------*/
		/*addr += (UINT16)ucLength;*/
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		/*printk("#");*/
#endif

		mcsdl_program_flash_part(pData);
		pData  += ucLength;

		/*---------------------------------------------
		//      Tail
		//---------------------------------------------*/

		MCSDL_RESETB_SET_OUTPUT(1);

		MCSDL_GPIO_SDA_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_10US); /* 40us*/
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_5US);

		for (i = 0; i < 6; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_10US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_20US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_5US); /* 10us*/
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_5US); /* 10us*/

		}
		MCSDL_GPIO_SDA_SET_LOW();
		MCSDL_RESETB_SET_LOW();

		mcsdl_unselect_isp_mode_sys();
		/*mcsdl_delay(MCSDL_DELAY_300US);*/
	}

	return MCSDL_RET_SUCCESS;
}

static int mcsdl_verify_flash_sys(UINT8 *pDataOriginal, UINT16 unLength, INT8 IdxNum)
{
	int       j;
	int       nRet;

	UINT8 *pData;
	UINT8 ucLength;

	UINT16 addr;
	UINT32 wordData;

	addr  = 0;
	pData = (UINT8 *) pDataOriginal;
	ucLength  = MELFAS_TRANSFER_LENGTH;

	while ((addr*4) < (int)unLength) {
		if ((unLength - (addr*4)) < MELFAS_TRANSFER_LENGTH)
			ucLength = (UINT8)(unLength - (addr * 4));

		/* start ADD DELAY */
		mcsdl_delay(MCSDL_DELAY_20US); /* 40 us*/

		/*--------------------------------------
		//      Select ISP Mode
		//--------------------------------------*/
		if (IdxNum > 0) {
			mcsdl_select_isp_mode_sys(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}

		mcsdl_select_isp_mode_sys(ISP_MODE_SERIAL_READ);

		/*---------------------------------------------
		//      Header
		//      Address[13ibts] <<1
		//---------------------------------------------*/
		wordData   = ((addr & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;
		mcsdl_write_bits(wordData, 18);
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		/* printk("#"); */
#endif
		/*--------------------
		// Read flash
		//--------------------*/
		mcsdl_read_flash_sys(ucVerifyBuffer);

		MCSDL_GPIO_SDA_SET_LOW();
		MCSDL_GPIO_SDA_SET_OUTPUT(0);

		/*--------------------
		// Comparing
		//--------------------*/
		if (IdxNum == 0) {
			for (j = 0; j < (int)ucLength; j++) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				udelay(2);
#endif
				if (ucVerifyBuffer[j] != pData[j]) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
					printk("\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", addr, pData[j], ucVerifyBuffer[j]);
#endif
					nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
					goto MCSDL_VERIFY_FLASH_FINISH;
				}
			}
		}
		pData += ucLength;
		mcsdl_unselect_isp_mode_sys();
	}

	nRet = MCSDL_RET_SUCCESS;

MCSDL_VERIFY_FLASH_FINISH:

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n");
#endif

	mcsdl_unselect_isp_mode();
	return nRet;
}

static void mcsdl_select_isp_mode_sys(UINT8 ucMode)
{
	int    i;

	UINT8 enteringCodeMassErase[16]   = { 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1 };
	UINT8 enteringCodeSerialWrite[16] = { 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
	UINT8 enteringCodeSerialRead[16]  = { 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1 };
	UINT8 enteringCodeNextChipBypass[16]  = { 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 };

	UINT8 *pCode = 0;

	/*------------------------------------
	// Entering ISP mode : Part 1
	//------------------------------------*/
	if (ucMode == ISP_MODE_ERASE_FLASH)
		pCode = enteringCodeMassErase;
	else if (ucMode == ISP_MODE_SERIAL_WRITE)
		pCode = enteringCodeSerialWrite;
	else if (ucMode == ISP_MODE_SERIAL_READ)
		pCode = enteringCodeSerialRead;
	else if (ucMode == ISP_MODE_NEXT_CHIP_BYPASS)
		pCode = enteringCodeNextChipBypass;

	/*MCSDL_RESETB_SET_LOW();*/
	MCSDL_RESETB_SET_OUTPUT(0);
	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();

	for (i = 0; i < 16; i++) {
		if (pCode[i] == 1)
			MCSDL_RESETB_SET_HIGH();
		else
			MCSDL_RESETB_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_1US); /* 3us*/
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_1US); /* 3us*/
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_1US); /* 3us*/
	}

	MCSDL_RESETB_SET_LOW();

	/*---------------------------------------------------
	// Entering ISP mode : Part 2   - Only Mass Erase
	//---------------------------------------------------*/
	mcsdl_delay(MCSDL_DELAY_7US);

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();
	if (ucMode == ISP_MODE_ERASE_FLASH) {
		mcsdl_delay(MCSDL_DELAY_7US);
		for (i = 0; i < 4; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_25MS);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_150US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_7US);
		}
	}
	MCSDL_GPIO_SDA_SET_LOW();
}

static void mcsdl_unselect_isp_mode_sys(void)
{
	int i;

	MCSDL_RESETB_SET_OUTPUT(0);
	mcsdl_delay(MCSDL_DELAY_3US);

	for (i = 0; i < 10; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_1US); /* 3us*/
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_1US); /* 3us*/
	}
}

static void mcsdl_read_flash_sys(UINT8 *pBuffer)
{
	int i;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_40US);

	MCSDL_RESETB_SET_OUTPUT(1);
	for (i = 0; i < 6; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_5US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_5US);
	}
	MCSDL_RESETB_SET_LOW();
	mcsdl_read_32bits(pBuffer);
}
/*============================================================
//
//      Delay Function
//
//============================================================*/
void mcsdl_delay(UINT32 nCount)
{
	if (nCount > 10000)
		mdelay(nCount / 1000);
	else
		udelay(nCount);
}

/*============================================================
//
//      Debugging print functions.
//
//============================================================*/

#ifdef MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet)
{
	if (nRet == MCSDL_RET_SUCCESS) {
		printk(" > MELFAS Firmware downloading SUCCESS.\n");
	} else {
		printk(" > MELFAS Firmware downloading FAILED  :  ");
		switch (nRet) {
		case MCSDL_RET_SUCCESS:
			printk("MCSDL_RET_SUCCESS\n");
			break;
		case MCSDL_RET_ERASE_FLASH_VERIFY_FAILED:
			printk("MCSDL_RET_ERASE_FLASH_VERIFY_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_VERIFY_FAILED:
			printk("MCSDL_RET_PROGRAM_VERIFY_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_SIZE_IS_WRONG:
			printk("MCSDL_RET_PROGRAM_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_VERIFY_SIZE_IS_WRONG:
			printk("MCSDL_RET_VERIFY_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_WRONG_BINARY:
			printk("MCSDL_RET_WRONG_BINARY\n");
			break;
		case MCSDL_RET_READING_HEXFILE_FAILED:
			printk("MCSDL_RET_READING_HEXFILE_FAILED\n");
			break;
		case MCSDL_RET_FILE_ACCESS_FAILED:
			printk("MCSDL_RET_FILE_ACCESS_FAILED\n");
			break;
		case MCSDL_RET_MELLOC_FAILED:
			printk("MCSDL_RET_MELLOC_FAILED\n");
			break;
		case MCSDL_RET_WRONG_MODULE_REVISION:
			printk("MCSDL_RET_WRONG_MODULE_REVISION\n");
			break;
		default:
			printk("UNKNOWN ERROR. [0x%02X].\n", nRet);
		break;
		}
		printk("\n");
	}
}

#endif


#if MELFAS_ENABLE_DELAY_TEST

/*============================================================
//
//      For initial testing of delay and gpio control
//
//      You can confirm GPIO control and delay time by calling this function.
//
//============================================================*/

void mcsdl_delay_test(INT32 nCount)
{
	INT16 i;
	MELFAS_DISABLE_BASEBAND_ISR();                          /*Disable Baseband touch interrupt ISR*/
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();                  /* Disable Baseband watchdog timer*/

	/*--------------------------------
	//      Repeating 'nCount' times
	//--------------------------------*/
	/*MCSDL_SET_GPIO_I2C();*/
	MCSDL_GPIO_SCL_SET_OUTPUT(0);
	MCSDL_GPIO_SDA_SET_OUTPUT(0);
	MCSDL_RESETB_SET_OUTPUT(0);

	MCSDL_GPIO_SCL_SET_HIGH();

	for (i = 0; i < nCount; i++) {
#if 1
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_20US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_100US);
#elif 0
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_500US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_1MS);
#else
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_25MS);
		TKEY_INTR_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_45MS);
		TKEY_INTR_SET_HIGH();
#endif
	}

	MCSDL_GPIO_SCL_SET_HIGH();

	MELFAS_ROLLBACK_BASEBAND_ISR();                         /* Roll-back Baseband touch interrupt ISR.*/
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();                 /* Roll-back Baseband watchdog timer */
}
#endif



