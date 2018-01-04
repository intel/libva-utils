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

class VAAPIConfigAttribs
    : public VAAPIFixture
{
public:
    VAAPIConfigAttribs()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPIConfigAttribs()
    {
        doTerminate();
    }
};

TEST_F(VAAPIConfigAttribs, GetConfigAttribs)
{
    doGetMaxValues();

    doQueryConfigProfiles();
    const Profiles& profiles = getSupportedProfileList();
    ASSERT_FALSE(profiles.empty());

    for (const auto& profile : profiles) {
        doQueryConfigEntrypoints(profile);
        const Entrypoints& entrypoints = getSupportedEntrypointList();
        ASSERT_FALSE(entrypoints.empty());

        for (const auto& entrypoint : entrypoints) {
            doGetMaxNumConfigAttribs();
            doCreateConfigNoAttrib(profile, entrypoint);

            // Once ConfigID is obtained, then the attrib list is populated as
            // well.  This will confirm that the values returned by calling
            // vaGetConfigAttributes do match

            // make a copy since we want to set .value = 0
            ConfigAttributes attribs = getQueryConfigAttribList();
            for (auto& attrib : attribs) {
                attrib.value = 0;
            }

            doGetConfigAttributes(profile, entrypoint, attribs);
            doCheckAttribsMatch(attribs);
            doDestroyConfig();
        }
    }
}

} // namespace VAAPI
