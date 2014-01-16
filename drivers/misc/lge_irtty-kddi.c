/*********************************************************************
 *                
 * Filename:      lge_irtty-kddi.c
 * Version:       1.0
 * Description:   please refer to n_tty.c
 *     
 ********************************************************************/    

#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include <linux/audit.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <asm/system.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#define IRTTY_WAKEUP_CHARS 256

#define IRTTY_THRESHOLD_THROTTLE		128 /* now based on remaining room */
#define IRTTY_THRESHOLD_UNTHROTTLE 	128

#define IRTTY_ECHO_OP_START 0xff
#define IRTTY_ECHO_OP_MOVE_BACK_COL 0x80
#define IRTTY_ECHO_OP_SET_CANON_COL 0x81
#define IRTTY_ECHO_OP_ERASE_TAB 0x82

#define N_IRDA_BUF_SIZE 65536

#define GPIO_IRDA_PWDN		29
#define GPIO_IRDA_PWDN_DISABLE	1
#define GPIO_IRDA_PWDN_ENABLE		0

static inline int irtty_put_user(struct tty_struct *tty, unsigned char x,
			       unsigned char __user *ptr)
{
	tty_audit_add_data(tty, &x, 1);
	return put_user(x, ptr);
}

static void irtty_set_room(struct tty_struct *tty)
{
	/* tty->read_cnt is not read locked ? */
	int	left = N_IRDA_BUF_SIZE - tty->read_cnt - 1;
	int old_left;

	if (left <= 0)
		left = tty->icanon && !tty->canon_data;
	old_left = tty->receive_room;
	tty->receive_room = left;

	/* Did this open up the receive buffer? We may need to flip */
	if (left && !old_left)
		schedule_work(&tty->buf.work);
}

static void irtty_check_unthrottle(struct tty_struct *tty)
{
	if (tty->count)
		tty_unthrottle(tty);
}

static void irtty_reset_buffer_flags(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&tty->read_lock, flags);
	tty->read_head = tty->read_tail = tty->read_cnt = 0;
	spin_unlock_irqrestore(&tty->read_lock, flags);

	mutex_lock(&tty->echo_lock);
	tty->echo_pos = tty->echo_cnt = tty->echo_overrun = 0;
	mutex_unlock(&tty->echo_lock);

	tty->canon_head = tty->canon_data = tty->erasing = 0;
	memset(&tty->read_flags, 0, sizeof tty->read_flags);
	irtty_set_room(tty);
	irtty_check_unthrottle(tty);
}

static void irtty_flush_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	/* clear everything and unthrottle the driver */

	irtty_reset_buffer_flags(tty);

	if (!tty->link)
		return;

	spin_lock_irqsave(&tty->ctrl_lock, flags);
	if (tty->link->packet) {
		tty->ctrl_status |= TIOCPKT_FLUSHREAD;
		wake_up_interruptible(&tty->link->read_wait);
	}
	spin_unlock_irqrestore(&tty->ctrl_lock, flags);
}

static ssize_t irtty_chars_in_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	ssize_t n = 0;

	spin_lock_irqsave(&tty->read_lock, flags);
	if (!tty->icanon) {
		n = tty->read_cnt;
	} else if (tty->canon_data) {
		n = (tty->canon_head > tty->read_tail) ?
			tty->canon_head - tty->read_tail :
			tty->canon_head + (N_IRDA_BUF_SIZE - tty->read_tail);
	}
	spin_unlock_irqrestore(&tty->read_lock, flags);
	
	return n;
}

static void irtty_write_wakeup(struct tty_struct *tty)
{
	if (tty->fasync && test_and_clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags))
		kill_fasync(&tty->fasync, SIGIO, POLL_OUT);
}

static void irtty_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			      char *fp, int count)
{
	int	i;
	unsigned long cpuflags;

	if (!tty->read_buf)
		return;

	spin_lock_irqsave(&tty->read_lock, cpuflags);

	if(tty->read_cnt < 0 ) {
		i = N_IRDA_BUF_SIZE - tty->read_head;
	}
	else {
	i = min(N_IRDA_BUF_SIZE - tty->read_cnt, N_IRDA_BUF_SIZE - tty->read_head);
	}			
	i = min(count, i);
	memcpy(tty->read_buf + tty->read_head, cp, i);
	tty->read_head = (tty->read_head + i) & (N_IRDA_BUF_SIZE-1);
	tty->read_cnt += i;

	cp += i;
	count -= i;
	
	i = min(N_IRDA_BUF_SIZE - tty->read_cnt, N_IRDA_BUF_SIZE - tty->read_head);			
	i = min(count, i);
	memcpy(tty->read_buf + tty->read_head, cp, i);
	tty->read_head = (tty->read_head + i) & (N_IRDA_BUF_SIZE-1);
	tty->read_cnt += i;

	//printk(KERN_INFO "count: %d, i: %d, cnt: %d, head: %d , tail: %d \n",
	//	count, i, tty->read_cnt, tty->read_head, tty->read_tail);

	spin_unlock_irqrestore(&tty->read_lock, cpuflags);

	irtty_set_room(tty);

	if ((!tty->icanon && (tty->read_cnt >= tty->minimum_to_wake)) ||
		L_EXTPROC(tty)) {
		kill_fasync(&tty->fasync, SIGIO, POLL_IN);
		if (waitqueue_active(&tty->read_wait))
			wake_up_interruptible(&tty->read_wait);
	}

	if (tty->receive_room < IRTTY_THRESHOLD_THROTTLE)
		tty_throttle(tty);
}

int irtty_is_ignored(int sig)
{
	return (sigismember(&current->blocked, sig) ||
		current->sighand->action[sig-1].sa.sa_handler == SIG_IGN);
}

static void irtty_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	int canon_change = 1;
	BUG_ON(!tty);

	if (old)
		canon_change = (old->c_lflag ^ tty->termios->c_lflag) & ICANON;
	if (canon_change) {
		memset(&tty->read_flags, 0, sizeof tty->read_flags);
		tty->canon_head = tty->read_tail;
		tty->canon_data = 0;
		tty->erasing = 0;
	}

	if (canon_change && !L_ICANON(tty) && tty->read_cnt)
		wake_up_interruptible(&tty->read_wait);

	tty->icanon = (L_ICANON(tty) != 0);
	if (test_bit(TTY_HW_COOK_IN, &tty->flags)) {
		tty->raw = 1;
		tty->real_raw = 1;
		irtty_set_room(tty);
		return;
	}
	if (I_ISTRIP(tty) || I_IUCLC(tty) || I_IGNCR(tty) ||
	    I_ICRNL(tty) || I_INLCR(tty) || L_ICANON(tty) ||
	    I_IXON(tty) || L_ISIG(tty) || L_ECHO(tty) ||
	    I_PARMRK(tty)) {
		memset(tty->process_char_map, 0, 256/8);

		if (I_IGNCR(tty) || I_ICRNL(tty))
			set_bit('\r', tty->process_char_map);
		if (I_INLCR(tty))
			set_bit('\n', tty->process_char_map);

		if (L_ICANON(tty)) {
			set_bit(ERASE_CHAR(tty), tty->process_char_map);
			set_bit(KILL_CHAR(tty), tty->process_char_map);
			set_bit(EOF_CHAR(tty), tty->process_char_map);
			set_bit('\n', tty->process_char_map);
			set_bit(EOL_CHAR(tty), tty->process_char_map);
			if (L_IEXTEN(tty)) {
				set_bit(WERASE_CHAR(tty),
					tty->process_char_map);
				set_bit(LNEXT_CHAR(tty),
					tty->process_char_map);
				set_bit(EOL2_CHAR(tty),
					tty->process_char_map);
				if (L_ECHO(tty))
					set_bit(REPRINT_CHAR(tty),
						tty->process_char_map);
			}
		}
		if (I_IXON(tty)) {
			set_bit(START_CHAR(tty), tty->process_char_map);
			set_bit(STOP_CHAR(tty), tty->process_char_map);
		}
		if (L_ISIG(tty)) {
			set_bit(INTR_CHAR(tty), tty->process_char_map);
			set_bit(QUIT_CHAR(tty), tty->process_char_map);
			set_bit(SUSP_CHAR(tty), tty->process_char_map);
		}
		clear_bit(__DISABLED_CHAR, tty->process_char_map);
		tty->raw = 0;
		tty->real_raw = 0;
	} else {
		tty->raw = 1;
		if ((I_IGNBRK(tty) || (!I_BRKINT(tty) && !I_PARMRK(tty))) &&
		    (I_IGNPAR(tty) || !I_INPCK(tty)) &&
		    (tty->driver->flags & TTY_DRIVER_REAL_RAW))
			tty->real_raw = 1;
		else
			tty->real_raw = 0;
	}
	irtty_set_room(tty);
	/* The termios change make the tty ready for I/O */
	wake_up_interruptible(&tty->write_wait);
	wake_up_interruptible(&tty->read_wait);
}

static void irtty_close(struct tty_struct *tty)
{
	irtty_flush_buffer(tty);
	if (tty->read_buf) {
		kfree(tty->read_buf);
		tty->read_buf = NULL;
	}
	if (tty->echo_buf) {
		kfree(tty->echo_buf);
		tty->echo_buf = NULL;
	}

	printk(KERN_INFO "%s - %s: Irda GPIO PIN Disabled\n", __func__, tty->name);
	/* we must switch on IrDA HW module & Level shifter
	 * prior to transfering any data */
	gpio_set_value_cansleep(GPIO_IRDA_PWDN, GPIO_IRDA_PWDN_DISABLE);
}

static int irtty_open(struct tty_struct *tty)
{
	if (!tty)
		return -EINVAL;

	/* These are ugly. Currently a malloc failure here can panic */
	if (!tty->read_buf) {
		tty->read_buf = kzalloc(N_IRDA_BUF_SIZE, GFP_KERNEL);
		if (!tty->read_buf)
			return -ENOMEM;
	}
	if (!tty->echo_buf) {
		tty->echo_buf = kzalloc(N_IRDA_BUF_SIZE, GFP_KERNEL);

		if (!tty->echo_buf)
			return -ENOMEM;
	}
	irtty_reset_buffer_flags(tty);
	tty->column = 0;
	irtty_set_termios(tty, NULL);
	tty->minimum_to_wake = 1;
	tty->closing = 0;

	printk(KERN_INFO "%s - %s: Irda GPIO PIN Enabled\n", __func__, tty->name);
	/* we must switch on IrDA HW module & Level shifter
	 * prior to transfering any data */	
	gpio_set_value_cansleep(GPIO_IRDA_PWDN, GPIO_IRDA_PWDN_ENABLE);

	return 0;
}

static inline int irtty_input_available_p(struct tty_struct *tty, int amt)
{
	tty_flush_to_ldisc(tty);
	if (tty->icanon && !L_EXTPROC(tty)) {
		if (tty->canon_data)
			return 1;
	} else if (tty->read_cnt >= (amt ? amt : 1))
		return 1;

	return 0;
}

static int irtty_copy_from_read_buf(struct tty_struct *tty,
				      unsigned char __user **b, size_t *nr) {
	int retval;
	size_t n;
	unsigned long flags;

	retval = 0;
	spin_lock_irqsave(&tty->read_lock, flags);
	if(tty->read_cnt < 0) {
		n = 0;
	}
	else {
		n = min(tty->read_cnt, N_IRDA_BUF_SIZE - tty->read_tail);
		n = min(*nr, n);
	}
	spin_unlock_irqrestore(&tty->read_lock, flags);
	if (n) {
		retval = copy_to_user(*b, &tty->read_buf[tty->read_tail], n);
		n -= retval;
		tty_audit_add_data(tty, &tty->read_buf[tty->read_tail], n);
		spin_lock_irqsave(&tty->read_lock, flags);
		tty->read_tail = (tty->read_tail + n) & (N_IRDA_BUF_SIZE-1);
		tty->read_cnt -= n;
		/* Turn single EOF into zero-length read */
		if (L_EXTPROC(tty) && tty->icanon && n == 1) {
			if (!tty->read_cnt && (*b)[n-1] == EOF_CHAR(tty))
				n--;
		}
		spin_unlock_irqrestore(&tty->read_lock, flags);
		*b += n;
		*nr -= n;
	}
	return retval;
}

extern ssize_t redirected_tty_write(struct file *, const char __user *,
							size_t, loff_t *);

static int irtty_job_control(struct tty_struct *tty, struct file *file)
{
	if (file->f_op->write != redirected_tty_write &&
	    current->signal->tty == tty) {
		if (!tty->pgrp)
			printk(KERN_ERR "n_tty_read: no tty->pgrp!\n");
		else if (task_pgrp(current) != tty->pgrp) {
			if (irtty_is_ignored(SIGTTIN) ||
			    is_current_pgrp_orphaned())
				return -EIO;
			kill_pgrp(task_pgrp(current), SIGTTIN, 1);
			set_thread_flag(TIF_SIGPENDING);
			return -ERESTARTSYS;
		}
	}
	return 0;
}

static ssize_t irtty_read(struct tty_struct *tty, struct file *file,
			 unsigned char __user *buf, size_t nr)
{
	unsigned char __user *b = buf;
	DECLARE_WAITQUEUE(wait, current);
	int c;
	int minimum, time;
	ssize_t retval = 0;
	ssize_t size;
	long timeout;
	unsigned long flags;
	int packet;

do_it_again:

	BUG_ON(!tty->read_buf);

	c = irtty_job_control(tty, file);
	if (c < 0)
		return c;

	minimum = time = 0;
	timeout = MAX_SCHEDULE_TIMEOUT;
	if (!tty->icanon) {
		time = (HZ / 10) * TIME_CHAR(tty);
		minimum = MIN_CHAR(tty);
		if (minimum) {
			if (time)
				tty->minimum_to_wake = 1;
			else if (!waitqueue_active(&tty->read_wait) ||
				 (tty->minimum_to_wake > minimum))
				tty->minimum_to_wake = minimum;
		} else {
			timeout = 0;
			if (time) {
				timeout = time;
				time = 0;
			}
			tty->minimum_to_wake = minimum = 1;
		}
	}

	/*
	 *	Internal serialization of reads.
	 */
	if (file->f_flags & O_NONBLOCK) {
		if (!mutex_trylock(&tty->atomic_read_lock))
			return -EAGAIN;
	} else {
		if (mutex_lock_interruptible(&tty->atomic_read_lock))
			return -ERESTARTSYS;
	}
	packet = tty->packet;

	add_wait_queue(&tty->read_wait, &wait);
	while (nr) {
		/* First test for status change. */
		if (packet && tty->link->ctrl_status) {
			unsigned char cs;
			if (b != buf)
				break;
			spin_lock_irqsave(&tty->link->ctrl_lock, flags);
			cs = tty->link->ctrl_status;
			tty->link->ctrl_status = 0;
			spin_unlock_irqrestore(&tty->link->ctrl_lock, flags);
			if (irtty_put_user(tty, cs, b++)) {
				retval = -EFAULT;
				b--;
				break;
			}
			nr--;
			break;
		}
		/* This statement must be first before checking for input
		   so that any interrupt will set the state back to
		   TASK_RUNNING. */
		set_current_state(TASK_INTERRUPTIBLE);

		if (((minimum - (b - buf)) < tty->minimum_to_wake) &&
		    ((minimum - (b - buf)) >= 1))
			tty->minimum_to_wake = (minimum - (b - buf));

		if (!irtty_input_available_p(tty, 0)) {
			if (test_bit(TTY_OTHER_CLOSED, &tty->flags)) {
				retval = -EIO;
				break;
			}
			if (tty_hung_up_p(file))
				break;
			if (!timeout)
				break;
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				retval = -ERESTARTSYS;
				break;
			}
			/* FIXME: does n_tty_set_room need locking ? */
			irtty_set_room(tty);
			timeout = schedule_timeout(timeout);
			BUG_ON(!tty->read_buf);
			continue;
		}
		__set_current_state(TASK_RUNNING);

		/* Deal with packet mode. */
		if (packet && b == buf) {
			if (irtty_put_user(tty, TIOCPKT_DATA, b++)) {
				retval = -EFAULT;
				b--;
				break;
			}
			nr--;
		}
		{
			int uncopied;
			/* The copy function takes the read lock and handles
			   locking internally for this case */
			uncopied = irtty_copy_from_read_buf(tty, &b, &nr);
			uncopied += irtty_copy_from_read_buf(tty, &b, &nr);
			if (uncopied) {
				retval = -EFAULT;
				break;
			}
		}

		if (irtty_chars_in_buffer(tty) <= IRTTY_THRESHOLD_UNTHROTTLE) {
			irtty_set_room(tty);
			irtty_check_unthrottle(tty);
		}

		if (b - buf >= minimum)
			break;
		if (time)
			timeout = time;
	}
	mutex_unlock(&tty->atomic_read_lock);
	remove_wait_queue(&tty->read_wait, &wait);

	if (!waitqueue_active(&tty->read_wait))
		tty->minimum_to_wake = minimum;

	__set_current_state(TASK_RUNNING);
	size = b - buf;
	if (size) {
		retval = size;
		if (nr)
			clear_bit(TTY_PUSH, &tty->flags);
	} else if (test_and_clear_bit(TTY_PUSH, &tty->flags))
		 goto do_it_again;

	irtty_set_room(tty);
	return retval;
}

static ssize_t irtty_write(struct tty_struct *tty, struct file *file,
			   const unsigned char *buf, size_t nr)
{
	const unsigned char *b = buf;
	DECLARE_WAITQUEUE(wait, current);
	int c;
	ssize_t retval = 0;
	unsigned long cpuflags;
	
	/* Job control check -- must be done at start (POSIX.1 7.1.1.4). */
	if (L_TOSTOP(tty) && file->f_op->write != redirected_tty_write) {
		retval = tty_check_change(tty);
		if (retval)
			return retval;
	}

	add_wait_queue(&tty->write_wait, &wait);

	spin_lock_irqsave(&tty->read_lock, cpuflags);
	tty->read_cnt -= nr;
	tty->read_tail += nr;	
	if(tty->read_tail >= N_IRDA_BUF_SIZE) {
		tty->read_tail = nr;
		tty->read_head = 0;
	}
	spin_unlock_irqrestore(&tty->read_lock, cpuflags);
	

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		if (tty_hung_up_p(file) || (tty->link && !tty->link->count)) {
			retval = -EIO;
			break;
		}
		while (nr > 0) {
			c = tty->ops->write(tty, b, nr);
			if (c < 0) {
				retval = c;
				goto break_out;
			}
			if (!c)
				break;
			b += c;
			nr -= c;
		}

		if (!nr)
			break;
		
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		schedule();
	}
break_out:
	__set_current_state(TASK_RUNNING);
	
	remove_wait_queue(&tty->write_wait, &wait);
	if (b - buf != nr && tty->fasync)
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	return (b - buf) ? b - buf : retval;
}

static int irtty_ioctl(struct tty_struct *tty, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	int retval;

	switch (cmd) {
	case TIOCOUTQ:
		return put_user(tty_chars_in_buffer(tty), (int __user *) arg);
		
	case TIOCINQ:
		retval = tty->read_cnt;
		return put_user(retval, (unsigned int __user *) arg);
		
	default:
		return n_tty_ioctl_helper(tty, file, cmd, arg);
	}

}

struct tty_ldisc_ops tty_ldisc_N_IRDA = {
	.magic           = TTY_LDISC_MAGIC,
	.name            = "irtty",
	.open            = irtty_open,
	.close           = irtty_close,
	.flush_buffer    = irtty_flush_buffer,
	.chars_in_buffer = irtty_chars_in_buffer,
	.read            = irtty_read,
	.write           = irtty_write,
	.ioctl           = irtty_ioctl,
	.set_termios     = irtty_set_termios,
	.poll            = NULL,//irtty_poll,
	.receive_buf     = irtty_receive_buf,
	.write_wakeup    = irtty_write_wakeup
};


static int __init irtty_sir_init(void)
{
	int err;

	if ((err = tty_register_ldisc(N_IRDA, &tty_ldisc_N_IRDA)) != 0)
		printk(KERN_ERR "IrDA: can't register line discipline (err = %d)\n", err);
	/* we must switch on IrDA HW module & Level shifter
	 * prior to transfering any data
	 */

	if((err = gpio_request_one(GPIO_IRDA_PWDN, GPIOF_OUT_INIT_HIGH, "IrDA_PWDN")) != 0) {
		printk(KERN_ERR "%s : irda HW module open error\n", __func__);
		return err;
	}
	
	printk(KERN_INFO "%s : IrTTY Initialized.\n", __func__);
	return err;
}

static void __exit irtty_sir_cleanup(void) 
{
	int err;


	gpio_free(GPIO_IRDA_PWDN);

	if ((err = tty_unregister_ldisc(N_IRDA))) {

		printk(KERN_ERR "%s(), can't unregister line discipline (err = %d)\n", __func__, err);
	}
}

module_init(irtty_sir_init);
module_exit(irtty_sir_cleanup);

MODULE_DESCRIPTION("IrDA TTY device driver");
MODULE_ALIAS_LDISC(N_IRDA);
MODULE_LICENSE("GPL");

