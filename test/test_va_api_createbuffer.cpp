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

std::ostream& operator<<(std::ostream& os, const testInput& t)
{
    return os << t.inputConfig.first
              << "," << t.inputConfig.second
              << "," << t.inputBufferType
    ;
}

typedef std::pair<VAProfile, VAEntrypoint> ConfigPair;

static const std::vector<ConfigPair> decoders = {
    std::make_pair(VAProfileMPEG2Simple, VAEntrypointVLD),
    std::make_pair(VAProfileMPEG2Main, VAEntrypointVLD),
    std::make_pair(VAProfileH264Main, VAEntrypointVLD),
    std::make_pair(VAProfileH264ConstrainedBaseline, VAEntrypointVLD),
    std::make_pair(VAProfileH264High, VAEntrypointVLD),
    std::make_pair(VAProfileH264MultiviewHigh, VAEntrypointVLD),
    std::make_pair(VAProfileH264StereoHigh, VAEntrypointVLD),
    std::make_pair(VAProfileVC1Simple, VAEntrypointVLD),
    std::make_pair(VAProfileVC1Main, VAEntrypointVLD),
    std::make_pair(VAProfileVC1Advanced, VAEntrypointVLD),
    std::make_pair(VAProfileJPEGBaseline, VAEntrypointVLD),
    std::make_pair(VAProfileVP8Version0_3, VAEntrypointVLD),
    std::make_pair(VAProfileHEVCMain, VAEntrypointVLD),
    std::make_pair(VAProfileHEVCMain10, VAEntrypointVLD),
    std::make_pair(VAProfileVP9Profile0, VAEntrypointVLD),
    std::make_pair(VAProfileVP9Profile2, VAEntrypointVLD),
};

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

static const std::vector<ConfigPair> encoders = {
    std::make_pair(VAProfileMPEG2Simple, VAEntrypointEncSlice),
    std::make_pair(VAProfileMPEG2Main, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264Main, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264ConstrainedBaseline, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264High, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264MultiviewHigh, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264StereoHigh, VAEntrypointEncSlice),
    std::make_pair(VAProfileH264Main, VAEntrypointFEI),
    std::make_pair(VAProfileH264ConstrainedBaseline, VAEntrypointFEI),
    std::make_pair(VAProfileH264High, VAEntrypointFEI),
    std::make_pair(VAProfileJPEGBaseline, VAEntrypointEncSlice),
    std::make_pair(VAProfileVP8Version0_3, VAEntrypointEncSlice),
    std::make_pair(VAProfileHEVCMain, VAEntrypointEncSlice),
    std::make_pair(VAProfileHEVCMain10, VAEntrypointEncSlice),
    std::make_pair(VAProfileVP9Profile0, VAEntrypointEncSlice),
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
    VAEncMacroblockMapBufferType,
    VAEncFEIMVBufferType,
    VAEncFEIMBCodeBufferType,
    VAEncFEIDistortionBufferType,
    VAEncFEIMBControlBufferType,
    VAEncFEIMVPredictorBufferType,
};

static const std::vector<ConfigPair> vpps = {
    std::make_pair(VAProfileNone, VAEntrypointVideoProc),
};

static const std::vector<VABufferType> postProcessorBufferTypes = {
    VAProcPipelineParameterBufferType,
    VAProcFilterParameterBufferType,
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

    if (doFindProfileInList(currentTestInput.inputConfig.first)) {
        doQueryConfigEntrypoints(currentTestInput.inputConfig.first);
        if (doFindEntrypointInList(currentTestInput.inputConfig.second)) {
            // profile and entrypoint are supported, if not supported then do
            // not test for vaCreateBuffer

            doCreateConfig(
                currentTestInput.inputConfig.first,
                currentTestInput.inputConfig.second);

            // vaCreateContext input requires resolution, since this test
            // doesn't create surfaces, passing 0x0 resolution should provide
            // the desired result.
            doCreateContext(std::make_pair(0, 0));

            doCreateBuffer(currentTestInput.inputBufferType);
            doDestroyBuffer();

            doDestroyContext();
            doDestroyConfig();
        }
    }
    else {
        doLogSkipTest(
            currentTestInput.inputConfig.first,
            currentTestInput.inputConfig.second);
    }
}

std::vector<testInput> generateInput()
{
    std::vector<testInput> inputs;

    for (auto config : decoders) {
        for (auto bufferType : decoderBufferTypes) {
            inputs.push_back(testInput{config, bufferType});
        }
    }

    for (auto config : encoders) {
        for (auto bufferType : encoderBufferTypes) {
            inputs.push_back(testInput{config, bufferType});
        }
    }

    for (auto config : vpps) {
        for (auto bufferType : postProcessorBufferTypes) {
            inputs.push_back(testInput{config, bufferType});
        }
    }

    return inputs;
}

INSTANTIATE_TEST_CASE_P(
    CreateBuffer, VAAPICreateBuffer,
    ::testing::ValuesIn(generateInput()));
} // VAAPI
