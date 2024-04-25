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
#define LOG_TAG "EncoderCapabilities"

#include <android-base/strings.h>

#include <media/CodecCapabilities.h>
#include <media/EncoderCapabilities.h>
#include <media/stagefright/MediaCodecConstants.h>

namespace android {

Range<int> EncoderCapabilities::getQualityRange() {
    return mQualityRange;
}

Range<int> EncoderCapabilities::getComplexityRange() {
    return mComplexityRange;
}

// static
int EncoderCapabilities::ParseBitrateMode(std::string mode) {
    for (Feature feat: Bitrates) {
        if (strcasecmp(feat.mName.c_str(), mode.c_str()) == 0) {
            return feat.mValue;
        }
    }
    return 0;
}

bool EncoderCapabilities::isBitrateModeSupported(int mode) {
    for (Feature feat : Bitrates) {
        if (mode == feat.mValue) {
            return (mBitControl & (1 << mode)) != 0;
        }
    }
    return false;
}

// static
std::shared_ptr<EncoderCapabilities> EncoderCapabilities::Create(std::string mediaType,
        std::vector<ProfileLevel> profLevs, const sp<AMessage> &format) {
    std::shared_ptr<EncoderCapabilities> caps(new EncoderCapabilities());
    caps->init(mediaType, profLevs, format);
    return caps;
}

void EncoderCapabilities::init(std::string mediaType, std::vector<ProfileLevel> profLevs,
        const sp<AMessage> &format) {
    // no support for complexity or quality yet
    mMediaType = mediaType;
    mProfileLevels = profLevs;
    mComplexityRange = Range(0, 0);
    mQualityRange = Range(0, 0);
    mBitControl = (1 << BITRATE_MODE_VBR);

    applyLevelLimits();
    parseFromInfo(format);
}

void EncoderCapabilities::applyLevelLimits() {
    const char* mediaType = mMediaType.c_str();
    if (strcasecmp(mediaType, MIMETYPE_AUDIO_FLAC) == 0) {
        mComplexityRange = Range(0, 8);
        mBitControl = (1 << BITRATE_MODE_CQ);
    } else if (strcasecmp(mediaType, MIMETYPE_AUDIO_AMR_NB) == 0
            || strcasecmp(mediaType, MIMETYPE_AUDIO_AMR_WB) == 0
            || strcasecmp(mediaType, MIMETYPE_AUDIO_G711_ALAW) == 0
            || strcasecmp(mediaType, MIMETYPE_AUDIO_G711_MLAW) == 0
            || strcasecmp(mediaType, MIMETYPE_AUDIO_MSGSM) == 0) {
        mBitControl = (1 << BITRATE_MODE_CBR);
    }
}

void EncoderCapabilities::parseFromInfo(const sp<AMessage> &format) {
    AString complexityRangeAStr;
    if (format->findString("complexity-range", &complexityRangeAStr)) {
        std::optional<Range<int>> complexityRangeOpt
                = ParseIntRange(std::string(complexityRangeAStr.c_str()));
        mComplexityRange = complexityRangeOpt.value_or(mComplexityRange);
        // TODO should we limit this to level limits?
    }
    AString qualityRangeAStr;
    if (format->findString("quality-range", &qualityRangeAStr)) {
        std::optional<Range<int>> qualityRangeOpt
                = ParseIntRange(std::string(qualityRangeAStr.c_str()));
        mQualityRange = qualityRangeOpt.value_or(mQualityRange);
    }
    AString bitrateModesAStr;
    if (format->findString("feature-bitrate-modes", &bitrateModesAStr)) {
        mBitControl = 0;
        for (std::string mode: base::Split(std::string(bitrateModesAStr.c_str()), ",")) {
            mBitControl |= (1 << ParseBitrateMode(mode));
        }
    }
    format->findInt32("complexity-default", &mDefaultComplexity);
    format->findInt32("quality-default", &mDefaultQuality);
    AString qualityScaleAStr;
    if (format->findString("quality-scale", &qualityScaleAStr)) {
        mQualityScale = std::string(qualityScaleAStr.c_str());
    }
}

bool EncoderCapabilities::supports(
        std::optional<int> complexity, std::optional<int> quality, std::optional<int> profile) {
    bool ok = true;
    if (ok && complexity) {
        ok = mComplexityRange.contains(complexity.value());
    }
    if (ok && quality) {
        ok = mQualityRange.contains(quality.value());
    }
    if (ok && profile) {
        for (ProfileLevel pl: mProfileLevels) {
            if (pl.mProfile == profile.value()) {
                profile = std::nullopt;
                break;
            }
        }
        ok = profile == std::nullopt;
    }
    return ok;
}

void EncoderCapabilities::getDefaultFormat(sp<AMessage> &format) {
    // don't list trivial quality/complexity as default for now
    if (mQualityRange.upper() != mQualityRange.lower()
            && mDefaultQuality != 0) {
        format->setInt32(KEY_QUALITY, mDefaultQuality);
    }
    if (mComplexityRange.upper() != mComplexityRange.lower()
            && mDefaultComplexity != 0) {
        format->setInt32(KEY_COMPLEXITY, mDefaultComplexity);
    }
    // bitrates are listed in order of preference
    for (Feature feat : Bitrates) {
        if ((mBitControl & (1 << feat.mValue)) != 0) {
            format->setInt32(KEY_BITRATE_MODE, feat.mValue);
            break;
        }
    }
}

bool EncoderCapabilities::supportsFormat(const sp<AMessage> &format) {
    int32_t mode;
    if (format->findInt32(KEY_BITRATE_MODE, &mode) && !isBitrateModeSupported(mode)) {
        return false;
    }

    std::optional<int> complexityOpt = std::nullopt;
    int complexity;
    if (format->findInt32(KEY_COMPLEXITY, &complexity)) {
        complexityOpt = complexity;
    }
    if (strcasecmp(mMediaType.c_str(), MIMETYPE_AUDIO_FLAC) == 0) {
        int flacComplexity;
        if (format->findInt32(KEY_FLAC_COMPRESSION_LEVEL, &flacComplexity)) {
            if (!complexityOpt) {
                complexityOpt = flacComplexity;
            } else if (flacComplexity != complexityOpt.value()) {
                // CHECK(complexity == flacComplexity);
                ALOGE("conflicting values for complexity and flac-compression-level");
                return false;
            }
        }
    }

    // other audio parameters
    std::optional<int> profileOpt = std::nullopt;
    int profile;
    if (format->findInt32(KEY_PROFILE, &profile)) {
        profileOpt = profile;
    }
    if (strcasecmp(mMediaType.c_str(), MIMETYPE_AUDIO_AAC) == 0) {
        int aacProfile;
        if (format->findInt32(KEY_AAC_PROFILE, &aacProfile)) {
            if (!profileOpt) {
                profileOpt = aacProfile;
            } else if (aacProfile != profileOpt.value()) {
                ALOGE("conflicting values for profile and aac-profile");
                return false;
            }
        }
    }

    std::optional<int> qualityOpt = std::nullopt;
    int quality;
    if (format->findInt32(KEY_QUALITY, &quality)) {
        qualityOpt = quality;
    }

    return supports(complexity, quality, profile);
}

}  // namespace android