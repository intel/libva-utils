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

#include "test_va_api_getimage.h"

namespace VAAPI {

VAAPIGetImage::VAAPIGetImage () {
    m_vaDisplay = doInitialize();
}

VAAPIGetImage::~VAAPIGetImage() { doTerminate(); }

TEST_P(VAAPIGetImage, GetImageTest)
{
    uint32_t src_Fourcc = ::testing::get<0>(GetParam());
    const std::pair<uint32_t, uint32_t>& currentResolution
        = ::testing::get<1>(GetParam());

    const VAProfile currentProfile = VAProfileNone;
    const VAEntrypoint currentEntrypoint = VAEntrypointVideoProc;

    uint32_t result ;
    int index = 0;
    VASurfaceID surface_id = VA_INVALID_SURFACE;

    doGetMaxValues();
    doQueryConfigProfiles();

    if (doFindProfileInList(currentProfile)) {

        doQueryConfigEntrypoints(currentProfile);
        if (doFindEntrypointInList(currentEntrypoint)) {
            // profile and entrypoint are supported
            doFillConfigAttribList();

            doGetConfigAttributes(currentProfile, currentEntrypoint);

            doQuerySurfacesWithConfigAttribs(currentProfile,
                                                  currentEntrypoint);

            doGetMaxNumImageFormats();

            doQueryImageFormats();

            result = doFindImageFormatInList(src_Fourcc);

            if( result == src_Fourcc) {
                doCreateSurfaces(currentProfile, currentEntrypoint,
                                                        currentResolution);
                doUploadImage();
                surface_id = doGetNextSurface(index);
                doGetImage(src_Fourcc, surface_id, currentResolution);

                doMapBuffer();
                doUnMapBuffer();

                doDestroyImage();
                doDestroySurfaces();
            }
        }
        doDestroyConfig();
   }
}
INSTANTIATE_TEST_CASE_P(
    GetImage, VAAPIGetImage,
    ::testing::Combine(::testing::ValuesIn(m_vaImageFormats),
                       ::testing::ValuesIn(m_vaResolutions)));
} // VAAPI
