/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "APM::AudioOutputDescriptor"
//#define LOG_NDEBUG 0

#include <AudioPolicyInterface.h>
#include "AudioOutputDescriptor.h"
#include "IOProfile.h"
#include "AudioGain.h"
#include "Volume.h"
#include "HwModule.h"
#include <media/AudioParameter.h>
#include <media/AudioPolicy.h>

// A device mask for all audio output devices that are considered "remote" when evaluating
// active output devices in isStreamActiveRemotely()
#define APM_AUDIO_OUT_DEVICE_REMOTE_ALL  AUDIO_DEVICE_OUT_REMOTE_SUBMIX

namespace android {

AudioOutputDescriptor::AudioOutputDescriptor(const sp<AudioPort>& port,
                                             AudioPolicyClientInterface *clientInterface)
    : mPort(port), mDevice(AUDIO_DEVICE_NONE),
      mClientInterface(clientInterface), mPatchHandle(AUDIO_PATCH_HANDLE_NONE), mId(0)
{
    // clear usage count for all stream types
    for (int i = 0; i < AUDIO_STREAM_CNT; i++) {
        mRefCount[i] = 0;
        mCurVolume[i] = -1.0;
        mMuteCount[i] = 0;
        mStopTime[i] = 0;
    }
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        mStrategyMutedByDevice[i] = false;
    }
    if (mPort.get() != nullptr) {
        mPort->pickAudioProfile(mSamplingRate, mChannelMask, mFormat);
        if (mPort->mGains.size() > 0) {
            mPort->mGains[0]->getDefaultConfig(&mGain);
        }
    }
}

audio_module_handle_t AudioOutputDescriptor::getModuleHandle() const
{
    return mPort.get() != nullptr ? mPort->getModuleHandle() : AUDIO_MODULE_HANDLE_NONE;
}

audio_port_handle_t AudioOutputDescriptor::getId() const
{
    return mId;
}

audio_devices_t AudioOutputDescriptor::device() const
{
    return mDevice;
}

audio_devices_t AudioOutputDescriptor::supportedDevices()
{
    return mDevice;
}

bool AudioOutputDescriptor::sharesHwModuleWith(
        const sp<AudioOutputDescriptor>& outputDesc)
{
    if (outputDesc->isDuplicated()) {
        return sharesHwModuleWith(outputDesc->subOutput1()) ||
                    sharesHwModuleWith(outputDesc->subOutput2());
    } else {
        return hasSameHwModuleAs(outputDesc);
    }
}

void AudioOutputDescriptor::changeRefCount(audio_stream_type_t stream,
                                                                   int delta)
{
    if ((delta + (int)mRefCount[stream]) < 0) {
        ALOGW("changeRefCount() invalid delta %d for stream %d, refCount %d",
              delta, stream, mRefCount[stream]);
        mRefCount[stream] = 0;
        return;
    }
    mRefCount[stream] += delta;
    ALOGV("changeRefCount() stream %d, count %d", stream, mRefCount[stream]);
}

bool AudioOutputDescriptor::isActive(uint32_t inPastMs) const
{
    nsecs_t sysTime = 0;
    if (inPastMs != 0) {
        sysTime = systemTime();
    }
    for (int i = 0; i < (int)AUDIO_STREAM_CNT; i++) {
        if (i == AUDIO_STREAM_PATCH) {
            continue;
        }
        if (isStreamActive((audio_stream_type_t)i, inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

bool AudioOutputDescriptor::isStreamActive(audio_stream_type_t stream,
                                           uint32_t inPastMs,
                                           nsecs_t sysTime) const
{
    if (mRefCount[stream] != 0) {
        return true;
    }
    if (inPastMs == 0) {
        return false;
    }
    if (sysTime == 0) {
        sysTime = systemTime();
    }
    if (ns2ms(sysTime - mStopTime[stream]) < inPastMs) {
        return true;
    }
    return false;
}


bool AudioOutputDescriptor::isFixedVolume(audio_devices_t device __unused)
{
    return false;
}

bool AudioOutputDescriptor::setVolume(float volume,
                                      audio_stream_type_t stream,
                                      audio_devices_t device __unused,
                                      uint32_t delayMs,
                                      bool force)
{
    // We actually change the volume if:
    // - the float value returned by computeVolume() changed
    // - the force flag is set
    if (volume != mCurVolume[stream] || force) {
        ALOGV("setVolume() for stream %d, volume %f, delay %d", stream, volume, delayMs);
        mCurVolume[stream] = volume;
        return true;
    }
    return false;
}

void AudioOutputDescriptor::toAudioPortConfig(
                                                 struct audio_port_config *dstConfig,
                                                 const struct audio_port_config *srcConfig) const
{
    dstConfig->config_mask = AUDIO_PORT_CONFIG_SAMPLE_RATE|AUDIO_PORT_CONFIG_CHANNEL_MASK|
                            AUDIO_PORT_CONFIG_FORMAT|AUDIO_PORT_CONFIG_GAIN;
    if (srcConfig != NULL) {
        dstConfig->config_mask |= srcConfig->config_mask;
    }
    AudioPortConfig::toAudioPortConfig(dstConfig, srcConfig);

    dstConfig->id = mId;
    dstConfig->role = AUDIO_PORT_ROLE_SOURCE;
    dstConfig->type = AUDIO_PORT_TYPE_MIX;
    dstConfig->ext.mix.hw_module = getModuleHandle();
    dstConfig->ext.mix.usecase.stream = AUDIO_STREAM_DEFAULT;
}

void AudioOutputDescriptor::toAudioPort(struct audio_port *port) const
{
    // Should not be called for duplicated ports, see SwAudioOutputDescriptor::toAudioPortConfig.
    mPort->toAudioPort(port);
    port->id = mId;
    port->ext.mix.hw_module = getModuleHandle();
}

status_t AudioOutputDescriptor::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, " ID: %d\n", mId);
    result.append(buffer);
    snprintf(buffer, SIZE, " Sampling rate: %d\n", mSamplingRate);
    result.append(buffer);
    snprintf(buffer, SIZE, " Format: %08x\n", mFormat);
    result.append(buffer);
    snprintf(buffer, SIZE, " Channels: %08x\n", mChannelMask);
    result.append(buffer);
    snprintf(buffer, SIZE, " Devices %08x\n", device());
    result.append(buffer);
    snprintf(buffer, SIZE, " Stream volume refCount muteCount\n");
    result.append(buffer);
    for (int i = 0; i < (int)AUDIO_STREAM_CNT; i++) {
        snprintf(buffer, SIZE, " %02d     %.03f     %02d       %02d\n",
                 i, mCurVolume[i], mRefCount[i], mMuteCount[i]);
        result.append(buffer);
    }
    write(fd, result.string(), result.size());

    return NO_ERROR;
}

void AudioOutputDescriptor::log(const char* indent)
{
    ALOGI("%sID: %d,0x%X, [rt:%d fmt:0x%X ch:0x%X]",
          indent, mId, mId, mSamplingRate, mFormat, mChannelMask);
}

// SwAudioOutputDescriptor implementation
SwAudioOutputDescriptor::SwAudioOutputDescriptor(const sp<IOProfile>& profile,
                                                 AudioPolicyClientInterface *clientInterface)
    : AudioOutputDescriptor(profile, clientInterface),
    mProfile(profile), mIoHandle(AUDIO_IO_HANDLE_NONE), mLatency(0),
    mFlags((audio_output_flags_t)0), mPolicyMix(NULL),
    mOutput1(0), mOutput2(0), mDirectOpenCount(0),
    mDirectClientSession(AUDIO_SESSION_NONE), mGlobalRefCount(0)
{
    if (profile != NULL) {
        mFlags = (audio_output_flags_t)profile->getFlags();
    }
}

status_t SwAudioOutputDescriptor::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, " Latency: %d\n", mLatency);
    result.append(buffer);
    snprintf(buffer, SIZE, " Flags %08x\n", mFlags);
    result.append(buffer);
    write(fd, result.string(), result.size());

    AudioOutputDescriptor::dump(fd);

    return NO_ERROR;
}

audio_devices_t SwAudioOutputDescriptor::device() const
{
    if (isDuplicated()) {
        return (audio_devices_t)(mOutput1->mDevice | mOutput2->mDevice);
    } else {
        return mDevice;
    }
}

bool SwAudioOutputDescriptor::sharesHwModuleWith(
        const sp<AudioOutputDescriptor>& outputDesc)
{
    if (isDuplicated()) {
        return mOutput1->sharesHwModuleWith(outputDesc) || mOutput2->sharesHwModuleWith(outputDesc);
    } else if (outputDesc->isDuplicated()){
        return sharesHwModuleWith(outputDesc->subOutput1()) ||
                    sharesHwModuleWith(outputDesc->subOutput2());
    } else {
        return AudioOutputDescriptor::sharesHwModuleWith(outputDesc);
    }
}

audio_devices_t SwAudioOutputDescriptor::supportedDevices()
{
    if (isDuplicated()) {
        return (audio_devices_t)(mOutput1->supportedDevices() | mOutput2->supportedDevices());
    } else {
        return mProfile->getSupportedDevicesType();
    }
}

uint32_t SwAudioOutputDescriptor::latency()
{
    if (isDuplicated()) {
        return (mOutput1->mLatency > mOutput2->mLatency) ? mOutput1->mLatency : mOutput2->mLatency;
    } else {
        return mLatency;
    }
}

void SwAudioOutputDescriptor::changeRefCount(audio_stream_type_t stream,
                                                                   int delta)
{
    // forward usage count change to attached outputs
    if (isDuplicated()) {
        mOutput1->changeRefCount(stream, delta);
        mOutput2->changeRefCount(stream, delta);
    }
    AudioOutputDescriptor::changeRefCount(stream, delta);

    // handle stream-independent ref count
    uint32_t oldGlobalRefCount = mGlobalRefCount;
    if ((delta + (int)mGlobalRefCount) < 0) {
        ALOGW("changeRefCount() invalid delta %d globalRefCount %d", delta, mGlobalRefCount);
        mGlobalRefCount = 0;
    } else {
        mGlobalRefCount += delta;
    }
    if ((oldGlobalRefCount == 0) && (mGlobalRefCount > 0)) {
        if ((mPolicyMix != NULL) && ((mPolicyMix->mCbFlags & AudioMix::kCbFlagNotifyActivity) != 0))
        {
            mClientInterface->onDynamicPolicyMixStateUpdate(mPolicyMix->mDeviceAddress,
                    MIX_STATE_MIXING);
        }

    } else if ((oldGlobalRefCount > 0) && (mGlobalRefCount == 0)) {
        if ((mPolicyMix != NULL) && ((mPolicyMix->mCbFlags & AudioMix::kCbFlagNotifyActivity) != 0))
        {
            mClientInterface->onDynamicPolicyMixStateUpdate(mPolicyMix->mDeviceAddress,
                    MIX_STATE_IDLE);
        }
    }
}


bool SwAudioOutputDescriptor::isFixedVolume(audio_devices_t device)
{
    // unit gain if rerouting to external policy
    if (device == AUDIO_DEVICE_OUT_REMOTE_SUBMIX) {
        if (mPolicyMix != NULL) {
            ALOGV("max gain when rerouting for output=%d", mIoHandle);
            return true;
        }
    }
    if (device == AUDIO_DEVICE_OUT_TELEPHONY_TX) {
        ALOGV("max gain when output device is telephony tx");
        return true;
    }
    return false;
}

void SwAudioOutputDescriptor::toAudioPortConfig(
                                                 struct audio_port_config *dstConfig,
                                                 const struct audio_port_config *srcConfig) const
{

    ALOG_ASSERT(!isDuplicated(), "toAudioPortConfig() called on duplicated output %d", mIoHandle);
    AudioOutputDescriptor::toAudioPortConfig(dstConfig, srcConfig);

    dstConfig->ext.mix.handle = mIoHandle;
}

void SwAudioOutputDescriptor::toAudioPort(
                                                    struct audio_port *port) const
{
    ALOG_ASSERT(!isDuplicated(), "toAudioPort() called on duplicated output %d", mIoHandle);

    AudioOutputDescriptor::toAudioPort(port);

    toAudioPortConfig(&port->active_config);
    port->ext.mix.handle = mIoHandle;
    port->ext.mix.latency_class =
            mFlags & AUDIO_OUTPUT_FLAG_FAST ? AUDIO_LATENCY_LOW : AUDIO_LATENCY_NORMAL;
}

bool SwAudioOutputDescriptor::setVolume(float volume,
                                        audio_stream_type_t stream,
                                        audio_devices_t device,
                                        uint32_t delayMs,
                                        bool force)
{
    bool changed = AudioOutputDescriptor::setVolume(volume, stream, device, delayMs, force);

    if (changed) {
        // Force VOICE_CALL to track BLUETOOTH_SCO stream volume when bluetooth audio is
        // enabled
        float volume = Volume::DbToAmpl(mCurVolume[stream]);
        if (stream == AUDIO_STREAM_BLUETOOTH_SCO) {
            mClientInterface->setStreamVolume(
                    AUDIO_STREAM_VOICE_CALL, volume, mIoHandle, delayMs);
        }
        mClientInterface->setStreamVolume(stream, volume, mIoHandle, delayMs);
    }
    return changed;
}

status_t SwAudioOutputDescriptor::open(const audio_config_t *config,
                                       audio_devices_t device,
                                       const String8& address,
                                       audio_stream_type_t stream,
                                       audio_output_flags_t flags,
                                       audio_io_handle_t *output)
{
    audio_config_t lConfig;
    if (config == nullptr) {
        lConfig = AUDIO_CONFIG_INITIALIZER;
        lConfig.sample_rate = mSamplingRate;
        lConfig.channel_mask = mChannelMask;
        lConfig.format = mFormat;
    } else {
        lConfig = *config;
    }

    mDevice = device;
    // if the selected profile is offloaded and no offload info was specified,
    // create a default one
    if ((mProfile->getFlags() & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) &&
            lConfig.offload_info.format == AUDIO_FORMAT_DEFAULT) {
        flags = (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD);
        lConfig.offload_info = AUDIO_INFO_INITIALIZER;
        lConfig.offload_info.sample_rate = lConfig.sample_rate;
        lConfig.offload_info.channel_mask = lConfig.channel_mask;
        lConfig.offload_info.format = lConfig.format;
        lConfig.offload_info.stream_type = stream;
        lConfig.offload_info.duration_us = -1;
        lConfig.offload_info.has_video = true; // conservative
        lConfig.offload_info.is_streaming = true; // likely
    }

    mFlags = (audio_output_flags_t)(mFlags | flags);

    ALOGV("opening output for device %08x address %s profile %p name %s",
          mDevice, address.string(), mProfile.get(), mProfile->getName().string());

    status_t status = mClientInterface->openOutput(mProfile->getModuleHandle(),
                                                   output,
                                                   &lConfig,
                                                   &mDevice,
                                                   address,
                                                   &mLatency,
                                                   mFlags);
    LOG_ALWAYS_FATAL_IF(mDevice != device,
                        "%s openOutput returned device %08x when given device %08x",
                        __FUNCTION__, mDevice, device);

    if (status == NO_ERROR) {
        LOG_ALWAYS_FATAL_IF(*output == AUDIO_IO_HANDLE_NONE,
                            "%s openOutput returned output handle %d for device %08x",
                            __FUNCTION__, *output, device);
        mSamplingRate = lConfig.sample_rate;
        mChannelMask = lConfig.channel_mask;
        mFormat = lConfig.format;
        mId = AudioPort::getNextUniqueId();
        mIoHandle = *output;
        mProfile->curOpenCount++;
    }

    return status;
}

status_t SwAudioOutputDescriptor::start()
{
    if (isDuplicated()) {
        status_t status = mOutput1->start();
        if (status != NO_ERROR) {
            return status;
        }
        status = mOutput2->start();
        if (status != NO_ERROR) {
            mOutput1->stop();
            return status;
        }
        return NO_ERROR;
    }
    if (!isActive()) {
        if (!mProfile->canStartNewIo()) {
            return INVALID_OPERATION;
        }
        mProfile->curActiveCount++;
    }
    return NO_ERROR;
}

void SwAudioOutputDescriptor::stop()
{
    if (isDuplicated()) {
        mOutput1->stop();
        mOutput2->stop();
        return;
    }

    if (!isActive()) {
        LOG_ALWAYS_FATAL_IF(mProfile->curActiveCount < 1,
                            "%s invalid profile active count %u",
                            __func__, mProfile->curActiveCount);
        mProfile->curActiveCount--;
    }
}

void SwAudioOutputDescriptor::close()
{
    if (mIoHandle != AUDIO_IO_HANDLE_NONE) {
        AudioParameter param;
        param.add(String8("closing"), String8("true"));
        mClientInterface->setParameters(mIoHandle, param.toString());

        mClientInterface->closeOutput(mIoHandle);

        LOG_ALWAYS_FATAL_IF(mProfile->curOpenCount < 1, "%s profile open count %u",
                            __FUNCTION__, mProfile->curOpenCount);
        // do not call stop() here as stop() is supposed to be called after changeRefCount(-1)
        // and we don't know how many streams are still active at this time
        if (isActive()) {
            mProfile->curActiveCount--;
        }
        mProfile->curOpenCount--;
        mIoHandle = AUDIO_IO_HANDLE_NONE;
    }
}

status_t SwAudioOutputDescriptor::openDuplicating(const sp<SwAudioOutputDescriptor>& output1,
                                                  const sp<SwAudioOutputDescriptor>& output2,
                                                  audio_io_handle_t *ioHandle)
{
    // open a duplicating output thread for the new output and the primary output
    // Note: openDuplicateOutput() API expects the output handles in the reverse order from the
    // numbering in SwAudioOutputDescriptor mOutput1 and mOutput2
    *ioHandle = mClientInterface->openDuplicateOutput(output2->mIoHandle, output1->mIoHandle);
    if (*ioHandle == AUDIO_IO_HANDLE_NONE) {
        return INVALID_OPERATION;
    }

    mId = AudioPort::getNextUniqueId();
    mIoHandle = *ioHandle;
    mOutput1 = output1;
    mOutput2 = output2;
    mSamplingRate = output2->mSamplingRate;
    mFormat = output2->mFormat;
    mChannelMask = output2->mChannelMask;
    mLatency = output2->mLatency;

    return NO_ERROR;
}

// HwAudioOutputDescriptor implementation
HwAudioOutputDescriptor::HwAudioOutputDescriptor(const sp<AudioSourceDescriptor>& source,
                                                 AudioPolicyClientInterface *clientInterface)
    : AudioOutputDescriptor(source->mDevice, clientInterface),
      mSource(source)
{
}

status_t HwAudioOutputDescriptor::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    AudioOutputDescriptor::dump(fd);

    snprintf(buffer, SIZE, "Source:\n");
    result.append(buffer);
    write(fd, result.string(), result.size());
    mSource->dump(fd);

    return NO_ERROR;
}

audio_devices_t HwAudioOutputDescriptor::supportedDevices()
{
    return mDevice;
}

void HwAudioOutputDescriptor::toAudioPortConfig(
                                                 struct audio_port_config *dstConfig,
                                                 const struct audio_port_config *srcConfig) const
{
    mSource->mDevice->toAudioPortConfig(dstConfig, srcConfig);
}

void HwAudioOutputDescriptor::toAudioPort(
                                                    struct audio_port *port) const
{
    mSource->mDevice->toAudioPort(port);
}


bool HwAudioOutputDescriptor::setVolume(float volume,
                                        audio_stream_type_t stream,
                                        audio_devices_t device,
                                        uint32_t delayMs,
                                        bool force)
{
    bool changed = AudioOutputDescriptor::setVolume(volume, stream, device, delayMs, force);

    if (changed) {
      // TODO: use gain controller on source device if any to adjust volume
    }
    return changed;
}

// SwAudioOutputCollection implementation
bool SwAudioOutputCollection::isStreamActive(audio_stream_type_t stream, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isStreamActive(stream, inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

bool SwAudioOutputCollection::isStreamActiveLocally(audio_stream_type_t stream, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isStreamActive(stream, inPastMs, sysTime)
                && ((outputDesc->device() & APM_AUDIO_OUT_DEVICE_REMOTE_ALL) == 0)) {
            return true;
        }
    }
    return false;
}

bool SwAudioOutputCollection::isStreamActiveRemotely(audio_stream_type_t stream,
                                                   uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (((outputDesc->device() & APM_AUDIO_OUT_DEVICE_REMOTE_ALL) != 0) &&
                outputDesc->isStreamActive(stream, inPastMs, sysTime)) {
            // do not consider re routing (when the output is going to a dynamic policy)
            // as "remote playback"
            if (outputDesc->mPolicyMix == NULL) {
                return true;
            }
        }
    }
    return false;
}

audio_io_handle_t SwAudioOutputCollection::getA2dpOutput() const
{
    for (size_t i = 0; i < size(); i++) {
        sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (!outputDesc->isDuplicated() && outputDesc->device() & AUDIO_DEVICE_OUT_ALL_A2DP) {
            return this->keyAt(i);
        }
    }
    return 0;
}

bool SwAudioOutputCollection::isA2dpOffloadedOnPrimary() const
{
    sp<SwAudioOutputDescriptor> primaryOutput = getPrimaryOutput();

    if ((primaryOutput != NULL) && (primaryOutput->mProfile != NULL)
        && (primaryOutput->mProfile->mModule != NULL)) {
        sp<HwModule> primaryHwModule = primaryOutput->mProfile->mModule;
        Vector <sp<IOProfile>> primaryHwModuleOutputProfiles =
                                   primaryHwModule->getOutputProfiles();
        for (size_t i = 0; i < primaryHwModuleOutputProfiles.size(); i++) {
            if (primaryHwModuleOutputProfiles[i]->supportDevice(AUDIO_DEVICE_OUT_ALL_A2DP)) {
                return true;
            }
        }
    }
    return false;
}

bool SwAudioOutputCollection::isA2dpSupported() const
{
    return (isA2dpOffloadedOnPrimary() || (getA2dpOutput() != 0));
}

sp<SwAudioOutputDescriptor> SwAudioOutputCollection::getPrimaryOutput() const
{
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->mFlags & AUDIO_OUTPUT_FLAG_PRIMARY) {
            return outputDesc;
        }
    }
    return NULL;
}

sp<SwAudioOutputDescriptor> SwAudioOutputCollection::getOutputFromId(audio_port_handle_t id) const
{
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->getId() == id) {
            return outputDesc;
        }
    }
    return NULL;
}

bool SwAudioOutputCollection::isAnyOutputActive(audio_stream_type_t streamToIgnore) const
{
    for (size_t s = 0 ; s < AUDIO_STREAM_CNT ; s++) {
        if (s == (size_t) streamToIgnore) {
            continue;
        }
        for (size_t i = 0; i < size(); i++) {
            const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
            if (outputDesc->mRefCount[s] != 0) {
                return true;
            }
        }
    }
    return false;
}

audio_devices_t SwAudioOutputCollection::getSupportedDevices(audio_io_handle_t handle) const
{
    sp<SwAudioOutputDescriptor> outputDesc = valueFor(handle);
    audio_devices_t devices = outputDesc->mProfile->getSupportedDevicesType();
    return devices;
}


status_t SwAudioOutputCollection::dump(int fd) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];

    snprintf(buffer, SIZE, "\nOutputs dump:\n");
    write(fd, buffer, strlen(buffer));
    for (size_t i = 0; i < size(); i++) {
        snprintf(buffer, SIZE, "- Output %d dump:\n", keyAt(i));
        write(fd, buffer, strlen(buffer));
        valueAt(i)->dump(fd);
    }

    return NO_ERROR;
}

// HwAudioOutputCollection implementation
bool HwAudioOutputCollection::isStreamActive(audio_stream_type_t stream, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<HwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isStreamActive(stream, inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

bool HwAudioOutputCollection::isAnyOutputActive(audio_stream_type_t streamToIgnore) const
{
    for (size_t s = 0 ; s < AUDIO_STREAM_CNT ; s++) {
        if (s == (size_t) streamToIgnore) {
            continue;
        }
        for (size_t i = 0; i < size(); i++) {
            const sp<HwAudioOutputDescriptor> outputDesc = valueAt(i);
            if (outputDesc->mRefCount[s] != 0) {
                return true;
            }
        }
    }
    return false;
}

status_t HwAudioOutputCollection::dump(int fd) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];

    snprintf(buffer, SIZE, "\nOutputs dump:\n");
    write(fd, buffer, strlen(buffer));
    for (size_t i = 0; i < size(); i++) {
        snprintf(buffer, SIZE, "- Output %d dump:\n", keyAt(i));
        write(fd, buffer, strlen(buffer));
        valueAt(i)->dump(fd);
    }

    return NO_ERROR;
}

}; //namespace android
