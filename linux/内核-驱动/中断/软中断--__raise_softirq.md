---
title: __raise_softirq 分析
date: 2024-11-20 14:46:23
tags:
---

# 软中断的调度

```c
#ifndef local_softirq_pending_ref
#define local_softirq_pending_ref irq_stat.__softirq_pending //软中断挂起位图，每bit代表一种软中断
#endif

#define local_softirq_pending()	(__this_cpu_read(local_softirq_pending_ref))
#define set_softirq_pending(x)	(__this_cpu_write(local_softirq_pending_ref, (x)))
#define or_softirq_pending(x)	(__this_cpu_or(local_softirq_pending_ref, (x)))

/*
 * This function must run with irqs disabled!
 */
inline void raise_softirq_irqoff(unsigned int nr)
{
	__raise_softirq_irqoff(nr);

	/*
	 * If we're in an interrupt or softirq, we're done
	 * (this also catches softirq-disabled code). We will
	 * actually run the softirq once we return from
	 * the irq or softirq.
	 *
	 * Otherwise we wake up ksoftirqd to make sure we
	 * schedule the softirq soon.
	 */
    //检查当前是否不在中断或软中断上下文中, 如果当前在硬中断或软中断上下文中，软中断处理程序会在返回中断时自动执行，
    //无需额外处理。如果当前不在中断上下文中，需要主动唤醒 ksoftirqd，确保软中断被及时处理。
	if (!in_interrupt() && should_wake_ksoftirqd())
		wakeup_softirqd();
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

// 禁用中毒的场景下设置软中断
void __raise_softirq_irqoff(unsigned int nr)
{
	lockdep_assert_irqs_disabled();
	trace_softirq_raise(nr);
	or_softirq_pending(1UL << nr);
}
```