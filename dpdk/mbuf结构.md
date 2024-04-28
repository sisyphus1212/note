![memzone](../../medias/images_0/dpdk内存管理_image.png)
![rte_memseg_list](../../medias/images_0/dpdk内存管理_image-1.png)
![rte_mempool_create](../../medias/images_0/dpdk内存管理_image-2.png)
![alt text](../../medias/images_0/dpdk内存管理_image-3.png)
rte_memseg_list虚拟空间是连续的，但是物理空间则不一定，每个rte_memseg的使用状态可通过bitmap进行标识(代码参考rte_fbarray_set_used)

虚拟堆的申请主要借助mmap函数(参考eal_get_virtual_area)，对应的flags参数被设置为MAP_PRIVATE，MAP_ANONYMOUS，MAP_HUGETLB(映射到大页)和MAP_FIXED，注意此时尚未与物理空间形成绑定，因为映射过程中并没有指定MAP_POPULATE。

heap可预留的最大虚拟空间为512G(RTE_MAX_MEM_MB)，真实预留情况可参考memseg_primary_init处理流程。

物理空间是以rte_memseg为粒度进行映射管理的，每个rte_memseg对应一个hugepage(参考alloc_seg)，执行mmap的时候会指定MAP_POPULATE参数来避免缺页异常的产生。

http://blog.chinaunix.net/uid-28541347-id-5828312.html

https://blog.csdn.net/Phoenix_zxk/article/details/136199068?utm_medium=distribute.pc_relevant.none-task-blog-2~default~baidujs_baidulandingword~default-0-136199068-blog-106116077.235^v43^pc_blog_bottom_relevance_base4&spm=1001.2101.3001.4242.1&utm_relevant_index=1

![alt text](../../medias/images_0/dpdk内存管理_image-5.png)
![alt text](../../medias/images_0/dpdk内存管理_image-6.png)
https://blog.csdn.net/jeawayfox/article/details/106116123
https://zhuanlan.zhihu.com/p/654772667

![alt text](../../medias/images_0/dpdk内存管理_image-7.png)

https://zhuanlan.zhihu.com/p/454346807
em_alloc_rx_queue_mbufs
![alt text](../../medias/images_0/dpdk内存管理_image-8.png)
https://blog.csdn.net/anyegongjuezjd/article/details/136270856
# rte_malloc
![rte_malloc](../../medias/images_0/dpdk内存管理_image-4.png)


