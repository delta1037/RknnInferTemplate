#include <cstdio>
#include <cstdarg>
#include "dialog_server.h"

using namespace std;

bool DialogServer::s_init_status = false;
string DialogServer::s_dialog_buffer = "\n";

uint32_t DialogServer::s_dialog_idx = 0;
std::map<uint32_t, DialogRegisterData> DialogServer::s_dialog_map;

bool DialogServer::s_thread_exit_flag;
std::thread *DialogServer::s_thread_ctrl;

SOCKET DialogServer::s_dialog_server_socket;

int DialogServer::s_server_init_count = 0;

DialogServer::DialogServer() {
    s_server_init_count++;
    if(s_init_status){
        // 服务端socket只初始化一次，其余由后台线程来完成
        return;
    }
    // 初始化socket之类的
#ifndef __linux__
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,0),&wsa);
#endif
    s_dialog_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // 设置服务端地址和协议
    SOCKADDR_IN ser_addr;
    ser_addr.sin_family=AF_INET;
    ser_addr.sin_port=htons(SERVER_PORT);
#ifdef __linux__
    ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
#else
    ser_addr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
#endif

    // 绑定
    if(bind(s_dialog_server_socket, (SOCKADDR*)&ser_addr, sizeof(ser_addr)) != 0){
        dialog_info("server %s:%d bind socket error", SERVER_IP, SERVER_PORT)
        return;
    }

    // 开启监听
    if(listen(s_dialog_server_socket, 5) != 0){
        dialog_info("server %s:%d listen error", SERVER_IP, SERVER_PORT)
        return;
    }

    // 启动后台线程 dialog_thread
    s_thread_exit_flag = false;
    dialog_debug("start accept thread")
    s_thread_ctrl = new std::thread([this] { dialog_thread(); });

    // 初始化完毕
    s_init_status = true;
    dialog_info("dialog server %s:%d init success", SERVER_IP, SERVER_PORT)
}

DialogServer::~DialogServer() {
    s_server_init_count--;
    // 关闭线程
    if(s_server_init_count == 0 && s_thread_ctrl){
        // 关闭socket
#ifdef __linux__
        close(s_dialog_server_socket);
#else
        closesocket(s_dialog_server_socket);
        WSACleanup();
#endif
        s_thread_exit_flag = true;
        s_thread_ctrl->join();
        s_thread_ctrl = nullptr;
    }
}

void DialogServer::dialog_thread() {
    // 启动一个线程，用来监听客户端发过来的内容
    dialog_debug("dialog thread start")
    while(!s_thread_exit_flag){
        SOCKADDR_IN cli_addr;
#ifdef __linux__
        unsigned int len = sizeof(SOCKADDR_IN);
#else
        int len = sizeof(SOCKADDR_IN);
#endif
        SOCKET cli_socket = accept(s_dialog_server_socket, (SOCKADDR*)&cli_addr, &len);
        dialog_debug("accept one request")

        // -1是主目录，其它是其它类型的目录
        int idx_choose = -1;

        // 发送大纲数据，准备循环处理客户端指令（包括退出的数据）
        string main_content = get_tips_table(idx_choose);
        dialog_debug("send main content: \n%s", main_content.c_str())
        send(cli_socket, main_content.c_str(), (int)main_content.size(), 0);

        // 循环接收数据
        char socket_buffer[SOCKET_BUFFER_SIZE];
        while(true){
            memset(socket_buffer, 0 , SOCKET_BUFFER_SIZE);
            int recv_length = recv(cli_socket, socket_buffer, SOCKET_BUFFER_SIZE, 0);
            // 异常的接收
            if(recv_length <= 0){
                dialog_debug("wrong recv, exit connect")
                break;
            }

            // 在主目录如果遇到退出选项就退出
            if(socket_buffer[0] == 'q' && idx_choose == -1){
                main_content = "exit";
                dialog_debug("main content use e to exit, send: %s", main_content.c_str())
                send(cli_socket, main_content.c_str(), (int)main_content.size(), 0);
                break;
            }
            // 在非主目录上退出遇到退出选项就回到主目录
            if(socket_buffer[0] == 'q' && idx_choose != -1){
                idx_choose = -1;
                dialog_debug("child content use e to exit, send: %s", main_content.c_str())
                send(cli_socket, main_content.c_str(), (int)main_content.size(), 0);
                continue;
            }
            // 发送当前级别的目录
            if(socket_buffer[0] == 'i' || (socket_buffer[0] < '0' || socket_buffer[0] > '9')){
                string ret_content = get_tips_table(idx_choose);
                dialog_debug("send info table: %s", ret_content.c_str())
                send(cli_socket, ret_content.c_str(), (int)ret_content.size(), 0);
                continue;
            }

            // 其它选项就选择对应的输出方式
            dialog_debug("server recv msg: %s", socket_buffer)
            int idx_recv = atoi(socket_buffer);

            // 处理根目录下的情况
            if(idx_choose == -1){
                // 主目录里去选
                string ret_content;
                if(s_dialog_map.find(idx_recv) != s_dialog_map.end()){
                    // 已注册的内容，切换到对应的菜单
                    dialog_debug("switch to idx : %d", idx_recv)
                    idx_choose = idx_recv;
                    ret_content = get_tips_table(idx_recv);
                }else{
                    // 未注册的内容，返回主菜单加以提示
                    dialog_debug("invalid idx, return main content")
                    ret_content = main_content;
                }
                dialog_debug("recv %d and send: %s", idx_recv, ret_content.c_str())
                send(cli_socket, ret_content.c_str(), (int)ret_content.size(), 0);

                // 结束，忽略后面的内容
                continue;
            }

            // 处理子目录菜单下的情况
            if(s_dialog_map.find(idx_choose) != s_dialog_map.end()){
                dialog_debug("idx %d:%s call menu func %d", idx_choose, s_dialog_map[idx_choose].name.c_str(), idx_recv)
                (s_dialog_map[idx_choose].p_instance->*(s_dialog_map[idx_choose].func))(idx_recv);
                dialog_debug("call func end:%s", s_dialog_buffer.c_str())
                send(cli_socket, s_dialog_buffer.c_str(), (int)s_dialog_buffer.size(), 0);
                s_dialog_buffer = "\n";
                dialog_debug("send data:%s", s_dialog_buffer.c_str())
            }else{
                // 未知情况，重新初始化
                idx_choose = -1;
                dialog_debug("send main content: \n%s", main_content.c_str())
                send(cli_socket, main_content.c_str(), (int)main_content.size(), 0);
            }
        }
        // 服务端关闭连接
#ifdef __linux__
        close(cli_socket);
#else
        closesocket(cli_socket);
#endif
    }
}

DialogRegisterData *DialogServer::register_main(const std::string &name, MENU_FUNC menu_func, DialogServer *p_instance) {
    dialog_debug("register name : %s, ptr:%p, this ptr:%p", name.c_str(), menu_func, &DialogServer::dialog)
    for(const auto &it : s_dialog_map){
        if(it.second.name == name){
            dialog_info("name %s already exists", name.c_str())
            return nullptr;
        }
    }

    // 构造新的菜单
    DialogRegisterData dialog_data;
    dialog_data.name = name;
    dialog_data.p_instance = p_instance;
    dialog_data.func = menu_func;
    s_dialog_map[s_dialog_idx++] = dialog_data;
    dialog_debug("map size: %d", s_dialog_map.size())
    return &s_dialog_map[s_dialog_idx - 1];
}

void DialogServer::dialog_print(const char *format, ...) {
    char buffer[DIALOG_MAX_BUFFER]{0};

    va_list va_args;
    va_start(va_args, format);
    std::vsnprintf(buffer, DIALOG_MAX_BUFFER, format, va_args);
    va_end(va_args);

    s_dialog_buffer += buffer;
    s_dialog_buffer += '\n';
}

std::string DialogServer::get_tips_table(int idx) {
    string tips_table;
    char buffer_line[CONTENT_BUFFER];
    const char *format = "\t%c\t%s\n\0";

    // 返回次级目录
    if(idx != -1){
        for(const auto &it : s_dialog_map){
            if(it.first == idx){
                for(const auto &idx_it: it.second.menu){
                    snprintf(buffer_line, CONTENT_BUFFER, format, idx_it.first + '0', idx_it.second.c_str());
                    tips_table += buffer_line;
                }
                break;
            }
        }
    }

    // 返回主目录
    if(idx == -1 or tips_table.empty()){
        // 生成主目录
        tips_table.clear();
        for(const auto &it : s_dialog_map){
            snprintf(buffer_line, CONTENT_BUFFER, format, it.first + '0', it.second.name.c_str());
            tips_table += buffer_line;
        }
    }

    snprintf(buffer_line, CONTENT_BUFFER, format, 'i', "打印提示信息");
    tips_table += buffer_line;
    snprintf(buffer_line, CONTENT_BUFFER, format, 'q', "退出或者返回到上一级");
    tips_table += buffer_line;
    return tips_table;
}
