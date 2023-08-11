/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频编码封装， 来源于 https://blog.csdn.net/qq_39839546/article/details/122023991
 */
#include <cstring>
#include "mpp_video_encoder.h"
#include "mpp_video_utils.h"
#include "utils_log.h"

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

MppVideoEncoder::MppVideoEncoder(const std::string &video_path,
                                 int32_t width,int32_t height,
                                 MppFrameFormat fmt,
                                 MppCodingType type,
                                 int32_t fps, int32_t gop){
    memset(&m_enc_data, 0, sizeof(m_enc_data));

    m_enc_data.fp_output = fopen(video_path.c_str(), "wb+");
    CHECK_VAL(m_enc_data.fp_output == nullptr, d_mpp_module_error("failed to open output file %s", video_path.c_str()); return;)

    //使用输入的配置初始化编码器
    if(width != 0 && height != 0){
        int ret = init_encoder(width, height, fmt, type, fps, gop);
        CHECK_VAL(ret != MPP_OK, d_mpp_module_error("init encoder failed!"); return;)
        m_mpp_init_flag = true;
        d_mpp_module_info("init encoder success!")
    }
    m_init_flag = true;
}

MppVideoEncoder::~MppVideoEncoder() {
    uninit_encoder();
}

int MppVideoEncoder::uninit_encoder(){
    if(m_enc_data.fp_output != nullptr){
        fclose(m_enc_data.fp_output);
    }
    if (m_enc_data.ctx) {
        m_enc_data.mpi->reset(m_enc_data.ctx);
        mpp_destroy(m_enc_data.ctx);
        m_enc_data.ctx = nullptr;
    }

    if (m_enc_data.frm_buf) {
        mpp_buffer_put(m_enc_data.frm_buf);
        m_enc_data.frm_buf = nullptr;
    }

    if (m_enc_data.pkt_buf) {
        mpp_buffer_put(m_enc_data.pkt_buf);
        m_enc_data.pkt_buf = nullptr;
    }

    if(m_enc_data.md_info){
        mpp_buffer_put(m_enc_data.md_info);
        m_enc_data.md_info = nullptr;
    }
    if (m_enc_data.buf_grp) {
        mpp_buffer_group_put(m_enc_data.buf_grp);
        m_enc_data.buf_grp = nullptr;
    }
    d_mpp_module_info("MppVideoEncoder release success! buffer now total: %d",
                      mpp_buffer_total_now())
    return 0;
}

int MppVideoEncoder::init_encoder(int32_t width,int32_t height,
                                  MppFrameFormat fmt,
                                  MppCodingType type,
                                  int32_t fps, int32_t gop) {
    MPP_RET ret = MPP_OK;

    m_enc_data.width = width;
    m_enc_data.height = height;
    m_enc_data.hor_stride = MPP_ALIGN(m_enc_data.width, 16);
    m_enc_data.ver_stride = MPP_ALIGN(m_enc_data.height, 16);
    m_enc_data.fmt = fmt;
    m_enc_data.type = type;
    m_enc_data.fps = fps;
    m_enc_data.gop = gop;

    m_enc_data.mdinfo_size  = (MPP_VIDEO_CodingHEVC == m_enc_data.type) ?
                         (MPP_ALIGN(m_enc_data.hor_stride, 32) >> 5) *
                         (MPP_ALIGN(m_enc_data.ver_stride, 32) >> 5) * 16 :
                         (MPP_ALIGN(m_enc_data.hor_stride, 64) >> 6) *
                         (MPP_ALIGN(m_enc_data.ver_stride, 16) >> 4) * 16;
    //不同的图像格式所占的内存大小和其长宽的关系是不同的
    //所以要根据不同的输入图像格式为编码器编码开辟不同的内存大小，
    if (m_enc_data.fmt <= MPP_FMT_YUV420SP_VU){
        m_enc_data.frame_size = m_enc_data.hor_stride * m_enc_data.ver_stride * 3/2;

    } else if (m_enc_data.fmt <= MPP_FMT_YUV422_UYVY) {
        m_enc_data.hor_stride *= 2;
        m_enc_data.frame_size = m_enc_data.hor_stride * m_enc_data.ver_stride;
    } else {
        m_enc_data.frame_size = m_enc_data.hor_stride * m_enc_data.ver_stride * 4;
    }
    d_mpp_module_info("frame_size : %d", m_enc_data.frame_size)

    //开辟编码时需要的内存
    ret = mpp_buffer_group_get_internal(&m_enc_data.buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        d_mpp_module_error("failed to get mpp buffer group ret %d", ret);
        goto MPP_INIT_OUT;
    }
    ret = mpp_buffer_get(m_enc_data.buf_grp, &m_enc_data.frm_buf, m_enc_data.frame_size);
    if (ret) {
        d_mpp_module_error("failed to get buffer for input frame ret %d", ret);
        goto MPP_INIT_OUT;
    }
    ret = mpp_buffer_get(m_enc_data.buf_grp, &m_enc_data.pkt_buf, m_enc_data.frame_size);
    if (ret) {
        d_mpp_module_error("failed to get buffer for output packet ret %d", ret);
        goto MPP_INIT_OUT;
    }
    ret = mpp_buffer_get(m_enc_data.buf_grp, &m_enc_data.md_info, m_enc_data.mdinfo_size);
    if (ret) {
        d_mpp_module_error("failed to get buffer for motion info output packet ret %d", ret);
        goto MPP_INIT_OUT;
    }

    //创建 MPP context 和 MPP api 接口
    ret = mpp_create(&m_enc_data.ctx, &m_enc_data.mpi);
    if (ret) {
        d_mpp_module_error("mpp_create failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    /*
     * 初始化编码还是解码，以及编解码的格式
     * MPP_CTX_DEC ： 解码
     * MPP_CTX_ENC ： 编码
     *
     * MPP_VIDEO_CodingAVC ： H.264
     * MPP_VIDEO_CodingHEVC :  H.265
     * MPP_VIDEO_CodingVP8 :  VP8
     * MPP_VIDEO_CodingVP9 :  VP9
     * MPP_VIDEO_CodingMJPEG : MJPEG
     */
    ret = mpp_init(m_enc_data.ctx, MPP_CTX_ENC, m_enc_data.type);
    if (ret) {
        d_mpp_module_error("mpp_init failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    // 设置编码参数：宽高、对齐后宽高等参数
    m_enc_data.bps = m_enc_data.width * m_enc_data.height / 8 * m_enc_data.fps;
    m_enc_data.prep_cfg.change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                                        MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                                        MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    m_enc_data.prep_cfg.width         = m_enc_data.width;
    m_enc_data.prep_cfg.height        = m_enc_data.height;
    m_enc_data.prep_cfg.hor_stride    = m_enc_data.hor_stride;
    m_enc_data.prep_cfg.ver_stride    = m_enc_data.ver_stride;
    m_enc_data.prep_cfg.format        = m_enc_data.fmt;
    m_enc_data.prep_cfg.rotation      = MPP_ENC_ROT_0;
    ret = m_enc_data.mpi->control(m_enc_data.ctx, MPP_ENC_SET_PREP_CFG, &m_enc_data.prep_cfg);
    if (ret) {
        d_mpp_module_error("mpi control enc set prep cfg failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    /*设置编码码率、质量、定码率变码率*/
    m_enc_data.rc_cfg.change  = MPP_ENC_RC_CFG_CHANGE_ALL;
    m_enc_data.rc_cfg.rc_mode = MPP_ENC_RC_MODE_VBR;
    m_enc_data.rc_cfg.quality = MPP_ENC_RC_QUALITY_MEDIUM;
    if (m_enc_data.rc_cfg.rc_mode == MPP_ENC_RC_MODE_CBR) {
        /* constant bitrate has very small bps range of 1/16 bps */
        m_enc_data.rc_cfg.bps_target   = m_enc_data.bps;
        m_enc_data.rc_cfg.bps_max      = m_enc_data.bps * 17 / 16;
        m_enc_data.rc_cfg.bps_min      = m_enc_data.bps * 15 / 16;
    } else if (m_enc_data.rc_cfg.rc_mode ==  MPP_ENC_RC_MODE_VBR) {
        if (m_enc_data.rc_cfg.quality == MPP_ENC_RC_QUALITY_CQP) {
            /* constant QP does not have bps */
            m_enc_data.rc_cfg.bps_target   = -1;
            m_enc_data.rc_cfg.bps_max      = -1;
            m_enc_data.rc_cfg.bps_min      = -1;
        } else {
            /* variable bitrate has large bps range */
            m_enc_data.rc_cfg.bps_target   = m_enc_data.bps;
            m_enc_data.rc_cfg.bps_max      = m_enc_data.bps * 17 / 16;
            m_enc_data.rc_cfg.bps_min      = m_enc_data.bps * 1 / 16;
        }
    }
    /* fix input / output frame rate */
    m_enc_data.rc_cfg.fps_in_flex      = 0;
    m_enc_data.rc_cfg.fps_in_num       = m_enc_data.fps;
    m_enc_data.rc_cfg.fps_in_denorm    = 1;
    m_enc_data.rc_cfg.fps_out_flex     = 0;
    m_enc_data.rc_cfg.fps_out_num      = m_enc_data.fps;
    m_enc_data.rc_cfg.fps_out_denorm   = 1;

    m_enc_data.rc_cfg.gop              = m_enc_data.gop;
    m_enc_data.rc_cfg.skip_cnt         = 0;

    ret = m_enc_data.mpi->control(m_enc_data.ctx, MPP_ENC_SET_RC_CFG, &m_enc_data.rc_cfg);
    if (ret) {
        d_mpp_module_error("mpi control enc set rc cfg failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    /*设置264相关的其他编码参数*/
    m_enc_data.codec_cfg.coding = m_enc_data.type;
    switch (m_enc_data.codec_cfg.coding) {
        case MPP_VIDEO_CodingAVC : {
            m_enc_data.codec_cfg.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                                                 MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                                                 MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;
            /*
             * H.264 profile_idc parameter
             * 66  - Baseline profile
             * 77  - Main profile
             * 100 - High profile
             */
            m_enc_data.codec_cfg.h264.profile  = 77;
            /*
             * H.264 level_idc parameter
             * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
             * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
             * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
             * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
             * 50 / 51 / 52         - 4K@30fps
             */
            m_enc_data.codec_cfg.h264.level    = 40;
            m_enc_data.codec_cfg.h264.entropy_coding_mode  = 1;
            m_enc_data.codec_cfg.h264.cabac_init_idc  = 0;
            m_enc_data.codec_cfg.h264.transform8x8_mode = 1;
        }
            break;
        case MPP_VIDEO_CodingMJPEG : {
            m_enc_data.codec_cfg.jpeg.change  = MPP_ENC_JPEG_CFG_CHANGE_QP;
            m_enc_data.codec_cfg.jpeg.quant   = 10;
        }
            break;
        case MPP_VIDEO_CodingVP8 :
        case MPP_VIDEO_CodingHEVC :
        default : {
            d_mpp_module_info("support encoder coding type %d", m_enc_data.codec_cfg.coding);
        }
            break;
    }
    ret = m_enc_data.mpi->control(m_enc_data.ctx, MPP_ENC_SET_CODEC_CFG, &m_enc_data.codec_cfg);
    if (ret) {
        d_mpp_module_error("mpi control enc set codec cfg failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    /* optional */
    m_enc_data.sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = m_enc_data.mpi->control(m_enc_data.ctx, MPP_ENC_SET_SEI_CFG, &m_enc_data.sei_mode);
    if (ret) {
        d_mpp_module_error("mpi control enc set sei cfg failed ret %d", ret);
        goto MPP_INIT_OUT;
    }

    if (m_enc_data.type == MPP_VIDEO_CodingAVC) {
        MppPacket packet = nullptr;
        /*
         * Can use packet with normal malloc buffer as input not pkt_buf.
         * Please refer to vpu_api_legacy.cpp for normal buffer case.
         * Using pkt_buf buffer here is just for simplifing demo.
         */
        mpp_packet_init_with_buffer(&packet, m_enc_data.pkt_buf);
        /* NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);
        ret = m_enc_data.mpi->control(m_enc_data.ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
        if (MPP_OK != ret) {
            d_mpp_module_error("mpi control enc get extra info failed, ret : %d", ret);
            goto MPP_INIT_OUT;
        }

        /* get and write sps/pps for H.264 */
        d_mpp_module_info("mpi control enc get extra info successful, packet : %p", packet)
        if (packet) {
            void *ptr = mpp_packet_get_pos(packet);
            size_t len = mpp_packet_get_length(packet);
            if (m_enc_data.fp_output) {
                d_mpp_module_info("write extra data %d bytes", len)
                uint32_t w_len = fwrite(ptr, 1, len, m_enc_data.fp_output);
                if (w_len != len){
                    d_mpp_module_error("failed to save extra data! w_len %d len %d", w_len, len);
                    goto MPP_INIT_OUT;
                }
            }
            mpp_packet_deinit(&packet);
        }
    }
    return 0;

MPP_INIT_OUT:
    uninit_encoder();
    d_mpp_module_error("init mpp failed!");
    return -1;
}

/****************************************************************************
MppPacket  ：   存放编码数据，例如264、265数据
MppFrame  ：   存放解码的数据，例如YUV、RGB数据
MppTask   :     一次编码或者解码的session

编码就是push MppFrame，输出MppPacket；
解码就是push MppPacket，输出MppFrame；

MPI包含两套接口做编解码：
一套是简易接口， 类似 decode_put_packet / decode_get_frame 这样put/get即可
一套是高级接口， 类似 poll / enqueue/ dequeue 这样的对input output队列进行操作
*****************************************************************************/
bool MppVideoEncoder::process_image(uint8_t *image_data,
                                    int32_t width,int32_t height,
                                    MppFrameFormat fmt,
                                    MppCodingType type,
                                    int32_t fps, int32_t gop) {
    // 初次处理图像时初始化编码器
    if(!m_mpp_init_flag && width != 0) {
        int ret = init_encoder(width, height, fmt, type, fps, gop);
        CHECK_VAL(ret != MPP_OK, d_mpp_module_error("init encoder failed!"); return false;)

        m_mpp_init_flag = true;
        d_mpp_module_info("init encoder success!")
    }

    MPP_RET ret = MPP_OK;
    MppFrame frame = nullptr;
    MppPacket packet = nullptr;
    //获得开辟的内存的首地址
    void *buf = mpp_buffer_get_ptr(m_enc_data.frm_buf);

    // TODO: improve performance here?
    // 从输入图像的首地址开始读取图像数据，但是读取时会考虑16位对齐，即读取的长和宽都是16的整数倍。
    // 若图像一行或者一列不满16整数倍，则会用空数据补齐行和列
    yuv_add_stride(image_data,
                    m_enc_data.width, m_enc_data.height,
                    m_enc_data.hor_stride, m_enc_data.ver_stride,
                    m_enc_data.fmt,
                    (uint8_t *)buf);
    ret = mpp_frame_init(&frame);
    if (ret) {
        d_mpp_module_error("mpp_frame_init failed\n");
        return true;
    }
    //设置编码图像的格式
    mpp_frame_set_width(frame, m_enc_data.width);
    mpp_frame_set_height(frame, m_enc_data.height);
    mpp_frame_set_hor_stride(frame, m_enc_data.hor_stride);
    mpp_frame_set_ver_stride(frame, m_enc_data.ver_stride);
    mpp_frame_set_fmt(frame, m_enc_data.fmt);
    mpp_frame_set_buffer(frame, m_enc_data.frm_buf);
//    mpp_frame_set_buf_size(frame, m_enc_data.buf_size);
    mpp_frame_set_eos(frame, m_enc_data.frm_eos);

    MppMeta meta = mpp_frame_get_meta(frame);
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
    mpp_meta_set_buffer(meta, KEY_MOTION_INFO, m_enc_data.md_info);

    mpp_packet_init_with_buffer(&packet, m_enc_data.pkt_buf);
    /* NOTE: It is important to clear output packet length!! */
    mpp_packet_set_length(packet, 0);
    //输入图像进行编码
    ret = m_enc_data.mpi->encode_put_frame(m_enc_data.ctx, frame);
    mpp_frame_deinit(&frame);
    if (ret) {
        d_mpp_module_error("mpp encode put frame failed")
        return false;
    }
    //获得编码后的packet
    ret = m_enc_data.mpi->encode_get_packet(m_enc_data.ctx, &packet);
    if (ret) {
        d_mpp_module_error("mpp encode get packet failed")
        return false;
    }
    //获得编码后的数据长度和首地址，将其写入文件
    if (packet) {
        // write packet to file here
        void *ptr   = mpp_packet_get_pos(packet);
        size_t len  = mpp_packet_get_length(packet);
        m_enc_data.pkt_eos = mpp_packet_get_eos(packet);

        if (m_enc_data.fp_output) {
            fwrite(ptr, 1, len, m_enc_data.fp_output);
        }
        mpp_packet_deinit(&packet);

        m_enc_data.stream_size += len;
        m_enc_data.frame_count++;

        if (m_enc_data.pkt_eos) {
            d_mpp_module_warn("found last packet")
        }
    }

    if (m_enc_data.num_frames && m_enc_data.frame_count >= m_enc_data.num_frames) {
        d_mpp_module_error("encode max %d frames", m_enc_data.frame_count)
        return false;
    }
    if (m_enc_data.frm_eos && m_enc_data.pkt_eos) {
        d_mpp_module_error("encode frm_eos  %d pkt_eos %d ", m_enc_data.frm_eos, m_enc_data.pkt_eos)
        return false;
    }
    return true;
}
