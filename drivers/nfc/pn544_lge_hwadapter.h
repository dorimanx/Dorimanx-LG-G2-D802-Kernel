#ifndef _PN544_LGE_HWADAPTER_H_
#define _PN544_LGE_HWADAPTER_H_

#include <linux/nfc/pn544_lge.h>

#include <linux/of_gpio.h>

bool pn544_validate_boot_mode(void);

int				pn544_get_hw_revision(void);
unsigned int	pn544_get_irq_pin(struct pn544_dev *dev);
int				pn544_gpio_to_irq(struct pn544_dev *dev);
void			pn544_gpio_enable(struct pn544_dev *pn544_dev);

void			pn544_shutdown_cb(struct pn544_dev *pn544_dev);

#endif /* _PN544_LGE_HWADAPTER_H_ */
