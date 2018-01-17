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

#include <functional>
#include <sstream>

namespace VAAPI {

// The following tests will operate on supported profile/entrypoint
// combinations that the driver does support.

class VAAPISurfaceFixture
    : public VAAPIFixture
{
public:
    VAAPISurfaceFixture(const VAProfile& p, const VAEntrypoint& e)
        : VAAPIFixture()
        , profile(p)
        , entrypoint(e)
    { }

protected:
    const VAProfile& profile;
    const VAEntrypoint& entrypoint;

    virtual void SetUp()
    {
        VAAPIFixture::SetUp();

        doInitialize();
        doGetMaxValues();
    }

    virtual void TearDown()
    {
        VAAPIFixture::TearDown();

        doTerminate();
    }

    void testWithSupportedConfigAttributes(
        const std::function<void (const ConfigAttributes&)>& test)
    {
        ConfigAttributes supported;
        getConfigAttributes(profile, entrypoint, supported);

        // create config with each individual supported attribute
        for (const auto& attrib : supported) {
            const auto match = g_vaConfigAttribBitMasks.find(attrib.type);
            if (match != g_vaConfigAttribBitMasks.end()) {
                // it's a bitfield attribute
                const BitMasks& masks = match->second;
                for (const auto mask : masks) { // for all bitmasks
                    if ((attrib.value & mask) == mask) { // supported value
                        const ConfigAttributes attribs(
                            1, {type : attrib.type, value : mask });
                        createConfig(profile, entrypoint, attribs);
                        test(attribs);
                        destroyConfig();
                    }
                }
            } else {
                // it's a standard attribute
                const ConfigAttributes attribs(1, attrib);
                createConfig(profile, entrypoint, attribs);
                test(attribs);
                destroyConfig();
            }
        }
    }

    void testWithSupportedSurfaceAttributes(
        const std::function<void (const SurfaceAttributes&)>& test)
    {
        SurfaceAttributes supported;
        querySurfaceAttributes(supported);

        const uint32_t drmMemMask = VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM
            | VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
            | VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;

        // create surfaces for each supported attribute
        for (const auto& attrib : supported) {
            const auto match = g_vaSurfaceAttribBitMasks.find(attrib.type);
            if (match != g_vaSurfaceAttribBitMasks.end()) {
                // it's a bitfield attribute
                ASSERT_EQ(attrib.value.type, VAGenericValueTypeInteger);
                uint32_t bitfield(0);
                const BitMasks& masks = match->second;
                for (const auto mask : masks) { // for all bitmasks
                    if ((attrib.value.value.i & mask) == mask) {
                        // supported value
                        bitfield |= mask;

                        if ((attrib.type == VASurfaceAttribMemoryType)
                            and (drmMemMask & mask) == mask) {
                            // skip drm memory types for now as it requires much
                            // more setup
                            continue;
                        } else {
                            VASurfaceAttrib maskAttrib = attrib;
                            maskAttrib.value.value.i = mask;
                            const SurfaceAttributes attribs = {maskAttrib,};
                            test(attribs);
                        }
                    }
                }
                // ensure we processed all supported values
                EXPECT_EQ(bitfield, (uint32_t)attrib.value.value.i);
            } else {
                // it's a standard attribute
                const SurfaceAttributes attribs = {attrib,};
                test(attribs);
            }
        }
    }
};

typedef ::testing::WithParamInterface<
    std::tuple<VAProfile, VAEntrypoint> > QuerySurfacesParamInterface;

class VAAPIQuerySurfaces
    : public QuerySurfacesParamInterface
    , public VAAPISurfaceFixture
{
public:
    VAAPIQuerySurfaces()
        : QuerySurfacesParamInterface()
        , VAAPISurfaceFixture(
            ::testing::get<0>(GetParam()),
            ::testing::get<1>(GetParam()))
    { }
};

TEST_P(VAAPIQuerySurfaces, QuerySurfacesWithConfigAttribs)
{
    const auto test = [&](const ConfigAttributes& ca) {
        SurfaceAttributes attribs;
        querySurfaceAttributes(attribs);
    };

    doQueryConfigProfiles();
    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            testWithSupportedConfigAttributes(test);
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
    doQueryConfigProfiles();
    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            createConfig(profile, entrypoint);
            SurfaceAttributes attribs;
            querySurfaceAttributes(attribs);
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

typedef typename ::testing::WithParamInterface<std::tuple<
    VAProfile, VAEntrypoint, Resolution>> CreateSurfacesParamInterface;

class VAAPICreateSurfaces
    : public CreateSurfacesParamInterface
    , public VAAPISurfaceFixture
{
public:
    VAAPICreateSurfaces()
        : CreateSurfacesParamInterface()
        , VAAPISurfaceFixture(
            ::testing::get<0>(GetParam()),
            ::testing::get<1>(GetParam()))
        , resolution(::testing::get<2>(GetParam()))
    { }

protected:
    const Resolution& resolution;
};

TEST_P(VAAPICreateSurfaces, CreateSurfacesWithConfigAttribs)
{
    // VA_RT_FORMAT_YUV420 is considered the universal supported format by
    // drivers
    unsigned format = VA_RT_FORMAT_YUV420;

    const auto testSurfaces = [&](const SurfaceAttributes& attribs) {
        Surfaces surfaces(10, VA_INVALID_SURFACE);
        createSurfaces(surfaces, format, resolution, attribs);
        destroySurfaces(surfaces);
    };

    const auto test = [&](const ConfigAttributes& attribs) {
        const auto match = std::find_if(attribs.begin(), attribs.end(),
            [](const VAConfigAttrib& a)
                { return a.type == VAConfigAttribRTFormat; });
        if (match != attribs.end()) {
            format = match->value;
        }
        testWithSupportedSurfaceAttributes(testSurfaces);

        // reset format to default
        format = VA_RT_FORMAT_YUV420;
    };

    doQueryConfigProfiles();
    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            testWithSupportedConfigAttributes(test);
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
    const auto test = [&](const SurfaceAttributes& attribs) {
        Surfaces surfaces(10, VA_INVALID_SURFACE);
        createSurfaces(surfaces, VA_RT_FORMAT_YUV420, resolution, attribs);
        destroySurfaces(surfaces);
    };

    doQueryConfigProfiles();
    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            createConfig(profile, entrypoint);
            testWithSupportedSurfaceAttributes(test);
            destroyConfig();
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
    doQueryConfigProfiles();
    if (doFindProfileInList(profile)) {
        doQueryConfigEntrypoints(profile);
        if (doFindEntrypointInList(entrypoint)) {
            // profile and entrypoint are supported
            createConfig(profile, entrypoint);

            Surfaces surfaces(10, VA_INVALID_SURFACE);
            Resolution minRes, maxRes;
            getMinMaxSurfaceResolution(minRes, maxRes);
            if (resolution.isWithin(minRes, maxRes)) {
                createSurfaces(surfaces, VA_RT_FORMAT_YUV420, resolution);
                destroySurfaces(surfaces);
            } else {
                // TODO: test createsurfaces with invalid resolution
            }

            destroyConfig();
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
