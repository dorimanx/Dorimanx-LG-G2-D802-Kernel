#ifndef _ASM_UM_BARRIER_H_
#define _ASM_UM_BARRIER_H_

#include <asm/asm.h>
#include <asm/segment.h>
#include <asm/cpufeature.h>
#include <asm/cmpxchg.h>
#include <asm/nops.h>

#include <linux/kernel.h>
#include <linux/irqflags.h>

/*
 * Force strict CPU ordering.
 * And yes, this is required on UP too when we're talking
 * to devices.
 */
#ifdef CONFIG_X86_32

#define mb()	alternative("lock; addl $0,0(%%esp)", "mfence", X86_FEATURE_XMM2)
#define rmb()	alternative("lock; addl $0,0(%%esp)", "lfence", X86_FEATURE_XMM2)
#define wmb()	alternative("lock; addl $0,0(%%esp)", "sfence", X86_FEATURE_XMM)

#else /* CONFIG_X86_32 */

#define mb()	asm volatile("mfence" : : : "memory")
#define rmb()	asm volatile("lfence" : : : "memory")
#define wmb()	asm volatile("sfence" : : : "memory")

#endif /* CONFIG_X86_32 */

<<<<<<< HEAD
#define read_barrier_depends()	do { } while (0)

#ifdef CONFIG_SMP

#define smp_mb()	mb()
=======
>>>>>>> 1077fa3... arch: Add lightweight memory barriers dma_rmb() and dma_wmb()
#ifdef CONFIG_X86_PPRO_FENCE
#define dma_rmb()	rmb()
#else /* CONFIG_X86_PPRO_FENCE */
#define dma_rmb()	barrier()
#endif /* CONFIG_X86_PPRO_FENCE */
#define dma_wmb()	barrier()

<<<<<<< HEAD
#ifdef CONFIG_X86_OOSTORE
#define smp_wmb()	wmb()
#else /* CONFIG_X86_OOSTORE */
#define smp_wmb()	barrier()
#endif /* CONFIG_X86_OOSTORE */

#define smp_read_barrier_depends()	read_barrier_depends()
=======
#ifdef CONFIG_SMP

#define smp_mb()	mb()
#define smp_rmb()	dma_rmb()
#define smp_wmb()	barrier()
>>>>>>> 1077fa3... arch: Add lightweight memory barriers dma_rmb() and dma_wmb()
#define set_mb(var, value) do { (void)xchg(&var, value); } while (0)

#else /* CONFIG_SMP */

#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
<<<<<<< HEAD
<<<<<<< HEAD
#define smp_read_barrier_depends()	do { } while (0)
#define set_mb(var, value) do { var = value; barrier(); } while (0)
=======
#define set_mb(var, value) do { WRITE_ONCE(var, value); barrier(); } while (0)
>>>>>>> ab3f02f... locking/arch: Add WRITE_ONCE() to set_mb()
=======

#define smp_store_mb(var, value) do { WRITE_ONCE(var, value); barrier(); } while (0)
>>>>>>> b92b8b3... locking/arch: Rename set_mb() to smp_store_mb()

#endif /* CONFIG_SMP */

/*
 * Stop RDTSC speculation. This is needed when you need to use RDTSC
 * (or get_cycles or vread that possibly accesses the TSC) in a defined
 * code region.
 *
 * (Could use an alternative three way for this if there was one.)
 */
static inline void rdtsc_barrier(void)
{
	alternative(ASM_NOP3, "mfence", X86_FEATURE_MFENCE_RDTSC);
	alternative(ASM_NOP3, "lfence", X86_FEATURE_LFENCE_RDTSC);
}

#endif
