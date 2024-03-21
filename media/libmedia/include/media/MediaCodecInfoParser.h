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

#ifndef MEDIA_CODEC_INFO_PARSER_H_

#define MEDIA_CODEC_INFO_PARSER_H_

#include <media/MediaCodecInfo.h>
#include <media/MediaCodecInfoParserUtils.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodecConstants.h>

#include <system/audio.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/StrongPointer.h>

namespace android {

struct AMessage;

struct MediaCodecInfoParser : public RefBase {

    struct CodecCapabilities;

    struct XCapabilitiesBase {
    protected:
        /**
         * Set mError of CodecCapabilities.
         *
         * @param error error code
         */
        void setParentError(int error);

        std::weak_ptr<CodecCapabilities> mParent;
    };

    struct AudioCapabilities : XCapabilitiesBase {

    };

    struct VideoCapabilities : XCapabilitiesBase {

    };

    struct EncoderCapabilities : XCapabilitiesBase {

    };

    struct CodecCapabilities {
    private:
        int mError;

        friend struct XCapabilitiesBase;
    };

};

}  // namespace android

#endif  // MEDIA_CODEC_INFO_PARSER_H_