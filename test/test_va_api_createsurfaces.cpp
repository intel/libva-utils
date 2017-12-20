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

// The following tests will operate on supported profile/entrypoint
// combinations that the driver does support, there is no real need
// to report SKIPPED tests as those cases are considered on the
// get_create_config use cases and properly handled.

class VAAPIQuerySurfaces
    : public VAAPIFixture
    , public ::testing::WithParamInterface<std::tuple<VAProfile, VAEntrypoint> >
{
public:
    VAAPIQuerySurfaces()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPIQuerySurfaces()
    {
        doTerminate();
    }
};

TEST_P(VAAPIQuerySurfaces, QuerySurfacesWithCofigAttribs)
{

    VAProfile currentProfile = ::testing::get<0>(GetParam());
    VAEntrypoint currentEntrypoint = ::testing::get<1>(GetParam());

    doGetMaxValues();

    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

	doQueryConfigEntrypoints(currentProfile);
	if (doFindEntrypointInList(currentEntrypoint)) {
	    // profile and entrypoint are supported

	    doFillConfigAttribList();

	    doGetConfigAttributes(currentProfile, currentEntrypoint);

	    doQuerySurfacesWithConfigAttribs(currentProfile, currentEntrypoint);
	}
    }
}

TEST_P(VAAPIQuerySurfaces, QuerySurfacesNoConfigAttribs)
{
    VAProfile currentProfile = ::testing::get<0>(GetParam());
    VAEntrypoint currentEntrypoint = ::testing::get<1>(GetParam());

    doGetMaxValues();

    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

        doQueryConfigEntrypoints(currentProfile);
        if (doFindEntrypointInList(currentEntrypoint)) {
            // profile and entrypoint are supported

            doQuerySurfacesNoConfigAttribs(currentProfile,
                                                   currentEntrypoint);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    QuerySurfaces, VAAPIQuerySurfaces,
    ::testing::Combine(::testing::ValuesIn(m_vaProfiles),
                       ::testing::ValuesIn(m_vaEntrypoints)));

class VAAPICreateSurfaces
    : public VAAPIFixture
    , public ::testing::WithParamInterface<
          std::tuple<VAProfile, VAEntrypoint, std::pair<uint32_t, uint32_t> > >
{
public:
    VAAPICreateSurfaces()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPICreateSurfaces()
    {
        doTerminate();
    }
};

TEST_P(VAAPICreateSurfaces, CreateSurfacesWithCofigAttribs)
{

    VAProfile currentProfile = ::testing::get<0>(GetParam());
    VAEntrypoint currentEntrypoint = ::testing::get<1>(GetParam());
    std::pair<uint32_t, uint32_t> currentResolution = ::testing::get<2>(GetParam());

    doGetMaxValues();

    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

        doQueryConfigEntrypoints(currentProfile);
        if (doFindEntrypointInList(currentEntrypoint)) {
            // profile and entrypoint are supported

            doFillConfigAttribList();

	    doGetConfigAttributes(currentProfile,
						currentEntrypoint);

	    doQuerySurfacesWithConfigAttribs(currentProfile,
                                                   currentEntrypoint);

            doCreateSurfaces(currentProfile, currentEntrypoint,
                                           currentResolution);
	}
    }
}

TEST_P(VAAPICreateSurfaces, CreateSurfacesNoConfigAttrib)
{
    VAProfile currentProfile = ::testing::get<0>(GetParam());
    VAEntrypoint currentEntrypoint = ::testing::get<1>(GetParam());
    std::pair<uint32_t, uint32_t> currentResolution = ::testing::get<2>(GetParam());

    doGetMaxValues();

    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

        doQueryConfigEntrypoints(currentProfile);
        if (doFindEntrypointInList(currentEntrypoint)) {
            // profile and entrypoint are supported

            doQuerySurfacesNoConfigAttribs(currentProfile,
                                                   currentEntrypoint);

            doCreateSurfaces(currentProfile, currentEntrypoint,
                                           currentResolution);
        }
    }
}

TEST_P(VAAPICreateSurfaces, CreateSurfacesNoAttrib)
{

    VAProfile currentProfile = ::testing::get<0>(GetParam());
    VAEntrypoint currentEntrypoint = ::testing::get<1>(GetParam());
    std::pair<uint32_t, uint32_t> currentResolution = ::testing::get<2>(GetParam());

    doGetMaxValues();

    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

        doQueryConfigEntrypoints(currentProfile);
        if (doFindEntrypointInList(currentEntrypoint)) {
            // profile and entrypoint are supported

            doCreateSurfaces(currentProfile, currentEntrypoint,
                                           currentResolution);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    CreateSurfaces, VAAPICreateSurfaces,
    ::testing::Combine(::testing::ValuesIn(m_vaProfiles),
                       ::testing::ValuesIn(m_vaEntrypoints),
                       ::testing::ValuesIn(m_vaResolutions)));
} // VAAPI
