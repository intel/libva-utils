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
#define CLIP_HEIGHT 1080

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
    std::vector<VASurfaceID> surfacesPool;
    start_time = clock();
    for (uint32_t i = 0; i < 168; i++)
    {
        VASurfaceID surface_id;
        va_status = vaCreateSurfaces(
                        va_dpy,
                        VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                        &surface_id, 1,
                        NULL, 0
                    );
        CHECK_VASTATUS(va_status, "vaCreateSurfaces");
        surfacesPool.push_back(surface_id);
    }
    end_time = clock();
    execution_time_ms = ((double)(end_time - start_time) * 1000) / CLOCKS_PER_SEC;
    printf("168 * 1080p y420 surfaces execution time = %f \n", execution_time_ms);

    for (std::vector<VASurfaceID>::iterator it = surfacesPool.begin(); it != surfacesPool.end(); it++)
    {
        vaDestroySurfaces(va_dpy, &*it, 1);
    }

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    return 0;
}
