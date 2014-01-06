/*
 *	snfc_hw_rev.c
 *  
 */
 
/*
 *    Include header files
 */
#include "snfc_gpio.h"
#include <mach/board_lge.h>

/*
 *  Defines
 */

/*
*    Internal definition
*/
  
/*
 *	  Internal variables
 */
static int isopen_snfchwrev = 0; // 0 : No open 1 : Open

/*
 *    Function prototypes
 */

/*
 *    Function definitions
 */

/*
* Description :
* Input : 
* Output :
*/
static int snfc_hw_rev_open (struct inode *inode, struct file *fp)
{
	int rc = 0;

	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_open - start \n");

	if(isopen_snfchwrev == 1)
	{
		SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_open - already open \n");
		return 0;
	}
	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_open - end \n");

	isopen_snfchwrev = 1;

	return rc;
}


/*
* Description :
* Input : 
* Output :
*/
static ssize_t snfc_hw_rev_read(struct file *pf, char *pbuf, size_t size, loff_t *pos)
{
	int rc = 0;
	int rev = lge_get_board_revno();
	int retrev;
	
	switch(rev)
	{
		case HW_REV_A:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.a\n");
			retrev = 'a';
			break;
		case HW_REV_B:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.b\n");
			retrev = 'b';
			break;
		case HW_REV_C:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.c\n");
			retrev = 'c';
			break;
		case HW_REV_D:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.d\n");
			retrev = 'd';
			break;			
		case HW_REV_E:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.e\n");
			retrev = 'e';
			break;
		case HW_REV_F:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.f\n");
			retrev = 'f';
			break;
		case HW_REV_G:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.g\n");
			retrev = 'g';
			break;
		case HW_REV_H:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.h\n");
			retrev = 'g';
			break;			
		case HW_REV_1_0:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.1.0\n");
			retrev = '0';
			break;
		case HW_REV_1_1:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.1.1\n");
			retrev = '1';
			break;		
		case HW_REV_1_2:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as rev.1.2\n");
			retrev = '2';
			break;				
		default:
			SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] read hw as unknown\n");
	}
	
	rc = copy_to_user((void*)pbuf, (void*)&retrev, size);
	if(rc)
	{
		SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] ERROR -  copy_to_user \n");
		return rc;
	}

	return size;
}


/*
* Description : 
* Input : 
* Output : 
*/
static int snfc_hw_rev_release (struct inode *inode, struct file *fp)
{
	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_release - start \n");

	if(isopen_snfchwrev ==0)
	{
		SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_release - not open \n");
		return -1;
	}

	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_release - end \n");

	isopen_snfchwrev = 0;

	return 0;

}

static struct file_operations snfc_hw_rev_fops = 
{
  .owner    = THIS_MODULE,
  .open      = snfc_hw_rev_open,
  .read		= snfc_hw_rev_read,
  .release  = snfc_hw_rev_release,
};

static struct miscdevice snfc_hw_rev_device = 
{
  .minor = 124,
  .name = "snfc_hw_rev",
  .fops = &snfc_hw_rev_fops,
};

static int snfc_hw_rev_init(void)
{
	int rc = 0;

	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_init - start \n");

	/* Register the device file */
	rc = misc_register(&snfc_hw_rev_device);
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_hw_rev] FAIL!! can not register snfc_hw_rev_init \n");
		return rc;
	}

	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_hw_rev_init - end \n");

	return 0;
}

static void snfc_hw_rev_exit(void)
{
	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_pon_exit - start \n");

	/* Deregister the device file */
	misc_deregister(&snfc_hw_rev_device);

	SNFC_DEBUG_MSG_MIDDLE("[snfc_hw_rev] snfc_pon_exit - end \n");
}

module_init(snfc_hw_rev_init);
module_exit(snfc_hw_rev_exit);

MODULE_LICENSE("Dual BSD/GPL");

