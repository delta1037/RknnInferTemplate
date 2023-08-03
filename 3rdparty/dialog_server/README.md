# dialog_server
dialog项目目标：查看运行中的软件的数据，dialog_server是服务端部分，需要与客户端配合使用。

实现方案：分为服务端和客户端，通过socket互联。服务端与需要查看数据的程序集成到一起，并注册查看数据的命令；客户端通过向正在运行的程序发送指令和接收输出来显示正在运行的程序的数据。

### 使用

首先需要输出数据的类需要继承DialogServer类，并重写void dialog(uint32_t idx)函数。

```c++
class test_1 : public DialogServer {
    public:
    void dialog(uint32_t idx) override {
    }
}
```

然后需要注册查看数据的命令，包括主目录的注册和子命令的注册。

```c++
// 注册主目录名称和注册数据操作函数
DialogRegisterData *data = register_main("test_1", reinterpret_cast<MENU_FUNC>(&test_1::dialog), this);
// 注册子命令id和简要信息
data->register_menu(0, "print 0");
data->register_menu(1, "print 1");
```

编写dialog函数，设置子命令id需要输出的内容：（注意这里的idx是和上面注册的命令id是有关联的）

```c++
void dialog(uint32_t idx) {
    if(idx == 0){
        dialog_print("test_2 idx = 0\n");
    }else if(idx == 1){
        dialog_print("test_2 idx = 1\n");
    }else{
        dialog_print("invalid idx\n");
    }
}
```



### 测试

进入DialogServer主目录，创建build文件夹并切换到该目录：

```bash
cd /path/to/dialog_server
mkdir build
cd build
```

编译：

```bash
cmake .. -G "MinGW Makefiles"
make
```

运行：

```bash
./dialog_server.exe
```



## 服务端客户端配合

服务端注册的内容就是客户端收到的菜单，客户端根据菜单输入对应的数字标号就可以获取对应内容的输出。
