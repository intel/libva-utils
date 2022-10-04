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

#include <sstream>

namespace VAAPI
{

typedef VAAPIFixture VAAPICreateContextToFail;

TEST_F(VAAPICreateContextToFail, CreateContextWithNoConfig)
{
    // There's no need to test all inputs for this to be a good test
    // as long as there's no config the expected error should be
    // returned
    doInitialize();
    ASSERT_FALSE(HasFailure());
    doCreateContext({1920, 1080}, VA_STATUS_ERROR_INVALID_CONFIG);
    doTerminate();
}

class VAAPICreateContext
    : public VAAPIFixture
    , public ::testing::WithParamInterface <
      std::tuple<VAProfile, VAEntrypoint, Resolution> >
{
public:
    VAAPICreateContext()
        : profile(::testing::get<0>(GetParam()))
        , entrypoint(::testing::get<1>(GetParam()))
        , resolution(::testing::get<2>(GetParam()))
    { }

protected:
    const VAProfile& profile;
    const VAEntrypoint& entrypoint;
    const Resolution& resolution;

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

TEST_P(VAAPICreateContext, CreateContext)
{
    // vaCreateContext requires a valid VAConfigID, vaCreateConfig requires a
    // supported profile and entrypoint

    if (!isSupported(profile, entrypoint)) {
        skipTest(profile, entrypoint);
        return;
    }

    // profile and entrypoint are supported
    createConfig(profile, entrypoint);

    Resolution minRes, maxRes;
    getMinMaxSurfaceResolution(minRes, maxRes);

    std::ostringstream oss;
    oss << "resolution: min=" << minRes << "; max=" << maxRes
        << "; current=" << resolution;
    SCOPED_TRACE(oss.str());

    if (!resolution.isWithin(minRes, maxRes)) {
        doCreateContext(resolution, VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED);
        doDestroyContext(VA_STATUS_ERROR_INVALID_CONTEXT);
    } else {
        doCreateContext(resolution);
        doDestroyContext();
    }

    destroyConfig();
}

INSTANTIATE_TEST_SUITE_P(
    CreateContext, VAAPICreateContext,
    ::testing::Combine(::testing::ValuesIn(g_vaProfiles),
                       ::testing::ValuesIn(g_vaEntrypoints),
                       ::testing::ValuesIn(g_vaResolutions)));

} // namespace VAAPI
