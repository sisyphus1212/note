---
title: ethtool 代码架构
date: 2023-11-03 15:11:45
categories:
- [linux, 内核网络, 网卡驱动]
tags:
 - 网卡
 - ethool
 - linux内核网络
---
# ethtool 支持 dump 网卡的寄存器

最新版 ethtool 支持如下网卡的 dump：

```c
static const struct {
	const char *name;
	int (*func)(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

} driver_list[] = {
#ifdef ETHTOOL_ENABLE_PRETTY_DUMP
	{ "8139cp", realtek_dump_regs },
	{ "8139too", realtek_dump_regs },
	{ "r8169", realtek_dump_regs },
	{ "de2104x", de2104x_dump_regs },
	{ "e1000", e1000_dump_regs },
	{ "e1000e", e1000_dump_regs },
	{ "igb", igb_dump_regs },
	{ "ixgb", ixgb_dump_regs },
	{ "ixgbe", ixgbe_dump_regs },
	{ "ixgbevf", ixgbevf_dump_regs },
	{ "natsemi", natsemi_dump_regs },
	{ "e100", e100_dump_regs },
	{ "amd8111e", amd8111e_dump_regs },
	{ "pcnet32", pcnet32_dump_regs },
	{ "fec_8xx", fec_8xx_dump_regs },
	{ "ibm_emac", ibm_emac_dump_regs },
	{ "tg3", tg3_dump_regs },
	{ "skge", skge_dump_regs },
	{ "sky2", sky2_dump_regs },
	{ "vioc", vioc_dump_regs },
	{ "smsc911x", smsc911x_dump_regs },
	{ "at76c50x-usb", at76c50x_usb_dump_regs },
	{ "sfc", sfc_dump_regs },
	{ "st_mac100", st_mac100_dump_regs },
	{ "st_gmac", st_gmac_dump_regs },
	{ "et131x", et131x_dump_regs },
	{ "altera_tse", altera_tse_dump_regs },
	{ "vmxnet3", vmxnet3_dump_regs },
	{ "fjes", fjes_dump_regs },
	{ "lan78xx", lan78xx_dump_regs },
	{ "dsa", dsa_dump_regs },
	{ "fec", fec_dump_regs },
#endif
};
```

## ethtool dump 网卡寄存器的主要工作原理

1. ethtool 中首先通过 ioctl 调用底层驱动实现的 **get_regs_len** 函数，获取到 dump 寄存器的大小
2. ethtool 使用获取到的寄存器空间大小申请内存空间
3. 调用驱动中实现的 get_regs 函数来读取寄存器信息，并填充到之前申请的内存空间中
4. 最后 在ethtool 中调用适配的不同网卡驱动的 dump_regs 函数来解析并打印读取到的寄存器值

## ethtool.c 中解析获取到的寄存器值的函数

ethtool.c 中的 dump_regs 函数完成**解析** dump 出来的寄存器信息的功能。

对于 gregs_dump_hex 为 0 的情况，它会**遍历 driver_list** 列表，这就是上面我列出来的那个很长的表喽，它会用 info->driver 中的**驱动名称****匹配** **driver_list**，匹配到了一个则**调用相应的 func 函数**来完成寄存器解析工作，因为可能存在**嵌套的 regs 结构体内容**，这个过程会不断的**递归执行**。

对于这种没有单独的解析函数的网卡，ethtool 中将会调用 dump_hex 以十六进制格式进行输出。

dump_regs 函数源码如下：

```c
static int dump_regs(int gregs_dump_raw, int gregs_dump_hex,
		     struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	int i;

	if (gregs_dump_raw) {
		fwrite(regs->data, regs->len, 1, stdout);
		goto nested;
	}

	if (!gregs_dump_hex)
		for (i = 0; i < ARRAY_SIZE(driver_list); i++)
			if (!strncmp(driver_list[i].name, info->driver,
				     ETHTOOL_BUSINFO_LEN)) {
				if (driver_list[i].func(info, regs) == 0)
					goto nested;
				/* This version (or some other
				 * variation in the dump format) is
				 * not handled; fall back to hex
				 */
				break;
			}

	dump_hex(stdout, regs->data, regs->len, 0);

nested:
	/* Recurse dump if some drvinfo and regs structures are nested */
	if (info->regdump_len > regs->len + sizeof(*info) + sizeof(*regs)) {
		info = (struct ethtool_drvinfo *)(&regs->data[0] + regs->len);
		regs = (struct ethtool_regs *)(&regs->data[0] + regs->len + sizeof(*info));

		return dump_regs(gregs_dump_raw, gregs_dump_hex, info, regs);
	}

	return 0;
}
```

## ixgbe 内核态驱动中 dump 寄存器的实现参考

ixgbe 内核态驱动中 ixgbe_get_regs 函数的部分实现如下：

```c
static void ixgbe_get_regs(struct net_device *netdev, struct ethtool_regs *regs,
			   void *p)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 *regs_buff = p;
	u8 i;

	memset(p, 0, IXGBE_REGS_LEN * sizeof(u32));

	regs->version = (1 << 24) | hw->revision_id << 16 | hw->device_id;

	/* General Registers */
	regs_buff[0] = IXGBE_READ_REG(hw, IXGBE_CTRL);
	//printk(KERN_DEBUG "ixgbe_get_regs_3\n");
	regs_buff[1] = IXGBE_READ_REG(hw, IXGBE_STATUS);
	regs_buff[2] = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	regs_buff[3] = IXGBE_READ_REG(hw, IXGBE_ESDP);
	regs_buff[4] = IXGBE_READ_REG(hw, IXGBE_EODSDP);
	regs_buff[5] = IXGBE_READ_REG(hw, IXGBE_LEDCTL);
	regs_buff[6] = IXGBE_READ_REG(hw, IXGBE_FRTIMER);
```
