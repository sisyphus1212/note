# 用户态接口
```c
root@vm0:~# cat /sys/kernel/debug/irq/domains/DMAR-MSI
name:   DMAR-MSI
 size:   0
 mapped: 1
 flags:  0x00000013
 parent: VECTOR
    name:   VECTOR
     size:   0
     mapped: 41
     flags:  0x00000003
Online bitmaps:        8
Global available:   1580
Global reserved:      13
Total allocated:      28
System: 38: 0-19,32,50,128,236,240-242,244,246-255
     | CPU | avl | man | mac | act | vectors
         0   197     0     0    4  33-35,48
         1   197     0     0    4  33-36
         2   197     0     0    4  33-36
         3   197     0     0    4  33-36
         4   198     0     0    3  33-35
         5   198     0     0    3  33-35
         6   198     0     0    3  33-35
         7   198     0     0    3  33-35
````
