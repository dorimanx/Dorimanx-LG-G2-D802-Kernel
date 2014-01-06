/*--------------------------------------------------------
//
//
//      Melfas MMS100 Series Download base v1.7 2011.09.23
//
//--------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <mach/gpio.h>
#include <mach/gpiomux.h>

#include "mms136_ISP_download.h"

/*============================================================
//
//      Include MELFAS Binary code File ( ex> MELFAS_FIRM_bin.c)
//
//      Warning!!!!
//              Please, don't add binary.c file into project
//              Just #include here !!
//
//============================================================*/


extern UINT16 MELFAS_binary_nLength;
extern UINT8 *MELFAS_binary;

UINT8  ucSlave_addr = ISC_MODE_SLAVE_ADDRESS;
UINT8 ucInitial_Download = FALSE;


const UINT16 test_nLength = 16;
const  UINT8 test_binary[] = {
	0x00, 0x20, 0x00, 0x20, 0xB1, 0x02, 0x00, 0x00, 0x71, 0x02, 0x00, 0x00, 0x75, 0x02, 0x00, 0x00,
};

extern void mcsdl_delay(UINT32 nCount);

static struct gpiomux_setting gpio_sda_config[2] = {
	{
		.pull = GPIOMUX_PULL_NONE,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_GPIO,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.pull = GPIOMUX_PULL_DOWN,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_3,/*.func = GPIOMUX_FUNC_1,*/
	},
};

static struct gpiomux_setting gpio_scl_config[2] = {
	{
		.pull = GPIOMUX_PULL_NONE,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_GPIO,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.pull = GPIOMUX_PULL_DOWN,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_3,/*.func = GPIOMUX_FUNC_1,*/
	},
};

#if 0
static struct gpiomux_setting gpio_tocuhen_config = {
	.pull = GPIOMUX_PULL_NONE,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
	.dir = GPIOMUX_OUT_LOW,
};
#endif

static struct gpiomux_setting gpio_tocuhint_config = {
	.pull = GPIOMUX_PULL_NONE,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config melfas_gpio_configs[] = {
	{
		.gpio = GPIO_TSP_SDA,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sda_config[0],
			[GPIOMUX_SUSPENDED]    = &gpio_sda_config[0],
		}
	},
	{
		.gpio = GPIO_TSP_SCL,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_scl_config[0],
			[GPIOMUX_SUSPENDED]    = &gpio_scl_config[0],
		}
	},
#if 0
	{
		.gpio = GPIO_TOUCH_EN,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_tocuhen_config,
			[GPIOMUX_SUSPENDED]    = &gpio_tocuhen_config,
		}
	},
#endif
	{
		.gpio = GPIO_TOUCH_INT,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_tocuhint_config,
			[GPIOMUX_SUSPENDED]    = &gpio_tocuhint_config,
		}
	},
};

static struct msm_gpiomux_config melfas_i2c_configs[] = {
	{
		.gpio = GPIO_TSP_SDA,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sda_config[1],
			[GPIOMUX_SUSPENDED]    = &gpio_sda_config[1],
		}
	},
	{
		.gpio = GPIO_TSP_SCL,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_scl_config[1],
			[GPIOMUX_SUSPENDED]    = &gpio_scl_config[1],
		}
	},
};

/*---------------------------------
//      Downloading functions
//---------------------------------*/

static int mms100_ISC_download(const UINT8 *pBianry, const UINT16 unLength, const UINT8 nMode);

static void mms100_ISC_set_ready(void);
static void mms100_ISC_reboot_mcs(void);

static UINT8 mcsdl_read_ack(void);
/*static void mcsdl_ISC_read_32bits( UINT8 *pData );*/
static void mcsdl_ISC_write_bits(UINT32 wordData, int nBits);
static UINT8 mms100_ISC_read_data(UINT8 addr);

static void mms100_ISC_enter_download_mode(void);
static void mms100_ISC_firmware_update_mode_enter(void);
static UINT8 mms100_ISC_firmware_update(UINT8 *_pBinary_reordered, UINT16 _unDownload_size, UINT8 flash_start, UINT8 flash_end);
static UINT8 mms100_ISC_read_firmware_status(void);
/*static int mms100_ISC_Slave_download(UINT8 nMode);*/
/*static void mms100_ISC_slave_download_start(UINT8 nMode);*/
/*static UINT8 mms100_ISC_slave_crc_ok(void);*/
static void mms100_ISC_leave_firmware_update_mode(void);
static void mcsdl_i2c_start(void);
static void mcsdl_i2c_stop(void);
static UINT8 mcsdl_read_byte(void);

/*---------------------------------
//      For debugging display
//---------------------------------*/
#if MELFAS_ENABLE_DBG_PRINT
static void mcsdl_ISC_print_result(int nRet);
#endif

/*----------------------------------
// Download enable command
//----------------------------------*/
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
void melfas_send_download_enable_command(void){
	/* TO DO : Fill this up */
}
#endif

int mms100_download(int download_type, int file_download)
{
	int ret = 0;
#if !MELFAS_ISP_DOWNLOAD
	int i = 0;
#endif
	msm_gpiomux_install(melfas_gpio_configs, ARRAY_SIZE(melfas_gpio_configs));

	gpio_request(GPIO_TSP_SDA, "Melfas_I2C_SDA");
	gpio_request(GPIO_TSP_SCL, "Melfas_I2C_SCL");
	gpio_request(GPIO_TOUCH_INT, "Melfas_I2C_INT");

	if (file_download == external_img)
		ret = mms100_ISC_download_binary_file();
	else {
#if MELFAS_ISP_DOWNLOAD
		ret = mms100_ISP_download_binary_data(MELFAS_ISP_DOWNLOAD);
		if (ret)
			printk("<MELFAS> SET Download ISP Fail\n");
#else
		if (download_type == isc_type) {
			ret = mms100_ISC_download_binary_data();
			if (ret) {
				printk("<MELFAS> SET Download ISC Fail\n");
				for (i = 0; i < 3 ; i++) {
					if (ret)
						ret = mms100_ISP_download_binary_data(MELFAS_ISP_DOWNLOAD);     /*ISP mode download ( CORE + PRIVATE )*/
					if (ret)
						printk("<MELFAS> SET Download ISC & ISP Fail\n");
					else
						break;
				}
			}
		} else if (download_type == isp_type) {
			printk("<MELFAS> I2C Fail ISP download start\n");
			ret = mcsdl_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength , 0);  /*ISP mode download ( CORE + PRIVATE )*/
		} else if (download_type == check_type) {
			printk("<MELFAS> ISP download check\n");
			ret = mcsdl_download((const UINT8 *) test_binary, (const UINT16)test_nLength , 0);  /*ISP mode download ( CORE + PRIVATE )*/
		} else {
			printk("<MELFAS> ISP download for sys \n");
			ret = mcsdl_download_sys((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength , 0);  /*ISP mode download ( CORE + PRIVATE )*/
		}
#endif
	}
	msm_gpiomux_install(melfas_i2c_configs , ARRAY_SIZE(melfas_i2c_configs));
	gpio_free(GPIO_TSP_SDA);
	gpio_free(GPIO_TSP_SCL);
	gpio_free(GPIO_TOUCH_INT);
	return ret;

}

/*============================================================
//
//      Main Download furnction
//
//   1. Run mms100_ISC_download(*pBianry,unLength, nMode)
//       nMode : 0 (Core download)
//       nMode : 1 (Private Custom download)
//       nMode : 2 (Public Custom download)
//
//============================================================*/

int mms100_ISC_download_binary_data(void)
{
	int i, nRet;
	INT8 dl_enable_bit = 0x00;
	INT8 core_version_info = 0;
	INT8 private_version_info = 0;
	INT8 public_version_info = 0;

#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR();				/* Disable Baseband touch interrupt ISR.*/
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();                  /* Disable Baseband watchdog timer */

	mms100_ISC_set_ready();

	/*---------------------------------
	// set download enable mode
	//---------------------------------*/

	if (ucInitial_Download)
		ucSlave_addr = ISC_DEFAULT_SLAVE_ADDR;

	if (MELFAS_CORE_FIRWMARE_UPDATE_ENABLE || ucInitial_Download) {
		core_version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_CORE);
		printk("<MELFAS> CORE_VERSION : 0x%2X\n", core_version_info);
		private_version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PRIVATE_CUSTOM);
		printk("<MELFAS> PRIVATE_CUSTOM_VERSION : 0x%2X\n", private_version_info);
		public_version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PUBLIC_CUSTOM);
		printk("<MELFAS> PUBLIC_CUSTOM_VERSION : 0x%2X\n", public_version_info);
#if 0
		if (core_version_info < MELFAS_DOWNLAOD_CORE_VERSION || core_version_info == 0xFF)
#endif
		dl_enable_bit |= 0x01;
	}

	if (MELFAS_PRIVATE_CONFIGURATION_UPDATE_ENABLE) {
		private_version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PRIVATE_CUSTOM);
		printk("<MELFAS> PRIVATE_CUSTOM_VERSION : 0x%2X\n", private_version_info);
		if (private_version_info < MELFAS_DOWNLAOD_PRIVATE_VERSION || private_version_info == 0xFF)
			dl_enable_bit |= 0x02;
	}

#if 0
	if (MELFAS_PUBLIC_CONFIGURATION_UPDATE_ENABLE || ucInitial_Download) {
		version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PUBLIC_CUSTOM);
		printk("<MELFAS> PUBLIC_CUSTOM_VERSION : 0x%2X\n", version_info);
		if (version_info < MELFAS_DOWNLAOD_PUBLIC_VERSION || version_info == 0xFF)
			dl_enable_bit |= 0x04;
	}
#endif


	/*------------------------
	// Run Download
	//------------------------*/


	for (i = 0; i < 3; i++) {
		if (dl_enable_bit & (1 << i)) {
			if (i < 2) { /* 0: core, 1: private custom */
				nRet = mms100_ISC_download((const UINT8 *) MELFAS_binary, (const UINT16)MELFAS_binary_nLength, (const INT8)i);
				ucSlave_addr = ISC_MODE_SLAVE_ADDRESS;
				ucInitial_Download = FALSE;
			}
			/*else      // 2: public custom
				nRet = mms100_ISC_download( (const UINT8*) MELFAS_binary, (const UINT16)MELFAS_binary_nLength, (const INT8)i);*/
			if (nRet)
				goto fw_error;
#if MELFAS_2CHIP_DOWNLOAD_ENABLE
			nRet = mms100_ISC_Slave_download((const INT8) i); /* Slave Binary data download */
			if (nRet)
				goto fw_error;
#endif
		}
	}

	MELFAS_ROLLBACK_BASEBAND_ISR();                         /* Roll-back Baseband touch interrupt ISR.*/
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();                 /*  Roll-back Baseband watchdog timer */
	return 0;

fw_error:
/*      mcsdl_erase_flash(0);
	mcsdl_erase_flash(1);*/
	return nRet;

}

int mms100_ISC_download_binary_file(void)
{
	int nRet;
	INT8 dl_enable_bit = 0x00;
	INT8 version_info = 0;

	UINT8 *pBinary;
	int nBinary_length;

	/*==================================================
	//
	//   1. Read '.bin file'
	//   2. *pBinary[0]       : Binary data(Core + Private Custom)
	//       *pBinary[1]       : Binary data(Public)
	//         nBinary_length[0] : Firmware size(Core + Private Custom)
	//         nBinary_length[1] : Firmware size(Public)
	//   3. Run mms100_ISC_download(*pBianry,unLength, nMode)
	//       nMode : 0 (Core download)
	//       nMode : 1 (Private Custom download)
	//       nMode : 2 (Public Custom download)
	//
	//==================================================*/

#if 1

	/* TO DO : File Process & Get file Size(== Binary size)
	//                      This is just a simple sample*/

	int fd;
	int nRead;
	mm_segment_t old_fs = 0;
	struct stat fw_bin_stat;

	/*------------------------------
	// Open a file
	//------------------------------*/

	old_fs = get_fs();
	set_fs(get_ds());

	fd = sys_open("/data/MELFAS_FIRMWARE.bin", O_RDONLY, 0);
	if (fd < 0) {
		printk("read data fail\n");
		nRet = MCSDL_RET_FILE_ACCESS_FAILED;
		goto read_fail;
	}

	/*------------------------------
	// Get Binary Size
	//------------------------------*/

	nRet = sys_newstat("/data/MELFAS_FIRMWARE.bin", (struct stat *)&fw_bin_stat);
	if (nRet < 0) {
		printk("new stat fail\n");
		nRet = MCSDL_RET_FILE_ACCESS_FAILED;
		goto fw_mem_alloc_fail;
	}

	nBinary_length = fw_bin_stat.st_size;

	/*------------------------------
	// Memory allocation
	//------------------------------*/
	printk("length ==> %d\n", nBinary_length);

	pBinary = kzalloc(sizeof(char) * (nBinary_length + 1), GFP_KERNEL);
	if (pBinary == NULL) {
		printk("binary is NULL\n");
		nRet = MCSDL_RET_MELLOC_FAILED;
		goto fw_mem_alloc_fail;
	}

	/*------------------------------
	// Read binary file
	//------------------------------*/

	nRead = sys_read(fd, (char __user *)pBinary, nBinary_length);           /* Read binary file */
	if (nRead != nBinary_length) {
		sys_close(fd);                                             /* Close file */
		if (pBinary != NULL)                                   /* free memory alloced.*/
			kfree(pBinary);
		goto fw_mem_alloc_fail;
	}

	/*------------------------------
	// Close file
	//------------------------------*/

#endif

#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR();                  /* Disable Baseband touch interrupt ISR.*/
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();          /* Disable Baseband watchdog timer */

	if (pBinary != NULL && nBinary_length > 0 && nBinary_length < MELFAS_FIRMWARE_MAX_SIZE) {
	} else
		nRet = MCSDL_RET_WRONG_BINARY;

	mms100_ISC_set_ready();

	/*---------------------------------
	// set download enable mode
	//---------------------------------*/
	if (MELFAS_CORE_FIRWMARE_UPDATE_ENABLE) {
		version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_CORE);
		printk("<MELFAS> CORE_VERSION : 0x%2X\n", version_info);
		if (version_info < 0x01 || version_info == 0xFF)
			dl_enable_bit |= 0x01;
	}
	if (MELFAS_PRIVATE_CONFIGURATION_UPDATE_ENABLE) {
		version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PRIVATE_CUSTOM);
		printk("<MELFAS> PRIVATE_CUSTOM_VERSION : 0x%2X\n", version_info);
		if (version_info < 0x01 || version_info == 0xFF)
			dl_enable_bit |= 0x02;
	}
	if (MELFAS_PUBLIC_CONFIGURATION_UPDATE_ENABLE) {
		version_info = mms100_ISC_read_data(MELFAS_FIRMWARE_VER_REG_PUBLIC_CUSTOM);
		printk("<MELFAS> PUBLIC_CUSTOM_VERSION : 0x%2X\n", version_info);
		if (version_info < 0x01 || version_info == 0xFF)
			dl_enable_bit |= 0x04;
	}

	/*------------------------
	// Run Download
	//------------------------*/
	nRet = mms100_ISC_download((const UINT8 *) pBinary, (const UINT16)nBinary_length, 0);
	if (nRet)
		goto fw_error;

	MELFAS_ROLLBACK_BASEBAND_ISR();                 /* Roll-back Baseband touch interrupt ISR.*/
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();         /* Roll-back Baseband watchdog timer */

	kfree(pBinary);

fw_mem_alloc_fail:
	sys_close(fd);
read_fail:
	set_fs(old_fs);
fw_error:
	return nRet;
}

/*------------------------------------------------------------------
//
//      Download function
//
//------------------------------------------------------------------*/

static int mms100_ISC_download(const UINT8 *pBianry, const UINT16 unLength, const UINT8 nMode)
{
	int nRet;
	UINT8 fw_status = 0;

	UINT8 private_flash_start = ISC_PRIVATE_CONFIG_FLASH_START;
	UINT8 public_flash_start = ISC_PUBLIC_CONFIG_FLASH_START;

	UINT8 flash_start[3] = {0, 0, 0};
	UINT8 flash_end[3] =  {31, 31, 31};

	/*---------------------------------
	// Check Binary Size
	//---------------------------------*/
	if (unLength > MELFAS_FIRMWARE_MAX_SIZE) {
		nRet = MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
		goto MCSDL_DOWNLOAD_FINISH;
	}

	/*---------------------------------
	// Make it ready
	//---------------------------------*/
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("<MELFAS> Ready\n");
#endif
	/* mms100_ISC_set_ready(); */

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	if (nMode == 0)
		printk("<MELFAS> Core_firmware_download_via_ISC start!!!\n");
	else if (nMode == 1)
		printk("<MELFAS> Private_Custom_firmware_download_via_ISC start!!!\n");
	else
		printk("<MELFAS> Public_Custom_firmware_download_via_ISC start!!!\n");
#endif

	/*--------------------------------------------------------------
	// INITIALIZE
	//--------------------------------------------------------------*/
	printk("<MELFAS> ISC_DOWNLOAD_MODE_ENTER\n\n");
	mms100_ISC_enter_download_mode();
	mcsdl_delay(MCSDL_DELAY_100MS);

#if ISC_READ_DOWNLOAD_POSITION
	printk("<MELFAS> Read download position.\n\n");
	if (1 << nMode != ISC_CORE_FIRMWARE_DL_MODE) {
		private_flash_start = mms100_ISC_read_data(ISC_PRIVATE_CONFIGURATION_START_ADDR);
		public_flash_start = mms100_ISC_read_data(ISC_PUBLIC_CONFIGURATION_START_ADDR);
	}
#endif

	flash_start[0] = 0;
	flash_end[0] = flash_end[2] = 31;
	flash_start[1] = private_flash_start;
	flash_start[2] = flash_end[1] = public_flash_start;
	printk("<MELFAS> Private Configration start at %2dKB, Public Configration start at %2dKB\n", private_flash_start, public_flash_start);

	mcsdl_delay(MCSDL_DELAY_60MS);

	/*--------------------------------------------------------------
	// FIRMWARE UPDATE MODE ENTER
	//--------------------------------------------------------------*/
	printk("<MELFAS> FIRMWARE_UPDATE_MODE_ENTER\n\n");
	mms100_ISC_firmware_update_mode_enter();
	mcsdl_delay(MCSDL_DELAY_60MS);

	fw_status = mms100_ISC_read_firmware_status();
	if (fw_status == 0x01)
		printk("<MELFAS> Firmware update mode enter success!!!\n");
	else {
		printk("<MELFAS> Error detected!! firmware status is 0x%02x.\n", fw_status);
		nRet = MCSDL_FIRMWARE_UPDATE_MODE_ENTER_FAILED;
		goto MCSDL_DOWNLOAD_FINISH;
	}

	mcsdl_delay(MCSDL_DELAY_60MS);

	/*--------------------------------------------------------------
	// FIRMWARE UPDATE
	//--------------------------------------------------------------*/
	printk("<MELFAS> FIRMWARE UPDATE\n\n");
	nRet = mms100_ISC_firmware_update((UINT8 *)pBianry, (UINT16)unLength, flash_start[nMode], flash_end[nMode]);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/*--------------------------------------------------------------
	// LEAVE FIRMWARE UPDATE MODE
	//--------------------------------------------------------------*/
	printk("<MELFAS> LEAVE FIRMWARE UPDATE MODE\n\n");
	mms100_ISC_leave_firmware_update_mode();
	mcsdl_delay(MCSDL_DELAY_60MS);

#if 0
	fw_status = mms100_ISC_read_firmware_status();
	if (fw_status == 0xFF || fw_status == 0x00)
		printk("<MELFAS> Living firmware update mode success!!!\n");
	else {
		printk("<MELFAS> Error detected!! firmware status is 0x%02x.\n", fw_status);
		nRet = MCSDL_LEAVE_FIRMWARE_UPDATE_MODE_FAILED;
		goto MCSDL_DOWNLOAD_FINISH;
	}
#endif
	nRet = MCSDL_RET_SUCCESS;


MCSDL_DOWNLOAD_FINISH:
#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_ISC_print_result(nRet);		/*Show result*/
#endif

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("<MELMAS> Rebooting\n");
	printk("<MELMAS>  - Fin.\n\n");
#endif
	mms100_ISC_reboot_mcs();
	return nRet;
}

/*static int mms100_ISC_Slave_download(UINT8 nMode)
{
	int nRet;
	//int core_version =0;
	//---------------------------------
	// Make it ready
	//---------------------------------
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("<MELFAS> Ready\n");
#endif

	mms100_ISC_set_ready();

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	if(nMode==0)
		printk("<MELFAS> Core_firmware_slave_download_via_ISC start!!!\n");
	else if(nMode==1)
		printk("<MELFAS> Private_Custom_firmware_slave_download_via_ISC start!!!\n");
	else printk("<MELFAS>
		Public_Custom_firmware_slave_download_via_ISC start!!!\n");
#endif

	//--------------------------------------------------------------
	// INITIALIZE
	//--------------------------------------------------------------
	printk("<MELFAS> ISC_DOWNLOAD_MODE_ENTER\n\n");
	mms100_ISC_enter_download_mode();
	mcsdl_delay(MCSDL_DELAY_100MS);

	//--------------------------------------------------------------
	// Slave download start
	//--------------------------------------------------------------
	mms100_ISC_slave_download_start(nMode+1);
	nRet = mms100_ISC_slave_crc_ok();
	if(nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	nRet = MCSDL_RET_SUCCESS;

MCSDL_DOWNLOAD_FINISH :
#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_ISC_print_result( nRet );                                                         // Show result
#endif

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("<MELMAS> Rebooting\n");
	printk("<MELMAS>  - Fin.\n\n");
#endif
	mms100_ISC_reboot_mcs();
	return nRet;
}
*/

/*------------------------------------------------------------------
//
//      Sub functions
//
//------------------------------------------------------------------*/
static UINT8 mms100_ISC_read_data(UINT8 addr)
{
	UINT32 wordData = 0x00000000;
	UINT8  write_buffer[4];
	UINT8 flash_start;

	mcsdl_i2c_start();
	write_buffer[0] = ucSlave_addr << 1;
	write_buffer[1] = addr; /* command */
	wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16);

	mcsdl_ISC_write_bits(wordData, 16);
	mcsdl_delay(MCSDL_DELAY_60MS);

	mcsdl_i2c_start();
	/* 1byte read */
	wordData = (ucSlave_addr << 1 | 0x01) << 24;
	mcsdl_ISC_write_bits(wordData, 8);
	flash_start = mcsdl_read_byte();
	wordData = (0x01) << 31;
	mcsdl_ISC_write_bits(wordData, 1); /* Nack */
	mcsdl_i2c_stop();
	return flash_start;
}

static void mms100_ISC_enter_download_mode(void)
{
	UINT32 wordData = 0x00000000;
	UINT8  write_buffer[4];

	mcsdl_i2c_start();
	write_buffer[0] = ucSlave_addr << 1; /* slave addr */
	write_buffer[1] = ISC_DOWNLOAD_MODE_ENTER; /* command */
	write_buffer[2] = 0x01; /* sub_command */
	wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16) | (write_buffer[2] << 8);
	mcsdl_ISC_write_bits(wordData, 24);
	mcsdl_i2c_stop();
}

static void mms100_ISC_firmware_update_mode_enter(void)
{
	UINT32 wordData = 0x00000000;
	mcsdl_i2c_start();
	wordData = (ucSlave_addr << 1) << 24 | (0xAE << 16) | (0x55 << 8) | (0x00);
	mcsdl_ISC_write_bits(wordData, 32);
	wordData = 0x00000000;
	mcsdl_ISC_write_bits(wordData, 32);
	mcsdl_ISC_write_bits(wordData, 24);
	mcsdl_i2c_stop();
}

static UINT8 mms100_ISC_firmware_update(UINT8 *_pBinary_reordered, UINT16 _unDownload_size, UINT8 flash_start, UINT8 flash_end)
{
	int      i = 0, j = 0, n, m;
	UINT8 fw_status;

	UINT32 wordData = 0x00000000;
	UINT16 nOffset = 0;
	UINT16 cLength = 8;
	UINT16 CRC_check_buf, CRC_send_buf, IN_data;
	UINT16 XOR_bit_1, XOR_bit_2, XOR_bit_3;

	UINT8  write_buffer[64];

	fw_status = 0;
	nOffset =  0;
	cLength = 8; /*256*/

	printk("<MELFAS> flash start : %2d, flash end : %2d\n", flash_start, flash_end);

	while (flash_start + nOffset < flash_end) {
		CRC_check_buf = 0xFFFF;
		mcsdl_i2c_start();
		write_buffer[0] = ucSlave_addr << 1;
		write_buffer[1] = 0XAE; /* command */
		write_buffer[2] = 0XF1; /* sub_command */
		write_buffer[3] = flash_start + nOffset;

		wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16) | (write_buffer[2] << 8) | write_buffer[3];
		mcsdl_ISC_write_bits(wordData, 32);
		mcsdl_delay(MCSDL_DELAY_100MS);
		mcsdl_delay(MCSDL_DELAY_100MS);

		for (m = 7; m >= 0; m--) {
			IN_data = (write_buffer[3] >> m) & 0x01;
			XOR_bit_1 = (CRC_check_buf & 0x0001) ^ IN_data;
			XOR_bit_2 = XOR_bit_1^(CRC_check_buf >> 11 & 0x01);
			XOR_bit_3 = XOR_bit_1^(CRC_check_buf >> 4 & 0x01);
			CRC_send_buf = (XOR_bit_1 << 4) | (CRC_check_buf >> 12 & 0x0F);
			CRC_send_buf = (CRC_send_buf << 7) | (XOR_bit_2 << 6) | (CRC_check_buf >> 5 & 0x3F);
			CRC_send_buf = (CRC_send_buf << 4) | (XOR_bit_3 << 3) | (CRC_check_buf >> 1 & 0x0007);
			CRC_check_buf = CRC_send_buf;
		}

		for (j = 0 ; j < 32 ; j++) {
			for (i = 0 ; i < cLength ; i++) {
				write_buffer[i*4+3] = _pBinary_reordered[(flash_start+nOffset)*1024+j*32+i*4+0];
				write_buffer[i*4+2] = _pBinary_reordered[(flash_start+nOffset)*1024+j*32+i*4+1];
				write_buffer[i*4+1] = _pBinary_reordered[(flash_start+nOffset)*1024+j*32+i*4+2];
				write_buffer[i*4+0] = _pBinary_reordered[(flash_start+nOffset)*1024+j*32+i*4+3];
				for (n = 0 ; n < 4 ; n++) {
					for (m = 7; m >= 0; m--) {
						IN_data = (write_buffer[i*4+n] >> m) & 0x0001;
						XOR_bit_1 = (CRC_check_buf & 0x0001) ^ IN_data;
						XOR_bit_2 = XOR_bit_1^(CRC_check_buf >> 11 & 0x01);
						XOR_bit_3 = XOR_bit_1^(CRC_check_buf >> 4 & 0x01);
						CRC_send_buf = (XOR_bit_1 << 4) | (CRC_check_buf >> 12 & 0x0F);
						CRC_send_buf = (CRC_send_buf << 7) | (XOR_bit_2 << 6) | (CRC_check_buf >> 5 & 0x3F);
						CRC_send_buf = (CRC_send_buf << 4) | (XOR_bit_3 << 3) | (CRC_check_buf >> 1 & 0x0007);
						CRC_check_buf = CRC_send_buf;
					}
				}
			}

			for (i = 0; i < cLength; i++) {
				wordData = (write_buffer[i*4+0] << 24) | (write_buffer[i*4+1] << 16) | (write_buffer[i*4+2] << 8) | write_buffer[i*4+3];
				mcsdl_ISC_write_bits(wordData, 32);
				mcsdl_delay(MCSDL_DELAY_100US);
			}
		}

		write_buffer[1] =  CRC_check_buf & 0xFF;
		write_buffer[0] = CRC_check_buf >> 8 & 0xFF;

		wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16);
		mcsdl_ISC_write_bits(wordData, 16);
		mcsdl_delay(MCSDL_DELAY_100US);
		mcsdl_i2c_stop();

		fw_status = mms100_ISC_read_firmware_status();
		if (fw_status == 0x03) {
			/* printk("<MELFAS> Firmware update success!!!\n"); */
		} else {
			printk("<MELFAS> Error detected!! firmware status is 0x%02x.\n", fw_status);
			return MCSDL_FIRMWARE_UPDATE_FAILED;
		}
		nOffset += 1;
		printk("<MELFAS> %d KB Downloaded...\n", nOffset);
	}

	return MCSDL_RET_SUCCESS;
}

static UINT8 mms100_ISC_read_firmware_status()
{
	UINT32 wordData = 0x00000000;
	UINT8 fw_status;
	mcsdl_i2c_start();
	/* WRITE 0xAF */
	wordData = (ucSlave_addr << 1) << 24 | (0xAF << 16);
	mcsdl_ISC_write_bits(wordData, 16);
	mcsdl_i2c_stop();
	mcsdl_delay(MCSDL_DELAY_100MS);

	mcsdl_i2c_start();
	/* 1byte read */
	wordData = (ucSlave_addr << 1 | 0x01) << 24;
	mcsdl_ISC_write_bits(wordData, 8);
	fw_status = mcsdl_read_byte();
	wordData = (0x01) << 31;
	mcsdl_ISC_write_bits(wordData, 1); /* Nack */
	mcsdl_i2c_stop();
	return fw_status;
}

#if 0
static void mms100_ISC_slave_download_start(UINT8 nMode)
{
	UINT32 wordData = 0x00000000;
	UINT8  write_buffer[4];

	mcsdl_i2c_start();
	/* WRITE 0xAF */
	write_buffer[0] = ucSlave_addr << 1;
	write_buffer[1] = ISC_DOWNLOAD_MODE; /* command */
	write_buffer[2] = nMode;  /* 0x01: core, 0x02: private custom, 0x03: public custsom*/
	wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16) | (write_buffer[2] << 8);
	mcsdl_ISC_write_bits(wordData, 24);
	mcsdl_i2c_stop();
	mcsdl_delay(MCSDL_DELAY_100MS);
}

static UINT8 mms100_ISC_slave_crc_ok(void)
{
	UINT32 wordData = 0x00000000;
	UINT8 CRC_status = 0;
	UINT8  write_buffer[4];
	UINT8 check_count = 0;

	mcsdl_i2c_start();
	write_buffer[0] = ucSlave_addr << 1;
	write_buffer[1] = ISC_READ_SLAVE_CRC_OK; /* command */
	wordData = (write_buffer[0] << 24) | (write_buffer[1] << 16);

	mcsdl_ISC_write_bits(wordData, 16);

	while (CRC_status != 0x01 && check_count < 200) {/* check_count 200 : 20sec */
		mcsdl_i2c_start();
		/* 1byte read */
		wordData = (ucSlave_addr << 1 | 0x01) << 24;
		mcsdl_ISC_write_bits(wordData, 8);
		CRC_status = mcsdl_read_byte();
		wordData = (0x01) << 31;
		mcsdl_ISC_write_bits(wordData, 1); /*Nack*/
		if (check_count % 10 == 0)
			printk("<MELFAS> %d sec...\n", check_count/10);
		mcsdl_i2c_stop();

		if (CRC_status == 1)
			return MCSDL_RET_SUCCESS;
		else if (CRC_status == 2)
			return MCSDL_RET_ISC_SLAVE_CRC_CHECK_FAILED;
		mcsdl_delay(MCSDL_DELAY_100MS);
		check_count++;
	}

	return MCSDL_RET_ISC_SLAVE_DOWNLOAD_TIME_OVER;
}
#endif

static void mms100_ISC_leave_firmware_update_mode()
{
	UINT32 wordData = 0x00000000;
	mcsdl_i2c_start();
	wordData = (ucSlave_addr << 1) << 24 | (0xAE << 16) | (0x0F << 8) | (0xF0);
	mcsdl_ISC_write_bits(wordData, 32);
	mcsdl_i2c_stop();
}

static void mcsdl_i2c_start(void)
{
	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);
	mcsdl_delay(MCSDL_DELAY_1US);
	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);
	mcsdl_delay(MCSDL_DELAY_1US);

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_1US);

	MCSDL_GPIO_SDA_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_1US);
	MCSDL_GPIO_SCL_SET_LOW();
}

static void mcsdl_i2c_stop(void)
{
	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SCL_SET_OUTPUT(0);
	mcsdl_delay(MCSDL_DELAY_1US);
	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);
	mcsdl_delay(MCSDL_DELAY_1US);

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_1US);
	MCSDL_GPIO_SDA_SET_HIGH();
}

static void mms100_ISC_set_ready(void)
{
	MCSDL_VDD_SET_LOW(); /* power */

	MCSDL_SET_GPIO_I2C();

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);

	MCSDL_RESETB_SET_INPUT();

	MCSDL_CE_SET_HIGH();
	MCSDL_CE_SET_OUTPUT(1);
	mcsdl_delay(MCSDL_DELAY_60MS);		/* Delay for Stable VDD */

	MCSDL_VDD_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_500MS);		/* Delay for Stable VDD */
}

static void mms100_ISC_reboot_mcs(void)
{
	mms100_ISC_set_ready();
}

static UINT8 mcsdl_read_ack(void)
{
	UINT8 pData = 0x00;
	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_INPUT();

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_3US);
	if (MCSDL_GPIO_SDA_IS_HIGH())
		pData = 0x01;
	MCSDL_GPIO_SCL_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_3US);
	return pData;
}

/*static void mcsdl_ISC_read_32bits( UINT8 *pData ){
	int i, j;
	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_INPUT();

	for (i = 3; i >= 0; i--){
		pData[i] = 0;
		for (j = 0; j < 8; j++) {
			pData[i] <<= 1;
			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			if (MCSDL_GPIO_SDA_IS_HIGH())
				pData[i] |= 0x01;
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_3US);
		}
	}
}*/

static UINT8 mcsdl_read_byte(void)
{
	int i, count = 50;
	UINT8 pData = 0x00;
	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_INPUT();

	MCSDL_GPIO_SCL_SET_INPUT();
	while (!MCSDL_GPIO_SCL_IS_HIGH()) {
		if (count == 0) {
			printk("<MELFAS> mcsdl_read_byte :SCL is still low\n");
			break;
		}
		count--;
	}
	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);

	for (i = 0; i < 8; i++) {
		pData <<= 1;
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		if (MCSDL_GPIO_SDA_IS_HIGH())
			pData |= 0x01;
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}
	return pData;
}


static void mcsdl_ISC_write_bits(UINT32 wordData, int nBits)
{
	int i;
	int count = 50;

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);

	for (i = 0; i < nBits; i++) {
		if (wordData & 0x80000000)
			MCSDL_GPIO_SDA_SET_HIGH();
		else
			MCSDL_GPIO_SDA_SET_LOW();
		if (i == 0) {
			MCSDL_GPIO_SCL_SET_INPUT();
			while (!MCSDL_GPIO_SCL_IS_HIGH()) {
				if (count == 0) {
					printk("<MELFAS> mcsdl_ISC_write_bits :SCL is still low\n");
					break;
				}
				count--;
			}
			MCSDL_GPIO_SCL_SET_OUTPUT(1);
			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_3US);
		} else {
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_3US);
		}
		wordData <<= 1;
		if ((i%8) == 7) {
			mcsdl_read_ack(); /* read Ack */
			MCSDL_GPIO_SDA_SET_LOW();
			MCSDL_GPIO_SDA_SET_OUTPUT(0);
		}
	}
}

/*============================================================
//
//      Debugging print functions.
//
//============================================================*/

#ifdef MELFAS_ENABLE_DBG_PRINT

static void mcsdl_ISC_print_result(int nRet)
{
	if (nRet == MCSDL_RET_SUCCESS) {
		printk("<MELFAS> Firmware downloading SUCCESS.\n");
	} else {
		printk("<MELFAS> Firmware downloading FAILED  :  ");
		switch (nRet) {
		case MCSDL_RET_SUCCESS:
			printk("<MELFAS> MCSDL_RET_SUCCESS\n");
			break;
		case MCSDL_FIRMWARE_UPDATE_MODE_ENTER_FAILED:
			printk("<MELFAS> MCSDL_FIRMWARE_UPDATE_MODE_ENTER_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_VERIFY_FAILED:
			printk("<MELFAS> MCSDL_RET_PROGRAM_VERIFY_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_SIZE_IS_WRONG:
			printk("<MELFAS> MCSDL_RET_PROGRAM_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_VERIFY_SIZE_IS_WRONG:
			printk("<MELFAS> MCSDL_RET_VERIFY_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_WRONG_BINARY:
			printk("<MELFAS> MCSDL_RET_WRONG_BINARY\n");
			break;
		case MCSDL_RET_READING_HEXFILE_FAILED:
			printk("<MELFAS> MCSDL_RET_READING_HEXFILE_FAILED\n");
			break;
		case MCSDL_RET_FILE_ACCESS_FAILED:
			printk("<MELFAS> MCSDL_RET_FILE_ACCESS_FAILED\n");
			break;
		case MCSDL_RET_MELLOC_FAILED:
			printk("<MELFAS> MCSDL_RET_MELLOC_FAILED\n");
			break;
		case MCSDL_RET_ISC_SLAVE_CRC_CHECK_FAILED:
			printk("<MELFAS> MCSDL_RET_ISC_SLAVE_CRC_CHECK_FAILED\n");
			break;
		case MCSDL_RET_ISC_SLAVE_DOWNLOAD_TIME_OVER:
			printk("<MELFAS> MCSDL_RET_ISC_SLAVE_DOWNLOAD_TIME_OVER\n");
			break;
		case MCSDL_RET_WRONG_MODULE_REVISION:
			printk("<MELFAS> MCSDL_RET_WRONG_MODULE_REVISION\n");
			break;
		default:
			printk("<MELFAS> UNKNOWN ERROR. [0x%02X].\n", nRet);
			break;
		}
		printk("\n");
	}
}

#endif
