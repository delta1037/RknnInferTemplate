/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频编码封装
 */

#ifndef RKNN_INFER_PLUGIN_MPP_VIDEO_ENCODER_H
#define RKNN_INFER_PLUGIN_MPP_VIDEO_ENCODER_H

#include <string>

#include "rk_mpi.h"

class MppVideoEncoder {
public:
    explicit MppVideoEncoder(
            const std::string &video_path,
            int32_t width = 0,int32_t height = 0,
            MppFrameFormat fmt = MPP_FMT_YUV422_YUYV,
            MppCodingType type = MPP_VIDEO_CodingAVC,
            int32_t fps = 30, int32_t gop = 60);
    ~MppVideoEncoder();

    bool process_image(uint8_t *image_data, int32_t width = 0, int32_t height = 0,
                       MppFrameFormat fmt = MPP_FMT_YUV422_YUYV,
                       MppCodingType type = MPP_VIDEO_CodingAVC,
                       int32_t fps = 30, int32_t gop = 60);

    [[nodiscard]] bool is_init() const { return m_init_flag; };

private:
    int init_encoder(int32_t width,int32_t height,
                     MppFrameFormat fmt,
                     MppCodingType type,
                     int32_t fps, int32_t gop);

    static MPP_RET read_yuv_image(uint8_t *buf, uint8_t *image, uint32_t width, uint32_t height,
                           uint32_t hor_stride, uint32_t ver_stride, MppFrameFormat fmt);

private:
    bool m_init_flag = false;
    bool m_mpp_init_flag = false;
    //编码所需要的数据
    struct MPP_ENC_DATA {
        // global flow control flag
        uint32_t frm_eos;
        uint32_t pkt_eos;
        uint32_t frame_count;
        uint64_t stream_size;

        // base flow context
        MppCtx ctx;
        MppApi *mpi;
        MppEncPrepCfg prep_cfg;
        MppEncRcCfg rc_cfg;
        MppEncCodecCfg codec_cfg;

        // input / output
        MppBuffer frm_buf = nullptr;
        MppBuffer pkt_buf = nullptr;
        MppBuffer md_info = nullptr;
        MppBufferGroup buf_grp = nullptr;
        MppEncSeiMode sei_mode;

        int32_t width;
        int32_t height;
        int32_t hor_stride;
        int32_t ver_stride;
        MppFrameFormat fmt = MPP_FMT_YUV422_YUYV;
        MppCodingType type = MPP_VIDEO_CodingAVC;
        uint32_t num_frames;

        // resources
        size_t frame_size;
        size_t mdinfo_size;

        int32_t gop = 60;
        int32_t fps = 30;
        int32_t bps;

        FILE *fp_output;
    };
    MPP_ENC_DATA m_enc_data{};
};

#endif //RKNN_INFER_PLUGIN_MPP_VIDEO_ENCODER_H
