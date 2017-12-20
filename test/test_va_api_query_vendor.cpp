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

// no addtl. fixture functionality needed... just a unique test name
typedef VAAPIFixture VAAPIQueryVendor;

TEST_F(VAAPIQueryVendor, Intel_i965_Vendor)
{
    const char* vendor = NULL;
    std::string vendorString, findIntel("Intel i965 driver");
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int majorVersion, minorVersion;

    m_vaDisplay = getDisplay();
    ASSERT_TRUE(m_vaDisplay);

    vaStatus = vaInitialize(m_vaDisplay, &majorVersion, &minorVersion);
    ASSERT_STATUS(vaStatus);
    vendor = vaQueryVendorString(m_vaDisplay);
    vendorString.assign(vendor);
    EXPECT_NE(std::string::npos, vendorString.find(findIntel))
        << "couldn't find vendor in " << vendorString << std::endl;

    vaStatus = vaTerminate(m_vaDisplay);
    ASSERT_STATUS(vaStatus);
}

} // VAAPI
