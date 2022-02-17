/*
 * Copyright (C) 2018 The Android Open Source Project
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
#define LOG_TAG "NetworkUtils"
#include <utils/Log.h>

#include <media/stagefright/rtsp/NetworkUtils.h>

// NetworkUtils implementation for application process.
namespace android {

// static
void NetworkUtils::RegisterSocketUserTag(int, uid_t, uint32_t) {
    // No op. Framework already handles the data usage billing for applications.
}

// static
void NetworkUtils::UnRegisterSocketUserTag(int) {
    // No op.
}

// static
void NetworkUtils::RegisterSocketUserMark(int, uid_t) {
    // No op. Framework already handles the data usage billing for applications.
}

// static
void NetworkUtils::UnRegisterSocketUserMark(int) {
    // No op.
}

}  // namespace android
