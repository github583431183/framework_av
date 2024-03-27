/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef FUZZER_MEDIAMIMETYPES_H_
#define FUZZER_MEDIAMIMETYPES_H_

#include <media/stagefright/foundation/MediaDefs.h>
#include <unordered_map>

namespace android {

static const std::unordered_map<std::string, const char*> decoderToMediaType = {
        {"c2.android.vp8.decoder", MEDIA_MIMETYPE_VIDEO_VP8},
        {"c2.android.vp9.decoder", MEDIA_MIMETYPE_VIDEO_VP9},
        {"c2.android.av1.decoder", MEDIA_MIMETYPE_VIDEO_AV1},
        {"c2.android.avc.decoder", MEDIA_MIMETYPE_VIDEO_AVC},
        {"c2.android.hevc.decoder", MEDIA_MIMETYPE_VIDEO_HEVC},
        {"c2.android.mpeg4.decoder", MEDIA_MIMETYPE_VIDEO_MPEG4},
        {"c2.android.h263.decoder", MEDIA_MIMETYPE_VIDEO_H263}};

}  // namespace android

#endif  // FUZZER_MEDIAMIMETYPES_H_
