# DMA remapping初始化流程
```c
start_kernel
	mem_init
		pci_iommu_alloc
			detect_intel_iommu
				dmar_table_detect
				dmar_walk_dmar_table
					for (iter = start; iter < end; iter = next) {
						dmar_validate_one_drhd
					}
				x86_init.iommu.iommu_init = intel_iommu_init
```
- dmar_table_detect 在ACPI表中查找是否有DMAR表
- dmar_walk_dmar_table 对DMAR表的每一项执行 validate_drhd_cb 中指定的操作。此处会对每一表通过 dmar_validate_one_drhd 判断是否是合法
- 设置iommu_init钩子为 intel_iommu_init

```c
kernel_init
	kernel_init_freeable
		do_one_initcall
			pci_iommu_init
				iommu_init钩子(intel_iommu_init)
					iommu_init_mempool
					dmar_table_init
						if (dmar_table_initialized == 0) {
							parse_dmar_table
							dmar_table_initialized = 1
						}
					dmar_dev_scope_init
					dmar_init_reserved_ranges
						reserve_iova(IOAPIC_RANGE_START, IOAPIC_RANGE_END)
						for_each_pci_dev(pdev) {
							reserve_iova(r->start, r->end)
						}
					init_no_remapping_devices
					init_dmars
					dma_ops = &intel_dma_ops;
					bus_set_iommu
					intel_iommu_enabled = 1
```

pci_map_single
    dma_map_single_attrs
        ops->map_page -> intel_map_page
            __intel_map_single
                intel_alloc_iova
                domain_pfn_mapping

# 如何知道哪些设备的dma要走页表进行转换，哪些设备的dma不需要进行地址转换呢
，contex_entry的format里面有一个标志位(TT)来表明这个设备的DMA是否是paasthroug
![alt text](../../../../medias/images_0/iommu流程_image-1.png)
# TT为paasthrough的translation type是什么时候设置的
init_dmars
    iommu_prepare_static_identity_mapping
        domain_add_dev_info
            domain_context_mapping_one

IOVA-连续的内存块，建议使用rte_memzone_reserve()函数，并指定RTE_MEMZONE_IOVA_CONTIG标志