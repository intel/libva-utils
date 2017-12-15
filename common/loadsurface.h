/*
 * Copyright (c) 2008-2009 Intel Corporation. All Rights Reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                                   \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                            \
    }

int scale_2dimage(unsigned char *src_img, int src_imgw, int src_imgh,
    unsigned char *dst_img, int dst_imgw, int dst_imgh);

int YUV_blend_with_pic(int width, int height, unsigned char *Y_start,
    int Y_pitch, unsigned char *U_start, int U_pitch,
    unsigned char *V_start, int V_pitch, unsigned int fourcc, int fixed_alpha);
int yuvgen_planar(int width, int height, unsigned char *Y_start, int Y_pitch,
    unsigned char *U_start, int U_pitch, unsigned char *V_start, int V_pitch,
    unsigned int fourcc, int box_width, int row_shift, int field);
int upload_surface(VADisplay va_dpy, VASurfaceID surface_id,
    int box_width, int row_shift, int field);
int upload_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
    int src_fourcc, int src_width, int src_height, unsigned char *src_Y,
    unsigned char *src_U, unsigned char *src_V);
int download_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
    int dst_fourcc, int dst_width, int dst_height, unsigned char *dst_Y,
    unsigned char *dst_U, unsigned char *dst_V);
#ifdef __cplusplus
}
#endif
