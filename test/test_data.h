/*
 * Copyright (C) 2016 Intel Corporation. All Rights Reserved.
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

#ifndef TESTVAAPI_test_data_h
#define TESTVAAPI_test_data_h

#include <va/va.h>

namespace VAAPI {
static const std::vector<VAProfile> m_vaProfiles
    = { VAProfileNone,
        VAProfileMPEG2Simple,
        VAProfileMPEG2Main,
        VAProfileMPEG4Simple,
        VAProfileMPEG4AdvancedSimple,
        VAProfileMPEG4Main,
        VAProfileH264Main,
        VAProfileH264High,
        VAProfileVC1Simple,
        VAProfileVC1Main,
        VAProfileVC1Advanced,
        VAProfileH263Baseline,
        VAProfileJPEGBaseline,
        VAProfileH264ConstrainedBaseline,
        VAProfileVP8Version0_3,
        VAProfileH264MultiviewHigh,
        VAProfileH264StereoHigh,
        VAProfileHEVCMain,
        VAProfileHEVCMain10,
        VAProfileVP9Profile0,
        VAProfileVP9Profile1,
        VAProfileVP9Profile2,
        VAProfileVP9Profile3 };

static const std::vector<VAEntrypoint> m_vaEntrypoints
    = { VAEntrypointVLD,        VAEntrypointIZZ,        VAEntrypointIDCT,
        VAEntrypointMoComp,     VAEntrypointDeblocking, VAEntrypointEncSlice,
        VAEntrypointEncPicture, VAEntrypointEncSliceLP, VAEntrypointVideoProc,
        VAEntrypointFEI };

static const std::vector<VAConfigAttribType> m_vaConfigAttribs
    = { VAConfigAttribRTFormat,
        VAConfigAttribSpatialResidual,
        VAConfigAttribSpatialClipping,
        VAConfigAttribIntraResidual,
        VAConfigAttribEncryption,
        VAConfigAttribRateControl,
        VAConfigAttribDecSliceMode,
        VAConfigAttribEncPackedHeaders,
        VAConfigAttribEncInterlaced,
        VAConfigAttribEncMaxRefFrames,
        VAConfigAttribEncMaxSlices,
        VAConfigAttribEncSliceStructure,
        VAConfigAttribEncMacroblockInfo,
        VAConfigAttribEncJPEG,
        VAConfigAttribEncQualityRange,
        VAConfigAttribEncSkipFrame,
        VAConfigAttribEncROI,
        VAConfigAttribEncRateControlExt,
        VAConfigAttribFEIFunctionType,
        VAConfigAttribFEIMVPredictors };

static const std::vector<VASurfaceAttribType> m_vaSurfaceAttribs
    = { VASurfaceAttribNone,       VASurfaceAttribPixelFormat,
        VASurfaceAttribMinWidth,   VASurfaceAttribMaxWidth,
        VASurfaceAttribMinHeight,  VASurfaceAttribMaxHeight,
        VASurfaceAttribMemoryType, VASurfaceAttribExternalBufferDescriptor,
        VASurfaceAttribUsageHint,  VASurfaceAttribCount };

static const std::vector<std::pair<uint32_t, uint32_t> > m_vaResolutions
    = { { 16, 16 },      { 32, 32 },     { 176, 120 },   { 352, 240 },
        { 352, 288 },    { 480, 320 },   { 720, 480 },   { 720, 576 },
        { 768, 576 },    { 800, 480 },   { 854, 480 },   { 1280, 720 },
        { 1920, 1080 },  { 2048, 1080 }, { 4096, 2160 }, { 8192, 8192 },
        { 10240, 10240 } };

static const std::vector<uint32_t> m_vaRTFormats
    = { VA_RT_FORMAT_YUV420,   VA_RT_FORMAT_YUV422, VA_RT_FORMAT_YUV444,
        VA_RT_FORMAT_YUV411,   VA_RT_FORMAT_YUV400, VA_RT_FORMAT_YUV420_10BPP,
        VA_RT_FORMAT_RGB16,    VA_RT_FORMAT_RGB32,  VA_RT_FORMAT_RGBP,
        VA_RT_FORMAT_PROTECTED };

static const std::vector<VABufferType> m_vaBufferTypes
    = { VAPictureParameterBufferType,
        VAIQMatrixBufferType,
        VABitPlaneBufferType,
        VASliceGroupMapBufferType,
        VASliceParameterBufferType,
        VASliceDataBufferType,
        VAMacroblockParameterBufferType,
        VAResidualDataBufferType,
        VADeblockingParameterBufferType,
        VAImageBufferType,
        VAProtectedSliceDataBufferType,
        VAQMatrixBufferType,
        VAHuffmanTableBufferType,
        VAProbabilityBufferType,
        VAEncCodedBufferType,
        VAEncSequenceParameterBufferType,
        VAEncPictureParameterBufferType,
        VAEncSliceParameterBufferType,
        VAEncPackedHeaderParameterBufferType,
        VAEncPackedHeaderDataBufferType,
        VAEncMiscParameterBufferType,
        VAEncMacroblockParameterBufferType,
        VAEncMacroblockMapBufferType,
        VAProcPipelineParameterBufferType,
        VAProcFilterParameterBufferType,
        VAEncFEIMVBufferType,
        VAEncFEIMBCodeBufferType,
        VAEncFEIDistortionBufferType,
        VAEncFEIMBControlBufferType,
        VAEncFEIMVPredictorBufferType };
}
#endif
