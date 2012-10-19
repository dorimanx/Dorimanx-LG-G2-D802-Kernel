/*
 * Copyright (C) 2010 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 *
 * Provides a framework for enqueueing and running callbacks from hardirq
 * context. The enqueueing is NMI-safe.
 */

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/irq_work.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <asm/processor.h>


static DEFINE_PER_CPU(struct llist_head, irq_work_list);
static DEFINE_PER_CPU(int, irq_work_raised);

/*
 * Claim the entry so that no one else will poke at it.
 */
static bool irq_work_claim(struct irq_work *work)
{
	unsigned long flags, nflags;

	for (;;) {
		flags = work->flags;
		if (flags & IRQ_WORK_PENDING)
			return false;
		nflags = flags | IRQ_WORK_FLAGS;
		if (cmpxchg(&work->flags, flags, nflags) == flags)
			break;
		cpu_relax();
	}

	return true;
}

void __weak arch_irq_work_raise(void)
{
	/*
	 * Lame architectures will get the timer tick callback
	 */
}

/*
 * Queue the entry and raise the IPI if needed.
 */
static void __irq_work_queue(struct irq_work *work)
{
	preempt_disable();

	llist_add(&work->llnode, &__get_cpu_var(irq_work_list));

	/*
	 * If the work is not "lazy" or the tick is stopped, raise the irq
	 * work interrupt (if supported by the arch), otherwise, just wait
	 * for the next tick.
	 */
	if (!(work->flags & IRQ_WORK_LAZY) || tick_nohz_tick_stopped()) {
		if (!this_cpu_cmpxchg(irq_work_raised, 0, 1))
			arch_irq_work_raise();
	}

	preempt_enable();
}

/*
 * Enqueue the irq_work @entry, returns true on success, failure when the
 * @entry was already enqueued by someone else.
 *
 * Can be re-enqueued while the callback is still in progress.
 */
bool irq_work_queue(struct irq_work *work)
{
	if (!irq_work_claim(work)) {
		/*
		 * Already enqueued, can't do!
		 */
		return false;
	}

	__irq_work_queue(work);
	return true;
}
EXPORT_SYMBOL_GPL(irq_work_queue);

bool irq_work_needs_cpu(void)
{
	struct llist_head *this_list;

	this_list = &__get_cpu_var(irq_work_list);
	if (llist_empty(this_list))
		return false;

	return true;
}

/*
 * Run the irq_work entries on this cpu. Requires to be ran from hardirq
 * context with local IRQs disabled.
 */
void irq_work_run(void)
{
	unsigned long flags;
	struct irq_work *work;
	struct llist_head *this_list;
	struct llist_node *llnode;


	/*
	 * Reset the "raised" state right before we check the list because
	 * an NMI may enqueue after we find the list empty from the runner.
	 */
	__this_cpu_write(irq_work_raised, 0);
	barrier();

	this_list = &__get_cpu_var(irq_work_list);
	if (llist_empty(this_list))
		return;

	BUG_ON(!in_irq());
	BUG_ON(!irqs_disabled());

	llnode = llist_del_all(this_list);
	while (llnode != NULL) {
		work = llist_entry(llnode, struct irq_work, llnode);

		llnode = llist_next(llnode);

		/*
		 * Clear the PENDING bit, after this point the @work
		 * can be re-used.
		 */
		flags = work->flags & ~IRQ_WORK_PENDING;
		work->flags = flags;

		work->func(work);
		/*
		 * Clear the BUSY bit and return to the free state if
		 * no-one else claimed it meanwhile.
		 */
		(void)cmpxchg(&work->flags, flags, flags & ~IRQ_WORK_BUSY);
	}
}
EXPORT_SYMBOL_GPL(irq_work_run);

/*
 * Synchronize against the irq_work @entry, ensures the entry is not
 * currently in use.
 */
void irq_work_sync(struct irq_work *work)
{
	WARN_ON_ONCE(irqs_disabled());

	while (work->flags & IRQ_WORK_BUSY)
		cpu_relax();
}
EXPORT_SYMBOL_GPL(irq_work_sync);
