---
title: git 常用命令
date: 2020-11-01 14:31:09
categories:
- [其它]
tags:
- git
---

git 命令选项非常多，对于软件开发只需熟练掌握其中十几个就可以cover 住90% 的场景了

# git 常用操作
## 快速配置
`git config --global core.editor vim`
`git config --global user.name <用户名>`
`git config --global user.email <邮箱地址>`
`git config -l`
git config --global core.autocrlf true
## 仓库操作
### 分支操作
1. clone分支
`git clone <远程仓库的网址> -b <分支名称> <本地目录>`

1. 创建分支
`git checkout -b <分支名称>`

1. 删除分支
`git branch -d <分支名称> # 删除分支`
`git push origin --delete <远程分支名>`

1. 远程关联
`git remote set-url origin git@github.com:yourusername/yourrepo.git`
`git remote -v`

`git push <远程主机名> <本地分支名> <远程分支名>`
`git push origin lcj_qemu:master`

1. 合并与变基
`git merge <分支名称>`
`git cherry-pick <commit ID>`
`git rebase -i <commit ID>`
`git rebase master`
`git config pull.rebase true`

### 版本控制
1. 硬重置到远程分支最新状态：
`git reset --hard  origin/master`

1. 软重置到指定提交：
`git reset --soft <commit ID>`

1. 恢复文件
`git restore --staged <文件名>`
`git checkout <commit ID> -- <文件名>`

`git config --global url."http://192.168.2.114/".insteadOf "git@192.168.2.114:"`

## git 子模块
`git submodule add git@github.com:sisyphus1212/mypage.git mypage`

`git submodule init`

`git submodule update`

`git clone --recursive <主仓库的URL>`

`git submodule update --remote --recursive`

`git rm --cached medias`

`rm -rf .git/modules/medias`

`rm -rf medias`

`git config -f .git/config --remove-section submodule.medias/image_0`

`git submodule sync --recursive # 更新submodule的url(防止出现域名替换等问题）`

## git 大文件
`git lfs install`

`git lfs migrate import --include-ref=master --include="PCI_Express_System_Architecture_ocred.pdf"`

**git** lfs track **"*.psd"**

`git add .gitattributes`

