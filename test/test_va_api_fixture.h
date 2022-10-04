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

#ifndef TESTVAAPI_test_va_api_fixture_h
#define TESTVAAPI_test_va_api_fixture_h

#include "test.h"
#include "test_data.h"
#include "test_defs.h"
#include "test_streamable.h"

namespace VAAPI
{

// The fixture for testing class Foo.
class VAAPIFixture : public ::testing::Test
{
public:
    VAAPIFixture();

    virtual ~VAAPIFixture();

    VADisplay getDisplay();
    VADisplay doInitialize();
    void doTerminate();

    void doCreateContext(const Resolution&,
                         const VAStatus& expectation = VA_STATUS_SUCCESS);
    void doDestroyContext(const VAStatus& expectation = VA_STATUS_SUCCESS);

    void queryConfigProfiles(Profiles&) const;
    void queryConfigEntrypoints(const VAProfile&, Entrypoints&,
                                const VAStatus& = VA_STATUS_SUCCESS) const;
    VAStatus getSupportStatus(const VAProfile&, const VAEntrypoint&) const;
    bool isSupported(const VAProfile&, const VAEntrypoint&) const;

    void getConfigAttributes(const VAProfile&, const VAEntrypoint&,
                             ConfigAttributes&, const VAStatus& = VA_STATUS_SUCCESS) const;
    void createConfig(const VAProfile&, const VAEntrypoint&,
                      const ConfigAttributes& = ConfigAttributes(),
                      const VAStatus& = VA_STATUS_SUCCESS);
    void queryConfigAttributes(const VAProfile&, const VAEntrypoint&,
                               ConfigAttributes&, const VAStatus& = VA_STATUS_SUCCESS) const;
    void destroyConfig(const VAStatus& = VA_STATUS_SUCCESS);

    void querySurfaceAttributes(SurfaceAttributes&) const;
    void getMinMaxSurfaceResolution(Resolution&, Resolution&) const;
    void createSurfaces(Surfaces&, const unsigned format, const Resolution&,
                        const SurfaceAttributes& = SurfaceAttributes(),
                        const VAStatus& = VA_STATUS_SUCCESS) const;
    void destroySurfaces(Surfaces&) const;

    void createBuffer(const VABufferType&, const size_t,
                      const VAStatus& = VA_STATUS_SUCCESS);
    void destroyBuffer(const VAStatus& = VA_STATUS_SUCCESS);

    void skipTest(const std::string& message);
    void skipTest(const VAProfile&, const VAEntrypoint&);

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
    std::string m_restoreDriverName;
    int m_drmHandle;

    VAConfigID m_configID;
    VAContextID m_contextID;
    VABufferID m_bufferID;

    std::string m_skip;
};

/* Test fixture that initializes a shared display once per test suite, and
 * provides it to you in m_vaDisplay from its SetUp() routine.
 */
class VAAPIFixtureSharedDisplay : public VAAPIFixture
{
public:
    VAAPIFixtureSharedDisplay();

    virtual void SetUp();

    static void SetUpTestSuite();
    static void TearDownTestSuite();

private:
    static int s_drmHandle;
    static VADisplay s_vaDisplay;
    static VAStatus s_initStatus;
};

} // namespace VAAPI

#endif
