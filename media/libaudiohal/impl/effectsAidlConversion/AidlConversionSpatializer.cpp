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

#include <cstdint>
#include <cstring>
#include <optional>
#define LOG_TAG "AidlConversionSpatializer"
//#define LOG_NDEBUG 0

#include <aidl/android/hardware/audio/effect/DefaultExtension.h>
#include <aidl/android/hardware/audio/effect/VendorExtension.h>
#include <error/expected_utils.h>
#include <media/AidlConversionNdk.h>
#include <media/AidlConversionEffect.h>
#include <system/audio_effects/effect_spatializer.h>
#include <system/audio_effects/aidl_effects_utils.h>

#include <utils/Log.h>

#include "AidlConversionSpatializer.h"

namespace android {
namespace effect {

using aidl::android::getParameterSpecificField;
using aidl::android::aidl_utils::statusTFromBinderStatus;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::hardware::audio::effect::DefaultExtension;
using aidl::android::hardware::audio::effect::Parameter;
using aidl::android::hardware::audio::effect::Range;
using aidl::android::hardware::audio::effect::Spatializer;
using aidl::android::hardware::audio::effect::VendorExtension;
using aidl::android::media::audio::common::HeadTracking;
using aidl::android::media::audio::common::Spatialization;
using aidl::android::media::audio::common::toString;
using android::status_t;
using utils::EffectParamReader;
using utils::EffectParamWriter;

status_t AidlConversionSpatializer::setParameter(EffectParamReader& param) {
    Parameter aidlParam;
    if (mIsSpatializerAidlParamSupported) {
        uint32_t command = 0;
        if (!param.validateParamValueSize(sizeof(uint32_t), sizeof(int8_t)) ||
            OK != param.readFromParameter(&command)) {
            ALOGE("%s %d invalid param %s", __func__, __LINE__, param.toString().c_str());
            return BAD_VALUE;
        }

        switch (command) {
            case SPATIALIZER_PARAM_LEVEL: {
                Spatialization::Level level = Spatialization::Level::NONE;
                if (OK != param.readFromValue(&level)) {
                    ALOGE("%s invalid level value %s", __func__, param.toString().c_str());
                    return BAD_VALUE;
                }
                aidlParam = MAKE_SPECIFIC_PARAMETER(Spatializer, spatializer,
                                                    spatializationLevel, level);
                break;
            }
            case SPATIALIZER_PARAM_HEADTRACKING_MODE: {
                HeadTracking::Mode mode = HeadTracking::Mode::DISABLED;
                if (OK != param.readFromValue(&mode)) {
                    ALOGE("%s invalid mode value %s", __func__, param.toString().c_str());
                    return BAD_VALUE;
                }
                aidlParam = MAKE_SPECIFIC_PARAMETER(Spatializer, spatializer, headTrackingMode,
                                                    mode);
                break;
            }
            case SPATIALIZER_PARAM_HEAD_TO_STAGE: {
                const size_t valueSize = param.getValueSize();
                if (valueSize % sizeof(float) != 0) {
                    ALOGE("%s invalid parameter value size %zu", __func__, valueSize);
                    return BAD_VALUE;
                }
                std::array<float, 6> headToStage = {};
                for (size_t i = 0; i < valueSize / sizeof(float); i++) {
                    if (OK != param.readFromValue(&headToStage[i])) {
                        ALOGE("%s failed to read headToStage from %s", __func__,
                              param.toString().c_str());
                        return BAD_VALUE;
                    }
                }
                HeadTracking::SensorData sensorData =
                        HeadTracking::SensorData::make<HeadTracking::SensorData::headToStage>(
                                headToStage);
                aidlParam = MAKE_SPECIFIC_PARAMETER(Spatializer, spatializer,
                                                    headTrackingSensorData, sensorData);
                break;
            }
            case SPATIALIZER_PARAM_HEADTRACKING_CONNECTION: {
                int32_t modeInt32 = 0;
                int32_t sensorId = -1;
                if (OK != param.readFromValue(&modeInt32) || OK != param.readFromValue(&sensorId)) {
                    ALOGE("%s %d invalid parameter value %s", __func__, __LINE__,
                          param.toString().c_str());
                    return BAD_VALUE;
                }

                const auto mode = static_cast<HeadTracking::ConnectionMode>(modeInt32);
                if (mode < *ndk::enum_range<HeadTracking::ConnectionMode>().begin() ||
                    mode > *ndk::enum_range<HeadTracking::ConnectionMode>().end()) {
                    ALOGE("%s %d invalid mode %d", __func__, __LINE__, modeInt32);
                    return BAD_VALUE;
                }
                aidlParam = MAKE_SPECIFIC_PARAMETER(Spatializer, spatializer,
                                                    headTrackingConnectionMode, mode);
                if (status_t status = statusTFromBinderStatus(mEffect->setParameter(aidlParam));
                    status != OK) {
                    ALOGE("%s failed to set headTrackingConnectionMode %s", __func__,
                          toString(mode).c_str());
                    return status;
                }
                aidlParam = MAKE_SPECIFIC_PARAMETER(Spatializer, spatializer, headTrackingSensorId,
                                                    sensorId);
                break;
            }
            default: {
                ALOGE("%s %d invalid command %u", __func__, __LINE__, command);
                return BAD_VALUE;
            }
        }
    } else {
        aidlParam = VALUE_OR_RETURN_STATUS(
                ::aidl::android::legacy2aidl_EffectParameterReader_Parameter(param));
    }

    ALOGI("%s %d: %s", __func__, __LINE__,
          aidlParam.get<Parameter::specific>().toString().c_str());
    return statusTFromBinderStatus(mEffect->setParameter(aidlParam));
}

status_t AidlConversionSpatializer::getParameter(EffectParamWriter& param) {
    Parameter::Id id;
    Parameter aidlParam;
    if (mIsSpatializerAidlParamSupported) {
        uint32_t command = 0;
        if (!param.validateParamValueSize(sizeof(uint32_t), sizeof(int8_t)) ||
            OK != param.readFromParameter(&command)) {
            ALOGE("%s %d invalid param %s", __func__, __LINE__, param.toString().c_str());
            return BAD_VALUE;
        }

        switch (command) {
            case SPATIALIZER_PARAM_SUPPORTED_LEVELS: {
                const auto& range = getRange<Range::spatializer, Range::SpatializerRange>(
                        mDesc.capability, Spatializer::spatializationLevel);
                if (!range) {
                    return BAD_VALUE;
                }
                for (const auto level : ::ndk::enum_range<Spatialization::Level>()) {
                    if (const auto spatializer =
                                Spatializer::make<Spatializer::spatializationLevel>(level);
                        spatializer >= range->min && spatializer <= range->max) {
                        param.writeToValue(&level);
                    }
                }
                break;
            }
            case SPATIALIZER_PARAM_LEVEL: {
                id = MAKE_SPECIFIC_PARAMETER_ID(Spatializer, spatializerTag,
                                                Spatializer::spatializationLevel);
                RETURN_STATUS_IF_ERROR(
                        statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
                const auto level = VALUE_OR_RETURN_STATUS(GET_PARAMETER_SPECIFIC_FIELD(
                        aidlParam, Spatializer, spatializer, Spatializer::spatializationLevel,
                        Spatialization::Level));
                param.writeToValue(&level);
                break;
            }
            case SPATIALIZER_PARAM_HEADTRACKING_SUPPORTED: {
                // check capability and see if HeadTrack::Mode have more than DISABLE
                const bool support = isRangeValid<Range::spatializer>(Spatializer::headTrackingMode,
                                                                      mDesc.capability) &&
                                     !isTheOnlySupportedCapability<Range::spatializer>(
                                             mDesc.capability, Spatializer::headTrackingMode,
                                             Spatializer::make<Spatializer::headTrackingMode>(
                                                     HeadTracking::Mode::DISABLED));
                param.writeToValue(&support);
                break;
            }
            case SPATIALIZER_PARAM_HEADTRACKING_MODE: {
                id = MAKE_SPECIFIC_PARAMETER_ID(Spatializer, spatializerTag,
                                                Spatializer::headTrackingMode);
                RETURN_STATUS_IF_ERROR(
                        statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
                const auto mode = VALUE_OR_RETURN_STATUS(GET_PARAMETER_SPECIFIC_FIELD(
                        aidlParam, Spatializer, spatializer, Spatializer::headTrackingMode,
                        HeadTracking::Mode));
                param.writeToValue(&mode);
                break;
            }
            case SPATIALIZER_PARAM_SUPPORTED_CHANNEL_MASKS: {
                id = MAKE_SPECIFIC_PARAMETER_ID(Spatializer, spatializerTag,
                                                Spatializer::supportedChannelLayout);
                RETURN_STATUS_IF_ERROR(
                        statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
                const auto& supportedLayouts = VALUE_OR_RETURN_STATUS(GET_PARAMETER_SPECIFIC_FIELD(
                        aidlParam, Spatializer, spatializer, Spatializer::supportedChannelLayout,
                        std::vector<aidl::android::media::audio::common::AudioChannelLayout>));
                param.writeToValue(&supportedLayouts);
                break;
            }
            case SPATIALIZER_PARAM_SUPPORTED_SPATIALIZATION_MODES: {
                const auto& range = getRange<Range::spatializer, Range::SpatializerRange>(
                        mDesc.capability, Spatializer::spatializationMode);
                if (!range) {
                    return BAD_VALUE;
                }
                for (const auto mode : ::ndk::enum_range<Spatialization::Mode>()) {
                    if (const auto spatializer =
                                Spatializer::make<Spatializer::spatializationMode>(mode);
                        spatializer >= range->min && spatializer <= range->max) {
                        param.writeToValue(&mode);
                    }
                }
                break;
            }
            case SPATIALIZER_PARAM_SUPPORTED_HEADTRACKING_CONNECTION: {
                const auto& range = getRange<Range::spatializer, Range::SpatializerRange>(
                        mDesc.capability, Spatializer::spatializationMode);
                if (!range) {
                    return BAD_VALUE;
                }
                for (const auto mode : ::ndk::enum_range<HeadTracking::ConnectionMode>()) {
                    if (const auto spatializer =
                                Spatializer::make<Spatializer::headTrackingConnectionMode>(mode);
                        spatializer >= range->min && spatializer <= range->max) {
                        param.writeToValue(&mode);
                    }
                }
                break;
            }
            case SPATIALIZER_PARAM_HEADTRACKING_CONNECTION: {
                id = MAKE_SPECIFIC_PARAMETER_ID(Spatializer, spatializerTag,
                                                Spatializer::headTrackingConnectionMode);
                RETURN_STATUS_IF_ERROR(
                        statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
                const auto mode = VALUE_OR_RETURN_STATUS(GET_PARAMETER_SPECIFIC_FIELD(
                        aidlParam, Spatializer, spatializer,
                        Spatializer::headTrackingConnectionMode, HeadTracking::ConnectionMode));

                id = MAKE_SPECIFIC_PARAMETER_ID(Spatializer, spatializerTag,
                                                Spatializer::headTrackingSensorId);
                RETURN_STATUS_IF_ERROR(
                        statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
                const auto sensorId = VALUE_OR_RETURN_STATUS(GET_PARAMETER_SPECIFIC_FIELD(
                        aidlParam, Spatializer, spatializer,
                        Spatializer::headTrackingSensorId, int32_t));
                const auto modeInt32 = static_cast<int32_t>(mode);
                param.writeToValue(&modeInt32);
                param.writeToValue(&sensorId);
                break;
            }
            default: {
                ALOGE("%s %d invalid command %u", __func__, __LINE__, command);
                return BAD_VALUE;
            }
        }
        ALOGI("%s %d: %s", __func__, __LINE__,
              aidlParam.get<Parameter::specific>().toString().c_str());
        return OK;
    } else {
        DefaultExtension defaultExt;
        // read parameters into DefaultExtension vector<uint8_t>
        defaultExt.bytes.resize(param.getParameterSize());
        if (OK != param.readFromParameter(defaultExt.bytes.data(), param.getParameterSize())) {
            ALOGE("%s %d invalid param %s", __func__, __LINE__, param.toString().c_str());
            param.setStatus(BAD_VALUE);
            return BAD_VALUE;
        }

        VendorExtension idTag;
        idTag.extension.setParcelable(defaultExt);
        id = UNION_MAKE(Parameter::Id, vendorEffectTag, idTag);
        RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
        ALOGI("%s %d: %s", __func__, __LINE__,
              aidlParam.get<Parameter::specific>().toString().c_str());
        // copy the AIDL extension data back to effect_param_t
        return VALUE_OR_RETURN_STATUS(
                ::aidl::android::aidl2legacy_Parameter_EffectParameterWriter(aidlParam, param));
    }
}

} // namespace effect
} // namespace android
