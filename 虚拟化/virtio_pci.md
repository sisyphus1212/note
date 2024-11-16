# msix table entry 填充
#0  pci_msi_domain_write_msg (irq_data=0xffff888102363c28, msg=0xffffc900003934a8) at drivers/pci/msi.c:1270
#1  0xffffffff810d19fb in irq_chip_write_msi_msg (msg=0xffffc900003934a8, data=0xffff888102363c28) at kernel/irq/msi.c:212
#2  msi_domain_activate (domain=<optimized out>, irq_data=0xffff888102363c28, early=<optimized out>) at kernel/irq/msi.c:263
#3  0xffffffff810ce300 in __irq_domain_activate_irq (irqd=irqd@entry=0xffff888102363c28, reserve=reserve@entry=true) at kernel/irq/irqdomain.c:1763
#4  0xffffffff810cfe54 in irq_domain_activate_irq (irq_data=0xffff888102363c28, reserve=reserve@entry=true) at kernel/irq/irqdomain.c:1786
#5  0xffffffff810d2101 in __msi_domain_alloc_irqs (domain=0xffff888100049480, dev=0xffff888102eeb0d0, nvec=<optimized out>) at kernel/irq/msi.c:600
#6  0xffffffff810d23f2 in msi_domain_alloc_irqs (domain=0xffff888102363c28, dev=0xffffc900003934a8, dev@entry=0xffff888102eeb0d0, nvec=0, nvec@entry=3) at kernel/irq/msi.c:638
#7  0xffffffff814949d0 in pci_msi_setup_msi_irqs (type=17, nvec=3, dev=0xffff888102eeb000) at drivers/pci/msi.c:43
#8  msix_capability_init (affd=<optimized out>, nvec=<optimized out>, entries=0x0 <fixed_percpu_data>, dev=0xffff888102eeb000) at drivers/pci/msi.c:726
#9  __pci_enable_msix (flags=<optimized out>, affd=<optimized out>, nvec=<optimized out>, entries=0x0 <fixed_percpu_data>, dev=0xffff888102eeb000) at drivers/pci/msi.c:937
#10 __pci_enable_msix_range (dev=dev@entry=0xffff888102eeb000, entries=entries@entry=0x0 <fixed_percpu_data>, minvec=minvec@entry=3, maxvec=maxvec@entry=3, affd=affd@entry=0x0 <fixed_percpu_data>,
    flags=flags@entry=4) at drivers/pci/msi.c:1069
#11 0xffffffff81494c6a in __pci_enable_msix_range (flags=4, affd=0x0 <fixed_percpu_data>, maxvec=3, minvec=3, entries=0x0 <fixed_percpu_data>, dev=0xffff888102eeb000) at drivers/pci/msi.c:1059
#12 pci_alloc_irq_vectors_affinity (dev=0xffff888102eeb000, min_vecs=min_vecs@entry=3, max_vecs=max_vecs@entry=3, flags=4, affd=affd@entry=0x0 <fixed_percpu_data>) at drivers/pci/msi.c:1138
#13 0xffffffff81518fe3 in vp_request_msix_vectors (desc=0x0 <fixed_percpu_data>, per_vq_vectors=<optimized out>, nvectors=<optimized out>, vdev=0xffff888102ef9400) at drivers/virtio/virtio_pci_common.c:133
#14 vp_find_vqs_msix (vdev=vdev@entry=0xffff888102ef9400, nvqs=nvqs@entry=3, vqs=vqs@entry=0xffff888104c7e760, callbacks=callbacks@entry=0xffff888104c7e7a0, names=names@entry=0xffff888104c7e7c0,
    per_vq_vectors=per_vq_vectors@entry=true, ctx=0xffff8881009acc50, desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_common.c:304
#15 0xffffffff815193f3 in vp_find_vqs (vdev=vdev@entry=0xffff888102ef9400, nvqs=3, vqs=0xffff888104c7e760, callbacks=0xffff888104c7e7a0, names=0xffff888104c7e7c0, ctx=0xffff8881009acc50,
    desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_common.c:400
#16 0xffffffff81518636 in vp_modern_find_vqs (vdev=0xffff888102ef9400, nvqs=<optimized out>, vqs=<optimized out>, callbacks=<optimized out>, names=<optimized out>, ctx=<optimized out>,
    desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_modern.c:259
#17 0xffffffff817c9e56 in virtio_find_vqs_ctx (desc=0x0 <fixed_percpu_data>, ctx=0xffff8881009acc50, names=0xffff888104c7e7c0, callbacks=0xffff888104c7e7a0, vqs=0xffff888104c7e760, nvqs=3,
    vdev=0xffff888102363c28) at ./include/linux/virtio_config.h:215
#18 virtnet_find_vqs (vi=vi@entry=0xffff888102eea8c0) at drivers/net/virtio_net.c:2882
#19 0xffffffff817ca3b1 in init_vqs (vi=0xffff888102eea8c0) at drivers/net/virtio_net.c:2967
#20 virtnet_probe (vdev=0xffff888102ef9400) at drivers/net/virtio_net.c:3218
#21 0xffffffff8151404b in virtio_dev_probe (_d=0xffff888102ef9410) at drivers/virtio/virtio.c:273
#22 0xffffffff81717e6d in call_driver_probe (drv=0xffffffff82de7d00 <virtio_net_driver>, dev=0xffff888102ef9410) at drivers/base/dd.c:517
#23 really_probe (dev=dev@entry=0xffff888102ef9410, drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>) at drivers/base/dd.c:596
#24 0xffffffff81718294 in __driver_probe_device (drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>, dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:751
#25 0xffffffff8171832e in driver_probe_device (drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>, dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:781
#26 0xffffffff81718911 in __device_attach_driver (drv=0xffffffff82de7d00 <virtio_net_driver>, _data=0xffffc900003939e8) at drivers/base/dd.c:898
#27 0xffffffff81715a8d in bus_for_each_drv (bus=<optimized out>, start=start@entry=0x0 <fixed_percpu_data>, data=data@entry=0xffffc900003939e8, fn=fn@entry=0xffffffff817188a0 <__device_attach_driver>)
    at drivers/base/bus.c:427
#28 0xffffffff817185f9 in __device_attach (dev=dev@entry=0xffff888102ef9410, allow_async=allow_async@entry=true) at drivers/base/dd.c:969
#29 0xffffffff81718b2e in device_initial_probe (dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:1016
#30 0xffffffff81716dea in bus_probe_device (dev=dev@entry=0xffff888102ef9410) at drivers/base/bus.c:487
#31 0xffffffff81714312 in device_add (dev=dev@entry=0xffff888102ef9410) at drivers/base/core.c:3396
#32 0xffffffff81513ca2 in register_virtio_device (dev=dev@entry=0xffff888102ef9400) at drivers/virtio/virtio.c:423
#33 0xffffffff81518bcb in virtio_pci_probe (pci_dev=0xffff888102eeb000, id=<optimized out>) at drivers/virtio/virtio_pci_common.c:552
#34 0xffffffff8148a0d3 in local_pci_probe (_ddi=_ddi@entry=0xffffc90000393b68) at drivers/pci/pci-driver.c:323
#35 0xffffffff8148b94d in pci_call_probe (id=<optimized out>, dev=0xffff888102eeb000, drv=0xffffffff82dc0da0 <virtio_pci_driver>) at drivers/pci/pci-driver.c:380
#36 __pci_device_probe (pci_dev=0xffff888102eeb000, drv=0xffffffff82dc0da0 <virtio_pci_driver>) at drivers/pci/pci-driver.c:405
#37 pci_device_probe (dev=0xffff888102eeb0d0) at drivers/pci/pci-driver.c:448

# msix table entry unmask
#0  pci_msi_unmask_irq (data=0xffff888102363c28) at drivers/pci/msi.c:243
#1  0xffffffff810cc97f in unmask_irq (desc=<optimized out>) at kernel/irq/chip.c:438
#2  unmask_irq (desc=<optimized out>) at kernel/irq/chip.c:432
#3  irq_enable (desc=desc@entry=0xffff888102363c00) at kernel/irq/chip.c:345
#4  0xffffffff810cca15 in __irq_startup (desc=desc@entry=0xffff888102363c00) at kernel/irq/chip.c:249
#5  0xffffffff810ccaee in irq_startup (desc=desc@entry=0xffff888102363c00, resend=resend@entry=true, force=force@entry=false) at kernel/irq/chip.c:270
#6  0xffffffff810ca198 in __setup_irq (irq=irq@entry=33, desc=desc@entry=0xffff888102363c00, new=new@entry=0xffff888104cfd300) at kernel/irq/manage.c:1737
#7  0xffffffff810ca2df in request_threaded_irq (irq=33, handler=handler@entry=0xffffffff81518910 <vp_config_changed>, thread_fn=thread_fn@entry=0x0 <fixed_percpu_data>, irqflags=irqflags@entry=0,
    devname=devname@entry=0xffff888102ef9c00 "virtio1-config", dev_id=dev_id@entry=0xffff888102ef9400) at kernel/irq/manage.c:2172
#8  0xffffffff8151905b in request_irq (dev=0xffff888102ef9400, name=0xffff888102ef9c00 "virtio1-config", flags=0, handler=0xffffffff81518910 <vp_config_changed>, irq=<optimized out>)
    at ./include/linux/interrupt.h:168
#9  vp_request_msix_vectors (desc=<optimized out>, per_vq_vectors=<optimized out>, nvectors=<optimized out>, vdev=0xffff888102ef9400) at drivers/virtio/virtio_pci_common.c:143
#10 vp_find_vqs_msix (vdev=vdev@entry=0xffff888102ef9400, nvqs=nvqs@entry=3, vqs=vqs@entry=0xffff888104c7e760, callbacks=callbacks@entry=0xffff888104c7e7a0, names=names@entry=0xffff888104c7e7c0,
    per_vq_vectors=per_vq_vectors@entry=true, ctx=0xffff8881009acc50, desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_common.c:304
#11 0xffffffff815193f3 in vp_find_vqs (vdev=vdev@entry=0xffff888102ef9400, nvqs=3, vqs=0xffff888104c7e760, callbacks=0xffff888104c7e7a0, names=0xffff888104c7e7c0, ctx=0xffff8881009acc50,
    desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_common.c:400
#12 0xffffffff81518636 in vp_modern_find_vqs (vdev=0xffff888102ef9400, nvqs=<optimized out>, vqs=<optimized out>, callbacks=<optimized out>, names=<optimized out>, ctx=<optimized out>,
    desc=0x0 <fixed_percpu_data>) at drivers/virtio/virtio_pci_modern.c:259
#13 0xffffffff817c9e56 in virtio_find_vqs_ctx (desc=0x0 <fixed_percpu_data>, ctx=0xffff8881009acc50, names=0xffff888104c7e7c0, callbacks=0xffff888104c7e7a0, vqs=0xffff888104c7e760, nvqs=3,
    vdev=0xffff888102363c28) at ./include/linux/virtio_config.h:215
#14 virtnet_find_vqs (vi=vi@entry=0xffff888102eea8c0) at drivers/net/virtio_net.c:2882
#15 0xffffffff817ca3b1 in init_vqs (vi=0xffff888102eea8c0) at drivers/net/virtio_net.c:2967
#16 virtnet_probe (vdev=0xffff888102ef9400) at drivers/net/virtio_net.c:3218
#17 0xffffffff8151404b in virtio_dev_probe (_d=0xffff888102ef9410) at drivers/virtio/virtio.c:273
#18 0xffffffff81717e6d in call_driver_probe (drv=0xffffffff82de7d00 <virtio_net_driver>, dev=0xffff888102ef9410) at drivers/base/dd.c:517
#19 really_probe (dev=dev@entry=0xffff888102ef9410, drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>) at drivers/base/dd.c:596
#20 0xffffffff81718294 in __driver_probe_device (drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>, dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:751
#21 0xffffffff8171832e in driver_probe_device (drv=drv@entry=0xffffffff82de7d00 <virtio_net_driver>, dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:781
#22 0xffffffff81718911 in __device_attach_driver (drv=0xffffffff82de7d00 <virtio_net_driver>, _data=0xffffc900003939e8) at drivers/base/dd.c:898
#23 0xffffffff81715a8d in bus_for_each_drv (bus=<optimized out>, start=start@entry=0x0 <fixed_percpu_data>, data=data@entry=0xffffc900003939e8, fn=fn@entry=0xffffffff817188a0 <__device_attach_driver>)
    at drivers/base/bus.c:427
#24 0xffffffff817185f9 in __device_attach (dev=dev@entry=0xffff888102ef9410, allow_async=allow_async@entry=true) at drivers/base/dd.c:969
#25 0xffffffff81718b2e in device_initial_probe (dev=dev@entry=0xffff888102ef9410) at drivers/base/dd.c:1016
#26 0xffffffff81716dea in bus_probe_device (dev=dev@entry=0xffff888102ef9410) at drivers/base/bus.c:487
#27 0xffffffff81714312 in device_add (dev=dev@entry=0xffff888102ef9410) at drivers/base/core.c:3396
#28 0xffffffff81513ca2 in register_virtio_device (dev=dev@entry=0xffff888102ef9400) at drivers/virtio/virtio.c:423
#29 0xffffffff81518bcb in virtio_pci_probe (pci_dev=0xffff888102eeb000, id=<optimized out>) at drivers/virtio/virtio_pci_common.c:552
#30 0xffffffff8148a0d3 in local_pci_probe (_ddi=_ddi@entry=0xffffc90000393b68) at drivers/pci/pci-driver.c:323
#31 0xffffffff8148b94d in pci_call_probe (id=<optimized out>, dev=0xffff888102eeb000, drv=0xffffffff82dc0da0 <virtio_pci_driver>) at drivers/pci/pci-driver.c:380


# 中断配置
## qemu
virtio_pci_modern_regions_init

## kernel
struct virtio_pci_common_cfg {
	/* About the whole device. */
	__le32 device_feature_select;	/* read-write */
	__le32 device_feature;		/* read-only */
	__le32 guest_feature_select;	/* read-write */
	__le32 guest_feature;		/* read-write */
	__le16 msix_config;		/* read-write */
	__le16 num_queues;		/* read-only */
	__u8 device_status;		/* read-write */
	__u8 config_generation;		/* read-only */

	/* About a specific virtqueue. */
	__le16 queue_select;		/* read-write */
	__le16 queue_size;		/* read-write, power of 2. */
	__le16 queue_msix_vector;	/* read-write */
	__le16 queue_enable;		/* read-write */
	__le16 queue_notify_off;	/* read-only */
	__le32 queue_desc_lo;		/* read-write */
	__le32 queue_desc_hi;		/* read-write */
	__le32 queue_avail_lo;		/* read-write */
	__le32 queue_avail_hi;		/* read-write */
	__le32 queue_used_lo;		/* read-write */
	__le32 queue_used_hi;		/* read-write */
};