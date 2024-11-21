```c
/*
 * handle_IRQ handles all hardware IRQ's.  Decoded IRQs should
 * not come via this function.  Instead, they should provide their
 * own 'handler'.  Used by platform code implementing C-based 1st
 * level decoding.
 */
void handle_IRQ(unsigned int irq, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	struct irq_desc *desc;

	irq_enter(); //irq_enter函数中的preempt_count_add相对应HARDIRQ_OFFSET，用来标识进入硬中断

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (unlikely(!irq || irq >= nr_irqs))
		desc = NULL;
	else
		desc = irq_to_desc(irq);

	if (likely(desc))
		handle_irq_desc(desc);
	else
		ack_bad_irq(irq);

	irq_exit(); //irq_exit函数首先将preempt_count_sub计数器减去HARDIRQ_OFFSET，用来标识退出硬中断
	set_irq_regs(old_regs);
}

/*
 * asm_do_IRQ is the interface to be used from assembly code.
 */
asmlinkage void __exception_irq_entry
asm_do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	handle_IRQ(irq, regs);
}
```

```c
/**
 * irq_enter - Enter an interrupt context including RCU update
 * softirq.c
 */
void irq_enter(void)
{
	rcu_irq_enter();
	irq_enter_rcu();
}

/**
 * irq_enter_rcu - Enter an interrupt context with RCU watching
 */
void irq_enter_rcu(void)
{
	__irq_enter_raw();

	if (is_idle_task(current) && (irq_count() == HARDIRQ_OFFSET))
		tick_irq_enter();

	account_hardirq_enter(current);
}

/*
 * Like __irq_enter() without time accounting for fast
 * interrupts, e.g. reschedule IPI where time accounting
 * is more expensive than the actual interrupt.
 */
#define __irq_enter_raw()				\
	do {						\
		preempt_count_add(HARDIRQ_OFFSET);	\
		lockdep_hardirq_enter();		\
	} while (0)
```