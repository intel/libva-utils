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

class VAAPIGetMaxValues
    : public VAAPIFixtureSharedDisplay
{
};

TEST_F(VAAPIGetMaxValues, CheckMaxProfile)
{
    EXPECT_GT(vaMaxNumProfiles(m_vaDisplay), 0);
}

TEST_F(VAAPIGetMaxValues, CheckMaxEntrypoints)
{
    EXPECT_GT(vaMaxNumEntrypoints(m_vaDisplay), 0);
}

TEST_F(VAAPIGetMaxValues, CheckMaxConfigAttributes)
{
    EXPECT_GT(vaMaxNumConfigAttributes(m_vaDisplay), 0);
}

} // namespace VAAPI
