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

#ifndef CODEC_CAPABILITIES_H_

#define CODEC_CAPABILITIES_H_

#include <media/AudioCapabilities.h>
#include <media/CodecCapabilitiesUtils.h>
#include <media/EncoderCapabilities.h>
#include <media/VideoCapabilities.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodecConstants.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/StrongPointer.h>

namespace android {

struct CodecCapabilities : public std::enable_shared_from_this<CodecCapabilities> {

    static bool SupportsBitrate(Range<int> bitrateRange,
            const sp<AMessage> &format);

    /**
     * Returns the media type for which this codec-capability object was created.
     */
    AString getMediaType();

    /**
     * Returns the supported profile levels.
     */
    std::vector<ProfileLevel> getProfileLevels();

private:
    AString mMediaType;
    std::vector<ProfileLevel> mProfileLevels;
    int mError;

    std::shared_ptr<AudioCapabilities> mAudioCaps;
    std::shared_ptr<VideoCapabilities> mVideoCaps;
    std::shared_ptr<EncoderCapabilities> mEncoderCaps;

    friend struct XCapabilitiesBase;
    friend struct AudioCapabilities;
    friend struct VideoCapabilities;
    friend struct EncoderCapabilities;
};

}  // namespace android

#endif // CODEC_CAPABILITIES_H_