
#include "snfc_driver.h"

struct snfc_gp snfc_gpios;

static int __devinit snfc_platdrv_probe(struct platform_device *pdev)
{
    int rc=0;

    struct device_node *np;

    np = pdev->dev.of_node;

    rc = snfc_uart_control_probe(np);
    rc |= snfc_intu_probe(np);
    rc |= snfc_rfs_probe(np);
    rc |= snfc_avail_poll_probe(np);

    return rc;
}

static int snfc_platdrv_remove(struct platform_device *pdev)
{
    snfc_uart_control_remove();
    snfc_intu_remove();
    snfc_rfs_remove();
    snfc_avail_poll_remove();
    
    return 0;
}
static const struct of_device_id sony_uart_dev_id[] = {
	{
		.compatible = "sony,driver",
	},
};

static struct platform_driver snfc_driver = {
    .driver = {
        .name = "snfc_driver",    
        .of_match_table	= of_match_ptr(sony_uart_dev_id),
    },
    .probe = snfc_platdrv_probe,
    .remove = snfc_platdrv_remove,
};

static int snfc_driver_init(void)
{
	int rc=0;

	SNFC_DEBUG_MSG_LOW("[snfc_driver] snfc_uart_control_init - start \n");

    rc = platform_driver_register(&snfc_driver);
	if (rc)
	{
		SNFC_DEBUG_MSG("[snfc_driver] FAIL!! can not register snfc_uart_con_driver \n");
		return rc;
	}  

	SNFC_DEBUG_MSG_LOW("[snfc_driver] snfc_uart_control_init - end \n");

	return 0;  	
}

/*
* Description : 
* Input : 
* Output : 
*/
static void snfc_driver_exit(void)
{
  SNFC_DEBUG_MSG_LOW("[snfc_driver] snfc_uart_control_exit - start \n");

  /* deregister the device file */
  platform_driver_unregister(&snfc_driver);
  
  SNFC_DEBUG_MSG_LOW("[snfc_driver] snfc_uart_control_exit - end \n");
}

module_init(snfc_driver_init);
module_exit(snfc_driver_exit);

MODULE_DEVICE_TABLE(of, sony_uart_dev_id);

MODULE_LICENSE("Dual BSD/GPL");

