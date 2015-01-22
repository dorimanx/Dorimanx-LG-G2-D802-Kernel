#ifndef _LINUX_DEBUG_LOG_H
#define _LINUX_DEBUG_LOG_H

/*
               
                     
                                
                             
                                                                                                       
 */
#if defined(CONFIG_LGE_MMC_DYNAMIC_LOG)

extern uint32_t mmc_debug_level;

#define mmc_print(level, x...)			\
    do {								\
        if (mmc_debug_level >= (level))	\
            printk(x);					\
    } while (0)


#define mmc_debug_print(level, x...)	\
    do {								\
        if (mmc_debug_level >= (level))	\
            pr_info(x);					\
    } while (0)


#undef pr_emerg
#undef pr_alert
#undef pr_crit
#undef pr_err
#undef pr_warning
#undef pr_warn
#undef pr_notice
#undef pr_info
#undef pr_debug
#undef pr_devel

#define pr_emerg(x...)					mmc_print(0, x)		/* system is unusable */
#define pr_alert(x...)					mmc_print(1, x)		/* action must be taken immediately */
#define pr_crit(x...)					mmc_print(2, x)		/* critical conditions	*/
#define pr_err(x...)					mmc_print(3, x)		/* error conditions 	*/
#define	pr_warn(x...)					pr_warning(x)
#define pr_warning(x...)				mmc_print(4, x)		/* warning conditions	*/
#define pr_notice(x...)					mmc_print(5, x)		/* normal but significant condition	*/
#define pr_info(x...)					mmc_print(6, x)		/* informational		*/
#define	pr_devel(x...)					pr_debug(x)
#define pr_debug(x...)					mmc_debug_print(7, x)	/* debug-level message 	*/

#else
#define mmc_print(level, x...)			// map to empty
#endif	/*                     */

#endif 	/* _LINUX_DEBUG_LOG_H */
