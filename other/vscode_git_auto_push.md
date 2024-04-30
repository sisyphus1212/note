---
title: vscode-git-自动push
date: 2024-03-11 19:40:53
categories:
- [其它]
tags: vscode
---

在用vscode写笔记时可以利用File Watcher 插件来实现 git的自动pull 和 push，其原理是每当发现文件发生变更保存就会触发对应的git 命令
![filewatcher setting.json](../../../../../medias/images_0/vscode_git_auto_push_image-2.png)
配置如下：
```json
    "filewatcher.commands": [
        {
            "match": "\\.*",
            "isAsync": true,
            "cmd": "cd ${fileDirname} && git pull && git add .* && git commit -m auto_update && git push -f",
            "event": "onFileChange"
        }
    ]
```

配置成功后可以看到如下状态:

![File Watcher status](../../../../../medias/images_0/vscode_git_auto_push_image.png)
