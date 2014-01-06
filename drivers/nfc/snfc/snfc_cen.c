/*
 *  felica_cen.c
 *  
 */
 
/*
 *    Include header files
 */
//#include <linux/module.h>
#include <linux/kernel.h>


#include "snfc_cen.h"

/*
 *    Internal definition
 */

 //#define FEATURE_DEBUG_LOW
#define SNFC_I2C_SLAVE_ADDRESS  0x56
#define SNFC_I2C_REG_ADDRSS_01  0x01
#define SNFC_I2C_REG_ADDRSS_02  0x02

static DEFINE_MUTEX(nfc_cen_mutex);

static int isopen = 0; // 0 : No open 1 : Open

/*
 *    Function definition
 */

/*
* Description:
* Input:
* Output:
*/
static int snfc_cen_open (struct inode *inode, struct file *fp)
{
	if(1 == isopen)
	{
	 	SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_open - already open \n");
	 	return 0;
	}
	else
	{
	 	SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_open - start \n");
	 	isopen = 1;
	}
	return 0;
}


/*
 * Description:
 * Input:
 * Output:
 */
static int snfc_cen_release (struct inode *inode, struct file *fp)
{


	if(0 == isopen)
	{
		SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_release - not open \n");

		return -1;
	}
	else
	{
		SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_release - start \n");

		isopen = 0;
	}

	return 0;
}


/*
 * Description:
 * Input:
 * Output:
 */
static ssize_t snfc_cen_read(struct file *fp, char *buf, size_t count, loff_t *pos)
{
  unsigned char read_buf = 0x00;
  char snfc_cen = -1, rc = -1;

  SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_read - start \n");

/* Check error */
  if(NULL == fp)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] ERROR fp \n");
    return -1;    
  }

  if(NULL == buf)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] ERROR buf \n");
    return -1;    
  }
  
  if(1 != count)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] ERROR count, count = %d \n", count);
    //return -1;    
  }

  if(NULL == pos)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] ERROR file \n");
    //return -1;    
  }


  mutex_lock(&nfc_cen_mutex);
  rc = snfc_i2c_read(0x02, &read_buf, 1);
  mutex_unlock(&nfc_cen_mutex);

  if(rc)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] snfc_i2c_read : %d \n",rc);
    return -1;
  }

  // check bit 7(locken)
  if(read_buf&0x01)  // unlock
  {
    SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][read] CEN = High (UNLOCK) \n");
    snfc_cen = (char)GPIO_HIGH_VALUE;
  }
  else  // lock
  {
    SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][read] CEN = Low (LOCK) \n");
    snfc_cen = (char)GPIO_LOW_VALUE;
  }

  rc = copy_to_user(buf, &snfc_cen, count);
  if(rc)
  {
    SNFC_DEBUG_MSG("[snfc_cen][read] ERROR - copy_from_user \n");
    return -1;
  }

  SNFC_DEBUG_MSG_LOW("[snfc_cen][read] snfc_cen_read - end \n");

  return 1;
}

/*
 * Description:
 * Input:
 * Output:
 */
static ssize_t snfc_cen_write(struct file *fp, const char *buf, size_t count, loff_t *pos)
{
	unsigned char write_buf = 0x00/*, read_buf = 0x00*/;
	int rc = -1;


	SNFC_DEBUG_MSG_LOW("[snfc_cen][write] snfc_cen_write - start \n");

	/* Check error */
	if(NULL == fp)
	{
		SNFC_DEBUG_MSG("[snfc_cen][write] ERROR file \n");
		return -1;    
	}

	if(NULL == buf)
	{
		SNFC_DEBUG_MSG("[snfc_cen][write] ERROR buf \n");
		return -1;    
	}

	if(1 != count)
	{
		SNFC_DEBUG_MSG("[snfc_cen][write] ERROR count, count = %d \n", count);
		//return -1;    
	}

	if(NULL == pos)
	{
		SNFC_DEBUG_MSG("[snfc_cen][write] ERROR file \n");
		//return -1;    
	}

	/* copy from user data */
	rc = copy_from_user(&write_buf, buf, count);
	if(rc)
	{
		SNFC_DEBUG_MSG("[snfc_cen][write] ERROR - copy_from_user \n");
		return -1;
	}

	SNFC_DEBUG_MSG_LOW("[snfc_cen][write] copy_from_user(%d) \n",*buf);

	/* check user data */
	if(*buf == 1) 
	{
		SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][write] CEN = High (UNLOCK) \n");
		
		write_buf = 0x81; // set unlock  
		mutex_lock(&nfc_cen_mutex);	
		rc = snfc_i2c_write(0x02, &write_buf, 1);
		mutex_unlock(&nfc_cen_mutex);   
		mdelay(2);		
	}
	else if(*buf == 0) 
	{
		SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][write] CEN = Low (LOCK) \n");
		
		write_buf = 0x80; // set lock
		mutex_lock(&nfc_cen_mutex);
		rc = snfc_i2c_write(0x02, &write_buf, 1);
		mutex_unlock(&nfc_cen_mutex);  
		mdelay(2);				
	}
	else if(*buf == 2)
	{
		write_buf = 0x80; // set lock
		mutex_lock(&nfc_cen_mutex);
		rc = snfc_i2c_write(0x02, &write_buf, 1);
		mutex_unlock(&nfc_cen_mutex);  
		SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][write] CEN = Low & Hgh(LOCK) \n");
	
		mdelay(1);	

		write_buf = 0x81; // set unlock
		mutex_lock(&nfc_cen_mutex);
		rc = snfc_i2c_write(0x02, &write_buf, 1);
		mutex_unlock(&nfc_cen_mutex);  
		SNFC_DEBUG_MSG_MIDDLE("[snfc_cen][write] CEN = Low & Hgh(UNLOCK) \n");	
	}

	SNFC_DEBUG_MSG_LOW("[snfc_cen][write] snfc_cen_write - end \n");

	return 1;
}

static struct file_operations snfc_cen_fops = 
{
  .owner    = THIS_MODULE,
  .open    = snfc_cen_open,
  .read    = snfc_cen_read,
  .write    = snfc_cen_write,
  .release  = snfc_cen_release,
};

static struct miscdevice snfc_cen_device = 
{
  .minor = 122,
  .name = "snfc_cen",
  .fops = &snfc_cen_fops
};

/*
 * Description:
 * Input:
 * Output:
 */
static int snfc_cen_init(void)
{
  int rc = -1;

  SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_init - start \n");

  /* register the device file */
  rc = misc_register(&snfc_cen_device);
  if (rc < 0)
  {
    SNFC_DEBUG_MSG("[snfc_cen] FAIL!! can not register snfc_cen \n");
    return rc;
  }

  SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_init - end \n");

  return 0;
}

/*
 * Description:
 * Input:
 * Output:
 */
static void snfc_cen_exit(void)
{
  SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_exit - start \n");

  /* deregister the device file */
  misc_deregister(&snfc_cen_device);

  SNFC_DEBUG_MSG_LOW("[snfc_cen] snfc_cen_exit - end \n");
}

module_init(snfc_cen_init);
module_exit(snfc_cen_exit);

MODULE_LICENSE("Dual BSD/GPL");

