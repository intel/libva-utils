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

#include "test.h"
#include "test_va_api_init_terminate.h"
#include "va_version.h"

namespace VAAPI {
VAAPIInitTerminate::VAAPIInitTerminate() {}
VAAPIInitTerminate::~VAAPIInitTerminate() {}

void VAAPIInitTerminate::doInitTerminate()
{
    VADisplay vaDisplay;
    int majorVersion, minorVersion;

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);

    if (vaDisplay) {
	ASSERT_STATUS(vaInitialize(vaDisplay, &majorVersion, &minorVersion));
    }

    EXPECT_EQ(VA_MAJOR_VERSION, majorVersion)
	<< "Check installed driver version";

    EXPECT_EQ(VA_MINOR_VERSION, minorVersion)
	<< "Check installed driver version";

    ASSERT_STATUS(vaTerminate(vaDisplay));
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate) { doInitTerminate(); }

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_i965_Environment)
{
    EXPECT_EQ(0, setenv("LIBVA_DRIVER_NAME", "i965", 1))
	<< "Could not set enviroment variable";
    doInitTerminate();
    EXPECT_EQ(0, unsetenv("LIBVA_DRIVER_NAME"))
	<< "Could not un-set enviroment variable";
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_i965_vaSetDriverName)
{
    VADisplay vaDisplay;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int majorVersion, minorVersion;
    char driver[5] = "i965";

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);

    if (vaDisplay) {
	vaStatus = vaSetDriverName(vaDisplay, driver);
	EXPECT_STATUS(vaStatus);

	vaStatus = vaInitialize(vaDisplay, &majorVersion, &minorVersion);
	EXPECT_STATUS(vaStatus);
    }

    vaStatus = vaTerminate(vaDisplay);
    EXPECT_STATUS(vaStatus);
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_Bad_Environment)
{
    VADisplay vaDisplay;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int majorVersion, minorVersion;

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);

    EXPECT_EQ(0, setenv("LIBVA_DRIVER_NAME", "bad", 1));

    if (vaDisplay)
	vaStatus = vaInitialize(vaDisplay, &majorVersion, &minorVersion);
    EXPECT_STATUS_EQ(VA_STATUS_ERROR_UNKNOWN, (unsigned)vaStatus);

    EXPECT_EQ(0, unsetenv("LIBVA_DRIVER_NAME"));

    vaStatus = vaTerminate(vaDisplay);
    EXPECT_STATUS(vaStatus);
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_Bad_vaSetDriverName)
{
    VADisplay vaDisplay;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    char driver[4] = "bad";

    vaDisplay = getDisplay();
    EXPECT_TRUE(vaDisplay);

    if (vaDisplay) {
	vaStatus = vaSetDriverName(vaDisplay, driver);
	EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_PARAMETER, vaStatus);
    }
}

TEST_F(VAAPIInitTerminate, InitTermWithoutDisplay)
{
    VADisplay vaDisplay = 0; // no display
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int majorVersion, minorVersion;

    vaStatus = vaInitialize(vaDisplay, &majorVersion, &minorVersion);
    EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_DISPLAY, vaStatus);

    vaStatus = vaTerminate(vaDisplay);
    EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_DISPLAY, vaStatus);
}
} // namespace VAAPI
