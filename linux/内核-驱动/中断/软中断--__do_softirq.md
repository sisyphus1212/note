---
title: __do_softirq 分析
date: 2024-11-20 14:46:23
tags:
---

- 在硬中断执行完毕刚刚开中断的时候，以 ARM 架构为例，缺省没有强制线程化（`force_irqthreads` 默认为 0）的执行流程如下：`handle_IRQ()` -> `irq_exit()` -> `__irq_exit_rcu()` -> `invoke_softirq()` –> `__do_softirq()`。
- 在 ksoftirqd 内核态线程被调度中被执行：`run_ksoftirqd()` -> `__do_softirq()`。
- 在所有内核执行路径中可能会使能软中断处理的地方，具体来说就是调用诸如 `local_bh_enable()` 这些函数中。函数调用序列为 `local_bh_enable()` -> `_local_bh_enable_ip()` -> `do_softirq()` -> `do_softirq_own_stack` -> `__do_softirq()`。

软中断与硬中断的一个关键区别在于：硬中断需要现场保护、恢复，而软中断是由进程调度实现的，只是说软中断的进程调度优先级高于一般进程，值得注意的是软中断并不处于进程上下文，而是出于软中断上下文

```c
asmlinkage __visible void __softirq_entry __do_softirq(void)
{
	unsigned long end = jiffies + MAX_SOFTIRQ_TIME;
	unsigned long old_flags = current->flags;
	int max_restart = MAX_SOFTIRQ_RESTART;
	struct softirq_action *h;
	bool in_hardirq;
	__u32 pending;
	int softirq_bit;

	/*
	 * Mask out PF_MEMALLOC as the current task context is borrowed for the
	 * softirq. A softirq handled, such as network RX, might set PF_MEMALLOC
	 * again if the socket is related to swapping.
	 */
	current->flags &= ~PF_MEMALLOC;

	pending = local_softirq_pending();
	softirq_handle_begin();
	in_hardirq = lockdep_softirq_start();
	account_softirq_enter(current);

restart:
	/* Reset the pending bitmask before enabling irqs */
	set_softirq_pending(0);

	local_irq_enable();

	h = softirq_vec;

	while ((softirq_bit = ffs(pending))) {
		unsigned int vec_nr;
		int prev_count;

		h += softirq_bit - 1;

		vec_nr = h - softirq_vec;
		prev_count = preempt_count();

		kstat_incr_softirqs_this_cpu(vec_nr);

		trace_softirq_entry(vec_nr);
		h->action(h);
		trace_softirq_exit(vec_nr);
		if (unlikely(prev_count != preempt_count())) {
			pr_err("huh, entered softirq %u %s %p with preempt_count %08x, exited with %08x?\n",
			       vec_nr, softirq_to_name[vec_nr], h->action,
			       prev_count, preempt_count());
			preempt_count_set(prev_count);
		}
		h++;
		pending >>= softirq_bit;
	}

	if (!IS_ENABLED(CONFIG_PREEMPT_RT) &&
	    __this_cpu_read(ksoftirqd) == current)
		rcu_softirq_qs();

	local_irq_disable();

    //重复条件：1. 同一个硬中断多次持续到来
    //         2. 且前面软中断处理时间不超过MAX_SOFTIRQ_TIME

	pending = local_softirq_pending();
	if (pending) {
		if (time_before(jiffies, end) && !need_resched() &&
		    --max_restart) // 最大重复10次
			goto restart;

		wakeup_softirqd(); // 唤醒softirqd
	}

	account_softirq_exit(current);
	lockdep_softirq_end(in_hardirq);
	softirq_handle_end();
	current_restore_flags(old_flags, PF_MEMALLOC);
}
```

```c
static inline void invoke_softirq(void)
{
	if (ksoftirqd_running(local_softirq_pending()))
		return;

	if (!force_irqthreads() || !__this_cpu_read(ksoftirqd)) {
#ifdef CONFIG_HAVE_IRQ_EXIT_ON_IRQ_STACK
		/*
		 * We can safely execute softirq on the current stack if
		 * it is the irq stack, because it should be near empty
		 * at this stage.
		 */
		__do_softirq();
#else
		/*
		 * Otherwise, irq_exit() is called on the task stack that can
		 * be potentially deep already. So call softirq in its own stack
		 * to prevent from any overrun.
		 */
		do_softirq_own_stack();
#endif
	} else {
		wakeup_softirqd();
	}
}
```1