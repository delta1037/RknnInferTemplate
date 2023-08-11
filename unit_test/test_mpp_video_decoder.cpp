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

#include "mpp_video_utils.h"

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
        frame_count++;
        d_unit_test_info("frame count: %d", frame_count)

//        cv::Mat image;
//        frame_data_to_mat(
//                (uint8_t *) frame.data_buf,
//                frame.hor_width, frame.ver_height,
//                frame.hor_stride, frame.ver_stride,
//                frame.mpp_frame_format,
//                image);
//        cv::imwrite("1080p_ffmpeg.jpg", image);
        video_decoder.release_frame(frame);
//        break;
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
        d_unit_test_info("frame count: %d", frame_count)

        uint8_t image_data[frame.data_size];
        yuv_del_stride(
                (uint8_t *) frame.data_buf,
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