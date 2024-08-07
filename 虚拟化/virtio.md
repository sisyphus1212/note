
---
title: virtio 协议
date: 2024-07-23 15:20:11
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,virtio,虚拟化开发]
tags:
 - kernel_network
 - virtio
---

# split ring

# packed ring
## wrap_counter

#define VIRTQ_DESC_F_AVAIL (1 << 7)
#define VIRTQ_DESC_F_USED (1 << 15)


avail buf
avail_wrap_counter 初始化为1
VIRTQ_DESC_F_AVAIL == avail_wrap_counter
VIRTQ_DESC_F_USED != avail_wrap_counter

used buf
used_wrap_counter 初始化为1
used = !!(flags & VRING_PACKED_DESC_F_USED);
avail = !!(flags & VRING_PACKED_DESC_F_AVAIL);
avail == used && used == vq->vq_packed.used_wrap_counter;

avail_wrap_counter ==  1 -> a 1 u 0
avail_wrap_counter ==  0 -> a 0 u 1

if (vq->used_wrap_counter) {
    flags |= VRING_DESC_F_USED;
    flags |= VRING_DESC_F_AVAIL;
} else {
    flags &= ~VRING_DESC_F_USED;
    flags &= ~VRING_DESC_F_AVAIL;
}

# dpdk packed ring 初始化
```graphviz
digraph {
    fontname="Helvetica,Arial,sans-serif"
    //node [fontname="Helvetica,Arial,sans-serif"]
    //edge [fontname="Helvetica,Arial,sans-serif"]
    //rankdir="LR"
    node [fontsize=10, shape=record, height=0.25]
    edge [fontsize=10]
    virtio_init_vring -> vring_init_packed
}
```

# dpdk packed ring 收包逻辑
## 前端处理逻辑
```graphviz
digraph {
    fontname="Helvetica,Arial,sans-serif"
    //node [fontname="Helvetica,Arial,sans-serif"]
    //edge [fontname="Helvetica,Arial,sans-serif"]
    //rankdir="LR"
    node [fontsize=10, shape=record, height=0.25]
    edge [fontsize=10]
    virtio_recv_pkts_packed_vec -> virtio_recv_refill_packed_vec
    virtio_recv_pkts_packed -> virtqueue_enqueue_recv_refill_packed
}
```
## 后端处理逻辑
```graphviz
digraph {
    fontname="Helvetica,Arial,sans-serif"
    //node [fontname="Helvetica,Arial,sans-serif"]
    //edge [fontname="Helvetica,Arial,sans-serif"]
    //rankdir="LR"
    node [fontsize=10, shape=record, height=0.25]
    edge [fontsize=10]
    virtio_dev_rx_single_packed -> vhost_enqueue_single_packed -> fill_vec_buf_packed
    -> vhost_flush_enqueue_shadow_packed
}
```