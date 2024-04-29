---
title:  frpc极简部署
date: 2024-03-18 10:39:03
categories:
- [工具]
tags: frp
---

```shell
remote_port=14
wget  https://gh-proxy.com/https://github.com/fatedier/frp/releases/download/v0.55.1/frp_0.55.1_linux_amd64.tar.gz
tar -xvf frp_0.55.1_linux_amd64.tar.gz
cp frp_0.55.1_linux_amd64/frpc /usr/local/bin/frpc
cat << eof > /usr/lib/systemd/system/frpc.service
[Unit]
Description=frps server daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/frpc -c /usr/share/frp/frpc.ini
KillMode=process
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
eof

mkdir -p  /usr/share/frp

cat << eof >  /usr/share/frp/frpc.ini
[common]
server_addr="192.168.2.71"
server_port = 7000
token = b9eb1007-2d57-4225-951b-d5883134fc35
[ssh_${remote_port}]
type = tcp
local_ip = 0.0.0.0
local_port = 22
remote_port = 70${remote_port}
eof
systemctl start frpc
systemctl enable frpc
```