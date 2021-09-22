/*
 * Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved.
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
#include <va/va_enc_h264.h>
#include "va_display.h"

#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }

#include "loadsurface.h"

#define NAL_REF_IDC_NONE        0
#define NAL_REF_IDC_LOW         1
#define NAL_REF_IDC_MEDIUM      2
#define NAL_REF_IDC_HIGH        3

#define NAL_NON_IDR             1
#define NAL_IDR                 5
#define NAL_SPS                 7
#define NAL_PPS                 8
#define NAL_SEI         6

#define SLICE_TYPE_P            0
#define SLICE_TYPE_B            1
#define SLICE_TYPE_I            2
#define IS_P_SLICE(type) (SLICE_TYPE_P == (type))
#define IS_B_SLICE(type) (SLICE_TYPE_B == (type))
#define IS_I_SLICE(type) (SLICE_TYPE_I == (type))


#define ENTROPY_MODE_CAVLC      0
#define ENTROPY_MODE_CABAC      1

#define PROFILE_IDC_BASELINE    66
#define PROFILE_IDC_MAIN        77
#define PROFILE_IDC_HIGH        100

#define BITSTREAM_ALLOCATE_STEPPING     4096

#define SURFACE_NUM 16 /* 16 surfaces for source YUV */
#define SURFACE_NUM 16 /* 16 surfaces for reference */
static  VADisplay va_dpy;
static  VAProfile h264_profile = ~0;
static  VAConfigAttrib attrib[VAConfigAttribTypeMax];
static  VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
static  int config_attrib_num = 0, enc_packed_header_idx;
static  VASurfaceID src_surface[SURFACE_NUM];
static  VABufferID  coded_buf[SURFACE_NUM];
static  VASurfaceID ref_surface[SURFACE_NUM];
static  VAConfigID config_id;
static  VAContextID context_id;
static  VAEncSequenceParameterBufferH264 seq_param;
static  VAEncPictureParameterBufferH264 pic_param;
static  VAEncSliceParameterBufferH264 slice_param;
static  VAPictureH264 CurrentCurrPic;
static  VAPictureH264 ReferenceFrames[16], RefPicList0_P[32], RefPicList0_B[32], RefPicList1_B[32];

static  unsigned int MaxFrameNum = (2 << 16);
static  unsigned int MaxPicOrderCntLsb = (2 << 8);
static  unsigned int Log2MaxFrameNum = 16;
static  unsigned int Log2MaxPicOrderCntLsb = 8;

static  unsigned int num_ref_frames = 2;
static  unsigned int numShortTerm = 0;
static  int constraint_set_flag = 0;
static  int h264_packedheader = 0; /* support pack header? */
static  int h264_maxref = (1 << 16 | 1);
static  int h264_entropy_mode = 1; /* cabac */

static  char *coded_fn = NULL, *srcyuv_fn = NULL, *recyuv_fn = NULL;
static  FILE *coded_fp = NULL, *srcyuv_fp = NULL, *recyuv_fp = NULL;
static  unsigned long long srcyuv_frames = 0;
static  int srcyuv_fourcc = VA_FOURCC_NV12;
static  int calc_psnr = 0;

static  int frame_width = 176;
static  int frame_height = 144;
static  int frame_width_mbaligned;
static  int frame_height_mbaligned;
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

//Default entrypoint for Encode
static VAEntrypoint requested_entrypoint = -1;
static VAEntrypoint selected_entrypoint = -1;

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
bitstream_put_ui(bitstream *bs, unsigned int val, int size_in_bits)
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
bitstream_put_ue(bitstream *bs, unsigned int val)
{
    int size_in_bits = 0;
    int tmp_val = ++val;

    while (tmp_val) {
        tmp_val >>= 1;
        size_in_bits++;
    }

    bitstream_put_ui(bs, 0, size_in_bits - 1); // leading zero
    bitstream_put_ui(bs, val, size_in_bits);
}

static void
bitstream_put_se(bitstream *bs, int val)
{
    unsigned int new_val;

    if (val <= 0)
        new_val = -2 * val;
    else
        new_val = 2 * val - 1;

    bitstream_put_ue(bs, new_val);
}

static void
bitstream_byte_aligning(bitstream *bs, int bit)
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

    bitstream_put_ui(bs, new_val, bit_left);
}

static void
rbsp_trailing_bits(bitstream *bs)
{
    bitstream_put_ui(bs, 1, 1);
    bitstream_byte_aligning(bs, 0);
}

static void nal_start_code_prefix(bitstream *bs)
{
    bitstream_put_ui(bs, 0x00000001, 32);
}

static void nal_header(bitstream *bs, int nal_ref_idc, int nal_unit_type)
{
    bitstream_put_ui(bs, 0, 1);                /* forbidden_zero_bit: 0 */
    bitstream_put_ui(bs, nal_ref_idc, 2);
    bitstream_put_ui(bs, nal_unit_type, 5);
}

static void sps_rbsp(bitstream *bs)
{
    int profile_idc = PROFILE_IDC_BASELINE;

    if (h264_profile  == VAProfileH264High)
        profile_idc = PROFILE_IDC_HIGH;
    else if (h264_profile  == VAProfileH264Main)
        profile_idc = PROFILE_IDC_MAIN;

    bitstream_put_ui(bs, profile_idc, 8);               /* profile_idc */
    bitstream_put_ui(bs, !!(constraint_set_flag & 1), 1);                         /* constraint_set0_flag */
    bitstream_put_ui(bs, !!(constraint_set_flag & 2), 1);                         /* constraint_set1_flag */
    bitstream_put_ui(bs, !!(constraint_set_flag & 4), 1);                         /* constraint_set2_flag */
    bitstream_put_ui(bs, !!(constraint_set_flag & 8), 1);                         /* constraint_set3_flag */
    bitstream_put_ui(bs, 0, 4);                         /* reserved_zero_4bits */
    bitstream_put_ui(bs, seq_param.level_idc, 8);      /* level_idc */
    bitstream_put_ue(bs, seq_param.seq_parameter_set_id);      /* seq_parameter_set_id */

    if (profile_idc == PROFILE_IDC_HIGH) {
        bitstream_put_ue(bs, 1);        /* chroma_format_idc = 1, 4:2:0 */
        bitstream_put_ue(bs, 0);        /* bit_depth_luma_minus8 */
        bitstream_put_ue(bs, 0);        /* bit_depth_chroma_minus8 */
        bitstream_put_ui(bs, 0, 1);     /* qpprime_y_zero_transform_bypass_flag */
        bitstream_put_ui(bs, 0, 1);     /* seq_scaling_matrix_present_flag */
    }

    bitstream_put_ue(bs, seq_param.seq_fields.bits.log2_max_frame_num_minus4); /* log2_max_frame_num_minus4 */
    bitstream_put_ue(bs, seq_param.seq_fields.bits.pic_order_cnt_type);        /* pic_order_cnt_type */

    if (seq_param.seq_fields.bits.pic_order_cnt_type == 0)
        bitstream_put_ue(bs, seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);     /* log2_max_pic_order_cnt_lsb_minus4 */
    else {
        assert(0);
    }

    bitstream_put_ue(bs, seq_param.max_num_ref_frames);        /* num_ref_frames */
    bitstream_put_ui(bs, 0, 1);                                 /* gaps_in_frame_num_value_allowed_flag */

    bitstream_put_ue(bs, seq_param.picture_width_in_mbs - 1);  /* pic_width_in_mbs_minus1 */
    bitstream_put_ue(bs, seq_param.picture_height_in_mbs - 1); /* pic_height_in_map_units_minus1 */
    bitstream_put_ui(bs, seq_param.seq_fields.bits.frame_mbs_only_flag, 1);    /* frame_mbs_only_flag */

    if (!seq_param.seq_fields.bits.frame_mbs_only_flag) {
        assert(0);
    }

    bitstream_put_ui(bs, seq_param.seq_fields.bits.direct_8x8_inference_flag, 1);      /* direct_8x8_inference_flag */
    bitstream_put_ui(bs, seq_param.frame_cropping_flag, 1);            /* frame_cropping_flag */

    if (seq_param.frame_cropping_flag) {
        bitstream_put_ue(bs, seq_param.frame_crop_left_offset);        /* frame_crop_left_offset */
        bitstream_put_ue(bs, seq_param.frame_crop_right_offset);       /* frame_crop_right_offset */
        bitstream_put_ue(bs, seq_param.frame_crop_top_offset);         /* frame_crop_top_offset */
        bitstream_put_ue(bs, seq_param.frame_crop_bottom_offset);      /* frame_crop_bottom_offset */
    }

    //if ( frame_bit_rate < 0 ) { //TODO EW: the vui header isn't correct
    if (1) {
        bitstream_put_ui(bs, 0, 1); /* vui_parameters_present_flag */
    } else {
        bitstream_put_ui(bs, 1, 1); /* vui_parameters_present_flag */
        bitstream_put_ui(bs, 0, 1); /* aspect_ratio_info_present_flag */
        bitstream_put_ui(bs, 0, 1); /* overscan_info_present_flag */
        bitstream_put_ui(bs, 0, 1); /* video_signal_type_present_flag */
        bitstream_put_ui(bs, 0, 1); /* chroma_loc_info_present_flag */
        bitstream_put_ui(bs, 1, 1); /* timing_info_present_flag */
        {
            bitstream_put_ui(bs, 15, 32);
            bitstream_put_ui(bs, 900, 32);
            bitstream_put_ui(bs, 1, 1);
        }
        bitstream_put_ui(bs, 1, 1); /* nal_hrd_parameters_present_flag */
        {
            // hrd_parameters
            bitstream_put_ue(bs, 0);    /* cpb_cnt_minus1 */
            bitstream_put_ui(bs, 4, 4); /* bit_rate_scale */
            bitstream_put_ui(bs, 6, 4); /* cpb_size_scale */

            bitstream_put_ue(bs, frame_bitrate - 1); /* bit_rate_value_minus1[0] */
            bitstream_put_ue(bs, frame_bitrate * 8 - 1); /* cpb_size_value_minus1[0] */
            bitstream_put_ui(bs, 1, 1);  /* cbr_flag[0] */

            bitstream_put_ui(bs, 23, 5);   /* initial_cpb_removal_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* cpb_removal_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* dpb_output_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* time_offset_length  */
        }
        bitstream_put_ui(bs, 0, 1);   /* vcl_hrd_parameters_present_flag */
        bitstream_put_ui(bs, 0, 1);   /* low_delay_hrd_flag */

        bitstream_put_ui(bs, 0, 1); /* pic_struct_present_flag */
        bitstream_put_ui(bs, 0, 1); /* bitstream_restriction_flag */
    }

    rbsp_trailing_bits(bs);     /* rbsp_trailing_bits */
}


static void pps_rbsp(bitstream *bs)
{
    bitstream_put_ue(bs, pic_param.pic_parameter_set_id);      /* pic_parameter_set_id */
    bitstream_put_ue(bs, pic_param.seq_parameter_set_id);      /* seq_parameter_set_id */

    bitstream_put_ui(bs, pic_param.pic_fields.bits.entropy_coding_mode_flag, 1);  /* entropy_coding_mode_flag */

    bitstream_put_ui(bs, 0, 1);                         /* pic_order_present_flag: 0 */

    bitstream_put_ue(bs, 0);                            /* num_slice_groups_minus1 */

    bitstream_put_ue(bs, pic_param.num_ref_idx_l0_active_minus1);      /* num_ref_idx_l0_active_minus1 */
    bitstream_put_ue(bs, pic_param.num_ref_idx_l1_active_minus1);      /* num_ref_idx_l1_active_minus1 1 */

    bitstream_put_ui(bs, pic_param.pic_fields.bits.weighted_pred_flag, 1);     /* weighted_pred_flag: 0 */
    bitstream_put_ui(bs, pic_param.pic_fields.bits.weighted_bipred_idc, 2); /* weighted_bipred_idc: 0 */

    bitstream_put_se(bs, pic_param.pic_init_qp - 26);  /* pic_init_qp_minus26 */
    bitstream_put_se(bs, 0);                            /* pic_init_qs_minus26 */
    bitstream_put_se(bs, 0);                            /* chroma_qp_index_offset */

    bitstream_put_ui(bs, pic_param.pic_fields.bits.deblocking_filter_control_present_flag, 1); /* deblocking_filter_control_present_flag */
    bitstream_put_ui(bs, 0, 1);                         /* constrained_intra_pred_flag */
    bitstream_put_ui(bs, 0, 1);                         /* redundant_pic_cnt_present_flag */

    /* more_rbsp_data */
    bitstream_put_ui(bs, pic_param.pic_fields.bits.transform_8x8_mode_flag, 1);    /*transform_8x8_mode_flag */
    bitstream_put_ui(bs, 0, 1);                         /* pic_scaling_matrix_present_flag */
    bitstream_put_se(bs, pic_param.second_chroma_qp_index_offset);     /*second_chroma_qp_index_offset */

    rbsp_trailing_bits(bs);
}

static void slice_header(bitstream *bs)
{
    int first_mb_in_slice = slice_param.macroblock_address;

    bitstream_put_ue(bs, first_mb_in_slice);        /* first_mb_in_slice: 0 */
    bitstream_put_ue(bs, slice_param.slice_type);   /* slice_type */
    bitstream_put_ue(bs, slice_param.pic_parameter_set_id);        /* pic_parameter_set_id: 0 */
    bitstream_put_ui(bs, pic_param.frame_num, seq_param.seq_fields.bits.log2_max_frame_num_minus4 + 4); /* frame_num */

    /* frame_mbs_only_flag == 1 */
    if (!seq_param.seq_fields.bits.frame_mbs_only_flag) {
        /* FIXME: */
        assert(0);
    }

    if (pic_param.pic_fields.bits.idr_pic_flag)
        bitstream_put_ue(bs, slice_param.idr_pic_id);       /* idr_pic_id: 0 */

    if (seq_param.seq_fields.bits.pic_order_cnt_type == 0) {
        bitstream_put_ui(bs, pic_param.CurrPic.TopFieldOrderCnt, seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 + 4);
        /* pic_order_present_flag == 0 */
    } else {
        /* FIXME: */
        assert(0);
    }

    /* redundant_pic_cnt_present_flag == 0 */
    /* slice type */
    if (IS_P_SLICE(slice_param.slice_type)) {
        bitstream_put_ui(bs, slice_param.num_ref_idx_active_override_flag, 1);            /* num_ref_idx_active_override_flag: */

        if (slice_param.num_ref_idx_active_override_flag)
            bitstream_put_ue(bs, slice_param.num_ref_idx_l0_active_minus1);

        /* ref_pic_list_reordering */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l0: 0 */
    } else if (IS_B_SLICE(slice_param.slice_type)) {
        bitstream_put_ui(bs, slice_param.direct_spatial_mv_pred_flag, 1);            /* direct_spatial_mv_pred: 1 */

        bitstream_put_ui(bs, slice_param.num_ref_idx_active_override_flag, 1);       /* num_ref_idx_active_override_flag: */

        if (slice_param.num_ref_idx_active_override_flag) {
            bitstream_put_ue(bs, slice_param.num_ref_idx_l0_active_minus1);
            bitstream_put_ue(bs, slice_param.num_ref_idx_l1_active_minus1);
        }

        /* ref_pic_list_reordering */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l0: 0 */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l1: 0 */
    }

    if ((pic_param.pic_fields.bits.weighted_pred_flag &&
         IS_P_SLICE(slice_param.slice_type)) ||
        ((pic_param.pic_fields.bits.weighted_bipred_idc == 1) &&
         IS_B_SLICE(slice_param.slice_type))) {
        /* FIXME: fill weight/offset table */
        assert(0);
    }

    /* dec_ref_pic_marking */
    if (pic_param.pic_fields.bits.reference_pic_flag) {     /* nal_ref_idc != 0 */
        unsigned char no_output_of_prior_pics_flag = 0;
        unsigned char long_term_reference_flag = 0;
        unsigned char adaptive_ref_pic_marking_mode_flag = 0;

        if (pic_param.pic_fields.bits.idr_pic_flag) {
            bitstream_put_ui(bs, no_output_of_prior_pics_flag, 1);            /* no_output_of_prior_pics_flag: 0 */
            bitstream_put_ui(bs, long_term_reference_flag, 1);            /* long_term_reference_flag: 0 */
        } else {
            bitstream_put_ui(bs, adaptive_ref_pic_marking_mode_flag, 1);            /* adaptive_ref_pic_marking_mode_flag: 0 */
        }
    }

    if (pic_param.pic_fields.bits.entropy_coding_mode_flag &&
        !IS_I_SLICE(slice_param.slice_type))
        bitstream_put_ue(bs, slice_param.cabac_init_idc);               /* cabac_init_idc: 0 */

    bitstream_put_se(bs, slice_param.slice_qp_delta);                   /* slice_qp_delta: 0 */

    /* ignore for SP/SI */

    if (pic_param.pic_fields.bits.deblocking_filter_control_present_flag) {
        bitstream_put_ue(bs, slice_param.disable_deblocking_filter_idc);           /* disable_deblocking_filter_idc: 0 */

        if (slice_param.disable_deblocking_filter_idc != 1) {
            bitstream_put_se(bs, slice_param.slice_alpha_c0_offset_div2);          /* slice_alpha_c0_offset_div2: 2 */
            bitstream_put_se(bs, slice_param.slice_beta_offset_div2);              /* slice_beta_offset_div2: 2 */
        }
    }

    if (pic_param.pic_fields.bits.entropy_coding_mode_flag) {
        bitstream_byte_aligning(bs, 1);
    }
}

static int
build_packed_pic_buffer(unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);
    nal_header(&bs, NAL_REF_IDC_HIGH, NAL_PPS);
    pps_rbsp(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int
build_packed_seq_buffer(unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);
    nal_header(&bs, NAL_REF_IDC_HIGH, NAL_SPS);
    sps_rbsp(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

#if 0

static int
build_packed_sei_buffer_timing(unsigned int init_cpb_removal_length,
                               unsigned int init_cpb_removal_delay,
                               unsigned int init_cpb_removal_delay_offset,
                               unsigned int cpb_removal_length,
                               unsigned int cpb_removal_delay,
                               unsigned int dpb_output_length,
                               unsigned int dpb_output_delay,
                               unsigned char **sei_buffer)
{
    unsigned char *byte_buf;
    int bp_byte_size, i, pic_byte_size;

    bitstream nal_bs;
    bitstream sei_bp_bs, sei_pic_bs;

    bitstream_start(&sei_bp_bs);
    bitstream_put_ue(&sei_bp_bs, 0);       /*seq_parameter_set_id*/
    bitstream_put_ui(&sei_bp_bs, init_cpb_removal_delay, cpb_removal_length);
    bitstream_put_ui(&sei_bp_bs, init_cpb_removal_delay_offset, cpb_removal_length);
    if (sei_bp_bs.bit_offset & 0x7) {
        bitstream_put_ui(&sei_bp_bs, 1, 1);
    }
    bitstream_end(&sei_bp_bs);
    bp_byte_size = (sei_bp_bs.bit_offset + 7) / 8;

    bitstream_start(&sei_pic_bs);
    bitstream_put_ui(&sei_pic_bs, cpb_removal_delay, cpb_removal_length);
    bitstream_put_ui(&sei_pic_bs, dpb_output_delay, dpb_output_length);
    if (sei_pic_bs.bit_offset & 0x7) {
        bitstream_put_ui(&sei_pic_bs, 1, 1);
    }
    bitstream_end(&sei_pic_bs);
    pic_byte_size = (sei_pic_bs.bit_offset + 7) / 8;

    bitstream_start(&nal_bs);
    nal_start_code_prefix(&nal_bs);
    nal_header(&nal_bs, NAL_REF_IDC_NONE, NAL_SEI);

    /* Write the SEI buffer period data */
    bitstream_put_ui(&nal_bs, 0, 8);
    bitstream_put_ui(&nal_bs, bp_byte_size, 8);

    byte_buf = (unsigned char *)sei_bp_bs.buffer;
    for (i = 0; i < bp_byte_size; i++) {
        bitstream_put_ui(&nal_bs, byte_buf[i], 8);
    }
    free(byte_buf);
    /* write the SEI timing data */
    bitstream_put_ui(&nal_bs, 0x01, 8);
    bitstream_put_ui(&nal_bs, pic_byte_size, 8);

    byte_buf = (unsigned char *)sei_pic_bs.buffer;
    for (i = 0; i < pic_byte_size; i++) {
        bitstream_put_ui(&nal_bs, byte_buf[i], 8);
    }
    free(byte_buf);

    rbsp_trailing_bits(&nal_bs);
    bitstream_end(&nal_bs);

    *sei_buffer = (unsigned char *)nal_bs.buffer;

    return nal_bs.bit_offset;
}

#endif

static int build_packed_slice_buffer(unsigned char **header_buffer)
{
    bitstream bs;
    int is_idr = !!pic_param.pic_fields.bits.idr_pic_flag;
    int is_ref = !!pic_param.pic_fields.bits.reference_pic_flag;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);

    if (IS_I_SLICE(slice_param.slice_type)) {
        nal_header(&bs, NAL_REF_IDC_HIGH, is_idr ? NAL_IDR : NAL_NON_IDR);
    } else if (IS_P_SLICE(slice_param.slice_type)) {
        nal_header(&bs, NAL_REF_IDC_MEDIUM, NAL_NON_IDR);
    } else {
        assert(IS_B_SLICE(slice_param.slice_type));
        nal_header(&bs, is_ref ? NAL_REF_IDC_LOW : NAL_REF_IDC_NONE, NAL_NON_IDR);
    }

    slice_header(&bs);
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
  3) intra_idr_period % intra_period (intra_period > 0) and intra_period % ip_period must be 0
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
  >=2          0              >=2          IDR(PBB)(PBB)(IBB) (6/0/3)
                                              (PBB)(IBB)(PBB)(IBB)...
  >=2          >=2              1          IDRPPPPP IPPPPP IPPPPP (6/18/1)
                                           IDRPPPPP IPPPPP IPPPPP...
  >=2          >=2              >=2        {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)} (6/18/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)(IBB)(PBB)}           (6/12/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)}                     (6/6/3)
                                           {IDR(PBB)(PBB)}.
*/

/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type
 */
#define FRAME_P 0
#define FRAME_B 1
#define FRAME_I 2
#define FRAME_IDR 7
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
               ((ip_period == 1 && encoding_order_gop % intra_period == 0) || /* for IDR PPPPP IPPPP */
                /* for IDR (PBB)(PBB)(IBB) */
                (ip_period >= 2 && ((encoding_order_gop - 1) / ip_period % (intra_period / ip_period)) == 0))) {
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
    printf("./h264encode <options>\n");
    printf("   -w <width> -h <height>\n");
    printf("   -framecount <frame number>\n");
    printf("   -n <frame number>\n");
    printf("      if set to 0 and srcyuv is set, the frame count is from srcuv file\n");
    printf("   -o <coded file>\n");
    printf("   -f <frame rate>\n");
    printf("   --intra_period <number>\n");
    printf("   --idr_period <number>\n");
    printf("   --ip_period <number>\n");
    printf("   --bitrate <bitrate>\n");
    printf("   --initialqp <number>\n");
    printf("   --minqp <number>\n");
    printf("   --rcmode <NONE|CBR|VBR|VCM|CQP|VBR_CONTRAINED>\n");
    printf("   --syncmode: sequentially upload source, encoding, save result, no multi-thread\n");
    printf("   --srcyuv <filename> load YUV from a file\n");
    printf("   --fourcc <NV12|IYUV|YV12> source YUV fourcc\n");
    printf("   --recyuv <filename> save reconstructed YUV into a file\n");
    printf("   --enablePSNR calculate PSNR of recyuv vs. srcyuv\n");
    printf("   --entropy <0|1>, 1 means cabac, 0 cavlc\n");
    printf("   --profile <BP|MP|HP>\n");
    printf("   --low_power <num> 0: Normal mode, 1: Low power mode, others: auto mode\n");
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
        {"entropy", required_argument, NULL, 17 },
        {"profile", required_argument, NULL, 18 },
        {"low_power", required_argument, NULL, 19 },
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
            if (!coded_fn)
                coded_fn = strdup(optarg);
            assert(coded_fn);
            break;
        case 0:
            print_help();
            exit(0);
        case 1:
            frame_bitrate = atoi(optarg);
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
            if (!srcyuv_fn)
                srcyuv_fn = strdup(optarg);
            assert(srcyuv_fn);
            break;
        case 10:
            if (!recyuv_fn)
                recyuv_fn = strdup(optarg);
            assert(recyuv_fn);
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
            h264_entropy_mode = atoi(optarg) ? 1 : 0;
            break;
        case 18:
            if (strncmp(optarg, "BP", 2) == 0)
                h264_profile = VAProfileH264ConstrainedBaseline;
            else if (strncmp(optarg, "MP", 2) == 0)
                h264_profile = VAProfileH264Main;
            else if (strncmp(optarg, "HP", 2) == 0)
                h264_profile = VAProfileH264High;
            else
                h264_profile = 0;
            break;
        case 19: {
            int lp_option = atoi(optarg);
            if (lp_option == 0)
                requested_entrypoint = VAEntrypointEncSlice; //normal 0
            else if (lp_option == 1)
                requested_entrypoint =  VAEntrypointEncSliceLP; //low power 1
            else
                requested_entrypoint = -1;
        }
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
    if (intra_period != 1 && intra_period % ip_period != 0) {
        printf(" intra_period must be a multiplier of ip_period\n");
        exit(0);
    }
    if (intra_period != 0 && intra_idr_period % intra_period != 0) {
        printf(" intra_idr_period must be a multiplier of intra_period\n");
        exit(0);
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

            fstat(fileno(srcyuv_fp), &tmp);
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
            coded_fn = strdup("/tmp/test.264");
        else if (stat("/sdcard", &buf) == 0)
            coded_fn = strdup("/sdcard/test.264");
        else
            coded_fn = strdup("./test.264");

        assert(coded_fn);
    }

    /* store coded data into a file */
    coded_fp = fopen(coded_fn, "w+");
    if (coded_fp == NULL) {
        printf("Open file %s failed, exit\n", coded_fn);
        exit(1);
    }

    frame_width_mbaligned = (frame_width + 15) & (~15);
    frame_height_mbaligned = (frame_height + 15) & (~15);
    if (frame_width != frame_width_mbaligned ||
        frame_height != frame_height_mbaligned) {
        printf("Source frame is %dx%d and will code clip to %dx%d with crop\n",
               frame_width, frame_height,
               frame_width_mbaligned, frame_height_mbaligned
              );
    }

    return 0;
}

static int init_va(void)
{
    VAProfile profile_list[] = {VAProfileH264High, VAProfileH264Main, VAProfileH264ConstrainedBaseline};
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
        if ((h264_profile != ~0) && h264_profile != profile_list[i])
            continue;

        h264_profile = profile_list[i];
        vaQueryConfigEntrypoints(va_dpy, h264_profile, entrypoints, &num_entrypoints);
        for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
            if (requested_entrypoint == -1) {
                //Select the entry point based on what is avaiable
                if ((entrypoints[slice_entrypoint] == VAEntrypointEncSlice) ||
                    (entrypoints[slice_entrypoint] == VAEntrypointEncSliceLP)) {
                    support_encode = 1;
                    selected_entrypoint = entrypoints[slice_entrypoint];
                    break;
                }
            } else if ((entrypoints[slice_entrypoint] == requested_entrypoint)) {
                //Select the entry point based on what was requested in cmd line option
                support_encode = 1;
                selected_entrypoint = entrypoints[slice_entrypoint];
                break;
            }
        }
        if (support_encode == 1) {
            printf("Using EntryPoint - %d \n", selected_entrypoint);
            break;
        }
    }

    if (support_encode == 0) {
        printf("Can't find VAEntrypointEncSlice or  VAEntrypointEncSliceLP for H264 profiles\n");
        exit(1);
    } else {
        switch (h264_profile) {
        case VAProfileH264ConstrainedBaseline:
            printf("Use profile VAProfileH264ConstrainedBaseline\n");
            constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.2 */
            ip_period = 1;
            break;

        case VAProfileH264Main:
            printf("Use profile VAProfileH264Main\n");
            constraint_set_flag |= (1 << 1); /* Annex A.2.2 */
            break;

        case VAProfileH264High:
            constraint_set_flag |= (1 << 3); /* Annex A.2.4 */
            printf("Use profile VAProfileH264High\n");
            break;
        default:
            printf("unknow profile. Set to Constrained Baseline");
            h264_profile = VAProfileH264ConstrainedBaseline;
            constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.1 & A.2.2 */
            ip_period = 1;
            break;
        }
    }

    /* find out the format for the render target, and rate control mode */
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attrib[i].type = i;

    va_status = vaGetConfigAttributes(va_dpy, h264_profile, selected_entrypoint,
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

        h264_packedheader = 1;
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
        h264_maxref = attrib[VAConfigAttribEncMaxRefFrames].value;

        printf("Support %d RefPicList0 and %d RefPicList1\n",
               h264_maxref & 0xffff, (h264_maxref >> 16) & 0xffff);
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

    free(entrypoints);
    return 0;
}

static int setup_encode()
{
    VAStatus va_status;
    VASurfaceID *tmp_surfaceid;
    int codedbuf_size, i;

    va_status = vaCreateConfig(va_dpy, h264_profile, selected_entrypoint,
                               &config_attrib[0], config_attrib_num, &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    /* create source surfaces */
    va_status = vaCreateSurfaces(va_dpy,
                                 VA_RT_FORMAT_YUV420, frame_width_mbaligned, frame_height_mbaligned,
                                 &src_surface[0], SURFACE_NUM,
                                 NULL, 0);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* create reference surfaces */
    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, frame_width_mbaligned, frame_height_mbaligned,
                    &ref_surface[0], SURFACE_NUM,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    tmp_surfaceid = calloc(2 * SURFACE_NUM, sizeof(VASurfaceID));
    assert(tmp_surfaceid);
    memcpy(tmp_surfaceid, src_surface, SURFACE_NUM * sizeof(VASurfaceID));
    memcpy(tmp_surfaceid + SURFACE_NUM, ref_surface, SURFACE_NUM * sizeof(VASurfaceID));

    /* Create a context for this encode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                                frame_width_mbaligned, frame_height_mbaligned,
                                VA_PROGRESSIVE,
                                tmp_surfaceid, 2 * SURFACE_NUM,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
    free(tmp_surfaceid);

    codedbuf_size = ((long long int)frame_width_mbaligned * frame_height_mbaligned * 400) / (16 * 16);

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

static void sort_one(VAPictureH264 ref[], int left, int right,
                     int ascending, int frame_idx)
{
    int i = left, j = right;
    unsigned int key;
    VAPictureH264 tmp;

    if (frame_idx) {
        key = ref[(left + right) / 2].frame_idx;
        partition(ref, frame_idx, key, ascending);
    } else {
        key = ref[(left + right) / 2].TopFieldOrderCnt;
        partition(ref, TopFieldOrderCnt, (signed int)key, ascending);
    }

    /* recursion */
    if (left < j)
        sort_one(ref, left, j, ascending, frame_idx);

    if (i < right)
        sort_one(ref, i, right, ascending, frame_idx);
}

static void sort_two(VAPictureH264 ref[], int left, int right, unsigned int key, unsigned int frame_idx,
                     int partition_ascending, int list0_ascending, int list1_ascending)
{
    int i = left, j = right;
    VAPictureH264 tmp;

    if (frame_idx) {
        partition(ref, frame_idx, key, partition_ascending);
    } else {
        partition(ref, TopFieldOrderCnt, (signed int)key, partition_ascending);
    }


    sort_one(ref, left, i - 1, list0_ascending, frame_idx);
    sort_one(ref, j + 1, right, list1_ascending, frame_idx);
}

static int update_ReferenceFrames(void)
{
    int i;

    if (current_frame_type == FRAME_B)
        return 0;

    CurrentCurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    numShortTerm++;
    if (numShortTerm > num_ref_frames)
        numShortTerm = num_ref_frames;
    for (i = numShortTerm - 1; i > 0; i--)
        ReferenceFrames[i] = ReferenceFrames[i - 1];
    ReferenceFrames[0] = CurrentCurrPic;

    if (current_frame_type != FRAME_B)
        current_frame_num++;
    if (current_frame_num > MaxFrameNum)
        current_frame_num = 0;

    return 0;
}


static int update_RefPicList(void)
{
    unsigned int current_poc = CurrentCurrPic.TopFieldOrderCnt;

    if (current_frame_type == FRAME_P) {
        memcpy(RefPicList0_P, ReferenceFrames, numShortTerm * sizeof(VAPictureH264));
        sort_one(RefPicList0_P, 0, numShortTerm - 1, 0, 1);
    }

    if (current_frame_type == FRAME_B) {
        memcpy(RefPicList0_B, ReferenceFrames, numShortTerm * sizeof(VAPictureH264));
        sort_two(RefPicList0_B, 0, numShortTerm - 1, current_poc, 0,
                 1, 0, 1);

        memcpy(RefPicList1_B, ReferenceFrames, numShortTerm * sizeof(VAPictureH264));
        sort_two(RefPicList1_B, 0, numShortTerm - 1, current_poc, 0,
                 0, 1, 0);
    }

    return 0;
}


static int render_sequence(void)
{
    VABufferID seq_param_buf, rc_param_buf, misc_param_tmpbuf, render_id[2];
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param, *misc_param_tmp;
    VAEncMiscParameterRateControl *misc_rate_ctrl;

    seq_param.level_idc = 41 /*SH_LEVEL_3*/;
    seq_param.picture_width_in_mbs = frame_width_mbaligned / 16;
    seq_param.picture_height_in_mbs = frame_height_mbaligned / 16;
    seq_param.bits_per_second = frame_bitrate;

    seq_param.intra_period = intra_period;
    seq_param.intra_idr_period = intra_idr_period;
    seq_param.ip_period = ip_period;

    seq_param.max_num_ref_frames = num_ref_frames;
    seq_param.seq_fields.bits.frame_mbs_only_flag = 1;
    seq_param.time_scale = 900;
    seq_param.num_units_in_tick = 15; /* Tc = num_units_in_tick / time_sacle */
    seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = Log2MaxPicOrderCntLsb - 4;
    seq_param.seq_fields.bits.log2_max_frame_num_minus4 = Log2MaxFrameNum - 4;;
    seq_param.seq_fields.bits.frame_mbs_only_flag = 1;
    seq_param.seq_fields.bits.chroma_format_idc = 1;
    seq_param.seq_fields.bits.direct_8x8_inference_flag = 1;

    if (frame_width != frame_width_mbaligned ||
        frame_height != frame_height_mbaligned) {
        seq_param.frame_cropping_flag = 1;
        seq_param.frame_crop_left_offset = 0;
        seq_param.frame_crop_right_offset = (frame_width_mbaligned - frame_width) / 2;
        seq_param.frame_crop_top_offset = 0;
        seq_param.frame_crop_bottom_offset = (frame_height_mbaligned - frame_height) / 2;
    }

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
    CHECK_VASTATUS(va_status, "vaRenderPicture");;

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

static int calc_poc(int pic_order_cnt_lsb)
{
    static int PicOrderCntMsb_ref = 0, pic_order_cnt_lsb_ref = 0;
    int prevPicOrderCntMsb, prevPicOrderCntLsb;
    int PicOrderCntMsb, TopFieldOrderCnt;

    if (current_frame_type == FRAME_IDR)
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
    else {
        prevPicOrderCntMsb = PicOrderCntMsb_ref;
        prevPicOrderCntLsb = pic_order_cnt_lsb_ref;
    }

    if ((pic_order_cnt_lsb < prevPicOrderCntLsb) &&
        ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (int)(MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (int)(MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
        PicOrderCntMsb = prevPicOrderCntMsb;

    TopFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb;

    if (current_frame_type != FRAME_B) {
        PicOrderCntMsb_ref = PicOrderCntMsb;
        pic_order_cnt_lsb_ref = pic_order_cnt_lsb;
    }

    return TopFieldOrderCnt;
}

static int render_picture(void)
{
    VABufferID pic_param_buf;
    VAStatus va_status;
    int i = 0;

    pic_param.CurrPic.picture_id = ref_surface[current_slot];
    pic_param.CurrPic.frame_idx = current_frame_num;
    pic_param.CurrPic.flags = 0;
    pic_param.CurrPic.TopFieldOrderCnt = calc_poc((current_frame_display - current_IDR_display) % MaxPicOrderCntLsb);
    pic_param.CurrPic.BottomFieldOrderCnt = pic_param.CurrPic.TopFieldOrderCnt;
    CurrentCurrPic = pic_param.CurrPic;

    if (getenv("TO_DEL")) { /* set RefPicList into ReferenceFrames */
        update_RefPicList(); /* calc RefPicList */
        memset(pic_param.ReferenceFrames, 0xff, 16 * sizeof(VAPictureH264)); /* invalid all */
        if (current_frame_type == FRAME_P) {
            pic_param.ReferenceFrames[0] = RefPicList0_P[0];
        } else if (current_frame_type == FRAME_B) {
            pic_param.ReferenceFrames[0] = RefPicList0_B[0];
            pic_param.ReferenceFrames[1] = RefPicList1_B[0];
        }
    } else {
        memcpy(pic_param.ReferenceFrames, ReferenceFrames, numShortTerm * sizeof(VAPictureH264));
        for (i = numShortTerm; i < SURFACE_NUM; i++) {
            pic_param.ReferenceFrames[i].picture_id = VA_INVALID_SURFACE;
            pic_param.ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
        }
    }

    pic_param.pic_fields.bits.idr_pic_flag = (current_frame_type == FRAME_IDR);
    pic_param.pic_fields.bits.reference_pic_flag = (current_frame_type != FRAME_B);
    pic_param.pic_fields.bits.entropy_coding_mode_flag = h264_entropy_mode;
    pic_param.pic_fields.bits.deblocking_filter_control_present_flag = 1;
    pic_param.frame_num = current_frame_num;
    pic_param.coded_buf = coded_buf[current_slot];
    pic_param.last_picture = (current_frame_encoding == frame_count);
    pic_param.pic_init_qp = initial_qp;

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncPictureParameterBufferType,
                               sizeof(pic_param), 1, &pic_param, &pic_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &pic_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    return 0;
}

static int render_packedsequence(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedseq_para_bufid, packedseq_data_bufid, render_id[2];
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

    return 0;
}


static int render_packedpicture(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedpic_para_bufid, packedpic_data_bufid, render_id[2];
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

    return 0;
}

#if 0

static void render_packedsei(void)
{
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VABufferID packed_sei_header_param_buf_id, packed_sei_buf_id, render_id[2];
    unsigned int length_in_bits /*offset_in_bytes*/;
    unsigned char *packed_sei_buffer = NULL;
    VAStatus va_status;
    int init_cpb_size, target_bit_rate, i_initial_cpb_removal_delay_length, i_initial_cpb_removal_delay;
    int i_cpb_removal_delay, i_dpb_output_delay_length, i_cpb_removal_delay_length;

    /* it comes for the bps defined in SPS */
    target_bit_rate = frame_bitrate;
    init_cpb_size = (target_bit_rate * 8) >> 10;
    i_initial_cpb_removal_delay = init_cpb_size * 0.5 * 1024 / target_bit_rate * 90000;

    i_cpb_removal_delay = 2;
    i_initial_cpb_removal_delay_length = 24;
    i_cpb_removal_delay_length = 24;
    i_dpb_output_delay_length = 24;


    length_in_bits = build_packed_sei_buffer_timing(
                         i_initial_cpb_removal_delay_length,
                         i_initial_cpb_removal_delay,
                         0,
                         i_cpb_removal_delay_length,
                         i_cpb_removal_delay * current_frame_encoding,
                         i_dpb_output_delay_length,
                         0,
                         &packed_sei_buffer);

    //offset_in_bytes = 0;
    packed_header_param_buffer.type = VAEncPackedHeaderRawData;
    packed_header_param_buffer.bit_length = length_in_bits;
    packed_header_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                               &packed_sei_header_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packed_sei_buffer,
                               &packed_sei_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");


    render_id[0] = packed_sei_header_param_buf_id;
    render_id[1] = packed_sei_buf_id;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");


    free(packed_sei_buffer);

    return;
}

static int render_hrd(void)
{
    VABufferID misc_parameter_hrd_buf_id;
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *misc_hrd_param;

    va_status = vaCreateBuffer(va_dpy, context_id,
                               VAEncMiscParameterBufferType,
                               sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                               1,
                               NULL,
                               &misc_parameter_hrd_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy,
                misc_parameter_hrd_buf_id,
                (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeHRD;
    misc_hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    if (frame_bitrate > 0) {
        misc_hrd_param->initial_buffer_fullness = frame_bitrate * 1024 * 4;
        misc_hrd_param->buffer_size = frame_bitrate * 1024 * 8;
    } else {
        misc_hrd_param->initial_buffer_fullness = 0;
        misc_hrd_param->buffer_size = 0;
    }
    vaUnmapBuffer(va_dpy, misc_parameter_hrd_buf_id);

    va_status = vaRenderPicture(va_dpy, context_id, &misc_parameter_hrd_buf_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");;

    return 0;
}

#endif

static void render_packedslice()
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedslice_para_bufid, packedslice_data_bufid, render_id[2];
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
}

static int render_slice(void)
{
    VABufferID slice_param_buf;
    VAStatus va_status;
    int i;

    update_RefPicList();

    /* one frame, one slice */
    slice_param.macroblock_address = 0;
    slice_param.num_macroblocks = frame_width_mbaligned * frame_height_mbaligned / (16 * 16); /* Measured by MB */
    slice_param.slice_type = (current_frame_type == FRAME_IDR) ? 2 : current_frame_type;
    if (current_frame_type == FRAME_IDR) {
        if (current_frame_encoding != 0)
            ++slice_param.idr_pic_id;
    } else if (current_frame_type == FRAME_P) {
        int refpiclist0_max = h264_maxref & 0xffff;
        memcpy(slice_param.RefPicList0, RefPicList0_P, ((refpiclist0_max > 32) ? 32 : refpiclist0_max)*sizeof(VAPictureH264));

        for (i = refpiclist0_max; i < 32; i++) {
            slice_param.RefPicList0[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
        }
    } else if (current_frame_type == FRAME_B) {
        int refpiclist0_max = h264_maxref & 0xffff;
        int refpiclist1_max = (h264_maxref >> 16) & 0xffff;

        memcpy(slice_param.RefPicList0, RefPicList0_B, ((refpiclist0_max > 32) ? 32 : refpiclist0_max)*sizeof(VAPictureH264));
        for (i = refpiclist0_max; i < 32; i++) {
            slice_param.RefPicList0[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
        }

        memcpy(slice_param.RefPicList1, RefPicList1_B, ((refpiclist1_max > 32) ? 32 : refpiclist1_max)*sizeof(VAPictureH264));
        for (i = refpiclist1_max; i < 32; i++) {
            slice_param.RefPicList1[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList1[i].flags = VA_PICTURE_H264_INVALID;
        }
    }

    slice_param.slice_alpha_c0_offset_div2 = 0;
    slice_param.slice_beta_offset_div2 = 0;
    slice_param.direct_spatial_mv_pred_flag = 1;
    slice_param.pic_order_cnt_lsb = (current_frame_display - current_IDR_display) % MaxPicOrderCntLsb;


    if (h264_packedheader &&
        config_attrib[enc_packed_header_idx].value & VA_ENC_PACKED_HEADER_SLICE)
        render_packedslice();

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncSliceParameterBufferType,
                               sizeof(slice_param), 1, &slice_param, &slice_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &slice_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

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
        if (mmap_ptr)
            munmap(mmap_ptr, mmap_size);
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

    printf("\r      "); /* return back to startpoint */
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
    printf("(%06d bytes coded)", coded_size);

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
    assert(tmp);
    tmp->display_order = display_order;
    tmp->encode_order = encode_order;

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

        /* check if the source frame is ready */
        while (srcsurface_status[current_slot] != SRC_SURFACE_IN_ENCODING) {
            usleep(1);
        }

        tmp = GetTickCount();
        va_status = vaBeginPicture(va_dpy, context_id, src_surface[current_slot]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
        BeginPictureTicks += GetTickCount() - tmp;

        tmp = GetTickCount();
        if (current_frame_type == FRAME_IDR) {
            render_sequence();
            render_picture();
            if (h264_packedheader) {
                render_packedsequence();
                render_packedpicture();
            }
            //if (rc_mode == VA_RC_CBR)
            //    render_packedsei();
            //render_hrd();
        } else {
            //render_sequence();
            render_picture();
            //if (rc_mode == VA_RC_CBR)
            //    render_packedsei();
            //render_hrd();
        }
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
    printf("\n\nINPUT:Try to encode H264...\n");
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
                printf("Failed to mmap YUV files\n");

                if (srcyuv_ptr != MAP_FAILED)
                    munmap(srcyuv_ptr, fourM);
                if (recyuv_ptr != MAP_FAILED)
                    munmap(recyuv_ptr, fourM);

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

    free(srcyuv_fn);
    free(recyuv_fn);
    free(coded_fn);

    if (srcyuv_fp)
        fclose(srcyuv_fp);

    if (recyuv_fp)
        fclose(recyuv_fp);

    if (coded_fp)
        fclose(coded_fp);

    return 0;
}
