# 用户态调试接口
/sys/kernel/debug/irq/domains
```sh
root@vm0:~# ls /sys/kernel/debug/irq/domains/
 DMAR-MSI     INTEL-IR-MSI-0-3   PCI-MSI-3  '\_SB.PCI0.SFB'
 INTEL-IR-0   IO-APIC-0          VECTOR      default

root@vm0:~# cat /sys/kernel/debug/irq/domains/DMAR-MSI
name:   DMAR-MSI
 size:   0
 mapped: 1
 flags:  0x00000013
 parent: VECTOR
    name:   VECTOR
     size:   0
     mapped: 41
     flags:  0x00000003
Online bitmaps:        8
Global available:   1580
Global reserved:      13
Total allocated:      28
System: 38: 0-19,32,50,128,236,240-242,244,246-255
     | CPU | avl | man | mac | act | vectors
         0   197     0     0    4  33-35,48
         1   197     0     0    4  33-36
         2   197     0     0    4  33-36
         3   197     0     0    4  33-36
         4   198     0     0    3  33-35
         5   198     0     0    3  33-35
         6   198     0     0    3  33-35
         7   198     0     0    3  33-35
```
# x86 初始化
对于x86来说__irq_domain_add在arch_early_irq_init中调用

## 初始化核心函数
```c
[kernel/irq/irqdomain.c]
struct irq_domain *__irq_domain_add(struct device_node *of_node, int size,
				    irq_hw_number_t hwirq_max, int direct_max,
				    const struct irq_domain_ops *ops,
				    void *host_data)
{
	struct irq_domain *domain;

	domain = kzalloc_node(sizeof(*domain) + (sizeof(unsigned int) * size),
			      GFP_KERNEL, of_node_to_nid(of_node));

	/* Fill structure */
	INIT_RADIX_TREE(&domain->revmap_tree, GFP_KERNEL);
	domain->ops = ops;
	domain->host_data = host_data;
	domain->of_node = of_node_get(of_node);
	domain->hwirq_max = hwirq_max;
	domain->revmap_size = size;
	domain->revmap_direct_max_irq = direct_max;
	irq_domain_check_hierarchy(domain);

	mutex_lock(&irq_domain_mutex);
	list_add(&domain->link, &irq_domain_list);  // 加入全局链表
	mutex_unlock(&irq_domain_mutex);

	pr_debug("Added domain %s\n", domain->name);
	return domain;
}
EXPORT_SYMBOL_GPL(__irq_domain_add);
```

domain->ops  为：
```c
static const struct irq_domain_ops x86_vector_domain_ops = {
	.select		= x86_vector_select,
	.alloc		= x86_vector_alloc_irqs,
	.free		= x86_vector_free_irqs,
	.activate	= x86_vector_activate,
	.deactivate	= x86_vector_deactivate,
#ifdef CONFIG_GENERIC_IRQ_DEBUGFS
	.debug_show	= x86_vector_debug_show,
#endif
};
```

## 调用堆栈
```c
[    0.000000]  dump_stack_lvl+0x45/0x63
[    0.000000]  dump_stack+0x10/0x12
[    0.000000]  __irq_domain_add+0x1a8/0x290
[    0.000000]  irq_domain_create_tree+0xb9/0xf0
[    0.000000]  arch_early_irq_init+0x3a/0x79
[    0.000000]  early_irq_init+0xe5/0xec
[    0.000000]  start_kernel+0x449/0x661
[    0.000000]  x86_64_start_reservations+0x24/0x26
[    0.000000]  x86_64_start_kernel+0x86/0x8a
[    0.000000]  secondary_startup_64_no_verify+0xc2/0xcb
```

```c
[    0.002000] irq: ===========================__irq_domain_add===========================
[    0.002000] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.002000] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.002000] Call Trace:
[    0.002000]  dump_stack_lvl+0x45/0x63
[    0.002000]  dump_stack+0x10/0x12
[    0.002000]  __irq_domain_add+0x1a8/0x290
[    0.002000]  irq_domain_create_hierarchy+0x1e/0x50
[    0.002000]  intel_setup_irq_remapping.part.0+0xc0/0x2b0
[    0.002000]  intel_prepare_irq_remapping+0x120/0x18a
[    0.002000]  irq_remapping_prepare+0x16/0x4e
[    0.002000]  enable_IR_x2apic+0x21/0x85
[    0.002000]  default_setup_apic_routing+0x11/0x60
[    0.002000]  apic_intr_mode_init+0x7c/0x10a
[    0.002000]  x86_late_time_init+0x1f/0x30
[    0.002000]  start_kernel+0x5a9/0x661
[    0.002000]  x86_64_start_reservations+0x24/0x26
[    0.002000]  x86_64_start_kernel+0x86/0x8a
[    0.002000]  secondary_startup_64_no_verify+0xc2/0xcb
```

```c
[    0.002000] irq: ===========================__irq_domain_add===========================
[    0.002000] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.002000] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.002000] Call Trace:
[    0.002000]  dump_stack_lvl+0x45/0x63
[    0.002000]  dump_stack+0x10/0x12
[    0.002000]  __irq_domain_add+0x1a8/0x290
[    0.002000]  irq_domain_create_hierarchy+0x3f/0x50
[    0.002000]  msi_create_irq_domain+0x8b/0x180
[    0.002000]  pci_msi_create_irq_domain+0x32/0x110
[    0.002000]  arch_create_remap_msi_irq_domain+0x3d/0x80
[    0.002000]  ? irq_domain_create_hierarchy+0x1e/0x50
[    0.002000]  intel_setup_irq_remapping.part.0+0xe3/0x2b0
[    0.002000]  intel_prepare_irq_remapping+0x120/0x18a
[    0.002000]  irq_remapping_prepare+0x16/0x4e
[    0.002000]  enable_IR_x2apic+0x21/0x85
[    0.002000]  default_setup_apic_routing+0x11/0x60
[    0.002000]  apic_intr_mode_init+0x7c/0x10a
[    0.002000]  x86_late_time_init+0x1f/0x30
[    0.002000]  start_kernel+0x5a9/0x661
[    0.002000]  x86_64_start_reservations+0x24/0x26
[    0.002000]  x86_64_start_kernel+0x86/0x8a
[    0.002000]  secondary_startup_64_no_verify+0xc2/0xcb
[    0.003000] DMAR-IR: Enabled IRQ remapping in xapic mode
```
```c
[    0.003000] irq: ===========================__irq_domain_add===========================
[    0.003000] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.003000] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.003000] Call Trace:
[    0.003000]  dump_stack_lvl+0x45/0x63
[    0.003000]  dump_stack+0x10/0x12
[    0.003000]  __irq_domain_add+0x1a8/0x290
[    0.003000]  ? __irq_domain_alloc_fwnode+0x32/0xf0
[    0.003000]  irq_domain_create_hierarchy+0x3f/0x50
[    0.003000]  msi_create_irq_domain+0x8b/0x180
[    0.003000]  dmar_alloc_hwirq+0xe8/0x120
[    0.003000]  ? __ioapic_read_entry+0x35/0x50
[    0.003000]  ? clear_IO_APIC_pin+0x16c/0x240
[    0.003000]  dmar_set_interrupt.part.0+0x1b/0x60
[    0.003000]  enable_drhd_fault_handling+0x2a/0x6a
[    0.003000]  irq_remap_enable_fault_handling+0x25/0x28
[    0.003000]  apic_intr_mode_init+0xfd/0x10a
[    0.003000]  x86_late_time_init+0x1f/0x30
[    0.003000]  start_kernel+0x5a9/0x661
[    0.003000]  x86_64_start_reservations+0x24/0x26
[    0.003000]  x86_64_start_kernel+0x86/0x8a
[    0.003000]  secondary_startup_64_no_verify+0xc2/0xcb
```

```c
[    0.003000] irq: ===========================__irq_domain_add===========================
[    0.003000] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.003000] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.003000] Call Trace:
[    0.003000]  dump_stack_lvl+0x45/0x63
[    0.003000]  dump_stack+0x10/0x12
[    0.003000]  __irq_domain_add+0x1a8/0x290
[    0.003000]  mp_irqdomain_create+0xb1/0x170
[    0.003000]  ? __setup_irq+0x448/0x770
[    0.003000]  setup_IO_APIC+0x7a/0x87f
[    0.003000]  apic_intr_mode_init+0x102/0x10a
[    0.003000]  x86_late_time_init+0x1f/0x30
[    0.003000]  start_kernel+0x5a9/0x661
[    0.003000]  x86_64_start_reservations+0x24/0x26
[    0.003000]  x86_64_start_kernel+0x86/0x8a
[    0.003000]  secondary_startup_64_no_verify+0xc2/0xcb
```

```c
[    0.016279] irq: ===========================__irq_domain_add===========================
[    0.016279] CPU: 0 PID: 1 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.016279] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.016279] Call Trace:
[    0.016279]  dump_stack_lvl+0x45/0x63
[    0.016279]  dump_stack+0x10/0x12
[    0.016279]  __irq_domain_add+0x1a8/0x290
[    0.016281]  irq_domain_create_hierarchy+0x3f/0x50
[    0.016282]  msi_create_irq_domain+0x8b/0x180
[    0.016285]  pci_msi_create_irq_domain+0x32/0x110
[    0.016287]  native_create_pci_msi_domain+0x49/0x74
[    0.016289]  x86_create_pci_msi_domain+0xd/0x16
[    0.016290]  pci_arch_init+0x2e/0x76
[    0.016296]  ? pcibios_resource_survey+0x70/0x70
[    0.016298]  do_one_initcall+0x41/0x1c0
[    0.016300]  kernel_init_freeable+0x1d5/0x224
[    0.016303]  ? rest_init+0xc0/0xc0
[    0.016310]  kernel_init+0x15/0x110
[    0.016312]  ret_from_fork+0x1f/0x30
```

```c
[    0.734619] irq: ===========================__irq_domain_add===========================
[    0.734624] CPU: 1 PID: 1 Comm: swapper/0 Not tainted 5.15.0+ #45
[    0.734626] Hardware name: QEMU Standard PC (Q35 + ICH9, 2009), BIOS rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org 04/01/2014
[    0.734630] Call Trace:
[    0.734638]  dump_stack_lvl+0x45/0x63
[    0.734656]  dump_stack+0x10/0x12
[    0.734660]  __irq_domain_add+0x1a8/0x290
[    0.734674]  i2c_register_adapter+0x299/0x480
[    0.734685]  i2c_add_adapter+0x57/0x80
[    0.734686]  i801_probe.cold+0x70/0x2a8
[    0.734694]  local_pci_probe+0x43/0x80
[    0.734710]  pci_device_probe+0xfd/0x1b0
[    0.734712]  really_probe+0xcd/0x3f0
[    0.734720]  ? pm_runtime_barrier+0x43/0x80
[    0.734728]  __driver_probe_device+0x104/0x180
[    0.734730]  driver_probe_device+0x1e/0x90
[    0.734731]  __driver_attach+0xa7/0x1a0
[    0.734733]  ? __device_attach_driver+0xe0/0xe0
[    0.734735]  bus_for_each_dev+0x77/0xc0
[    0.734738]  driver_attach+0x19/0x20
[    0.734739]  bus_add_driver+0x130/0x1f0
[    0.734745]  driver_register+0x90/0xf0
[    0.734746]  ? smbalert_driver_init+0x14/0x14
[    0.734758]  __pci_register_driver+0x63/0x70
[    0.734760]  i2c_i801_init+0xa8/0xc7
[    0.734763]  ? i2c_register_driver+0x7b/0xb0
[    0.734764]  ? smbalert_driver_init+0x14/0x14
[    0.734767]  do_one_initcall+0x41/0x1c0
[    0.734770]  kernel_init_freeable+0x1d5/0x224
[    0.734775]  ? rest_init+0xc0/0xc0
[    0.734782]  kernel_init+0x15/0x110
[    0.734784]  ret_from_fork+0x1f/0x30
```
