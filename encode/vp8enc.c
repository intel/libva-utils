/*
 * Copyright (c) 2018 Georg Ottinger. All Rights Reserved.
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

/* This file includes code taken from vp9enc.c (libva-utils) copyright 2017
 * by Intel Cooperation and licensed under same conditions.
 * This file includes code ported from vaapiencoder_vp8.cpp (libyami) copyright
 * by 2014-2016 Intel Corporation. https://github.com/intel/libyami
 * The original copyright and licence statement as below.
 */

/*
 * Copyright (C) 2014-2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include <va/va.h>
#include <va/va_enc_vp8.h>
#include "va_display.h"

#define MAX_XY_RESOLUTION       16364

#define KEY_FRAME               0
#define INTER_FRAME             1

#define NUM_REF_SURFACES        4
#define NUM_INPUT_SURFACES      2
#define NUM_SURFACES_TOTAL      (NUM_REF_SURFACES+NUM_INPUT_SURFACES)
#define SID_INPUT_PICTURE_0     (NUM_REF_SURFACES)
#define SID_INPUT_PICTURE_1     (NUM_REF_SURFACES+1)
#define NUM_BUFFERS             10

#define VP8ENC_OK               0
#define VP8ENC_FAIL             -1
#define PARSE_OPTIONS_OK        0
#define PARSE_OPTIONS_FAIL      -1

#ifndef N_ELEMENTS
#define N_ELEMENTS(array) (sizeof(array)/sizeof(array[0]))
#endif

#ifndef CHECK_VASTATUS
#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }
#endif

#define CHECK_CONDITION(cond)                                                \
    if(!(cond))                                                              \
    {                                                                        \
        fprintf(stderr, "Unexpected condition: %s:%d\n", __func__, __LINE__); \
        exit(1);                                                             \
    }

static const struct option long_opts[] = {
    {"help", no_argument, NULL, 0 },
    {"rcmode", required_argument, NULL, 1 },
    {"qp", required_argument, NULL, 2 },
    {"intra_period", required_argument, NULL, 3 },
    {"fb", required_argument, NULL, 4 },
    {"lf_level", required_argument, NULL, 5 },
    {"hrd_win", required_argument, NULL, 6},
    {"vbr_max", required_argument, NULL, 7},
    {"fn_num", required_argument, NULL, 8},
    {"error_resilient", no_argument, NULL, 9},
    {"debug", no_argument, NULL, 10},
    {"temp_svc", required_argument, NULL, 11},
    {"repeat", required_argument, NULL, 12},
    {NULL, no_argument, NULL, 0 }
};


static const int default_rc_modes[4] = {
    VA_RC_CQP, // 0
    VA_RC_CBR, // 1
    VA_RC_VBR, // 2
    VA_RC_NONE
};

struct vp8enc_settings {
    int width;
    int height;
    int frame_rate;
    int frame_size;
    int loop_filter_level;
    int clamp_qindex_low;
    int clamp_qindex_high;
    int intra_period;
    int quantization_parameter;
    int frame_bitrate;
    int max_variable_bitrate;
    int rc_mode;
    int num_frames;
    VAEntrypoint vaapi_entry_point;
    int codedbuf_size;
    int hrd_window;
    int error_resilient;
    int debug;
    int temporal_svc_layers;
    int repeat_times;
};


static struct vp8enc_settings settings = {
    //Default Values - unless otherwise specified with command line options
    .frame_rate = 30,
    .loop_filter_level = 19,
    .clamp_qindex_low = 9,
    .clamp_qindex_high = 127,
    .intra_period = 30,
    .quantization_parameter = 60,
    .frame_bitrate = -1,
    .max_variable_bitrate = -1,
    .num_frames = 0,
    .rc_mode = VA_RC_CQP,
    .vaapi_entry_point = VAEntrypointEncSlice, //VAEntrypointEncSliceLP would be LowPower Mode - but not supported with VP8Encoder
    .hrd_window = 1500,
    .error_resilient = 0,
    .debug = 0,
    .temporal_svc_layers = 1,
    .repeat_times = 1,
};

struct vp8enc_vaapi_context {
    VADisplay display;
    VAProfile profile;
    VAContextID context_id;
    VAConfigID config_id;
    VAEncSequenceParameterBufferVP8 seq_param;
    VAEncPictureParameterBufferVP8 pic_param;
    VAQMatrixBufferVP8 q_matrix;
    VASurfaceID surfaces[NUM_SURFACES_TOTAL];
    VASurfaceID recon_surface, last_ref_surface, golden_ref_surface, alt_ref_surface;
    VASurfaceID input_surface;
    VABufferID codedbuf_buf_id;
    VABufferID va_buffers[NUM_BUFFERS];
    int num_va_buffers;
    int is_golden_refreshed;
    struct {
        VAEncMiscParameterBuffer header;
        VAEncMiscParameterHRD data;
    } hrd_param;
    struct {
        VAEncMiscParameterBuffer header;
        VAEncMiscParameterFrameRate data;
    } frame_rate_param;
    struct {
        VAEncMiscParameterBuffer header;
        VAEncMiscParameterRateControl data;
    } rate_control_param;
    struct {
        pthread_t id;
        int value;
        FILE *input_fp;
        int input_surface_num;
        VASurfaceID input_surface;
        int processed_frame;
    } upload_thread;
};

static struct vp8enc_vaapi_context vaapi_context;


/********************************************
*
* START: IVF Container Releated Stuff
*
********************************************/
static void
vp8enc_write_word(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
}

static void
vp8enc_write_dword(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
    *(tmp + 2) = (value >> 16) & 0XFF;
    *(tmp + 3) = (value >> 24) & 0XFF;
}

static void
vp8enc_write_qword(char *ptr, uint64_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
    *(tmp + 2) = (value >> 16) & 0XFF;
    *(tmp + 3) = (value >> 24) & 0XFF;
    *(tmp + 4) = (value >> 32) & 0XFF;
    *(tmp + 5) = (value >> 40) & 0XFF;
    *(tmp + 6) = (value >> 48) & 0XFF;
    *(tmp + 7) = (value >> 56) & 0XFF;
}

static void
vp8enc_write_frame_header(FILE *vp8_output, uint32_t data_length, uint64_t timestamp)
{
    char header[12];

    vp8enc_write_dword(header, data_length);
    vp8enc_write_qword(header + 4, timestamp);

    fwrite(header, 1, 12, vp8_output);
}

static void
vp8enc_write_ivf_header(FILE *vp8_file)
{


#define VP8_FOURCC    0x30385056

    char header[32];

    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';

    vp8enc_write_word(header + 4, 0);
    vp8enc_write_word(header + 6, 32);
    vp8enc_write_dword(header + 8, VP8_FOURCC);
    vp8enc_write_word(header + 12, settings.width);
    vp8enc_write_word(header + 14, settings.height);
    vp8enc_write_dword(header + 16, settings.frame_rate);
    vp8enc_write_dword(header + 20, 1);
    vp8enc_write_dword(header + 24, settings.num_frames * settings.repeat_times);
    vp8enc_write_dword(header + 28, 0);

    fwrite(header, 1, 32, vp8_file);
}

/********************************************
*
* END: IVF Container Releated Stuff
*
********************************************/


/********************************************
*
* START: Read YUV Input File Releated Stuff
*
********************************************/
static void
vp8enc_upload_yuv_to_surface(FILE *yuv_fp, VASurfaceID surface_id, int current_frame)
{
    VAImage surface_image;
    VAStatus va_status;
    void *surface_p = NULL;
    uint8_t *y_src, *u_src, *v_src;
    uint8_t *y_dst, *u_dst, *v_dst;
    int y_size = settings.width * settings.height;
    int u_size = (settings.width >> 1) * (settings.height >> 1);
    int row, col;
    char *yuv_mmap_ptr = NULL;
    unsigned long long frame_start_pos, mmap_start;
    int mmap_size;

    frame_start_pos = (unsigned long long)current_frame * settings.frame_size;

    mmap_start = frame_start_pos & (~0xfff);
    mmap_size = (settings.frame_size + (frame_start_pos & 0xfff) + 0xfff) & (~0xfff);
    yuv_mmap_ptr = mmap(0, mmap_size, PROT_READ, MAP_SHARED,
                        fileno(yuv_fp), mmap_start);

    if (yuv_mmap_ptr == MAP_FAILED) {
        fprintf(stderr, "Error: Failed to mmap YUV file.\n");
        assert(0);
    }

    y_src = (uint8_t*)yuv_mmap_ptr + (frame_start_pos & 0xfff);
    u_src = y_src + y_size; /* UV offset for NV12 */
    v_src = y_src + y_size + u_size;


    va_status = vaDeriveImage(vaapi_context.display, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    vaMapBuffer(vaapi_context.display, surface_image.buf, &surface_p);
    assert(VA_STATUS_SUCCESS == va_status);


    y_dst = surface_p + surface_image.offsets[0];
    u_dst = surface_p + surface_image.offsets[1]; /* UV offset for NV12 */
    v_dst = surface_p + surface_image.offsets[2];

    /* Y plane */
    for (row = 0; row < surface_image.height; row++) {
        memcpy(y_dst, y_src, surface_image.width);
        y_dst += surface_image.pitches[0];
        y_src += settings.width;
    }

    if (surface_image.format.fourcc == VA_FOURCC_NV12) { /* UV plane */
        for (row = 0; row < surface_image.height / 2; row++) {
            for (col = 0; col < surface_image.width / 2; col++) {
                u_dst[col * 2] = u_src[col];
                u_dst[col * 2 + 1] = v_src[col];
            }

            u_dst += surface_image.pitches[1];
            u_src += (settings.width / 2);
            v_src += (settings.width / 2);
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
            u_src += (settings.width / 2);
            v_src += (settings.width / 2);
        }
    }

    vaUnmapBuffer(vaapi_context.display, surface_image.buf);
    vaDestroyImage(vaapi_context.display, surface_image.image_id);

    if (yuv_mmap_ptr)
        munmap(yuv_mmap_ptr, mmap_size);
}

static void *
vp8enc_upload_thread_function(void *data)
{
    vp8enc_upload_yuv_to_surface(vaapi_context.upload_thread.input_fp, vaapi_context.upload_thread.input_surface, vaapi_context.upload_thread.processed_frame);

    return NULL;
}
/********************************************
*
* END: Read YUV Input File Releated Stuff
*
********************************************/

void vp8enc_init_QMatrix(VAQMatrixBufferVP8 *qMatrix)
{
    // When segmentation is disabled, only quantization_index[0] will be used
    size_t i;
    for (i = 0; i < N_ELEMENTS(qMatrix->quantization_index); i++) {
        qMatrix->quantization_index[i] = settings.quantization_parameter;
    }

    for (i = 0; i < N_ELEMENTS(qMatrix->quantization_index_delta); i++) {
        qMatrix->quantization_index_delta[i] = 0;
    }
}

void vp8enc_init_SequenceParameterBuffer(VAEncSequenceParameterBufferVP8* seqParam)
{
    size_t i;

    memset(seqParam, 0, sizeof(VAEncSequenceParameterBufferVP8));

    seqParam->frame_width = settings.width;
    seqParam->frame_height = settings.height;

    if (settings.frame_bitrate > 0)
        seqParam->bits_per_second = settings.frame_bitrate * 1000;
    else
        seqParam->bits_per_second = 0;

    seqParam->intra_period = settings.intra_period;
    seqParam->error_resilient = settings.error_resilient;

    for (i = 0; i < N_ELEMENTS(seqParam->reference_frames); i++)
        seqParam->reference_frames[i] = VA_INVALID_ID;
}

void vp8enc_init_PictureParameterBuffer(VAEncPictureParameterBufferVP8 *picParam)
{
    size_t i;
    memset(picParam, 0, sizeof(VAEncPictureParameterBufferVP8));

    picParam->ref_last_frame = VA_INVALID_SURFACE;
    picParam->ref_gf_frame = VA_INVALID_SURFACE;
    picParam->ref_arf_frame = VA_INVALID_SURFACE;

    /* always show it */
    picParam->pic_flags.bits.show_frame = 1;

    for (i = 0; i < N_ELEMENTS(picParam->loop_filter_level); i++) {
        picParam->loop_filter_level[i] = settings.loop_filter_level;
    }

    picParam->clamp_qindex_low = settings.clamp_qindex_low;
    picParam->clamp_qindex_high = settings.clamp_qindex_high;

}

void vp8enc_set_refreshparameter_for_svct_2layers(VAEncPictureParameterBufferVP8 *picParam, int current_frame, int *is_golden_refreshed)
{
    //Pattern taken from libyami

    picParam->ref_flags.bits.no_ref_arf = 1;

    if (! *is_golden_refreshed)
        picParam->ref_flags.bits.no_ref_gf = 1;

    switch (current_frame % 2) {
    case 0:
        //Layer 0
        picParam->pic_flags.bits.refresh_last = 1;
        picParam->ref_flags.bits.no_ref_gf = 1;
        picParam->ref_flags.bits.temporal_id = 0;
        break;
    case 1:
        //Layer 1
        picParam->pic_flags.bits.refresh_golden_frame = 1;
        *is_golden_refreshed = 1;
        picParam->ref_flags.bits.temporal_id = 1;
        break;
    }
}

void vp8enc_set_refreshparameter_for_svct_3layers(VAEncPictureParameterBufferVP8 *picParam, int current_frame, int *is_golden_refreshed)
{
    //Pattern taken from libyami - Note that the alternate frame is never referenced,
    //this is because, libyami implementation suggests to be able to drop individual
    //frames from Layer 2 on bad network connections
    picParam->ref_flags.bits.no_ref_arf = 1;

    if (! *is_golden_refreshed)
        picParam->ref_flags.bits.no_ref_gf = 1;

    switch (current_frame % 4) {
    case 0:
        //Layer 0
        picParam->pic_flags.bits.refresh_last = 1;
        picParam->ref_flags.bits.no_ref_gf = 1;
        picParam->ref_flags.bits.temporal_id = 0;
        break;
    case 1:
    case 3:
        //Layer 2
        picParam->pic_flags.bits.refresh_alternate_frame  = 1;
        picParam->ref_flags.bits.temporal_id = 2;
        break;
    case 2:
        //Layer 1
        picParam->pic_flags.bits.refresh_golden_frame = 1;
        *is_golden_refreshed = 1;
        picParam->ref_flags.bits.temporal_id = 1;
        break;
    }
}

void vp8enc_reset_picture_parameter_references(VAEncPictureParameterBufferVP8 *picParam)
{
    picParam->ref_last_frame = VA_INVALID_SURFACE;
    picParam->ref_gf_frame = VA_INVALID_SURFACE;
    picParam->ref_arf_frame = VA_INVALID_SURFACE;
    picParam->pic_flags.bits.refresh_last = 0;
    picParam->pic_flags.bits.refresh_golden_frame = 0;
    picParam->pic_flags.bits.refresh_alternate_frame = 0;
    picParam->pic_flags.bits.copy_buffer_to_golden = 0;
    picParam->pic_flags.bits.copy_buffer_to_alternate = 0;
    picParam->ref_flags.bits.no_ref_last = 0;
    picParam->ref_flags.bits.no_ref_gf = 0;
    picParam->ref_flags.bits.no_ref_arf = 0;
}

void vp8enc_update_picture_parameter(int frame_type, int current_frame)
{
    VAEncPictureParameterBufferVP8 *picParam = &vaapi_context.pic_param;

    picParam->reconstructed_frame = vaapi_context.recon_surface;

    vp8enc_reset_picture_parameter_references(picParam);

    if (frame_type == KEY_FRAME) {
        picParam->ref_flags.bits.force_kf = 1;
        picParam->pic_flags.bits.frame_type = KEY_FRAME;
        vaapi_context.is_golden_refreshed = 0;
        return;
    }

    // INTER_FRAME
    picParam->ref_flags.bits.force_kf = 0;
    picParam->pic_flags.bits.frame_type = INTER_FRAME;

    switch (settings.temporal_svc_layers) {
    case 1:
        //Standard behavoir only 1 Temporal Layer
        picParam->pic_flags.bits.refresh_last = 1;
        picParam->pic_flags.bits.copy_buffer_to_golden = 1;
        picParam->pic_flags.bits.copy_buffer_to_alternate = 2;
        picParam->ref_flags.bits.temporal_id = 0;
        break;
    case 2:
        //2 Temporal Layers
        vp8enc_set_refreshparameter_for_svct_2layers(picParam, current_frame, &vaapi_context.is_golden_refreshed);
        break;
    case 3:
        //3 Temporal Layers
        vp8enc_set_refreshparameter_for_svct_3layers(picParam, current_frame, &vaapi_context.is_golden_refreshed);
        break;
    default:
        //should never happen
        fprintf(stderr, "Error: Only 1,2 or 3 TemporalLayers supported.\n");
        assert(0);
        break;
    }

    if (!picParam->ref_flags.bits.no_ref_last)
        picParam->ref_last_frame = vaapi_context.last_ref_surface;
    if (!picParam->ref_flags.bits.no_ref_gf)
        picParam->ref_gf_frame = vaapi_context.golden_ref_surface;
    if (!picParam->ref_flags.bits.no_ref_arf)
        picParam->ref_arf_frame = vaapi_context.alt_ref_surface;

}

VASurfaceID vp8enc_get_unused_surface()
{
    VASurfaceID current_surface;
    size_t i = 0;

    for (i = 0; i < NUM_REF_SURFACES; i++) {
        current_surface = vaapi_context.surfaces[i];

        if (current_surface != vaapi_context.last_ref_surface && current_surface != vaapi_context.golden_ref_surface && current_surface != vaapi_context.alt_ref_surface)
            return current_surface;
    }

    //No unused surface found - should never happen.
    fprintf(stderr, "Error: No unused surface found!\n");
    assert(0);

}

VASurfaceID vp8enc_update_reference(VASurfaceID current_surface, VASurfaceID second_copy_surface, bool refresh_with_recon, int copy_flag)
{
    if (refresh_with_recon)
        return vaapi_context.recon_surface;
    switch (copy_flag) {
    case 0:
        return current_surface;
    case 1:
        return vaapi_context.last_ref_surface;
    case 2:
        return second_copy_surface;
    default: // should never happen
        fprintf(stderr, "Error: Invalid copy_buffer_to_X flag\n");
        assert(0);
    }

    return VA_INVALID_ID; // should never happen
}


void vp8enc_update_reference_list(int frame_type)
{

    VAEncPictureParameterBufferVP8 *picParam = &vaapi_context.pic_param;

    if (frame_type == KEY_FRAME) {
        vaapi_context.last_ref_surface = vaapi_context.recon_surface;
        vaapi_context.golden_ref_surface = vaapi_context.recon_surface;
        vaapi_context.alt_ref_surface = vaapi_context.recon_surface;
    } else { // INTER_FRAME
        //check refresh_X and copy_buffer_to_golden_X and update references accordingly
        if (picParam->pic_flags.bits.refresh_last)
            vaapi_context.last_ref_surface = vaapi_context.recon_surface;
        vaapi_context.golden_ref_surface = vp8enc_update_reference(vaapi_context.golden_ref_surface, vaapi_context.alt_ref_surface, picParam->pic_flags.bits.refresh_golden_frame, picParam->pic_flags.bits.copy_buffer_to_golden);
        vaapi_context.alt_ref_surface = vp8enc_update_reference(vaapi_context.alt_ref_surface, vaapi_context.golden_ref_surface, picParam->pic_flags.bits.refresh_alternate_frame, picParam->pic_flags.bits.copy_buffer_to_alternate);
    }

    vaapi_context.recon_surface = vp8enc_get_unused_surface();
}

void vp8enc_init_MiscParameterBuffers(VAEncMiscParameterHRD *hrd, VAEncMiscParameterFrameRate *frame_rate, VAEncMiscParameterRateControl *rate_control)
{
    if (hrd != NULL) {
        if (settings.frame_bitrate) {
            hrd->initial_buffer_fullness = settings.frame_bitrate * settings.hrd_window / 2;
            hrd->buffer_size = settings.frame_bitrate * settings.hrd_window;
        } else {
            hrd->initial_buffer_fullness = 0;
            hrd->buffer_size = 0;
        }
    }

    if (frame_rate != NULL) {
        frame_rate->framerate = settings.frame_rate;
    }

    if (rate_control != NULL) {
        rate_control->window_size = settings.hrd_window;
        rate_control->initial_qp = settings.quantization_parameter;
        rate_control->min_qp = settings.clamp_qindex_low;
        //rate_control->rc_flags.bits.disable_bit_stuffing = 1;

        if (settings.rc_mode == VA_RC_VBR) {
            rate_control->bits_per_second = settings.max_variable_bitrate * 1000;
            rate_control->target_percentage = (settings.frame_bitrate * 100) / settings.max_variable_bitrate;
        } else {
            rate_control->bits_per_second = settings.frame_bitrate * 1000;
            rate_control->target_percentage = 95;

        }
    }
}

void vp8enc_create_EncoderPipe()
{
    VAEntrypoint entrypoints[5];
    int num_entrypoints;
    VAConfigAttrib conf_attrib[2];
    VASurfaceAttrib surface_attrib;
    int major_ver, minor_ver;
    VAStatus va_status;

    vaapi_context.display = va_open_display();
    va_status = vaInitialize(vaapi_context.display, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");

    vaQueryConfigEntrypoints(vaapi_context.display, vaapi_context.profile, entrypoints,
                             &num_entrypoints);

    /* find out the format for the render target, and rate control mode */
    conf_attrib[0].type = VAConfigAttribRTFormat;
    conf_attrib[1].type = VAConfigAttribRateControl;
    vaGetConfigAttributes(vaapi_context.display, vaapi_context.profile, settings.vaapi_entry_point,
                          &conf_attrib[0], 2);

    if ((conf_attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        fprintf(stderr, "Error: Input colorspace YUV420 not supported, exit\n");
        assert(0);
    }

    if ((conf_attrib[1].value & settings.rc_mode) == 0) {
        /* Can't find matched RC mode */
        fprintf(stderr, "Error: Can't find the desired RC mode, exit\n");
        assert(0);
    }

    conf_attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    conf_attrib[1].value = settings.rc_mode; /* set to desired RC mode */

    va_status = vaCreateConfig(vaapi_context.display, vaapi_context.profile, settings.vaapi_entry_point,
                               &conf_attrib[0], 2, &vaapi_context.config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = VA_FOURCC_NV12;

    // Create surface (Reference Surfaces + Input Surfaces)
    va_status = vaCreateSurfaces(
                    vaapi_context.display,
                    VA_RT_FORMAT_YUV420, settings.width, settings.height,
                    vaapi_context.surfaces, NUM_SURFACES_TOTAL,
                    &surface_attrib, 1
                );

    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    vaapi_context.recon_surface = vaapi_context.surfaces[0];
    vaapi_context.last_ref_surface = VA_INVALID_SURFACE;
    vaapi_context.golden_ref_surface = VA_INVALID_SURFACE;
    vaapi_context.alt_ref_surface = VA_INVALID_SURFACE;
    vaapi_context.input_surface = vaapi_context.surfaces[NUM_REF_SURFACES]; // input surfaces trail the reference surfaces

    /* Create a context for this Encoder pipe */
    /* the surface is added to the render_target list when creating the context */
    va_status = vaCreateContext(vaapi_context.display, vaapi_context.config_id,
                                settings.width, settings.height,
                                VA_PROGRESSIVE,
                                vaapi_context.surfaces, NUM_SURFACES_TOTAL,
                                &vaapi_context.context_id);

    CHECK_VASTATUS(va_status, "vaCreateContext");


}

void vp8enc_destory_EncoderPipe()
{
    pthread_join(vaapi_context.upload_thread.id, NULL);
    vaDestroySurfaces(vaapi_context.display, vaapi_context.surfaces, NUM_SURFACES_TOTAL);
    vaDestroyContext(vaapi_context.display, vaapi_context.context_id);
    vaDestroyConfig(vaapi_context.display, vaapi_context.config_id);
    vaTerminate(vaapi_context.display);
    va_close_display(vaapi_context.display);
}


void vp8enc_init_VaapiContext()
{
    size_t i;
    vaapi_context.profile = VAProfileVP8Version0_3;

    vp8enc_init_SequenceParameterBuffer(&vaapi_context.seq_param);
    vp8enc_init_PictureParameterBuffer(&vaapi_context.pic_param);
    vp8enc_init_QMatrix(&vaapi_context.q_matrix);

    vaapi_context.hrd_param.header.type = VAEncMiscParameterTypeHRD;
    vaapi_context.frame_rate_param.header.type = VAEncMiscParameterTypeFrameRate;
    vaapi_context.rate_control_param.header.type = VAEncMiscParameterTypeRateControl;
    vp8enc_init_MiscParameterBuffers(&vaapi_context.hrd_param.data, &vaapi_context.frame_rate_param.data, &vaapi_context.rate_control_param.data);

    for (i = 0; i < N_ELEMENTS(vaapi_context.va_buffers); i++)
        vaapi_context.va_buffers[i] = VA_INVALID_ID;
    vaapi_context.num_va_buffers = 0;

    vaapi_context.is_golden_refreshed = 0;
}



static int
vp8enc_store_coded_buffer(FILE *vp8_fp, uint64_t timestamp)
{
    VACodedBufferSegment *coded_buffer_segment;
    uint8_t *coded_mem;
    int data_length;
    VAStatus va_status;
    VASurfaceStatus surface_status;
    size_t w_items;

    va_status = vaSyncSurface(vaapi_context.display, vaapi_context.recon_surface);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    surface_status = 0;
    va_status = vaQuerySurfaceStatus(vaapi_context.display, vaapi_context.recon_surface, &surface_status);
    CHECK_VASTATUS(va_status, "vaQuerySurfaceStatus");

    va_status = vaMapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id, (void **)(&coded_buffer_segment));
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    coded_mem = coded_buffer_segment->buf;

    if (coded_buffer_segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) {
        fprintf(stderr, "Error: CodeBuffer Size too small\n");
        vaUnmapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);
        assert(0);
    }

    data_length = coded_buffer_segment->size;

    vp8enc_write_frame_header(vp8_fp, data_length, timestamp);

    do {
        w_items = fwrite(coded_mem, data_length, 1, vp8_fp);
    } while (w_items != 1);

    if (settings.debug)
        fprintf(stderr, "Timestamp: %ld Bytes written %d\n", timestamp, data_length);

    vaUnmapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);

    return 0;
}

size_t vp8enc_get_FileSize(FILE *fp)
{
    struct stat st;
    int ret = fstat(fileno(fp), &st);
    CHECK_CONDITION(ret == 0);
    return st.st_size;
}

int vp8enc_prepare_buffers(int frame_type)
{
    int num_buffers = 0;
    VABufferID *va_buffers;
    VAStatus va_status;
    VAEncPictureParameterBufferVP8 *picParam = &vaapi_context.pic_param;


    va_buffers = vaapi_context.va_buffers;
    /* coded buffer */
    va_status = vaCreateBuffer(vaapi_context.display,
                               vaapi_context.context_id,
                               VAEncCodedBufferType,
                               settings.codedbuf_size, 1, NULL,
                               &vaapi_context.codedbuf_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    /* sequence parameter set */
    va_status = vaCreateBuffer(vaapi_context.display,
                               vaapi_context.context_id,
                               VAEncSequenceParameterBufferType,
                               sizeof(vaapi_context.seq_param), 1, &vaapi_context.seq_param,
                               va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_buffers ++;
    num_buffers++;

    /* picture parameter set */
    picParam->coded_buf = vaapi_context.codedbuf_buf_id;

    va_status = vaCreateBuffer(vaapi_context.display,
                               vaapi_context.context_id,
                               VAEncPictureParameterBufferType,
                               sizeof(vaapi_context.pic_param), 1, &vaapi_context.pic_param,
                               va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");
    va_buffers ++;
    num_buffers++;



    /* hrd parameter */
    va_status = vaCreateBuffer(vaapi_context.display,
                   vaapi_context.context_id,
                   VAEncMiscParameterBufferType,
                   sizeof(vaapi_context.hrd_param), 1, &vaapi_context.hrd_param,
                   va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_buffers ++;
    num_buffers++;

    /* QMatrix */
    va_status = vaCreateBuffer(vaapi_context.display,
                               vaapi_context.context_id,
                               VAQMatrixBufferType,
                               sizeof(vaapi_context.q_matrix), 1, &vaapi_context.q_matrix,
                               va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");


    va_buffers ++;
    num_buffers++;
    /* Create the Misc FR/RC buffer under non-CQP mode */
    if (settings.rc_mode != VA_RC_CQP && frame_type == KEY_FRAME) {
        va_status = vaCreateBuffer(vaapi_context.display,
                       vaapi_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(vaapi_context.frame_rate_param), 1, &vaapi_context.frame_rate_param,
                       va_buffers);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_buffers ++;
        num_buffers++;

        va_status = vaCreateBuffer(vaapi_context.display,
                       vaapi_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(vaapi_context.rate_control_param), 1, &vaapi_context.rate_control_param,
                       va_buffers);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_buffers ++;
        num_buffers++;
    }

    vaapi_context.num_va_buffers = num_buffers;

    return num_buffers;
}




static void
vp8enc_render_picture()
{
    VAStatus va_status;

    va_status = vaBeginPicture(vaapi_context.display,
                               vaapi_context.context_id,
                               vaapi_context.input_surface);
    CHECK_VASTATUS(va_status, "vaBeginPicture");


    va_status = vaRenderPicture(vaapi_context.display,
                                vaapi_context.context_id,
                                vaapi_context.va_buffers,
                                vaapi_context.num_va_buffers);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(vaapi_context.display, vaapi_context.context_id);
    CHECK_VASTATUS(va_status, "vaEndPicture");

}

void vp8enc_destroy_buffers()
{
    int i;
    VAStatus va_status;

    for (i = 0; i < vaapi_context.num_va_buffers; i++) {
        if (vaapi_context.va_buffers[i] != VA_INVALID_ID) {
            va_status = vaDestroyBuffer(vaapi_context.display, vaapi_context.va_buffers[i]);
            CHECK_VASTATUS(va_status, "vaDestroyBuffer");
            vaapi_context.va_buffers[i] = VA_INVALID_ID;
        }
    }

    if (vaapi_context.codedbuf_buf_id != VA_INVALID_ID) {
        va_status = vaDestroyBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);
        CHECK_VASTATUS(va_status, "vaDestroyBuffer");
        vaapi_context.codedbuf_buf_id = VA_INVALID_ID;
    }

}
void vp8enc_show_help()
{
    printf("Usage: vp8enc <width> <height> <input_yuvfile> <output_vp8> additional_option\n");
    printf("output_vp8 should use *.ivf\n");
    printf("The additional option is listed\n");
    printf("-f <frame rate> \n");
    printf("--intra_period <key_frame interval>\n");
    printf("--qp <quantization parameter> \n");
    printf("--rcmode <rate control mode> 0: CQP, 1: CBR, 2: VBR\n");
    printf("--fb <bitrate> (kbps unit)\n");
    printf("--lf_level <loop filter level>  [0-63]\n");
    printf("--hrd_win <num>  [1000-8000]\n");
    printf("--vbr_max <num> (kbps unit. It should be greater than fb)\n");
    printf("--fn_num <num>\n  how many frames to be encoded\n");
    printf("--error_resilient Turn on Error resilient mode\n");
    printf("--debug Turn debug info on\n");
    printf("--temp_svc <num> Number of temporal layers 2 or 3\n");
    printf("--repeat <num> Number of times to repeat the encoding\n");
}

void parameter_check(const char *param, int val, int min, int max)
{
    if (val < min || val > max) {
        fprintf(stderr, "Error: %s out of range (%d..%d) \n", param, min, max);
        exit(VP8ENC_FAIL);
    }
}

void parameter_check_positive(const char *param, int val, int min)
{
    if (val < 1) {
        fprintf(stderr, "Error: %s demands a positive value greater than %d \n", param, min);
        exit(VP8ENC_FAIL);
    }
}

int parse_options(int ac, char *av[])
{
    int c, long_index, tmp_input;
    while (1) {
        c = getopt_long_only(ac, av, "hf:?", long_opts, &long_index);

        if (c == -1)
            break;

        switch (c) {
        case 'f':
            tmp_input = atoi(optarg);
            parameter_check_positive("-f", tmp_input, 1);
            settings.frame_rate = tmp_input;
            break;
        case 1:
            tmp_input = atoi(optarg);
            parameter_check("--rcmode", tmp_input, 0, 2);
            settings.rc_mode = default_rc_modes[tmp_input];
            break;
        case 2:
            tmp_input = atoi(optarg);
            parameter_check("--qp", tmp_input, 0, 255);
            settings.quantization_parameter = tmp_input;
            break;
        case 3:
            tmp_input = atoi(optarg);
            parameter_check_positive("--intra_period", tmp_input, 1);
            settings.intra_period = tmp_input;
            break;
        case 4:
            tmp_input = atoi(optarg);
            parameter_check_positive("--fb", tmp_input, 1);
            settings.frame_bitrate = tmp_input;
            break;
        case 5:
            tmp_input = atoi(optarg);
            parameter_check("--lf_level", tmp_input, 0, 63);
            settings.loop_filter_level = tmp_input;
            break;
        case 6:
            tmp_input = atoi(optarg);
            parameter_check("--hrd_win", tmp_input, 1000, 8000);
            settings.hrd_window = tmp_input;
            break;
        case 7:
            tmp_input = atoi(optarg);
            parameter_check_positive("--vbr_max", tmp_input, 1);
            settings.max_variable_bitrate = tmp_input;
            break;
        case 8:
            tmp_input = atoi(optarg);
            parameter_check_positive("--fn_num", tmp_input, 1);
            settings.num_frames = tmp_input;
            break;
        case 9:
            settings.error_resilient = 1;
            break;
        case 10:
            settings.debug = 1;
            break;
        case 11:
            tmp_input = atoi(optarg);
            parameter_check("--temp_svc", tmp_input, 2, 3);
            settings.temporal_svc_layers = tmp_input;
            break;
        case 12:
            tmp_input = atoi(optarg);
            parameter_check("--repeat", tmp_input, 1, 1000000);
            settings.repeat_times = tmp_input;
            break;
        case 'h':
        case 0:
        default:
            return PARSE_OPTIONS_FAIL;
            break;
        }
    }
    return PARSE_OPTIONS_OK;
}

int main(int argc, char *argv[])
{
    int current_frame, frame_type;
    FILE *fp_vp8_output = NULL;
    FILE *fp_yuv_input = NULL;
    uint64_t timestamp;
    struct timeval t1, t2;
    double fps, elapsed_time;


    if (argc < 5) {
        vp8enc_show_help();
        return VP8ENC_FAIL;
    }

    if (parse_options(argc - 4, &argv[4]) != PARSE_OPTIONS_OK) {
        vp8enc_show_help();
        return VP8ENC_FAIL;
    }

    settings.width = atoi(argv[1]);
    parameter_check("Width", settings.width, 16, MAX_XY_RESOLUTION);

    settings.height = atoi(argv[2]);
    parameter_check("Height", settings.height, 16, MAX_XY_RESOLUTION);

    if (settings.rc_mode == VA_RC_VBR && settings.max_variable_bitrate < settings.frame_bitrate) {
        fprintf(stderr, "Error: max. variable bitrate should be greater than frame bitrate (--vbr_max >= --fb)\n");
        return VP8ENC_FAIL;
    }

    if (argv[3]) {
        fp_yuv_input = fopen(argv[3], "rb");
    }
    if (fp_yuv_input == NULL) {
        fprintf(stderr, "Error: Couldn't open input file.\n");
        return VP8ENC_FAIL;
    }
    vaapi_context.upload_thread.input_fp = fp_yuv_input;

    fp_vp8_output = fopen(argv[4], "wb");
    if (fp_vp8_output == NULL) {
        fprintf(stderr, "Error: Couldn't open output file.\n");
        return VP8ENC_FAIL;
    }

    if (settings.temporal_svc_layers == 2 && settings.intra_period % 2)
        fprintf(stderr, "Warning: Choose Key-Frame interval (--intra_period) to be integer mutliply of 2 to match temporal layer pattern");

    if (settings.temporal_svc_layers == 3 && settings.intra_period % 4)
        fprintf(stderr, "Warning: Choose Key-Frame interval (--intra_period) to be integer mutliply of 4 to match temporal layer pattern");


    settings.frame_size = settings.width * settings.height * 3 / 2; //NV12 Colorspace - For a 2x2 group of pixels, you have 4 Y samples and 1 U and 1 V sample.
    if (!settings.num_frames)
        settings.num_frames = vp8enc_get_FileSize(fp_yuv_input) / (size_t)settings.frame_size;
    settings.codedbuf_size = settings.width * settings.height; //just a generous assumptions

    fprintf(stderr, "Info: Encoding total of %d frames.\n", settings.num_frames);

    gettimeofday(&t1, 0); //Measure Runtime

    vp8enc_init_VaapiContext();
    vp8enc_create_EncoderPipe();

    vp8enc_write_ivf_header(fp_vp8_output);

    current_frame = 0;
    timestamp = 0;
    vaapi_context.input_surface = vaapi_context.surfaces[SID_INPUT_PICTURE_0];
    vaapi_context.upload_thread.input_surface_num = SID_INPUT_PICTURE_0;

    while (current_frame < settings.num_frames * settings.repeat_times) {
        fprintf(stderr, "\rProcessing frame: %d", current_frame);

        if ((current_frame % settings.intra_period) == 0)
            frame_type = KEY_FRAME;
        else
            frame_type = INTER_FRAME;

        if (current_frame == 0) {
            // Preload first input_surface
            vp8enc_upload_yuv_to_surface(fp_yuv_input, vaapi_context.input_surface, current_frame); //prefill
        } else {
            // wait for input processing thread to finish
            pthread_join(vaapi_context.upload_thread.id, NULL);
            vaapi_context.input_surface = vaapi_context.upload_thread.input_surface;
        }

        // Start Upload thread
        if ((current_frame + 1) < settings.num_frames * settings.repeat_times) {
            vaapi_context.upload_thread.processed_frame = (current_frame % settings.num_frames - 1) + 1;

            if (vaapi_context.upload_thread.input_surface_num == SID_INPUT_PICTURE_0)
                vaapi_context.upload_thread.input_surface_num = SID_INPUT_PICTURE_1;
            else
                vaapi_context.upload_thread.input_surface_num = SID_INPUT_PICTURE_0;

            vaapi_context.upload_thread.input_surface = vaapi_context.surfaces[vaapi_context.upload_thread.input_surface_num];
            vaapi_context.upload_thread.value = pthread_create(&vaapi_context.upload_thread.id, NULL, vp8enc_upload_thread_function, NULL);
        }


        vp8enc_update_picture_parameter(frame_type, current_frame);
        vp8enc_prepare_buffers(frame_type);

        vp8enc_render_picture();

        vp8enc_store_coded_buffer(fp_vp8_output, timestamp);
        vp8enc_destroy_buffers();

        vp8enc_update_reference_list(frame_type);

        current_frame ++;
        timestamp ++;
    }

    vp8enc_destory_EncoderPipe();
    fclose(fp_vp8_output);
    fclose(fp_yuv_input);

    gettimeofday(&t2, 0);
    elapsed_time = (double)(t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec) / 1000000.0;
    fps = (double)current_frame / elapsed_time;

    fprintf(stderr, "\nProcessed %d frames in %.0f ms (%.2f FPS)\n", current_frame, elapsed_time * 1000.0, fps);

    return VP8ENC_OK;
}
