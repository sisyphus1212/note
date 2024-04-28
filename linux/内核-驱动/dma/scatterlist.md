```c
struct scatterlist {
	unsigned long	page_link;
	unsigned int	offset;
	unsigned int	length;
	dma_addr_t	dma_address;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	unsigned int	dma_length;
#endif
};
```
**page_link**：

对于chain sg 来说，记录下一个 SG 数组的首地址，并且用bit[0] 和 bit[1] 来表示是chain sg 还是 end sg；
对于 end sg 来说，只有bit[1] 为1，其他无意义；
对于普通 sg 来说，记录的是关联的内存页块的地址；

![sgl 管理图](../../../../../../../medias/images_0/scatterlist_image.png)
如图，假设这里有 4 个 sg 数组：

第一个sg 数组的首地址会存入 sg_table 的 sql 中；
每一个 sg 数组的最后一个 sg 为sg 铰链(chain)，指向下一个 sg 数组，其成员page_link 的 bit[0] 和 bit[1] 将作为铰链的状态：
> 若都为0，表示其为有效的、普通的 sg；
> 若 bit[0] = 1表示该sg 为铰链 sg；
> 若 bit[1] = 1表示该sg为 结束sg；