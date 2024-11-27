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