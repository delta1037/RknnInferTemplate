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
