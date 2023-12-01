/*
 * Copyright (c) 2018 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#define LIBVA_UTILS_UPLOAD_DOWNLOAD_YUV_SURFACE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <va/va.h>
#include <va/va_enc_hevc.h>
#include "va_display.h"
#define ALIGN16(x)  ((x+15)&~15)
#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }

#define CHECK_CONDITION(cond)                                                \
    if(!(cond))                                                              \
    {                                                                        \
        fprintf(stderr, "Unexpected condition: %s:%d\n", __func__, __LINE__);\
        exit(1);                                                             \
    }

#include "loadsurface.h"

#define NAL_REF_IDC_NONE        0
#define NAL_REF_IDC_LOW         1
#define NAL_REF_IDC_MEDIUM      2
#define NAL_REF_IDC_HIGH        3

#define FRAME_I 1
#define FRAME_P 2
#define FRAME_B 3
#define FRAME_IDR 7

// SLICE TYPE HEVC ENUM
enum {
    SLICE_B = 0,
    SLICE_P = 1,
    SLICE_I = 2,
};
#define IS_I_SLICE(type) (SLICE_I == (type))
#define IS_P_SLICE(type) (SLICE_P == (type))
#define IS_B_SLICE(type) (SLICE_B == (type))



#define ENTROPY_MODE_CAVLC      0
#define ENTROPY_MODE_CABAC      1

#define PROFILE_IDC_MAIN        1
#define PROFILE_IDC_MAIN10      2

#define BITSTREAM_ALLOCATE_STEPPING     4096
static  int LCU_SIZE = 32;

#define SURFACE_NUM 16 /* 16 surfaces for source YUV */
#define SURFACE_NUM 16 /* 16 surfaces for reference */
enum NALUType {
    NALU_TRAIL_N        = 0x00, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
    NALU_TRAIL_R        = 0x01, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
    NALU_TSA_N          = 0x02, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
    NALU_TSA_R          = 0x03, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
    NALU_STSA_N         = 0x04, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
    NALU_STSA_R         = 0x05, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
    NALU_RADL_N         = 0x06, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
    NALU_RADL_R         = 0x07, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
    NALU_RASL_N         = 0x08, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
    NALU_RASL_R         = 0x09, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
    /* 0x0a..0x0f - Reserved */
    NALU_BLA_W_LP       = 0x10, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_BLA_W_DLP      = 0x11, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_BLA_N_LP       = 0x12, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_IDR_W_DLP      = 0x13, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
    NALU_IDR_N_LP       = 0x14, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
    NALU_CRA            = 0x15, // Coded slice segment of an CRA picture - slice_segment_layer_rbsp, VLC
    /* 0x16..0x1f - Reserved */
    NALU_VPS            = 0x20, // Video parameter set - video_parameter_set_rbsp, non-VLC
    NALU_SPS            = 0x21, // Sequence parameter set - seq_parameter_set_rbsp, non-VLC
    NALU_PPS            = 0x22, // Picture parameter set - pic_parameter_set_rbsp, non-VLC
    NALU_AUD            = 0x23, // Access unit delimiter - access_unit_delimiter_rbsp, non-VLC
    NALU_EOS            = 0x24, // End of sequence - end_of_seq_rbsp, non-VLC
    NALU_EOB            = 0x25, // End of bitsteam - end_of_bitsteam_rbsp, non-VLC
    NALU_FD             = 0x26, // Filler data - filler_data_rbsp, non-VLC
    NALU_PREFIX_SEI     = 0x27, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
    NALU_SUFFIX_SEI     = 0x28, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
    /* 0x29..0x2f - Reserved */
    /* 0x30..0x3f - Unspecified */
    //this should be the last element of this enum
    //chagne this value if NAL unit type increased
    MAX_HEVC_NAL_TYPE   = 0x3f,

};

// Config const values
#define MAX_TEMPORAL_SUBLAYERS         8
#define MAX_LAYER_ID                   64
#define MAX_LONGTERM_REF_PIC           32
#define NUM_OF_EXTRA_SLICEHEADER_BITS  3
struct ProfileTierParamSet {
    uint8_t      general_profile_space;                                        //u(2)
    int          general_tier_flag;                                            //u(1)
    uint8_t      general_profile_idc;                                          //u(5)
    int          general_profile_compatibility_flag[32];                       //u(1)
    int          general_progressive_source_flag;                              //u(1)
    int          general_interlaced_source_flag;                               //u(1)
    int          general_non_packed_constraint_flag;                           //u(1)
    int          general_frame_only_constraint_flag;                           //u(1)
    int          general_reserved_zero_43bits[43];                             //u(1)
    int          general_reserved_zero_bit;                                    //u(1)
    uint8_t      general_level_idc;                                            //u(8)
};
// Video parameter set structure
struct VideoParamSet {
    uint8_t       vps_video_parameter_set_id;                                   //u(4)
    int           vps_base_layer_internal_flag;                                 //u(1)
    int           vps_base_layer_available_flag;                                //u(1)
    uint8_t       vps_max_layers_minus1;                                        //u(6)
    uint8_t       vps_max_sub_layers_minus1;                                    //u(3)
    int           vps_temporal_id_nesting_flag;                                 //u(1)
    uint16_t      vps_reserved_0xffff_16bits;                                   //u(16)

    struct        ProfileTierParamSet ptps;
    uint8_t       vps_max_nuh_reserved_zero_layer_id;
    uint32_t      vps_max_op_sets;
    uint32_t      vps_num_op_sets_minus1;

    int           vps_sub_layer_ordering_info_present_flag;                     //u(1)
    uint32_t      vps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS];      //ue(v)
    uint32_t      vps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS];              //ue(v)
    uint32_t      vps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS];        //ue(v)
    uint8_t       vps_max_layer_id;                                             //u(6)
    uint32_t      vps_num_layer_sets_minus1;                                    //ue(v)
    int           layer_id_included_flag[MAX_TEMPORAL_SUBLAYERS][MAX_LAYER_ID];   //u(1)
    int           vps_timing_info_present_flag;                                 //u(1)
    uint32_t      vps_num_units_in_tick;                                        //u(32)
    uint32_t      vps_time_scale;                                               //u(32
    int           vps_poc_proportional_to_timing_flag;                          //u(1)
    uint32_t      vps_num_ticks_poc_diff_one_minus1;                            //ue(v)
    uint32_t      vps_num_hrd_parameters;                                       //ue(v)
    uint32_t      hrd_layer_set_idx[MAX_TEMPORAL_SUBLAYERS];                     //ue(v)
    int           cprms_present_flag[MAX_TEMPORAL_SUBLAYERS];                    //u(1)
    int           vps_extension_flag;                                           //u(1)
    int           vps_extension_data_flag;                                      //u(1)
};

struct ShortTermRefPicParamSet {
    int         inter_ref_pic_set_prediction_flag;                               //u(1)
    uint32_t    delta_idx_minus1;                                               //ue(v)
    uint8_t     delta_rps_sign;                                                 //u(1)
    uint32_t    abs_delta_rps_minus1;                                           //ue(v)
    uint8_t     used_by_curr_pic_flag[32];                                      //u(1)
    uint8_t     use_delta_flag[32];                                             //u(1)
    uint32_t    num_negative_pics;                                              //ue(v)
    uint32_t    num_positive_pics;                                              //ue(v)
    uint32_t    delta_poc_s0_minus1[32];                                        //ue(v)
    uint8_t     used_by_curr_pic_s0_flag[32];                                   //u(1)
    uint32_t    delta_poc_s1_minus1[32];                                        //ue(v)
    uint8_t     used_by_curr_pic_s1_flag[32];                                   //u(1)
};
struct SeqParamSet {
    uint8_t     sps_video_parameter_set_id;                                     //u(4)
    uint8_t     sps_max_sub_layers_minus1;                                      //u(3)
    int         sps_temporal_id_nesting_flag;                                   //u(1)

    struct      ProfileTierParamSet ptps;
    uint32_t    sps_seq_parameter_set_id;                                       //ue(v)
    uint32_t    chroma_format_idc;                                              //ue(v)
    int         separate_colour_plane_flag;                                     //u(1)
    uint32_t    pic_width_in_luma_samples;                                      //ue(v)
    uint32_t    pic_height_in_luma_samples;                                     //ue(v)
    int         conformance_window_flag;                                        //u(1)
    uint32_t    conf_win_left_offset;                                           //ue(v)
    uint32_t    conf_win_right_offset;                                          //ue(v)
    uint32_t    conf_win_top_offset;                                            //ue(v)
    uint32_t    conf_win_bottom_offset;                                         //ue(v)
    uint32_t    bit_depth_luma_minus8;                                          //ue(v)
    uint32_t    bit_depth_chroma_minus8;                                        //ue(v)
    uint32_t    log2_max_pic_order_cnt_lsb_minus4;                              //ue(v)
    int         sps_sub_layer_ordering_info_present_flag;                       //u(1)
    uint32_t    sps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS];        //ue(v)
    uint32_t    sps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS];                //ue(v)
    uint32_t    sps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS];          //ue(v)
    uint32_t    log2_min_luma_coding_block_size_minus3;                         //ue(v)
    uint32_t    log2_diff_max_min_luma_coding_block_size;
    uint32_t    log2_max_coding_block_size_minus3; //ue(v)
    uint32_t    log2_min_luma_transform_block_size_minus2;                      //ue(v)
    uint32_t    log2_diff_max_min_luma_transform_block_size;                    //ue(v)
    uint32_t    max_transform_hierarchy_depth_inter;                            //ue(v)
    uint32_t    max_transform_hierarchy_depth_intra;                            //ue(v)
    uint8_t     scaling_list_enabled_flag;                                      //u(1)
    uint8_t     sps_scaling_list_data_present_flag;                             //u(1)
    uint8_t     amp_enabled_flag;                                               //u(1)
    uint8_t     sample_adaptive_offset_enabled_flag;                            //u(1)
    uint8_t     pcm_enabled_flag;                                               //u(1)
    uint8_t     pcm_sample_bit_depth_luma_minus1;                               //u(4)
    uint8_t     pcm_sample_bit_depth_chroma_minus1;                             //u(4)
    uint32_t    log2_min_pcm_luma_coding_block_size_minus3;
    uint32_t    log2_max_pcm_luma_coding_block_size_minus3;                     //ue(v)
    uint32_t    log2_diff_max_min_pcm_luma_coding_block_size;                   //ue(v)
    uint8_t     pcm_loop_filter_disabled_flag;                                  //u(1)
    uint32_t    num_short_term_ref_pic_sets;                                    //ue(v)

    struct      ShortTermRefPicParamSet strp[66];
    uint8_t     long_term_ref_pics_present_flag;                                //u(1)
    uint32_t    num_long_term_ref_pics_sps;                                     //ue(v)
    uint32_t    lt_ref_pic_poc_lsb_sps[MAX_LONGTERM_REF_PIC];                   //u(v)
    uint8_t     used_by_curr_pic_lt_sps_flag[MAX_LONGTERM_REF_PIC];             //u(1)
    uint8_t     sps_temporal_mvp_enabled_flag;                                  //u(1)
    uint8_t     strong_intra_smoothing_enabled_flag;                            //u(1)
    uint8_t     vui_parameters_present_flag;                                    //u(1)
    //VuiParameters   vui_parameters;
    int         sps_extension_present_flag;                                     //u(1)
    int         sps_range_extension_flag;                                       //u(1)
    int         sps_multilayer_extension_flag;                                  //u(1)
    int         sps_3d_extension_flag;                                          //u(1)
    uint8_t     sps_extension_5bits;                                           //u(5)
    int         sps_extension_data_flag;                                        //u(1)
};
struct PicParamSet {
    uint32_t    pps_pic_parameter_set_id;                                       //ue(v)
    uint32_t    pps_seq_parameter_set_id;                                       //ue(v)
    int         dependent_slice_segments_enabled_flag;                          //u(1)
    int         output_flag_present_flag;                                       //u(1)
    uint8_t     num_extra_slice_header_bits;                                    //u(3)
    int         sign_data_hiding_enabled_flag;                                  //u(1)
    int         cabac_init_present_flag;                                        //u(1)
    uint32_t    num_ref_idx_l0_default_active_minus1;                           //ue(v)
    uint32_t    num_ref_idx_l1_default_active_minus1;                           //ue(v)
    int32_t     init_qp_minus26;                                                //se(v)
    int         constrained_intra_pred_flag;                                    //u(1)
    int         transform_skip_enabled_flag;                                    //u(1)
    int         cu_qp_delta_enabled_flag;                                       //u(1)
    uint32_t    diff_cu_qp_delta_depth;                                         //ue(v)
    uint32_t    pps_cb_qp_offset;                                               //se(v)
    uint32_t    pps_cr_qp_offset;                                               //se(v)
    int         pps_slice_chroma_qp_offsets_present_flag;                       //u(1)
    int         weighted_pred_flag;                                             //u(1)
    int         weighted_bipred_flag;                                           //u(1)
    int         transquant_bypass_enabled_flag;                                 //u(1)
    int         tiles_enabled_flag;                                             //u(1)
    int         entropy_coding_sync_enabled_flag;                               //u(1)
    uint32_t    num_tile_columns_minus1;                                        //ue(v)
    uint32_t    num_tile_rows_minus1;                                           //ue(v)
    int         uniform_spacing_flag;                                           //u(1)
    uint32_t    *column_width_minus1;                                           //ue(v)
    uint32_t    *row_height_minus1;                                             //ue(v)
    int         loop_filter_across_tiles_enabled_flag;                          //u(1)
    int         pps_loop_filter_across_slices_enabled_flag;                     //u(1)
    int         deblocking_filter_control_present_flag;                         //u(1)
    int         deblocking_filter_override_enabled_flag;                        //u(1)
    int         pps_deblocking_filter_disabled_flag;                            //u(1)
    int32_t     pps_beta_offset_div2;                                           //se(v)
    int32_t     pps_tc_offset_div2;                                             //se(v)
    int         pps_scaling_list_data_present_flag;                             //u(1)
    int         lists_modification_present_flag;                                //u(1)
    uint32_t    log2_parallel_merge_level_minus2;                               //ue(v)
    int         slice_segment_header_extension_present_flag;                    //u(1)
    int         pps_extension_present_flag;                                     //u(1)
    int         pps_range_extension_flag;                                       //u(1)
    int         pps_multilayer_extension_flag;                                  //u(1)
    int         pps_3d_extension_flag;                                          //u(1)
    uint8_t     pps_extension_5bits;                                            //u(5)
    uint8_t     pps_extension_data_flag;                                        //u(1)
    uint32_t    log2_max_transform_skip_block_size_minus2;                      //ue(v)
    uint8_t     cross_component_prediction_enabled_flag;                        //ue(1)
    uint8_t     chroma_qp_offset_list_enabled_flag;                             //ue(1)
    uint32_t    diff_cu_chroma_qp_offset_depth;                                 //ue(v)
    uint32_t    chroma_qp_offset_list_len_minus1;                               //ue(v)
    uint32_t    cb_qp_offset_list[6];                                           //se(v)
    uint32_t    cr_qp_offset_list[6];                                           //se(v)
    uint32_t    log2_sao_offset_scale_luma;                                     //ue(v)
    uint32_t    log2_sao_offset_scale_chroma;                                   //ue(v)
};
struct SliceHeader {
    int         first_slice_segment_in_pic_flag;                                //u(1)
    int         no_output_of_prior_pics_flag;                                   //u(1)
    uint32_t    slice_pic_parameter_set_id;                                     //ue(v)
    int         dependent_slice_segment_flag;                                   //u(1)
    uint32_t    picture_width_in_ctus;
    uint32_t    picture_height_in_ctus;
    uint32_t    slice_segment_address;                                          //u(v)
    int         slice_reserved_undetermined_flag[NUM_OF_EXTRA_SLICEHEADER_BITS];               //u(1)
    uint32_t    slice_type;                                                     //ue(v)
    int         pic_output_flag;                                                //u(1)
    uint8_t     colour_plane_id;                                                //u(2)
    uint32_t    pic_order_cnt_lsb;
    uint32_t    num_negative_pics;
    uint32_t    num_positive_pics;
    uint32_t    delta_poc_s0_minus1;

    struct      ShortTermRefPicParamSet strp;
    int         short_term_ref_pic_set_sps_flag;                                //u(1)
    uint32_t    short_term_ref_pic_set_idx;                                     //u(v)
    uint32_t    num_long_term_sps;                                              //ue(v)
    uint32_t    num_long_term_pics;                                             //ue(v)
    uint32_t    *lt_idx_sps;                                                    //u(v)
    uint32_t    *poc_lsb_lt;                                                    //u(v)
    int         *used_by_curr_pic_lt_flag;                                      //u(1)
    int         *delta_poc_msb_present_flag;                                    //u(1)
    uint32_t    *delta_poc_msb_cycle_lt;                                        //ue(v)
    int         slice_temporal_mvp_enabled_flag;                                //u(1)
    int         slice_sao_luma_flag;                                            //u(1)
    int         slice_sao_chroma_flag;                                          //u(1)
    int         num_ref_idx_active_override_flag;                               //u(1)
    uint32_t    num_ref_idx_l0_active_minus1;                                   //ue(v)
    uint32_t    num_ref_idx_l1_active_minus1;
    uint32_t    num_poc_total_cur;
    int         ref_pic_list_modification_flag_l0;
    int         ref_pic_list_modification_flag_l1;
    uint32_t*   list_entry_l0;
    uint32_t*   list_entry_l1;

    int         ref_pic_list_combination_flag;

    uint32_t    num_ref_idx_lc_active_minus1;
    uint32_t    ref_pic_list_modification_flag_lc;
    int         pic_from_list_0_flag;
    uint32_t    ref_idx_list_curr;
    int         mvd_l1_zero_flag;                                               //u(1)
    int         cabac_init_present_flag;
    int         pic_temporal_mvp_enable_flag;

    int         collocated_from_l0_flag;                                        //u(1)
    uint32_t    collocated_ref_idx;                                             //ue(v)
    uint32_t    five_minus_max_num_merge_cand;                                  //ue(v)
    int32_t     delta_pic_order_cnt_bottom;                                     //se(v)
    int32_t     slice_qp_delta;                                                 //se(v)
    int32_t     slice_qp_delta_cb;                                             //se(v)
    int32_t     slice_qp_delta_cr;                                             //se(v)
    int         cu_chroma_qp_offset_enabled_flag;                               //u(1)
    int         deblocking_filter_override_flag;                                //u(1)
    int         disable_deblocking_filter_flag;                          //u(1)
    int32_t     beta_offset_div2;                                         //se(v)
    int32_t     tc_offset_div2;                                           //se(v)
    int         slice_loop_filter_across_slices_enabled_flag;                   //u(1)
    uint32_t    num_entry_point_offsets;                                        //ue(v)
    uint32_t    offset_len_minus1;                                              //ue(v)
    uint32_t    *entry_point_offset;                                     //u(v)
    uint32_t    slice_segment_header_extension_length;                          //ue(v)
    uint8_t     *slice_segment_header_extension_data_byte;                      //u(8)
};

struct BlockSizes {
    uint32_t log2_max_coding_tree_block_size_minus3;
    uint32_t log2_min_coding_tree_block_size_minus3;
    uint32_t log2_min_luma_coding_block_size_minus3;
    uint32_t log2_max_luma_transform_block_size_minus2;
    uint32_t log2_min_luma_transform_block_size_minus2;
    uint32_t log2_max_pcm_coding_block_size_minus3;
    uint32_t log2_min_pcm_coding_block_size_minus3;
    uint32_t max_max_transform_hierarchy_depth_inter;
    uint32_t min_max_transform_hierarchy_depth_inter;
    uint32_t max_max_transform_hierarchy_depth_intra;
    uint32_t min_max_transform_hierarchy_depth_intra;
};

struct Features {
    uint32_t amp; //sps->amp_enable_flag
    uint32_t constrained_intra_pred;
    uint32_t cu_qp_delta; // pps->cu_qp_delta_enabled_flag
    uint32_t deblocking_filter_disable;
    uint32_t dependent_slices;
    uint32_t pcm; // sps->pcm_enable_flag
    uint32_t sao; //sps->sample_adaptive_offset_enabled_flag
    uint32_t scaling_lists;
    uint32_t separate_colour_planes;
    uint32_t sign_data_hiding;
    uint32_t strong_intra_smoothing;
    uint32_t temporal_mvp; //sps->sps_temporal_mvp_enabled_flag
    uint32_t transform_skip; // pps->transform_skip_enabled_flag
    uint32_t transquant_bypass;
    uint32_t weighted_prediction;
};

static  struct VideoParamSet vps;
static  struct SeqParamSet sps;
static  struct PicParamSet pps;
static  struct SliceHeader ssh;
static  struct BlockSizes block_sizes;
static  int use_block_sizes = 0;
static  struct Features features;
static  int use_features = 0;
static  VADisplay va_dpy;
static  VAProfile hevc_profile = ~0;
static  int real_hevc_profile = 0;
static  VAEntrypoint entryPoint = VAEntrypointEncSlice;
static  int p2b = 1;
static  int lowpower = 0;
static  VAConfigAttrib attrib[VAConfigAttribTypeMax];
static  VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
static  int config_attrib_num = 0, enc_packed_header_idx;
static  VASurfaceID src_surface[SURFACE_NUM];
static  VABufferID  coded_buf[SURFACE_NUM];
static  VASurfaceID ref_surface[SURFACE_NUM];
static  VAConfigID config_id;
static  VAContextID context_id;
static  struct ProfileTierParamSet protier_param;

static  VAEncSequenceParameterBufferHEVC seq_param;
static  VAEncPictureParameterBufferHEVC pic_param;
static  VAEncSliceParameterBufferHEVC slice_param;
static  VAPictureHEVC CurrentCurrPic;
static  VAPictureHEVC ReferenceFrames[16], RefPicList0_P[32], RefPicList0_B[32], RefPicList1_B[32];

static  unsigned int MaxPicOrderCntLsb = (2 << 8);

static  unsigned int num_ref_frames = 2;
static  unsigned int num_active_ref_p = 1;
static  unsigned int numShortTerm = 0;
static  int constraint_set_flag = 0;
static  int hevc_packedheader = 0;
static  int hevc_maxref = 16;

static  char *coded_fn = NULL, *srcyuv_fn = NULL, *recyuv_fn = NULL;
static  FILE *coded_fp = NULL, *srcyuv_fp = NULL, *recyuv_fp = NULL;
static  unsigned long long srcyuv_frames = 0;
static  int srcyuv_fourcc = VA_FOURCC_NV12;
static  int calc_psnr = 0;

static  int frame_width = 176;
static  int frame_height = 144;
static  int frame_width_aligned;
static  int frame_height_aligned;
static  int frame_rate = 30;
static  unsigned int frame_count = 60;
static  unsigned int frame_coded = 0;
static  unsigned int frame_bitrate = 0;
static  unsigned int frame_slices = 1;
static  double frame_size = 0;
static  int initial_qp = 26;
static  int minimal_qp = 0;
static  int intra_period = 30;
static  int intra_idr_period = 60;
static  int ip_period = 1;
static  int rc_mode = -1;
static  int rc_default_modes[] = {
    VA_RC_VBR,
    VA_RC_CQP,
    VA_RC_VBR_CONSTRAINED,
    VA_RC_CBR,
    VA_RC_VCM,
    VA_RC_NONE,
};
static  unsigned long long current_frame_encoding = 0;
static  unsigned long long current_frame_display = 0;
static  unsigned long long current_IDR_display = 0;
static  unsigned int current_frame_num = 0;
static  int current_frame_type;
#define current_slot (current_frame_display % SURFACE_NUM)

static  int misc_priv_type = 0;
static  int misc_priv_value = 0;

#define MIN(a, b) ((a)>(b)?(b):(a))
#define MAX(a, b) ((a)>(b)?(a):(b))

/* thread to save coded data/upload source YUV */
struct storage_task_t {
    void *next;
    unsigned long long display_order;
    unsigned long long encode_order;
};
static  struct storage_task_t *storage_task_header = NULL, *storage_task_tail = NULL;
#define SRC_SURFACE_IN_ENCODING 0
#define SRC_SURFACE_IN_STORAGE  1
static  int srcsurface_status[SURFACE_NUM];
static  int encode_syncmode = 0;
static  pthread_mutex_t encode_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_cond_t  encode_cond = PTHREAD_COND_INITIALIZER;
static  pthread_t encode_thread;

/* for performance profiling */
static unsigned int UploadPictureTicks = 0;
static unsigned int BeginPictureTicks = 0;
static unsigned int RenderPictureTicks = 0;
static unsigned int EndPictureTicks = 0;
static unsigned int SyncPictureTicks = 0;
static unsigned int SavePictureTicks = 0;
static unsigned int TotalTicks = 0;

struct __bitstream {
    unsigned int *buffer;
    int bit_offset;
    int max_size_in_dword;
};
typedef struct __bitstream bitstream;

static unsigned int
va_swap32(unsigned int val)
{
    unsigned char *pval = (unsigned char *)&val;

    return ((pval[0] << 24)     |
            (pval[1] << 16)     |
            (pval[2] << 8)      |
            (pval[3] << 0));
}

static void
bitstream_start(bitstream *bs)
{
    bs->max_size_in_dword = BITSTREAM_ALLOCATE_STEPPING;
    bs->buffer = calloc(bs->max_size_in_dword * sizeof(int), 1);
    assert(bs->buffer);
    bs->bit_offset = 0;
}

static void
bitstream_end(bitstream *bs)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (bit_offset) {
        bs->buffer[pos] = va_swap32((bs->buffer[pos] << bit_left));
    }
}

static void
put_ui(bitstream *bs, unsigned int val, int size_in_bits)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (!size_in_bits)
        return;

    bs->bit_offset += size_in_bits;

    if (bit_left > size_in_bits) {
        bs->buffer[pos] = (bs->buffer[pos] << size_in_bits | val);
    } else {
        size_in_bits -= bit_left;
        bs->buffer[pos] = (bs->buffer[pos] << bit_left) | (val >> size_in_bits);
        bs->buffer[pos] = va_swap32(bs->buffer[pos]);

        if (pos + 1 == bs->max_size_in_dword) {
            bs->max_size_in_dword += BITSTREAM_ALLOCATE_STEPPING;
            bs->buffer = realloc(bs->buffer, bs->max_size_in_dword * sizeof(unsigned int));
            assert(bs->buffer);
        }

        bs->buffer[pos + 1] = val;
    }
}

static void
put_ue(bitstream *bs, unsigned int val)
{
    int size_in_bits = 0;
    int tmp_val = ++val;

    while (tmp_val) {
        tmp_val >>= 1;
        size_in_bits++;
    }

    put_ui(bs, 0, size_in_bits - 1); // leading zero
    put_ui(bs, val, size_in_bits);
}

static void
put_se(bitstream *bs, int val)
{
    unsigned int new_val;

    if (val <= 0)
        new_val = -2 * val;
    else
        new_val = 2 * val - 1;

    put_ue(bs, new_val);
}

static void
byte_aligning(bitstream *bs, int bit)
{
    int bit_offset = (bs->bit_offset & 0x7);
    int bit_left = 8 - bit_offset;
    int new_val;

    if (!bit_offset)
        return;

    assert(bit == 0 || bit == 1);

    if (bit)
        new_val = (1 << bit_left) - 1;
    else
        new_val = 0;

    put_ui(bs, new_val, bit_left);
}

static void
rbsp_trailing_bits(bitstream *bs)
{
    put_ui(bs, 1, 1);
    byte_aligning(bs, 0);
}

static void nal_start_code_prefix(bitstream *bs, int nal_unit_type)
{
    if (nal_unit_type == NALU_VPS ||
        nal_unit_type == NALU_SPS ||
        nal_unit_type == NALU_PPS ||
        nal_unit_type == NALU_AUD)
        put_ui(bs, 0x00000001, 32);
    else
        put_ui(bs, 0x000001, 24);
}

static void nal_header(bitstream *bs, int nal_unit_type)
{
    put_ui(bs, 0, 1);                /* forbidden_zero_bit: 0 */
    put_ui(bs, nal_unit_type, 6);
    put_ui(bs, 0, 6);
    put_ui(bs, 1, 3);
}

static int calc_poc(int pic_order_cnt_lsb)
{
    static int picOrderCntMsb_ref = 0, pic_order_cnt_lsb_ref = 0;
    int prevPicOrderCntMsb, prevPicOrderCntLsb;
    int picOrderCntMsb, picOrderCnt;

    if (current_frame_type == FRAME_IDR)
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
    else {
        prevPicOrderCntMsb = picOrderCntMsb_ref;
        prevPicOrderCntLsb = pic_order_cnt_lsb_ref;
    }

    if ((pic_order_cnt_lsb < prevPicOrderCntLsb) &&
        ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (int)(MaxPicOrderCntLsb / 2)))
        picOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (int)(MaxPicOrderCntLsb / 2)))
        picOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
        picOrderCntMsb = prevPicOrderCntMsb;

    picOrderCnt = picOrderCntMsb + pic_order_cnt_lsb;

    if (current_frame_type != FRAME_B) {
        picOrderCntMsb_ref = picOrderCntMsb;
        pic_order_cnt_lsb_ref = pic_order_cnt_lsb;
    }

    return picOrderCnt;
}

static void fill_profile_tier_level(
    uint8_t vps_max_layers_minus1,
    struct ProfileTierParamSet *ptps,
    uint8_t profilePresentFlag)
{
    if (!profilePresentFlag)
        return;

    memset(ptps, 0, sizeof(*ptps));

    ptps->general_profile_space = 0;
    ptps->general_tier_flag = 0;
    ptps->general_profile_idc = real_hevc_profile;
    memset(ptps->general_profile_compatibility_flag, 0, 32 * sizeof(int));
    ptps->general_profile_compatibility_flag[ptps->general_profile_idc] = 1;
    ptps->general_progressive_source_flag = 1;
    ptps->general_interlaced_source_flag = 0;
    ptps->general_non_packed_constraint_flag = 0;
    ptps->general_frame_only_constraint_flag = 1;

    ptps->general_level_idc = 30;
    ptps->general_level_idc = ptps->general_level_idc * 4;

}
static void fill_vps_header(struct VideoParamSet *vps)
{
    int i = 0;
    memset(vps, 0, sizeof(*vps));

    vps->vps_video_parameter_set_id = 0;
    vps->vps_base_layer_internal_flag = 1;
    vps->vps_base_layer_available_flag = 1;
    vps->vps_max_layers_minus1 = 0;
    vps->vps_max_sub_layers_minus1 = 0; // max temporal layer minus 1
    vps->vps_temporal_id_nesting_flag = 1;
    vps->vps_reserved_0xffff_16bits = 0xFFFF;
    // hevc::ProfileTierParamSet ptps;
    memset(&vps->ptps, 0, sizeof(vps->ptps));
    fill_profile_tier_level(vps->vps_max_layers_minus1, &protier_param, 1);
    vps->vps_sub_layer_ordering_info_present_flag = 0;
    for (i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
        vps->vps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
        vps->vps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
        vps->vps_max_latency_increase_plus1[i] = 0;
    }
    vps->vps_max_layer_id = 0;
    vps->vps_num_layer_sets_minus1 = 0;
    vps->vps_sub_layer_ordering_info_present_flag = 0;
    vps->vps_max_nuh_reserved_zero_layer_id = 0;
    vps->vps_max_op_sets = 1;
    vps->vps_timing_info_present_flag = 0;
    vps->vps_extension_flag = 0;
}

static void fill_short_term_ref_pic_header(
    struct ShortTermRefPicParamSet  *strp,
    uint8_t strp_index)
{
    uint32_t i = 0;
    // inter_ref_pic_set_prediction_flag is always 0 now
    strp->inter_ref_pic_set_prediction_flag = 0;
    /* don't need to set below parameters since inter_ref_pic_set_prediction_flag equal to 0
    strp->delta_idx_minus1 should be set to 0 since strp_index != num_short_term_ref_pic_sets in sps
    strp->delta_rps_sign;
    strp->abs_delta_rps_minus1;
    strp->used_by_curr_pic_flag[j];
    strp->use_delta_flag[j];
    */
    strp->num_negative_pics = num_active_ref_p;
    int num_positive_pics = ip_period > 1 ? 1 : 0;
    strp->num_positive_pics = strp_index == 0 ? 0 : num_positive_pics;

    if (strp_index == 0) {
        for (i = 0; i < strp->num_negative_pics; i++) {
            strp->delta_poc_s0_minus1[i] = ip_period - 1;
            strp->used_by_curr_pic_s0_flag[i] = 1;
        }
    } else {
        for (i = 0; i < strp->num_negative_pics; i++) {
            strp->delta_poc_s0_minus1[i] = (i == 0) ?
                                           (strp_index - 1) : (ip_period - 1);
            strp->used_by_curr_pic_s0_flag[i] = 1;
        }
        for (i = 0; i < strp->num_positive_pics; i++) {
            strp->delta_poc_s1_minus1[i] = ip_period - 1 - strp_index;
            strp->used_by_curr_pic_s1_flag[i] = 1;
        }

    }
}

void fill_sps_header(struct  SeqParamSet *sps, int id)
{
    int i = 0;
    memset(sps, 0, sizeof(struct  SeqParamSet));

    sps->sps_video_parameter_set_id = 0;
    sps->sps_max_sub_layers_minus1 = 0;
    sps->sps_temporal_id_nesting_flag = 1;
    fill_profile_tier_level(sps->sps_max_sub_layers_minus1, &sps->ptps, 1);
    sps->sps_seq_parameter_set_id = id;
    sps->chroma_format_idc = 1;
    if (sps->chroma_format_idc == 3) {
        sps->separate_colour_plane_flag = use_features ? features.separate_colour_planes : 0;
    }
    frame_width_aligned = ALIGN16(frame_width);
    frame_height_aligned = ALIGN16(frame_height);
    sps->pic_width_in_luma_samples = frame_width_aligned;
    sps->pic_height_in_luma_samples = frame_height_aligned;
    if (frame_width_aligned != frame_width ||
        frame_height_aligned != frame_height) {
        sps->conformance_window_flag = 1;
        sps->conf_win_left_offset = 0;
        sps->conf_win_top_offset = 0;
        switch (sps->chroma_format_idc) {
        case 0:
        case 3:  // 4:4:4 format
            sps->conf_win_right_offset = (frame_width_aligned - frame_width);
            sps->conf_win_bottom_offset = (frame_height_aligned - frame_height);
            break;

        case 2:  // 4:2:2 format
            sps->conf_win_right_offset = (frame_width_aligned - frame_width) >> 1;
            sps->conf_win_bottom_offset = (frame_height_aligned - frame_height);
            break;

        case 1:
        default: // 4:2:0 format
            sps->conf_win_right_offset = (frame_width_aligned - frame_width) >> 1;
            sps->conf_win_bottom_offset = (frame_height_aligned - frame_height) >> 1;
            break;
        }
    } else {
        sps->conformance_window_flag = 0;
    }

    sps->bit_depth_luma_minus8 = 0;
    sps->bit_depth_chroma_minus8 = 0;
    sps->log2_max_pic_order_cnt_lsb_minus4 = MAX((ceil(log(ip_period - 1 + 4) / log(2.0)) + 3), 4) - 4;
    sps->sps_sub_layer_ordering_info_present_flag = 0;
    for (i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
        sps->sps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
        sps->sps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
        sps->sps_max_latency_increase_plus1[i] = 0;
    }
    sps->log2_min_luma_coding_block_size_minus3 = use_block_sizes ? block_sizes.log2_min_luma_coding_block_size_minus3 : 0;
    int log2_max_luma_coding_block_size = use_block_sizes ? block_sizes.log2_max_coding_tree_block_size_minus3 + 3 : log2(LCU_SIZE);
    int log2_min_luma_coding_block_size = sps->log2_min_luma_coding_block_size_minus3 + 3;
    sps->log2_diff_max_min_luma_coding_block_size = log2_max_luma_coding_block_size -
            log2_min_luma_coding_block_size;
    sps->log2_min_luma_transform_block_size_minus2 = use_block_sizes ? block_sizes.log2_min_luma_transform_block_size_minus2 : 0;
    sps->log2_diff_max_min_luma_transform_block_size = use_block_sizes ? (block_sizes.log2_max_luma_transform_block_size_minus2 - 
                                                                          sps->log2_min_luma_transform_block_size_minus2) : 3;
    sps->max_transform_hierarchy_depth_inter = use_block_sizes ? block_sizes.max_max_transform_hierarchy_depth_inter : 2;
    sps->max_transform_hierarchy_depth_intra = use_block_sizes ? block_sizes.max_max_transform_hierarchy_depth_intra : 2;
    sps->scaling_list_enabled_flag = use_features ? features.scaling_lists : 0;
    sps->sps_scaling_list_data_present_flag = 0;
    sps->amp_enabled_flag = use_features ? features.amp : 1;
    sps->sample_adaptive_offset_enabled_flag = use_features ? features.sao : 1;
    sps->pcm_enabled_flag = use_features ? features.pcm : 0;
    /* ignore below parameters seting since pcm_enabled_flag equal to 0
    pcm_sample_bit_depth_luma_minus1;
    pcm_sample_bit_depth_chroma_minus1;
    log2_min_pcm_luma_coding_block_size_minus3;
    log2_diff_max_min_pcm_luma_coding_block_size;
    pcm_loop_filter_disabled_flag;
    */
    sps->num_short_term_ref_pic_sets = ip_period;

    memset(&sps->strp[0], 0, sizeof(sps->strp));
    for (i = 0; i < MIN(sps->num_short_term_ref_pic_sets, 64); i++)
        fill_short_term_ref_pic_header(&sps->strp[i], i);
    sps->long_term_ref_pics_present_flag = 0;
    /* ignore below parameters seting since long_term_ref_pics_present_flag equal to 0
    num_long_term_ref_pics_sps;
    lt_ref_pic_poc_lsb_sps[kMaxLongTermRefPic];
    used_by_curr_pic_lt_sps_flag[kMaxLongTermRefPic];
    */
    sps->sps_temporal_mvp_enabled_flag = use_features ? features.temporal_mvp : 1;
    sps->strong_intra_smoothing_enabled_flag = use_features ? features.strong_intra_smoothing : 0;
    
    sps->vui_parameters_present_flag = 0;
    sps->sps_extension_present_flag = 0;
    /* ignore below parameters seting since sps_extension_present_flag equal to 0
    sps->sps_range_extension_flag
    sps->sps_multilayer_extension_flag
    sps->sps_3d_extension_flag
    sps->sps_extension_5bits
    sps->sps_extension_data_flag
    */
}

static void fill_pps_header(
    struct PicParamSet *pps,
    uint32_t pps_id,
    uint32_t sps_id)
{
    memset(pps, 0, sizeof(struct PicParamSet));

    pps->pps_pic_parameter_set_id = pps_id;
    pps->pps_seq_parameter_set_id = sps_id;
    pps->dependent_slice_segments_enabled_flag = use_features ? features.dependent_slices : 0;
    pps->output_flag_present_flag = 0;
    pps->num_extra_slice_header_bits = 0;
    pps->sign_data_hiding_enabled_flag = use_features ? features.sign_data_hiding : 0;
    pps->cabac_init_present_flag = 1;

    pps->num_ref_idx_l0_default_active_minus1 = 0;
    pps->num_ref_idx_l1_default_active_minus1 = 0;

    pps->init_qp_minus26 = initial_qp - 26;
    pps->constrained_intra_pred_flag = use_features ? features.constrained_intra_pred : 0;
    pps->transform_skip_enabled_flag = use_features ? features.transform_skip : 0;
    pps->cu_qp_delta_enabled_flag = use_features ? features.cu_qp_delta : 1;
    if (pps->cu_qp_delta_enabled_flag)
        pps->diff_cu_qp_delta_depth = 2;
    pps->pps_cb_qp_offset = 0;
    pps->pps_cr_qp_offset = 0;
    pps->pps_slice_chroma_qp_offsets_present_flag = 0;
    pps->weighted_pred_flag = use_features ? features.weighted_prediction : 0;
    pps->weighted_bipred_flag = 0;
    pps->transquant_bypass_enabled_flag = use_features ? features.transquant_bypass : 0;
    pps->entropy_coding_sync_enabled_flag = 0;
    pps->tiles_enabled_flag = 0;

    pps->pps_loop_filter_across_slices_enabled_flag = 0;
    pps->deblocking_filter_control_present_flag = 1;
    pps->deblocking_filter_override_enabled_flag = 0,
         pps->pps_deblocking_filter_disabled_flag = use_features ? features.deblocking_filter_disable : 0,
              pps->pps_beta_offset_div2 = 2,
                   pps->pps_tc_offset_div2 = 0,
                        pps->pps_scaling_list_data_present_flag = 0;
    pps->lists_modification_present_flag = 0;
    pps->log2_parallel_merge_level_minus2 = 0;
    pps->slice_segment_header_extension_present_flag = 0;
    pps->pps_extension_present_flag = 0;
    pps->pps_range_extension_flag = 0;

}
static void fill_slice_header(
    uint32_t count,
    struct PicParamSet *pps,
    struct SliceHeader *slice)
{
    memset(slice, 0, sizeof(struct SliceHeader));
    slice->pic_output_flag = 1;
    slice->colour_plane_id = 0;
    slice->no_output_of_prior_pics_flag = 0;
    slice->pic_order_cnt_lsb = calc_poc((current_frame_display - current_IDR_display) % MaxPicOrderCntLsb);

    //slice_segment_address (u(v))
    int lcu_size = use_block_sizes ? (1 << (block_sizes.log2_max_coding_tree_block_size_minus3 + 3)) : LCU_SIZE;
    slice->picture_height_in_ctus = (frame_height + lcu_size - 1) / lcu_size;
    slice->picture_width_in_ctus = (frame_width + lcu_size - 1) / lcu_size;
    slice->slice_segment_address = 0;
    slice->first_slice_segment_in_pic_flag = ((slice->slice_segment_address == 0) ? 1 : 0);
    slice->slice_type = current_frame_type == FRAME_P ? (p2b ? SLICE_B : SLICE_P) :
                        current_frame_type == FRAME_B ? SLICE_B : SLICE_I;

    slice->dependent_slice_segment_flag = 0;
    slice->short_term_ref_pic_set_sps_flag = 1;
    slice->num_ref_idx_active_override_flag = 0;
    slice->short_term_ref_pic_set_idx = slice->pic_order_cnt_lsb % ip_period;
    slice->strp.num_negative_pics = numShortTerm;
    slice->strp.num_positive_pics = 0;
    slice->slice_sao_luma_flag = 0;
    slice->slice_sao_chroma_flag = 0;
    slice->slice_temporal_mvp_enabled_flag = use_features ? features.temporal_mvp : 1;

    slice->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
    slice->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;

    slice->num_poc_total_cur = 0;
    // for I slice
    if (current_frame_type == FRAME_I || current_frame_type == FRAME_IDR) {
        slice->ref_pic_list_modification_flag_l0 = 0;
        slice->list_entry_l0 = 0;
        slice->ref_pic_list_modification_flag_l1 = 0;
        slice->list_entry_l1 = 0;
    } else {
        slice->ref_pic_list_modification_flag_l0 = 1;
        slice->num_poc_total_cur = 2;
    }

    slice->ref_pic_list_combination_flag = 0;
    slice->num_ref_idx_lc_active_minus1 = 0;
    slice->ref_pic_list_modification_flag_lc = 0;
    slice->pic_from_list_0_flag = 0;
    slice->ref_idx_list_curr = 0;
    slice->mvd_l1_zero_flag = 0;
    slice->cabac_init_present_flag = 0;

    slice->slice_qp_delta = 0;
    slice->slice_qp_delta_cb = pps->pps_cb_qp_offset;
    slice->slice_qp_delta_cr = pps->pps_cr_qp_offset;

    slice->deblocking_filter_override_flag = 0;
    slice->disable_deblocking_filter_flag = 0;
    slice->tc_offset_div2 = pps->pps_tc_offset_div2;
    slice->beta_offset_div2 = pps->pps_beta_offset_div2;

    slice->collocated_from_l0_flag = 1;
    slice->collocated_ref_idx = pps->num_ref_idx_l0_default_active_minus1;

    slice->five_minus_max_num_merge_cand = 0;

    slice->slice_loop_filter_across_slices_enabled_flag = 0;
    slice->num_entry_point_offsets = 0;
    slice->offset_len_minus1 = 0;
}

static void protier_rbsp(bitstream *bs)
{
    uint32_t i = 0;
    put_ui(bs, protier_param.general_profile_space, 2);
    put_ui(bs, protier_param.general_tier_flag, 1);
    put_ui(bs, protier_param.general_profile_idc, 5);

    for (i = 0; i < 32; i++)
        put_ui(bs, protier_param.general_profile_compatibility_flag[i], 1);

    put_ui(bs, protier_param.general_progressive_source_flag, 1);
    put_ui(bs, protier_param.general_interlaced_source_flag, 1);
    put_ui(bs, protier_param.general_non_packed_constraint_flag, 1);
    put_ui(bs, protier_param.general_frame_only_constraint_flag, 1);
    put_ui(bs, 0, 16);
    put_ui(bs, 0, 16);
    put_ui(bs, 0, 12);
    put_ui(bs, protier_param.general_level_idc, 8);
}
void pack_short_term_ref_pic_setp(
    bitstream *bs,
    struct ShortTermRefPicParamSet* strp,
    int first_strp)
{
    uint32_t i = 0;
    if (!first_strp)
        put_ui(bs, strp->inter_ref_pic_set_prediction_flag, 1);

    // inter_ref_pic_set_prediction_flag is always 0 now
    put_ue(bs, strp->num_negative_pics);
    put_ue(bs, strp->num_positive_pics);

    for (i = 0; i < strp->num_negative_pics; i++) {
        put_ue(bs, strp->delta_poc_s0_minus1[i]);
        put_ui(bs, strp->used_by_curr_pic_s0_flag[i], 1);
    }
    for (i = 0; i < strp->num_positive_pics; i++) {
        put_ue(bs, strp->delta_poc_s1_minus1[i]);
        put_ui(bs, strp->used_by_curr_pic_s1_flag[i], 1);
    }
}
static void vps_rbsp(bitstream *bs)
{
    uint32_t i = 0;
    put_ui(bs, vps.vps_video_parameter_set_id, 4);
    put_ui(bs, 3, 2);  //vps_reserved_three_2bits
    put_ui(bs, 0, 6);  //vps_reserved_zero_6bits

    put_ui(bs, vps.vps_max_sub_layers_minus1, 3);
    put_ui(bs, vps.vps_temporal_id_nesting_flag, 1);
    put_ui(bs, 0xFFFF, 16); //vps_reserved_0xffff_16bits
    protier_rbsp(bs);

    put_ui(bs, vps.vps_sub_layer_ordering_info_present_flag, 1);

    for (i = (vps.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers_minus1); i <= vps.vps_max_sub_layers_minus1; i++) {
        // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
        // here just follow the spec 7.3.2.1
        put_ue(bs, vps.vps_max_dec_pic_buffering_minus1[i]);
        put_ue(bs, vps.vps_max_num_reorder_pics[i]);
        put_ue(bs, vps.vps_max_latency_increase_plus1[i]);
    }

    put_ui(bs, vps.vps_max_nuh_reserved_zero_layer_id, 6);
    put_ue(bs, vps.vps_num_op_sets_minus1);

    put_ui(bs, vps.vps_timing_info_present_flag, 1);

    if (vps.vps_timing_info_present_flag) {
        put_ue(bs, vps.vps_num_units_in_tick);
        put_ue(bs, vps.vps_time_scale);
        put_ue(bs, vps.vps_poc_proportional_to_timing_flag);
        if (vps.vps_poc_proportional_to_timing_flag) {
            put_ue(bs, vps.vps_num_ticks_poc_diff_one_minus1);
        }
        put_ue(bs, vps.vps_num_hrd_parameters);
        for (i = 0; i < vps.vps_num_hrd_parameters; i++) {
            put_ue(bs, vps.hrd_layer_set_idx[i]);
            if (i > 0) {
                put_ui(bs, vps.cprms_present_flag[i], 1);
            }
        }
    }

    // no extension flag
    put_ui(bs, 0, 1);
}

static void sps_rbsp(bitstream *bs)
{
    uint32_t  i = 0;
    put_ui(bs, sps.sps_video_parameter_set_id, 4);
    put_ui(bs, sps.sps_max_sub_layers_minus1, 3);
    put_ui(bs, sps.sps_temporal_id_nesting_flag, 1);

    protier_rbsp(bs);

    put_ue(bs, sps.sps_seq_parameter_set_id);
    put_ue(bs, sps.chroma_format_idc);

    if (sps.chroma_format_idc == 3) {
        put_ui(bs, sps.separate_colour_plane_flag, 1);

    }
    put_ue(bs, sps.pic_width_in_luma_samples);
    put_ue(bs, sps.pic_height_in_luma_samples);

    put_ui(bs, sps.conformance_window_flag, 1);

    if (sps.conformance_window_flag) {
        put_ue(bs, sps.conf_win_left_offset);
        put_ue(bs, sps.conf_win_right_offset);
        put_ue(bs, sps.conf_win_top_offset);
        put_ue(bs, sps.conf_win_bottom_offset);
    }
    put_ue(bs, sps.bit_depth_luma_minus8);
    put_ue(bs, sps.bit_depth_chroma_minus8);
    put_ue(bs, sps.log2_max_pic_order_cnt_lsb_minus4);
    put_ui(bs, sps.sps_sub_layer_ordering_info_present_flag, 1);

    for (i = (sps.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers_minus1); i <= sps.sps_max_sub_layers_minus1; i++) {
        // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
        // here just follow the spec 7.3.2.2
        put_ue(bs, sps.sps_max_dec_pic_buffering_minus1[i]);
        put_ue(bs, sps.sps_max_num_reorder_pics[i]);
        put_ue(bs, sps.sps_max_latency_increase_plus1[i]);
    }

    put_ue(bs, sps.log2_min_luma_coding_block_size_minus3);
    put_ue(bs, sps.log2_diff_max_min_luma_coding_block_size);
    put_ue(bs, sps.log2_min_luma_transform_block_size_minus2);
    put_ue(bs, sps.log2_diff_max_min_luma_transform_block_size);
    put_ue(bs, sps.max_transform_hierarchy_depth_inter);
    put_ue(bs, sps.max_transform_hierarchy_depth_intra);

    // scaling_list_enabled_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, sps.scaling_list_enabled_flag, 1);
    if (sps.scaling_list_enabled_flag) {
        put_ui(bs, sps.sps_scaling_list_data_present_flag, 1);
        if (sps.sps_scaling_list_data_present_flag) {
            //scaling_list_data();
        }
    }

    put_ui(bs, sps.amp_enabled_flag, 1);
    put_ui(bs, sps.sample_adaptive_offset_enabled_flag, 1);

    // pcm_enabled_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, sps.pcm_enabled_flag, 1);
    if (sps.pcm_enabled_flag) {
        put_ui(bs, sps.pcm_sample_bit_depth_luma_minus1, 4);
        put_ui(bs, sps.pcm_sample_bit_depth_chroma_minus1, 4);
        put_ue(bs, sps.log2_min_pcm_luma_coding_block_size_minus3);
        put_ue(bs, sps.log2_diff_max_min_pcm_luma_coding_block_size);
        put_ui(bs, sps.pcm_loop_filter_disabled_flag, 1);
    }

    put_ue(bs, sps.num_short_term_ref_pic_sets);
    for (i = 0; i < sps.num_short_term_ref_pic_sets; i++) {
        pack_short_term_ref_pic_setp(bs, &sps.strp[i], i == 0);
    }

    // long_term_ref_pics_present_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, sps.long_term_ref_pics_present_flag, 1);
    if (sps.long_term_ref_pics_present_flag) {
        put_ue(bs, sps.num_long_term_ref_pics_sps);
        for (i = 0; i < sps.num_long_term_ref_pics_sps; i++) {
            put_ue(bs, sps.lt_ref_pic_poc_lsb_sps[i]);
            put_ui(bs, sps.used_by_curr_pic_lt_sps_flag[i], 1);
        }
    }

    put_ui(bs, sps.sps_temporal_mvp_enabled_flag, 1);
    put_ui(bs, sps.strong_intra_smoothing_enabled_flag, 1);

    // vui_parameters_present_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, sps.vui_parameters_present_flag, 1);

    put_ui(bs, sps.sps_extension_present_flag, 1);
}

static void pps_rbsp(bitstream *bs)
{
    uint32_t  i = 0;
    put_ue(bs, pps.pps_pic_parameter_set_id);
    put_ue(bs, pps.pps_seq_parameter_set_id);
    put_ui(bs, pps.dependent_slice_segments_enabled_flag, 1);
    put_ui(bs, pps.output_flag_present_flag, 1);
    put_ui(bs, pps.num_extra_slice_header_bits, 3);
    put_ui(bs, pps.sign_data_hiding_enabled_flag, 1);
    put_ui(bs, pps.cabac_init_present_flag, 1);

    put_ue(bs, pps.num_ref_idx_l0_default_active_minus1);
    put_ue(bs, pps.num_ref_idx_l1_default_active_minus1);
    put_se(bs, pps.init_qp_minus26);

    put_ui(bs, pps.constrained_intra_pred_flag, 1);
    put_ui(bs, pps.transform_skip_enabled_flag, 1);

    put_ui(bs, pps.cu_qp_delta_enabled_flag, 1);
    if (pps.cu_qp_delta_enabled_flag) {
        put_ue(bs, pps.diff_cu_qp_delta_depth);
    }

    put_se(bs, pps.pps_cb_qp_offset);
    put_se(bs, pps.pps_cr_qp_offset);

    put_ui(bs, pps.pps_slice_chroma_qp_offsets_present_flag, 1);
    put_ui(bs, pps.weighted_pred_flag, 1);
    put_ui(bs, pps.weighted_bipred_flag, 1);
    put_ui(bs, pps.transquant_bypass_enabled_flag, 1);
    put_ui(bs, pps.tiles_enabled_flag, 1);
    put_ui(bs, pps.entropy_coding_sync_enabled_flag, 1);

    if (pps.tiles_enabled_flag) {
        put_ue(bs, pps.num_tile_columns_minus1);
        put_ue(bs, pps.num_tile_rows_minus1);
        put_ui(bs, pps.uniform_spacing_flag, 1);
        if (!pps.uniform_spacing_flag) {
            for (i = 0; i < pps.num_tile_columns_minus1; i++) {
                put_ue(bs, pps.column_width_minus1[i]);
            }

            for (i = 0; i < pps.num_tile_rows_minus1; i++) {
                put_ue(bs, pps.row_height_minus1[i]);
            }

        }
        put_ui(bs, pps.loop_filter_across_tiles_enabled_flag, 1);
    }

    put_ui(bs, pps.pps_loop_filter_across_slices_enabled_flag, 1);
    put_ui(bs, pps.deblocking_filter_control_present_flag, 1);
    if (pps.deblocking_filter_control_present_flag) {
        put_ui(bs, pps.deblocking_filter_override_enabled_flag, 1);
        put_ui(bs, pps.pps_deblocking_filter_disabled_flag, 1);
        if (!pps.pps_deblocking_filter_disabled_flag) {
            put_se(bs, pps.pps_beta_offset_div2);
            put_se(bs, pps.pps_tc_offset_div2);
        }
    }

    // pps_scaling_list_data_present_flag is set as 0 in fill_pps_header() for now
    put_ui(bs, pps.pps_scaling_list_data_present_flag, 1);
    if (pps.pps_scaling_list_data_present_flag) {
        //scaling_list_data();
    }

    put_ui(bs, pps.lists_modification_present_flag, 1);
    put_ue(bs, pps.log2_parallel_merge_level_minus2);
    put_ui(bs, pps.slice_segment_header_extension_present_flag, 1);

    put_ui(bs, pps.pps_extension_present_flag, 1);
    if (pps.pps_extension_present_flag) {
        put_ui(bs, pps.pps_range_extension_flag, 1);
        put_ui(bs, pps.pps_multilayer_extension_flag, 1);
        put_ui(bs, pps.pps_3d_extension_flag, 1);
        put_ui(bs, pps.pps_extension_5bits, 1);

    }

    if (pps.pps_range_extension_flag) {
        if (pps.transform_skip_enabled_flag)
            put_ue(bs, pps.log2_max_transform_skip_block_size_minus2);
        put_ui(bs, pps.cross_component_prediction_enabled_flag, 1);
        put_ui(bs, pps.chroma_qp_offset_list_enabled_flag, 1);

        if (pps.chroma_qp_offset_list_enabled_flag) {
            put_ue(bs, pps.diff_cu_chroma_qp_offset_depth);
            put_ue(bs, pps.chroma_qp_offset_list_len_minus1);
            for (i = 0; i <= pps.chroma_qp_offset_list_len_minus1; i++) {
                put_ue(bs, pps.cb_qp_offset_list[i]);
                put_ue(bs, pps.cr_qp_offset_list[i]);
            }
        }

        put_ue(bs, pps.log2_sao_offset_scale_luma);
        put_ue(bs, pps.log2_sao_offset_scale_chroma);
    }

}
static void sliceHeader_rbsp(
    bitstream *bs,
    struct SliceHeader *slice_header,
    struct SeqParamSet *sps,
    struct PicParamSet *pps,
    int isidr)
{
    uint8_t nal_unit_type = NALU_TRAIL_R;
    int gop_ref_distance = ip_period;
    int i = 0;

    put_ui(bs, slice_header->first_slice_segment_in_pic_flag, 1);
    if (slice_header->pic_order_cnt_lsb == 0)
        nal_unit_type = NALU_IDR_W_DLP;

    if (nal_unit_type >= 16 && nal_unit_type <= 23)
        put_ui(bs, slice_header->no_output_of_prior_pics_flag, 1);

    put_ue(bs, slice_header->slice_pic_parameter_set_id);

    if (!slice_header->first_slice_segment_in_pic_flag) {
        if (slice_header->dependent_slice_segment_flag) {
            put_ui(bs, slice_header->dependent_slice_segment_flag, 1);
        }

        put_ui(bs, slice_header->slice_segment_address,
               (uint8_t)(ceil(log(slice_header->picture_height_in_ctus * slice_header->picture_width_in_ctus) / log(2.0))));
    }
    if (!slice_header->dependent_slice_segment_flag) {
        for (i = 0; i < pps->num_extra_slice_header_bits; i++) {
            put_ui(bs, slice_header->slice_reserved_undetermined_flag[i], 1);
        }
        put_ue(bs, slice_header->slice_type);
        if (pps->output_flag_present_flag) {
            put_ui(bs, slice_header->pic_output_flag, 1);
        }
        if (sps->separate_colour_plane_flag == 1) {
            put_ui(bs, slice_header->colour_plane_id, 2);
        }

        if (!(nal_unit_type == NALU_IDR_W_DLP || nal_unit_type == NALU_IDR_N_LP)) {
            put_ui(bs, slice_header->pic_order_cnt_lsb, (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
            put_ui(bs, slice_header->short_term_ref_pic_set_sps_flag, 1);

            if (!slice_header->short_term_ref_pic_set_sps_flag) {
                // refer to Teddi
                if (sps->num_short_term_ref_pic_sets > 0)
                    put_ui(bs, 0, 1); // inter_ref_pic_set_prediction_flag, always 0 for now

                put_ue(bs, slice_header->strp.num_negative_pics);
                put_ue(bs, slice_header->strp.num_positive_pics);

                // below chunks of codes (majorly two big 'for' blocks) are refering both
                // Teddi and mv_encoder, they look kind of ugly, however, keep them as these
                // since it will be pretty easy to update if change/update in Teddi side.
                // According to Teddi, these are CModel Implementation.
                int prev = 0;
                int frame_cnt_in_gop = slice_header->pic_order_cnt_lsb / 2;
                // this is the first big 'for' block
                for (i = 0; i < slice_header->strp.num_negative_pics; i++) {
                    // Low Delay B case
                    if (1 == gop_ref_distance) {
                        put_ue(bs, 0 /*delta_poc_s0_minus1*/);
                    } else {
                        // For Non-BPyramid GOP i.e B0 type
                        if (num_active_ref_p > 1) {
                            // DeltaPOC Equals NumB
                            int DeltaPoc = -(int)(gop_ref_distance);
                            put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                        } else {
                            //  the big 'if' wraps here is -
                            //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                            // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                            // either for B-Prymid or first several frames in a GOP in multi-ref cases
                            // when there are not enough backward refs.
                            // So though there are really some codes under this 'else'in Teddi, don't
                            // want to introduce them in MEA to avoid confusion, and put an assert
                            // here to guard that there is new case we need handle in the future.
                            assert(0);
                        }
                    }
                    put_ui(bs, 1 /*used_by_curr_pic_s0_flag*/, 1);
                }

                prev = 0;
                // this is the second big 'for' block
                for (i = 0; i < slice_header->strp.num_positive_pics; i++) {
                    // Non-BPyramid GOP
                    if (num_active_ref_p > 1) {
                        // MultiRef Case
                        if (frame_cnt_in_gop < gop_ref_distance) {
                            int DeltaPoc = (int)(gop_ref_distance - frame_cnt_in_gop);
                            put_ue(bs, DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                        } else if (frame_cnt_in_gop > gop_ref_distance) {
                            int DeltaPoc = (int)(gop_ref_distance * slice_header->strp.num_negative_pics - frame_cnt_in_gop);
                            put_ue(bs, DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                        }
                    } else {
                        //  the big 'if' wraps here is -
                        //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                        // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                        // either for B-Prymid or first several frames in a GOP in multi-ref cases
                        // when there are not enough backward refs.
                        // So though there are really some codes under this 'else'in Teddi, don't
                        // want to introduce them in MEA to avoid confusion, and put an assert
                        // here to guard that there is new case we need handle in the future.
                        assert(0);
                    }
                    put_ui(bs, 1 /*used_by_curr_pic_s1_flag*/, 1);
                }
            } else if (sps->num_short_term_ref_pic_sets > 1)
                put_ui(bs, slice_header->short_term_ref_pic_set_idx,
                       (uint8_t)(ceil(log(sps->num_short_term_ref_pic_sets) / log(2.0))));

            if (sps->long_term_ref_pics_present_flag) {
                if (sps->num_long_term_ref_pics_sps > 0)
                    put_ue(bs, slice_header->num_long_term_sps);

                put_ue(bs, slice_header->num_long_term_pics);
            }

            if (sps->sps_temporal_mvp_enabled_flag)
                put_ui(bs, slice_header->slice_temporal_mvp_enabled_flag, 1);

        }

        if (sps->sample_adaptive_offset_enabled_flag) {
            put_ui(bs, slice_header->slice_sao_luma_flag, 1);
            put_ui(bs, slice_header->slice_sao_chroma_flag, 1);
        }

        if (slice_header->slice_type != SLICE_I) {
            put_ui(bs, slice_header->num_ref_idx_active_override_flag, 1);

            if (slice_header->num_ref_idx_active_override_flag) {
                put_ue(bs, slice_header->num_ref_idx_l0_active_minus1);
                if (slice_header->slice_type == SLICE_B)
                    put_ue(bs, slice_header->num_ref_idx_l1_active_minus1);
            }

            if (pps->lists_modification_present_flag &&  slice_header->num_poc_total_cur > 1) {
                /* ref_pic_list_modification */
                put_ui(bs, slice_header->ref_pic_list_modification_flag_l0, 1);

                if (slice_header->ref_pic_list_modification_flag_l0) {
                    for (i = 0; i <= slice_header->num_ref_idx_l0_active_minus1; i++) {
                        put_ui(bs, slice_header->list_entry_l0[i],
                               (uint8_t)(ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                    }
                }

                put_ui(bs, slice_header->ref_pic_list_modification_flag_l1, 1);

                if (slice_header->ref_pic_list_modification_flag_l1) {
                    for (i = 0; i <= slice_header->num_ref_idx_l1_active_minus1; i++) {
                        put_ui(bs, slice_header->list_entry_l1[i],
                               (uint8_t)(ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                    }
                }
            }

            if (slice_header->slice_type == SLICE_B) {
                put_ui(bs, slice_header->mvd_l1_zero_flag, 1);
            }

            if (pps->cabac_init_present_flag) {
                put_ui(bs, slice_header->cabac_init_present_flag, 1);
            }

            if (slice_header->slice_temporal_mvp_enabled_flag) {
                int collocated_from_l0_flag = 1;

                if (slice_header->slice_type == SLICE_B) {
                    collocated_from_l0_flag = slice_header->collocated_from_l0_flag;
                    put_ui(bs, slice_header->collocated_from_l0_flag, 1);
                }

                if (((collocated_from_l0_flag && (slice_header->num_ref_idx_l0_active_minus1 > 0)) ||
                     (!collocated_from_l0_flag && (slice_header->num_ref_idx_l1_active_minus1 > 0)))) {
                    put_ue(bs, slice_header->collocated_ref_idx);
                }
            }

            put_ue(bs, slice_header->five_minus_max_num_merge_cand);
        }

        put_se(bs, slice_header->slice_qp_delta);

        if (pps->chroma_qp_offset_list_enabled_flag) {
            put_se(bs, slice_header->slice_qp_delta_cb);
            put_se(bs, slice_header->slice_qp_delta_cr);
        }

        if (pps->deblocking_filter_override_enabled_flag) {
            put_ui(bs, slice_header->deblocking_filter_override_flag, 1);
        }
        if (slice_header->deblocking_filter_override_flag) {
            put_ui(bs, slice_header->disable_deblocking_filter_flag, 1);

            if (!slice_header->disable_deblocking_filter_flag) {
                put_se(bs, slice_header->beta_offset_div2);
                put_se(bs, slice_header->tc_offset_div2);
            }
        }

        if (pps->pps_loop_filter_across_slices_enabled_flag &&
            (slice_header->slice_sao_luma_flag || slice_header->slice_sao_chroma_flag ||
             !slice_header->disable_deblocking_filter_flag)) {
            put_ui(bs, slice_header->slice_loop_filter_across_slices_enabled_flag, 1);
        }

    }

    if ((pps->tiles_enabled_flag) || (pps->entropy_coding_sync_enabled_flag)) {
        put_ue(bs, slice_header->num_entry_point_offsets);

        if (slice_header->num_entry_point_offsets > 0) {
            put_ue(bs, slice_header->offset_len_minus1);
        }
    }

    if (pps->slice_segment_header_extension_present_flag) {
        int slice_header_extension_length = 0;

        put_ue(bs, slice_header_extension_length);
    }
}

static int
build_packed_pic_buffer(unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_PPS);
    nal_header(&bs, NALU_PPS);
    pps_rbsp(&bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}
static int
build_packed_video_buffer(unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_VPS);
    nal_header(&bs, NALU_VPS);
    vps_rbsp(&bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int
build_packed_seq_buffer(unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_SPS);
    nal_header(&bs, NALU_SPS);
    sps_rbsp(&bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int build_packed_slice_buffer(unsigned char **header_buffer)
{
    bitstream bs;
    int is_idr = !!pic_param.pic_fields.bits.idr_pic_flag;
    int naluType = is_idr ? NALU_IDR_W_DLP : NALU_TRAIL_R;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_TRAIL_R);
    nal_header(&bs, naluType);
    sliceHeader_rbsp(&bs, &ssh, &sps, &pps, 0);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}


/*
 * Helper function for profiling purposes
 */
static unsigned int GetTickCount()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

/*
  Assume frame sequence is: Frame#0,#1,#2,...,#M,...,#X,... (encoding order)
  1) period between Frame #X and Frame #N = #X - #N
  2) 0 means infinite for intra_period/intra_idr_period, and 0 is invalid for ip_period
  3) intra_idr_period % intra_period (intra_period > 0) and (intra_period -1)% ip_period must be 0
  4) intra_period and intra_idr_period take precedence over ip_period
  5) if ip_period > 1, intra_period and intra_idr_period are not  the strict periods
     of I/IDR frames, see bellow examples
  -------------------------------------------------------------------
  intra_period intra_idr_period ip_period frame sequence (intra_period/intra_idr_period/ip_period)
  0            ignored          1          IDRPPPPPPP ...     (No IDR/I any more)
  0            ignored        >=2          IDR(PBB)(PBB)...   (No IDR/I any more)
  1            0                ignored    IDRIIIIIII...      (No IDR any more)
  1            1                ignored    IDR IDR IDR IDR...
  1            >=2              ignored    IDRII IDRII IDR... (1/3/ignore)
  >=2          0                1          IDRPPP IPPP I...   (3/0/1)
  >=2          0              >=2          IDR(PBB)(PBB)(IBB) (7/0/3)
                                              (PBB)(IBB)(PBB)(IBB)...
  >=2          >=2              1          IDRPPPPP IPPPPP IPPPPP (7/14/1)
                                           IDRPPPPP IPPPPP IPPPPP...
  >=2          >=2              >=2        {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)} (7/14/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)(IBB)(PBB)}           (7/14/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)}                     (7/7/3)
                                           {IDR(PBB)(PBB)}.
*/

/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type
 */
void encoding2display_order(
    unsigned long long encoding_order, int intra_period,
    int intra_idr_period, int ip_period,
    unsigned long long *displaying_order,
    int *frame_type)
{
    int encoding_order_gop = 0;

    if (intra_period == 1) { /* all are I/IDR frames */
        *displaying_order = encoding_order;
        if (intra_idr_period == 0)
            *frame_type = (encoding_order == 0) ? FRAME_IDR : FRAME_I;
        else
            *frame_type = (encoding_order % intra_idr_period == 0) ? FRAME_IDR : FRAME_I;
        return;
    }

    if (intra_period == 0)
        intra_idr_period = 0;

    /* new sequence like
     * IDR PPPPP IPPPPP
     * IDR (PBB)(PBB)(IBB)(PBB)
     */
    encoding_order_gop = (intra_idr_period == 0) ? encoding_order :
                         (encoding_order % (intra_idr_period + ((ip_period == 1) ? 0 : 1)));

    if (encoding_order_gop == 0) { /* the first frame */
        *frame_type = FRAME_IDR;
        *displaying_order = encoding_order;
    } else if (((encoding_order_gop - 1) % ip_period) != 0) { /* B frames */
        *frame_type = FRAME_B;
        *displaying_order = encoding_order - 1;
    } else if ((intra_period != 0) && /* have I frames */
               (encoding_order_gop >= 2) &&
               ((ip_period == 1 && encoding_order_gop % (intra_period - 1) == 0) || /* for IDR PPPPP IPPPP */
                /* for IDR (PBB)(PBB)(IBB) */
                (ip_period >= 2 && ((encoding_order_gop - 1) / ip_period % ((intra_period - 1) / ip_period)) == 0))) {
        *frame_type = FRAME_I;
        *displaying_order = encoding_order + ip_period - 1;
    } else {
        *frame_type = FRAME_P;
        *displaying_order = encoding_order + ip_period - 1;
    }


}


static char *fourcc_to_string(int fourcc)
{
    switch (fourcc) {
    case VA_FOURCC_NV12:
        return "NV12";
    case VA_FOURCC_IYUV:
        return "IYUV";
    case VA_FOURCC_YV12:
        return "YV12";
    case VA_FOURCC_UYVY:
        return "UYVY";
    default:
        return "Unknown";
    }
}

static int string_to_fourcc(char *str)
{
    int fourcc;

    if (!strncmp(str, "NV12", 4))
        fourcc = VA_FOURCC_NV12;
    else if (!strncmp(str, "IYUV", 4))
        fourcc = VA_FOURCC_IYUV;
    else if (!strncmp(str, "YV12", 4))
        fourcc = VA_FOURCC_YV12;
    else if (!strncmp(str, "UYVY", 4))
        fourcc = VA_FOURCC_UYVY;
    else {
        printf("Unknow FOURCC\n");
        fourcc = -1;
    }
    return fourcc;
}


static char *rc_to_string(int rcmode)
{
    switch (rc_mode) {
    case VA_RC_NONE:
        return "NONE";
    case VA_RC_CBR:
        return "CBR";
    case VA_RC_VBR:
        return "VBR";
    case VA_RC_VCM:
        return "VCM";
    case VA_RC_CQP:
        return "CQP";
    case VA_RC_VBR_CONSTRAINED:
        return "VBR_CONSTRAINED";
    default:
        return "Unknown";
    }
}

static int string_to_rc(char *str)
{
    int rc_mode;

    if (!strncmp(str, "NONE", 4))
        rc_mode = VA_RC_NONE;
    else if (!strncmp(str, "CBR", 3))
        rc_mode = VA_RC_CBR;
    else if (!strncmp(str, "VBR", 3))
        rc_mode = VA_RC_VBR;
    else if (!strncmp(str, "VCM", 3))
        rc_mode = VA_RC_VCM;
    else if (!strncmp(str, "CQP", 3))
        rc_mode = VA_RC_CQP;
    else if (!strncmp(str, "VBR_CONSTRAINED", 15))
        rc_mode = VA_RC_VBR_CONSTRAINED;
    else {
        printf("Unknown RC mode\n");
        rc_mode = -1;
    }
    return rc_mode;
}


static int print_help(void)
{
    printf("./hevcencode <options>\n");
    printf("   -w <width> -h <height>\n");
    printf("   -framecount <frame number>\n");
    printf("   -n <frame number>\n");
    printf("      if set to 0 and srcyuv is set, the frame count is from srcuv file\n");
    printf("   -o <coded file>\n");
    printf("   -f <frame rate>\n");
    printf("   --intra_period <number>\n");
    printf("   --idr_period <number>\n");
    printf("   --ip_period <number>\n");
    printf("   --bitrate <bitrate> Kbits per second\n");
    printf("   --initialqp <number>\n");
    printf("   --minqp <number>\n");
    printf("   --rcmode <NONE|CBR|VBR|VCM|CQP|VBR_CONTRAINED>\n");
    printf("   --syncmode: sequentially upload source, encoding, save result, no multi-thread\n");
    printf("   --srcyuv <filename> load YUV from a file\n");
    printf("   --fourcc <NV12|IYUV|YV12> source YUV fourcc\n");
    printf("   --recyuv <filename> save reconstructed YUV into a file\n");
    printf("   --enablePSNR calculate PSNR of recyuv vs. srcyuv\n");
    printf("   --profile 1: main 2 : main10\n");
    printf("   --p2b 1: enable 0 : disalbe(defalut)\n");
    printf("   --lowpower 1: enable 0 : disalbe(defalut)\n");
    return 0;
}

static int process_cmdline(int argc, char *argv[])
{
    int c;
    const struct option long_opts[] = {
        {"help", no_argument, NULL, 0 },
        {"bitrate", required_argument, NULL, 1 },
        {"minqp", required_argument, NULL, 2 },
        {"initialqp", required_argument, NULL, 3 },
        {"intra_period", required_argument, NULL, 4 },
        {"idr_period", required_argument, NULL, 5 },
        {"ip_period", required_argument, NULL, 6 },
        {"rcmode", required_argument, NULL, 7 },
        {"srcyuv", required_argument, NULL, 9 },
        {"recyuv", required_argument, NULL, 10 },
        {"fourcc", required_argument, NULL, 11 },
        {"syncmode", no_argument, NULL, 12 },
        {"enablePSNR", no_argument, NULL, 13 },
        {"prit", required_argument, NULL, 14 },
        {"priv", required_argument, NULL, 15 },
        {"framecount", required_argument, NULL, 16 },
        {"profile", required_argument, NULL, 17 },
        {"p2b", required_argument, NULL, 18 },
        {"lowpower", required_argument, NULL, 19 },
        {NULL, no_argument, NULL, 0 }
    };
    int long_index;

    while ((c = getopt_long_only(argc, argv, "w:h:n:f:o:?", long_opts, &long_index)) != EOF) {
        switch (c) {
        case 'w':
            frame_width = atoi(optarg);
            break;
        case 'h':
            frame_height = atoi(optarg);
            break;
        case 'n':
        case 16:
            frame_count = atoi(optarg);
            break;
        case 'f':
            frame_rate = atoi(optarg);
            break;
        case 'o':
            if (coded_fn)
                free(coded_fn);
            coded_fn = strdup(optarg);
            break;
        case 0:
            print_help();
            exit(0);
        case 1:
            frame_bitrate = atoi(optarg)*1000;
            break;
        case 2:
            minimal_qp = atoi(optarg);
            break;
        case 3:
            initial_qp = atoi(optarg);
            break;
        case 4:
            intra_period = atoi(optarg);
            break;
        case 5:
            intra_idr_period = atoi(optarg);
            break;
        case 6:
            ip_period = atoi(optarg);
            break;
        case 7:
            rc_mode = string_to_rc(optarg);
            if (rc_mode < 0) {
                print_help();
                exit(1);
            }
            break;
        case 9:
            if (srcyuv_fn)
                free(srcyuv_fn);
            srcyuv_fn = strdup(optarg);
            break;
        case 10:
            if (recyuv_fn)
                free(recyuv_fn);
            recyuv_fn = strdup(optarg);
            break;
        case 11:
            srcyuv_fourcc = string_to_fourcc(optarg);
            if (srcyuv_fourcc <= 0) {
                print_help();
                exit(1);
            }
            break;
        case 12:
            encode_syncmode = 1;
            break;
        case 13:
            calc_psnr = 1;
            break;
        case 14:
            misc_priv_type = strtol(optarg, NULL, 0);
            break;
        case 15:
            misc_priv_value = strtol(optarg, NULL, 0);
            break;
        case 17:
            if (strncmp(optarg, "1", 1) == 0) {
                real_hevc_profile = 1;
                hevc_profile = VAProfileHEVCMain;
            } else if (strncmp(optarg, "2", 1) == 0) {
                real_hevc_profile = 2;
                hevc_profile = VAProfileHEVCMain10;
            } else
                hevc_profile = 0;
            break;
        case 18:
            p2b = atoi(optarg);
            break;
        case 19:
            lowpower = atoi(optarg);
            break;

        case ':':
        case '?':
            print_help();
            exit(0);
        }
    }

    if (ip_period < 1) {
        printf(" ip_period must be greater than 0\n");
        exit(0);
    }
    if (intra_period != 1 && (intra_period - 1) % ip_period != 0) {
        printf(" intra_period -1 must be a multiplier of ip_period\n");
        exit(0);
    }
    if (intra_period != 0 && intra_idr_period % intra_period != 0) {
        printf(" intra_idr_period must be a multiplier of intra_period\n");
        exit(0);
    }
    if (ip_period > 1) {
        frame_count -= (frame_count - 1) % ip_period;
    }

    if (frame_bitrate == 0)
        frame_bitrate = (long long int) frame_width * frame_height * 12 * frame_rate / 50;

    /* open source file */
    if (srcyuv_fn) {
        srcyuv_fp = fopen(srcyuv_fn, "r");

        if (srcyuv_fp == NULL)
            printf("Open source YUV file %s failed, use auto-generated YUV data\n", srcyuv_fn);
        else {
            struct stat tmp;

            int ret = fstat(fileno(srcyuv_fp), &tmp);
            CHECK_CONDITION(ret == 0);
            srcyuv_frames = tmp.st_size / (frame_width * frame_height * 1.5);
            printf("Source YUV file %s with %llu frames\n", srcyuv_fn, srcyuv_frames);

            if (frame_count == 0)
                frame_count = srcyuv_frames;
        }
    }

    /* open source file */
    if (recyuv_fn) {
        recyuv_fp = fopen(recyuv_fn, "w+");

        if (recyuv_fp == NULL)
            printf("Open reconstructed YUV file %s failed\n", recyuv_fn);
    }

    if (coded_fn == NULL) {
        struct stat buf;
        if (stat("/tmp", &buf) == 0)
            coded_fn = strdup("/tmp/test.265");
        else if (stat("/sdcard", &buf) == 0)
            coded_fn = strdup("/sdcard/test.265");
        else
            coded_fn = strdup("./test.265");
    }

    /* store coded data into a file */
    if (coded_fn) {
        coded_fp = fopen(coded_fn, "w+");
    } else {
        printf("Copy file string failed");
        exit(1);
    }
    if (coded_fp == NULL) {
        printf("Open file %s failed, exit\n", coded_fn);
        exit(1);
    }

    frame_width_aligned = (frame_width + 63) & (~63);
    frame_height_aligned = (frame_height + 63) & (~63);
    if (frame_width != frame_width_aligned ||
        frame_height != frame_height_aligned) {
        printf("Source frame is %dx%d and will code clip to %dx%d with crop\n",
               frame_width, frame_height,
               frame_width_aligned, frame_height_aligned
              );
    }

    return 0;
}

static int init_va(void)
{
    VAProfile profile_list[] = {VAProfileHEVCMain, VAProfileHEVCMain10};
    VAEntrypoint *entrypoints;
    int num_entrypoints, slice_entrypoint;
    int support_encode = 0;
    int major_ver, minor_ver;
    VAStatus va_status;
    unsigned int i;

    va_dpy = va_open_display();
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");

    num_entrypoints = vaMaxNumEntrypoints(va_dpy);
    entrypoints = malloc(num_entrypoints * sizeof(*entrypoints));
    if (!entrypoints) {
        fprintf(stderr, "error: failed to initialize VA entrypoints array\n");
        exit(1);
    }

    /* use the highest profile */
    for (i = 0; i < sizeof(profile_list) / sizeof(profile_list[0]); i++) {
        if ((hevc_profile != ~0) && hevc_profile != profile_list[i])
            continue;

        hevc_profile = profile_list[i];
        vaQueryConfigEntrypoints(va_dpy, hevc_profile, entrypoints, &num_entrypoints);
        for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
            if (entrypoints[slice_entrypoint] == VAEntrypointEncSlice ||
                entrypoints[slice_entrypoint] == VAEntrypointEncSliceLP ) {
                support_encode = 1;
                break;
            }
        }
        if (support_encode == 1)
            break;
    }

    if (support_encode == 0) {
        printf("Can't find VAEntrypointEncSlice for HEVC profiles\n");
        exit(1);
    } else {
        switch (hevc_profile) {
        case VAProfileHEVCMain:
            hevc_profile = VAProfileHEVCMain;
            printf("Use profile VAProfileHEVCMain\n");
            break;

        case VAProfileHEVCMain10:
            hevc_profile = VAProfileHEVCMain10;
            printf("Use profile VAProfileHEVCMain10\n");
            break;
        default:
            printf("unknow profile. Set to Main");
            hevc_profile = VAProfileHEVCMain;
            constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.1 & A.2.2 */
            ip_period = 1;
            break;
        }
    }

    /* find out the format for the render target, and rate control mode */
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attrib[i].type = i;

    if (lowpower)
    {
        entryPoint = VAEntrypointEncSliceLP;
        LCU_SIZE = 64;
    }

    va_status = vaGetConfigAttributes(va_dpy, hevc_profile, entryPoint,
                                      &attrib[0], VAConfigAttribTypeMax);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    /* check the interested configattrib */
    if ((attrib[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420) == 0) {
        printf("Not find desired YUV420 RT format\n");
        exit(1);
    } else {
        config_attrib[config_attrib_num].type = VAConfigAttribRTFormat;
        config_attrib[config_attrib_num].value = VA_RT_FORMAT_YUV420;
        config_attrib_num++;
    }

    if (attrib[VAConfigAttribRateControl].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribRateControl].value;

        printf("Support rate control mode (0x%x):", tmp);

        if (tmp & VA_RC_NONE)
            printf("NONE ");
        if (tmp & VA_RC_CBR)
            printf("CBR ");
        if (tmp & VA_RC_VBR)
            printf("VBR ");
        if (tmp & VA_RC_VCM)
            printf("VCM ");
        if (tmp & VA_RC_CQP)
            printf("CQP ");
        if (tmp & VA_RC_VBR_CONSTRAINED)
            printf("VBR_CONSTRAINED ");

        printf("\n");

        if (rc_mode == -1 || !(rc_mode & tmp))  {
            if (rc_mode != -1) {
                printf("Warning: Don't support the specified RateControl mode: %s!!!, switch to ", rc_to_string(rc_mode));
            }

            for (i = 0; i < sizeof(rc_default_modes) / sizeof(rc_default_modes[0]); i++) {
                if (rc_default_modes[i] & tmp) {
                    rc_mode = rc_default_modes[i];
                    break;
                }
            }

            printf("RateControl mode: %s\n", rc_to_string(rc_mode));
        }

        config_attrib[config_attrib_num].type = VAConfigAttribRateControl;
        config_attrib[config_attrib_num].value = rc_mode;
        config_attrib_num++;
    }


    if (attrib[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncPackedHeaders].value;

        printf("Support VAConfigAttribEncPackedHeaders\n");

        hevc_packedheader = 1;
        config_attrib[config_attrib_num].type = VAConfigAttribEncPackedHeaders;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;

        if (tmp & VA_ENC_PACKED_HEADER_SEQUENCE) {
            printf("Support packed sequence headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_PICTURE) {
            printf("Support packed picture headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_PICTURE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_SLICE) {
            printf("Support packed slice headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SLICE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_MISC) {
            printf("Support packed misc headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_MISC;
        }

        enc_packed_header_idx = config_attrib_num;
        config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncInterlaced].value;

        printf("Support VAConfigAttribEncInterlaced\n");

        if (tmp & VA_ENC_INTERLACED_FRAME)
            printf("support VA_ENC_INTERLACED_FRAME\n");
        if (tmp & VA_ENC_INTERLACED_FIELD)
            printf("Support VA_ENC_INTERLACED_FIELD\n");
        if (tmp & VA_ENC_INTERLACED_MBAFF)
            printf("Support VA_ENC_INTERLACED_MBAFF\n");
        if (tmp & VA_ENC_INTERLACED_PAFF)
            printf("Support VA_ENC_INTERLACED_PAFF\n");

        config_attrib[config_attrib_num].type = VAConfigAttribEncInterlaced;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncMaxRefFrames].value != VA_ATTRIB_NOT_SUPPORTED) {
        hevc_maxref = attrib[VAConfigAttribEncMaxRefFrames].value;

        printf("Support %d RefPicList0 and %d RefPicList1\n",
               hevc_maxref & 0xffff, (hevc_maxref >> 16) & 0xffff);
    }

    if (attrib[VAConfigAttribEncMaxSlices].value != VA_ATTRIB_NOT_SUPPORTED)
        printf("Support %d slices\n", attrib[VAConfigAttribEncMaxSlices].value);

    if (attrib[VAConfigAttribEncSliceStructure].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncSliceStructure].value;

        printf("Support VAConfigAttribEncSliceStructure\n");

        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS\n");
    }
    if (attrib[VAConfigAttribEncMacroblockInfo].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("Support VAConfigAttribEncMacroblockInfo\n");
    }
    if (attrib[VAConfigAttribEncHEVCBlockSizes].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("Support VAConfigAttribEncHEVCBlockSizes\n");
        uint32_t tmp = attrib[VAConfigAttribEncHEVCBlockSizes].value;
        VAConfigAttribValEncHEVCBlockSizes bs = { .value = tmp };
        block_sizes.log2_max_coding_tree_block_size_minus3 = bs.bits.log2_max_coding_tree_block_size_minus3;
        block_sizes.log2_min_coding_tree_block_size_minus3 = bs.bits.log2_min_coding_tree_block_size_minus3;
        block_sizes.log2_min_luma_coding_block_size_minus3 = bs.bits.log2_min_luma_coding_block_size_minus3;
        block_sizes.log2_max_luma_transform_block_size_minus2 = bs.bits.log2_max_luma_transform_block_size_minus2;
        block_sizes.log2_min_luma_transform_block_size_minus2 = bs.bits.log2_min_luma_transform_block_size_minus2;
        block_sizes.log2_max_pcm_coding_block_size_minus3 = bs.bits.log2_max_pcm_coding_block_size_minus3;
        block_sizes.log2_min_pcm_coding_block_size_minus3 = bs.bits.log2_min_pcm_coding_block_size_minus3;
        block_sizes.max_max_transform_hierarchy_depth_inter = bs.bits.max_max_transform_hierarchy_depth_inter;
        block_sizes.min_max_transform_hierarchy_depth_inter = bs.bits.min_max_transform_hierarchy_depth_inter;
        block_sizes.max_max_transform_hierarchy_depth_intra = bs.bits.max_max_transform_hierarchy_depth_intra;
        block_sizes.min_max_transform_hierarchy_depth_intra = bs.bits.min_max_transform_hierarchy_depth_intra;

        use_block_sizes = 1;
        config_attrib[config_attrib_num].type = VAConfigAttribEncHEVCBlockSizes;
        config_attrib[config_attrib_num].value = tmp;
        config_attrib_num++;
    }
    if (attrib[VAConfigAttribEncHEVCFeatures].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("Support VAConfigAttribEncHEVCFeatures\n");
        uint32_t tmp = attrib[VAConfigAttribEncHEVCFeatures].value;
        VAConfigAttribValEncHEVCFeatures f = { .value = tmp };
        features.amp = f.bits.amp;
        features.constrained_intra_pred = f.bits.constrained_intra_pred;
        features.cu_qp_delta = f.bits.cu_qp_delta;
        features.deblocking_filter_disable = f.bits.deblocking_filter_disable;
        features.dependent_slices = f.bits.dependent_slices;
        features.pcm = f.bits.pcm;
        features.sao = f.bits.sao;
        features.scaling_lists = f.bits.scaling_lists;
        features.separate_colour_planes = f.bits.separate_colour_planes;
        features.sign_data_hiding = f.bits.sign_data_hiding;
        features.strong_intra_smoothing = f.bits.strong_intra_smoothing;
        features.temporal_mvp = f.bits.temporal_mvp;
        features.transform_skip = f.bits.transform_skip;
        features.transquant_bypass = f.bits.transquant_bypass;
        features.weighted_prediction = f.bits.weighted_prediction;

        use_features = 1;
        config_attrib[config_attrib_num].type = VAConfigAttribEncHEVCFeatures;
        config_attrib[config_attrib_num].value = attrib[VAConfigAttribEncHEVCFeatures].value;
        config_attrib_num++;
    }

    free(entrypoints);
    return 0;
}

static int setup_encode()
{
    VAStatus va_status;
    VASurfaceID *tmp_surfaceid;
    int codedbuf_size, i;

    va_status = vaCreateConfig(va_dpy, hevc_profile, entryPoint,
                               &config_attrib[0], config_attrib_num, &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    /* create source surfaces */
    va_status = vaCreateSurfaces(va_dpy,
                                 VA_RT_FORMAT_YUV420, frame_width_aligned, frame_height_aligned,
                                 &src_surface[0], SURFACE_NUM,
                                 NULL, 0);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* create reference surfaces */
    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, frame_width_aligned, frame_height_aligned,
                    &ref_surface[0], SURFACE_NUM,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    tmp_surfaceid = calloc(2 * SURFACE_NUM, sizeof(VASurfaceID));
    if (tmp_surfaceid) {
        memcpy(tmp_surfaceid, src_surface, SURFACE_NUM * sizeof(VASurfaceID));
        memcpy(tmp_surfaceid + SURFACE_NUM, ref_surface, SURFACE_NUM * sizeof(VASurfaceID));
    }

    /* Create a context for this encode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                                frame_width_aligned, frame_height_aligned,
                                VA_PROGRESSIVE,
                                tmp_surfaceid, 2 * SURFACE_NUM,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
    free(tmp_surfaceid);

    codedbuf_size = ((long long int) frame_width_aligned * frame_height_aligned * 400) / (16 * 16);

    for (i = 0; i < SURFACE_NUM; i++) {
        /* create coded buffer once for all
         * other VA buffers which won't be used again after vaRenderPicture.
         * so APP can always vaCreateBuffer for every frame
         * but coded buffer need to be mapped and accessed after vaRenderPicture/vaEndPicture
         * so VA won't maintain the coded buffer
         */
        va_status = vaCreateBuffer(va_dpy, context_id, VAEncCodedBufferType,
                                   codedbuf_size, 1, NULL, &coded_buf[i]);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    }

    return 0;
}



#define partition(ref, field, key, ascending)   \
    while (i <= j) {                            \
        if (ascending) {                        \
            while (ref[i].field < key)          \
                i++;                            \
            while (ref[j].field > key)          \
                j--;                            \
        } else {                                \
            while (ref[i].field > key)          \
                i++;                            \
            while (ref[j].field < key)          \
                j--;                            \
        }                                       \
        if (i <= j) {                           \
            tmp = ref[i];                       \
            ref[i] = ref[j];                    \
            ref[j] = tmp;                       \
            i++;                                \
            j--;                                \
        }                                       \
    }                                           \

static void sort_one(VAPictureHEVC ref[], int left, int right,
                     int ascending)
{
    VAPictureHEVC tmp;
    int i = left, j = right;
    unsigned int key = ref[(left + right) / 2].pic_order_cnt;
    partition(ref, pic_order_cnt, (signed int)key, ascending);

    /* recursion */
    if (left < j)
        sort_one(ref, left, j, ascending);

    if (i < right)
        sort_one(ref, i, right, ascending);
}

static void sort_two(VAPictureHEVC ref[], int left, int right, unsigned int key,
                     int partition_ascending, int list0_ascending, int list1_ascending)
{
    VAPictureHEVC tmp;
    int i = left, j = right;

    partition(ref, pic_order_cnt, (signed int)key, partition_ascending);

    sort_one(ref, left, i - 1, list0_ascending);
    sort_one(ref, j + 1, right, list1_ascending);
}

static int update_ReferenceFrames(void)
{
    int i;

    if (current_frame_type == FRAME_B)
        return 0;

    numShortTerm++;
    if (numShortTerm > num_ref_frames)
        numShortTerm = num_ref_frames;
    for (i = numShortTerm - 1; i > 0; i--)
        ReferenceFrames[i] = ReferenceFrames[i - 1];
    ReferenceFrames[0] = CurrentCurrPic;

    return 0;
}

static int update_RefPicList(void)
{
    unsigned int current_poc = CurrentCurrPic.pic_order_cnt;

    if (current_frame_type == FRAME_P) {
        memcpy(RefPicList0_P, ReferenceFrames, numShortTerm * sizeof(VAPictureHEVC));
        sort_one(RefPicList0_P, 0, numShortTerm - 1, 0);
    }

    if (current_frame_type == FRAME_B) {
        memcpy(RefPicList0_B, ReferenceFrames, numShortTerm * sizeof(VAPictureHEVC));
        sort_two(RefPicList0_B, 0, numShortTerm - 1, current_poc, 1, 0, 1);

        memcpy(RefPicList1_B, ReferenceFrames, numShortTerm * sizeof(VAPictureHEVC));
        sort_two(RefPicList1_B, 0, numShortTerm - 1, current_poc, 0, 1, 0);
    }

    return 0;
}


static int render_sequence(struct SeqParamSet *sps)
{

    VABufferID seq_param_buf = VA_INVALID_ID;
    VABufferID rc_param_buf = VA_INVALID_ID;
    VABufferID misc_param_tmpbuf = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param, *misc_param_tmp;
    VAEncMiscParameterRateControl *misc_rate_ctrl;
    seq_param.general_profile_idc = sps->ptps.general_profile_idc;
    seq_param.general_level_idc = sps->ptps.general_level_idc;
    seq_param.general_tier_flag = (uint8_t)(sps->ptps.general_tier_flag);

    seq_param.intra_period = intra_period;
    seq_param.intra_idr_period = intra_idr_period;
    seq_param.ip_period = ip_period;

    seq_param.bits_per_second = frame_bitrate;
    seq_param.pic_width_in_luma_samples = sps->pic_width_in_luma_samples;
    seq_param.pic_height_in_luma_samples = sps->pic_height_in_luma_samples;

    seq_param.seq_fields.bits.chroma_format_idc = 1;
    seq_param.seq_fields.bits.separate_colour_plane_flag = 0;
    seq_param.seq_fields.bits.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
    seq_param.seq_fields.bits.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
    seq_param.seq_fields.bits.scaling_list_enabled_flag = sps->scaling_list_enabled_flag;
    seq_param.seq_fields.bits.strong_intra_smoothing_enabled_flag = sps->strong_intra_smoothing_enabled_flag;
    seq_param.seq_fields.bits.amp_enabled_flag = sps->amp_enabled_flag;
    seq_param.seq_fields.bits.sample_adaptive_offset_enabled_flag = sps->sample_adaptive_offset_enabled_flag;
    seq_param.seq_fields.bits.pcm_enabled_flag = sps->pcm_enabled_flag;
    seq_param.seq_fields.bits.pcm_loop_filter_disabled_flag = sps->pcm_loop_filter_disabled_flag;
    seq_param.seq_fields.bits.sps_temporal_mvp_enabled_flag = sps->sps_temporal_mvp_enabled_flag;

    seq_param.log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3;
    seq_param.log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size;
    seq_param.log2_min_transform_block_size_minus2 = sps->log2_min_luma_transform_block_size_minus2;
    seq_param.log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_luma_transform_block_size;
    seq_param.max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter;
    seq_param.max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra;

    seq_param.vui_parameters_present_flag = sps->vui_parameters_present_flag;

    va_status = vaCreateBuffer(va_dpy, context_id,
                               VAEncSequenceParameterBufferType,
                               sizeof(seq_param), 1, &seq_param, &seq_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy, context_id,
                               VAEncMiscParameterBufferType,
                               sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                               1, NULL, &rc_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy, rc_param_buf, (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeRateControl;
    misc_rate_ctrl = (VAEncMiscParameterRateControl *)misc_param->data;
    memset(misc_rate_ctrl, 0, sizeof(*misc_rate_ctrl));
    misc_rate_ctrl->bits_per_second = frame_bitrate;
    misc_rate_ctrl->target_percentage = 66;
    misc_rate_ctrl->window_size = 1000;
    misc_rate_ctrl->initial_qp = initial_qp;
    misc_rate_ctrl->min_qp = minimal_qp;
    misc_rate_ctrl->basic_unit_size = 0;
    vaUnmapBuffer(va_dpy, rc_param_buf);

    render_id[0] = seq_param_buf;
    render_id[1] = rc_param_buf;

    va_status = vaRenderPicture(va_dpy, context_id, &render_id[0], 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");
    if (seq_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, seq_param_buf);
        seq_param_buf = VA_INVALID_ID;
    }

    if (rc_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, rc_param_buf);
        rc_param_buf = VA_INVALID_ID;
    }


    if (misc_priv_type != 0) {
        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VAEncMiscParameterBufferType,
                                   sizeof(VAEncMiscParameterBuffer),
                                   1, NULL, &misc_param_tmpbuf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
        vaMapBuffer(va_dpy, misc_param_tmpbuf, (void **)&misc_param_tmp);
        misc_param_tmp->type = misc_priv_type;
        misc_param_tmp->data[0] = misc_priv_value;
        vaUnmapBuffer(va_dpy, misc_param_tmpbuf);

        va_status = vaRenderPicture(va_dpy, context_id, &misc_param_tmpbuf, 1);
    }

    return 0;
}

static int render_picture(struct PicParamSet *pps)
{
    VABufferID pic_param_buf = VA_INVALID_ID;
    VAStatus va_status;
    int i = 0;

    memcpy(pic_param.reference_frames, ReferenceFrames, numShortTerm * sizeof(VAPictureHEVC));
    for (i = numShortTerm; i < SURFACE_NUM - 1; i++) {
        pic_param.reference_frames[i].picture_id = VA_INVALID_SURFACE;
        pic_param.reference_frames[i].flags = VA_PICTURE_HEVC_INVALID;
    }

    pic_param.last_picture = 0;
    pic_param.last_picture |= ((current_frame_encoding + 1) % intra_period == 0) ? HEVC_LAST_PICTURE_EOSEQ : 0;
    pic_param.last_picture |= ((current_frame_encoding + 1) == frame_count) ? HEVC_LAST_PICTURE_EOSTREAM : 0;
    pic_param.coded_buf = coded_buf[current_slot];

    pic_param.decoded_curr_pic.picture_id = ref_surface[current_slot];
    pic_param.decoded_curr_pic.pic_order_cnt = calc_poc((current_frame_display - current_IDR_display) % MaxPicOrderCntLsb) * 2;
    pic_param.decoded_curr_pic.flags = 0;
    CurrentCurrPic = pic_param.decoded_curr_pic;

    pic_param.collocated_ref_pic_index = pps->num_ref_idx_l0_default_active_minus1;
    pic_param.pic_init_qp = pps->init_qp_minus26 + 26;
    pic_param.diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth;
    pic_param.pps_cb_qp_offset = pps->pps_cb_qp_offset;
    pic_param.pps_cr_qp_offset = pps->pps_cr_qp_offset;

    pic_param.num_tile_columns_minus1 = pps->num_tile_columns_minus1;
    pic_param.num_tile_rows_minus1 = pps->num_tile_rows_minus1;
    for (i = 0; i <= (unsigned int)(pic_param.num_tile_columns_minus1); i++) {
        pic_param.column_width_minus1[i] = 0;
    }
    for (i = 0; i <= (unsigned int)(pic_param.num_tile_rows_minus1); i++) {
        pic_param.row_height_minus1[i] = 0;
    }

    pic_param.log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2;
    pic_param.ctu_max_bitsize_allowed = 0;
    pic_param.num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
    pic_param.num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
    pic_param.slice_pic_parameter_set_id = 0;
    pic_param.pic_fields.bits.idr_pic_flag         = (current_frame_type == FRAME_IDR);
    pic_param.pic_fields.bits.coding_type          = current_frame_type == FRAME_IDR ? FRAME_I : current_frame_type;
    pic_param.pic_fields.bits.reference_pic_flag   = current_frame_type != FRAME_B ? 1 : 0;
    pic_param.pic_fields.bits.dependent_slice_segments_enabled_flag = pps->dependent_slice_segments_enabled_flag;
    pic_param.pic_fields.bits.sign_data_hiding_enabled_flag = pps->sign_data_hiding_enabled_flag;
    pic_param.pic_fields.bits.constrained_intra_pred_flag = pps->constrained_intra_pred_flag;
    pic_param.pic_fields.bits.transform_skip_enabled_flag = pps->transform_skip_enabled_flag;
    pic_param.pic_fields.bits.cu_qp_delta_enabled_flag = pps->cu_qp_delta_enabled_flag;
    pic_param.pic_fields.bits.weighted_pred_flag = pps->weighted_pred_flag;
    pic_param.pic_fields.bits.weighted_bipred_flag = pps->weighted_bipred_flag;
    pic_param.pic_fields.bits.transquant_bypass_enabled_flag = pps->transquant_bypass_enabled_flag;
    pic_param.pic_fields.bits.tiles_enabled_flag = pps->tiles_enabled_flag;
    pic_param.pic_fields.bits.entropy_coding_sync_enabled_flag = pps->entropy_coding_sync_enabled_flag;
    pic_param.pic_fields.bits.loop_filter_across_tiles_enabled_flag = pps->loop_filter_across_tiles_enabled_flag;
    pic_param.pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = pps->pps_loop_filter_across_slices_enabled_flag;
    pic_param.pic_fields.bits.scaling_list_data_present_flag = pps->pps_scaling_list_data_present_flag;

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncPictureParameterBufferType,
                               sizeof(pic_param), 1, &pic_param, &pic_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &pic_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (pic_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, pic_param_buf);
        pic_param_buf = VA_INVALID_ID;
    }

    return 0;
}

static int render_packedvideo(void)
{

    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedvideo_para_bufid = VA_INVALID_ID;
    VABufferID packedvideo_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedvideo_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_video_buffer(&packedvideo_buffer);

    packedheader_param_buffer.type = VAEncPackedHeaderSequence;

    packedheader_param_buffer.bit_length = length_in_bits; /*length_in_bits*/
    packedheader_param_buffer.has_emulation_bytes = 0;
    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedvideo_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedvideo_buffer,
                               &packedvideo_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedvideo_para_bufid;
    render_id[1] = packedvideo_data_bufid;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedvideo_buffer);

    if (packedvideo_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedvideo_para_bufid);
        packedvideo_para_bufid = VA_INVALID_ID;
    }
    if (packedvideo_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedvideo_data_bufid);
        packedvideo_data_bufid = VA_INVALID_ID;
    }

    return 0;
}

static int render_packedsequence(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedseq_para_bufid = VA_INVALID_ID;
    VABufferID packedseq_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedseq_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_seq_buffer(&packedseq_buffer);

    packedheader_param_buffer.type = VAEncPackedHeaderSequence;

    packedheader_param_buffer.bit_length = length_in_bits; /*length_in_bits*/
    packedheader_param_buffer.has_emulation_bytes = 0;
    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedseq_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedseq_buffer,
                               &packedseq_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedseq_para_bufid;
    render_id[1] = packedseq_data_bufid;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedseq_buffer);

    if (packedseq_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedseq_para_bufid);
        packedseq_para_bufid = VA_INVALID_ID;
    }
    if (packedseq_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedseq_data_bufid);
        packedseq_para_bufid = VA_INVALID_ID;
    }

    return 0;
}


static int render_packedpicture(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedpic_para_bufid = VA_INVALID_ID;
    VABufferID packedpic_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedpic_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_pic_buffer(&packedpic_buffer);
    packedheader_param_buffer.type = VAEncPackedHeaderPicture;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedpic_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedpic_buffer,
                               &packedpic_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedpic_para_bufid;
    render_id[1] = packedpic_data_bufid;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedpic_buffer);

    if (packedpic_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedpic_para_bufid);
        packedpic_para_bufid = VA_INVALID_ID;
    }
    if (packedpic_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedpic_data_bufid);
        packedpic_para_bufid = VA_INVALID_ID;
    }

    return 0;
}

static void render_packedslice()
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedslice_para_bufid =  VA_INVALID_ID;
    VABufferID packedslice_data_bufid =  VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedslice_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_slice_buffer(&packedslice_buffer);
    packedheader_param_buffer.type = VAEncPackedHeaderSlice;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedslice_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedslice_buffer,
                               &packedslice_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedslice_para_bufid;
    render_id[1] = packedslice_data_bufid;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedslice_buffer);

    if (packedslice_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedslice_para_bufid);
        packedslice_para_bufid = VA_INVALID_ID;
    }
    if (packedslice_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packedslice_data_bufid);
        packedslice_para_bufid = VA_INVALID_ID;
    }
}

static int render_slice(void)
{
    VABufferID slice_param_buf = VA_INVALID_ID;
    VAStatus va_status;
    memset(&slice_param, 0x00, sizeof(VAEncSliceParameterBufferHEVC));

    update_RefPicList();

    slice_param.slice_segment_address = 0;
    slice_param.num_ctu_in_slice = ssh.picture_width_in_ctus * ssh.picture_height_in_ctus;
    slice_param.slice_type = ssh.slice_type;
    slice_param.slice_pic_parameter_set_id = ssh.slice_pic_parameter_set_id; // right???

    slice_param.num_ref_idx_l0_active_minus1 = ssh.num_ref_idx_l0_active_minus1;
    slice_param.num_ref_idx_l1_active_minus1 = ssh.num_ref_idx_l1_active_minus1;
    memset(slice_param.ref_pic_list0, 0xff, sizeof(slice_param.ref_pic_list0));
    memset(slice_param.ref_pic_list1, 0xff, sizeof(slice_param.ref_pic_list1));

    if (current_frame_type == FRAME_P) {
        memcpy(slice_param.ref_pic_list0, RefPicList0_P, sizeof(VAPictureHEVC));
        if (p2b) {
            memcpy(slice_param.ref_pic_list1, RefPicList0_P, sizeof(VAPictureHEVC));
        }
    } else if (current_frame_type == FRAME_B) {
        memcpy(slice_param.ref_pic_list0, RefPicList0_B, sizeof(VAPictureHEVC));
        memcpy(slice_param.ref_pic_list1, RefPicList1_B, sizeof(VAPictureHEVC));
    }

    slice_param.luma_log2_weight_denom = 0;
    slice_param.delta_chroma_log2_weight_denom = 0;

    slice_param.max_num_merge_cand = 5 - ssh.five_minus_max_num_merge_cand;

    slice_param.slice_qp_delta = ssh.slice_qp_delta;
    slice_param.slice_cb_qp_offset = 0;
    slice_param.slice_cr_qp_offset = 0;
    slice_param.slice_beta_offset_div2 = ssh.beta_offset_div2;
    slice_param.slice_tc_offset_div2 = ssh.tc_offset_div2;

    slice_param.slice_fields.bits.dependent_slice_segment_flag = 0;
    slice_param.slice_fields.bits.colour_plane_id = ssh.colour_plane_id;
    slice_param.slice_fields.bits.slice_temporal_mvp_enabled_flag = ssh.slice_temporal_mvp_enabled_flag;
    slice_param.slice_fields.bits.slice_sao_luma_flag = ssh.slice_sao_luma_flag;
    slice_param.slice_fields.bits.slice_sao_chroma_flag = ssh.slice_sao_luma_flag;
    slice_param.slice_fields.bits.num_ref_idx_active_override_flag = ssh.num_ref_idx_active_override_flag;
    slice_param.slice_fields.bits.mvd_l1_zero_flag = 0;
    slice_param.slice_fields.bits.cabac_init_flag = 0;
    slice_param.slice_fields.bits.slice_deblocking_filter_disabled_flag = ssh.disable_deblocking_filter_flag;
    slice_param.slice_fields.bits.slice_loop_filter_across_slices_enabled_flag = ssh.slice_loop_filter_across_slices_enabled_flag;
    slice_param.slice_fields.bits.collocated_from_l0_flag = ssh.collocated_from_l0_flag;

    if (hevc_packedheader &&
        config_attrib[enc_packed_header_idx].value & VA_ENC_PACKED_HEADER_SLICE)
        render_packedslice();

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncSliceParameterBufferType,
                               sizeof(slice_param), 1, &slice_param, &slice_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &slice_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (slice_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, slice_param_buf);
        slice_param_buf = VA_INVALID_ID;
    }

    return 0;
}


static int upload_source_YUV_once_for_all()
{
    int box_width = 8;
    int row_shift = 0;
    int i;

    for (i = 0; i < SURFACE_NUM; i++) {
        printf("\rLoading data into surface %d.....", i);
        upload_surface(va_dpy, src_surface[i], box_width, row_shift, 0);

        row_shift++;
        if (row_shift == (2 * box_width)) row_shift = 0;
    }
    printf("Complete surface loading\n");

    return 0;
}

static int load_surface(VASurfaceID surface_id, unsigned long long display_order)
{
    unsigned char *srcyuv_ptr = NULL, *src_Y = NULL, *src_U = NULL, *src_V = NULL;
    unsigned long long frame_start, mmap_start;
    char *mmap_ptr = NULL;
    int frame_size, mmap_size;

    if (srcyuv_fp == NULL)
        return 0;

    /* allow encoding more than srcyuv_frames */
    display_order = display_order % srcyuv_frames;
    frame_size = frame_width * frame_height * 3 / 2; /* for YUV420 */
    frame_start = display_order * frame_size;

    mmap_start = frame_start & (~0xfff);
    mmap_size = (frame_size + (frame_start & 0xfff) + 0xfff) & (~0xfff);
    mmap_ptr = mmap(0, mmap_size, PROT_READ, MAP_SHARED,
                    fileno(srcyuv_fp), mmap_start);
    if (mmap_ptr == MAP_FAILED) {
        printf("Failed to mmap YUV file (%s)\n", strerror(errno));
        return 1;
    }
    srcyuv_ptr = (unsigned char *)mmap_ptr + (frame_start & 0xfff);
    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        src_Y = srcyuv_ptr;
        src_U = src_Y + frame_width * frame_height;
        src_V = NULL;
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        src_Y = srcyuv_ptr;
        if (srcyuv_fourcc == VA_FOURCC_IYUV) {
            src_U = src_Y + frame_width * frame_height;
            src_V = src_U + (frame_width / 2) * (frame_height / 2);
        } else { /* YV12 */
            src_V = src_Y + frame_width * frame_height;
            src_U = src_V + (frame_width / 2) * (frame_height / 2);
        }
    } else {
        printf("Unsupported source YUV format\n");
        exit(1);
    }

    upload_surface_yuv(va_dpy, surface_id,
                       srcyuv_fourcc, frame_width, frame_height,
                       src_Y, src_U, src_V);
    if (mmap_ptr)
        munmap(mmap_ptr, mmap_size);

    return 0;
}


static int save_recyuv(VASurfaceID surface_id,
                       unsigned long long display_order,
                       unsigned long long encode_order)
{
    unsigned char *dst_Y = NULL, *dst_U = NULL, *dst_V = NULL;

    if (recyuv_fp == NULL)
        return 0;

    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        int uv_size = 2 * (frame_width / 2) * (frame_height / 2);
        dst_Y = malloc(2 * uv_size);
        if (dst_Y == NULL) {
            printf("Failed to allocate memory for dst_Y\n");
            exit(1);
        }

        dst_U = malloc(uv_size);
        if (dst_U == NULL) {
            printf("Failed to allocate memory for dst_U\n");
            free(dst_Y);
            exit(1);
        }

        memset(dst_Y, 0, 2 * uv_size);
        memset(dst_U, 0, uv_size);
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        int uv_size = (frame_width / 2) * (frame_height / 2);
        dst_Y = malloc(4 * uv_size);
        if (dst_Y == NULL) {
            printf("Failed to allocate memory for dst_Y\n");
            exit(1);
        }

        dst_U = malloc(uv_size);
        if (dst_U == NULL) {
            printf("Failed to allocate memory for dst_U\n");
            free(dst_Y);
            exit(1);
        }

        dst_V = malloc(uv_size);
        if (dst_V == NULL) {
            printf("Failed to allocate memory for dst_V\n");
            free(dst_Y);
            free(dst_U);
            exit(1);
        }

        memset(dst_Y, 0, 4 * uv_size);
        memset(dst_U, 0, uv_size);
        memset(dst_V, 0, uv_size);
    } else {
        printf("Unsupported source YUV format\n");
        exit(1);
    }

    download_surface_yuv(va_dpy, surface_id,
                         srcyuv_fourcc, frame_width, frame_height,
                         dst_Y, dst_U, dst_V);
    fseek(recyuv_fp, display_order * frame_width * frame_height * 1.5, SEEK_SET);

    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        int uv_size = 2 * (frame_width / 2) * (frame_height / 2);
        fwrite(dst_Y, uv_size * 2, 1, recyuv_fp);
        fwrite(dst_U, uv_size, 1, recyuv_fp);
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        int uv_size = (frame_width / 2) * (frame_height / 2);
        fwrite(dst_Y, uv_size * 4, 1, recyuv_fp);

        if (srcyuv_fourcc == VA_FOURCC_IYUV) {
            fwrite(dst_U, uv_size, 1, recyuv_fp);
            fwrite(dst_V, uv_size, 1, recyuv_fp);
        } else {
            fwrite(dst_V, uv_size, 1, recyuv_fp);
            fwrite(dst_U, uv_size, 1, recyuv_fp);
        }
    }

    if (dst_Y)
        free(dst_Y);
    if (dst_U)
        free(dst_U);
    if (dst_V)
        free(dst_V);

    fflush(recyuv_fp);

    return 0;
}


static int save_codeddata(unsigned long long display_order, unsigned long long encode_order)
{
    VACodedBufferSegment *buf_list = NULL;
    VAStatus va_status;
    unsigned int coded_size = 0;

    va_status = vaMapBuffer(va_dpy, coded_buf[display_order % SURFACE_NUM], (void **)(&buf_list));
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    while (buf_list != NULL) {
        coded_size += fwrite(buf_list->buf, 1, buf_list->size, coded_fp);
        buf_list = (VACodedBufferSegment *) buf_list->next;

        frame_size += coded_size;
    }
    vaUnmapBuffer(va_dpy, coded_buf[display_order % SURFACE_NUM]);

    printf("\n      "); /* return back to startpoint */
    switch (encode_order % 4) {
    case 0:
        printf("|");
        break;
    case 1:
        printf("/");
        break;
    case 2:
        printf("-");
        break;
    case 3:
        printf("\\");
        break;
    }
    printf("%08lld", encode_order);
    printf("(%06d bytes coded)\n", coded_size);

    fflush(coded_fp);

    return 0;
}


static struct storage_task_t * storage_task_dequeue(void)
{
    struct storage_task_t *header;

    pthread_mutex_lock(&encode_mutex);

    header = storage_task_header;
    if (storage_task_header != NULL) {
        if (storage_task_tail == storage_task_header)
            storage_task_tail = NULL;
        storage_task_header = header->next;
    }

    pthread_mutex_unlock(&encode_mutex);

    return header;
}

static int storage_task_queue(unsigned long long display_order, unsigned long long encode_order)
{
    struct storage_task_t *tmp;

    tmp = calloc(1, sizeof(struct storage_task_t));
    if (tmp) {
        tmp->display_order = display_order;
        tmp->encode_order = encode_order;
    }

    pthread_mutex_lock(&encode_mutex);

    if (storage_task_header == NULL) {
        storage_task_header = tmp;
        storage_task_tail = tmp;
    } else {
        storage_task_tail->next = tmp;
        storage_task_tail = tmp;
    }

    srcsurface_status[display_order % SURFACE_NUM] = SRC_SURFACE_IN_STORAGE;
    pthread_cond_signal(&encode_cond);

    pthread_mutex_unlock(&encode_mutex);

    return 0;
}

static void storage_task(unsigned long long display_order, unsigned long long encode_order)
{
    unsigned int tmp;
    VAStatus va_status;

    tmp = GetTickCount();
    va_status = vaSyncSurface(va_dpy, src_surface[display_order % SURFACE_NUM]);
    CHECK_VASTATUS(va_status, "vaSyncSurface");
    SyncPictureTicks += GetTickCount() - tmp;
    tmp = GetTickCount();
    save_codeddata(display_order, encode_order);
    SavePictureTicks += GetTickCount() - tmp;

    save_recyuv(ref_surface[display_order % SURFACE_NUM], display_order, encode_order);

    /* reload a new frame data */
    tmp = GetTickCount();
    if (srcyuv_fp != NULL)
        load_surface(src_surface[display_order % SURFACE_NUM], display_order + SURFACE_NUM);
    UploadPictureTicks += GetTickCount() - tmp;

    pthread_mutex_lock(&encode_mutex);
    srcsurface_status[display_order % SURFACE_NUM] = SRC_SURFACE_IN_ENCODING;
    pthread_mutex_unlock(&encode_mutex);
}


static void * storage_task_thread(void *t)
{
    while (1) {
        struct storage_task_t *current;

        current = storage_task_dequeue();
        if (current == NULL) {
            pthread_mutex_lock(&encode_mutex);
            pthread_cond_wait(&encode_cond, &encode_mutex);
            pthread_mutex_unlock(&encode_mutex);
            continue;
        }

        storage_task(current->display_order, current->encode_order);

        free(current);

        /* all frames are saved, exit the thread */
        if (++frame_coded >= frame_count)
            break;
    }

    return 0;
}


static int encode_frames(void)
{
    unsigned int i, tmp;
    VAStatus va_status;
    //VASurfaceStatus surface_status;

    /* upload RAW YUV data into all surfaces */
    tmp = GetTickCount();
    if (srcyuv_fp != NULL) {
        for (i = 0; i < SURFACE_NUM; i++)
            load_surface(src_surface[i], i);
    } else
        upload_source_YUV_once_for_all();
    UploadPictureTicks += GetTickCount() - tmp;

    /* ready for encoding */
    memset(srcsurface_status, SRC_SURFACE_IN_ENCODING, sizeof(srcsurface_status));

    memset(&seq_param, 0, sizeof(seq_param));
    memset(&pic_param, 0, sizeof(pic_param));
    memset(&slice_param, 0, sizeof(slice_param));

    if (encode_syncmode == 0)
        pthread_create(&encode_thread, NULL, storage_task_thread, NULL);

    for (current_frame_encoding = 0; current_frame_encoding < frame_count; current_frame_encoding++) {
        encoding2display_order(current_frame_encoding, intra_period, intra_idr_period, ip_period,
                               &current_frame_display, &current_frame_type);
        if (current_frame_type == FRAME_IDR) {
            numShortTerm = 0;
            current_frame_num = 0;
            current_IDR_display = current_frame_display;
        }
        printf("%s : %lld %s : %lld type : %d\n", "encoding order", current_frame_encoding, "Display order", current_frame_display, current_frame_type);
        /* check if the source frame is ready */
        while (srcsurface_status[current_slot] != SRC_SURFACE_IN_ENCODING) {
            usleep(1);
        }

        tmp = GetTickCount();
        va_status = vaBeginPicture(va_dpy, context_id, src_surface[current_slot]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
        BeginPictureTicks += GetTickCount() - tmp;
        fill_vps_header(&vps);
        fill_sps_header(&sps, 0);
        fill_pps_header(&pps, 0, 0);
        tmp = GetTickCount();
        if (current_frame_type == FRAME_IDR) {
            render_sequence(&sps);
            render_packedvideo();
            render_packedsequence();
        }
        render_packedpicture();
        render_picture(&pps);
        fill_slice_header(0, &pps, &ssh);
        render_slice();
        RenderPictureTicks += GetTickCount() - tmp;

        tmp = GetTickCount();
        va_status = vaEndPicture(va_dpy, context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");;
        EndPictureTicks += GetTickCount() - tmp;

        if (encode_syncmode)
            storage_task(current_frame_display, current_frame_encoding);
        else /* queue the storage task queue */
            storage_task_queue(current_frame_display, current_frame_encoding);

        update_ReferenceFrames();
    }

    if (encode_syncmode == 0) {
        int ret;
        pthread_join(encode_thread, (void **)&ret);
    }

    return 0;
}


static int release_encode()
{
    int i;

    vaDestroySurfaces(va_dpy, &src_surface[0], SURFACE_NUM);
    vaDestroySurfaces(va_dpy, &ref_surface[0], SURFACE_NUM);

    for (i = 0; i < SURFACE_NUM; i++)
        vaDestroyBuffer(va_dpy, coded_buf[i]);

    vaDestroyContext(va_dpy, context_id);
    vaDestroyConfig(va_dpy, config_id);

    return 0;
}

static int deinit_va()
{
    vaTerminate(va_dpy);

    va_close_display(va_dpy);

    return 0;
}


static int print_input()
{
    printf("\n\nINPUT:Try to encode HEVC...\n");
    if (rc_mode != -1)
        printf("INPUT: RateControl  : %s\n", rc_to_string(rc_mode));
    printf("INPUT: Resolution   : %dx%d, %d frames\n",
           frame_width, frame_height, frame_count);
    printf("INPUT: FrameRate    : %d\n", frame_rate);
    printf("INPUT: Bitrate      : %d\n", frame_bitrate);
    printf("INPUT: Slieces      : %d\n", frame_slices);
    printf("INPUT: IntraPeriod  : %d\n", intra_period);
    printf("INPUT: IDRPeriod    : %d\n", intra_idr_period);
    printf("INPUT: IpPeriod     : %d\n", ip_period);
    printf("INPUT: Initial QP   : %d\n", initial_qp);
    printf("INPUT: Min QP       : %d\n", minimal_qp);
    printf("INPUT: P As B       : %d\n", p2b);
    printf("INPUT: lowpower     : %d\n", lowpower);
    printf("INPUT: Source YUV   : %s", srcyuv_fp ? "FILE" : "AUTO generated");
    if (srcyuv_fp)
        printf(":%s (fourcc %s)\n", srcyuv_fn, fourcc_to_string(srcyuv_fourcc));
    else
        printf("\n");
    printf("INPUT: Coded Clip   : %s\n", coded_fn);
    if (recyuv_fp == NULL)
        printf("INPUT: Rec   Clip   : %s\n", "Not save reconstructed frame");
    else
        printf("INPUT: Rec   Clip   : Save reconstructed frame into %s (fourcc %s)\n", recyuv_fn,
               fourcc_to_string(srcyuv_fourcc));

    printf("\n\n"); /* return back to startpoint */

    return 0;
}

static int calc_PSNR(double *psnr)
{
    char *srcyuv_ptr = NULL, *recyuv_ptr = NULL, tmp;
    unsigned long long min_size;
    unsigned long long i, sse = 0;
    double ssemean;
    int fourM = 0x400000; /* 4M */

    min_size = MIN(srcyuv_frames, frame_count) * frame_width * frame_height * 1.5;
    for (i = 0; i < min_size; i++) {
        unsigned long long j = i % fourM;

        if ((i % fourM) == 0) {
            if (srcyuv_ptr)
                munmap(srcyuv_ptr, fourM);
            if (recyuv_ptr)
                munmap(recyuv_ptr, fourM);

            srcyuv_ptr = mmap(0, fourM, PROT_READ, MAP_SHARED, fileno(srcyuv_fp), i);
            recyuv_ptr = mmap(0, fourM, PROT_READ, MAP_SHARED, fileno(recyuv_fp), i);
            if ((srcyuv_ptr == MAP_FAILED) || (recyuv_ptr == MAP_FAILED)) {
                if (srcyuv_ptr != MAP_FAILED) 
                    munmap(srcyuv_ptr, fourM);
                if (recyuv_ptr != MAP_FAILED) 
                    munmap(recyuv_ptr, fourM);
                printf("Failed to mmap YUV files\n");
                return 1;
            }
        }
        tmp = srcyuv_ptr[j] - recyuv_ptr[j];
        sse += tmp * tmp;
    }
    ssemean = (double)sse / (double)min_size;
    *psnr = 20.0 * log10(255) - 10.0 * log10(ssemean);

    if (srcyuv_ptr)
        munmap(srcyuv_ptr, fourM);
    if (recyuv_ptr)
        munmap(recyuv_ptr, fourM);

    return 0;
}

static int print_performance(unsigned int PictureCount)
{
    unsigned int psnr_ret = 1, others = 0;
    double psnr = 0, total_size = frame_width * frame_height * 1.5 * frame_count;

    if (calc_psnr && srcyuv_fp && recyuv_fp)
        psnr_ret = calc_PSNR(&psnr);

    others = TotalTicks - UploadPictureTicks - BeginPictureTicks
             - RenderPictureTicks - EndPictureTicks - SyncPictureTicks - SavePictureTicks;

    printf("\n\n");

    printf("PERFORMANCE:   Frame Rate           : %.2f fps (%d frames, %d ms (%.2f ms per frame))\n",
           (double) 1000 * PictureCount / TotalTicks, PictureCount,
           TotalTicks, ((double)  TotalTicks) / (double) PictureCount);
    printf("PERFORMANCE:   Compression ratio    : %d:1\n", (unsigned int)(total_size / frame_size));
    if (psnr_ret == 0)
        printf("PERFORMANCE:   PSNR                 : %.2f (%lld frames calculated)\n",
               psnr, MIN(frame_count, srcyuv_frames));

    printf("PERFORMANCE:     UploadPicture      : %d ms (%.2f, %.2f%% percent)\n",
           (int) UploadPictureTicks, ((double)  UploadPictureTicks) / (double) PictureCount,
           UploadPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaBeginPicture     : %d ms (%.2f, %.2f%% percent)\n",
           (int) BeginPictureTicks, ((double)  BeginPictureTicks) / (double) PictureCount,
           BeginPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaRenderHeader     : %d ms (%.2f, %.2f%% percent)\n",
           (int) RenderPictureTicks, ((double)  RenderPictureTicks) / (double) PictureCount,
           RenderPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaEndPicture       : %d ms (%.2f, %.2f%% percent)\n",
           (int) EndPictureTicks, ((double)  EndPictureTicks) / (double) PictureCount,
           EndPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaSyncSurface      : %d ms (%.2f, %.2f%% percent)\n",
           (int) SyncPictureTicks, ((double) SyncPictureTicks) / (double) PictureCount,
           SyncPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     SavePicture        : %d ms (%.2f, %.2f%% percent)\n",
           (int) SavePictureTicks, ((double)  SavePictureTicks) / (double) PictureCount,
           SavePictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     Others             : %d ms (%.2f, %.2f%% percent)\n",
           (int) others, ((double) others) / (double) PictureCount,
           others / (double) TotalTicks / 0.01);

    if (encode_syncmode == 0)
        printf("(Multithread enabled, the timing is only for reference)\n");

    return 0;
}


int main(int argc, char **argv)
{
    unsigned int start;

    va_init_display_args(&argc, argv);
    process_cmdline(argc, argv);

    print_input();

    start = GetTickCount();

    init_va();
    setup_encode();

    encode_frames();

    release_encode();
    deinit_va();

    TotalTicks += GetTickCount() - start;
    print_performance(frame_count);

    return 0;
}
