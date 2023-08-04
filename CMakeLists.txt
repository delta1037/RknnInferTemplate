# CMake版本
cmake_minimum_required(VERSION 3.16.3)
# C++版本
set(CMAKE_CXX_STANDARD 17)
# 指定运行时加载路径位置
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
SET(CMAKE_CXX_FLAGS "-Wl,-rpath=./:./lib")
SET(CMAKE_CXX_FLAGS "-Wl,-E")
set(CMAKE_INSTALL_RPATH "./:./lib")
# 选项-本地测试（不依赖 rknn，空转）
SET(ENABLE_LOCAL_TEST FALSE)
# 选项-性能统计
SET(ENABLE_PERFORMANCE_STATISTIC TRUE)

# RKNN 推理系统
project(rknn_infer)

# 第三方工程
## RKNN 推理库
SET(RKNN_DIR ${CMAKE_SOURCE_DIR}/3rdparty/rknn/)
link_directories(${RKNN_DIR}/lib)
include_directories(${RKNN_DIR}/include)

## opencv
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/OpenCV-android-sdk/sdk/native/jni/abi-${CMAKE_ANDROID_ARCH_ABI})
else()
  if(LIB_ARCH STREQUAL "armhf")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/opencv-linux-armhf/share/OpenCV)
  else()
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/opencv-linux-aarch64/share/OpenCV)
  endif()
endif()
find_package(OpenCV REQUIRED)

## 日志记录功能
SET(DLOG_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlog/)
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
)

#link_libraries(${RKNN_DIR}/lib/librknn_api.so)
#link_libraries(${RKNN_DIR}/lib/librknnrt.so)
#link_libraries(pthread dl)

# 推理系统
add_executable(rknn_infer
    ${CMAKE_SOURCE_DIR}/rknn_infer/main.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/rknn_infer.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/rknn_model.cpp
    ${CMAKE_SOURCE_DIR}/rknn_infer/plugin_ctrl.cpp
    ${DLOG_SRC}
)
target_link_libraries(rknn_infer
    ${RKNN_DIR}/lib/librknn_api.so
    ${RKNN_DIR}/lib/librknnrt.so
    pthread
    dl
)

# 测试
project(test_rknn_model)
add_executable(test_rknn_model
        ${CMAKE_SOURCE_DIR}/unit_test/test_rknn_model.cpp
        ${CMAKE_SOURCE_DIR}/rknn_infer/rknn_model.cpp
        ${DLOG_SRC}
        )
target_link_libraries(test_rknn_model
        ${OpenCV_LIBS}
        ${RKNN_DIR}/lib/librknn_api.so
        ${RKNN_DIR}/lib/librknnrt.so
        )

# 图像图例插件示例
# https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_mobilenet_demo
# include_directories(${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_mobilenet/)
add_library(rknn_mobilenet SHARED
    ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_mobilenet/rknn_mobilenet.cpp
    ${DLOG_SRC}
)
target_link_libraries(rknn_mobilenet
    ${OpenCV_LIBS}
    ${RKNN_DIR}/lib/librknn_api.so
    ${RKNN_DIR}/lib/librknnrt.so
)
# https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_yolov5_demo
#include_directories(${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolov5/)
#add_library(rknn_yolov5 SHARED
#        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolov5/rknn_yolov5.cpp
#        ${DLOG_SRC}
#        )
#target_link_libraries(rknn_yolov5
#        ${OpenCV_LIBS}
#        ${RKNN_DIR}/lib/librknn_api.so
#        ${RKNN_DIR}/lib/librknnrt.so
#        )
