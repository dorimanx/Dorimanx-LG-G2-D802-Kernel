/*
 *  snfc_uart_collision_control.h
 *  
 */
#ifndef __SNFC_UART_COLLISION_CONTROL_H__
#define __SNFC_UART_COLLISION_CONTROL_H__
/*
 *  Include header files
 */
#include "snfc_common.h"
#include "snfc_gpio.h"
#include "snfc_i2c.h"


 /*
 *	Defines
 */
#define IOCTL_SNFC_MAGIC	254

#define IOCTL_SNFC_START_SETTING		_IO(IOCTL_SNFC_MAGIC,0)
#define IOCTL_SNFC_START_AUTOPOLL		_IO(IOCTL_SNFC_MAGIC,1)
#define IOCTL_SNFC_START_RW				_IO(IOCTL_SNFC_MAGIC,2)
#define IOCTL_SNFC_START_TARGET			_IO(IOCTL_SNFC_MAGIC,3)
#define IOCTL_SNFC_START_INTU			_IO(IOCTL_SNFC_MAGIC,4)
#define IOCTL_SNFC_START_WAITSIMBOOT	_IO(IOCTL_SNFC_MAGIC,5)
#define IOCTL_SNFC_HSEL_UP				_IO(IOCTL_SNFC_MAGIC,6)
#define IOCTL_SNFC_HSEL_DOWN			_IO(IOCTL_SNFC_MAGIC,7)
#define IOCTL_SNFC_PON_UP				_IO(IOCTL_SNFC_MAGIC,8)
#define IOCTL_SNFC_PON_DOWN				_IO(IOCTL_SNFC_MAGIC,9)
#define IOCTL_SNFC_BOOT_CEN_HI			_IO(IOCTL_SNFC_MAGIC,10)
#define IOCTL_SNFC_BOOT_CEN_LO			_IO(IOCTL_SNFC_MAGIC,11)
#define IOCTL_SNFC_END					_IO(IOCTL_SNFC_MAGIC,12)

#define IOCTL_SNFC_MAXNR			12

typedef enum _e_snfc_uart_status {
	UART_STATUS_KOTO_OFF = 0,
	UART_STATUS_READY,
	UART_STATUS_FOR_FELICA,
	UART_STATUS_FOR_NFC,
	UART_STATUS_NONE,
} _e_snfc_uart_status;


 /*
  *   Function prototype
  */
_e_snfc_uart_status __snfc_uart_control_get_uart_status(void);

 /*
  *   Internal definition
  */
 /*
 *	Internal variables
 */
int snfc_uart_control_probe(struct device_node *np);
void snfc_uart_control_remove(void);

#endif	//__SNFC_UART_COLLISION_CONTROL_H__
