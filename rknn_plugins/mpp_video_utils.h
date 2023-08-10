/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频编码解码函数封装
 */
#ifndef RKNN_INFER_PLUGIN__MPP_VIDEO_UTILS_H
#define RKNN_INFER_PLUGIN__MPP_VIDEO_UTILS_H
#include "rk_mpi.h"
#include "opencv2/opencv.hpp"
#include "utils_log.h"

/**
 * @brief 将 MPP 帧数据中的 padding 部分去掉得到原始图像，参考 https://github.com/MUZLATAN/ffmpeg_rtsp_mpp/blob/master/MppDecode.cpp#L214
 */
static int yuv_del_stride(
        const uint8_t *data_buf,
        uint32_t width, uint32_t height,
        uint32_t h_stride, uint32_t v_stride,
        MppFrameFormat fmt,
        uint8_t *image) {
    switch (fmt) {
        case MPP_FMT_YUV420SP : {
            const uint8_t *base_y = data_buf;
            const uint8_t *base_c = data_buf + h_stride * v_stride;
            for (uint32_t row = 0; row < height; row++) {
                memcpy(image, base_y + row * h_stride, width);
                image += width;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(image, base_c + row * h_stride, width);
                image += width;
            }
        } break;
        case MPP_FMT_YUV420P : {
            const uint8_t *base_y = data_buf;
            const uint8_t *base_u = base_y + h_stride * v_stride;
            const uint8_t *base_v = base_u + h_stride * v_stride / 4; // NOTE: diff from gen_yuv_image
            for (uint32_t row = 0; row < height; row++) {
                memcpy(image, base_y + row * h_stride, width);
                image += width;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(image, base_u + row * h_stride / 2, width / 2);
                image += width / 2;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(image, base_v + row * h_stride / 2, width / 2);
                image += width / 2;
            }
        } break;
        case MPP_FMT_ARGB8888 : {
            const uint8_t *base_y = data_buf;
            for (uint32_t row = 0; row < height; row++) {
                memcpy(image, base_y + row * h_stride * 4, width * 4);
                image += width * 4;
            }
        } break;
        case MPP_FMT_YUV422_YUYV :
        case MPP_FMT_YUV422_UYVY : {
            const uint8_t *base_y = data_buf;
            for (uint32_t row = 0; row < height; row++) {
                memcpy(image, base_y + row * h_stride, width * 2);
                image += width * 2;
            }
        } break;
        default : {
            d_mpp_module_error("read image do not support fmt %d", fmt)
            return -1;
        }
    }
    return 0;
}

/**
 * @brief 给原始图像添加 padding，封装成 MPP 帧数据，来源于 https://blog.csdn.net/qq_39839546/article/details/122023991
 */
static int yuv_add_stride(
        const uint8_t *image,
        uint32_t width, uint32_t height,
        uint32_t hor_stride, uint32_t ver_stride,
        MppFrameFormat fmt,
        uint8_t *buf) {
    // YUV格式的图像都是将YUV三个通道分为三部分存取，YYYYYY*****UUUUU*****VVVVV,所以在将其按照16位对齐copy时先将YUV三个通道的起始地址指定好。
    uint8_t *buf_y = buf;
    uint8_t *buf_u = buf_y + hor_stride * ver_stride; // NOTE: diff from gen_yuv_image
    uint8_t *buf_v = buf_u + hor_stride * ver_stride / 4; // NOTE: diff from gen_yuv_image

    // 然后按照不同的格式，按照16位对齐copy图像数据到buf下，不同格式读取方式不同。
    switch (fmt) {
        case MPP_FMT_YUV420SP : {
            for (uint32_t row = 0; row < height; row++) {
                memcpy(buf_y + row * hor_stride, image,width);
                image += width;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(buf_u + row * hor_stride, image, width);
                image += width;
            }
        } break;
        case MPP_FMT_YUV420P : {
            for (uint32_t row = 0; row < height; row++) {
                memcpy(buf_y + row * hor_stride, image, width);
                image += width;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(buf_u + row * hor_stride/2, image, width/2);
                image += width/2;
            }
            for (uint32_t row = 0; row < height / 2; row++) {
                memcpy(buf_v + row * hor_stride/2, image, width/2);
                image += width/2;
            }
        } break;
        case MPP_FMT_ARGB8888 : {
            for (uint32_t row = 0; row < height; row++) {
                memcpy(buf_y + row * hor_stride*4, image, width*4);
                image += width*4;
            }
        } break;
        case MPP_FMT_YUV422_YUYV :
        case MPP_FMT_YUV422_UYVY : {
            for (uint32_t row = 0; row < height; row++) {
                memcpy(buf_y + row * hor_stride, image, width*2);
                image += width*2;
            }
        } break;
        default : {
            d_mpp_module_error("read image do not support fmt %d", fmt)
            return -1;
        } break;
    }
    return 0;
}

/**
 * @brief 将 MPP 帧数据转换为 OpenCV Mat
 */
static int frame_data_to_mat(
        const uint8_t *data_buf,
        uint32_t width, uint32_t height,
        uint32_t hor_stride, uint32_t ver_stride,
        MppFrameFormat fmt,
        cv::Mat &rgb_img) {
    cv::Mat yuv_img;
    switch (fmt) {
        case MPP_FMT_YUV420SP :{
            yuv_img.create(height * 3 / 2, width, CV_8UC1);
            yuv_del_stride(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420sp2RGB);
        } break;
        case MPP_FMT_YUV420P : {
            yuv_img.create(height * 3 / 2, width, CV_8UC1);
            yuv_del_stride(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420p2RGB);
        } break;
//        case MPP_FMT_ARGB8888 : {
//            yuv_img.create(height, width * 4, CV_8UC1);
//            yuv_del_stride(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
//            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420p2RGB);
//        } break;
//        case MPP_FMT_YUV422_YUYV :
//        case MPP_FMT_YUV422_UYVY : {
//            yuv_img.create(height, width * 2, CV_8UC1);
//            yuv_del_stride(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
//            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV2RGBA_Y422);
//        } break;
        default : {
            d_mpp_module_error("read image do not support fmt %d", fmt)
            return -1;
        }
    }
    return 0;
}
#endif //RKNN_INFER_PLUGIN__MPP_VIDEO_UTILS_H
