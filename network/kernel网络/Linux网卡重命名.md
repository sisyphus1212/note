---
title: Linux 网卡重命名
date: 2022-09-07 16:04:02
index_img: https://remil1a.github.io/2019/11/15/Linux-Network-Manage/2.png
categories:
- [linux,网络开发,网卡驱动]
tags:
 - linux
 - 网卡驱动
 - kernel
---
# 前言
由于内核启动时对于多网络接口的枚举是并行的，这导致每次创建的ethx 与真实的物理口之间的映射关系是无法预测的, 因此就有人考虑，根据网卡在物理板子上的topo结构来给网卡命名，于是就引入net.ifnames 和 biosdevname规范，来重命名内核创建的网络接口
下面是根据net.ifnames 和 biosdevname规范实现的几种网卡接口命名方式：
1. systemd.link 重命名(systemd-udevd)
2. udev rule 重命名
3. biosdevname 重命名 (常见于centos)
4. ifrename动态重命名

## 查看网卡命名
当前网卡的命名方式可以通过proc文件查看，比如网卡ens160，命名方式为4，即对应内核中的NET_NAME_RENAMED，表示网卡名是被用户空间程序修改的：
```bash
# cat /sys/class/net/ens160/name_assign_type
4

#define NET_NAME_ENUM       1   /* enumerated by kernel */
#define NET_NAME_PREDICTABLE    2   /* predictably named by the kernel */
#define NET_NAME_USER       3   /* provided by user-space */
#define NET_NAME_RENAMED    4   /* renamed by user-space */
```
网卡具体的命名策略可以通过udevadm查看，可以看到enp62s0，是根据/lib/systemd/network/99-default.link进行的命名
```bash
 udevadm info /sys/class/net/ifname | grep ID_NET_NAME

P: /devices/pci0000:3a/0000:3a:02.0/0000:3c:00.0/0000:3d:01.0/0000:3e:00.0/net/bf_pci0
E: DEVPATH=/devices/pci0000:3a/0000:3a:02.0/0000:3c:00.0/0000:3d:01.0/0000:3e:00.0/net/bf_pci0
E: ID_BUS=pci
E: ID_MODEL_ID=0x0010
E: ID_NET_DRIVER=bf_kpkt
E: ID_NET_LINK_FILE=/lib/systemd/network/99-default.link
E: ID_NET_NAME_PATH=enp62s0
E: ID_NET_NAME_SLOT=ens1
E: ID_PATH=pci-0000:3e:00.0
E: ID_PATH_TAG=pci-0000_3e_00_0
E: ID_PCI_CLASS_FROM_DATABASE=Unassigned class
E: ID_VENDOR_ID=0x1d1c
E: IFINDEX=491
E: INTERFACE=bf_pci0
E: SUBSYSTEM=net
E: SYSTEMD_ALIAS=/sys/subsystem/net/devices/bf_pci0
E: TAGS=:systemd:
E: USEC_INITIALIZED=1909742190470
```

# systemd.link 重命名
参考:[http://www.jinbuguo.com/systemd/systemd.link.html]
在用户空间，默认情况下ubuntu会根据systemd目录下的link文件命名网卡。
Link文件分别位于：
1. 系统网络目录(/usr/lib/systemd/network)。
1. 运行时网络目录(/run/systemd/network)。
1. 本机网络目录(/etc/systemd/network)。

所有的Link文件(无论位于哪个目录中)，统一按照文件名的字典顺序处理。 对于不同目录下的同名Link文件， 仅以优先级最高的目录中的那一个为准。 具体说来就是： /etc/ 的优先级最高、 /run/ 的优先级居中、 /usr/lib/ 的优先级最低。
在这里只详细介绍一下Link文件中的MACAddressPolicy和NamePolicy：

## MACAddressPolicy
应该如何设置网卡的MAC地址：

“persistent”: 如果内核使用了网卡硬件固有的MAC地址(绝大多数网卡都有)， 那么啥也不做，直接使用内核的MAC地址。 否则，将会随机新生成一个 确保在多次启动之间保持固定不变的MAC地址(针对给定的主板与网卡)。 自动生成MAC地址的特性 要求网卡必须存在 ID_NET_NAME_* 属性， 否则无法自动生成MAC地址。

“random”: 如果内核使用了随机生成的MAC地址(而不是网卡硬件固有的MAC地址)， 那么啥也不做，直接使用内核的MAC地址。 否则，将在网卡每次出现的时候(一般在启动过程中)随机新生成一个MAC地址。 无论使用上述哪种方式生成的MAC地址， 都将设置 “unicast” 与 “locally administered” 位。

“none”:无条件的直接使用内核的MAC地址。

MACAddress
在未设置 “MACAddressPolicy=” 时所使用MAC地址。
## NamePolicy
应该如何设置网卡的名称， 仅在未使用 “net.ifnames=0″ 内核引导选项时有意义。 接受一个空格分隔的策略列表， 顺序尝试每个策略，并以第一个成功的策略为准。 所得的名字将被用于设置网卡的 “ID_NET_NAME” 属性。 注意，默认的udev规则会用 “ID_NET_NAME” 的值设置 “NAME” 属性(也就是网卡的名称)。 如果网卡已经被空户空间命名，那么将不会进行任何重命名操作。 可用的策略如下：

“kernel”: 如果内核已经为此网卡设置了固定的可预测名称， 那么不进行任何重命名操作。

“database”: 基于网卡的 “ID_NET_NAME_FROM_DATABASE” 属性值(来自于udev硬件数据库)设置网卡的名称。

“onboard”: 基于网卡的 “ID_NET_NAME_ONBOARD” 属性值(来自于板载网卡固件)设置网卡的名称。

“slot”: 基于网卡的 “ID_NET_NAME_SLOT” 属性值(来自于可插拔网卡固件)设置网卡的名称。

“path”: 基于网卡的 “ID_NET_NAME_PATH” 属性值(来自于网卡的总线位置)设置网卡的名称。

“mac”: 基于网卡的 “ID_NET_NAME_MAC” 属性值(来自于网卡的固定MAC地址)设置网卡的名称。

Name: 在 NamePolicy= 无效时应该使用的网卡名称。 无效的情况包括： (1)未设置 NamePolicy= ； (2)NamePolicy= 中的策略全失败； (3)使用了”net.ifnames=0″内核引导选项

注意， 千万不要设置可能被内核用于其他网口的名称(例如 “eth0″)， 这可能会导致 udev 在分配名称时与内核产生竞争， 从而导致不可预期的后果。 最好的做法是使用一些永远不会导致冲突名称或前缀，例如： “internal0″”external0″ 或 “lan0″”lan1″/”lan3″

# udev rule 重命名
udev 辅助工具程序 /lib/udev/rename_device 会根据 /usr/lib/udev/rules.d/60-net.rules 中的指示去查询 /etc/sysconfig/network-script/ifcfg-IFACE 配置文件，根据HWADDR 读取设备名称,如果在ifcfg-xx中匹配到HWADDR=xx:xx:xx:xx:xx:xx参数的网卡接口则选取DEVICE=yyyy中设置的名字作为网卡名称
```c
SUBSYSTEM=="net",ACTION=="add",DRIVERS=="?*",ATTR{address}=="需要修改名称的网卡MAC地址",ATTR｛type｝=="1",KERNEL=="myeth*",NAME="myeth0"
```
## net.ifnames(systemd, udev) 的命名规范为：设备类型+设备位置+数字
udev 和 systemd.link都是使用该规则
设备类型：
en 表示Ethernet
wl 表示WLAN
ww 表示无线广域网WWAN

|  format   | description  |
|  ----  | ----  |
| ```o<index>```  | on-board device index number ,集成设备索引号|
| ```s<slot>[f<functoin>][d<dev_id>]```  | hotplug slot index number,扩展槽的索引号  |
| ```x<MAC>``` | MAC address ,基于MAC进行命名|
| ```p<bus>s<slot>[f<function>][d<dev_id>]```| PCI geographical location ,PCI扩展总线 |
|```p<bus>s<slot>[f<function>][u<port>][...][c<config>][i<interface>] ```| USB port numberchain |
```
实际的例子:
​ eno1 板载网卡
​ enp0s2 pci网卡
​ ens33 pci网卡
​ wlp3s0 PCI无线网卡
​ wwp0s29f7u2i2 4Gmodem
​ wlp0s2f1u4u1 连接在USB Hub上的无线网卡
​ enx78e7d1ea46da pci网卡
```

# biosdevname 重命名
biosdevname 根据 /user/lib/udev/rules.d/71-boosdevname.rules
biosdevname 程序使用系统 BIOS 的信息，特别是类型 9 (System Slot) 和类型 41 （板设备扩展信息）字段包含在 SMBIOS 中。如果系统的 BIOS 没有 SMBIOS 版本 2.6 或更高版本，且此数据不会使用，则不会使用新的命名规则。大多数较旧的硬件不支持此功能，因为缺少包含正确的 SMBIOS 版本和字段信息的 BIOS。
## biosdevname 的命名规范：
|  device   | name  |
|  ----  | ----  |
| Embedded network interface(LOM) | ```em[1234....]``` |
| PCI card network interface | ```p<slot>p<ethernet port>``` |
| Virtual function | ```p<slot>p<ethernet port>_<virtual_interface>``` |
```
实际的例子:
​ em1 板载网卡
​ p3p4 pci网卡
​ p3p4_1 虚拟网卡
```
systemd中rule的实际执行顺序:
按照如下顺序执行udev的rule
1. /usr/lib/udev/rules.d/60-net.rules
2. /usr/lib/udev/rules.d/71-biosdevname.rules
3. /lib/udev/rules.d/75-net-description.rules
4. /usr/lib/udev/rules.d/80-net-name-slot.rules

# ifrename动态重命名
一些系统中，有根据网口的不同功能重命名 netdev 的需求。这可以通过调用 ifrename 命令来完成。这个命令在我的系统中并没有安装，我首先执行如下命令，搜索需要安装的程序名。

```bash
[sisyphus@ubuntu] ~ $ sudo apt-cache search ifrename
ifrename - Rename network interfaces based on various static criteria
```
从上面的输出中，我确定需要安装的程序名就是 ifrename。执行如下命令安装 ifrename 命令。

```bash
[sisyphus@ubuntu] ~ $ sudo apt-get install ifrename
```
安装成功后，man ifrename 查看如何使用这个命令。下面是 man ifrename 的部分输出。

```
ifrename [-c configfile] [-p] [-d] [-u] [-v] [-V] [-D] [-C]
ifrename [-c configfile] [-i interface] [-n newname]
```
注意 ifrename 必须在接口被 up 之前执行，不能对一个 up 的接口执行 ifrename 操作，可以将这样的行为理解为，在 up 的时候 ifrename 被引用。这表明 down 掉接口就可以修改名字了。

一个具体的示例如下：

```bash
[sisyphus@ubuntu]$ sudo /sbin/ifconfig enp2s0 down
[sisyphus@ubuntu]$ sudo ifrename -i enp2s0 -n eth0
eth0
[sisyphus@ubuntu]$ /sbin/ifconfig eth0
eth0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
        ether 80:e8:2c:17:f0:77  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```
在上面的示例中，我首先使用 ifconfig down 掉接口，然后执行 ifrename 将网络接口名称从 enp2s0 修改为 eth0，这之后 ifconfig eth0 查看接口信息确认修改成功。

这时候查看 dmesg 的输出信息，发现有下面的输出：

```
[ 4288.006591] r8169 0000:02:00.0 eth0: renamed from enp2s0
```
使用 strace 命令查看系统调用过程，发现核心的过程在于下面两个系统调用：

```
socket(AF_INET, SOCK_DGRAM, IPPROTO_IP) = 3
ioctl(3, SIOCSIFNAME, {ifr_name="eth0", ifr_newname="enp2s0"}) = 0
```
上面的两个系统调用首先执行 socket 打开一个网络设备，然后通过 ioctl 命令来修改网络设备的配置。

内核里面的流程如下所示：

```
sock_ioctl -> sock_do_ioctl -> dev_ioctl -> dev_ifsioc -> dev_change_name
```
最终是通过调用 dev_change_name 函数来完成虚拟网卡接口重命名的。

# 禁用一致的网络设备命名
通过 dmesg 中的部分输出，可以看到最开始虚拟网络接口的名称为 eth0，最后一行的信息中可以看到接口名称被改为了 enp2s0。

```
[    4.218735] r8169 0000:02:00.0 eth0: RTL8168h/8111h, 30:88:2c:17:55:d7, XID 54100880, IRQ 126
[    4.218736] r8169 0000:02:00.0 eth0: jumbo features [frames: 9200 bytes, tx checksumming: ko]
[    4.219142] xhci_hcd 0000:00:14.0: hcc params 0x200077c1 hci version 0x110 quirks 0x0000000000009810
[    4.219435] hub 1-0:1.0: USB hub found
[    4.219456] hub 1-0:1.0: 12 ports detected
[    4.220818] r8169 0000:02:00.0 enp2s0: renamed from eth0
```
执行grub2-mkconfig -o /boot/grub2/grub.cfg命令生成更新grub配置参数:
```c
vim /etc/default/grub
net.ifnames=0 biosdevname=0

grub2-mkconfig -o /boot/grub2/grub.cfg
reboot
```
如果系统管理员想要屏蔽 /usr/lib/ 目录中的某个Link文件， 那么最佳做法是在 /etc/ 目录中创建一个 指向 /dev/null 的同名符号链接， 即可彻底屏蔽 /usr/lib/ 目录中的同名文件。

在上面Centos7中命名的策略顺序是系统默认的。
如系统BIOS符合要求，且系统中安装了biosdevname，且biosdevname=1启用，则biosdevname优先；
如果BIOS不符合biosdevname要求或biosdevname=0，则仍然是systemd的规则优先。
如果用户自己定义了udevrule来修改内核设备名字，则用户规则优先。

内核参数组合使用的时候，其结果如下：
1. 默认内核参数(biosdevname=0，net.ifnames=1): 网卡名”enp5s2”
1. biosdevname=1，net.ifnames=0：网卡名 “em1”
1. biosdevname=0，net.ifnames=0：网卡名 “eth0” (最传统的方式,eth0 eth1傻傻分不清)
