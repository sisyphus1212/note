---
title: dpdk 程序kni 虚拟网络接口原理
date: 2022-09-07 16:04:02
categories:
- [linux,网络开发,网卡驱动]
tags:
 - linux
 - 网卡驱动
 - kernel
---

# dpdk 程序创建 kni 虚拟网络接口失败的问题
## 问题描述
今天接到了同事的一个反馈，说更新了 dpdk 后 kni 口无法正常创建。怀疑可能是 rte_kni.ko 模块存在问题导致创建失败。

## 排查过程
运行产品的 dpdk 程序后发现确实**没有创建** kni 口，并且 dpdk 程序在不断的**重启**。

使用 kni 命令也能够复现问题，在这种基础上，需要排查问题出在**内核还是用户态程序**中。这时我想到在 kni 口创建过程中内核会有信息打印，查看 dmesg 信息，发现 ioctl 的过程没有任何相关的打印。

## 是否打印级别的问题？
没有打印可能是 printk 打印级别的问题，可以通过修改 /proc/sys/kernel/printk 文件来解决。manual 中相关内容摘录如下：

```
   /proc/sys/kernel/printk
       /proc/sys/kernel/printk  is  a writable file containing four integer values that influence kernel printk() behavior when printing or logging error messages.  The
       four values are:

       console_loglevel
              Only messages with a log level lower than this value will be printed to the console.  The default value for this field  is  DEFAULT_CONSOLE_LOGLEVEL  (7),
              but  it  is set to 4 if the kernel command line contains the word "quiet", 10 if the kernel command line contains the word "debug", and to 15 in case of a
              kernel fault (the 10 and 15 are just silly, and equivalent to 8).  The value of console_loglevel can be set (to a value in the range 1–8)  by  a  syslog()
              call with a type of 8.

       default_message_loglevel
              This  value  will be used as the log level for printk() messages that do not have an explicit level.  Up to and including Linux 2.6.38, the hard-coded de‐
              fault value for this field was 4 (KERN_WARNING); since Linux 2.6.39, the default value is a defined by the kernel configuration option CONFIG_DEFAULT_MES‐
              SAGE_LOGLEVEL, which defaults to 4.

       minimum_console_loglevel
              The value in this field is the minimum value to which console_loglevel can be set.

       default_console_loglevel
              This is the default value for console_loglevel.
```
重要的东西在于 console_loglevel 这个字段，只有当消息的日志级别低于这个值才会打印，我将这个值调高到 7 后重新执行程序发现还是没有打印，修改了 kni 中创建虚拟网络接口的代码，重新测试发现也没有打印，确定是没有调用到。

## 内核与用户态问题界定

根据上面的结果，我没有界定出问题到底出在内核中还是用户态程序中，我对 kni 口创建的过程比较清楚，知道这个是用户态程序中调用 ioctl 发 IOCTL_CREATE 子命令到内核中，调用 kni_ioctl_create 函数来完成的。

那么我要界定问题出在哪里其实很简单，使用 strace 跟踪系统调用就可以了。用 strace 跟踪，确认程序没有调用到 ioctl 来创建 kni 口，确定问题出在用户态。

## 用户态程序中创建 kni 口的流程
用户态程序中创建 kni 接口的步骤主要有如下两步：

1. rte_kni_init 初始化相关数据结构
2. rte_kni_alloc 创建 kni 虚拟接口

dpdk-16.04 中 rte_kni_alloc 中相关代码如下：

```bash
	snprintf(mz_name, sizeof(mz_name), RTE_MEMPOOL_OBJ_NAME,
		pktmbuf_pool->name);
	mz = rte_memzone_lookup(mz_name);
	KNI_MEM_CHECK(mz == NULL);
	dev_info.mbuf_va = mz->addr;
	dev_info.mbuf_phys = mz->phys_addr;
	ctx->pktmbuf_pool = pktmbuf_pool;
	ctx->group_id = conf->group_id;
	ctx->slot_id = slot->id;
	ctx->mbuf_size = conf->mbuf_size;

	ret = ioctl(kni_fd, RTE_KNI_IOCTL_CREATE, &dev_info);
```
怀疑是在这个 KNI_MEM_CHECK 这出了问题，进一步排查确认了问题确实是 mz 为 NULL 导致没有调用 ioctl 就提前终止了。至于为啥 rte_memzone_lookup 没有找到 mz_name，原因是没有人创建它。

## 总结
在分析一个问题时首先要对问题进行界定，界定问题是为了缩小问题的范围。在这个问题中内核与用户态就是需要界定的点，没有这个界定问题就不好搞。其实也是在这里进行了一个 2 分法，这个过程可以一直执行下去，直到解决了问题。

