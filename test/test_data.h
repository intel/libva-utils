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

#include "test_defs.h"
#include "test_utils.h"

#include <map>
#include <va/va.h>
#include <va/va_drmcommon.h>

namespace VAAPI
{

static const Profiles g_vaProfiles = {
    VAProfileNone,
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
    VAProfileVP9Profile3,
#if VA_CHECK_VERSION(1,8,0)
    VAProfileAV1Profile0,
    VAProfileAV1Profile1,
#endif
#if VA_CHECK_VERSION(1,18,0)
    VAProfileH264High10,
#endif
};

static const Profiles g_vaNoneProfiles = {
    VAProfileNone,
};

static const Profiles g_vaMPEG2Profiles = {
    VAProfileMPEG2Main, VAProfileMPEG2Simple,
};

static const Profiles g_vaMPEG4Profiles = {
    VAProfileMPEG4Simple, VAProfileMPEG4Simple, VAProfileMPEG4AdvancedSimple,
    VAProfileMPEG4Main,
};

static const Profiles g_vaH263Profiles = {
    VAProfileH263Baseline,
};

static const Profiles g_vaH264Profiles = {
    VAProfileH264Main, VAProfileH264High, VAProfileH264ConstrainedBaseline,
    VAProfileH264MultiviewHigh, VAProfileH264StereoHigh,
#if VA_CHECK_VERSION(1,18,0)
    VAProfileH264High10,
#endif
};

static const Profiles g_vaVC1Profiles = {
    VAProfileVC1Simple, VAProfileVC1Main, VAProfileVC1Advanced,
};

static const Profiles g_vaJPEGProfiles = {
    VAProfileJPEGBaseline,
};

static const Profiles g_vaVP8Profiles = {
    VAProfileVP8Version0_3,
};

static const Profiles g_vaHEVCProfiles = {
    VAProfileHEVCMain, VAProfileHEVCMain10,
};

static const Profiles g_vaVP9Profiles = {
    VAProfileVP9Profile0, VAProfileVP9Profile1, VAProfileVP9Profile2,
    VAProfileVP9Profile3,
};

static const Profiles g_vaAV1Profiles = {
#if VA_CHECK_VERSION(1,8,0)
    VAProfileAV1Profile0, VAProfileAV1Profile1,
#endif
};

static const Entrypoints g_vaEntrypoints = {
    VAEntrypointVLD,
    VAEntrypointIZZ,
    VAEntrypointIDCT,
    VAEntrypointMoComp,
    VAEntrypointDeblocking,
    VAEntrypointEncSlice,
    VAEntrypointEncPicture,
    VAEntrypointEncSliceLP,
    VAEntrypointVideoProc,
    VAEntrypointFEI,
};

static const ConfigAttribTypes g_vaConfigAttribTypes = {
    VAConfigAttribRTFormat,
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
    VAConfigAttribFEIMVPredictors,
};

static const SurfaceAttribTypes g_vaSurfaceAttribTypes = {
    VASurfaceAttribNone,
    VASurfaceAttribPixelFormat,
    VASurfaceAttribMinWidth,
    VASurfaceAttribMaxWidth,
    VASurfaceAttribMinHeight,
    VASurfaceAttribMaxHeight,
    VASurfaceAttribMemoryType,
    VASurfaceAttribExternalBufferDescriptor,
    VASurfaceAttribUsageHint,
    VASurfaceAttribCount,
};

static const Resolutions g_vaResolutions = {
    { 16, 16 },      { 32, 32 },     { 176, 120 },   { 352, 240 },
    { 352, 288 },    { 480, 320 },   { 720, 480 },   { 720, 576 },
    { 768, 576 },    { 800, 480 },   { 854, 480 },   { 1280, 720 },
    { 1920, 1080 },  { 2048, 1080 }, { 4096, 2160 }, { 8192, 8192 },
    { 10240, 10240 }
};

static const BitMasks g_vaRTFormats = {
    VA_RT_FORMAT_YUV420, VA_RT_FORMAT_YUV422, VA_RT_FORMAT_YUV444,
    VA_RT_FORMAT_YUV411, VA_RT_FORMAT_YUV400, VA_RT_FORMAT_YUV420_10BPP,
    VA_RT_FORMAT_YUV422_10, VA_RT_FORMAT_YUV444_10, VA_RT_FORMAT_YUV420_12,
    VA_RT_FORMAT_YUV422_12, VA_RT_FORMAT_YUV444_12,
    VA_RT_FORMAT_RGB16, VA_RT_FORMAT_RGB32, VA_RT_FORMAT_RGBP,
    VA_RT_FORMAT_RGB32_10BPP, VA_RT_FORMAT_PROTECTED,
};

static const BufferTypes g_vaBufferTypes = {
    VAPictureParameterBufferType,
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
    VAEncFEIMVPredictorBufferType,
};

static const BitMasks g_vaRateControls = {
    VA_RC_NONE, VA_RC_CBR, VA_RC_VBR, VA_RC_VCM, VA_RC_CQP,
    VA_RC_VBR_CONSTRAINED, VA_RC_ICQ, VA_RC_MB, VA_RC_CFS,
    VA_RC_PARALLEL, VA_RC_QVBR, VA_RC_AVBR,
#if VA_CHECK_VERSION(1, 10, 0)
    VA_RC_TCBRC,
#endif
};

static const BitMasks g_vaDecSliceModes = {
    VA_DEC_SLICE_MODE_NORMAL, VA_DEC_SLICE_MODE_BASE,
};

static const BitMasks g_vaEncPackedHeaders = {
    VA_ENC_PACKED_HEADER_SEQUENCE, VA_ENC_PACKED_HEADER_PICTURE,
    VA_ENC_PACKED_HEADER_SLICE, VA_ENC_PACKED_HEADER_MISC,
    VA_ENC_PACKED_HEADER_RAW_DATA,
};

static const BitMasks g_vaEncInterlaced = {
    VA_ENC_INTERLACED_FRAME, VA_ENC_INTERLACED_FIELD, VA_ENC_INTERLACED_MBAFF,
    VA_ENC_INTERLACED_PAFF,
};

static const BitMasks g_vaFEIFunctionTypes = {
    VA_FEI_FUNCTION_ENC, VA_FEI_FUNCTION_PAK, VA_FEI_FUNCTION_ENC_PAK,
};

#if VA_CHECK_VERSION(1, 11, 0)
static const BitMasks g_vaEncryptionTypes = {
    VA_ENCRYPTION_TYPE_FULLSAMPLE_CTR,
    VA_ENCRYPTION_TYPE_FULLSAMPLE_CBC,
    VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR,
    VA_ENCRYPTION_TYPE_SUBSAMPLE_CBC,
};
#endif

static const std::map<VAConfigAttribType, const BitMasks&>
g_vaConfigAttribBitMasks = {
    {VAConfigAttribRTFormat, g_vaRTFormats},
    {VAConfigAttribRateControl, g_vaRateControls},
    {VAConfigAttribDecSliceMode, g_vaDecSliceModes},
    {VAConfigAttribEncPackedHeaders, g_vaEncPackedHeaders},
    {VAConfigAttribEncInterlaced, g_vaEncInterlaced},
    {VAConfigAttribFEIFunctionType, g_vaFEIFunctionTypes},
#if VA_CHECK_VERSION(1, 11, 0)
    {VAConfigAttribEncryption, g_vaEncryptionTypes},
#endif
};

static const BitMasks g_vaSurfaceMemTypes = {
    VA_SURFACE_ATTRIB_MEM_TYPE_VA, VA_SURFACE_ATTRIB_MEM_TYPE_V4L2,
    VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR, VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM,
    VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME,
    VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
#if VA_CHECK_VERSION(1, 21, 0)
    VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_3,
#endif
};

static const std::map<VASurfaceAttribType, const BitMasks&>
g_vaSurfaceAttribBitMasks = {
    {VASurfaceAttribMemoryType, g_vaSurfaceMemTypes},
};

} // namespace VAAPI
#endif
