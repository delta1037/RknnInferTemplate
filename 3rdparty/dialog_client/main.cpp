#include <iostream>
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
#else
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")
#endif

/* 注意与服务端保持一致 */
#define SOCKET_BUFFER_SIZE 20480
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4321

#define FORMAT_PREFIX "<%s, %d> "
#ifdef __linux__
#define FILENAME ( __builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__ )
#else
#define FILENAME ( __builtin_strrchr(__FILE__, '\\') ? __builtin_strrchr(__FILE__, '\\') + 1 : __FILE__ )
#endif
#define dialog_info(format, args...) printf(FORMAT_PREFIX#format, FILENAME , __LINE__, ##args);printf("\n");
#ifdef DEBUG
#define dialog_debug(format, args...) printf(FORMAT_PREFIX#format, FILENAME, __FUNCTION__ , __LINE__, ##args);printf("\n");
#else
#define dialog_debug(format, args...)
#endif

// 发送指令与接收指令
void call_server(){
    SOCKET cli_socket;
    SOCKADDR_IN ser_addr;
    char socket_buffer[SOCKET_BUFFER_SIZE];

#ifndef __linux__
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,0),&wsa);
#endif

    // 创建客户端描述符
    cli_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    // 设置连接协议和服务端地址
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SERVER_PORT);
#ifdef __linux__
    ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
#else
    ser_addr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
#endif

    // 连接服务端
    dialog_debug("start connect server")
    if(connect(cli_socket,(SOCKADDR*)&ser_addr, sizeof(ser_addr)) != 0){
        dialog_info("connect server %s:%d error", SERVER_IP, SERVER_PORT)
        return;
    }
    dialog_info("connect server %s:%d success", SERVER_IP, SERVER_PORT)

    while(true){
        // 清理空间接收消息
        memset(socket_buffer, 0, SOCKET_BUFFER_SIZE);
        recv(cli_socket, socket_buffer, SOCKET_BUFFER_SIZE, 0);
        std::string recv_str = socket_buffer;
        if(recv_str == "exit") {
            break;
        }else{
            std::cout << recv_str << std::endl;
        }

        // 根据接收到的内容发送指令
        memset(socket_buffer, 0, SOCKET_BUFFER_SIZE);
        printf("> ");
        std::cin.getline(socket_buffer, SOCKET_BUFFER_SIZE);
        send(cli_socket, socket_buffer, SOCKET_BUFFER_SIZE, 0);
    }

#ifdef __linux__
    close(cli_socket);
#else
    // 客户端关闭连接
    closesocket(cli_socket);
    WSACleanup();
#endif
}

int main() {
    call_server();
    return 0;
}
