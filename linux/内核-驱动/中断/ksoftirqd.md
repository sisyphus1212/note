---
title: ksoftirqd 分析
date: 2024-11-20 14:46:23
tags:
---

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