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
 * This test covers different surface format copy.
 * Usage: ./vacopy process_copy.cfg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iostream>
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
using namespace std;
static VADisplay va_dpy = NULL;
static VAContextID context_id = 0;

typedef struct _SurfInfo {
    FILE        *fd;
    char        name[MAX_LEN];
    uint32_t    width;
    uint32_t    height;
    uint32_t    fourCC;
    uint32_t    format;
    uint32_t    memtype;
    uint32_t    alignsize;
    void        *pBuf;
    uint8_t     *pBufBase;
    uintptr_t   ptrb;
} SurfInfo;

static SurfInfo g_src;
static SurfInfo g_dst;

static VAConfigID  config_id = 0;
static FILE* g_config_file_fd = NULL;
static char g_config_file_name[MAX_LEN];

static VASurfaceID g_in_surface_id = VA_INVALID_ID;
static VASurfaceID g_out_surface_id = VA_INVALID_ID;

static uint32_t g_src_file_fourcc = VA_FOURCC('I', '4', '2', '0');
static uint32_t g_dst_file_fourcc = VA_FOURCC('Y', 'V', '1', '2');

#define _FREE(p)                       \
    if(p != NULL){                      \
        free(p);  p = NULL;                        \
    }

static uint32_t g_frame_count = 0;
static uint32_t g_copy_method = 0; //0 blance, 1 perf. 2 power_saving

static int8_t
parse_memtype_format(char *str, uint32_t *dst_memtype)
{
    uint32_t tmemtype = VA_SURFACE_ATTRIB_MEM_TYPE_VA;

    if (!strcmp(str, "VA")) {
        tmemtype = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
    } else if (!strcmp(str, "CPU")) {
        tmemtype = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
    } else {
        printf("Not supported format: %s! Currently only support following format: %s\n",
               str, "VA,CPU");
        assert(0);
    }
    if (dst_memtype)
        *dst_memtype = tmemtype;
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
create_surface(VASurfaceID * p_surface_id, SurfInfo &surf)
{
    VAStatus va_status = VA_STATUS_ERROR_INVALID_PARAMETER;
    if (surf.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        VASurfaceAttrib    surface_attrib;
        surface_attrib.type =  VASurfaceAttribPixelFormat;
        surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib.value.type = VAGenericValueTypeInteger;
        surface_attrib.value.value.i = surf.fourCC;

        va_status = vaCreateSurfaces(va_dpy,
                                     surf.format,
                                     surf.width,
                                     surf.height,
                                     p_surface_id,
                                     1,
                                     &surface_attrib,
                                     1);
    } else if (surf.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR) {
        VASurfaceAttrib surfaceAttrib[3];
        VASurfaceAttribExternalBuffers extBuffer;
        uint32_t base_addr_align = 0x1000;
        uint32_t size = 0;
        surfaceAttrib[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[0].type = VASurfaceAttribPixelFormat;
        surfaceAttrib[0].value.type = VAGenericValueTypeInteger;
        surfaceAttrib[0].value.value.i = surf.fourCC;

        surfaceAttrib[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[1].type = VASurfaceAttribMemoryType;
        surfaceAttrib[1].value.type = VAGenericValueTypeInteger;
        surfaceAttrib[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

        surfaceAttrib[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[2].type = VASurfaceAttribExternalBufferDescriptor;
        surfaceAttrib[2].value.type = VAGenericValueTypePointer;
        surfaceAttrib[2].value.value.p = (void *)&extBuffer;
        memset(&extBuffer, 0, sizeof(extBuffer));

        uint32_t pitch_align = surf.alignsize;
        switch (surf.fourCC) {
        case VA_FOURCC_NV12:
            extBuffer.pitches[0] = ((surf.width + pitch_align - 1) / pitch_align) * pitch_align;
            size = (extBuffer.pitches[0] * surf.height) * 3 / 2; // frame size align with pitch.
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align; // frame size align as 4K page.
            extBuffer.offsets[0] = 0;// Y channel
            extBuffer.offsets[1] = extBuffer.pitches[0] * surf.height; // UV channel.
            extBuffer.pitches[1] = extBuffer.pitches[0];
            extBuffer.num_planes = 2;
            break;
        case VA_FOURCC_RGBP:
            extBuffer.pitches[0] = ((surf.width + pitch_align - 1) / pitch_align) * pitch_align;
            size = (extBuffer.pitches[0] * surf.height) * 3;// frame size align with pitch.
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align; // frame size align as 4K page.
            extBuffer.offsets[0] = 0;// Y channel
            extBuffer.offsets[1] = extBuffer.pitches[0] * surf.height; // U channel.
            extBuffer.pitches[1] = extBuffer.pitches[0];
            extBuffer.offsets[2] = extBuffer.pitches[0] * surf.height * 2; // V channel.
            extBuffer.pitches[2] = extBuffer.pitches[0];
            extBuffer.num_planes = 3;
            break;
        default :
            std::cout << surf.fourCC << "format doesn't support!" << endl;
            return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
        }
        if (!surf.pBuf && !surf.pBufBase) {
            surf.pBuf = malloc(size + base_addr_align);
            surf.pBufBase = (uint8_t*)((((uint64_t)(surf.pBuf) + base_addr_align - 1) / base_addr_align) * base_addr_align);

            extBuffer.pixel_format = surf.fourCC;
            extBuffer.width = surf.width;
            extBuffer.height = surf.height;
            extBuffer.data_size = size;
            extBuffer.num_buffers = 1;
            extBuffer.buffers = &(surf.ptrb);
            extBuffer.buffers[0] = (uintptr_t)(surf.pBufBase);
            extBuffer.flags = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

            va_status = vaCreateSurfaces(va_dpy, surf.format, surf.width, surf.height, p_surface_id, 1, surfaceAttrib, 3);
            CHECK_VASTATUS(va_status, "vaCreateSurfaces");
        } else {
            std::cout << "previous frame buffer hasn't be released!" << endl;
        }
    }

    return va_status;
}

/* Load frame to surface*/
static VAStatus
upload_frame_to_surface(FILE *fp,
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
    uint32_t frame_size, row;
    size_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    if (g_src.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        std::cout << "2D src surface width = " << g_src.width << " pitch = " << surface_image.pitches[0] << endl;
    } else {
        std::cout << "linear src surface width = " << g_src.width << " pitch = " << surface_image.pitches[0] << ((g_src.width % surface_image.pitches[0]) ? " it is 2D linear" : " it is 1D linear") << endl;
    }

    if (surface_image.format.fourcc == VA_FOURCC_RGBP) {
        frame_size = surface_image.width * surface_image.height * 3;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        u_src = newImageBuffer + surface_image.width * surface_image.height;
        v_src = newImageBuffer + surface_image.width * surface_image.height * 2;

        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);

        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_dst += surface_image.pitches[0];
            y_src += surface_image.width;

            memcpy(u_dst, u_src, surface_image.width);
            u_dst += surface_image.pitches[0];
            u_src += surface_image.width;

            memcpy(v_dst, v_src, surface_image.width);
            v_dst += surface_image.pitches[0];
            v_src += surface_image.width;
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
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
store_surface_to_file(FILE *fp,
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
    uint32_t row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    if (g_dst.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        std::cout << "2D dst surface width = " << g_dst.width << " pitch = " << surface_image.pitches[0] << endl;
    } else {
        std::cout << "linear dst surface width = " << g_dst.width << " pitch = " << surface_image.pitches[0] << ((g_dst.width % surface_image.pitches[0]) ? " it is 2D linear" : " it is 1D linear") << endl;
    }

    /* store the surface to one nv12 file */
    if (surface_image.format.fourcc == VA_FOURCC_NV12 ||
        surface_image.format.fourcc == VA_FOURCC_RGBP) {

        y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);

        if (surface_image.format.fourcc == VA_FOURCC_RGBP) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        } else if (surface_image.format.fourcc == VA_FOURCC_NV12) {
            u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
            v_src = u_src;
        }

        if (g_dst.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
            if (surface_image.format.fourcc == VA_FOURCC_NV12) {
                uint32_t y_size = surface_image.width * surface_image.height;
                newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
                assert(newImageBuffer);

                y_dst = newImageBuffer;
                u_dst = v_dst = newImageBuffer + y_size;

                /* Y plane copy */
                for (row = 0; row < surface_image.height; row++) {
                    memcpy(y_dst, y_src, surface_image.width);
                    y_src += surface_image.pitches[0];
                    y_dst += surface_image.width;
                }
                // UV plane
                for (row = 0; row < surface_image.height / 2; row++) {
                    memcpy(u_dst, u_src, surface_image.width);
                    u_dst += surface_image.width;
                    u_src += surface_image.pitches[1];
                }

                /* write frame to file */
                do {
                    n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
                } while (n_items != 1);
            } else if (surface_image.format.fourcc == VA_FOURCC_RGBP) {
                uint32_t y_size = surface_image.width * surface_image.height;
                newImageBuffer = (unsigned char*)malloc(y_size * 3);
                assert(newImageBuffer);

                y_dst = newImageBuffer;
                u_dst = newImageBuffer + y_size;
                v_dst = newImageBuffer + y_size * 2;

                for (row = 0; row < surface_image.height; row++) {
                    memcpy(y_dst, y_src, surface_image.width);
                    y_src += surface_image.pitches[0];
                    y_dst += surface_image.width;

                    memcpy(u_dst, u_src, surface_image.width);
                    u_dst += surface_image.width;
                    u_src += surface_image.pitches[0];

                    memcpy(v_dst, v_src, surface_image.width);
                    v_dst += surface_image.width;
                    v_src += surface_image.pitches[0];
                }

                do {
                    n_items = fwrite(newImageBuffer, y_size * 3, 1, fp);
                } while (n_items != 1);
            }
        } else { // usrptr surface.
            if (surface_image.format.fourcc == VA_FOURCC_NV12) {
                // directly copy NV12 1D/2D surface. skip derive and map image.
                uint32_t y_size = surface_image.height * surface_image.pitches[0];
                newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
                assert(newImageBuffer);
                memcpy(newImageBuffer, g_dst.pBufBase, (y_size * 3 / 2));

                do {
                    n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
                } while (n_items != 1);
            } else if (surface_image.format.fourcc == VA_FOURCC_RGBP) {
                uint32_t y_size = surface_image.height * surface_image.pitches[0];
                newImageBuffer = (unsigned char*)malloc(y_size * 3);
                assert(newImageBuffer);
                memcpy(newImageBuffer, g_dst.pBufBase, (y_size * 3));

                do {
                    n_items = fwrite(newImageBuffer, y_size * 3, 1, fp);
                } while (n_items != 1);
            }
        }
    } else {
        printf("Not supported surface fourcc !!! \n");
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
video_frame_process(VASurfaceID in_surface_id,
                    VASurfaceID out_surface_id)
{
    VAStatus va_status;
#if VA_CHECK_VERSION(1, 10, 0)
    VACopyObject src_obj, dst_obj;
    VACopyOption option;
    memset(&src_obj, 0, sizeof(src_obj));
    memset(&dst_obj, 0, sizeof(dst_obj));
    memset(&option, 0, sizeof(option));

    src_obj.obj_type = VACopyObjectSurface;
    src_obj.object.surface_id = in_surface_id;
    dst_obj.obj_type = VACopyObjectSurface;
    dst_obj.object.surface_id = out_surface_id;
    option.bits.va_copy_mode = g_copy_method; // VA_COPY_MODE_BALANCE;

    va_status = vaCopy(va_dpy, &dst_obj, &src_obj, option);
#else
    printf("incorrect libva version!\n");
    va_status = VA_STATUS_ERROR_OPERATION_FAILED;
#endif
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
    if (!(attrib.value & g_dst.format)) {
        printf("RT format %d is not supported by VPP !\n", g_dst.format);
        assert(0);
    }

    /* Create surface/config/context for VPP pipeline */
    va_status = create_surface(&g_in_surface_id, g_src);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for input");

    va_status = create_surface(&g_out_surface_id, g_dst);
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
                                g_dst.width,
                                g_dst.height,
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

    _FREE(g_src.pBuf);
    _FREE(g_dst.pBuf);
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
    } else if (!strcmp(str, "BGRA")) {
        tfourcc = VA_FOURCC_BGRA;
    } else if (!strcmp(str, "BGRX")) {
        tfourcc = VA_FOURCC_BGRX;
    } else if (!strcmp(str, "RGBP")) {
        tfourcc = VA_FOURCC_RGBP;
    } else if (!strcmp(str, "BGRP")) {
        tfourcc = VA_FOURCC_BGRP;
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
    memset(&g_src, 0, sizeof(g_src));
    memset(&g_dst, 0, sizeof(g_dst));

    /* Read src frame file information */
    read_value_string(g_config_file_fd, "SRC_FILE_NAME", g_src.name);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_WIDTH", &g_src.width);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_HEIGHT", &g_src.height);
    read_value_string(g_config_file_fd, "SRC_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_src.fourCC, &g_src.format);
    read_value_string(g_config_file_fd, "SRC_SURFACE_MEMORY_TYPE", str);
    parse_memtype_format(str, &g_src.memtype);
    read_value_uint32(g_config_file_fd, "SRC_SURFACE_CPU_ALIGN_SIZE", &g_src.alignsize);

    /* Read dst frame file information */
    read_value_string(g_config_file_fd, "DST_FILE_NAME", g_dst.name);
    read_value_uint32(g_config_file_fd, "DST_FRAME_WIDTH", &g_dst.width);
    read_value_uint32(g_config_file_fd, "DST_FRAME_HEIGHT", &g_dst.height);
    read_value_string(g_config_file_fd, "DST_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_dst.fourCC, &g_dst.format);
    read_value_string(g_config_file_fd, "DST_SURFACE_MEMORY_TYPE", str);
    parse_memtype_format(str, &g_dst.memtype);
    read_value_uint32(g_config_file_fd, "DST_SURFACE_CPU_ALIGN_SIZE", &g_dst.alignsize);

    read_value_string(g_config_file_fd, "SRC_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_src_file_fourcc, NULL);

    read_value_string(g_config_file_fd, "DST_FILE_FORMAT", str);
    parse_fourcc_and_format(str, &g_dst_file_fourcc, NULL);

    read_value_uint32(g_config_file_fd, "FRAME_SUM", &g_frame_count);
    read_value_uint32(g_config_file_fd, "COPY_METHOD", &g_copy_method);

    if (g_src.width != g_dst.width ||
        g_src.height != g_dst.height) {
        std::cout << "va copy doesn't support resize!" << endl;
        return -1;
    }

    if (g_src.fourCC != g_dst.fourCC) {
        std::cout << "va copy doesn't support CSC!" << endl;
        return -1;
    }

    std::cout << "=========Media Copy=========" << endl;

    if (g_src.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        std::cout << "copy from 2D tile surface to ";
    } else {
        if (g_src.alignsize == 1 || !(g_src.width % g_src.alignsize))
            std::cout << "copy from 1D linear surface to ";
        else
            std::cout << "copy from 2D linear surface with pitch_align " << g_src.alignsize << " to ";
    }

    if (g_dst.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        std::cout << "2D tile surface." << endl;
    } else {
        if (g_dst.alignsize == 1 || !(g_dst.width % g_dst.alignsize))
            std::cout << "1D linear surface." << endl;
        else
            std::cout << "2D linear surface with pitch_align " << g_dst.alignsize << endl;
    }
    std::cout << "prefer hw engine is " << g_copy_method << ". notification, 0: blanance(vebox), 1: perf(EU), 2 powersaving(blt)" << endl;

    return 0;
}

static void
print_help()
{
    printf("The app is used to test the scaling and csc feature.\n");
    printf("Cmd Usage: ./vacopy process_copy.cfg\n");
    printf("The configure file process_copy.cfg is used to configure the para.\n");
    printf("You can refer process_copy.cfg.template for each para meaning and create the configure file.\n");
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
    if (NULL == (g_src.fd = fopen(g_src.name, "r"))) {
        printf("Open SRC_FILE_NAME: %s failed, please specify it in config file: %s !\n",
               g_src.name, g_config_file_name);
        assert(0);
    }

    if (NULL == (g_dst.fd = fopen(g_dst.name, "w"))) {
        printf("Open DST_FILE_NAME: %s failed, please specify it in config file: %s !\n",
               g_dst.name, g_config_file_name);
        assert(0);
    }

    printf("\nStart to process, ...\n");
    struct timespec Pre_time;
    struct timespec Cur_time;
    unsigned int duration = 0;
    clock_gettime(CLOCK_MONOTONIC, &Pre_time);

    for (i = 0; i < g_frame_count; i ++) {
        upload_frame_to_surface(g_src.fd, g_in_surface_id);
        if (VA_STATUS_SUCCESS != video_frame_process(g_in_surface_id, g_out_surface_id)) {
            std::cout << "***vaCopy failed***" << std::endl;
        }
        store_surface_to_file(g_dst.fd, g_out_surface_id);
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

    if (g_src.fd)
        fclose(g_src.fd);

    if (g_dst.fd)
        fclose(g_dst.fd);

    if (g_config_file_fd)
        fclose(g_config_file_fd);

    vpp_context_destroy();

    return 0;
}

