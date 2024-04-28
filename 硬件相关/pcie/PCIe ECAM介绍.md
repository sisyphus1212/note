---
title: PCIe ECAM介绍
date: 2024-03-23 16:05:52
categories:
  - [接口协议]
tags:
- pcie
- mmio
---

ECAM 全称：PCI Express Enhanced Configuration Access Mechanism
ECAM是访问PCIe配置空间一种机制，PCIe配置空间大小是4k

4kbyte寄存器地址空间，需要12bit bit 0~bit11

Function Number bit 12~bit 14

Device Number bit 15~bit 19

Bus Number bit 20～bit 27

如何访问一个PCIe设备的配置空间呢

比如ECAM 基地址是0xd0000000

devmem 0xd0000000就是访问00:00.0 设备偏移0寄存器,就是Device ID和Vendor ID

devmem 0xd0100000就是访问01:00.0 设备偏移0寄存器

drivers/pci/ecam.c实现ECAM配置访问

# qemu 软件仿真shixian
```c
/*
 * PCI express ECAM (Enhanced Configuration Address Mapping) format.
 * AKA mmcfg address
 * bit 20 - 27: bus number
 * bit 15 - 19: device number
 * bit 12 - 14: function number
 * bit  0 - 11: offset in configuration space of a given device
 */
#define PCIE_MMCFG_SIZE_MAX             (1ULL << 28)
#define PCIE_MMCFG_SIZE_MIN             (1ULL << 20)
#define PCIE_MMCFG_BUS_BIT              20
#define PCIE_MMCFG_BUS_MASK             0xff
#define PCIE_MMCFG_DEVFN_BIT            12
#define PCIE_MMCFG_DEVFN_MASK           0xff
#define PCIE_MMCFG_CONFOFFSET_MASK      0xfff
#define PCIE_MMCFG_BUS(addr)            (((addr) >> PCIE_MMCFG_BUS_BIT) & \
                                         PCIE_MMCFG_BUS_MASK)
#define PCIE_MMCFG_DEVFN(addr)          (((addr) >> PCIE_MMCFG_DEVFN_BIT) & \
                                         PCIE_MMCFG_DEVFN_MASK)
#define PCIE_MMCFG_CONFOFFSET(addr)     ((addr) & PCIE_MMCFG_CONFOFFSET_MASK)

/* a helper function to get a PCIDevice for a given mmconfig address */
static inline PCIDevice *pcie_dev_find_by_mmcfg_addr(PCIBus *s,
                                                     uint32_t mmcfg_addr)
{
    return pci_find_device(s, PCIE_MMCFG_BUS(mmcfg_addr),
                           PCIE_MMCFG_DEVFN(mmcfg_addr));
}

static void pcie_mmcfg_data_write(void *opaque, hwaddr mmcfg_addr,
                                  uint64_t val, unsigned len)
{   ...
    PCIDevice *pci_dev = pcie_dev_find_by_mmcfg_addr(s, mmcfg_addr);
    uint32_t addr;
    uint32_t limit;
    ...
    addr = PCIE_MMCFG_CONFOFFSET(mmcfg_addr); // 寄存器偏移
    limit = pci_config_size(pci_dev); // PCIe配置空间大小是4k
    pci_host_config_write_common(pci_dev, addr, limit, val, len);
}
```

