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

## rga
SET(RGA_DIR ${CMAKE_SOURCE_DIR}/3rdparty/rga/RK356X/)
include_directories(${RGA_DIR}/include)
link_directories(${RGA_DIR}/lib/Linux/aarch64)

## mpp
SET(MPP_DIR ${CMAKE_SOURCE_DIR}/3rdparty/mpp/)
include_directories(${MPP_DIR}/include/rockchip)
link_directories(${MPP_DIR}/Linux/aarch64)

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
    ${CMAKE_SOURCE_DIR}/rknn_plugins
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

project(test_mpp_video_decoder)
add_executable(test_mpp_video_decoder
        ${CMAKE_SOURCE_DIR}/unit_test/test_mpp_video_decoder.cpp
        ${CMAKE_SOURCE_DIR}/rknn_plugins/mpp_video_decoder.cpp
        ${CMAKE_SOURCE_DIR}/rknn_plugins/mpp_video_encoder.cpp
        ${DLOG_SRC}
        )
target_link_libraries(test_mpp_video_decoder
        ${OpenCV_LIBS}
        ${RKNN_DIR}/lib/librknn_api.so
        ${RKNN_DIR}/lib/librknnrt.so
        ${MPP_DIR}/Linux/aarch64/librockchip_mpp.so.0
        tbb
        )

# 图像图例插件示例
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
## rknn_mobilenet_demo
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
## rknn_yolov5_demo
# https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_yolov5_demo
include_directories(${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolo_v5/)
add_library(rknn_yolo_v5 SHARED
        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolo_v5/rknn_yolo_v5.cpp
        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolo_v5/postprocess.cc
        ${DLOG_SRC}
        )
target_link_libraries(rknn_yolo_v5
        ${OpenCV_LIBS}
        ${RKNN_DIR}/lib/librknn_api.so
        ${RKNN_DIR}/lib/librknnrt.so
        ${RGA_DIR}/lib/Linux/aarch64/librga.so
        )
add_library(rknn_yolo_v5_video SHARED
        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolo_v5/rknn_yolo_v5_video.cpp
        ${CMAKE_SOURCE_DIR}/rknn_plugins/rknn_yolo_v5/postprocess.cc
        ${CMAKE_SOURCE_DIR}/rknn_plugins/mpp_video_decoder.cpp
        ${DLOG_SRC}
        )
target_link_libraries(rknn_yolo_v5_video
        ${OpenCV_LIBS}
        ${RKNN_DIR}/lib/librknn_api.so
        ${RKNN_DIR}/lib/librknnrt.so
        ${RGA_DIR}/lib/Linux/aarch64/librga.so
        ${MPP_DIR}/Linux/aarch64/librockchip_mpp.so
        )