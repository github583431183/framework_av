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

// #define LOG_NDEBUG 0
#define LOG_TAG "audioflinger_bufferproviders_tests"

#include <log/log.h>
#include <media/AudioBufferProvider.h>
#include <media/BufferProviders.h>

#include <gtest/gtest.h>

#include <sstream>

#include "test_utils.h"

namespace android {

template <typename T>
static std::vector<T> toVector(const AudioBufferProvider::Buffer& buffer, uint32_t channels) {
    if (buffer.frameCount == 0 || buffer.raw == nullptr) {
        return {};
    }

    std::vector<T> result(channels * buffer.frameCount);
    memcpy(result.data(), buffer.raw, sizeof(T) * channels * buffer.frameCount);
    return result;
}

TEST(audioflinger_bufferproviders, optionalprovider) {
    OptionalBufferProvider optionalProvider;

    // Returns 0 frames when provider is not set
    AudioBufferProvider::Buffer buffer;
    buffer.frameCount = 16;
    ASSERT_EQ(optionalProvider.getNextBuffer(&buffer), OK);
    ASSERT_EQ(buffer.frameCount, 0);
    ASSERT_EQ(buffer.raw, nullptr);

    // Data is returned when provider is set
    SignalProvider inputProvider;
    inputProvider.setSine<float>(/*channels=*/2, /*freq=*/100, /*sampleRate=*/48000,
                                 /*duration=*/0.01);
    optionalProvider.setBufferProvider(&inputProvider);
    buffer.frameCount = 16;
    ASSERT_EQ(optionalProvider.getNextBuffer(&buffer), OK);
    ASSERT_EQ(buffer.frameCount, 16);
    ASSERT_NE(buffer.raw, nullptr);
}

TEST(audioflinger_bufferproviders, resamplerprovider) {
    auto resampleTest = [](uint32_t channels, uint32_t inSampleRate, uint32_t outSampleRate) {
        // This test does not validate that the results of the ResampleBufferProvider returns the
        // same result as when using AudioResampler
        constexpr size_t frameCount = 32;
        constexpr double duration = 0.01;  // 10ms

        SignalProvider inputProvider;
        inputProvider.setSine<float>(channels, inSampleRate / 16, inSampleRate, duration);

        // Resample using ResampleBufferProvider
        ResampleBufferProvider resampleProvider(channels, AUDIO_FORMAT_PCM_FLOAT, inSampleRate,
                                                outSampleRate, frameCount);
        resampleProvider.setBufferProvider(&inputProvider);
        std::vector<float> output;
        while (true) {
            AudioBufferProvider::Buffer buffer;
            buffer.raw = nullptr;
            buffer.frameCount = frameCount;
            resampleProvider.getNextBuffer(&buffer);
            if (buffer.frameCount == 0) {
                break;
            }
            float* in = reinterpret_cast<float*>(buffer.raw);
            output.insert(output.end(), in, in + buffer.frameCount * channels);
            resampleProvider.releaseBuffer(&buffer);
        }

        ASSERT_EQ(resampleProvider.getUnreleasedFrames(), 0);

        // Verify the size.  Allow error of half a buffer.
        int expectedSize = duration * outSampleRate * channels;
        ASSERT_LT(std::abs(static_cast<int>(output.size()) - expectedSize), frameCount / 2);
    };

    std::array<uint32_t, 3> channels = {1, 2, 4};
    std::array<uint32_t, 4> sampleRates = {16000, 32000, 44100, 48000};
    for (const auto& channel : channels) {
        for (const auto& inSampleRate : sampleRates) {
            for (const auto& outSampleRate : sampleRates) {
                resampleTest(channel, inSampleRate, outSampleRate);
            }
        }
    }
}

TEST(audioflinger_bufferproviders, volumeProvider) {
    auto volumeTest = [](bool inPlace) {
        constexpr size_t frameCount = 16;
        constexpr uint32_t channels = 2;

        class UnityProvider : public TestProvider {
          public:
            UnityProvider(uint32_t channels, size_t frameCount) : TestProvider() {
                mData.resize(channels * frameCount);
                std::fill(mData.begin(), mData.end(), 1);
                mAddr = mData.data();
                mNumFrames = frameCount;
                mFrameSize = sizeof(float) * channels;
            }

            std::vector<float> mData;
        } inputProvider(channels, frameCount);

        VolumeBufferProvider volumeProvider(channels, AUDIO_FORMAT_PCM_FLOAT, frameCount, inPlace);
        volumeProvider.setBufferProvider(&inputProvider);

        AudioBufferProvider::Buffer buffer;

        // Verify initial volume is 0
        buffer.frameCount = 2;
        volumeProvider.getNextBuffer(&buffer);
        ASSERT_EQ(toVector<float>(buffer, channels), (std::vector<float>{0, 0, 0, 0}));
        volumeProvider.releaseBuffer(&buffer);

        // Increase volume (no ramp)
        buffer.frameCount = 2;
        volumeProvider.setVolume(1, false, 0);
        volumeProvider.getNextBuffer(&buffer);
        ASSERT_EQ(toVector<float>(buffer, channels), (std::vector<float>{1, 1, 1, 1}));
        volumeProvider.releaseBuffer(&buffer);

        // Decrease volume (no ramp)
        buffer.frameCount = 2;
        volumeProvider.setVolume(0.5, false, 0);
        volumeProvider.getNextBuffer(&buffer);
        ASSERT_EQ(toVector<float>(buffer, channels), (std::vector<float>{0.5, 0.5, 0.5, 0.5}));
        volumeProvider.releaseBuffer(&buffer);

        // Increase rolume (ramp)
        buffer.frameCount = 3;
        volumeProvider.setVolume(1, true, 2);
        volumeProvider.getNextBuffer(&buffer);
        ASSERT_EQ(toVector<float>(buffer, channels),
                  (std::vector<float>{0.5, 0.5, 0.75, 0.75, 1.0, 1.0}));
        volumeProvider.releaseBuffer(&buffer);

        // Decrease rolume (ramp)
        buffer.frameCount = 3;
        volumeProvider.setVolume(0.5, true, 2);
        volumeProvider.getNextBuffer(&buffer);
        ASSERT_EQ(toVector<float>(buffer, channels),
                  (std::vector<float>{1.0, 1.0, 0.75, 0.75, 0.5, 0.5}));
        volumeProvider.releaseBuffer(&buffer);

        if (!inPlace) {
            // Verify that the original buffer didn't change
            for (float val : inputProvider.mData) {
                ASSERT_EQ(val, 1);
            }
        }
    };

    volumeTest(false);
    volumeTest(true);
}

}  // namespace android
