/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include <android-base/logging.h>

#include "effect-impl/EffectImpl.h"
#include "effect-impl/EffectUUID.h"

#include "BundleContext.h"
#include "BundleTypes.h"
#include "GlobalSession.h"

namespace aidl::android::hardware::audio::effect {

class EffectBundleAidl final : public EffectImpl {
  public:
    explicit EffectBundleAidl(const AudioUuid& uuid);
    ~EffectBundleAidl() override;

    ndk::ScopedAStatus getDescriptor(Descriptor* _aidl_return) override;
    ndk::ScopedAStatus setParameterCommon(const Parameter& param) override;
    ndk::ScopedAStatus setParameterSpecific(const Parameter::Specific& specific) override;
    ndk::ScopedAStatus getParameterSpecific(const Parameter::Id& id,
                                            Parameter::Specific* specific) override;
    IEffect::Status effectProcessImpl(float *in, float *out, int process) override;

    std::shared_ptr<EffectContext> createContext(const Parameter::Common& common) override;
    RetCode releaseContext() override;

    ndk::ScopedAStatus commandStart() override {
        mContext->enable();
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus commandStop() override {
        mContext->disable();
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus commandReset() override {
            mContext->disable();
            return ndk::ScopedAStatus::ok();
    }

  private:
    const Descriptor* mDescriptor;
    lvm::BundleEffectType mType = lvm::BundleEffectType::EQUALIZER;
    std::shared_ptr<BundleContext> mContext;

    IEffect::Status status(binder_status_t status, size_t consumed, size_t produced);
    ndk::ScopedAStatus getParameterEqualizer(const Equalizer::Tag& tag,
                                             Parameter::Specific* specific);
};

}  // namespace aidl::android::hardware::audio::effect
