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

#define LOG_TAG "AfMixer"
// #define LOG_NDEBUG 0

#include <media/AudioMixer.h>

#include "IAfMixer.h"

namespace android {

class DefaultMixer : public IAfMixer {
  public:
    DefaultMixer(size_t frameCount, uint32_t sampleRate) : mAudioMixer(frameCount, sampleRate) {}

    bool isValidFormat(audio_format_t format) const final {
        return mAudioMixer.isValidFormat(format);
    }

    bool isValidChannelMask(audio_channel_mask_t channelMask) const final {
        return mAudioMixer.isValidChannelMask(channelMask);
    }

    status_t createTrack(int name, audio_channel_mask_t channelMask, audio_format_t format,
                         int sessionId) final {
        return mAudioMixer.create(name, channelMask, format, sessionId);
    }

    void destroyTrack(int name) final { return mAudioMixer.destroy(name); }

    bool exists(int name) const final { return mAudioMixer.exists(name); }

    void enable(int name) final {
        mAudioMixer.enable(name);
    }

    void disable(int name) final {
        mAudioMixer.disable(name);
    }

    void setBufferProvider(int name, AudioBufferProvider* bufferProvider) final {
        mAudioMixer.setBufferProvider(name, bufferProvider);
    }

    void setChannelMask(int name, audio_channel_mask_t channelMask) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::CHANNEL_MASK,
                                 (void*)(uintptr_t)channelMask);
    }

    void setFormat(int name, audio_format_t format) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::FORMAT, (void*)format);
    }

    void setMixerChannelMask(int name, audio_channel_mask_t channelMask) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::MIXER_CHANNEL_MASK,
                                 (void*)(uintptr_t)channelMask);
    }

    void setMixerFormat(int name, audio_format_t format) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::MIXER_FORMAT,
                                 (void*)(uintptr_t)format);
    }

    void setMainBuffer(int name, void* buffer) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::MAIN_BUFFER, buffer);
    }

    void setAuxBuffer(int name, void* buffer) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::AUX_BUFFER, buffer);
    }

    void setVolume(int name, float left, float right, float auxLevel, bool ramp) final {
        int target = ramp ? AudioMixer::RAMP_VOLUME : AudioMixer::VOLUME;
        mAudioMixer.setParameter(name, target, AudioMixer::VOLUME0, &left);
        mAudioMixer.setParameter(name, target, AudioMixer::VOLUME1, &right);
        mAudioMixer.setParameter(name, target, AudioMixer::AUXLEVEL, &auxLevel);
    }

    void setResampler(int name, uint32_t sampleRate) final {
        mAudioMixer.setParameter(name, AudioMixer::RESAMPLE, AudioMixer::SAMPLE_RATE,
                                 (void*)(uintptr_t)sampleRate);
    }

    void removeResampler(int name) final {
        mAudioMixer.setParameter(name, AudioMixer::RESAMPLE, AudioMixer::REMOVE, nullptr);
    }

    void resetResampler(int name) final {
        mAudioMixer.setParameter(name, AudioMixer::RESAMPLE, AudioMixer::RESET, nullptr);
    }

    void setPlaybackRate(int name, AudioPlaybackRate playbackRate) final {
        mAudioMixer.setParameter(name, AudioMixer::TIMESTRETCH, AudioMixer::PLAYBACK_RATE,
                                 &playbackRate);
    }

    void setHaptics(int name, bool enabled, os::HapticScale scale, float maxAmplitude) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::HAPTIC_ENABLED,
                                 (void*)(uintptr_t)enabled);
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::HAPTIC_INTENSITY,
                                 (void*)(uintptr_t)scale);
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::HAPTIC_MAX_AMPLITUDE,
                                 (void*)&maxAmplitude);
    }

    void setTeeBuffer(int name, void* buffer, size_t frameCount) final {
        mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::TEE_BUFFER, (void*)buffer);
        if (buffer) {
            mAudioMixer.setParameter(name, AudioMixer::TRACK, AudioMixer::TEE_BUFFER,
                                     (void*)(uintptr_t)frameCount);
        }
    }

    size_t getUnreleasedFrames(int name) const final {
        return mAudioMixer.getUnreleasedFrames(name);
    }

    void process() final { mAudioMixer.process(); }

    std::string trackNames() const final { return mAudioMixer.trackNames(); }

  private:
    AudioMixer mAudioMixer;
};

sp<IAfMixer> IAfMixer::create(size_t frameCount, uint32_t sampleRate) {
    return sp<DefaultMixer>::make(frameCount, sampleRate);
}

}  // namespace android
