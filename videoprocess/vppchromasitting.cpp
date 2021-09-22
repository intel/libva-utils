/*
* Copyright (c) 2009-2018, Intel Corporation
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
 * This test covers chromasitting feature.
 * Usage: ./vppchromasitting process_chromasitting.cfg
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
      fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
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

static uint32_t g_in_pic_width = 352;
static uint32_t g_in_pic_height = 288;
static uint32_t g_out_pic_width = 352;
static uint32_t g_out_pic_height = 288;

static uint32_t g_in_fourcc  = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_in_format  = VA_RT_FORMAT_YUV420;
static uint32_t g_out_fourcc = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_out_format = VA_RT_FORMAT_YUV420;
static uint32_t g_src_file_fourcc = VA_FOURCC('I', '4', '2', '0');
static uint32_t g_dst_file_fourcc = VA_FOURCC('Y', 'V', '1', '2');

static uint32_t g_frame_count = 0;

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
    return va_status;
}

/* Load yuv frame to NV12/YV12/I420 surface*/
static VAStatus
upload_yuv_frame_to_yuv_surface(FILE *fp,
                                VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    void *surface_p = NULL;
    uint32_t frame_size, row, col;
    size_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
        surface_image.format.fourcc == VA_FOURCC_I420 ||
        surface_image.format.fourcc == VA_FOURCC_NV12) {

        frame_size = surface_image.width * surface_image.height * 3 / 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        if (g_src_file_fourcc == VA_FOURCC_I420) {
            u_src = newImageBuffer + surface_image.width * surface_image.height;
            v_src = newImageBuffer + surface_image.width * surface_image.height * 5 / 4;
        } else if (g_src_file_fourcc == VA_FOURCC_YV12) {
            v_src = newImageBuffer + surface_image.width * surface_image.height;
            u_src = newImageBuffer + surface_image.width * surface_image.height * 5 / 4;
        } else if (g_src_file_fourcc == VA_FOURCC_NV12) {
            u_src = newImageBuffer + surface_image.width * surface_image.height;
            v_src = u_src;
        } else {
            printf("Not supported YUV fourcc for input file !!!\n");
            free(newImageBuffer);
            return VA_STATUS_ERROR_INVALID_SURFACE;
        }

        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        if (surface_image.format.fourcc == VA_FOURCC_YV12) {
            v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_I420) {
            u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else {
            u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_dst = u_dst;
        }

        /* Y plane, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_dst += surface_image.pitches[0];
            y_src += surface_image.width;
        }

        /* UV plane */
        if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
            surface_image.format.fourcc == VA_FOURCC_I420) {
            for (row = 0; row < surface_image.height / 2; row ++) {
                if (g_src_file_fourcc == VA_FOURCC_I420 ||
                    g_src_file_fourcc == VA_FOURCC_YV12) {
                    memcpy(v_dst, v_src, surface_image.width / 2);
                    memcpy(u_dst, u_src, surface_image.width / 2);

                    v_src += surface_image.width / 2;
                    u_src += surface_image.width / 2;
                } else {
                    for (col = 0; col < surface_image.width / 2; col++) {
                        u_dst[col] = u_src[col * 2];
                        v_dst[col] = u_src[col * 2 + 1];
                    }

                    u_src += surface_image.width;
                    v_src = u_src;
                }

                if (surface_image.format.fourcc == VA_FOURCC_YV12) {
                    v_dst += surface_image.pitches[1];
                    u_dst += surface_image.pitches[2];
                } else {
                    v_dst += surface_image.pitches[2];
                    u_dst += surface_image.pitches[1];
                }
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            for (row = 0; row < surface_image.height / 2; row++) {
                if (g_src_file_fourcc == VA_FOURCC_I420 ||
                    g_src_file_fourcc == VA_FOURCC_YV12) {
                    for (col = 0; col < surface_image.width / 2; col++) {
                        u_dst[col * 2] = u_src[col];
                        u_dst[col * 2 + 1] = v_src[col];
                    }

                    u_src += (surface_image.width / 2);
                    v_src += (surface_image.width / 2);
                } else {
                    memcpy(u_dst, u_src, surface_image.width);
                    u_src += surface_image.width;
                    v_src = u_src;
                }

                u_dst += surface_image.pitches[1];
            }
        }
    } else if ((surface_image.format.fourcc == VA_FOURCC_YUY2 &&
                g_src_file_fourcc == VA_FOURCC_YUY2) ||
               (surface_image.format.fourcc == VA_FOURCC_UYVY &&
                g_src_file_fourcc == VA_FOURCC_UYVY)) {
        frame_size = surface_image.width * surface_image.height * 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        /* plane 0, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 2);
            y_src += surface_image.width * 2;
            y_dst += surface_image.pitches[0];
        }
    } else if ((surface_image.format.fourcc == VA_FOURCC_P010 &&
                g_src_file_fourcc == VA_FOURCC_P010) ||
               (surface_image.format.fourcc == VA_FOURCC_I010 &&
                g_src_file_fourcc == VA_FOURCC_I010)) {
        frame_size = surface_image.width * surface_image.height * 3;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        /* plane 0, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 2);
            y_src += surface_image.width * 2;
            y_dst += surface_image.pitches[0];
        }

        /* UV plane */
        if (surface_image.format.fourcc == VA_FOURCC_I010) {
            assert(g_src_file_fourcc == VA_FOURCC_I010);

            u_src = newImageBuffer + surface_image.width * surface_image.height * 2;
            v_src = newImageBuffer + surface_image.width * surface_image.height * 5 / 2;

            u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);

            for (row = 0; row < surface_image.height / 2; row++) {
                memcpy(u_dst, u_src, surface_image.width);
                memcpy(v_dst, v_src, surface_image.width);

                u_src += surface_image.width;
                v_src += surface_image.width;

                u_dst += surface_image.pitches[1];
                v_dst += surface_image.pitches[2];
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_P010) {
            assert(g_src_file_fourcc == VA_FOURCC_P010);

            u_src = newImageBuffer + surface_image.width * surface_image.height * 2;
            v_src = u_src;

            u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_dst = u_dst;

            for (row = 0; row < surface_image.height / 2; row++) {
                memcpy(u_dst, u_src, surface_image.width * 2);

                u_src += surface_image.width * 2;
                v_src = u_src;

                u_dst += surface_image.pitches[1];
                v_dst = u_dst;
            }
        }
    }  else if ((surface_image.format.fourcc == VA_FOURCC_RGBA &&
                 g_src_file_fourcc == VA_FOURCC_RGBA) ||
                (surface_image.format.fourcc == VA_FOURCC_RGBX &&
                 g_src_file_fourcc == VA_FOURCC_RGBX) ||
                (surface_image.format.fourcc == VA_FOURCC_BGRA &&
                 g_src_file_fourcc == VA_FOURCC_BGRA) ||
                (surface_image.format.fourcc == VA_FOURCC_BGRX &&
                 g_src_file_fourcc == VA_FOURCC_BGRX)) {
        frame_size = surface_image.width * surface_image.height * 4;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        /* plane 0, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 4);
            y_src += surface_image.width * 4;
            y_dst += surface_image.pitches[0];
        }
    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

/* Store NV12/YV12/I420 surface to yv12 file */
static VAStatus
store_yuv_surface_to_yv12_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    uint32_t row, col;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one YV12 file or one bmp file*/
    if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
        surface_image.format.fourcc == VA_FOURCC_I420 ||
        surface_image.format.fourcc == VA_FOURCC_NV12) {

        uint32_t y_size = surface_image.width * surface_image.height;
        uint32_t u_size = y_size / 4;

        newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
        assert(newImageBuffer);

        /* stored as YV12 format */
        y_dst = newImageBuffer;
        v_dst = newImageBuffer + y_size;
        u_dst = newImageBuffer + y_size + u_size;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        if (surface_image.format.fourcc == VA_FOURCC_YV12) {
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_I420) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = u_src;
        }

        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }

        /* UV plane copy */
        if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
            surface_image.format.fourcc == VA_FOURCC_I420) {
            for (row = 0; row < surface_image.height / 2; row ++) {
                memcpy(v_dst, v_src, surface_image.width / 2);
                memcpy(u_dst, u_src, surface_image.width / 2);

                v_dst += surface_image.width / 2;
                u_dst += surface_image.width / 2;

                if (surface_image.format.fourcc == VA_FOURCC_YV12) {
                    v_src += surface_image.pitches[1];
                    u_src += surface_image.pitches[2];
                } else {
                    v_src += surface_image.pitches[2];
                    u_src += surface_image.pitches[1];
                }
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            for (row = 0; row < surface_image.height / 2; row++) {
                for (col = 0; col < surface_image.width / 2; col++) {
                    u_dst[col] = u_src[col * 2];
                    v_dst[col] = u_src[col * 2 + 1];
                }

                u_src += surface_image.pitches[1];
                u_dst += (surface_image.width / 2);
                v_dst += (surface_image.width / 2);
            }
        }

        /* write frame to file */
        do {
            n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
        } while (n_items != 1);

    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_yuv_surface_to_i420_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    uint32_t row, col;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one i420 file */
    if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
        surface_image.format.fourcc == VA_FOURCC_I420 ||
        surface_image.format.fourcc == VA_FOURCC_NV12) {

        uint32_t y_size = surface_image.width * surface_image.height;
        uint32_t u_size = y_size / 4;

        newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
        assert(newImageBuffer);

        /* stored as YV12 format */
        y_dst = newImageBuffer;
        u_dst = newImageBuffer + y_size;
        v_dst = newImageBuffer + y_size + u_size;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        if (surface_image.format.fourcc == VA_FOURCC_YV12) {
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_I420) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = u_src;
        }

        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }

        /* UV plane copy */
        if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
            surface_image.format.fourcc == VA_FOURCC_I420) {
            for (row = 0; row < surface_image.height / 2; row ++) {
                memcpy(v_dst, v_src, surface_image.width / 2);
                memcpy(u_dst, u_src, surface_image.width / 2);

                v_dst += surface_image.width / 2;
                u_dst += surface_image.width / 2;

                if (surface_image.format.fourcc == VA_FOURCC_YV12) {
                    v_src += surface_image.pitches[1];
                    u_src += surface_image.pitches[2];
                } else {
                    v_src += surface_image.pitches[2];
                    u_src += surface_image.pitches[1];
                }
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            for (row = 0; row < surface_image.height / 2; row++) {
                for (col = 0; col < surface_image.width / 2; col++) {
                    u_dst[col] = u_src[col * 2];
                    v_dst[col] = u_src[col * 2 + 1];
                }

                u_src += surface_image.pitches[1];
                u_dst += (surface_image.width / 2);
                v_dst += (surface_image.width / 2);
            }
        }

        /* write frame to file */
        do {
            n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
        } while (n_items != 1);

    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_yuv_surface_to_nv12_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    uint32_t row, col;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one nv12 file */
    if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
        surface_image.format.fourcc == VA_FOURCC_I420 ||
        surface_image.format.fourcc == VA_FOURCC_NV12) {

        uint32_t y_size = surface_image.width * surface_image.height;

        newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
        assert(newImageBuffer);

        /* stored as YV12 format */
        y_dst = newImageBuffer;
        u_dst = v_dst = newImageBuffer + y_size;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        if (surface_image.format.fourcc == VA_FOURCC_YV12) {
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_I420) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = u_src;
        }

        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }

        /* UV plane copy */
        if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
            surface_image.format.fourcc == VA_FOURCC_I420) {
            for (row = 0; row < surface_image.height / 2; row ++) {
                for (col = 0; col < surface_image.width / 2; col++) {
                    u_dst[col * 2] = u_src[col];
                    u_dst[col * 2 + 1] = v_src[col];
                }

                u_dst += surface_image.width;

                if (surface_image.format.fourcc == VA_FOURCC_YV12) {
                    v_src += surface_image.pitches[1];
                    u_src += surface_image.pitches[2];
                } else {
                    v_src += surface_image.pitches[2];
                    u_src += surface_image.pitches[1];
                }
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            for (row = 0; row < surface_image.height / 2; row++) {
                memcpy(u_dst, u_src, surface_image.width);
                u_dst += surface_image.width;
                u_src += surface_image.pitches[1];
            }
        }

        /* write frame to file */
        do {
            n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
        } while (n_items != 1);

    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_packed_yuv_surface_to_packed_file(FILE *fp,
                                        VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *y_dst = NULL;
    uint32_t row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one YUY2 or UYVY file */
    if (surface_image.format.fourcc == VA_FOURCC_YUY2 ||
        surface_image.format.fourcc == VA_FOURCC_UYVY) {
        uint32_t frame_size = surface_image.width * surface_image.height * 2;

        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        memset(newImageBuffer, 0, frame_size);

        /* stored as YUY2 or UYVY format */
        y_dst = newImageBuffer;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        /* Plane 0 copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 2);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width * 2;
        }

        /* write frame to file */
        do {
            n_items = fwrite(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_yuv_surface_to_10bit_file(FILE *fp, VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    uint32_t row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one 10bit file */
    uint32_t y_size = surface_image.width * surface_image.height * 2;
    uint32_t u_size = y_size / 4;

    newImageBuffer = (unsigned char*)malloc(y_size * 3);
    assert(newImageBuffer);
    y_dst = newImageBuffer;

    y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

    /* Y plane copy */
    for (row = 0; row < surface_image.height; row++) {
        memcpy(y_dst, y_src, surface_image.width * 2);
        y_src += surface_image.pitches[0];
        y_dst += surface_image.width * 2;
    }

    if (surface_image.format.fourcc == VA_FOURCC_I010) {
        u_dst = newImageBuffer + y_size;
        v_dst = newImageBuffer + y_size + u_size;

        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);

        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);

            u_dst += surface_image.width;
            v_dst += surface_image.width;

            u_src += surface_image.pitches[1];
            v_src += surface_image.pitches[2];
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_P010) {
        u_dst = newImageBuffer + y_size;
        v_dst = u_dst;

        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = u_src;

        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width * 2);
            u_dst += surface_image.width * 2;
            u_src += surface_image.pitches[1];
        }
    } else {
        printf("Not supported YUV surface fourcc !!! \n");
        free(newImageBuffer);
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    /* write frame to file */
    do {
        n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
    } while (n_items != 1);

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_rgb_surface_to_rgb_file(FILE *fp, VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *y_dst = NULL;
    uint32_t frame_size, row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    frame_size = surface_image.width * surface_image.height * 4;
    newImageBuffer = (unsigned char*)malloc(frame_size);
    assert(newImageBuffer);
    y_dst = newImageBuffer;

    y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

    for (row = 0; row < surface_image.height; row++) {
        memcpy(y_dst, y_src, surface_image.width * 4);
        y_src += surface_image.pitches[0];
        y_dst += surface_image.width * 4;
    }

    /* write frame to file */
    do {
        n_items = fwrite(newImageBuffer, frame_size, 1, fp);
    } while (n_items != 1);

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus
store_yuv_surface_to_file(FILE *fp,
                          VASurfaceID surface_id)
{
    if (g_out_fourcc == VA_FOURCC_YV12 ||
        g_out_fourcc == VA_FOURCC_I420 ||
        g_out_fourcc == VA_FOURCC_NV12) {
        if (g_dst_file_fourcc == VA_FOURCC_YV12)
            return store_yuv_surface_to_yv12_file(fp, surface_id);
        else if (g_dst_file_fourcc == VA_FOURCC_I420)
            return store_yuv_surface_to_i420_file(fp, surface_id);
        else if (g_dst_file_fourcc == VA_FOURCC_NV12)
            return store_yuv_surface_to_nv12_file(fp, surface_id);
        else {
            printf("Not supported YUV fourcc for output !!!\n");
            return VA_STATUS_ERROR_INVALID_SURFACE;
        }
    } else if ((g_out_fourcc == VA_FOURCC_YUY2 &&
                g_dst_file_fourcc == VA_FOURCC_YUY2) ||
               (g_out_fourcc == VA_FOURCC_UYVY &&
                g_dst_file_fourcc == VA_FOURCC_UYVY)) {
        return store_packed_yuv_surface_to_packed_file(fp, surface_id);
    } else if ((g_out_fourcc == VA_FOURCC_I010 &&
                g_dst_file_fourcc == VA_FOURCC_I010) ||
               (g_out_fourcc == VA_FOURCC_P010 &&
                g_dst_file_fourcc == VA_FOURCC_P010)) {
        return store_yuv_surface_to_10bit_file(fp, surface_id);
    } else if ((g_out_fourcc == VA_FOURCC_RGBA &&
                g_dst_file_fourcc == VA_FOURCC_RGBA) ||
               (g_out_fourcc == VA_FOURCC_XRGB &&
                g_dst_file_fourcc == VA_FOURCC_XRGB) ||
               (g_out_fourcc == VA_FOURCC_RGBX &&
                g_dst_file_fourcc == VA_FOURCC_RGBX) ||
               (g_out_fourcc == VA_FOURCC_RGBA &&
                g_dst_file_fourcc == VA_FOURCC_BGRA) ||
               (g_out_fourcc == VA_FOURCC_BGRX &&
                g_dst_file_fourcc == VA_FOURCC_BGRX)) {
        return store_rgb_surface_to_rgb_file(fp, surface_id);
    } else {
        printf("Not supported YUV fourcc for output !!!\n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }
}

static VAStatus
chromasitting_param_init(uint8_t *in_chroma_sample_location, uint8_t *dst_chroma_sample_location)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    uint8_t  in_sample_location = 0;
    char     in_chroma_siting_mode[MAX_LEN];

    uint8_t  dst_sample_location = 0;
    char     dst_chroma_siting_mode[MAX_LEN];

    /* Read filter type */
    if (read_value_string(g_config_file_fd, "IN_CHROMA_SITTING_MODE", in_chroma_siting_mode)) {
        printf("Read IN_CHROMA_SITTING_MODE type error !\n");
        assert(0);
    }
    if (!strcmp(in_chroma_siting_mode, "UNKNOWN"))
        in_sample_location = VA_CHROMA_SITING_UNKNOWN;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_TOP_LEFT"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_TOP_CENTER"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_CENTER_LEFT"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_CENTER_CENTER"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_BOTTOM_LEFT"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(in_chroma_siting_mode, "CHROMA_SITING_BOTTOM_CENTER"))
        in_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else {
        printf("Unsupported IN_CHROMA_SITTING_MODE type :%s \n", in_chroma_siting_mode);
        return -1;
    }
    *in_chroma_sample_location = in_sample_location;

    if (read_value_string(g_config_file_fd, "DST_CHROMA_SITTING_MODE", dst_chroma_siting_mode)) {
        printf("Read DST_CHROMA_SITTING_MODE type error !\n");
        assert(0);
    }
    if (!strcmp(dst_chroma_siting_mode, "UNKNOWN"))
        dst_sample_location = VA_CHROMA_SITING_UNKNOWN;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_TOP_LEFT"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_TOP_CENTER"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_CENTER_LEFT"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_CENTER_CENTER"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_BOTTOM_LEFT"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_LEFT;
    else if (!strcmp(dst_chroma_siting_mode, "CHROMA_SITING_BOTTOM_CENTER"))
        dst_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_CENTER;
    else {
        printf("Unsupported DST_CHROMA_SITTING_MODE type :%s \n", dst_chroma_siting_mode);
        return -1;
    }
    *dst_chroma_sample_location = dst_sample_location;

    return va_status;
}

static VAStatus
video_frame_process(VASurfaceID in_surface_id,
                    VASurfaceID out_surface_id)
{
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    uint8_t in_chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;
    uint8_t dst_chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;
    chromasitting_param_init(&in_chroma_sample_location, &dst_chroma_sample_location);
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
    pipeline_param.input_color_properties.chroma_sample_location = in_chroma_sample_location;
    pipeline_param.output_color_properties.chroma_sample_location = dst_chroma_sample_location;

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

    if (pipeline_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(va_dpy, pipeline_param_buf_id);

    return va_status;
}

static VAStatus
vpp_context_create()
{
    VAStatus va_status = VA_STATUS_SUCCESS;
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
        assert(0);
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
        tfourcc = VA_FOURCC_RGBA;
    } else if (!strcmp(str, "RGBX")) {
        tfourcc = VA_FOURCC_RGBX;
    } else if (!strcmp(str, "XRGB")) {
        tfourcc = VA_FOURCC_XRGB;
    } else if (!strcmp(str, "BGRA")) {
        tfourcc = VA_FOURCC_BGRA;
    } else if (!strcmp(str, "BGRX")) {
        tfourcc = VA_FOURCC_BGRX;
    } else {
        printf("Not supported format: %s! Currently only support following format: %s\n",
               str, "YV12, I420, NV12, YUY2(YUYV), UYVY, P010, I010, RGBA, RGBX, BGRA or BGRX");
        assert(0);
    }

    if (fourcc)
        *fourcc = tfourcc;

    if (format)
        *format = tformat;

    return 0;
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

    /* Read dst frame file information */
    read_value_string(g_config_file_fd, "DST_FILE_NAME", g_dst_file_name);
    read_value_uint32(g_config_file_fd, "DST_FRAME_WIDTH", &g_out_pic_width);
    read_value_uint32(g_config_file_fd, "DST_FRAME_HEIGHT", &g_out_pic_height);
    read_value_string(g_config_file_fd, "DST_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_out_fourcc, &g_out_format);

    read_value_string(g_config_file_fd, "SRC_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_src_file_fourcc, NULL);

    read_value_string(g_config_file_fd, "DST_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_dst_file_fourcc, NULL);

    read_value_uint32(g_config_file_fd, "FRAME_SUM", &g_frame_count);

    if (g_in_pic_width != g_out_pic_width ||
        g_in_pic_height != g_out_pic_height)
        printf("Scaling will be done : from %4d x %4d to %4d x %4d \n",
               g_in_pic_width, g_in_pic_height,
               g_out_pic_width, g_out_pic_height);

    if (g_in_fourcc != g_out_fourcc)
        printf("Format conversion will be done: from %d to %d \n",
               g_in_fourcc, g_out_fourcc);

    return 0;
}

static void
print_help()
{
    printf("The app is used to test the chromasitting feature.\n");
    printf("Cmd Usage: ./vppchromasitting process_chromasitting.cfg\n");
    printf("The configure file process_chromasitting.cfg is used to configure the para.\n");
    printf("You can refer process_chromasitting.cfg.template for each para meaning and create the configure file.\n");
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
        upload_yuv_frame_to_yuv_surface(g_src_file_fd, g_in_surface_id);
        video_frame_process(g_in_surface_id, g_out_surface_id);
        store_yuv_surface_to_file(g_dst_file_fd, g_out_surface_id);
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

    if (g_src_file_fd)
        fclose(g_src_file_fd);

    if (g_dst_file_fd)
        fclose(g_dst_file_fd);

    if (g_config_file_fd)
        fclose(g_config_file_fd);

    vpp_context_destroy();

    return 0;
}
