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

#include <cstddef>
#define LOG_TAG "PreProcessingContext"
#include <Utils.h>

#include "PreProcessingContext.h"

namespace aidl::android::hardware::audio::effect {

using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioDeviceType;

RetCode PreProcessingContext::init(const Parameter::Common& common) {
    std::lock_guard lg(mMutex);
    webrtc::AudioProcessingBuilder apBuilder;
    mAudioProcessingModule = apBuilder.Create();
    if (mAudioProcessingModule == nullptr) {
        LOG(ERROR) << "init could not get apm engine";
        return RetCode::ERROR_EFFECT_LIB_ERROR;
    }

    updateConfigs(common);

    mEnabledMsk = 0;
    mProcessedMsk = 0;
    mRevEnabledMsk = 0;
    mRevProcessedMsk = 0;

    auto config = mAudioProcessingModule->GetConfig();
    switch (mType) {
        case PreProcessingEffectType::ACOUSTIC_ECHO_CANCELLATION:
            config.echo_canceller.mobile_mode = true;
            break;
        case PreProcessingEffectType::AUTOMATIC_GAIN_CONTROL_V2:
            config.gain_controller2.fixed_digital.gain_db = 0.f;
            break;
        case PreProcessingEffectType::NOISE_SUPPRESSION:
            config.noise_suppression.level = kNsDefaultLevel;
            break;
    }
    mAudioProcessingModule->ApplyConfig(config);
    mState = PRE_PROC_STATE_INITIALIZED;
    return RetCode::SUCCESS;
}

RetCode PreProcessingContext::deInit() {
    std::lock_guard lg(mMutex);
    mAudioProcessingModule = nullptr;
    mState = PRE_PROC_STATE_UNINITIALIZED;
    return RetCode::SUCCESS;
}

RetCode PreProcessingContext::enable() {
    if (mState != PRE_PROC_STATE_INITIALIZED) {
        return RetCode::ERROR_EFFECT_LIB_ERROR;
    }
    int typeMsk = (1 << int(mType));
    std::lock_guard lg(mMutex);
    // Check if effect is already enabled.
    if ((mEnabledMsk & typeMsk) == typeMsk) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    mEnabledMsk |= typeMsk;
    auto config = mAudioProcessingModule->GetConfig();
    switch (mType) {
        case PreProcessingEffectType::ACOUSTIC_ECHO_CANCELLATION:
            config.echo_canceller.enabled = true;
            // AEC has reverse stream
            mRevEnabledMsk |= typeMsk;
            mRevProcessedMsk = 0;
            break;
        case PreProcessingEffectType::AUTOMATIC_GAIN_CONTROL_V2:
            config.gain_controller2.enabled = true;
            break;
        case PreProcessingEffectType::NOISE_SUPPRESSION:
            config.noise_suppression.enabled = true;
            break;
    }
    mProcessedMsk = 0;
    mAudioProcessingModule->ApplyConfig(config);
    mState = PRE_PROC_STATE_ACTIVE;
    return RetCode::SUCCESS;
}

RetCode PreProcessingContext::disable() {
    if (mState != PRE_PROC_STATE_ACTIVE) {
        return RetCode::ERROR_EFFECT_LIB_ERROR;
    }
    int typeMsk = (1 << int(mType));
    std::lock_guard lg(mMutex);
    // Check if effect is already disabled.
    if ((mEnabledMsk & typeMsk) != typeMsk) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    mEnabledMsk &= ~typeMsk;
    auto config = mAudioProcessingModule->GetConfig();
    switch (mType) {
        case PreProcessingEffectType::ACOUSTIC_ECHO_CANCELLATION:
            config.echo_canceller.enabled = false;
            // AEC has reverse stream
            mRevEnabledMsk &= ~typeMsk;
            mRevProcessedMsk = 0;
            break;
        case PreProcessingEffectType::AUTOMATIC_GAIN_CONTROL_V2:
            config.gain_controller2.enabled = false;
            break;
        case PreProcessingEffectType::NOISE_SUPPRESSION:
            config.noise_suppression.enabled = false;
            break;
    }
    mProcessedMsk = 0;
    mAudioProcessingModule->ApplyConfig(config);
    mState = PRE_PROC_STATE_INITIALIZED;
    return RetCode::SUCCESS;
}

RetCode PreProcessingContext::setCommon(const Parameter::Common& common) {
    updateConfigs(common);
    LOG(INFO) << __func__ << mCommon.toString();
    return RetCode::SUCCESS;
}

void PreProcessingContext::updateConfigs(const Parameter::Common& common) {
    mInputConfig.set_sample_rate_hz(common.input.base.sampleRate);
    mInputConfig.set_num_channels(
            ::android::hardware::audio::common::getChannelCount(common.input.base.channelMask));
    mOutputConfig.set_sample_rate_hz(common.input.base.sampleRate);
    mOutputConfig.set_num_channels(
            ::android::hardware::audio::common::getChannelCount(common.output.base.channelMask));
}

RetCode PreProcessingContext::setAcousticEchoCancelerEchoDelay(int echoDelayUs) {
    if (echoDelayUs < 0 || echoDelayUs > kAcousticEchoCancelerCap.maxEchoDelayUs) {
        LOG(DEBUG) << __func__ << " illegal delay " << echoDelayUs;
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    mEchoDelayUs = echoDelayUs;
    std::lock_guard lg(mMutex);
    mAudioProcessingModule->set_stream_delay_ms(mEchoDelayUs / 1000);
    return RetCode::SUCCESS;
}

int PreProcessingContext::getAcousticEchoCancelerEchoDelay() const {
    return mEchoDelayUs;
}

RetCode PreProcessingContext::setAcousticEchoCancelerMobileMode(bool mobileMode) {
    mMobileMode = mobileMode;
    std::lock_guard lg(mMutex);
    auto config = mAudioProcessingModule->GetConfig();
    config.echo_canceller.mobile_mode = mobileMode;
    mAudioProcessingModule->ApplyConfig(config);
    return RetCode::SUCCESS;
}

bool PreProcessingContext::getAcousticEchoCancelerMobileMode() const {
    return mMobileMode;
}

RetCode PreProcessingContext::setAutomaticGainControlV2DigitalGain(int gain) {
    if (gain < 0 || gain > kAutomaticGainControlV2Cap.maxFixedDigitalGainMb) {
        LOG(DEBUG) << __func__ << " illegal digital gain " << gain;
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }
    mDigitalGain = gain;
    std::lock_guard lg(mMutex);
    auto config = mAudioProcessingModule->GetConfig();
    config.gain_controller2.fixed_digital.gain_db = mDigitalGain;
    mAudioProcessingModule->ApplyConfig(config);
    return RetCode::SUCCESS;
}

int PreProcessingContext::getAutomaticGainControlV2DigitalGain() const {
    return mDigitalGain;
}

RetCode PreProcessingContext::setNoiseSuppressionLevel(NoiseSuppression::Level level) {
    mLevel = level;
    std::lock_guard lg(mMutex);
    auto config = mAudioProcessingModule->GetConfig();
    config.noise_suppression.level =
            (webrtc::AudioProcessing::Config::NoiseSuppression::Level)level;
    mAudioProcessingModule->ApplyConfig(config);
    return RetCode::SUCCESS;
}

NoiseSuppression::Level PreProcessingContext::getNoiseSuppressionLevel() const {
    return mLevel;
}

IEffect::Status PreProcessingContext::lvmProcess(float* in, float* out, int samples) {
    IEffect::Status status = {EX_NULL_POINTER, 0, 0};
    RETURN_VALUE_IF(!in, status, "nullInput");
    RETURN_VALUE_IF(!out, status, "nullOutput");
    status = {EX_ILLEGAL_STATE, 0, 0};
    int64_t inputFrameCount = getCommon().input.frameCount;
    int64_t outputFrameCount = getCommon().output.frameCount;
    RETURN_VALUE_IF(inputFrameCount != outputFrameCount, status, "FrameCountMismatch");
    RETURN_VALUE_IF(0 == getInputFrameSize(), status, "zeroFrameSize");

    LOG(DEBUG) << __func__ << " start processing";
    std::lock_guard lg(mMutex);

    mProcessedMsk |= (1 << int(mType));

    if ((mProcessedMsk & mEnabledMsk) == mEnabledMsk) {
        mProcessedMsk = 0;
        int processStatus = mAudioProcessingModule->ProcessStream(
                (const int16_t* const)in, mInputConfig, mOutputConfig, (int16_t* const)out);
        if (processStatus != 0) {
            LOG(ERROR) << "Process Stream failed with error " << processStatus;
            return status;
        }
    }

    mRevProcessedMsk |= (1 << int(mType));

    if ((mRevProcessedMsk & mRevEnabledMsk) == mRevEnabledMsk) {
        mRevProcessedMsk = 0;
        int processStatus = mAudioProcessingModule->ProcessReverseStream(
                (const int16_t* const)in, mInputConfig, mInputConfig, (int16_t* const)out);
        if (processStatus != 0) {
            LOG(ERROR) << "Process Stream failed with error " << processStatus;
            return status;
        }
    }

    return {STATUS_OK, samples, samples};
}

}  // namespace aidl::android::hardware::audio::effect
