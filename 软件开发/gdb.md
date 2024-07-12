---
title: 常用调试方法
date: 2024-07-11 15:01:09
categories:
- [其它]
tags:
- git
---

# gdb
```shell
handle SIGUSR1 nostop noprint pass

set print array-indexes on
set print pretty on

# watch point
wa a thread 2
aw a
rw a
```
## 多进程调试
![alt text](../../medias/images_0/gdb_image.png)
