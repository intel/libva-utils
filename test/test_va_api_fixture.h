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
#include <va/va.h>

#include "test.h"
#include "test_data.h"
#include "test_streamable.h"
#include "gtest/gtest.h"

namespace VAAPI {

// The fixture for testing class Foo.
class VAAPIFixture : public ::testing::Test {
public:
    VAAPIFixture();

    virtual ~VAAPIFixture();

    VADisplay getDisplay();
    VADisplay doInitialize();
    void doTerminate();

    void doGetMaxProfiles();
    void doGetMaxEntrypoints();
    void doGetMaxNumConfigAttribs();
    void doGetMaxValues();
    void doQueryConfigProfiles();
    std::vector<VAProfile> getSupportedProfileList();
    bool doFindProfileInList(VAProfile profile);
    void doQueryConfigEntrypoints(VAProfile profile);
    std::vector<VAEntrypoint> getSupportedEntrypointList();
    bool doFindEntrypointInList(VAEntrypoint entrypoint);

    void doFillConfigAttribList();
    void doGetConfigAttributes(VAProfile profile, VAEntrypoint entrypoint);
    void doGetConfigAttributes(VAProfile profile, VAEntrypoint entrypoint,
                               std::vector<VAConfigAttrib>& configAttrib);
    const std::vector<VAConfigAttrib>& getConfigAttribList() const;
    const std::vector<VAConfigAttrib>& getQueryConfigAttribList() const;
    void doCheckAttribsMatch(std::vector<VAConfigAttrib> configAttrib);
    void doCreateConfigWithAttrib(VAProfile profile, VAEntrypoint entrypoint);
    void doQueryConfigAttributes(VAProfile profile, VAEntrypoint entrypoint,
                                 VAStatus expectation = VA_STATUS_SUCCESS);
    void doQuerySurfacesWithConfigAttribs(VAProfile profile,
                                          VAEntrypoint entrypoint);
    void doQuerySurfacesNoConfigAttribs(VAProfile profile,
                                        VAEntrypoint entrypoint);
    void doCreateSurfaces(VAProfile profile, VAEntrypoint entrypoint,
                          std::pair<uint32_t, uint32_t> resolution);
    void
    doGetMaxSurfaceResolution(VAProfile profile, VAEntrypoint entrypoint,
                              std::pair<uint32_t, uint32_t>& maxResolution);

    void doCreateContext(std::pair<uint32_t, uint32_t> resolution,
                         VAStatus expectation = VA_STATUS_SUCCESS);
    void doDestroyContext(VAStatus expectation = VA_STATUS_SUCCESS);
    void doCreateBuffer(VABufferType bufferType);
    void doDestroyBuffer();
    void doCreateConfigNoAttrib(VAProfile profile, VAEntrypoint entrypoint);
    void doCreateConfig(VAProfile profile, VAEntrypoint entrypoint);
    void doCreateConfigToFail(VAProfile profile, VAEntrypoint entrypoint, int error);
    void doDestroyConfig();

    void doLogSkipTest(VAProfile profile, VAEntrypoint entrypoint);

protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
	// Code here will be called immediately after the constructor (right
	// before each test).
    }

    virtual void TearDown()
    {
	// Code here will be called immediately after each test (right
	// before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for
    // VAAPIFixture.
    VADisplay m_vaDisplay;

private:
    char *m_restoreDriverName;
    int m_drmHandle;
    const std::string drmDevicePaths[2];

    int m_maxEntrypoints;
    int m_maxProfiles;
    int m_numProfiles;
    int m_maxConfigAttributes;

    VAConfigID m_configID;
    VAContextID m_contextID;
    VABufferID m_bufferID;

    std::vector<VAProfile> m_profileList;
    std::vector<VAEntrypoint> m_entrypointList;
    std::vector<VAConfigAttrib> m_configAttribList;
    std::vector<VAConfigAttrib> m_configAttribToCreateConfig;
    std::vector<VAConfigAttrib> m_queryConfigAttribList;
    std::vector<VASurfaceAttrib> m_querySurfaceAttribList;
    std::vector<VASurfaceID> m_surfaceID;
};

} // namespace

