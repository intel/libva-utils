/*
 * Copyright (c) 2024 Intel Corporation. All Rights Reserved.
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
#include <va/va_drmcommon.h>

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
    if (drm_fd < 0) {
        printf("####INFO: device %s is invalid !\n", device_paths);
        return NULL;
    }

    VADisplay va_dpy = vaGetDisplayDRM(drm_fd);
    if (va_dpy)
        return va_dpy;

    close(drm_fd);
    drm_fd = -1;
    return 0;
}

int main(int argc, char **argv)
{
    VASurfaceID surface_id_0, surface_id_1, surface_id_2;
    int major_ver, minor_ver;
    VADisplay   va_dpy_1, va_dpy_2;
    VAStatus va_status;

    va_init_display_args(&argc, argv);
    va_dpy_1 = openDriver(argv[1]);
    va_status = vaInitialize(va_dpy_1, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

    va_dpy_2 = openDriver(argv[1]);
    va_status = vaInitialize(va_dpy_2, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

////////////////////////////vaCopy in va_dpy_1////////////////////////////////
    va_status = vaCreateSurfaces(
                    va_dpy_1,
                    VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                    &surface_id_0, 1,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    va_status = vaCreateSurfaces(
                    va_dpy_1,
                    VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                    &surface_id_1, 1,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    //vaCopy
    VACopyObject dst;
    dst.obj_type = VACopyObjectSurface;
    dst.object.surface_id = surface_id_1;
    VACopyObject src;
    src.obj_type = VACopyObjectSurface;
    src.object.surface_id = surface_id_0;
    VACopyOption option;
    option.bits.va_copy_sync = VA_EXEC_SYNC;
    option.bits.va_copy_mode = VA_EXEC_MODE_DEFAULT;
    va_status = vaCopy(va_dpy_1, &dst, &src, option);
    CHECK_VASTATUS(va_status, "vaCopy");

    //va_status = vaSyncSurface(va_dpy_1, surface_id_1);
    //CHECK_VASTATUS(va_status, "vaSyncSurface");

    //vaExportSurface to get surface prime_fd
    VADRMPRIMESurfaceDescriptor desc;
    memset(&desc, 0, sizeof(VADRMPRIMESurfaceDescriptor));
    va_status = vaExportSurfaceHandle(va_dpy_1, surface_id_1, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, 0, &desc);
////////////////////////////end vaCopy in va_dpy_1/////////////////////////////////////////////////////////////

////////////////////////////copy out surface from va_dpy_1 to another surface in va_dpy_2////////////
    ////import surface from va_dpy_1 and create output surface
    VASurfaceAttrib surf_attrib[2];

    surf_attrib[0].type = VASurfaceAttribMemoryType;
    surf_attrib[0].value.type = VAGenericValueTypeInteger;
    surf_attrib[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    surf_attrib[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;

    surf_attrib[1].type = VASurfaceAttribExternalBufferDescriptor;
    surf_attrib[1].value.type = VAGenericValueTypePointer;
    surf_attrib[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    surf_attrib[1].value.value.p = &desc;
    va_status = vaCreateSurfaces(
                    va_dpy_2,
                    VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                    &surface_id_2, 1,
                    surf_attrib, 2
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    VASurfaceID surface_id_3;
    va_status = vaCreateSurfaces(
                    va_dpy_2,
                    VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                    &surface_id_3, 1,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    //vaCopy
    dst.obj_type = VACopyObjectSurface;
    dst.object.surface_id = surface_id_3;
    src.obj_type = VACopyObjectSurface;
    src.object.surface_id = surface_id_2;
    option.bits.va_copy_sync = VA_EXEC_SYNC;
    option.bits.va_copy_mode = VA_EXEC_MODE_DEFAULT;
    va_status = vaCopy(va_dpy_2, &dst, &src, option);
    CHECK_VASTATUS(va_status, "vaCopy");

    va_status = vaSyncSurface(va_dpy_2, surface_id_2);
    va_status = vaSyncSurface(va_dpy_2, surface_id_3);
    CHECK_VASTATUS(va_status, "vaSyncSurface");
////////////////////////////end copy//////////////////////////////////////////////////////////////////

    vaDestroySurfaces(va_dpy_1, &surface_id_0, 1);
    vaDestroySurfaces(va_dpy_1, &surface_id_1, 1);
    vaDestroySurfaces(va_dpy_2, &surface_id_2, 1);
    vaDestroySurfaces(va_dpy_2, &surface_id_3, 1);

    vaTerminate(va_dpy_1);
    vaTerminate(va_dpy_2);
    va_close_display(va_dpy_1);
    va_close_display(va_dpy_2);
    return 0;
}
