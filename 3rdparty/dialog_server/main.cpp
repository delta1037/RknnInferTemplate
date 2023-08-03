#include <iostream>
#include "dialog_server.h"

class test_1 : public DialogServer {
public:
    test_1(){
        dialog_debug("1:%p\n", &test_1::dialog);
        DialogRegisterData *data = register_main("test_1", reinterpret_cast<MENU_FUNC>(&test_1::dialog), this);
        data->register_menu(0, "print 0");
        data->register_menu(1, "print 1");
    }

    void dialog(uint32_t idx) override {
        if(idx == 0){
            dialog_print("test_1 idx = 0\n");
        }else if(idx == 1){
            dialog_print("test_1 idx = 1\n");
        }else{
            dialog_print("invalid idx\n");
        }
    }
};

class test_2 : public DialogServer {
public:
    test_2(){
        dialog_debug("2:%p\n", &test_2::dialog);
        DialogRegisterData *data = register_main("test_2", reinterpret_cast<MENU_FUNC>(&test_2::dialog), this);
        data->register_menu(0, "print 0");
        data->register_menu(1, "print 1");
    }

    void dialog(uint32_t idx) override {
        if(idx == 0){
            dialog_print("test_2 idx = 0\n");
        }else if(idx == 1){
            dialog_print("test_2 idx = 1\n");
        }else{
            dialog_print("invalid idx\n");
        }
    }
};

int main() {
    test_1 test_1_i;
    test_2 test_2_i;
    while(true){
#ifdef __linux
        sleep(60);
#else
        Sleep(60);
#endif
    }
    return 0;
}
