/*
 *  snfc_uart_collsion_control.c
 *
 */
 /*
  *    Include header files
  */
#include "snfc_uart_collision_control.h"

/*
*       Define
*/

/*
*       Internal definitions
*/
/*
*       Internal variables
*/
static int isopen_snfcuartcontrol = 0; // 0 : No open 1 : Opend
_e_snfc_uart_status g_uartcollisoncontrol = UART_STATUS_KOTO_OFF;
static int gpio_init = 0;
static int forced_hsel_up_flag=0;
static int forced_pon_up_flag=0;
int koto_abnormal=0;
static int autopoll_status = 0;
/*
 *      Function definitions
 */

/*
* Description : open uart collision control
* Input :
* Output :
*/
void __snfc_uart_control_set_uart_status(_e_snfc_uart_status uart_status)
{
        _e_snfc_uart_status current_status = g_uartcollisoncontrol;

        if(current_status == uart_status)
                return;

        g_uartcollisoncontrol = uart_status;
        SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] uart status %d -> %d\n", current_status, g_uartcollisoncontrol );

        return;
}

EXPORT_SYMBOL(__snfc_uart_control_set_uart_status);
/*
* Description : open uart collision control
* Input :
* Output :
*/
_e_snfc_uart_status __snfc_uart_control_get_uart_status(void)
{
        return g_uartcollisoncontrol;
}
EXPORT_SYMBOL(__snfc_uart_control_get_uart_status);
/*
* Description : open uart collision control
* Input :
* Output :
*/
static int snfc_uart_control_open(struct inode *inode, struct file *fp)
{
        int rc = 0;

        if(isopen_snfcuartcontrol == 1)
        {
                SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - already open \n");
                return 0;
        }
        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - start \n");
        isopen_snfcuartcontrol = 1;

        if(gpio_init ==0)
        {
            rc = gpio_request(snfc_get_hsel_gpio_num(), "snfc_hsel");
            if (rc){
                    SNFC_DEBUG_MSG("[snfc_intu_poll] gpio_request snfc_hsel fail\n");
            }
            rc = gpio_request(GPIO_SNFC_PON, "snfc_pon");
            if (rc){
                    SNFC_DEBUG_MSG("[snfc_intu_poll] gpio_request snfc_pon fail\n");
            }

            //snfc_gpio_open(GPIO_SNFC_HSEL,GPIO_DIRECTION_OUT,GPIO_LOW_VALUE);
            //snfc_gpio_open(GPIO_SNFC_PON,GPIO_DIRECTION_OUT,GPIO_LOW_VALUE);
            SNFC_DEBUG_MSG_LOW("[snfc_uart_control] GPIO_SNFC_PON = %d, GPIO_SNFC_HSEL = %d\n",
                    snfc_gpio_read(GPIO_SNFC_PON),snfc_gpio_read(snfc_get_hsel_gpio_num()) );

            rc = gpio_request(snfc_get_hvdd_gpio_num(),"snfc_hvdd");
            if(rc){
                SNFC_DEBUG_MSG("[snfc_intu_poll] gpio_request snfc_hvdd fail\n");
            }
            snfc_gpio_write(snfc_get_hvdd_gpio_num(), GPIO_HIGH_VALUE);

            gpio_init = 1;
        }
        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - end \n");

        return rc;
}

/*
* Description :
* Input :
* Output :
*/
static int snfc_uart_control_release (struct inode *inode, struct file *fp)
{
        if(isopen_snfcuartcontrol == 0)
        {
                SNFC_DEBUG_MSG("[snfc_uart_control] snfc_uart_control_release - not open \n");
                return -1;
        }

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_release - start \n");

        isopen_snfcuartcontrol = 0;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_release - end \n");

        return 0;
}
/*
* Description :
* Input :
* Output :
*/
static long snfc_uart_control_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
        //ioctl_buf *k_buf;
        //int i,err;
        int size;
        _e_snfc_uart_status current_status;
        int autopoll_wait_cnt;
        int break_cnt;
        unsigned char write_buf = 0x00/*, read_buf = 0x00*/;
        int rc =0;

        size = _IOC_SIZE(cmd);
        SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] snfc_uart_control_ioctl - start,cmd =%d\n", cmd);

        current_status = __snfc_uart_control_get_uart_status();
        if( current_status == UART_STATUS_FOR_FELICA )
        {
                SNFC_DEBUG_MSG("[snfc_uart_control] snfc_uart_control_ioctl, UART is used to FeliCa\n");
                return -1;
        }

        __snfc_uart_control_set_uart_status(UART_STATUS_FOR_NFC);

        switch(cmd)
        {
                case IOCTL_SNFC_START_SETTING :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_SETTING - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        mdelay(10);
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_SETTING - end\n");
                        break;

                case IOCTL_SNFC_START_AUTOPOLL :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_AUTOPOLL - start\n");
                        autopoll_wait_cnt = 0;
                        break_cnt = 0;
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);

                        #ifndef CONFIG_CXD2235AGG_GJ_KDDI
                                //SNFC_DEBUG_MSG("[snfc_uart_control] !!!! RFS disable start !!!!\n");
                                disable_irq(gpio_to_irq(gpio_rfs));
                        #endif

                        mdelay(10);
                        autopoll_status = 1;
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_AUTOPOLL - end\n");
                        break;

                case IOCTL_SNFC_START_RW :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_RW - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        mdelay(10);
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_RW - end\n");
                        break;

                case IOCTL_SNFC_START_TARGET :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_TARGET - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        if(GPIO_HIGH_VALUE == snfc_gpio_read(gpio_rfs))
                                mdelay(10);
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_TARGET - end\n");
                        break;

                case IOCTL_SNFC_START_INTU :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_INTU - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        //mdelay(10);
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_INTU - end\n");
                        break;

                case IOCTL_SNFC_START_WAITSIMBOOT:
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_WAITSIMBOOT - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                                break;
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        mdelay(10);
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_WAITSIMBOOT - end\n");
                        break;

                case IOCTL_SNFC_HSEL_UP:
                        forced_hsel_up_flag = 1;
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_hsel_up\n");
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_HIGH_VALUE);
                        break;

                case IOCTL_SNFC_HSEL_DOWN:
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_hsel_down\n");
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_LOW_VALUE);
                        forced_hsel_up_flag = 0;
                        if(forced_pon_up_flag == 0 && forced_hsel_up_flag == 0)
                                __snfc_uart_control_set_uart_status(UART_STATUS_READY);
                        break;

                case IOCTL_SNFC_PON_UP:
                        forced_pon_up_flag = 1;
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_pon_up\n");
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_HIGH_VALUE);
                        mdelay(10);
                        break;

                case IOCTL_SNFC_PON_DOWN:
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_pon_down\n");
                        forced_pon_up_flag = 0;
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_LOW_VALUE);
                        if(forced_pon_up_flag == 0 && forced_hsel_up_flag == 0)
                                __snfc_uart_control_set_uart_status(UART_STATUS_READY);
                        break;

                case IOCTL_SNFC_BOOT_CEN_HI:            //Kernel init only
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_boot_hi\n");
                        SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] CEN = High (UNLOCK) \n");
                        write_buf = 0x81; // set unlock
                        //mutex_lock(&nfc_cen_mutex);
                        rc = snfc_i2c_write(0x02, &write_buf, 1);
                        //mutex_unlock(&nfc_cen_mutex);
                        break;

                case IOCTL_SNFC_BOOT_CEN_LO:    //Kernel init only
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_boot_low\n");
                        SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] CEN = Low (LOCK) \n");
                        write_buf = 0x80; // set lock
                        //mutex_lock(&nfc_cen_mutex);
                        rc = snfc_i2c_write(0x02, &write_buf, 1);
                        //mutex_unlock(&nfc_cen_mutex);
                        break;

                case IOCTL_SNFC_END :
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_END - start\n");
                        if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
                        {
                                SNFC_DEBUG_MSG("[snfc_uart_control] pon & hsel forced up!! pon and/or sel will keep high\n");
                                break;
                        }
                        if(current_status != UART_STATUS_FOR_NFC)
                        {
                                SNFC_DEBUG_MSG("[snfc_uart_control] IOCTL_SNFC_END, UART is not used to NFC\n");
                                //return -2;
                        }
                        snfc_gpio_write(snfc_get_hsel_gpio_num(), GPIO_LOW_VALUE);
                        snfc_gpio_write(GPIO_SNFC_PON, GPIO_LOW_VALUE);
                        __snfc_uart_control_set_uart_status(UART_STATUS_READY);

                        #ifndef CONFIG_CXD2235AGG_GJ_KDDI
                                if(autopoll_status == 1)
                                {
                                        //SNFC_DEBUG_MSG("[snfc_uart_control] !!!! RFS disable end !!!!\n");
                                        enable_irq(gpio_to_irq(gpio_rfs));
                                }
                        #endif

                        autopoll_status = 0;
                        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_END - end (hsel low)(pon low)\n");
                        break;

        }
        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_ioctl - end\n");

        return 0;
}
/*
* Description :
* Input :
* Output :
*/
int snfc_temp_flag = 0;
static int snfc_uart_control_read(struct file *pf, char *pbuf, size_t size, loff_t *pos)
{
        int current_status;
        int rc;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_read - start \n");

        current_status = koto_abnormal;

        rc = copy_to_user((void*)pbuf, (void*)&current_status, size);
        if(rc)
        {
                SNFC_DEBUG_MSG("[snfc_uart_control] ERROR -  copy_to_user \n");
                return rc;
        }

        //if(snfc_temp_flag == 1)
        //      koto_abnormal = 0;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_read :koto_abnormal=%d - end \n",koto_abnormal);

        return size;
}
/*
* Description :
* Input :
* Output :
*/
static int snfc_uart_control_write(struct file *pf, const char *pbuf, size_t size, loff_t *pos)
{
        int new_status;
        int rc;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_write - start \n");

        rc = copy_from_user(&new_status, (void*)pbuf, size);
        if(rc)
        {
                SNFC_DEBUG_MSG("[snfc_uart_control] ERROR -  copy_to_user \n");
                return rc;
        }

        //if(autopoll_status == 1)
                koto_abnormal = new_status;
        snfc_temp_flag = 1;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_write - end:koto_abnormal=%d \n",koto_abnormal);

        return size;
}

static struct file_operations snfc_uart_control_fops =
{
        .owner    = THIS_MODULE,
        .open     = snfc_uart_control_open,
        .read           = snfc_uart_control_read,
        .write    = snfc_uart_control_write,
        .unlocked_ioctl = snfc_uart_control_ioctl,
        .release  = snfc_uart_control_release,
};

static struct miscdevice snfc_uart_control_device =
{
        .minor = 126,
        .name = "snfc_uart_control",
        .fops = &snfc_uart_control_fops,
};

/*
* Description :
* Input :
* Output :
*/
static int snfc_uart_control_init(void)
{
        int rc=0;

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_init - start \n");

        /* register the device file */
        rc = misc_register(&snfc_uart_control_device);
        if (rc)
        {
                SNFC_DEBUG_MSG("[snfc_uart_control] FAIL!! can not register snfc_uart_control \n");
                return rc;
        }

        rc = gpio_request(GPIO_SNFC_UICC_CON, "snfc_uicc_con");
        if(rc){
                SNFC_DEBUG_MSG("[snfc_driver] gpio_request snfc_uicc_con fail\n");
        }

        snfc_gpio_open(GPIO_SNFC_UICC_CON,GPIO_DIRECTION_OUT,GPIO_LOW_VALUE);
        __snfc_uart_control_set_uart_status(UART_STATUS_READY);

        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_init - end \n");
//      koto_abnormal = 2;

        return 0;
}

/*
* Description :
* Input :
* Output :
*/
static void snfc_uart_control_exit(void)
{
  SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_exit - start \n");

  /* deregister the device file */
  misc_deregister(&snfc_uart_control_device);

  SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_exit - end \n");
}

module_init(snfc_uart_control_init);
module_exit(snfc_uart_control_exit);

MODULE_LICENSE("Dual BSD/GPL");

