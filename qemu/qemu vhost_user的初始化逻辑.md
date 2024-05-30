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
fontname="Helvetica,Arial,sans-serif"
//node [fontname="Helvetica,Arial,sans-serif"]
//edge [fontname="Helvetica,Arial,sans-serif"]
//rankdir="LR"
node [fontsize=10, shape=record, height=0.25]
edge [fontsize=10]
  subgraph netdev_init {
    label="Cluster A";
    cluster=true;
    qemu_init -> qemu_create_late_backends
              -> net_init_clients
              -> qemu_opts_foreach
              -> net_init_netdev
              -> net_client_init
              -> visit_type_Netdev
  }
}
```