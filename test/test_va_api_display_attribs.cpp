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

bool IsAttribType(VADisplayAttribute displayAttribute,
              VADisplayAttribType displayAttribType)
{
    return displayAttribute.type == displayAttribType;
}

class VAAPIDisplayAttribs
    : public VAAPIFixture
{
public:
    VAAPIDisplayAttribs()
        : m_maxNumDisplayAttribs(0)
        , m_actualNumDisplayAttribs(0)
    {
        m_vaDisplay = doInitialize();
    }

    virtual ~VAAPIDisplayAttribs()
    {
        doTerminate();
    }

    bool findDisplayAttribInQueryList(VADisplayAttribType displayAttribType)
    {
        return std::find_if(m_vaQueryDisplayAttribList.begin(),
                            m_vaQueryDisplayAttribList.end(),
                            std::bind(IsAttribType, std::placeholders::_1,
                                      displayAttribType))
              != m_vaQueryDisplayAttribList.end();
    }

protected:
    int m_maxNumDisplayAttribs;
    int m_actualNumDisplayAttribs;
    DisplayAttributes m_vaQueryDisplayAttribList;
};

TEST_F(VAAPIDisplayAttribs, MaxNumDisplayAttribs)
{
    m_maxNumDisplayAttribs = vaMaxNumDisplayAttributes(m_vaDisplay);

    EXPECT_NE(m_maxNumDisplayAttribs, 0);
}

static const DisplayAttribTypes inputTest
    = { VADisplayAttribBrightness,
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
        VADisplayAttribRenderRect };

TEST_F(VAAPIDisplayAttribs, GetDisplayAttribs)
{

    m_maxNumDisplayAttribs = vaMaxNumDisplayAttributes(m_vaDisplay);

    m_vaQueryDisplayAttribList.resize(m_maxNumDisplayAttribs);

    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay,
                                           &m_vaQueryDisplayAttribList[0],
                                           &m_actualNumDisplayAttribs));

    EXPECT_TRUE((unsigned int)m_actualNumDisplayAttribs
                <= m_vaQueryDisplayAttribList.size());

    for (auto& it : inputTest) {
        VADisplayAttribute attrib;
        attrib.type = it;
        ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay, &attrib, 1));

        if (findDisplayAttribInQueryList(attrib.type)) {
            ASSERT_TRUE((attrib.flags & VA_DISPLAY_ATTRIB_GETTABLE)
                        || (attrib.flags & VA_DISPLAY_ATTRIB_SETTABLE));
        }
        else {
            ASSERT_FALSE(attrib.flags & VA_DISPLAY_ATTRIB_NOT_SUPPORTED);
        }
    }
}

TEST_F(VAAPIDisplayAttribs, SetDisplayAttribs)
{

    m_maxNumDisplayAttribs = vaMaxNumDisplayAttributes(m_vaDisplay);

    m_vaQueryDisplayAttribList.resize(m_maxNumDisplayAttribs);

    ASSERT_STATUS(vaQueryDisplayAttributes(m_vaDisplay,
                                           &m_vaQueryDisplayAttribList[0],
                                           &m_actualNumDisplayAttribs));

    m_vaQueryDisplayAttribList.resize(m_actualNumDisplayAttribs);

    ASSERT_STATUS(vaGetDisplayAttributes(m_vaDisplay,
                                         &m_vaQueryDisplayAttribList[0],
                                         m_vaQueryDisplayAttribList.size()));

    for (auto& it : m_vaQueryDisplayAttribList) {
        VADisplayAttribute attrib;
        attrib = it;
        if (it.flags & VA_DISPLAY_ATTRIB_SETTABLE) {
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
} // VAAPI
