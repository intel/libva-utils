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
 * @file VDecAccelVA.h
 * @brief LibVA decode accelerator declaration
 */

#ifndef MV_ACCELERATOR_VAAPI_DECODE_H
#define MV_ACCELERATOR_VAAPI_DECODE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <algorithm>
#include <va/va.h>
#include "DecodeParamBuffer.h"

namespace mvaccel
{
/**
 * @brief LibVA decode accelerator
 */
class VDecAccelVAImpl
{
public:
    VDecAccelVAImpl(void* device);
    VDecAccelVAImpl();
    virtual ~VDecAccelVAImpl();

    // VDecAccel interface
    virtual int         Open();
    virtual void        Close();
    virtual uint32_t    GetSurfaceID(uint32_t index);
    bool                DecodePicture();
    void                bind_buffer(uint8_t* base);

protected:
    // GfxSurfaceAccess interface
    uint32_t    get_width(VASurfaceID id);
    uint32_t    get_height(VASurfaceID id);
    uint32_t    get_stride(VASurfaceID id);
    uint8_t*    lock_surface(VASurfaceID id, bool write);
    void        unlock_surface(VASurfaceID id);
    void*       get_raw(VASurfaceID id);
    void*       get_device(VASurfaceID id);

    typedef std::vector<VAConfigAttrib>     VAConfigAttribArray;
    typedef std::vector<VASurfaceAttrib>    VASurfaceAttribArray;

    // Member functions inherit by children
    virtual bool    is_config_compatible(DecodeDesc& desc);
    virtual bool    is_rt_foramt_supported(DecodeDesc& desc);
    virtual void    prepare_config_attribs(
        DecodeDesc& desc,
        VAConfigAttribArray& attribs);
    virtual void    prepare_surface_attribs(
        DecodeDesc& desc,
        VASurfaceAttribArray& attribs,
        bool bDecodeDownsamplingHinted);

    // Member fucntions NOT inherit by children
    bool        is_slice_mode_supported(DecodeDesc& desc);
    bool        is_encryption_supported(DecodeDesc& desc);
    bool        is_sfc_config_supported(DecodeDesc& desc);
    VAStatus    render_picture(VAContextID& vaContextID);
    VAStatus    query_status();
    VAStatus    create_surface(
        uint32_t width,
        uint32_t height,
        VASurfaceAttribArray& vaAttribs,
        VASurfaceID& vaID);
    void        delete_surface(VASurfaceID& vaID);
    void        create_decode_desc();
    int         check_process_pipeline_caps(DecodeDesc& desc);
    int         create_resources();

protected:
    int drm_fd = -1;
    // VA ID & Handles
    VADisplay       m_vaDisplay;    /**< @brief VA hardware device */
    VAProfile       m_vaProfile;    /**< @brief Video decoder profile */
    VAEntrypoint    m_vaEntrypoint; /**< @brief VA entry point */
    VAConfigID      m_vaConfigID;   /**< @brief VA decode config id*/
    VAContextID     m_vaContextID;  /**< @brief Video decoder context */

    // VASurfaces id and attributes
    std::vector<VASurfaceID>    m_vaIDs;
    VASurfaceAttribArray        m_vaSurfAttribs;

    uint32_t    m_surfaceType;   /**< @brief surface type */

    // Gfx surface access management
    std::map<VASurfaceID, VAImage>      m_images;   /**< @brief buf pointer */

    enum SFC {
        MAX_IN_W,
        MAX_IN_H,
        MIN_IN_W,
        MIN_IN_H,
        MAX_OUT_W,
        MAX_OUT_H,
        MIN_OUT_W,
        MIN_OUT_H,
        NEW_W,
        NEW_H,
    };

    DecodeDesc                    m_DecodeDesc{};     /**< @brief decode discription */
    VARectangle                   m_rectSrc{};        /**< @brief Rectangle for source input */
    VARectangle                   m_rectSFC{};        /**< @brief Rectangle for SFC output */
    std::vector<uint32_t>         m_in4CC{};          /**< @brief input FOURCC */
    std::vector<uint32_t>         m_out4CC{};         /**< @brief output FOURCC */
    std::map<SFC, uint32_t>        m_sfcSize{};       /**< @brief SFC sizes */
    std::vector<VASurfaceID>      m_sfcIDs{};         /**< @brief sfc surfaces */
    VAProcPipelineParameterBuffer m_vaProcBuffer{};   /**< @brief sfc pipeline buffer */
};
} // namespace mvaccel
#endif // MV_ACCELERATOR_VAAPI_DECODE_H
