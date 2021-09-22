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

#ifndef TESTVAAPI_test_streamable_h
#define TESTVAAPI_test_streamable_h

#include <iostream>
#include <va/va.h>
#include <va/va_str.h>

#define CASE_ENUM_TO_STREAM(caseEnum) case caseEnum: os << #caseEnum; break;

inline std::ostream&
operator<<(std::ostream& os, const VAProfile& profile)
{
    return os << static_cast<int>(profile)
           << ":" << vaProfileStr(profile);
}

inline std::ostream&
operator<<(std::ostream& os, const VAEntrypoint& entrypoint)
{
    return os << static_cast<int>(entrypoint)
           << ":" << vaEntrypointStr(entrypoint);
}

inline std::ostream&
operator<<(std::ostream& os, const VABufferType& type)
{
    return os << static_cast<int>(type)
           << ":" << vaBufferTypeStr(type);
}

inline std::ostream&
operator<<(std::ostream& os, const VAGenericValueType& type)
{
    switch (type) {
        CASE_ENUM_TO_STREAM(VAGenericValueTypeInteger)
        CASE_ENUM_TO_STREAM(VAGenericValueTypeFloat)
        CASE_ENUM_TO_STREAM(VAGenericValueTypePointer)
        CASE_ENUM_TO_STREAM(VAGenericValueTypeFunc)
    default:
        break;
    }
    return os << "(" << static_cast<int>(type) << ")";
}

inline std::ostream&
operator<<(std::ostream& os, const VAGenericValue& value)
{
#define TOSTR(enumCase, field) case enumCase: return os << value.value.field; break
    os << value.type << ":";
    switch (value.type) {
        TOSTR(VAGenericValueTypeInteger, i);
        TOSTR(VAGenericValueTypeFloat, f);
        TOSTR(VAGenericValueTypePointer, p);
        TOSTR(VAGenericValueTypeFunc, fn);
    default:
        return os << "?";
    }
#undef TOSTR
}

inline std::ostream&
operator <<(std::ostream& os, const VASurfaceAttribType& type)
{
    switch (type) {
        CASE_ENUM_TO_STREAM(VASurfaceAttribNone)
        CASE_ENUM_TO_STREAM(VASurfaceAttribPixelFormat)
        CASE_ENUM_TO_STREAM(VASurfaceAttribMinWidth)
        CASE_ENUM_TO_STREAM(VASurfaceAttribMaxWidth)
        CASE_ENUM_TO_STREAM(VASurfaceAttribMinHeight)
        CASE_ENUM_TO_STREAM(VASurfaceAttribMaxHeight)
        CASE_ENUM_TO_STREAM(VASurfaceAttribMemoryType)
        CASE_ENUM_TO_STREAM(VASurfaceAttribExternalBufferDescriptor)
        CASE_ENUM_TO_STREAM(VASurfaceAttribUsageHint)
        CASE_ENUM_TO_STREAM(VASurfaceAttribCount)
    default:
        break;
    }
    return os << "(" << static_cast<int>(type) << ")";
}

inline std::ostream&
operator<<(std::ostream& os, const VASurfaceAttrib& attrib)
{
    return os << "VASurfaceAttrib("
           << "type = " << attrib.type
           << ", "
           << "flags = " << attrib.flags
           << ", "
           << "value = " << attrib.value
           << ")"
           ;
}

inline std::ostream&
operator <<(std::ostream& os, const VAConfigAttribType& type)
{
    switch (type) {
        CASE_ENUM_TO_STREAM(VAConfigAttribRTFormat)
        CASE_ENUM_TO_STREAM(VAConfigAttribSpatialResidual)
        CASE_ENUM_TO_STREAM(VAConfigAttribSpatialClipping)
        CASE_ENUM_TO_STREAM(VAConfigAttribIntraResidual)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncryption)
        CASE_ENUM_TO_STREAM(VAConfigAttribRateControl)
        CASE_ENUM_TO_STREAM(VAConfigAttribDecSliceMode)
        CASE_ENUM_TO_STREAM(VAConfigAttribDecJPEG)
        CASE_ENUM_TO_STREAM(VAConfigAttribDecProcessing)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncPackedHeaders)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncInterlaced)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncMaxRefFrames)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncMaxSlices)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncSliceStructure)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncMacroblockInfo)
        CASE_ENUM_TO_STREAM(VAConfigAttribMaxPictureWidth)
        CASE_ENUM_TO_STREAM(VAConfigAttribMaxPictureHeight)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncJPEG)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncQualityRange)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncQuantization)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncIntraRefresh)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncSkipFrame)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncROI)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncRateControlExt)
        CASE_ENUM_TO_STREAM(VAConfigAttribProcessingRate)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncDirtyRect)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncParallelRateControl)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncDynamicScaling)
        CASE_ENUM_TO_STREAM(VAConfigAttribFrameSizeToleranceSupport)
        CASE_ENUM_TO_STREAM(VAConfigAttribFEIFunctionType)
        CASE_ENUM_TO_STREAM(VAConfigAttribFEIMVPredictors)
        CASE_ENUM_TO_STREAM(VAConfigAttribStats)
        CASE_ENUM_TO_STREAM(VAConfigAttribEncTileSupport)
        CASE_ENUM_TO_STREAM(VAConfigAttribCustomRoundingControl)
        CASE_ENUM_TO_STREAM(VAConfigAttribQPBlockSize)
        CASE_ENUM_TO_STREAM(VAConfigAttribMaxFrameSize)
        CASE_ENUM_TO_STREAM(VAConfigAttribPredictionDirection)
        CASE_ENUM_TO_STREAM(VAConfigAttribTypeMax)
    default:
        break;
    }
    return os << "(" << static_cast<int>(type) << ")";
}

inline std::ostream&
operator <<(std::ostream& os, const VAConfigAttrib& attrib)
{
    return os << "VAConfigAttrib("
           << "type = " << attrib.type
           << ", "
           << "value = 0x" << std::hex << attrib.value << std::dec
           << ")"
           ;
}

#undef CASE_ENUM_TO_STREAM

#endif
