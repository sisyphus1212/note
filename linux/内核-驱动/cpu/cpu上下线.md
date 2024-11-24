# CPU 的上下线
```c
echo 0 > /sys/devices/system/cpu/cpuX/online  # 下线 CPU X
echo 1 > /sys/devices/system/cpu/cpuX/online  # 上线 CPU X
```

