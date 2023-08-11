/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.09
 * @brief: Rockchip MPP 视频解码封装，参考 https://blog.csdn.net/viengo/article/details/121439122
 */

#include <string>
#include <execution>
#include "mpp_video_decoder.h"
#include "utils_log.h"

MppVideoDecoder::MppVideoDecoder(const std::string &video_path) {
    // 打开视频输入源
    m_in_fp = fopen(video_path.c_str(), "rb");
    CHECK_VAL(m_in_fp == nullptr, d_mpp_module_error("open %s failed!", video_path.c_str()); return;)

    // 初始化解码器
    int ret = init_decoder();
    CHECK_VAL(ret < 0, d_mpp_module_error("init decoder failed!"); return;)

    m_init_flag = true;
}

MppVideoDecoder::~MppVideoDecoder() {
    if(m_in_fp != nullptr){
        fclose(m_in_fp);
        m_in_buf = nullptr;
    }
    if(m_pkt){
        mpp_packet_deinit(&m_pkt);
        m_pkt = nullptr;
    }
    if(m_in_buf != nullptr){
        free(m_in_buf);
        m_in_buf = nullptr;
    }
    if(m_mpp_mpi && m_mpp_ctx){
        m_mpp_mpi->reset(m_mpp_ctx);
        m_mpp_mpi = nullptr;
    }
    if(m_mpp_ctx != nullptr){
        mpp_destroy(m_mpp_ctx);
        m_mpp_ctx = nullptr;
    }
    if(m_frame_buffer_group != nullptr){
        mpp_buffer_group_put(m_frame_buffer_group);
        m_frame_buffer_group = nullptr;
    }
    d_mpp_module_info("MppVideoDecoder release success! buffer now total: %d, unreleased frame count :%d ",
                      mpp_buffer_total_now(),
                      m_frame_count)
}

int MppVideoDecoder::init_decoder() {
    // 初始化解码器上下文，MppCtx MppApi
    MPP_RET ret = mpp_create(&m_mpp_ctx, &m_mpp_mpi);
    CHECK_VAL(MPP_OK != ret, d_mpp_module_error("mpp_create error"); return -1;)

    // 设置解码器参数
    RK_U32 need_split = 1;
    ret = m_mpp_mpi->control(m_mpp_ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, (MppParam*)&need_split);
    CHECK_VAL(MPP_OK != ret, d_mpp_module_error("m_mpp_mpi->control error MPP_DEC_SET_PARSER_SPLIT_MODE"); return -1;)

    ret = mpp_init(m_mpp_ctx, MPP_CTX_DEC, MPP_VIDEO_CodingAVC);  // 固定为H264
    CHECK_VAL(MPP_OK != ret, d_mpp_module_error("mpp_init error"); return -1;)

    // 申请解码器输入缓冲区，初始化packet
    m_in_buf = (char*)malloc(MAX_READ_BUFFER_SIZE);
    CHECK_VAL(nullptr == m_in_buf, d_mpp_module_error("malloc m_in_buf error"); return -1;)
    memset(m_in_buf, 0, MAX_READ_BUFFER_SIZE);

    ret = mpp_packet_init(&m_pkt, m_in_buf, MAX_READ_BUFFER_SIZE);
    CHECK_VAL(MPP_OK != ret, d_mpp_module_error("mpp_packet_init error"); return -1;)
    mpp_packet_set_length(m_pkt, 0);
    return 0;
}

int MppVideoDecoder::get_next_frame(DecoderMppFrame &decoder_frame) {
    if(m_video_eos){
        // 已经读到最后一帧了
        d_rknn_plugin_warn("video eos!")
        return -1;
    }
    if(m_frame_count >= MAX_DECODER_FRAME_NUM){
        d_mpp_module_warn("unreleased frame count reach max: %d", MAX_DECODER_FRAME_NUM)
    }

    // 读取一个数据帧
    bool get_valid_frame = false;
    while (!get_valid_frame){
        MPP_RET ret = MPP_OK;
        // 读取数据包
        get_one_packet();

        // 设置数据包
        int pkt_len = (int)mpp_packet_get_length(m_pkt);
        if(pkt_len > 0){
            ret = m_mpp_mpi->decode_put_packet(m_mpp_ctx, m_pkt);
            d_mpp_module_info("pkt send ret:%d remain:%d", ret, (int)mpp_packet_get_length(m_pkt))
        }

        // 解析帧
        ret = m_mpp_mpi->decode_get_frame(m_mpp_ctx, &decoder_frame.mpp_frame);
        if (MPP_OK != ret || !decoder_frame.mpp_frame) {
            d_mpp_module_debug("decode_get_frame failed ret:%d, frame:%p", ret, decoder_frame.mpp_frame);
            // 等待一下2ms，通常1080p解码时间2ms
            usleep(2000);
            continue;
        }

        decoder_frame.hor_stride = mpp_frame_get_hor_stride(decoder_frame.mpp_frame);
        decoder_frame.ver_stride = mpp_frame_get_ver_stride(decoder_frame.mpp_frame);
        decoder_frame.hor_width = mpp_frame_get_width(decoder_frame.mpp_frame);
        decoder_frame.ver_height = mpp_frame_get_height(decoder_frame.mpp_frame);
        decoder_frame.data_size = mpp_frame_get_buf_size(decoder_frame.mpp_frame);
        d_mpp_module_debug("decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d",
                           decoder_frame.hor_width,
                           decoder_frame.ver_height,
                           decoder_frame.hor_stride,
                           decoder_frame.ver_stride,
                           decoder_frame.data_size)

        if (mpp_frame_get_info_change(decoder_frame.mpp_frame)){
            d_mpp_module_warn("decode_get_frame info changed")
            if(m_frame_buffer_group == nullptr){
                /* If buffer group is not set create one and limit it */
                ret = mpp_buffer_group_get_internal(&m_frame_buffer_group, MPP_BUFFER_TYPE_DRM);
                if (ret) {
                    d_mpp_module_error("%p get mpp buffer group failed ret %d ", m_mpp_ctx, ret);
                    mpp_frame_deinit(&decoder_frame.mpp_frame);
                    return -1;
                }

                /* Set buffer to mpp decoder */
                ret = m_mpp_mpi->control(m_mpp_ctx, MPP_DEC_SET_EXT_BUF_GROUP, m_frame_buffer_group);
                if (ret) {
                    d_mpp_module_error("%p set buffer group failed ret %d ", m_mpp_ctx, ret);
                    mpp_frame_deinit(&decoder_frame.mpp_frame);
                    return -1;
                }
            }else{
                /* If old buffer group exist clear it */
                ret = mpp_buffer_group_clear(m_frame_buffer_group);
                if (ret) {
                    d_mpp_module_error("%p clear buffer group failed ret %d ", m_mpp_ctx, ret);
                    mpp_frame_deinit(&decoder_frame.mpp_frame);
                    return -1;
                }
            }

            // Use limit config to limit buffer count
            ret = mpp_buffer_group_limit_config(m_frame_buffer_group, decoder_frame.data_size, MAX_DECODER_FRAME_NUM);
            if (ret) {
                d_mpp_module_error("%p limit buffer group failed ret %d ", m_mpp_ctx, ret);
                mpp_frame_deinit(&decoder_frame.mpp_frame);
                return -1;
            }

            // All buffer group config done. Set info change ready to let decoder continue decoding
            ret = m_mpp_mpi->control(m_mpp_ctx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);
            if (ret) {
                d_mpp_module_error("%p info change ready failed ret %d ", m_mpp_ctx, ret);
                mpp_frame_deinit(&decoder_frame.mpp_frame);
                return -1;
            }
        }else{
            d_mpp_module_debug("decode_get_frame success")
            uint32_t err_info = mpp_frame_get_errinfo(decoder_frame.mpp_frame) | mpp_frame_get_discard(decoder_frame.mpp_frame);
            if (err_info) {
                d_mpp_module_error("decoder_get_frame get err info:%d discard:%d",
                                   mpp_frame_get_errinfo(decoder_frame.mpp_frame),
                                   mpp_frame_get_discard(decoder_frame.mpp_frame))
            }
//            d_mpp_module_info("decoder_get_frame, format : %d", mpp_frame_get_fmt(decoder_frame.mpp_frame))
            decoder_frame.mpp_frame_format = mpp_frame_get_fmt(decoder_frame.mpp_frame);
            decoder_frame.data_buf = (char *) mpp_buffer_get_ptr(mpp_frame_get_buffer(decoder_frame.mpp_frame));
            decoder_frame.data_fd = mpp_buffer_get_fd(mpp_frame_get_buffer(decoder_frame.mpp_frame));
            get_valid_frame = true;
            m_frame_count++;
        }

        if (mpp_frame_get_eos(decoder_frame.mpp_frame)) {
            // 最后一帧
            d_mpp_module_info("mpp_frame_get_eos");
            m_video_eos = true;
        }
    }
    return 0;
}

void MppVideoDecoder::release_frame(DecoderMppFrame &decoder_frame) {
    m_frame_count--;
    mpp_frame_deinit(&decoder_frame.mpp_frame);
}

int MppVideoDecoder::get_one_packet() {
    int pkt_len = (int)mpp_packet_get_length(m_pkt);
    if (pkt_len > 0) {
        // 数据包未使用，不需要再读取
        d_mpp_module_warn("pkt get before remain:%d", pkt_len)
        return 0;
    }
    if (feof(m_in_fp)) {
        // 文件已经读完
        d_mpp_module_debug("file read end")
        return -1;
    }

    // 读取新的数据包
    uint32_t len = fread(m_in_buf, 1, MAX_READ_BUFFER_SIZE, m_in_fp);
    d_mpp_module_info("read file len:%d", len)
    if (len > 0) {
        mpp_packet_set_data(m_pkt, m_in_buf);
        mpp_packet_set_size(m_pkt, len);
        mpp_packet_set_pos(m_pkt, m_in_buf);
        mpp_packet_set_length(m_pkt, len);
        if (feof(m_in_fp) || len < MAX_READ_BUFFER_SIZE) {
            // 读到了最后一个包
            mpp_packet_set_eos(m_pkt);
            d_mpp_module_info("mpp_packet_set_eos")
        }
    }
    return 0;
}
