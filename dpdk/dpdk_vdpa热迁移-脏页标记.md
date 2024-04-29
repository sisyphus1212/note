---
title: dpdk_vdpa热迁移-脏页标记
date: 2022-03-19 10:55:27
categories:
- [dpdk]
tags:
- dpdk
- 网络开发
- vdpa
- 脏页标记
---

```c
static __rte_always_inline void
vhost_log_page(uint8_t *log_base, uint64_t page)
{
	vhost_set_bit(page % 8, &log_base[page / 8]);
}
```
log_base 每bit代表一个page, 0表示没有改变过，1表示改变过

## 例子:
假设 page 的值为 9，这意味着我们需要标记第 9 个页面已修改。
计算 page / 8 得到 1，表示log_base[1]中存储第 9 个页面的状态。
计算 page % 8 得到 1，表示第 9 个页面的状态用log_base[1]中的bit 1存储。

