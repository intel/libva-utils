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

#include <functional>

namespace VAAPI
{

class VAAPIDisplayAttribs
    : public VAAPIFixture
{
protected:
    void SetUp()
    {
        VAAPIFixture::SetUp();
        doInitialize();
        ASSERT_FALSE(HasFailure());
    }

    void TearDown()
    {
        doTerminate();
        VAAPIFixture::TearDown();
    }

    bool findDisplayAttribInQueryList(const VADisplayAttribType& type)
    {
        const DisplayAttributes::const_iterator begin = m_attribs.begin();
        const DisplayAttributes::const_iterator end = m_attribs.end();
        auto predicate = [&](const VADisplayAttribute & a) {
            return a.type == type;
        };

        return std::find_if(begin, end, predicate) != end;
    }

protected:
    DisplayAttributes m_attribs;
};

static const DisplayAttribTypes types = {
    VADisplayAttribBrightness,
    VADisplayAttribContrast,
    VADisplayAttribHue,
    VADisplayAttribSaturation,
    VADisplayAttribBackgroundColor,
    VADisplayAttribDirectSurface,
    VADisplayAttribRotation,
    VADisplayAttribOutofLoopDeblock,
    VADisplayAttribBLEBlackMode,
    VADisplayAttribBLEWhiteMode,
    VADisplayAttribBlueStretch,
    VADisplayAttribSkinColorCorrection,
    VADisplayAttribCSCMatrix,
    VADisplayAttribBlendColor,
    VADisplayAttribOverlayAutoPaintColorKey,
    VADisplayAttribOverlayColorKey,
    VADisplayAttribRenderMode,
    VADisplayAttribRenderDevice,
    VADisplayAttribRenderRect
};

TEST_F(VAAPIDisplayAttribs, MaxNumDisplayAttribs)
{
    EXPECT_GE(vaMaxNumDisplayAttributes(m_vaDisplay), 0);
}

TEST_F(VAAPIDisplayAttribs, QueryDisplayAttribs)
{
    const int maxAttribs(vaMaxNumDisplayAttributes(m_vaDisplay));
    int numAttribs(0);

    if (maxAttribs <= 0) {
        numAttribs = 256;
        const VaapiStatus status(
            vaQueryDisplayAttributes(m_vaDisplay, NULL, &numAttribs));

        EXPECT_TRUE(
            status == VaapiStatus(VA_STATUS_SUCCESS) ||
            status == VaapiStatus(VA_STATUS_ERROR_UNIMPLEMENTED));

        if (status == VaapiStatus(VA_STATUS_SUCCESS)) {
            EXPECT_EQ(numAttribs, 0);
        }
    } else {
        m_attribs.resize(maxAttribs);
        EXPECT_STATUS(vaQueryDisplayAttributes(m_vaDisplay, m_attribs.data(),
                                               &numAttribs));
        EXPECT_GT(numAttribs, 0);
        EXPECT_LE(numAttribs, maxAttribs);
    }
}

TEST_F(VAAPIDisplayAttribs, GetDisplayAttribs)
{
    const int maxAttribs(vaMaxNumDisplayAttributes(m_vaDisplay));

    if (maxAttribs <= 0) {
        EXPECT_STATUS_EQ(VA_STATUS_ERROR_UNIMPLEMENTED,
                         vaGetDisplayAttributes(m_vaDisplay, NULL, 0));
        return;
    }

    m_attribs.resize(maxAttribs);

    int numAttribs(0);
    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay, m_attribs.data(),
                                           &numAttribs));

    ASSERT_GT(numAttribs, 0);
    ASSERT_LE(numAttribs, maxAttribs);
    m_attribs.resize(numAttribs);

    for (const auto& type : types) {
        VADisplayAttribute attrib{/*type:*/ type};
        ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay, &attrib, 1));

        if (findDisplayAttribInQueryList(attrib.type)) {
            EXPECT_TRUE((attrib.flags & VA_DISPLAY_ATTRIB_GETTABLE)
                        || (attrib.flags & VA_DISPLAY_ATTRIB_SETTABLE));
        } else {
            EXPECT_FALSE(attrib.flags & VA_DISPLAY_ATTRIB_NOT_SUPPORTED);
        }
    }
}

TEST_F(VAAPIDisplayAttribs, SetDisplayAttribs)
{
    const int maxAttribs(vaMaxNumDisplayAttributes(m_vaDisplay));

    if (maxAttribs <= 0) {
        EXPECT_STATUS_EQ(VA_STATUS_ERROR_UNIMPLEMENTED,
                         vaSetDisplayAttributes(m_vaDisplay, NULL, 0));
        return;
    }

    m_attribs.resize(maxAttribs);

    int numAttribs(0);
    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay, m_attribs.data(),
                                           &numAttribs));

    ASSERT_GT(numAttribs, 0);
    ASSERT_LE(numAttribs, maxAttribs);
    m_attribs.resize(numAttribs);

    ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay, m_attribs.data(),
                                         m_attribs.size()));

    for (auto attrib : m_attribs) {
        if (attrib.flags & VA_DISPLAY_ATTRIB_SETTABLE) {
            ASSERT_LE(attrib.min_value, attrib.max_value);

            attrib.value = (attrib.min_value + attrib.max_value) / 2;
            EXPECT_STATUS(vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));

            attrib.value = attrib.min_value - 1;
            EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_PARAMETER,
                             vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));

            attrib.value = attrib.max_value + 1;
            EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_PARAMETER,
                             vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));
        }
    }
}

} // namespace VAAPI
