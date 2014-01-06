
#include "pn544_lge_hwadapter.h"

bool pn544_validate_boot_mode(void) {
    enum lge_boot_mode_type boot_mode;
    boot_mode = lge_get_boot_mode();
	printk("pn544_probe() boot_mode : %d\n",boot_mode);
    if (boot_mode == LGE_BOOT_MODE_FACTORY || boot_mode == LGE_BOOT_MODE_FACTORY2 || boot_mode == LGE_BOOT_MODE_MINIOS)
    {
        printk("boot_mode :LGE_BOOT_MODE_FACTORY or LGE_BOOT_MODE_FACTORY2 or LGE_BOOT_MODE_MINIOS\n");
        return true;
    }
    return false;
}

int pn544_get_hw_revision(void)
{
#if defined(CONFIG_LGE_NFC_HW_TI_OMAP4430)
	int  hw_revision = LGE_PCB_MAX;
	hw_revision = system_rev;
#else
	hw_rev_type hw_revision = HW_REV_MAX;
	hw_revision = lge_get_board_revno();
#endif
	dprintk(PN544_DRV_NAME ":ioctl: pn544_read hw revision : %d\n", hw_revision);
	return (int)hw_revision;
}

unsigned int pn544_get_irq_pin(struct pn544_dev *dev)
{
#if defined(CONFIG_LGE_NFC_HW_QCT_APQ8064)||defined(CONFIG_LGE_NFC_HW_QCT_MSM8255)
	return dev->client->irq;
#elif defined(CONFIG_LGE_NFC_HW_TI_OMAP4430)
	return OMAP_GPIO_IRQ(dev->irq_gpio);
#elif defined(CONFIG_LGE_NFC_HW_NV_AP3X)
	return dev->client->irq;
#else
	return dev->client->irq;
#endif
}

int pn544_gpio_to_irq(struct pn544_dev *dev)
{
#if defined(CONFIG_LGE_NFC_HW_TI_OMAP4430)||defined(CONFIG_LGE_NFC_HW_QCT_MSM8255)
	return gpio_to_irq(dev->irq_gpio);
#else
	return dev->client->irq;
#endif
}

void pn544_gpio_enable(struct pn544_dev *pn544_dev)
{
#if defined(CONFIG_LGE_NFC_HW_NV_AP3X)
	tegra_gpio_enable(pn544_dev->ven_gpio);
	tegra_gpio_enable(pn544_dev->firm_gpio);
	tegra_gpio_enable(pn544_dev->irq_gpio);
#endif
	return;
}

void pn544_shutdown_cb(struct pn544_dev *pn544_dev)
{
#if defined(CONFIG_LGE_NFC_HW_QCT_MSM8660)
		dprintk("================ pn544_shutdown() start ================\n");
	
		// Make all output GPIOs to Low
		gpio_set_value(pn544_dev->ven_gpio, 0);
		gpio_set_value(pn544_dev->firm_gpio, 0);
		msleep(10);
		dprintk("Output GPIO Status : VEN = %d, FIRM = %d\n", 
		gpio_get_value(pn544_dev->ven_gpio), 
		gpio_get_value(pn544_dev->firm_gpio));
	
		dprintk("================ pn544_shutdown() end ================\n");
	
#elif defined(CONFIG_LGE_NFC_HW_NV_AP3X)
		gpio_set_value(pn544_dev->ven_gpio, 0);
#endif
	return;
}

