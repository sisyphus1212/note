---
title: dpdk 程序对pause 帧的处理
date: 2022-09-07 16:04:02
categories:
- [linux,网络开发,网卡驱动]
tags:
 - linux
 - 网卡驱动
 - kernel
---

# pause 帧导致 dpdk-16.04 rte_kni crash 问题
## 问题描述
系统运行时，内核 crash，oops 信息如下：

```bash
<1>[17934.849450] BUG: unable to handle kernel NULL pointer dereference at 0000000000000030
<1>[17934.849530] IP: [<ffffffffa009b135>] ixgbe_update_stats+0x38b/0xc7f [rte_kni]
.........
<4>[17934.850140] RIP: 0010:[<ffffffffa009b135>]  [<ffffffffa009b135>] ixgbe_update_stats+0x38b/0xc7f [rte_kni]
<4>[17934.850211] RSP: 0018:ffff8806feeb7c38  EFLAGS: 00010297
<4>[17934.850243] RAX: 0000000000000000 RBX: 00000000ffffffff RCX: 0000000000000000
<4>[17934.850278] RDX: ffff8806fed5c640 RSI: ffff8806fed5c000 RDI: ffff8806feeb7c58
<4>[17934.850314] RBP: ffff8806feeb7c78 R08: ffff8806fed5c648 R09: 00000007fffffff8
<4>[17934.850349] R10: 0000000000000000 R11: 00000000fffffff8 R12: ffff8806fed5c640
<4>[17934.850385] R13: ffff8806fed5c0e0 R14: ffff8806fedc0e00 R15: ffff8806feeb7d38
<4>[17934.850421] FS:  00007f38dd3e8700(0000) GS:ffff880840240000(0000) knlGS:0000000000000000
<4>[17934.850459] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
<4>[17934.850492] CR2: 0000000000000030 CR3: 000000057cfec000 CR4: 00000000000406e0
<4>[17934.850528] DR0: 0000000000000000 DR1: 0000000000000000 DR2: 0000000000000000
<4>[17934.850610] DR3: 0000000000000000 DR6: 00000000ffff0ff0 DR7: 0000000000000400
<4>[17934.850690] Process ethtool (pid: 14817, threadinfo ffff8806feeb6000, task ffff88069e2373d0)
<0>[17934.850817] Stack:
<4>[17934.850888]  0000000000000000 0000000000000000 0000000000000000 0000000000000000
<4>[17934.851103]  ffff8806fedc0e00 ffff8806fed5c640 ffff8806fed5c0e0 ffff8806fedc0e00
<4>[17934.851316]  ffff8806feeb7ca8 ffffffffa00a0d90 ffff8806feeb7cb8 ffff8806fed61000
<0>[17934.851531] Call Trace:
<4>[17934.851620]  [<ffffffffa00a0d90>] ixgbe_get_ethtool_stats+0x26/0xf8 [rte_kni]
<4>[17934.851724]  [<ffffffffa00dd688>] kni_get_ethtool_stats+0x20/0x22 [rte_kni]
<4>[17934.851812]  [<ffffffff8147e1af>] dev_ethtool+0xef6/0x1c57
<4>[17934.851895]  [<ffffffff8147c0d0>] dev_ioctl+0x4db/0x61a
<4>[17934.851977]  [<ffffffff8103abf7>] ? get_parent_ip+0x11/0x41
<4>[17934.852060]  [<ffffffff81273019>] ? __percpu_counter_add+0x70/0x93
<4>[17934.852146]  [<ffffffff81467641>] sock_do_ioctl+0x38/0x43
<4>[17934.852229]  [<ffffffff81467a24>] sock_ioctl+0x1e9/0x1f6
<4>[17934.852314]  [<ffffffff810de1e6>] do_vfs_ioctl+0x418/0x459
<4>[17934.852398]  [<ffffffff810de278>] sys_ioctl+0x51/0x75
<4>[17934.852482]  [<ffffffff8158fffb>] system_call_fastpath+0x16/0x1b
<0>[17934.852562] Code: 08 8b 80 68 cf 00 00 eb 06 8b 80 a8 41 00 00 89 c1 48 01 8a 40 14 00 00 85 c0 0f 84 a6 00 00 00 31 c0 eb 0e 49 8b 88 38 02 00 00 <f0> 80 61 30 fb ff c0 49 83 c0 08 3b 82 20 02 00 00 7c e6 e9 83
<1>[17934.854260] RIP  [<ffffffffa009b135>] ixgbe_update_stats+0x38b/0xc7f [rte_kni]
<4>[17934.854421]  RSP <ffff8806feeb7c38>
<0>[17934.854495] CR2: 0000000000000030
```
堆栈表明，问题出在执行 ethtool -S 获取 82599 网卡统计计数的过程中。

## 反汇编 rte_kni.ko
objdump 反汇编 rte_kni.ko 得到如下相关信息：

```assemble
    10e7:       8b 82 0c 12 00 00       mov    0x120c(%rdx),%eax
    10ed:       83 f8 01                cmp    $0x1,%eax
    10f0:       74 09                   je     10fb <ixgbe_update_stats+0x351>
    10f2:       83 f8 03                cmp    $0x3,%eax
    10f5:       0f 85 d5 00 00 00       jne    11d0 <ixgbe_update_stats+0x426>
    10fb:       83 ba 50 0f 00 00 01    cmpl   $0x1,0xf50(%rdx)
    1102:       48 8b 82 80 0d 00 00    mov    0xd80(%rdx),%rax
    1109:       75 08                   jne    1113 <ixgbe_update_stats+0x369>
    110b:       8b 80 68 cf 00 00       mov    0xcf68(%rax),%eax
    1111:       eb 06                   jmp    1119 <ixgbe_update_stats+0x36f>
    1113:       8b 80 a8 41 00 00       mov    0x41a8(%rax),%eax
    1119:       89 c1                   mov    %eax,%ecx
    111b:       48 01 8a 40 14 00 00    add    %rcx,0x1440(%rdx)
    1122:       85 c0                   test   %eax,%eax
    1124:       0f 84 a6 00 00 00       je     11d0 <ixgbe_update_stats+0x426>
    112a:       31 c0                   xor    %eax,%eax
    112c:       eb 0e                   jmp    113c <ixgbe_update_stats+0x392>
    112e:       49 8b 88 38 02 00 00    mov    0x238(%r8),%rcx
    1135:       f0 80 61 30 fb          lock andb $0xfb,0x30(%rcx)
    113a:       ff c0                   inc    %eax
    113c:       49 83 c0 08             add    $0x8,%r8
    1140:       3b 82 20 02 00 00       cmp    0x220(%rdx),%eax
    1146:       7c e6                   jl     112e <ixgbe_update_stats+0x384>
    1148:       e9 83 00 00 00          jmpq   11d0 <ixgbe_update_stats+0x426>
    114d:       83 ba 50 0f 00 00 01    cmpl   $0x1,0xf50(%rdx)
    1154:       48 89 c1                mov    %rax,%rcx
    1157:       74 07                   je     1160 <ixgbe_update_stats+0x3b6
```

相关代码如下：

```c
static void ixgbe_update_xoff_rx_lfc(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_hw_stats *hwstats = &adapter->stats;
	u32 data;

	if ((hw->fc.current_mode != ixgbe_fc_full) &&
	    (hw->fc.current_mode != ixgbe_fc_rx_pause))
		return;

	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		break;
	default:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	}
	hwstats->lxoffrxc += data;

	/* refill credits (no tx hang) if we received xoff */
	if (!data)
		return;

	for (i = 0; i < adapter->num_tx_queues; i++)
		clear_bit(__IXGBE_HANG_CHECK_ARMED,
			  &adapter->tx_ring[i]->state);
}
```

## 汇编分析结果如下：

10e7~10f5

```c
	if ((hw->fc.current_mode != ixgbe_fc_full) &&
	    (hw->fc.current_mode != ixgbe_fc_rx_pause))
		return;
```

10fb~1119
```c
    switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		break;
	default:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	}
```

111b~1124

```c
	hwstats->lxoffrxc += data;

	/* refill credits (no tx hang) if we received xoff */
	if (!data)
		return;
```

112a~1148

```c
	for (i = 0; i < adapter->num_tx_queues; i++)
		clear_bit(__IXGBE_HANG_CHECK_ARMED,
			  &adapter->tx_ring[i]->state);
```

根据 crash 信息，出问题的指令如下：

```assemble
    1135:       f0 80 61 30 fb          lock andb $0xfb,0x30(%rcx)
```

## 根本原因

**rte_kni.ko** 中 ixgbe 模块 **tx_ring** 没有初始化，其值为 NULL，当网卡收到一个 XOFF 报文导致网卡寄存器统计不为 0 的时候，清除 adapter->tx_ring 中的 state 标志位时，由于 **tx_ring 为 NULL，adapter->tx_ring[i]->state 的地址为 0x30 (非法地址)，写入此地址导致内核 crash!**

## 解决方法
**不设定 tx_ring[i]->state 变量！**

