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
#include <android/binder_enums.h>
#include <audio_effects/effect_environmentalreverb.h>
#include <audio_effects/effect_presetreverb.h>
#include "effect-impl/EffectUUID.h"
// from Reverb/lib
#include "LVREV.h"

namespace aidl::android::hardware::audio::effect {
namespace lvm {

constexpr inline int kMaxCallSize = 256;
constexpr inline int kMinLevel = -6000;
constexpr inline int kMaxT60 = 7000; /* Maximum decay time */
constexpr inline int kMaxReverbLevel = 2000;
constexpr inline int kMaxFrameSize = 2560;
constexpr inline int kCpuLoadARM9E = 470;                      // Expressed in 0.1 MIPS
constexpr inline int kMemUsage = (71 + (kMaxFrameSize >> 7));  // Expressed in kB

static const EnvironmentalReverb::Capability kEnvReverbCap = {.minRoomLevelMb = lvm::kMinLevel,
                                                              .maxRoomLevelMb = 0,
                                                              .minRoomHfLevelMb = -4000,
                                                              .maxRoomHfLevelMb = 0,
                                                              .maxDecayTimeMs = lvm::kMaxT60,
                                                              .minDecayHfRatioPm = 100,
                                                              .maxDecayHfRatioPm = 2000,
                                                              .minLevelMb = lvm::kMinLevel,
                                                              .maxLevelMb = 0,
                                                              .maxDelayMs = 65,
                                                              .maxDiffusionPm = 1000,
                                                              .maxDensityPm = 1000};

// NXP SW auxiliary environmental reverb
static const std::string kAuxEnvReverbEffectName = "Auxiliary Environmental Reverb";
static const Descriptor kAuxEnvReverbDesc = {
        .common = {.id = {.type = kEnvReverbTypeUUID,
                          .uuid = kAuxEnvReverbImplUUID,
                          .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::AUXILIARY},
                   .cpuLoad = kCpuLoadARM9E,
                   .memoryUsage = kMemUsage,
                   .name = kAuxEnvReverbEffectName,
                   .implementor = "NXP Software Ltd."},
        .capability = Capability::make<Capability::environmentalReverb>(kEnvReverbCap)};

// NXP SW insert environmental reverb
static const std::string kInsertEnvReverbEffectName = "Insert Environmental Reverb";
static const Descriptor kInsertEnvReverbDesc = {
        .common = {.id = {.type = kEnvReverbTypeUUID,
                          .uuid = kInsertEnvReverbImplUUID,
                          .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::INSERT,
                             .insert = Flags::Insert::FIRST,
                             .volume = Flags::Volume::CTRL},
                   .cpuLoad = kCpuLoadARM9E,
                   .memoryUsage = kMemUsage,
                   .name = kInsertEnvReverbEffectName,
                   .implementor = "NXP Software Ltd."},
        .capability = Capability::make<Capability::environmentalReverb>(kEnvReverbCap)};

static const std::vector<PresetReverb::Presets> kSupportedPresets{
        ndk::enum_range<PresetReverb::Presets>().begin(),
        ndk::enum_range<PresetReverb::Presets>().end()};
static const PresetReverb::Capability kPresetReverbCap = {.supportedPresets = kSupportedPresets};

// NXP SW auxiliary preset reverb
static const std::string kAuxPresetReverbEffectName = "Auxiliary Preset Reverb";
static const Descriptor kAuxPresetReverbDesc = {
        .common = {.id = {.type = kPresetReverbTypeUUID,
                          .uuid = kAuxPresetReverbImplUUID,
                          .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::AUXILIARY},
                   .cpuLoad = kCpuLoadARM9E,
                   .memoryUsage = kMemUsage,
                   .name = kAuxPresetReverbEffectName,
                   .implementor = "NXP Software Ltd."},
        .capability = Capability::make<Capability::presetReverb>(kPresetReverbCap)};

// NXP SW insert preset reverb
static const std::string kInsertPresetReverbEffectName = "Insert Preset Reverb";
static const Descriptor kInsertPresetReverbDesc = {
        .common = {.id = {.type = kPresetReverbTypeUUID,
                          .uuid = kInsertPresetReverbImplUUID,
                          .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::INSERT,
                             .insert = Flags::Insert::FIRST,
                             .volume = Flags::Volume::CTRL},
                   .cpuLoad = kCpuLoadARM9E,
                   .memoryUsage = kMemUsage,
                   .name = kInsertPresetReverbEffectName,
                   .implementor = "NXP Software Ltd."},
        .capability = Capability::make<Capability::presetReverb>(kPresetReverbCap)};

enum class ReverbEffectType {
    AUX_ENV,
    INSERT_ENV,
    AUX_PRESET,
    INSERT_PRESET,
};

inline std::ostream& operator<<(std::ostream& out, const ReverbEffectType& type) {
    switch (type) {
        case ReverbEffectType::AUX_ENV:
            return out << kAuxEnvReverbEffectName;
        case ReverbEffectType::INSERT_ENV:
            return out << kInsertEnvReverbEffectName;
        case ReverbEffectType::AUX_PRESET:
            return out << kAuxPresetReverbEffectName;
        case ReverbEffectType::INSERT_PRESET:
            return out << kInsertPresetReverbEffectName;
    }
    return out << "EnumReverbEffectTypeError";
}

inline std::ostream& operator<<(std::ostream& out, const LVREV_ReturnStatus_en& status) {
    switch (status) {
        case LVREV_SUCCESS:
            return out << "LVREV_SUCCESS";
        case LVREV_NULLADDRESS:
            return out << "LVREV_NULLADDRESS";
        case LVREV_OUTOFRANGE:
            return out << "LVREV_OUTOFRANGE";
        case LVREV_INVALIDNUMSAMPLES:
            return out << "LVREV_INVALIDNUMSAMPLES";
        case LVREV_RETURNSTATUS_DUMMY:
            return out << "LVREV_RETURNSTATUS_DUMMY";
    }
    return out << "EnumLvrevRetStatusError";
}

}  // namespace lvm
}  // namespace aidl::android::hardware::audio::effect
