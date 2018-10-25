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
 * it is a real program to show how VAAPI decode work,
 * It does VLD decode for a simple AVC clip.
 * The bitstream and VA parameters are hardcoded into avcstreamoutdemo.cpp,
 *
 * ./avcstreamoutdemo  : only do decode
 * ./avcstreamoutdemo <any parameter >: do decode and dump mv info
 *
 */  
 
#include "avcdemo.h"
#include "avcstreamoutdemo.h"

#define IF_EQUAL(a, b)         (a == b)
#define IF_EQUAL_M(a, b, c, d) (a == b && a == c && a == d && b == c && b == d && c == d)

void dumpMvs(VADecStreamOutData *streamout, int mbIndex)
{
    if(IF_EQUAL_M(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x, streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_x)
        && IF_EQUAL_M(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y, streamout->QW8[2].MvFwd_y, streamout->QW8[2].MvFwd_y)) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*          %3s            *\n", streamout->DW0.MbSkipFlag != 0 ? "Skip" : "    ");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n\n");
    }
    else if((IF_EQUAL(streamout->QW8[1].MvFwd_x, streamout->QW8[3].MvFwd_x)
        && IF_EQUAL(streamout->QW8[1].MvFwd_y, streamout->QW8[3].MvFwd_y))
        && (IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[2].MvFwd_x)
        && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[2].MvFwd_y))
        && !(IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x)
        && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y))) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_x, streamout->QW8[1].MvFwd_y);
        printf("*            *             *\n");
        printf("****************************\n\n");
    }
    else if((IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x)
        && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y))
        && (IF_EQUAL(streamout->QW8[2].MvFwd_x, streamout->QW8[3].MvFwd_x)
        && IF_EQUAL(streamout->QW8[2].MvFwd_y, streamout->QW8[3].MvFwd_y))
        && !(IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[2].MvFwd_x)
        && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[2].MvFwd_y))) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n\n");
    }
    else {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_x, streamout->QW8[1].MvFwd_y);
        printf("*            *             *\n");
        printf("***************************\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_y, streamout->QW8[3].MvFwd_x, streamout->QW8[3].MvFwd_y);
        printf("*            *             *\n");
        printf("****************************\n\n");
    }
}

int main(int argc,char **argv)
{
    VAEntrypoint entrypoints[5];
    int num_entrypoints,vld_entrypoint;
    VAConfigAttrib attrib;
    VAConfigID config_id;
    VASurfaceID surface_ids[AVC_SURFACE_NUM];
    VAContextID context_id;
    VABufferID pic_param_buf,iqmatrix_buf,slice_param_buf,slice_data_buf,streamout_buf;
    VABufferID tmp_buff_ids[5];
    int major_ver, minor_ver;
    VADisplay   va_dpy;
    VAStatus va_status;
    int is_dump_streamout = 0;
	int surface_index;
    VASurfaceStatus surface_status;
    unsigned char *pbuf;
    unsigned int mb_counts = ((CLIP_WIDTH+ 15) / 16) * ((CLIP_HEIGHT+ 15) / 16);
    unsigned int streamout_buffsize = mb_counts * sizeof(VADecStreamOutData);

    va_init_display_args(&argc, argv);

    if (argc > 1)
        is_dump_streamout = 1;
    
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

    /* Assuming finding VLD, find out the format for the render target */
    attrib.type = VAConfigAttribRTFormat;
    vaGetConfigAttributes(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                          &attrib, 1);
    if ((attrib.value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        assert(0);
    }
    
    va_status = vaCreateConfig(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                              &attrib, 1,&config_id);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    va_status = vaCreateSurfaces(
        va_dpy,
        VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
        &surface_ids[0], 2,
        NULL, 0
    );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* Create a context for this decode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                               CLIP_WIDTH,
                               ((CLIP_HEIGHT+15)/16)*16,
                               VA_PROGRESSIVE,
                               &surface_ids[0],
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
                                      &slice_param_surface0[0], &slice_param_buf);
        }
        else {
            va_status = vaCreateBuffer(va_dpy, context_id,
                                  VASliceParameterBufferType,
                                  sizeof(VASliceParameterBufferH264),
                                  2,
                                  &slice_param_surface1[0], &slice_param_buf);
        }
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    
        va_status = vaCreateBuffer(va_dpy, context_id,
                                  VASliceDataBufferType,
                                  surface_index == 0 ? avc_clip_size : avc_clip1_size,
                                  1,
                                  surface_index == 0 ? avc_clip : avc_clip1,
                                  &slice_data_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    
        /* Create StreamOut va buffer */    
        va_status = vaCreateBuffer(va_dpy, context_id,
                                  VADecodeStreamoutBufferType,
                                  streamout_buffsize,
                                  1, NULL, &streamout_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    
        tmp_buff_ids[0] = pic_param_buf;
        tmp_buff_ids[1] = iqmatrix_buf;
        tmp_buff_ids[2] = slice_param_buf;
        tmp_buff_ids[3] = slice_data_buf;
        tmp_buff_ids[4] = streamout_buf;
    
        va_status = vaBeginPicture(va_dpy, context_id, surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
    
        va_status = vaRenderPicture(va_dpy,context_id, tmp_buff_ids, 5);
        CHECK_VASTATUS(va_status, "vaRenderPicture");
        
        va_status = vaEndPicture(va_dpy,context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");
    
        va_status = vaSyncSurface(va_dpy, surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaSyncSurface");
    
        va_status = vaQuerySurfaceStatus(va_dpy, surface_ids[surface_index], &surface_status);//to check surface_status if needed
        CHECK_VASTATUS(va_status, "vaQuerySurfaceStatus");
    
        /*map streamout buffer to dump*/
        VADecStreamOutData *dec_streamout_buf = (VADecStreamOutData *)malloc(streamout_buffsize);
        if (NULL == dec_streamout_buf) {
            printf("Failed to malloc for dec streamout buf.\n");
            assert(0);
        }
        va_status = vaMapBuffer(va_dpy, streamout_buf, (void **)(&pbuf));
        CHECK_VASTATUS(va_status, "vaMapBuffer");
        memcpy(dec_streamout_buf, pbuf, streamout_buffsize);// to check streamout data for usage
        va_status = vaUnmapBuffer(va_dpy, streamout_buf);
        CHECK_VASTATUS(va_status, "vaUnmapBuffer");
        
        if (is_dump_streamout && surface_index != 0) {
            //dump streamout buffer to local file
            VADecStreamOutData *temp_dec_streamout_buf = dec_streamout_buf;
			unsigned int i;
            for(i = 0; i < mb_counts && temp_dec_streamout_buf != NULL; i++) {
                dumpMvs(temp_dec_streamout_buf++, i);
            }
        }

        if(dec_streamout_buf) {
            free(dec_streamout_buf);
        }
    }
    
    printf("press any key to exit\n");
    getchar();

    vaDestroySurfaces(va_dpy,surface_ids,2);
    vaDestroyConfig(va_dpy,config_id);
    vaDestroyContext(va_dpy,context_id);

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    return 0;
}
