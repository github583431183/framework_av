/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef _MTP_UTILS_H
#define _MTP_UTILS_H

#include <stdint.h>

namespace android {

bool parseDateTime(const char* dateTime, time_t& outSeconds);
void formatDateTime(time_t seconds, char* buffer, int bufferLength);

int copyFile(const char *fromPath, const char *toPath);
void deleteRecursive(const char* path);
void deletePath(const char* path);

}; // namespace android

#endif // _MTP_UTILS_H
