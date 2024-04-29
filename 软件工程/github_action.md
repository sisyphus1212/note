---
title: github action使用介绍
date: 2022-10-01 17:38:45
categories:
- [其它]
tags:
- github
- ci/cd
---
# github action介绍

github action 是一个由 GitHub 提供的自动化工作流程平台(ci/cd)，用于帮助开发人员构建、测试、部署和管理其软件项目, 说人话就是一个能自动化触发的脚本运行环境, 比如：github action检测到我们push代码后,可以自动检查代码合规性，自动编译代码并打包发布,下图是github_action 运行逻辑图:
![github_action_逻辑图](../../../../../medias/images_0/github_action_1698980204213.png)
github 的每个repositories 仓库都可以关联到一个github action,  其工作流程大体分三步：

1. 初始化repositories 仓库关联的github action
2. github action 获取repositories 仓库
3. 运行任务，发布部署

## 初始化repositories 仓库关联的github action

在github 上我们要关联上一个github action， 可以在项目里加一个.github/workflows文件夹，并在里面添加yml 配置文件实现，这里的yml 文件名随意配置
![yml配置文件](../../../../../medias/images_0/github_action/1698981649117.png)
![yml配置文件](../../../../../medias/images_0/github_action_1698981649117.png)
举个例子，这是我github page 自动化部署的配置文件[dp.yml](https://github.com/sisyphus1212/mypage/blob/74d506540d6bf3ec4b062a1b05bd332d3d8844e9/.github/workflows/dp.yml)

像这些run标签下就可以任意使用shell 命令
![github action run 命令](../../../../../medias/images_0/github_action_1698982166108.png)
## github action 获取repositories 仓库
使用这玩意时最头疼的问题就是各种权限问题，这里来给大家说明一下如何配置。

我们要使用github action 完成我们期望的任务，首先就得让github action能自动访问我们repositories，对于与项目关联的action 我们可以直接使用这个语句：
![actions/checkout@v2](../../../../../medias/images_0/github_action_1698982686133.png)
但是往往我们的项目还会关联到其它项目，这样github action 去访问关联的其它项目时就会出现权限问题，github提供的解决方案是通过tocken 或者非对称秘钥解决，非对称秘钥是一种比较方便的方式，这里主要就介绍这种方式。
![私钥/公钥](../../../../../medias/images_0/github_action_1698983377964.png)
要github action对repositories读写访问，我们先得给项目分一个公钥，然后在将对应私钥传递给github action，下面就是一个repositories给配置公钥和传递私钥的地方：
environments：配置私钥(可以配置不同项目的私钥)
Deploy keys: 配置公钥
![github action秘钥](../../../../../medias/images_0/github_action_1698983192255.png)
注意这里的environments 配置可以配置不同项目的私钥，这样github action 就能访问不同的项目了

通过environments配置，在yml配置文件中，我们就能够获取到对应的秘钥，如下图：
![私钥](../../../../../medias/images_0/github_action_1698984096009.png)
下面是私钥的git 使用方法
![私钥的git 使用](../../../../../medias/images_0/github_action_1698984216948.png)
通过上面公私钥的生成我们就能够自动的对项目进行操作(包括读写), 不过除了上面公私钥的配置，我们在github 里面为项目配置的这些权限也会影响到github action 的访问
![其他权限](../../../../../medias/images_0/github_action_1698984595315.png)

## 公钥/私钥 生成方法
```shell
ssh-keygen -t rsa -b 4096 -m PEM -f  ./id_rsa
```

## 运行任务，发布部署
这里就比较简单了，直接在run 标签下编写对应的shell  命令就行

