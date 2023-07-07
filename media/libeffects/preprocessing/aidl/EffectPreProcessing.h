/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <aidl/android/hardware/audio/effect/BnEffect.h>

#include "PreProcessingContext.h"
#include "PreProcessingSession.h"
#include "effect-impl/EffectImpl.h"

namespace aidl::android::hardware::audio::effect {

class EffectPreProcessing final : public EffectImpl {
  public:
    explicit EffectPreProcessing(const AudioUuid& uuid);
    ~EffectPreProcessing() override;

    ndk::ScopedAStatus getDescriptor(Descriptor* _aidl_return) override;

    ndk::ScopedAStatus setParameterSpecific(const Parameter::Specific& specific) override;
    ndk::ScopedAStatus getParameterSpecific(const Parameter::Id& id,
                                            Parameter::Specific* specific) override;

    std::shared_ptr<EffectContext> createContext(const Parameter::Common& common) override;
    std::shared_ptr<EffectContext> getContext() override;
    RetCode releaseContext() override;

    IEffect::Status effectProcessImpl(float* in, float* out, int samples) override;

    ndk::ScopedAStatus commandImpl(CommandId command) override;

    std::string getEffectName() override { return *mEffectName; }

  private:
    std::shared_ptr<PreProcessingContext> mContext;
    const Descriptor* mDescriptor;
    const std::string* mEffectName;
    PreProcessingEffectType mType;

    ndk::ScopedAStatus setParameterAcousticEchoCanceler(const Parameter::Specific& specific);
    ndk::ScopedAStatus getParameterAcousticEchoCanceler(const AcousticEchoCanceler::Id& id,
                                                        Parameter::Specific* specific);

    ndk::ScopedAStatus setParameterAutomaticGainControlV1(const Parameter::Specific& specific);
    ndk::ScopedAStatus getParameterAutomaticGainControlV1(const AutomaticGainControlV1::Id& id,
                                                          Parameter::Specific* specific);

    ndk::ScopedAStatus setParameterAutomaticGainControlV2(const Parameter::Specific& specific);
    ndk::ScopedAStatus getParameterAutomaticGainControlV2(const AutomaticGainControlV2::Id& id,
                                                          Parameter::Specific* specific);

    ndk::ScopedAStatus setParameterNoiseSuppression(const Parameter::Specific& specific);
    ndk::ScopedAStatus getParameterNoiseSuppression(const NoiseSuppression::Id& id,
                                                    Parameter::Specific* specific);
};

}  // namespace aidl::android::hardware::audio::effect
