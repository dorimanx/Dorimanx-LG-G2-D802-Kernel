/*
 *  felicacommon.h
 *
 */

#ifndef __FELICACOMMON_H__
#define __FELICACOMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/module.h>/*THIS_MODULE*/
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>/* printk() */
#include <linux/types.h>/* size_t */
#include <linux/miscdevice.h>/*misc_register, misc_deregister*/
#include <linux/vmalloc.h>
#include <linux/fs.h>/*file_operations*/
#include <linux/delay.h>/*mdelay*/
#include <linux/irq.h>

#include <asm/uaccess.h>/*copy_from_user*/
#include <asm/io.h>/*static*/
#include <linux/gpio.h>
#include <mach/socinfo.h>
#include <mach/board_lge.h>/*lge_get_board_revno*/

/*
 *  DEFINE
 */

enum{
  FELICA_UART_NOTAVAILABLE = 0,
  FELICA_UART_AVAILABLE,
};

/* Check NFC code - drivers/nfc/snfc */
typedef enum _e_snfc_uart_status {
	UART_STATUS_KOTO_OFF = 0,
	UART_STATUS_READY,
	UART_STATUS_FOR_FELICA,
	UART_STATUS_FOR_NFC,
	UART_STATUS_NONE,
} _e_snfc_uart_status;

typedef enum _e_snfc_i2c_status {
             I2C_STATUS_NO_USE = 0,
             I2C_STATUS_READY,
             I2C_STATUS_FOR_FELICA,
             I2C_STATUS_FOR_NFC,
             I2C_STATUS_NONE,
} _e_snfc_i2c_status;


/* function feature */
#define FELICA_LED_SUPPORT
#define FELICA_NFC_INTERFACE

/* debug message */
#define FEATURE_DEBUG_HIGH
//#define FEATURE_DEBUG_MED
//#define FEATURE_DEBUG_LOW
#define RXTX_LOG_ENABLE
#define FELICA_DEBUG_MSG printk

/* felica */
#define FELICA_NAME    "felica"

/* felica_pon */
#define FELICA_PON_NAME    "felica_pon"

/* felica_cen */
#define FELICA_CEN_NAME    "felica_cen"

/* felica_rfs */
#define FELICA_RFS_NAME    "felica_rfs"

/* felica_cal */
#define FELICA_CAL_NAME    "felica_cal"

/* felica I2C */
#define FELICA_I2C_NAME    "felica_i2c"

/* felica_int */
#define FELICA_RWS_NAME    "felica_rws"


/* minor number */
#define MINOR_NUM_FELICA_PON 250
#define MINOR_NUM_FELICA     251
#define MINOR_NUM_FELICA_CEN 252
#define MINOR_NUM_FELICA_RFS 253
#define MINOR_NUM_FELICA_RWS 254

/*
 *  EXTERNAL VARIABLE
*/
/* Must check each model's path in 'android/system/core/rootdir/init.rc'file */
#define FELICA_LD_LIBRARY_PATH "LD_LIBRARY_PATH=/vendor/lib:/system/lib"

#define FELICA_PATH "PATH=/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin"

/* JB */
//#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework_ext.jar:/system/framework/framework2.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/com.lge.core.jar"

/* JB MR1 */
//#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework2.jar:/system/framework/telephony-common.jar:/system/framework/mms-common.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/telephony-msim.jar:/system/framework/qcmediaplayer.jar:/system/framework/WfdCommon.jar:/system/framework/com.lge.core.jar:/system/framework/qcrilhook.jar:/system/framework/security-bridge.jar:/system/framework/qsb-port.jar:/system/framework/oem-services.jar"
//#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework2.jar:/system/framework/telephony-common.jar:/system/framework/mms-common.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/telephony-msim.jar:/system/framework/com.lge.core.jar:/system/framework/qcmediaplayer.jar:/system/framework/oem-services.jar"
//#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework2.jar:/system/framework/telephony-common.jar:/system/framework/mms-common.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/telephony-msim.jar:/system/framework/com.lge.core.jar:/system/framework/qcmediaplayer.jar:/system/framework/security-bridge.jar:/system/framework/oem-services.jar"
//#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework2.jar:/system/framework/telephony-common.jar:/system/framework/mms-common.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/telephony-msim.jar:/system/framework/com.lge.core.jar:/system/framework/oem-services.jar"
#define FELICA_BOOTCLASSPATH "BOOTCLASSPATH=/system/framework/core.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/framework2.jar:/system/framework/telephony-common.jar:/system/framework/mms-common.jar:/system/framework/android.policy.jar:/system/framework/services.jar:/system/framework/apache-xml.jar:/system/framework/telephony-msim.jar:/system/framework/com.lge.core.jar:/system/framework/qcmediaplayer.jar:/system/framework/oem-services.jar"

/* Must check each model's VALUE from UART developer */
#define FELICA_IC2_NAME "/dev/i2c-0"  // dev/i2c-84
#define FELICA_UART_NAME "/dev/ttyHSL2" // dev/ttyHSL1

/*
 *  DEFINE FUNCTIONS
 */
void lock_felica_wake_lock(void);
void unlock_felica_wake_lock(void);
void init_felica_wake_lock(void);
void destroy_felica_wake_lock(void);

int get_felica_uart_status(void);
void set_felica_uart_status(_e_snfc_uart_status uart_status);
_e_snfc_i2c_status get_felica_i2c_status(void);
void set_felica_i2c_status(_e_snfc_i2c_status i2c_status);

#if defined(CONFIG_LGE_FELICA_NFC)&& defined(FELICA_NFC_INTERFACE)
extern _e_snfc_uart_status __snfc_uart_control_get_uart_status(void);
extern void __snfc_uart_control_set_uart_status(_e_snfc_uart_status uart_status);
extern _e_snfc_i2c_status __snfc_i2c_control_get_status(void);
extern void __snfc_i2c_control_set_status(_e_snfc_i2c_status i2c_status);
#endif

#ifdef FELICA_LED_SUPPORT
void lock_felica_rfs_wake_lock(void);
void unlock_felica_rfs_wake_lock(void);
void init_felica_rfs_wake_lock(void);
void destroy_felica_rfs_wake_lock(void);
#endif


#ifdef __cplusplus
}
#endif

#endif // __FELICACOMMON_H__
