/*
* Copyright (c) 2018, Intel Corporation
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
#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_x11.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* Chroma siting mode */
enum Chroma_Siting_Mode {
    UNKNOWN                     = 0,
    CHROMA_SITING_TOP_LEFT      = 1,
    CHROMA_SITING_TOP_CENTER    = 2,
    CHROMA_SITING_CENTER_LEFT   = 3,
    CHROMA_SITING_CENTER_CENTER = 4,
    CHROMA_SITING_BOTTOM_LEFT    = 5,
    CHROMA_SITING_BOTTOM_CENTER = 6
};

typedef struct {
    char                input_file[100];
    unsigned char      *src_buffer;
    unsigned int        src_format;
    unsigned int        src_buffer_size;
    unsigned int        src_width;
    unsigned int        src_height;
    VARectangle         region_in;
    char                output_file[100];
    unsigned int        dst_width;
    unsigned int        dst_height;
    unsigned int        dst_format;
    VARectangle         region_out;
    unsigned int        chroma_siting_mode;
} VPP_ImageInfo;

typedef struct{
    char in_format[10];
    int  cc_format;
}VPP_FormatMap;

VPP_FormatMap sFomatMap[] = {{"nv12",VA_FOURCC_NV12},
                             {"i420",VA_FOURCC_I420},
                             {"yv12",VA_FOURCC_YV12},
                             {"yuy2",VA_FOURCC_YUY2},
                             {"argb",VA_FOURCC_ARGB},
                             {"abgr",VA_FOURCC_ABGR},
                             {"xrgb",VA_FOURCC_XRGB},
                             {"xbgr",VA_FOURCC_XBGR},
                             {"rgba",VA_FOURCC_RGBA},
                             {"bgra",VA_FOURCC_BGRA},
                             {"rgbx",VA_FOURCC_RGBX},
                             {"bgrx",VA_FOURCC_BGRX},
                             {"rgb565",VA_FOURCC_RGB565},
                             {"p411",VA_FOURCC_411P},
                             {"p444",VA_FOURCC_444P},
                             {"v422",VA_FOURCC_422V},
                             {"h422",VA_FOURCC_422H},
                             {"imc3",VA_FOURCC_IMC3},
                             {"p010",VA_FOURCC_P010},
                             {"ayuv", VA_FOURCC_AYUV},
                             {"uyvy", VA_FOURCC_UYVY},
                             {"y210", VA_FOURCC_Y210},
                             {"y410", VA_FOURCC_Y410},
                             {"y216", VA_FOURCC_Y216},
                             {"y416", VA_FOURCC_Y416}};

Display        *x11_display;
VADisplay      _va_dpy;
VAConfigID     _cfg_id;
VAContextID    _context_id;
VASurfaceID    in_surface;
VASurfaceID    out_surface;

VABufferID     vp_pipeline_inbuf = VA_INVALID_ID;
VPP_ImageInfo  vpp_Imageinfo;

bool copyToVaSurface( VASurfaceID surface_id )
{
    VAImage       va_image;
    VAStatus      va_status;
    void          *surface_p = NULL;
    unsigned char *src_buffer;
    unsigned char *y_src, *u_src,*v_src;

    unsigned char *y_dst, *u_dst,*v_dst;
    int           y_size = vpp_Imageinfo.src_width * vpp_Imageinfo.src_height;
    int           u_size = (vpp_Imageinfo.src_width >> 1) * (vpp_Imageinfo.src_height >> 1); //default, for special, will update
    int           i, rgb_shift_factor = 2;

    src_buffer = vpp_Imageinfo.src_buffer;

    va_status = vaDeriveImage(_va_dpy, surface_id, &va_image);
    va_status = vaMapBuffer(_va_dpy, va_image.buf, &surface_p);

    switch (va_image.format.fourcc)
    {
    case VA_FOURCC_NV12:
        y_src = src_buffer;
        u_src = src_buffer + y_size; // UV offset for NV12

        y_dst = (unsigned char*)surface_p + va_image.offsets[0];
        u_dst = (unsigned char*)surface_p + va_image.offsets[1]; // U offset for NV12

        // Y plane
        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst+va_image.pitches[0]*i, y_src, vpp_Imageinfo.src_width);
            y_src += vpp_Imageinfo.src_width;
        }
        // UV offset for NV12
        for (i = 0; i < vpp_Imageinfo.src_height >> 1; i++)
        {
            memcpy(u_dst+va_image.pitches[0]*i, u_src, vpp_Imageinfo.src_width);
            u_src += vpp_Imageinfo.src_width;
        }
        break;
    case VA_FOURCC_P010:
       y_src = src_buffer;
       u_src = src_buffer + 2*y_size; // UV offset for P010

       y_dst = (char*)surface_p + va_image.offsets[0];
       u_dst = (char*)surface_p + va_image.offsets[1]; // U offset for P010
       // Y plane

       for (i = 0; i < vpp_Imageinfo.src_height; i++)
       {
           memcpy(y_dst, y_src, vpp_Imageinfo.src_width*2);
           y_dst += va_image.pitches[0];
           y_src += vpp_Imageinfo.src_width*2;
       }

       for (i = 0; i < vpp_Imageinfo.src_height >> 1; i++)
       {
           memcpy(u_dst, u_src, vpp_Imageinfo.src_width*2);
           u_dst += va_image.pitches[1];
           u_src += vpp_Imageinfo.src_width*2;
       }
       break;

    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
        y_src = src_buffer;
        if (VA_FOURCC_YV12 == vpp_Imageinfo.src_format)   // VU offset for YV12 source
        {
            v_src = src_buffer + y_size;
            u_src = src_buffer + y_size + u_size;
        }
        else   // UV offset for I420 source
        {
            u_src = src_buffer + y_size;
            v_src = src_buffer + y_size + u_size;
        }
        y_dst = (char*)surface_p + va_image.offsets[0];
        v_dst = (char*)surface_p + va_image.offsets[1]; // V offset for YV12
        u_dst = (char*)surface_p + va_image.offsets[2]; // U offset for YV12

        // Y plane
        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width);
            y_dst += va_image.pitches[0];
            y_src += vpp_Imageinfo.src_width;
        }

        v_dst = (char*)surface_p + va_image.offsets[1];
        u_dst = (char*)surface_p + va_image.offsets[2];

        for (i = 0; i < vpp_Imageinfo.src_height >> 1; i++)
        {
            memcpy(v_dst, v_src, vpp_Imageinfo.src_width >> 1);
            memcpy(u_dst, u_src, vpp_Imageinfo.src_width >> 1);

            u_dst += va_image.pitches[1];
            v_dst += va_image.pitches[2];
            u_src += vpp_Imageinfo.src_width >> 1;
            v_src += vpp_Imageinfo.src_width >> 1;
        }
        break;
    case VA_FOURCC_Y210:
    case VA_FOURCC_Y216:
        y_src = src_buffer;
        y_dst = (char*)surface_p + va_image.offsets[0];

        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width << 2);
            y_dst += va_image.pitches[0];
            y_src += vpp_Imageinfo.src_width << 2;
        }
        break;
    case VA_FOURCC_UYVY:
    case VA_FOURCC_YUY2:
        y_src = src_buffer;
        y_dst = (char*)surface_p + va_image.offsets[0];

        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width << 1);
            y_dst += va_image.pitches[0];
            y_src += vpp_Imageinfo.src_width << 1;
        }
        break;

    case VA_FOURCC_RGB565: //RGBP => rgb565
        rgb_shift_factor = 1; //Default is 2 for 4-byte RGB formats (which includes alpha), but should be 1 for 2 byte formats
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_XRGB:
    case VA_FOURCC_XBGR:
    case VA_FOURCC_RGBA:
    case VA_FOURCC_BGRA:
    case VA_FOURCC_RGBX:
    case VA_FOURCC_BGRX:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_Y410:
        y_src = src_buffer;
        y_dst = (char*)surface_p + va_image.offsets[0];

        for (i = 0; i < vpp_Imageinfo.src_height; i++) {
            memcpy(y_dst, y_src, (vpp_Imageinfo.src_width << rgb_shift_factor));
            y_src += vpp_Imageinfo.src_width << rgb_shift_factor;
            y_dst += va_image.pitches[0];
         }
        break;
    case VA_FOURCC_Y416:
        y_src = src_buffer;
        y_dst = (char*)surface_p + va_image.offsets[0];

        for (i = 0; i < vpp_Imageinfo.src_height; i++) {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width << 3);
            y_src += vpp_Imageinfo.src_width << 3;
            y_dst += va_image.pitches[0];
        }
        break;

    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
        u_size = (vpp_Imageinfo.src_width) * (vpp_Imageinfo.src_height >> 1); //4:2:2
        y_src = src_buffer;
        u_src = src_buffer + y_size;
        v_src = src_buffer + y_size + u_size;

        y_dst = (char*)surface_p + va_image.offsets[0];
        u_dst = (char*)surface_p + va_image.offsets[1];
        v_dst = (char*)surface_p + va_image.offsets[2];

        /* Y plane */
        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width);
            y_dst += va_image.pitches[0];
            y_src += vpp_Imageinfo.src_width;
        }

        /* U V plane */
        for (i = 0; i < vpp_Imageinfo.src_height >> 1; i++)
        {
            memcpy(u_dst, u_src, vpp_Imageinfo.src_width);
            memcpy(v_dst, v_src, vpp_Imageinfo.src_width);

            u_dst += va_image.pitches[1];
            v_dst += va_image.pitches[2];
            u_src += vpp_Imageinfo.src_width;
            v_src += vpp_Imageinfo.src_width;
        }
        break;

    case VA_FOURCC_411P:        //U, V plane are 1/4 width+paddings and full height
    case VA_FOURCC_444P:        //4:4:4
    case VA_FOURCC_422H:
        u_size = (vpp_Imageinfo.src_width) * (vpp_Imageinfo.src_height); //4:2:2 + padding
        y_src = src_buffer;
        u_src = src_buffer + y_size;
        v_src = src_buffer + y_size + u_size;

        y_dst = (char*)surface_p + va_image.offsets[0];
        u_dst = (char*)surface_p + va_image.offsets[1];
        v_dst = (char*)surface_p + va_image.offsets[2];

        /* Y plane */
        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(y_dst, y_src, vpp_Imageinfo.src_width);
            y_dst += va_image.pitches[0];
            y_src += vpp_Imageinfo.src_width;
        }

        /* U V plane */
        for (i = 0; i < vpp_Imageinfo.src_height; i++)
        {
            memcpy(u_dst, u_src, vpp_Imageinfo.src_width);
            memcpy(v_dst, v_src, vpp_Imageinfo.src_width);

            u_dst += va_image.pitches[1];
            v_dst += va_image.pitches[2];
            u_src += vpp_Imageinfo.src_width;
            v_src += vpp_Imageinfo.src_width;
        }
        break;
    default: // should not come here
        printf("VA_STATUS_ERROR_INVALID_IMAGE_FORMAT");
        va_status = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
        break;
    }

    vaUnmapBuffer(_va_dpy, va_image.buf);
    vaDestroyImage(_va_dpy, va_image.image_id);
    if (va_status != VA_STATUS_SUCCESS)
        return false;
    else
        return true;
}

bool copyFrameFromSurface(VASurfaceID surface_id)
{
    VAStatus      va_status;
    VAImage va_image;
    char *image_data=NULL;
    va_status = vaSyncSurface (_va_dpy,surface_id);
    FILE *fOut=fopen(vpp_Imageinfo.output_file,"wb");
    va_status = vaDeriveImage(_va_dpy, surface_id, &va_image );
    assert ( va_status == VA_STATUS_SUCCESS);
    va_status = vaMapBuffer(_va_dpy, va_image.buf, (void **)&image_data);
    assert ( va_status == VA_STATUS_SUCCESS);
    switch (va_image.format.fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC_NV21:
    {
        char *pY = ( char *)image_data +va_image.offsets[0];
        char *pUV = ( char *)image_data + va_image.offsets[1];
        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
             fwrite(pY, 1, vpp_Imageinfo.dst_width, fOut);
             pY += va_image.pitches[0];
        }
        //UV
        for(int i = 0; i < vpp_Imageinfo.dst_height/2; i++)
        {
            fwrite(pUV, 1, vpp_Imageinfo.dst_width, fOut);
            pUV += va_image.pitches[1];
        }
        break;
    }
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
    {
        if (vpp_Imageinfo.dst_format == VA_FOURCC_YV12)
        {
            char *pY = ( char *)image_data +va_image.offsets[0];
            char *pV = ( char *)image_data + va_image.offsets[1];
            char *pU = ( char *)image_data + va_image.offsets[2];

           for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
           {
               fwrite(pY, 1, vpp_Imageinfo.dst_width, fOut);
               pY += va_image.pitches[0];
           }

           for(int i = 0; i < vpp_Imageinfo.dst_height/4; i++)
           {
               fwrite(pV, 1, vpp_Imageinfo.dst_width, fOut);
               pV += va_image.pitches[1];
           }
           for(int i = 0; i < vpp_Imageinfo.dst_height/4; i++)
           {
               fwrite(pU, 1, vpp_Imageinfo.dst_width, fOut);
               pU += va_image.pitches[2];
           }
       }
      else if(vpp_Imageinfo.dst_format == VA_FOURCC_I420)
      {
          char *pY = ( char *)image_data +va_image.offsets[0];
          char *pV = ( char *)image_data + va_image.offsets[1];
          char *pU = ( char *)image_data + va_image.offsets[2];

          for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
          {
              fwrite(pY, 1, vpp_Imageinfo.dst_width, fOut);
              pY += va_image.pitches[0];
          }

          for(int i = 0; i < vpp_Imageinfo.dst_height/4; i++)
          {
              fwrite(pU, 1, vpp_Imageinfo.dst_width, fOut);
              pU += va_image.pitches[2];
          }
          for(int i = 0; i < vpp_Imageinfo.dst_height/4; i++)
          {
              fwrite(pV, 1, vpp_Imageinfo.dst_width, fOut);
              pV += va_image.pitches[1];
          }
      }
      break;
    }
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_RGB565:
    case VA_FOURCC_XRGB:
    case VA_FOURCC_XBGR:
    case VA_FOURCC_RGBA:
    case VA_FOURCC_BGRA:
    case VA_FOURCC_RGBX:
    case VA_FOURCC_BGRX:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_Y410:
    case VA_FOURCC_Y416:
    {
        char *pdst = ( char *)image_data +va_image.offsets[0];
        int  bytes_per_pixel = 4;
        if (vpp_Imageinfo.dst_format == VA_FOURCC_RGB565)
            bytes_per_pixel = 2; //rgb565
        else if (vpp_Imageinfo.dst_format == VA_FOURCC_Y416)
            bytes_per_pixel = 8; //Y416
        else
            bytes_per_pixel = 4; //ARGB
        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pdst, 1, vpp_Imageinfo.dst_width*bytes_per_pixel, fOut);
            pdst += va_image.pitches[0];
        }
        break;
    }
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_Y210:
    case VA_FOURCC_Y216:
    {
        int   shift_yuv422 = 1;
        if(vpp_Imageinfo.dst_format == VA_FOURCC_Y210 || vpp_Imageinfo.dst_format == VA_FOURCC_Y216)
            shift_yuv422 = 2;

        char *pdst = ( char *)image_data +va_image.offsets[0];
        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pdst, 1, vpp_Imageinfo.dst_width<<shift_yuv422, fOut);
            pdst += va_image.pitches[0];
        }
        break;
    }
    case VA_FOURCC_P010:
    {
        char *pY  = ( char *)image_data + va_image.offsets[0];
        char *pUV = ( char *)image_data + va_image.offsets[1];
        // copy Y plane

        for (int i=0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pY, 1, vpp_Imageinfo.dst_width*2, fOut);
            pY += va_image.pitches[0];
        }

        for (int i=0; i < vpp_Imageinfo.dst_height/2; i++)
        {
            fwrite(pUV, 1, vpp_Imageinfo.dst_width*2, fOut);
            pUV += va_image.pitches[1];
        }
        break;
    }
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
    {
        char *pY = ( char *)image_data +va_image.offsets[0];
        char *pU = ( char *)image_data + va_image.offsets[1];
        char *pV = ( char *)image_data + va_image.offsets[2];

        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pY, 1, vpp_Imageinfo.dst_width, fOut);
            pY += va_image.pitches[0];
        }

        for(int i = 0; i < vpp_Imageinfo.dst_height>>1; i++)
        {
            fwrite(pU, 1, vpp_Imageinfo.dst_width, fOut);
            pU += va_image.pitches[1];
        }
        for(int i = 0; i < vpp_Imageinfo.dst_height>>1; i++)
        {
            fwrite(pV, 1, vpp_Imageinfo.dst_width, fOut);
            pV += va_image.pitches[2];
        }
        break;
    }
    case VA_FOURCC_411P:        //U, V plane are 1/4 width+paddings and full height
    case VA_FOURCC_444P:        //4:4:4
    case VA_FOURCC_422H:
    {
        char *pY = ( char *)image_data +va_image.offsets[0];
        char *pU = ( char *)image_data + va_image.offsets[1];
        char *pV = ( char *)image_data + va_image.offsets[2];

        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pY, 1, vpp_Imageinfo.dst_width, fOut);
            pY += va_image.pitches[0];
        }

        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pU, 1, vpp_Imageinfo.dst_width, fOut);
            pU += va_image.pitches[1];
        }
        for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
        {
            fwrite(pV, 1, vpp_Imageinfo.dst_width, fOut);
            pV += va_image.pitches[2];
        }
        break;
    }
    default:
        return false;
    }
    va_status = vaUnmapBuffer(_va_dpy, va_image.buf);
    va_status = vaDestroyImage(_va_dpy,va_image.image_id);
    return true;
}

bool libvaInit()
{
    int          major_ver, minor_ver;
    const char  *driver=NULL;
    VAConfigAttrib  vpAttrib;

    VAStatus    vaStatus;

    x11_display = XOpenDisplay(":0.0");
    if(x11_display == NULL)
    {
        char *disp_name = getenv("DISPLAY");
        if(disp_name == NULL)
        {
            printf("please check /root/.vnc/ for the correct display name, if not\n");
            printf("please run vncserver first to get the display name and set the env DISPLAY!\n");
            exit(-1);
        }else{
            printf("the display set is %s\n",disp_name);
            x11_display = XOpenDisplay(disp_name);
        }
    }
    if (NULL == x11_display) {
        printf("Error: Can't connect X server! %s %s(line %d)\n", __FILE__, __func__, __LINE__);
        return false;
    }
    _va_dpy = vaGetDisplay(x11_display);

    vaStatus = vaInitialize(_va_dpy, &major_ver, &minor_ver);
    if (vaStatus != VA_STATUS_SUCCESS) {
        printf("Error: Failed vaInitialize(): in %s %s(line %d)\n", __FILE__, __func__, __LINE__);
        return false;
    }
    printf("libva version: %d.%d\n",major_ver,minor_ver);

    driver = vaQueryVendorString(_va_dpy);
    printf("VAAPI Init complete; Driver version :%s\n",driver);

    memset(&vpAttrib, 0, sizeof(vpAttrib));
    vpAttrib.type  = VAConfigAttribRTFormat;
    vpAttrib.value = VA_RT_FORMAT_YUV420;
    vaStatus=vaCreateConfig(_va_dpy, VAProfileNone,
                                VAEntrypointVideoProc,
                                &vpAttrib,
                                1, &_cfg_id);
    assert (vaStatus == VA_STATUS_SUCCESS);
    vaStatus=vaCreateContext(_va_dpy, _cfg_id, vpp_Imageinfo.src_width, vpp_Imageinfo.src_height, 0, NULL, 0, &_context_id);
    assert (vaStatus == VA_STATUS_SUCCESS);

    return true;
}

unsigned int RawVidFrameBytesCalc()
{
    unsigned int width = vpp_Imageinfo.src_width;
    unsigned int height = vpp_Imageinfo.src_height;
    unsigned int fourcc = vpp_Imageinfo.src_format;
    switch(fourcc)
    {
    case VA_FOURCC_I420:
    case VA_FOURCC_NV12:
    case VA_FOURCC_YV12:
    case VA_FOURCC_NV21:
        return width*height*3/2;
    case VA_FOURCC_411P:
    case VA_FOURCC_444P:
    case VA_FOURCC_422H:
    case VA_FOURCC_P010:
        return width*height*3;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_422V:
    case VA_FOURCC_RGB565:
        return width*height*2;
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_RGBA:
    case VA_FOURCC_BGRA:
    case VA_FOURCC_XRGB:
    case VA_FOURCC_XBGR:
    case VA_FOURCC_RGBX:
    case VA_FOURCC_BGRX:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_Y210:
    case VA_FOURCC_Y216:
    case VA_FOURCC_Y410:
        return width*height*4;
    case VA_FOURCC_Y416:
        return width*height*8;
    default:
        printf(": Error - Unhandled fourccType:%d Returning I420_FOURCC_FMT as default\n",fourcc);
        return width*height*3/2;
    }
}

bool readInputFile()
{
    FILE* fp_yuv=fopen(vpp_Imageinfo.input_file,"rb");
    unsigned int read_len = RawVidFrameBytesCalc();
    vpp_Imageinfo.src_buffer=(unsigned char *)malloc(read_len);
    if( fp_yuv != NULL )
    {
        printf("success to open input file\n");
        fread(vpp_Imageinfo.src_buffer,1,read_len,fp_yuv);
        fclose(fp_yuv);
    }
    else
    {
        printf("fail to open input file\n");
    }
    copyToVaSurface(in_surface);
}

bool createInputSurface()
{
    VAStatus    vaStatus;
    VASurfaceAttrib surfaceAttrib;
    unsigned int surface_width, surface_height;

    surfaceAttrib.type = VASurfaceAttribPixelFormat;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_width = vpp_Imageinfo.src_width;
    surface_height = vpp_Imageinfo.src_height;
    surfaceAttrib.value.value.i = vpp_Imageinfo.src_format;

    vaStatus=vaCreateSurfaces(_va_dpy,
                    VA_RT_FORMAT_YUV420,
                    surface_width,
                    surface_height,
                    &in_surface,
                    1,    //Allocating one surface per layer at a time
                    &surfaceAttrib,
                    1);
    assert (vaStatus == VA_STATUS_SUCCESS);
    readInputFile();
    return true;

}

bool createOutputSurface()
{
    VAStatus      va_status;
    uint32_t format = VA_RT_FORMAT_YUV420;
    int surface_width, surface_height;
    surface_width = vpp_Imageinfo.dst_width;
    surface_height = vpp_Imageinfo.dst_height;
    uint32_t fourcc = vpp_Imageinfo.dst_format;
    VASurfaceAttrib surfaceAttrib;
    surfaceAttrib.type = VASurfaceAttribPixelFormat;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surfaceAttrib.value.value.i = fourcc;

    vaCreateSurfaces(_va_dpy,
            format,
            surface_width,
            surface_height,
            &out_surface,
            1,      //Allocating one surface per layer at a time
            &surfaceAttrib,
            1);

    return true;
}

bool createPipeLineBuf()
{
    VAStatus    vaStatus;
    VAProcPipelineParameterBuffer vpInputParam;
    memset(&vpInputParam, 0, sizeof(VAProcPipelineParameterBuffer));

    vpInputParam.surface = in_surface;
    vpp_Imageinfo.region_in.x = 0;
    vpp_Imageinfo.region_in.y = 0;
    vpp_Imageinfo.region_in.width = vpp_Imageinfo.src_width;
    vpp_Imageinfo.region_in.height = vpp_Imageinfo.src_height;
    vpp_Imageinfo.region_out.x = 0;
    vpp_Imageinfo.region_out.y = 0;
    vpp_Imageinfo.region_out.width = vpp_Imageinfo.dst_width;
    vpp_Imageinfo.region_out.height = vpp_Imageinfo.dst_height;
    vpInputParam.surface_region = &vpp_Imageinfo.region_in;
    vpInputParam.output_region = &vpp_Imageinfo.region_out;
    switch (vpp_Imageinfo.chroma_siting_mode) {
    case UNKNOWN:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_UNKNOWN; break;
    case CHROMA_SITING_TOP_LEFT:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_LEFT; break;
    case CHROMA_SITING_TOP_CENTER:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_TOP | VA_CHROMA_SITING_HORIZONTAL_CENTER; break;
    case CHROMA_SITING_CENTER_LEFT:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_LEFT; break;
    case CHROMA_SITING_CENTER_CENTER:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_CENTER | VA_CHROMA_SITING_HORIZONTAL_CENTER; break;
    case CHROMA_SITING_BOTTOM_LEFT:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_LEFT; break;
    case CHROMA_SITING_BOTTOM_CENTER:
        vpInputParam.input_color_properties.chroma_sample_location = VA_CHROMA_SITING_VERTICAL_BOTTOM | VA_CHROMA_SITING_HORIZONTAL_CENTER; break;
    default:
        printf("Unsupported chroma siting mode : %d", vpp_Imageinfo.chroma_siting_mode);
        return false;
    }
    vaStatus=vaCreateBuffer(_va_dpy,
                                    _context_id,
                                    VAProcPipelineParameterBufferType,
                                    sizeof(VAProcPipelineParameterBuffer),
                                    1,
                                    &vpInputParam,
                                    &vp_pipeline_inbuf);
    assert (vaStatus == VA_STATUS_SUCCESS);
    return true;
}

bool processOneFrame()
{
    VAStatus    va_status;
    va_status = vaBeginPicture(_va_dpy, _context_id, out_surface);
    va_status = vaRenderPicture(_va_dpy, _context_id,&vp_pipeline_inbuf, 1);
    va_status = vaEndPicture(_va_dpy, _context_id);
    copyFrameFromSurface(out_surface);
    return true;
}

bool destorySurface()
{
    VAStatus    va_status;

    if (vp_pipeline_inbuf != VA_INVALID_ID)
        vaDestroyBuffer(_va_dpy,vp_pipeline_inbuf);

       va_status = vaDestroySurfaces( _va_dpy,&in_surface, 1);
    va_status = vaDestroySurfaces(_va_dpy,&out_surface, 1);

    if(vpp_Imageinfo.src_buffer != NULL){
        free(vpp_Imageinfo.src_buffer);
        vpp_Imageinfo.src_buffer = NULL;
    }

    vaDestroyContext(_va_dpy, _context_id);
    vaDestroyConfig(_va_dpy, _cfg_id);
    return true;
}

bool libvaTerminate()
{
    vaTerminate(_va_dpy);
    XCloseDisplay(x11_display);
    _va_dpy = x11_display = NULL;
    return true;
}

void print_help()
{
    printf("./vppchromasitting <options>\n");
    printf("-i  <input file>\n");
    printf("-iw <input width>\n");
    printf("-ih <input height>\n");
    printf("-if <input format>\n");
    printf("-m  <PictureCodingType 0:PROGRESSIVE, 1:TOP_FIELD_FIRST,2:MB_A_FIELD_FRAME,3:BOTTOM_FIELD_FIRST>\n");
    printf("-o  <output file>\n");
    printf("-ow <output width>\n");
    printf("-oh <output height>\n");
    printf("-of <output format>\n");
    printf("format can be assigned:nv12 i420 yuy2 argb yv12 abgr xrgb xbgr rgba bgra rgbx bgrx rgb565 p411 p444 v422 h422 imc3 p010 ayuv uyvy y210 y410 y216 y416\n");
    printf("example: ./vppchromasitting -i input_720x516.nv12 -iw 720 -ih 516 -if nv12 -m 0 -o output_352x288.xrgb -ow 352 -oh 288 -of xrgb\n");
    exit(-1);
}

bool parseCommand( int argc, char** argv)
{
    if (( argc % 2 == 0 ) || ( argc == 1 ))
    {
    printf("wrong parameters count\n");
    print_help();
    return false;
    }

    for (int arg_idx = 1; arg_idx < argc - 1; arg_idx += 2)
    {
        if (!strcmp(argv[arg_idx], "-i"))
        {
            strncpy(vpp_Imageinfo.input_file, argv[arg_idx + 1], 100);
        }
        else if (!strcmp(argv[arg_idx], "-iw"))
        {
            vpp_Imageinfo.src_width = atoi(argv[arg_idx + 1]);
        }
        else if (!strcmp(argv[arg_idx], "-ih"))
        {
            vpp_Imageinfo.src_height = atoi(argv[arg_idx + 1]);
        }
        else if(!strcmp(argv[arg_idx], "-if"))
        {
            int i;
            for(i = 0; i< sizeof(sFomatMap)/sizeof(VPP_FormatMap);i++){
                if(strcmp(argv[arg_idx + 1],sFomatMap[i].in_format) == 0){
                    vpp_Imageinfo.src_format = sFomatMap[i].cc_format;
                    break;
                }
            }
            if(i == sizeof(sFomatMap)/sizeof(VPP_FormatMap)){
                print_help();
            }
        }
        else if (!strcmp(argv[arg_idx], "-m"))
        {
            vpp_Imageinfo.chroma_siting_mode = atoi(argv[arg_idx + 1]);
        }
        else if(!strcmp(argv[arg_idx], "-o"))
        {
            strncpy(vpp_Imageinfo.output_file, argv[arg_idx + 1], 100);
        }
        else if (!strcmp(argv[arg_idx], "-ow"))
        {
            vpp_Imageinfo.dst_width = atoi(argv[arg_idx + 1]);
        }
        else if (!strcmp(argv[arg_idx], "-oh"))
        {
            vpp_Imageinfo.dst_height = atoi(argv[arg_idx + 1]);
        }
        else if(!strcmp(argv[arg_idx], "-of"))
        {
            int i;
            for(i = 0; i< sizeof(sFomatMap)/sizeof(VPP_FormatMap);i++){
                if(strcmp(argv[arg_idx + 1],sFomatMap[i].in_format) == 0){
                    vpp_Imageinfo.dst_format = sFomatMap[i].cc_format;
                    break;
                }
            }
            if(i == sizeof(sFomatMap)/sizeof(VPP_FormatMap)){
            print_help();
            }
        }
        else
        {
            print_help();
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    parseCommand(argc, argv);
    libvaInit();
    createInputSurface();
    createOutputSurface();
    createPipeLineBuf();
    processOneFrame();
    destorySurface();
    libvaTerminate();

    return 0;
}
