/*
 *  snfc_intu_poll.c
 *
 */

/*
 *  Include header files
 *
 */
#include "snfc_intu_poll.h"
#include <linux/delay.h>
#include <linux/kmod.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/gpio.h>

/*
 *  Defines
 */

 /*
  *    Internal definition
  */
//static struct wake_lock snfc_intu_lock;
static int isopen_snfcintu = 0; // 0 : No open 1 : Open
//static int suspend_flag = 0;
wait_queue_head_t intuwq;
int intu_sig;
/*
 *    Function definition
 */

/*
* Description :
* Input :
* Output :
*/

static irqreturn_t snfc_int_low_isr(int irq, void *dev_id)
{
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_int_low_isr - start \n");
        disable_irq_nosync(gpio_to_irq(snfc_get_intu_gpio_num()));
        disable_irq_wake(gpio_to_irq(snfc_get_intu_gpio_num()));

        /* Wake up waiting readers */
        if(snfc_gpio_read(snfc_get_intu_gpio_num()) != 1)
        {
                wake_up(&intuwq);
                intu_sig = 1;
        }
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_int_low_isr - end \n");

        return IRQ_HANDLED;
}

static int snfc_intu_poll_open (struct inode *inode, struct file *fp)
{

        int rc = 0;

        rc = gpio_request(snfc_get_intu_gpio_num(), "snfc_intu");

        if (rc)
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] gpio_request snfc_intu fail (rc = %d)\n",rc);
        }

        rc = request_irq(gpio_to_irq(snfc_get_intu_gpio_num()), snfc_int_low_isr,IRQF_TRIGGER_FALLING | IRQF_DISABLED , "snfc_intu", NULL);

        if (rc)
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] request_irq fail (rc = %d )\n",rc);
                return rc;
        }

        if(isopen_snfcintu == 1)
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] snfc_intu_poll_open - already open \n");
                return 0;
        }

	 irq_set_irq_wake(gpio_to_irq(snfc_get_intu_gpio_num()),1);

        init_waitqueue_head(&intuwq);

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] GPIO_SNFC_INTU =%d\n",snfc_get_intu_gpio_num());

        rc = gpio_direction_input((unsigned)snfc_get_intu_gpio_num());
        if(rc)
        {
                SNFC_DEBUG_MSG("[snfc] ERROR -  gpio_direction_input \n");
                return rc;
        }
        SNFC_DEBUG_MSG_LOW("[snfc] set gpio %d input \n",snfc_get_intu_gpio_num());

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_poll_open - end \n");
        isopen_snfcintu = 1;

        return rc;
}

/*
 * Description: Notify a change in the device status. A blocking function
 * Input:
 * Output: Intu changed form low to high - 1, Intu changed from high to low - 0
 */
static ssize_t snfc_intu_read(struct file *pf, char *pbuf, size_t size, loff_t *pos)
{
        int rc = 0;
        int current_intu_status = GPIO_LOW_VALUE;
        //int new_intu_status = GPIO_LOW_VALUE;
        int return_val;

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_read - start \n");

        /* Parameters check*/
        if(pf == NULL || pbuf == NULL || size == !1 /*|| pos == NULL*/) //need to know meaning of pos, size is fixed to 1
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] ERROR pf = %p , buf = %p, size = %d, pos = %d\n",pf,pbuf,(int)size,(int)pos);
                return -1;
        }

        /* Get intu status */
        current_intu_status = snfc_gpio_read(snfc_get_intu_gpio_num());
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] current intu value is %d",current_intu_status);

        intu_sig=0;
        enable_irq_wake(gpio_to_irq(snfc_get_intu_gpio_num()));
        enable_irq(gpio_to_irq(snfc_get_intu_gpio_num()));
        SNFC_DEBUG_MSG_LOW("enable_irq intu irq");

        rc = wait_event_interruptible(intuwq,intu_sig);

        if(intu_sig==0)
        {
                disable_irq_nosync(gpio_to_irq(snfc_get_intu_gpio_num()));
                disable_irq_wake(gpio_to_irq(snfc_get_intu_gpio_num()));
                return_val = 1;
        }
        else
                return_val = 0;

        intu_sig=0;

        SNFC_DEBUG_MSG_LOW("snfc_intu_poll] wait_event_interruptible(),rc =%d !!!\n",rc);

        //current_intu_status = snfc_gpio_read(GPIO_SNFC_INTU);

        rc = copy_to_user((void*)pbuf, (void*)&return_val, size);
        if(rc)
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] ERROR -  copy_to_user \n");
                return rc;
        }

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_read - end \n");

        return size;
}

/*
 * Description: snfc intu release
 * Input:
 * Output:
 */
static int snfc_intu_release (struct inode *inode, struct file *fp)
{
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] felica_rfs_release - start \n");
        if(isopen_snfcintu == 0)
        {
                #ifdef FEATURE_DEBUG_LOW
                SNFC_DEBUG_MSG("[snfc_intu_poll] snfc_intu_release - not open \n");
                #endif
                return -1;
        }

        free_irq(gpio_to_irq(snfc_get_intu_gpio_num()), NULL);

        isopen_snfcintu = 0;
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_release - end \n");

        return 0;
}

static struct file_operations snfc_intu_fops =
{
  .owner    = THIS_MODULE,
  .open      = snfc_intu_poll_open,
  .read      = snfc_intu_read,
  .release  = snfc_intu_release,
};
static struct miscdevice snfc_intu_device = {
  .minor = 123,
  .name = "snfc_intu_poll",
  .fops = &snfc_intu_fops,
};

static int snfc_intu_init(void)
{
        int rc = 0;

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_poll_init - start \n");

        /* register the device file */
        rc = misc_register(&snfc_intu_device);
        if (rc < 0)
        {
                SNFC_DEBUG_MSG("[snfc_intu_poll] FAIL!! can not register snfc_intu_poll_init \n");
                return rc;
        }

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_poll_init - end \n");

        return 0;
}

static void snfc_intu_exit(void)
{
        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_poll_exit - start \n");

        /* deregister the device file */
        misc_deregister(&snfc_intu_device);

        SNFC_DEBUG_MSG_LOW("[snfc_intu_poll] snfc_intu_poll_exit - end \n");
}

module_init(snfc_intu_init);
module_exit(snfc_intu_exit);

MODULE_LICENSE("Dual BSD/GPL");

