---
title: linux kernel debugfs
date:2024-03-11 19:43:57
tags: 驱动
---

内核里面很多模块的数据结构定义在c 文件中，如果我们外部模块想要访问这些模块中的变量，就需要有对应头文件，但是如果手动拷贝，一旦内核发生变更。就会非常麻烦，那有啥办法解决吗，我们可以考虑直接使用 btf 信息来达到这个目的：
![btf virtio_net](../../../../../../medias/images_0/linux-kernel-debugfs_image.png)