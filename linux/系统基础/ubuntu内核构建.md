---
title: ubuntu内核构建
date: 2024-03-08 12:13:09
tags:
    - linux
    - ubuntu
---
```sh
#https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/jammy/

git  init
git remote add origin "git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/jammy"
git ls-remote  --tags
git fetch --depth 1 origin 948bd6b3f8eb95d50cf7acbd0f7d082ab7d8d9db
git checkout 948bd6b3f8eb95

或者
git clone git@github.com:sisyphus1212/Ubuntu-5.15.0-91.101.git

unset FAKEROOTKEY
fakeroot debian/rules clean
fakeroot debian/rules editconfigs
```