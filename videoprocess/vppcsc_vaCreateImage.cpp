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
 * This test covers scaling and several surface format conversion.
 * Usage: ./vppscaling_csc process_scaling_csc.cfg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <va/va.h>
#include <va/va_vpp.h>
#include "va_display.h"
#define VA_FOURCC_A2B10G10R10   0x30334241
#define VA_FOURCC_A2R10G10B10   0x30335241 /* VA_FOURCC('A','R','3','0') */

#define MAX_LEN   1024

#define CHECK_VASTATUS(va_status,func)                                      \
  if (va_status != VA_STATUS_SUCCESS) {                                     \
      fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
      exit(1);                                                              \
  }


static VADisplay va_dpy = NULL;
static VAConfigID  config_id = 0;
static VASurfaceID g_in_surface_id = VA_INVALID_ID;

static FILE* g_config_file_fd = NULL;
static FILE* g_src_file_fd = NULL;
static FILE* g_dst_file_fd = NULL;

static char g_config_file_name[MAX_LEN];
static char g_src_file_name[MAX_LEN];
static char g_dst_file_name[MAX_LEN];

static uint32_t g_in_pic_width = 352;
static uint32_t g_in_pic_height = 288;
static uint32_t g_full_format_test = 0;

static uint32_t g_in_fourcc  = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_in_format  = VA_RT_FORMAT_YUV420;
static uint32_t g_out_fourcc = VA_FOURCC('N', 'V', '1', '2');
static uint32_t g_out_format = VA_RT_FORMAT_YUV420;
std::vector<uint32_t> g_format_list;

static int8_t get_fourcc_str(uint32_t cc, char *str)
{
    if(!str){
        printf("str is null\n");
        return -1;
    }
    sprintf(str, "%c%c%c%c",cc & 0xff, (cc>>8) &0xff,(cc >> 16) & 0xff,(cc >> 24) & 0xff);
    return 0;
}
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

        *(str + strlen(str)-1) = '\0';
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
                                 width ,
                                 height,
                                 p_surface_id,
                                 1,
                                 &surface_attrib,
                                 1);
   return va_status;
}

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
    uint16_t  row;
    uint32_t frame_size;
    size_t n_items;
    unsigned char * newImageBuffer = NULL;
    //vaImage with src fourcc
    VAImageFormat image_format;
    image_format.fourcc = g_in_fourcc;
    va_status = vaCreateImage(va_dpy, &image_format,
                 g_in_pic_width, g_in_pic_height,
                 &surface_image);
    CHECK_VASTATUS(va_status,"vaCreateImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    switch (surface_image.format.fourcc)
    {
    case VA_FOURCC_NV12:
    case VA_FOURCC_NV21:
        frame_size = surface_image.width * surface_image.height * 3 / 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        u_src = newImageBuffer + surface_image.width * surface_image.height;
        v_src = u_src;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_dst = u_dst;
        /* Y plane, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_dst += surface_image.pitches[0];
            y_src += surface_image.width;
        }
        /* UV plane */
        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width);
            u_src += surface_image.width;
            v_src = u_src;
            u_dst += surface_image.pitches[1];
        }
        break;
     case VA_FOURCC_YV12:
     case VA_FOURCC_I420:
        frame_size = surface_image.width * surface_image.height * 3 / 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        v_src = newImageBuffer + surface_image.width * surface_image.height;
        u_src = v_src + (surface_image.width >> 1) * (surface_image.height >> 1);
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        /* Y plane, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_dst += surface_image.pitches[0];
            y_src += surface_image.width;
        }
        /* UV plane */
        for (row = 0; row < surface_image.height >> 1; row++) {
            memcpy(u_dst, u_src, surface_image.width >> 1);
            memcpy(v_dst, v_src, surface_image.width >> 1);
            u_src += surface_image.width >> 1;
            v_src += surface_image.width >> 1;
            v_dst += surface_image.pitches[1];
            u_dst += surface_image.pitches[1];
        }
        break;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_RGB565:
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
        break;
    case VA_FOURCC_P010:
    case VA_FOURCC_I010:
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
        } else if (surface_image.format.fourcc == VA_FOURCC_P010){
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
        break;
    case VA_FOURCC_RGBA:
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_XRGB:
    case VA_FOURCC_RGBX:
    case VA_FOURCC_BGRA:
    case VA_FOURCC_BGRX:
    case VA_FOURCC_XBGR:
    case VA_FOURCC_A2R10G10B10:
    case VA_FOURCC_A2B10G10R10:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_Y410:
    case VA_FOURCC_Y210:
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
        break;
    case VA_FOURCC_RGBP:
    case VA_FOURCC_BGRP:
        frame_size = surface_image.width * surface_image.height * 3;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        /* plane 0, directly copy */
        for (row = 0; row < surface_image.height * 3; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.width;
            y_dst += surface_image.pitches[0];
        }
        break;
    case VA_FOURCC_422H:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
        frame_size = surface_image.width * surface_image.height * 3;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        u_src = newImageBuffer + surface_image.width * surface_image.height;
        v_src = u_src + surface_image.width * surface_image.height ;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);

        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);
            y_src += surface_image.width;
            u_src += surface_image.width;
            v_src += surface_image.width;
            y_dst += surface_image.pitches[0];
            u_dst += surface_image.pitches[1];
            v_dst += surface_image.pitches[2];
        }
        break;
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
        frame_size = surface_image.width * surface_image.height * 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        
        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);
        
        y_src = newImageBuffer;
        u_src = newImageBuffer + surface_image.width * surface_image.height;
        v_src = u_src + surface_image.width * (surface_image.height >> 1) ;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.width;
            y_dst += surface_image.pitches[0];
        }
        for (row = 0; row < (surface_image.height >> 1); row++) {
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);
            u_src += surface_image.width;
            v_src += surface_image.width;
            u_dst += surface_image.pitches[1];
            v_dst += surface_image.pitches[2];
        }
        break;
    case VA_FOURCC_Y800:
        frame_size = surface_image.width * surface_image.height;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        
        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);
        
        y_src = newImageBuffer;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        
        /* plane 0, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.width;
            y_dst += surface_image.pitches[0];
        }
        break;
    default:
        printf("Not supported YUV surface input fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }
   
    if (newImageBuffer){
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    // render csc_src_fourcc image to input surface
    va_status = vaPutImage(va_dpy, g_in_surface_id, surface_image.image_id,
                       0, 0, g_in_pic_width, g_in_pic_height, 
                       0, 0, g_in_pic_width, g_in_pic_height);
    CHECK_VASTATUS(va_status,"vaPutImage");
    vaDestroyImage(va_dpy, surface_image.image_id);
    CHECK_VASTATUS(va_status,"vaDestroyImage");

    return VA_STATUS_SUCCESS;
}


static VAStatus
store_yuv_surface_to_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    VAImageFormat image_format;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *v_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    unsigned char *v_dst = NULL;
    uint16_t row;
    int32_t n_items;
    uint32_t frame_size = 0;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface (va_dpy,surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    image_format.fourcc = g_out_fourcc;
    va_status = vaCreateImage(va_dpy, &image_format, g_in_pic_width, g_in_pic_height, &surface_image);
    CHECK_VASTATUS(va_status, "vaCreateImage");
    va_status = vaGetImage(va_dpy, surface_id, 0, 0, g_in_pic_width, g_in_pic_height, surface_image.image_id);
    CHECK_VASTATUS(va_status, "vaGetImage");
    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    switch (surface_image.format.fourcc)
    {
    case VA_FOURCC_NV12:
    case VA_FOURCC_NV21:
        frame_size = surface_image.width * surface_image.height * 3 /2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        /* stored as YV12 format */
        y_dst = newImageBuffer;
        u_dst = v_dst = newImageBuffer + surface_image.width * surface_image.height;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = u_src;
        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }
        /* UV plane copy */
        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width);
            u_dst += surface_image.width;
            u_src += surface_image.pitches[1];
        }
        break;
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
        frame_size = surface_image.width * surface_image.height * 3 / 2;

        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        /* stored as YV12 format */
        y_dst = newImageBuffer;
        v_dst = newImageBuffer + surface_image.width * surface_image.height;
        u_dst = newImageBuffer + surface_image.width * surface_image.height * 5/4;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);

        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }

        /* UV plane copy */
        for (row = 0; row < surface_image.height /2; row ++){
            memcpy(v_dst, v_src, surface_image.width/2);
            memcpy(u_dst, u_src, surface_image.width/2);

            v_dst += surface_image.width/2;
            u_dst += surface_image.width/2;
            v_src += surface_image.pitches[1];
            u_src += surface_image.pitches[2];
        }
        break;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_RGB565:
        frame_size = surface_image.width * surface_image.height * 2;
        
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
        break;
    case VA_FOURCC_I010:
        frame_size = surface_image.width * surface_image.height * 3;
        
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        y_dst = newImageBuffer;
        
        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        
        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 2);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width * 2;
        }
        u_dst = newImageBuffer + surface_image.width * surface_image.height * 2;
        v_dst = newImageBuffer + surface_image.width * surface_image.height * 5/2;
        
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        
        for (row = 0; row < surface_image.height / 2; row++){
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);
        
            u_dst += surface_image.width;
            v_dst += surface_image.width;
        
            u_src += surface_image.pitches[1];
            v_src += surface_image.pitches[2];
        }
        break;
    case VA_FOURCC_P010:
        frame_size = surface_image.width * surface_image.height * 3;
        
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        y_dst = newImageBuffer;
        
        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        
        /* Y plane copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width * 2);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width * 2;
        }
        u_dst = newImageBuffer + surface_image.width * surface_image.height * 2;
        v_dst = u_dst;
        
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = u_src;
        
        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width * 2);
            u_dst += surface_image.width * 2;
            u_src += surface_image.pitches[1];
        }
        break;
    case VA_FOURCC_RGBA:
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_XRGB:
    case VA_FOURCC_RGBX:
    case VA_FOURCC_BGRX:
    case VA_FOURCC_BGRA:
    case VA_FOURCC_XBGR:
    case VA_FOURCC_A2R10G10B10:
    case VA_FOURCC_A2B10G10R10:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_Y410:
    case VA_FOURCC_Y210:
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
        break;
    case VA_FOURCC_RGBP:
    case VA_FOURCC_BGRP:
        frame_size = surface_image.width * surface_image.height * 3;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        y_dst = newImageBuffer;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        for (row = 0; row < surface_image.height * 3; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }
        break;
    case VA_FOURCC_422H:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
        frame_size = surface_image.width * surface_image.height * 3;
        
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        y_dst = newImageBuffer;
        u_dst = newImageBuffer + surface_image.width * surface_image.height;
        v_dst = u_dst + surface_image.width * surface_image.height;
        
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);
            y_src += surface_image.pitches[0];
            u_src += surface_image.pitches[1];
            v_src += surface_image.pitches[2];
            y_dst += surface_image.width;
            u_dst += surface_image.width;
            v_dst += surface_image.width;
        }
        break;
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
        frame_size = surface_image.width * surface_image.height * 2;
        
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        
        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        y_dst = newImageBuffer;
        u_dst = newImageBuffer + surface_image.width * surface_image.height;
        v_dst = u_dst + surface_image.width * (surface_image.height >> 1);
        
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }
        for (row = 0; row < (surface_image.height >> 1); row++) {
            memcpy(u_dst, u_src, surface_image.width);
            memcpy(v_dst, v_src, surface_image.width);
            u_src += surface_image.pitches[1];
            v_src += surface_image.pitches[2];
            u_dst += surface_image.width;
            v_dst += surface_image.width;
        }
        break;
    case VA_FOURCC_Y800:
        frame_size = surface_image.width * surface_image.height;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);
        y_dst = newImageBuffer;
        
        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[0];
            y_dst += surface_image.width;
        }
        break;
    default:
        printf("Not supported YUV surface fourcc !!! \n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    /* write frame to file */
    do {
        n_items = fwrite(newImageBuffer,frame_size, 1, fp);
    } while (n_items != 1);

    if (newImageBuffer){
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    va_status = vaUnmapBuffer(va_dpy, surface_image.buf);
    CHECK_VASTATUS(va_status, "vaUnmapBuffer");
    va_status = vaDestroyImage(va_dpy, surface_image.image_id);
    CHECK_VASTATUS(va_status, "vaDestroyImage");

    return VA_STATUS_SUCCESS;
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

    va_status = vaCreateConfig(va_dpy,
                               VAProfileNone,
                               VAEntrypointVideoProc,
                               &attrib,
                               1,
                               &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

   return va_status;
}

static void
vpp_context_destroy()
{
    VAStatus va_status;

    /* Release resource */
    va_status = vaDestroySurfaces(va_dpy, &g_in_surface_id, 1);
    
    CHECK_VASTATUS(va_status, "vaDestroySurfaces");
    va_status = vaDestroyConfig(va_dpy, config_id);
    CHECK_VASTATUS(va_status, "vaDestroyConfig");

    va_status = vaTerminate(va_dpy);
    CHECK_VASTATUS(va_status, "vaTerminate");
    va_close_display(va_dpy);
}

static int8_t
parse_fourcc_and_format(char *str, uint32_t *fourcc, uint32_t *format)
{
    uint32_t tfourcc = VA_FOURCC_NV12;
    uint32_t tformat = VA_RT_FORMAT_YUV420;

    if (!strcmp(str, "YV12")){
        tfourcc = VA_FOURCC_YV12;
    } else if(!strcmp(str, "I420")){
        tfourcc = VA_FOURCC_I420;
    } else if(!strcmp(str, "NV12")){
        tfourcc = VA_FOURCC_NV12;
    } else if(!strcmp(str, "YUY2") || !strcmp(str, "YUYV")) {
        tfourcc = VA_FOURCC_YUY2;
    } else if(!strcmp(str, "UYVY")){
        tfourcc = VA_FOURCC_UYVY;
    } else if (!strcmp(str, "P010")) {
        tfourcc = VA_FOURCC_P010;
    } else if (!strcmp(str, "I010")) {
        tfourcc = VA_FOURCC_I010;
    } else if (!strcmp(str, "RGBA")) {
        tfourcc = VA_FOURCC_RGBA;
    } else if (!strcmp(str, "RGBX")) {
        tfourcc = VA_FOURCC_RGBX;
    } else if (!strcmp(str, "BGRA")) {
        tfourcc = VA_FOURCC_BGRA;
    } else if (!strcmp(str, "BGRX")) {
        tfourcc = VA_FOURCC_BGRX;
    } else if (!strcmp(str, "ARGB")) {
        tfourcc = VA_FOURCC_ARGB;
    } else if (!strcmp(str, "XRGB")) {
        tfourcc = VA_FOURCC_XRGB;
    } else if (!strcmp(str, "XBGR")) {
        tfourcc = VA_FOURCC_XBGR;
    }else if (!strcmp(str, "RGBP")) {
        tfourcc = VA_FOURCC_RGBP;
    }else if (!strcmp(str, "BGRP")) {
        tfourcc = VA_FOURCC_BGRP;
    }else if (!strcmp(str, "RG16")) {
        tfourcc = VA_FOURCC_RGB565;
    }else if (!strcmp(str, "AYUV")) {
        tfourcc = VA_FOURCC_AYUV;
    }else if (!strcmp(str, "NV21")) {
        tfourcc = VA_FOURCC_NV21;
    }else if (!strcmp(str, "422H")) {
        tfourcc = VA_FOURCC_422H;
    }else if (!strcmp(str, "422V")) {
        tfourcc = VA_FOURCC_422V;
    }else if (!strcmp(str, "444P")) {
        tfourcc = VA_FOURCC_444P;
    }else if (!strcmp(str, "IMC3")) {
        tfourcc = VA_FOURCC_IMC3;
    }else if (!strcmp(str, "Y210")) {
        tfourcc = VA_FOURCC_Y210;
    }else if (!strcmp(str, "Y410")) {
        tfourcc = VA_FOURCC_Y410;
    }else if (!strcmp(str, "Y800")) {
        tfourcc = VA_FOURCC_Y800;
    }else if (!strcmp(str, "411P")) {
        tfourcc = VA_FOURCC_411P;
    }else if (!strcmp(str, "AR30")) {
        tfourcc = VA_FOURCC_A2R10G10B10;
        tformat = VA_RT_FORMAT_RGB32_10BPP;
    }else if (!strcmp(str, "AB30")) {
        tfourcc = VA_FOURCC_A2B10G10R10;
        tformat = VA_RT_FORMAT_RGB32_10BPP;
    }
    else{
        printf("Not supported format: %s! Currently only support following format: %s\n",
               str, "YV12, I420, NV12, YUY2(YUYV), UYVY, P010, I010, RGBA, RGBX, BGRA or BGRX,ARGB,XRGB,XBGR,RGBP,BGRP,RG16, \
              AYUV, NV21, 422H, 422V, 444P,IMC3,Y210,Y410,Y800,411P,AR30,AB30");
        assert(0);
    }

    if (fourcc)
        *fourcc = tfourcc;

    if (format)
        *format = tformat;

    return 0;
}

static int8_t parse_format_list(char *str)
{
    if(!str){
        printf("the para format list is null!\n");
        return -1;
    }
    char *format = strtok(str, ";");
    while(format != NULL){
        g_format_list.push_back(VA_FOURCC(format[0],format[1],format[2],format[3]));
        format = strtok(NULL, ";");
    }
    if(g_format_list.size() == 0 )
        return -1;
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
    read_value_string(g_config_file_fd, "DST_FILE_NAME", g_dst_file_name);
    read_value_string(g_config_file_fd, "DST_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_out_fourcc, &g_out_format);
    read_value_string(g_config_file_fd, "SUPPORT_FORMAT_LIST", str);
    parse_format_list(str);
    read_value_uint32(g_config_file_fd, "FULL_FORMAT_TEST", &g_full_format_test);
    
    return 0;
}

static void
print_help()
{
    printf("The app is used to test the scaling and csc feature.\n");
    printf("Cmd Usage: ./vppscaling_csc process_scaling_csc.cfg\n");
    printf("The configure file process_scaling_csc.cfg is used to configure the para.\n");
    printf("You can refer process_scaling_csc.cfg.template for each para meaning and create the configure file.\n");
}
int32_t main(int32_t argc, char *argv[])
{
    VAStatus va_status;

    if (argc != 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")){
        print_help();
        return -1;
    }

    /* Parse the configure file for video process*/
    strncpy(g_config_file_name, argv[1], MAX_LEN);
    g_config_file_name[MAX_LEN - 1] = '\0';

    if (NULL == (g_config_file_fd = fopen(g_config_file_name, "r"))){
        printf("Open configure file %s failed!\n",g_config_file_name);
        assert(0);
    }

    /* Parse basic parameters */
    if (parse_basic_parameters()){
        printf("Parse parameters in configure file error\n");
        assert(0);
    }

    va_status = vpp_context_create();
    if (va_status != VA_STATUS_SUCCESS) {
        printf("vpp context create failed \n");
        assert(0);
    }

    /* Video frame fetch, process and store */
   
    int num_formats;
    uint16_t i;
    std::vector<VAImageFormat> format_list;
    uint32_t count = vaMaxNumImageFormats(va_dpy);
    format_list.resize(count);

    va_status = vaQueryImageFormats(
                va_dpy,
                &format_list[0],	/* out */
                &num_formats);		/* out */
    CHECK_VASTATUS(va_status, "vaQueryImageFormats");
    std::vector<VAImageFormat>::iterator it = format_list.begin();
    printf("num format is %d,the support format list as:\n",num_formats);
    char format[5];
    while (it != format_list.end())
    {
        get_fourcc_str(it->fourcc,format);
        printf("%s;",format);
        ++it;
    }
    if((unsigned int)num_formats != g_format_list.size()){
        printf("the format list size not match query list, please check\n");
        return -1;
    }
    //find if the given list and query list match, if not match, need update sample
    std::vector<uint32_t>::iterator it1 = g_format_list.begin();
    for(i = 0; i< format_list.size();i++){
        it1 = std::find(g_format_list.begin(), g_format_list.end(), format_list[i].fourcc);
        if (it1 == g_format_list.end())
        {
            get_fourcc_str(format_list[i].fourcc, format);
            printf("VAImageFormat %s in qurey list is not found",format );
            return -1;
        }
    }
    if( i == format_list.size()){
        printf("vaQueryImageFormats success with no format change\n");
    }
    printf("\nStart to process, ...\n");
    struct timespec Pre_time;
    struct timespec Cur_time;
    unsigned int duration = 0;
    clock_gettime(CLOCK_MONOTONIC, &Pre_time);
    
    /* Create surface with input format */
    va_status = create_surface(&g_in_surface_id, g_in_pic_width, g_in_pic_height,
                               g_in_fourcc, g_in_format);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for input");

    char dst_file_name[100];
    if(g_full_format_test){
        //copy the input file content to input image
        for(i = 0; i< g_format_list.size(); i++){
            g_out_fourcc = g_format_list[i];
            char str[5];
            get_fourcc_str(g_out_fourcc, str);
            printf("begin to process output format %s\n",str);
            sprintf(dst_file_name,"%s.%s",g_dst_file_name, str);
            if (NULL == (g_src_file_fd = fopen(g_src_file_name, "r"))){
                printf("Open SRC_FILE_NAME: %s failed, please specify it in config file: %s !\n",
                g_src_file_name, g_config_file_name);
                assert(0);
            }
            if (NULL == (g_dst_file_fd = fopen(dst_file_name, "w"))){
                printf("Open DST_FILE_NAME: %s failed, please specify it in config file: %s !\n",
                       dst_file_name, g_config_file_name);
                assert(0);
            }
            upload_yuv_frame_to_yuv_surface(g_src_file_fd,g_in_surface_id);
            CHECK_VASTATUS(va_status, "upload_yuv_frame_to_yuv_surface");
            store_yuv_surface_to_file(g_dst_file_fd, g_in_surface_id);
            CHECK_VASTATUS(va_status, "store_yuv_surface_to_file");
            if (g_src_file_fd)
                fclose(g_src_file_fd);
    
            if (g_dst_file_fd)
                fclose(g_dst_file_fd);
        }
    }else{
         if (NULL == (g_src_file_fd = fopen(g_src_file_name, "r"))){
            printf("Open SRC_FILE_NAME: %s failed, please specify it in config file: %s !\n",
                    g_src_file_name, g_config_file_name);
            assert(0);
        }

        if (NULL == (g_dst_file_fd = fopen(g_dst_file_name, "w"))){
            printf("Open DST_FILE_NAME: %s failed, please specify it in config file: %s !\n",
                   g_dst_file_name, g_config_file_name);
            assert(0);
        }
        upload_yuv_frame_to_yuv_surface(g_src_file_fd,g_in_surface_id);
        CHECK_VASTATUS(va_status, "upload_yuv_frame_to_yuv_surface");
        store_yuv_surface_to_file(g_dst_file_fd, g_in_surface_id);
        CHECK_VASTATUS(va_status, "store_yuv_surface_to_file");
        if (g_src_file_fd)
            fclose(g_src_file_fd);
    
        if (g_dst_file_fd)
            fclose(g_dst_file_fd);
    }
    clock_gettime(CLOCK_MONOTONIC, &Cur_time);
    duration = (Cur_time.tv_sec - Pre_time.tv_sec) * 1000;
    if (Cur_time.tv_nsec > Pre_time.tv_nsec) {
        duration += (Cur_time.tv_nsec - Pre_time.tv_nsec) / 1000000;
    } else {
        duration += (Cur_time.tv_nsec + 1000000000 - Pre_time.tv_nsec) / 1000000 - 1000;
    }

    printf("Finish processing, performance: \n" );
    printf("1 frame processed in: %d ms\n", duration);

    if (g_config_file_fd)
       fclose(g_config_file_fd);

    vpp_context_destroy();

    return 0;
}
