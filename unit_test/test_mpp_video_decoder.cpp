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

int test_decoder(){
    // 初始化解码器
    std::string path = "1080p.264";
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
//        d_unit_test_info("frame size: %d",frame.data_size)
        frame_count++;
        d_unit_test_info("frame count: %d", frame_count)
        video_decoder.release_frame(frame);
    }
    uint64_t time_end = get_time_of_ms();
    d_unit_test_warn("frame count: %d", frame_count)
    d_unit_test_warn("time cost: %d ms", time_end - time_start)

    return 0;
}

int test_encoder(){
    // 初始化解码器
    std::string path = "1080p.264";
    MppVideoDecoder video_decoder = MppVideoDecoder(path);
    if(!video_decoder.is_init()){
        d_unit_test_error("MppVideoDecoder init failed!")
        return -1;
    }
    std::string output_path = "1080p.yuv";
    MppVideoEncoder video_encoder = MppVideoEncoder(output_path);

    // 获取视频的下一帧数据
    uint64_t time_start = get_time_of_ms();
    uint32_t frame_count = 0;
    DecoderMppFrame frame{};
    while(video_decoder.get_next_frame(frame) == 0){
//        d_unit_test_info("frame size: %d",frame.data_size)
        frame_count++;
        d_unit_test_info("frame count: %d", frame_count)

        video_encoder.process_image(static_cast<uint8_t *>(frame.data_buf), frame.hor_width, frame.ver_height, frame.mpp_frame_format);

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