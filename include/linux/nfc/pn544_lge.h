/* lge/include/nfc_nxp_pn544pn65n.h
 *
 * Copyright (C) 2010 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _PN544_LGE_H_
#define _PN544_LGE_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include <mach/board_lge.h>

#define PN544_MAGIC	0xE9


#define PN544_DRV_NAME      "pn544"    
//#define NFC_GPIO_VEN	47		// byunggu       
//#define NFC_GPIO_IRQ	59    	// byunggu       
//#define NFC_GPIO_FIRM	48    	// byunggu       
//#define NFC_I2C_SLAVE_ADDR 	0x28    // byunggu       



/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN544_SET_PWR	_IOW(PN544_MAGIC, 0x01, unsigned int)

#define PN544_HW_REVISION _IOR(PN544_MAGIC, 0x02, unsigned int)

struct pn544_i2c_platform_data {
	unsigned int sda_gpio;
	unsigned int scl_gpio;
	unsigned int irq_gpio;
	unsigned int ven_gpio;
	unsigned int firm_gpio;
};


struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

struct pn544_gpio {
	unsigned int 		sda_gpio;	// byunggu
	unsigned int		scl_gpio;	// byunggu
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;	
};

#define LGE_NFC_READ_IRQ_MODIFY//DY_TEST

/* seokmin added for debugging */
#define PN544_INTERRUPT_CMD	2
#define PN544_READ_POLLING_CMD 3

#if defined(CONFIG_LGE_NFC_DEBUG_MESSAGE)
#define dprintk(fmt, args...) printk(fmt, ##args)
#else
#define dprintk(fmt, args...) do{ } while(0)
#endif

#endif /* _PN544_LGE_H_ */

