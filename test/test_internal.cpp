/*
 * Copyright (C) 2017 Intel Corporation. All Rights Reserved.
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
#include "test_utils.h"

#include <vector>

TEST(Internal, Resolution)
{
    using VAAPI::Resolution;

    Resolution res(2, 7);

    ASSERT_EQ(res.width, Resolution::DataType(2));
    ASSERT_EQ(res.height, Resolution::DataType(7));

    EXPECT_LE(res, res);
    EXPECT_LE(res, Resolution(res.width + 1, res.height));
    EXPECT_LE(res, Resolution(res.width, res.height + 1));

    EXPECT_FALSE(res <= Resolution(res.width - 1, res.height));
    EXPECT_FALSE(res <= Resolution(res.width, res.height - 1));

    EXPECT_GE(res, res);
    EXPECT_GE(res, Resolution(res.width - 1, res.height));
    EXPECT_GE(res, Resolution(res.width, res.height - 1));

    EXPECT_FALSE(res >= Resolution(res.width + 1, res.height));
    EXPECT_FALSE(res >= Resolution(res.width, res.height + 1));

    EXPECT_TRUE(res.isWithin(res, res));
    EXPECT_TRUE(res.isWithin(Resolution(res.width - 1, res.height), res));
    EXPECT_TRUE(res.isWithin(Resolution(res.width, res.height - 1), res));
    EXPECT_TRUE(res.isWithin(res, Resolution(res.width + 1, res.height)));
    EXPECT_TRUE(res.isWithin(res, Resolution(res.width, res.height + 1)));

    EXPECT_FALSE(res.isWithin(
                     Resolution(res.width + 1, res.height),
                     Resolution(res.width + 1, res.height + 1)));
    EXPECT_FALSE(res.isWithin(
                     Resolution(res.width, res.height + 1),
                     Resolution(res.width + 1, res.height + 1)));
    EXPECT_FALSE(res.isWithin(
                     Resolution(res.width - 1, res.height - 1),
                     Resolution(res.width - 1, res.height)));
    EXPECT_FALSE(res.isWithin(
                     Resolution(res.width - 1, res.height - 1),
                     Resolution(res.width, res.height - 1)));

    // Verify different initializers
    {
        Resolution resolution{10, 100};
        EXPECT_EQ(resolution.width, Resolution::DataType(10));
        EXPECT_EQ(resolution.height, Resolution::DataType(100));
    }
    {
        Resolution resolution = {10, 100};
        EXPECT_EQ(resolution.width, Resolution::DataType(10));
        EXPECT_EQ(resolution.height, Resolution::DataType(100));
    }
    {
        std::vector<Resolution> resolutions{{10, 100}, {12, 15}};
        ASSERT_EQ(resolutions.size(), 2u);
        EXPECT_EQ(resolutions[0].width, Resolution::DataType(10));
        EXPECT_EQ(resolutions[0].height, Resolution::DataType(100));
        EXPECT_EQ(resolutions[1].width, Resolution::DataType(12));
        EXPECT_EQ(resolutions[1].height, Resolution::DataType(15));
    }
    {
        std::vector<Resolution> resolutions = {{10, 100}, {12, 15}};
        ASSERT_EQ(resolutions.size(), 2u);
        EXPECT_EQ(resolutions[0].width, Resolution::DataType(10));
        EXPECT_EQ(resolutions[0].height, Resolution::DataType(100));
        EXPECT_EQ(resolutions[1].width, Resolution::DataType(12));
        EXPECT_EQ(resolutions[1].height, Resolution::DataType(15));
    }
}
