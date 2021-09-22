/*
 * * Copyright (C) 2018 Intel Corporation. All Rights Reserved.
 * *
 ** Permission is hereby granted, free of charge, to any person obtaining a
 * * copy of this software and associated documentation files (the
 * * "Software"), to deal in the Software without restriction, including
 * * without limitation the rights to use, copy, modify, merge, publish,
 * * distribute, sub license, and/or sell copies of the Software, and to
 * * permit persons to whom the Software is furnished to do so, subject to
 * * the following conditions:
 * *
 * * The above copyright notice and this permission notice (including the
 * * next paragraph) shall be included in all copies or substantial portions
 * * of the Software.
 * *
 * * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * */


/**
 * @file VDecAccelVA.cpp
 * @brief LibVA decode accelerator implementation.
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <string.h>
#include "VDecAccelVA.h"
#include <va/va.h>
#include <va/va_drm.h>

#define VASUCCEEDED(err)    (err == VA_STATUS_SUCCESS)
#define VAFAILED(err)       (err != VA_STATUS_SUCCESS)

mvaccel::VDecAccelVAImpl::VDecAccelVAImpl(void* device)
    : m_vaDisplay(0)
    , m_vaProfile(VAProfileNone)
    , m_vaEntrypoint(VAEntrypointVLD)
    , m_vaConfigID(0)
    , m_vaContextID(0)
    , m_surfaceType(VA_RT_FORMAT_YUV420)
{
    if (device)
        m_vaDisplay = *(reinterpret_cast<VADisplay*>(device));

    if (!m_vaDisplay) {
        printf("Invalid VADisplay\n");
        delete this;
        return;
    }
}

mvaccel::VDecAccelVAImpl::VDecAccelVAImpl()
    : m_vaDisplay(0)
    , m_vaProfile(VAProfileNone)
    , m_vaEntrypoint(VAEntrypointVLD)
    , m_vaConfigID(0)
    , m_vaContextID(0)
    , m_surfaceType(VA_RT_FORMAT_YUV420)
{
}

mvaccel::VDecAccelVAImpl::~VDecAccelVAImpl()
{
    if (drm_fd != -1) {
        close(drm_fd);
    }
}


int mvaccel::VDecAccelVAImpl::Open()
{
    VAStatus vaStatus = VA_STATUS_ERROR_UNKNOWN;

    //get display device
    int MajorVer, MinorVer;

    if (vaStatus != VA_STATUS_SUCCESS) {
        drm_fd = open("/dev/dri/renderD128", O_RDWR);
        if (drm_fd >= 0) {
            m_vaDisplay = vaGetDisplayDRM(drm_fd);
            vaStatus = vaInitialize(m_vaDisplay, &MajorVer, &MinorVer);
        }
    }

    //initialize decode description
    create_decode_desc();

    m_vaProfile = VAProfileH264Main;

    // We only support VLD currently
    m_vaEntrypoint = VAEntrypointVLD;

    int count = 0;
    count = vaMaxNumEntrypoints(m_vaDisplay);
    assert(count);

    std::vector<VAEntrypoint> vaEntrypoints(count);
    vaStatus = vaQueryConfigEntrypoints(
                   m_vaDisplay,
                   m_vaProfile,
                   &vaEntrypoints[0],
                   &count);
    if (VAFAILED(vaStatus))
        printf("vaQueryConfigEntrypoints fail\n");

    std::vector<VAEntrypoint>::iterator it = std::find(vaEntrypoints.begin(), vaEntrypoints.end(), m_vaEntrypoint);
    if (it == vaEntrypoints.end()) {
        if (VAFAILED(vaStatus))
            printf("VAEntrypoint is not found\n");
        return 1;
    }

    if (!is_config_compatible(m_DecodeDesc)) {
        if (VAFAILED(vaStatus))
            printf("Decode configuration is not compatible\n");
        return 1;
    }

    // Setup config attributes
    std::vector<VAConfigAttrib> vaAttribs;
    prepare_config_attribs(m_DecodeDesc, vaAttribs);
    // Create config
    vaStatus = vaCreateConfig(
                   m_vaDisplay,
                   m_vaProfile,
                   m_vaEntrypoint,
                   &vaAttribs.at(0),
                   vaAttribs.size(),
                   &m_vaConfigID
               );
    if (VAFAILED(vaStatus))
        printf("vaCreateConfig fail\n");

    if (!is_rt_foramt_supported(m_DecodeDesc)) {
        if (VAFAILED(vaStatus))
            printf("Render target is not supported\n");
        return 1;
    }

    // Calculate aligned width/height for gfx surface
    uint32_t aligned_width  = m_DecodeDesc.width;
    uint32_t aligned_height = m_DecodeDesc.height;

    // Setup surface attributes
    prepare_surface_attribs(m_DecodeDesc, m_vaSurfAttribs, false);
    // Create surfaces
    for (uint32_t i = 0; i < m_DecodeDesc.surfaces_num; i++) {
        VASurfaceID vaID = VA_INVALID_SURFACE;
        vaStatus = vaCreateSurfaces(
                       m_vaDisplay,
                       m_surfaceType,
                       aligned_width,
                       aligned_height,
                       &vaID,
                       1,
                       &(m_vaSurfAttribs.at(0)),
                       m_vaSurfAttribs.size()
                   );

        if (VASUCCEEDED(vaStatus))
            m_vaIDs.push_back(vaID);
    }

    // Check if surfaces created is equal to requested.
    if (m_vaIDs.size() != m_DecodeDesc.surfaces_num) {
        if (VAFAILED(vaStatus))
            printf("Create surface fail\n");
        return 1;
    }

    // Create context
    vaStatus = vaCreateContext(
                   m_vaDisplay,
                   m_vaConfigID,
                   aligned_width,
                   aligned_height,
                   VA_PROGRESSIVE,
                   &(m_vaIDs.at(0)),
                   m_vaIDs.size(),
                   &m_vaContextID
               );
    if (VAFAILED(vaStatus))
        printf("vaCreateContext fail\n");

    check_process_pipeline_caps(m_DecodeDesc);

    return vaStatus;
}

void mvaccel::VDecAccelVAImpl::Close()
{
    std::vector<VASurfaceID>::iterator it;
    for (it = m_vaIDs.begin(); it != m_vaIDs.end(); ++it)
        delete_surface(*it);

    vaDestroyConfig(m_vaDisplay, m_vaConfigID);
    vaDestroyContext(m_vaDisplay, m_vaContextID);

    m_images.clear();
    m_vaIDs.clear();
    m_vaSurfAttribs.clear();

    m_vaConfigID = 0;
    m_vaContextID = 0;
}

uint32_t mvaccel::VDecAccelVAImpl::GetSurfaceID(uint32_t index)
{
    assert(index < m_vaIDs.size());
    return m_vaIDs[index];
}

/**
 * @brief   Check if video decode acceleration description is supported.
 * @param   cc Video decode acceleration description.
 * @return  true if supported, false if not.
 */
bool mvaccel::VDecAccelVAImpl::is_config_compatible(DecodeDesc& desc)
{
    if (!is_slice_mode_supported(desc))
        return false;

    if (!is_encryption_supported(desc))
        return false;

    if (!is_sfc_config_supported(desc))
        return false;

    return true;
}

/**
 * @brief   Check if long or short format is supported not not.
 * @param   cc Video decode acceleration description.
 * @return  true if supported, false if not.
 */
bool mvaccel::VDecAccelVAImpl::is_slice_mode_supported(DecodeDesc& desc)
{
    VAConfigAttrib vaAttrib;
    memset(&vaAttrib, 0, sizeof(vaAttrib));
    vaAttrib.type = VAConfigAttribDecSliceMode;
    vaAttrib.value = 0;

    vaGetConfigAttributes(
        m_vaDisplay,
        m_vaProfile,
        m_vaEntrypoint,
        &vaAttrib,
        1);

    if (vaAttrib.value & VA_DEC_SLICE_MODE_BASE || vaAttrib.value & VA_DEC_SLICE_MODE_NORMAL)
        return true;
    else
        return false;
}

/**
 * @brief   Check if encryption is supported or not.
 * @param   cc Video decode acceleration description.
 * @return  true if supported, false if not.
 */
bool mvaccel::VDecAccelVAImpl::is_encryption_supported(DecodeDesc& desc)
{
    VAConfigAttrib vaAttrib;
    memset(&vaAttrib, 0, sizeof(vaAttrib));
    vaAttrib.type = VAConfigAttribEncryption;
    vaAttrib.value = 0;

    vaGetConfigAttributes(
        m_vaDisplay,
        m_vaProfile,
        m_vaEntrypoint,
        &vaAttrib,
        1
    );
    if (vaAttrib.value & VA_ATTRIB_NOT_SUPPORTED)
        return true;
    else
        return false;
}

/**
 * @brief   Check if SFC attribute is supported or not.
 * @param   cc Video decode acceleration description.
 * @return  true if supported, false if not.
 */
bool mvaccel::VDecAccelVAImpl::is_sfc_config_supported(DecodeDesc& desc)
{
    // SFC attribute check
    VAConfigAttrib vaAttrib;
    memset(&vaAttrib, 0, sizeof(vaAttrib));
    vaAttrib.type = VAConfigAttribDecProcessing;
    vaAttrib.value = 0;

    vaGetConfigAttributes(
        m_vaDisplay,
        m_vaProfile,
        m_vaEntrypoint,
        &vaAttrib,
        1);

    if (vaAttrib.value != VA_DEC_PROCESSING)
        return false;

    return true;
}

/**
 * @brief   Check if render target format is supported or not.
 * @param   cc Video decode acceleration description.
 * @return  true if supported, false if not.
 */
bool mvaccel::VDecAccelVAImpl::is_rt_foramt_supported(DecodeDesc& desc)
{
    uint32_t count = VASurfaceAttribCount + vaMaxNumImageFormats(m_vaDisplay);
    std::vector<VASurfaceAttrib> attribs(count);
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vaStatus = vaQuerySurfaceAttributes(
                   m_vaDisplay,
                   m_vaConfigID,
                   &attribs.at(0),
                   &count
               );
    if (VAFAILED(vaStatus)) {
        printf("vaQuerySurfaceAttributes failed\n");
        return false;
    }

    return true;
}

/**
 * @brief   Prepare config attribs VAContext creation.
 * @param   desc Video decode acceleration description.
 * @param   vaAttribs Array of VASurfaceAttrib which will contains the attrib.
 */
void mvaccel::VDecAccelVAImpl::prepare_config_attribs(
    DecodeDesc& desc,
    VAConfigAttribArray& attribs)
{
    VAConfigAttrib attrib;
    memset(&attrib, 0, sizeof(attrib));

    // RT formats
    attrib.type = VAConfigAttribRTFormat;
    attrib.value = VA_RT_FORMAT_YUV420;
    attribs.push_back(attrib);

    // Slice Mode
    attrib.type = VAConfigAttribDecSliceMode;
    attrib.value = VA_DEC_SLICE_MODE_NORMAL;
    attribs.push_back(attrib);

    //dec processing attribs
    attrib.type = VAConfigAttribDecProcessing;
    attrib.value = VA_DEC_PROCESSING;
    attribs.push_back(attrib);
}

/**
 * @brief   Prepare the VA surface attribs for creation.
 * @param   desc Video decode acceleration description.
 * @param   vaSurfAttribs Array of VASurfaceAttrib which will contains attrib.
 */
void mvaccel::VDecAccelVAImpl::prepare_surface_attribs(
    DecodeDesc& desc,
    VASurfaceAttribArray& attribs,
    bool bDecodeDownsamplingHinted)
{
    VASurfaceAttrib attrib;
    memset(&attrib, 0, sizeof(attrib));

    attrib.type = VASurfaceAttribPixelFormat;
    attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrib.value.type = VAGenericValueTypeInteger;

    // VA_FOURCC and MVFOURCC are interchangeable
    if (bDecodeDownsamplingHinted)
        attrib.value.value.i = VA_FOURCC_ARGB;
    else
        attrib.value.value.i = VA_FOURCC_NV12;

    attribs.push_back(attrib);
}

/**
 * @brief   Delete allocated surface
 * @param   surfaceID Index of allocated surface. After delete, the surface
 *          values will be set to invalid value.
 */
void mvaccel::VDecAccelVAImpl::delete_surface(VASurfaceID& vaID)
{
    // Make sure no others is using this surface
    if (m_images.count(vaID))
        m_images.erase(m_images.find(vaID));

    vaDestroySurfaces(m_vaDisplay, &vaID, 1);
    vaID = VA_INVALID_SURFACE;
}

uint8_t* mvaccel::VDecAccelVAImpl::lock_surface(VASurfaceID id, bool write)
{
    // Check if decode is completed
    VAStatus status = vaSyncSurface(m_vaDisplay, id);
    if (VAFAILED(status)) {
        printf("vaSyncSurface Error.\n");
        return NULL;
    }

    //sync decode surface
    VASurfaceStatus surf_status = VASurfaceSkipped;
    for (;;) {
        vaQuerySurfaceStatus(m_vaDisplay, id, &surf_status);
        if (surf_status != VASurfaceRendering &&
            surf_status != VASurfaceDisplaying)
            break;
    }

    if (surf_status != VASurfaceReady) {
        printf("Surface is not ready by vaQueryStatusSurface");
        return NULL;
    }

    uint8_t* buffer = NULL;
    //map the decoded buffer
    for (;;) {
        status = vaDeriveImage(m_vaDisplay, id, &m_images[id]);
        if (VAFAILED(status))
            printf("vaDeriveImage fail. \n");

        status = vaMapBuffer(m_vaDisplay, m_images[id].buf, (void**)&buffer);
        if (VAFAILED(status))
            printf("vaDeriveImage fail. \n");

        break;
    }

    if (VAFAILED(status)) {
        status = vaUnmapBuffer(m_vaDisplay, m_images[id].buf);
        status = vaDestroyImage(m_vaDisplay, m_images[id].image_id);
    }
    return buffer;
}

void mvaccel::VDecAccelVAImpl::unlock_surface(VASurfaceID id)
{
    VAStatus status = vaUnmapBuffer(m_vaDisplay, m_images[id].buf);
    assert(VASUCCEEDED(status));

    status = vaDestroyImage(m_vaDisplay, m_images[id].image_id);
    assert(VASUCCEEDED(status));
}

//prepare basic format/resolution parameter
void mvaccel::VDecAccelVAImpl::create_decode_desc()
{
    m_DecodeDesc.format       = VA_FOURCC_NV12;
    m_DecodeDesc.sfcformat    = VA_FOURCC_ARGB;
    m_DecodeDesc.width        = 352;
    m_DecodeDesc.height       = 288;
    m_DecodeDesc.sfc_widht    = 176;
    m_DecodeDesc.sfc_height   = 144;
    m_DecodeDesc.surfaces_num = 2;
}

bool mvaccel::VDecAccelVAImpl::DecodePicture()
{
    // Create addition surfaces for scaled video output
    if (m_sfcIDs.empty()) {
        if (create_resources())
            return 1;
    }

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAContextID vaContextID = 0;
    VASurfaceID vaID = 0;

    //va begin picture
    vaStatus = vaBeginPicture(m_vaDisplay, m_vaContextID, vaID);
    if (VAFAILED(vaStatus))
        printf("vaBeginPicture fail.");

    // Set Context ID for End Picture
    vaContextID = m_vaContextID;

    std::vector<VABufferID> vaBufferIDs;
    // Pic parameters buffers
    VABufferID vaBufferID;
    vaStatus = vaCreateBuffer(m_vaDisplay,
                              m_vaContextID,
                              VAPictureParameterBufferType,
                              sizeof(g_PicParams_AVC),
                              1,
                              (uint8_t*)g_PicParams_AVC,
                              &vaBufferID);
    assert(VASUCCEEDED(vaStatus));
    if (VASUCCEEDED(vaStatus))
        vaBufferIDs.push_back(vaBufferID);

    // IQ matrics
    vaStatus = vaCreateBuffer(m_vaDisplay,
                              m_vaContextID,
                              VAIQMatrixBufferType,
                              sizeof(g_Qmatrix_AVC),
                              1,
                              (uint8_t*)g_Qmatrix_AVC,
                              &vaBufferID);
    assert(VASUCCEEDED(vaStatus));
    if (VASUCCEEDED(vaStatus))
        vaBufferIDs.push_back(vaBufferID);

    //slice parameter buffers
    vaStatus = vaCreateBuffer(m_vaDisplay,
                              m_vaContextID,
                              VASliceParameterBufferType,
                              sizeof(g_SlcParams_AVC),
                              1,
                              (uint8_t*)g_SlcParams_AVC,
                              &vaBufferID);
    assert(VASUCCEEDED(vaStatus));
    if (VASUCCEEDED(vaStatus))
        vaBufferIDs.push_back(vaBufferID);

    //BITSTREAM buffers
    vaStatus = vaCreateBuffer(m_vaDisplay,
                              m_vaContextID,
                              VASliceDataBufferType,
                              sizeof(g_Bitstream_AVC),
                              1,
                              (uint8_t*)g_Bitstream_AVC,
                              &vaBufferID);
    assert(VASUCCEEDED(vaStatus));
    if (VASUCCEEDED(vaStatus))
        vaBufferIDs.push_back(vaBufferID);

    //PROC_PIPELINE buffers
    vaStatus = vaCreateBuffer(m_vaDisplay,
                              m_vaContextID,
                              VAProcPipelineParameterBufferType,
                              sizeof(m_vaProcBuffer),
                              1,
                              (uint8_t*)&m_vaProcBuffer,
                              &vaBufferID);
    assert(VASUCCEEDED(vaStatus));
    if (VASUCCEEDED(vaStatus))
        vaBufferIDs.push_back(vaBufferID);

    if (vaBufferIDs.size()) {
        vaStatus = vaRenderPicture(
                       m_vaDisplay,
                       m_vaContextID,
                       &(vaBufferIDs.at(0)),
                       vaBufferIDs.size());
    }

    //va end picture
    vaStatus = vaEndPicture(m_vaDisplay, vaContextID);
    if (VAFAILED(vaStatus))
        printf("vaEndPicture fail.");

    //lock surface
    uint8_t* gfx_surface_buf = lock_surface(m_sfcIDs[0], false);
    if (gfx_surface_buf == NULL) {
        printf("Fail to lock gfx surface\n");
        return 1;
    }

    //write to yuv file
    FILE* sfc_stream = fopen("sfc_sample_176_144_argb.yuv", "wb");
    uint32_t file_size = m_DecodeDesc.sfc_widht * m_DecodeDesc.sfc_height * 4; //ARGB format
    if (sfc_stream) {
        fwrite(gfx_surface_buf, file_size, 1, sfc_stream);
        fclose(sfc_stream);
    }

    //unlock surface and clear
    unlock_surface(m_sfcIDs[0]);
    Close();

    return (VASUCCEEDED(vaStatus) ? 0 : 1);
}

// check vaQueryVideoProcPipelineCaps
int mvaccel::VDecAccelVAImpl::check_process_pipeline_caps(DecodeDesc& desc)
{
    VAProcPipelineCaps caps;

    VAProcColorStandardType inputCST[VAProcColorStandardCount];
    VAProcColorStandardType outputCST[VAProcColorStandardCount];
    caps.input_color_standards = inputCST;
    caps.output_color_standards = outputCST;

    m_in4CC.resize(vaMaxNumImageFormats(m_vaDisplay));
    m_out4CC.resize(vaMaxNumImageFormats(m_vaDisplay));
    caps.input_pixel_format = &m_in4CC.at(0);
    caps.output_pixel_format = &m_out4CC.at(0);

    VABufferID filterIDs[VAProcFilterCount];
    uint32_t filterCount = 0;

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vaStatus = vaQueryVideoProcPipelineCaps(
                   m_vaDisplay,
                   m_vaContextID,
                   filterIDs,
                   filterCount,
                   &caps
               );
    if (VAFAILED(vaStatus)) {
        printf("vaQueryVideoProcPipelineCaps fail\n");
        return 1;
    }

    m_in4CC.resize(caps.num_input_pixel_formats);
    m_out4CC.resize(caps.num_output_pixel_formats);

    return 0;
}

int mvaccel::VDecAccelVAImpl::create_resources()
{
    if (m_sfcIDs.empty())
        m_sfcIDs.resize(1);

    // Create addition surfaces for scaled video output
    uint16_t width = m_DecodeDesc.sfc_widht;
    uint16_t height = m_DecodeDesc.sfc_height;

    //prepare sfc surface attribs
    DecodeDesc SfcDesc;
    SfcDesc.sfcformat = VA_FOURCC('A', 'R', 'G', 'B');
    VASurfaceAttribArray Sfc_vaSurfAttribs;
    prepare_surface_attribs(SfcDesc, Sfc_vaSurfAttribs, true);

    uint32_t surfaceType = m_surfaceType;
    m_surfaceType = (uint32_t)SfcDesc.sfcformat;

    for (uint32_t i = 0; i < m_sfcIDs.size(); i++) {
        vaCreateSurfaces(
            m_vaDisplay,
            m_surfaceType,
            width,
            height,
            &m_sfcIDs[i],
            1,
            &(Sfc_vaSurfAttribs.at(0)),
            Sfc_vaSurfAttribs.size()
        );
    }
    m_surfaceType = surfaceType;
    m_rectSFC.x = m_rectSFC.y = 0;
    m_rectSFC.width = width;
    m_rectSFC.height = height;

    // Prepare VAProcPipelineParameterBuffer for decode
    VAProcPipelineParameterBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    m_rectSrc.x = m_rectSrc.y = 0;
    m_rectSrc.width = (uint16_t)m_DecodeDesc.width;
    m_rectSrc.height = (uint16_t)m_DecodeDesc.height;

    buffer.surface_region = &m_rectSrc;
    buffer.output_region = &m_rectSFC;
    buffer.additional_outputs = (VASurfaceID*) & (m_sfcIDs[0]);
    buffer.num_additional_outputs = 1;
    m_vaProcBuffer = buffer;

    return 0;
}
