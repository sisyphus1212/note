![alt text](../../medias/images_0/qom模型_image.png)

qemu采用了模块机制。总共有5中模块定义在include/qemu/module.h 中
typedef enum {
    MODULE_INIT_BLOCK,
    MODULE_INIT_OPTS,
    MODULE_INIT_QAPI,
    MODULE_INIT_QOM,
    MODULE_INIT_TRACE,
    MODULE_INIT_MAX
} module_init_type;

#define block_init(function) module_init(function, MODULE_INIT_BLOCK)
#define opts_init(function) module_init(function, MODULE_INIT_OPTS)
#define qapi_init(function) module_init(function, MODULE_INIT_QAPI)
#define type_init(function) module_init(function, MODULE_INIT_QOM)
#define trace_init(function) module_init(function, MODULE_INIT_TRACE)

QOM是QEMU在C的基础上自己实现的一套面向对象机制，负责将device、bus等设备都抽象成为对象。对象的初始化分为四步：

将 TypeInfo 注册为 TypeImpl
实例化 ObjectClass
实例化 Object
添加 Property

```graphviz
digraph {

device -> type
device -> instance
device -> class

type -> type_new

ModuleEntry ->

}


net_vhost_user_init ->