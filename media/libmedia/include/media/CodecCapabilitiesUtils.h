/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef CODEC_CAPABILITIES__UTILS_H_

#define CODEC_CAPABILITIES__UTILS_H_

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <utils/Log.h>

#include <media/stagefright/foundation/AUtils.h>

namespace android {

struct ProfileLevel {
    uint32_t mProfile;
    uint32_t mLevel;
    bool operator <(const ProfileLevel &o) const {
        return mProfile < o.mProfile || (mProfile == o.mProfile && mLevel < o.mLevel);
    }
};

}  // namespace android

#endif  // CODEC_CAPABILITIES__UTILS_H_