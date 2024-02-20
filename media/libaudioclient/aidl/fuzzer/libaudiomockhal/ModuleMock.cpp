/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "core-mock/ModuleMock.h"

namespace aidl::android::hardware::audio::core {

ModuleMock::ModuleMock() {
    // Device ports
    auto outDevice = createPort(/* PortId */ 0, /* Name */ "Default",
                                /* Flags */ 1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE,
                                /* isInput */ false,
                                createDeviceExt(
                                        /* DeviceType */ AudioDeviceType::OUT_DEFAULT,
                                        /* Flags */ AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE));
    mPorts.push_back(outDevice);
    auto inDevice = createPort(/* PortId */ 1, /* Name */ "Default",
                               /* Flags */ 1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE,
                               /* isInput */ true,
                               createDeviceExt(
                                       /* DeviceType */ AudioDeviceType::IN_DEFAULT,
                                       /* Flags */ 0));
    mPorts.push_back(outDevice);
}

ndk::ScopedAStatus ModuleMock::setModuleDebug(const ModuleDebug&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getTelephony(std::shared_ptr<ITelephony>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getBluetooth(std::shared_ptr<IBluetooth>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getBluetoothA2dp(std::shared_ptr<IBluetoothA2dp>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getBluetoothLe(std::shared_ptr<IBluetoothLe>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::connectExternalDevice(const AudioPort&, AudioPort*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::disconnectExternalDevice(int32_t) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioPatches(std::vector<AudioPatch>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioPort(int32_t, AudioPort*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioPortConfigs(std::vector<AudioPortConfig>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioPorts(std::vector<AudioPort>* _aidl_return) {
    *_aidl_return = mPorts;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioRoutes(std::vector<AudioRoute>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAudioRoutesForAudioPort(int32_t, std::vector<AudioRoute>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::openInputStream(const OpenInputStreamArguments&,
                                               OpenInputStreamReturn*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::openOutputStream(const OpenOutputStreamArguments&,
                                                OpenOutputStreamReturn*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getSupportedPlaybackRateFactors(SupportedPlaybackRateFactors*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setAudioPatch(const AudioPatch&, AudioPatch*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setAudioPortConfig(const AudioPortConfig&, AudioPortConfig*, bool*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::resetAudioPatch(int32_t) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::resetAudioPortConfig(int32_t) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getMasterMute(bool*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setMasterMute(bool) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getMasterVolume(float*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setMasterVolume(float) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getMicMute(bool*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setMicMute(bool) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getMicrophones(std::vector<MicrophoneInfo>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::updateAudioMode(AudioMode) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::updateScreenRotation(ScreenRotation) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::updateScreenState(bool) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getSoundDose(std::shared_ptr<sounddose::ISoundDose>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::generateHwAvSyncId(int32_t*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getVendorParameters(const std::vector<std::string>&,
                                                   std::vector<VendorParameter>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::setVendorParameters(const std::vector<VendorParameter>&, bool) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::addDeviceEffect(int32_t, const std::shared_ptr<IEffect>&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::removeDeviceEffect(int32_t, const std::shared_ptr<IEffect>&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getMmapPolicyInfos(AudioMMapPolicyType,
                                                  std::vector<AudioMMapPolicyInfo>*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::supportsVariableLatency(bool*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAAudioMixerBurstCount(int32_t*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::getAAudioHardwareBurstMinUsec(int32_t*) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleMock::prepareToDisconnectExternalDevice(int32_t) {
    return ndk::ScopedAStatus::ok();
}

AudioPortExt ModuleMock::createDeviceExt(AudioDeviceType devType, int32_t flags) {
    AudioPortDeviceExt deviceExt;
    deviceExt.device.type.type = devType;
    deviceExt.flags = flags;
    return AudioPortExt::make<AudioPortExt::Tag::device>(deviceExt);
}

AudioPort ModuleMock::createPort(int32_t id, const std::string& name, int32_t flags, bool isInput,
                                 const AudioPortExt& ext) {
    AudioPort port;
    port.id = id;
    port.name = name;
    port.flags = isInput ? AudioIoFlags::make<AudioIoFlags::Tag::input>(flags)
                         : AudioIoFlags::make<AudioIoFlags::Tag::output>(flags);
    port.ext = ext;
    return port;
}

}  // namespace aidl::android::hardware::audio::core
