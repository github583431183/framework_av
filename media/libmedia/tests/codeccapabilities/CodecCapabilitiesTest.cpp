/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CodecCapabilitiesTest"

#include <utils/Log.h>

#include <memory>

#include <gtest/gtest.h>

#include <binder/Parcel.h>

#include <media/CodecCapabilities.h>
#include <media/CodecCapabilitiesUtils.h>
#include <media/MediaCodecInfo.h>

#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AString.h>

using namespace android;

TEST(CodecCapabilitiesTest, AudioCapsTest) {

    // Test Case 1 : AAC

    sp<AMessage> details1 = new AMessage;
    details1->setString("bitrate-range", "8000-960000");
    details1->setString("max-channel-count", "8");
    details1->setString("sample-rate-ranges",
            "7350,8000,11025,12000,16000,22050,24000,32000,44100,48000");

    std::vector<ProfileLevel> profileLevel1{
        ProfileLevel(2, 0),
        ProfileLevel(5, 0),
        ProfileLevel(29, 0),
        ProfileLevel(23, 0),
        ProfileLevel(39, 0),
        ProfileLevel(20, 0),
        ProfileLevel(42, 0),
    };

    // Test

    std::shared_ptr<AudioCapabilities> audioCaps1
            = AudioCapabilities::Create(MIMETYPE_AUDIO_AAC, profileLevel1, details1);

    Range<int> bitrateRange1 = audioCaps1->getBitrateRange();
    EXPECT_EQ(bitrateRange1.lower(), 8000) << "bitrate range1 does not match. lower: "
            << bitrateRange1.lower();
    EXPECT_EQ(bitrateRange1.upper(), 510000) << "bitrate range1 does not match. upper: "
            << bitrateRange1.upper();

    int maxInputChannelCount1 = audioCaps1->getMaxInputChannelCount();
    EXPECT_EQ(maxInputChannelCount1, 8);
    int minInputChannelCount1 = audioCaps1->getMinInputChannelCount();
    EXPECT_EQ(minInputChannelCount1, 1);

    std::vector<int> sampleRates1 = audioCaps1->getSupportedSampleRates();
    EXPECT_EQ(sampleRates1.at(0), 7350);
    EXPECT_EQ(sampleRates1.at(2), 11025);

    EXPECT_EQ(audioCaps1->isSampleRateSupported(6000), false) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(8000), true) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(12000), true) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(44000), false) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(48000), true) << "isSampleRateSupported failed";

    // Test case 2 : RAW

    sp<AMessage> details2 = new AMessage;
    details2->setString("bitrate-range", "1-10000000");
    details2->setString("max-channel-count", "12");
    details2->setString("sample-rate-ranges", "8000-192000");

    std::vector<ProfileLevel> profileLevel2;

    // Test

    std::shared_ptr<AudioCapabilities> audioCaps2
            = AudioCapabilities::Create(MIMETYPE_AUDIO_RAW, profileLevel2, details2);

    Range<int> bitrateRange2 = audioCaps2->getBitrateRange();
    EXPECT_EQ(bitrateRange2.lower(), 1);
    EXPECT_EQ(bitrateRange2.upper(), 10000000);

    int maxInputChannelCount2 = audioCaps2->getMaxInputChannelCount();
    EXPECT_EQ(maxInputChannelCount2, 12);
    int minInputChannelCount2 = audioCaps2->getMinInputChannelCount();
    EXPECT_EQ(minInputChannelCount2, 1);

    std::vector<Range<int>> sampleRateRanges2 = audioCaps2->getSupportedSampleRateRanges();
    EXPECT_EQ(sampleRateRanges2.size(), 1);
    EXPECT_EQ(sampleRateRanges2.at(0).lower(), 8000);
    EXPECT_EQ(sampleRateRanges2.at(0).upper(), 192000);

    EXPECT_EQ(audioCaps2->isSampleRateSupported(7000), false);
    EXPECT_EQ(audioCaps2->isSampleRateSupported(10000), true);
    EXPECT_EQ(audioCaps2->isSampleRateSupported(193000), false);
}
