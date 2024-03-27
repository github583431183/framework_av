
/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <media/stagefright/MetaData.h>
#include "MediaMimeTypes.h"

namespace android {

constexpr uint8_t kMinKeyHeight = 32;
constexpr uint8_t kMinKeyWidth = 32;
constexpr uint16_t kMaxKeyHeight = 2160;
constexpr uint16_t kMaxKeyWidth = 3840;
size_t gMaxMediaBufferSize = 0;

sp<MetaData> generateMetaData(FuzzedDataProvider* fdp, std::string componentName) {
    sp<MetaData> newMeta = sp<MetaData>::make();

    auto it = decoderToMediaType.find(componentName);
    auto mime = it->second;
    newMeta->setCString(kKeyMIMEType, mime);

    auto height = fdp->ConsumeIntegralInRange<uint16_t>(kMinKeyHeight, kMaxKeyHeight);
    auto width = fdp->ConsumeIntegralInRange<uint16_t>(kMinKeyWidth, kMaxKeyWidth);
    newMeta->setInt32(kKeyHeight, height);
    newMeta->setInt32(kKeyWidth, width);

    gMaxMediaBufferSize = height * width;

    if (fdp->ConsumeBool()) {
        newMeta->setInt32(kKeyTileHeight,
                          fdp->ConsumeIntegralInRange<uint16_t>(kMinKeyHeight, height));
        newMeta->setInt32(kKeyTileWidth,
                          fdp->ConsumeIntegralInRange<uint16_t>(kMinKeyWidth, width));
        newMeta->setInt32(kKeyGridRows, fdp->ConsumeIntegral<uint8_t>());
        newMeta->setInt32(kKeyGridCols, fdp->ConsumeIntegral<uint8_t>());
    }

    if (fdp->ConsumeBool()) {
        newMeta->setInt32(kKeySARHeight, fdp->ConsumeIntegral<uint8_t>());
        newMeta->setInt32(kKeySARWidth, fdp->ConsumeIntegral<uint8_t>());
    }

    if (fdp->ConsumeBool()) {
        newMeta->setInt32(kKeyDisplayHeight,
                          fdp->ConsumeIntegralInRange<uint16_t>(height, UINT16_MAX));
        newMeta->setInt32(kKeyDisplayWidth,
                          fdp->ConsumeIntegralInRange<uint16_t>(width, UINT16_MAX));
    }

    if (fdp->ConsumeBool()) {
        newMeta->setRect(kKeyCropRect, fdp->ConsumeIntegral<int32_t>() /* left */,
                         fdp->ConsumeIntegral<int32_t>() /* top */,
                         fdp->ConsumeIntegral<int32_t>() /* right */,
                         fdp->ConsumeIntegral<int32_t>() /* bottom */);
    }

    if (fdp->ConsumeBool()) {
        newMeta->setInt32(kKeyRotation, fdp->ConsumeIntegralInRange<uint8_t>(0, 3) * 90);
    }

    if (fdp->ConsumeBool()) {
        newMeta->setInt64(kKeyThumbnailTime, fdp->ConsumeIntegral<uint64_t>());
        newMeta->setInt32(kKeyThumbnailHeight, fdp->ConsumeIntegral<uint8_t>());
        newMeta->setInt32(kKeyThumbnailWidth, fdp->ConsumeIntegral<uint8_t>());

        size_t thumbnailSize = fdp->ConsumeIntegral<size_t>();
        std::vector<uint8_t> thumbnailData = fdp->ConsumeBytes<uint8_t>(thumbnailSize);
        if (mime == MEDIA_MIMETYPE_VIDEO_AV1) {
            newMeta->setData(kKeyThumbnailAV1C, fdp->ConsumeIntegral<int32_t>() /* type */,
                             thumbnailData.data(), thumbnailData.size());
        } else {
            newMeta->setData(kKeyThumbnailHVCC, fdp->ConsumeIntegral<int32_t>() /* type */,
                             thumbnailData.data(), thumbnailData.size());
        }
    }

    if (fdp->ConsumeBool()) {
        size_t profileSize = fdp->ConsumeIntegral<size_t>();
        std::vector<uint8_t> profileData = fdp->ConsumeBytes<uint8_t>(profileSize);
        newMeta->setData(kKeyIccProfile, fdp->ConsumeIntegral<int32_t>() /* type */,
                         profileData.data(), profileData.size());
    }

    return newMeta;
}

}  // namespace android
