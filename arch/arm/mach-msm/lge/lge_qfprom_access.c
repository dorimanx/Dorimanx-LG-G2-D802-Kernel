/*
* arch/arm/mach-msm/lge/lge_qfprom_access.c
*
* Copyright (C) 2010 LGE, Inc
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/setup.h>
#include <mach/board_lge.h>
#include <mach/scm.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>


#define LGE_QFPROM_INTERFACE_NAME "lge-msm8974-qfprom"
/* service ID inside tzbsp */
#define QFPROM_SVC_ID		8
#define QFPROM_WRITE_CMD	0x3
#define QFPROM_WRITE_MULT_CMD   0x4
#define QFPROM_READ_CMD		0x5
#define QFPROM_ROLLBACK_CMD     0x6
#define QFPROM_PRNG_CMD		0x7
#define QFPROM_OVERRIDE_CMD	0x8
/* qfprom read type */
#define QFPROM_ADDR_SPACE_RAW 0
#define QFPROM_ADDR_SPACE_CORR 1

#define QFPROM_CLOCK	(0x40*1000)

/* QFPROM address to blow */
#define QFPROM_CTRL_BASE                0xfc4b8000
#define QFPROM_RD_WR_PERMISSION		(QFPROM_CTRL_BASE + 0x00A8)
#define QFPROM_DEBUG_DISABLE		(QFPROM_CTRL_BASE + 0x00E8) /* 0xFC4B80E8 */
#define QFPROM_OEM_CONFIG		(QFPROM_CTRL_BASE + 0x00F0) /* 0xFC4B80F0 */
#define QFPROM_SECONDARY_HW_KEY		(QFPROM_CTRL_BASE + 0x03A8)
#define QFPROM_SECURE_BOOT_ENABLE	(QFPROM_CTRL_BASE + 0x03f8)/* 0xFC4B83F8 */
#define QFPROM_HW_KEY_STATUS		(QFPROM_CTRL_BASE + 0x204C)
#define QFPROM_OVERRIDE_REG		(QFPROM_CTRL_BASE + 0x60B8)
#define QFPROM_CHECK_HW_KEY		0x123456

/* secondary hw key status flag */
#define PRI_KEY_DERIVATION_KEY   0x00000001
#define SEC_KEY_DERIVATION_BLOWN 0x00000002
#define APP_KEYS_BLOCKED         0x00000004
#define MSA_KEYS_BLOCKED         0x00000008
#define KDF_DONE                 0x00000010

#define HW_KEY_LSB_FEC_MASK 0xC1FF83FF
#define HW_KEY_MSB_FEC_MASK 0x007FE0FF
#define FUSING_COMPLETED_STATE 0x1F

/* command buffer to write */
struct qfprom_write_cmd_buffer {
	u32 qfprom_addr;	/* qfprom address */
	u32 buf;		/* data to write qfprom */
	u32 qfprom_clk;	/* qfprom clock */
	u32 qfprom_status;	/* qfprom status */
};
/* command buffer to read */
struct qfprom_read_cmd_buffer {
	u32 qfprom_addr;
	u32 qfprom_addr_type;
	u32 read_buf;
	u32 qfprom_status;
};
/* blow data structure */
struct qfprom_blow_data {
	u32 qfprom_addr;
	u32 lsb_data;
	u32 msb_data;
};

static u32 qfprom_address;
static u32 qfprom_lsb_value;
static u32 qfprom_msb_value;
static u32 qfprom_read_kind = 1;

u32 qfprom_secondary_hwkey_status(void);
u32 qfprom_verification_blow_data(void);
int qfprom_blow_secondary_hwkey_region(void);
int qfuse_write_single_row(u32 fuse_addr, u32 fuse_lsb, u32 fuse_msb);
int qfuse_read_single_row(u32 fuse_addr, u32 addr_type, u32 *r_buf);
u32 qfprom_read(u32 fuse_addr);

static struct qfprom_blow_data blow_data[] = {
		/* Don't change array order !!!!!!!!!!!!!!*/
		/* addr	 			 LSB		MSB*/
		{ QFPROM_OEM_CONFIG, 		0x00310000, 	0x00000000},		/* OEM ID        */
		{ QFPROM_SECURE_BOOT_ENABLE, 	0x00202020, 	0x00000000},		/* SECURE ENABLE */
		{ QFPROM_DEBUG_DISABLE, 	0x3FC00000, 	0x040001FE},		/* JTAG DISABLE   */
		{ QFPROM_CHECK_HW_KEY,		0x0,            0x0	  },
		{ QFPROM_RD_WR_PERMISSION, 	0x00400000, 	0xc5c20200},	    	/* READ WRITE PERMISSION */
};

/* this api handle diag command(fusing check command) from ATD
 * if fusing value 0 ==> is not fused
 * if fusing value 1 ==> fused (secure boot enable, jtag disable, oem config, hw secondary key, RW permission)
 */
static ssize_t qfusing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 verification_check_value = 0;

	printk(KERN_INFO "%s start\n", __func__);

	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "verification_check_value = %x\n", verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {
		verification_check_value = 1;
		printk(KERN_INFO "%s: verification success\n", __func__);
		printk(KERN_INFO "%s end\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{
		verification_check_value = 0;
		printk(KERN_ERR "%s: verification fail 1\n", __func__);
	}
	/* defensive code */
	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "%s: verification_check_value = %x\n", __func__, verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {
		verification_check_value = 1;
		printk(KERN_INFO "%s: verification success\n", __func__);
		printk(KERN_INFO "%s end\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{
		verification_check_value = 0;
		printk(KERN_ERR "%s: verification fail 2\n", __func__);
	}
	/* defensive code */
	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "%s, verification_check_value = %x\n", __func__, verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {
		verification_check_value = 1;
		printk(KERN_INFO "%s: verification success\n", __func__);
		printk(KERN_INFO "%s end\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{
		verification_check_value = 0;
		printk(KERN_ERR "%s: verification fail 3\n", __func__);
		printk(KERN_ERR "%s end\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}
}


/* this api handle diag command(fusing command) from ATD
 * this api fuse secure boot, jtag disable, oem config, secondary hw key, R/W permission
 * this api check secondary hw key status before fusing R/W permission
 */
static ssize_t qfusing_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -1;
}
static DEVICE_ATTR(qfusing, S_IWUSR | S_IRUGO, qfusing_show, qfusing_store);
static ssize_t qfusing_verification_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 verification_check_value = 0;
	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "verification_check_value = %x\n", verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {

		printk(KERN_INFO "%s: verification success\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{

		printk(KERN_ERR "%s: verification fail 1\n", __func__);
	}

	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "verification_check_value = %x\n", verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {

		printk(KERN_INFO "%s: verification success\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{
		printk(KERN_ERR "%s: verification fail 2\n", __func__);
	}
	verification_check_value = qfprom_verification_blow_data();
	printk(KERN_INFO "verification_check_value = %x\n", verification_check_value);

	if (verification_check_value == FUSING_COMPLETED_STATE) {
		printk(KERN_INFO "%s: verification success\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}else{
		printk(KERN_ERR "%s: verification fail 3\n", __func__);
		return sprintf(buf, "%x\n", verification_check_value);
	}


}
static DEVICE_ATTR(qfusing_verification, S_IWUSR | S_IRUGO, qfusing_verification_show, qfusing_store);

static ssize_t qfprom_addr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", qfprom_address);
}

static ssize_t qfprom_addr_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	if (strict_strtoul(buf, 16, &val) < 0)
		return -EINVAL;
	qfprom_address = val;
	return count;
}
static DEVICE_ATTR(addr, S_IWUSR | S_IRUGO, qfprom_addr_show, qfprom_addr_store);

static ssize_t qfprom_read_kind_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", qfprom_read_kind);
}

static ssize_t qfprom_read_kind_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	long val;

	printk(KERN_INFO "%s: input:%s\n", __func__, buf);

	if (strict_strtol(buf, 10, &val) < 0)
		return -EINVAL;

	qfprom_read_kind = val;

	printk(KERN_INFO "%s: read_kind:%ld\n", __func__, val);

	return 0;
}
static DEVICE_ATTR(read_kind, S_IWUSR | S_IRUGO, qfprom_read_kind_show, qfprom_read_kind_store);

int qfprom_read_one_row(u32 address, u32 *buf)
{
	int ret = 0;
	printk(KERN_INFO "%s : address %x\n", __func__, address);

	switch (qfprom_read_kind)
	{
		case 0:
			ret = qfuse_read_single_row(address, 0, buf);
			break;
		case 1:
			ret = 0;
			buf[0] = qfprom_read(address);
			buf[1] = qfprom_read(address + 4);
			break;
		case 2:
			/* ret = qfprom_read_from_misc(address, &buf[0], &buf[1]); */
			break;
	}
	printk(KERN_INFO "%s : return %x\n", __func__, ret);

	return ret;
}
static ssize_t qfprom_read_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 *p_buf = NULL;
	int ret = 0;

	printk(KERN_INFO "%s start\n", __func__);

	if (!qfprom_address) {
		printk(KERN_ERR "%s: qfprom address is NULL\n", __func__);
		printk(KERN_ERR "%s end\n", __func__);
		return -EINVAL;
	}

	p_buf = kmalloc(sizeof(u32)*2, GFP_KERNEL);
	if (!p_buf) {
		printk(KERN_ERR "%s : buffer memory alloc fail\n", __func__);
		return -ENOMEM;
	}
	
	ret = qfprom_read_one_row(qfprom_address, p_buf);

	qfprom_address = 0;
	if (ret == 0) {
		qfprom_lsb_value = p_buf[0];
		qfprom_msb_value = p_buf[1];
		kfree(p_buf);
		printk(KERN_INFO "%s end\n", __func__);
		return count;
	} else if (ret < 0)	{
		printk(KERN_INFO "%s: qfuse_read_single_row ret: 0x%x\n", __func__, ret);
	} else
		printk(KERN_ERR "%s: qfprom write status error = %x\n", __func__, ret);

	kfree(p_buf);

	printk(KERN_INFO "%s end\n", __func__);

	return -EINVAL;
}
static DEVICE_ATTR(read, S_IWUSR | S_IRUGO, NULL, qfprom_read_store);

static ssize_t qfprom_hwstatus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", qfprom_secondary_hwkey_status());
}

static ssize_t qfprom_hwstatus_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR(hwstatus,  S_IWUSR | S_IRUGO, qfprom_hwstatus_show, qfprom_hwstatus_store);
static ssize_t qfprom_lsb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "%s start\n", __func__);
	return sprintf(buf, "%x\n", qfprom_lsb_value);
}

static ssize_t qfprom_lsb_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	if (strict_strtoul(buf, 16, &val) < 0)
		return -EINVAL;
	qfprom_lsb_value = val;
	return count;
}
static DEVICE_ATTR(lsb, S_IWUSR | S_IRUGO, qfprom_lsb_show, qfprom_lsb_store);

static ssize_t qfprom_msb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "%s start\n", __func__);
	return sprintf(buf, "%x\n", qfprom_msb_value);
}

static ssize_t qfprom_msb_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	if (strict_strtoul(buf, 16, &val) < 0)
		return -EINVAL;
	qfprom_msb_value = val;
	return count;
}
static DEVICE_ATTR(msb, S_IWUSR | S_IRUGO, qfprom_msb_show, qfprom_msb_store);

static ssize_t qfprom_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* u32 lsb, msb; */
	u32 value = 0;
	u32 addr = 0;

	printk(KERN_INFO "%s ------- OEM ID -------\n", __func__);
	addr = QFPROM_OEM_CONFIG; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);
	addr = QFPROM_OEM_CONFIG+4; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);

	printk(KERN_INFO "%s ------- Secure Boot Enable -------\n", __func__);
	addr = QFPROM_SECURE_BOOT_ENABLE; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);
	addr = QFPROM_SECURE_BOOT_ENABLE+4; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);

	printk(KERN_INFO "%s ------- Debug Disable -------\n", __func__);
	addr = QFPROM_DEBUG_DISABLE; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);
	addr = QFPROM_DEBUG_DISABLE+4; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);


	printk(KERN_INFO "%s ------- Read/Write Permission -------\n", __func__);
	addr = QFPROM_RD_WR_PERMISSION; value = qfprom_read(addr);
	printk(KERN_INFO "%s address:0x%x, value:0x%x\n", __func__, addr, value);

	return sprintf(buf, "Not supported in this version.\n");
}
static DEVICE_ATTR(value, S_IWUSR | S_IRUGO, qfprom_value_show, NULL);

/* if return value == 0, success
 * if return value < 0, scm call fail
 * if return value > 0, status error to read qfprom
 * This API can use in range 0x700XXX
 */
int qfuse_read_single_row(u32 fuse_addr, u32 addr_type, u32 *r_buf)
{
	struct qfprom_read_cmd_buffer request;
	u32 *p_status = NULL;
	u32	scm_ret = 0;
	int	ret = 0;

	printk(KERN_INFO "%s start. fuse_addr:0x%x\n", __func__, fuse_addr);

	p_status = kmalloc(sizeof(u32), GFP_KERNEL);
	if (!p_status) {
		printk(KERN_ERR "%s : status memory alloc fail\n", __func__);
		ret = -ENOMEM;
		goto error_stat;
	}

	request.qfprom_addr = fuse_addr;
	request.qfprom_addr_type = addr_type;
	request.read_buf = virt_to_phys((void *)r_buf);
	request.qfprom_status = virt_to_phys((void *)p_status);

	msleep(10);
	ret = scm_call(QFPROM_SVC_ID, QFPROM_READ_CMD, &request, sizeof(request), &scm_ret, sizeof(scm_ret));
	if (ret < 0) {
		printk(KERN_ERR "%s: scm call fail, scm_call ret:%d\n", __func__, ret);
		goto error_scm;
	}
	ret = *((u32 *)phys_to_virt(request.qfprom_status));
	printk(KERN_INFO "%s: qfprom_status = 0x%x\n", __func__, ret);

error_scm:
	kfree(p_status);

error_stat:
	printk(KERN_INFO "%s end\n", __func__);
	return ret;
}

static struct attribute *qfprom_attributes[] = {
	&dev_attr_qfusing.attr,
	&dev_attr_qfusing_verification.attr,
	&dev_attr_hwstatus.attr,
	&dev_attr_value.attr,
	&dev_attr_addr.attr,
	&dev_attr_lsb.attr,
	&dev_attr_msb.attr,
	&dev_attr_read.attr,
	&dev_attr_read_kind.attr,
	NULL
};
static const struct attribute_group qfprom_attribute_group = {
	.attrs = qfprom_attributes,
};

/* We cant access qfporm address range 0x70xxxxx using qfuse_single_read_row api
 * so we read the range using io read
 */
u32 qfprom_secondary_hwkey_status(void)
{
	void __iomem *key_status_addr;
	u32	hw_key_status;

	key_status_addr = ioremap(QFPROM_HW_KEY_STATUS, sizeof(u32));
	hw_key_status = (u32)readl(key_status_addr);
	iounmap(key_status_addr);
	printk(KERN_INFO "%s: hwkey status=0x%x\n", __func__, hw_key_status);
	return hw_key_status;
}

u32 qfprom_verification_blow_data(void)
{
	int i;
	u32 key_status = 0;
	u32 *p_buf = NULL;
	u32 fusing_verification = 0;

	printk(KERN_INFO "%s start\n", __func__);

	p_buf = kmalloc(sizeof(u32)*2, GFP_KERNEL);
	if (!p_buf) {
		printk(KERN_ERR "%s: memory alloc fail\n", __func__);
		goto err_mem;
	}
	for (i = 0 ; i < ARRAY_SIZE(blow_data) ; i++) {
		if (blow_data[i].qfprom_addr == QFPROM_CHECK_HW_KEY) {
			key_status = qfprom_secondary_hwkey_status();
			if ((key_status&SEC_KEY_DERIVATION_BLOWN)  == SEC_KEY_DERIVATION_BLOWN) {
				fusing_verification |= (0x1<<i);
				printk(KERN_INFO "%s: hw key is blown\n", __func__);
			}
			printk(KERN_INFO "%s:secondary HW key check complete!!!!!\n", __func__);
			continue;
		}
		/* msleep(10); */
		/* ret = qfuse_read_single_row(blow_data[i].qfprom_addr, 0, p_buf); */
		/* ret = 0; */
		p_buf[0] = qfprom_read(blow_data[i].qfprom_addr);
		p_buf[1] = qfprom_read(blow_data[i].qfprom_addr + 4);
		/* ret = qfprom_read(blow_data[i].qfprom_addr); */

		if (((p_buf[0]&blow_data[i].lsb_data) == blow_data[i].lsb_data) &&
			((p_buf[1]&blow_data[i].msb_data) == blow_data[i].msb_data)) {
			printk(KERN_INFO "%s: 0x%x check complete\n", __func__, blow_data[i].qfprom_addr);
			fusing_verification |= (0x1<<i);
			printk(KERN_INFO "%s: %d fusing_verification\n", __func__, fusing_verification);
		} else {
			printk(KERN_INFO "%s: 0x%x fusing value is not match\n",__func__, blow_data[i].qfprom_addr);
			/* msleep(10); */
		}
	}
err_mem:
	kfree(p_buf);

	printk(KERN_INFO "%s end\n", __func__);

	return fusing_verification;
}

u32 qfprom_read(u32 fuse_addr)
{
	void __iomem *value_addr;
	u32	value;

	value_addr = ioremap(fuse_addr, sizeof(u32));
	value = (u32)readl(value_addr);
	iounmap(value_addr);

	return value;
}

static int __devexit lge_qfprom_interface_remove(struct platform_device *pdev)
{
	return 0;
}

static int __init lge_qfprom_probe(struct platform_device *pdev)
{
	int err;
	printk(KERN_INFO "%s : qfprom init\n", __func__);
	err = sysfs_create_group(&pdev->dev.kobj, &qfprom_attribute_group);
	if (err < 0)
		printk(KERN_ERR "%s: cant create attribute file\n", __func__);
	return err;
}

static struct platform_driver lge_qfprom_driver __refdata = {
	.probe  = lge_qfprom_probe,
	.remove = __devexit_p(lge_qfprom_interface_remove),
	.driver = {
		.name = LGE_QFPROM_INTERFACE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lge_qfprom_interface_init(void)
{
	return platform_driver_register(&lge_qfprom_driver);
}

static void __exit lge_qfprom_interface_exit(void)
{
	platform_driver_unregister(&lge_qfprom_driver);
}

module_init(lge_qfprom_interface_init);
module_exit(lge_qfprom_interface_exit);

MODULE_DESCRIPTION("LGE QFPROM interface driver");
MODULE_AUTHOR("Taehung <taehung.kim@lge.com>");
MODULE_LICENSE("GPL");
