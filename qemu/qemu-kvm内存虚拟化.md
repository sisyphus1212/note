title: qemu-kvm 内存虚拟化
date: 2024-04-20 10:04:03
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,网络开发,虚拟化开发]
tags:
 - 内存虚拟化
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
    qemu_init -> qemu_create_machine
              -> cpu_exec_init_all
              -> memory_map_init
              -> memory_region_init
              ->
}
```