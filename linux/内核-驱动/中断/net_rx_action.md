---
title: net_rx_action 分析
date: 2024-11-20 14:46:23
tags:
---
```c
static __latent_entropy void net_rx_action(struct softirq_action *h)
{
	struct softnet_data *sd = this_cpu_ptr(&softnet_data);

    //netdev_budget和 time_limit 防止在一次软中断中处理过多的数据包，导致系统长时间无法响应其他任务，影响系统的实时性和响应性
	unsigned long time_limit = jiffies +
		usecs_to_jiffies(netdev_budget_usecs);
	int budget = netdev_budget;

	LIST_HEAD(list);
	LIST_HEAD(repoll);

	local_irq_disable();
	list_splice_init(&sd->poll_list, &list);
	local_irq_enable();

	for (;;) {
		struct napi_struct *n;

		if (list_empty(&list)) {
			if (!sd_has_rps_ipi_waiting(sd) && list_empty(&repoll))
				return;
			break;
		}

		n = list_first_entry(&list, struct napi_struct, poll_list);
		budget -= napi_poll(n, &repoll);

		/* If softirq window is exhausted then punt.
		 * Allow this to run for 2 jiffies since which will allow
		 * an average latency of 1.5/HZ.
		 */
		if (unlikely(budget <= 0 ||
			     time_after_eq(jiffies, time_limit))) {
			sd->time_squeeze++; //记录由于预算或时间限制而中断处理的次数,用于统计和诊断，帮助分析软中断处理是否经常受到限制，以便进行优化，通过cat  /proc/net/softnet_stat 查看
			break;
		}
	}

	local_irq_disable();

	list_splice_tail_init(&sd->poll_list, &list);
	list_splice_tail(&repoll, &list);
	list_splice(&list, &sd->poll_list); //这是 softnet_data 结构中的一个链表，存储需要处理的 napi_struct 实例,理想情况下在前面的 list_first_entry(&list, struct napi_struct, poll_list);中就应该处理完
	if (!list_empty(&sd->poll_list))
		__raise_softirq_irqoff(NET_RX_SOFTIRQ); //设置软中断挂起位：将 NET_RX_SOFTIRQ 标记为待处理。
                                                //不启用中断：保持当前的中断禁用状态，防止防止与硬中断冲突

	net_rps_action_and_irq_enable(sd);
}
```