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

typedef struct {
    char                input_file[100];
    char                output_file[100];;
    unsigned char       *src_buffer;
    unsigned int        src_format;
    unsigned int        src_color_standard;
    unsigned int        src_buffer_size;
    unsigned int        src_width;
    unsigned int        src_height;
    unsigned int        dst_width;
    unsigned int        dst_height;
    unsigned int        dst_format;
    unsigned int        frame_num;
} VPP_ImageInfo;

    Display        *x11_display;
    VADisplay      _va_dpy;
    VAContextID    _context_id;
    VASurfaceID    out_surface,in_surface;
    VABufferID     vp_pipeline_outbuf = VA_INVALID_ID;
    VABufferID     vp_pipeline_inbuf = VA_INVALID_ID;
    VPP_ImageInfo  vpp_Imageinfo;


bool copyToVaSurface( VASurfaceID surface_id )
{
    VAImage       va_image;
    VAStatus      va_status;
    void          *surface_p = NULL;
    unsigned char *src_buffer;
    unsigned char *y_src, *u_src;

    unsigned char *y_dst, *u_dst;
    int           y_size = vpp_Imageinfo.src_width * vpp_Imageinfo.src_height;
    int           u_size = (vpp_Imageinfo.src_width >> 1) * (vpp_Imageinfo.src_height >> 1); 
    int           i;
   

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
    default:
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
	case VA_FOURCC_ARGB:
	{
        char *pdst = ( char *)image_data +va_image.offsets[0];
   	    for(int i = 0; i < vpp_Imageinfo.dst_height; i++)
   	    {
		    fwrite(pdst, 1, vpp_Imageinfo.dst_width*4, fOut);
		    pdst += va_image.pitches[0];
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
    VAConfigID cfg;
    VAStatus    vaStatus;

    x11_display = XOpenDisplay(":0.0");
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
                                1, &cfg);
    assert (vaStatus == VA_STATUS_SUCCESS);
    vaStatus=vaCreateContext(_va_dpy, cfg, vpp_Imageinfo.src_width, vpp_Imageinfo.src_height, 0, NULL, 0, &_context_id);
    assert (vaStatus == VA_STATUS_SUCCESS);

    return true;
}

bool readInputFile()
{
    FILE* fp_yuv=fopen(vpp_Imageinfo.input_file,"rb");
    memset(&vpp_Imageinfo,sizeof(vpp_Imageinfo),0);
    vpp_Imageinfo.src_buffer=(unsigned char *)malloc(vpp_Imageinfo.src_width*vpp_Imageinfo.src_height*3/2);
    if( fp_yuv != NULL )
    {
        printf("success to open input file\n");
        fread(vpp_Imageinfo.src_buffer,1,vpp_Imageinfo.src_width*vpp_Imageinfo.src_height*3/2,fp_yuv);
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
    VASurfaceAttrib surfaceAttrib;
    VAProcPipelineParameterBuffer vpInputParam;
    VARectangle srcRect,outRect;  
    VAStatus    vaStatus;
    surfaceAttrib.type = VASurfaceAttribPixelFormat;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    unsigned int surface_width, surface_height;
    surface_width = vpp_Imageinfo.src_width;
    surface_height = vpp_Imageinfo.src_height;
    surfaceAttrib.value.value.i = VA_FOURCC_NV12;
   
    vaStatus=vaCreateSurfaces(_va_dpy,
					VA_RT_FORMAT_YUV420,
					surface_width,
					surface_height,
					&in_surface,
					1,    //Allocating one surface per layer at a time
					//&surfaceAttrib,
					NULL,
					0);
    assert (vaStatus == VA_STATUS_SUCCESS);
    readInputFile();

    memset(&vpInputParam, 0, sizeof(VAProcPipelineParameterBuffer));
    srcRect.x=0;
    srcRect.y=0;
    srcRect.width = vpp_Imageinfo.src_width;
    srcRect.height = vpp_Imageinfo.src_height;
    outRect.x=0;
    outRect.y=0;
    outRect.width = vpp_Imageinfo.src_width; 
    outRect.height = vpp_Imageinfo.src_height;
    vpInputParam.surface_color_standard  = VAProcColorStandardBT601;
    vpInputParam.output_background_color = 0;
    vpInputParam.output_color_standard   = VAProcColorStandardNone;
		
		 
    vpInputParam.filters                 = NULL;
    vpInputParam.num_filters             = 0;
    vpInputParam.forward_references      = NULL;
    vpInputParam.num_forward_references  = 0;
    vpInputParam.backward_references     = 0;
    vpInputParam.num_backward_references = 0;
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

bool createOutputSurface()
{
    VASurfaceAttrib surfaceAttrib;
    VARectangle targetRect;
    VAProcPipelineParameterBuffer vpOutparam;

    surfaceAttrib.type = VASurfaceAttribPixelFormat;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    int surface_width, surface_height;
    surface_width = vpp_Imageinfo.dst_width;
    surface_height = vpp_Imageinfo.dst_height;
    if ( vpp_Imageinfo.dst_format == 1 )
	{
	    surfaceAttrib.value.value.i = VA_FOURCC_ARGB;
	}
	else
	{
		surfaceAttrib.value.value.i = VA_FOURCC_NV12;
	}
   
    vaCreateSurfaces(_va_dpy,
					VA_RT_FORMAT_YUV420,
					surface_width,
					surface_height,
					&out_surface,
					1,    //Allocating one surface per layer at a time
					&surfaceAttrib,
					//NULL,
					1);

    targetRect.x = 0;
    targetRect.y = 0;
    targetRect.width = vpp_Imageinfo.dst_width;
    targetRect.height = vpp_Imageinfo.dst_height;
   
    vpOutparam.output_region = &targetRect;
    vpOutparam.surface_region = &targetRect;
    vpOutparam.surface = out_surface;
   
      
    vaCreateBuffer(_va_dpy,
		        _context_id,
		        VAProcPipelineParameterBufferType,
		        sizeof(VAProcPipelineParameterBuffer),
		        1,
		        &vpOutparam,
		        &vp_pipeline_outbuf);
   
    
    return true;
}

bool processOneFrame()
{
    VAStatus    va_status;
	va_status = vaBeginPicture(_va_dpy, _context_id, out_surface);
	va_status = vaRenderPicture(_va_dpy, _context_id,&vp_pipeline_inbuf, 1);
	va_status = vaEndPicture(_va_dpy, _context_id);
	copyFrameFromSurface(out_surface);
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
	printf("./vpp <options>\n");
	printf("-i <input file>\n");
	printf("-iw <input width>\n");	
	printf("-ih <input height>\n");
	printf("-o <output file>\n");
	printf("-ow <input width>\n");	
	printf("-oh <output height>\n");
	printf("-of <output format,0:NV12 1:ARGB>\n");
	printf("example: ./vppscaling -i input.nv12 -iw 640 -ih 480 -o output.argb -ow 320 -oh 240 -of 1\n");
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
	else if (!strcmp(argv[arg_idx], "-o"))
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
  	else if (!strcmp(argv[arg_idx], "-of"))
	{
	    vpp_Imageinfo.dst_format = atoi(argv[arg_idx + 1]);
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
    processOneFrame();
    libvaTerminate();

    return 0;
}
