/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "EffectsFactoryHalHidl"
//#define LOG_NDEBUG 0

#include <cutils/native_handle.h>

#include "ConversionHelperHidl.h"
#include "EffectBufferHalHidl.h"
#include "EffectHalHidl.h"
#include "EffectsFactoryHalHidl.h"
#include "HidlUtils.h"
#include <libaudiohal/FactoryHalHidl.h>

using ::android::hardware::audio::common::CPP_VERSION::implementation::HidlUtils;
using ::android::hardware::Return;

namespace android {
namespace effect {
namespace CPP_VERSION {

using namespace ::android::hardware::audio::common::CPP_VERSION;
using namespace ::android::hardware::audio::effect::CPP_VERSION;

EffectsFactoryHalHidl::EffectsFactoryHalHidl(sp<IEffectsFactory> effectsFactory)
        : ConversionHelperHidl("EffectsFactory") {
    ALOG_ASSERT(effectsFactory != nullptr, "Provided IDevicesFactory service is NULL");
    mEffectsFactory = effectsFactory;
}

status_t EffectsFactoryHalHidl::queryAllDescriptors() {
    if (mEffectsFactory == 0) return NO_INIT;
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mEffectsFactory->getAllDescriptors(
            [&](Result r, const hidl_vec<EffectDescriptor>& result) {
                retval = r;
                if (retval == Result::OK) {
                    mLastDescriptors = result;
                }
            });
    if (ret.isOk()) {
        return retval == Result::OK ? OK : NO_INIT;
    }
    mLastDescriptors.resize(0);
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::queryNumberEffects(uint32_t *pNumEffects) {
    status_t queryResult = queryAllDescriptors();
    if (queryResult == OK) {
        *pNumEffects = mLastDescriptors.size();
    }
    return queryResult;
}

status_t EffectsFactoryHalHidl::getDescriptor(
        uint32_t index, effect_descriptor_t *pDescriptor) {
    // TODO: We need somehow to track the changes on the server side
    // or figure out how to convert everybody to query all the descriptors at once.
    // TODO: check for nullptr
    if (mLastDescriptors.size() == 0) {
        status_t queryResult = queryAllDescriptors();
        if (queryResult != OK) return queryResult;
    }
    if (index >= mLastDescriptors.size()) return NAME_NOT_FOUND;
    EffectHalHidl::effectDescriptorToHal(mLastDescriptors[index], pDescriptor);
    return OK;
}

status_t EffectsFactoryHalHidl::getDescriptor(
        const effect_uuid_t *pEffectUuid, effect_descriptor_t *pDescriptor) {
    // TODO: check for nullptr
    if (mEffectsFactory == 0) return NO_INIT;
    Uuid hidlUuid;
    HidlUtils::uuidFromHal(*pEffectUuid, &hidlUuid);
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mEffectsFactory->getDescriptor(hidlUuid,
            [&](Result r, const EffectDescriptor& result) {
                retval = r;
                if (retval == Result::OK) {
                    EffectHalHidl::effectDescriptorToHal(result, pDescriptor);
                }
            });
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return NAME_NOT_FOUND;
        else return NO_INIT;
    }
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::createEffect(
        const effect_uuid_t *pEffectUuid, int32_t sessionId, int32_t ioId,
        int32_t deviceId __unused, sp<EffectHalInterface> *effect) {
    if (mEffectsFactory == 0) return NO_INIT;
    Uuid hidlUuid;
    HidlUtils::uuidFromHal(*pEffectUuid, &hidlUuid);
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret;
#if MAJOR_VERSION >= 6
    ret = mEffectsFactory->createEffect(
            hidlUuid, sessionId, ioId, deviceId,
            [&](Result r, const sp<IEffect>& result, uint64_t effectId) {
                retval = r;
                if (retval == Result::OK) {
                    *effect = new EffectHalHidl(result, effectId);
                }
            });
#else
    if (sessionId == AUDIO_SESSION_DEVICE && ioId == AUDIO_IO_HANDLE_NONE) {
        return INVALID_OPERATION;
    }
    ret = mEffectsFactory->createEffect(
            hidlUuid, sessionId, ioId,
            [&](Result r, const sp<IEffect>& result, uint64_t effectId) {
                retval = r;
                if (retval == Result::OK) {
                    *effect = new EffectHalHidl(result, effectId);
                }
            });
#endif
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return NAME_NOT_FOUND;
        else return NO_INIT;
    }
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::dumpEffects(int fd) {
    if (mEffectsFactory == 0) return NO_INIT;
    native_handle_t* hidlHandle = native_handle_create(1, 0);
    hidlHandle->data[0] = fd;
    Return<void> ret = mEffectsFactory->debug(hidlHandle, {} /* options */);
    native_handle_delete(hidlHandle);
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::allocateBuffer(size_t size, sp<EffectBufferHalInterface>* buffer) {
    return EffectBufferHalHidl::allocate(size, buffer);
}

status_t EffectsFactoryHalHidl::mirrorBuffer(void* external, size_t size,
                          sp<EffectBufferHalInterface>* buffer) {
    return EffectBufferHalHidl::mirror(external, size, buffer);
}

} // namespace CPP_VERSION
} // namespace effect

template<>
sp<EffectsFactoryHalInterface> createFactoryHal<AudioHALVersion::CPP_VERSION>() {
    auto service = hardware::audio::effect::CPP_VERSION::IEffectsFactory::getService();
    return service ? new effect::CPP_VERSION::EffectsFactoryHalHidl(service) : nullptr;
}

} // namespace android
