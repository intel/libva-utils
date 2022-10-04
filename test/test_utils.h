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

#ifndef TESTVAAPI_test_utils_h
#define TESTVAAPI_test_utils_h

#include <iostream>

namespace VAAPI
{

struct Resolution {
    typedef uint32_t DataType;

    Resolution(const DataType w = 1, const DataType h = 1)
        : width(w), height(h)
    { }

    inline bool operator <=(const Resolution& other) const
    {
        return (width <= other.width) && (height <= other.height);
    }

    inline bool operator >=(const Resolution& other) const
    {
        return (width >= other.width) && (height >= other.height);
    }

    inline bool isWithin(
        const Resolution& minRes, const Resolution& maxRes) const
    {
        return (*this >= minRes) && (*this <= maxRes);
    }

    friend std::ostream& operator <<(std::ostream& os, const Resolution& res)
    {
        return os << res.width << "x" << res.height;
    }

    DataType width;
    DataType height;
};

} // namespace VAAPI

#endif
