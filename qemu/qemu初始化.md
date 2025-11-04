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

    subgraph virtio_net_init {
        label="virtio_net设备初始化";
        cluster=true;
        virtio_net_pci_instance_init
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
                            -> vhost_user_start
                            -> vhost_net_init
                            -> vhost_dev_init[weight=500]
                               vhost_dev_init
                            -> vhost_set_backend_type[label="根据backend_type设置vhost_ops为user_ops"]
        net_vhost_user_init -> qemu_chr_fe_init[label="关联vhost user 与 chrdev"]
    }

    subgraph vhost_user_negotiation {
        label="vhost_user 协商";
        cluster=true;
        //vhost_dev_init ->
        vhost_dev_init -> vhost_backend_init[label="1"]
            vhost_backend_init -> vhost_user_get_features
            vhost_backend_init -> vhost_user_set_protocol_features
        vhost_dev_init -> vhost_set_owner[label="2"]
        vhost_dev_init -> vhost_get_features[label="3"]
        vhost_dev_init -> vhost_virtqueue_init[label="4"]
                          vhost_virtqueue_init
                       -> vhost_user_set_vring_call

        //vhost_net_init ->
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
              -> qemu_create_cli_devices
              -> qemu_opts_foreach
              -> device_init_func
              -> qdev_device_add
              -> qdev_device_add_from_qdict
              -> qdev_new
              -> object_new
                 object_new
              -> virtio_net_pci_instance_init

    qmp_x_exit_preconfig
              -> qemu_init_board[label="初始化cpu内存"]

    //vhost_dev_init

    //net_vhost_user_event

    //vhost_user_one_time_request
}
```

http://blog.chinaunix.net/uid-28541347-id-5786547.html
https://blog.csdn.net/hzj_001/article/details/94156107
https://blog.csdn.net/qq_41596356/article/details/128538073

问题: pcie的配置空间是在哪里初始化的

main
    ->qemu_init
    	->qemu_create_late_backends
    		->net_init_clients	/* 初始化网络设备 */
    			->qemu_opts_foreach(qemu_find_opts("netdev"), net_init_netdev, NULL,&error_fatal)
    				->net_init_netdev
    					->net_client_init
    						->net_client_init1
    							->net_client_init_fun[netdev->type](netdev, netdev->id, peer, errp)	/* 根据传入的参数类型再 net_client_init_fun 选择相应函数处理 */
    	->qmp_x_exit_preconfig
			->qemu_create_cli_devices
    			->qemu_opts_foreach(qemu_find_opts("device"),	/* 根据需要虚拟化的设备的数量for循环执行 */
                      device_init_func, NULL, &error_fatal); ->device_init_func
device_init_func
    ->qdev_device_add
    	->qdev_device_add
    		->qdev_device_add_from_qdict
    			->qdev_get_device_class
    				->module_object_class_by_name
    					->type_initialize
    						->type_initialize_interface	/* class_init 赋值 */
    							->ti->class_init(ti->class, ti->class_data)    /* 根据对应的驱动类型调用相应的 class_init 函数初始化类 */
    			->qdev_new
    				->DEVICE(object_new(name))
    					->object_new_with_type
    						->object_initialize_with_type
    							->object_init_with_type
    								->ti->instance_init(obj)	/* 创建 virtio-xx 设备实例初始化对象 */
    			->qdev_realize
    				->object_property_set_bool
    					->object_property_set_qobject
    						->object_property_set
    							->prop->set(obj, v, name, prop->opaque, errp)	/* realize 调用 */
