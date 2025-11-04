# Nvidia VirtIO Net Controller Daemon

VirtIO net controller enables users to create VirtIO-net emulated PCIe devices
in the system where the NVIDIA® BlueField® DPU is connected. This is done by the
virtio-net-controller software module present in the DPU. Virtio-net emulated
devices allow users to hot plug virtio-net PCIe PF Ethernet NIC devices and
virtio-net PCI VF Ethernet NIC devices in the host system where the DPU is
plugged in. By using this solution, virtio net device offloads traffic handling
from host CPU to the NIC of DPU.

# System Preparation

Use BlueField-2 as example, mst device should be adjusted accordingly otherwise.

## Prepare DPU for hotplug devices:
```sh
$ mst start
$ mlxconfig -d /dev/mst/mt41686_pciconf0 reset
$ mlxconfig -d /dev/mst/mt41686_pciconf0 s \
                PF_BAR2_ENABLE=0 \
                PER_PF_NUM_SF=1
$ mlxconfig -d /dev/mst/mt41686_pciconf0 s \
                PCI_SWITCH_EMULATION_ENABLE=1 \
                PCI_SWITCH_EMULATION_NUM_PORT=16 \
                VIRTIO_NET_EMULATION_ENABLE=1 \
                VIRTIO_NET_EMULATION_NUM_VF=0 \
                VIRTIO_NET_EMULATION_NUM_PF=0 \
                VIRTIO_NET_EMULATION_NUM_MSIX=16 \
                ECPF_ESWITCH_MANAGER=1 \
                ECPF_PAGE_SUPPLIER=1 \
                SRIOV_EN=0 \
                PF_SF_BAR_SIZE=8 \
                PF_TOTAL_SF=64
$ mlxconfig -d /dev/mst/mt41686_pciconf0.1 s \
                PF_SF_BAR_SIZE=10 \
                PF_TOTAL_SF=64
```

For transitional (virtio spec 0.95) device, the following options are needed:

```sh
VIRTIO_NET_EMULATION_PF_PCI_LAYOUT=1
VIRTIO_EMULATION_HOTPLUG_TRANS=1
```

Cold reboot host

## Prepare DPU for static devices with SR-IOV (504 VFs) support:
```sh
$ mst start
$ mlxconfig -d /dev/mst/mt41686_pciconf0 reset
$ mlxconfig -d /dev/mst/mt41686_pciconf0 s \
                PF_BAR2_ENABLE=0 \
                PER_PF_NUM_SF=1
$ mlxconfig -d /dev/mst/mt41686_pciconf0 s \
                PCI_SWITCH_EMULATION_ENABLE=0 \
                PCI_SWITCH_EMULATION_NUM_PORT=0 \
                VIRTIO_NET_EMULATION_ENABLE=1 \
                VIRTIO_NET_EMULATION_NUM_VF=126 \
                VIRTIO_NET_EMULATION_NUM_PF=4 \
                VIRTIO_NET_EMULATION_NUM_MSIX=4 \
                ECPF_ESWITCH_MANAGER=1 \
                ECPF_PAGE_SUPPLIER=1 \
                SRIOV_EN=1 \
                PF_SF_BAR_SIZE=8 \
                PF_TOTAL_SF=508 \
                NUM_OF_VFS=0
$ mlxconfig -d /dev/mst/mt41686_pciconf0.1 s \
                PF_TOTAL_SF=1 \
                PF_SF_BAR_SIZE=8
```

For transitional (virtio spec 0.95) device, the following option is needed:

```sh
VIRTIO_NET_EMULATION_VF_PCI_LAYOUT=1
```

Cold reboot host

**NOTE**:
1. To use static device, it is recommended to blacklist kernel module
virtio_pci and virtio_net from guest OS before proceeding. If those modules
are built into kernel image, they are not blacklistable. In such case, it's
recommended for users to have a way to start the virtio_net_controller from
DPU using SSH or serial/oob interfaces. This can be achieved by connecting
the RShim/serial cable to another host, or setting up the OOB interface.

    Normally, this is not needed as controller daemon starts automatically when
system boots.

2. For the systems which BIOS is not doing address space allocation for
unpopulated downstream ports of the pcie switch, __"pci=realloc"__ is needed
inside boot commandline.

    It is recommended to add __"pci=assign-busses"__ to boot command line when
creating more than 127 VFs. Without this option, errors below might appear
from host. And virio driver won't probe those devices as type is incorrect.
    ```sh
    [  617.382854] pci 0000:84:00.0: [1af4:1041] type 7f class 0xffffff
    [  617.382883] pci 0000:84:00.0: unknown header type 7f, ignoring device
    ```

3. To support more than 127 VFs, hotplug capability shall be disabled by changing mlxconfig options below:
```sh
    PCI_SWITCH_EMULATION_ENABLE=0
    PCI_SWITCH_EMULATION_NUM_PORT=0
```

4. Max number of VFs supported by each mst device is calculated by the formula
below based on mlxconfig options.
    ```sh
    max_num_vf = VIRTIO_NET_EMULATION_NUM_VF * VIRTIO_NET_EMULATION_NUM_PF
    ```
    VIRTIO_NET_EMULATION_NUM_VF is reserved for each PF out of VIRTIO_NET_EMULATION_NUM_PF.
    As of now, max_num_vf is 504 for BlueField-2. Note that, to support the
    maximal number of virtio net VFs, NUM_OF_VFS, VIRTIO_BLK_EMULATION_NUM_VF and
    VIRTIO_BLK_EMULATION_NUM_PF should all be 0.

# Building

Dependency:
    mlnx-ofed, mlnx-libsnap, libev, libev-devel, libmnl-devel

To build and install this daemon, depends on the packet, there are different
steps.

## Release Tarball

Follow steps below if you received the release tarball after untar:
```sh
$ ./build.sh -g [-d]       # configure [debug]
$ ./build.sh -b [-c] [-v]  # make [clean] [verbose] and make install
```

Or you can run all steps above in one shot.
```sh
$ ./build.sh
```

## Git Repository

Follow steps below if you're developing on top of the git repository.
```sh
$ ./build.sh -a            # autogen and apply patches
$ ./build.sh -g [-d]       # configure [debug]
$ ./build.sh -b [-c] [-v]  # make [clean] [verbose] and make install
```

Also you can run all steps above in one shot.
```sh
$ ./build.sh
```

Typically the autogen and configure steps only need to be done the first time
unless configure.ac or Makefile.am changes.

To build rpm/deb:
```sh
$ scripts/build_rpm.sh
$ tar -xvf *.gz
$ cd virtio-net-controller-<ver>
$ dpkg-buildpackage
```

# Usage

The controller has a systemd service running and a user interface tool to
communicate with the service.

* Service: virtio-net-controller.service
* User Interface: virtnet
* Log: controller is using system log system. Run:
```sh
  $ journalctl -u virtio-net-controller
```

## Controller Service

If controller is running on a DPU, the service is enabled by default. It starts
automatically. Run command below to check the status.

```sh
$ systemctl status virtio-net-controller.service
```

If daemon is not running, start controller by running command below.
Make sure to check the status after command starts.

```sh
$ systemctl start virtio-net-controller.service
```

To stop the daemon, first unload virtio-net/virtio-pci from host. Then run
command below from DPU:

```sh
$ systemctl stop virtio-net-controller.service
```

To enable the daemon automatically start, run:

```sh
$ systemctl enable virtio-net-controller.service
```
## Controller Recovery

It is possible to recover the control and data planes if communications are
interrupted so the original traffic can resume.

Recovery depends on the JSON files stored in __/opt/mellanox/mlnx_virtnet/recovery__
where each recovery file corresponds to each device (either PF or VF).

Following is an example of the file:

```json
{
  "port_ib_dev": "mlx5_0",
  "pf_id": 0,
  "function_type": "pf",
  "bdf_raw": 26624,
  "device_type": "hotplug",
  "mac": "0c:c4:7a:ff:22:93",
  "pf_num": 0,
  "sf_num": 2000,
  "mq": 1
}
```

These files should not be modified/deleted under normal circumstances.
However, if necessary, advanced users may tune settings or delete the file
to meet their requirements.

Note: users are responsible for the validity of the recovery files and should
only modify/delete them when the controller is not running.


## Controller Configuration Files

### Main Configuration

A config file __/opt/mellanox/mlnx_virtnet/virtnet.conf__ can be used to set
parameters for the service. If config file is changed while controller service
is running, the new config won't take effect until service is reloaded.

Currently supported options:

* ib_dev_p0:
        RDMA device(e.g, mlx5_0) which is used to create SF on port 0.
        This port is emu manager when is_lag is 0, default value is mlx5_0.
* ib_dev_p1:
        RDMA device(e.g, mlx5_1) which is used to create SF on port 1.
        Default value is mlx5_1.
* ib_dev_lag:
        RDMA lag device(e.g, mlx5_bond_0) which is used to create SF on lag.
        Default value is mlx5_bond_0, this port is emu manager when is_lag is 1.
        ib_dev_lag and ib_dev_p0/1 can't be configured simultaneously.
* ib_dev_for_static_pf:
        RDMA device(e.g, mlx5_0) which the static virtIO PF will be created on.
* is_lag:
        Whether or not LAG is used. Note that if lag is used, make sure to use
        the correct IB dev for static PF.
* recovery:
        Whether recovery feature is enabled. If not specified, recovery feature
        is enabled by default; to disable it, put recovery to 0.
* sf_pool_percent:
        Determines the initial SF pool size as the percentage of max_sf of
        HCA capability. The valid value is in [0, 100]. For instance, if this
        value is 5, it means a SF pool with 5% x max_sf will be created.
        0 means no SF pool will be reserved beforehand. Default is 0.
* sf_pool_force_destroy:
        Whether to destroy the SF pool. When set 1, the controller will destroy
        the SF pool when being stopped/restarted (and the SF pool will be recreated
        if sf_pool_percent is not 0 when starting), otherwise it won't. Default is 0.
* single_port:
        Whether the NIC has only single port. Default is 0 i.e. dual ports. Note
        this flag and "is_lag" are mutually exclusive.
* static_pf:
        mac_base: Base MAC address for static PFs. MACs are automatically assigned
        withfollowing pattern: mac_base->pf_0, mac_base + 1 -> pf_1, etc. Note that
        controller won't validate the MAC address (other than the length), user
        must ensure MAC is valid and unique.
        features: Virtio features to use. If not specified, a default one will be used.
* vf:
        mac_base: Base MAC address for VFs. MACs are automatically assigned with
        following pattern: mac_base->vf_0, mac_base + 1 -> vf_1, etc. Note that
        controller won't validate the MAC address (other than the length), user
        must ensure MAC is valid and unique.
        features: Virtio features to use. If not specified, a default one will be used.
        vfs_per_pf: The number of VFs will be created on each PF, and __this is
        mandatory if mac_base is specified__. Note this is not
        VIRTIO_NET_EMULATION_NUM_VF in mlxconfig, but
        vfs_per_pf <= VIRTIO_NET_EMULATION_NUM_VF.

Here is an example for non-lag(JSON format):

```json
{
  "ib_dev_p0": "mlx5_0",
  "ib_dev_p1": "mlx5_1",
  "ib_dev_for_static_pf": "mlx5_0",
  "is_lag": 0,
  "recovery": 1,
  "sf_pool_percent": 0,
  "sf_pool_force_destroy": 0,
  "static_pf": {
    "mac_base": "11:22:33:44:55:66",
    "features": "0x230047082b",
  },
  "vf": {
    "mac_base": "CC:48:15:FF:00:00",
    "features": "0x230047082b",
    "vfs_per_pf": 100
  }
}
```
Here is an example for lag(JSON format):

```json
{
  "ib_dev_lag": "mlx5_bond_0",
  "ib_dev_for_static_pf": "mlx5_bond_0",
  "is_lag": 1,
  "recovery": 1,
  "sf_pool_percent": 0,
  "sf_pool_force_destroy": 0
}
```

Here is an example for single_port (JSON format):

```json
{
  "ib_dev_p0": "mlx5_2",
  "ib_dev_for_static_pf": "mlx5_2",
  "single_port": 1
}
```

### Provider Configuration

To support different provider other than the default ACE, configuration files
located in __/opt/mellanox/mlnx_virtnet/providers__ can be used.

Format of config files:

* File name should be the provider name
* Two keywords are supported:
  1. provider: name of the provider, this has to be the accurate provider name,
     currently __ace__ and __dpa__ are the only valid options.
  2. score: weight score for this provider, the higher one will win if tie
* More than one config file can be used, each should have at most one provider
* In each config file, if more than one provider/score are defined, only the last
  one will take effect
* Each line can be commented out via "#"
* Empty line will be ignored
* Case insensitive, i.e. Provider=abc is same as provider=abc

A Sample:

```sh
ace.provider
# ace.so is one provider
Provider=ace
Score=100

dpa.provider
# dpa.so is another provider
Provider=dpa
Score=200
```

## Controller User Interface

Each command has its own help manual, e.g, _virtnet list -h_

1. list current devices

```sh
$ virtnet list
```

2. query detailed info of all current devices (-a) or a specific device (-p, -v or -u).
   --brief (or -b) can be used to ignore queue info.

```sh
$ virtnet query -a [-b]
$ virtnet query -p pf_id [-v vf_id] [-b]
$ virtnet query -u "VUID string" [-b]
```

3. Hotplug virtio net device with 1500 mtu and 3 virtio queues of depth 1024 entries with vlan control support, data path relies on mlx5_0:

```sh
$ virtnet hotplug -i mlx5_0 -f 0x80000 -m 0C:C4:7A:FF:22:93 -t 1500 -n 3 -s 1024
```

For transitional (virtio spec 0.95) device, "-l" or "--legacy" can be used.

```sh
$ virtnet hotplug -i mlx5_0 ... -l
```

Note: if Linux kernel version is < 4.0 or CentOS 6 or Ubuntu 12.04, --legacy is
mandatory. Otherwise, even with --legacy, the device will still be driven as a modern
one by default, unless the force_legacy flag is used for virtio_pci driver __on the host__,
as below:

```sh
$ modprobe -v virtio_pci force_legacy=1

or

Append virtio_pci.force_legacy=1 in kernel cmdline
```

Max number of virtio queues are bounded by the minimum of the numbers below

* `VIRTIO_NET_EMULATION_NUM_MSIX` from `mlxconfig -d <mst_dev> q`
* `max_virtq` from `virtnet list`

If hotplug succeeds, hotplug device info returns similar as below:

```json
{
  "bdf": "84:0.0",
  "id": 1,
  "transitional": 0,
  "sf_rep_net_device": "en3f0pf0sf1001",
  "mac": "0C:C4:7A:FF:22:93"
}
```
Add the SF interface to corresponding OVS bridge. Also, make sure the MTU of SF
rep is configured the same or greater than the virtio net device.

```sh
$ ovs add-port <bridge> en3f0pf0sf1001
```

4. Modify the MAC address of virtio physical device ID 0 (or with its "VUID string",
which can be obtained through virtnet list/query)

```sh
$ virtnet modify -p 0 device -m 0C:C4:7A:FF:22:93
$ virtnet modify -u "VUID string" device -m 0C:C4:7A:FF:22:93
```

5. Modify the max queue size of device

```sh
$ virtnet modify -p 0 -v 0 device -q 2048
$ virtnet modify -u "VUID string" device -q 2048
```

6. Modify the MSI-X number of VF device

```sh
$ virtnet modify -p 0 -v 0 device -n 8
$ virtnet modify -u "VUID string" device -n 8
```

7. Modify the queue options of virtio physical device ID 0

```sh
$ virtnet modify -p 0 queue -e event -n 10 -c 30
$ virtnet modify -u "VUID string" queue -e event -n 10 -c 30
```

__Note: to modify MAC, MTU, features, msix_num and max_queue_size, device
unbind and bind on guest OS is necessary, as follows:__

```sh
$ cd /sys/bus/pci/drivers/virtio-pci/
$ echo "bdf of device" > unbind

perform "virtnet modify ..." on controller side

$ echo "bdf of device" > bind
```

8. Unplug virtio physical device ID 0, only applicable to hotplug device

```sh
$ virtnet unplug -p 0
$ virtnet unplug -u "VUID string"
```

9. Live update controller

Install rpm/deb package

```sh
$ rpm -Uvh virtio-net-controller-x.y.z-1.mlnx.aarch64.rpm --force
$ dpkg --force-all -i virtio-net-controller-x.y.z-1.mlnx.aarch64.deb
```

Then perform following commands:

```sh
$ virtnet version        to check destination controller version
$ virtnet update -s      to start live update
$ virtnet update -t      to check live update status
```

## Limitations
1. When port MTU (p0/p1 from DPU) is changed(e.g, from 1500 to 9000) after
controller starts, it is required to restart controller service.