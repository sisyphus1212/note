---
title: pcie 入门
date: 2023-09-20 10:04:03
index_img: https://img-en.fs.com/community/upload/wangEditor/202003/24/_1585046553_TZOmBePO8Z.jpg
categories:
  - [linux,网络开发]
tags:
 - kernel network
---
# pcie 设备类型

```
    ----------------------------------------------------------------------------
        |                |                    |                  |
   -----------   ------------------   -------------------   --------------
   | PCI Dev |   | PCIe Root Port |   | PCIe-PCI Bridge |   |  pxb-pcie  |
   -----------   ------------------   -------------------   --------------
    (1) PCI Devices (e.g. network card, graphics card, IDE controller),
        not controllers. Place only legacy PCI devices on
        the Root Complex. These will be considered Integrated Endpoints.
        Note: Integrated Endpoints are not hot-pluggable.

        Although the PCI Express spec does not forbid PCI Express devices as
        Integrated Endpoints, existing hardware mostly integrates legacy PCI
        devices with the Root Complex. Guest OSes are suspected to behave
        strangely when PCI Express devices are integrated
        with the Root Complex.

    (2) PCI Express Root Ports (pcie-root-port), for starting exclusively
        PCI Express hierarchies.

    (3) PCI Express to PCI Bridge (pcie-pci-bridge), for starting legacy PCI
        hierarchies.

    (4) Extra Root Complexes (pxb-pcie), if multiple PCI Express Root Buses
        are needed.
```

# PCIE 规格

Each PCI domain：256 buses

A PCI Express Root bus: 32 devices.

A single PCI Express to PCI Bridge: 32 slots

each PCI Express Root Port : 8 functions

The PCI Express Root Ports and PCI Express Downstream ports are seen by
Firmware/Guest OS as PCI-PCI Bridges.

![alt text](../../../../../medias/images_0/pcie_虚拟化_1701778456275.png)

![alt text](../../../../../medias/images_0/pcie_虚拟化_1701778440662.png)

![alt text](../../../../../medias/images_0/pcie_虚拟化_1701778510427.png)

![alt text](../../../../../medias/images_0/pcie_虚拟化_1701778479591.png)

![alt text](../../../../../medias/images_0/pcie_虚拟化_1701778582404.png)

# pcie 拓扑

# pcie 枚举

# pcie 热拔插

any devices plugged into Root Complexes cannot be hot-plugged/hot-unplugged:
    (1) PCI Express Integrated Endpoints
    (2) PCI Express Root Ports
    (3) PCI Express to PCI Bridges
    (4) pxb-pcie


### ACPI 基础的 PCI 热插拔

* PCI设备可以通过ACPI（高级配置和电源接口）技术热插拔到PCI-PCI桥接器中。ACPI是一种开放标准，用于设备配置和电源管理，由操作系统控制。

### SHPC 基础的 PCI 热插拔

* PCI设备也可以通过SHPC（Standard Hot Plug Controller）技术热插拔到PCI Express到PCI桥接器中。SHPC是一种硬件机制，允许用户不关闭电脑的情况下，添加或移除PCI设备。

### PCI Express 原生热插拔

* PCI Express设备可以通过PCI Express的原生热插拔功能直接热插拔到PCI Express Root Ports（和PCI Express Downstream Ports）。这是PCI Express规范的一部分，允许设备被安全地添加或移除，而无需关闭或重新启动系统。
