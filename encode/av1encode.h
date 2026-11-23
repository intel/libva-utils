#ifndef AV1_ENCODE_H_
#define AV1_ENCODE_H_

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SWBRC 1
struct Av1InputParameters
{
    char* srcyuv;
    char* recyuv;
    char* output;
    uint32_t profile;
	char *libpath;

    
    uint32_t order_hint_bits;
    uint32_t enable_cdef;
    uint32_t width;
    uint32_t height;
    uint32_t LDB;
    uint32_t frame_rate_extN;
    uint32_t frame_rate_extD;
    uint32_t level;

    // for brc
    uint32_t bit_rate;
    uint8_t MinBaseQIndex;
    uint8_t MaxBaseQIndex;

    uint32_t intra_period;
    uint32_t ip_period;
    uint32_t RateControlMethod;
    uint32_t BRefType;
    int encode_syncmode;
    int calc_psnr;
    int frame_count;
    int frame_width_aligned;
    int frame_height_aligned;
    uint32_t base_qindex;
    int bit_depth;
    int target_bitrate;
    int vbr_max_bitrate;
    int buffer_size;
    int initial_buffer_fullness;
    int enable_swbrc;
};



#ifdef __cplusplus
}
#endif

#endif//AV1_ENCODE_H_