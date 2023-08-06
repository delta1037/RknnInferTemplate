# RknnInferTemplate
RKNN模型推理部署模板：将数据的输入输出（不同模型适配不同的输入源和做输出结果的处理）做成易于替换的插件的形式，将模型调度部分（通用部分）单独封装起来，减小模型部署工作量。

# 一、简介

> 首先需要说明的是采购的时 RK3568 的板子，所以部署模板的设计会以 RK3568 为基准，后续会做其它型号的兼容。

为什么使用 NPU 推理部署：板卡上包含了 CPU 算力和 NPU 算力，其中 NPU 算力**仅**可用于加速推理（作为边缘设备，推理应用更广泛一些）。于是为了充分使用板卡上的算力，可以考虑将 CPU 算力用于模型的训练（对于有需要在板卡上训练的场景来说）；将 NPU 算力用于模型的推理应用（一般放在边缘上的模型都会做推理，所以推理场景更广泛），而且将模型训练和模型推理使用的算力分开，也能让推理更顺畅（不会因为突如其来的模型训练占用算力导致不能推理，另外 NPU 的速度相对 CPU 来说更快一些，大约快了 CPU 的 50%（来源于[网友的测试](https://zhuanlan.zhihu.com/p/529861266)））。

## 1.1 需求简介

有通用模板的需求是因为在 6G 项目中购置了一批板卡，后续将在板卡上做训练和运行模型；并且加了一个 ROCK PI 的开发群，群内大多都对模型的 NPU 推理部署有需求但是对 C++ 和多线程不是很了解，于是有做一个通用模板来部署模型的想法，**将 NPU 推理调度流程与输入输出处理流程分离，降低在推理调度上的工作量**。

ROCK PI 上使用 NPU 推理的模型需要做模型转换，转换后的模型也都基本是一致的，于是调用模型推理的方式也基本是一样的，所以可以将==调度的部分==封装起来做成一个模板。在经过在开发群的一段实际窥屏后，整理了对模板的以下**要求**：

* 模型推理调度功能：能够同时支持多个输入源，和多线程推理（这也是比较有挑战性的一点，需要考虑线程之间的协同工作）
* 模板对不同需求的适应性：不同的模型有不同的数据源，包括文本数据，图像数据，视频数据（实时的/非实时的），甚至语音数据，所以需要考虑不同类型输入的接入，已经异类输入的可扩展性。

## 1.2 需求分析

从目标上来看，推理模板的**要点在于推理调度与输入输出流程分离**，将输入输出流程剥离出去之后就剩下了模型调度问题。

以之前的经验来看，推理调度与输入输出分离的设计可以参考 [FIO 软件中的插件式设计](http://www.delta1037.cn/2021/C_C++/C-C++%E6%8F%92%E4%BB%B6%E5%BC%8F%E8%AE%BE%E8%AE%A1/)，将输入输出的部分做成插件，推理调度部分做成可以加载插件的主程序，这样就能实现同一套推理调度，但是可以有不同的数据源和输出处理。

> FIO 软件中的插件式设计我已经调研了好久了，终于有了用武之地。我很中意这个设计，本来这个项目可以给学弟来练手，但是我等不及就开始做了。

推理调度部分是多线程之间的协同，需要封装好协同处理的队列（主要是数据结构的定义，使其具有可扩展性）。队列处理线程部分也可以使用线程池来做，但是我想这样队列产生线程是零散的，处理线程单独封装有点不雅观，索性可以散装放在推理调度部分。

# 二、设计

以下主要对架构设计和细节设计做详细说明，而设计的主要原则依赖于具体的需求。具体的需求我参考了 RK 官方给出的一些 RKNPU 使用样例，包括一个比较简单的 [rknn_mobilenet_demo](https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_mobilenet_demo) 和 [rknn_yolov5_demo](https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_yolov5_demo)；以及官方的 SDK 使用说明，使用说明里包含了 RKNN 模型的推理流程。

## 2.1 架构

架构设计图在：[RKNN 部署模板设计-架构图](siyuan://plugins/siyuan-plugin-excalidrawexcalidraw?icon=iconExcalidraw&title=RKNN%E9%83%A8%E7%BD%B2%E6%A8%A1%E6%9D%BF%E8%AE%BE%E8%AE%A1-%E6%9E%B6%E6%9E%84%E5%9B%BE&data=%7B%22name%22:%22RKNN%E9%83%A8%E7%BD%B2%E6%A8%A1%E6%9D%BF%E8%AE%BE%E8%AE%A1-%E6%9E%B6%E6%9E%84%E5%9B%BE%22%7D)

整体的架构图如下，主要划分为调度程序和插件程序两个部分。其中插件程序主要完成数据的输入处理和推理结果的输出处理；调度程序包括了核心的推理调度模块，对推理流程进一步封装的模型管理模块，最后是一个插件管理模块做插件的加载和校验。调度程序在插件加载之后通过插件接口与插件程序通信。

![image](https://github.com/delta1037/RknnInferTemplate/tree/main/assets/architecture.png)

## 2.2 细节

### 插件接口

插件接口部分是整个程序的核心部分，这部分关系到插件程序如何被调度程序调用。首先对调度程序如何使用插件做简要分析，插件中最重要的部分就是获取输入数据和设置推理的输出结果；另外还有插件程序向调度程序设置调度配置（这部分主要是调度程序的线程个数，以及输出结果类型的定制），调度程序向插件发送插件运行配置（这部分主要是加载的模型参数）；最后需要包含插件初始化部分和反初始化做模型内部针对每个线程的配置管理。根据 FIO 中的插件式设计，插件接口定义一个函数结构体来做插件中函数的定义。结构体定义如下：

```cpp
struct PluginStruct {
    // 插件名称
    const char *plugin_name;
    // 插件版本
    int plugin_version;

    // 从插件中获取调度配置
    int (*get_config)(PluginConfigGet *plugin_config);

    // 给插件设置运行配置
    int (*set_config)(PluginConfigSet *plugin_config);

    // 插件初始化
    int (*init)(struct ThreadData *);
    // 插件反初始化
    int (*uninit)(struct ThreadData *);

    // 收集输入数据和释放输入资源
    int (*rknn_input)(struct ThreadData *, struct InputUnit *);
    int (*rknn_input_release)(struct ThreadData *, struct InputUnit *);

    int (*rknn_output)(struct ThreadData *, struct OutputUnit *);
};
```

`struct ThreadData` 是每个调度线程独有的数据，上面所有包含该结构的函数都会被数据输入线程和结果输出线程调用；未包含该结构体的函数是全局类型的，只会调用一次。

### 插件程序

在插件程序部分主要介绍如何使用插件接口来完成一个完整插件程序的撰写（只涉及到接口的功能方面）。

#### 名称和版本

名称和版本是为了做插件的规范化管理。

#### 配置设置与获取

配置是双向的，插件程序需要给调度程序提供必要的参数来启动调度（调度线程的个数），调度程序也必须给插件提供必要的模型信息来让插件给出合适的输入格式。`get_config` 是给调度程序设置配置，`set_config` 是给插件程序设置配置，其中 `get_config` 先于 `set_config` 调用。

#### 初始化与反初始化

初始化 `init` 和反初始化 `uninit` 是为了让插件给每一个线程做私有域的初始化。例如在多个摄像头捕获输入的情况下，需要给每一个线程设置捕获的来源；在多个模型同时被调度的情况下，需要给每一个输出线程设置输出的处理（做统一处理也可以，这里举个不恰当例子）。

#### 数据输入和结果输出

数据输入 `rknn_input` 是调度程序获取推理数据源，可以根据线程数据中的私有化数据（在初始化中设置）来获取不同的输入源；数据输入释放 `rknn_input_release` 是在推理结束后释放掉 `rknn_input` 中申请的动态内存。结果输出 `rknn_output` 是调度程序返回推理的结果，有进一步的处理可以放在这里处理。

### 插件管理

插件管理部分通过 `dlopen` 和 `dlsym` 来注册插件，并使用 STL 中的 `map` 来做插件的管理。插件注册成功之后，插件管理部分会检测插件中各个函数的可用性（一些非必要函数可以不给出定义）。

### 模型管理

模型管理部分是对 RKNN 模型推理的流程做了简要的封装（参考了 RKNN SDK 文档和示例模型）。

### 推理调度

推理调度部分主要的部分是数据获取线程、模型推理线程和这两类线程间的数据队列缓存。工作模式是数据获取线程调用插件的数据获取接口获取模型的数据，将获取的数据放入到任务队列中；推理线程从队列中获取需要处理的数据，送入到模型管理部分得到推理的结果，并调用插件的结果输出接口返回推理结果。

# 三、使用

使用此模板做新模型推理时，仅需编写针对新模型的插件，也就是实现插件中的各个接口；另外需要修改 `CMakeList` 使插件能够编译出来。

## 3.1 插件模板

### 插件功能汇聚

以下对插件模板 `rknn_plugin_template` 中的各个位置做出解释，首先是插件模板文件的后半部分：

```cpp
// 注册所有本插件的相关函数到插件结构体中
// 注意：插件名称和结构体定义名称必须和插件动态库名称一致
static struct PluginStruct rknn_plugin_name = {
        .plugin_name 		= "rknn_plugin_name",
        .plugin_version 	= 1,
        .get_config         = get_config,
        .set_config         = set_config,
        .init				= rknn_plugin_init,
        .uninit 			= rknn_plugin_uninit,
        .rknn_input 		= rknn_plugin_input,
        .rknn_input_release = rknn_plugin_input_release,
        .rknn_output		= rknn_plugin_output,
};

// 插件动态库在加载时会自动调用该函数
static void plugin_init plugin_auto_register(){
    d_rknn_plugin_info("auto register plugin %p, name: %s", &rknn_plugin_name, rknn_plugin_name.plugin_name)
    plugin_register(&rknn_plugin_name);
}

// 插件动态库在关闭时会自动调用该函数
static void plugin_exit plugin_auto_unregister(){
    d_rknn_plugin_info("auto unregister plugin %p, name: %s", &rknn_plugin_name, rknn_plugin_name.plugin_name)
    plugin_unregister(&rknn_plugin_name);
}

```

后半部分是对插件自身的功能做汇聚，并向外部注册。需要注意的有一点：**结构体的名称需要和插件的名称一致**。

### 插件配置处理

插件的配置主要有两个部分，一个是插件向调度程序设置配置；另一个是调度程序向插件设置配置。

```cpp
static int get_config(PluginConfigGet *plugin_config){
    // 输入线程个数
    plugin_config->input_thread_nums = 2;
    // 输出线程个数
    plugin_config->output_thread_nums = 2;
    // 是否需要输出float类型的输出结果
    plugin_config->output_want_float = true;
    return 0;
}

PluginConfigSet g_plugin_config_set;
static int set_config(PluginConfigSet *plugin_config){
    // 注意拷贝构造函数
    memcpy(&g_plugin_config_set, plugin_config, sizeof(PluginConfigSet));
    d_rknn_plugin_info("plugin config set success")
    return 0;
}
```

`get_config` 是插件向调度程序设置配置，函数名是调度程序向插件程序获取配置的意思（调度程序是主，插件程序是从）。在这部分插件程序需要向调度程序声明输入线程的个数，推理（输出）线程的个数，以及输出的格式。

`set_config` 是调度程序向插件设置配置。在这部分，调度程序将模型相关的信息（在 `rknn_infer_api` 中定义）传递给插件，例如模型版本，输入输出 `tensor ​` 的个数，输入输出 `tensor ​` 特征等等。

### 插件初始化和反初始化

插件初始化时需要对每一个线程做单独的处理，例如对输入线程单独配置独立的输入源（媒体数据等）。

```cpp
static int rknn_plugin_init(struct ThreadData *td) {
    // 设置输入线程的输入源
    return 0;
}

static int rknn_plugin_uninit(struct ThreadData *td) {
    // 释放输入线程的输入源
    return 0;
}
```

每个线程独立的数据源可以存放在自定义的结构体中，并将结构体的地址传给 `ThreadData` 中的 `plugin_private_data` 变量，以便调用其它接口时能够获取到每个线程的私有数据（线程访问变量“全局化”：各个线程私有数据互不干扰，每次调用接口都能拿到该线程私有数据）。注意线程私有数据范围限制在了插件内部，因为访问 `plugin_private_data` 变量需要知道该变量的结构信息，而结构信息仅有插件内部知道。

### 插件输入和输出

插件的输入是插件程序从数据源封装需要推理的数据；插件的输出是插件程序处理推理结果。

```cpp
static int rknn_plugin_input(struct ThreadData *td, struct InputUnit *input_unit) {
    // 根据数据源采集数据，使用 动态 的内存做封装
    return 0;
}

static int rknn_plugin_input_release(struct ThreadData *td, struct InputUnit *input_unit) {
    // 释放 rknn_plugin_input 中申请的内存
     return 0;
}

static int rknn_plugin_output(struct ThreadData *td, struct OutputUnit *output_unit) {
    // 处理输出数据
    d_rknn_plugin_info("plugin print output data, thread_id: %d", td->thread_id)
    return 0;
}
```

在多源推理的情况下，输入接口可以从 `ThreadData` 中的 `plugin_private_data` 获取到输入线程的私有数据源，然后从该私有数据源装配需要推理的数据。

## 3.2 插件编译

### 配置

插件的编译是获取插件动态库文件，需要在 CMakeList 中添加以下内容（以编译 `rknn_plugin_template` 为例）：

```cmake
## rknn_plugin_template
include_directories(${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_plugin_template/)
add_library(rknn_plugin_template SHARED
        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_plugin_template/rknn_plugin_template.cpp
        ${DLOG_SRC}
)
target_link_libraries(rknn_plugin_template
        ${RKNN_DIR}/lib/librknn_api.so
        ${RKNN_DIR}/lib/librknnrt.so
)
```

**其中：**

* include_directories：添加头文件包含路径（如果插件不依赖其它文件可以不加）
* add_library：将插件源代码装配成动态库文件，`rknn_plugin_template` 是动态库名称，`SHARED` 表示装配成动态库而不是静态库，后续是动态库中的文件（`${DLOG_SRC}` 是日志相关的代码）
* target_link_libraries：添加插件动态库依赖的其它库文件

### 编译

在根目录下执行如下命令：

```cmake
# 生成编译目录
mkdir build
cd build

# 生成Makefile
cmake ..

# 编译全部文件,-j8是多路同时编译
make -j 8
# 编译部分文件
make -j 8 project_name
```

将依赖的库文件和模型文件拷贝到编译目录。

### 运行

在编译目录执行如下命令运行：

```cmake
./rknn_infer -m <model_path> -p <plugin_name>
```