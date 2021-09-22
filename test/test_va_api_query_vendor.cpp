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

namespace VAAPI
{

// Inheriting from VAAPIFixture is not necessary as this test is not
// overriding or extending any functionality defined in fixture.
// Thus, a typedef is sufficient for getting a unique test name for gtest.

typedef VAAPIFixture VAAPIQueryVendor;

TEST_F(VAAPIQueryVendor, NotEmpty)
{
    int major, minor;

    VADisplay display = getDisplay();
    ASSERT_TRUE(display);

    ASSERT_STATUS(vaInitialize(display, &major, &minor));
    const std::string vendor(vaQueryVendorString(display));
    EXPECT_GT(vendor.size(), 0u);
    ASSERT_STATUS(vaTerminate(display));
}

} // namespace VAAPI
