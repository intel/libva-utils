/*
* Copyright (c) 2009-2022, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
/*
 * Video process test case based on LibVA.
 * This test covers high dynamic range tone mapping feature.
 * Usage: ./vpphdr_tm process_hdr_tm.cfg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <va/va.h>
#include <va/va_vpp.h>
#include "va_display.h"

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420 0x30323449
#endif

#define MAX_LEN   1024

#define CHECK_VASTATUS(va_status,func)                                      \
  if (va_status != VA_STATUS_SUCCESS) {                                     \
      printf("%s:%s (%d) failed,exit\n", __func__, func, __LINE__);         \
      exit(1);                                                              \
  }

static VADisplay va_dpy = NULL;
static VAContextID context_id = 0;
static VAConfigID  config_id = 0;
static VASurfaceID g_in_surface_id = VA_INVALID_ID;
static VASurfaceID g_out_surface_id = VA_INVALID_ID;

static FILE* g_config_file_fd = NULL;
static FILE* g_src_file_fd = NULL;
static FILE* g_dst_file_fd = NULL;

static char g_config_file_name[MAX_LEN];
static char g_src_file_name[MAX_LEN];
static char g_dst_file_name[MAX_LEN];

static uint32_t g_in_pic_width = 1920;
static uint32_t g_in_pic_height = 1080;
static uint32_t g_out_pic_width = 1920;
static uint32_t g_out_pic_height = 1080;

static uint32_t g_in_fourcc  = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_in_format  = VA_FOURCC_P010;
static uint32_t g_out_fourcc = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_out_format = VA_RT_FORMAT_YUV420;
static uint32_t g_src_file_fourcc = VA_FOURCC('I', '4', '2', '0');
static uint32_t g_dst_file_fourcc = VA_FOURCC('Y', 'V', '1', '2');

static uint32_t g_frame_count = 1;
// The maximum display luminace is 1000 nits by default.
static uint32_t g_in_max_display_luminance = 10000000;
static uint32_t g_in_min_display_luminance = 100;
static uint32_t g_in_max_content_luminance = 4000;
static uint32_t g_in_pic_average_luminance = 1000;
// The maximum display luminace is 1000 nits by default.
static uint32_t g_out_max_display_luminance = 10000000;
static uint32_t g_out_min_display_luminance = 100;
static uint32_t g_out_max_content_luminance = 4000;
static uint32_t g_out_pic_average_luminance = 1000;

static uint32_t g_in_colour_primaries = 9;
static uint32_t g_in_transfer_characteristic = 16;

static uint32_t g_out_colour_primaries = 9;
static uint32_t g_out_transfer_characteristic = 16;

static uint32_t g_tm_type = 1;

static int8_t
read_value_string(FILE *fp, const char* field_name, char* value)
{
    char strLine[MAX_LEN];
    char* field = NULL;
    char* str = NULL;
    uint16_t i;

    if (!fp || !field_name || !value)  {
        printf("Invalid fuction parameters\n");
        return -1;
    }

    rewind(fp);

    while (!feof(fp)) {
        if (!fgets(strLine, MAX_LEN, fp))
            continue;

        for (i = 0; i < MAX_LEN && strLine[i]; i++)
            if (strLine[i] != ' ') break;

        if (i == MAX_LEN || strLine[i] == '#' || strLine[i] == '\n')
            continue;

        field = strtok(&strLine[i], ":");
        if (strncmp(field, field_name, strlen(field_name)))
            continue;

        if (!(str = strtok(NULL, ":")))
            continue;

        /* skip blank space in string */
        while (*str == ' ')
            str++;

        *(str + strlen(str) - 1) = '\0';
        strcpy(value, str);

        return 0;
    }
    return -1;
}

static int8_t
read_value_uint32(FILE* fp, const char* field_name, uint32_t* value)
{
    char str[MAX_LEN];

    if (read_value_string(fp, field_name, str)) {
        printf("Failed to find integer field: %s", field_name);
        return -1;
    }

    *value = (uint32_t)atoi(str);

    return 0;
}

static VAStatus
create_surface(VASurfaceID * p_surface_id,
               uint32_t width, uint32_t height,
               uint32_t fourCC, uint32_t format)
{
    VAStatus va_status;
    VASurfaceAttrib    surface_attrib;
    surface_attrib.type =  VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = fourCC;

    va_status = vaCreateSurfaces(va_dpy,
                                 format,
                                 width,
                                 height,
                                 p_surface_id,
                                 1,
                                 &surface_attrib,
                                 1);

    printf("create_surface: p_surface_id %d, width %d, height %d, fourCC 0x%x, format 0x%x\n",
           *p_surface_id, width, height, fourCC, format);

    return va_status;
}

static VAStatus
hdrtm_filter_init(VABufferID *filter_param_buf_id, uint32_t tm_type)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    VAProcFilterParameterBufferHDRToneMapping hdrtm_param;

    VAHdrMetaDataHDR10 in_hdr10_metadata = {};

    // The input is HDR content
    in_hdr10_metadata.max_display_mastering_luminance = g_in_max_display_luminance;
    in_hdr10_metadata.min_display_mastering_luminance = g_in_min_display_luminance;
    in_hdr10_metadata.max_content_light_level         = g_in_max_content_luminance;
    in_hdr10_metadata.max_pic_average_light_level     = g_in_pic_average_luminance;
    in_hdr10_metadata.display_primaries_x[0] = 8500;
    in_hdr10_metadata.display_primaries_y[0] = 39850;
    in_hdr10_metadata.display_primaries_x[1] = 35400;
    in_hdr10_metadata.display_primaries_y[1] = 14600;
    in_hdr10_metadata.display_primaries_x[2] = 6550;
    in_hdr10_metadata.display_primaries_y[2] = 2300;
    in_hdr10_metadata.white_point_x = 15635;
    in_hdr10_metadata.white_point_y = 16450;

    hdrtm_param.type = VAProcFilterHighDynamicRangeToneMapping;
    hdrtm_param.data.metadata_type = VAProcHighDynamicRangeMetadataHDR10;
    hdrtm_param.data.metadata = &in_hdr10_metadata;
    hdrtm_param.data.metadata_size = sizeof(VAHdrMetaDataHDR10);

    va_status = vaCreateBuffer(va_dpy, context_id, VAProcFilterParameterBufferType, sizeof(hdrtm_param), 1, (void *)&hdrtm_param, filter_param_buf_id);

    return va_status;
}

static VAStatus
hdrtm_metadata_init(VAHdrMetaData &out_metadata, uint32_t tm_type, VAHdrMetaDataHDR10 &out_hdr10_metadata)
{
    VAStatus va_status = VA_STATUS_SUCCESS;


    out_hdr10_metadata.max_display_mastering_luminance = g_out_max_display_luminance;
    out_hdr10_metadata.min_display_mastering_luminance = g_out_min_display_luminance;
    out_hdr10_metadata.max_content_light_level         = g_out_max_content_luminance;
    out_hdr10_metadata.max_pic_average_light_level     = g_out_pic_average_luminance;
    printf("hdrtm_metadata_init g_out_max_display_luminance %d, g_out_min_display_luminance %d\n", g_out_max_display_luminance, g_out_min_display_luminance);
    printf("hdrtm_metadata_init g_out_max_content_luminance %d, g_out_pic_average_luminance %d\n", g_out_max_content_luminance, g_out_pic_average_luminance);

    // HDR display or SDR display
    switch (tm_type) {
    case VA_TONE_MAPPING_HDR_TO_HDR:
        out_hdr10_metadata.display_primaries_x[0] = 8500;
        out_hdr10_metadata.display_primaries_y[0] = 39850;
        out_hdr10_metadata.display_primaries_x[1] = 35400;
        out_hdr10_metadata.display_primaries_y[1] = 14600;
        out_hdr10_metadata.display_primaries_x[2] = 6550;
        out_hdr10_metadata.display_primaries_y[2] = 2300;
        out_hdr10_metadata.white_point_x = 15635;
        out_hdr10_metadata.white_point_y = 16450;
        break;
    case VA_TONE_MAPPING_HDR_TO_SDR:
        out_hdr10_metadata.display_primaries_x[0] = 15000;
        out_hdr10_metadata.display_primaries_y[0] = 30000;
        out_hdr10_metadata.display_primaries_x[1] = 32000;
        out_hdr10_metadata.display_primaries_y[1] = 16500;
        out_hdr10_metadata.display_primaries_x[2] = 7500;
        out_hdr10_metadata.display_primaries_y[2] = 3000;
        out_hdr10_metadata.white_point_x = 15635;
        out_hdr10_metadata.white_point_y = 16450;
        break;
    default:
        break;
    }

    out_metadata.metadata_type = VAProcHighDynamicRangeMetadataHDR10;
    out_metadata.metadata = &out_hdr10_metadata;
    out_metadata.metadata_size = sizeof(VAHdrMetaDataHDR10);

    return va_status;
}

static VAStatus
video_frame_process(VASurfaceID in_surface_id,
                    VASurfaceID out_surface_id)
{
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipeline_param = {};
    VARectangle surface_region = {}, output_region = {};
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VABufferID filter_param_buf_id = VA_INVALID_ID;
    VAHdrMetaData out_metadata = {};

    /*Query Filter's Caps: The return value will be HDR10 and H2S, H2H, H2E. */
    VAProcFilterCapHighDynamicRange hdrtm_caps[VAProcHighDynamicRangeMetadataTypeCount];
    uint32_t num_hdrtm_caps = VAProcHighDynamicRangeMetadataTypeCount;
    memset(&hdrtm_caps, 0, sizeof(VAProcFilterCapHighDynamicRange)*num_hdrtm_caps);
    va_status = vaQueryVideoProcFilterCaps(va_dpy, context_id,
                                           VAProcFilterHighDynamicRangeToneMapping,
                                           (void *)hdrtm_caps, &num_hdrtm_caps);
    CHECK_VASTATUS(va_status, "vaQueryVideoProcFilterCaps");
    printf("vaQueryVideoProcFilterCaps num_hdrtm_caps %d\n", num_hdrtm_caps);
    for (int i = 0; i < num_hdrtm_caps; ++i)    {
        printf("vaQueryVideoProcFilterCaps hdrtm_caps[%d]: metadata type %d, flag %d\n", i, hdrtm_caps[i].metadata_type, hdrtm_caps[i].caps_flag);
    }

    hdrtm_filter_init(&filter_param_buf_id, g_tm_type);
    VAHdrMetaDataHDR10 out_hdr10_metadata = {};
    hdrtm_metadata_init(out_metadata, g_tm_type, out_hdr10_metadata);

    /* Fill pipeline buffer */
    surface_region.x = 0;
    surface_region.y = 0;
    surface_region.width = g_in_pic_width;
    surface_region.height = g_in_pic_height;
    output_region.x = 0;
    output_region.y = 0;
    output_region.width = g_out_pic_width;
    output_region.height = g_out_pic_height;

    memset(&pipeline_param, 0, sizeof(pipeline_param));
    pipeline_param.surface = in_surface_id;
    pipeline_param.surface_region = &surface_region;
    pipeline_param.output_region = &output_region;
    pipeline_param.filter_flags = 0;
    pipeline_param.filters      = &filter_param_buf_id;
    pipeline_param.num_filters  = 1;
    pipeline_param.surface_color_standard = VAProcColorStandardExplicit;
    pipeline_param.input_color_properties.colour_primaries = g_in_colour_primaries;
    pipeline_param.input_color_properties.transfer_characteristics = g_in_transfer_characteristic;
    pipeline_param.output_color_standard = VAProcColorStandardExplicit;
    pipeline_param.output_color_properties.colour_primaries = g_out_colour_primaries;
    pipeline_param.output_color_properties.transfer_characteristics = g_out_transfer_characteristic;
    pipeline_param.output_hdr_metadata = &out_metadata;

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAProcPipelineParameterBufferType,
                               sizeof(pipeline_param),
                               1,
                               &pipeline_param,
                               &pipeline_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaBeginPicture(va_dpy,
                               context_id,
                               out_surface_id);
    CHECK_VASTATUS(va_status, "vaBeginPicture");

    va_status = vaRenderPicture(va_dpy,
                                context_id,
                                &pipeline_param_buf_id,
                                1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(va_dpy, context_id);
    CHECK_VASTATUS(va_status, "vaEndPicture");

    if (filter_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(va_dpy, filter_param_buf_id);

    if (pipeline_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(va_dpy, pipeline_param_buf_id);

    return va_status;
}

static VAStatus
vpp_context_create()
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    uint32_t i;
    int32_t j;

    /* VA driver initialization */
    va_dpy = va_open_display();
    int32_t major_ver, minor_ver;
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

    /* Check whether VPP is supported by driver */
    VAEntrypoint entrypoints[5];
    int32_t num_entrypoints;
    num_entrypoints = vaMaxNumEntrypoints(va_dpy);
    va_status = vaQueryConfigEntrypoints(va_dpy,
                                         VAProfileNone,
                                         entrypoints,
                                         &num_entrypoints);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    for (j = 0; j < num_entrypoints; j++) {
        if (entrypoints[j] == VAEntrypointVideoProc)
            break;
    }

    if (j == num_entrypoints) {
        printf("VPP is not supported by driver\n");
        assert(0);
    }

    /* Render target surface format check */
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    va_status = vaGetConfigAttributes(va_dpy,
                                      VAProfileNone,
                                      VAEntrypointVideoProc,
                                      &attrib,
                                      1);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    if (!(attrib.value & g_out_format)) {
        printf("RT format %d is not supported by VPP !\n", g_out_format);
        //assert(0);
    }

    /* Create surface/config/context for VPP pipeline */
    va_status = create_surface(&g_in_surface_id, g_in_pic_width, g_in_pic_height,
                               g_in_fourcc, g_in_format);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for input");

    va_status = create_surface(&g_out_surface_id, g_out_pic_width, g_out_pic_height,
                               g_out_fourcc, g_out_format);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for output");

    va_status = vaCreateConfig(va_dpy,
                               VAProfileNone,
                               VAEntrypointVideoProc,
                               &attrib,
                               1,
                               &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    va_status = vaCreateContext(va_dpy,
                                config_id,
                                g_out_pic_width,
                                g_out_pic_height,
                                VA_PROGRESSIVE,
                                &g_out_surface_id,
                                1,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");

    uint32_t supported_filter_num = VAProcFilterCount;
    VAProcFilterType supported_filter_types[VAProcFilterCount];

    va_status = vaQueryVideoProcFilters(va_dpy,
                                        context_id,
                                        supported_filter_types,
                                        &supported_filter_num);

    CHECK_VASTATUS(va_status, "vaQueryVideoProcFilters");

    for (i = 0; i < supported_filter_num; i++) {
        if (supported_filter_types[i] == VAProcFilterHighDynamicRangeToneMapping)
            break;
    }

    if (i == supported_filter_num) {
        printf("VPP filter type VAProcFilterHighDynamicRangeToneMapping is not supported by driver !\n");
    }
    return va_status;
}

static void
vpp_context_destroy()
{
    /* Release resource */
    vaDestroySurfaces(va_dpy, &g_in_surface_id, 1);
    vaDestroySurfaces(va_dpy, &g_out_surface_id, 1);
    vaDestroyContext(va_dpy, context_id);
    vaDestroyConfig(va_dpy, config_id);

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
}

static int8_t
parse_fourcc_and_format(char *str, uint32_t *fourcc, uint32_t *format)
{
    uint32_t tfourcc = VA_FOURCC('N', 'V', '1', '2');
    uint32_t tformat = VA_RT_FORMAT_YUV420;

    if (!strcmp(str, "YV12")) {
        tfourcc = VA_FOURCC('Y', 'V', '1', '2');
    } else if (!strcmp(str, "I420")) {
        tfourcc = VA_FOURCC('I', '4', '2', '0');
    } else if (!strcmp(str, "NV12")) {
        tfourcc = VA_FOURCC('N', 'V', '1', '2');
    } else if (!strcmp(str, "YUY2") || !strcmp(str, "YUYV")) {
        tfourcc = VA_FOURCC('Y', 'U', 'Y', '2');
    } else if (!strcmp(str, "UYVY")) {
        tfourcc = VA_FOURCC('U', 'Y', 'V', 'Y');
    } else if (!strcmp(str, "P010")) {
        tfourcc = VA_FOURCC('P', '0', '1', '0');
    } else if (!strcmp(str, "I010")) {
        tfourcc = VA_FOURCC('I', '0', '1', '0');
    } else if (!strcmp(str, "RGBA")) {
        tformat = VA_RT_FORMAT_RGB32;
        tfourcc = VA_FOURCC_RGBA;
        printf("parse_fourcc_and_format: RGBA format 0x%8x, fourcc 0x%8x\n", tformat, tfourcc);
    } else if (!strcmp(str, "RGBX")) {
        tfourcc = VA_FOURCC_RGBX;
    } else if (!strcmp(str, "BGRA")) {
        tfourcc = VA_FOURCC_BGRA;
    } else if (!strcmp(str, "BGRX")) {
        tfourcc = VA_FOURCC_BGRX;
    } else if (!strcmp(str, "P010")) {
        tfourcc = VA_FOURCC_P010;
        printf("parse_fourcc_and_format: P010\n");
    } else if (!strcmp(str, "A2RGB10")) {  //A2R10G10B10
        tfourcc = VA_FOURCC_A2R10G10B10;
        printf("parse_fourcc_and_format: ARGB10 format 0x%8x, fourcc 0x%8x\n", tformat, tfourcc);
    } else {
        printf("Not supported format: %s! Currently only support following format: %s\n",
               str, "YV12, I420, NV12, YUY2(YUYV), UYVY, I010, RGBA, RGBX, BGRA or BGRX");
        assert(0);
    }

    printf("parse_fourcc_and_format: format 0x%x, fourcc 0x%x\n", tformat, tfourcc);

    if (fourcc)
        *fourcc = tfourcc;

    if (format)
        *format = tformat;

    return 0;
}

bool read_frame_to_surface(FILE *fp, VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage  va_image;

    int i = 0;

    int frame_size = 0, y_size = 0;

    unsigned char *y_src = NULL, *u_src = NULL;
    unsigned char *y_dst = NULL, *u_dst = NULL;

    int bytes_per_pixel = 2;
    size_t n_items;
    void *out_buf = NULL;
    unsigned char *src_buffer = NULL;

    if (fp == NULL)
        return false;

    // This function blocks until all pending operations on the surface have been completed.
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &va_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, va_image.buf, &out_buf);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    printf("read_frame_to_surface: va_image.width %d, va_image.height %d, va_image.pitches[0]: %d, va_image.pitches[1] %d, va_image.pitches[2] %d\n",
           va_image.width, va_image.height, va_image.pitches[0], va_image.pitches[1], va_image.pitches[1]);

    switch (va_image.format.fourcc) {
    case VA_FOURCC_P010:
        frame_size = va_image.width * va_image.height * bytes_per_pixel * 3 / 2;
        y_size = va_image.width * va_image.height * bytes_per_pixel;

        src_buffer = (unsigned char*)malloc(frame_size);
        assert(src_buffer);
        n_items = fread(src_buffer, 1, frame_size, fp);
        if (n_items != frame_size) {
            printf("read file failed on VA_FOURCC_P010\n");
        }
        y_src = src_buffer;
        u_src = src_buffer + y_size; // UV offset for P010

        y_dst = (unsigned char*)out_buf + va_image.offsets[0]; // Y plane
        u_dst = (unsigned char*)out_buf + va_image.offsets[1]; // U offset for P010

        for (i = 0; i < va_image.height; i++) {
            memcpy(y_dst, y_src, va_image.width * 2);
            y_dst += va_image.pitches[0];
            y_src += va_image.width * 2;
        }
        for (i = 0; i < va_image.height >> 1; i++)  {
            memcpy(u_dst, u_src, va_image.width * 2);
            u_dst += va_image.pitches[1];
            u_src += va_image.width * 2;
        }
        printf("read_frame_to_surface: P010 \n");
        break;

    case VA_RT_FORMAT_RGB32_10BPP:
    case VA_FOURCC_RGBA:
    case VA_FOURCC_A2R10G10B10:
    case VA_FOURCC_A2B10G10R10:
        frame_size = va_image.width * va_image.height * 4;
        src_buffer = (unsigned char*)malloc(frame_size);
        assert(src_buffer);
        n_items = fread(src_buffer, 1, frame_size, fp);
        if (n_items != frame_size) {
            printf("read file failed on VA_RT_FORMAT_RGB32_10BPP or VA_FOURCC_RGBA \n");
        }
        y_src = src_buffer;
        y_dst = (unsigned char*)out_buf + va_image.offsets[0];

        for (i = 0; i < va_image.height; i++)  {
            memcpy(y_dst, y_src, va_image.width * 4);
            y_dst += va_image.pitches[0];
            y_src += va_image.width * 4;
        }
        printf("read_frame_to_surface: RGBA or A2RGB10 \n");
        break;

    default: // should not come here
        printf("VA_STATUS_ERROR_INVALID_IMAGE_FORMAT \n");
        va_status = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
        break;
    }

    vaUnmapBuffer(va_dpy, va_image.buf);
    vaDestroyImage(va_dpy, va_image.image_id);
    if (src_buffer) {
        free(src_buffer);
        src_buffer = NULL;
    }

    if (va_status != VA_STATUS_SUCCESS)
        return false;
    else
        return true;
}

bool write_surface_to_frame(FILE *fp, VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage  va_image;

    int i = 0;

    int frame_size = 0, y_size = 0;

    unsigned char *y_src = NULL, *u_src = NULL;
    unsigned char *y_dst = NULL, *u_dst = NULL;

    int bytes_per_pixel = 2;

    void *in_buf = NULL;
    unsigned char *dst_buffer = NULL;

    if (fp == NULL)
        return false;

    // This function blocks until all pending operations on the surface have been completed.
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &va_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, va_image.buf, &in_buf);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    printf("write_surface_to_frame: va_image.width %d, va_image.height %d, va_image.pitches[0]: %d, va_image.pitches[1] %d, va_image.pitches[2] %d\n",
           va_image.width, va_image.height, va_image.pitches[0], va_image.pitches[1], va_image.pitches[1]);


    switch (va_image.format.fourcc) {
    case VA_FOURCC_P010:
    case VA_FOURCC_NV12:
        bytes_per_pixel = (va_image.format.fourcc == VA_FOURCC_P010) ? 2 : 1;
        frame_size = va_image.width * va_image.height * bytes_per_pixel * 3 / 2;
        dst_buffer = (unsigned char*)malloc(frame_size);
        assert(dst_buffer);
        y_size = va_image.width * va_image.height * bytes_per_pixel;
        y_dst = dst_buffer;
        u_dst = dst_buffer + y_size; // UV offset for P010
        y_src = (unsigned char*)in_buf + va_image.offsets[0];
        u_src = (unsigned char*)in_buf + va_image.offsets[1]; // U offset for P010
        for (i = 0; i < va_image.height; i++)  {
            memcpy(y_dst, y_src, static_cast<size_t>(va_image.width * bytes_per_pixel));
            y_dst += va_image.width * bytes_per_pixel;
            y_src += va_image.pitches[0];
        }
        for (i = 0; i < va_image.height >> 1; i++)  {
            memcpy(u_dst, u_src, static_cast<size_t>(va_image.width * bytes_per_pixel));
            u_dst += va_image.width * bytes_per_pixel;
            u_src += va_image.pitches[1];
        }
        printf("read_frame_to_surface: P010 \n");
        break;

    case VA_FOURCC_RGBA:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_A2B10G10R10:
    case VA_FOURCC_A2R10G10B10:
        frame_size = va_image.width * va_image.height * 4;
        dst_buffer = (unsigned char*)malloc(frame_size);
        assert(dst_buffer);
        y_dst = dst_buffer;
        y_src = (unsigned char*)in_buf + va_image.offsets[0];

        for (i = 0; i < va_image.height; i++) {
            memcpy(y_dst, y_src, va_image.width * 4);
            y_dst += va_image.pitches[0];
            y_src += va_image.width * 4;
        }
        printf("read_frame_to_surface: RGBA and A2R10G10B10 \n");
        break;

    default: // should not come here
        printf("VA_STATUS_ERROR_INVALID_IMAGE_FORMAT %x\n", va_image.format.fourcc);
        va_status = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
        break;
    }
    assert(dst_buffer);
    fwrite(dst_buffer, 1, frame_size, fp);

    if (dst_buffer)  {
        free(dst_buffer);
        dst_buffer = NULL;
    }

    vaUnmapBuffer(va_dpy, va_image.buf);
    vaDestroyImage(va_dpy, va_image.image_id);
    if (va_status != VA_STATUS_SUCCESS)
        return false;
    else
        return true;
}

static int8_t
parse_basic_parameters()
{
    char str[MAX_LEN];

    /* Read src frame file information */
    read_value_string(g_config_file_fd, "SRC_FILE_NAME", g_src_file_name);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_WIDTH", &g_in_pic_width);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_HEIGHT", &g_in_pic_height);
    read_value_string(g_config_file_fd, "SRC_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_in_fourcc, &g_in_format);

    printf("Input file: %s, width: %d, height: %d, fourcc 0x%x, format 0x%x\n", g_src_file_name, g_in_pic_width, g_in_pic_height, g_in_fourcc, g_in_format);

    /* Read dst frame file information */
    read_value_string(g_config_file_fd, "DST_FILE_NAME", g_dst_file_name);
    read_value_uint32(g_config_file_fd, "DST_FRAME_WIDTH", &g_out_pic_width);
    read_value_uint32(g_config_file_fd, "DST_FRAME_HEIGHT", &g_out_pic_height);
    read_value_string(g_config_file_fd, "DST_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_out_fourcc, &g_out_format);

    printf("Output file: %s, width: %d, height: %d, fourcc 0x%x, format 0x%x\n", g_dst_file_name, g_out_pic_width, g_out_pic_height, g_out_fourcc, g_out_format);

    read_value_string(g_config_file_fd, "SRC_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_src_file_fourcc, NULL);

    read_value_string(g_config_file_fd, "DST_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_dst_file_fourcc, NULL);

    read_value_uint32(g_config_file_fd, "FRAME_SUM", &g_frame_count);

    read_value_uint32(g_config_file_fd, "SRC_MAX_DISPLAY_MASTERING_LUMINANCE", &g_in_max_display_luminance);
    read_value_uint32(g_config_file_fd, "SRC_MIN_DISPLAY_MASTERING_LUMINANCE", &g_in_min_display_luminance);
    read_value_uint32(g_config_file_fd, "SRC_MAX_CONTENT_LIGHT_LEVEL",         &g_in_max_content_luminance);
    read_value_uint32(g_config_file_fd, "SRC_MAX_PICTURE_AVERAGE_LIGHT_LEVEL", &g_in_pic_average_luminance);

    read_value_uint32(g_config_file_fd, "DST_MAX_DISPLAY_MASTERING_LUMINANCE", &g_out_max_display_luminance);
    read_value_uint32(g_config_file_fd, "DST_MIN_DISPLAY_MASTERING_LUMINANCE", &g_out_min_display_luminance);
    read_value_uint32(g_config_file_fd, "DST_MAX_CONTENT_LIGHT_LEVEL",         &g_out_max_content_luminance);
    read_value_uint32(g_config_file_fd, "DST_MAX_PICTURE_AVERAGE_LIGHT_LEVEL", &g_out_pic_average_luminance);

    read_value_uint32(g_config_file_fd, "SRC_FRAME_COLOUR_PRIMARIES",         &g_in_colour_primaries);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_TRANSFER_CHARACTERISTICS", &g_in_transfer_characteristic);
    read_value_uint32(g_config_file_fd, "DST_FRAME_COLOUR_PRIMARIES",         &g_out_colour_primaries);
    read_value_uint32(g_config_file_fd, "DST_FRAME_TRANSFER_CHARACTERISTICS", &g_out_transfer_characteristic);

    read_value_uint32(g_config_file_fd, "TM_TYPE", &g_tm_type);

    return 0;
}

static void
print_help()
{
    printf("The app is used to test the hdr tm feature.\n");
    printf("Cmd Usage: ./process_hdr_tm.cfg process_hdr_tm.cfg.cfg\n");
    printf("The configure file process_hdr_tm.cfg is used to configure the para.\n");
    printf("You can refer process_hdr_tm.cfg.template for each para meaning and create the configure file.\n");
}

int32_t main(int32_t argc, char *argv[])
{
    VAStatus va_status;
    uint32_t i;

    if (argc != 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        print_help();
        return -1;
    }

    /* Parse the configure file for video process*/
    strncpy(g_config_file_name, argv[1], MAX_LEN);
    g_config_file_name[MAX_LEN - 1] = '\0';

    if (NULL == (g_config_file_fd = fopen(g_config_file_name, "r"))) {
        printf("Open configure file %s failed!\n", g_config_file_name);
        assert(0);
    }

    /* Parse basic parameters */
    if (parse_basic_parameters()) {
        printf("Parse parameters in configure file error\n");
        assert(0);
    }

    va_status = vpp_context_create();
    if (va_status != VA_STATUS_SUCCESS) {
        printf("vpp context create failed \n");
        assert(0);
    }

    /* Video frame fetch, process and store */
    if (NULL == (g_src_file_fd = fopen(g_src_file_name, "r"))) {
        printf("Open SRC_FILE_NAME: %s failed, please specify it in config file: %s !\n",
               g_src_file_name, g_config_file_name);
        assert(0);
    }

    if (NULL == (g_dst_file_fd = fopen(g_dst_file_name, "w"))) {
        printf("Open DST_FILE_NAME: %s failed, please specify it in config file: %s !\n",
               g_dst_file_name, g_config_file_name);
        assert(0);
    }

    printf("\nStart to process, ...\n");
    struct timespec Pre_time;
    struct timespec Cur_time;
    unsigned int duration = 0;
    clock_gettime(CLOCK_MONOTONIC, &Pre_time);

    for (i = 0; i < g_frame_count; i ++) {
        read_frame_to_surface(g_src_file_fd, g_in_surface_id);
        video_frame_process(g_in_surface_id, g_out_surface_id);
        write_surface_to_frame(g_dst_file_fd, g_out_surface_id);
    }

    clock_gettime(CLOCK_MONOTONIC, &Cur_time);
    duration = (Cur_time.tv_sec - Pre_time.tv_sec) * 1000;
    if (Cur_time.tv_nsec > Pre_time.tv_nsec) {
        duration += (Cur_time.tv_nsec - Pre_time.tv_nsec) / 1000000;
    } else {
        duration += (Cur_time.tv_nsec + 1000000000 - Pre_time.tv_nsec) / 1000000 - 1000;
    }
    printf("Finish processing, performance: \n");
    printf("%d frames processed in: %d ms, ave time = %d ms\n", g_frame_count, duration, duration / g_frame_count);

    if (g_src_file_fd) {
        fclose(g_src_file_fd);
        g_src_file_fd = NULL;
    }

    if (g_dst_file_fd) {
        fclose(g_dst_file_fd);
        g_dst_file_fd = NULL;
    }

    if (g_config_file_fd) {
        fclose(g_config_file_fd);
        g_config_file_fd = NULL;
    }

    vpp_context_destroy();

    return 0;
}
