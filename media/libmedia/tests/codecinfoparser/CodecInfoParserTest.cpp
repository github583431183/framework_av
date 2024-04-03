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
        cap->getSupportedColorFormats(&colorFormats);

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

TEST(CodecInfoParserTest, CodecCapabilitiesConstructionTest) {
    sp<IMediaCodecList> list = MediaCodecList::getInstance();
    ASSERT_NE(list, nullptr) << "Unable to get MediaCodecList instance.";

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