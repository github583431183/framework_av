/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ANDROID_IMEDIACODECLIST_H
#define ANDROID_IMEDIACODECLIST_H

#include <utils/Errors.h>  // for status_t
#include <binder/IInterface.h>
#include <binder/Parcel.h>

#include <media/stagefright/foundation/AMessage.h>

namespace android {

struct MediaCodecInfo;

class IMediaCodecList: public IInterface
{
public:
    DECLARE_META_INTERFACE(MediaCodecList);

    virtual size_t countCodecs() const = 0;
    virtual sp<MediaCodecInfo> getCodecInfo(size_t index) const = 0;

    virtual const sp<AMessage> getGlobalSettings() const = 0;

    virtual ssize_t findCodecByType(
            const char *type, bool encoder, size_t startIndex = 0) const = 0;

    virtual ssize_t findCodecByName(const char *name) const = 0;
};

// ----------------------------------------------------------------------------

class BnMediaCodecList: public BnInterface<IMediaCodecList>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

}; // namespace android

#endif // ANDROID_IMEDIACODECLIST_H
