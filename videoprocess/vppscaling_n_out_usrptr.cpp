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
 * This test covers 1:N(N>=2) output for scaling and several surface format conversion.
 * also support 2nd scale, regarding the 1st scale output as input for 2nd scale
 * also support  UserPtr 16 alignment as NV12/YV12/YUY2 input
 * support none (0, 0) top/left in render target as RGB/YV12 output
 * support none (0, 0) top/left input crop
 * Usage: ./vppscaling_n_out_usrptr process_scaling_n_out_usrptr.cfg
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
#if 0
#include <va/va_x11.h>
#endif

#define MAX_LEN   1024

#define CHECK_VASTATUS(va_status,func)                                      \
    if (va_status != VA_STATUS_SUCCESS) {                                     \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                              \
    }

#define VPP_FREE(p)                       \
    if(p != NULL){                      \
        free(p);  p = NULL;                        \
    }

typedef struct {
    char                file_name[100];
    FILE*               file_fd;
    uint32_t            pic_width;
    uint32_t            pic_height;
    uint32_t            fourcc;
    uint32_t            rtformat;
    uint32_t            memtype;
    uint32_t            align_mode;
    void                *pBuf;
    uint8_t             *pUserBase;
    uintptr_t           ptrb;
} VPP_ImageInfo;


#if 0
static Display     *x11_display = NULL;
#endif
static VADisplay   va_dpy = NULL;
static VAContextID context_id = 0;
static VAConfigID  config_id = 0;
static VASurfaceID g_in_surface_id = VA_INVALID_ID;
static VASurfaceID *g_out_surface_ids = NULL;

static FILE* g_config_file_fd = NULL;
static char g_config_file_name[MAX_LEN];
static VPP_ImageInfo    g_src_info;
static VPP_ImageInfo    *g_dst_info = NULL;

static uint32_t g_frame_count = 0;
static uint32_t g_dst_count = 1;
static uint32_t g_scale_again = 0;


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

static int8_t
read_value_int16(FILE* fp, const char* field_name, int16_t* value)
{
    char str[MAX_LEN];

    if (read_value_string(fp, field_name, str)) {
        printf("Failed to find integer field: %s", field_name);
        return -1;
    }

    *value = (int16_t)atoi(str);
    return 0;
}

static int8_t
read_value_uint16(FILE* fp, const char* field_name, uint16_t* value)
{
    char str[MAX_LEN];

    if (read_value_string(fp, field_name, str)) {
        printf("Failed to find integer field: %s", field_name);
        return -1;
    }

    *value = (uint16_t)atoi(str);
    return 0;
}


static VAStatus
create_surface(VPP_ImageInfo &img_info, VASurfaceID * p_surface_id)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    if (img_info.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
        VASurfaceAttrib    surface_attrib;
        surface_attrib.type =  VASurfaceAttribPixelFormat;
        surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib.value.type = VAGenericValueTypeInteger;
        surface_attrib.value.value.i = img_info.fourcc;

        va_status = vaCreateSurfaces(va_dpy,
                                     img_info.rtformat,
                                     img_info.pic_width,
                                     img_info.pic_height,
                                     p_surface_id,
                                     1,
                                     &surface_attrib,
                                     1);
        CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    } else if (img_info.memtype == VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR) {
        VASurfaceAttrib surfaceAttrib[3];
        VASurfaceAttribExternalBuffers extBuffer;
        uint32_t base_addr_align = 0x1000;
        //VAAppSurfaceResInfo surfaceinfo;
        //va_status = vaQuerySurfaceAllocation(va_dpy, format, fourCC,width, height, VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR, &surfaceinfo);
        uint32_t size = 0;
        surfaceAttrib[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[0].type = VASurfaceAttribPixelFormat;
        surfaceAttrib[0].value.type = VAGenericValueTypeInteger;
        surfaceAttrib[0].value.value.i = img_info.fourcc;

        surfaceAttrib[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[1].type = VASurfaceAttribMemoryType;
        surfaceAttrib[1].value.type = VAGenericValueTypeInteger;
        surfaceAttrib[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

        surfaceAttrib[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surfaceAttrib[2].type = VASurfaceAttribExternalBufferDescriptor;
        surfaceAttrib[2].value.type = VAGenericValueTypePointer;
        surfaceAttrib[2].value.value.p = (void *)&extBuffer;
        memset(&extBuffer, 0, sizeof(extBuffer));

        uint32_t pitch_align = img_info.align_mode;
        switch (img_info.fourcc) {
        case VA_FOURCC_NV12:
            extBuffer.pitches[0] = ((img_info.pic_width + pitch_align - 1) / pitch_align) * pitch_align;
            size = (extBuffer.pitches[0] * img_info.pic_height) * 3 / 2;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align;
            extBuffer.offsets[0] = 0;
            extBuffer.offsets[1] = extBuffer.pitches[0] * img_info.pic_height;
            extBuffer.pitches[1] = extBuffer.pitches[0];
            extBuffer.num_planes = 2;
            break;
        case VA_FOURCC_YUY2:
            extBuffer.pitches[0] = ((img_info.pic_width + pitch_align - 1) / pitch_align) * pitch_align;
            size = (extBuffer.pitches[0] * (img_info.pic_height + 2)) * 2;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align;
            extBuffer.offsets[0] = 0;
            extBuffer.pitches[0] = extBuffer.pitches[0] * 2;
            extBuffer.num_planes = 1;
            break;
        case VA_FOURCC_YV12: {
            int y_align = 32;
            int uv_align = 16;
            extBuffer.pitches[0] = ((img_info.pic_width + y_align - 1) / y_align) * y_align;
            extBuffer.pitches[1] = ((img_info.pic_width / 2 + uv_align - 1) / uv_align) * uv_align;
            extBuffer.pitches[2] = extBuffer.pitches[1];
            size = extBuffer.pitches[0] * img_info.pic_height + extBuffer.pitches[1] * img_info.pic_height;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align;
            extBuffer.offsets[0] = 0;
            extBuffer.offsets[1] = extBuffer.pitches[0] * img_info.pic_height;
            extBuffer.offsets[2] = extBuffer.offsets[1] + extBuffer.pitches[1] * img_info.pic_height / 2;
            extBuffer.num_planes = 3;
            break;
        }
        case VA_FOURCC_ARGB:
            extBuffer.pitches[0] = ((img_info.pic_width + pitch_align - 1) / pitch_align) * pitch_align;
            size = (extBuffer.pitches[0] * (img_info.pic_height  + 2)) * 4;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align;
            extBuffer.offsets[0] = 0;
            extBuffer.pitches[0] = extBuffer.pitches[0] * 4;
            extBuffer.num_planes = 1;
            break;
        default:
            break;
        }
        img_info.pBuf = malloc(size + base_addr_align);
        img_info.pUserBase = (uint8_t*)((((uint64_t)(img_info.pBuf) + base_addr_align - 1) / base_addr_align) * base_addr_align);

        extBuffer.pixel_format = img_info.fourcc;
        extBuffer.width = img_info.pic_width;
        extBuffer.height = img_info.pic_height;
        extBuffer.data_size = size;

        extBuffer.num_buffers = 1;
        extBuffer.buffers = &(img_info.ptrb);
        extBuffer.buffers[0] = (uintptr_t)(img_info.pUserBase);

        extBuffer.flags = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
        va_status = vaCreateSurfaces(va_dpy, img_info.rtformat, img_info.pic_width, img_info.pic_height, p_surface_id, 1, surfaceAttrib, 3);
        CHECK_VASTATUS(va_status, "vaCreateSurfaces");
    }
    return va_status;
}


/* Load yuv frame to NV12/YUY2/YV12/ARGB surface*/
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
    uint32_t frame_size, row;
    size_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    if (surface_image.format.fourcc == VA_FOURCC_NV12) {
        frame_size = surface_image.width * surface_image.height * 3 / 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        u_src = newImageBuffer + surface_image.width * surface_image.height;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);

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
            u_dst += surface_image.pitches[1];
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_YUY2) {
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
    } else if (surface_image.format.fourcc == VA_FOURCC_YV12) {
        frame_size = surface_image.width * surface_image.height * 3 / 2;
        newImageBuffer = (unsigned char*)malloc(frame_size);
        assert(newImageBuffer);

        do {
            n_items = fread(newImageBuffer, frame_size, 1, fp);
        } while (n_items != 1);

        y_src = newImageBuffer;
        v_src = newImageBuffer + surface_image.width * surface_image.height;
        u_src = newImageBuffer + surface_image.width * surface_image.height * 5 / 4;
        y_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
        v_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);
        u_dst = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[2]);
        /* Y plane, directly copy */
        for (row = 0; row < surface_image.height; row++) {
            memcpy(y_dst, y_src, surface_image.width);
            y_dst += surface_image.pitches[0];
            y_src += surface_image.width;
        }
        for (row = 0; row < surface_image.height / 2; row ++) {
            memcpy(v_dst, v_src, surface_image.width / 2);
            memcpy(u_dst, u_src, surface_image.width / 2);
            v_src += surface_image.width / 2;
            u_src += surface_image.width / 2;
            v_dst += surface_image.pitches[1];
            u_dst += surface_image.pitches[2];
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_ARGB) {
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
    }

    if (newImageBuffer) {
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
store_yuv_surface_to_nv12_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    uint32_t row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;
    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one nv12 file */
    uint32_t y_size = surface_image.width * surface_image.height;

    newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
    assert(newImageBuffer);

    /* stored as YV12 format */
    y_dst = newImageBuffer;
    u_dst = newImageBuffer + y_size;

    y_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[0]);
    u_src = (unsigned char *)((unsigned char*)surface_p + surface_image.offsets[1]);

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
    /* write frame to file */
    do {
        n_items = fwrite(newImageBuffer, y_size * 3 / 2, 1, fp);
    } while (n_items != 1);

    if (newImageBuffer) {
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
store_packed_yuv_surface_to_yuy2_file(FILE *fp,
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
    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    /* store the surface to one YUY2 file */
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

    if (newImageBuffer) {
        free(newImageBuffer);
        newImageBuffer = NULL;
    }

    va_status = vaUnmapBuffer(va_dpy, surface_image.buf);
    CHECK_VASTATUS(va_status, "vaUnmapBuffer");
    va_status = vaDestroyImage(va_dpy, surface_image.image_id);
    CHECK_VASTATUS(va_status, "vaDestroyImage");

    return VA_STATUS_SUCCESS;
}

/* Store YV12 surface to yv12 file */
static VAStatus
store_yuv_surface_to_yv12_file(FILE *fp,
                               VASurfaceID surface_id)
{
    VAStatus va_status;
    VAImage surface_image;
    void *surface_p = NULL;
    unsigned char *y_src, *u_src, *v_src;
    unsigned char *y_dst, *u_dst, *v_dst;
    uint32_t row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    uint32_t y_size = surface_image.width * surface_image.height;
    uint32_t u_size = y_size / 4;

    newImageBuffer = (unsigned char*)malloc(y_size * 3 / 2);
    assert(newImageBuffer);

    /* stored as YV12 format */
    y_dst = newImageBuffer;
    v_dst = newImageBuffer + y_size;
    u_dst = newImageBuffer + y_size + u_size;

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
    for (row = 0; row < surface_image.height / 2; row ++) {
        memcpy(v_dst, v_src, surface_image.width / 2);
        memcpy(u_dst, u_src, surface_image.width / 2);
        v_dst += surface_image.width / 2;
        u_dst += surface_image.width / 2;
        v_src += surface_image.pitches[1];
        u_src += surface_image.pitches[2];
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
    unsigned char *y_src;
    unsigned char *y_dst;
    uint32_t frame_size, row;
    int32_t n_items;
    unsigned char * newImageBuffer = NULL;

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
                          VASurfaceID surface_id, uint32_t  out_fourcc)
{
    if (out_fourcc == VA_FOURCC_NV12) {
        return store_yuv_surface_to_nv12_file(fp, surface_id);
    } else if (out_fourcc == VA_FOURCC_YUY2) {
        return store_packed_yuv_surface_to_yuy2_file(fp, surface_id);
    } else if (out_fourcc == VA_FOURCC_YV12) {
        return store_yuv_surface_to_yv12_file(fp, surface_id);
    } else if (out_fourcc == VA_FOURCC_ARGB) {
        return store_rgb_surface_to_rgb_file(fp, surface_id);
    } else {
        printf("Not supported YUV fourcc for output !!!\n");
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }
}

static VAStatus
video_frame_process(VASurfaceID in_surface_id,
                    VASurfaceID *out_surface_ids)
{
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    /* Fill pipeline buffer */
    memset(&pipeline_param, 0, sizeof(pipeline_param));
    pipeline_param.surface = in_surface_id;
    if (g_dst_count > 1 && g_scale_again == 0) {
        pipeline_param.additional_outputs = &out_surface_ids[1];
        pipeline_param.num_additional_outputs = g_dst_count - 1;
    }
    uint32_t input_crop = 0;
    read_value_uint32(g_config_file_fd, "SRC_SURFACE_CROP", &input_crop);
    if (input_crop == 0) {
        surface_region.x = 0;
        surface_region.y = 0;
        surface_region.width = g_src_info.pic_width;
        surface_region.height = g_src_info.pic_height;
    } else {
        //do the input crop
        read_value_int16(g_config_file_fd, "SRC_CROP_LEFT_X", &surface_region.x);
        read_value_int16(g_config_file_fd, "SRC_CROP_TOP_Y", &surface_region.y);
        read_value_uint16(g_config_file_fd, "SRC_CROP_WIDTH", &surface_region.width);
        read_value_uint16(g_config_file_fd, "SRC_CROP_HEIGHT", &surface_region.height);
    }

    uint32_t output_crop = 0;
    read_value_uint32(g_config_file_fd, "DST_SURFACE_CROP", &output_crop);
    if (output_crop == 0) {
        output_region.x = 0;
        output_region.y = 0;
        output_region.width = g_dst_info[0].pic_width;
        output_region.height = g_dst_info[0].pic_height;
    } else {
        //do the output crop
        read_value_int16(g_config_file_fd, "DST_CROP_LEFT_X", &output_region.x);
        read_value_int16(g_config_file_fd, "DST_CROP_TOP_Y", &output_region.y);
        read_value_uint16(g_config_file_fd, "DST_CROP_WIDTH", &output_region.width);
        read_value_uint16(g_config_file_fd, "DST_CROP_HEIGHT", &output_region.height);
    }
    pipeline_param.surface_region = &surface_region;
    pipeline_param.output_region = &output_region;
    pipeline_param.output_background_color = 0xff000000;
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
                               out_surface_ids[0]);
    CHECK_VASTATUS(va_status, "vaBeginPicture");

    va_status = vaRenderPicture(va_dpy,
                                context_id,
                                &pipeline_param_buf_id,
                                1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(va_dpy, context_id);
    CHECK_VASTATUS(va_status, "vaEndPicture");

    if (pipeline_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, pipeline_param_buf_id);
        CHECK_VASTATUS(va_status, "vaDestroyBuffer");
    }
    if (g_scale_again == 1) {
        printf("begin to scale the 16align out as input\n");
        memset(&pipeline_param, 0, sizeof(pipeline_param));
        pipeline_param.surface = out_surface_ids[0];

        va_status = vaCreateBuffer(va_dpy,
                                   context_id,
                                   VAProcPipelineParameterBufferType,
                                   sizeof(pipeline_param),
                                   1,
                                   &pipeline_param,
                                   &pipeline_param_buf_id);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
        va_status = vaBeginPicture(va_dpy, context_id, out_surface_ids[1]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
        va_status = vaRenderPicture(va_dpy, context_id, &pipeline_param_buf_id, 1);
        CHECK_VASTATUS(va_status, "vaRenderPicture");
        va_status = vaEndPicture(va_dpy, context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");
        if (pipeline_param_buf_id != VA_INVALID_ID) {
            vaDestroyBuffer(va_dpy, pipeline_param_buf_id);
            CHECK_VASTATUS(va_status, "vaDestroyBuffer");
        }
    }

    return va_status;
}

static VAStatus
vpp_context_create()
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    int32_t j;

    /* VA driver initialization */
#if 0
    x11_display = XOpenDisplay("intel-CoffeeLake-Client-Platform:2");
    if (NULL == x11_display) {
        printf("Error: Can't connect X server! %s %s(line %d)\n", __FILE__, __func__, __LINE__);
        return false;
    }
    va_dpy = vaGetDisplay(x11_display);
#endif
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

    /* Create surface/config/context for VPP pipeline */
    va_status = create_surface(g_src_info, &g_in_surface_id);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for input");

    for (uint32_t i = 0; i < g_dst_count; i++) {
        va_status = create_surface(g_dst_info[i], &g_out_surface_ids[i]);
    }
    CHECK_VASTATUS(va_status, "vaCreateSurfaces for output");
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    attrib.value = VA_RT_FORMAT_YUV420;

    va_status = vaCreateConfig(va_dpy,
                               VAProfileNone,
                               VAEntrypointVideoProc,
                               &attrib,
                               1,
                               &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    va_status = vaCreateContext(va_dpy,
                                config_id,
                                g_dst_info[0].pic_width,
                                g_dst_info[0].pic_height,
                                VA_PROGRESSIVE,
                                g_out_surface_ids,
                                g_dst_count,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
    return va_status;
}

static void
vpp_context_destroy()
{
    VAStatus va_status;

    /* Release resource */
    va_status = vaDestroySurfaces(va_dpy, &g_in_surface_id, 1);
    CHECK_VASTATUS(va_status, "vaDestroySurfaces");
    VPP_FREE(g_src_info.pBuf);
    for (uint32_t index = 0; index < g_dst_count; index++) {
        vaDestroySurfaces(va_dpy, &g_out_surface_ids[index], 1);
        CHECK_VASTATUS(va_status, "vaDestroySurfaces");
        VPP_FREE(g_dst_info[index].pBuf);
    }
    VPP_FREE(g_dst_info);
    VPP_FREE(g_out_surface_ids);
    vaDestroyContext(va_dpy, context_id);
    CHECK_VASTATUS(va_status, "vaDestroyContext");
    vaDestroyConfig(va_dpy, config_id);
    CHECK_VASTATUS(va_status, "vaDestroyConfig");
    vaTerminate(va_dpy);
    CHECK_VASTATUS(va_status, "vaTerminate");
#if 0
    XCloseDisplay(x11_display);
    x11_display = NULL;
#endif
    va_close_display(va_dpy);
}

static int8_t
parse_fourcc_and_format(char *str, uint32_t *fourcc, uint32_t *format)
{
    uint32_t tfourcc = VA_FOURCC('N', 'V', '1', '2');
    uint32_t tformat = VA_RT_FORMAT_YUV420;

    if (!strcmp(str, "NV12")) {
        tfourcc = VA_FOURCC('N', 'V', '1', '2');
    } else if (!strcmp(str, "YUY2")) {
        tfourcc = VA_FOURCC('Y', 'U', 'Y', '2');
    } else if (!strcmp(str, "YV12")) {
        tfourcc = VA_FOURCC('Y', 'V', '1', '2');
    } else if (!strcmp(str, "ARGB")) {
        tfourcc = VA_FOURCC('A', 'R', 'G', 'B');
    } else {
        printf("Not supported format: %s! Currently only support following format: %s for this sample\n",
               str, "NV12, YUY2,YV12,ARGB");
        assert(0);
    }

    if (fourcc)
        *fourcc = tfourcc;

    if (format)
        *format = tformat;

    return 0;
}

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
parse_basic_parameters()
{
    char str[MAX_LEN];

    /* Read src frame file information */
    read_value_string(g_config_file_fd, "SRC_FILE_NAME", g_src_info.file_name);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_WIDTH", &g_src_info.pic_width);
    read_value_uint32(g_config_file_fd, "SRC_FRAME_HEIGHT", &g_src_info.pic_height);
    read_value_string(g_config_file_fd, "SRC_FRAME_FORMAT", str);
    parse_fourcc_and_format(str, &g_src_info.fourcc, &g_src_info.rtformat);
    read_value_string(g_config_file_fd, "SRC_SURFACE_MEMORY_TYPE", str);
    parse_memtype_format(str, &g_src_info.memtype);
    read_value_uint32(g_config_file_fd, "SRC_SURFACE_CPU_ALIGN_MODE", &g_src_info.align_mode);

    read_value_uint32(g_config_file_fd, "2ND_SCALE", &g_scale_again);

    /* Read dst frame file information */
    read_value_uint32(g_config_file_fd, "DST_NUMBER", &g_dst_count);
    g_out_surface_ids = (VASurfaceID*)malloc(g_dst_count * sizeof(VASurfaceID));
    g_dst_info = (VPP_ImageInfo *)malloc(g_dst_count * sizeof(VPP_ImageInfo));
    for (uint32_t i = 0; i < g_dst_count; i++) {
        char dst_file_name[MAX_LEN];
        char dst_frame_width[MAX_LEN];
        char dst_frame_height[MAX_LEN];
        char dst_frame_format[MAX_LEN];
        char dst_memtype[MAX_LEN];
        char dst_align_mode[MAX_LEN];
        sprintf(dst_file_name, "DST_FILE_NAME_%d", i + 1);
        sprintf(dst_frame_width, "DST_FRAME_WIDTH_%d", i + 1);
        sprintf(dst_frame_height, "DST_FRAME_HEIGHT_%d", i + 1);
        sprintf(dst_frame_format, "DST_FRAME_FORMAT_%d", i + 1);
        sprintf(dst_memtype, "DST_SURFACE_MEMORY_TYPE_%d", i + 1);
        sprintf(dst_align_mode, "DST_SURFACE_CPU_ALIGN_MODE_%d", i + 1);
        read_value_string(g_config_file_fd, dst_file_name, g_dst_info[i].file_name);
        read_value_uint32(g_config_file_fd, dst_frame_width, &g_dst_info[i].pic_width);
        read_value_uint32(g_config_file_fd, dst_frame_height, &g_dst_info[i].pic_height);
        read_value_string(g_config_file_fd, dst_frame_format, str);
        parse_fourcc_and_format(str, &g_dst_info[i].fourcc, &g_dst_info[i].rtformat);
        read_value_string(g_config_file_fd, dst_memtype, str);
        parse_memtype_format(str, &g_dst_info[i].memtype);
        read_value_uint32(g_config_file_fd, dst_align_mode, &g_dst_info[i].align_mode);
    }
    read_value_uint32(g_config_file_fd, "FRAME_SUM", &g_frame_count);
    return 0;
}

static void
print_help()
{
    printf("The app is used to test the usrptr and 1:N output and crop feature.\n");
    printf("Cmd Usage: ./vppscaling_n_out_usrptr process_scaling_n_out_usrptr.cfg\n");
    printf("The configure file process_scaling_n_out_usrptr.cfg is used to configure the para.\n");
    printf("You can refer process_scaling_n_out_usrptr.cfg.template for each para meaning and create the configure file.\n");
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
    if (NULL == (g_src_info.file_fd = fopen(g_src_info.file_name, "r"))) {
        printf("Open SRC_FILE_NAME: %s failed, please specify it in config file: %s !\n",
               g_src_info.file_name, g_config_file_name);
        assert(0);
    }
    for (uint32_t index = 0; index < g_dst_count; index++) {
        if (NULL == (g_dst_info[index].file_fd = fopen(g_dst_info[index].file_name, "w"))) {
            printf("Open DST_FILE_NAME: %s failed, please specify it in config file: %s !\n",
                   g_dst_info[index].file_name, g_config_file_name);
            assert(0);
        }
    }

    printf("\nStart to process, ...\n");
    struct timespec Pre_time;
    struct timespec Cur_time;
    unsigned int duration = 0;
    clock_gettime(CLOCK_MONOTONIC, &Pre_time);

    for (i = 0; i < g_frame_count; i ++) {
        upload_yuv_frame_to_yuv_surface(g_src_info.file_fd, g_in_surface_id);
        video_frame_process(g_in_surface_id, g_out_surface_ids);
        //first sync surface to check the process ready
        va_status = vaSyncSurface(va_dpy, g_out_surface_ids[g_dst_count - 1]);
        CHECK_VASTATUS(va_status, "vaSyncSurface");
        for (uint32_t index = 0; index < g_dst_count; index++) {
            store_yuv_surface_to_file(g_dst_info[index].file_fd, g_out_surface_ids[index], g_dst_info[index].fourcc);
        }
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

    if (g_src_info.file_fd)
        fclose(g_src_info.file_fd);
    for (uint32_t index = 0; index < g_dst_count; index++) {
        if (g_dst_info[index].file_fd)
            fclose(g_dst_info[index].file_fd);
    }
    if (g_config_file_fd)
        fclose(g_config_file_fd);

    vpp_context_destroy();

    return 0;
}
