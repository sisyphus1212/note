---
title: 为qemu运行快速部署dhcp服务
date: 2023-12-21 11:10:21
categories:
- [qemu,设备模拟]
tags:
- virtio-net
- tap
- 网络设备
- dhcp
---
```sh
sudo mkdir -p /etc/systemd/system/docker.service.d
sudo touch /etc/systemd/system/docker.service.d/proxy.conf
cat << 'eof' > /etc/systemd/system/docker.service.d/proxy.conf
[Service]
Environment="HTTP_PROXY=http://172.17.0.1:20171/"
Environment="HTTPS_PROXY=http://172.17.0.1:20171/"
Environment="NO_PROXY=localhost,127.0.0.1,.example.com"
eof
systemctl daemon-reload
systemctl restart docker

dhcpd_cfg=/var/lcj/dhcpd.conf
mkdir -p `dirname $dhcpd_cfg`
cat <<'EOF' > ${dhcpd_cfg}
default-lease-time 600;
max-lease-time 7200;

subnet  172.17.0.0 netmask 255.255.255.0 {
    range  172.17.0.2 172.17.0.10;
    option domain-name-servers ns1.example.org, ns2.example.org;
    option domain-name "mydomain.example";
    option routers 172.17.0.1;
    option broadcast-address 172.17.0.1;
    default-lease-time 600;
    max-lease-time 7200;
}

EOF
chmod  0777 ${dhcpd_cfg}

sudo  docker run -itd  --name hadep-dhcp-server --net host  -v /var/lcj/:/data/ -d networkboot/dhcpd
docker update --restart always hadep-dhcp-server

```