/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频解码测试
 */
#include <string>
#include "mpp_video_decoder.h"
#include "utils_log.h"
#include "utils.h"
#include "mpp_video_encoder.h"

#include "opencv2/opencv.hpp"


// 参考 https://github.com/MUZLATAN/ffmpeg_rtsp_mpp/blob/master/MppDecode.cpp#L214
int YUV2Image(
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
            const uint8_t *base_u = data_buf + h_stride * v_stride;
            const uint8_t *base_v = data_buf + h_stride * v_stride / 4; // NOTE: diff from gen_yuv_image
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
//        case MPP_FMT_ARGB8888 : {
//            const uint8_t *base_y = data_buf;
//            for (uint32_t row = 0; row < height; row++) {
//                memcpy(image, base_y + row * h_stride * 4, width * 4);
//                image += width * 4;
//            }
//        } break;
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

int YUV2Mat(
        const uint8_t *data_buf,
        uint32_t width, uint32_t height, 
        uint32_t hor_stride, uint32_t ver_stride, 
        MppFrameFormat fmt, 
        cv::Mat &rgb_img) {
    cv::Mat yuv_img;
    switch (fmt) {
        case MPP_FMT_YUV420SP :{
            yuv_img.create(height * 3 / 2, width, CV_8UC1);
            YUV2Image(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420sp2RGB);
        } break;
        case MPP_FMT_YUV420P : {
            yuv_img.create(height * 3 / 2, width, CV_8UC1);
            YUV2Image(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420p2RGB);
        } break;
//        case MPP_FMT_ARGB8888 : {
//            yuv_img.create(height, width * 4, CV_8UC1);
//            YUV2Image(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
//            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV420p2RGB);
//        } break;
        case MPP_FMT_YUV422_YUYV :
        case MPP_FMT_YUV422_UYVY : {
            yuv_img.create(height, width * 2, CV_8UC1);
            YUV2Image(data_buf, width, height, hor_stride, ver_stride, fmt, yuv_img.data);
            cv::cvtColor(yuv_img, rgb_img, cv::COLOR_YUV2RGBA_Y422);
        } break;
        default : {
            d_mpp_module_error("read image do not support fmt %d", fmt)
            return -1;
        }
    }
    return 0;
}

int test_decoder(){
    // 初始化解码器
    std::string path = "1080p_ffmpeg.h264";
    MppVideoDecoder video_decoder = MppVideoDecoder(path);
    if(!video_decoder.is_init()){
        d_unit_test_error("MppVideoDecoder init failed!")
        return -1;
    }

    // 获取视频的下一帧数据
    uint64_t time_start = get_time_of_ms();
    uint32_t frame_count = 0;
    DecoderMppFrame frame{};
    while(video_decoder.get_next_frame(frame) == 0){
        d_unit_test_info("frame size: %d",frame.data_size)
        frame_count++;
        d_unit_test_info("frame count: %d", frame_count)

        cv::Mat image;
        YUV2Mat(
                (uint8_t*)frame.data_buf,
                frame.hor_width, frame.ver_height,
                frame.hor_stride, frame.ver_stride,
                frame.mpp_frame_format,
                image);
        cv::imwrite("1080p_ffmpeg.jpg", image);
        video_decoder.release_frame(frame);
        break;
    }
    uint64_t time_end = get_time_of_ms();
    d_unit_test_warn("frame count: %d", frame_count)
    d_unit_test_warn("time cost: %d ms", time_end - time_start)

    return 0;
}

int test_encoder(){
    // 初始化解码器
    std::string path = "1080p_ffmpeg.h264";
    MppVideoDecoder video_decoder = MppVideoDecoder(path);
    if(!video_decoder.is_init()){
        d_unit_test_error("MppVideoDecoder init failed!")
        return -1;
    }
    std::string output_path = "1080p_ffmpeg_encoder.h264";
    MppVideoEncoder video_encoder = MppVideoEncoder(output_path);
    if(!video_encoder.is_init()){
        d_unit_test_error("MppVideoEncoder init failed!")
        return -1;
    }

    // 获取视频的下一帧数据
    uint64_t time_start = get_time_of_ms();
    uint32_t frame_count = 0;
    DecoderMppFrame frame{};
    while(video_decoder.get_next_frame(frame) == 0){
        frame_count++;
//        d_unit_test_info("frame count: %d", frame_count)
//        d_unit_test_info("frame format: %d", frame.mpp_frame_format)
//        d_unit_test_info("frame width: %d", frame.hor_width)
//        d_unit_test_info("frame height: %d", frame.ver_height)
//        d_unit_test_info("frame data_size: %d", frame.data_size)

        uint8_t image_data [frame.data_size];
        YUV2Image(
                (uint8_t *)frame.data_buf,
                frame.hor_width, frame.ver_height,
                frame.hor_stride, frame.ver_stride,
                frame.mpp_frame_format,
                image_data);

        video_encoder.process_image(image_data, frame.hor_width, frame.ver_height, frame.mpp_frame_format);

        video_decoder.release_frame(frame);
    }
    uint64_t time_end = get_time_of_ms();
    d_unit_test_warn("frame count: %d", frame_count)
    d_unit_test_warn("time cost: %d ms", time_end - time_start)
    return 0;
}

int main(){
//    test_decoder();

    test_encoder();
    return 0;
}