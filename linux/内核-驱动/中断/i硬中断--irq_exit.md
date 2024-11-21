```c
/*
 * Exit an interrupt context. Process softirqs if needed and possible:
 */
void irq_exit(void)
{
#ifndef __ARCH_IRQ_EXIT_IRQS_DISABLED
        local_irq_disable();
#else
        WARN_ON_ONCE(!irqs_disabled());
#endif

        account_irq_exit_time(current);
        preempt_count_sub(HARDIRQ_OFFSET);
        if (!in_interrupt() && local_softirq_pending())
                invoke_softirq();

        tick_irq_exit();
        rcu_irq_exit();
        trace_hardirq_exit(); /* must be last! */
}
```
irq_exit函数首先将preempt_count_sub计数器减去HARDIRQ_OFFSET，用来标识退出硬中断，这与irq_enter函数中的preempt_count_add相对应。
 之后irq_exit会通过宏in_interrupt()判断当前是否处在中断（interrupt）中。