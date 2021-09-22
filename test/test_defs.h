/*
 * Copyright (C) 2018 Intel Corporation. All Rights Reserved.
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

#ifndef TESTVAAPI_test_defs_h
#define TESTVAAPI_test_defs_h

#include "test_utils.h"

#include <va/va.h>
#include <vector>

namespace VAAPI
{

typedef std::vector<VAProfile>            Profiles;
typedef std::vector<VAEntrypoint>         Entrypoints;
typedef std::vector<VAConfigAttribType>   ConfigAttribTypes;
typedef std::vector<VAConfigAttrib>       ConfigAttributes;
typedef std::vector<VASurfaceAttribType>  SurfaceAttribTypes;
typedef std::vector<VASurfaceAttrib>      SurfaceAttributes;
typedef std::vector<VASurfaceID>          Surfaces;
typedef std::vector<VABufferType>         BufferTypes;
typedef std::vector<VABufferID>           Buffers;
typedef std::vector<VADisplayAttribType>  DisplayAttribTypes;
typedef std::vector<VADisplayAttribute>   DisplayAttributes;
typedef std::vector<Resolution>           Resolutions;
typedef std::vector<uint32_t>             BitMasks;

} // namespace VAAPI

#endif
