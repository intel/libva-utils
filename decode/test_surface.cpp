/*
 * Copyright (c) 2007-2008 Intel Corporation. All Rights Reserved.
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
 * it is a real program to show how VAAPI decode work,
 * It does VLD decode for a simple MPEG2 clip "mpeg2-I.m2v"
 * "mpeg2-I.m2v" and VA parameters are hardcoded into mpeg2vldemo.c,
 * See mpeg2-I.jif to know how those VA parameters come from
 *
 * gcc -o  mpeg2vldemo  mpeg2vldemo.c -lva -lva-x11 -I/usr/include/va
 * ./mpeg2vldemo  : only do decode
 * ./mpeg2vldemo <any parameter >: decode+display
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <va/va.h>
#include <vector>
#include <va/va_drm.h>
#include "va_display.h"
#include <time.h>

#define CHECK_VASTATUS(va_status,func)                                  \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
    exit(1);                                                            \
}


#define CLIP_WIDTH  1920
#define CLIP_HEIGHT 1920

VADisplay openDriver(char *device_paths)
{
    int drm_fd = open(device_paths, O_RDWR);
    if (drm_fd < 0)
    {
        printf("####INFO: device %s is invalid !\n", device_paths);
        return NULL;
    }

    VADisplay va_dpy = vaGetDisplayDRM(drm_fd);
    if(va_dpy)
        return va_dpy;

    close(drm_fd);
    drm_fd = -1;
    return 0;
}

uint32_t va_fourcc[] = 
{
    VA_FOURCC_BGRA,
    VA_FOURCC_ARGB,
    VA_FOURCC_RGBA,
    VA_FOURCC_ABGR,
    VA_FOURCC_BGRX,
    VA_FOURCC_XRGB,
    VA_FOURCC_RGBX,
    VA_FOURCC_XBGR,
    VA_FOURCC_RGBP,
    VA_FOURCC_BGRP,
    VA_FOURCC_RGB565,
    VA_FOURCC_AYUV,
    VA_FOURCC_XYUV,
    VA_FOURCC_NV12,
    VA_FOURCC_NV21,
    VA_FOURCC_YUY2,
    VA_FOURCC_UYVY,
    VA_FOURCC_YV12,
    VA_FOURCC_I420,
    VA_FOURCC_IYUV,
    VA_FOURCC_411P,
    VA_FOURCC_422H,
    VA_FOURCC_422V,
    VA_FOURCC_444P,
    VA_FOURCC_IMC3,
    VA_FOURCC_P208,
    VA_FOURCC_P010,
    VA_FOURCC_P012,
    VA_FOURCC_P016,
    VA_FOURCC_Y210,
    VA_FOURCC_Y410,
    VA_FOURCC_Y212,
    VA_FOURCC_Y216,
    VA_FOURCC_Y412,
    VA_FOURCC_Y416,
    VA_FOURCC_Y800,
    VA_FOURCC_A2R10G10B10,
    VA_FOURCC_A2B10G10R10,
    VA_FOURCC_X2R10G10B10,
    VA_FOURCC_X2B10G10R10
};


int main(int argc, char **argv)
{
    clock_t start_time, end_time;
    double execution_time_ms;
    int major_ver, minor_ver;
    VADisplay   va_dpy;
    VAStatus va_status;
    va_init_display_args(&argc, argv);

    va_dpy = openDriver(argv[1]);
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

    //VAImage csc_dst_fourcc_image;
    //VAImageFormat image_format;
    //image_format.fourcc = VA_FOURCC_NV12;
    //image_format.byte_order = VA_LSB_FIRST;
    //image_format.bits_per_pixel = 16;

    for (int i = 0; i < 30; i++)
    {
        VAImage csc_dst_fourcc_image;
        VAImageFormat image_format;
        image_format.fourcc = va_fourcc[i];
        image_format.byte_order = VA_LSB_FIRST;
        image_format.bits_per_pixel = 16;

        printf(">>>>>>>create image fourcc[%d]: %d\n", i, va_fourcc[i]);
        va_status = vaCreateImage(va_dpy, &image_format,
                                  16, 16384,
                                  &csc_dst_fourcc_image);

        if (va_status != VA_STATUS_SUCCESS)
        {
            printf(">>>>>>>failed to create image fourcc[%d]: %d\n", i, va_fourcc[i]);
        }
        else
        {
            vaDestroyImage(va_dpy, csc_dst_fourcc_image.image_id);
        }
    }

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    return 0;
}
