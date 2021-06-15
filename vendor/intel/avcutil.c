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

#include "avcdemo.h"

/* It is copied from ../../videoprocess/vavpp.cpp */
/* Store NV12/YV12/I420 surface to yv12 file */
VAStatus store_yuv_surface_to_yv12_file(FILE *fp, VASurfaceID surface_id,
                                        VADisplay va_dpy)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src, *u_src, *v_src;
    unsigned char *y_dst, *u_dst, *v_dst;
    uint32_t row, col;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one YV12 file or one bmp file*/
    if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
        surface_image.format.fourcc == VA_FOURCC_I420 ||
        surface_image.format.fourcc == VA_FOURCC_NV12){

        uint32_t y_size = surface_image.width * surface_image.height;
        uint32_t u_size = y_size/4;

        newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
        assert(newImageBuffer);

        /* stored as YV12 format */
        y_dst = newImageBuffer;
        v_dst = newImageBuffer + y_size;
        u_dst = newImageBuffer + y_size + u_size;

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        if (surface_image.format.fourcc == VA_FOURCC_YV12){
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if(surface_image.format.fourcc == VA_FOURCC_I420){
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if(surface_image.format.fourcc == VA_FOURCC_NV12){
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
        if (surface_image.format.fourcc == VA_FOURCC_YV12||
            surface_image.format.fourcc == VA_FOURCC_I420){
            for (row = 0; row < surface_image.height /2; row ++){
                memcpy(v_dst, v_src, surface_image.width/2);
                memcpy(u_dst, u_src, surface_image.width/2);

                v_dst += surface_image.width/2;
                u_dst += surface_image.width/2;

                if (surface_image.format.fourcc == VA_FOURCC_YV12){
                    v_src += surface_image.pitches[1];
                    u_src += surface_image.pitches[2];
                } else {
                    v_src += surface_image.pitches[2];
                    u_src += surface_image.pitches[1];
                }
            }
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12){
            for (row = 0; row < surface_image.height / 2; row++) {
                for (col = 0; col < surface_image.width /2; col++) {
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

    if (newImageBuffer){
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);

    return VA_STATUS_SUCCESS;
}

