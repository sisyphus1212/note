Root-table是devices mapping的最顶层结构。它的起始地址由Root Table Address Register指定，共计4-KB大小，由256个root-entries组成（可以算出：每个root-entry有16个字节空间）。每个root-entry对应一个bus number，所以source-id中的bus number在mapping过程中会被当做root-table中的index来使用。

# Root Table Address Register
![alt text](../../medias/images_0/dmar--root_table_image.png)
![alt text](../../medias/images_0/dmar--root_table_image-1.png)

可以看到：有两种形式的table，根据RTT bit进行控制。下面就依次来看这两种table

# regular Root-table entry
![alt text](../../medias/images_0/dmar--root_table_image-2.png)
![alt text](../../medias/images_0/dmar--root_table_image-3.png)

# Extended Root-table entry
在支持Extended-Context-Support (ECS=1 in Extended Capability Register)的硬件上，如果RTADDR_REG的RTT=1，则它指向的是一个extended root-table。
![alt text](../../medias/images_0/dmar--root_table_image-4.png)
![alt text](../../medias/images_0/dmar--root_table_image-5.png)

综上，又会涉及到两种context entry，下面依次来看。
# regular Context-Entry
一个context-entry用于将一个bus上的一个特定I/O device映射到其所属的domain中，也就是指向该domain的地址翻译结构。
source-id中的低8位（device#和function#）用来作为设备在context-table中的index使用


![alt text](../../medias/images_0/dmar--root_table_image-6.png)

## 多设备共享同一地址翻译结构
- Device 0: Function 0（由 Context-entry 0 表示）。
- Device 31: Function 7（由 Context-entry 255 表示）。
这些设备可能属于同一个虚拟机或任务域（比如一个 IOMMU 配置的共享域）。，这种配置典型用于 SR-IOV (Single-Root I/O Virtualization) 场景，多个 PCIe 虚拟功能 (VF) 共享相同的 IOMMU 映射。

Context-entries只能支持requests-without-PASID，它详细结构如下：
![alt text](../../medias/images_0/dmar--root_table_image-7.png)

# Extended-Context-Entry
一个Extended root entry可以同时索引一个lower-context-table和一个upper-context-table，这两者都是4-KB大小、包含128个extended-context-entries。
- lower-context-table 对应了特定bus（root entry索引）上device#为0-15的PCI设备
- upper-context-table 对应了特定bus（root entry索引）上device#为16-31的PCI设备
![alt text](../../medias/images_0/dmar--root_table_image-8.png)
Extended-context-entries既支持requests-without-PASID，也支持requests-with-PASID。对于前者，entry结构与上述regular context entry一致；对于后者，其结构如下所示：
![alt text](../../medias/images_0/dmar--root_table_image-9.png)
![alt text](../../medias/images_0/dmar--root_table_image-10.png)
First-level Translation
Extended-Context-Entry可以被配置为通过First-level Translation来翻译requests-with-PASID。这种配置下Extended-Context-Entry包含了指向PASID-table的指针和大小，而requests-with-PASID中的PASID-number作为PASID-table中的offset来索引一个PASID-entry。在PASID-entry中包含了相应进程地址空间的first-level translation structure的指针。

second-level translation可以用来转换requests-without-PASID，也可以在nested translation过程中用来转换requests-with-PASID的first-level translation。（这种使用方式还不太明白，或许和嵌套虚拟化相关，L1虚拟机向L2虚拟机呈现vt-d dma remapping功能时或许会用到）