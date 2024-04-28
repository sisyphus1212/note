---
title: dropwatch 监控内核的网络栈丢包
date: 2020-11-01 18:21:45
categories:
- [linux,网络开发, 驱动, udev]
tags: 网络网络协议栈
---
# dropwatch 监控内核的网络栈丢包

## dropwatch 的功能

dropwatch 功能可以用来监控内核的网络栈丢弃的数据包。

接收的数据包在内核中被丢弃，很多时候并不会在日志中记录，一般难以发现。

## 启用内核 dropwatch 功能

dropwatch 功能需要开启 CONFIG_NET_DROP_MONITOR 配置，在我的虚机中，相关的配置信息如下：

```
lcj@debian:~$ grep 'NET_DROP_MONITOR' /boot/config-4.19.0-8-amd64
CONFIG_NET_DROP_MONITOR=m
```

## 加载 dropwatch 相关的内核模块

可以看到，CONFIG_NET_DROP_MONITOR 被编译为了独立的内核模块。我在 /lib/modules 中搜索，找到了名为 drop_monitor.ko 的内核模块，这个就是 dropwatch 的内核模块。这之后我执行 modprobe 进行加载，并在加载完成后检索 lsmod 的结果，确认模块加载成功。

操作记录如下：

```
lcj@debian:~$ find /lib/modules/4.19.0-8-amd64/ -name '*monitor.ko'
/lib/modules/4.19.0-8-amd64/kernel/net/core/drop_monitor.ko
lcj@debian:~$ sudo modprobe drop_monitor
lcj@debian:~$ lsmod | grep 'drop_monitor'
drop_monitor           20480  0
```

dropwatch 是使用 Kernel Tracepoint API 安装的，在各协议释放 socket 缓冲区时收集信息。

## 手动编译 dropwatch

下载到源码后，在源码中首先执行如下命令：

```
./autogen.sh
```

执行后报错信息如下：

```
configure.ac:16: error: possibly undefined macro: AC_MSG_ERROR
      If this token and others are legitimate, please use m4_pattern_allow.
      See the Autoconf documentation.
configure.ac:20: error: possibly undefined macro: AC_CHECK_LIB
```

网上搜索找到了如下链接：

[possibly-undefined-macro-ac-msg-error](https://stackoverflow.com/questions/8811381/possibly-undefined-macro-ac-msg-error)

从其中的一个回答中看出可能是没有安装 pkg-config 命令造成的问题。执行如下命令安装 pkg-config 命令。

```
sudo apt-get  --fix-broken install pkg-config
```

安装完成后，重新执行 autogen.sh 脚本，这次能够成功执行了。

### ./configure 报错

执行 ./configure 有如下报错：

```
configure: error: libpcap is required
```

执行如下命令安装 libpcap-dev 解决这个问题。

```
sudo apt-get install libpcap-dev
```

#### Couldn't find or include bfd.h

重新执行 ./configure 又报了新的错误，错误内容如下：

```
checking bfd.h usability... no
checking bfd.h presence... no
checking for bfd.h... no
configure: error: Couldn't find or include bfd.h
```

这应该是需要安装某个库的头文件。我执行 apt-cache search 搜索相关的内容，并根据经验选择安装 libbsd-dev 函数库，这之后发现还有问题。操作记录如下：

```
lcj@debian:~/dropwatch-1.5.3$ sudo apt-cache search bsd | grep libbsd
libbsd-dev - utility functions from BSD systems - development files
libbsd0 - utility functions from BSD systems - shared library
libbsd-arc4random-perl - CPAN's BSD::arc4random -- Perl bindings for arc4random
libbsd-resource-perl - BSD process resource limit and priority functions
lcj@debian:~/dropwatch-1.5.3$
lcj@debian:~/dropwatch-1.5.3$ sudo apt-get install libbsd-dev
lcj@debian:~/dropwatch-1.5.3$
```

网上搜索找到了如下链接：[compile errors using bfd.h on linux](https://stackoverflow.com/questions/8003763/compile-errors-using-bfd-h-on-linux).

执行如下命令安装 binutils-dev 组件：

```
sudo apt-get install binutils-dev
```

安装成功后 ./configure 命令成功执行。

### make 操作报错

这之后执行 make 命令时又有新的报错，报错信息如下：

```
main.c:23:10: fatal error: readline/readline.h: No such file or directory
 #include <readline/readline.h>
```

执行如下命令解决这个问题：

```
lcj@debian:~/dropwatch-1.5.3$ sudo apt-get install libreadline-dev
```

再次 make 又报了如下错误：

```
/usr/bin/ld: cannot find -lnl-genl-3
```

执行如下命令可以解决这个问题：

```
lcj@debian:~/dropwatch-1.5.3$ sudo apt-get install libnl-genl-3-dev
```

### 成功编译

```
lcj@debian:~/dropwatch-1.5.3$ make
make  all-recursive
make[1]: Entering directory '/home/lcj/dropwatch-1.5.3'
Making all in src
make[2]: Entering directory '/home/lcj/dropwatch-1.5.3/src'
/usr/bin/bash ../libtool  --tag=CC   --mode=link gcc -g -Wall -Werror -I/usr/include/libnl3  -g -O2 -lnl-3 -lnl-genl-3 -lreadline -lpcap -lbfd  -o dropwatch main.o lookup.o lookup_kas.o lookup_bfd.o  -lpcap
libtool: link: gcc -g -Wall -Werror -I/usr/include/libnl3 -g -O2 -o dropwatch main.o lookup.o lookup_kas.o lookup_bfd.o  -lnl-3 -lnl-genl-3 -lreadline -lbfd -lpcap
/usr/bin/bash ../libtool  --tag=CC   --mode=link gcc -g -Wall -Werror -I/usr/include/libnl3  -g -O2 -lnl-3 -lnl-genl-3 -lreadline -lpcap -lbfd  -o dwdump dwdump.o  -lpcap
libtool: link: gcc -g -Wall -Werror -I/usr/include/libnl3 -g -O2 -o dwdump dwdump.o  -lnl-3 -lnl-genl-3 -lreadline -lbfd -lpcap
make[2]: Leaving directory '/home/lcj/dropwatch-1.5.3/src'
Making all in doc
make[2]: Entering directory '/home/lcj/dropwatch-1.5.3/doc'
make[2]: Nothing to be done for 'all'.
make[2]: Leaving directory '/home/lcj/dropwatch-1.5.3/doc'
Making all in tests
make[2]: Entering directory '/home/lcj/dropwatch-1.5.3/tests'
make[2]: Nothing to be done for 'all'.
make[2]: Leaving directory '/home/lcj/dropwatch-1.5.3/tests'
make[2]: Entering directory '/home/lcj/dropwatch-1.5.3'
make[2]: Leaving directory '/home/lcj/dropwatch-1.5.3'
make[1]: Leaving directory '/home/lcj/dropwatch-1.5.3'
```

## 运行 dropwatch

进入 src 目录中执行 dropwatch 命令，操作记录如下：

```
lcj@debian:~/dropwatch-1.5.3/src$ ./dropwatch
Initializing null lookup method
dropwatch> start
Enabling monitoring...
Kernel monitoring activated.
Issue Ctrl-C to stop monitoring
6 drops at location 0xffffffff9406e5b6 [software]
6 drops at location 0xffffffff9403171b [software]
4 drops at location 0xffffffff9406e5b6 [software]
10 drops at location 0xffffffff9403171b [software]
3 drops at location 0xffffffff9406e5b6 [software]
12 drops at location 0xffffffff9403171b [software]
16 drops at location 0xffffffff9403171b [software]
4 drops at location 0xffffffff9406e5b6 [software]
2 drops at location 0xffffffff93fe192f [software]
6 drops at location 0xffffffff9403171b [software]
3 drops at location 0xffffffff9406e5b6 [software]
1 drops at location 0xffffffff93fe192f [software]
```

左边数值为丢弃包的数目，location 右边是内核函数的地址。

dropwatch -l kas 指定使用内核符号表。

```
lcj@debian:~/dropwatch-1.5.3/src$ ./dropwatch -l kas
Initializing kallsyms db
dropwatch> start
Enabling monitoring...
Kernel monitoring activated.
Issue Ctrl-C to stop monitoring
Error Scanning File: : Success
5 drops at location 0xffffffff9403171b [software]
Error Scanning File: : Success
4 drops at location 0xffffffff9406e5b6 [software]
Error Scanning File: : Success
2 drops at location 0xffffffff9403171b [software]
Error Scanning File: : Success
4 drops at location 0xffffffff9406e5b6 [software]
```

网上搜索了下，发现没有显示函数的原因在于我使用普通用户访问 /proc/kallsyms 文件。

## 普通用户与 root 用户查看 /proc/kallsyms 文件

普通用户查看 /proc/kallsyms 文件的部分输出信息如下：

```
0000000000000000 d descriptor.42136     [i2c_piix4]
0000000000000000 d descriptor.42131     [i2c_piix4]
0000000000000000 t piix4_driver_exit    [i2c_piix4]
0000000000000000 r __func__.42149       [i2c_piix4]
0000000000000000 r __func__.42117       [i2c_piix4]
0000000000000000 r __func__.42132       [i2c_piix4]
0000000000000000 r __func__.42095       [i2c_piix4]
0000000000000000 r piix4_ids    [i2c_piix4]
0000000000000000 r __param_force_addr   [i2c_piix4]
0000000000000000 r __param_str_force_addr       [i2c_piix4]
0000000000000000 r __param_force        [i2c_piix4]
0000000000000000 r __param_str_force    [i2c_piix4]
0000000000000000 d __this_module        [i2c_piix4]
0000000000000000 r __mod_pci__piix4_ids_device_table    [i2c_piix4]
0000000000000000 t cleanup_module       [i2c_piix4]
```

root 用户查看 /proc/kallsyms 文件的部分输出信息如下：

```
ffffffffc010c378 d descriptor.42136     [i2c_piix4]
ffffffffc010c3b0 d descriptor.42131     [i2c_piix4]
ffffffffc01098c9 t piix4_driver_exit    [i2c_piix4]
ffffffffc010a7a0 r __func__.42149       [i2c_piix4]
ffffffffc010a7c0 r __func__.42117       [i2c_piix4]
ffffffffc010a7e0 r __func__.42132       [i2c_piix4]
ffffffffc010a7f0 r __func__.42095       [i2c_piix4]
ffffffffc010a800 r piix4_ids    [i2c_piix4]
ffffffffc010b120 r __param_force_addr   [i2c_piix4]
ffffffffc010b108 r __param_str_force_addr       [i2c_piix4]
ffffffffc010b148 r __param_force        [i2c_piix4]
ffffffffc010b113 r __param_str_force    [i2c_piix4]
ffffffffc010c500 d __this_module        [i2c_piix4]
ffffffffc010a800 r __mod_pci__piix4_ids_device_table    [i2c_piix4]
ffffffffc01098c9 t cleanup_module       [i2c_piix4]
```

可以看到，只有使用 root 用户来访问 /proc/kallsyms 时才能看到函数地址．

## 使用 root 权限执行 dropwatch 命令

使用 root 权限执行 dropwatch 命令，记录信息如下：

```
lcj@debian:~/dropwatch-1.5.3/src$ sudo ./dropwatch -l kas
Initializing kallsyms db
dropwatch> start
Enabling monitoring...
Kernel monitoring activated.
Issue Ctrl-C to stop monitoring
3 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
10 drops at ip_error+8b (0xffffffff9403171b) [software]
2 drops at __netif_receive_skb_core+13f (0xffffffff93fe192f) [software]
13 drops at ip_error+8b (0xffffffff9403171b) [software]
4 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
1 drops at __netif_receive_skb_core+13f (0xffffffff93fe192f) [software]
8 drops at ip_error+8b (0xffffffff9403171b) [software]
8 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
5 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
5 drops at ip_error+8b (0xffffffff9403171b) [software]
5 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
4 drops at ip_error+8b (0xffffffff9403171b) [software]
1 drops at __netif_receive_skb_core+13f (0xffffffff93fe192f) [software]
7 drops at ip_error+8b (0xffffffff9403171b) [software]
2 drops at __netif_receive_skb_core+13f (0xffffffff93fe192f) [software]
2 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
5 drops at ip_error+8b (0xffffffff9403171b) [software]
4 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
5 drops at ip_error+8b (0xffffffff9403171b) [software]
2 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
2 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
1 drops at ip_error+8b (0xffffffff9403171b) [software]
3 drops at __netif_receive_skb_core+13f (0xffffffff93fe192f) [software]
3 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
1 drops at ip_error+8b (0xffffffff9403171b) [software]
4 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
2 drops at ip_error+8b (0xffffffff9403171b) [software]
6 drops at ip_error+8b (0xffffffff9403171b) [software]
1 drops at __udp4_lib_rcv+a06 (0xffffffff9406e5b6) [software]
9 drops at ip_error+8b (0xffffffff9403171b) [software]
```

可以看到，函数地址转化为了具体的函数 + 地址偏移量的格式输出。这个信息每隔 1s 输出一次，或者在废弃的数据包数达到 64 时输出。
