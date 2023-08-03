# CMake版本
cmake_minimum_required(VERSION 3.22.1)
# C++版本
set(CMAKE_CXX_STANDARD 17)
# 指定运行时加载路径位置
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
SET(CMAKE_CXX_FLAGS "-finput-charset=UTF-8 -Wl,-rpath=./:./lib")
# 编译选项设置
## 本地测试
#SET(ENABLE_LOCAL_TEST FALSE)
## 性能统计
SET(ENABLE_PERFORMANCE_STATISTIC TRUE)
## 动态调试功能（通过Dialog实现）
SET(ENABLE_DYNAMIC_DEBUG FALSE)
## 动态加载配置功能（包括日志配置、系统配置）
SET(ENABLE_DYNAMIC_CONFIG FALSE)

# 跨平台设置
#IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
#    MESSAGE(STATUS "current platform: Linux ")
##    set(CMAKE_C_COMPILER "/usr/local/gcc-10.3.0/bin/gcc")
##    set(CMAKE_CXX_COMPILER "/usr/local/gcc-10.3.0/bin/g++")
#
#    ADD_DEFINITIONS(-DLINUX_PLATFORM_BUILD)
#    SET(BUILD_PLATFORM "Linux")
#ELSE()
#    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
#ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

# RKNN 推理系统
project(rknn_infer)
# 插件
project(plugin_rknn_image)

# 第三方工程
## RKNN 推理库
SET(RKNN_DIR ${CMAKE_SOURCE_DIR}/3rdparty/rknn/)
link_directories(${RKNN_DIR}/lib)
include_directories(${RKNN_DIR}/include)

## 日志记录功能
SET(DLOG_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlog/)
link_directories(${DLOG_DIR}/lib)
include_directories(${DLOG_DIR}/include)
SET(DLOG_SRC
    ${DLOG_DIR}/src/log.cpp
    ${DLOG_DIR}/src/log_manage.cpp
)
## Dialog功能
if(${ENABLE_DYNAMIC_DEBUG})
#    project(${CMAKE_SOURCE_DIR}/3rdparty/dialog_client)
#    project(${CMAKE_SOURCE_DIR}/3rdparty/dialog_server)
    ADD_DEFINITIONS(-DDYNAMIC_DEBUG)
    SET(DIALOG_SERVER_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dialog_server/)
    SET(DIALOG_CLIENT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dialog_client/)
    include_directories(${DIALOG_SERVER_DIR})
    SET(DIALOG_SRC
        ${DIALOG_SERVER_DIR}/dialog_server.cpp
    )
    #add_executable(3rdparty/dialog_client
    #        ${DIALOG_CLIENT_DIR}/main.cpp
    #)
    #
    #add_executable(3rdparty/dialog_server
    #        ${DIALOG_SERVER_DIR}/main.cpp
    #        ${DIALOG_SERVER_DIR}/dialog_server.cpp
    #        ${DIALOG_SERVER_DIR}/dialog_server.h
    #)
endif()
## 动态配置功能
if (${ENABLE_DYNAMIC_CONFIG})
    ADD_DEFINITIONS(-DDYNAMIC_CONFIG)
    SET(DPATH_DETECT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dpath_detect/)
    include_directories(${DPATH_DETECT_DIR}/include)
endif ()

# 系统内部统计
if (${ENABLE_PERFORMANCE_STATISTIC})
    ADD_DEFINITIONS(-DPERFORMANCE_STATISTIC)
endif ()

# 工程包含目录设置
include_directories(
    ${CMAKE_SOURCE_DIR}/rknn_infer
    ${CMAKE_SOURCE_DIR}/rknn_infer_api
    ${CMAKE_SOURCE_DIR}/unit_test
    ${CMAKE_SOURCE_DIR}/utils
)

# 推理系统
add_executable(rknn_infer
    ${CMAKE_SOURCE_DIR}/rknn_infer/main.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/rknn_infer.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/rknn_model.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/plugin_ctrl.cpp
    ${DIALOG_SRC}
    ${DLOG_SRC}
)
find_library(rknn_api rknn_api ${RKNN_DIR}/lib)
find_library(rknnrt rknnrt ${RKNN_DIR}/lib)
link_libraries(rknn_infer ${rknn_api} ${rknnrt})

# 图像图例插件示例
add_library(plugin_rknn_image SHARED
    ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_image.cpp
    ${DLOG_SRC}
)
