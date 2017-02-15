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

#include "test_va_api_createbuffer.h"

namespace VAAPI {

// Testing VABufferType in groups that will be associated with VAProfile and
// VAEntrypoint. vaCreateBuffer doesn't require these itself but its input
// parameter do care about them.

// VAProtectedSliceDataBufferType left out on purpose
static const std::vector<VABufferType> decoderBufferTypes ={
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
    VAQMatrixBufferType,
    VAHuffmanTableBufferType,
    VAProbabilityBufferType,
};

// VAEncMacroblockParameterBufferType left out on purpose
static const std::vector<VABufferType> encoderBufferTypes ={
    VAEncCodedBufferType,
    VAEncSequenceParameterBufferType,
    VAEncPictureParameterBufferType,
    VAEncSliceParameterBufferType,
    VAEncPackedHeaderParameterBufferType,
    VAEncPackedHeaderDataBufferType,
    VAEncMiscParameterBufferType,
    VAEncMacroblockMapBufferType
};

static const std::vector<VABufferType> postProcessorBufferTypes
    = { VAProcPipelineParameterBufferType, VAProcFilterParameterBufferType };

static const std::vector<testInput> input = {
    { VAProfileMPEG2Simple, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileMPEG2Main, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileH264Main, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileH264ConstrainedBaseline, VAEntrypointVLD, decoderBufferTypes},
    { VAProfileH264High, VAEntrypointVLD, decoderBufferTypes},
    { VAProfileH264MultiviewHigh, VAEntrypointVLD, decoderBufferTypes},
    { VAProfileH264StereoHigh, VAEntrypointVLD, decoderBufferTypes},
    { VAProfileVC1Simple, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileVC1Main, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileVC1Advanced, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileJPEGBaseline, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileVP8Version0_3, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileHEVCMain, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileHEVCMain10, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileVP9Profile0, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileVP9Profile2, VAEntrypointVLD, decoderBufferTypes },
    { VAProfileH264Main, VAEntrypointEncSlice, encoderBufferTypes},
    { VAProfileH264ConstrainedBaseline, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileH264High, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileH264MultiviewHigh, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileH264StereoHigh, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileJPEGBaseline, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileVP8Version0_3, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileHEVCMain, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileHEVCMain10, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileVP9Profile0, VAEntrypointEncSlice, encoderBufferTypes },
    { VAProfileNone, VAEntrypointVideoProc, postProcessorBufferTypes }
};

VAAPICreateBuffer::VAAPICreateBuffer()
{
    m_vaDisplay = doInitialize();
}

VAAPICreateBuffer::~VAAPICreateBuffer()
{
    doTerminate();
}

TEST_P(VAAPICreateBuffer, CreateBufferWithOutData)
{


    // vaCreateBuffer uses a VAContextID as an input.  This VAContextID requires
    // a VAConfigID to be created.  VAConfigID requires VAProfile and
    // VAEntrypoint to be given.  As such, to test vaCreateBuffer these are
    // the minimum requirements.  There's no need to create surfaces or attach
    // them to a VAConfigID.

    testInput currentTestInput = GetParam();

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(currentTestInput.inputProfile)) {

        doQueryConfigEntrypoints(currentTestInput.inputProfile);
        if (doFindEntrypointInList(
                currentTestInput.inputEntrypoint)) {
            // profile and entrypoint are supported, if not supported then do
            // not test for vaCreateBuffer

            doCreateConfig(
                currentTestInput.inputProfile,
                currentTestInput.inputEntrypoint);

            // vaCreateContext input requires resolution, since this test
            // doesn't create surfaces, passing 0x0 resolution should provide
            // the desired result.
            doCreateContext(std::make_pair(0, 0));

            for (auto& bufferTypeIT : currentTestInput.inputBufferType) {
                doCreateBuffer(bufferTypeIT);
                doDestroyBuffer();
            }

            doDestroyContext();
            doDestroyConfig();
        }
    }
    else {
        doLogSkipTest(currentTestInput.inputProfile, currentTestInput.inputEntrypoint);
    }
}

INSTANTIATE_TEST_CASE_P(
    CreateBuffer, VAAPICreateBuffer,
		      ::testing::ValuesIn(input));
} // VAAPI
