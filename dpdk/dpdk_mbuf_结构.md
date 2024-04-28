---
title: dpdk mbuf 结构
date: 2022-03-19 10:55:27
categories:
- [dpdk]
tags:
- dpdk
- 网络开发
---

# mbuf 的结构
占用2个cache line，且最常访问的成员在第1个cache line中。
![rte_mbuf](../../../../../medias/images_0/dpdk_mbuf_结构_image-1.png)
![multi-segmented rte_mbuf](../../../../../medias/images_0/dpdk_mbuf_结构_image.png)
图片摘自 [Mbuf Library](https://doc.dpdk.org/guides/prog_guide/mbuf_lib.html)。

对于multi-segmented rte_mbuf，仅在第一个mbuf结构体中包含元数据信息。

mbuf 可以分为四部分：
1. mbuf 结构体
2. headroom
3. dataroom
4. tailroom

这四部分中:
    第一部分用于存储 mbuf 内部的数据结构，
    第二部分与第四部分的使用由用户控制，
    第三部分用于存储报文内容。

|mbuf成员                   |说明                                  |
|---------------------------|-------------------------------------|
|m	                        |首部，即mbuf结构体                    |
|m->buf_addr	            |headroom起始地址                      |
|m->data_off	            |data起始地址相对于buf_addr的偏移       |
|m->buf_len	                |mbuf和priv之后内存的长度，包含headroom  |
|m->pkt_len	                |整个mbuf链的data总长度                 |
|m->data_len	            |实际data的长度                         |
|m->buf_addr+m->data_off	|实际data的起始地址                      |
|rte_pktmbuf_mtod(m)	    |同上                                   |
|rte_pktmbuf_data_len(m)	|同m->data_len                          |
|rte_pktmbuf_pkt_len	    |同m->pkt_len                           |
|rte_pktmbuf_data_room_size	|同m->buf_len                           |
|rte_pktmbuf_headroom	    |headroom长度                           |
|rte_pktmbuf_tailroom	    |尾部剩余空间长度                        |

# mbuf 的日常操作
mbuf 的日常操作主要有如下几类：
1. 读取、写入 mbuf 结构中的不同字段
2. 从 pktmbuf pool 中 alloc  mbuf
3. 释放 mbuf 到 pktmbuf pool 中
4. 获取 mbuf 的 dataroom 的物理地址
5. 获取 mbuf 的 headroom 位置
6. 获取 mbuf 的 tailroom 的位置
7. 使用 mbuf 的 headroom 在 dataroom 前插入指定长度数据
8. 使用 mbuf 的 tailroom 在 dataroom 后插入指定长度数据
9. 使用已有的 mbuf 克隆一个新的 mbuf
```c
struct rte_mempool *rte_mempool_create(const char *name,
                                       unsigned int n,
                                       unsigned int elt_size,
                                       unsigned int cache_size,
                                       uint16_t private_data_size,
                                       rte_mempool_ctor_t *mp_init,
                                       void *mp_init_arg,
                                       rte_mempool_obj_cb_t *obj_init,
                                       void *obj_init_arg,
                                       int socket_id,
                                       unsigned int flags);
name:              内存池名称。
n:                 内存池中元素（即rte_mbuf结构体）的数量。
elt_size:          每个元素的大小，通常为RTE_MBUF_DEFAULT_BUF_SIZE。
cache_size:        每个逻辑核心缓存中可缓存的元素数目，通常为MBUF_CACHE_SIZE。
private_data_size: 与每个元素相关联的私有数据大小，如果不需要私有数据则设为0。
mp_init:           内存池初始化函数指针，在内存池创建时调用，可以为NULL。
mp_init_arg:       初始化函数参数，在内存池初始化函数被调用时传入该参数，可以为NULL。
obj_init:          元素初始化回调函数指针，在每次从内存池中取出元素时被调用，可以为NULL。
obj_init_arg:      元素初始化回调函数参数，在每次从内存池中取出元素时传入该参数，可以为NULL。
socket_id:         内存池在哪个NUMA节点上创建，可以为RTE_SOCKET_ID_ANY表示任意节点。
flags:             内存池的一些标志位，例如RTE_MEMPOOL_F_SP_PUT等。

该函数会返回一个rte_mempool类型的指针，如果创建失败则返回NULL。
```

以下代码分别创建了两个mbuf，给它们添加数据，最后将它们组合成链。在此过程中打印了上表中的一些数据，可以帮助理解各指针和长度的含义，其中省去了错误处理代码。
```c
#define ITEM_COUNT 1024
#define ITEM_SIZE  (1600 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define CACHE_SIZE 32
static int mbuf_demo(void)
{
    int ret;
    struct rte_mempool* mpool;
    struct rte_mbuf *m, *m2;
    struct rte_pktmbuf_pool_private priv;

    priv.mbuf_data_room_size = 1600 + RTE_PKTMBUF_HEADROOM - 16; /*创建mempool时传入了priv，1548                              */
    priv.mbuf_priv_size = 16;                                    /*这将在每个mbuf的首部后面添加16字节的私有数据,然后才是head room*/

    mpool = rte_mempool_create("test_pool",
                               ITEM_COUNT,
                               ITEM_SIZE,
                               CACHE_SIZE,
                               sizeof(struct rte_pktmbuf_pool_private),
                               rte_pktmbuf_pool_init,
                               &priv,                           /*传入priv*/
                               rte_pktmbuf_init,
                               NULL,
                               0,
                               MEMPOOL_F_SC_GET);
    m = rte_pktmbuf_alloc(mpool);
    mbuf_dump(m);   // (1)

    rte_pktmbuf_append(m, 1400);
    mbuf_dump(m);   // (2)

    m2 = rte_pktmbuf_alloc(mpool);
    rte_pktmbuf_append(m2, 500);
    mbuf_dump(m2);

    ret = rte_pktmbuf_chain(m, m2);
    mbuf_dump(m);   // (3)

    return 0;
}
```

**(1)** dump 结果
```c
RTE_PKTMBUF_HEADROOM: 128
sizeof(mbuf): 128
m: 0x7fbf1a810000
m->buf_addr: 0x7fbf1a810090
m->data_off: 128
m->buf_len: 1712
m->pkt_len: 0
m->data_len: 0
m->buf_addr+m->data_off: 0x7fbf1a810110
rte_pktmbuf_mtod(m): 0x7fbf1a810110
rte_pktmbuf_data_len(m): 0
rte_pktmbuf_pkt_len(m): 0
rte_pktmbuf_headroom(m): 128
rte_pktmbuf_tailroom(m): 1584
rte_pktmbuf_data_room_size(mpool): 1712
rte_pktmbuf_priv_size(mpool): 16
```
![(1) dump 结果](../../../../../medias/images_0/dpdk_mbuf_结构_image-2.png)

**(2)** dump 结果
```c
m: 0x7fbf1a810000
m->buf_addr: 0x7fbf1a810090
m->data_off: 128
m->buf_len: 1712
m->pkt_len: 1400
m->data_len: 1400
m->buf_addr+m->data_off: 0x7fbf1a810110
rte_pktmbuf_mtod(m): 0x7fbf1a810110
rte_pktmbuf_data_len(m): 1400
rte_pktmbuf_pkt_len(m): 1400
rte_pktmbuf_headroom(m): 128
rte_pktmbuf_tailroom(m): 184
rte_pktmbuf_data_room_size(mpool): 1712
rte_pktmbuf_priv_size(mpool): 16
```
![(2) dump 结果](../../../../../medias/images_0/dpdk_mbuf_结构_image-3.png)

**(3)** dump 结果
之后创建m2并给它添加data，在(3)处将m与m2连接，m做为链的首节点，此时m的打印结果如下:
```c
m: 0x7fbf1a810000
m->buf_addr: 0x7fbf1a810090
m->data_off: 128
m->buf_len: 1712
m->pkt_len: 1900
m->data_len: 1400
m->buf_addr+m->data_off: 0x7fbf1a810110
rte_pktmbuf_mtod(m): 0x7fbf1a810110
rte_pktmbuf_data_len(m): 1400
rte_pktmbuf_pkt_len(m): 1900
rte_pktmbuf_headroom(m): 128
rte_pktmbuf_tailroom(m): 184
rte_pktmbuf_data_room_size(mpool): 1712
rte_pktmbuf_priv_size(mpool): 16
```
注意pkt_len的变化，它已经加上了m2的500字节。如果此时打印m—>next， 会发现m->next == m2。