/*
* snfc_i2c.c
*
*/

/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/syscalls.h>
#include <linux/i2c-dev.h>

#include "snfc_i2c.h"

/*
 *   INTERNAL DEFINITION
 */

#define I2C_SNFC_SLAVE_ADDRESS     0x56
#define I2C_STATUS_LOOP_MAX_CNT     0xFFFFFF

/*
 *   INTERNAL VARIABLE
 */

static int fd = -1;

_e_snfc_i2c_status g_i2c_status = I2C_STATUS_NO_USE;

void __snfc_i2c_control_set_status(_e_snfc_i2c_status i2c_status)
{
//	_e_snfc_i2c_status current_status = g_i2c_status;

	g_i2c_status = i2c_status;

	SNFC_DEBUG_MSG_MIDDLE("[snfc_i2c_control] i2c status %d -> %d\n", current_status, g_i2c_status );

	return;
}

EXPORT_SYMBOL(__snfc_i2c_control_set_status);
/*
* Description : 
* Input : 
* Output : 
*/
_e_snfc_i2c_status __snfc_i2c_control_get_status(void)
{
	return g_i2c_status;
}
EXPORT_SYMBOL(__snfc_i2c_control_get_status);

/*
 *   FUNCTION DEFINITION
 */

/*
* Description : 
* Input :
* Output :
*/
int snfc_i2c_open (void)
{
	mm_segment_t old_fs = get_fs();
	int i;

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] snfc_i2c_open\n");

	for(i=0; i<I2C_STATUS_LOOP_MAX_CNT; i++)
	{
		if(__snfc_i2c_control_get_status() != I2C_STATUS_FOR_FELICA)
			break;
		usleep(100);
	}
	//while(__snfc_i2c_control_get_uart_status() == I2C_STATUS_FOR_FELICA)
	//	usleep(100);
	
	__snfc_i2c_control_set_status(I2C_STATUS_FOR_NFC);

	set_fs(KERNEL_DS);
	fd = sys_open("/dev/i2c-0", O_RDWR|O_NONBLOCK, 0);
	//fd = sys_open("/dev/snfc_i2c", O_RDWR|O_NONBLOCK, 0);  
	if (fd < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_open (/dev/snfc_i2c): %d \n", fd);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return fd;
	}

	set_fs(old_fs);

	return 0;
}
/*
* Description : 
* Input :
* Output :
*/
int snfc_i2c_release (void)
{
	int rc = 0;
	mm_segment_t old_fs = get_fs();

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] snfc_i2c_release\n");

	set_fs(KERNEL_DS);
	rc = sys_close(fd);
	if (rc < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_release : %d \n", rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}
	set_fs(old_fs);

	__snfc_i2c_control_set_status(I2C_STATUS_READY);

	return 0;
}

/*
* Description : 
* Input :
* Output :
*/
int snfc_i2c_set_slave_address (unsigned char slave_address)
{
	int rc = -1;

	rc = sys_ioctl(fd, I2C_SLAVE, slave_address>>1); // 7-bit address

	if (rc < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - sys_ioctl : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] slave address : 0x%02x\n",slave_address>>1);

	return 0;
}

/*
* Description : 
* Input :
* Output :
*/
int snfc_i2c_read(unsigned char reg, unsigned char *buf, size_t count)
{
	ssize_t rc = 0;
	mm_segment_t old_fs = get_fs();
	int retry;

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] snfc_i2c_read\n");

	set_fs(KERNEL_DS);

	/* dev/i2c-0 device file open */
	for(retry=0;retry<100000;retry++)
	{
		rc = snfc_i2c_open();
		if(rc == 0)
			break;
		else
			usleep(100);
	}  
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_open : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* Set slave address */
	rc = snfc_i2c_set_slave_address(I2C_SNFC_SLAVE_ADDRESS);
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_set_slave_address : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* set register address */
	rc = sys_write(fd, &reg, 1);
	if (rc < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - sys_write : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* read register data */
	rc = sys_read(fd, buf, count);

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] read data : 0x%02x \n",*buf);

	if (rc < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - sys_read : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* release i2c */
	rc = snfc_i2c_release();
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - felica_i2c_release : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	set_fs(old_fs);
	return 0;
}


/*
* Description : 
* Input :
* Output :
*/
int snfc_i2c_write(unsigned char reg, unsigned char *buf, size_t count)
{
	ssize_t rc = 0;
	unsigned char write_buf[2];
	mm_segment_t old_fs = get_fs();

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] snfc_i2c_write\n");

	set_fs(KERNEL_DS);

	/* dev/i2c-0 device file open */
	rc = snfc_i2c_open();
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_open : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* set slave address */
	rc = snfc_i2c_set_slave_address(I2C_SNFC_SLAVE_ADDRESS);
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_set_slave_address : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* set register  */  
	memset(write_buf,0x00,2*sizeof(unsigned char));
	write_buf[0] = reg;
	write_buf[1] = *buf;

	SNFC_DEBUG_MSG_LOW("[snfc_i2c] write_buf[0][1] : 0x%02x 0x%02x \n",write_buf[0],write_buf[1]);

	/* write data */    
	rc = sys_write(fd, write_buf, 2);
	if (rc < 0)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - sys_write : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	/* release i2c */
	rc = snfc_i2c_release();
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_i2c] ERROR - snfc_i2c_release : %d \n",rc);
		__snfc_i2c_control_set_status(I2C_STATUS_READY);
		return rc;
	}

	set_fs(old_fs);

	return 0;
}

