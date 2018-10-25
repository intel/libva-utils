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

/*
 * it is a real program to show how VAAPI decode + vpp work,
 * It does VLD decode for a simple AVC clip and down-scaling by VPP.
 * The bitstream and VA parameters are hardcoded into avcstream.cpp,
 *
 * ./avcdecprocdemo  : decode and process the hardcoded AVC clip.
 * The scaling factor is [1/2, 1/2].
 */

#include "avcdemo.h"

#define OUT_WIDTH  128
#define OUT_HEIGHT 128

VADisplay va_dpy;
VAConfigID config_id;
VAContextID context_id;

const char out_dec_yuv_name[] = "dec.yuv";
const char out_proc_yuv_name[] = "proc.yuv";

void output_yuv(VASurfaceID* surf, int num, const char* filename)
{
    FILE* yuv_file = NULL;
    int i;

    if (NULL == (yuv_file = fopen(filename, "w"))) {
        printf("Fail to open output yuv file: %s!\n", filename);
        assert(0);
        return;
    }

    for (i = 0; i < num; i++)
        store_yuv_surface_to_yv12_file(yuv_file, surf[i], va_dpy);

    if (yuv_file)
            fclose(yuv_file);
}

int main(int argc,char **argv)
{
    VAEntrypoint entrypoints[5];
    int num_entrypoints,vld_entrypoint;
    VAConfigAttrib attribs[2];
    VASurfaceID dec_surface_ids[AVC_SURFACE_NUM];
    VASurfaceID proc_surface_ids[AVC_SURFACE_NUM];
    VABufferID pic_param_buf,iqmatrix_buf,slice_param_buf,slice_data_buf;
    VABufferID tmp_buff_ids[5];
    int major_ver, minor_ver;
    VAStatus va_status;
	int surface_index;
    VASurfaceStatus surface_status;

    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;

    printf("Decoding and down scaling starts...\n");

    va_init_display_args(&argc, argv);

    va_dpy = va_open_display();
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

    va_status = vaQueryConfigEntrypoints(va_dpy, VAProfileH264Main, entrypoints,
                             &num_entrypoints);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    for(vld_entrypoint = 0; vld_entrypoint < num_entrypoints; vld_entrypoint++) {
        if (entrypoints[vld_entrypoint] == VAEntrypointVLD)
            break;
    }
    if (vld_entrypoint == num_entrypoints) {
        /* not find VLD entry point */
        assert(0);
    }

    /* Assuming finding VLD, find out the attributes for the render target */
    attribs[0].type = VAConfigAttribRTFormat;
    attribs[1].type = VAConfigAttribDecProcessing;

    vaGetConfigAttributes(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                          attribs, 2);
    if ((attribs[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        assert(0);
    }

    if (attribs[1].value == VA_DEC_PROCESSING_NONE) {
        printf("VAConfigAttribDecProcessing isn't supported. Exit.\n");
        return -1;
    }

    va_status = vaCreateConfig(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                               attribs, 2, &config_id);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    va_status = vaCreateSurfaces(
        va_dpy,
        VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
        &dec_surface_ids[0], 2,
        NULL, 0
    );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    va_status = vaCreateSurfaces(va_dpy,
                                 VA_RT_FORMAT_YUV420, OUT_WIDTH, OUT_HEIGHT,
                                 &proc_surface_ids[0], 2,
                                 NULL, 0);

    CHECK_VASTATUS(va_status, "vaCreateSurfaces");
    /* Create a context for this decode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                                CLIP_WIDTH,
                                ((CLIP_HEIGHT+15)/16)*16,
                                VA_PROGRESSIVE,
                                &dec_surface_ids[0],
                                2,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");

    for(surface_index = 0 ; surface_index < AVC_SURFACE_NUM; surface_index++)
    {
        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VAPictureParameterBufferType,
                                   sizeof(VAPictureParameterBufferH264),
                                   1, &pic_param[surface_index],
                                   &pic_param_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VAIQMatrixBufferType,
                                   sizeof(VAIQMatrixBufferH264),
                                   1, &iq_matrix[surface_index],
                                   &iqmatrix_buf );
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        if(surface_index == 0) {
            va_status = vaCreateBuffer(va_dpy, context_id,
                                       VASliceParameterBufferType,
                                       sizeof(VASliceParameterBufferH264),
                                       4,
                                       &slice_param_surface0[0],
                                       &slice_param_buf);
        }
        else {
            va_status = vaCreateBuffer(va_dpy, context_id,
                                       VASliceParameterBufferType,
                                       sizeof(VASliceParameterBufferH264),
                                       2,
                                       &slice_param_surface1[0],
                                       &slice_param_buf);
        }
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_status = vaCreateBuffer(va_dpy, context_id,
                                  VASliceDataBufferType,
                                  surface_index == 0 ? avc_clip_size : avc_clip1_size,
                                  1,
                                  surface_index == 0 ? avc_clip : avc_clip1,
                                  &slice_data_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        /* Fill pipeline buffer with down scaling */
        surface_region.x = 0;
        surface_region.y = 0;
        surface_region.width = CLIP_WIDTH;
        surface_region.height = CLIP_HEIGHT;
        output_region.x = 0;
        output_region.y = 0;
        output_region.width = CLIP_WIDTH / 2;
        output_region.height = CLIP_HEIGHT / 2;

        memset(&pipeline_param, 0, sizeof(pipeline_param));
        pipeline_param.additional_outputs = &proc_surface_ids[surface_index];
        pipeline_param.num_additional_outputs = 1;
        pipeline_param.surface_region = &surface_region;
        pipeline_param.output_region = &output_region;

        va_status = vaCreateBuffer(va_dpy,
                                   context_id,
                                   VAProcPipelineParameterBufferType,
                                   sizeof(pipeline_param),
                                   1,
                                   &pipeline_param,
                                   &pipeline_param_buf_id);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        tmp_buff_ids[0] = pic_param_buf;
        tmp_buff_ids[1] = iqmatrix_buf;
        tmp_buff_ids[2] = slice_param_buf;
        tmp_buff_ids[3] = slice_data_buf;
        tmp_buff_ids[4] = pipeline_param_buf_id;

        va_status = vaBeginPicture(va_dpy, context_id,
                                   dec_surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");

        va_status = vaRenderPicture(va_dpy, context_id, tmp_buff_ids, 5);
        CHECK_VASTATUS(va_status, "vaRenderPicture");

        va_status = vaEndPicture(va_dpy, context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");

        va_status = vaSyncSurface(va_dpy, dec_surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaSyncSurface");

        //to check surface_status if needed
        va_status = vaQuerySurfaceStatus(va_dpy, dec_surface_ids[surface_index],
                                         &surface_status);
        CHECK_VASTATUS(va_status, "vaQuerySurfaceStatus");
    }

    // Restore decoded surace to file
    output_yuv(dec_surface_ids, AVC_SURFACE_NUM, out_dec_yuv_name);
    output_yuv(proc_surface_ids, AVC_SURFACE_NUM, out_proc_yuv_name);

    vaDestroySurfaces(va_dpy, dec_surface_ids, AVC_SURFACE_NUM);
    vaDestroySurfaces(va_dpy, proc_surface_ids, AVC_SURFACE_NUM);
    vaDestroyConfig(va_dpy, config_id);
    vaDestroyContext(va_dpy, context_id);

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    printf("Decoded YUV: %s Down Scaling YUV: %s\n",
           out_dec_yuv_name, out_proc_yuv_name);
    return 0;
}

