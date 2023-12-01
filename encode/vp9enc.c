/*
 * Copyright (c) 2017 Intel Corporation. All Rights Reserved.
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

/*
 * This file has some codes for vp9 bitstream built, and
 * they are ported from libvpx (https://github.com/webmproject/libvpx/).
 * The original copyright and licence statement as below.
 */

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include <va/va.h>
#include <va/va_enc_vp9.h>
#include "va_display.h"

#define KEY_FRAME               0
#define INTER_FRAME             1

#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }

#define CHECK_CONDITION(cond)                                                \
    if(!(cond))                                                              \
    {                                                                        \
        fprintf(stderr, "Unexpected condition: %s:%d\n", __func__, __LINE__); \
        exit(1);                                                             \
    }

static VADisplay va_dpy;

static int rc_mode;

static int picture_width;
static int picture_height;
static int frame_size;
static uint8_t *newImageBuffer = 0;

static int hrd_window = 1500;

static int vbr_max;

static int qp_value = 60;

static int intra_period = 30;
static int frame_bit_rate = -1;
static int frame_rate = 30;

static int lf_level = 10;
static int opt_header = 0;

static int current_slot;

static int frame_number;
static int current_frame_type;

static  VASurfaceID vp9_ref_list[8];

#define SURFACE_NUM                             8
#define SID_INPUT_PICTURE_0                     0
#define SID_INPUT_PICTURE_1                     1
#define SID_REFERENCE_PICTURE_L0                2
#define SID_REFERENCE_PICTURE_L1                3
#define SID_NUMBER                              2

static  VASurfaceID surface_ids[SID_NUMBER];
static  VASurfaceID ref_surfaces[SURFACE_NUM + SID_NUMBER];
static  int use_slot[SURFACE_NUM];

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420          0x30323449
#endif

static int rc_default_mode[4] = {
    VA_RC_CQP,
    VA_RC_CBR,
    VA_RC_VBR,
    VA_RC_NONE
};

static int vp9enc_entrypoint_lists[2] = {
    VAEntrypointEncSlice,
    VAEntrypointEncSliceLP
};

static int select_entrypoint = -1;

static const struct option long_opts[] = {
    {"help", no_argument, NULL, 0 },
    {"rcmode", required_argument, NULL, 1 },
    {"qp", required_argument, NULL, 2 },
    {"intra_period", required_argument, NULL, 3 },
    {"fb", required_argument, NULL, 4 },
    {"lf_level", required_argument, NULL, 6 },
    {"opt_header", required_argument, NULL, 7},
    {"hrd_win", required_argument, NULL, 8},
    {"vbr_max", required_argument, NULL, 9},
    {"fn_num", required_argument, NULL, 10},
    {"low_power", required_argument, NULL, 11},
    {NULL, no_argument, NULL, 0 }
};

struct vp9enc_bit_buffer {
    uint8_t *bit_buffer;
    int bit_offset;
};

struct upload_thread_param {
    FILE *yuv_fp;
    VASurfaceID surface_id;
};

struct vp9encode_context {
    VAProfile profile;
    VAEncSequenceParameterBufferVP9 seq_param;
    VAEncPictureParameterBufferVP9 pic_param;
    VAContextID context_id;
    VAConfigID config_id;
    VABufferID seq_param_buf_id;                /* Sequence level parameter */
    VABufferID pic_param_buf_id;                /* Picture level parameter */
    VABufferID codedbuf_buf_id;                 /* Output buffer, compressed data */
    VABufferID misc_parameter_hrd_buf_id;
    /* for VAEncMiscParameterTypeVP9PerSegmantParam. VAQMatrixBufferType */
    VABufferID qmatrix_buf_id;
    /* the buffer for VP9 super block. VAEncMacroblockMapBufferType */
    VABufferID mb_seg_buf_id;
    VABufferID raw_data_header_buf_id;
    VABufferID raw_data_buf_id;
    VABufferID misc_fr_buf_id;
    VABufferID misc_rc_buf_id;

    int codedbuf_i_size;
    int codedbuf_pb_size;
    int current_input_surface;
    int rate_control_method;

    struct upload_thread_param upload_thread_param;
    pthread_t upload_thread_id;
    int upload_thread_value;
};

static struct vp9encode_context vp9enc_context;

static void
vp9enc_write_word(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
}

static void
vp9enc_write_dword(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
    *(tmp + 2) = (value >> 16) & 0XFF;
    *(tmp + 3) = (value >> 24) & 0XFF;
}

static void
vp9enc_wb_write_bit(struct vp9enc_bit_buffer *wb, int bit)
{
    const int off = wb->bit_offset;
    const int p = off / 8;
    const int q = 7 - off % 8;

    if (q == 7) {
        wb->bit_buffer[p] = bit << q;
    } else {
        wb->bit_buffer[p] &= ~(1 << q);
        wb->bit_buffer[p] |= bit << q;
    }
    wb->bit_offset = off + 1;
}

static void
vp9enc_wb_write_literal(struct vp9enc_bit_buffer *wb, int data, int bits)
{
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
        vp9enc_wb_write_bit(wb, (data >> bit) & 1);
}

static void
vp9enc_write_bitdepth_colorspace_sampling(int codec_profile,
        struct vp9enc_bit_buffer *wb)
{
    if (codec_profile >= 2) {
        /* the bit-depth will be added for VP9Profile2/3 */
        /* this will be added later */
        assert(0);
    }

    /* Add the default color-space */
    vp9enc_wb_write_literal(wb, 0, 3);
    vp9enc_wb_write_bit(wb, 0);  // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]

    /* the sampling_x/y will be added for VP9Profile1/2/3 later */
}

#define    MAX_TILE_WIDTH_B64    64
#define    MIN_TILE_WIDTH_B64    4

static int
vp9enc_get_min_log2_tile_cols(const int sb_cols)
{
    int min_log2 = 0;

    while ((MAX_TILE_WIDTH_B64 << min_log2) < sb_cols)
        ++min_log2;

    return min_log2;
}

static int
vp9enc_get_max_log2_tile_cols(const int sb_cols)
{
    int max_log2 = 1;

    while ((sb_cols >> max_log2) >= MIN_TILE_WIDTH_B64)
        ++max_log2;

    return max_log2 - 1;
}

static void
vp9enc_write_uncompressed_header(struct vp9encode_context *enc_context,
                                 char *header_data,
                                 int *header_length)
{
#define    VP9_SYNC_CODE_0    0x49
#define    VP9_SYNC_CODE_1    0x83
#define    VP9_SYNC_CODE_2    0x42

#define    VP9_FRAME_MARKER   0x2

#define    REFS_PER_FRAME     3

#define    REF_FRAMES_LOG2    3
#define    REF_FRAMES         (1 << REF_FRAMES_LOG2)

#define    VP9_KEY_FRAME      0

    VAEncPictureParameterBufferVP9 *pic_param;
    struct vp9enc_bit_buffer *wb, vp9_wb;

    if (!header_data || !header_length)
        return;

    pic_param = &enc_context->pic_param;

    vp9_wb.bit_buffer = (uint8_t *)header_data;
    vp9_wb.bit_offset = 0;
    wb = &vp9_wb;
    vp9enc_wb_write_literal(wb, VP9_FRAME_MARKER, 2);

    vp9enc_wb_write_literal(wb, 0, 2);

    vp9enc_wb_write_bit(wb, 0);  // show_existing_frame
    vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.frame_type);
    vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.show_frame);
    vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.error_resilient_mode);

    if (pic_param->pic_flags.bits.frame_type == VP9_KEY_FRAME) {
        vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
        vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
        vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);

        vp9enc_write_bitdepth_colorspace_sampling(0, wb);

        /* write the encoded frame size */
        vp9enc_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
        vp9enc_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);
        /* write display size */
        if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
            (pic_param->frame_height_dst != pic_param->frame_height_src)) {
            vp9enc_wb_write_bit(wb, 1);
            vp9enc_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
            vp9enc_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
        } else
            vp9enc_wb_write_bit(wb, 0);
    } else {
        /* for the non-Key frame */
        if (!pic_param->pic_flags.bits.show_frame)
            vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.intra_only);

        if (!pic_param->pic_flags.bits.error_resilient_mode)
            vp9enc_wb_write_literal(wb, pic_param->pic_flags.bits.reset_frame_context, 2);

        if (pic_param->pic_flags.bits.intra_only) {
            vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
            vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
            vp9enc_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);

            /* Add the bit_depth for VP9Profile1/2/3 */
            /* write the refreshed_frame_flags */
            vp9enc_wb_write_literal(wb, pic_param->refresh_frame_flags, REF_FRAMES);
            /* write the encoded frame size */
            vp9enc_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
            vp9enc_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);
            /* write display size */
            if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
                (pic_param->frame_height_dst != pic_param->frame_height_src)) {
                vp9enc_wb_write_bit(wb, 1);
                vp9enc_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
                vp9enc_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
            } else
                vp9enc_wb_write_bit(wb, 0);

        } else {
            /* The refresh_frame_map is  for the next frame so that it can select Last/Godlen/Alt ref_index */
            vp9enc_wb_write_literal(wb, pic_param->refresh_frame_flags, REF_FRAMES);

            vp9enc_wb_write_literal(wb, pic_param->ref_flags.bits.ref_last_idx, REF_FRAMES_LOG2);
            vp9enc_wb_write_bit(wb, pic_param->ref_flags.bits.ref_last_sign_bias);
            vp9enc_wb_write_literal(wb, pic_param->ref_flags.bits.ref_gf_idx, REF_FRAMES_LOG2);
            vp9enc_wb_write_bit(wb, pic_param->ref_flags.bits.ref_gf_sign_bias);
            vp9enc_wb_write_literal(wb, pic_param->ref_flags.bits.ref_arf_idx, REF_FRAMES_LOG2);
            vp9enc_wb_write_bit(wb, pic_param->ref_flags.bits.ref_arf_sign_bias);

            /* write three bits with zero so that it can parse width/height directly */
            vp9enc_wb_write_literal(wb, 0, 3);
            vp9enc_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
            vp9enc_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);

            /* write display size */
            if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
                (pic_param->frame_height_dst != pic_param->frame_height_src)) {

                vp9enc_wb_write_bit(wb, 1);
                vp9enc_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
                vp9enc_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
            } else
                vp9enc_wb_write_bit(wb, 0);

            vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.allow_high_precision_mv);

#define    SWITCHABLE_FILTER    4
#define    FILTER_MASK          3

            if (pic_param->pic_flags.bits.mcomp_filter_type == SWITCHABLE_FILTER)
                vp9enc_wb_write_bit(wb, 1);
            else {
                const int filter_to_literal[4] = { 1, 0, 2, 3 };
                uint8_t filter_flag = pic_param->pic_flags.bits.mcomp_filter_type;
                filter_flag = filter_flag & FILTER_MASK;
                vp9enc_wb_write_bit(wb, 0);
                vp9enc_wb_write_literal(wb, filter_to_literal[filter_flag], 2);
            }
        }
    }

    /* write refresh_frame_context/paralle frame_decoding */
    if (!pic_param->pic_flags.bits.error_resilient_mode) {
        vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.refresh_frame_context);
        vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.frame_parallel_decoding_mode);
    }

    vp9enc_wb_write_literal(wb, pic_param->pic_flags.bits.frame_context_idx, 2);

    /* write loop filter */
    pic_param->bit_offset_lf_level = wb->bit_offset;
    vp9enc_wb_write_literal(wb, pic_param->filter_level, 6);
    vp9enc_wb_write_literal(wb, pic_param->sharpness_level, 3);

    {
        int i, mode_flag;

        vp9enc_wb_write_bit(wb, 1);
        vp9enc_wb_write_bit(wb, 1);
        pic_param->bit_offset_ref_lf_delta = wb->bit_offset;
        for (i = 0; i < 4; i++) {
            /*
             * This check is skipped to prepare the bit_offset_lf_ref
            if (pic_param->ref_lf_delta[i] == 0) {
                vp9enc_wb_write_bit(wb, 0);
                continue;
            }
             */

            vp9enc_wb_write_bit(wb, 1);
            mode_flag = pic_param->ref_lf_delta[i];
            if (mode_flag >= 0) {
                vp9enc_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9enc_wb_write_bit(wb, 0);
            } else {
                mode_flag = -mode_flag;
                vp9enc_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9enc_wb_write_bit(wb, 1);
            }
        }

        pic_param->bit_offset_mode_lf_delta = wb->bit_offset;
        for (i = 0; i < 2; i++) {
            /*
             * This check is skipped to prepare the bit_offset_lf_ref
            if (pic_param->mode_lf_delta[i] == 0) {
                vp9enc_wb_write_bit(wb, 0);
                continue;
            }
             */
            vp9enc_wb_write_bit(wb, 1);
            mode_flag = pic_param->mode_lf_delta[i];
            if (mode_flag >= 0) {
                vp9enc_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9enc_wb_write_bit(wb, 0);
            } else {
                mode_flag = -mode_flag;
                vp9enc_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9enc_wb_write_bit(wb, 1);
            }
        }
    }

    /* write basic quantizer */
    pic_param->bit_offset_qindex = wb->bit_offset;
    vp9enc_wb_write_literal(wb, pic_param->luma_ac_qindex, 8);
    if (pic_param->luma_dc_qindex_delta) {
        int delta_q = pic_param->luma_dc_qindex_delta;
        vp9enc_wb_write_bit(wb, 1);
        vp9enc_wb_write_literal(wb, abs(delta_q), 4);
        vp9enc_wb_write_bit(wb, delta_q < 0);
    } else
        vp9enc_wb_write_bit(wb, 0);

    if (pic_param->chroma_dc_qindex_delta) {
        int delta_q = pic_param->chroma_dc_qindex_delta;
        vp9enc_wb_write_bit(wb, 1);
        vp9enc_wb_write_literal(wb, abs(delta_q), 4);
        vp9enc_wb_write_bit(wb, delta_q < 0);
    } else
        vp9enc_wb_write_bit(wb, 0);

    if (pic_param->chroma_ac_qindex_delta) {
        int delta_q = pic_param->chroma_ac_qindex_delta;
        vp9enc_wb_write_bit(wb, 1);
        vp9enc_wb_write_literal(wb, abs(delta_q), 4);
        vp9enc_wb_write_bit(wb, delta_q < 0);
    } else
        vp9enc_wb_write_bit(wb, 0);

    /* segment is not enabled */
    //vp9enc_wb_write_bit(wb, pic_param->pic_flags.bits.segmentation_enabled);
    vp9enc_wb_write_bit(wb, 0);

    {
        int sb_cols = (pic_param->frame_width_dst + 63) / 64;
        int min_log2_tile_cols, max_log2_tile_cols;
        int col_data;

        /* write tile column info */
        min_log2_tile_cols = vp9enc_get_min_log2_tile_cols(sb_cols);
        max_log2_tile_cols = vp9enc_get_max_log2_tile_cols(sb_cols);

        col_data = pic_param->log2_tile_columns - min_log2_tile_cols;
        while (col_data--) {
            vp9enc_wb_write_bit(wb, 1);
        }
        if (pic_param->log2_tile_columns < max_log2_tile_cols)
            vp9enc_wb_write_bit(wb, 0);

        /* write tile row info */
        vp9enc_wb_write_bit(wb, pic_param->log2_tile_rows);
        if (pic_param->log2_tile_rows)
            vp9enc_wb_write_bit(wb, (pic_param->log2_tile_rows != 1));
    }

    /* get the bit_offset of the first partition size */
    pic_param->bit_offset_first_partition_size = wb->bit_offset;

    /* reserve the space for writing the first partitions ize */
    vp9enc_wb_write_literal(wb, 0, 16);

    *header_length = (wb->bit_offset + 7) / 8;
}

static void
vp9enc_upload_yuv_to_surface(FILE *yuv_fp, VASurfaceID surface_id)
{
    VAImage surface_image;
    VAStatus va_status;
    void *surface_p = NULL;
    uint8_t *y_src, *u_src, *v_src;
    uint8_t *y_dst, *u_dst, *v_dst;
    int y_size = picture_width * picture_height;
    int u_size = (picture_width >> 1) * (picture_height >> 1);
    int row, col;
    size_t n_items;

    do {
        n_items = fread(newImageBuffer, frame_size, 1, yuv_fp);
    } while (n_items != 1);

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    assert(VA_STATUS_SUCCESS == va_status);

    y_src = newImageBuffer;
    u_src = newImageBuffer + y_size; /* UV offset for NV12 */
    v_src = newImageBuffer + y_size + u_size;

    y_dst = surface_p + surface_image.offsets[0];
    u_dst = surface_p + surface_image.offsets[1]; /* UV offset for NV12 */
    v_dst = surface_p + surface_image.offsets[2];

    /* Y plane */
    for (row = 0; row < surface_image.height; row++) {
        memcpy(y_dst, y_src, surface_image.width);
        y_dst += surface_image.pitches[0];
        y_src += picture_width;
    }

    if (surface_image.format.fourcc == VA_FOURCC_NV12) { /* UV plane */
        for (row = 0; row < surface_image.height / 2; row++) {
            for (col = 0; col < surface_image.width / 2; col++) {
                u_dst[col * 2] = u_src[col];
                u_dst[col * 2 + 1] = v_src[col];
            }

            u_dst += surface_image.pitches[1];
            u_src += (picture_width / 2);
            v_src += (picture_width / 2);
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
               surface_image.format.fourcc == VA_FOURCC_I420) {
        const int U = surface_image.format.fourcc == VA_FOURCC_I420 ? 1 : 2;
        const int V = surface_image.format.fourcc == VA_FOURCC_I420 ? 2 : 1;

        u_dst = surface_p + surface_image.offsets[U];
        v_dst = surface_p + surface_image.offsets[V];

        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width / 2);
            memcpy(v_dst, v_src, surface_image.width / 2);
            u_dst += surface_image.pitches[U];
            v_dst += surface_image.pitches[V];
            u_src += (picture_width / 2);
            v_src += (picture_width / 2);
        }
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);
}

static void *
vp9enc_upload_thread_function(void *data)
{
    struct upload_thread_param *param = data;

    vp9enc_upload_yuv_to_surface(param->yuv_fp, param->surface_id);

    return NULL;
}

static void
vp9enc_alloc_encode_resource(FILE *yuv_fp)
{
    VAStatus va_status;
    VASurfaceAttrib attrib;
    int i;

    attrib.type = VASurfaceAttribPixelFormat;
    attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrib.value.type = VAGenericValueTypeInteger;
    attrib.value.value.i = VA_FOURCC_NV12;

    // Create surface
    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, picture_width, picture_height,
                    surface_ids, SID_NUMBER,
                    &attrib, 1
                );

    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    // Create surface
    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, picture_width, picture_height,
                    ref_surfaces, SURFACE_NUM,
                    &attrib, 1
                );

    for (i = 0; i < SID_NUMBER; i++)
        ref_surfaces[i + SURFACE_NUM] = surface_ids[i];

    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    newImageBuffer = (uint8_t *)malloc(frame_size);

    /* firstly upload YUV data to SID_INPUT_PICTURE_0 */
    vp9enc_context.upload_thread_param.yuv_fp = yuv_fp;
    vp9enc_context.upload_thread_param.surface_id = surface_ids[SID_INPUT_PICTURE_0];

    vp9enc_context.upload_thread_value = pthread_create(&vp9enc_context.upload_thread_id,
                                         NULL,
                                         vp9enc_upload_thread_function,
                                         (void*)&vp9enc_context.upload_thread_param);
}

static void
vp9enc_create_encode_pipe(FILE *yuv_fp)
{
    VAEntrypoint entrypoints[5];
    int num_entrypoints, slice_entrypoint;
    VAConfigAttrib attrib[2];
    int major_ver, minor_ver;
    VAStatus va_status;
    int i;

    va_dpy = va_open_display();
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");

    vaQueryConfigEntrypoints(va_dpy, vp9enc_context.profile, entrypoints,
                             &num_entrypoints);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (select_entrypoint == -1) {
            for (i = 0; i < 2; i++) {
                if (entrypoints[slice_entrypoint] == vp9enc_entrypoint_lists[i])
                    break;
            }

            if (i < 2) {
                select_entrypoint = i;
                break;
            }
        } else {
            assert(select_entrypoint == 0 || select_entrypoint == 1);

            if (entrypoints[slice_entrypoint] == vp9enc_entrypoint_lists[select_entrypoint])
                break;
        }
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find Slice entry point */
        assert(0);
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaGetConfigAttributes(va_dpy, vp9enc_context.profile, vp9enc_entrypoint_lists[select_entrypoint],
                          &attrib[0], 2);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        assert(0);
    }

    if ((attrib[1].value & vp9enc_context.rate_control_method) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        assert(0);
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = vp9enc_context.rate_control_method; /* set to desired RC mode */

    va_status = vaCreateConfig(va_dpy, vp9enc_context.profile, vp9enc_entrypoint_lists[select_entrypoint],
                               &attrib[0], 2, &vp9enc_context.config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    vp9enc_alloc_encode_resource(yuv_fp);

    /* Create a context for this decode pipe */
    /* the surface is added to the render_target list when creating the context */
    va_status = vaCreateContext(va_dpy, vp9enc_context.config_id,
                                picture_width, picture_height,
                                VA_PROGRESSIVE,
                                ref_surfaces, SURFACE_NUM + 2,
                                &vp9enc_context.context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
}

static void
vp9enc_destory_encode_pipe()
{
    vaDestroyContext(va_dpy, vp9enc_context.context_id);
    vaDestroyConfig(va_dpy, vp9enc_context.config_id);
    vaTerminate(va_dpy);
    va_close_display(va_dpy);
}

static void
vp9enc_release_encode_resource()
{
    pthread_join(vp9enc_context.upload_thread_id, NULL);
    free(newImageBuffer);

    vaDestroySurfaces(va_dpy, surface_ids, SID_NUMBER);
    vaDestroySurfaces(va_dpy, ref_surfaces, SURFACE_NUM);
}

static int
vp9enc_get_free_slot()
{
    int i, index = -1;

    for (i = 0; i < SURFACE_NUM; i++) {
        if (use_slot[i] == 0) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        printf("WARNING: No free slot to store the reconstructed frame \n");
        index = SURFACE_NUM - 1;
    }

    return index;
}

static void
vp9enc_update_reference_list(void)
{
    VASurfaceID last_surf;
    int last_slot;
    int i;

    /* Todo: Add the full support of reference frames */

    if (current_frame_type == KEY_FRAME) {
        memset(use_slot, 0, sizeof(use_slot));
        use_slot[current_slot] = 1;
        for (i = 0; i < SURFACE_NUM; i++)
            vp9_ref_list[i] = ref_surfaces[current_slot];

        return;
    }

    last_slot = -1;
    use_slot[current_slot] = 1;
    last_surf = vp9_ref_list[0];

    vp9_ref_list[0] = ref_surfaces[current_slot];

    for (i = 0; i < SURFACE_NUM; i++) {
        if (ref_surfaces[i] == last_surf) {
            last_slot = i;
            break;
        }
    }

    if (last_slot != -1) {
        int used_flag = 0;

        for (i = 1; i < SURFACE_NUM; i++) {
            if (vp9_ref_list[i] == last_surf) {
                used_flag = 1;
                break;
            }
        }

        if (!used_flag)
            use_slot[last_slot] = 0;
    }
}

static void
vp9enc_update_picture_parameter(int frame_type)
{
    VAEncPictureParameterBufferVP9 *pic_param;
    int recon_index;
    VASurfaceID current_surface;
    int ref_num = 0;
    int i = 0;

    recon_index = vp9enc_get_free_slot();
    current_slot = recon_index;
    current_surface = ref_surfaces[recon_index];

    pic_param = &vp9enc_context.pic_param;

    pic_param->reconstructed_frame = current_surface;
    pic_param->coded_buf = vp9enc_context.codedbuf_buf_id;

    ref_num = sizeof(pic_param->reference_frames) / sizeof(VASurfaceID);

    if (frame_type == KEY_FRAME) {
        pic_param->pic_flags.bits.frame_type = KEY_FRAME;
        pic_param->pic_flags.bits.frame_context_idx = 0;
        pic_param->ref_flags.bits.ref_last_idx = 0;
        pic_param->ref_flags.bits.ref_gf_idx = 0;
        pic_param->ref_flags.bits.ref_arf_idx = 0;
        pic_param->pic_flags.bits.frame_context_idx = 0;
        pic_param->ref_flags.bits.ref_frame_ctrl_l0 = 0;

        for (i = 0; i < ref_num; i++)
            pic_param->reference_frames[i] = VA_INVALID_ID;
    } else {
        pic_param->refresh_frame_flags = 0x01;
        pic_param->pic_flags.bits.frame_type = INTER_FRAME;
        pic_param->ref_flags.bits.ref_frame_ctrl_l0 = 0x7;

        pic_param->ref_flags.bits.ref_last_idx = 0;
        pic_param->ref_flags.bits.ref_gf_idx = 1;
        pic_param->ref_flags.bits.ref_arf_idx = 2;
        pic_param->pic_flags.bits.frame_context_idx = 0;

        memcpy(&pic_param->reference_frames, vp9_ref_list, sizeof(vp9_ref_list));
    }
}

static void
vp9enc_create_picture_parameter_buf()
{
    VAStatus va_status;

    va_status = vaCreateBuffer(va_dpy,
                               vp9enc_context.context_id,
                               VAEncPictureParameterBufferType,
                               sizeof(vp9enc_context.pic_param), 1,
                               &vp9enc_context.pic_param,
                               &vp9enc_context.pic_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");
}

static void
vp9enc_begin_picture(FILE *yuv_fp, int frame_num, int frame_type)
{
    VAStatus va_status;

    if (vp9enc_context.upload_thread_value != 0) {
        fprintf(stderr, "FATAL error!!!\n");
        exit(1);
    }

    pthread_join(vp9enc_context.upload_thread_id, NULL);

    vp9enc_context.upload_thread_value = -1;

    /* sequence parameter set */
    VAEncSequenceParameterBufferVP9 *seq_param = &vp9enc_context.seq_param;
    va_status = vaCreateBuffer(va_dpy,
                               vp9enc_context.context_id,
                               VAEncSequenceParameterBufferType,
                               sizeof(*seq_param), 1, seq_param,
                               &vp9enc_context.seq_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    /* hrd parameter */
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *misc_hrd_param;
    va_status = vaCreateBuffer(va_dpy,
                   vp9enc_context.context_id,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                   1,
                   NULL,
                   &vp9enc_context.misc_parameter_hrd_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy,
                vp9enc_context.misc_parameter_hrd_buf_id,
                (void **)&misc_param);

    misc_param->type = VAEncMiscParameterTypeHRD;
    misc_hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    if (frame_bit_rate > 0) {
        misc_hrd_param->initial_buffer_fullness = frame_bit_rate * hrd_window / 2;
        misc_hrd_param->buffer_size = frame_bit_rate * hrd_window;
    } else {
        misc_hrd_param->initial_buffer_fullness = 0;
        misc_hrd_param->buffer_size = 0;
    }

    vaUnmapBuffer(va_dpy, vp9enc_context.misc_parameter_hrd_buf_id);

    VAEncMiscParameterTypeVP9PerSegmantParam seg_param;

    memset(&seg_param, 0, sizeof(seg_param));

    va_status = vaCreateBuffer(va_dpy,
                               vp9enc_context.context_id,
                               VAQMatrixBufferType,
                               sizeof(seg_param), 1, &seg_param,
                               &vp9enc_context.qmatrix_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    /* Create the Misc FR/RC buffer under non-CQP mode */
    if (rc_mode != VA_RC_CQP && frame_type == KEY_FRAME) {
        VAEncMiscParameterFrameRate *misc_fr;
        VAEncMiscParameterRateControl *misc_rc;

        va_status = vaCreateBuffer(va_dpy,
                       vp9enc_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                       1,
                       NULL,
                       &vp9enc_context.misc_fr_buf_id);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        vaMapBuffer(va_dpy,
                    vp9enc_context.misc_fr_buf_id,
                    (void **)&misc_param);
        misc_param->type = VAEncMiscParameterTypeFrameRate;
        misc_fr = (VAEncMiscParameterFrameRate *)misc_param->data;
        misc_fr->framerate = frame_rate;
        vaUnmapBuffer(va_dpy, vp9enc_context.misc_fr_buf_id);

        va_status = vaCreateBuffer(va_dpy,
                       vp9enc_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                       1,
                       NULL,
                       &vp9enc_context.misc_rc_buf_id);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        vaMapBuffer(va_dpy,
                    vp9enc_context.misc_rc_buf_id,
                    (void **)&misc_param);
        misc_param->type = VAEncMiscParameterTypeRateControl;
        misc_rc = (VAEncMiscParameterRateControl *)misc_param->data;

        misc_rc->bits_per_second = frame_bit_rate * 1000;
        misc_rc->window_size = hrd_window;
        misc_rc->initial_qp = qp_value;
        /* The target percentage is only for VBR. It is ignored in CBR */
        misc_rc->target_percentage = 95;
        if (rc_mode == VA_RC_VBR) {
            misc_rc->bits_per_second = vbr_max * 1000;
            misc_rc->target_percentage = (frame_bit_rate * 100) / vbr_max;
        }
        vaUnmapBuffer(va_dpy, vp9enc_context.misc_rc_buf_id);
    }
}

static void
vp9enc_render_picture()
{
    VAStatus va_status;
    VABufferID va_buffers[10];
    uint32_t num_va_buffers = 0;

    va_buffers[num_va_buffers++] = vp9enc_context.seq_param_buf_id;
    va_buffers[num_va_buffers++] = vp9enc_context.pic_param_buf_id;

    if (vp9enc_context.misc_parameter_hrd_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] =  vp9enc_context.misc_parameter_hrd_buf_id;

    if (vp9enc_context.qmatrix_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] =  vp9enc_context.qmatrix_buf_id;

    if (vp9enc_context.mb_seg_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] =  vp9enc_context.mb_seg_buf_id;

    if (vp9enc_context.raw_data_header_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] = vp9enc_context.raw_data_header_buf_id;

    if (vp9enc_context.raw_data_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] = vp9enc_context.raw_data_buf_id;

    if (vp9enc_context.misc_fr_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] = vp9enc_context.misc_fr_buf_id;

    if (vp9enc_context.misc_rc_buf_id != VA_INVALID_ID)
        va_buffers[num_va_buffers++] = vp9enc_context.misc_rc_buf_id;

    va_status = vaBeginPicture(va_dpy,
                               vp9enc_context.context_id,
                               surface_ids[vp9enc_context.current_input_surface]);
    CHECK_VASTATUS(va_status, "vaBeginPicture");

    va_status = vaRenderPicture(va_dpy,
                                vp9enc_context.context_id,
                                va_buffers,
                                num_va_buffers);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(va_dpy, vp9enc_context.context_id);
    CHECK_VASTATUS(va_status, "vaEndPicture");
}

static void
vp9enc_destroy_buffers(VABufferID *va_buffers, uint32_t num_va_buffers)
{
    VAStatus va_status;
    uint32_t i;

    for (i = 0; i < num_va_buffers; i++) {
        if (va_buffers[i] != VA_INVALID_ID) {
            va_status = vaDestroyBuffer(va_dpy, va_buffers[i]);
            CHECK_VASTATUS(va_status, "vaDestroyBuffer");
            va_buffers[i] = VA_INVALID_ID;
        }
    }
}

static void
vp9enc_write_frame_header(FILE *vp9_output, int frame_size)
{
    char header[12];

    vp9enc_write_dword(header, (uint32_t)frame_size);
    vp9enc_write_dword(header + 4, 0);
    vp9enc_write_dword(header + 8, 0);

    fwrite(header, 1, 12, vp9_output);
}

static int
vp9enc_store_coded_buffer(FILE *vp9_fp, int frame_type)
{
    VACodedBufferSegment *coded_buffer_segment;
    uint8_t *coded_mem;
    int data_length;
    VAStatus va_status;
    VASurfaceStatus surface_status;
    size_t w_items;

    va_status = vaSyncSurface(va_dpy, surface_ids[vp9enc_context.current_input_surface]);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    surface_status = 0;
    va_status = vaQuerySurfaceStatus(va_dpy, surface_ids[vp9enc_context.current_input_surface], &surface_status);
    CHECK_VASTATUS(va_status, "vaQuerySurfaceStatus");

    va_status = vaMapBuffer(va_dpy, vp9enc_context.codedbuf_buf_id, (void **)(&coded_buffer_segment));
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    coded_mem = coded_buffer_segment->buf;

    if (coded_buffer_segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) {
        if (frame_type == KEY_FRAME)
            vp9enc_context.codedbuf_i_size *= 2;
        else
            vp9enc_context.codedbuf_pb_size *= 2;

        vaUnmapBuffer(va_dpy, vp9enc_context.codedbuf_buf_id);
        return -1;
    }

    data_length = coded_buffer_segment->size;

    vp9enc_write_frame_header(vp9_fp, data_length);

    do {
        w_items = fwrite(coded_mem, data_length, 1, vp9_fp);
    } while (w_items != 1);

    vaUnmapBuffer(va_dpy, vp9enc_context.codedbuf_buf_id);

    return 0;
}

static void
vp9enc_write_ivf_header(FILE *vp9_file,
                        int width, int height,
                        int frame_num,
                        int frame_rate)
{

#define VP9_FOURCC    0x30395056

    char header[32];

    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';

    vp9enc_write_word(header + 4, 0);
    vp9enc_write_word(header + 6, 32);
    vp9enc_write_dword(header + 8, VP9_FOURCC);
    vp9enc_write_word(header + 12, width);
    vp9enc_write_word(header + 14, height);
    vp9enc_write_dword(header + 16, 1);
    vp9enc_write_dword(header + 20, frame_rate);
    vp9enc_write_dword(header + 24, frame_num);
    vp9enc_write_dword(header + 28, 0);

    fwrite(header, 1, 32, vp9_file);
}

static void
vp9enc_get_frame_type(int encoding_order,
                      int gop_size,
                      int ip_period,
                      int *frame_type)
{
    if (ip_period == 0 ||
        (encoding_order % gop_size == 0))
        *frame_type = KEY_FRAME;
    else
        *frame_type = INTER_FRAME;
}

static void
vp9enc_end_picture()
{
    vp9enc_destroy_buffers(&vp9enc_context.seq_param_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.pic_param_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.codedbuf_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.qmatrix_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.misc_parameter_hrd_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.mb_seg_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.raw_data_header_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.raw_data_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.misc_fr_buf_id, 1);
    vp9enc_destroy_buffers(&vp9enc_context.misc_rc_buf_id, 1);

    if (vp9enc_context.current_input_surface == SID_INPUT_PICTURE_0)
        vp9enc_context.current_input_surface = SID_INPUT_PICTURE_1;
    else
        vp9enc_context.current_input_surface = SID_INPUT_PICTURE_0;
}

static void
vp9enc_encode_picture(FILE *yuv_fp, FILE *vp9_fp,
                      int frame_num,
                      int frame_type,
                      int next_enc_frame)
{
    VAStatus va_status;
    int ret = 0, codedbuf_size;

    vp9enc_begin_picture(yuv_fp, frame_num, frame_type);

    if (next_enc_frame < frame_num) {
        int index;

        /* prepare for next frame */
        if (vp9enc_context.current_input_surface == SID_INPUT_PICTURE_0)
            index = SID_INPUT_PICTURE_1;
        else
            index = SID_INPUT_PICTURE_0;

        ret = fseeko(yuv_fp, (off_t)frame_size * next_enc_frame, SEEK_SET);
        CHECK_CONDITION(ret == 0);

        vp9enc_context.upload_thread_param.yuv_fp = yuv_fp;
        vp9enc_context.upload_thread_param.surface_id = surface_ids[index];

        vp9enc_context.upload_thread_value = pthread_create(&vp9enc_context.upload_thread_id,
                                             NULL,
                                             vp9enc_upload_thread_function,
                                             (void*)&vp9enc_context.upload_thread_param);
    }

    do {
        vp9enc_destroy_buffers(&vp9enc_context.codedbuf_buf_id, 1);
        vp9enc_destroy_buffers(&vp9enc_context.pic_param_buf_id, 1);

        if (frame_type == KEY_FRAME)
            codedbuf_size = vp9enc_context.codedbuf_i_size;
        else
            codedbuf_size = vp9enc_context.codedbuf_pb_size;

        /* coded buffer */
        va_status = vaCreateBuffer(va_dpy,
                                   vp9enc_context.context_id,
                                   VAEncCodedBufferType,
                                   codedbuf_size, 1, NULL,
                                   &vp9enc_context.codedbuf_buf_id);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        /* picture parameter set */
        vp9enc_update_picture_parameter(current_frame_type);

        if (opt_header) {
            char raw_data[64];
            int raw_data_length;
            VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

            memset(raw_data, 0, sizeof(raw_data));
            vp9enc_write_uncompressed_header(&vp9enc_context,
                                             raw_data,
                                             &raw_data_length);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.bit_length = raw_data_length * 8;
            packed_header_param_buffer.has_emulation_bytes = 0;

            va_status = vaCreateBuffer(va_dpy,
                           vp9enc_context.context_id,
                           VAEncPackedHeaderParameterBufferType,
                           sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                           &vp9enc_context.raw_data_header_buf_id);
            CHECK_VASTATUS(va_status, "vaCreateBuffer");

            va_status = vaCreateBuffer(va_dpy,
                           vp9enc_context.context_id,
                           VAEncPackedHeaderDataBufferType,
                           raw_data_length, 1, raw_data,
                           &vp9enc_context.raw_data_buf_id);
            CHECK_VASTATUS(va_status, "vaCreateBuffer");
        }

        vp9enc_create_picture_parameter_buf();

        vp9enc_render_picture();

        ret = vp9enc_store_coded_buffer(vp9_fp, current_frame_type);
    } while (ret);

    vp9enc_update_reference_list();

    vp9enc_end_picture();
}

static void
vp9enc_context_seq_param_init(VAEncSequenceParameterBufferVP9 *seq_param,
                              int width, int height)
{
    seq_param->intra_period = intra_period;
    seq_param->max_frame_width = width;
    seq_param->max_frame_height = height;
    seq_param->kf_auto = 0;
    seq_param->kf_min_dist = 1;
    seq_param->kf_max_dist = intra_period;

    if (frame_bit_rate > 0)
        seq_param->bits_per_second = 1000 * frame_bit_rate; /* use kbps as input */
    else
        seq_param->bits_per_second = 0;
}

static void
vp9enc_context_pic_param_init(VAEncPictureParameterBufferVP9 *pic_param,
                              int width, int height)
{
    int i, ref_num;

    memset(pic_param, 0, sizeof(VAEncPictureParameterBufferVP9));

    ref_num = sizeof(pic_param->reference_frames) / sizeof(VASurfaceID);
    for (i = 0; i < ref_num; i++)
        pic_param->reference_frames[i] = VA_INVALID_ID;

    pic_param->coded_buf = VA_INVALID_ID;
    pic_param->reconstructed_frame = VA_INVALID_ID;

    pic_param->frame_width_src = width;
    pic_param->frame_height_src = height;
    pic_param->frame_width_dst = width;
    pic_param->frame_height_dst = height;

    pic_param->ref_flags.bits.force_kf = 0;
    /* always show it */
    pic_param->pic_flags.bits.show_frame = 1;
    /* Not use silience mode */
    pic_param->pic_flags.bits.error_resilient_mode = 0;
    /* Not use the frame parallel mode */
    pic_param->pic_flags.bits.frame_parallel_decoding_mode = 0;

    pic_param->luma_ac_qindex = qp_value;
    pic_param->luma_dc_qindex_delta = 1;
    pic_param->chroma_ac_qindex_delta = 1;
    pic_param->chroma_dc_qindex_delta = 1;
    /* use the zero sharpness_level/lf_ref&mode deltas */
    pic_param->filter_level = lf_level;

    for (i = 0; i < 4; i++)
        pic_param->ref_lf_delta[i] = 1;

    for (i = 0; i < 2; i++)
        pic_param->mode_lf_delta[i] = 1;
}


static void
vp9enc_context_init(int width, int height)
{
    memset(&vp9enc_context, 0, sizeof(vp9enc_context));
    vp9enc_context.profile = VAProfileVP9Profile0;

    memset(&use_slot, 0, sizeof(use_slot));

    vp9enc_context.seq_param_buf_id = VA_INVALID_ID;
    vp9enc_context.pic_param_buf_id = VA_INVALID_ID;
    vp9enc_context.mb_seg_buf_id = VA_INVALID_ID;
    vp9enc_context.misc_parameter_hrd_buf_id = VA_INVALID_ID;
    vp9enc_context.qmatrix_buf_id = VA_INVALID_ID;
    vp9enc_context.codedbuf_buf_id = VA_INVALID_ID;
    vp9enc_context.raw_data_header_buf_id = VA_INVALID_ID;
    vp9enc_context.raw_data_buf_id = VA_INVALID_ID;
    vp9enc_context.misc_fr_buf_id = VA_INVALID_ID;
    vp9enc_context.misc_rc_buf_id = VA_INVALID_ID;

    vp9enc_context.codedbuf_i_size = width * height;
    vp9enc_context.codedbuf_pb_size = width * height;
    vp9enc_context.current_input_surface = SID_INPUT_PICTURE_0;
    vp9enc_context.upload_thread_value = -1;

    vp9enc_context.rate_control_method = rc_mode;

    vp9enc_context_seq_param_init(&vp9enc_context.seq_param, 8192, 8192);
    vp9enc_context_pic_param_init(&vp9enc_context.pic_param, width, height);
}

static void
vp9enc_show_help()
{
    printf("Usage: vp9encode <width> <height> <input_yuvfile> <output_vp9> additional_option\n");
    printf("output_vp9 should use *.ivf\n");
    printf("The additional option is listed\n");
    printf("-f <frame rate> \n");
    printf("--intra_period <key_frame interval>\n");
    printf("--qp <quantization parameter> \n");
    printf("--rcmode <rate control mode> 0: CQP, 1: CBR, 2: VBR\n");
    printf("--fb <bitrate> (kbps unit)\n");
    printf("--lf_level <loop filter level>  [0-63]\n");
    printf("--hrd_win <num>  [1000-8000]\n");
    printf("--vbr_max <num> (kbps unit. It should be greater than fb)\n");
    printf("--opt_header \n  write the uncompressed header manually. without this, the driver will add those headers by itself\n");
    printf("--fn_num <num>\n  how many frames to be encoded\n");
    printf("--low_power <num> 0: Normal mode, 1: Low power mode, others: auto mode\n");
}

int
main(int argc, char *argv[])
{
    char *yuv_input, *vp9_output, *tmp_vp9;
    FILE *yuv_fp;
    FILE *vp9_fp;
    off_t file_size;
    struct timeval tpstart, tpend;
    float  timeuse;
    int ip_period;
    int fn_num;
    int frame_idx;

    va_init_display_args(&argc, argv);

    //TODO may be we should using option analytics library
    if (argc < 5) {
        vp9enc_show_help();
        exit(0);
    }

    picture_width = atoi(argv[1]);
    picture_height = atoi(argv[2]);
    yuv_input = argv[3];
    vp9_output = argv[4];
    fn_num = -1;

    if (!yuv_input || !vp9_output) {
        vp9enc_show_help();
        exit(0);
    }

    tmp_vp9 = strdup(argv[4]);
    assert(tmp_vp9);

    if (!strstr(tmp_vp9, ".ivf")) {
        free(tmp_vp9);
        vp9enc_show_help();
        exit(0);
    }

    free(tmp_vp9);

    rc_mode = VA_RC_CQP;
    frame_rate = 30;
    qp_value = 60;
    intra_period = 30;
    frame_bit_rate = -1;
    vbr_max = -1;
    if (argc > 5) {
        int c, long_index, tmp_input;
        while (1) {
            c = getopt_long_only(argc - 4, argv + 4, "hf:?", long_opts, &long_index);

            if (c == -1)
                break;

            switch (c) {
            case 'h':
            case 0:
                vp9enc_show_help();
                exit(0);
                break;
            case 'f':
                frame_rate = atoi(optarg);
                break;
            case 1:
                tmp_input = atoi(optarg);
                if (tmp_input > 3 || tmp_input < 0)
                    tmp_input = 0;
                rc_mode = rc_default_mode[tmp_input];
                break;
            case 2:
                tmp_input = atoi(optarg);
                if (tmp_input < 0 || tmp_input > 255)
                    tmp_input = 60;
                qp_value = tmp_input;
                break;
            case 3:
                tmp_input = atoi(optarg);
                if (tmp_input < 0)
                    tmp_input = 30;
                intra_period = tmp_input;
                break;
            case 4:
                tmp_input = atoi(optarg);
                frame_bit_rate = tmp_input;
                break;
            case 6:
                tmp_input = atoi(optarg);
                if (tmp_input < 0 || tmp_input > 63)
                    tmp_input = 10;
                lf_level = tmp_input;
                break;
            case 7:
                tmp_input = atoi(optarg);
                if (tmp_input)
                    tmp_input = 1;
                opt_header = tmp_input;
                break;
            case 8:
                tmp_input = atoi(optarg);
                if (tmp_input > 8000 || tmp_input < 1000)
                    tmp_input = 1500;
                hrd_window = tmp_input;
                break;
            case 9:
                tmp_input = atoi(optarg);
                if (tmp_input < 0)
                    tmp_input = 20000;
                else if (tmp_input > 100000)
                    tmp_input = 100000;
                vbr_max = tmp_input;
                break;
            case 10:
                tmp_input = atoi(optarg);
                fn_num = tmp_input;
                break;
            case 11:
                tmp_input = atoi(optarg);

                if (tmp_input == 0 || tmp_input == 1)
                    select_entrypoint = tmp_input;
                else
                    select_entrypoint = -1;

                break;

            default:
                vp9enc_show_help();
                exit(0);
                break;
            }
        }
    }

    if (rc_mode != VA_RC_CQP && (frame_bit_rate < 0)) {
        printf("Please specifiy the bit rate for CBR/VBR\n");
        vp9enc_show_help();
        exit(0);
    }

    if (rc_mode == VA_RC_VBR) {
        if (vbr_max < 0) {
            vbr_max = frame_bit_rate;
            frame_bit_rate = (vbr_max * 95 / 100);
        }

        if (vbr_max < frame_bit_rate) {
            printf("Under VBR, the max bit rate should be greater than or equal to fb\n");
            vp9enc_show_help();
            exit(0);
        }

        if (vbr_max > (frame_bit_rate * 2)) {
            printf("under VBR, the max bit rate is too much greater than the average bit\n");
            vp9enc_show_help();
            exit(0);
        }
    }

    yuv_fp = fopen(yuv_input, "rb");
    if (yuv_fp == NULL) {
        printf("Can't open input YUV file\n");
        return -1;
    }
    fseeko(yuv_fp, (off_t)0, SEEK_END);
    file_size = ftello(yuv_fp);
    frame_size = picture_width * picture_height + ((picture_width * picture_height) >> 1) ;

    if (frame_size == 0) {
        fclose(yuv_fp);
        printf("Frame size is not correct\n");
        return -1;
    }
    if ((file_size < frame_size) || (file_size % frame_size)) {
        fclose(yuv_fp);
        printf("The YUV file's size is not correct\n");
        return -1;
    }
    frame_number = file_size / frame_size;
    fseeko(yuv_fp, (off_t)0, SEEK_SET);

    if (fn_num > 0 && fn_num <= frame_number)
        frame_number = fn_num;

    vp9_fp = fopen(vp9_output, "wb");
    if (vp9_fp == NULL) {
        fclose(yuv_fp);
        printf("Can't open output avc file\n");
        return -1;
    }

    if (intra_period == 0)
        intra_period = frame_number;

    ip_period = 1;
    if (intra_period == 1)
        ip_period = 0;

    gettimeofday(&tpstart, NULL);

    vp9enc_write_ivf_header(vp9_fp, picture_width, picture_height,
                            frame_number, frame_rate);
    vp9enc_context_init(picture_width, picture_height);
    vp9enc_create_encode_pipe(yuv_fp);

    for (frame_idx = 0; frame_idx < frame_number; frame_idx++) {
        vp9enc_get_frame_type(frame_idx, intra_period, ip_period,
                              &current_frame_type);

        vp9enc_encode_picture(yuv_fp, vp9_fp, frame_number,
                              current_frame_type, frame_idx + 1);

        printf("\r %d/%d ...", (frame_idx + 1), frame_number);
        fflush(stdout);
    }

    gettimeofday(&tpend, NULL);
    timeuse = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeuse /= 1000000;

    printf("\ndone!\n");
    printf("encode %d frames in %f secondes, FPS is %.1f\n", frame_number, timeuse, frame_number / timeuse);

    vp9enc_release_encode_resource();
    vp9enc_destory_encode_pipe();

    fclose(yuv_fp);
    fclose(vp9_fp);

    return 0;
}
