---
title: ksoftirqd 分析
date: 2024-11-20 14:46:23
tags:
---

# ksoftirqd 结构
```c
static struct smp_hotplug_thread softirq_threads = {
	.store			= &ksoftirqd,
	.thread_should_run	= ksoftirqd_should_run,
	.thread_fn		= run_ksoftirqd,
	.thread_comm		= "ksoftirqd/%u",
};

static void run_ksoftirqd(unsigned int cpu)
{
	ksoftirqd_run_begin(); // 关闭cpu 中断响应
	if (local_softirq_pending()) {
		/*
		 * We can safely run softirq on inline stack, as we are not deep
		 * in the task stack here.
		 */
		__do_softirq();
		ksoftirqd_run_end();
		cond_resched();
		return;
	}
	ksoftirqd_run_end();  // 开启cpu 中断响应
}
```

# ksoftirqd的调度
内核本身不会主动周期性的调度softirqd，一般是通过驱动模块或者第三方事件触发：
内核中有三个地方调用wakeup_softirqd唤醒ksoftirqd内核线程：
1. 在__do_softirq函数中
2. 在raise_softirq_irqoff函数中
3. 在invoke_softirq函数中

```c
static inline void __irq_exit_rcu(void)
{
#ifndef __ARCH_IRQ_EXIT_IRQS_DISABLED
	local_irq_disable(); // 保证硬中断退出时的原子性
#else
	lockdep_assert_irqs_disabled();
#endif
	account_hardirq_exit(current);
    // 退出硬中断上下文
	preempt_count_sub(HARDIRQ_OFFSET);
    // 避免了在软中断环境下的硬中断断，导致的softirqd被多次调度，也就是说如果硬中断发生在当前cpu 的软中断环境下那么该硬中断在退出时不会触发软中断
	if (!in_interrupt() && local_softirq_pending())
		invoke_softirq();

	tick_irq_exit();
}
```