# dialog_client
dialog项目目标：查看运行中的软件的数据，dialog_client是客户端部分，需要与服务端配合使用。

实现方案：分为服务端和客户端，通过socket互联。服务端与需要查看数据的程序集成到一起，并注册查看数据的命令；客户端通过向正在运行的程序发送指令和接收输出来显示正在运行的程序的数据。

### 使用

进入DialogSClient主目录，创建build文件夹并切换到该目录：

```bash
cd /path/to/dialog_client
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
./dialog_client.exe
```



## 服务端客户端配合

服务端注册的内容就是客户端收到的菜单，客户端根据菜单输入对应的数字标号就可以获取对应内容的输出。
