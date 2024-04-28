---
title: dpdk 程序创建 kni 虚拟网络接口失败的问题
date: 2022-09-07 16:04:02
categories:
- [linux,网络开发,网卡驱动]
tags:
 - linux
 - 网卡驱动
 - kernel
---

# dpdk-16.04 rte_kni 模块与内核内存泄露问题
# 问题描述

某设备运行 dpdk 程序，free 的内存空间在逐渐下降。

第一次查看：

```bash
[root]# free -h
total        used        free      shared  buff/cache   available
Mem:           3.6G        1.5G        425M        403M        1.7G        534M
Swap:            0B          0B          0B
```

几分钟后查看：

```bash
[root]# free -h
total        used        free      shared  buff/cache   available
Mem:           3.6G        1.2G        425M        403M        1.4G        534M
Swap:            0B          0B          0B
```

持续观察确定 free 的内存在【持续降低】。

# 问题定位过程

1. 使用 top 查看用户态程序的占用的内存空间，未发现明显异常
2. 执行 echo 3 > /proc/sys/vm/drop_caches 清除缓存，free 的空间仍旧没有增加
3. 观察 /proc/slabinfo 发现 kmalloc-8192 这种类型的 slab 空间一直在增加

第一次查看：

```bash
kmalloc-8192      128016 128016   8192    4    8 : tunables    0    0    0 : slabdata  32004  32004      0
```

几分钟后查看：

```bash
kmalloc-8192 135760 135760 8192 4 8 : tunables 0 0 0 : slabdata 33940 33940 0
```

怀疑就是 kmalloc-8192 导致的问题。

# 跟踪 kmalloc-8192 的 alloc 与 free 过程

执行如下命令开启内核 kmalloc-8192 类 slab 对象的 **trace** 功能：

```bash
echo 1 > /sys/kernel/slab/kmalloc-8192/trace
```

几十秒后查看 dmesg 信息，看到如下内容：

```bash

[90556.444309] TRACE kmalloc-8192 alloc 0xffff88001a0d8000 inuse=4 fp=0x          (null)
.........
[90556.444342] Call Trace:
[90556.444355]  [<ffffffff826cbd7c>] dump_stack+0x63/0x7f
[90556.444362]  [<ffffffff826ca454>] alloc_debug_processing+0xb1/0x105
[90556.444368]  [<ffffffff826ca84a>] __slab_alloc+0x3a2/0x40f
[90556.444375]  [<ffffffff825a7d4c>] ? kzalloc+0xf/0x11
[90556.444383]  [<ffffffff826d3792>] ? _raw_spin_unlock_irqrestore+0x1a/0x2d
[90556.444389]  [<ffffffff8258eb55>] ? pci_conf1_read+0xdb/0xf3
[90556.444394]  [<ffffffff825a7d4c>] ? kzalloc+0xf/0x11
[90556.444401]  [<ffffffff82139fad>] __kmalloc+0xf5/0x156
[90556.444409]  [<ffffffff825c4f0b>] ? eth_mac_addr+0x28/0x28
[90556.444413]  [<ffffffff825a7d4c>] kzalloc+0xf/0x11
[90556.444419]  [<ffffffff825ae850>] alloc_netdev_mqs+0x80/0x2d9
[90556.444425]  [<ffffffff825c5075>] alloc_etherdev_mqs+0x1c/0x1e
[90556.444463]  [<ffffffffa0058840>] igb_kni_probe+0x64/0xa56 [rte_kni]
[90556.444471]  [<ffffffff82402fdf>] ? put_device+0x12/0x14
[90556.444478]  [<ffffffff82305dda>] ? pci_get_dev_by_id+0x71/0x79
[90556.444483]  [<ffffffff82305e16>] ? pci_get_subsys+0x34/0x3b
[90556.444507]  [<ffffffffa003567e>] kni_ioctl_create+0x32d/0x5a1 [rte_kni]
[90556.444531]  [<ffffffffa0035924>] kni_ioctl+0x32/0x3b [rte_kni]
[90556.444539]  [<ffffffff82156a05>] do_vfs_ioctl+0x351/0x415
[90556.444546]  [<ffffffff8215e905>] ? __fget+0x66/0x70
[90556.444552]  [<ffffffff82156b14>] SyS_ioctl+0x4b/0x76
[90556.444558]  [<ffffffff826d3f89>] system_call_fastpath+0x16/0x1b
```

上面的信息表明此处的 alloc 是在 kni 接口初始化时执行的。同时也看到了其它模块的打印，reiserfs 的一个打印信息如下：

```bash
90560.232045] TRACE kmalloc-8192 alloc 0xffff88001a0da000 inuse=4 fp=0x          (null)
.........
[90560.232083] Call Trace:
[90560.232096]  [<ffffffff826cbd7c>] dump_stack+0x63/0x7f
[90560.232103]  [<ffffffff826ca454>] alloc_debug_processing+0xb1/0x105
[90560.232109]  [<ffffffff826ca84a>] __slab_alloc+0x3a2/0x40f
[90560.232117]  [<ffffffff821adca2>] ? fix_nodes+0x188/0x1b1d
[90560.232124]  [<ffffffff8216bb01>] ? touch_buffer+0xd/0xf
[90560.232130]  [<ffffffff8216c4eb>] ? __find_get_block+0x1e5/0x1f7
[90560.232136]  [<ffffffff821adca2>] ? fix_nodes+0x188/0x1b1d
[90560.232143]  [<ffffffff82139fad>] __kmalloc+0xf5/0x156
[90560.232149]  [<ffffffff821adca2>] fix_nodes+0x188/0x1b1d
[90560.232156]  [<ffffffff8210b43b>] ? mark_page_accessed+0xf/0xc5
[90560.232161]  [<ffffffff8216bb01>] ? touch_buffer+0xd/0xf
[90560.232167]  [<ffffffff8210b43b>] ? mark_page_accessed+0xf/0xc5
[90560.232172]  [<ffffffff8216bb01>] ? touch_buffer+0xd/0xf
[90560.232178]  [<ffffffff821bdd39>] ? journal_mark_dirty+0x62/0x25f
[90560.232185]  [<ffffffff821b95f1>] reiserfs_insert_item+0x1ba/0x247
[90560.232196]  [<ffffffff821b1675>] add_save_link+0x170/0x1b8
[90560.232202]  [<ffffffff826d1ccf>] ? mutex_unlock+0x11/0x13
[90560.232209]  [<ffffffff821a8a2b>] reiserfs_truncate_file+0x197/0x278
[90560.232215]  [<ffffffff8210c341>] ? truncate_pagecache+0x4d/0x54
[90560.232221]  [<ffffffff821abc4a>] reiserfs_setattr+0x2e2/0x311
[90560.232228]  [<ffffffff8218c405>] ? __dquot_initialize+0x20/0x14c
[90560.232236]  [<ffffffff820ac766>] ? preempt_count_add+0x75/0x88
[90560.232241]  [<ffffffff820ac6c2>] ? get_parent_ip+0xd/0x3c
[90560.232248]  [<ffffffff8215e092>] notify_change+0x1e2/0x2c6
[90560.232255]  [<ffffffff82147096>] do_truncate+0x64/0x89
[90560.232262]  [<ffffffff82153f2c>] do_last.isra.46+0x976/0x9a5
[90560.232269]  [<ffffffff82154172>] path_openat+0x217/0x4ab
[90560.232275]  [<ffffffff8215547d>] do_filp_open+0x35/0x7a
[90560.232281]  [<ffffffff826d3765>] ? _raw_spin_unlock+0x12/0x25
[90560.232287]  [<ffffffff8215ee1f>] ? __alloc_fd+0xe5/0xf4
[90560.232293]  [<ffffffff8214830f>] do_sys_open+0x6b/0xfa
[90560.232298]  [<ffffffff821483b7>] SyS_open+0x19/0x1b
[90560.232303]  [<ffffffff826d3f89>] system_call_fastpath+0x16/0x1b
[90560.232321] TRACE kmalloc-8192 free 0xffff88001a0da000 inuse=3 fp=0xffff88001a0dc000
```

它与 kni 模块 alloc 空间打印的堆栈信息的区别在于最后一行，即 TRACE **kmalloc-8192 free 0xffff88001a0da000 .......**，此行表明【释放】了 8192 大小的空间，而 kni 模块的打印中没有释放的打印，表明只在创建。

# 为什么 kni 模块在一直创建 netdev 呢？

使用 kni 模块的程序只在【初始化】的过程中才会创建 netdev，程序死亡后 netdev 会释放，那上面说的没有释放是否是因为程序一直在运行？

查看程序状态发现，程序竟然在一直重启，同时 kmalloc-8192 的数量在以固定的数目缓慢增长，重命名程序后不再增长，**能够确定是 rte_kni 内部模块存在内存泄露问题**。

# rte_kni.ko 代码分析

阅读 rte_kni.ko 源码发现，在 **igb_kni_probe** 函数中会调用 **alloc_etherdev_mq** 创建一个 **netdev**，此外 **kni_ioctl_create** 中也会创建一个 **netdev**，而在【释放】逻辑中，**kni_dev_remove** 中只释放了 **kni_ioctl_create**  中创建的 **netdev**，【没有释放】 e1000、igb、ixgbe xxx_kni_probe 中创建的 netdev 结构，导致内存泄露。

# 解决方案

在 e1000、igb、ixgbe 的 xxx_kni_probe 中创建的 netdev 结构会被填充到 kni_dev 结构的 **lad_dev** 字段中，只需要在 kni_dev_remove 函数中判断 lad_dev 字段是否为空，不为空则调用 free_netdev 释放即可。

修改后的 kni_dev_remove 函数代码如下：

```c
static int
kni_dev_remove(struct kni_dev *dev)
{
	if (!dev)
		return -ENODEV;
	.........
	if (dev->lad_dev) {
		free_netdev(dev->lad_dev);
	}

	if (dev->net_dev) {
		unregister_netdev(dev->net_dev);
		free_netdev(dev->net_dev);
	}

	return 0;
}
```

# 验证方案

使用 **ftrace** 跟踪**内核函数调用**来验证修改有效，使用 ftrace 跟踪 kni 程序初始化时调用 alloc_netdev_mqs 以及死亡时调用 free_netdev 的过程来验证问题得到解决。

kni 程序运行命令如下：

```bash
[root]# ./kni  -- -p0x3 --config="(0,0,1),(1,1,1)"
```

ftrace 跟踪到 alloc_netdev_mqs 函数调用情况如下：

```bash
# tracer: function
#
# entries-in-buffer/entries-written: 4/4   #P:4
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
             kni-11436 [000] ....  1906.033365: alloc_netdev_mqs <-kni_ioctl_create
             kni-11436 [000] ....  1906.033435: alloc_netdev_mqs <-alloc_etherdev_mqs
             kni-11436 [000] ....  1906.220368: alloc_netdev_mqs <-kni_ioctl_create
             kni-11436 [000] ....  1906.220440: alloc_netdev_mqs <-alloc_etherdev_mqs

```

杀死 kni 程序后，ftrace 跟踪到 free_netdev 调用情况如下：

```bash
 Develop>cat ./trace
# tracer: function
#
# entries-in-buffer/entries-written: 2/2   #P:4
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
             kni-11436 [000] ....  1990.960338: free_netdev <-kni_dev_remove
             kni-11436 [000] ....  1990.986674: free_netdev <-kni_dev_remove
```

能够看到这里申请了四次，但是只释放了两次，泄露了两个 netdev 结构（每个口泄露一个），符合预期。

重新加载修改后的 rte_kni.ko 文件后测试记录如下：

ftrace 跟踪到 alloc_netdev_mqs 函数调用情况如下：

```bash

 Develop>cat ./trace
# tracer: function
#
# entries-in-buffer/entries-written: 8/8   #P:4
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
             kni-13861 [000] ....  2951.871499: alloc_netdev_mqs <-kni_ioctl_create
             kni-13861 [000] ....  2951.871571: alloc_netdev_mqs <-alloc_etherdev_mqs
             kni-13861 [000] ....  2952.059302: alloc_netdev_mqs <-kni_ioctl_create
             kni-13861 [000] ....  2952.059372: alloc_netdev_mqs <-alloc_etherdev_mqs

```

杀掉 kni 程序后，ftrace 跟踪到 free_netdev 调用情况如下：

```bash
# tracer: function
#
# entries-in-buffer/entries-written: 4/4   #P:4
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
             kni-13861 [000] ....  3000.154817: free_netdev <-kni_dev_remove
             kni-13861 [000] ....  3000.165235: free_netdev <-kni_dev_remove
             kni-13861 [000] ....  3000.181801: free_netdev <-kni_dev_remove
             kni-13861 [000] ....  3000.190228: free_netdev <-kni_dev_remove
```

修复后申请了四次，释放了四次，符合预期。

# 总结

前期已经解决过一些 rte_kni.ko 模块的问题，但是这次暴露出的问题表明其中仍旧潜藏着一些问题。

高版本的趋势是先【剥离】 rte_kni.ko 模块的 ethtool 功能，最终【完全抛弃】，这是正确的方向。

rte_kni.ko 模块的 ethtool 功能本身就是一个【过渡功能】，【稳定性堪忧】，我们也应当追随潮流弃用 rte_kni.ko 的 ethtool 功能。