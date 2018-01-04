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

namespace VAAPI {

class VAAPICreateContextToFail
    : public VAAPIFixture
{
public:
    VAAPICreateContextToFail()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPICreateContextToFail()
    {
        doTerminate();
    }
};

TEST_F(VAAPICreateContextToFail, CreateContextWithNoConfig)
{
    // There's no need to test all inputs for this to be a good test
    // as long as there's no config the expected error should be
    // returned
    doCreateContext({1920, 1080}, VA_STATUS_ERROR_INVALID_CONFIG);
}

class VAAPICreateContext
    : public VAAPIFixture
    , public ::testing::WithParamInterface<
          std::tuple<VAProfile, VAEntrypoint, Resolution> >
{
public:
    VAAPICreateContext()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPICreateContext()
    {
        doTerminate();
    }
};

TEST_P(VAAPICreateContext, CreateContext)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());
    const Resolution& res           = ::testing::get<2>(GetParam());

    // vaCreateContext requires a valid VAConfigID, vaCreateConfig requires a
    // supported profile and entrypoint

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doCreateConfig(profile, entrypoint);

            Resolution minRes;
            doGetMinSurfaceResolution(profile, entrypoint, minRes);

            Resolution maxRes;
            doGetMaxSurfaceResolution(profile, entrypoint, maxRes);

            if (not res.isWithin(minRes, maxRes)) {
                doCreateContext(res, VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED);
                doDestroyContext(VA_STATUS_ERROR_INVALID_CONTEXT);
            } else {
                doCreateContext(res);
                doDestroyContext();
            }

            doDestroyConfig();
        } else {
            // entrypoint not supported
            doLogSkipTest(profile, entrypoint);
        }
    } else {
        // profile not supported
        doLogSkipTest(profile, entrypoint);
    }
}

INSTANTIATE_TEST_CASE_P(
    CreateContext, VAAPICreateContext,
    ::testing::Combine(::testing::ValuesIn(m_vaProfiles),
        ::testing::ValuesIn(m_vaEntrypoints),
        ::testing::ValuesIn(m_vaResolutions)));

} // namespace VAAPI
