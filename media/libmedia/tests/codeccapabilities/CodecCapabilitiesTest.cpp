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

// struct CddReq {
//     CddReq(const char *type, bool encoder) {
//         mediaType = type;
//         isEncoder = encoder;
//     }

//     const char *mediaType;
//     bool isEncoder;
// };

// static void printMediaCodecInfo(sp<MediaCodecInfo> &info) {
//     Vector<AString> mediaTypes;
//     info->getSupportedMediaTypes(&mediaTypes);
//     for (AString mediaType : mediaTypes) {
//         const sp<MediaCodecInfo::Capabilities> cap = info->getCapabilitiesFor(mediaType.c_str());
//         sp<AMessage> details = cap->getDetails();
//         ALOGD("Details: %s", details->debugString().c_str());

//         Vector<ProfileLevel> profileLevels;
//         Vector<uint32_t> colorFormats;
//         cap->getSupportedProfileLevels(&profileLevels);
//         ALOGD("ProfileLevels: ");
//         for (ProfileLevel profileLevel : profileLevels) {
//             ALOGD("Profile: %d, Level: %d", profileLevel.mProfile, profileLevel.mLevel);
//         }
//         cap->getSupportedColorFormats(&colorFormats);
//         ALOGD("SupportedColorFormats: ");
//         for (uint32_t colorFormat : colorFormats) {
//             ALOGD("colorFormat: %d", colorFormat);
//         }
//     }
// }

// TEST(CodecCapabilitiesTest, DeviceDataConstructionTest) {
//     sp<IMediaCodecList> list = MediaCodecList::getInstance();
//     ASSERT_NE(list, nullptr) << "Unable to Detaget MediaCodecList instance.";

//     std::vector<CddReq> cddReq{
//             // media type, isEncoder
//             CddReq(MIMETYPE_AUDIO_AAC, false),
//             CddReq(MIMETYPE_AUDIO_AAC, true),

//             CddReq(MIMETYPE_VIDEO_AVC, false),
//             CddReq(MIMETYPE_VIDEO_HEVC, false),
//             CddReq(MIMETYPE_VIDEO_MPEG4, false),
//             CddReq(MIMETYPE_VIDEO_VP8, false),
//             CddReq(MIMETYPE_VIDEO_VP9, false),
//             CddReq(MIMETYPE_VIDEO_AV1, false),

//             CddReq(MIMETYPE_VIDEO_AVC, true),
//             CddReq(MIMETYPE_VIDEO_HEVC, true),
//             CddReq(MIMETYPE_VIDEO_VP8, true),
//             CddReq(MIMETYPE_VIDEO_AV1, true),
//     };

//     for (CddReq codecReq : cddReq) {
//         ssize_t index = list->findCodecByType(codecReq.mediaType, codecReq.isEncoder);
//         EXPECT_GE(index, 0) << "Wasn't able to find codec for media type: " << codecReq.mediaType
//                             << (codecReq.isEncoder ? " encoder" : " decoder");

//         sp<MediaCodecInfo> info = list->getCodecInfo(index);
//         ASSERT_NE(info, nullptr) << "CodecInfo is null";

//         ALOGD("MediaType: %s", codecReq.mediaType);
//         printMediaCodecInfo(info);
//     }
// }

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

    std::vector<uint32_t> colorFormats1;

    sp<AMessage> defaultFormat1 = new AMessage();
    defaultFormat1->setString(KEY_MIME, MIMETYPE_AUDIO_AAC);

    // Test

    std::shared_ptr<CodecCapabilities> codecCaps1 = std::make_shared<CodecCapabilities>();
    codecCaps1->init(profileLevel1, colorFormats1, false, defaultFormat1, details1);

    std::shared_ptr<AudioCapabilities> audioCaps1 = codecCaps1->getAudioCapabilities();

    Range<int> bitrateRange1 = audioCaps1->getBitrateRange();
    EXPECT_EQ(bitrateRange1.lower(), 8000) << "bitrate range1 does not match. lower: "
            << bitrateRange1.lower();
    EXPECT_EQ(bitrateRange1.upper(), 510000) << "bitrate range1 does not match. upper: "
            << bitrateRange1.upper();

    int maxInputChannelCount1 = audioCaps1->getMaxInputChannelCount();
    EXPECT_EQ(maxInputChannelCount1, 8) << "maxInputChannelCount1 does not match. "
            << maxInputChannelCount1;

    std::vector<int> sampleRates = audioCaps1->getSupportedSampleRates();

    EXPECT_EQ(sampleRates.at(0), 7350);
    EXPECT_EQ(sampleRates.at(2), 11025);

    EXPECT_EQ(audioCaps1->isSampleRateSupported(6000), false) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(8000), true) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(12000), true) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(44000), false) << "isSampleRateSupported failed";
    EXPECT_EQ(audioCaps1->isSampleRateSupported(48000), true) << "isSampleRateSupported failed";
}