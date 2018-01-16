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

TEST_P(VAAPIQuerySurfaces, QuerySurfacesWithConfigAttribs)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doFillConfigAttribList();
            doGetConfigAttributes(profile, entrypoint);
            doQuerySurfacesWithConfigAttribs(profile, entrypoint);
        } else {
            // entrypoint not supported
            doLogSkipTest(profile, entrypoint);
        }
    } else {
        // profile not supported
        doLogSkipTest(profile, entrypoint);
    }
}

TEST_P(VAAPIQuerySurfaces, QuerySurfacesNoConfigAttribs)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doQuerySurfacesNoConfigAttribs(profile, entrypoint);
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
    QuerySurfaces, VAAPIQuerySurfaces,
    ::testing::Combine(::testing::ValuesIn(g_vaProfiles),
        ::testing::ValuesIn(g_vaEntrypoints)));

class VAAPICreateSurfaces
    : public VAAPIFixture
    , public ::testing::WithParamInterface<
          std::tuple<VAProfile, VAEntrypoint, Resolution> >
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

TEST_P(VAAPICreateSurfaces, CreateSurfacesWithConfigAttribs)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());
    const Resolution& resolution    = ::testing::get<2>(GetParam());

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doFillConfigAttribList();
            doGetConfigAttributes(profile, entrypoint);
            doQuerySurfacesWithConfigAttribs(profile, entrypoint);
            doCreateSurfaces(profile, entrypoint, resolution);
        } else {
            // entrypoint not supported
            doLogSkipTest(profile, entrypoint);
        }
    } else {
        // profile not supported
        doLogSkipTest(profile, entrypoint);
    }
}

TEST_P(VAAPICreateSurfaces, CreateSurfacesNoConfigAttrib)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());
    const Resolution& resolution    = ::testing::get<2>(GetParam());

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doQuerySurfacesNoConfigAttribs(profile, entrypoint);
            doCreateSurfaces(profile, entrypoint, resolution);
        } else {
            // entrypoint not supported
            doLogSkipTest(profile, entrypoint);
        }
    } else {
        // profile not supported
        doLogSkipTest(profile, entrypoint);
    }
}

TEST_P(VAAPICreateSurfaces, CreateSurfacesNoAttrib)
{
    const VAProfile& profile        = ::testing::get<0>(GetParam());
    const VAEntrypoint& entrypoint  = ::testing::get<1>(GetParam());
    const Resolution& resolution    = ::testing::get<2>(GetParam());

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            doCreateSurfaces(profile, entrypoint, resolution);
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
    CreateSurfaces, VAAPICreateSurfaces,
    ::testing::Combine(::testing::ValuesIn(g_vaProfiles),
        ::testing::ValuesIn(g_vaEntrypoints),
        ::testing::ValuesIn(g_vaResolutions)));

} // namespace VAAPI
