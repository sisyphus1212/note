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
    subgraph memory_map_init {
        label="创建两个AddressSpace：\n
                address_space_memory,\n
                address_space_io";

        cluster=true;
        memory_map_init
                -> memory_region_init
                -> object_initialize
                -> object_initialize_with_type
                object_initialize_with_type
                -> object_init_with_type[label="初始化cpu内存"]
                object_init_with_type
                -> memory_region_initfn
              memory_map_init
                -> memory_region_init_io
    }

    subgraph vm_mem_alloc {
        cluster=true;
        label="虚拟机内存初始化"
        qemu_init_board -> machine_run_board_init
                        -> pc_init_v7_2
                        ->
    }
    qemu_init -> qemu_create_machine
              -> cpu_exec_init_all
              -> memory_map_init

}
```
object_init_with_type会调用自身的实例化函数并递归地调用父类的实例化函数。对于MemoryRegion类的对象，其实例化函数为memory_region_initfn()