---
title: qemu 初始化流程
date: 2024-04-20 10:04:03
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,网络开发,虚拟化开发]
tags:
 - kernel_network
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
        label="netdev 初始化";
        cluster=true;
        net_init_netdev
        -> net_client_init
        -> visit_type_Netdev
    }

    subgraph chardev_init {
        label="chardev 初始化";
        cluster=true;
        chardev_init_func
            -> qemu_chr_new_from_opts
            -> qemu_chardev_new
            -> chardev_new
            -> qemu_char_open
            -> qmp_chardev_open_socket
            -> qmp_chardev_open_socket_server
            -> tcp_chr_accept_server_sync
            -> qio_net_listener_wait_client
            -> g_main_loop_run
    }
    subgraph netdev_init {
        label="netdev 初始化";
        cluster=true;
        net_init_netdev
        -> net_client_init
        -> visit_type_Netdev
    }
    subgraph vhost_user_init {
        label="qemu vhost_user 初始化";
        cluster=true;
        net_init_vhost_user -> net_vhost_user_init
                            -> qemu_chr_fe_set_handlers
                            -> qemu_chr_fe_set_handlers_full
                            -> qemu_chr_be_event
                            -> chr_be_event
                            -> net_vhost_user_event


        net_vhost_user_init -> qemu_chr_fe_init[label="关联vhost user 与 chrdev"]
    }

    subgraph vhost_user_negotiation {
        label="vhost_user 协商";
        cluster=true;
        net_vhost_user_event -> vhost_user_start
                             -> vhost_net_init
                             -> vhost_dev_init
                             -> vhost_virtqueue_init
                             -> vhost_user_set_vring_call
    }

    subgraph vdpa_restart {
        label="vdpa_重启";
        cluster=true;
        net_vhost_user_init -> qemu_chr_fe_wait_connected[label="vdpa 重启逻辑"]
                               qemu_chr_fe_wait_connected
                            -> vhost_dev_init
    }

    qemu_init -> qemu_create_early_backends[label="创建chardev 并等待连接"]
                 qemu_create_early_backends
              -> qemu_opts_foreach
              -> chardev_init_func

    qemu_init -> qemu_create_late_backends
              -> net_init_clients
              -> qemu_opts_foreach
              -> net_init_netdev
              -> net_client_init1
              -> net_client_init_fun
              -> net_init_vhost_user

    qemu_init -> qmp_x_exit_preconfig
              -> qemu_init_board[label="初始化cpu内存"]

    //vhost_dev_init

    //net_vhost_user_event

    //vhost_user_one_time_request
}
```

http://blog.chinaunix.net/uid-28541347-id-5786547.html