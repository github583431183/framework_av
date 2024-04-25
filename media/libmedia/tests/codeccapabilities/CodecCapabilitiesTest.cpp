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

TEST(CodecCapabilitiesTest, VideoCapsTest) {

    // Test Case 1 : c2.exynos.hevc.decoder

    sp<AMessage> details1 = new AMessage;
    details1->setString("alignment", "2x2");
    details1->setString("bitrate-range", "1-120000000");
    details1->setString("block-count-range", "1-32640");
    details1->setString("block-size", "16x16");
    details1->setString("blocks-per-second-range", "1-3916800");
    details1->setInt32("feature-adaptive-playback", 0);
    details1->setInt32("feature-can-swap-width-height", 1);
    details1->setString("max-concurrent-instances", "16");
    details1->setString("measured-frame-rate-1280x720-range", "547-553");
    details1->setString("measured-frame-rate-1920x1080-range", "569-572");
    details1->setString("measured-frame-rate-352x288-range", "1150-1250");
    details1->setString("measured-frame-rate-3840x2160-range", "159-159");
    details1->setString("measured-frame-rate-640x360-range", "528-529");
    details1->setString("measured-frame-rate-720x480-range", "546-548");
    details1->setString("performance-point-1280x720-range", "240");
    details1->setString("performance-point-3840x2160-range", "120");
    details1->setString("size-range", "64x64-3840x2176");

    std::vector<ProfileLevel> profileLevel1{
        ProfileLevel(1, 8388608),
        ProfileLevel(2, 8388608),
        ProfileLevel(4096, 8388608),
        ProfileLevel(8192, 8388608),
    };

    // Test

    std::shared_ptr<VideoCapabilities> videoCaps1
            = VideoCapabilities::Create(MIMETYPE_VIDEO_HEVC, profileLevel1, details1);

    int widthAlignment1 = videoCaps1->getWidthAlignment();
    EXPECT_EQ(widthAlignment1, 2);
    int heightAlignment1 = videoCaps1->getHeightAlignment();
    EXPECT_EQ(heightAlignment1, 2);

    Range<int> bitrateRange1 = videoCaps1->getBitrateRange();
    EXPECT_EQ(bitrateRange1.lower(), 1);
    EXPECT_EQ(bitrateRange1.upper(), 120000000);

    Range<int> supportedWidths1 = videoCaps1->getSupportedWidths();
    EXPECT_EQ(supportedWidths1.upper(), 3840);
    Range<int> supportedHeights1 = videoCaps1->getSupportedHeights();
    EXPECT_EQ(supportedHeights1.upper(), 3840);

    // "frame-rate-range"
    Range<int> supportedFrameRates = videoCaps1->getSupportedFrameRates();
    EXPECT_EQ(supportedFrameRates.lower(), 0);
    EXPECT_EQ(supportedFrameRates.upper(), 960);

    Range<double> supportedFR720p1 = videoCaps1->getSupportedFrameRatesFor(1280, 720);
    EXPECT_EQ(supportedFR720p1.upper(), 960.0);
    Range<double> supportedFR1080p1 = videoCaps1->getSupportedFrameRatesFor(1920, 1080);
    EXPECT_EQ(supportedFR1080p1.upper(), 480.0);
    Range<double> supportedFR4k1 = videoCaps1->getSupportedFrameRatesFor(3840, 2160);
    EXPECT_EQ(std::round(supportedFR4k1.upper()), 121);

    std::optional<Range<double>> achievableFR1080p1
            = videoCaps1->getAchievableFrameRatesFor(1920, 1080);
    ASSERT_NE(achievableFR1080p1, std::nullopt) << "resolution not supported";
    EXPECT_EQ(achievableFR1080p1.value().lower(), 569);
    EXPECT_EQ(achievableFR1080p1.value().upper(), 572);
}