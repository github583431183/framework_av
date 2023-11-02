/*
 * Copyright 2023 The Android Open Source Project
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

#ifndef MEDIAFLAGS_AMEDIAFLAGS_H_
#define MEDIAFLAGS_AMEDIAFLAGS_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Returns true iff Codec2 AIDL is enabled on this device.
 */
bool AMediaFlags_isCodec2AidlEnabled() __INTRODUCED_IN(__ANDROID_API_V__);

__END_DECLS

#endif  // MEDIAFLAGS_AMEDIAFLAGS_H_
