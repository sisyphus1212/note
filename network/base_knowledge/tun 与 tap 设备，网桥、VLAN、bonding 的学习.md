---
title: linux内核网络设备
date: 2022-09-20 16:04:03
index_img: https://github.com/sisyphus1212/images/blob/main/mk-2022-09-20-23-08-10.png?raw=true
categories:
  - [linux,网络开发]
tags:
 - kernel network
---

# tun 与 tap 设备，网桥、VLAN、bonding 的学习
## tun 与 tap 设备
这两个都是虚拟网络设备，tun 设备用来实现三层隧道（三层 ip 数据报），tap 设备用来实现二层隧道（二层以太网数据帧）。

tun 示例程序：

```
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tun_open(void)
{
	struct ifreq ifr;
	int fd;
	char dev[IFNAMSIZ];
	char buf[512];

	fd = open("/dev/net/tun", O_RDWR);

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy(ifr.ifr_name, "tun%d", IFNAMSIZ);
	ioctl(fd, TUNSETIFF, &ifr);

	strncpy(dev, ifr.ifr_name, IFNAMSIZ);
	sprintf(buf, "ifconfig %s 192.168.1.1 pointopoint 192.168.1.2", dev);
	system(buf);

	return fd;
}

void dump_pkt(unsigned char *pkt, int len)
{
	int i;

	for (i = 0; i < len; i++)
		printf("%02x ", pkt[i]);

	printf("\n");
}

void pingpong(int fd)
{
	fd_set fds;
	int len;
	unsigned char pkt[512];

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	select(fd + 1, &fds, NULL, NULL, NULL);

	if (FD_ISSET(fd, &fds)) {
		len = read(fd, pkt, 512);
		dump_pkt(pkt, len);

		/* echo request */
		if (pkt[20] != 0x08)
			return;

		/* 修改 ip 头的 src */
		pkt[15] = 0x02;
		pkt[19] = 0x01;
		pkt[20] = 0x00;
		write(fd, pkt, len);
	}
}

int main(int argc, char *argv[])
{
	int fd;

	fd = tun_open();

	for (;;)
		pingpong(fd);

	return 0;
}
```
第一步需要加载 tun.ko 模块，执行 modprobe tun 即可。加载此模块后 /dev/net/tun 设备文件会被生成，这样上述程序才能正常工作。

```
lcj@virt-debian10:~$ ls -lh /dev/net/tun
crw-rw-rw- 1 root root 10, 200 7月  28 08:23 /dev/net/tun
```
使用 root 权限执行上述程序，执行后在另外一个终端中执行 ifconfig 命令查看，系统中增加了一个 tun0 这个虚拟接口。

```
tun0: flags=4305<UP,POINTOPOINT,RUNNING,NOARP,MULTICAST>  mtu 1500
        inet 192.168.1.1  netmask 255.255.255.255  destination 192.168.1.2
        inet6 fe80::f384:8174:9911:8438  prefixlen 64  scopeid 0x20<link>
        unspec 00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00  txqueuelen 500  (UNSPEC)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 2  bytes 96 (96.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

执行 ping 192.168.1.2 后有如下报警：

```
64 bytes from 192.168.1.2: icmp_seq=122 ttl=64 time=0.105 ms (BAD CHECKSUM!)
64 bytes from 192.168.1.2: icmp_seq=123 ttl=64 time=0.110 ms (BAD CHECKSUM!)
64 bytes from 192.168.1.2: icmp_seq=124 ttl=64 time=0.111 ms (BAD CHECKSUM!)
```

BAD CHECKSUM 这个提示表示数据包头部校验失败。有了这个提示再来审视上面的代码，我发现上面的代码在修改了 ip header 中的 src 与 dst 地址后并没有重新生成 chechsum 并更新就直接发送了。当 ping 收到这个报文时，校验 checksum 就会失败。

这里我直接将 write 调用注释掉，然后重新编译并运行程序，这次继续 ping 会报地址不可达的警告。

```
From 192.168.1.5 icmp_seq=141 Destination Host Unreachable
```
这是正常的。

ping 了几个包后，观察 ifconfig 收包情况，可以看到有统计数目。

```
lcj@virt-debian10:~$ sudo ifconfig tun0
tun0: flags=4305<UP,POINTOPOINT,RUNNING,NOARP,MULTICAST>  mtu 1500
        inet 192.168.1.1  netmask 255.255.255.255  destination 192.168.1.2
        inet6 fe80::f6fe:2e04:68c7:dfd3  prefixlen 64  scopeid 0x20<link>
        unspec 00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00  txqueuelen 500  (UNSPEC)
        RX packets 5  bytes 420 (420.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 8  bytes 564 (564.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```
执行 ethtool 命令输出如下：
```
lcj@virt-debian10:~$ sudo ethtool tun0
Settings for tun0:
	Supported ports: [ ]
	Supported link modes:   Not reported
	Supported pause frame use: No
	Supports auto-negotiation: No
	Supported FEC modes: Not reported
	Advertised link modes:  Not reported
	Advertised pause frame use: No
	Advertised auto-negotiation: No
	Advertised FEC modes: Not reported
	Speed: 10Mb/s
	Duplex: Full
	Port: Twisted Pair
	PHYAD: 0
	Transceiver: internal
	Auto-negotiation: off
	MDI-X: Unknown
	Current message level: 0xffffffa1 (-95)
			       drv ifup tx_err tx_queued intr tx_done rx_status pktdata hw wol 0xffff8000
	Link detected: yes
```
上述输出是由 tun 的实现决定的，在其 ethtool_ops 的实现中，speed、duplex、port 等等信息都是固定的，不会改变，这与常见的物理网卡有所区别。

## 网桥
使用网桥可以将多个接口连接到同一网段内，这一功能等同于交换式集线器。

使用 brctl 命令来创建网桥接口。

使用此命令前，需要加载 bridge.ko 内核模块，直接执行 modprobe bridge.ko 即可。

在我的虚拟机中有四个网卡接口：
```
lcj@virt-debian10:~$ sudo ifconfig -a
enp1s0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 192.168.122.29  netmask 255.255.255.0  broadcast 192.168.122.255
        inet6 fe80::5054:ff:fe71:c999  prefixlen 64  scopeid 0x20<link>
        ether 52:54:00:71:c9:99  txqueuelen 1000  (Ethernet)
        RX packets 2188027  bytes 208647751 (198.9 MiB)
        RX errors 0  dropped 3  overruns 0  frame 0
        TX packets 5180286  bytes 856255243 (816.5 MiB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

enp4s0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
        ether 52:54:00:d9:bf:99  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
        device interrupt 22  memory 0xfca40000-fca60000

enp8s0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
        ether 52:54:00:df:a9:b0  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
        device interrupt 22  memory 0xfc240000-fc260000

enp9s0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
        ether 52:54:00:08:3f:ef  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
        device interrupt 23  memory 0xfc040000-fc060000
```

创建 br0 并设置 ip 地址等信息。
```
lcj@virt-debian10:~$ sudo brctl addbr br0
lcj@virt-debian10:~$ sudo ifconfig br0
br0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
        ether c2:7e:25:a0:16:f7  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

lcj@virt-debian10:~$ sudo ifconfig br0 10.1.0.100 netmask 255.255.255.0 broadmask 10.1.0.255 up
lcj@virt-debian10:~$ sudo ifconfig br0
br0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 10.1.0.255  netmask 255.0.0.0  broadcast 10.255.255.255
        inet6 fe80::c07e:25ff:fea0:16f7  prefixlen 64  scopeid 0x20<link>
        ether c2:7e:25:a0:16:f7  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 6  bytes 516 (516.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

添加接口到 br0 中：
```
lcj@virt-debian10:~$ sudo brctl addif br0 enp4s0
lcj@virt-debian10:~$ sudo brctl addif br0 enp8s0
lcj@virt-debian10:~$ sudo brctl addif br0 enp9s0
lcj@virt-debian10:~$ sudo brctl show
bridge name	bridge id		STP enabled	interfaces
br0		8000.525400083fef	no		enp4s0
							enp8s0
							enp9s0
```

将添加到的接口激活：

```
lcj@virt-debian10:~$ sudo ifconfig enp4s0 up
lcj@virt-debian10:~$ sudo ifconfig enp8s0 up
lcj@virt-debian10:~$ sudo ifconfig enp9s0 up
```
/etc/network/interfaces 文件中添加 br0 配置项目：

```
auto br0
iface br0 inet dhcp
	bridge_ports enp4s0 enp8s0 enp9s0
	bridge_stp off
```
br0 中会存储一张转发表，可以用 brctl showmacs 命令来查看。

```
lcj@virt-debian10:~$ sudo brctl showmacs br0
port no	mac addr		is local?	ageing timer
  3	52:54:00:08:3f:ef	yes		   0.00
  3	52:54:00:08:3f:ef	yes		   0.00
  1	52:54:00:22:e3:e6	no		   1.48
  1	52:54:00:d9:bf:99	yes		   0.00
  1	52:54:00:d9:bf:99	yes		   0.00
  2	52:54:00:df:a9:b0	yes		   0.00
  2	52:54:00:df:a9:b0	yes		   0.00
  1	fe:54:00:d9:bf:99	no		   0.62
  2	fe:54:00:df:a9:b0	no		 182.63
lcj@virt-debian10:~$
```
网络端口可能会不断的变化，当 bridge 学习到了一个地址时，一个定时器也会自动启动，经验值是 5 分钟，这个时间称为 ageing time。在一个 ageing time 时间内如果一个地址入口不可见，这个地址将从 bridge 的数据库中移除。

可以执行 brctl setageing 命令来设定 ageing time.

```
lcj@virt-debian10:~$ sudo brctl setageing br0 1
lcj@virt-debian10:~$ sudo brctl showmacs br0
port no	mac addr		is local?	ageing timer
  3	52:54:00:08:3f:ef	yes		   0.00
  3	52:54:00:08:3f:ef	yes		   0.00
  1	52:54:00:d9:bf:99	yes		   0.00
  1	52:54:00:d9:bf:99	yes		   0.00
  2	52:54:00:df:a9:b0	yes		   0.00
  2	52:54:00:df:a9:b0	yes		   0.00
```

## VLAN
vlan 全程 virtual lan，能够用来虚拟分配以太网。归属于不同的 VLAN ID 的设备之间需要一个路由才能够通信，这意味这不同的 VLAN ID 将以太网划分成了不同的分组。

即使将 LAN 的连线街道相同集线器或交换机上，VLAN ID 不同也不能相互通信。

vconfig 命令生成 vlan 接口。

```
sudo ip link add link enp4s0 name enp4s0.100 type vlan id 100
lcj@virt-debian10:~$ sudo ip link add link enp8s0 name enp8s0.100 type vlan id 100
lcj@virt-debian10:~$ sudo ip link add link enp9s0 name enp9s0.101 type vlan id 101
```
生成 vlan 接口后查看 proc 文件信息，有如下内容：
```
lcj@virt-debian10:~$ ls /proc/net/vlan/
config      enp4s0.100  enp8s0.100  enp9s0.101
lcj@virt-debian10:~$ sudo cat /proc/net/vlan/config
VLAN Dev name	 | VLAN ID
Name-Type: VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD
enp4s0.100     | 100  | enp4s0
enp8s0.100     | 100  | enp8s0
enp9s0.101     | 101  | enp9s0
```

删除接口的 vlan 分组可以继续执行 ip 命令来完成，将 add 改为 delete 并只加上 vlan 接口名。

示例如下：

```
ip link delete enp4s0.100
```
VLAN 接口名的种类：

1, VLAN_PLUS_VID 字符串 vlan + 0 padding+ VLAN ID
2. VLAN_PLUS_VID_NO_PAD 字符串 vlan + VLAN ID
3. DEV_PLUS_VID 物理接口名+点+0 padding + VLAN ID
4. DEV_PLUS_VID_NO_PAD 物理接口名+点+VLAN ID

我们上面的 vlan 接口名就是用了第 4 种命名方式。

## bonding 驱动程序
bonding 用于聚合多个物理网络端口。

bonding 有 7 种模式，列举如下：

1. balance-rr
2. active-backup
3. balance-xor
4. broadcast
5. 802.3ad
6. balance-tlb
7. balance-alb

可以在模块加载的时候执行 mode 参数来设定不同的模式，既可以指定字符串，也可以指定数字。

命令示例如下：

```
modprobe bonding mode=active-backup
modprobe bonding mode=1
```
内核源码目录中有如下对 bonding 功能的说明文档：


Documentation/networking/bonding.txt

阅读上述文件，手动创建一个 bond0 接口可以通过向 /sys/class/net/bonding_masters 写值来完成。命令示例如下：

```
su -c 'echo +bond0 > /sys/class/net/bonding_masters'
```
创建后查看 dmesg 信息可以看到创建成功了，ifconfig bond0 能够看到接口信息，这个 bod0 是 master 接口。操作日志记录如下：

```bash
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo dmesg | tail -n 1
[93154.264172] bonding: bond0 is being created...
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo ifconfig bond0
bond0: flags=5122<BROADCAST,MASTER,MULTICAST>  mtu 1500
        ether c6:75:a1:27:73:1c  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```
此时 /proc/net/bonding 目录中也添加了 bond0 的内容，查看内容如下：

```
ls /proc/net/bonding/
bond0
```
这之后可以将多个网口添加到 bond0 中，这可以通过执行 ifenslave 命令来完成。我的系统中没有预装这个软件，执行 sudo apt-get install ifenslave 完成安装。

```
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo ifenslave bond0 enp8s0 enp9s0
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo dmesg | tail -n 10
[93659.999911] bond0: Enslaving enp8s0 as an active interface with an up link
[93660.091509] 8021q: adding VLAN 0 to HW filter on device enp9s0
[93660.101759] bond0: Enslaving enp9s0 as an active interface with an up link
[93662.056500] e1000e: enp9s0 NIC Link is Up 1000 Mbps Full Duplex, Flow Control: Rx/Tx
[93662.217044] e1000e: enp8s0 NIC Link is Up 1000 Mbps Full Duplex, Flow Control: Rx/Tx
```
上面的例子中，我执行 ifenslave 将 enp8s0 与 enp9s0 添加到了 bond0 中，并查看了 demsg 信息。这两个网络接口添加后，再次执行 ifconfig 查看接口信息，发现都增加了 SLAVE 标志。示例如下：
```
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo ifconfig enp8s0
enp8s0: flags=6211<UP,BROADCAST,RUNNING,SLAVE,MULTICAST>  mtu 1500
        ether 52:54:00:df:a9:b0  txqueuelen 1000  (Ethernet)
        RX packets 18027  bytes 1217882 (1.1 MiB)
        RX errors 1005  dropped 0  overruns 0  frame 1005
        TX packets 19884  bytes 3872849553 (3.6 GiB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
        device interrupt 22  memory 0xfc240000-fc260000

lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ sudo ifconfig enp9s0
enp9s0: flags=6211<UP,BROADCAST,RUNNING,SLAVE,MULTICAST>  mtu 1500
        ether 52:54:00:df:a9:b0  txqueuelen 1000  (Ethernet)
        RX packets 215721  bytes 19494398 (18.5 MiB)
        RX errors 997  dropped 0  overruns 0  frame 997
        TX packets 3766  bytes 415268735 (396.0 MiB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
        device interrupt 23  memory 0xfc040000-fc060000
```
查看 /proc/net/bonding/bond0 文件，发现其中添加了 slave 接口的信息，命令示例如下：

```
lcj@virt-debian10:~/linux-source-4.19/Documentation/networking$ cat /proc/net/bonding/bond0
Ethernet Channel Bonding Driver: v3.7.1 (April 27, 2011)

Bonding Mode: load balancing (round-robin)
MII Status: up
MII Polling Interval (ms): 0
Up Delay (ms): 0
Down Delay (ms): 0

Slave Interface: enp8s0
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: 52:54:00:df:a9:b0
Slave queue ID: 0

Slave Interface: enp9s0
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: 52:54:00:08:3f:ef
Slave queue ID: 0
```
可以看到 enp8s0 与 enp9s0 都作为了 Slave 并处于 up 状态。

