#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

#include <linux/sched.h>
#include <linux/export.h>

#include "ftt_ctrl_comm.h"
#include "ftt_status.h"
#include "ftt_charger_v3.h"


#if FTT_CHARACTER_DEVICE
extern void ftt_enable(struct ftt_charger_device *ftt_pdev);
extern void ftt_disable(struct ftt_charger_device *ftt_pdev);
extern u32 get_ftt_frequency_poll_ex(struct ftt_charger_device *ftt_pdev);
extern bool is_ftt_online(struct ftt_charger_device *ftt_pdev);
extern void set_ftt_charger_status_interrupt(struct ftt_charger_device *ftt_pdev);


bool ftt_send_cmd(struct ftt_charger_device *ftt_pdev, enum FTT_READ_COMMAND cmd, u32 frequency);
bool ftt_send_cmd_interrupt(struct ftt_charger_device *ftt_pdev, u32 frequency);
bool ftt_send_cmd_interrupt_ping(struct ftt_charger_device *ftt_pdev, u16 *frequency_table);
bool ftt_send_cmd_interrupt_online(struct ftt_charger_device *ftt_pdev, u16 frequency);
bool ftt_send_cmd_suspend(struct ftt_charger_device *ftt_pdev, u32 frequency);
bool ftt_send_cmd_init(struct ftt_charger_device *ftt_pdev);


static struct node_read_cmd *ftt_dequeue(struct ftt_charger_device *ftt_pdev)
{
	struct node_read_cmd *pnode = NULL;

	if (!list_empty(&ftt_pdev->read_cmd_queue)) {
		pnode = list_entry(ftt_pdev->read_cmd_queue.next, struct node_read_cmd, link);
		list_del(ftt_pdev->read_cmd_queue.next);
	}

	return (pnode);
}

static bool ftt_remove_all_queue(struct ftt_charger_device *ftt_pdev)
{
	struct node_read_cmd *pnode = NULL;

	for (;!list_empty(&ftt_pdev->read_cmd_queue);) {
		pnode = list_entry(ftt_pdev->read_cmd_queue.next, struct node_read_cmd, link);
		list_del(ftt_pdev->read_cmd_queue.next);
		if (pnode->cmd)
			kfree(pnode->cmd);
		kfree(pnode);
	}

	return true;
}

static ssize_t ftt_get_cmd(struct ftt_charger_device *ftt_pdev, char *str_cmd)
{
	struct node_read_cmd *pnode = NULL;
	u32 cmd_len;
	pnode = ftt_dequeue(ftt_pdev);
	if (pnode) {
		cmd_len = pnode->cmd_len;
		memcpy(str_cmd, pnode->cmd, pnode->cmd_len);
		if (pnode->cmd) kfree(pnode->cmd);
		kfree(pnode);
		return cmd_len;
	}
	else {
		return 0;
	}
}

bool ftt_send_cmd(struct ftt_charger_device *ftt_pdev, enum FTT_READ_COMMAND cmd, u32 data)
{
	struct node_read_cmd *send_cmd;
	struct ftt_cmd_hdr *ftt_cmd;
	u32 *pdata;
	size_t payload_len;

	if (!(send_cmd = kzalloc(sizeof(struct node_read_cmd), GFP_KERNEL))) {
		return 0;
	}
	switch (cmd) {
	case RD_INTERRUPT :
		payload_len = FTT_READ_CMD_DATA_SIZE;
		if (!(send_cmd->cmd = kzalloc(sizeof(struct ftt_cmd_hdr) + payload_len, GFP_KERNEL))) {
			kfree(send_cmd);
			return 0;
		}
		send_cmd->cmd_len = sizeof(struct ftt_cmd_hdr) + payload_len;
		ftt_cmd = (struct ftt_cmd_hdr *)send_cmd->cmd;
		ftt_cmd->cmd = RD_INTERRUPT;
		ftt_cmd->ver = FTT_PROTOCOL_VER;
		ftt_cmd->checksum = FTT_CHECKSUM;
		ftt_cmd->payload_len = F_HTONL(payload_len);
		((struct ftt_cmd_data*)ftt_cmd)->u.payload32 = F_HTONL(data);
		break;

	case RD_INTERRUPT_PING :
		payload_len = sizeof(ftt_pdev->ftt_ping_freq_tbl);
		if (!(send_cmd->cmd = kzalloc(sizeof(struct ftt_cmd_hdr) + payload_len, GFP_KERNEL))) {
			kfree(send_cmd);
			return 0;
		}
		send_cmd->cmd_len = sizeof(struct ftt_cmd_hdr) + payload_len;
		ftt_cmd = (struct ftt_cmd_hdr *)send_cmd->cmd;
		ftt_cmd->cmd = RD_INTERRUPT_PING;
		ftt_cmd->ver = FTT_PROTOCOL_VER;
		ftt_cmd->checksum = FTT_CHECKSUM;
		ftt_cmd->payload_len = F_HTONL(payload_len);
		pdata = (u32 *)data;
		memcpy((void *)ftt_cmd + sizeof(struct ftt_cmd_hdr), (void *)pdata, payload_len);
		break;

	case RD_INTERRUPT_ONLINE :
		payload_len = FTT_READ_CMD_FREQ_SIZE;
		if (!(send_cmd->cmd = kzalloc(sizeof(struct ftt_cmd_hdr) + payload_len, GFP_KERNEL))) {
			kfree(send_cmd);
			return 0;
		}
		send_cmd->cmd_len = sizeof(struct ftt_cmd_hdr) + payload_len;
		ftt_cmd = (struct ftt_cmd_hdr *)send_cmd->cmd;
		ftt_cmd->cmd = RD_INTERRUPT_ONLINE;
		ftt_cmd->ver = FTT_PROTOCOL_VER;
		ftt_cmd->checksum = FTT_CHECKSUM;
		ftt_cmd->payload_len = F_HTONL(payload_len);
		((struct ftt_cmd_data*)ftt_cmd)->u.payload16 = F_HTONS(data);
		break;

	case RD_SUSPEND :
		payload_len = FTT_READ_CMD_DATA_SIZE;
		if (!(send_cmd->cmd = kzalloc(sizeof(struct ftt_cmd_hdr) + payload_len, GFP_KERNEL))) {
			kfree(send_cmd);
			return 0;
		}
		send_cmd->cmd_len = sizeof(struct ftt_cmd_hdr) + payload_len;
		ftt_cmd = (struct ftt_cmd_hdr *)send_cmd->cmd;
		ftt_cmd->cmd = RD_SUSPEND;
		ftt_cmd->ver = FTT_PROTOCOL_VER;
		ftt_cmd->checksum = FTT_CHECKSUM;
		ftt_cmd->payload_len = F_HTONL(payload_len);
		((struct ftt_cmd_data*)ftt_cmd)->u.payload32 = F_HTONL(data);
		break;

	default :
		return false;
	}
	list_add_tail(&send_cmd->link, &ftt_pdev->read_cmd_queue);
	wake_up_interruptible(&ftt_pdev->wait);
	return true;
}

bool ftt_send_cmd_interrupt(struct ftt_charger_device *ftt_pdev, u32 frequency)
{
	DPRINT(FTT_CMD, "ftt_send_cmd_interrupt\n");
	return ftt_send_cmd(ftt_pdev, RD_INTERRUPT, frequency);
}

bool ftt_send_cmd_interrupt_ping(struct ftt_charger_device *ftt_pdev, u16 *frequency_table)
{
	DPRINT(FTT_CMD, "ftt_send_cmd_interrupt_ping\n");
	return ftt_send_cmd(ftt_pdev, RD_INTERRUPT_PING, (u32)frequency_table);
}

bool ftt_send_cmd_interrupt_online(struct ftt_charger_device *ftt_pdev, u16 frequency)
{
	DPRINT(FTT_CMD, "ftt_send_cmd_interrupt_online\n");
	return ftt_send_cmd(ftt_pdev, RD_INTERRUPT_ONLINE, frequency);
}

bool ftt_send_cmd_suspend(struct ftt_charger_device *ftt_pdev, u32 frequency)
{
	DPRINT(FTT_CMD, "ftt_send_cmd_suspend\n");
	return ftt_send_cmd(ftt_pdev, RD_SUSPEND, frequency);
}

bool ftt_send_cmd_init(struct ftt_charger_device *ftt_pdev)
{
	return true;
}

static bool ftt_checksum(struct ftt_cmd_data *ftt_cmd)
{
	if (ftt_cmd->cmd_hdr.checksum != FTT_CHECKSUM) {
		return false;
	}
	return true;
}

static ssize_t ftt_receive_cmd(struct ftt_charger_device *ftt_pdev, char *cmd_buf, ssize_t size)
{
	struct ftt_cmd_data *ftt_cmd = (struct ftt_cmd_data *)cmd_buf;
	u32 index;

	DPRINT(FTT_DEBUG, "ftt_receive_cmd=%d\n", size);
	if (ftt_cmd->cmd_hdr.ver != FTT_PROTOCOL_VER) {
		DPRINT(FTT_ERROR, "[WRITE CMD : ERROR] version mismatch\n");
		return size;
	}
	if (!ftt_checksum(ftt_cmd)) {
		DPRINT(FTT_ERROR, "[WRITE CMD : ERROR] checksum mismatch\n");
		return size;
	}

	switch (ftt_cmd->cmd_hdr.cmd) {
	case WT_PAD_TABLE :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_PAD_TABLE] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		DPRINT(FTT_CMD, "pad_type=%d, pad_name=%s, hysteresis=%u, pad_table_size=%u\n", ftt_cmd->u.ppt.pad_type, ftt_cmd->u.ppt.pad_name, ftt_cmd->u.ppt.ftt_hysteresis, ftt_cmd->u.ppt.pad_table_size);

		ftt_pdev->detect_pad = ftt_cmd->u.ppt.pad_type;
		memset (ftt_pdev->detect_pad_name, 0, FTT_READ_CMD_PUT_PAD_STRING_SIZE + 1);
		memcpy(ftt_pdev->detect_pad_name, ftt_cmd->u.ppt.pad_name, FTT_READ_CMD_PUT_PAD_STRING_SIZE);
		ftt_pdev->ftt_hysteresis = ftt_cmd->u.ppt.ftt_hysteresis;
		ftt_pdev->ant_level_type_table_size = ftt_cmd->u.ppt.pad_table_size;
		memcpy(ftt_pdev->ant_level_type_table, (struct ant_level_type *)&ftt_cmd->u.ppt.pad_table, ftt_cmd->u.ppt.pad_table_size * sizeof(struct ant_level_type));

		for(index=0;index < ftt_cmd->u.ppt.pad_table_size;index++) {
			DPRINT(FTT_DEBUG, "index=%u, level=%u, freq=%ukHz\n", index, ftt_pdev->ant_level_type_table[index].ant_level, ftt_pdev->ant_level_type_table[index].ping_freq);
		}

		break;

	case WT_DEBUG_LEVEL :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_DEBUG_LEVEL] cmd=%u, ver=%u, len=%u, debug_level=%d\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len, ftt_cmd->u.payload32);

		ftt_is_debug = ftt_cmd->u.payload32;
		break;

	case WT_FTT_START :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_FTT_START] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		if (ftt_cmd->u.payload32 == START_STOP_CHECK_CODE) {
			ftt_enable(ftt_pdev);
		}
		else {
			DPRINT(FTT_ERROR, "[WRITE CMD : WT_FTT_START] mismatch checkcode\n");
		}
		break;

	case WT_FTT_STOP :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_FTT_STOP] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		if (ftt_cmd->u.payload32 == START_STOP_CHECK_CODE) {
			ftt_disable(ftt_pdev);
		}
		else {
			DPRINT(FTT_ERROR, "[WRITE CMD : WT_FTT_STOP] mismatch checkcode\n");
		}
		break;

	case WT_FTT_TIMER :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_FTT_TIMER] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		memcpy(ftt_pdev->tst.ftt_status_timer, ftt_cmd->u.tst.ftt_status_timer, ftt_cmd->cmd_hdr.payload_len);
		for(index=0;index < FTT_STATUS_TIMER_MAX;index++) {
			DPRINT(FTT_DEBUG, "timer_status_table[%u]=%u\n", index, ftt_pdev->tst.ftt_status_timer[index]);
		}
		break;

	case WT_FTT_COUNT :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_FTT_COUNT] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		memcpy(ftt_pdev->fct.ftt_count, ftt_cmd->u.fct.ftt_count, ftt_cmd->cmd_hdr.payload_len);
		for(index=0;index < sizeof(struct ftt_cmd_count_table)/sizeof(ftt_pdev->fct.ftt_count[0]);index++) {
			DPRINT(FTT_DEBUG, "ftt_count_table[%u]=%u\n", index, ftt_pdev->fct.ftt_count[index]);
		}
		break;

	case WT_FTT_VALUE :
		DPRINT(FTT_CMD, "[WRITE CMD : WT_FTT_VALUE] cmd=%u, ver=%u, len=%u\n", ftt_cmd->cmd_hdr.cmd, ftt_cmd->cmd_hdr.ver, ftt_cmd->cmd_hdr.payload_len);
		memcpy(ftt_pdev->fvt.ftt_value, ftt_cmd->u.fvt.ftt_value, ftt_cmd->cmd_hdr.payload_len);
		for(index=0;index < sizeof(struct ftt_cmd_value_table)/sizeof(ftt_pdev->fvt.ftt_value[0]);index++) {
			DPRINT(FTT_DEBUG, "ftt_value_table[%u]=%u\n", index, ftt_pdev->fvt.ftt_value[index]);
		}
		break;

	default :
		break;
	}

	return size;
}

static void ftt_reset_data(struct ftt_charger_device *ftt_pdev)
{
	ftt_is_debug = FTT_LOG_LEVEL_ALWAYS;
}

static int ftt_open(struct inode *inode, struct file *filp)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);

	ftt_pdev->daemon_state = 1;
	ftt_pdev->openfile_num++;
	DPRINT(FTT_DEBUG, "ftt_open\n");

	ftt_send_cmd_init(ftt_pdev);

	return 0;
}

static int ftt_release(struct inode *inode, struct file *filp)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);

	ftt_pdev->daemon_state = 0;
	ftt_pdev->openfile_num--;

	DPRINT(FTT_DEBUG, "ftt_release\n");

	if (ftt_pdev->openfile_num == 0) {
		ftt_remove_all_queue(ftt_pdev);
#ifdef FTT_FILE_OPEN_ENABLE
		DPRINT(FTT_DEBUG, "ftt_disable\n");
		ftt_disable(ftt_pdev);
		ftt_reset_data(ftt_pdev);
#endif /* FTT_FILE_OPEN_ENABLE */
	}

	return 0;
}

static ssize_t ftt_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);
	char cmd[MAX_READ_CMD_BUFFER];
	size_t cmd_len;

	DPRINT(FTT_DEBUG, "ftt_read\n");

	if ((cmd_len = ftt_get_cmd(ftt_pdev, cmd))) {
		if (copy_to_user(buffer, cmd, cmd_len))
			return -EFAULT;
		return cmd_len;
	}

	return 0;
}

static ssize_t ftt_write(struct file *filp,
			const char __user *buf, size_t count, loff_t *pos)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);
	char cmd[MAX_WRITE_CMD_BUFFER];

	DPRINT(FTT_DEBUG, "ftt_write\n");

	if (copy_from_user((void *)cmd, buf, count)) return -EFAULT;

	return ftt_receive_cmd(ftt_pdev, cmd, count);
}

static long ftt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);
	struct ftt_charger_pdata *pdata = ftt_pdev->pdev->dev.platform_data;
	void __user *argp = (void __user *) arg;
	u32 value;

	switch (cmd) {
	case CMD_GET_FTT_FREQ:
		value = get_ftt_frequency_poll_ex(ftt_pdev);
		if (copy_to_user(argp, &value, sizeof(value))) return -EFAULT;
		DPRINT(FTT_CMD, "ftt_ioctl CMD_GET_FTT_FREQ=%u\n", value);
		break;
	case CMD_GET_ONLINE:
		value = is_ftt_online(ftt_pdev);
		if (copy_to_user(argp, &value, sizeof(value))) return -EFAULT;
		DPRINT(FTT_CMD, "ftt_ioctl CMD_GET_ONLINE=%u\n", value);
		break;
	case CMD_GET_FTT_INT_STATE:
		DPRINT(FTT_CMD, "ftt_ioctl CMD_GET_FTT_INT_STATE\n");
		break;
	case CMD_GET_FTT_DD_VERSION :
		DPRINT(FTT_CMD, "ftt_ioctl CMD_GET_FTT_DD_VERSION\n");
		value = (FTT_DD_MAJOR_VERSION << 16) | (FTT_DD_MINOR_VERSION_A << 8) | FTT_DD_MINOR_VERSION_B;
		if (copy_to_user(argp, &value, sizeof(value))) return -EFAULT;
		break;
	case CMD_SET_LEVEL:
		if (copy_from_user(&value, argp, sizeof(value))) return -EFAULT;
		ftt_pdev->ant_level = value;
#if FTT_UEVENT
		if (ftt_pdev->ftt_supply.name) power_supply_changed(&ftt_pdev->ftt_supply);
#endif /* FTT_UEVENT */
		if (pdata->on_change_level_fn) pdata->on_change_level_fn(value);
		DPRINT(FTT_CMD, "ftt_ioctl CMD_SET_LEVEL=%u\n", value);
		break;
	case CMD_SET_FTT_INT_STATE:
		set_ftt_charger_status_interrupt(ftt_pdev);
		DPRINT(FTT_CMD, "ftt_ioctl CMD_SET_FTT_INT_STATE\n");
		break;
	case CMD_SET_PADTYPE:
		if (copy_from_user(&value, argp, sizeof(value))) return -EFAULT;
		ftt_pdev->detect_pad = value;
		DPRINT(FTT_CMD, "ftt_ioctl CMD_SET_PADTYPE=%u\n", value);
		break;
	default:
		DPRINT(FTT_ERROR, "ftt_ioctl Not Defined\n");
		return -EINVAL;
	}
	return 0;
}

static unsigned int ftt_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct ftt_charger_device *ftt_pdev = container_of(filp->private_data,
			struct ftt_charger_device, ftt_misc);
	unsigned int mask = 0;

	poll_wait(filp, &ftt_pdev->wait, wait);

	if (!list_empty(&ftt_pdev->read_cmd_queue))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

const struct file_operations ftt_fops = {
	.owner		= THIS_MODULE,
	.open		= ftt_open,
	.release	= ftt_release,
	.read		= ftt_read,
	.write		= ftt_write,
	.unlocked_ioctl = ftt_ioctl,
	.poll		= ftt_poll,
};
#endif /* FTT_CHARACTER_DEVICE */


