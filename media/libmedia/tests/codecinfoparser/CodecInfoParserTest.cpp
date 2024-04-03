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
#define LOG_TAG "MediaCodecInfoParserTest"
#include <utils/Log.h>

#include <memory>

#include <gtest/gtest.h>

#include <binder/Parcel.h>

#include <media/MediaCodecInfo.h>
#include <media/MediaCodecInfoParser.h>

#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AString.h>

using namespace android;

struct CddReq {
    CddReq(const char *type, bool encoder) {
        mediaType = type;
        isEncoder = encoder;
    }

    const char *mediaType;
    bool isEncoder;
};

static void verifyInfoParserResults(sp<MediaCodecInfo> &info, bool isEncoder) {
    Vector<AString> mediaTypes;
    info->getSupportedMediaTypes(&mediaTypes);
    for (AString mediaType : mediaTypes) {
        const sp<MediaCodecInfo::Capabilities> cap = info->getCapabilitiesFor(mediaType.c_str());
        sp<AMessage> details = cap->getDetails();
        ALOGD("Details: %s", details->debugString().c_str());

        Vector<MediaCodecInfo::ProfileLevel> profileLevels;
        Vector<uint32_t> colorFormats;
        cap->getSupportedProfileLevels(&profileLevels);
        ALOGD("ProfileLevels: ");
        for (MediaCodecInfo::ProfileLevel profileLevel : profileLevels) {
            ALOGD("Profile: %d, Level: %d", profileLevel.mProfile, profileLevel.mLevel);
        }
        cap->getSupportedColorFormats(&colorFormats);
        ALOGD("SupportedColorFormats: ");
        for (uint32_t colorFormat : colorFormats) {
            ALOGD("colorFormat: %d", colorFormat);
        }

        sp<AMessage> defaultFormat = new AMessage();
        defaultFormat->setString(KEY_MIME, mediaType);

        MediaCodecInfoParser::CodecCapabilities codecCap
                = MediaCodecInfoParser::CodecCapabilities(
                profileLevels, colorFormats, isEncoder, defaultFormat, details);

        std::shared_ptr<MediaCodecInfoParser::AudioCapabilities>
                audioCap = codecCap.getAudioCapabilities();
        std::shared_ptr<MediaCodecInfoParser::VideoCapabilities>
                videoCap = codecCap.getVideoCapabilities();
        std::shared_ptr<MediaCodecInfoParser::EncoderCapabilities>
                encoderCap = codecCap.getEncoderCapabilities();

        if (audioCap != nullptr) {
            Range<int> bitrateRange = audioCap->getBitrateRange();
            std::vector<int> supportedSampleRates = audioCap->getSupportedSampleRates();
            std::vector<Range<int>> supportedSampleRateRanges
                    = audioCap->getSupportedSampleRateRanges();
            int maxInputChannelCount = audioCap->getMaxInputChannelCount();
            int minInputChannelCount = audioCap->getMinInputChannelCount();
            std::vector<Range<int>> inputChannelCountRanges
                    = audioCap->getInputChannelCountRanges();
        }
    }
}

// // Audio coddec details
// AMessage details0 = {
//     string bitrate-range = "8000-960000",
//     string max-channel-count = "8",
//     string sample-rate-ranges = "7350,8000,11025,12000,16000,22050,24000,32000,44100,48000"
// }

// // Video codec details
// AMessage details1 = {
//     string alignment = "2x2",
//     string bitrate-range = "1-120000000",
//     string block-count-range = "1-32400",
//     string block-size = "16x16",
//     string blocks-per-second-range = "1-3888000",
//     int32_t feature-adaptive-playback = 0,
//     int32_t feature-can-swap-width-height = 1,
//     string max-concurrent-instances = "16",
//     string measured-frame-rate-1280x720-range = "540-551",
//     string measured-frame-rate-1920x1088-range = "409-411",
//     string measured-frame-rate-320x240-range = "528-531",
//     string measured-frame-rate-720x480-range = "550-555",
//     string performance-point-1280x720-range = "240",
//     string performance-point-3840x2160-range = "120",
//     string size-range = "32x32-3840x2160"
// }

static Vector<MediaCodecInfo::ProfileLevel>
        stdVecToVec(std::vector<MediaCodecInfo::ProfileLevel> pls) {
    Vector<MediaCodecInfo::ProfileLevel> res;
    for (MediaCodecInfo::ProfileLevel pl : pls) {
        res.push_back(pl);
    }
    return res;
}

static std::vector<MediaCodecInfoParser::CodecCapabilities> constructCodecCaps() {
    std::vector<MediaCodecInfoParser::CodecCapabilities> ret;

    // Test case 1 : MIMETYPE_AUDIO_AAC

    sp<AMessage> details1 = new AMessage;
    details1->setString("bitrate-range", "8000-960000");
    details1->setString("max-channel-count", "8");
    details1->setString("sample-rate-ranges",
            "7350,8000,11025,12000,16000,22050,24000,32000,44100,48000");

    std::vector<MediaCodecInfo::ProfileLevel> profileLevel1_{
        MediaCodecInfo::ProfileLevel(2, 0),
        MediaCodecInfo::ProfileLevel(5, 0),
        MediaCodecInfo::ProfileLevel(29, 0),
        MediaCodecInfo::ProfileLevel(23, 0),
        MediaCodecInfo::ProfileLevel(39, 0),
        MediaCodecInfo::ProfileLevel(20, 0),
        MediaCodecInfo::ProfileLevel(42, 0),
    };
    Vector<MediaCodecInfo::ProfileLevel> profileLevel1 = stdVecToVec(profileLevel1_);

    Vector<uint32_t> colorFormats1;

    sp<AMessage> defaultFormat1 = new AMessage();
    defaultFormat1->setString(KEY_MIME, MIMETYPE_AUDIO_AAC);

    MediaCodecInfoParser::CodecCapabilities codecCap1 = MediaCodecInfoParser::CodecCapabilities(
            profileLevel1, colorFormats1, false, defaultFormat1, details1);
    ret.push_back(codecCap1);

    // Test case 2 :

    return ret;
}

static void verifyInfoParserResults() {
    std::vector<MediaCodecInfoParser::CodecCapabilities> codecCaps = constructCodecCaps();

    for (MediaCodecInfoParser::CodecCapabilities codecCap : codecCaps) {

        std::shared_ptr<MediaCodecInfoParser::AudioCapabilities>
                audioCap = codecCap.getAudioCapabilities();
        std::shared_ptr<MediaCodecInfoParser::VideoCapabilities>
                videoCap = codecCap.getVideoCapabilities();
        std::shared_ptr<MediaCodecInfoParser::EncoderCapabilities>
                encoderCap = codecCap.getEncoderCapabilities();

        if (audioCap != nullptr) {
            Range<int> bitrateRange = audioCap->getBitrateRange();
            std::vector<int> supportedSampleRates = audioCap->getSupportedSampleRates();
            std::vector<Range<int>> supportedSampleRateRanges
                    = audioCap->getSupportedSampleRateRanges();
            int maxInputChannelCount = audioCap->getMaxInputChannelCount();
            int minInputChannelCount = audioCap->getMinInputChannelCount();
            std::vector<Range<int>> inputChannelCountRanges
                    = audioCap->getInputChannelCountRanges();
        }
    }
}

TEST(CodecInfoParserTest, DeviceDataConstructionTest) {
    sp<IMediaCodecList> list = MediaCodecList::getInstance();
    ASSERT_NE(list, nullptr) << "Unable to Detaget MediaCodecList instance.";

    // verifyInfoParserResults();

    std::vector<CddReq> cddReq{
            // media type, isEncoder
            CddReq(MIMETYPE_AUDIO_AAC, false),
            CddReq(MIMETYPE_AUDIO_AAC, true),

            CddReq(MIMETYPE_VIDEO_AVC, false),
            CddReq(MIMETYPE_VIDEO_HEVC, false),
            CddReq(MIMETYPE_VIDEO_MPEG4, false),
            CddReq(MIMETYPE_VIDEO_VP8, false),
            CddReq(MIMETYPE_VIDEO_VP9, false),
            CddReq(MIMETYPE_VIDEO_AV1, false),

            CddReq(MIMETYPE_VIDEO_AVC, true),
            CddReq(MIMETYPE_VIDEO_HEVC, true),
            CddReq(MIMETYPE_VIDEO_VP8, true),
            CddReq(MIMETYPE_VIDEO_AV1, true),
    };

    for (CddReq codecReq : cddReq) {
        ssize_t index = list->findCodecByType(codecReq.mediaType, codecReq.isEncoder);
        EXPECT_GE(index, 0) << "Wasn't able to find codec for media type: " << codecReq.mediaType
                            << (codecReq.isEncoder ? " encoder" : " decoder");

        sp<MediaCodecInfo> info = list->getCodecInfo(index);
        ASSERT_NE(info, nullptr) << "CodecInfo is null";

        verifyInfoParserResults(info, codecReq.isEncoder);
    }
}

TEST(CodecInfoParserTest, StaticDataConstructionTest) {
    verifyInfoParserResults();
}