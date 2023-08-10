/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.06
 * @brief: 插件公用函数
 */
#ifndef RKNN_INFER_PLUGIN_COMMON_H
#define RKNN_INFER_PLUGIN_COMMON_H

#include <cstring>

static int rknn_plugin_get_top (
        const float* p_prob,
        float* p_max_prob,
        uint32_t* p_max_class,
        uint32_t output_count,
        uint32_t top_num){
    uint32_t i, j;
    memset(p_max_prob, 0, sizeof(float) * top_num);
    memset(p_max_class, 0xff, sizeof(float) * top_num);

    for (j = 0; j < top_num; j++) {
        for (i = 0; i < output_count; i++) {
            if ((i == *(p_max_class + 0)) || (i == *(p_max_class + 1)) || (i == *(p_max_class + 2)) || (i == *(p_max_class + 3)) ||
                (i == *(p_max_class + 4))) {
                continue;
            }

            if (p_prob[i] > *(p_max_prob + j)) {
                *(p_max_prob + j) = p_prob[i];
                *(p_max_class + j) = i;
            }
        }
    }
    return 1;
}


#endif //RKNN_INFER_PLUGIN_COMMON_H
