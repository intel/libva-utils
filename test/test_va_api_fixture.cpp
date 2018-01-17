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
#include <unistd.h> // for close()
#include <va/va_drm.h>

namespace VAAPI {

VAAPIFixture::VAAPIFixture()
    : ::testing::Test::Test()
    , m_vaDisplay(NULL)
    , m_restoreDriverName(getenv("LIBVA_DRIVER_NAME"))
    , m_drmHandle(-1)
    , drmDevicePaths({ "/dev/dri/renderD128", "/dev/dri/card0" })
    , m_maxEntrypoints(0)
    , m_maxProfiles(0)
    , m_numProfiles(0)
    , m_maxConfigAttributes(0)
    , m_configID(VA_INVALID_ID)
    , m_contextID(VA_INVALID_ID)
    , m_bufferID(VA_INVALID_ID)
{
    m_profileList.clear();
    m_entrypointList.clear();
    m_configAttribList.clear();
    m_configAttribToCreateConfig.clear();
    m_querySurfaceAttribList.clear();
    m_surfaceID.clear();
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
    if (m_restoreDriverName)
        setenv("LIBVA_DRIVER_NAME", m_restoreDriverName, 1);
}

VADisplay VAAPIFixture::getDisplay()
{
    uint32_t i;

    for (i = 0; i < sizeof(drmDevicePaths) / sizeof(*drmDevicePaths); i++) {
        m_drmHandle = open(drmDevicePaths[i].c_str(), O_RDWR);
        if (m_drmHandle < 0)
            continue;
        m_vaDisplay = vaGetDisplayDRM(m_drmHandle);

        if (m_vaDisplay)
            return m_vaDisplay;
    }

    return NULL;
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
    EXPECT_STATUS(status);
    if (status != VA_STATUS_SUCCESS) {
        return NULL;
    }

    return vaDisplay;
}

void VAAPIFixture::doGetMaxProfiles()
{
    m_maxProfiles = vaMaxNumProfiles(m_vaDisplay);
    EXPECT_TRUE(m_maxProfiles > 0) << m_maxProfiles
                                   << " profiles are reported, check setup";
}

void VAAPIFixture::doGetMaxEntrypoints()
{
    m_maxEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    EXPECT_TRUE(m_maxEntrypoints > 0)
        << m_maxEntrypoints << " entrypoints are reported, check setup";
}

void VAAPIFixture::doGetMaxNumConfigAttribs()
{
    m_maxConfigAttributes = vaMaxNumConfigAttributes(m_vaDisplay);

    EXPECT_TRUE(m_maxConfigAttributes > 0);
}

void VAAPIFixture::doGetMaxValues()
{
    doGetMaxProfiles();
    doGetMaxEntrypoints();
    doGetMaxNumConfigAttribs();
}

void VAAPIFixture::doQueryConfigProfiles()
{
    m_profileList.resize(m_maxProfiles);

    ASSERT_STATUS(
        vaQueryConfigProfiles(m_vaDisplay, &m_profileList[0], &m_numProfiles));

    // at least one profile should be supported for tests to be executed
    ASSERT_TRUE(m_numProfiles > 0);

    m_profileList.resize(m_numProfiles);
}

const Profiles& VAAPIFixture::getSupportedProfileList() const
{
    return m_profileList;
}

const Entrypoints& VAAPIFixture::getSupportedEntrypointList() const
{
    return m_entrypointList;
}

bool VAAPIFixture::doFindProfileInList(const VAProfile& profile) const
{
    return std::find(m_profileList.begin(), m_profileList.end(), profile)
           != m_profileList.end();
}

void VAAPIFixture::doQueryConfigEntrypoints(const VAProfile& profile)
{
    int numEntrypoints = 0;

    m_entrypointList.resize(m_maxEntrypoints);
    ASSERT_STATUS(vaQueryConfigEntrypoints(
        m_vaDisplay, profile, &m_entrypointList[0], &numEntrypoints));

    EXPECT_TRUE(numEntrypoints > 0);

    m_entrypointList.resize(numEntrypoints);
}

bool VAAPIFixture::doFindEntrypointInList(const VAEntrypoint& entrypoint) const
{
    return std::find(m_entrypointList.begin(), m_entrypointList.end(),
                     entrypoint)
           != m_entrypointList.end();
}

void VAAPIFixture::getConfigAttributes(const VAProfile& profile,
    const VAEntrypoint& entrypoint, ConfigAttributes& attribs,
    const VAStatus& expectation) const
{
    const bool defaults(attribs.empty());

    if (defaults) {
        // fill config attributes with default config attributes
        const auto op = [](const VAConfigAttribType& t) {
            return VAConfigAttrib{type: t, value: VA_ATTRIB_NOT_SUPPORTED};
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
        const auto predicate = [](const VAConfigAttrib& a) {
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

    attributes.resize(m_maxConfigAttributes);

    EXPECT_STATUS_EQ(
        expectedStatus,
        vaQueryConfigAttributes(m_vaDisplay, m_configID, &actualProfile,
            &actualEntrypoint, attributes.data(), &numAttributes));

    if (expectedStatus == VA_STATUS_SUCCESS) {
        EXPECT_EQ(expectedProfile, actualProfile);
        EXPECT_EQ(expectedEntrypoint, actualEntrypoint);
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

void VAAPIFixture::doFillConfigAttribList()
{
    m_configAttribList.clear();
    // fill it with all the VAConfigAttribs known
    for (auto& it : g_vaConfigAttribTypes) {
        VAConfigAttrib configAttrib;

        configAttrib.type = it;

        m_configAttribList.push_back(configAttrib);
    }

    EXPECT_EQ(m_configAttribList.size(), g_vaConfigAttribTypes.size());
}

void VAAPIFixture::doGetConfigAttributes(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{
    int numAttributes = m_configAttribList.size();
    ASSERT_STATUS(vaGetConfigAttributes(m_vaDisplay, profile, entrypoint,
                                        &m_configAttribList[0], numAttributes));
}

void VAAPIFixture::doGetConfigAttributes(const VAProfile& profile,
    const VAEntrypoint& entrypoint, ConfigAttributes& configAttrib)
{
    int numAttributes = configAttrib.size();
    ASSERT_STATUS(vaGetConfigAttributes(m_vaDisplay, profile, entrypoint,
                                        &configAttrib[0], numAttributes));
}

const ConfigAttributes& VAAPIFixture::getConfigAttribList() const
{
    return m_configAttribList;
}

const ConfigAttributes& VAAPIFixture::getQueryConfigAttribList() const
{
    return m_queryConfigAttribList;
}

void VAAPIFixture::doCheckAttribsMatch(
    const ConfigAttributes& configAttrib) const
{
    auto itOne = m_queryConfigAttribList.begin();
    auto itTwo = configAttrib.begin();
    auto diff = 0;

    EXPECT_EQ(configAttrib.size(), m_queryConfigAttribList.size());

    while (itOne != m_queryConfigAttribList.end()
           && itTwo != configAttrib.end()) {
        if (itOne->value != itTwo->value || itOne->type != itTwo->type) {
            diff++;
        }
        itOne++;
        itTwo++;
    }

    EXPECT_TRUE(diff == 0);
}

void VAAPIFixture::doCreateConfigWithAttrib(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{
    m_configAttribToCreateConfig.clear();
    for (auto& it : m_configAttribList) {
        if (it.value != VA_ATTRIB_NOT_SUPPORTED)
            m_configAttribToCreateConfig.push_back(it);
    }

    ASSERT_STATUS(vaCreateConfig(
        m_vaDisplay, profile, entrypoint, &m_configAttribToCreateConfig[0],
        m_configAttribToCreateConfig.size(), &m_configID));

    EXPECT_ID(m_configID);

    doQueryConfigAttributes(profile, entrypoint);
}

void VAAPIFixture::doDestroyConfig()
{
    ASSERT_STATUS(vaDestroyConfig(m_vaDisplay, m_configID));
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
    match = std::find_if(begin, end, [](const VASurfaceAttrib& a)
        {return a.type == VASurfaceAttribMinWidth;});
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        minRes.width = match->value.value.i;
    }

    // minimum surface height
    match = std::find_if(begin, end, [](const VASurfaceAttrib& a)
        {return a.type == VASurfaceAttribMinHeight;});
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        minRes.height = match->value.value.i;
    }

    // maximum surface width
    match = std::find_if(begin, end, [](const VASurfaceAttrib& a)
        {return a.type == VASurfaceAttribMaxWidth;});
    if (match != end) {
        EXPECT_EQ(VAGenericValueTypeInteger, match->value.type);
        ASSERT_GE(match->value.value.i, 1);
        ASSERT_LE((Resolution::DataType)match->value.value.i, maxVal);
        maxRes.width = match->value.value.i;
    }

    // maximum surface height
    match = std::find_if(begin, end, [](const VASurfaceAttrib& a)
        {return a.type == VASurfaceAttribMaxHeight;});
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

void VAAPIFixture::doQuerySurfacesWithConfigAttribs(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{
    uint32_t queryNumSurfaceAttribs;

    doCreateConfigWithAttrib(profile, entrypoint);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID,
                                           NULL,
                                           &queryNumSurfaceAttribs));
    EXPECT_TRUE(queryNumSurfaceAttribs > 0);
    m_querySurfaceAttribList.resize(queryNumSurfaceAttribs);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID,
                                           &m_querySurfaceAttribList[0],
                                           &queryNumSurfaceAttribs));

    EXPECT_TRUE(queryNumSurfaceAttribs > 0);
    EXPECT_TRUE(queryNumSurfaceAttribs <= m_querySurfaceAttribList.size());
    m_querySurfaceAttribList.resize(queryNumSurfaceAttribs);

    for (auto& it : m_querySurfaceAttribList) {

        unsigned int flags
            = 0 | VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;

        EXPECT_NE(it.flags & flags,
                  (unsigned int)VA_SURFACE_ATTRIB_NOT_SUPPORTED);
        EXPECT_TRUE((it.value.type >= VAGenericValueTypeInteger)
                    && it.value.type <= VAGenericValueTypeFunc);
    }
}

inline bool isSurfaceAttribInList(const VASurfaceAttrib& surfaceAttrib,
    const VASurfaceAttribType& surfaceAttribType)
{
    return surfaceAttrib.type == surfaceAttribType;
}

void VAAPIFixture::doGetMaxSurfaceResolution(const VAProfile& profile,
    const VAEntrypoint& entrypoint, Resolution& maxResolution)
{
    SurfaceAttributes::iterator it;
    doQuerySurfacesNoConfigAttribs(profile, entrypoint);
    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMaxWidth));

    if (it != m_querySurfaceAttribList.end()) {
        EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);
        ASSERT_LE(
            it->value.type, std::numeric_limits<Resolution::DataType>::max());
        maxResolution.width = it->value.value.i;
    } else {
        maxResolution.width = std::numeric_limits<Resolution::DataType>::max();
    }

    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMaxHeight));

    if (it != m_querySurfaceAttribList.end()) {
        EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);
        ASSERT_LE(
            it->value.type, std::numeric_limits<Resolution::DataType>::max());
        maxResolution.height = it->value.value.i;
    } else {
        maxResolution.height = std::numeric_limits<Resolution::DataType>::max();
    }
}

void VAAPIFixture::doGetMinSurfaceResolution(const VAProfile& profile,
    const VAEntrypoint& entrypoint, Resolution& minResolution)
{
    SurfaceAttributes::iterator it;
    doQuerySurfacesNoConfigAttribs(profile, entrypoint);
    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMinWidth));

    if (it != m_querySurfaceAttribList.end()) {
        EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);
        ASSERT_GE(it->value.type, 1);
        minResolution.width = it->value.value.i;
    } else {
        minResolution.width = 1;
    }

    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMinHeight));

    if (it != m_querySurfaceAttribList.end()) {
        EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);
        ASSERT_GE(it->value.type, 1);
        minResolution.height = it->value.value.i;
    } else {
        minResolution.height = 1;
    }
}

void VAAPIFixture::doCreateConfigNoAttrib(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{

    ASSERT_STATUS(
        vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0, &m_configID));

    EXPECT_ID(m_configID);

    doQueryConfigAttributes(profile, entrypoint);
}

void VAAPIFixture::doQueryConfigAttributes(const VAProfile& profile,
    const VAEntrypoint& entrypoint, const VAStatus& expectation)
{
    VAProfile queryProfile;
    VAEntrypoint queryEntrypoint;
    int queryNumConfigAttribs;

    m_queryConfigAttribList.resize(m_maxConfigAttributes); // va-api requirement

    ASSERT_STATUS_EQ(expectation,
                     vaQueryConfigAttributes(m_vaDisplay, m_configID,
                                             &queryProfile, &queryEntrypoint,
                                             &m_queryConfigAttribList[0],
                                             &queryNumConfigAttribs));

    if (expectation == VA_STATUS_SUCCESS) {
        m_queryConfigAttribList.resize(queryNumConfigAttribs);
        EXPECT_EQ(queryProfile, profile);
        EXPECT_EQ(queryEntrypoint, entrypoint);
        EXPECT_TRUE(queryNumConfigAttribs > 0);

        m_queryConfigAttribList.resize(queryNumConfigAttribs);

        // reported Config Attributes should be supported
        for (auto& it : m_queryConfigAttribList) {
            EXPECT_NE(it.value, VA_ATTRIB_NOT_SUPPORTED);
        }
    }
}

void VAAPIFixture::doQuerySurfacesNoConfigAttribs(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{
    uint32_t queryNumSurfaceAttribs;

    doCreateConfigNoAttrib(profile, entrypoint);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID,
                                           NULL,
                                           &queryNumSurfaceAttribs));
    EXPECT_TRUE(queryNumSurfaceAttribs > 0);
    m_querySurfaceAttribList.resize(queryNumSurfaceAttribs);

    ASSERT_STATUS(vaQuerySurfaceAttributes(m_vaDisplay, m_configID,
                                           &m_querySurfaceAttribList[0],
                                           &queryNumSurfaceAttribs));

    EXPECT_TRUE(queryNumSurfaceAttribs > 0);
    EXPECT_TRUE(queryNumSurfaceAttribs <= m_querySurfaceAttribList.size());
    m_querySurfaceAttribList.resize(queryNumSurfaceAttribs);

    for (auto& it : m_querySurfaceAttribList) {

        unsigned int flags
            = 0 | VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;

        EXPECT_NE(it.flags & flags,
                  (unsigned int)VA_SURFACE_ATTRIB_NOT_SUPPORTED);
        EXPECT_TRUE((it.value.type >= VAGenericValueTypeInteger)
                    && it.value.type <= VAGenericValueTypeFunc);
    }
}

inline bool isConfigAttribInList(const VAConfigAttrib& configAttrib,
    const VAConfigAttribType& type)
{
    return configAttrib.type == type;
}

void VAAPIFixture::doCreateSurfaces(const VAProfile& profile,
    const VAEntrypoint& entrypoint, const Resolution& resolution)
{
    VASurfaceAttrib* attribList = NULL;
    uint32_t numAttribs = 0;
    // when ConfigAttribs were not queried just do YUV420 as it is considered
    // the universal supported format by the driver.  RT formats depend on
    // profile and entrypoint.
    unsigned int formats = VA_RT_FORMAT_YUV420;

    m_surfaceID.resize(10);

    if (!m_querySurfaceAttribList.empty()) {
        numAttribs = m_querySurfaceAttribList.size();
        attribList = &m_querySurfaceAttribList[0];
    }

    if (!m_queryConfigAttribList.empty()) {
        ConfigAttributes::iterator it = std::find_if(
            m_queryConfigAttribList.begin(), m_queryConfigAttribList.end(),
            std::bind(isConfigAttribInList, std::placeholders::_1,
                      VAConfigAttribRTFormat));
        formats = it->value;
    }

    for (auto& itFormat : g_vaRTFormats) {
        unsigned int currentFormat = formats & itFormat;

        if (currentFormat) {

            ASSERT_STATUS(vaCreateSurfaces(
                m_vaDisplay, currentFormat, resolution.width, resolution.height,
                &m_surfaceID[0], 10, attribList, numAttribs));
            formats &= ~itFormat;
        }
    }
}

void VAAPIFixture::doCreateContext(const Resolution& resolution,
    const VAStatus& expectation)
{
    m_contextID = 0;
    ASSERT_STATUS_EQ(expectation,
                     vaCreateContext(m_vaDisplay, m_configID, resolution.width,
                                     resolution.height, VA_PROGRESSIVE,
                                     &m_surfaceID[0], m_surfaceID.size(),
                                     &m_contextID));
}

void VAAPIFixture::doDestroyContext(const VAStatus& expectation)
{
    ASSERT_STATUS_EQ(expectation, vaDestroyContext(m_vaDisplay, m_contextID));
}

void VAAPIFixture::doCreateBuffer(const VABufferType& bufferType)
{
    ASSERT_STATUS(vaCreateBuffer(m_vaDisplay, m_contextID, bufferType,
                                 sizeof(bufferType), 1, NULL, &m_bufferID));
}

void VAAPIFixture::doDestroyBuffer()
{
    ASSERT_STATUS(vaDestroyBuffer(m_vaDisplay, m_bufferID));
}

void VAAPIFixture::doCreateConfig(const VAProfile& profile,
    const VAEntrypoint& entrypoint)
{
    m_configID = VA_INVALID_ID;
    ASSERT_STATUS(
        vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0, &m_configID));
    EXPECT_ID(m_configID);
}

void VAAPIFixture::doCreateConfigToFail(const VAProfile& profile,
    const VAEntrypoint& entrypoint, int error)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    m_configID = VA_INVALID_ID;

    vaStatus = vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0,
                              &m_configID);
    ASSERT_STATUS_EQ(error, vaStatus);

    if (VA_STATUS_SUCCESS == error) {
        EXPECT_ID(m_configID);
    } else {
        EXPECT_INVALID_ID(m_configID);
    }

    m_queryConfigAttribList.resize(m_maxConfigAttributes); // va-api requirement

    doQueryConfigAttributes(profile, entrypoint,
                            VA_STATUS_ERROR_INVALID_CONFIG);

    ASSERT_STATUS_EQ(VA_STATUS_ERROR_INVALID_CONFIG,
                     vaDestroyConfig(m_vaDisplay, m_configID));
}

void VAAPIFixture::doTerminate()
{
    EXPECT_STATUS(vaTerminate(m_vaDisplay));
}

void VAAPIFixture::doLogSkipTest(const VAProfile& profile,
    const VAEntrypoint& entrypoint) const
{
    RecordProperty("skipped", true);
    std::cout << "[ SKIPPED ]"
              << " " << profile << " / " << entrypoint
              << " not supported on this hardware" << std::endl;
}

TEST_F(VAAPIFixture, getDisplay)
{
    VADisplay vaDisplay;

    vaDisplay = getDisplay();
    ASSERT_TRUE(vaDisplay);
    EXPECT_STATUS(vaTerminate(vaDisplay));
}

} // namespace VAAPI
