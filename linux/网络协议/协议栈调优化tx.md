---
layout    : post
title     :  "[译] Linux 网络栈监控和调优：发送数据（2017）"
date      : 2018-12-17
lastupdate: 2020-09-29
author    : ArthurChiao
categories: network-stack kernel monitoring tuning
---

## 译者序

本文翻译自 2017 年的一篇英文博客
[Monitoring and Tuning the Linux Networking Stack: Sending Data](https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data)。**如果能看懂英文，建议阅读原文，或者和本文对照看。**

这篇文章写的是 **“Linux networking stack"**，这里的 ”stack“ 不仅仅是内核协议栈，
而是包括内核协议栈在内的，从应用程序通过系统调用**写数据到 socket**，到数据被组织
成一个或多个数据包最终被物理网卡发出去的整个路径。所以文章有三方面，交织在一起，
看起来非常累（但是很过瘾）：

1. 原理及代码实现：网络各层，包括驱动、硬中断、软中断、内核协议栈、socket 等等。
2. 监控：对代码中的重要计数进行监控，一般在`/proc` 或`/sys` 下面有对应输出。
3. 调优：修改网络配置参数。

本文的另一个特色是，几乎所有讨论的内核代码，都在相应的地方给出了 github 上的链接，
具体到行。

网络栈非常复杂，原文太长又没有任何章节号，看起来非常累。因此本文翻译时添加了适当
的章节号，以期按图索骥。

----

**2020 更新**:

* 基于 Prometheus+Grafana 监控网络栈：[Monitoring Network Stack]({% link _posts/2022-07-02-monitoring-network-stack.md %})。

以下是翻译。

----

## 太长不读（TL; DR）

本文介绍了运行 Linux 内核的机器是如何**发包**（send packets）的，包是怎样从用户程
序一步步到达硬件网卡并被发出去的，以及如何**监控**（monitoring）和**调优**（
tuning）这一路径上的各个网络栈组件。

本文的姊妹篇是 [Linux 网络栈监控和调优：接收数据]({% link _posts/2018-12-05-tuning-stack-rx-zh.md %})，
对应的原文是 [Monitoring and Tuning the Linux Networking Stack: Receiving
Data](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/)
。

* TOC
{:toc}

想对 Linux 网络栈进行监控或调优，必须对其正在发生什么有一个深入的理解，
而这离不开读内核源码。希望本文可以给那些正准备投身于此的人提供一份参考。

<a name="chap_1"></a>

# 1 网络栈监控和调优：常规建议

正如我们前一篇文章提到的，网络栈很复杂，没有一种方式适用于所有场景。如果性能和网络
健康状态对你或你的业务非常重要，那你没有别的选择，只能花大量的时间、精力和金钱去
深入理解系统的各个部分之间是如何交互的。

本文中的一些示例配置仅为了方便理解（效果），并不作为任何特定配置或默认配置的建议
。在做任何配置改动之前，你应该有一个能够对系统进行监控的框架，以查看变更是否带来
预期的效果。

对远程连接上的机器进行网络变更是相当危险的，机器很可能失联。另外，不要在生产环境
直接调整这些配置；如果可能的话，在新机器上改配置，然后将机器灰度上线到生产。

<a name="chap_2"></a>

# 2 发包过程俯瞰

本文将拿**Intel I350**网卡的 `igb` 驱动作为参考，网卡的 data sheet 这里可以下载
[PDF](http://www.intel.com/content/dam/www/public/us/en/documents/datasheets/ethernet-controller-i350-datasheet.pdf)
（警告：文件很大）。

从比较高的层次看，一个数据包从用户程序到达硬件网卡的整个过程如下：

1. 使用**系统调用**（如 `sendto`，`sendmsg` 等）写数据
1. 数据穿过**socket 子系统**，进入**socket 协议族**（protocol family）系统（在我们的例子中为 `AF_INET`）
1. 协议族处理：数据穿过**协议层**，这一过程（在许多情况下）会将**数据**（data）转换成**数据包**（packet）
1. 数据穿过**路由层**，这会涉及路由缓存和 ARP 缓存的更新；如果目的 MAC 不在 ARP 缓存表中，将触发一次 ARP 广播来查找 MAC 地址
1. 穿过协议层，packet 到达**设备无关层**（device agnostic layer）
1. 使用 XPS（如果启用）或散列函数**选择发送队列**
1. 调用网卡驱动的**发送函数**
1. 数据传送到网卡的 `qdisc`（queue discipline，排队规则）
1. qdisc 会直接**发送数据**（如果可以），或者将其放到队列，下次触发**`NET_TX` 类型软中断**（softirq）的时候再发送
1. 数据从 qdisc 传送给驱动程序
1. 驱动程序创建所需的**DMA 映射**，以便网卡从 RAM 读取数据
1. 驱动向网卡发送信号，通知**数据可以发送了**
1. **网卡从 RAM 中获取数据并发送**
1. 发送完成后，设备触发一个**硬中断**（IRQ），表示发送完成
1. **硬中断处理函数**被唤醒执行。对许多设备来说，这会**触发 `NET_RX` 类型的软中断**，然后 NAPI poll 循环开始收包
1. poll 函数会调用驱动程序的相应函数，**解除 DMA 映射**，释放数据

接下来会详细介绍整个过程。

<a name="chap_3"></a>

# 3 协议层注册

协议层分析我们将会关注 IP 和 UDP 层，其他协议层可参考这个过程。

我们首先来看协议族是如何注册到内核，并被 socket 子系统使用的。

当用户程序像下面这样创建 UDP socket 时会发生什么？

```c
sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
```

简单来说，内核会去查找由 UDP 协议栈导出的一组函数（其中包括用于发送和接收网络数据
的函数），并赋给 socket 的相应字段。准确理解这个过程需要查看 `AF_INET` 地址族的
代码。

内核初始化的很早阶段就执行了 `inet_init` 函数，这个函数会注册 `AF_INET` 协议族
，以及该协议族内的各协议栈（TCP，UDP，ICMP 和 RAW），并调用初始化函数使协议栈准备
好处理网络数据。`inet_init` 定义在
[net/ipv4/af_inet.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/af_inet.c#L1678-L1804)
。

`AF_INET` 协议族导出一个包含 `create` 方法的 `struct net_proto_family` 类型实例。当从
用户程序创建 socket 时，内核会调用此方法：

```c
static const struct net_proto_family inet_family_ops = {
    .family = PF_INET,
    .create = inet_create,
    .owner  = THIS_MODULE,
};
```

`inet_create` 根据传递的 socket 参数，在已注册的协议中查找对应的协议。我们来看一下：

```c
        /* Look for the requested type/protocol pair. */
lookup_protocol:
        err = -ESOCKTNOSUPPORT;
        rcu_read_lock();
        list_for_each_entry_rcu(answer, &inetsw[sock->type], list) {

                err = 0;
                /* Check the non-wild match. */
                if (protocol == answer->protocol) {
                        if (protocol != IPPROTO_IP)
                                break;
                } else {
                        /* Check for the two wild cases. */
                        if (IPPROTO_IP == protocol) {
                                protocol = answer->protocol;
                                break;
                        }
                        if (IPPROTO_IP == answer->protocol)
                                break;
                }
                err = -EPROTONOSUPPORT;
        }
```

然后，将该协议的回调方法（集合）赋给这个新创建的 socket：

```c
sock->ops = answer->ops;
```

可以在 `af_inet.c` 中看到所有协议的初始化参数。
下面是[TCP 和 UDP](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/af_inet.c#L998-L1020)的初始化参数：

```c
/* Upon startup we insert all the elements in inetsw_array[] into
 * the linked list inetsw.
 */
static struct inet_protosw inetsw_array[] =
{
        {
                .type =       SOCK_STREAM,
                .protocol =   IPPROTO_TCP,
                .prot =       &tcp_prot,
                .ops =        &inet_stream_ops,
                .no_check =   0,
                .flags =      INET_PROTOSW_PERMANENT |
                              INET_PROTOSW_ICSK,
        },

        {
                .type =       SOCK_DGRAM,
                .protocol =   IPPROTO_UDP,
                .prot =       &udp_prot,
                .ops =        &inet_dgram_ops,
                .no_check =   UDP_CSUM_DEFAULT,
                .flags =      INET_PROTOSW_PERMANENT,
       },

            /* .... more protocols ... */
```

`IPPROTO_UDP` 协议类型有一个 `ops` 变量，包含[很多信息
](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/af_inet.c#L935-L960)，包
括用于发送和接收数据的回调函数：

```c
const struct proto_ops inet_dgram_ops = {
  .family          = PF_INET,
  .owner           = THIS_MODULE,

  /* ... */

  .sendmsg     = inet_sendmsg,
  .recvmsg     = inet_recvmsg,

  /* ... */
};
EXPORT_SYMBOL(inet_dgram_ops);
```

`prot` 字段指向一个协议相关的变量（的地址），对于 UDP 协议，其中包含了 UDP 相关的
回调函数。 UDP 协议对应的 `prot` 变量为 `udp_prot`，定义在
[net/ipv4/udp.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/udp.c#L2171-L2203)：

```
struct proto udp_prot = {
  .name        = "UDP",
  .owner           = THIS_MODULE,

  /* ... */

  .sendmsg     = udp_sendmsg,
  .recvmsg     = udp_recvmsg,

  /* ... */
};
EXPORT_SYMBOL(udp_prot);
```

现在，让我们转向发送 UDP 数据的用户程序，看看 `udp_sendmsg` 是如何在内核中被调用的。

<a name="chap_4"></a>

# 4 通过 socket 发送网络数据

用户程序想发送 UDP 网络数据，因此它使用 `sendto` 系统调用，看起来可能是这样的：

```c
ret = sendto(socket, buffer, buflen, 0, &dest, sizeof(dest));
```

该系统调用穿过[Linux 系统调用（system call）层](https://blog.packagecloud.io/eng/2016/04/05/the-definitive-guide-to-linux-system-calls/)，最后到达[net/socket.c](https://github.com/torvalds/linux/blob/v3.13/net/socket.c#L1756-L1803)中的这个函数：

```c
/*
 *      Send a datagram to a given address. We move the address into kernel
 *      space and check the user space data area is readable before invoking
 *      the protocol.
 */

SYSCALL_DEFINE6(sendto, int, fd, void __user *, buff, size_t, len,
                unsigned int, flags, struct sockaddr __user *, addr,
                int, addr_len)
{
    /*  ... code ... */

    err = sock_sendmsg(sock, &msg, len);

    /* ... code  ... */
}
```

`SYSCALL_DEFINE6` 宏会展开成一堆宏，后者经过一波复杂操作创建出一个带 6 个参数的系统
调用（因此叫 `DEFINE6`）。作为结果之一，你会看到内核中的所有系统调用都带 `sys_`前
缀。

`sendto` 代码会先将数据整理成底层可以处理的格式，然后调用 `sock_sendmsg`。特别地，
它将传递给 `sendto` 的地址放到另一个变量（`msg`）中：

```c
  iov.iov_base = buff;
  iov.iov_len = len;
  msg.msg_name = NULL;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_namelen = 0;
  if (addr) {
          err = move_addr_to_kernel(addr, addr_len, &address);
          if (err < 0)
                  goto out_put;
          msg.msg_name = (struct sockaddr *)&address;
          msg.msg_namelen = addr_len;
  }
```

这段代码将用户程序传入到内核的（存放待发送数据的）地址，作为 `msg_name` 字段嵌入到
`struct msghdr` 类型变量中。这和用户程序直接调用 `sendmsg` 而不是 `sendto` 发送
数据差不多，这之所以可行，是因为 `sendto` 和 `sendmsg` 底层都会调用 `sock_sendmsg`。

<a name="chap_4.1"></a>

## 4.1 `sock_sendmsg`, `__sock_sendmsg`, `__sock_sendmsg_nosec`

`sock_sendmsg` 做一些错误检查，然后调用`__sock_sendmsg`；后者做一些自己的错误检查
，然后调用`__sock_sendmsg_nosec`。`__sock_sendmsg_nosec` 将数据传递到 socket 子系统
的更深处：

```c
static inline int __sock_sendmsg_nosec(struct kiocb *iocb, struct socket *sock,
                                       struct msghdr *msg, size_t size)
{
    struct sock_iocb *si =  ....

    /* other code ... */

    return sock->ops->sendmsg(iocb, sock, msg, size);
}
```

通过我们前面介绍的 socket 创建过程，你应该能看懂，注册到这里的 `sendmsg` 方法就是
`inet_sendmsg`。

<a name="chap_4.2"></a>

## 4.2 `inet_sendmsg`

从名字可以猜到，这是 `AF_INET` 协议族提供的通用函数。 此函数首先调用
`sock_rps_record_flow` 来记录最后一个处理该（数据所属的）flow 的 CPU; Receive
Packet Steering 会用到这个信息。接下来，调用 socket 的协议类型（本例是 UDP）对应的
`sendmsg` 方法：

```c
int inet_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
                 size_t size)
{
      struct sock *sk = sock->sk;

      sock_rps_record_flow(sk);

      /* We may need to bind the socket. */
      if (!inet_sk(sk)->inet_num && !sk->sk_prot->no_autobind && inet_autobind(sk))
              return -EAGAIN;

      return sk->sk_prot->sendmsg(iocb, sk, msg, size);
}
EXPORT_SYMBOL(inet_sendmsg);
```

本例是 UDP 协议，因此上面的 `sk->sk_prot->sendmsg` 指向的是我们之前看到的（通过
`udp_prot` 导出的）`udp_sendmsg` 函数。

**sendmsg()函数作为分界点，处理逻辑从 `AF_INET` 协议族通用处理转移到具体的 UDP 协议的处理。**

<a name="chap_5"></a>

# 5 UDP 协议层

<a name="chap_5.1"></a>

## 5.1 `udp_sendmsg`

这个函数定义在
[net/ipv4/udp.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/udp.c#L845-L1088)
，函数非常长，我们分段来看。

### 5.1.1 UDP corking（软木塞）

在变量声明和基本错误检查之后，`udp_sendmsg` 所做的第一件事就是检查 socket 是否“
塞住”了（corked）。 UDP corking 是一项优化技术，允许内核将多次数据累积成单个数据报发
送。在用户程序中有两种方法可以启用此选项：

1. 使用 `setsockopt` 系统调用设置 socket 的 `UDP_CORK` 选项
1. 程序调用 `send`，`sendto` 或 `sendmsg` 时，带 `MSG_MORE` 参数

详细信息参考 [UDP man page](http://man7.org/linux/man-pages/man7/udp.7.html)和
[`send/sendto/sendmsg` man
page](http://man7.org/linux/man-pages/man2/send.2.html)。

`udp_sendmsg` 代码检查 `up->pending` 以确定 socket 当前是否已被塞住(corked)，如果是，
则直接跳到 `do_append_data` 进行数据追加(append)。 我们将在稍后看到如何追加数据。

```c
int udp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
                size_t len)
{

    /* variables and error checking ... */

  fl4 = &inet->cork.fl.u.ip4;
  if (up->pending) {
          /*
           * There are pending frames.
           * The socket lock must be held while it's corked.
           */
          lock_sock(sk);
          if (likely(up->pending)) {
                  if (unlikely(up->pending != AF_INET)) {
                          release_sock(sk);
                          return -EINVAL;
                  }
                  goto do_append_data;
          }
          release_sock(sk);
  }
```

### 5.1.2 获取目的 IP 地址和端口

接下来获取目标地址和端口，有两个可能的来源：

1. 如果之前 socket 已经建立连接，那 socket 本身就存储了目标地址
1. 地址通过辅助结构（`struct msghdr`）传入，正如我们在 `sendto` 的内核代码中看到的那样

具体逻辑：

```c
/*
 *      Get and verify the address.
 */
  if (msg->msg_name) {
          struct sockaddr_in *usin = (struct sockaddr_in *)msg->msg_name;
          if (msg->msg_namelen < sizeof(*usin))
                  return -EINVAL;
          if (usin->sin_family != AF_INET) {
                  if (usin->sin_family != AF_UNSPEC)
                          return -EAFNOSUPPORT;
          }

          daddr = usin->sin_addr.s_addr;
          dport = usin->sin_port;
          if (dport == 0)
                  return -EINVAL;
  } else {
          if (sk->sk_state != TCP_ESTABLISHED)
                  return -EDESTADDRREQ;
          daddr = inet->inet_daddr;
          dport = inet->inet_dport;
          /* Open fast path for connected socket.
             Route will not be used, if at least one option is set.
           */
          connected = 1;
  }
```

是的，你没看错，UDP 代码中出现了 `TCP_ESTABLISHED`！UDP socket 的状态使用了 TCP 状态
来描述，不知道是好是坏。

回想前面我们看到用户程序调用 `sendto` 时，内核如何替用户初始化一个 `struct msghdr`
变量。上面的代码显示了内核如何解析该变量以便设置 `daddr` 和 `dport`。

如果没有 `struct msghdr` 变量，内核函数到达 `udp_sendmsg` 函数时，会从 socket 本身检索
目标地址和端口，并将 socket 标记为“已连接”。

### 5.1.3 Socket 发送：bookkeeping 和打时间戳

接下来，获取存储在 socket 上的源地址、设备索引（device index）和时间戳选项（例
如 `SOCK_TIMESTAMPING_TX_HARDWARE`, `SOCK_TIMESTAMPING_TX_SOFTWARE`,
`SOCK_WIFI_STATUS`）：

```c
ipc.addr = inet->inet_saddr;

ipc.oif = sk->sk_bound_dev_if;

sock_tx_timestamp(sk, &ipc.tx_flags);
```

### 5.1.4 辅助消息（Ancillary messages）

除了发送或接收数据包之外，`sendmsg` 和 `recvmsg` 系统调用还允许用户设置或请求辅助数
据。用户程序可以通过将请求信息组织成 `struct msghdr` 类型变量来利用此辅助数据。一些辅
助数据类型记录在[IP man page](http://man7.org/linux/man-pages/man7/ip.7.html)中
。

辅助数据的一个常见例子是 `IP_PKTINFO`。对于 `sendmsg`，`IP_PKTINFO` 允许程序在发送
数据时设置一个 `in_pktinfo` 变量。程序可以通过填写 `struct in_pktinfo` 变量中的字段
来指定要在 packet 上使用的源地址。如果程序是监听多个 IP 地址的服务端程序，那这是一个
很有用的选项。在这种情况下，服务端可能想使用客户端连接服务端的那个 IP 地址来回复客
户端，`IP_PKTINFO` 非常适合这种场景。

`setsockopt` 可以在**socket 级别**设置发送包的
[IP_TTL](https://en.wikipedia.org/wiki/Time_to_live#IP_packets)和
[IP_TOS](https://en.wikipedia.org/wiki/Type_of_service)。而辅助消息允
许在每个**数据包级别**设置 TTL 和 TOS 值。Linux 内核会使用一个[数组
](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/route.c#L179-L197)将 TOS
转换为优先级，后者会影响数据包如何以及合适从 qdisc 中发送出去。我们稍后会了解到这
意味着什么。

我们可以看到内核如何在 UDP socket 上处理 `sendmsg` 的辅助消息：

```c
if (msg->msg_controllen) {
        err = ip_cmsg_send(sock_net(sk), msg, &ipc,
                           sk->sk_family == AF_INET6);
        if (err)
                return err;
        if (ipc.opt)
                free = 1;
        connected = 0;
}
```

解析辅助消息的工作是由 `ip_cmsg_send` 完成的，定义在
[net/ipv4/ip_sockglue.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/ip_sockglue.c#L190-L241)
。注意，传递一个未初始化的辅助数据，将会把这个 socket 标记为“未建立连接的”（译者注
：因为从 5.1.2 的代码可以看出，有辅助消息时优先处理辅助消息，没有辅助消息才从 socket
里面拿信息）。

### 5.1.5 设置自定义 IP 选项

接下来，`sendmsg` 将检查用户是否通过辅助消息设置了的任何自定义 IP 选项。如果设置了
，将使用这些自定义值；如果没有，那就使用 socket 中（已经在用）的参数：

```c
if (!ipc.opt) {
        struct ip_options_rcu *inet_opt;

        rcu_read_lock();
        inet_opt = rcu_dereference(inet->inet_opt);
        if (inet_opt) {
                memcpy(&opt_copy, inet_opt,
                       sizeof(*inet_opt) + inet_opt->opt.optlen);
                ipc.opt = &opt_copy.opt;
        }
        rcu_read_unlock();
}
```

接下来，该函数检查是否设置了源记录路由（source record route, SRR）IP 选项。
SRR 有两种类型：[宽松源记录路由和严格源记录路由](https://en.wikipedia.org/wiki/Loose_Source_Routing)。
如果设置了此选项，则会记录第一跳地址并将其保存到 `faddr`，并将 socket 标记为“未连接”。
这将在后面用到：

```c
ipc.addr = faddr = daddr;

if (ipc.opt && ipc.opt->opt.srr) {
        if (!daddr)
                return -EINVAL;
        faddr = ipc.opt->opt.faddr;
        connected = 0;
}
```

处理完 SRR 选项后，将处理 TOS 选项，这可以从辅助消息中获取，或者从 socket 当前值中获取。
然后检查：

1. 是否（使用 `setsockopt`）在 socket 上设置了 `SO_DONTROUTE`，或
1. 是否（调用 `sendto` 或 `sendmsg` 时）指定了 `MSG_DONTROUTE` 标志，或
1. 是否已设置了 `is_strictroute`，表示需要严格的
   [SRR](http://www.networksorcery.com/enp/protocol/ip/option009.htm)

任何一个为真，`tos` 字段的 `RTO_ONLINK` 位将置 1，并且 socket 被视为“未连接”：

```c
tos = get_rttos(&ipc, inet);
if (sock_flag(sk, SOCK_LOCALROUTE) ||
    (msg->msg_flags & MSG_DONTROUTE) ||
    (ipc.opt && ipc.opt->opt.is_strictroute)) {
        tos |= RTO_ONLINK;
        connected = 0;
}
```

### 5.1.6 多播或单播（Multicast or unicast）

接下来代码开始处理 multicast。这有点复杂，因为用户可以通过 `IP_PKTINFO` 辅助消息
来指定发送包的源地址或设备号，如前所述。

如果目标地址是多播地址：

1. 将多播设备（device）的索引（index）设置为发送（写）这个 packet 的设备索引，并且
1. packet 的源地址将设置为 multicast 源地址

如果目标地址不是一个组播地址，则发送 packet 的设备制定为 `inet->uc_index`（单播），
除非用户使用 `IP_PKTINFO` 辅助消息覆盖了它。

```c
if (ipv4_is_multicast(daddr)) {
        if (!ipc.oif)
                ipc.oif = inet->mc_index;
        if (!saddr)
                saddr = inet->mc_addr;
        connected = 0;
} else if (!ipc.oif)
        ipc.oif = inet->uc_index;
```

### 5.1.7 路由

现在开始路由！

UDP 层中处理路由的代码以**快速路径**（fast path）开始。 如果 socket 已连接，则直接尝试获取路由：

```c
if (connected)
        rt = (struct rtable *)sk_dst_check(sk, 0);
```

如果 socket 未连接，或者虽然已连接，但路由辅助函数 `sk_dst_check` 认定路由已过期，则代码将进入**慢速路径**（slow
path）以生成一条路由记录。首先调用 `flowi4_init_output` 构造一个描述此 UDP 流的变量：

```c
if (rt == NULL) {
        struct net *net = sock_net(sk);

        fl4 = &fl4_stack;
        flowi4_init_output(fl4, ipc.oif, sk->sk_mark, tos,
                           RT_SCOPE_UNIVERSE, sk->sk_protocol,
                           inet_sk_flowi_flags(sk)|FLOWI_FLAG_CAN_SLEEP,
                           faddr, saddr, dport, inet->inet_sport);
```

然后，socket 及其 flow 实例会传递给安全子系统，这样[SELinux](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)或[SMACK](https://en.wikipedia.org/wiki/Smack_(software))这样的系统就可以在 flow 实例上设置安全 ID。
接下来，`ip_route_output_flow` 将调用 IP 路由代码，创建一个路由实例：

```c
security_sk_classify_flow(sk, flowi4_to_flowi(fl4));
rt = ip_route_output_flow(net, fl4, sk);
```

如果创建路由实例失败，并且返回码是 `ENETUNREACH`,
则 `OUTNOROUTES` 计数器将会加 1。

```c
if (IS_ERR(rt)) {
  err = PTR_ERR(rt);
  rt = NULL;
  if (err == -ENETUNREACH)
    IP_INC_STATS(net, IPSTATS_MIB_OUTNOROUTES);
  goto out;
}
```

这些统计计数器所在的源文件、其他可用的计数器及其含义，将将在下面的 UDP 监控部分讨
论。

接下来，如果是广播路由，但 socket 的 `SOCK_BROADCAST` 选项未设置，则处理过程终止。
如果 socket 被视为“已连接”，则路由实例将缓存到 socket 上：

```c
err = -EACCES;
if ((rt->rt_flags & RTCF_BROADCAST) &&
    !sock_flag(sk, SOCK_BROADCAST))
        goto out;
if (connected)
        sk_dst_set(sk, dst_clone(&rt->dst));
```

### 5.1.8 `MSG_CONFIRM`: 阻止 ARP 缓存过期

如果调用 `send`, `sendto` 或 `sendmsg` 的时候指定了 `MSG_CONFIRM` 参数，UDP 协议层将会如下处理：

```c
  if (msg->msg_flags&MSG_CONFIRM)
          goto do_confirm;
back_from_confirm:
```

该标志提示系统去确认一下 ARP 缓存条目是否仍然有效，防止其被垃圾回收。
`do_confirm` 标签位于此函数末尾处，很简单：

```c
do_confirm:
        dst_confirm(&rt->dst);
        if (!(msg->msg_flags&MSG_PROBE) || len)
                goto back_from_confirm;
        err = 0;
        goto out;
```

`dst_confirm` 函数只是在相应的缓存条目上设置一个标记位，稍后当查询邻居缓存并找到
条目时将检查该标志，我们后面一些会看到。此功能通常用于 UDP 网络应用程序，以减少
不必要的 ARP 流量。

此代码确认缓存条目然后跳回 `back_from_confirm` 标签。

一旦 `do_confirm` 代码跳回到 `back_from_confirm`（或者之前就没有执行到 `do_confirm`
），代码接下来将处理 UDP cork 和 uncorked 情况。

### 5.1.9 uncorked UDP sockets 快速路径：准备待发送数据

如果不需要 corking，数据就可以封装到一个 `struct sk_buff` 实例中并传递给
`udp_send_skb`，离 IP 协议层更进了一步。这是通过调用 `ip_make_skb` 来完成的。

注意，先前通过调用 `ip_route_output_flow` 生成的路由条目也会一起传进来，
它将保存到 skb 里，稍后在 IP 协议层中被使用。

```c
/* Lockless fast path for the non-corking case. */
if (!corkreq) {
        skb = ip_make_skb(sk, fl4, getfrag, msg->msg_iov, ulen,
                          sizeof(struct udphdr), &ipc, &rt,
                          msg->msg_flags);
        err = PTR_ERR(skb);
        if (!IS_ERR_OR_NULL(skb))
                err = udp_send_skb(skb, fl4);
        goto out;
}
```

`ip_make_skb` 函数将创建一个 skb，其中需要考虑到很多的事情，例如：

1. [MTU](https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data/Maximum_transmission_unit)
1. UDP corking（如果启用）
1. UDP Fragmentation Offloading（[UFO](https://wiki.linuxfoundation.org/networking/ufo)）
1. Fragmentation（分片）：如果硬件不支持 UFO，但是要传输的数据大于 MTU，需要软件做分片

大多数网络设备驱动程序不支持 UFO，因为网络硬件本身不支持此功能。我们来看下这段代码，先看 corking 禁用的情况，启用的情况我们更后面再看。

#### `ip_make_skb`

定义在[net/ipv4/ip_output.c]()，这个函数有点复杂。

构建 skb 的时候，`ip_make_skb` 依赖的底层代码需要使用一个 corking 变量和一个 queue 变量
，skb 将通过 queue 变量传入。如果 socket 未被 cork，则会传入一个假的 corking 变量和一个
空队列。

我们来看看假 corking 变量和空队列是如何初始化的：

```c
struct sk_buff *ip_make_skb(struct sock *sk, /* more args */)
{
        struct inet_cork cork;
        struct sk_buff_head queue;
        int err;

        if (flags & MSG_PROBE)
                return NULL;

        __skb_queue_head_init(&queue);

        cork.flags = 0;
        cork.addr = 0;
        cork.opt = NULL;
        err = ip_setup_cork(sk, &cork, /* more args */);
        if (err)
                return ERR_PTR(err);
```

如上所示，cork 和 queue 都是在栈上分配的，`ip_make_skb` 根本不需要它。
`ip_setup_cork` 初始化 cork 变量。接下来，调用`__ip_append_data` 并传入 cork 和 queue 变
量：

```c
err = __ip_append_data(sk, fl4, &queue, &cork,
                       &current->task_frag, getfrag,
                       from, length, transhdrlen, flags);
```

我们将在后面看到这个函数是如何工作的，因为不管 socket 是否被 cork，最后都会执行它。

现在，我们只需要知道`__ip_append_data` 将创建一个 skb，向其追加数据，并将该 skb 添加
到传入的 queue 变量中。如果追加数据失败，则调用`__ip_flush_pending_frame` 丢弃数据
并向上返回错误（指针类型）：

```c
if (err) {
        __ip_flush_pending_frames(sk, &queue, &cork);
        return ERR_PTR(err);
}
```

最后，如果没有发生错误，`__ip_make_skb` 将 skb 出队，添加 IP 选项，并返回一个准备好传
递给更底层发送的 skb：

```c
return __ip_make_skb(sk, fl4, &queue, &cork);
```

#### 发送数据

如果没有错误，skb 就会交给 `udp_send_skb`，后者会继续将其传给下一层协议，IP 协议：

```c
err = PTR_ERR(skb);
if (!IS_ERR_OR_NULL(skb))
        err = udp_send_skb(skb, fl4);
goto out;
```

如果有错误，错误计数就会有相应增加。后面的“错误计数”部分会详细介绍。

### 5.1.10 没有被 cork 的数据时的慢路径

如果使用了 UDP corking，但之前没有数据被 cork，则慢路径开始：

1. 对 socket 加锁
1. 检查应用程序是否有 bug：已经被 cork 的 socket 是否再次被 cork
1. 设置该 UDP flow 的一些参数，为 corking 做准备
1. 将要发送的数据追加到现有数据

`udp_sendmsg` 代码继续向下看，就是这一逻辑：

```c
  lock_sock(sk);
  if (unlikely(up->pending)) {
          /* The socket is already corked while preparing it. */
          /* ... which is an evident application bug. --ANK */
          release_sock(sk);

          LIMIT_NETDEBUG(KERN_DEBUG pr_fmt("cork app bug 2\n"));
          err = -EINVAL;
          goto out;
  }
  /*
   *      Now cork the socket to pend data.
   */
  fl4 = &inet->cork.fl.u.ip4;
  fl4->daddr = daddr;
  fl4->saddr = saddr;
  fl4->fl4_dport = dport;
  fl4->fl4_sport = inet->inet_sport;
  up->pending = AF_INET;

do_append_data:
  up->len += ulen;
  err = ip_append_data(sk, fl4, getfrag, msg->msg_iov, ulen,
                       sizeof(struct udphdr), &ipc, &rt,
                       corkreq ? msg->msg_flags|MSG_MORE : msg->msg_flags);
```

#### `ip_append_data`

这个函数简单封装了`__ip_append_data`，在调用后者之前，做了两件重要的事情：

1. 检查是否从用户传入了 `MSG_PROBE` 标志。该标志表示用户不想真正发送数据，只是做路
   径探测（例如，确定[PMTU](https://en.wikipedia.org/wiki/Path_MTU_Discovery)）
1. 检查 socket 的发送队列是否为空。如果为空，意味着没有 cork 数据等待处理，因此调用
   `ip_setup_cork` 来设置 corking

一旦处理了上述条件，就调用`__ip_append_data` 函数，该函数包含用于将数据处理成数据
包的大量逻辑。

#### `__ip_append_data`

如果 socket 是 corked，则从 `ip_append_data` 调用此函数；如果 socket 未被 cork，则从
`ip_make_skb` 调用此函数。在任何一种情况下，函数都将分配一个新缓冲区来存储传入
的数据，或者将数据附加到现有数据中。

这种工作的方式围绕 socket 的发送队列。等待发送的现有数据（例如，如果 socket 被 cork）
将在队列中有一个对应条目，可以被追加数据。

这个函数很复杂;它执行很多计算以确定如何构造传递给下面的网络层的 skb。

该函数的重点包括：

1. 如果硬件支持，则处理 UDP Fragmentation Offload（UFO）。绝大多数网络硬件不支持
   UFO。如果你的网卡驱动程序支持它，它将设置 `NETIF_F_UFO` 标记位
1. 处理支持分散/收集（
   [scatter/gather](https://en.wikipedia.org/wiki/Vectored_I/O)）IO 的网卡。许多
   卡都支持此功能，并使用 `NETIF_F_SG` 标志进行通告。支持该特性的网卡可以处理数据
   被分散到多个 buffer 的数据包;内核不需要花时间将多个缓冲区合并成一个缓冲区中。避
   免这种额外的复制会提升性能，大多数网卡都支持此功能
1. 通过调用 `sock_wmalloc` 跟踪发送队列的大小。当分配新的 skb 时，skb 的大小由创建它
   的 socket 计费（charge），并计入 socket 发送队列的已分配字节数。如果发送队列已经
   没有足够的空间（超过计费限制），则 skb 并分配失败并返回错误。我们将在下面的调优
   部分中看到如何设置 socket 发送队列大小（txqueuelen）
1. 更新错误统计信息。此函数中的任何错误都会增加“discard”计数。我们将在下面的监控部分中
   看到如何读取此值

函数执行成功后返回 0，以及一个适用于网络设备传输的 skb。

在 unorked 情况下，持有 skb 的 queue 被作为参数传递给上面描述的`__ip_make_skb`，在那里
它被出队并通过 `udp_send_skb` 发送到更底层。

在 cork 的情况下，`__ip_append_data` 的返回值向上传递。数据位于发送队列中，直到
`udp_sendmsg` 确定是时候调用 `udp_push_pending_frames` 来完成 skb，后者会进一步调用
`udp_send_skb`。

#### Flushing corked sockets

现在，`udp_sendmsg` 会继续，检查`__ip_append_skb` 的返回值（错误码）：

```c
if (err)
        udp_flush_pending_frames(sk);
else if (!corkreq)
        err = udp_push_pending_frames(sk);
else if (unlikely(skb_queue_empty(&sk->sk_write_queue)))
        up->pending = 0;
release_sock(sk);
```

我们来看看每个情况：

1. 如果出现错误（错误为非零），则调用 `udp_flush_pending_frames`，这将取消 cork 并从 socket 的发送队列中删除所有数据
1. 如果在未指定 `MSG_MORE` 的情况下发送此数据，则调用 `udp_push_pending_frames`，它将数据传递到更下面的网络层
1. 如果发送队列为空，请将 socket 标记为不再 cork

如果追加操作完成并且有更多数据要进入 cork，则代码将做一些清理工作，并返回追加数据的长度：

```c
ip_rt_put(rt);
if (free)
        kfree(ipc.opt);
if (!err)
        return len;
```

这就是内核如何处理 corked UDP sockets 的。

### 5.1.11 Error accounting

如果：

1. non-corking 快速路径创建 skb 失败，或 `udp_send_skb` 返回错误，或
1. `ip_append_data` 无法将数据附加到 corked UDP socket，或
1. 当 `udp_push_pending_frames` 调用 `udp_send_skb` 发送 corked skb 时后者返回错误

仅当返回的错误是 `ENOBUFS`（内核无可用内存）或 socket 已设置 `SOCK_NOSPACE`（发送队
列已满）时，`SNDBUFERRORS` 统计信息才会增加：

```c
/*
 * ENOBUFS = no kernel mem, SOCK_NOSPACE = no sndbuf space.  Reporting
 * ENOBUFS might not be good (it's not tunable per se), but otherwise
 * we don't have a good statistic (IpOutDiscards but it can be too many
 * things).  We could add another new stat but at least for now that
 * seems like overkill.
 */
if (err == -ENOBUFS || test_bit(SOCK_NOSPACE, &sk->sk_socket->flags)) {
        UDP_INC_STATS_USER(sock_net(sk),
                        UDP_MIB_SNDBUFERRORS, is_udplite);
}
return err;
```

我们接下来会在监控小节里看到如何读取这些计数。

<a name="chap_5.2"></a>

## 5.2 `udp_send_skb`

`udp_sendmsg` 通过调用 `udp_send_skb` 函数将 skb 送到下一网络层，在本例中是 IP 协议层。 这个函数做了一些重要的事情：

1. 向 skb 添加 UDP 头
1. 处理校验和：软件校验和，硬件校验和或无校验和（如果禁用）
1. 调用 `ip_send_skb` 将 skb 发送到 IP 协议层
1. 更新发送成功或失败的统计计数器

让我们来看看。首先，创建 UDP 头：

```c
static int udp_send_skb(struct sk_buff *skb, struct flowi4 *fl4)
{
                /* useful variables ... */

        /*
         * Create a UDP header
         */
        uh = udp_hdr(skb);
        uh->source = inet->inet_sport;
        uh->dest = fl4->fl4_dport;
        uh->len = htons(len);
        uh->check = 0;
```

接下来，处理校验和。有几种情况：

1. 首先处理[UDP-Lite](https://en.wikipedia.org/wiki/UDP-Lite)校验和
1. 接下来，如果 socket 校验和选项被关闭（`setsockopt` 带 `SO_NO_CHECK` 参数），它将被标记为校
   验和关闭
1. 接下来，如果硬件支持 UDP 校验和，则将调用 `udp4_hwcsum` 来设置它。请注意，如果数
   据包是分段的，内核将在软件中生成校验和，你可以在
   [udp4_hwcsum](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/udp.c#L720-L763)
   的源代码中看到这一点
1. 最后，通过调用 `udp_csum` 生成软件校验和

```c
if (is_udplite)                                  /*     UDP-Lite      */
        csum = udplite_csum(skb);

else if (sk->sk_no_check == UDP_CSUM_NOXMIT) {   /* UDP csum disabled */

        skb->ip_summed = CHECKSUM_NONE;
        goto send;

} else if (skb->ip_summed == CHECKSUM_PARTIAL) { /* UDP hardware csum */

        udp4_hwcsum(skb, fl4->saddr, fl4->daddr);
        goto send;

} else
        csum = udp_csum(skb);
```

接下来，添加了[伪头
](https://en.wikipedia.org/wiki/User_Datagram_Protocol#IPv4_Pseudo_Header)：

```c
uh->check = csum_tcpudp_magic(fl4->saddr, fl4->daddr, len,
                              sk->sk_protocol, csum);
if (uh->check == 0)
        uh->check = CSUM_MANGLED_0;
```

如果校验和为 0，则根据[RFC 768](https://tools.ietf.org/html/rfc768)，校验为全 1（
transmitted  as all ones (the equivalent  in one's complement  arithmetic)）。最
后，将 skb 传递给 IP 协议层并增加统计计数：

```c
send:
  err = ip_send_skb(sock_net(sk), skb);
  if (err) {
          if (err == -ENOBUFS && !inet->recverr) {
                  UDP_INC_STATS_USER(sock_net(sk),
                                     UDP_MIB_SNDBUFERRORS, is_udplite);
                  err = 0;
          }
  } else
          UDP_INC_STATS_USER(sock_net(sk),
                             UDP_MIB_OUTDATAGRAMS, is_udplite);
  return err;
```

如果 `ip_send_skb` 成功，将更新 `OUTDATAGRAMS` 统计。如果 IP 协议层报告错误，并且错误
是 `ENOBUFS`（内核缺少内存）而且错误 queue（`inet->recverr`）没有启用，则更新
`SNDBUFERRORS`。

在继续讨论 IP 协议层之前，让我们先看看如何在 Linux 内核中监视和调优 UDP 协议层。

<a name="chap_5.3"></a>

## 5.3 监控：UDP 层统计

两个非常有用的获取 UDP 协议统计文件：

* `/proc/net/snmp`
* `/proc/net/udp`

### /proc/net/snmp

监控 UDP 协议层统计：

```shell
$ cat /proc/net/snmp | grep Udp\:
Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors
Udp: 16314 0 0 17161 0 0
```

要准确地理解这些计数，你需要仔细地阅读内核代码。一些类型的错误计数并不是只出现在
一种计数中，而可能是出现在多个计数中。

* `InDatagrams`: Incremented when recvmsg was used by a userland program to read datagram. Also incremented when a UDP packet is encapsulated and sent back for processing.
* `NoPorts`: Incremented when UDP packets arrive destined for a port where no program is listening.
* `InErrors`: Incremented in several cases: no memory in the receive queue, when a bad checksum is seen, and if sk_add_backlog fails to add the datagram.
* `OutDatagrams`: Incremented when a UDP packet is handed down without error to the IP protocol layer to be sent.
* `RcvbufErrors`: Incremented when sock_queue_rcv_skb reports that no memory is available; this happens if sk->sk_rmem_alloc is greater than or equal to sk->sk_rcvbuf.
* `SndbufErrors`: Incremented if the IP protocol layer reported an error when trying to send the packet and no error queue has been setup. Also incremented if no send queue space or kernel memory are available.
* `InCsumErrors`: Incremented when a UDP checksum failure is detected. Note that in all cases I could find, InCsumErrors is incremented at the same time as InErrors. Thus, InErrors - InCsumErros should yield the count of memory related errors on the receive side.

注意，UDP 协议层发现的某些错误会出现在其他协议层的统计信息中。一个例子：路由错误
。 `udp_sendmsg` 发现的路由错误将导致 IP 协议层的 `OutNoRoutes` 统计增加。

### /proc/net/udp

监控 UDP socket 统计：

```shell
$ cat /proc/net/udp
  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops
  515: 00000000:B346 00000000:0000 07 00000000:00000000 00:00000000 00000000   104        0 7518 2 0000000000000000 0
  558: 00000000:0371 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 7408 2 0000000000000000 0
  588: 0100007F:038F 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 7511 2 0000000000000000 0
  769: 00000000:0044 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 7673 2 0000000000000000 0
  812: 00000000:006F 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 7407 2 0000000000000000 0
```

每一列的意思：

* `sl`: Kernel hash slot for the socket
* `local_address`: Hexadecimal local address of the socket and port number, separated by :.
* `rem_address`: Hexadecimal remote address of the socket and port number, separated by :.
* `st`: The state of the socket. Oddly enough, the UDP protocol layer seems to use some TCP socket states. In the example above, 7 is TCP_CLOSE.
* `tx_queue`: The amount of memory allocated in the kernel for outgoing UDP datagrams.
* `rx_queue`: The amount of memory allocated in the kernel for incoming UDP datagrams.
* `tr`, tm->when, retrnsmt: These fields are unused by the UDP protocol layer.
* `uid`: The effective user id of the user who created this socket.
* `timeout`: Unused by the UDP protocol layer.
* `inode`: The inode number corresponding to this socket. You can use this to help you determine which user process has this socket open. Check /proc/[pid]/fd, which will contain symlinks to socket[:inode].
* `ref`: The current reference count for the socket.
* `pointer`: The memory address in the kernel of the struct sock.
* `drops`: The number of datagram drops associated with this socket. Note that this does not include any drops related to sending datagrams (on corked UDP sockets or otherwise); this is only incremented in receive paths as of the kernel version examined by this blog post.

打印这些计数的代码在[net/ipv4/udp.c](https://github.com/torvalds/linux/blob/master/net/ipv4/udp.c#L2396-L2431)。

<a name="chap_5.4"></a>

## 5.4 调优：socket 发送队列内存大小

发送队列（也叫“写队列”）的最大值可以通过设置 `net.core.wmem_max sysctl` 进行修改。

```shell
$ sudo sysctl -w net.core.wmem_max=8388608
```

`sk->sk_write_queue` 用 `net.core.wmem_default` 初始化， 这个值也可以调整。

调整初始发送 buffer 大小：

```shell
$ sudo sysctl -w net.core.wmem_default=8388608
```

也可以通过从应用程序调用 `setsockopt` 并传递 `SO_SNDBUF` 来设置 `sk->sk_write_queue`
。通过 `setsockopt` 设置的最大值是 `net.core.wmem_max`。

不过，可以通过 `setsockopt` 并传递 `SO_SNDBUFFORCE` 来覆盖 `net.core.wmem_max` 限制，
这需要 `CAP_NET_ADMIN` 权限。

每次调用`__ip_append_data` 分配 skb 时，`sk->sk_wmem_alloc` 都会递增。正如我们所看到
的，UDP 数据报传输速度很快，通常不会在发送队列中花费太多时间。

<a name="chap_6"></a>

# 6 IP 协议层

UDP 协议层通过调用 `ip_send_skb` 将 skb 交给 IP 协议层，所以我们从这里开始，探索一下 IP
协议层。

<a name="chap_6.1"></a>

## 6.1 `ip_send_skb`

`ip_send_skb` 函数定义在
[net/ipv4/ip_output.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/ip_output.c#L1367-L1380)
中，非常简短。它只是调用 `ip_local_out`，如果调用失败，就更新相应的错误计数。让
我们来看看：

```c
int ip_send_skb(struct net *net, struct sk_buff *skb)
{
        int err;

        err = ip_local_out(skb);
        if (err) {
                if (err > 0)
                        err = net_xmit_errno(err);
                if (err)
                        IP_INC_STATS(net, IPSTATS_MIB_OUTDISCARDS);
        }

        return err;
}
```

`net_xmit_errno` 函数将低层错误转换为 IP 和 UDP 协议层所能理解的错误。如果发生错误，
IP 协议计数器 `OutDiscards` 会递增。稍后我们将看到读取哪些文件可以获取此统计信
息。现在，让我们继续，看看 `ip_local_out` 带我们去哪。

<a name="chap_6.2"></a>

## 6.2 `ip_local_out` and `__ip_local_out`

幸运的是，`ip_local_out` 和`__ip_local_out` 都很简单。`ip_local_out` 只需调用
`__ip_local_out`，如果返回值为 1，则调用路由层 `dst_output` 发送数据包：

```c
int ip_local_out(struct sk_buff *skb)
{
        int err;

        err = __ip_local_out(skb);
        if (likely(err == 1))
                err = dst_output(skb);

        return err;
}
```

我们来看看`__ip_local_out` 的代码：

```c
int __ip_local_out(struct sk_buff *skb)
{
        struct iphdr *iph = ip_hdr(skb);

        iph->tot_len = htons(skb->len);
        ip_send_check(iph);
        return nf_hook(NFPROTO_IPV4, NF_INET_LOCAL_OUT, skb, NULL,
                       skb_dst(skb)->dev, dst_output);
}
```

可以看到，该函数首先做了两件重要的事情：

1. 设置 IP 数据包的长度
1. 调用 `ip_send_check` 来计算要写入 IP 头的校验和。`ip_send_check` 函数将进一步调用
   名为 `ip_fast_csum` 的函数来计算校验和。在 x86 和 x86_64 体系结构上，此函数用汇编实
   现，代码：[64 位实现](https://github.com/torvalds/linux/blob/v3.13/arch/x86/include/asm/checksum_64.h#L40-L73)
   和[32 位实现](https://github.com/torvalds/linux/blob/v3.13/arch/x86/include/asm/checksum_32.h#L63-L98)
   。

接下来，IP 协议层将通过调用 `nf_hook` 进入 netfilter，其返回值将传递回 `ip_local_out`
。 如果 `nf_hook` 返回 1，则表示允许数据包通过，并且调用者应该自己发送数据包。这正
是我们在上面看到的情况：`ip_local_out` 检查返回值 1 时，自己通过调用 `dst_output` 发
送数据包。

<a name="chap_6.3"></a>

## 6.3 netfilter and nf_hook

简洁起见，我决定跳过对 netfilter，iptables 和 conntrack 的深入研究。如果你想深入了解
netfilter 的代码实现，可以从 `include/linux/netfilter.h`[这里
](https://github.com/torvalds/linux/blob/v3.13/include/linux/netfilter.h#L142-L147)
和 `net/netfilter/core.c`[这里
](https://github.com/torvalds/linux/blob/v3.13/net/netfilter/core.c#L168-L209)开
始。

简短版本是：`nf_hook` 只是一个 wrapper，它调用 `nf_hook_thresh`，首先检查是否有为这
个**协议族**和**hook 类型**（这里分别为 `NFPROTO_IPV4` 和 `NF_INET_LOCAL_OUT`）安装
的过滤器，然后将返回到 IP 协议层，避免深入到 netfilter 或更下面，比如 iptables 和
conntrack。

请记住：如果你有非常多或者非常复杂的 netfilter 或 iptables 规则，那些规则将在触发
`sendmsg` 系统调的用户进程的上下文中执行。如果对这个用户进程设置了 CPU 亲和性，相应
的 CPU 将花费系统时间（system time）处理出站（outbound）iptables 规则。如果你在做性
能回归测试，那可能要考虑根据系统的负载，将相应的用户进程绑到到特定的 CPU，或者是
减少 netfilter/iptables 规则的复杂度，以减少对性能测试的影响。

出于讨论目的，我们假设 `nf_hook` 返回 1，表示调用者（在这种情况下是 IP 协议层）应该
自己发送数据包。

<a name="chap_6.4"></a>

## 6.4 目的（路由）缓存

dst 代码在 Linux 内核中实现**协议无关**的目标缓存。为了继续学习发送 UDP 数据报的流程
，我们需要了解 dst 条目是如何被设置的，首先来看 dst 条目和路由是如何生成的。 目标缓
存，路由和邻居子系统，任何一个都可以拿来单独详细的介绍。我们不深入细节，只是快速
地看一下它们是如何组合到一起的。

我们上面看到的代码调用了 `dst_output(skb)`。 此函数只是查找关联到这个 skb 的 dst 条目
，然后调用 `output` 方法。代码如下：

```c
/* Output packet to network from transport.  */
static inline int dst_output(struct sk_buff *skb)
{
        return skb_dst(skb)->output(skb);
}
```

看起来很简单，但是 `output` 方法之前是如何关联到 dst 条目的？

首先很重要的一点，目标缓存条目是以多种不同方式添加的。到目前为止，我们已经在代码
中看到的一种方法是从 `udp_sendmsg` 调用
[ip_route_output_flow](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/route.c#L2252-L2267)
。`ip_route_output_flow` 函数调用
[__ip_route_output_key](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/route.c#L1990-L2173)
，后者进而调用
[__mkroute_output](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/route.c#L1868-L1988)
。 `__mkroute_output` 函数创建路由和目标缓存条目。当它执行创建操作时，它会判断哪
个 `output` 方法适合此 dst。大多数时候，这个函数是 `ip_output`。

<a name="chap_6.5"></a>

## 6.5 `ip_output`

在 UDP IPv4 情况下，上面的 `output` 方法指向的是 `ip_output`。`ip_output` 函数很简单：

```c
int ip_output(struct sk_buff *skb)
{
        struct net_device *dev = skb_dst(skb)->dev;

        IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUT, skb->len);

        skb->dev = dev;
        skb->protocol = htons(ETH_P_IP);

        return NF_HOOK_COND(NFPROTO_IPV4, NF_INET_POST_ROUTING, skb, NULL, dev,
                            ip_finish_output,
                            !(IPCB(skb)->flags & IPSKB_REROUTED));
}
```

首先，更新 `IPSTATS_MIB_OUT` 统计计数。`IP_UPD_PO_STATS` 宏将更新字节数和包数统计。
我们将在后面的部分中看到如何获取 IP 协议层统计信息以及它们各自的含义。接下来，设置
要发送此 skb 的设备，以及协议。

最后，通过调用 `NF_HOOK_COND` 将控制权交给 netfilter。查看 `NF_HOOK_COND` 的函数原型
有助于更清晰地解释它如何工作。来自
[include/linux/netfilter.h](https://github.com/torvalds/linux/blob/v3.13/include/linux/netfilter.h#L177-L188)
：

```c
static inline int
NF_HOOK_COND(uint8_t pf, unsigned int hook, struct sk_buff *skb,
             struct net_device *in, struct net_device *out,
             int (*okfn)(struct sk_buff *), bool cond)
```

`NF_HOOK_COND` 通过检查传入的条件来工作。在这里条件是`!(IPCB(skb)->flags &
IPSKB_REROUTED`。如果此条件为真，则 skb 将发送给 netfilter。如果 netfilter 允许包通过
，`okfn` 回调函数将被调用。在这里，`okfn` 是 `ip_finish_output`。

<a name="chap_6.6"></a>

## 6.6 `ip_finish_output`

```c
static int ip_finish_output(struct sk_buff *skb)
{
#if defined(CONFIG_NETFILTER) && defined(CONFIG_XFRM)
        /* Policy lookup after SNAT yielded a new policy */
        if (skb_dst(skb)->xfrm != NULL) {
                IPCB(skb)->flags |= IPSKB_REROUTED;
                return dst_output(skb);
        }
#endif
        if (skb->len > ip_skb_dst_mtu(skb) && !skb_is_gso(skb))
                return ip_fragment(skb, ip_finish_output2);
        else
                return ip_finish_output2(skb);
}
```

如果内核启用了 netfilter 和数据包转换（XFRM），则更新 skb 的标志并通过 `dst_output` 将
其发回。

更常见的两种情况是：

1. 如果数据包的长度大于 MTU 并且分片不会 offload 到设备，则会调用 `ip_fragment` 在发送之前对数据包进行分片
1. 否则，数据包将直接发送到 ip_finish_output2

在继续我们的内核之前，让我们简单地谈谈 Path MTU Discovery。

### Path MTU Discovery

Linux 提供了一个功能，我迄今为止一直避免提及：[路径 MTU 发现
](https://en.wikipedia.org/wiki/Path_MTU_Discovery)。此功能允许内核自动确定
路由的最大传输单元（
[MTU](https://en.wikipedia.org/wiki/Maximum_transmission_unit)
）。发送小于或等于该路由的 MTU 的包意味着可以避免 IP 分片，这是推荐设置，因为数
据包分片会消耗系统资源，而避免分片看起来很容易：只需发送足够小的不需要分片的数据
包。

你可以在应用程序中通过调用 `setsockopt` 带 `SOL_IP` 和 `IP_MTU_DISCOVER` 选项，在
packet 级别来调整路径 MTU 发现设置，相应的合法值参考 IP 协议的[man
page](http://man7.org/linux/man-pages/man7/ip.7.html)。例如，你可能想设置的值是
：`IP_PMTUDISC_DO`，表示“始终执行路径 MTU 发现”。更高级的网络应用程序或诊断工具可
能选择自己实现[RFC 4821](https://www.ietf.org/rfc/rfc4821.txt)，以在应用程序启动
时针对特定的路由做 PMTU。在这种情况下，你可以使用 `IP_PMTUDISC_PROBE` 选项告诉内核
设置“Do not Fragment”位，这就会允许你发送大于 PMTU 的数据。

应用程序可以通过调用 `getsockopt` 带 `SOL_IP` 和 `IP_MTU` 选项来查看当前 PMTU。可以使
用它指导应用程序在发送之前，构造 UDP 数据报的大小。

如果已启用 PMTU 发现，则发送大于 PMTU 的 UDP 数据将导致应用程序收到 `EMSGSIZE` 错误。
这种情况下，应用程序只能减小 packet 大小重试。

强烈建议启用 PTMU 发现，因此我将不再详细描述 IP 分片的代码。当我们查看 IP 协议层统计信
息时，我将解释所有统计信息，包括与分片相关的统计信息。其中许多计数都在
`ip_fragment` 中更新的。不管分片与否，代码最后都会调到 `ip_finish_output2`，所以让
我们继续。

<a name="chap_6.7"></a>

## 6.7 `ip_finish_output2`

IP 分片后调用 `ip_finish_output2`，另外 `ip_finish_output` 也会直接调用它。这个函数
在将包发送到邻居缓存之前处理各种统计计数器。让我们看看它是如何工作的：

```c
static inline int ip_finish_output2(struct sk_buff *skb)
{

                /* variable declarations */

        if (rt->rt_type == RTN_MULTICAST) {
                IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTMCAST, skb->len);
        } else if (rt->rt_type == RTN_BROADCAST)
                IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTBCAST, skb->len);

        /* Be paranoid, rather than too clever. */
        if (unlikely(skb_headroom(skb) < hh_len && dev->header_ops)) {
                struct sk_buff *skb2;

                skb2 = skb_realloc_headroom(skb, LL_RESERVED_SPACE(dev));
                if (skb2 == NULL) {
                        kfree_skb(skb);
                        return -ENOMEM;
                }
                if (skb->sk)
                        skb_set_owner_w(skb2, skb->sk);
                consume_skb(skb);
                skb = skb2;
        }
```

如果与此数据包关联的路由是多播类型，则使用 `IP_UPD_PO_STATS` 宏来增加
`OutMcastPkts` 和 `OutMcastOctets` 计数。如果广播路由，则会增加 `OutBcastPkts` 和
`OutBcastOctets` 计数。

接下来，确保 skb 结构有足够的空间容纳需要添加的任何链路层头。如果空间不够，则调用
`skb_realloc_headroom` 分配额外的空间，并且新的 skb 的费用（charge）记在相关的
socket 上。

```c
        rcu_read_lock_bh();
        nexthop = (__force u32) rt_nexthop(rt, ip_hdr(skb)->daddr);
        neigh = __ipv4_neigh_lookup_noref(dev, nexthop);
        if (unlikely(!neigh))
                neigh = __neigh_create(&arp_tbl, &nexthop, dev, false);
```

继续，查询路由层找到下一跳，再根据下一跳信息查找邻居缓存。如果未找到，则
调用`__neigh_create` 创建一个邻居。例如，第一次将数据发送到另一
台主机的时候，就是这种情况。请注意，创建邻居缓存的时候带了 `arp_tbl`（
[net/ipv4/arp.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/arp.c#L160-L187)
中定义）参数。其他系统（如 IPv6 或
[DECnet](https://en.wikipedia.org/wiki/DECnet)）维护自己的 ARP 表，并将不同的变量
传给`__neigh_create`。 这篇文章的目的并不是要详细介绍邻居缓存，但注意如果创建，
会导致缓存表增大。本文后面会介绍有关邻居缓存的更多详细信息。 邻居缓存会导出一组
统计信息，以便可以衡量这种增长。有关详细信息，请参阅下面的监控部分。

```c
        if (!IS_ERR(neigh)) {
                int res = dst_neigh_output(dst, neigh, skb);

                rcu_read_unlock_bh();
                return res;
        }
        rcu_read_unlock_bh();

        net_dbg_ratelimited("%s: No header cache and no neighbour!\n",
                            __func__);
        kfree_skb(skb);
        return -EINVAL;
}
```

最后，如果创建邻居缓存成功，则调用 `dst_neigh_output` 继续传递 skb；否则，释放 skb 并返
回 `EINVAL`，这会向上传递，导致 `OutDiscards` 在 `ip_send_skb` 中递增。让我们继续在
`dst_neigh_output` 中接近 Linux 内核的 netdevice 子系统。

<a name="chap_6.8"></a>

## 6.8 `dst_neigh_output`

`dst_neigh_output` 函数做了两件重要的事情。首先，回想一下之前在本文中我
们看到，如果用户调用 `sendmsg` 并通过辅助消息指定 `MSG_CONFIRM` 参数，则会设置一个标
志位以指示目标高速缓存条目仍然有效且不应进行垃圾回收。这个检查就是在这个函数里面
做的，并且邻居上的 `confirm` 字段设置为当前的 jiffies 计数。

```c
static inline int dst_neigh_output(struct dst_entry *dst, struct neighbour *n,
                                   struct sk_buff *skb)
{
        const struct hh_cache *hh;

        if (dst->pending_confirm) {
                unsigned long now = jiffies;

                dst->pending_confirm = 0;
                /* avoid dirtying neighbour */
                if (n->confirmed != now)
                        n->confirmed = now;
        }
```

其次，检查邻居的状态并调用适当的 `output` 函数。让我们看一下这些条件，并尝试了解发
生了什么：

```c
        hh = &n->hh;
        if ((n->nud_state & NUD_CONNECTED) && hh->hh_len)
                return neigh_hh_output(hh, skb);
        else
                return n->output(n, skb);
}
```

邻居被认为是 `NUD_CONNECTED`，如果它满足以下一个或多个条件：

1. `NUD_PERMANENT`：静态路由
1. `NUD_NOARP`：不需要 ARP 请求（例如，目标是多播或广播地址，或环回设备）
1. `NUD_REACHABLE`：邻居是“可达的。”只要[成功处理了](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/arp.c#L905-L923)ARP 请求，目标就会被标记为可达

进一步，如果“硬件头”（hh）被缓存（之前已经发送过数据，并生成了缓存），将调
用 `neigh_hh_output`。

否则，调用 `output` 函数。

以上两种情况，最后都会到 `dev_queue_xmit`，它将 skb 发送给 Linux 网络设备子系统，在它
进入设备驱动程序层之前将对其进行更多处理。让我们沿着 `neigh_hh_output` 和
`n->output` 代码继续向下，直到达到 `dev_queue_xmit`。

<a name="chap_6.9"></a>

## 6.9 `neigh_hh_output`

如果目标是 `NUD_CONNECTED` 并且硬件头已被缓存，则将调用 `neigh_hh_output`，在将 skb 移交
给 `dev_queue_xmit` 之前执行一小部分处理。 我们来看看
[include/net/neighbour.h](https://github.com/torvalds/linux/blob/v3.13/include/net/neighbour.h#L336-L356)
：

```c
static inline int neigh_hh_output(const struct hh_cache *hh, struct sk_buff *skb)
{
        unsigned int seq;
        int hh_len;

        do {
                seq = read_seqbegin(&hh->hh_lock);
                hh_len = hh->hh_len;
                if (likely(hh_len <= HH_DATA_MOD)) {
                        /* this is inlined by gcc */
                        memcpy(skb->data - HH_DATA_MOD, hh->hh_data, HH_DATA_MOD);
                 } else {
                         int hh_alen = HH_DATA_ALIGN(hh_len);

                         memcpy(skb->data - hh_alen, hh->hh_data, hh_alen);
                 }
         } while (read_seqretry(&hh->hh_lock, seq));

         skb_push(skb, hh_len);
         return dev_queue_xmit(skb);
}
```

这个函数理解有点难，部分原因是[seqlock](https://en.wikipedia.org/wiki/Seqlock)这
个东西，它用于在缓存的硬件头上做读/写锁。可以将上面的 `do {} while ()`循环想象成
一个简单的重试机制，它将尝试在循环中执行，直到成功。

循环里处理硬件头的长度对齐。这是必需的，因为某些硬件头（如[IEEE
802.11](https://github.com/torvalds/linux/blob/v3.13/include/linux/ieee80211.h#L210-L218)
头）大于 `HH_DATA_MOD`（16 字节）。

将头数据复制到 skb 后，`skb_push` 将更新 skb 内指向数据缓冲区的指针。最后调用
`dev_queue_xmit` 将 skb 传递给 Linux 网络设备子系统。

<a name="chap_6.10"></a>

## 6.10 `n->output`

如果目标不是 `NUD_CONNECTED` 或硬件头尚未缓存，则代码沿 `n->output` 路径向下。
neigbour 结构上的 `output` 指针指向哪个函数？这得看情况。要了解这是如何设置的，我们
需要更多地了解邻居缓存的工作原理。

`struct
neighbour` 包含几个重要字段：我们在上面看到的 `nud_state` 字段，`output` 函数和 `ops`
结构。回想一下，我们之前看到如果在缓存中找不到现有条目，会从 `ip_finish_output2`
调用`__neigh_create` 创建一个。当调用`__neigh_creaet` 时，将分配邻居，其 `output` 函
数[最初](https://github.com/torvalds/linux/blob/v3.13/net/core/neighbour.c#L294)
设置为 `neigh_blackhole`。随着`__neigh_create` 代码的进行，它将根据邻居的状态修改
`output` 值以指向适当的发送方法。

例如，当代码确定是“已连接的”邻居时，`neigh_connect` 会将 `output` 设置为
`neigh->ops->connected_output`。或者，当代码怀疑邻居可能已关闭时，`neigh_suspect`
会将 `output` 设置为 `neigh->ops->output`（例如，如果已超过
`/proc/sys/net/ipv4/neigh/default/delay_first_probe_time` 自发送探测以来的
`delay_first_probe_time` 秒）。

换句话说：`neigh->output` 会被设置为 `neigh->ops_connected_output` 或
`neigh->ops->output`，具体取决于邻居的状态。`neigh->ops` 来自哪里？

分配邻居后，调用 `arp_constructor`（
[net/ipv4/arp.c](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/arp.c#L220-L313)
）来设置 `struct neighbor` 的某些字段。特别是，此函数会检查与此邻居关联的设备是否
导出来一个 `struct header_ops` 实例（[以太网设备是这样做的
](https://github.com/torvalds/linux/blob/v3.13/net/ethernet/eth.c#L342-L348)），
该结构体有一个 `cache` 方法。

`neigh->ops` 设置为
[net/ipv4/arp](https://github.com/torvalds/linux/blob/v3.13/net/ipv4/arp.c#L138-L144)
中定义的以下实例：

```c
static const struct neigh_ops arp_hh_ops = {
        .family =               AF_INET,
        .solicit =              arp_solicit,
        .error_report =         arp_error_report,
        .output =               neigh_resolve_output,
        .connected_output =     neigh_resolve_output,
};
```

所以，不管 neighbor 是不是“已连接的”，或者邻居缓存代码是否怀疑连接“已关闭”，
`neigh_resolve_output` 最终都会被赋给 `neigh->output`。当执行到 `n->output` 时就会调
用它。

### neigh_resolve_output

此函数的目的是解析未连接的邻居，或已连接但没有缓存硬件头的邻居。我们来看看这个
函数是如何工作的：

```c
/* Slow and careful. */

int neigh_resolve_output(struct neighbour *neigh, struct sk_buff *skb)
{
        struct dst_entry *dst = skb_dst(skb);
        int rc = 0;

        if (!dst)
                goto discard;

        if (!neigh_event_send(neigh, skb)) {
                int err;
                struct net_device *dev = neigh->dev;
                unsigned int seq;
```

代码首先进行一些基本检查，然后调用 `neigh_event_send`。 `neigh_event_send` 函数是
`__neigh_event_send` 的简单封装，后者干大部分脏话累活。可以在
[net/core/neighbour.c](https://github.com/torvalds/linux/blob/v3.13/net/core/neighbour.c#L964-L1028)
中读`__neigh_event_send` 的源代码，从大的层面看，三种情况：

1. `NUD_NONE` 状态（默认状态）的邻居：假设
   `/proc/sys/net/ipv4/neigh/default/app_solicit` 和
   `/proc/sys/net/ipv4/neigh/default/mcast_solicit` 配置允许发送探测（如果不是，
   则将状态标记为 `NUD_FAILED`），将导致立即发送 ARP 请求。邻居状态将更新为
   `NUD_INCOMPLETE`
1. `NUD_STALE` 状态的邻居：将更新为 `NUD_DELAYED` 并且将设置计时器以稍后探测它们（
   稍后是现在的时间+`/proc/sys/net/ipv4/neigh/default/delay_first_probe_time` 秒
   ）
1. 检查 `NUD_INCOMPLETE` 状态的邻居（包括上面第一种情形），以确保未解析邻居的排
   队 packet 的数量小于等于`/proc/sys/net/ipv4/neigh/default/unres_qlen`。如果超过
   ，则数据包会出列并丢弃，直到小于等于 proc 中的值。 统计信息中有个计数器会因此
   更新

如果需要 ARP 探测，ARP 将立即被发送。`__neigh_event_send` 将返回 0，表示邻居被视为“已
连接”或“已延迟”，否则返回 1。返回值 0 允许 `neigh_resolve_output` 继续：

```c
                if (dev->header_ops->cache && !neigh->hh.hh_len)
                        neigh_hh_init(neigh, dst);
```

如果邻居关联的设备的协议实现（在我们的例子中是以太网）支持缓存硬件头，并且当前没
有缓存，`neigh_hh_init` 将缓存它。

```c
                do {
                        __skb_pull(skb, skb_network_offset(skb));
                        seq = read_seqbegin(&neigh->ha_lock);
                        err = dev_hard_header(skb, dev, ntohs(skb->protocol),
                                              neigh->ha, NULL, skb->len);
                } while (read_seqretry(&neigh->ha_lock, seq));
```

接下来，seqlock 锁控制对邻居的硬件地址字段（`neigh->ha`）的访问。
`dev_hard_header` 为 skb 创建以太网头时将读取该字段。

之后是错误检查：

```c
                if (err >= 0)
                        rc = dev_queue_xmit(skb);
                else
                        goto out_kfree_skb;
        }
```

如果以太网头写入成功，将调用 `dev_queue_xmit` 将 skb 传递给 Linux 网络设备子系统进行发
送。如果出现错误，goto 将删除 skb，设置并返回错误码：

```c
out:
        return rc;
discard:
        neigh_dbg(1, "%s: dst=%p neigh=%p\n", __func__, dst, neigh);
out_kfree_skb:
        rc = -EINVAL;
        kfree_skb(skb);
        goto out;
}
EXPORT_SYMBOL(neigh_resolve_output);
```

在我们进入 Linux 网络设备子系统之前，让我们看看一些用于监控和转换 IP 协议层的文件。

<a name="chap_6.11"></a>

## 6.11 监控: IP 层

### /proc/net/snmp

```shell
$ cat /proc/net/snmp
Ip: Forwarding DefaultTTL InReceives InHdrErrors InAddrErrors ForwDatagrams InUnknownProtos InDiscards InDelivers OutRequests OutDiscards OutNoRoutes ReasmTimeout ReasmReqds ReasmOKs ReasmFails FragOKs FragFails FragCreates
Ip: 1 64 25922988125 0 0 15771700 0 0 25898327616 22789396404 12987882 51 1 10129840 2196520 1 0 0 0
...
```

这个文件包扩多种协议的统计，IP 层的在最前面，每一列代表什么有说明。

前面我们已经看到 IP 协议层有一些地方会更新计数器。这些计数器的类型是 C 枚举类型，定
义在[include/uapi/linux/snmp.h](https://github.com/torvalds/linux/blob/v3.13/include/uapi/linux/snmp.h#L10-L59):

```c
enum
{
  IPSTATS_MIB_NUM = 0,
/* frequently written fields in fast path, kept in same cache line */
  IPSTATS_MIB_INPKTS,     /* InReceives */
  IPSTATS_MIB_INOCTETS,     /* InOctets */
  IPSTATS_MIB_INDELIVERS,     /* InDelivers */
  IPSTATS_MIB_OUTFORWDATAGRAMS,   /* OutForwDatagrams */
  IPSTATS_MIB_OUTPKTS,      /* OutRequests */
  IPSTATS_MIB_OUTOCTETS,      /* OutOctets */

  /* ... */
```

一些有趣的统计：

* `OutRequests`: Incremented each time an IP packet is attempted to be sent. It appears that this is incremented for every send, successful or not.
* `OutDiscards`: Incremented each time an IP packet is discarded. This can happen if appending data to the skb (for corked sockets) fails, or if the layers below IP return an error.
* `OutNoRoute`: Incremented in several places, for example in the UDP protocol layer (udp_sendmsg) if no route can be generated for a given destination. Also incremented when an application calls “connect” on a UDP socket but no route can be found.
* `FragOKs`: Incremented once per packet that is fragmented. For example, a packet split into 3 fragments will cause this counter to be incremented once.
* `FragCreates`: Incremented once per fragment that is created. For example, a packet split into 3 fragments will cause this counter to be incremented thrice.
* `FragFails`: Incremented if fragmentation was attempted, but is not permitted (because the “Don’t Fragment” bit is set). Also incremented if outputting the fragment fails.

其他（接收数据部分）的统计可以见本文的姊妹篇：[原文
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#monitoring-ip-protocol-layer-statistics)
，[中文翻译版]()。

### /proc/net/netstat

```shell
$ cat /proc/net/netstat | grep IpExt
IpExt: InNoRoutes InTruncatedPkts InMcastPkts OutMcastPkts InBcastPkts OutBcastPkts InOctets OutOctets InMcastOctets OutMcastOctets InBcastOctets OutBcastOctets InCsumErrors InNoECTPkts InECT0Pktsu InCEPkts
IpExt: 0 0 0 0 277959 0 14568040307695 32991309088496 0 0 58649349 0 0 0 0 0
```

格式与前面的类似，除了每列的名称都有 `IpExt` 前缀之外。

一些有趣的统计：

* `OutMcastPkts`: Incremented each time a packet destined for a multicast address is sent.
* `OutBcastPkts`: Incremented each time a packet destined for a broadcast address is sent.
* `OutOctects`: The number of packet bytes output.
* `OutMcastOctets`: The number of multicast packet bytes output.
* `OutBcastOctets`: The number of broadcast packet bytes output.

其他（接收数据部分）的统计可以见本文的姊妹篇：[原文
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#monitoring-ip-protocol-layer-statistics)
，[中文翻译版]()。

注意这些计数分别在 IP 层的不同地方被更新。由于代码一直在更新，重复计数或者计数错误
的 bug 可能会引入。如果这些计数对你非常重要，强烈建议你阅读内核的相应源码，确定它
们是在哪里被更新的，以及更新的对不对，是不是有 bug。

<a name="chap_7"></a>

# 7 Linux netdevice 子系统

在继续跟进 `dev_queue_xmit` 发送数据包之前，让我们花点时间介绍几个将在下一部分中出
现的重要概念。

<a name="chap_7.1"></a>

## 7.1 Linux traffic control（流量控制）

Linux 支持称为流量控制（[traffic
control](http://tldp.org/HOWTO/Traffic-Control-HOWTO/intro.html)）的功能。此功能
允许系统管理员控制数据包如何从机器发送出去。本文不会深入探讨 Linux 流量控制
的各个方面的细节。[这篇文档](http://tldp.org/HOWTO/Traffic-Control-HOWTO/)对流量
控制系统、它如何控制流量，及其其特性进行了深入的介绍。

这里介绍一些值得一提的概念，使后面的代码更容易理解。

流量控制系统包含几组不同的 queue system，每种有不同的排队特征。各个排队系统通常称
为 qdisc，也称为排队规则。你可以将 qdisc 视为**调度程序**; qdisc 决定数据包的发送时
间和方式。

在 Linux 上，每个 device 都有一个与之关联的默认 qdisc。对于仅支持单发送队列的网卡，使
用默认的 qdisc `pfifo_fast`。支持多个发送队列的网卡使用 mq 的默认 qdisc。可以运行 `tc
qdisc` 来查看系统 qdisc 信息。

某些设备支持硬件流量控制，这允许管理员将流量控制 offload 到网络硬件，节省系统的
CPU 资源。

现在已经介绍了这些概念，让我们从
[net/core/dev.c](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L2890-L2894)
继续 `dev_queue_xmit`。

<a name="chap_7.2"></a>

## 7.2 `dev_queue_xmit` and `__dev_queue_xmit`

`dev_queue_xmit` 简单封装了`__dev_queue_xmit`:

```c
int dev_queue_xmit(struct sk_buff *skb)
{
        return __dev_queue_xmit(skb, NULL);
}
EXPORT_SYMBOL(dev_queue_xmit);
```

`__dev_queue_xmit` 才是干脏活累活的地方。我们[一段段
](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L2808-L2825)来看：

```c
static int __dev_queue_xmit(struct sk_buff *skb, void *accel_priv)
{
        struct net_device *dev = skb->dev;
        struct netdev_queue *txq;
        struct Qdisc *q;
        int rc = -ENOMEM;

        skb_reset_mac_header(skb);

        /* Disable soft irqs for various locks below. Also
         * stops preemption for RCU.
         */
        rcu_read_lock_bh();

        skb_update_prio(skb);
```

开始的逻辑：

1. 声明变量
1. 调用 `skb_reset_mac_header`，准备发送 skb。这会重置 skb 内部的指针，使得 ether 头可
   以被访问
1. 调用 `rcu_read_lock_bh`，为接下来的读操作加锁。更多关于使用 RCU 安全访问数据的信
   息，可以参考[这里](https://www.kernel.org/doc/Documentation/RCU/checklist.txt)
1. 调用 `skb_update_prio`，如果启用了[网络优先级 cgroup](https://github.com/torvalds/linux/blob/v3.13/Documentation/cgroups/net_prio.txt)，这会设置 skb 的优先级

现在，我们来看更复杂的部分：

```c
        txq = netdev_pick_tx(dev, skb, accel_priv);
```

这会选择发送队列。本文后面会看到，一些网卡支持多发送队列。我们来看这是如何工作的。

### 7.2.1 `netdev_pick_tx`

`netdev_pick_tx` 定义在[net/core/flow_dissector.c](https://github.com/torvalds/linux/blob/v3.13/net/core/flow_dissector.c#L397-L417):

```c
struct netdev_queue *netdev_pick_tx(struct net_device *dev,
                                    struct sk_buff *skb,
                                    void *accel_priv)
{
        int queue_index = 0;

        if (dev->real_num_tx_queues != 1) {
                const struct net_device_ops *ops = dev->netdev_ops;
                if (ops->ndo_select_queue)
                        queue_index = ops->ndo_select_queue(dev, skb,
                                                            accel_priv);
                else
                        queue_index = __netdev_pick_tx(dev, skb);

                if (!accel_priv)
                        queue_index = dev_cap_txqueue(dev, queue_index);
        }

        skb_set_queue_mapping(skb, queue_index);
        return netdev_get_tx_queue(dev, queue_index);
}
```

如上所示，如果网络设备仅支持单个 TX 队列，则会跳过复杂的代码，直接返回单个 TX 队列。
大多高端服务器上使用的设备都有多个 TX 队列。具有多个 TX 队列的设备有两种情况：

1. 驱动程序实现 `ndo_select_queue`，以硬件或 feature-specific 的方式更智能地选择 TX 队列
1. 驱动程序没有实现 `ndo_select_queue`，这种情况需要内核自己选择设备

从 3.13 内核开始，没有多少驱动程序实现 `ndo_select_queue`。bnx2x 和 ixgbe 驱动程序实
现了此功能，但仅用于以太网光纤通道（[FCoE](https://en.wikipedia.org/wiki/Fibre_Channel_over_Ethernet)）。鉴于此，我们假设网络设备没有实现
`ndo_select_queue` 和/或没有使用 FCoE。在这种情况下，内核将使用`__netdev_pick_tx`
选择 tx 队列。

一旦`__netdev_pick_tx` 确定了队列号，`skb_set_queue_mapping` 将缓存该值（稍后将在
流量控制代码中使用），`netdev_get_tx_queue` 将查找并返回指向该队列的指针。让我们
看一下`__netdev_pick_tx` 在返回`__dev_queue_xmit` 之前的工作原理。

### 7.2.2 `__netdev_pick_tx`

我们来看内核如何选择 TX 队列。
[net/core/flow_dissector.c](https://github.com/torvalds/linux/blob/v3.13/net/core/flow_dissector.c#L375-L395):

```c
u16 __netdev_pick_tx(struct net_device *dev, struct sk_buff *skb)
{
        struct sock *sk = skb->sk;
        int queue_index = sk_tx_queue_get(sk);

        if (queue_index < 0 || skb->ooo_okay ||
            queue_index >= dev->real_num_tx_queues) {
                int new_index = get_xps_queue(dev, skb);
                if (new_index < 0)
                        new_index = skb_tx_hash(dev, skb);

                if (queue_index != new_index && sk &&
                    rcu_access_pointer(sk->sk_dst_cache))
                        sk_tx_queue_set(sk, new_index);

                queue_index = new_index;
        }

        return queue_index;
}
```

代码首先调用 `sk_tx_queue_get` 检查发送队列是否已经缓存在 socket 上，如果尚未缓存，
则返回-1。

下一个 if 语句检查是否满足以下任一条件：

1. `queue_index < 0`：表示尚未设置 TX queue 的情况
1. `ooo_okay` 标志是否非零：如果不为 0，则表示现在允许无序（out of order）数据包。
   协议层必须正确地地设置此标志。当 flow 的所有 outstanding（需要确认的？）数据包都
   已确认时，TCP 协议层将设置此标志。当发生这种情况时，内核可以为此数据包选择不同
   的 TX 队列。UDP 协议层不设置此标志 - 因此 UDP 数据包永远不会将 `ooo_okay` 设置为非零
   值。
1. TX queue index 大于 TX queue 数量：如果用户最近通过 ethtool 更改了设备上的队列数，
   则会发生这种情况。稍后会详细介绍。

以上任何一种情况，都表示没有找到合适的 TX queue，因此接下来代码会进入慢路径以继续
寻找合适的发送队列。首先调用 `get_xps_queue`，它会使用一个由用户配置的 TX queue 到
CPU 的映射，这称为 XPS（Transmit Packet Steering ，发送数据包控制），我们将更详细
地了解 XPS 是什么以及它如何工作。

如果内核不支持 XPS，或者系统管理员未配置 XPS，或者配置的映射引用了无效队列，
`get_xps_queue` 返回-1，则代码将继续调用 `skb_tx_hash`。

一旦 XPS 或内核使用 `skb_tx_hash` 自动选择了发送队列，`sk_tx_queue_set` 会将队列缓存
在 socket 对象上，然后返回。让我们看看 XPS，以及 `skb_tx_hash` 在继续调用
`dev_queue_xmit` 之前是如何工作的。

#### Transmit Packet Steering (XPS)

发送数据包控制（XPS）是一项功能，允许系统管理员配置哪些 CPU 可以处理网卡的哪些发送
队列。XPS 的主要目的是**避免处理发送请求时的锁竞争**。使用 XPS 还可以减少缓存驱逐，
避免[NUMA](https://en.wikipedia.org/wiki/Non-uniform_memory_access)机器上的远程
内存访问等。

可以查看内核有关 XPS 的[文档
](https://github.com/torvalds/linux/blob/v3.13/Documentation/networking/scaling.txt#L364-L422)
了解其如何工作的更多信息。我们后面会介绍如何调整系统的 XPS，现在，你只需要知道
配置 XPS，系统管理员需要定义 TX queue 到 CPU 的映射（bitmap 形式）。

上面代码中，`get_xps_queue` 将查询这个用户指定的映射，以确定应使用哪个发送
队列。如果 `get_xps_queue` 返回-1，则将改为使用 `skb_tx_hash`。

#### `skb_tx_hash`

如果 XPS 未包含在内核中，或 XPS 未配置，或配置的队列不可用（可能因为用户调整了队列数
），`skb_tx_hash` 将接管以确定应在哪个队列上发送数据。准确理解 `skb_tx_hash` 的工作
原理非常重要，具体取决于你的发送负载。请注意，这段代码已经随时间做过一些更新，因
此如果你使用的内核版本与本文不同，则应直接查阅相应版本的 j 内核源代码。

让我们看看它是如何工作的，来自
[include/linux/netdevice.h](https://github.com/torvalds/linux/blob/v3.13/include/linux/netdevice.h#L2331-L2340)
：

```c
/*
 * Returns a Tx hash for the given packet when dev->real_num_tx_queues is used
 * as a distribution range limit for the returned value.
 */
static inline u16 skb_tx_hash(const struct net_device *dev,
                              const struct sk_buff *skb)
{
        return __skb_tx_hash(dev, skb, dev->real_num_tx_queues);
}
```

直接调用了` __skb_tx_hash`, [net/core/flow_dissector.c](https://github.com/torvalds/linux/blob/v3.13/net/core/flow_dissector.c#L239-L271)：

```c
/*
 * Returns a Tx hash based on the given packet descriptor a Tx queues' number
 * to be used as a distribution range.
 */
u16 __skb_tx_hash(const struct net_device *dev, const struct sk_buff *skb,
                  unsigned int num_tx_queues)
{
        u32 hash;
        u16 qoffset = 0;
        u16 qcount = num_tx_queues;

        if (skb_rx_queue_recorded(skb)) {
                hash = skb_get_rx_queue(skb);
                while (unlikely(hash >= num_tx_queues))
                        hash -= num_tx_queues;
                return hash;
        }
```

这个函数中的第一个 if 是一个有趣的短路。函数名 `skb_rx_queue_recorded` 有点误导。skb
有一个 `queue_mapping` 字段，rx 和 tx 都会用到这个字段。无论如何，如果系统正在接收数
据包并将其转发到其他地方，则此 if 语句都为 `true`。否则，代码将继续向下：

```c
        if (dev->num_tc) {
                u8 tc = netdev_get_prio_tc_map(dev, skb->priority);
                qoffset = dev->tc_to_txq[tc].offset;
                qcount = dev->tc_to_txq[tc].count;
        }
```

要理解这段代码，首先要知道，程序可以设置 socket 上发送的数据的优先级。这可以通过
`setsockopt` 带 `SOL_SOCKET` 和 `SO_PRIORITY` 选项来完成。有关 `SO_PRIORITY` 的更多信息
，请参见[socket (7) man
page](http://man7.org/linux/man-pages/man7/socket.7.html)。

请注意，如果使用 `setsockopt` 带 `IP_TOS` 选项来设置在 socket 上发送的 IP 包的 TOS 标志（
或者作为辅助消息传递给 `sendmsg`，在数据包级别设置），内核会将其转换为
`skb->priority`。

如前所述，一些网络设备支持基于硬件的流量控制系统。**如果 num_tc 不为零，则表示此设
备支持基于硬件的流量控制**。这种情况下，将查询一个**packet priority 到该硬件支持
的流量控制**的映射，根据此映射选择适当的流量类型（traffic class）。

接下来，将计算出该 traffic class 的 TX queue 的范围，它将用于确定发送队列。

如果 `num_tc` 为零（网络设备不支持硬件流量控制），则 `qcount` 和 `qoffset` 变量分
别设置为发送队列数和 0。

使用 `qcount` 和 `qoffset`，将计算发送队列的 index：

```c
        if (skb->sk && skb->sk->sk_hash)
                hash = skb->sk->sk_hash;
        else
                hash = (__force u16) skb->protocol;
        hash = __flow_hash_1word(hash);

        return (u16) (((u64) hash * qcount) >> 32) + qoffset;
}
EXPORT_SYMBOL(__skb_tx_hash);
```

最后，通过`__netdev_pick_tx` 返回选出的 TX queue index。

<a name="chap_7.3"></a>

## 7.3 继续`__dev_queue_xmit`

至此已经选到了合适的发送队列。

继续`__dev_queue_xmit can continue`:

```c
        q = rcu_dereference_bh(txq->qdisc);

#ifdef CONFIG_NET_CLS_ACT
        skb->tc_verd = SET_TC_AT(skb->tc_verd, AT_EGRESS);
#endif
        trace_net_dev_queue(skb);
        if (q->enqueue) {
                rc = __dev_xmit_skb(skb, q, dev, txq);
                goto out;
        }
```

首先获取与此队列关联的 qdisc。回想一下，之前我们看到单发送队列设备的默认类型是
`pfifo_fast` qdisc，而对于多队列设备，默认类型是 `mq` qdisc。

接下来，如果内核中已启用数据包分类 API，则代码会为 packet 分配 traffic class。 接下
来，检查 disc 是否有合适的队列来存放 packet。像 `noqueue` 这样的 qdisc 没有队列。 如果
有队列，则代码调用`__dev_xmit_skb` 继续处理数据，然后跳转到此函数的末尾。我们很快
就会看到`__dev_xmit_skb`。现在，让我们看看如果没有队列会发生什么，从一个非常有用
的注释开始：

```c
        /* The device has no queue. Common case for software devices:
           loopback, all the sorts of tunnels...

           Really, it is unlikely that netif_tx_lock protection is necessary
           here.  (f.e. loopback and IP tunnels are clean ignoring statistics
           counters.)
           However, it is possible, that they rely on protection
           made by us here.

           Check this and shot the lock. It is not prone from deadlocks.
           Either shot noqueue qdisc, it is even simpler 8)
         */
        if (dev->flags & IFF_UP) {
                int cpu = smp_processor_id(); /* ok because BHs are off */
```

正如注释所示，**唯一可以拥有"没有队列的 qdisc"的设备是环回设备和隧道设备**。如果
设备当前处于运行状态，则获取当前 CPU，然后判断此设备队列上的发送锁是否由此 CPU 拥有
：

```c
                if (txq->xmit_lock_owner != cpu) {

                        if (__this_cpu_read(xmit_recursion) > RECURSION_LIMIT)
                                goto recursion_alert;
```

如果发送锁不由此 CPU 拥有，则在此处检查 per-CPU 计数器变量 `xmit_recursion`，判断其是
否超过 `RECURSION_LIMIT`。 一个程序可能会在这段代码这里持续发送数据，然后被抢占，
调度程序选择另一个程序来运行。第二个程序也可能驻留在此持续发送数据。因此，
`xmit_recursion` 计数器用于确保在此处竞争发送数据的程序不超过 `RECURSION_LIMIT` 个
。

我们继续：

```c
                        HARD_TX_LOCK(dev, txq, cpu);

                        if (!netif_xmit_stopped(txq)) {
                                __this_cpu_inc(xmit_recursion);
                                rc = dev_hard_start_xmit(skb, dev, txq);
                                __this_cpu_dec(xmit_recursion);
                                if (dev_xmit_complete(rc)) {
                                        HARD_TX_UNLOCK(dev, txq);
                                        goto out;
                                }
                        }
                        HARD_TX_UNLOCK(dev, txq);
                        net_crit_ratelimited("Virtual device %s asks to queue packet!\n",
                                             dev->name);
                } else {
                        /* Recursion is detected! It is possible,
                         * unfortunately
                         */
recursion_alert:
                        net_crit_ratelimited("Dead loop on virtual device %s, fix it urgently!\n",
                                             dev->name);
                }
        }
```

接下来的代码首先尝试获取发送锁，然后检查要使用的设备的发送队列是否被停用。如果没
有停用，则更新 `xmit_recursion` 计数，然后将数据向下传递到更靠近发送的设备。我们稍
后会更详细地看到 `dev_hard_start_xmit`。

或者，如果当前 CPU 是发送锁定的拥有者，或者如果 `RECURSION_LIMIT` 被命中，则不进行发
送，而会打印告警日志。

函数剩余部分的代码设置错误码并返回。

由于我们对真正的以太网设备感兴趣，让我们来看一下之前就需要跟进去的
`__dev_xmit_skb` 函数，这是发送主线上的函数。

<a name="chap_7.4"></a>

## 7.4 `__dev_xmit_skb`

现在我们带着排队规则 `qdisc`、网络设备 `dev` 和发送队列 `txq` 三个变量来到
`__dev_xmit_skb`，
[net/core/dev.c](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L2684-L2745)
：

```c
static inline int __dev_xmit_skb(struct sk_buff *skb, struct Qdisc *q,
                                 struct net_device *dev,
                                 struct netdev_queue *txq)
{
        spinlock_t *root_lock = qdisc_lock(q);
        bool contended;
        int rc;

        qdisc_pkt_len_init(skb);
        qdisc_calculate_pkt_len(skb, q);
        /*
         * Heuristic to force contended enqueues to serialize on a
         * separate lock before trying to get qdisc main lock.
         * This permits __QDISC_STATE_RUNNING owner to get the lock more often
         * and dequeue packets faster.
         */
        contended = qdisc_is_running(q);
        if (unlikely(contended))
                spin_lock(&q->busylock);
```

代码首先使用 `qdisc_pkt_len_init` 和 `qdisc_calculate_pkt_len` 来计算数据的准确长度
，稍后 qdisc 会用到该值。 对于硬件 offload（例如 UFO）这是必需的，因为添加的额外的头
信息，硬件 offload 的时候回用到。

接下来，使用另一个锁来帮助减少 qdisc 主锁上的竞争（我们稍后会看到这第二个锁）。 如
果 qdisc 当前正在运行，那么试图发送的其他程序将在 qdisc 的 `busylock` 上竞争。 这允许
运行 qdisc 的程序在处理数据包的同时，与较少量的程序竞争第二个主锁。随着竞争者数量
的减少，这种技巧增加了吞吐量。[原始 commit 描述
](https://github.com/torvalds/linux/commit/79640a4ca6955e3ebdb7038508fa7a0cd7fa5527)
。 接下来是主锁：

```c
        spin_lock(root_lock);
```

接下来处理 3 种可能情况：

1. 如果 qdisc 已停用
1. 如果 qdisc 允许数据包 bypass 排队系统，并且没有其他包要发送，并且 qdisc 当前没有运
   行。允许包 bypass 所谓的**“work-conserving qdisc” - 那些用于流量整形（traffic
   reshaping）目的并且不会引起发送延迟的 qdisc**
1. 所有其他情况

让我们来看看每种情况下发生什么，从 qdisc 停用开始：

```c
        if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
                kfree_skb(skb);
                rc = NET_XMIT_DROP;
```

这很简单。 如果 qdisc 停用，则释放数据并将返回代码设置为 `NET_XMIT_DROP`。

接下来，如果 qdisc 允许数据包 bypass，并且没有其他包要发送，并且 qdisc 当前没有运行：

```c
        } else if ((q->flags & TCQ_F_CAN_BYPASS) && !qdisc_qlen(q) &&
                   qdisc_run_begin(q)) {
                /*
                 * This is a work-conserving queue; there are no old skbs
                 * waiting to be sent out; and the qdisc is not running -
                 * xmit the skb directly.
                 */
                if (!(dev->priv_flags & IFF_XMIT_DST_RELEASE))
                        skb_dst_force(skb);

                qdisc_bstats_update(q, skb);

                if (sch_direct_xmit(skb, q, dev, txq, root_lock)) {
                        if (unlikely(contended)) {
                                spin_unlock(&q->busylock);
                                contended = false;
                        }
                        __qdisc_run(q);
                } else
                        qdisc_run_end(q);

                rc = NET_XMIT_SUCCESS;
```

这个 if 语句有点复杂，如果满足以下所有条件，则整个语句的计算结果为 true：

1. `q-> flags＆TCQ_F_CAN_BYPASS`：qdisc 允许数据包绕过排队系统。对于所谓的“
   work-conserving” qdiscs 这会是 `true`；即，允许 packet bypass 流量整形 qdisc。
   `pfifo_fast` qdisc 允许数据包 bypass
1. `!qdisc_qlen(q)`：qdisc 的队列中没有待发送的数据
1. `qdisc_run_begin(p)`：如果 qdisc 未运行，此函数将设置 qdisc 的状态为“running”并返
   回 `true`，如果 qdisc 已在运行，则返回 `false`

如果以上三个条件都为 `true`，那么：

* 检查 `IFF_XMIT_DST_RELEASE` 标志，此标志允许内核释放 skb 的目标缓存。如果标志已禁用，将强制对 skb 进行引用计数
* 调用 `qdisc_bstats_update` 更新 qdisc 发送的字节数和包数统计
* 调用 `sch_direct_xmit` 用于发送数据包。我们将很快深入研究 `sch_direct_xmit`，因为慢路径也会调用到它

`sch_direct_xmit` 的返回值有两种情况：

1. 队列不为空（返回> 0）。在这种情况下，`busylock` 将被释放，然后调用`__qdisc_run` 重新启动 qdisc 处理
1. 队列为空（返回 0）。在这种情况下，`qdisc_run_end` 用于关闭 qdisc 处理

在任何一种情况下，都会返回 `NET_XMIT_SUCCESS`，这不是太糟糕。

让我们检查最后一种情况：

```c
        } else {
                skb_dst_force(skb);
                rc = q->enqueue(skb, q) & NET_XMIT_MASK;
                if (qdisc_run_begin(q)) {
                        if (unlikely(contended)) {
                                spin_unlock(&q->busylock);
                                contended = false;
                        }
                        __qdisc_run(q);
                }
        }
```

在所有其他情况下：

1. 调用 `skb_dst_force` 强制对 skb 的目标缓存进行引用计数
1. 调用 qdisc 的 `enqueue` 方法将数据入队，保存函数返回值
1. 调用 `qdisc_run_begin(p)`将 qdisc 标记为正在运行。如果它尚未运行（`contended ==
   false`），则释放 `busylock`，然后调用`__qdisc_run(p)`启动 qdisc 处理

函数最后释放相应的锁，并返回状态码：

```c
        spin_unlock(root_lock);
        if (unlikely(contended))
                spin_unlock(&q->busylock);
        return rc;
```

<a name="chap_7.5"></a>

## 7.5 调优: Transmit Packet Steering (XPS)

使用 XPS 需要在内核配置中启用它（Ubuntu 上内核 3.13.0 有 XPS），并提供一个位掩码，用于
描述**CPU 和 TX queue 的对应关系**。

这些位掩码类似于
[RPS](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#receive-packet-steering-rps)
位掩码，你可以在内核[文档
](https://github.com/torvalds/linux/blob/v3.13/Documentation/networking/scaling.txt#L147-L150)
中找到有关这些位掩码的一些资料。

简而言之，要修改的位掩码位于以下位置：

```shell
/sys/class/net/DEVICE_NAME/queues/QUEUE/xps_cpus
```

因此，对于 eth0 和 TX queue 0，你需要使用十六进制数修改文件：
`/sys/class/net/eth0/queues/tx-0/xps_cpus`，制定哪些 CPU 应处理来自 eth0 的发送队列 0
的发送过程。另外，[文档
](https://github.com/torvalds/linux/blob/v3.13/Documentation/networking/scaling.txt#L412-L422)
指出，在某些配置中可能不需要 XPS。

<a name="chap_8"></a>

# 8 Queuing Disciplines（排队规则）

至此，我们需要先看一些 qdisc 代码。本文不打算涵盖 TX 所有选项的具体细节。
如果对此感兴趣，可以查看[这篇](http://lartc.org/howto/index.html)很棒的指南。

接下来将查看**通用的数据包调度程序**（generic packet scheduler）是如何工作的
。特别地，我们将分析 `qdisc_run_begin()`、`qdisc_run_end()`、`__ qdisc_run()` 和
`sch_direct_xmit()` 函数是如何一层层将数据传递给驱动程序的。

从 `qdisc_run_begin()` 的工作原理开始。

<a name="chap_8.1"></a>

## 8.1 `qdisc_run_begin()` and `qdisc_run_end()`：仅设置 qdisc 状态位

定义在[include/net/sch_generic.h](https://github.com/torvalds/linux/blob/v3.13/include/net/sch_generic.h#L101-L107):

```c
static inline bool qdisc_run_begin(struct Qdisc *qdisc)
{
        if (qdisc_is_running(qdisc))
                return false;
        qdisc->__state |= __QDISC___STATE_RUNNING;
        return true;
}

static inline void qdisc_run_end(struct Qdisc *qdisc)
{
        qdisc->__state &= ~__QDISC___STATE_RUNNING;
}
```

* `qdisc_run_begin()` 检查 qdisc 是否设置了`__QDISC___STATE_RUNNING` 状态
   位。如果设置了，直接返回 `false`；否则，设置此状态位，然后返回 `true`。
* `qdisc_run_end()` 执行相反的操作，清除此状态位。

需要注意的是，这两个函数都**只是设置状态位，并没有真正干活**。真正的处理过程是从
`__qdisc_run()` 开始的。

<a name="chap_8.2"></a>

## 8.2 `__qdisc_run()`：真正的 qdisc 执行入口

这个函数乍看非常简单，甚至让人产生错觉：

```c
void __qdisc_run(struct Qdisc *q)
{
        int quota = weight_p;

        while (qdisc_restart(q)) { // 从队列取出一个 skb 并发送，剩余队列不为空时返回非零，见 8.3

                // 如果发生下面情况之一，则延后处理：
                // 1. quota 用尽
                // 2. 其他进程需要 CPU
                if (--quota <= 0 || need_resched()) {
                        __netif_schedule(q);
                        break;
                }
        }

        qdisc_run_end(q);          // 清除 RUNNING 状态位
}
```

函数首先获取 `weight_p`，这个变量通常是通过 sysctl 设置的，收包路径也会用到。我们稍
后会看到如何调整此值。这个循环做两件事：

1. 在 `while` 循环中调用 `qdisc_restart()`，直到它返回 `false`（或触发下面的中断）。
1. 判断是否还有 quota，或 `need_resched()` 是否返回 `true`。其中任何一个为真，
   将调用 `__netif_schedule()` 然后跳出循环。

> 注意：用户程序调用 `sendmsg` **系统调用之后，内核便接管了执行过程，一路执行到
> 这里;用户程序一直在累积系统时间（system time）**。

* 如果用户程序在内核中用完其 time quota，`need_resched()` 将返回 `true`。
* 如果仍有 quota，且用户程序的时间片尚未使用，则将再次调用 `qdisc_restart()`。

先来看看 `qdisc_restart(q)`是如何工作的，然后将深入研究`__netif_schedule(q)`。

<a name="chap_8.3"></a>

## 8.3 `qdisc_restart`：从 qdisc 队列中取包，发送给网络驱动

[qdisc_restart()](https://github.com/torvalds/linux/blob/v3.13/net/sched/sch_generic.c#L156-L192):

```c
/*
 * NOTE: Called under qdisc_lock(q) with locally disabled BH.
 *
 * __QDISC_STATE_RUNNING guarantees only one CPU can process
 * this qdisc at a time. qdisc_lock(q) serializes queue accesses for this queue.
 *
 *  netif_tx_lock serializes accesses to device driver.
 *
 *  qdisc_lock(q) and netif_tx_lock are mutually exclusive,
 *  if one is grabbed, another must be free.
 *
 * Returns to the caller:
 *                                0  - queue is empty or throttled.
 *                                >0 - queue is not empty.
 */
static inline int qdisc_restart(struct Qdisc *q)
{
        struct sk_buff      *skb = dequeue_skb(q);
        if (!skb)
            return 0;

        spinlock_t          *root_lock = qdisc_lock(q);
        struct net_device   *dev = qdisc_dev(q);
        struct netdev_queue *txq = netdev_get_tx_queue(dev, skb_get_queue_mapping(skb));

        return sch_direct_xmit(skb, q, dev, txq, root_lock);
}
```

`qdisc_restart()` 函数开头的注释非常有用，描述了用到的三个锁：

1. `__QDISC_STATE_RUNNING` 保证了同一时间只有一个 CPU 可以处理这个 qdisc。
1. `qdisc_lock(q)` 将**访问此 qdisc** 的操作顺序化。
1. `netif_tx_lock` 将**访问设备驱动**的操作顺序化。

函数逻辑：

1. 首先调用 `dequeue_skb()` 从 qdisc 中取出要发送的 skb。如果队列为空，返回 0，
   这将导致上层的 `qdisc_restart()` 返回 `false`，继而退出 `while` 循环。
2. 如果 skb 不为空，接下来获取 qdisc 队列锁，然后找到相关的发送设备 `dev` 和发送
   队列 `txq`，最后带着这些参数调用 `sch_direct_xmit()`。

先来看 `dequeue_skb()`，然后再回到 `sch_direct_xmit()`。

### 8.3.1 `dequeue_skb()`：从 qdisc 队列取待发送 skb

定义在 [net/sched/sch_generic.c](https://github.com/torvalds/linux/blob/v3.13/net/sched/sch_generic.c#L59-L78)。

```c
static inline struct sk_buff *dequeue_skb(struct Qdisc *q)
{
    struct sk_buff      *skb = q->gso_skb;   // 待发送包
    struct netdev_queue *txq = q->dev_queue; // 之前发送失败的包所在的队列

    if (unlikely(skb)) {
        /* check the reason of requeuing without tx lock first */
        txq = netdev_get_tx_queue(txq->dev, skb_get_queue_mapping(skb));

        if (!netif_xmit_frozen_or_stopped(txq)) {
            q->gso_skb = NULL;
            q->q.qlen--;
        } else
            skb = NULL;
    } else {
        if (!(q->flags & TCQ_F_ONETXQUEUE) || !netif_xmit_frozen_or_stopped(txq))
            skb = q->dequeue(q);
    }

    return skb;
```

函数首先声明一个 `struct sk_buff *skb` 变量，这是接下来要处理的数据。这个变量后
面会依不同情况而被赋不同的值，最后作为返回值返回给调用方。

变量 `skb` 初始化为 qdisc 的 `gso_skb` 字段，这是**之前由于发送失败而重新入队的数据**。

接下来分为两种情况，根据 `skb = q->gso_skb` 是否为空：

1. 如果不为空，会将之前重新入队的 skb 出队，作为待处理数据返回。

    1. 检查发送队列是否已停止。
    2. 如果队列未停止，则 `gso_skb` 字段置空，队列长度减 1，返回 skb。
    3. 如果队列已停止，则 `gso_skb` 不动，返回空。

2. 如果为空（即之前没有数据重新入队），则从要处理的 qdisc 中取出一个新 skb，作为待处理数据返回。

    进入另一个 tricky 的 if 语句，如果：

    1. qdisc 不是单发送队列，或
    1. 发送队列未停止工作

    则调用 qdisc 的 `dequeue()` 方法获取新数据并返回。dequeue 的内部实现依 qdisc 的实现和功能而有所不同。

该函数最后返回变量 `skb`，这是接下来要处理的数据包。

### 8.3.2 `sch_direct_xmit()`：发送给网卡驱动

现在来到 `sch_direct_xmit()`（定义在
[net/sched/sch_generic.c](https://github.com/torvalds/linux/blob/v3.13/net/sched/sch_generic.c#L109-L154)
），这是将数据向下发送到网络设备的重要一步。

```c
/*
 * Transmit one skb, and handle the return status as required. Holding the
 * __QDISC_STATE_RUNNING bit guarantees that only one CPU can execute this
 * function.
 *
 * Returns to the caller:
 *                                0  - queue is empty or throttled.
 *                                >0 - queue is not empty.
 */
int sch_direct_xmit(struct sk_buff *skb, struct Qdisc *q,
                    struct net_device *dev, struct netdev_queue *txq,
                    spinlock_t *root_lock)
{
        int ret = NETDEV_TX_BUSY;

        spin_unlock(root_lock);
        if (!netif_xmit_frozen_or_stopped(txq))
            ret = dev_hard_start_xmit(skb, dev, txq);
        spin_lock(root_lock);

        if (dev_xmit_complete(ret)) {                    // 1. 驱动发送成功
            ret = qdisc_qlen(q);                         //    将 qdisc 队列的剩余长度作为返回值
        } else if (ret == NETDEV_TX_LOCKED) {            // 2. 驱动获取发送锁失败
            ret = handle_dev_cpu_collision(skb, txq, q);
        } else {                                         // 3. 驱动发送“正忙”，当前无法发送
            ret = dev_requeue_skb(skb, q);               //    将数据重新入队，等下次发送。
        }

        if (ret && netif_xmit_frozen_or_stopped(txq))
            ret = 0;

        return ret;
```

这段代码首先释放 qdisc（发送队列）锁，然后获取（设备驱动的）发送锁。

接下来，如果发送队列没有停止，就会调用 `dev_hard_start_xmit()`。稍后将看到，
后者会把数据从 Linux 内核的网络设备子系统发送到设备驱动程序。

`dev_hard_start_xmit()` 执行之后，（或因发送队列停止而跳过执行），队列的发送锁就会被释放。

接下来，再次获取此 qdisc 的锁，然后通过调用 `dev_xmit_complete()` 检查 `dev_hard_start_xmit()` 的返回值。

1. 如果 `dev_xmit_complete()` 返回 `true`，数据已成功发送，则将 qdisc 队列长度设置为返回值，否则
1. 如果 `dev_hard_start_xmit()` 返回的是 `NETDEV_TX_LOCKED`，调用 `handle_dev_cpu_collision()` 来处理锁竞争。

    当驱动程序锁定发送队列失败时，支持 `NETIF_F_LLTX` 功能的设备会返回 `NETDEV_TX_LOCKED`。 稍后会仔细研究 `handle_dev_cpu_collision`。

现在，让我们继续关注 `sch_direct_xmit()` 并查看，以上两种情况都不满足时的情况。
如果发送失败，而且不是以上两种情况，那还有第三种可能：由于 `NETDEV_TX_BUSY`。驱动
程序返回 `NETDEV_TX_BUSY` 表示设备或驱动程序“正忙”，数据现在无法发送。这种情
况下，调用 `dev_requeue_skb()` 将数据重新入队，等下次发送。

来深入地看一下 `handle_dev_cpu_collision()` 和 `dev_requeue_skb()`。

### 8.3.3 `handle_dev_cpu_collision()`

定义在 [net/sched/sch_generic.c](https://github.com/torvalds/linux/blob/v3.13/net/sched/sch_generic.c#L80-L107)，处理两种情况：

1. 发送锁由当前 CPU 保持
1. 发送锁由其他 CPU 保存

第一种情况认为是配置问题，打印一条警告。

第二种情况，更新统计计数器 `cpu_collision`，通过 `dev_requeue_skb` 将数据重新入队
以便稍后发送。回想一下，我们在 `dequeue_skb` 中看到了专门处理重新入队的 skb 的代码。

代码很简短，可以快速阅读：

```c
static inline int handle_dev_cpu_collision(struct sk_buff *skb,
                                           struct netdev_queue *dev_queue,
                                           struct Qdisc *q)
{
        int ret;

        if (unlikely(dev_queue->xmit_lock_owner == smp_processor_id())) {
                /*
                 * Same CPU holding the lock. It may be a transient
                 * configuration error, when hard_start_xmit() recurses. We
                 * detect it by checking xmit owner and drop the packet when
                 * deadloop is detected. Return OK to try the next skb.
                 */
                kfree_skb(skb);
                net_warn_ratelimited("Dead loop on netdevice %s, fix it urgently!\n",
                                     dev_queue->dev->name);
                ret = qdisc_qlen(q);
        } else {
                /*
                 * Another cpu is holding lock, requeue & delay xmits for
                 * some time.
                 */
                __this_cpu_inc(softnet_data.cpu_collision);
                ret = dev_requeue_skb(skb, q);
        }

        return ret;
}
```

接下来看看 `dev_requeue_skb` 做了什么，后面会看到，`sch_direct_xmit` 会调用它.

### 8.3.4 `dev_requeue_skb()`：重新压入 qdisc 队列，等待下次发送

这个函数很简短，[net/sched/sch_generic.c](https://github.com/torvalds/linux/blob/v3.13/net/sched/sch_generic.c#L39-L57):

```c
/* Modifications to data participating in scheduling must be protected with
 * qdisc_lock(qdisc) spinlock.
 *
 * The idea is the following:
 * - enqueue, dequeue are serialized via qdisc root lock
 * - ingress filtering is also serialized via qdisc root lock
 * - updates to tree and tree walking are only done under the rtnl mutex.
 */
static inline int dev_requeue_skb(struct sk_buff *skb, struct Qdisc *q)
{
        skb_dst_force(skb);   // skb 上强制增加一次引用计数
        q->gso_skb = skb;     // 回想一下，dequeue_skb() 中取出一个 skb 时会检查该字段
        q->qstats.requeues++; // 更新 `requeue` 计数
        q->q.qlen++;          // 更新 qdisc 队列长度

        __netif_schedule(q);  // 触发 softirq
        return 0;
}
```

接下来再回忆一遍我们一步步到达这里的过程，然后查看 `__netif_schedule()`。

<a name="chap_8.4"></a>

## 8.4 复习：`__qdisc_run()` 主逻辑

回想一下，我们是从 `__qdisc_run()` 开始到达这里的：

```c
void __qdisc_run(struct Qdisc *q)
{
        int quota = weight_p;
        while (qdisc_restart(q)) { // dequeue skb, send it
            if (--quota <= 0 || need_resched()) {// Ordered by possible occurrence: Postpone processing if
                    __netif_schedule(q);         // 1. we've exceeded packet quota
                    break;                       // 2. another process needs the CPU
            }
        }
        qdisc_run_end(q);
}
```

`while` 循环调用 `qdisc_restart()`，后者取出一个 skb，然后尝试通过
`sch_direct_xmit()` 来发送；`sch_direct_xmit` 调用 `dev_hard_start_xmit` 来向驱动
程序进行实际发送。任何无法发送的 skb 都重新入队，将在 `NET_TX` softirq 中进行
发送。

发送过程的下一步是查看 `dev_hard_start_xmit()`，了解如何调用驱动程序来发送数据。但
在此之前，应该先查看 `__netif_schedule()` 以完全理解 `__qdisc_run()` 和
`dev_requeue_skb()` 的工作方式。

### 8.4.1 `__netif_schedule`

现在来看 `__netif_schedule()`，
[net/core/dev.c](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L2127-L2146):

```c
void __netif_schedule(struct Qdisc *q)
{
    if (!test_and_set_bit(__QDISC_STATE_SCHED, &q->state))
            __netif_reschedule(q);
}
EXPORT_SYMBOL(__netif_schedule);

static inline void __netif_reschedule(struct Qdisc *q)
{
    struct softnet_data *sd;
    unsigned long flags;

    local_irq_save(flags);                  // 保存硬中断状态，并禁用硬中断（IRQ）
    sd = &__get_cpu_var(softnet_data);      // 获取当前 CPU 的 struct softnet_data 实例
    q->next_sched = NULL;
    *sd->output_queue_tailp = q;            // 将 qdisc 添加到 softnet_data 的 output 队列中
    sd->output_queue_tailp = &q->next_sched;
    raise_softirq_irqoff(NET_TX_SOFTIRQ);   // 重要步骤：触发 NET_TX_SOFTIRQ 类型软中断（softirq）
    local_irq_restore(flags);               // 恢复 IRQ 状态并重新启用硬中断
}
```

`test_and_set_bit()` 检查 `q->state` 中的 `__QDISC_STATE_SCHED` 位，如果为该位为 0，会将其置 1。
如果置位成功（意味着之前处于非 `__QDISC_STATE_SCHED` 状态），代码将调用
`__netif_reschedule()`，这个函数不长，但做的事情非常重要。

> 更多有关 `struct softnet_data` 初始化的内容，可参考我们之前关于网络栈接收数据的
> [文章](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#linux-network-device-subsystem)。

`__netif_reschedule()` 中的重要步骤是 `raise_softirq_irqoff()`，它触发一次 `NET_TX_SOFTIRQ` 类型
softirq。 softirqs 及其注册过程也包含在我们之前的[文章
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#softirqs)
中。简单来说，可以认为 **softirqs 是以很高优先级在执行的内核线程，并代表内核处理数据**，
用于网络数据的收发处理（incoming 和 outgoing）。

正如在[上一篇]()文章中看到的，`NET_TX_SOFTIRQ` softirq 有一个注册的回调函数
`net_tx_action()`，这意味着有一个内核线程将会执行 `net_tx_action()`。该线程偶尔会被暂
停（pause），`raise_softirq_irqoff()` 会恢复（resume）其执行。让我们看一下
`net_tx_action()` 的作用，以便了解内核如何处理发送数据请求。

### 8.4.2 `net_tx_action()`

定义在
[net/core/dev.c](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L3297-L3353)
，由两个 if 组成，分别处理 executing CPU 的 **`softnet_data` 实例的两个 queue**：

1. completion queue
1. output queue

让我们分别来看这两种情况，注意，**这段代码在 softirq 上下文中作为一个独立的内核线
程执行**。网络栈发送侧的**热路径中不适合执行的代码，将被延后（defer），然
后由执行 `net_tx_action()` 的线程处理**。

### 8.4.3 `net_tx_action()` completion queue：待释放 skb 队列

`softnet_data` 的 completion queue 存放**等待释放的 skb**。函数 `dev_kfree_skb_irq` 可以将
skbs 添加到队列中以便稍后释放。设备驱动程序通常使用它来推迟释放已经发送成功的 skbs。驱动
程序推迟释放 skb 的原因是，释放内存可能需要时间，而且有些代码（如 hardirq 处理程序）
需要尽可能快的执行并返回。

看一下 `net_tx_action` 第一段代码，该代码处理 completion queue 中等待释放的 skb：

```c
        if (sd->completion_queue) {
                struct sk_buff *clist;

                local_irq_disable();
                clist = sd->completion_queue;
                sd->completion_queue = NULL;
                local_irq_enable();

                while (clist) {
                        struct sk_buff *skb = clist;
                        clist = clist->next;
                        __kfree_skb(skb);
                }
        }
```

如果 completion queue 非空，`while` 循环将遍历这个列表并`__kfree_skb` 释放每个 skb 占
用的内存。**牢记，此代码在一个名为 softirq 的独立“线程”中运行 - 它并没有占用用
户程序的系统时间（system time）**。

### 8.4.4 `net_tx_action` output queue：待发送 skb 队列

output queue 存储 **待发送的 skb**。如前所述，`__netif_reschedule()` 将数据添加到 output
queue 中，通常从`__netif_schedule` 调用过来。

目前，我们看到 `__netif_schedule()` 函数在两个地方被调用：

1. `dev_requeue_skb()`：如果驱动程序返回 `NETDEV_TX_BUSY` 或者存在 CPU 冲突，可以调用此函数。
1. `__qdisc_run()`：一旦超出 quota 或者需要 reschedule，会调用`__netif_schedule`。

这个函数会将 qdisc 添加到 softnet_data 的 output queue 进行处理。 这里将输出队列处理代码拆分为三个块。

我们来看看第一块：

```c
    if (sd->output_queue) {       // 如果 output queue 上有 qdisc
        struct Qdisc *head;

        local_irq_disable();
        head = sd->output_queue;  // 将 head 指向第一个 qdisc
        sd->output_queue = NULL;
        sd->output_queue_tailp = &sd->output_queue; // 更新队尾指针
        local_irq_enable();
```

如果 output queue 上有 qdisc，则将 `head` 变量指向第一个 qdisc，并
更新队尾指针。

接下来，一个 **`while` 循环开始遍历 qdsics 列表**：

```c
    while (head) {
        struct Qdisc *q = head;
        head = head->next_sched;

        spinlock_t *root_lock = qdisc_lock(q);

        if (spin_trylock(root_lock)) {                 // 非阻塞：尝试获取 qdisc root lock
            smp_mb__before_clear_bit();
            clear_bit(__QDISC_STATE_SCHED, &q->state); // 清除 q->state SCHED 状态位

            qdisc_run(q);                              // 执行 qdisc 规则，这会设置 q->state 的 RUNNING 状态位

            spin_unlock(root_lock);                    // 释放 qdisc 锁
        } else {
            if (!test_bit(__QDISC_STATE_DEACTIVATED, &q->state)) { // qdisc 还在运行
                __netif_reschedule(q);                 // 重新放入 queue，稍后继续尝试获取 root lock
            } else {                                   // qdisc 已停止运行，清除 SCHED 状态位
                smp_mb__before_clear_bit();
                clear_bit(__QDISC_STATE_SCHED, &q->state);
            }
        }
    }
```

`spin_trylock()` 获得 root lock 后，

1. 调用 `clear_bit()` 清除 qdisc 的 `__QDISC_STATE_SCHED` 状态位。
2. 然后执行 `qdisc_run()`，这会将 `__QDISC___STATE_RUNNING` 状态位置 1，并执行`__qdisc_run()`。

这里很重要。从系统调用开始的发送过程代表 applition 执行，花费的是系统时间；但接
下来它将转入 softirq 上下文中执行（这个 qdisc 的 skb 之前没有被发送出去发），花
费的是 softirq 时间。这种区分非常重要，因为这**直接影响着应用程序的 CPU 使用量监
控**，尤其是发送大量数据的应用。换一种陈述方式：

1. 无论发送完成还是驱动程序返回错误，程序的系统时间都包括调用驱动程序发送数据所花的时间。
1. 如果驱动层发送失败（例如，设备忙于发送其他内容），则会将 qdisc 添加到
   output queue，稍后由 softirq 线程处理。在这种情况下，将会额外花费一些 softirq（
   `si`）时间在发送数据上。

因此，发送数据花费的总时间是下面二者之和：

1. **系统调用的系统时间**（sys time）
2. **`NET_TX` 类型的 softirq 时间**（softirq time）

如果 `spin_trylock()` 失败，则检查 qdisc 是否已经停止运行（`__QDISC_STATE_DEACTIVATED` 状态位），两种情况：

1. qdisc 未停用：调用 `__netif_reschedule()`，这会将 qdisc 放回到原 queue 中，稍后再次尝试获取 qdisc 锁。
1. qdisc 已停用：清除 `__QDISC_STATE_SCHED` 状态位。

<a name="chap_8.5"></a>

## 8.5 最终来到 `dev_hard_start_xmit`

至此，我们已经穿过了整个网络栈，最终来到 `dev_hard_start_xmit`。也许你是从
`sendmsg` 系统调用直接到达这里的，或者你是通过 qdisc 上的 softirq 线程处理网络数据来
到这里的。`dev_hard_start_xmit` 将调用设备驱动程序来实际执行发送操作。

这个函数处理两种主要情况：

1. 已经准备好要发送的数据，或
1. 需要 segmentation offloading 的数据

先看第一种情况，要发送的数据已经准备好的情况。
[net/code/dev.c](https://github.com/torvalds/linux/blob/v3.13/net/core/dev.c#L2541-L2652)
：

```c
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
                        struct netdev_queue *txq)
{
        const struct net_device_ops *ops = dev->netdev_ops;
        int rc = NETDEV_TX_OK;
        unsigned int skb_len;

        if (likely(!skb->next)) {
                netdev_features_t features;

                /*
                 * If device doesn't need skb->dst, release it right now while
                 * its hot in this cpu cache
                 */
                if (dev->priv_flags & IFF_XMIT_DST_RELEASE)
                        skb_dst_drop(skb);

                features = netif_skb_features(skb);
```

代码首先获取设备的回调函数集合 `ops`，后面让驱动程序做一些发送数据的工作时会用到
。检查 `skb->next` 以确定此数据不是已分片数据的一部分，然后继续执行以下两项操作：

首先，检查设备是否设置了 `IFF_XMIT_DST_RELEASE` 标志。这个版本的内核中的任何“真实”
以太网设备都不使用此标志，但环回设备和其他一些软件设备使用。如果启用此特性，则可
以减少目标高速缓存条目上的引用计数，因为驱动程序不需要它。

接下来，`netif_skb_features` 获取设备支持的功能列表，并根据数据的协议类型（
`dev->protocol`）对特性列表进行一些修改。例如，如果设备支持此协议的校验和计算，
则将对 skb 进行相应的标记。 VLAN tag（如果已设置）也会导致功能标记被修改。

接下来，将检查 vlan 标记，如果设备无法 offload VLAN tag，将通过`__vlan_put_tag` 在软
件中执行此操作：

```c
                if (vlan_tx_tag_present(skb) &&
                    !vlan_hw_offload_capable(features, skb->vlan_proto)) {
                        skb = __vlan_put_tag(skb, skb->vlan_proto,
                                             vlan_tx_tag_get(skb));
                        if (unlikely(!skb))
                                goto out;

                        skb->vlan_tci = 0;
                }
```

然后，检查数据以确定这是不是 encapsulation （隧道封装）offload 请求，例如，
[GRE](https://en.wikipedia.org/wiki/Generic_Routing_Encapsulation)。 在这种情况
下，feature flags 将被更新，以添加任何特定于设备的硬件封装功能：

```c
                /* If encapsulation offload request, verify we are testing
                 * hardware encapsulation features instead of standard
                 * features for the netdev
                 */
                if (skb->encapsulation)
                        features &= dev->hw_enc_features;
```

接下来，`netif_needs_gso` 用于确定 skb 是否需要分片。 如果需要，但设备不支持，则
`netif_needs_gso` 将返回 `true`，表示分片应在软件中进行。 在这种情况下，调用
`dev_gso_segment` 进行分片，代码将跳转到 gso 以发送数据包。我们稍后会看到 GSO 路径。

```c
                if (netif_needs_gso(skb, features)) {
                        if (unlikely(dev_gso_segment(skb, features)))
                                goto out_kfree_skb;
                        if (skb->next)
                                goto gso;
                }
```

如果数据不需要分片，则处理一些其他情况。 首先，数据是否需要顺序化？ 也就是说，如
果数据分布在多个缓冲区中，设备是否支持发送网络数据，还是首先需要将它们组合成单个
有序缓冲区？ 绝大多数网卡不需要在发送之前将数据顺序化，因此在几乎所有情况下，
`skb_needs_linearize` 将为 `false` 然后被跳过。

```c
                                    else {
                        if (skb_needs_linearize(skb, features) &&
                            __skb_linearize(skb))
                                goto out_kfree_skb;
```

从接下来的一段注释我们可以了解到，下面的代码判断数据包是否仍然需要计算校验和。 如果设备不支持计算校验和，则在这里通过软件计算：

```c
                        /* If packet is not checksummed and device does not
                         * support checksumming for this protocol, complete
                         * checksumming here.
                         */
                        if (skb->ip_summed == CHECKSUM_PARTIAL) {
                                if (skb->encapsulation)
                                        skb_set_inner_transport_header(skb,
                                                skb_checksum_start_offset(skb));
                                else
                                        skb_set_transport_header(skb,
                                                skb_checksum_start_offset(skb));
                                if (!(features & NETIF_F_ALL_CSUM) &&
                                     skb_checksum_help(skb))
                                        goto out_kfree_skb;
                        }
                }
```

再往前，我们来到了 packet taps（tap 是包过滤器的安插点，例如抓包执行的地方）。回想
一下在[接收数据的文章
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#netifreceiveskbcore-special-box-delivers-data-to-packet-taps-and-protocol-layers)
中，我们看到了数据包是如何传递给 tap（如
[PCAP](http://www.tcpdump.org/manpages/pcap.3pcap.html)）的。 该函数中的下一个代
码块将要发送的数据包传递给 tap（如果有的话）：

```c
                if (!list_empty(&ptype_all))
                        dev_queue_xmit_nit(skb, dev);
```

最终，调用驱动的 `ops` 里面的发送回调函数 `ndo_start_xmit` 将数据包传给网卡设备：

```c
                skb_len = skb->len;
                rc = ops->ndo_start_xmit(skb, dev);

                trace_net_dev_xmit(skb, rc, dev, skb_len);
                if (rc == NETDEV_TX_OK)
                        txq_trans_update(txq);
                return rc;
        }
```

`ndo_start_xmit` 的返回值表示发送成功与否，并作为这个函数的返回值被返回给更上层。
我们看到了这个返回值将如何影响上层：数据可能会被此时的 qdisc 重新入队，因此
稍后尝试再次发送。

我们来看看 GSO 的 case。如果此函数的前面部分完成了分片，或者之前已经完成了分片但是
上次发送失败，则会进入下面的代码：

```c
gso:
        do {
                struct sk_buff *nskb = skb->next;

                skb->next = nskb->next;
                nskb->next = NULL;

                if (!list_empty(&ptype_all))
                        dev_queue_xmit_nit(nskb, dev);

                skb_len = nskb->len;
                rc = ops->ndo_start_xmit(nskb, dev);
                trace_net_dev_xmit(nskb, rc, dev, skb_len);
                if (unlikely(rc != NETDEV_TX_OK)) {
                        if (rc & ~NETDEV_TX_MASK)
                                goto out_kfree_gso_skb;
                        nskb->next = skb->next;
                        skb->next = nskb;
                        return rc;
                }
                txq_trans_update(txq);
                if (unlikely(netif_xmit_stopped(txq) && skb->next))
                        return NETDEV_TX_BUSY;
        } while (skb->next);
```

你可能已经猜到，此 `while` 循环会遍历分片生成的 skb 列表。

每个数据包将被：

* 传给包过滤器（tap，如果有的话）
* 通过 `ndo_start_xmit` 传递给驱动程序进行发送

设备驱动 `ndo_start_xmit()`返回错误时，会进行一些错误处理，并将错误返回给更上层。
未发送的 skbs 可能会被重新入队以便稍后再次发送。

该函数的最后一部分做一些清理工作，在上面发生错误时释放一些资源：

```c
out_kfree_gso_skb:
        if (likely(skb->next == NULL)) {
                skb->destructor = DEV_GSO_CB(skb)->destructor;
                consume_skb(skb);
                return rc;
        }
out_kfree_skb:
        kfree_skb(skb);
out:
        return rc;
}
EXPORT_SYMBOL_GPL(dev_hard_start_xmit);
```

在继续进入到设备驱动程序之前，先来看一些和前面分析过的代码有关的监控和调优的内容。

<a name="chap_8.6"></a>

## 8.6 Monitoring qdiscs

### Using the tc command line tool

使用 `tc` 工具监控 qdisc 统计：

```shell
$ tc -s qdisc show dev eth1
qdisc mq 0: root
 Sent 31973946891907 bytes 2298757402 pkt (dropped 0, overlimits 0 requeues 1776429)
 backlog 0b 0p requeues 1776429
```

网络设备的 qdisc 统计对于监控系统发送数据包的运行状况至关重要。你可以通过运行命令
行工具 tc 来查看状态。 上面的示例显示了如何检查 eth1 的统计信息。

* `bytes`: The number of bytes that were pushed down to the driver for transmit.
* `pkt`: The number of packets that were pushed down to the driver for transmit.
* `dropped`: The number of packets that were dropped by the qdisc. This can
  happen if transmit queue length is not large enough to fit the data being
  queued to it.
* `overlimits`: Depends on the queuing discipline, but can be either the number
  of packets that could not be enqueued due to a limit being hit, and/or the
  number of packets which triggered a throttling event when dequeued.
* `requeues`: Number of times dev_requeue_skb has been called to requeue an skb.
  Note that an skb which is requeued multiple times will bump this counter each
  time it is requeued.
* `backlog`: Number of bytes currently on the qdisc’s queue. This number is
  usually bumped each time a packet is enqueued.

一些 qdisc 还会导出额外的统计信息。每个 qdisc 都不同，对同一个 counter 可能会累积不同
的次数。你需要查看相应 qdisc 的源代码，弄清楚每个 counter 是在哪里、什么条件下被更新
的，如果这些数据对你非常重要，那你必须这么谨慎。

<a name="chap_8.7"></a>

## 8.7 Tuning qdiscs

### 调整`__qdisc_run` 处理权重

你可以调整前面看到的`__qdisc_run` 循环的权重（上面看到的 `quota` 变量），这将导致
`__netif_schedule` 更多的被调用执行。 结果将是当前 qdisc 将被更多的添加到当前 CPU 的
output_queue，最终会使发包所占的时间变多。

例如：调整所有 qdisc 的`__qdisc_run` 权重：

```shell
$ sudo sysctl -w net.core.dev_weight=600
```

### 增加发送队列长度

每个网络设备都有一个可以修改的 txqueuelen。 大多数 qdisc 在将数据插入到其发送队列之
前，会检查 txqueuelen 是否足够（表示的是字节数？）。 你可以调整这个参数以增加 qdisc
队列的字节数。

Example: increase the `txqueuelen` of `eth0` to `10000`.

```shell
$ sudo ifconfig eth0 txqueuelen 10000
```

默认值是 1000，你可以通过 ifconfig 命令的输出，查看每个网络设备的 txqueuelen。

<a name="chap_9"></a>

# 9 网络设备驱动

我们即将结束我们的网络栈之旅。

要理解数据包的发送过程，有一个重要的概念。大多数设备和驱动程序通过两个阶段处理数
据包发送：

1. 合理地组织数据，然后触发设备通过 DMA 从 RAM 中读取数据并将其发送到网络中
1. 发送完成后，设备发出中断，驱动程序解除映射缓冲区、释放内存或清除其状态

第二阶段通常称为“发送完成”（transmit completion）阶段。我们将对以上两阶段进行研
究，先从第一个开始：发送阶段。

之前已经看到，`dev_hard_start_xmit` 通过调用 `ndo_start_xmit`（保持一个锁）来发送
数据，所以接下来先看驱动程序是如何注册 `ndo_start_xmit` 的，然后再深入理解该函数的
工作原理。

与上篇[Linux 网络栈监控和调优：接收数据]({% link _posts/2018-12-05-tuning-stack-rx-zh.md %})
一样，我们将拿 `igb` 驱动作为例子。

<a name="chap_9.1"></a>

## 9.1 驱动回调函数注册

驱动程序实现了一系列方法来支持设备操作，例如：

1. 发送数据（`ndo_start_xmit`）
1. 获取统计信息（`ndo_get_stats64`）
1. 处理设备 `ioctl`s（`ndo_do_ioctl`）

这些方法通过一个 `struct net_device_ops` 实例导出。让我们来看看[igb 驱动程序
](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L1905-L1928)
中这些操作：

```c
static const struct net_device_ops igb_netdev_ops = {
        .ndo_open               = igb_open,
        .ndo_stop               = igb_close,
        .ndo_start_xmit         = igb_xmit_frame,
        .ndo_get_stats64        = igb_get_stats64,

                /* ... more fields ... */
};
```

这个 `igb_netdev_ops` 变量在
[`igb_probe`](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L2090)
函数中注册给设备：

```c
static int igb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
                /* ... lots of other stuff ... */

        netdev->netdev_ops = &igb_netdev_ops;

                /* ... more code ... */
}
```

正如我们在上一节中看到的，更上层的代码将通过设备的 `netdev_ops` 字段
调用适当的回调函数。想了解更多关于 PCI 设备是如何启动的，以及何时/何处调用
`igb_probe`，请查看我们之前文章中的[驱动程序初始化
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#initialization)
部分。

<a name="chap_9.2"></a>

## 9.2 通过 `ndo_start_xmit` 发送数据

上层的网络栈通过 `struct net_device_ops` 实例里的回调函数，调用驱动程序来执行各种
操作。正如我们之前看到的，qdisc 代码调用 `ndo_start_xmit` 将数据传递给驱动程序进行
发送。对于大多数硬件设备，都是在保持一个锁时调用 `ndo_start_xmit` 函数。

在 igb 设备驱动程序中，`ndo_start_xmit` 字段初始化为 `igb_xmit_frame` 函数，所以
我们接下来从 `igb_xmit_frame` 开始，查看该驱动程序是如何发送数据的。跟随
[drivers/net/ethernet/intel/igb/igb_main.c](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L4664-L4741)
，并记得以下代码在整个执行过程中都 hold 着一个锁：

```c
netdev_tx_t igb_xmit_frame_ring(struct sk_buff *skb,
                                struct igb_ring *tx_ring)
{
        struct igb_tx_buffer *first;
        int tso;
        u32 tx_flags = 0;
        u16 count = TXD_USE_COUNT(skb_headlen(skb));
        __be16 protocol = vlan_get_protocol(skb);
        u8 hdr_len = 0;

        /* need: 1 descriptor per page * PAGE_SIZE/IGB_MAX_DATA_PER_TXD,
         *       + 1 desc for skb_headlen/IGB_MAX_DATA_PER_TXD,
         *       + 2 desc gap to keep tail from touching head,
         *       + 1 desc for context descriptor,
         * otherwise try next time
         */
        if (NETDEV_FRAG_PAGE_MAX_SIZE > IGB_MAX_DATA_PER_TXD) {
                unsigned short f;
                for (f = 0; f < skb_shinfo(skb)->nr_frags; f++)
                        count += TXD_USE_COUNT(skb_shinfo(skb)->frags[f].size);
        } else {
                count += skb_shinfo(skb)->nr_frags;
        }
```

函数首先使用 `TXD_USER_COUNT` 宏来计算发送 skb 所需的描述符数量，用 `count`
变量表示。然后根据分片情况，对 `count` 进行相应调整。

```c
        if (igb_maybe_stop_tx(tx_ring, count + 3)) {
                /* this is a hard error */
                return NETDEV_TX_BUSY;
        }
```

然后驱动程序调用内部函数 `igb_maybe_stop_tx`，检查 TX Queue 以确保有足够可用的描
述符。如果没有，则返回 `NETDEV_TX_BUSY`。正如我们之前在 qdisc 代码中看到的那样，这
将导致 qdisc 将 skb 重新入队以便稍后重试。

```c
        /* record the location of the first descriptor for this packet */
        first = &tx_ring->tx_buffer_info[tx_ring->next_to_use];
        first->skb = skb;
        first->bytecount = skb->len;
        first->gso_segs = 1;
```

然后，获取 TX Queue 中下一个可用缓冲区信息，用 `struct igb_tx_buffer *first` 表
示，这个信息稍后将用于设置缓冲区描述符。数据包 `skb` 指针及其大小 `skb->len`
也存储到 `first`。

```c
        skb_tx_timestamp(skb);
```

接下来代码调用 `skb_tx_timestamp`，获取基于软件的发送时间戳。应用程序可以
使用发送时间戳来确定数据包通过网络栈的发送路径所花费的时间。

某些设备还支持硬件时间戳，这允许系统将打时间戳任务 offload 到设备。程序员因此可以
获得更准确的时间戳，因为它更接近于硬件实际发送的时间。

某些网络设备可以使用[Precision Time
Protocol](https://events.linuxfoundation.org/sites/events/files/slides/lcjp14_ichikawa_0.pdf)
（PTP，精确时间协议）在硬件中为数据包加时间戳。驱动程序处理用户的硬件时间戳请求。

我们现在看到这个代码：

```c
        if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
                struct igb_adapter *adapter = netdev_priv(tx_ring->netdev);

                if (!(adapter->ptp_tx_skb)) {
                        skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
                        tx_flags |= IGB_TX_FLAGS_TSTAMP;

                        adapter->ptp_tx_skb = skb_get(skb);
                        adapter->ptp_tx_start = jiffies;
                        if (adapter->hw.mac.type == e1000_82576)
                                schedule_work(&adapter->ptp_tx_work);
                }
        }
```

上面的 if 语句检查 `SKBTX_HW_TSTAMP` 标志，该标志表示用户请求了硬件时间戳。接下来检
查是否设置了 `ptp_tx_skb`。一次只能给一个数据包加时间戳，因此给正在打时间戳的 skb
上设置了 `SKBTX_IN_PROGRESS` 标志。然后更新 `tx_flags`，将 `IGB_TX_FLAGS_TSTAMP` 标志
置位。`tx_flags` 变量稍后将被复制到缓冲区信息结构中。

当前的 `jiffies` 值赋给 `ptp_tx_start`。驱动程序中的其他代码将使用这个值，
以确保 TX 硬件打时间戳不会 hang 住。最后，如果这是一个 82576 以太网硬件网卡，将用
`schedule_work` 函数启动[工作队列](http://www.makelinux.net/ldd3/chp-7-sect-6)。

```c
        if (vlan_tx_tag_present(skb)) {
                tx_flags |= IGB_TX_FLAGS_VLAN;
                tx_flags |= (vlan_tx_tag_get(skb) << IGB_TX_FLAGS_VLAN_SHIFT);
        }
```

上面的代码将检查 skb 的 `vlan_tci` 字段是否设置了，如果是，将设置 `IGB_TX_FLAGS_VLAN`
标记，并保存 VLAN ID。

```c
        /* record initial flags and protocol */
        first->tx_flags = tx_flags;
        first->protocol = protocol;
```

最后将 `tx_flags` 和 `protocol` 值都保存到 `first` 变量里面。

```c
        tso = igb_tso(tx_ring, first, &hdr_len);
        if (tso < 0)
                goto out_drop;
        else if (!tso)
                igb_tx_csum(tx_ring, first);
```

接下来，驱动程序调用其内部函数 `igb_tso`，判断 skb 是否需要分片。如果需要
，缓冲区信息变量（`first`）将更新标志位，以提示硬件需要做 TSO。

如果不需要 TSO，则 `igb_tso` 返回 0；否则返回 1。 如果返回 0，则将调用 `igb_tx_csum` 来
处理校验和 offload 信息（是否需要 offload，是否支持此协议的 offload）。
`igb_tx_csum` 函数将检查 skb 的属性，修改 `first` 变量中的一些标志位，以表示需要校验
和 offload。

```c
        igb_tx_map(tx_ring, first, hdr_len);
```

`igb_tx_map` 函数准备给设备发送的数据。我们后面会仔细查看这个函数。

```c
        /* Make sure there is space in the ring for the next send. */
        igb_maybe_stop_tx(tx_ring, DESC_NEEDED);

        return NETDEV_TX_OK;
```

发送结束之后，驱动要检查确保有足够的描述符用于下一次发送。如果不够，TX Queue 将被
关闭。最后返回 `NETDEV_TX_OK` 给上层（qdisc 代码）。

```c
out_drop:
        igb_unmap_and_free_tx_resource(tx_ring, first);

        return NETDEV_TX_OK;
}
```

最后是一些错误处理代码，只有当 `igb_tso` 遇到某种错误时才会触发此代码。
`igb_unmap_and_free_tx_resource` 用于清理数据。在这种情况下也返回 `NETDEV_TX_OK`
。发送没有成功，但驱动程序释放了相关资源，没有什么需要做的了。请注意，在这种情
况下，此驱动程序不会增加 drop 计数，但或许它应该增加。

<a name="chap_9.3"></a>

## 9.3 `igb_tx_map`

`igb_tx_map` 函数处理将 skb 数据映射到 RAM 的 DMA 区域的细节。它还会更新设备 TX Queue 的
尾部指针，从而触发设备“被唤醒”，从 RAM 获取数据并开始发送。

让我们简单地看一下这个[函数
](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L4501-L4627)
的工作原理：

```c
static void igb_tx_map(struct igb_ring *tx_ring,
                       struct igb_tx_buffer *first,
                       const u8 hdr_len)
{
        struct sk_buff *skb = first->skb;

                /* ... other variables ... */

        u32 tx_flags = first->tx_flags;
        u32 cmd_type = igb_tx_cmd_type(skb, tx_flags);
        u16 i = tx_ring->next_to_use;

        tx_desc = IGB_TX_DESC(tx_ring, i);

        igb_tx_olinfo_status(tx_ring, tx_desc, tx_flags, skb->len - hdr_len);

        size = skb_headlen(skb);
        data_len = skb->data_len;

        dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);
```

上面的代码所做的一些事情：

1. 声明变量并初始化
1. 使用 `IGB_TX_DESC` 获取下一个可用描述符的指针
1. `igb_tx_olinfo_status` 函数更新 `tx_flags`，并将它们复制到描述符（`tx_desc`）中
1. 计算 skb 头长度和数据长度
1. 调用 `dma_map_single` 为 `skb->data` 构造内存映射，以允许设备通过 DMA 从 RAM 中读取数据

接下来是驱动程序中的一个**非常长的循环，用于为 skb 的每个分片生成有效映射**。具体如何
做的细节并不是特别重要，但如下步骤值得一提：

* 驱动程序遍历该数据包的所有分片
* 当前描述符有其数据的 DMA 地址信息
* 如果分片的大小大于单个 IGB 描述符可以发送的大小，则构造多个描述符指向可 DMA 区域的块，直到描述符指向整个分片
* 更新描述符迭代器
* 更新剩余长度
* 当没有剩余分片或者已经消耗了整个数据长度时，循环终止

下面提供循环的代码以供以上描述参考。这里的代码进一步向读者说明，**如果可能的话，避
免分片是一个好主意**。分片需要大量额外的代码来处理网络栈的每一层，包括驱动层。

```c
        tx_buffer = first;

        for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
                if (dma_mapping_error(tx_ring->dev, dma))
                        goto dma_error;

                /* record length, and DMA address */
                dma_unmap_len_set(tx_buffer, len, size);
                dma_unmap_addr_set(tx_buffer, dma, dma);

                tx_desc->read.buffer_addr = cpu_to_le64(dma);

                while (unlikely(size > IGB_MAX_DATA_PER_TXD)) {
                        tx_desc->read.cmd_type_len =
                                cpu_to_le32(cmd_type ^ IGB_MAX_DATA_PER_TXD);

                        i++;
                        tx_desc++;
                        if (i == tx_ring->count) {
                                tx_desc = IGB_TX_DESC(tx_ring, 0);
                                i = 0;
                        }
                        tx_desc->read.olinfo_status = 0;

                        dma += IGB_MAX_DATA_PER_TXD;
                        size -= IGB_MAX_DATA_PER_TXD;

                        tx_desc->read.buffer_addr = cpu_to_le64(dma);
                }

                if (likely(!data_len))
                        break;

                tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type ^ size);

                i++;
                tx_desc++;
                if (i == tx_ring->count) {
                        tx_desc = IGB_TX_DESC(tx_ring, 0);
                        i = 0;
                }
                tx_desc->read.olinfo_status = 0;

                size = skb_frag_size(frag);
                data_len -= size;

                dma = skb_frag_dma_map(tx_ring->dev, frag, 0,
                                       size, DMA_TO_DEVICE);

                tx_buffer = &tx_ring->tx_buffer_info[i];
        }
```

所有需要的描述符都已建好，且 `skb` 的所有数据都映射到 DMA 地址后，驱动就会
进入到它的最后一步，触发一次发送：

```c
        /* write last descriptor with RS and EOP bits */
        cmd_type |= size | IGB_TXD_DCMD;
        tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type);
```

对最后一个描述符设置 `RS` 和 `EOP` 位，以提示设备这是最后一个描述符了。

```c
        netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

        /* set the timestamp */
        first->time_stamp = jiffies;
```

调用 `netdev_tx_sent_queue` 函数，同时带着将发送的字节数作为参数。这个函数是 byte
query limit（字节查询限制）功能的一部分，我们将在稍后详细介绍。当前的 jiffies 存
储到 `first` 的时间戳字段。

接下来，有点 tricky：

```c
        /* Force memory writes to complete before letting h/w know there
         * are new descriptors to fetch.  (Only applicable for weak-ordered
         * memory model archs, such as IA-64).
         *
         * We also need this memory barrier to make certain all of the
         * status bits have been updated before next_to_watch is written.
         */
        wmb();

        /* set next_to_watch value indicating a packet is present */
        first->next_to_watch = tx_desc;

        i++;
        if (i == tx_ring->count)
                i = 0;

        tx_ring->next_to_use = i;

        writel(i, tx_ring->tail);

        /* we need this if more than one processor can write to our tail
         * at a time, it synchronizes IO on IA64/Altix systems
         */
        mmiowb();

        return;
```

上面的代码做了一些重要的事情：

1. 调用 `wmb` 函数强制完成内存写入。这通常称作**“写屏障”**（write barrier）
   ，是通过 CPU 平台相关的特殊指令完成的。这对某些 CPU 架构非常重要，因为如果触发
   设备启动 DMA 时不能确保所有内存写入已经完成，那设备可能从 RAM 中读取不一致
   状态的数据。[这篇文章](http://preshing.com/20120930/weak-vs-strong-memory-models/)和[这个课程](http://www.cs.utexas.edu/~pingali/CS378/2012fa/lectures/consistency.pdf)深
   入探讨了内存顺序的细节
1. 设置 `next_to_watch` 字段，它将在 completion 阶段后期使用
1. 更新计数，并且 TX Queue 的 `next_to_use` 字段设置为下一个可用的描述符。使用
   `writel` 函数更新 TX Queue 的尾部。`writel` 向[内存映射 I/O](https://en.wikipedia.org/wiki/Memory-mapped_I/O)地址写入一个 `long` 型数据
   ，这里地址是 `tx_ring->tail`（一个硬件地址），要写入的值是 `i`。这次写操作会让
   设备知道其他数据已经准备好，可以通过 DMA 从 RAM 中读取并写入网络
1. 最后，调用 `mmiowb` 函数。它执行特定于 CPU 体系结构的指令，对内存映射的
   写操作进行排序。它也是一个写屏障，用于内存映射的 I/O 写

想了解更多关于 `wmb`，`mmiowb` 以及何时使用它们的信息，可以阅读 Linux 内核中一些包含
内存屏障的优秀[文档](https://github.com/torvalds/linux/blob/v3.13/Documentation/memory-barriers.txt)
。

最后，代码包含了一些错误处理。只有 DMA API（将 skb 数据地址映射到 DMA 地址）返回错误
时，才会执行此代码。

```c
dma_error:
        dev_err(tx_ring->dev, "TX DMA map failed\n");

        /* clear dma mappings for failed tx_buffer_info map */
        for (;;) {
                tx_buffer = &tx_ring->tx_buffer_info[i];
                igb_unmap_and_free_tx_resource(tx_ring, tx_buffer);
                if (tx_buffer == first)
                        break;
                if (i == 0)
                        i = tx_ring->count;
                i--;
        }

        tx_ring->next_to_use = i;
```

在继续跟进“发送完成”（transmit completion）过程之前，让我们来看下之前跳过了的一
个东西：dynamic queue limits（动态队列限制）。

### Dynamic Queue Limits (DQL)

正如在本文中看到的，**数据在逐步接近网络设备的过程中，花费了大量时间在
不同阶段的 Queue 里面**。队列越大，在队列中所花费的时间就越多。

解决这个问题的一种方式是**背压**（back pressure）。动态队列限制（DQL）系统是一种
机制，驱动程序可以使用该机制向网络系统（network system）施加反压，以避免设备
无法发送时有过多的数据积压在队列。

要使用 DQL，驱动需要在其发送和完成例程（transmit and completion routines）中调用
几次简单的 API。DQL 内部算法判断何时数据已足够多，达到此阈值后，DQL 将暂时禁用 TX
Queue，从而对网络系统产生背压。当足够的数据已发送完后，DQL 再自动重新启用
该队列。

[这里](https://www.linuxplumbersconf.org/2012/wp-content/uploads/2012/08/bql_slide.pdf)
给出了 DQL 的一些性能数据及 DQL 内部算法的说明。

我们刚刚看到的 `netdev_tx_sent_queue` 函数就是 DQL API 一部分。当数据排
队到设备进行发送时，将调用此函数。发送完成后，驱动程序调用
`netdev_tx_completed_queue`。在内部，这两个函数都将调用 DQL 库（在
[lib/dynamic_queue_limits.c](https://github.com/torvalds/linux/blob/v3.13/lib/dynamic_queue_limits.c)
和
[include/linux/dynamic_queue_limits.h](https://github.com/torvalds/linux/blob/v3.13/include/linux/dynamic_queue_limits.h)
），以判断是否禁用、重新启用 DQL，或保持配置不动。

DQL 在 sysfs 中导出了一些统计信息和调优参数。调整 DQL 不是必需的；算法自己会随着时间
变化调整其参数。尽管如此，为了完整性，我们稍后会看到如何监控和调整 DQL。

<a name="chap_9.4"></a>

## 9.4 发送完成（Transmit completions）

设备发送数据之后会产生一个中断，表示发送已完成。然后，设备驱动程序可以调度一些长
时间运行的工作，例如解除 DMA 映射、释放数据。这是如何工作的取决于不同设备。对于
`igb` 驱动程序（及其关联设备），发送完成和数据包接收所触发的 IRQ 是相同的。这意味着
对于 `igb` 驱动程序，`NET_RX` 既用于处理发送完成，又用于处理数据包接收。

让我重申一遍，以强调这一点的重要性：**你的设备可能会发出与“接收到数据包时触发的中
断”相同的中断来表示“数据包发送已完成”**。如果是这种情况，则 `NET_RX` softirq 会被用于
处理**数据包接收**和**发送完成**两种情况。

由于两个操作共享相同的 IRQ，因此只能注册一个 IRQ 处理函数来处理这两种情况。
回忆以下收到网络数据时的流程：

1. 收到网络数据
1. 网络设备触发 IRQ
1. 驱动的 IRQ 处理程序执行，清除 IRQ 并运行 softIRQ（如果尚未运行）。这里触发的 softIRQ 是 `NET_RX` 类型
1. softIRQ 本质上作为单独的内核线程，执行 NAPI 轮询循环
1. 只要有足够的预算，NAPI 轮询循环就一直接收数据包
1. 每次处理数据包后，预算都会减少，直到没有更多数据包要处理、预算达到 0 或时间片已过期为止

在 igb（和 ixgbe）驱动中，上面的步骤 5 在处理接收数据之前会先处理发送完成（TX
completion）。请记住，**根据驱动程序的实现，处理发送完成和接收数据的函数可能共享一
份处理预算**。igb 和 ixgbe 驱动程序分别跟踪发送完成和接收数据包的预算，因此处理发送完
成不一定会消耗完 RX 预算。

也就是说，整个 NAPI 轮询循环在 hard code 时间片内运行。这意味着如果要处理大量的 TX 完成
，TX 完成可能会比处理接收数据时占用更多的时间片。对于在高负载环境中运行网络硬
件的人来说，这可能是一个重要的考虑因素。

让我们看看 igb 驱动程序在实际是如何实现的。

### 9.4.1 Transmit completion IRQ

收包过程我们已经在[数据接收部分的博客]({% link _posts/2018-12-05-tuning-stack-rx-zh.md %})
中介绍过，这里不再赘述，只给出相应链接。

那么，让我们从头开始：

1. 网络设备[启用](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#bringing-a-network-device-up)（bring up）
1. IRQ 处理函数完成[注册](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#register-an-interrupt-handler)
1. 用户程序将数据发送到 socket。数据穿过网络栈，最后被网络设备从内存中取出并发送
1. 设备完成数据发送并触发 IRQ 表示发送完成
1. 驱动程序的 IRQ 处理函数开始[处理中断](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#interrupt-handler)
1. IRQ 处理程序调用 `napi_schedule`
1. [NAPI 代码](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#napi-and-napischedule)触发 `NET_RX` 类型 softirq
1. `NET_RX` 类型 sofitrq 的中断处理函数 `net_rx_action`[开始执行](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#network-data-processing-begins)
1. `net_rx_action` 函数调用驱动程序注册的[NAPI 轮询函数](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#napi-poll-function-and-weight)
1. NAPI 轮询函数 `igb_poll`[开始运行](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#igbpoll)

poll 函数 `igb_poll` 同时处理接收数据包和发送完成（transmit completion）逻辑。让我
们深入研究这个函数的代码，看看发生了什么。

### 9.4.2 `igb_poll`

[drivers/net/ethernet/intel/igb/igb_main.c](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L5987-L6018):

```c
/**
 *  igb_poll - NAPI Rx polling callback
 *  @napi: napi polling structure
 *  @budget: count of how many packets we should handle
 **/
static int igb_poll(struct napi_struct *napi, int budget)
{
        struct igb_q_vector *q_vector = container_of(napi,
                                                     struct igb_q_vector,
                                                     napi);
        bool clean_complete = true;

#ifdef CONFIG_IGB_DCA
        if (q_vector->adapter->flags & IGB_FLAG_DCA_ENABLED)
                igb_update_dca(q_vector);
#endif
        if (q_vector->tx.ring)
                clean_complete = igb_clean_tx_irq(q_vector);

        if (q_vector->rx.ring)
                clean_complete &= igb_clean_rx_irq(q_vector, budget);

        /* If all work not completed, return budget and keep polling */
        if (!clean_complete)
                return budget;

        /* If not enough Rx work done, exit the polling mode */
        napi_complete(napi);
        igb_ring_irq_enable(q_vector);

        return 0;
}
```

函数按顺序执行以下操作：

1. 如果在内核中启用了直接缓存访问（[DCA](https://lwn.net/Articles/247493/)）功能
   ，则更新 CPU 缓存（预热，warm up），后续对 RX Ring Buffer 的访问将命中 CPU 缓存。可以在接
   收数据博客的 Extras 部分中阅读[有关 DCA 的更多信息](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#direct-cache-access-dca)
1. 调用 `igb_clean_tx_irq` 执行发送完成操作
1. 调用 `igb_clean_rx_irq` 处理收到的数据包
1. 最后，检查 `clean_complete` 变量，判断是否还有更多工作可以完成。如果是，则返
   回预算。如果是这种情况，`net_rx_action` 会将此 NAPI 实例移动到轮询列表的末尾，
   以便稍后再次处理

要了解 `igb_clean_rx_irq` 如何工作的，请阅读上一篇博客文章的[这一部分
](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#igbcleanrxirq)
。

本文主要关注发送方面，因此我们将继续研究上面的 `igb_clean_tx_irq` 如何工作。

### 9.4.3 `igb_clean_tx_irq`

来看一下这个函数的实现，
[drivers/net/ethernet/intel/igb/igb_main.c](https://github.com/torvalds/linux/blob/v3.13/drivers/net/ethernet/intel/igb/igb_main.c#L6020-L6189)。

这个函数有点长，分成几部分来看：

```c
static bool igb_clean_tx_irq(struct igb_q_vector *q_vector)
{
        struct igb_adapter *adapter = q_vector->adapter;
        struct igb_ring *tx_ring = q_vector->tx.ring;
        struct igb_tx_buffer *tx_buffer;
        union e1000_adv_tx_desc *tx_desc;
        unsigned int total_bytes = 0, total_packets = 0;
        unsigned int budget = q_vector->tx.work_limit;
        unsigned int i = tx_ring->next_to_clean;

        if (test_bit(__IGB_DOWN, &adapter->state))
                return true;
```

该函数首先初始化一些变量，其中比较重要的是预算（变量 `budget`）
，初始化为此队列的 `tx.work_limit`。在 igb 驱动程序中，`tx.work_limit` 初始化为 hard
code 值 `IGB_DEFAULT_TX_WORK`（128）。

值得注意的是，虽然我们现在看到的 TX 完成代码与 RX 处理在同一个 `NET_RX` softirq 中运行
，但 igb 驱动的 TX 和 RX 函数**不共享处理预算**。由于整个轮询函数在同一时间片内运行，因此
每次 `igb_poll` 运行不会出现 RX 或 TX 饥饿，只要调用 `igb_poll`，两者都将被处理。

继续前进，代码检查网络设备是否已关闭。如果是，则返回 `true` 并退出 `igb_clean_tx_irq`。

```c
        tx_buffer = &tx_ring->tx_buffer_info[i];
        tx_desc = IGB_TX_DESC(tx_ring, i);
        i -= tx_ring->count;
```

接下来：

1. `tx_buffer` 变量初始化为 `tx_ring->next_to_clean`（其本身被初始化为 0）
1. `tx_desc` 变量初始化为相关描述符的指针
1. 计数器 `i` 减去 TX Queue 的大小。可以调整此值（我们将在调优部分中看到），但初始化为 `IGB_DEFAULT_TXD`（256）

接下来，循环开始。它包含一些有用的注释，用于解释每个步骤中发生的情况：

```c
        do {
                union e1000_adv_tx_desc *eop_desc = tx_buffer->next_to_watch;

                /* if next_to_watch is not set then there is no work pending */
                if (!eop_desc)
                        break;

                /* prevent any other reads prior to eop_desc */
                read_barrier_depends();

                /* if DD is not set pending work has not been completed */
                if (!(eop_desc->wb.status & cpu_to_le32(E1000_TXD_STAT_DD)))
                        break;

                /* clear next_to_watch to prevent false hangs */
                tx_buffer->next_to_watch = NULL;

                /* update the statistics for this packet */
                total_bytes += tx_buffer->bytecount;
                total_packets += tx_buffer->gso_segs;

                /* free the skb */
                dev_kfree_skb_any(tx_buffer->skb);

                /* unmap skb header data */
                dma_unmap_single(tx_ring->dev,
                                 dma_unmap_addr(tx_buffer, dma),
                                 dma_unmap_len(tx_buffer, len),
                                 DMA_TO_DEVICE);

                /* clear tx_buffer data */
                tx_buffer->skb = NULL;
                dma_unmap_len_set(tx_buffer, len, 0);
```

1. 首先将 `eop_desc`（eop = end of packet）设置为发送缓冲区 `tx_buffer` 的 `next_to_watch`，后者是在我们之前看到的发送代码中设置的
1. 如果 `eop_desc` 为 `NULL`，则表示没有待处理的工作
1. 接下来调用 `read_barrier_depends` 函数，该函数执行此 CPU 体系结构相关的指令，通过屏障防止其他任何读操作
1. 接下来，检查描述符 `eop_desc` 的状态位。如果 `E1000_TXD_STAT_DD` 未设置，则表示发送尚未完成，因此跳出循环
1. 清除 `tx_buffer->next_to_watch`。驱动中的 watchdog 定时器将监视此字段以判断发送是否 hang 住。清除此字段将不会触发 watchdog
1. 统计发送的总字节数和包数，这些计数将被复制到驱动的相应计数中
1. 释放 skb
1. 调用 `dma_unmap_single` 取消 skb 数据区映射
1. `tx_buffer->skb` 设置为 `NULL`，解除 `tx_buffer` 映射

接下来，在上面的循环内部开始了另一个循环：

```c
                /* clear last DMA location and unmap remaining buffers */
                while (tx_desc != eop_desc) {
                        tx_buffer++;
                        tx_desc++;
                        i++;
                        if (unlikely(!i)) {
                                i -= tx_ring->count;
                                tx_buffer = tx_ring->tx_buffer_info;
                                tx_desc = IGB_TX_DESC(tx_ring, 0);
                        }

                        /* unmap any remaining paged data */
                        if (dma_unmap_len(tx_buffer, len)) {
                                dma_unmap_page(tx_ring->dev,
                                               dma_unmap_addr(tx_buffer, dma),
                                               dma_unmap_len(tx_buffer, len),
                                               DMA_TO_DEVICE);
                                dma_unmap_len_set(tx_buffer, len, 0);
                        }
                }
```

这个内层循环会遍历每个发送描述符，直到 `tx_desc` 等于 `eop_desc`，并会解除被其他描
述符引用的被 DMA 映射的数据。

外层循环继续：

```c
                /* move us one more past the eop_desc for start of next pkt */
                tx_buffer++;
                tx_desc++;
                i++;
                if (unlikely(!i)) {
                        i -= tx_ring->count;
                        tx_buffer = tx_ring->tx_buffer_info;
                        tx_desc = IGB_TX_DESC(tx_ring, 0);
                }

                /* issue prefetch for next Tx descriptor */
                prefetch(tx_desc);

                /* update budget accounting */
                budget--;
        } while (likely(budget));
```

外层循环递增迭代器，更新 budget，然后检查是否要进入下一次循环。

```c
        netdev_tx_completed_queue(txring_txq(tx_ring),
                                  total_packets, total_bytes);
        i += tx_ring->count;
        tx_ring->next_to_clean = i;
        u64_stats_update_begin(&tx_ring->tx_syncp);
        tx_ring->tx_stats.bytes += total_bytes;
        tx_ring->tx_stats.packets += total_packets;
        u64_stats_update_end(&tx_ring->tx_syncp);
        q_vector->tx.total_bytes += total_bytes;
        q_vector->tx.total_packets += total_packets;
```

这段代码：

1. 调用 `netdev_tx_completed_queue`，它是上面解释的 DQL API 的一部分。如果处理了足够的发送完成，这可能会重新启用 TX Queue
1. 更新各处的统计信息，以便用户可以访问它们，我们稍后会看到

代码继续，首先检查是否设置了 `IGB_RING_FLAG_TX_DETECT_HANG` 标志。每次运行定时器
回调函数时，watchdog 定时器都会设置此标志，以强制定期检查 TX Queue。如果该标志被设
置了，则代码将检查 TX Queue 是否 hang 住：

```c
        if (test_bit(IGB_RING_FLAG_TX_DETECT_HANG, &tx_ring->flags)) {
                struct e1000_hw *hw = &adapter->hw;

                /* Detect a transmit hang in hardware, this serializes the
                 * check with the clearing of time_stamp and movement of i
                 */
                clear_bit(IGB_RING_FLAG_TX_DETECT_HANG, &tx_ring->flags);
                if (tx_buffer->next_to_watch &&
                    time_after(jiffies, tx_buffer->time_stamp +
                               (adapter->tx_timeout_factor * HZ)) &&
                    !(rd32(E1000_STATUS) & E1000_STATUS_TXOFF)) {

                        /* detected Tx unit hang */
                        dev_err(tx_ring->dev,
                                "Detected Tx Unit Hang\n"
                                "  Tx Queue             <%d>\n"
                                "  TDH                  <%x>\n"
                                "  TDT                  <%x>\n"
                                "  next_to_use          <%x>\n"
                                "  next_to_clean        <%x>\n"
                                "buffer_info[next_to_clean]\n"
                                "  time_stamp           <%lx>\n"
                                "  next_to_watch        <%p>\n"
                                "  jiffies              <%lx>\n"
                                "  desc.status          <%x>\n",
                                tx_ring->queue_index,
                                rd32(E1000_TDH(tx_ring->reg_idx)),
                                readl(tx_ring->tail),
                                tx_ring->next_to_use,
                                tx_ring->next_to_clean,
                                tx_buffer->time_stamp,
                                tx_buffer->next_to_watch,
                                jiffies,
                                tx_buffer->next_to_watch->wb.status);
                        netif_stop_subqueue(tx_ring->netdev,
                                            tx_ring->queue_index);

                        /* we are about to reset, no point in enabling stuff */
                        return true;
                }
```

上面的 if 语句检查：

1. `tx_buffer->next_to_watch` 已设置，并且
1. 当前 jiffies 大于 `tx_buffer` 发送路径上记录的 `time_stamp` 加上超时因子，并且
1. 设备的发送状态寄存器未设置 `E1000_STATUS_TXOFF`

如果这三个条件都为真，则会打印一个错误，表明已检测到挂起。`netif_stop_subqueue`
用于关闭队列，最后函数返回 true。

让我们继续阅读代码，看看如果没有发送挂起检查会发生什么，或者如果有，但没有检测到
挂起：

```c
#define TX_WAKE_THRESHOLD (DESC_NEEDED * 2)
        if (unlikely(total_packets &&
            netif_carrier_ok(tx_ring->netdev) &&
            igb_desc_unused(tx_ring) >= TX_WAKE_THRESHOLD)) {
                /* Make sure that anybody stopping the queue after this
                 * sees the new next_to_clean.
                 */
                smp_mb();
                if (__netif_subqueue_stopped(tx_ring->netdev,
                                             tx_ring->queue_index) &&
                    !(test_bit(__IGB_DOWN, &adapter->state))) {
                        netif_wake_subqueue(tx_ring->netdev,
                                            tx_ring->queue_index);

                        u64_stats_update_begin(&tx_ring->tx_syncp);
                        tx_ring->tx_stats.restart_queue++;
                        u64_stats_update_end(&tx_ring->tx_syncp);
                }
        }

        return !!budget;
```

在上面的代码中，如果先前已禁用，则驱动程序将重新启动 TX Queue。
它首先检查：

1. 是否有数据包处理完成（`total_packets` 非零）
1. 调用 `netif_carrier_ok`，确保设备没有被关闭
1. TX Queue 中未使用的描述符数量大于等于 `TX_WAKE_THRESHOLD`（我的 x86_64 系统上此阈值为 42）

如果满足以上所有条件，则执行**写屏障**（`smp_mb`）。

接下来检查另一组条件。如果：

1. 队列停止了
1. 设备未关闭

则调用 `netif_wake_subqueue` 唤醒 TX Queue，并向更高层发信号通知它们可能需要将数据
再次入队。`restart_queue` 统计计数器递增。我们接下来会看到如何阅读这个值。

最后，返回一个布尔值。如果有任何剩余的未使用预算，则返回 true，否则为 false。在
`igb_poll` 中检查此值以确定返回 `net_rx_action` 的内容。

### 9.4.4 `igb_poll` 返回值

`igb_poll` 函数通过以下逻辑决定返回什么值给 `net_rx_action`：

```c
        if (q_vector->tx.ring)
                clean_complete = igb_clean_tx_irq(q_vector);

        if (q_vector->rx.ring)
                clean_complete &= igb_clean_rx_irq(q_vector, budget);

        /* If all work not completed, return budget and keep polling */
        if (!clean_complete)
                return budget;
```

换句话说，如果：

1. `igb_clean_tx_irq` 清除了所有**待发送**数据包，且未用完其 TX 预算（transmit
   completion budget），并且
1. `igb_clean_rx_irq` 清除了所有**接收到的**数据包，且未用完其 RX 预算（packet
   processing budget）

那么，最后将返回整个预算值（包括 igb 在内的大多数驱动程序 hard code 为 64）；否则，如果
RX 或 TX 处理中的任何用完了其 budget（因为还有更多工作要做），则调用 `napi_complete`
禁用 NAPI 并返回 0：

```c
        /* If not enough Rx work done, exit the polling mode */
        napi_complete(napi);
        igb_ring_irq_enable(q_vector);

        return 0;
}
```

<a name="chap_9.5"></a>

## 9.5 监控网络设备

监控网络设备有多种方式，每种方式提供的监控粒度和复杂度各不相同。我们先从最粗
大粒度开始，然后逐步到最细的粒度。

### 9.5.1 使用 `ethtool -S` 命令

Ubuntu 安装 ethtool：

```shell
$ sudo apt-get install ethtool.
```

`ethtool -S <NIC>`可以打印设备的收发统计信息（例如，发送错误）：

```shell
$ sudo ethtool -S eth0
NIC statistics:
     rx_packets: 597028087
     tx_packets: 5924278060
     rx_bytes: 112643393747
     tx_bytes: 990080156714
     rx_broadcast: 96
     tx_broadcast: 116
     rx_multicast: 20294528
     ....
```

监控这个数据不是太容易，因为并无统一的标准规定`-S` 应该打印出哪些字段。不同的设备
，甚至是相同设备的不同版本，都可能打印出名字不同但意思相同的字段。

你首先需要检查里面的“drop”、“buffer”、“miss”、“errors”等字段，然后查看驱动程序的
代码，以确定哪些计数是在软件里更新的（例如，内存不足时更新），哪些是直接来自硬件
寄存器更新的。如果是硬件寄存器值，那你需要查看网卡的 data sheet，确定这个计数真正
表示什么，因为 ethtool 给出的很多字段都是有误导性的（misleading）。

### 9.5.2 使用 `sysfs`

sysfs 也提供了很多统计值，但比网卡层的统计更上层一些。

例如，你可以通过 `cat <file>`的方式查看 eth0 接收的丢包数。

示例：

```shell
$ cat /sys/class/net/eth0/statistics/tx_aborted_errors
2
```

每个 counter 对应一个文件，包括 `tx_aborted_errors`, `tx_carrier_errors`, `tx_compressed`, `tx_dropped`,等等。

**不幸的是，每个值代表什么是由驱动决定的，因此，什么时候更新它们，在什么条件下更新
，都是驱动决定的。**例如，你可能已经注意到，对于同一种错误，有的驱动将其视为 drop
，而有的驱动将其视为 miss。

如果这些值对你非常重要，那你必须阅读驱动代码和网卡 data sheet，以确定每个值真正代
表什么。

### 9.5.3 使用`/proc/net/dev`

`/proc/net/dev` 提供了更高一层的统计，它给系统中的每个网络设备一个统计摘要。

```shell
$ cat /proc/net/dev
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
  eth0: 110346752214 597737500    0    2    0     0          0  20963860 990024805984 6066582604    0    0    0     0       0          0
    lo: 428349463836 1579868535    0    0    0     0          0         0 428349463836 1579868535    0    0    0     0       0          0
```

这里打印出来的字段是上面 sysfs 里字段的一个子集，可以作为通用 general reference。

上面的建议在这里同样适用，即：
如果这些值对你非常重要，那你必须阅读驱动代码和网卡 data sheet，以确定每个值真正代
表什么。

<a name="chap_9.6"></a>

## 9.6 监控 DQL

可以通过`/sys/class/net/<NIC>/queues/tx-<QUEUE_ID>/byte_queue_limits/`
监控网络设备的动态队列限制（DQL）信息。

```shell
$ cat /sys/class/net/eth0/queues/tx-0/byte_queue_limits/inflight
350
```

文件包括：

1. `hold_time`: Initialized to HZ (a single hertz). If the queue has been full for hold_time, then the maximum size is decreased.
1. `inflight`: This value is equal to (number of packets queued - number of packets completed). It is the current number of packets being transmit for which a completion has not been processed.
1. `limit_max`: A hardcoded value, set to DQL_MAX_LIMIT (1879048192 on my x86_64 system).
1. `limit_min`: A hardcoded value, set to 0.
1. `limit`: A value between limit_min and limit_max which represents the current maximum number of objects which can be queued.

在修改这些值之前，强烈建议先阅读[这些资料
](https://www.linuxplumbersconf.org/2012/wp-content/uploads/2012/08/bql_slide.pdf)，以更深入地了解其算法。

<a name="chap_9.7"></a>

## 9.7 调优网络设备

### 9.7.1 查询 TX Queue 数量

如果网络及其驱动支持多 TX Queue，那可以用 ethtool 调整 TX queue（也叫 TX channel）的数量。

查看网卡 TX Queue 数量：

```shell
$ sudo ethtool -l eth0
Channel parameters for eth0:
Pre-set maximums:
RX:   0
TX:   0
Other:    0
Combined: 8
Current hardware settings:
RX:   0
TX:   0
Other:    0
Combined: 4
```

这里显示了（由驱动和硬件）预设的最大值，以及当前值。

注意：不是所有设备驱动都支持这个选项。

如果你的网卡不支持，会遇到以下错误：

```shell
$ sudo ethtool -l eth0
Channel parameters for eth0:
Cannot get device channel parameters
: Operation not supported
```

这表示设备驱动没有实现 ethtool 的 `get_channels` 方法，这可能是由于网卡不支持调整
queue 数量，不支持多 TX Queue，或者驱动版本太旧导致不支持此操作。

### 9.7.2 调整 TX queue 数量

`ethtool -L` 可以修改 TX Queue 数量。

注意：一些设备及其驱动只支持 combined queue，这种情况下一个 TX queue 和和一个 RX queue 绑定到一起的。前面的例子中我们已经看到了。

例子：设置收发队列数量为 8：

```shell
$ sudo ethtool -L eth0 combined 8
```

如果你的设备和驱动支持分别设置 TX queue 和 RX queue 的数量，那你可以分别设置。

```shell
$ sudo ethtool -L eth0 tx 8
```

注意：对于大部分驱动，调整以上设置会导致网卡先 down 再 up，经过这个网卡的连接会断掉
。如果只是一次性改动，那这可能不是太大问题。

### 9.7.3 调整 TX queue 大小

一些设备及其驱动支持修改 TX queue 大小，这是如何实现的取决于具体的硬件，但是，
ethtool 提供了一个通用的接口可以调整这个大小。由于 DQL 在更高层面处理数据排队的问题
，因此调整队列大小可能不会产生明显的影响。然而，你可能还是想要将 TX queue 调到最大
，然后再把剩下的事情交给 DQL：

`ethtool -g` 查看队列当前的大小：

```shell
$ sudo ethtool -g eth0
Ring parameters for eth0:
Pre-set maximums:
RX:   4096
RX Mini:  0
RX Jumbo: 0
TX:   4096
Current hardware settings:
RX:   512
RX Mini:  0
RX Jumbo: 0
TX:   512
```

以上显示硬件支持最大 4096 个接收和发送描述符，但当前只使用了 512 个。

`-G` 修改 queue 大小：

```shell
$ sudo ethtool -G eth0 tx 4096
```

注意：对于大部分驱动，调整以上设置会导致网卡先 down 再 up，经过这个网卡的连接会断掉
。如果只是一次性改动，那这可能不是太大问题。

<a name="chap_10"></a>

# 10 网络栈之旅：结束

至此，你已经知道关于 Linux 如何发送数据包的全部内容了：从用户程序直到驱动，以及反
方向。

<a name="chap_11"></a>

# 11 Extras

<a name="chap_11.1"></a>

## 11.1 减少 ARP 流量 (MSG_CONFIRM)

`send`, `sendto` 和 `sendmsg` 系统调用都支持一个 `flags` 参数。如果你调用的时候传递了
`MSG_CONFIRM` flag，它会使内核里的 `dst_neigh_output` 函数更新邻居（ARP）缓存的时
间戳。所导致的结果是，相应的邻居缓存不会被垃圾回收。这会减少发出的 ARP 请求的数量
。

<a name="chap_11.2"></a>

## 11.2 UDP Corking（软木塞）

在查看 UDP 协议栈的时候我们深入地研究过了 UDP corking 这个选项。如果你想在应用中使用
这个选项，可以在调用 `setsockopt` 设置 IPPROTO_UDP 类型 socket 的时候，将 UDP_CORK 标记
位置 1。

<a name="chap_11.3"></a>

## 11.3 打时间戳

本文已经看到，网络栈可以收集发送包的时间戳信息。我们在文章中已经看到了软
件部分哪里可以设置时间戳；而一些网卡甚至还支持硬件时间戳。

如果你想看内核网络栈给收包增加了多少延迟，那这个特性非常有用。

内核[关于时间戳的文档](https://github.com/torvalds/linux/blob/v3.13/Documentation/networking/timestamping.txt)
非常优秀，甚至还包括一个[示例程序和相应的 Makefile](https://github.com/torvalds/linux/tree/v3.13/Documentation/networking/timestamping)，有兴趣的话可以上手试试。

使用 `ethtool -T` 可以查看网卡和驱动支持哪种打时间戳方式：

```shell
$ sudo ethtool -T eth0
Time stamping parameters for eth0:
Capabilities:
  software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
  software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
  software-system-clock (SOF_TIMESTAMPING_SOFTWARE)
PTP Hardware Clock: none
Hardware Transmit Timestamp Modes: none
Hardware Receive Filter Modes: none
```

从上面这个信息看，该网卡不支持硬件打时间戳。但这个系统上的软件打时间戳，仍然可以
帮助我判断内核在接收路径上到底带来多少延迟。

<a name="chap_12"></a>

# 12 结论

Linux 网络栈很复杂。

我们已经看到，即使是 `NET_RX` 这样看起来极其简单的（名字），也不是按照我们（字面上
）理解的方式在运行，虽然名字带 RX，但其实发送数据也在 `NET_RX` 软中断处理函数中被处
理。

这揭示了我认为的问题的核心：**不深入阅读和理解网络栈，就不可能优化和监控它**。
**你监控不了你没有深入理解的代码**。

<a name="chap_13"></a>

# 13 额外帮助

需要一些额外的关于网络栈的指导(navigating the network stack)？对本文有疑问，或有
相关内容本文没有提到？以上问题，都可以发邮件给[我们](support@packagecloud.io)，
以便我们知道如何提供帮助。