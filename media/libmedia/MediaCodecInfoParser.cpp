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
#include <media/MediaCodecInfoParserUtils.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

// XCapabilitiesBase

void MediaCodecInfoParser::XCapabilitiesBase::setParentError(int error) {
    auto lockParent = mParent.lock();
    if (!lockParent) {
        return;
    }
    lockParent->mError |= error;
}

}  // namespace android