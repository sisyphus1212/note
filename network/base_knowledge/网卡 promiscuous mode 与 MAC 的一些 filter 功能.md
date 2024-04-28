---
title: linux内核网络设备
date: 2019-07-20 13:04:02
index_img: https://github.com/sisyphus1212/images/blob/main/mk-2022-09-20-23-08-10.png?raw=true
categories:
  - [linux, 网络开发]
tags:
 - kernel network
---

# 网卡 promiscuous mode 与 MAC 的一些 filter 功能
## 基础知识
以太网包可以分为如下三种类型：

1. 单播
2. 广播
3. 多播

一般来说网卡只接收**目的地址是网卡的硬件地址或广播地址的以太网帧**，这里硬件地址指的是 48-bit 的 MAC 地址。

网卡接收多播包则以下有两种不同的方式。

一种是通过**多播硬件地址的哈希值**进行过滤，接收那些主机软件已经表明要关注的地址，这种情况下由于**哈希冲突**的存在，可能会接收到不想要的帧。

第二种是监听一个特定的**多播地址表**，这意味着如果主机需要接收更多的超出该表格中项目的多播地址帧，网卡就进入了**多播混淆模式**，在这种模式下，网卡将会接收所有类型的多播报文，由设备驱动或更上层的软件来检查接收到的报文是否是自己想要的。

## 混淆的内容是什么
上文提到了多播混淆，其实也存在**单播混淆**，在单播混淆下，网卡能够接收目的地址不是网卡硬件地址的以太网帧。从这两个点可以看出，这里混淆的是网卡接收时**对特定地址的过滤功能**，开启了这个模式意味着相应的过滤功能被关闭。

## 如何开启、关闭混淆模式
可以执行如下命令开启混淆模式：

```bash
ifconfig interface promisc
```
开启混淆后，执行 ifconfig interface 能够看到多出了 PROMISC 的标志。示例信息如下：

```bash
 Develop>ifconfig eth3
eth3      Link encap:Ethernet  HWaddr 08:35:71:ec:13:55
          inet6 addr: fe80::a35:71ff:feec:1355/64 Scope:Link
          UP BROADCAST RUNNING PROMISC MULTICAST  MTU:1500  Metric:1
          RX packets:0 errors:0 dropped:0 overruns:0 frame:0
          TX packets:6 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:0 (0.0 B)  TX bytes:468 (468.0 B)
```
关闭混淆模式可以通过在 promisc 前添加 - 号的方式，命令如下：

```bash
ifconfig ethx -promisc
```
## 测试仪打流时未开启混淆模式造成的问题
一般在我们使用测试仪打流的时候，流量的目的地址并不固定，这时如果未开启混淆模式可能会导致**无法收包**。

其实并不是无法收包，而是网卡**过滤掉了**这些**目的地址不是自己**的包，在这种情况下，我们开启混淆模式就能够让测试仪正常收包。

## 网卡手册中与混淆模式有关的内容
这里我以 I210 网卡数据手册为例，研究下网卡中对于混淆模式的使用。在 I210 网卡数据手册中对包接收过程的描述中找到了下面这个图:
![alt text](<../../../../../../medias/images_0/网卡 promiscuous mode 与 MAC 的一些 filter 功能_image-1.png>)我们可以从上图中看到，当网卡收到数据包后并不会直接将数据包投递给上层处理，而是对数据包执行特定的过滤行为，只有通过了这些过滤的包才会向上层投递，不然会被直接丢弃。

最上方是主机 MAC地址过滤，与其处于相同位置的还有 MNG filter，这里与混淆模式相关的过滤内容是主机 MAC 地址过滤。

主机 MAC 地址过滤的过程详见下图：
![alt text](<../../../../../../medias/images_0/网卡 promiscuous mode 与 MAC 的一些 filter 功能_image.png>)手册中对于上图有如下描述内容：

能够成功通过 MAC 地址过滤的包需要满足下面任意一个条件：

1. 单播包且单播混淆功能开启
2. 多播包且多播混淆功能开启
3. 单播包且能够匹配到主机上的一个单播 MAC 过滤条件
4. 多播包且能够匹配到主机上的一个多播过滤条件
5. 广播包且广播包接收模式使能

可以看到，对于设定了混淆功能的网卡，对单播与多播而言相应的 filter 将被跳过，这样网卡就能够接收到目的地址非本端 MAC 地址、多播地址的包。

同时需要主机的是，当单播过滤、多播过滤、广播过滤都通过后，下一步网卡将进行 VLAN 过滤，对于与接收网卡端口不属于同一个 VLAN 的报文直接丢弃。

## 总结
实际上 MAC 地址过滤只是网卡提供的过滤功能中的一部分，除此之外还有 L2 EtherType filter、SYN filter、flex filter、2-Tulple filter 等等，这些过滤不同于 MAC 地址过滤，目的在于将包投递到指定的队列中，用以实现更高级的功能。

