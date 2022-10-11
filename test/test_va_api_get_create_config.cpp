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

#include <sstream>

namespace VAAPI
{

class VAAPIGetCreateConfig
    : public VAAPIFixture
    , public ::testing::WithParamInterface<std::tuple<VAProfile, VAEntrypoint> >
{
public:
    VAAPIGetCreateConfig()
        : profile(::testing::get<0>(GetParam()))
        , entrypoint(::testing::get<1>(GetParam()))
    { }

protected:
    const VAProfile& profile;
    const VAEntrypoint& entrypoint;

    virtual void SetUp()
    {
        VAAPIFixture::SetUp();
        doInitialize();
        ASSERT_FALSE(HasFailure());
    }

    virtual void TearDown()
    {
        doTerminate();
        VAAPIFixture::TearDown();
    }
};

TEST_P(VAAPIGetCreateConfig, CreateConfigWithAttributes)
{
    const VAStatus expectedStatus = getSupportStatus(profile, entrypoint);

    if (VA_STATUS_SUCCESS != expectedStatus) {
        createConfig(profile, entrypoint, ConfigAttributes(), expectedStatus);
        destroyConfig(VA_STATUS_ERROR_INVALID_CONFIG);
        return;
    }

    // profile and entrypoint are supported
    ConfigAttributes attribs;
    getConfigAttributes(profile, entrypoint, attribs);

    // create config with each individual supported attribute
    for (const auto& attrib : attribs) {
        const auto match = g_vaConfigAttribBitMasks.find(attrib.type);
        if (match != g_vaConfigAttribBitMasks.end()) {
            // it's a bitfield attribute
            uint32_t bitfield(0);
            const BitMasks& masks = match->second;
            for (const auto mask : masks) { // for all bitmasks
                const bool isSet((attrib.value & mask) == mask);

                std::ostringstream oss;
                oss << std::hex << "0x" << attrib.type
                    << ":0x" << attrib.value
                    << ":0x" << mask << ":" << isSet;
                SCOPED_TRACE(oss.str());

                if (isSet) {
                    // supported value
                    bitfield |= mask;
                    createConfig(profile, entrypoint,
                                 ConfigAttributes(
                                     1, {/*type :*/ attrib.type, /*value :*/ mask }));
                    destroyConfig();
                } else {
                    // unsupported value
                    const VAStatus expectation(
                        (attrib.type == VAConfigAttribRTFormat) ?
                        VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT :
                        VA_STATUS_ERROR_INVALID_VALUE);
                    createConfig(profile, entrypoint,
                                 ConfigAttributes(
                                     1, {/*type :*/ attrib.type, /*value :*/ mask}),
                                 expectation);
                    destroyConfig(VA_STATUS_ERROR_INVALID_CONFIG);
                }
            }
            // ensure we processed all supported values
            EXPECT_EQ(bitfield, attrib.value);
        } else {
            // it's a standard attribute
            std::ostringstream oss;
            oss << std::hex << "0x" << attrib.type
                << ":0x" << attrib.value;
            SCOPED_TRACE(oss.str());

            createConfig(profile, entrypoint, ConfigAttributes(1, attrib));
            destroyConfig();
        }
    }
}

TEST_P(VAAPIGetCreateConfig, CreateConfigNoAttributes)
{
    const VAStatus expectedStatus = getSupportStatus(profile, entrypoint);

    if (VA_STATUS_SUCCESS != expectedStatus) {
        createConfig(profile, entrypoint, ConfigAttributes(), expectedStatus);
        destroyConfig(VA_STATUS_ERROR_INVALID_CONFIG);
        return;
    }

    // profile and entrypoint are supported
    createConfig(profile, entrypoint);
    destroyConfig();
}

TEST_P(VAAPIGetCreateConfig, CreateConfigPackedHeaders)
{
    if (!isSupported(profile, entrypoint)) {
        skipTest(profile, entrypoint);
        return;
    }

    ConfigAttributes packedHeaders{{/*.type = */VAConfigAttribEncPackedHeaders}};
    getConfigAttributes(profile, entrypoint, packedHeaders);

    for (uint32_t v(0x00); v < 0xff; ++v) {
        ConfigAttributes attribs = {{
                /*.type = */VAConfigAttribEncPackedHeaders,
                /*.value = */v
            }
        };
        if ((VA_ATTRIB_NOT_SUPPORTED == packedHeaders.front().value)
            || (v & ~packedHeaders.front().value)) {
            // Creating a config should fail if attribute is not supported
            // or for values that are not in the set of supported values.
            createConfig(
                profile, entrypoint, attribs, VA_STATUS_ERROR_INVALID_VALUE);
            destroyConfig(VA_STATUS_ERROR_INVALID_CONFIG);
        } else {
            // Creating a config should succeed for any value within the set of
            // supported values, including 0x0 (i.e. VA_ENC_PACKED_HEADER_NONE).
            createConfig(profile, entrypoint, attribs, VA_STATUS_SUCCESS);
            destroyConfig(VA_STATUS_SUCCESS);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    GetCreateConfig, VAAPIGetCreateConfig,
    ::testing::Combine(::testing::ValuesIn(g_vaProfiles),
                       ::testing::ValuesIn(g_vaEntrypoints)));

} // namespace VAAPI
