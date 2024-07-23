
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
used = !!(flags & VRING_PACKED_DESC_F_USED);
avail = !!(flags & VRING_PACKED_DESC_F_AVAIL);
avail == used && used == vq->vq_packed.used_wrap_counter;
}