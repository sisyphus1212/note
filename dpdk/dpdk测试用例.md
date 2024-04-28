---
title: dpdk测试用例
date: 2023-12-21 14:53:25
categories:
- [dpdk, 测试用例]
tags:
- dpdk
- 测试用例
- virtio_net
- mbuf
- dpdk
---

# 编译运行
打开./dpdk-stable-22.11.1/app/test/meson.build中的autotest
```sh
sudo  meson test -C build mempool_autotest
sudo gdb -args ./build/app/test/dpdk-test

```