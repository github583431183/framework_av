/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecInfoParser"

#include <android-base/strings.h>
#include <android-base/properties.h>
#include <utils/Log.h>

#include <media/MediaCodecInfoParser.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

static const Range<int> POSITIVE_INTEGERS = Range<int>(1, INT_MAX);
static const Range<long> POSITIVE_LONGS = Range(1L, LONG_MAX);
static const Range<int> BITRATE_RANGE = Range<int>(0, 500000000);
static const Range<int> FRAME_RATE_RANGE = Range<int>(0, 960);
static const Range<Rational> POSITIVE_RATIONALS =
            Range<Rational>(Rational(1, INT_MAX), Rational(INT_MAX, 1));

// found stuff that is not supported by framework (=> this should not happen)
static const int ERROR_UNRECOGNIZED   = (1 << 0);
// found profile/level for which we don't have capability estimates
static const int ERROR_UNSUPPORTED    = (1 << 1);
// have not found any profile/level for which we don't have capability estimate
// static const int ERROR_NONE_SUPPORTED = (1 << 2);

/** 480p 24fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_24
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 480, 24);
/** 576p 25fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_25
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 576, 25);
/** 480p 30fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_30
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 480, 30);
/** 480p 48fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_48
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 480, 48);
/** 576p 50fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_50
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 576, 50);
/** 480p 60fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint SD_60
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(720, 480, 60);

/** 720p 24fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_24
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 24);
/** 720p 25fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_25
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 25);
/** 720p 30fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_30
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 30);
/** 720p 50fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_50
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 50);
/** 720p 60fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_60
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 60);
/** 720p 100fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_100
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 100);
/** 720p 120fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_120
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 120);
/** 720p 200fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_200
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 200);
/** 720p 240fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint HD_240
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1280, 720, 240);

/** 1080p 24fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_24
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 24);
/** 1080p 25fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_25
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 25);
/** 1080p 30fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_30
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 30);
/** 1080p 50fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_50
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 50);
/** 1080p 60fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_60
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 60);
/** 1080p 100fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_100
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 100);
/** 1080p 120fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_120
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 120);
/** 1080p 200fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_200
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 200);
/** 1080p 240fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint FHD_240
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(1920, 1080, 240);

/** 2160p 24fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_24
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 24);
/** 2160p 25fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_25
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 25);
/** 2160p 30fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_30
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 30);
/** 2160p 50fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_50
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 50);
/** 2160p 60fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_60
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 60);
/** 2160p 100fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_100
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 100);
/** 2160p 120fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_120
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 120);
/** 2160p 200fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_200
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 200);
/** 2160p 240fps */
static const MediaCodecInfo::VideoCapabilities::PerformancePoint UHD_240
        = MediaCodecInfo::VideoCapabilities::PerformancePoint(3840, 2160, 240);

// static
Range<int> MediaCodecInfo::GetSizeRange() {
#ifdef __LP64__
    return Range<int>(1, 32768);
#else
    std::string valueStr = base::GetProperty("media.resolution.limit.32bit", "4096");
    int value = std::atoi(valueStr.c_str());
    return Range<int>(1, value);
#endif
}

// static
void MediaCodecInfo::CheckPowerOfTwo(int value) {
    CHECK((value & (value - 1)) == 0);
}

void MediaCodecInfoParser::XCapabilitiesBase::setParentError(int error) {
    auto lockParent = mParent.lock();
    if (!lockParent) {
        return;
    }
    lockParent->mError |= error;
}

// AudioCapabilities

Range<int> MediaCodecInfo::AudioCapabilities::getBitrateRange() const {
    return mBitrateRange;
}

std::vector<int> MediaCodecInfo::AudioCapabilities::getSupportedSampleRates() const {
    return mSampleRates;
}

std::vector<Range<int>> MediaCodecInfo::AudioCapabilities::getSupportedSampleRateRanges() const {
    return mSampleRateRanges;
}

int MediaCodecInfo::AudioCapabilities::getMaxInputChannelCount() const {
    int overall_max = 0;
    for (int i = mInputChannelRanges.size() - 1; i >= 0; i--) {
        int lmax = mInputChannelRanges[i].upper();
        if (lmax > overall_max) {
            overall_max = lmax;
        }
    }
    return overall_max;
}

int MediaCodecInfo::AudioCapabilities::getMinInputChannelCount() const {
    int overall_min = MAX_INPUT_CHANNEL_COUNT;
    for (int i = mInputChannelRanges.size() - 1; i >= 0; i--) {
        int lmin = mInputChannelRanges[i].lower();
        if (lmin < overall_min) {
            overall_min = lmin;
        }
    }
    return overall_min;
}

std::vector<Range<int>> MediaCodecInfo::AudioCapabilities::getInputChannelCountRanges() const {
    return mInputChannelRanges;
}

// static
std::shared_ptr<MediaCodecInfo::AudioCapabilities> MediaCodecInfo::AudioCapabilities::Create(
        const sp<AMessage> &format, CodecCapabilities &parent) {
    std::shared_ptr<AudioCapabilities> caps(new AudioCapabilities());
    caps->init(format, parent);
    return caps;
}

void MediaCodecInfo::AudioCapabilities::init(const sp<AMessage> &format,
        CodecCapabilities &parent) {
    mParent = std::make_shared<CodecCapabilities>(parent);
    initWithPlatformLimits();
    applyLevelLimits();
    parseFromInfo(format);
}

void MediaCodecInfo::AudioCapabilities::initWithPlatformLimits() {
    mBitrateRange = Range<int>(0, INT_MAX);
    mInputChannelRanges.push_back(Range<int>(1, MAX_INPUT_CHANNEL_COUNT));

    const int minSampleRate = base::GetIntProperty("ro.mediacodec.min_sample_rate", 7350);
    const int maxSampleRate = base::GetIntProperty("ro.mediacodec.max_sample_rate", 192000);
    mSampleRateRanges.push_back(Range<int>(minSampleRate, maxSampleRate));
}

bool MediaCodecInfo::AudioCapabilities::supports(int sampleRate, int inputChannels) {
    // channels and sample rates are checked orthogonally
    return std::any_of(mInputChannelRanges.begin(), mInputChannelRanges.end(),
            [inputChannels](Range<int> a) { return a.contains(inputChannels); })
            && std::any_of(mSampleRateRanges.begin(), mSampleRateRanges.end(),
            [sampleRate](Range<int> a) { return a.contains(sampleRate); });
}

bool MediaCodecInfo::AudioCapabilities::isSampleRateSupported(int sampleRate) {
    return supports(sampleRate, 0);
}

void MediaCodecInfo::AudioCapabilities::limitSampleRates(const std::vector<int> &rates) {
    for (int rate : rates) {
        if (supports(rate, 0 /* channels */)) {
            mSampleRateRanges.push_back(Range<int>(rate, rate));
        }
    }
    createDiscreteSampleRates();
}

void MediaCodecInfo::AudioCapabilities::createDiscreteSampleRates() {
    for (int i = 0; i < mSampleRateRanges.size(); i++) {
        mSampleRates.push_back(mSampleRateRanges[i].lower());
    }
}

void MediaCodecInfo::AudioCapabilities::limitSampleRates(
        std::vector<Range<int>> &rateRanges) {
    sortDistinctRanges(rateRanges);
    mSampleRateRanges = intersectSortedDistinctRanges(mSampleRateRanges, rateRanges);
    // check if all values are discrete
    for (Range<int> range: mSampleRateRanges) {
        if (range.lower() != range.upper()) {
            mSampleRates.clear();
            return;
        }
    }
    createDiscreteSampleRates();
}

void MediaCodecInfo::AudioCapabilities::applyLevelLimits() {
    std::vector<int> sampleRates;
    std::optional<Range<int>> sampleRateRange;
    std::optional<Range<int>> bitRates;
    int maxChannels = MAX_INPUT_CHANNEL_COUNT;

    auto lockParent = mParent.lock();
    if (!lockParent) {
        return;
    }
    std::vector<ProfileLevel> profileLevels = lockParent->getProfileLevels();
    AString mediaTypeStr = lockParent->getMediaType();
    const char *mediaType = mediaTypeStr.c_str();

    if (strcasecmp(mediaType, MIMETYPE_AUDIO_MPEG) == 0) {
        sampleRates = {
                8000, 11025, 12000,
                16000, 22050, 24000,
                32000, 44100, 48000 };
        bitRates = Range<int>(8000, 320000);
        maxChannels = 2;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AMR_NB) == 0) {
        sampleRates = { 8000 };
        bitRates = Range<int>(4750, 12200);
        maxChannels = 1;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AMR_WB) == 0) {
        sampleRates = { 16000 };
        bitRates = Range<int>(6600, 23850);
        maxChannels = 1;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AAC) == 0) {
        sampleRates = {
                7350, 8000,
                11025, 12000, 16000,
                22050, 24000, 32000,
                44100, 48000, 64000,
                88200, 96000 };
        bitRates = Range<int>(8000, 510000);
        maxChannels = 48;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_VORBIS) == 0) {
        bitRates = Range<int>(32000, 500000);
        sampleRateRange = Range<int>(8000, 192000);
        maxChannels = 255;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_OPUS) == 0) {
        bitRates = Range<int>(6000, 510000);
        sampleRates = { 8000, 12000, 16000, 24000, 48000 };
        maxChannels = 255;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_RAW) == 0) {
        sampleRateRange = Range<int>(1, 192000);
        bitRates = Range<int>(1, 10000000);
        maxChannels = MAX_NUM_CHANNELS;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_FLAC) == 0) {
        sampleRateRange = Range<int>(1, 655350);
        // lossless codec, so bitrate is ignored
        maxChannels = 255;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_G711_ALAW) == 0
            || strcasecmp(mediaType, MIMETYPE_AUDIO_G711_MLAW) == 0) {
        sampleRates = { 8000 };
        bitRates = Range<int>(64000, 64000);
        // platform allows multiple channels for this format
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_MSGSM) == 0) {
        sampleRates = { 8000 };
        bitRates = Range<int>(13000, 13000);
        maxChannels = 1;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AC3) == 0) {
        maxChannels = 6;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_EAC3) == 0) {
        maxChannels = 16;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_EAC3_JOC) == 0) {
        sampleRates = { 48000 };
        bitRates = Range<int>(32000, 6144000);
        maxChannels = 16;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AC4) == 0) {
        sampleRates = { 44100, 48000, 96000, 192000 };
        bitRates = Range<int>(16000, 2688000);
        maxChannels = 24;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_DTS) == 0) {
        sampleRates = { 44100, 48000 };
        bitRates = Range<int>(96000, 1524000);
        maxChannels = 6;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_DTS_HD) == 0) {
        for (ProfileLevel profileLevel: profileLevels) {
            switch (profileLevel.mProfile) {
                case DTS_HDProfileLBR:
                    sampleRates = { 22050, 24000, 44100, 48000 };
                    bitRates = Range<int>(32000, 768000);
                    break;
                case DTS_HDProfileHRA:
                case DTS_HDProfileMA:
                    sampleRates = { 44100, 48000, 88200, 96000, 176400, 192000 };
                    bitRates = Range<int>(96000, 24500000);
                    break;
                default:
                    ALOGW("Unrecognized profile %d for %s", profileLevel.mProfile, mediaType);
                    setParentError(ERROR_UNRECOGNIZED);
                    sampleRates = { 44100, 48000, 88200, 96000, 176400, 192000 };
                    bitRates = Range<int>(96000, 24500000);
            }
        }
        maxChannels = 8;
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_DTS_UHD) == 0) {
        for (ProfileLevel profileLevel: profileLevels) {
            switch (profileLevel.mProfile) {
                case DTS_UHDProfileP2:
                    sampleRates = { 48000 };
                    bitRates = Range<int>(96000, 768000);
                    maxChannels = 10;
                    break;
                case DTS_UHDProfileP1:
                    sampleRates = { 44100, 48000, 88200, 96000, 176400, 192000 };
                    bitRates = Range<int>(96000, 24500000);
                    maxChannels = 32;
                    break;
                default:
                    ALOGW("Unrecognized profile %d for %s", profileLevel.mProfile, mediaType);
                    setParentError(ERROR_UNRECOGNIZED);
                    sampleRates = { 44100, 48000, 88200, 96000, 176400, 192000 };
                    bitRates = Range<int>(96000, 24500000);
                    maxChannels = 32;
            }
        }
    } else {
        ALOGW("Unsupported mediaType %s", mediaType);
        setParentError(ERROR_UNSUPPORTED);
    }

    // restrict ranges
    if (!sampleRates.empty()) {
        limitSampleRates(sampleRates);
    } else if (sampleRateRange) {
        std::vector<Range<int>> rateRanges = { sampleRateRange.value() };
        limitSampleRates(rateRanges);
    }

    Range<int> channelRange = Range<int>(1, maxChannels);
    std::vector<Range<int>> inputChannels = { channelRange };
    applyLimits(inputChannels, bitRates);
}

void MediaCodecInfo::AudioCapabilities::applyLimits(
        const std::vector<Range<int>> &inputChannels,
        const std::optional<Range<int>> &bitRates) {
    // clamp & make a local copy
    std::vector<Range<int>> myInputChannels(inputChannels.size());
    for (int i = 0; i < inputChannels.size(); i++) {
        int lower = inputChannels[i].clamp(1);
        int upper = inputChannels[i].clamp(MAX_INPUT_CHANNEL_COUNT);
        myInputChannels[i] = Range<int>(lower, upper);
    }

    // sort, intersect with existing, & save channel list
    sortDistinctRanges(myInputChannels);
    mInputChannelRanges = intersectSortedDistinctRanges(myInputChannels, mInputChannelRanges);

    if (bitRates) {
        mBitrateRange = mBitrateRange.intersect(bitRates.value());
    }
}

void MediaCodecInfo::AudioCapabilities::parseFromInfo(const sp<AMessage> &format) {
    int maxInputChannels = MAX_INPUT_CHANNEL_COUNT;
    std::vector<Range<int>> channels = { Range<int>(1, maxInputChannels) };
    std::optional<Range<int>> bitRates = POSITIVE_INTEGERS;

    AString rateAString;
    if (format->findString("sample-rate-ranges", &rateAString)) {
        std::vector<std::string> rateStrings = base::Split(std::string(rateAString.c_str()), ",");
        std::vector<Range<int>> rateRanges(rateStrings.size());
        for (std::string rateString : rateStrings) {
            std::optional<Range<int>> rateRange = ParseIntRange(rateString);
            if (!rateRange) {
                continue;
            }
            rateRanges.push_back(rateRange.value());
        }
        limitSampleRates(rateRanges);
    }

    // we will prefer channel-ranges over max-channel-count
    AString aStr;
    if (format->findString("channel-ranges", &aStr)) {
        std::vector<std::string> channelStrings = base::Split(std::string(aStr.c_str()), ",");
        std::vector<Range<int>> channelRanges(channelStrings.size());
        for (std::string channelString : channelStrings) {
            std::optional<Range<int>> channelRange = ParseIntRange(channelString);
            if (!channelRange) {
                continue;
            }
            channelRanges.push_back(channelRange.value());
        }
        channels = channelRanges;
    } else if (format->findString("channel-range", &aStr)) {
        std::optional<Range<int>> oneRange = ParseIntRange(std::string(aStr.c_str()));
        if (oneRange) {
            channels = { oneRange.value() };
        }
    } else if (format->findString("max-channel-count", &aStr)) {
        maxInputChannels = std::atoi(aStr.c_str());
        if (maxInputChannels == 0) {
            channels = { Range<int>(0, 0) };
        } else {
            channels = { Range<int>(1, maxInputChannels) };
        }
    } else if (auto lockParent = mParent.lock()) {
        if ((lockParent->mError & ERROR_UNSUPPORTED) != 0) {
            maxInputChannels = 0;
            channels = { Range<int>(0, 0) };
        }
    }

    if (format->findString("bitrate-range", &aStr)) {
        std::optional<Range<int>> parsedBitrate = ParseIntRange(aStr.c_str());
        if (parsedBitrate) {
            bitRates = bitRates.value().intersect(parsedBitrate.value());
        }
    }

    applyLimits(channels, bitRates);
}

void MediaCodecInfo::AudioCapabilities::getDefaultFormat(sp<AMessage> &format) {
    // report settings that have only a single choice
    if (mBitrateRange.lower() == mBitrateRange.upper()) {
        format->setInt32(KEY_BIT_RATE, mBitrateRange.lower());
    }
    if (getMaxInputChannelCount() == 1) {
        // mono-only format
        format->setInt32(KEY_CHANNEL_COUNT, 1);
    }
    if (!mSampleRates.empty() && mSampleRates.size() == 1) {
        format->setInt32(KEY_SAMPLE_RATE, mSampleRates[0]);
    }
}

bool MediaCodecInfo::AudioCapabilities::supportsFormat(const sp<AMessage> &format) {
    int32_t sampleRate;
    format->findInt32(KEY_SAMPLE_RATE, &sampleRate);
    int32_t channels;
    format->findInt32(KEY_CHANNEL_COUNT, &channels);

    if (!supports(sampleRate, channels)) {
        return false;
    }

    if (!CodecCapabilities::SupportsBitrate(mBitrateRange, format)) {
        return false;
    }

    // nothing to do for:
    // KEY_CHANNEL_MASK: codecs don't get this
    // KEY_IS_ADTS:      required feature for all AAC decoders
    return true;
}

bool MediaCodecInfo::CodecCapabilities::SupportsBitrate(Range<int> bitrateRange,
        const sp<AMessage> &format) {
    // consider max bitrate over average bitrate for support
    int32_t maxBitrate = 0;
    format->findInt32(KEY_MAX_BIT_RATE, &maxBitrate);
    int32_t bitrate = 0;
    format->findInt32(KEY_BIT_RATE, &bitrate);

    if (bitrate == 0) {
        bitrate = maxBitrate;
    } else if (maxBitrate != 0) {
        bitrate = std::max(bitrate, maxBitrate);
    }

    if (bitrate > 0) {
        return bitrateRange.contains(bitrate);
    }

    return true;
}

AString MediaCodecInfo::CodecCapabilities::getMediaType() {
    return mMediaType;
}

std::vector<MediaCodecInfo::ProfileLevel> MediaCodecInfo::CodecCapabilities::getProfileLevels() {
    return mProfileLevels;
}

}  // namespace android