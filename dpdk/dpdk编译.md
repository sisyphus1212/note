---
title: dpdk 编译
date: 2023-03-19 10:55:27
categories:
- [dpdk]
tags:
- dpdk
- 网络开发
---

# 依赖安装
```shell
apt-get install libevent-dev -y  or dnf install libevent-devel -y
apt-get install pkg-config  -y or  dnf install pkg-config -y
pip3 install pyelftools

```

# meson ninja 安装
```shell
# 方法1：
yum install ninja-build

# 方法2：
wget https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip
python3 -m pip install meson

# 方法3：
python3 -m pip install meson
python3 -m pip install ninja
```

# dpdpk 编译
## 无脑编译
```shell
meson build
cd build
ninja
meson install
ldconfig
```

## 调整构建选项
```shell
meson setup -Dexamples=l2fwd,l3fwd build

#-m32CFLAGSLDFLAGS-Dc_args=-m32-Dc_link_args=-m32pkg-configPKG_CONFIG_LIBDIR
meson setup -Dc_args='-m32' -Dc_link_args='-m32' build

两种方式：
1. 最初配置build文件夹时传递：meson setup -Dbuildtype=debug    -Ddisable_drivers=net/ldma3,net/ysk2 -Dexamples=vdpa build
2. 配置build文件夹后传递：meson configure -Dbuildtype=debug  -Ddisable_drivers=net/ldma3,net/ysk2
```
meson setup -Dbuildtype=debug  -Ddisable_drivers=net/dpaa,net/dpaa2,net/memif,net/cnxk,net/bnx2x,net/mvpp2,net/iavf,net/ice,net/e1000,net/ena,net/enetc,net/enetfec,net/enic,net/failsafe,net/fm10k,net/gve,net/hinic,net/hns3,net/i40e,net/idpf,net/igc,net/ionic,net/ixgbe build
参考：
https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html