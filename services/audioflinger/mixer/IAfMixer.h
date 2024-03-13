/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <media/AudioBufferProvider.h>
#include <media/AudioResamplerPublic.h>
#include <media/AudioSystem.h>
#include <utils/RefBase.h>
#include <vibrator/ExternalVibrationUtils.h>

#include <stdint.h>

namespace android {

class IAfMixer : public virtual RefBase {
  public:
    static sp<IAfMixer> create(size_t frameCount, uint32_t sampleRate);

    virtual bool isValidFormat(audio_format_t format) const = 0;
    virtual bool isValidChannelMask(audio_channel_mask_t channelMask) const = 0;

    virtual status_t createTrack(int name, audio_channel_mask_t channelMask, audio_format_t format,
                                 int sessionId) = 0;
    virtual void destroyTrack(int name) = 0;
    virtual bool exists(int name) const = 0;
    virtual void enable(int name) = 0;
    virtual void disable(int name) = 0;

    virtual void setBufferProvider(int name, AudioBufferProvider* bufferProvider) = 0;
    virtual void setChannelMask(int name, audio_channel_mask_t channelMask) = 0;
    virtual void setFormat(int name, audio_format_t format) = 0;
    virtual void setMixerChannelMask(int name, audio_channel_mask_t channelMask);
    virtual void setMixerFormat(int name, audio_format_t format);
    virtual void setMainBuffer(int name, void* buffer) = 0;
    virtual void setAuxBuffer(int name, void* buffer) = 0;

    virtual void setVolume(int name, float left, float right, float auxLevel, bool ramp) = 0;

    virtual void setResampler(int name, uint32_t sampleRate) = 0;
    virtual void removeResampler(int name) = 0;
    virtual void resetResampler(int name) = 0;

    virtual void setPlaybackRate(int name, AudioPlaybackRate playbackRate) = 0;

    virtual void setHaptics(int name, bool enabled, os::HapticScale scale, float maxAmplitude) = 0;

    virtual void setTeeBuffer(int name, void* buffer, size_t frameCount) = 0;

    virtual size_t getUnreleasedFrames(int name) const = 0;

    virtual void process() = 0;

    virtual std::string trackNames() const = 0;
};

}  // namespace android
