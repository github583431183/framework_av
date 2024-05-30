
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
#define LOG_TAG "CodecCapabilities"

#include <utils/Log.h>
#include <media/CodecCapabilities.h>
#include <media/CodecCapabilitiesUtils.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

static const int DEFAULT_MAX_SUPPORTED_INSTANCES = 32;
static const int MAX_SUPPORTED_INSTANCES_LIMIT = 256;

// must not contain KEY_PROFILE
static const std::set<std::pair<std::string, AMessage::Type>> AUDIO_LEVEL_CRITICAL_FORMAT_KEYS = {
    // We don't set level-specific limits for audio codecs today. Key candidates would
    // be sample rate, bit rate or channel count.
    // MediaFormat.KEY_SAMPLE_RATE,
    // MediaFormat.KEY_CHANNEL_COUNT,
    // MediaFormat.KEY_BIT_RATE,
    { KEY_MIME, AMessage::kTypeString }
};

// CodecCapabilities Features
static const std::vector<Feature> DecoderFeatures = {
    Feature(FEATURE_AdaptivePlayback, (1 << 0), true),
    Feature(FEATURE_SecurePlayback,   (1 << 1), false),
    Feature(FEATURE_TunneledPlayback, (1 << 2), false),
    Feature(FEATURE_PartialFrame,     (1 << 3), false),
    Feature(FEATURE_FrameParsing,     (1 << 4), false),
    Feature(FEATURE_MultipleFrames,   (1 << 5), false),
    Feature(FEATURE_DynamicTimestamp, (1 << 6), false),
    Feature(FEATURE_LowLatency,       (1 << 7), true),
    // feature to exclude codec from REGULAR codec list
    Feature(FEATURE_SpecialCodec,     (1 << 30), false, true),
};
static const std::vector<Feature> EncoderFeatures = {
    Feature(FEATURE_IntraRefresh, (1 << 0), false),
    Feature(FEATURE_MultipleFrames, (1 << 1), false),
    Feature(FEATURE_DynamicTimestamp, (1 << 2), false),
    Feature(FEATURE_QpBounds, (1 << 3), false),
    Feature(FEATURE_EncodingStatistics, (1 << 4), false),
    Feature(FEATURE_HdrEditing, (1 << 5), false),
    // feature to exclude codec from REGULAR codec list
    Feature(FEATURE_SpecialCodec,     (1 << 30), false, true),
};

// must not contain KEY_PROFILE
static const std::set<std::pair<std::string, AMessage::Type>> VIDEO_LEVEL_CRITICAL_FORMAT_KEYS = {
    { KEY_WIDTH, AMessage::kTypeInt32 },
    { KEY_HEIGHT, AMessage::kTypeInt32 },
    { KEY_FRAME_RATE, AMessage::kTypeInt32 },
    { KEY_BIT_RATE, AMessage::kTypeInt32 },
    { KEY_MIME, AMessage::kTypeString }
};

bool CodecCapabilities::SupportsBitrate(Range<int> bitrateRange,
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

bool CodecCapabilities::isFeatureSupported(const std::string &name) const {
    return checkFeature(name, mFlagsSupported);
}

bool CodecCapabilities::isFeatureRequired(const std::string &name) const {
    return checkFeature(name, mFlagsRequired);
}

std::vector<std::string> CodecCapabilities::validFeatures() const {
    std::vector<Feature> features = getValidFeatures();
    std::vector<std::string> res;
    for (int i = 0; i < res.size(); i++) {
        if (!features.at(i).mInternal) {
            res.push_back(features.at(i).mName);
        }
    }
    return res;
}

std::vector<Feature> CodecCapabilities::getValidFeatures() const {
    if (isEncoder()) {
        return EncoderFeatures;
    } else {
        return DecoderFeatures;
    }
}

bool CodecCapabilities::checkFeature(std::string name, int flags) const {
    for (Feature feat: getValidFeatures()) {
        if (feat.mName == name) {
            return (flags & feat.mValue) != 0;
        }
    }
    return false;
}

bool CodecCapabilities::isRegular() const {
    // regular codecs only require default features
    for (Feature feat: getValidFeatures()) {
        if (!feat.mDefault && isFeatureRequired(feat.mName)) {
            return false;
        }
    }
    return true;
}

bool CodecCapabilities::isFormatSupported(const sp<AMessage> &format) const {
    AString mediaType;
    // mediaType must match if present
    if (format->findString(KEY_MIME, &mediaType) && !mMediaType.equalsIgnoreCase(mediaType)) {
        return false;
    }

    // check feature support
    for (Feature feat: getValidFeatures()) {
        if (feat.mInternal) {
            continue;
        }

        int yesNo;
        std::string key = KEY_FEATURE_;
        key = key + feat.mName;
        if (format->findInt32(key.c_str(), &yesNo)) {
            continue;
        }
        if ((yesNo == 1 && !isFeatureSupported(feat.mName)) ||
                (yesNo == 0 && isFeatureRequired(feat.mName))) {
            return false;
        }
    }

    int profile;
    if (format->findInt32(KEY_PROFILE, &profile)) {
        int level = -1;
        format->findInt32(KEY_LEVEL, &level);
        if (!supportsProfileLevel(profile, level)) {
            return false;
        }

        // If we recognize this profile, check that this format is supported by the
        // highest level supported by the codec for that profile. (Ignore specified
        // level beyond the above profile/level check as level is only used as a
        // guidance. E.g. AVC Level 1 CIF format is supported if codec supports level 1.1
        // even though max size for Level 1 is QCIF. However, MPEG2 Simple Profile
        // 1080p format is not supported even if codec supports Main Profile Level High,
        // as Simple Profile does not support 1080p.
        int maxLevel = 0;
        for (ProfileLevel pl : mProfileLevels) {
            if (pl.mProfile == profile && pl.mLevel > maxLevel) {
                // H.263 levels are not completely ordered:
                // Level45 support only implies Level10 support
                if (!mMediaType.equalsIgnoreCase(MIMETYPE_VIDEO_H263)
                        || pl.mLevel != H263Level45
                        || maxLevel == H263Level10) {
                    maxLevel = pl.mLevel;
                }
            }
        }
        std::shared_ptr<CodecCapabilities> levelCaps
                = CreateFromProfileLevel(mMediaType, profile, maxLevel);
        // We must remove the profile from this format otherwise levelCaps.isFormatSupported
        // will get into this same condition and loop forever. Furthermore, since levelCaps
        // does not contain features and bitrate specific keys, keep only keys relevant for
        // a level check.
        sp<AMessage> levelCriticalFormat = new AMessage;

        // critical keys will always contain KEY_MIME, but should also contain others to be
        // meaningful
        if ((isVideo() || isAudio()) && levelCaps != nullptr) {
            const std::set<std::pair<std::string, AMessage::Type>> criticalKeys =
                isVideo() ? VIDEO_LEVEL_CRITICAL_FORMAT_KEYS : AUDIO_LEVEL_CRITICAL_FORMAT_KEYS;
            for (std::pair<std::string, AMessage::Type> key : criticalKeys) {
                if (format->contains(key.first.c_str())) {
                    // AMessage::ItemData value = format->findItem(key.c_str());
                    // levelCriticalFormat->setItem(key.c_str(), value);
                    switch (key.second) {
                        case AMessage::kTypeInt32: {
                            int32_t value;
                            format->findInt32(key.first.c_str(), &value);
                            levelCriticalFormat->setInt32(key.first.c_str(), value);
                            break;
                        }
                        case AMessage::kTypeString: {
                            AString value;
                            format->findString(key.first.c_str(), &value);
                            levelCriticalFormat->setString(key.first.c_str(), value);
                            break;
                        }
                        default:
                            ALOGE("Unsupported type");
                    }
                }
            }
            if (!levelCaps->isFormatSupported(levelCriticalFormat)) {
                return false;
            }
        }
    }
    if (mAudioCaps && !mAudioCaps->supportsFormat(format)) {
        return false;
    }
    if (mVideoCaps && !mVideoCaps->supportsFormat(format)) {
        return false;
    }
    if (mEncoderCaps && !mEncoderCaps->supportsFormat(format)) {
        return false;
    }
    return true;
}

bool CodecCapabilities::supportsProfileLevel(int profile, int level) const {
    for (ProfileLevel pl: mProfileLevels) {
        if (pl.mProfile != profile) {
            continue;
        }

        // No specific level requested
        if (level == -1) {
            return true;
        }

        // AAC doesn't use levels
        if (mMediaType.equalsIgnoreCase(MIMETYPE_AUDIO_AAC)) {
            return true;
        }

        // DTS doesn't use levels
        if (mMediaType.equalsIgnoreCase(MIMETYPE_AUDIO_DTS)
                || mMediaType.equalsIgnoreCase(MIMETYPE_AUDIO_DTS_HD)
                || mMediaType.equalsIgnoreCase(MIMETYPE_AUDIO_DTS_UHD)) {
            return true;
        }

        // H.263 levels are not completely ordered:
        // Level45 support only implies Level10 support
        if (mMediaType.equalsIgnoreCase(MIMETYPE_VIDEO_H263)) {
            if (pl.mLevel != level && pl.mLevel == H263Level45
                    && level > H263Level10) {
                continue;
            }
        }

        // MPEG4 levels are not completely ordered:
        // Level1 support only implies Level0 (and not Level0b) support
        if (mMediaType.equalsIgnoreCase(MIMETYPE_VIDEO_MPEG4)) {
            if (pl.mLevel != level && pl.mLevel == MPEG4Level1
                    && level > MPEG4Level0) {
                continue;
            }
        }

        // HEVC levels incorporate both tiers and levels. Verify tier support.
        if (mMediaType.equalsIgnoreCase(MIMETYPE_VIDEO_HEVC)) {
            bool supportsHighTier =
                (pl.mLevel & HEVCHighTierLevels) != 0;
            bool checkingHighTier = (level & HEVCHighTierLevels) != 0;
            // high tier levels are only supported by other high tier levels
            if (checkingHighTier && !supportsHighTier) {
                continue;
            }
        }

        if (pl.mLevel >= level) {
            // if we recognize the listed profile/level, we must also recognize the
            // profile/level arguments.
            if (CreateFromProfileLevel(mMediaType, profile, pl.mLevel) != nullptr) {
                return CreateFromProfileLevel(mMediaType, profile, level) != nullptr;
            }
            return true;
        }
    }
    return false;
 }

sp<AMessage> CodecCapabilities::getDefaultFormat() const {
    return mDefaultFormat;
}

AString CodecCapabilities::getMediaType() {
    return mMediaType;
}

std::vector<ProfileLevel> CodecCapabilities::getProfileLevels() {
    return mProfileLevels;
}

std::vector<uint32_t> CodecCapabilities::getColorFormats() const {
    return mColorFormats;
}

int CodecCapabilities::getMaxSupportedInstances() const {
    return mMaxSupportedInstances;
}

bool CodecCapabilities::isAudio() const {
    return mAudioCaps != nullptr;
}

std::shared_ptr<AudioCapabilities>
        CodecCapabilities::getAudioCapabilities() const {
    return mAudioCaps;
}

bool CodecCapabilities::isEncoder() const {
    return mEncoderCaps != nullptr;
}

std::shared_ptr<EncoderCapabilities>
        CodecCapabilities::getEncoderCapabilities() const {
    return mEncoderCaps;
}

bool CodecCapabilities::isVideo() const {
    return mVideoCaps != nullptr;
}

std::shared_ptr<VideoCapabilities> CodecCapabilities::getVideoCapabilities() const {
    return mVideoCaps;
}

CodecCapabilities CodecCapabilities::dup() {
    CodecCapabilities caps = CodecCapabilities();

    // profileLevels and colorFormats may be modified by client.
    caps.mProfileLevels = mProfileLevels;
    caps.mColorFormats = mColorFormats;

    caps.mMediaType = mMediaType;
    caps.mMaxSupportedInstances = mMaxSupportedInstances;
    caps.mFlagsRequired = mFlagsRequired;
    caps.mFlagsSupported = mFlagsSupported;
    caps.mFlagsVerified = mFlagsVerified;
    caps.mAudioCaps = mAudioCaps;
    caps.mVideoCaps = mVideoCaps;
    caps.mEncoderCaps = mEncoderCaps;
    caps.mDefaultFormat = mDefaultFormat;
    caps.mCapabilitiesInfo = mCapabilitiesInfo;

    return caps;
 }

// static
std::shared_ptr<CodecCapabilities> CodecCapabilities::CreateFromProfileLevel(
        AString mediaType, int profile, int level, int32_t maxConcurrentInstances) {
    ProfileLevel pl;
    pl.mProfile = profile;
    pl.mLevel = level;
    sp<AMessage> defaultFormat = new AMessage;
    defaultFormat->setString(KEY_MIME, mediaType);

    std::vector<ProfileLevel> pls;
    pls.push_back(pl);
    std::vector<uint32_t> colFmts;
    sp<AMessage> capabilitiesInfo = new AMessage;
    std::shared_ptr<CodecCapabilities> ret(new CodecCapabilities());
    ret->init(pls, colFmts, true /* encoder */, defaultFormat, capabilitiesInfo,
            maxConcurrentInstances);
    if (ret->mError != 0) {
        return nullptr;
    }
    return ret;
}

void CodecCapabilities::init(std::vector<ProfileLevel> profLevs, std::vector<uint32_t> colFmts,
        bool encoder, sp<AMessage> &defaultFormat, sp<AMessage> &capabilitiesInfo,
        int32_t maxConcurrentInstances) {
    mColorFormats = colFmts;
    mFlagsVerified = 0; // TODO: remove as it is unused
    mDefaultFormat = defaultFormat;
    mCapabilitiesInfo = capabilitiesInfo;
    AString mediaTypeAStr;
    mDefaultFormat->findString(KEY_MIME, &mediaTypeAStr);
    mMediaType = mediaTypeAStr.c_str();

    /* VP9 introduced profiles around 2016, so some VP9 codecs may not advertise any
       supported profiles. Determine the level for them using the info they provide. */
    if (profLevs.size() == 0 && mMediaType == MIMETYPE_VIDEO_VP9) {
        ProfileLevel profLev;
        profLev.mProfile = VP9Profile0;
        profLev.mLevel = VideoCapabilities::EquivalentVP9Level(capabilitiesInfo);
        profLevs.push_back(profLev);
    }
    mProfileLevels = profLevs;

    if (mMediaType.startsWithIgnoreCase("audio/")) {
        mAudioCaps = AudioCapabilities::Create(capabilitiesInfo, shared_from_this());
        mAudioCaps->getDefaultFormat(mDefaultFormat);
    } else if (mMediaType.startsWithIgnoreCase("video/")
            || mMediaType.equalsIgnoreCase(MIMETYPE_IMAGE_ANDROID_HEIC)) {
        mVideoCaps = VideoCapabilities::Create(capabilitiesInfo, shared_from_this());
    }

    if (encoder) {
        mEncoderCaps = EncoderCapabilities::Create(capabilitiesInfo, shared_from_this());
        mEncoderCaps->getDefaultFormat(mDefaultFormat);
    }

    mMaxSupportedInstances = maxConcurrentInstances > 0
            ? maxConcurrentInstances : DEFAULT_MAX_SUPPORTED_INSTANCES;

    int maxInstances = mMaxSupportedInstances;
    capabilitiesInfo->findInt32("max-concurrent-instances", &maxInstances);
    mMaxSupportedInstances =
            Range(1, MAX_SUPPORTED_INSTANCES_LIMIT).clamp(maxInstances);

    for (Feature feat: getValidFeatures()) {
        std::string key = KEY_FEATURE_;
        key = key + feat.mName;
        int yesNo;
        if (!capabilitiesInfo->findInt32(key.c_str(), &yesNo)) {
            continue;
        }
        if (yesNo > 0) {
            mFlagsRequired |= feat.mValue;
        }
        mFlagsSupported |= feat.mValue;
        if (!feat.mInternal) {
            mDefaultFormat->setInt32(key.c_str(), 1);
        }
        // TODO restrict features by mFlagsVerified once all codecs reliably verify them
    }
}

}  // namespace android