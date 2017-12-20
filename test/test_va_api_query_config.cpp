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

class VAAPIQueryConfig
    : public VAAPIFixture
    , public ::testing::WithParamInterface<VAProfile>
{
public:
    VAAPIQueryConfig()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPIQueryConfig()
    {
        doTerminate();
    }
};

TEST_P(VAAPIQueryConfig, CheckEntrypointsForProfile)
{
    int maxEntrypoints = 0, maxProfiles = 0;
    int numEntrypoints = 0, numProfiles = 0;
    VAProfile currentProfile = GetParam();

    maxProfiles = vaMaxNumProfiles(m_vaDisplay);
    EXPECT_TRUE(maxProfiles > 0) << maxProfiles
                                 << " profiles are reported, check setup";

    std::vector<VAProfile> profileList(maxProfiles);

    ASSERT_STATUS(
        vaQueryConfigProfiles(m_vaDisplay, &profileList[0], &numProfiles));

    EXPECT_TRUE(numProfiles > 0) << numProfiles << " are supported by driver";

    maxEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    EXPECT_TRUE(maxEntrypoints > 0) << maxEntrypoints
                                    << " entrypoints are reported, check setup";

    std::vector<VAEntrypoint> entrypointList(maxEntrypoints);

    if (std::find(profileList.begin(), profileList.end(), currentProfile)
        != profileList.end()) {

        ASSERT_STATUS(vaQueryConfigEntrypoints(
            m_vaDisplay, currentProfile, &entrypointList[0], &numEntrypoints))
            << " profile used is " << currentProfile;

        EXPECT_TRUE(numEntrypoints > 0)
            << currentProfile << " is supported but no entrypoints are reported";
    }
    else {
        ASSERT_STATUS_EQ(VA_STATUS_ERROR_UNSUPPORTED_PROFILE,
                         vaQueryConfigEntrypoints(m_vaDisplay, currentProfile,
                                           &entrypointList[0], &numEntrypoints))
            << " profile used is " << currentProfile;

        EXPECT_FALSE(numEntrypoints > 0) << currentProfile
                                         << " profile is not "
                                            "supported but reported "
                                            "valid entrypoints";
    }
}

INSTANTIATE_TEST_CASE_P(QueryConfig,
                        VAAPIQueryConfig,
                        ::testing::ValuesIn(m_vaProfiles));

} // VAAPI
