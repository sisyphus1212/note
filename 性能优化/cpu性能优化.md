---
title: cpu性能优化
date: 2024-09-24 15:07:24
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,性能调优]
tags:
 - kernel_network
 - vdpa
---

# 上下文切换
1. 通用寄存器：EAX, EBX, ECX, EDX，EBP，ESP
2. 数据寄存器：CS（代码段寄存器），DS（数据段寄存器），SS（堆栈段寄存器）
3. 指令寄存器：EIP（指令指针），EFLAGS：保存CPU的状态标志
4. cr3: 页表基地寄存器
5. tlb 开销， cache 开销

## 自愿切换
1. 睡眠, io，内存资源不足
## 非自愿切换
2. 时间片，中断

## 上下文切换观测
pidstat -w -t -p `pidof top` 1
![alt text](../../medias/images_0/cpu性能优化_image.png)
