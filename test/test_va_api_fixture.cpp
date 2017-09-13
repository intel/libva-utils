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
    , m_maxImageFormat(0)
    , m_numImageFormat(0)
    , m_flags(0)
    , m_maxSubPicImageFormat(0)
    , m_numSubPicImageFormat(0)
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
    m_imageFmtList.clear();
    m_subPicFmtList.clear();
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

    return m_vaDisplay;
}

VADisplay VAAPIFixture::doInitialize()
{
    VADisplay vaDisplay;
    int majorVersion, minorVersion;

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);

    if (vaDisplay) {
        EXPECT_STATUS(vaInitialize(vaDisplay, &majorVersion, &minorVersion));
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

void VAAPIFixture::doGetMaxNumImageFormats()
{
    m_maxImageFormat = vaMaxNumImageFormats(m_vaDisplay);

    EXPECT_EQ(m_maxImageFormat, 10);
}

void VAAPIFixture::doGetMaxNumSubPicImageFormats()
{
    m_maxSubPicImageFormat =  vaMaxNumSubpictureFormats(m_vaDisplay);

    EXPECT_EQ(m_maxSubPicImageFormat, 6);
}

void VAAPIFixture::doQueryImageFormats()
{
    m_imageFmtList.resize(m_maxImageFormat);

    ASSERT_STATUS(
        vaQueryImageFormats(m_vaDisplay, &m_imageFmtList[0], &m_numImageFormat));

    // at least one imageformat be supported for tests to be executed
    ASSERT_TRUE(m_numImageFormat > 0);

    m_imageFmtList.resize(m_numImageFormat);
}

void VAAPIFixture::doQuerySubPicImageFormats()
{
    m_subPicFmtList.resize(m_maxSubPicImageFormat);

    ASSERT_STATUS(
        vaQuerySubpictureFormats(m_vaDisplay, &m_subPicFmtList[0], &m_flags, reinterpret_cast<unsigned int*>(&m_numSubPicImageFormat)));

    // at least one subpic image format has to be supported for tests to be executed
    ASSERT_TRUE(m_numSubPicImageFormat > 0);

    m_subPicFmtList.resize(m_numSubPicImageFormat);
}

void VAAPIFixture::doGetMaxValues()
{

    doGetMaxProfiles();
    doGetMaxEntrypoints();
    doGetMaxNumConfigAttribs();
    doGetMaxNumImageFormats();
    doGetMaxNumSubPicImageFormats();
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

std::vector<VAProfile> VAAPIFixture::getSupportedProfileList()
{
    return m_profileList;
}

std::vector<VAEntrypoint> VAAPIFixture::getSupportedEntrypointList()
{
    return m_entrypointList;
}

bool VAAPIFixture::doFindProfileInList(VAProfile profile)
{
    return std::find(m_profileList.begin(), m_profileList.end(), profile)
           != m_profileList.end();
}

void VAAPIFixture::doQueryConfigEntrypoints(VAProfile profile)
{
    int numEntrypoints = 0;

    m_entrypointList.resize(m_maxEntrypoints);
    ASSERT_STATUS(vaQueryConfigEntrypoints(
        m_vaDisplay, profile, &m_entrypointList[0], &numEntrypoints));

    EXPECT_TRUE(numEntrypoints > 0);

    m_entrypointList.resize(numEntrypoints);
}

bool VAAPIFixture::doFindEntrypointInList(VAEntrypoint entrypoint)
{
    return std::find(m_entrypointList.begin(), m_entrypointList.end(),
                     entrypoint)
           != m_entrypointList.end();
}

void VAAPIFixture::doFillConfigAttribList()
{
    m_configAttribList.clear();
    // fill it with all the VAConfigAttribs known
    for (auto& it : m_vaConfigAttribs) {
        VAConfigAttrib configAttrib;

        configAttrib.type = it;

        m_configAttribList.push_back(configAttrib);
    }

    EXPECT_EQ(m_configAttribList.size(), m_vaConfigAttribs.size());
}

bool VAAPIFixture::doFindFormat (VAImageFormat imageFormat, uint32_t current)
{
        return (imageFormat.fourcc == current);
}

uint32_t VAAPIFixture::doFindImageFormatInList (uint32_t format)
{
    if(std::find_if(m_imageFmtList.begin(), m_imageFmtList.end(),
        std::bind(doFindFormat,  std::placeholders::_1, format))
    != m_imageFmtList.end() )
        return format;
    else
	return 0;
}

bool VAAPIFixture::doFindSubPicFormat(VAImageFormat subPicImagefmt, uint32_t subpicfmt)
{
        return (subPicImagefmt.fourcc == subpicfmt);
}

uint32_t VAAPIFixture::doFindSubPicImageFormatInList(uint32_t subPicFmt)
{
    if(std::find_if(m_subPicFmtList.begin(), m_subPicFmtList.end(),
        std::bind(doFindSubPicFormat,  std::placeholders::_1, subPicFmt))
		 != m_subPicFmtList.end() )
  	return subPicFmt;
    else
	return 0;
}

void VAAPIFixture::doGetConfigAttributes(VAProfile profile,
                                         VAEntrypoint entrypoint)
{
    int numAttributes = m_configAttribList.size();
    ASSERT_STATUS(vaGetConfigAttributes(m_vaDisplay, profile, entrypoint,
                                        &m_configAttribList[0], numAttributes));
}

void VAAPIFixture::doGetConfigAttributes(
    VAProfile profile, VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib>& configAttrib)
{
    int numAttributes = configAttrib.size();
    ASSERT_STATUS(vaGetConfigAttributes(m_vaDisplay, profile, entrypoint,
                                        &configAttrib[0], numAttributes));
}

const std::vector<VAConfigAttrib>& VAAPIFixture::getConfigAttribList() const
{
    return m_configAttribList;
}

const std::vector<VAConfigAttrib>&
VAAPIFixture::getQueryConfigAttribList() const
{
    return m_queryConfigAttribList;
}

void VAAPIFixture::doCheckAttribsMatch(std::vector<VAConfigAttrib> configAttrib)
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

void VAAPIFixture::doCreateConfigWithAttrib(VAProfile profile,
                                            VAEntrypoint entrypoint)
{

    m_configAttribToCreateConfig.clear();
    for (auto& it : m_configAttribList) {

        if (it.value != VA_ATTRIB_NOT_SUPPORTED)
            m_configAttribToCreateConfig.push_back(it);
    }

    ASSERT_STATUS(vaCreateConfig(
        m_vaDisplay, profile, entrypoint, &m_configAttribToCreateConfig[0],
        m_configAttribToCreateConfig.size(), &m_configID));

    EXPECT_TRUE(m_configID > 0);

    doQueryConfigAttributes(profile, entrypoint);
}

void VAAPIFixture::doDestroyConfig()
{
    ASSERT_STATUS(vaDestroyConfig(m_vaDisplay, m_configID));
}

void VAAPIFixture::doQuerySurfacesWithConfigAttribs(VAProfile profile,
                                                    VAEntrypoint entrypoint)
{
    std::vector<VASurfaceAttrib> m_querySurfaceAttribList;
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

inline bool isSurfaceAttribInList(VASurfaceAttrib surfaceAttrib,
                                  VASurfaceAttribType surfaceAttribType)
{
    return surfaceAttrib.type == surfaceAttribType;
}

void VAAPIFixture::doGetMaxSurfaceResolution(
    VAProfile profile, VAEntrypoint entrypoint,
    std::pair<uint32_t, uint32_t>& maxResolution)
{
    std::vector<VASurfaceAttrib>::iterator it;
    doQuerySurfacesNoConfigAttribs(profile, entrypoint);
    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMaxWidth));

    EXPECT_TRUE(it != m_querySurfaceAttribList.end());

    EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);

    maxResolution.first = it->value.value.i;

    it = std::find_if(m_querySurfaceAttribList.begin(),
                      m_querySurfaceAttribList.end(),
                      std::bind(isSurfaceAttribInList, std::placeholders::_1,
                                VASurfaceAttribMaxHeight));

    EXPECT_TRUE(it != m_querySurfaceAttribList.end());

    EXPECT_EQ(it->value.type, VAGenericValueTypeInteger);

    maxResolution.second = it->value.value.i;
}

void VAAPIFixture::doCreateConfigNoAttrib(VAProfile profile,
                                          VAEntrypoint entrypoint)
{

    ASSERT_STATUS(
        vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0, &m_configID));

    EXPECT_TRUE(m_configID > 0);

    doQueryConfigAttributes(profile, entrypoint);
}

void VAAPIFixture::doQueryConfigAttributes(VAProfile profile,
                                           VAEntrypoint entrypoint,
                                           VAStatus expectation)
{
    VAProfile queryProfile;
    VAEntrypoint queryEntrypoint;
    int queryNumConfigAttribs;

    m_queryConfigAttribList.resize(m_maxConfigAttributes); // va-api requirement

    ASSERT_EQ(vaQueryConfigAttributes(
                  m_vaDisplay, m_configID, &queryProfile, &queryEntrypoint,
                  &m_queryConfigAttribList[0], &queryNumConfigAttribs),
              expectation);

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

void VAAPIFixture::doQuerySurfacesNoConfigAttribs(VAProfile profile,
                                                  VAEntrypoint entrypoint)
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

inline bool isConfigAttribInList(VAConfigAttrib configAttrib,
                                 VAConfigAttribType type)
{
    return configAttrib.type == type;
}

void VAAPIFixture::doCreateSurfaces(VAProfile profile, VAEntrypoint entrypoint,
                                    std::pair<uint32_t, uint32_t> resolution)
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
        std::vector<VAConfigAttrib>::iterator it = std::find_if(
            m_queryConfigAttribList.begin(), m_queryConfigAttribList.end(),
            std::bind(isConfigAttribInList, std::placeholders::_1,
                      VAConfigAttribRTFormat));
        formats = it->value;
    }

    for (auto& itFormat : m_vaRTFormats) {
        unsigned int currentFormat = formats & itFormat;

        if (currentFormat) {

            ASSERT_STATUS(vaCreateSurfaces(
                m_vaDisplay, currentFormat, resolution.first, resolution.second,
                &m_surfaceID[0], 10, attribList, numAttribs));
            formats &= ~itFormat;
        }
    }
}

void VAAPIFixture::doCreateContext(std::pair<uint32_t, uint32_t> resolution,
                                   VAStatus expectation)
{
    m_contextID = 0;
    ASSERT_EQ(vaCreateContext(m_vaDisplay, m_configID, resolution.first,
                              resolution.second, VA_PROGRESSIVE,
                              &m_surfaceID[0], m_surfaceID.size(),
                              &m_contextID),
              expectation);
}

void VAAPIFixture::doDestroyContext(VAStatus expectation)
{
    ASSERT_EQ(vaDestroyContext(m_vaDisplay, m_contextID), expectation);
}

void VAAPIFixture::doCreateBuffer(VABufferType bufferType)
{
    ASSERT_STATUS(vaCreateBuffer(m_vaDisplay, m_contextID, bufferType,
                                 sizeof(bufferType), 1, NULL, &m_bufferID));
}

void VAAPIFixture::doDestroyBuffer()
{
    ASSERT_STATUS(vaDestroyBuffer(m_vaDisplay, m_bufferID));
}

void VAAPIFixture::doCreateConfig(VAProfile profile, VAEntrypoint entrypoint)
{
    m_configID = 0;
    ASSERT_STATUS(
        vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0, &m_configID));
    EXPECT_NE(m_configID, 0u);
}

void VAAPIFixture::doCreateConfigToFail(VAProfile profile,
                                        VAEntrypoint entrypoint, int error)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    m_configID = 0;

    vaStatus = vaCreateConfig(m_vaDisplay, profile, entrypoint, NULL, 0,
                              &m_configID);
    ASSERT_EQ(vaStatus, error);

    EXPECT_EQ(m_configID, 0u);

    m_queryConfigAttribList.resize(m_maxConfigAttributes); // va-api requirement

    doQueryConfigAttributes(profile, entrypoint,
                            VA_STATUS_ERROR_INVALID_CONFIG);

    ASSERT_EQ(vaDestroyConfig(m_vaDisplay, m_configID),
              VA_STATUS_ERROR_INVALID_CONFIG);
}

void VAAPIFixture::doTerminate() { EXPECT_STATUS(vaTerminate(m_vaDisplay)); }

TEST_F(VAAPIFixture, getDisplay)
{
    VADisplay vaDisplay;

    vaDisplay = getDisplay();
    ASSERT_TRUE(vaDisplay);
}

void VAAPIFixture::doLogSkipTest(VAProfile profile, VAEntrypoint entrypoint)
{
    RecordProperty("skipped", true);
    std::cout << "[ SKIPPED ]"
	      << " " << profile << " / " << entrypoint
	      << " not supported on this hardware" << std::endl;
}
} // namespace VAAPI
