cmake_minimum_required(VERSION 3.21)
project(dialog_client)

set(CMAKE_CXX_STANDARD 17)

# 测试宏定义
# ADD_DEFINITIONS(-DDEBUG)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "current platform: Linux ")
    ADD_DEFINITIONS(-DLINUX_BUILD)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
    link_libraries(ws2_32 wsock32)
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
add_executable(dialog_client main.cpp)
