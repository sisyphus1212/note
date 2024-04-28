---
title: githubpage+vscocd+hexo构建笔记博客
date: 2022-08-29 18:21:45
categories:
- [其它, 方法分享]
tags: 其它
---

# 组件
vscode: markdowon 编辑器
 vscode插件: Paste Image

github page: 静态网站托管

github action: 自动构建网站发布流水(hexo cl;hexo g;hexo d)

图床：github 仓库

# 解决方案
大体思路：将博客项目部署github 仓库，在该项目下关联一个图片仓库用作图床，利用Paste Image将图片自动生成markdown链接并将图片放到图床仓库中，然后利用该博客项目的仓库的action自动完成bolg编译构建发布

# 实操
1. 利用hexo构建出一套blog 工程项目：[详细参考](https://hexo.io/zh-cn/docs/setup)
2. 将工程项目关联系到github下
3. Paste Image配置：
    ```json
    "pasteImage.insertPattern": "${imageSyntaxPrefix}https://github.com/sisyphus1212/images/blob/main/${imageFileName}?raw=true${imageSyntaxSuffix}"
    "pasteImage.path": "${currentFileDir}/../media"
    "pasteImage.basePath": "${currentFileDir}"
    ```
4. github action:
```
# This is a basic workflow to help you get started with Actions

name: Blog deploy

# Controls when the workflow will run
on:
  # Triggers the workflow on push or pull request events but only for the main branch
  push:
    branches: [ master ]

  # Allows you to run this workflow manually from the Actions tab
  workflow_dispatch:

# A workflow run is made up of one or more jobs that can run sequentially or in parallel
jobs:
  # This workflow contains a single job called "build"
  build-and-deploy:
    # The type of runner that the job will run on
    runs-on: ubuntu-latest

    # Steps represent a sequence of tasks that will be executed as part of the job
    steps:
      # Checks-out your repository under $GITHUB_WORKSPACE, so your job can access it
      - uses: actions/checkout@v2

      # Runs a set of commands using the runners shell
      - name: Set Node
        uses: actions/setup-node@v2
        with:
          node-version: '12.13'

      - name: 缓存 Hexo
        uses: actions/cache@v1
        id: cache
        with:
          path: node_modules
          key: ${{runner.OS}}-${{hashFiles('**/package-lock.json')}}

      - name: Npm Install
        if: steps.cache.outputs.cache-hit != 'true'
        run: |
          npm install hexo-cli -g
          npm install --save

      - name: Set Key
        env:
          DEPLOY_KEY: ${{ secrets.HEXO }}
        run: |
          mkdir -p ~/.ssh
          echo "$DEPLOY_KEY" > ~/.ssh/id_rsa
          chmod 600 ~/.ssh/id_rsa
          ssh-keyscan github.com >> ~/.ssh/known_hosts
          git config --global user.email "sisyphus12@aliyun.com"
          git config --global user.name "sisyphus12"

      - name: Hexo Deploy
        run: |
          hexo clean
          hexo generate
          gulp
          hexo deploy

```

# 项目参考地址
https://github.com/sisyphus1212/mypage

# github action 配置
https://sisyphus1212.github.io/2023/11/01/github_action/