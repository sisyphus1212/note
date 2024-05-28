qemu_init -> qemu_create_late_backends -> net_init_clients -> net_init_netdev -> net_client_init1 -> net_client_init_fun[netdev->type](netdev, netdev->id, peer, errp) -> net_init_vhost_user
qemu_init -> qmp_x_exit_preconfig -> qemu_init_board // 初始化cpu 内存
