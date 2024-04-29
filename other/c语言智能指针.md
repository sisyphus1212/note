---
title:  c语言-智能指针的用法
date: 2023-11-01 18:21:45
categories:
- [c语言]
tags: 其它
---

# G_DEFINE_AUTOPTR_CLEANUP_FUNC 智能指针的用法
## 上代码：
```c
#include <glib.h>
#include <stdio.h>

typedef struct {
    int data;
} MyObject;

// MyObject 的创建函数
MyObject* my_object_new(int data) {
    MyObject* obj = g_new(MyObject, 1);
    obj->data = data;
    return obj;
}

// MyObject 的清理函数
void my_object_free(MyObject* obj) {
    if (!obj) {
        return;
    }
    printf("Cleaning up MyObject with data: %d\n", obj->data);
    g_free(obj);
}

// 为 MyObject 类型定义自动指针的清理函数
G_DEFINE_AUTOPTR_CLEANUP_FUNC(MyObject, my_object_free)

// 一个创建并使用 MyObject 实例的函数
void use_my_object() {
    g_autoptr(MyObject) obj = my_object_new(42);
    printf("MyObject data inside function: %d\n", obj->data);
    // 当这个函数结束时，obj 将超出作用域，触发 my_object_free
}

int main(int argc, char *argv[]) {
    use_my_object();
    // 当 use_my_object 返回后，应该看到 my_object_free 被调用的信息
    printf("Returned from use_my_object\n");

    return 0;
}
```
## 编译：
```bash
gcc -o my_program my_program.c `pkg-config --cflags --libs glib-2.0`
```
## 运行结果
```bash
[root@localhost test]# ./my_program
MyObject data inside function: 42
Cleaning up MyObject with data: 42
Returned from use_my_object
```