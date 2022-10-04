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

class VAAPIConfigAttribs
    : public VAAPIFixtureSharedDisplay
    , public ::testing::WithParamInterface<std::tuple<VAProfile, VAEntrypoint> >
{
public:
    VAAPIConfigAttribs()
        : profile(::testing::get<0>(GetParam()))
        , entrypoint(::testing::get<1>(GetParam()))
    { }

protected:
    const VAProfile& profile;
    const VAEntrypoint& entrypoint;

    void validateConfigAttributes(const ConfigAttributes& actual,
                                  const ConfigAttributes& supported) const
    {
        const size_t size(actual.size());
        ASSERT_EQ(size, supported.size());

        // Require that actual and supported are in the same order, by type
        for (size_t i(0); i < size; ++i) {
            const VAConfigAttrib& aAttrib = actual[i];
            const VAConfigAttrib& sAttrib = supported[i];

            ASSERT_EQ(aAttrib.type, sAttrib.type);

            // NOTE: If an attribute was not explicitly set by user/app during
            // createConfig (as is the case for GetConfigAttribs test), then
            // all known drivers currently return the same value from
            // vaQueryConfigAttributes as returned from vaGetConfigAttributes.
            // However, there are several bitfield-type attributes that can
            // only take on "one" choice from the supported value relayed in the
            // bitfield from vaGetConfigAttributes.  It still remains to be
            // clarified whether drivers should actually return the "default"
            // chosen value from vaQueryConfigAttributes when it is not
            // specified by user/app during createConfig.  Hence, for now, for
            // bitfield-type attributes we only require that the actual value
            // is equal-to or a "subset" of the supported value.

            switch (aAttrib.type) {
            // Read/write bitfield attribute can be a subset of supported values
            case VAConfigAttribRTFormat:
            case VAConfigAttribRateControl:
            case VAConfigAttribDecSliceMode:
            case VAConfigAttribEncPackedHeaders:
            case VAConfigAttribEncInterlaced:
            case VAConfigAttribFEIFunctionType:
                EXPECT_EQ(sAttrib.value & aAttrib.value, aAttrib.value);
                break;

            // read-only and/or non-bitfield attributes
            default:
                EXPECT_EQ(sAttrib.value, aAttrib.value);
            }
        }
    }
};

TEST_P(VAAPIConfigAttribs, GetConfigAttribs)
{
    // The driver must support creating a config without any user specified
    // attributes, in which case the driver will use it's own defaults.  The
    // app should be able to query those default attributes for such created
    // config via vaQueryConfigAttributes.  Those default attribute values
    // should be consistent with supported attribute values returned by
    // vaGetConfigAttributes.

    if (!isSupported(profile, entrypoint)) {
        skipTest(profile, entrypoint);
        return;
    }

    // create config without attributes (i.e. use driver defaults)
    createConfig(profile, entrypoint);

    // query default attributes from the config we just created
    ConfigAttributes actual;
    queryConfigAttributes(profile, entrypoint, actual);

    // we're done with the config
    destroyConfig();

    // copy the actual attributes and reset their values so we
    // can get the supported values for them.
    ConfigAttributes supported = actual;
    std::for_each(supported.begin(), supported.end(),
    [](VAConfigAttrib & s) {
        s.value = 0;
    });

    // get supported config attribute values
    getConfigAttributes(profile, entrypoint, supported);

    // verify actual config attribute values are supported values
    validateConfigAttributes(actual, supported);
}

INSTANTIATE_TEST_SUITE_P(
    Attributes, VAAPIConfigAttribs,
    ::testing::Combine(::testing::ValuesIn(g_vaProfiles),
                       ::testing::ValuesIn(g_vaEntrypoints)));

} // namespace VAAPI
