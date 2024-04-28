---
title: dpdk 编译
date: 2023-03-19 10:55:27
categories:
- [dpdk]
tags:
- dpdk
- 网络开发
---

```sh
	main
	|	/* ------------------------ */
	|	/*    以太网设备的配置      */
	+-> rte_eth_dev_configure							/* 用户配置 以太网设备 */
	|	+-> memcpy(&dev->data->dev_conf, dev_conf, ...)	/* 拷贝用户设置到 设备的数据结构中 */
	|	+-> rte_eth_dev_rx_queue_config					/* 设置 收包队列 */
	|	|	+-> dev->data->rx_queues = rte_zmalloc()	/* 分配收包队列空间 */
	|	+-> rte_eth_dev_tx_queue_config 				/* 设置 发包队列 */
	|	|	+-> dev->data->tx_queues = rte_zmalloc()	/* 分配发包队列空间 */
	|	+-> (*dev->dev_ops->dev_configure)(dev)			/* 用户设置 以太网设备 的回调函数 */
	|		.	/* 千兆：eth_igb_ops.dev_configure = eth_igb_configure */
	|		+-> eth_igb_configure		/* 千兆：用户配置 以太网设备 */
	|		.	+-> igb_check_mq_mode	/* 千兆：检查用户配置的收发包模式 */
	|		.
	|		.	/* 万兆：ixgbe_eth_dev_ops.dev_configure = ixgbe_dev_configure */
	|		+-> ixgbe_dev_configure		/* 万兆：用户配置 以太网设备 */
	|			+-> ixgbe_check_mq_mode	/* 万兆：检查用户配置的收发包模式 */
	|
	+-> rte_eth_rx_queue_setup					/* 设置 以太网设备 的收包队列 */
	|	+-> (*dev->dev_ops->rx_queue_setup)()	/* 网口的收包队列 的初始化 */
	|		.	/* 千兆：eth_igb_ops.rx_queue_setup = eth_igb_rx_queue_setup */
	|		+~> eth_igb_rx_queue_setup
	|		.
	|		.	/* 万兆：ixgbe_eth_dev_ops.rx_queue_setup = ixgbe_dev_rx_queue_setup */
	|		+~> ixgbe_dev_rx_queue_setup
	|
	+-> rte_eth_tx_queue_setup					/* 设置 以太网设备 的发包队列 */
		+-> (*dev->dev_ops->tx_queue_setup)()	/* 网口的发包队列 的初始化 */
			.	/* 千兆：eth_igb_ops.tx_queue_setup = eth_igb_tx_queue_setup */
			+~> eth_igb_tx_queue_setup
			.
			.	/* 万兆：ixgbe_eth_dev_ops.tx_queue_setup = eth_igb_tx_queue_setup */
			+~> ixgbe_dev_tx_queue_setup
```