/*
 *  felicauart.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include <linux/syscalls.h>
#include <asm/termios.h>

#include "felica_uart.h"

static struct file *uart_f = NULL;

/*
 * Description : open uart
 * Input : None
 * Output : success : 0 fail : others
 */
int felica_uart_open(void)
{
    struct termios newtio;
    mm_segment_t old_fs = get_fs();

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] open_hs_uart - start \n");
    #endif

    if(FELICA_UART_NOTAVAILABLE == get_felica_uart_status())
    {
      #ifdef FEATURE_DEBUG_HIGH
      FELICA_DEBUG_MSG("[FELICA_UART] collision!! uart is used by other device \n");
	  #endif
      return -1;
    }

    if (uart_f != NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart is already opened\n");
		#endif
        return 0;
    }

    set_fs(KERNEL_DS);

    uart_f = filp_open(FELICA_UART_NAME, O_RDWR | O_NOCTTY | O_NONBLOCK, 0);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] open UART\n");
    #endif

    if (uart_f == NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] ERROR - can not sys_open \n");
		#endif
        set_fs(old_fs);
        return -1;
    }

    set_felica_uart_status(UART_STATUS_FOR_FELICA);

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = B460800 | CS8 | CLOCAL | CREAD;
    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 5;
    do_vfs_ioctl(uart_f, -1, TCFLSH, TCIOFLUSH);
    do_vfs_ioctl(uart_f, -1, TCSETSF, (unsigned long)&newtio);

    set_fs(old_fs);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] open_hs_uart - end \n");
    #endif

    return 0;
}

/*
 * Description : close uart
 * Input : None
 * Output : success : 0
 */
int felica_uart_close(void)
{
    mm_segment_t old_fs = get_fs();

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] close_hs_uart - start \n");
    #endif

    if (uart_f == NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart is not opened \n");
		#endif
        return 0;
    }

    set_felica_uart_status(UART_STATUS_READY);

    set_fs(KERNEL_DS);
    filp_close(uart_f, NULL);
    uart_f = NULL;
    set_fs(old_fs);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] close_hs_uart - end \n");
    #endif

    return 0;
}

/*
 * Description : write data to uart
 * Input : buf : data count : data length
 * Output : success : data length fail : 0
 */
int felica_uart_write(char *buf, size_t count)
{
    mm_segment_t old_fs = get_fs();
    int n;

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] write_hs_uart - start \n");
    #endif

    if (uart_f == NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart is not opened\n");
		#endif
        return 0;
    }

    set_fs(KERNEL_DS);
    n = vfs_write(uart_f, buf, count, &uart_f->f_pos);
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] write_hs_uart - write (%d)\n", n);
    #endif
    set_fs(old_fs);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] write_hs_uart - end \n");
    #endif

    return n;
}

/*
 * Description : read data from uart
 * Input : buf : data count : data length
 * Output : success : data length fail : 0
 */
int felica_uart_read(char *buf, size_t count)
{
    mm_segment_t old_fs = get_fs();
    int n;
    int retry = 5;
    
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] read_hs_uart - start \n");
    #endif

    if (uart_f == NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart is not opened\n");
		#endif
        return 0;
    }

    set_fs(KERNEL_DS);

    while ((n = vfs_read(uart_f, buf, count, &uart_f->f_pos)) == -EAGAIN && retry > 0)
    {
        mdelay(10);
        #ifdef FEATURE_DEBUG_MED
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart_read - delay : %d \n", retry);
        #endif
        retry--;
    }


    #ifdef FEATURE_DEBUG_MED
    FELICA_DEBUG_MSG("[FELICA_UART] read_hs_uart - count(%d), num of read data(%d) \n",count ,n);
    #endif

    set_fs(old_fs);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] read_hs_uart - end \n");
    #endif

    return n;
}
/*
 * Description : get size of remaing data
 * Input : none
 * Output : success : data length fail : 0
 */
int felica_uart_ioctrl(int *count)
{
    mm_segment_t old_fs = get_fs();
    int n;

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] felica_uart_ioctrl - start \n");
    #endif

    if (uart_f == NULL)
    {
        #ifdef FEATURE_DEBUG_HIGH
        FELICA_DEBUG_MSG("[FELICA_UART] felica_uart is not opened\n");
		#endif
        return 0;
    }

    set_fs(KERNEL_DS);
    n = do_vfs_ioctl(uart_f, -1, TIOCINQ, (unsigned long)count);
    #ifdef FEATURE_DEBUG_MED
    FELICA_DEBUG_MSG("[FELICA_UART] do_vfs_ioctl return(%d), count(%d) \n", n, *count);
    #endif
    set_fs(old_fs);

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_UART] felica_uart_ioctrl - end \n");
    #endif

    return n;
}
