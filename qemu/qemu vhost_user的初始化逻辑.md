---
title: qemu vhost_user的初始化逻辑
date: 2024-05-30 10:26:48
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,网络开发,虚拟化开发]
tags:
 - qemu
 - vdpa
---


```graphviz
digraph {
qemu_init -> qemu_create_late_backends -> net_init_netdev -> net_client_init ->
}
```