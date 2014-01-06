#ifndef __MTV818_GPIO_H__
#define __MTV818_GPIO_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#if defined(CONFIG_KS1001) || defined(CONFIG_KS1103)
#include <mach/gpio.h>
#include <mach/gpio-names.h>

#define MTV_1_2V_EN					TEGRA_GPIO_PF2
#define MTV_1_8V_EN					TEGRA_GPIO_PE4
#define MTV_PWR_EN					TEGRA_GPIO_PF3
#define RAONTV_IRQ_INT				TEGRA_GPIO_PO6

static inline int mtv250_configure_gpio(void)
{
	//2011-05-02 taew00k.kang local gpio setting
	gpio_request(MTV_1_2V_EN, "mtv250 1.2V EN");
	tegra_gpio_enable(MTV_1_2V_EN);
	gpio_direction_output(MTV_1_2V_EN, 0);

	//2011-07-22 taew00k.kang local gpio setting
	gpio_request(MTV_1_8V_EN, "mtv250 1.8V EN");
	tegra_gpio_enable(MTV_1_8V_EN);
	gpio_direction_output(MTV_1_8V_EN, 1);// init value always high value


	gpio_request(MTV_PWR_EN, "mtv250 EN");
	tegra_gpio_enable(MTV_PWR_EN);
	gpio_direction_output(MTV_PWR_EN, 0);	

	gpio_request(RAONTV_IRQ_INT, "MTV250 INT");
	tegra_gpio_enable(RAONTV_IRQ_INT);	
	gpio_direction_input(RAONTV_IRQ_INT);

	return 0;
}

#elif defined(CONFIG_MACH_LGE_P940)
#include <mach/gpio.h>

#if defined(CONFIG_MACH_LGE_P940_EVB)
#define MTV_PWR_EN					(42)
#else //                                                     
#define MTV_PWR_EN					(46)
#endif
#define RAONTV_IRQ_INT				(44)

static inline int mtv250_configure_gpio(void)
{
	if(gpio_request(MTV_PWR_EN, "MTV_PWR_EN"))		
		DMBMSG("MTV_PWR_EN Port request error!!!\n");
	
	gpio_direction_output(MTV_PWR_EN, 0); // power down
	
	
	if(gpio_request(RAONTV_IRQ_INT, "RAONTV_IRQ_INT"))		
		DMBMSG("RAONTV_IRQ_INT Port request error!!!\n");

	gpio_direction_input(RAONTV_IRQ_INT);  

	return 0;
}
//                                        
#elif defined(CONFIG_MACH_MSM8960_L_DCM)
	/* write L configuration here*/
//#define MTV_PWR_EN						(93)	// L_DCM rev_A will change to 55 on 2011-12-XX
#define MTV_PWR_EN					(55)	// L_DCM rev_A 2012-01-03
#define MTV_LDO_EN						(98)
#define RAONTV_IRQ_INT					(75)

static inline int mtv250_configure_gpio(void)
{
	DMBERR("mtv250_configure_gpio start\n");
	
	if(gpio_request(MTV_LDO_EN, "MTV_LDO_EN"))
		DMBERR("1SEG_LDO_EN Port request error!!!\n");
	gpio_direction_output(MTV_LDO_EN, 0);
	
	if(gpio_request(MTV_PWR_EN	, "MTV_PWR_EN"))		
		DMBERR("1SEG_PWR_EN Port request error!!!\n");
	gpio_direction_output(MTV_PWR_EN, 0); // power down
	
	if(gpio_request(RAONTV_IRQ_INT, "RAONTV_IRQ_INT"))
		DMBERR("1SEG_SPI_INT Port request error!!!\n");
	gpio_direction_input(RAONTV_IRQ_INT);

	DMBERR("mtv250_configure_gpio end\n");

	return 0;
}
//                                      
#else
	#error "Code not present"
#endif

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV818_GPIO_H__*/

