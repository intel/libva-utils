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

namespace VAAPI {

class VAAPIDisplayAttribs
    : public VAAPIFixture
{
public:
    VAAPIDisplayAttribs()
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPIDisplayAttribs()
    {
        doTerminate();
    }

    bool findDisplayAttribInQueryList(const VADisplayAttribType& type)
    {
        const DisplayAttributes::const_iterator begin = m_attribs.begin();
        const DisplayAttributes::const_iterator end = m_attribs.end();
        auto predicate = [&](const VADisplayAttribute& a) {
            return a.type == type;
        };

        return std::find_if(begin, end, predicate) != end;
    }

protected:
    DisplayAttributes m_attribs;
};

TEST_F(VAAPIDisplayAttribs, MaxNumDisplayAttribs)
{
    EXPECT_NE(vaMaxNumDisplayAttributes(m_vaDisplay), 0);
}

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

TEST_F(VAAPIDisplayAttribs, GetDisplayAttribs)
{
    m_attribs.resize(vaMaxNumDisplayAttributes(m_vaDisplay));

    int numAttribs(0);
    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay, m_attribs.data(),
        &numAttribs));

    EXPECT_LE((size_t)numAttribs, m_attribs.size());

    for (const auto& type : types) {
        VADisplayAttribute attrib{type: type};
        ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay, &attrib, 1));

        if (findDisplayAttribInQueryList(attrib.type)) {
            ASSERT_TRUE((attrib.flags & VA_DISPLAY_ATTRIB_GETTABLE)
                || (attrib.flags & VA_DISPLAY_ATTRIB_SETTABLE));
        } else {
            ASSERT_FALSE(attrib.flags & VA_DISPLAY_ATTRIB_NOT_SUPPORTED);
        }
    }
}

TEST_F(VAAPIDisplayAttribs, SetDisplayAttribs)
{
    m_attribs.resize(vaMaxNumDisplayAttributes(m_vaDisplay));

    int numAttribs(0);
    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay,
        m_attribs.data(), &numAttribs));

    m_attribs.resize(numAttribs);

    ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay, m_attribs.data(),
        m_attribs.size()));

    for (auto attrib : m_attribs) {
        if (attrib.flags & VA_DISPLAY_ATTRIB_SETTABLE) {
            attrib.value = (attrib.min_value + attrib.max_value) / 2;
            ASSERT_STATUS(vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));

            attrib.value = attrib.min_value - 1;
            ASSERT_STATUS_EQ(VA_STATUS_ERROR_INVALID_PARAMETER,
                vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));

            attrib.value = attrib.max_value + 1;
            ASSERT_STATUS_EQ(VA_STATUS_ERROR_INVALID_PARAMETER,
                vaSetDisplayAttributes(m_vaDisplay, &attrib, 1));
        }
    }
}

} // namespace VAAPI
