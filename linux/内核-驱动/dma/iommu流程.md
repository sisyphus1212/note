pci_map_single
    dma_map_single_attrs
        ops->map_page -> intel_map_page
            __intel_map_single
                intel_alloc_iova
                domain_pfn_mapping

# 如何知道哪些设备的dma要走页表进行转换，哪些设备的dma不需要进行地址转换呢
，contex_entry的format里面有一个标志位(TT)来表明这个设备的DMA是否是paasthroug
![alt text](../../../../../../../medias/images_0/iommu流程_image.png)

# TT为paasthrough的translation type是什么时候设置的
init_dmars
    iommu_prepare_static_identity_mapping
        domain_add_dev_info
            domain_context_mapping_one

IOVA-连续的内存块，建议使用rte_memzone_reserve()函数，并指定RTE_MEMZONE_IOVA_CONTIG标志