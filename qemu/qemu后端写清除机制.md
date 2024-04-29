
---
title: qemu后端写清除机制
date: 2023-12-21 14:53:25
categories:
- [qemu,设备模拟]
tags:
- 代码片段
---
```c
d->config[addr + i] = (d->config[addr + i] & ~wmask) | (val & wmask);
d->config[addr + i] &= ~(val & w1cmask); /* W1C: Write 1 to Clear */
```