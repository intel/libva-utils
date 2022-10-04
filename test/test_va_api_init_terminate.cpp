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
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include<compat_win32.h>
#endif

namespace VAAPI
{

class VAAPIInitTerminate
    : public VAAPIFixture
{
protected:
    // call vaInitialize and vaTerminate expecting success
    void doInitTerminate()
    {
        int major, minor;

        VADisplay display = getDisplay();
        ASSERT_TRUE(display);

        ASSERT_STATUS(vaInitialize(display, &major, &minor));

        EXPECT_EQ(VA_MAJOR_VERSION, major)
                << "Check installed driver version";
        EXPECT_LE(VA_MINOR_VERSION, minor)
                << "Check installed driver version";

        ASSERT_STATUS(vaTerminate(display));
    }
};

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate)
{
    doInitTerminate();
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_Bad_Environment)
{
    int major, minor;

    VADisplay display = getDisplay();
    ASSERT_TRUE(display);

    ASSERT_EQ(0, setenv("LIBVA_DRIVER_NAME", "bad", 1));

    EXPECT_STATUS_EQ(
        VA_STATUS_ERROR_UNKNOWN, vaInitialize(display, &major, &minor));

    EXPECT_EQ(0, unsetenv("LIBVA_DRIVER_NAME"));

    EXPECT_STATUS(vaTerminate(display));
}

TEST_F(VAAPIInitTerminate, vaInitialize_vaTerminate_Bad_vaSetDriverName)
{
    int major, minor;

    VADisplay display = getDisplay();
    ASSERT_TRUE(display);

    {
        // driver name length == 0 invalid
        char driver[1] = "";
        EXPECT_STATUS_EQ(
            VA_STATUS_ERROR_INVALID_PARAMETER, vaSetDriverName(display, driver));
    }

    {
        // driver name length >= 256 invalid
        char driver[257];
        driver[256] = '\0';
        std::string(256, 'x').copy(driver, 256);
        EXPECT_STATUS_EQ(
            VA_STATUS_ERROR_INVALID_PARAMETER, vaSetDriverName(display, driver));
    }

    {
        // acceptable driver name, but does not exist
        char driver[4] = "bad";
        EXPECT_STATUS(vaSetDriverName(display, driver));

        EXPECT_STATUS_EQ(
            VA_STATUS_ERROR_UNKNOWN, vaInitialize(display, &major, &minor));
    }

    EXPECT_STATUS(vaTerminate(display));
}

TEST_F(VAAPIInitTerminate, InitTermWithoutDisplay)
{
    VADisplay display = 0; // no display
    int major, minor;

    EXPECT_STATUS_EQ(
        VA_STATUS_ERROR_INVALID_DISPLAY, vaInitialize(display, &major, &minor));

    EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_DISPLAY, vaTerminate(display));
}

} // namespace VAAPI
