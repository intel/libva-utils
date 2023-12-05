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

#include "test_va_api_fixture.h"

namespace VAAPI
{

// Testing VABufferType in groups that will be associated with VAProfile and
// VAEntrypoint. vaCreateBuffer doesn't require these itself but its input
// parameter do care about them.

typedef ::testing::WithParamInterface<std::tuple<VAProfile, VAEntrypoint,
        VABufferType, size_t>> CreateBufferParamInterface;

class VAAPICreateBuffer
    : public VAAPIFixture
    , public CreateBufferParamInterface
{
public:
    VAAPICreateBuffer()
        : profile(::testing::get<0>(GetParam()))
        , entrypoint(::testing::get<1>(GetParam()))
        , bufferType(::testing::get<2>(GetParam()))
        , bufferSize(::testing::get<3>(GetParam()))
    { }

protected:
    const VAProfile& profile;
    const VAEntrypoint& entrypoint;
    const VABufferType& bufferType;
    const size_t bufferSize;

    virtual void SetUp()
    {
        VAAPIFixture::SetUp();
        doInitialize();
        ASSERT_FALSE(HasFailure());
    }

    virtual void TearDown()
    {
        doTerminate();
        VAAPIFixture::TearDown();
    }
};

typedef std::tuple<Profiles, size_t> BufferSpec;
typedef std::vector<BufferSpec> BufferSpecs;
typedef std::map<VABufferType, BufferSpecs> BufferSpecsMap;

using std::make_tuple;

static const BufferSpecsMap decodeBufferSpecs = {
    {
        VAPictureParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VAPictureParameterBufferMPEG2)),
            make_tuple(g_vaMPEG4Profiles, sizeof(VAPictureParameterBufferMPEG4)),
            make_tuple(g_vaH264Profiles, sizeof(VAPictureParameterBufferH264)),
            make_tuple(g_vaVC1Profiles, sizeof(VAPictureParameterBufferVC1)),
            make_tuple(g_vaJPEGProfiles,
                       sizeof(VAPictureParameterBufferJPEGBaseline)),
            make_tuple(g_vaVP8Profiles, sizeof(VAPictureParameterBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAPictureParameterBufferHEVC)),
            make_tuple(g_vaVP9Profiles, sizeof(VADecPictureParameterBufferVP9)),
        }
    },
    {
        VAIQMatrixBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VAIQMatrixBufferMPEG2)),
            make_tuple(g_vaMPEG4Profiles, sizeof(VAIQMatrixBufferMPEG4)),
            make_tuple(g_vaH264Profiles, sizeof(VAIQMatrixBufferH264)),
            make_tuple(g_vaJPEGProfiles, sizeof(VAIQMatrixBufferJPEGBaseline)),
            make_tuple(g_vaVP8Profiles, sizeof(VAIQMatrixBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAIQMatrixBufferHEVC)),
        }
    },
    {
        VASliceParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VASliceParameterBufferMPEG2)),
            make_tuple(g_vaMPEG4Profiles, sizeof(VASliceParameterBufferMPEG4)),
            make_tuple(g_vaH264Profiles, sizeof(VASliceParameterBufferH264)),
            make_tuple(g_vaVC1Profiles, sizeof(VASliceParameterBufferVC1)),
            make_tuple(g_vaJPEGProfiles,
                       sizeof(VASliceParameterBufferJPEGBaseline)),
            make_tuple(g_vaVP8Profiles, sizeof(VASliceParameterBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VASliceParameterBufferHEVC)),
            make_tuple(g_vaVP9Profiles, sizeof(VASliceParameterBufferVP9)),
        }
    },
    {
        VAMacroblockParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VAMacroblockParameterBufferMPEG2)),
        }
    },
    {
        VAQMatrixBufferType, {
            make_tuple(g_vaJPEGProfiles, sizeof(VAQMatrixBufferJPEG)),
            make_tuple(g_vaVP8Profiles, sizeof(VAQMatrixBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAQMatrixBufferHEVC)),
        }
    },
    {
        VAHuffmanTableBufferType, {
            make_tuple(g_vaJPEGProfiles, sizeof(VAHuffmanTableBufferJPEGBaseline)),
        }
    },
    {
        VAProbabilityBufferType, {
            make_tuple(g_vaVP8Profiles, sizeof(VAProbabilityDataBufferVP8)),
        }
    },
};

static const BufferSpecsMap encodeBufferSpecs = {
    {
        VAEncSequenceParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles,
                       sizeof(VAEncSequenceParameterBufferMPEG2)),
            make_tuple(g_vaMPEG4Profiles,
                       sizeof(VAEncSequenceParameterBufferMPEG4)),
            make_tuple(g_vaH263Profiles, sizeof(VAEncSequenceParameterBufferH263)),
            make_tuple(g_vaH264Profiles, sizeof(VAEncSequenceParameterBufferH264)),
            make_tuple(g_vaVP8Profiles, sizeof(VAEncSequenceParameterBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAEncSequenceParameterBufferHEVC)),
            make_tuple(g_vaVP9Profiles, sizeof(VAEncSequenceParameterBufferVP9)),
            make_tuple(g_vaAV1Profiles, sizeof(VAEncSequenceParameterBufferAV1)),
        }
    },
    {
        VAEncPictureParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VAEncPictureParameterBufferMPEG2)),
            make_tuple(g_vaMPEG4Profiles, sizeof(VAEncPictureParameterBufferMPEG4)),
            make_tuple(g_vaH263Profiles, sizeof(VAEncPictureParameterBufferH263)),
            make_tuple(g_vaH264Profiles, sizeof(VAEncPictureParameterBufferH264)),
            make_tuple(g_vaJPEGProfiles, sizeof(VAEncPictureParameterBufferJPEG)),
            make_tuple(g_vaVP8Profiles, sizeof(VAEncPictureParameterBufferVP8)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAEncPictureParameterBufferHEVC)),
            make_tuple(g_vaVP9Profiles, sizeof(VAEncPictureParameterBufferVP9)),
            make_tuple(g_vaAV1Profiles, sizeof(VAEncPictureParameterBufferAV1)),
        }
    },
    {
        VAEncSliceParameterBufferType, {
            make_tuple(g_vaMPEG2Profiles, sizeof(VAEncSliceParameterBufferMPEG2)),
            make_tuple(g_vaH264Profiles, sizeof(VAEncSliceParameterBufferH264)),
            make_tuple(g_vaJPEGProfiles, sizeof(VAEncSliceParameterBufferJPEG)),
            make_tuple(g_vaHEVCProfiles, sizeof(VAEncSliceParameterBufferHEVC)),
            make_tuple(g_vaAV1Profiles, sizeof(VAEncPictureParameterBufferAV1)),
        }
    },
    {
        VAEncPackedHeaderParameterBufferType, {
            make_tuple(g_vaProfiles, sizeof(VAEncPackedHeaderParameterBuffer)),
            make_tuple(g_vaAV1Profiles, sizeof(VAEncPictureParameterBufferAV1)),
        }
    },
    {
        VAEncMiscParameterBufferType, {
            make_tuple(g_vaProfiles, sizeof(VAEncMiscParameterBuffer)),
            make_tuple(g_vaAV1Profiles, sizeof(VAEncPictureParameterBufferAV1)),
        }
    },
};

static const BufferSpecsMap vppBufferSpecs = {
    {
        VAProcPipelineParameterBufferType, {
            make_tuple(g_vaNoneProfiles, sizeof(VAProcPipelineParameterBuffer)),
        }
    },
    {
        VAProcFilterParameterBufferType, {
            make_tuple(g_vaNoneProfiles, sizeof(VAProcFilterParameterBuffer)),
            make_tuple(g_vaNoneProfiles,
                       sizeof(VAProcFilterParameterBufferDeinterlacing)),
            make_tuple(g_vaNoneProfiles,
                       sizeof(VAProcFilterParameterBufferColorBalance)),
            make_tuple(g_vaNoneProfiles,
                       sizeof(VAProcFilterParameterBufferTotalColorCorrection)),
        }
    },
};

TEST_P(VAAPICreateBuffer, CreateBufferWithOutData)
{
    // vaCreateBuffer uses a VAContextID as an input.  This VAContextID requires
    // a VAConfigID to be created.  VAConfigID requires VAProfile and
    // VAEntrypoint to be given.  As such, to test vaCreateBuffer these are
    // the minimum requirements.  There's no need to create surfaces or attach
    // them to a VAConfigID.

    if (!isSupported(profile, entrypoint)) {
        skipTest(profile, entrypoint);
        return;
    }

    // profile and entrypoint are supported
    createConfig(profile, entrypoint);

    // vaCreateContext input requires resolution, since this test
    // doesn't create surfaces, passing min resolution should provide
    // the desired result.
    Resolution minRes, maxRes;
    getMinMaxSurfaceResolution(minRes, maxRes);
    doCreateContext(minRes);

    createBuffer(bufferType, bufferSize);
    destroyBuffer();

    doDestroyContext();
    destroyConfig();
}

std::vector<CreateBufferParamInterface::ParamType> generateInput()
{
    std::vector<CreateBufferParamInterface::ParamType> inputs;

    const auto addBufferSpecs = [&](
    const BufferSpecsMap & bsm, const Entrypoints & entrypoints) {
        for (const auto& specs : bsm) {
            const auto& bufferType = std::get<0>(specs);
            for (const auto& spec : std::get<1>(specs)) {
                const auto& bufferSize = std::get<1>(spec);
                for (const auto& profile : std::get<0>(spec)) {
                    for (const auto& entrypoint : entrypoints) {
                        inputs.push_back(
                            make_tuple(
                                profile, entrypoint, bufferType, bufferSize));
                    }
                }
            }
        }
    };

    addBufferSpecs(decodeBufferSpecs, {VAEntrypointVLD,});
    addBufferSpecs(encodeBufferSpecs, {VAEntrypointEncSlice,
                                       VAEntrypointEncSliceLP, VAEntrypointEncPicture,
                                      });
    addBufferSpecs(vppBufferSpecs, {VAEntrypointVideoProc,});

    return inputs;
}

INSTANTIATE_TEST_SUITE_P(
    CreateBuffer, VAAPICreateBuffer,
    ::testing::ValuesIn(generateInput()));

} // namespace VAAPI
