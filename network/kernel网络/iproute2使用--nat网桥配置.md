---
title: iproute2使用--nat网桥配置
date: 2019-09-07 16:04:02
index_img: https://i.stack.imgur.com/7xTB3.png
categories:
- [linux,网络开发]
tags:
 - iproute2
 - 网桥
 - kernel
---
# 工具安装
sudo apt install iproute2

# 配置一个nat br0环境
```shell
ip link add veth0 type veth peer name veth1

ip netns add ns0
ip link set veth0 netns ns0
ip netns exec ns0 ip link set veth0 up
ip netns exec ns0 ip addr add 10.10.0.2/24 dev veth0
ip netns exec ns0 ip route add default via 10.10.0.1

ip link add br1 type bridge
ip link set br1 up
ip addr add 10.10.0.1/24 dev br1
ip link set veth1 master br1
ip link set veth1 up
```

# 验证
```shell
ip netns exec ns0 ping 1.1.1.1
```