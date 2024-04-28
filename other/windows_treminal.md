---
title: windows terminal技巧
date: 2022-08-29 18:21:45
categories:
- [其它, 方法分享]
tags: 其它
---
windows terminal 配合 wsl 可以玩出很多技巧来：

# ssh密码登录 和 sftp支持

```shell
wsl -e sshpass -p 12345 sftp -P 22 root@10.0.25.19
wsl -e sshpass -p 12345 ssh -t root@10.0.25.19 -p 22
```
![windows_treminal cfg](../../../../../medias/images_0/windows_treminal_1701065028724.png)
