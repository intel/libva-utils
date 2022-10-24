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

class VAAPIQueryConfig
    : public VAAPIFixtureSharedDisplay
    , public ::testing::WithParamInterface<VAProfile>
{
};

TEST_P(VAAPIQueryConfig, CheckEntrypointsForProfile)
{
    int numEntrypoints = 0, numProfiles = 0;
    const VAProfile& profile = GetParam();

    const int maxProfiles = vaMaxNumProfiles(m_vaDisplay);
    EXPECT_TRUE(maxProfiles > 0)
            << maxProfiles << " profiles are reported";

    Profiles profiles(maxProfiles, VAProfileNone);

    EXPECT_STATUS(
        vaQueryConfigProfiles(m_vaDisplay, profiles.data(), &numProfiles));

    EXPECT_TRUE(numProfiles > 0)
            << numProfiles << " profiles are supported by driver";

    profiles.resize(numProfiles);

    const int maxEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    EXPECT_TRUE(maxEntrypoints > 0)
            << maxEntrypoints << " entrypoints are reported";

    Entrypoints entrypoints(maxEntrypoints);

    const Profiles::const_iterator begin = profiles.begin();
    const Profiles::const_iterator end = profiles.end();

    if (std::find(begin, end, profile) != end) {
        EXPECT_STATUS(vaQueryConfigEntrypoints(m_vaDisplay,
                                               profile, entrypoints.data(), &numEntrypoints))
                << " profile used is " << profile;

        EXPECT_TRUE(numEntrypoints > 0)
                << profile << " is supported but no entrypoints are reported";
    } else {
        EXPECT_STATUS_EQ(
            VA_STATUS_ERROR_UNSUPPORTED_PROFILE, vaQueryConfigEntrypoints(
                m_vaDisplay, profile, entrypoints.data(), &numEntrypoints))
                << " profile used is " << profile;

        EXPECT_FALSE(numEntrypoints > 0)
                << profile << " profile is not supported but \
            valid entrypoints are reported ";
    }
}

INSTANTIATE_TEST_SUITE_P(
    QueryConfig, VAAPIQueryConfig, ::testing::ValuesIn(g_vaProfiles));

} // namespace VAAPI
