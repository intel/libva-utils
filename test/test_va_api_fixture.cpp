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

#include <algorithm>
#include <functional>

#include <fcntl.h> // for O_RDWR
#include <limits>
#include <string.h>

#if defined(_WIN32)
#include <va/va_win32.h>
#include <compat_win32.h>
#else
#include <unistd.h> // for close()
#include <va/va_drm.h>
#include <xf86drm.h>
#endif

namespace VAAPI
{

int VAAPIFixtureSharedDisplay::s_drmHandle = -1;
VADisplay VAAPIFixtureSharedDisplay::s_vaDisplay = nullptr;
VAStatus VAAPIFixtureSharedDisplay::s_initStatus = VA_STATUS_SUCCESS;

VAAPIFixture::VAAPIFixture()
    : ::testing::Test::Test()
    , m_vaDisplay(NULL)
    , m_drmHandle(-1)
    , m_configID(VA_INVALID_ID)
    , m_contextID(VA_INVALID_ID)
    , m_bufferID(VA_INVALID_ID)
    , m_skip("")
{
    // If we do not copy the value and use the same pointer returned by getenv to restore the value
    // in ~VAAPIFixture with setenv, we see garbage memory being set on Windows platforms.
    char* libva_driver = getenv("LIBVA_DRIVER_NAME");
    if (libva_driver) m_restoreDriverName = libva_driver;
}

VAAPIFixture::~VAAPIFixture()
{
    m_vaDisplay = NULL;
    if (m_drmHandle >= 0)
        close(m_drmHandle);
    m_drmHandle = -1;

    // Ensure LIBVA_DRIVER_NAME environment is restored to its original
    // setting so successive tests use the expected driver.
    unsetenv("LIBVA_DRIVER_NAME");
    if (!m_restoreDriverName.empty())
        setenv("LIBVA_DRIVER_NAME", m_restoreDriverName.c_str(), 1);

    if (!m_skip.empty()) {
        EXPECT_FALSE(HasFailure())
                << "skip message is set, but something failed";
        if (!HasFailure()) {
            RecordProperty("skipped", true);
            std::cout << "[ SKIPPED ] " << m_skip << std::endl;
        }
    }
}

#if defined(_WIN32)
static VADisplay getWin32Display(LUID* adapter)
{
    return vaGetDisplayWin32(adapter);
}
#else
static VADisplay getDrmDisplay(int &fd)
{
    drmDevicePtr devices[32];
    int ret, max_devices = sizeof(devices) / sizeof(devices[0]);

    ret = drmGetDevices2(0, devices, max_devices);
    EXPECT_TRUE(ret >= 0);
    if (ret < 0)
        return NULL;
    max_devices = ret;

    for (int i = 0; i < max_devices; i++) {
        for (int j = DRM_NODE_MAX - 1; j >= 0; j--) {
            drmVersionPtr version;

            if (!(devices[i]->available_nodes & 1 << j))
                continue;

            fd = open(devices[i]->nodes[j], O_RDWR);
            if (fd < 0)
                continue;

            version = drmGetVersion(fd);
            if (!version) {
                close(fd);
                continue;
            }
            if (!strncmp(version->name, "vgem", 4)) {
                drmFreeVersion(version);
                close(fd);
                continue;
            }
            drmFreeVersion(version);

            VADisplay disp = vaGetDisplayDRM(fd);

            if (disp)
                return disp;

            close(fd);
        }
    }

    return NULL;
}
#endif
VADisplay VAAPIFixture::getDisplay()
{
#if defined(_WIN32)
    m_vaDisplay = getWin32Display(NULL);
#else
    m_vaDisplay = getDrmDisplay(m_drmHandle);
#endif
    return m_vaDisplay;
}

VADisplay VAAPIFixture::doInitialize()
{
    VADisplay vaDisplay;
    VAStatus status;
    int majorVersion, minorVersion;

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);
    if (!vaDisplay) {
        return NULL;
    }

    status = vaInitialize(vaDisplay, &majorVersion, &minorVersion);
    EXPECT_STATUS(status) << "failed to initialize vaapi";
    if (status != VA_STATUS_SUCCESS) {
        return NULL;
    }

    return vaDisplay;
}

void VAAPIFixture::queryConfigProfiles(Profiles& profiles) const
{
    const int maxProfiles = vaMaxNumProfiles(m_vaDisplay);
    ASSERT_GT(maxProfiles, 0);
    profiles.resize(maxProfiles);

    int numProfiles(0);
    EXPECT_STATUS(
        vaQueryConfigProfiles(m_vaDisplay, profiles.data(), &numProfiles));

    if (!HasFailure()) {
        ASSERT_LE(numProfiles, maxProfiles);
        ASSERT_GT(numProfiles, 0);
        profiles.resize(numProfiles);
    } else {
        profiles.clear();
    }
}

void VAAPIFixture::queryConfigEntrypoints(const VAProfile& profile,
        Entrypoints& entrypoints, const VAStatus& expectation) const
{
    const int maxEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    ASSERT_GT(maxEntrypoints, 0);
    entrypoints.resize(maxEntrypoints);

    int numEntrypoints(0);
    EXPECT_STATUS_EQ(
        expectation,
        vaQueryConfigEntrypoints(m_vaDisplay, profile, entrypoints.data(),
                                 &numEntrypoints));

    if ((VA_STATUS_SUCCESS == expectation) && !HasFailure()) {
        ASSERT_LE(numEntrypoints, maxEntrypoints);
        ASSERT_GT(numEntrypoints, 0);
        entrypoints.resize(numEntrypoints);
    } else {
        entrypoints.clear();
    }
}

VAStatus VAAPIFixture::getSupportStatus(const VAProfile& profile,
                                        const VAEntrypoint& entrypoint) const
{
    Profiles profiles;
    queryConfigProfiles(profiles);

    const auto pBegin(profiles.begin());
    const auto pEnd(profiles.end());
    if (std::find(pBegin, pEnd, profile) != pEnd) {
        Entrypoints entrypoints;
        queryConfigEntrypoints(profile, entrypoints);

        const auto eBegin(entrypoints.begin());
        const auto eEnd(entrypoints.end());
        return (std::find(eBegin, eEnd, entrypoint) != eEnd) ?
               VA_STATUS_SUCCESS : VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
    }

    return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

bool VAAPIFixture::isSupported(const VAProfile& profile,
                               const VAEntrypoint& entrypoint) const
{
    return VA_STATUS_SUCCESS == getSupportStatus(profile, entrypoint);
}

void VAAPIFixture::getConfigAttributes(const VAProfile& profile,
                                       const VAEntrypoint& entrypoint, ConfigAttributes& attribs,
                                       const VAStatus& expectation) const
{
    const bool defaults(attribs.empty());

    if (defaults) {
        // fill config attributes with default config attributes
        const auto op = [](const VAConfigAttribType & t) {
            return VAConfigAttrib{/*type:*/ t, /*value:*/ VA_ATTRIB_NOT_SUPPORTED};
        };
        std::transform(g_vaConfigAttribTypes.begin(),
                       g_vaConfigAttribTypes.end(), std::back_inserter(attribs), op);
    }

    ASSERT_FALSE(attribs.empty());

    EXPECT_STATUS_EQ(
        expectation,
        vaGetConfigAttributes(
            m_vaDisplay, profile, entrypoint, attribs.data(), attribs.size()));

    if (defaults) {
        // remove unsupported config attributes
        const auto begin(attribs.begin());
        const auto end(attribs.end());
        const auto predicate = [](const VAConfigAttrib & a) {
            return a.value == VA_ATTRIB_NOT_SUPPORTED;
        };
        attribs.erase(std::remove_if(begin, end, predicate), end);
    }
}

void VAAPIFixture::createConfig(const VAProfile& profile,
                                const VAEntrypoint& entrypoint, const ConfigAttributes& attribs,
                                const VAStatus& expectation)
{
    ASSERT_INVALID_ID(m_configID)
            << "test logic error: did you forget to call destroyConfig?";

    EXPECT_STATUS_EQ(
        expectation,
        vaCreateConfig(m_vaDisplay, profile, entrypoint,
                       (attribs.size() != 0 ?
                        const_cast<VAConfigAttrib*>(attribs.data()) : NULL),
                       attribs.size(), &m_configID))
            << "profile    = " << profile << std::endl
            << "entrypoint = " << entrypoint << std::endl
            << "numAttribs = " << attribs.size();

    if (expectation == VA_STATUS_SUCCESS) {
        EXPECT_ID(m_configID);
    } else {
        EXPECT_INVALID_ID(m_configID);
    }
}

void VAAPIFixture::queryConfigAttributes(
    const VAProfile& expectedProfile, const VAEntrypoint& expectedEntrypoint,
    ConfigAttributes& attributes, const VAStatus& expectedStatus) const
{
    VAProfile actualProfile;
    VAEntrypoint actualEntrypoint;
    int numAttributes(0);

    ASSERT_TRUE(attributes.empty())
            << "test logic error: attributes must be empty";

    const int maxAttributes = vaMaxNumConfigAttributes(m_vaDisplay);

    ASSERT_GT(maxAttributes, 0);

    attributes.resize(maxAttributes);

    EXPECT_STATUS_EQ(
        expectedStatus,
        vaQueryConfigAttributes(m_vaDisplay, m_configID, &actualProfile,
                                &actualEntrypoint, attributes.data(), &numAttributes));

    if (expectedStatus == VA_STATUS_SUCCESS) {
        EXPECT_EQ(expectedProfile, actualProfile);
        EXPECT_EQ(expectedEntrypoint, actualEntrypoint);
        ASSERT_LE(numAttributes, maxAttributes);
        ASSERT_GT(numAttributes, 0);

        attributes.resize(numAttributes);

        // reported config attributes should be supported
        for (const auto& attribute : attributes) {
            EXPECT_NE(VA_ATTRIB_NOT_SUPPORTED, attribute.value);
        }
    } else {
        attributes.clear();
    }
}

void VAAPIFixture::destroyConfig(const VAStatus& expectation)
{
    EXPECT_STATUS_EQ(expectation, vaDestroyConfig(m_vaDisplay, m_configID));
    m_configID = VA_INVALID_ID;
}

void VAAPIFixture::querySurfaceAttributes(SurfaceAttributes& attribs) const
{
    ASSERT_TRUE(attribs.empty())
            << "test logic error: surface attributes must be empty";

    unsigned numAttribs(0);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID, NULL,
                                           &numAttribs));

    ASSERT_GT(numAttribs, 0u);

    attribs.resize(numAttribs);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID,
                                           attribs.data(), &numAttribs));

    ASSERT_GT(numAttribs, 0u);
    EXPECT_GE(attribs.size(), numAttribs);

    attribs.resize(numAttribs);

    const uint32_t flags = 0x0 | VA_SURFACE_ATTRIB_GETTABLE
                           | VA_SURFACE_ATTRIB_SETTABLE;

    for (const auto& attrib : attribs) {
        EXPECT_NE(attrib.flags & flags,
                  (uint32_t)VA_SURFACE_ATTRIB_NOT_SUPPORTED);
        EXPECT_GE(attrib.value.type, VAGenericValueTypeInteger);
        EXPECT_LE(attrib.value.type, VAGenericValueTypeFunc);
    }
}

void VAAPIFixture::getMinMaxSurfaceResolution(
    Resolution& minRes, Resolution& maxRes) const
{
    const Resolution::DataType maxVal =
        std::numeric_limits<Resolution::DataType>::max();

    // set default resolutions
    minRes.width = 1;
    minRes.height = 1;
    maxRes.width = maxVal;
    maxRes.height = maxVal;

    SurfaceAttributes attribs;
    querySurfaceAttributes(attribs);

    SurfaceAttributes::const_iterator match;
    const SurfaceAttributes::const_iterator begin(attribs.begin());
    const SurfaceAttributes::const_iterator end(attribs.end());

    // minimum surface width
    match = std::find_if(begin, end, [](const VASurfaceAttrib & a) {
        return a.type == VASurfaceAttribMinWidth;
    });
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        minRes.width = match->value.value.i;
    }

    // minimum surface height
    match = std::find_if(begin, end, [](const VASurfaceAttrib & a) {
        return a.type == VASurfaceAttribMinHeight;
    });
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        minRes.height = match->value.value.i;
    }

    // maximum surface width
    match = std::find_if(begin, end, [](const VASurfaceAttrib & a) {
        return a.type == VASurfaceAttribMaxWidth;
    });
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        maxRes.width = match->value.value.i;
    }

    // maximum surface height
    match = std::find_if(begin, end, [](const VASurfaceAttrib & a) {
        return a.type == VASurfaceAttribMaxHeight;
    });
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        maxRes.height = match->value.value.i;
    }

    EXPECT_LE(minRes, maxRes);
}

void VAAPIFixture::createSurfaces(Surfaces& surfaces, const unsigned format,
                                  const Resolution& resolution, const SurfaceAttributes& attribs,
                                  const VAStatus& expectation) const
{
    ASSERT_GT(surfaces.size(), 0u)
            << "test logic error: surfaces must not be emtpy";
    for (const auto& surface : surfaces) {
        ASSERT_INVALID_ID(surface)
                << "test logic error: surfaces must all be VA_INVALID_SURFACE";
    }

    ASSERT_STATUS_EQ(
        expectation,
        vaCreateSurfaces(m_vaDisplay, format, resolution.width,
                         resolution.height, surfaces.data(), surfaces.size(),
                         (attribs.size() != 0 ?
                          const_cast<VASurfaceAttrib*>(attribs.data()) : NULL),
                         attribs.size()));

    if (expectation == VA_STATUS_SUCCESS) {
        for (const auto& surface : surfaces) {
            ASSERT_ID(surface);
        }
    }
}

void VAAPIFixture::destroySurfaces(Surfaces& surfaces) const
{
    if (surfaces.size() != 0) {
        EXPECT_STATUS(vaDestroySurfaces(m_vaDisplay, surfaces.data(),
                                        surfaces.size()));
    }
}

void VAAPIFixture::createBuffer(const VABufferType& bufferType,
                                const size_t bufferSize, const VAStatus& expectation)
{
    ASSERT_INVALID_ID(m_bufferID)
            << "test logic error: did you forget to call destroyBuffer?";

    EXPECT_STATUS_EQ(
        expectation,
        vaCreateBuffer(m_vaDisplay, m_contextID, bufferType, bufferSize,
                       1, NULL, &m_bufferID));
}

void VAAPIFixture::destroyBuffer(const VAStatus& expectation)
{
    EXPECT_STATUS_EQ(expectation, vaDestroyBuffer(m_vaDisplay, m_bufferID));
    m_bufferID = VA_INVALID_ID;
}

void VAAPIFixture::doCreateContext(const Resolution& resolution,
                                   const VAStatus& expectation)
{
    m_contextID = 0;
    ASSERT_STATUS_EQ(expectation,
                     vaCreateContext(m_vaDisplay, m_configID, resolution.width,
                                     resolution.height, VA_PROGRESSIVE,
                                     NULL, 0, &m_contextID));
}

void VAAPIFixture::doDestroyContext(const VAStatus& expectation)
{
    ASSERT_STATUS_EQ(expectation, vaDestroyContext(m_vaDisplay, m_contextID));
}

void VAAPIFixture::doTerminate()
{
    EXPECT_STATUS(vaTerminate(m_vaDisplay));
}

void VAAPIFixture::skipTest(const std::string& message)
{
    ASSERT_FALSE(message.empty())
            << "test logic error: skip message cannot be empty";
    ASSERT_TRUE(m_skip.empty())
            << "test logic error: test already marked as skipped";

    m_skip = message;
}

void VAAPIFixture::skipTest(const VAProfile& profile,
                            const VAEntrypoint& entrypoint)
{
    std::ostringstream oss;
    oss << profile << " / " << entrypoint << " not supported on this hardware";
    skipTest(oss.str());
}

TEST_F(VAAPIFixture, getDisplay)
{
    VADisplay vaDisplay;

    vaDisplay = getDisplay();
    ASSERT_TRUE(vaDisplay);
    EXPECT_STATUS(vaTerminate(vaDisplay));
}

VAAPIFixtureSharedDisplay::VAAPIFixtureSharedDisplay() : VAAPIFixture() { }

void VAAPIFixtureSharedDisplay::SetUpTestSuite()
{
    if (s_drmHandle < 0) {
#if defined(_WIN32)
        s_vaDisplay = getWin32Display(NULL);
#else
        s_vaDisplay = getDrmDisplay(s_drmHandle);
#endif
        int majorVersion, minorVersion;
        s_initStatus = vaInitialize(s_vaDisplay, &majorVersion, &minorVersion);
    }
}

void VAAPIFixtureSharedDisplay::TearDownTestSuite()
{
    if (s_vaDisplay) {
        if (s_initStatus == VA_STATUS_SUCCESS) {
            vaTerminate(s_vaDisplay);
            s_vaDisplay = nullptr;
        } else {
            s_initStatus = VA_STATUS_SUCCESS;
        }
    }
    if (s_drmHandle >= 0) {
        close(s_drmHandle);
        s_drmHandle = -1;
    }
}

void VAAPIFixtureSharedDisplay::SetUp()
{
    EXPECT_STATUS(s_initStatus) << "failed to initialize vaapi";

    m_vaDisplay = s_vaDisplay;
}

} // namespace VAAPI
