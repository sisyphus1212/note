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
Environment="HTTP_PROXY=http://192.168.2.71:20171/"
Environment="HTTPS_PROXY=http://192.168.2.71:20171/"
Environment="NO_PROXY=localhost,127.0.0.1,.example.com"
eof
systemctl daemon-reload
systemctl restart docker

dhcpd_cfg=/var/lcj/dhcpd.conf
mkdir -p `dirname $dhcpd_cfg`
cat <<'EOF' > ${dhcpd_cfg}
default-lease-time 600;
max-lease-time 7200;

subnet  182.16.2.0 netmask 255.255.255.0 {
    range  182.16.2.125 182.16.2.250;
    option domain-name-servers ns1.example.org, ns2.example.org;
    option domain-name "mydomain.example";
    #option routers 182.16.2.11;
    option broadcast-address 182.16.2.11;
    default-lease-time 600;
    max-lease-time 7200;
}

EOF
chmod  0777 ${dhcpd_cfg}

sudo  docker run -itd  --name hadep-dhcp-server --net host  -v  `dirname $dhcpd_cfg`:/data/ -d networkboot/dhcpd
docker update --restart always hadep-dhcp-server

```
scp lcj@192.168.2.71:/home/lcj/hadep-dhcp-server-image.tar ./hadep-dhcp-server-image.tar
docker load -i ./hadep-dhcp-server-image.tar
lcj@ps-aux