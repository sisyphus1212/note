---
title: testpmd使用
date: 2023-03-19 10:55:27
categories:
- [dpdk]
tags:
- dpdk
- 网络开发
---
```sh
./build/app/dpdk-testpmd -a 0000:0d:00.0 --nb-cores=2 -- --eth-peer=0,04:3f:72:e4:88:5b --tx-udp=11111,22222 --tx-ip=1.1.1.1,2.2.2.15   --forward-mode=txonly --txpkts=1500 --burst=64 --txq=4

./build/app/dpdk-testpmd -a 0000:0d:00.0 -m 2G  -l 0-2 -n 4 -- --eth-link-speed 25G --eth-peer=0,04:3f:72:e4:88:5b --tx-udp=11111,22222 --tx-ip=1.1.1.1,2.2.2.15   --forward-mode=txonly --txpkts=1500 --burst=64 --txq=4 --txd=1024 --flowgen-flows=8  --nb-cores=2 --txonly-multi-flow --display-xstats tx-test --stats-period 1 --record-core-cycles --record-burst-stats

./build/app/dpdk-testpmd -a 0000:0d:00.1 --file-prefix=testpmd -m 2G  -l 0-7 -n 4 -- \
                --total-num-mbufs=103000 --mbcache=512 -a --rss-udp \
                --forward-mode=macswap --nb-cores=4 --txonly-multi-flow \
                --eth-link-speed 100G --txpkts=1500 --burst=64 --txq=4 --txd=1024 --rxq=4 --rxd=1024 \
                --eth-peer=0,04:3f:72:e4:8c:13 --tx-ip=1.1.1.1,2.2.2.15 \
                --display-xstats tx-test --stats-period 1 --record-core-cycles --record-burst-stats


./build/app/dpdk-testpmd -a 0000:0d:00.1 --file-prefix=testpmd -m 2G  -l 0-7 -n 4 -- \
                --total-num-mbufs=103000 --mbcache=512 -a --rss-udp \
                --forward-mode=macswap --nb-cores=4 \
                --eth-link-speed 100G --txpkts=1500 --burst=64 --txq=4 --txd=1024 --rxq=4 --rxd=1024 \
                --eth-peer=0,04:3f:72:e4:8c:13 --tx-ip=1.1.1.1,2.2.2.15 \
                --display-xstats tx-test --stats-period 1 --record-core-cycles --record-burst-stats

./build/app/dpdk-testpmd -a 0000:0d:00.1 --file-prefix=testpmd -m 2G  -l 0-7 -n 4 -- \
                --total-num-mbufs=103000 --mbcache=512 -a --rss-udp \
                --forward-mode=macswap --nb-cores=4 \
                --eth-link-speed 100G --txpkts=1500 --burst=64 --txq=1 --txd=1024 --rxq=1 --rxd=1024 \
                --eth-peer=0,04:3f:72:e4:8c:13 --tx-ip=1.1.1.1,2.2.2.15 \
                --display-xstats tx-test --stats-period 1 --record-core-cycles --record-burst-stats

./build/app/dpdk-testpmd  -a 0000:0d:00.0 --proc-type=auto  -m 2G  -l 0-1 -- --eth-link-speed 100G --eth-peer=0,04:3f:72:e4:88:5b --tx-udp=11111,22222 --tx-ip=1.1.1.1,2.2.2.15   --forward-mode=txonly --txpkts=1500 --burst=64 --txq=4 --txd=1024 --flowgen-flows=8   --txonly-multi-flow --display-xstats tx-test --stats-period 1 --num-procs=2  --proc-id=0

./build/app/dpdk-testpmd  -a 0000:0d:00.0 --proc-type=auto  -m 2G  -l 2-3 -- --eth-link-speed 100G --eth-peer=0,04:3f:72:e4:88:5b --tx-udp=11111,22222 --tx-ip=1.1.1.1,2.2.2.15   --forward-mode=txonly --txpkts=1500 --burst=64 --txq=4 --txd=1024 --flowgen-flows=8   --txonly-multi-flow --display-xstats all --stats-period 1 --num-procs=2  --proc-id=1

```

```
port stop all
set fwd txonly
set  eth-peer 0 04:3f:72:e4:88:5b
port config all txq 4
port config all txd 1024
port config all burst 64
port config 0 tx_offload all on
set record-burst-stats on
set record-core-cycles on
set txpkts 1500
set nbcore 2
set corelist 3,1
csum set ip hw on
show port 0 gso
show config rxtx
show config fwd
port start all
start
show fwd stats all
```

sysctl -w net.ipv4.ipfrag_high_thresh=812000000
sysctl -w net.ipv4.ipfrag_low_thresh=446464000