/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频解码封装
 */

#ifndef RKNN_INFER_PLUGIN_MPP_VIDEO_DECODER_H
#define RKNN_INFER_PLUGIN_MPP_VIDEO_DECODER_H

#include <cstdio>

#include "rk_mpi.h"
#include "plugin_common.h"

#define MAX_READ_BUFFER_SIZE (5 * 1024 * 1024)
#define MAX_DECODER_FRAME_NUM (200)

struct DecoderMppFrame{
    uint32_t hor_stride;
    uint32_t ver_stride;

    uint32_t hor_width;
    uint32_t ver_height;

    int data_fd;
    void *data_buf;
    uint32_t data_size;

    MppFrame mpp_frame;
    MppFrameFormat mpp_frame_format;
};

class MppVideoDecoder {
public:
    // 初始化解码器
    explicit MppVideoDecoder(const std::string &video_path);

    // 释放解码器
    ~MppVideoDecoder();

    [[nodiscard]] bool is_init() const { return m_init_flag; };

    // 获取视频的下一帧数据
    int get_next_frame(DecoderMppFrame &frame);

    void release_frame(DecoderMppFrame &frame);
private:
    // 初始化解码器
    int init_decoder();

    // 封装一个包
    int get_one_packet();

private:
    bool m_init_flag = false;
    uint64_t m_frame_count = 0;
    // 视频输入源
    FILE *m_in_fp = nullptr;
    char *m_in_buf = nullptr;
    MppPacket m_pkt = nullptr;

    // Mpp视频解码器上下文
    MppCtx m_mpp_ctx = nullptr;
    MppApi *m_mpp_mpi = nullptr;

    // 缓存
    MppBufferGroup  m_frame_buffer_group = nullptr;

    // 视频解码信息
    bool m_video_eos = false; // 视频解码结束标志
    bool m_video_loop_decoder = true; // 视频循环解码标志
};
#endif //RKNN_INFER_PLUGIN_MPP_VIDEO_DECODER_H
