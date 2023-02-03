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
#define LOG_TAG "AidlConversionBassBoost"
//#define LOG_NDEBUG 0

#include <error/expected_utils.h>
#include <media/AidlConversionNdk.h>
#include <media/AidlConversionEffect.h>
#include <media/audiohal/AudioEffectUuid.h>
#include <system/audio_effects/effect_bassboost.h>

#include <utils/Log.h>

#include "AidlConversionBassBoost.h"

namespace android {
namespace effect {

using ::aidl::android::convertIntegral;
using ::aidl::android::aidl_utils::statusTFromBinderStatus;
using ::aidl::android::hardware::audio::effect::BassBoost;
using ::aidl::android::hardware::audio::effect::Parameter;
using ::android::status_t;
using utils::EffectParamReader;
using utils::EffectParamWriter;

status_t AidlConversionBassBoost::setParameter(EffectParamReader& param) {
    uint32_t type = 0;
    uint16_t value = 0;
    if (!param.validateParamValueSize(sizeof(uint32_t), sizeof(uint16_t)) ||
        OK != param.readFromParameter(&type) || OK != param.readFromValue(&value)) {
        ALOGE("%s invalid param %s", __func__, param.toString().c_str());
        return BAD_VALUE;
    }
    Parameter aidlParam;
    switch (type) {
        case BASSBOOST_PARAM_STRENGTH: {
            aidlParam = VALUE_OR_RETURN_STATUS(
                    aidl::android::legacy2aidl_uint16_strengthPm_Parameter_BassBoost(value));
            break;
        }
        case BASSBOOST_PARAM_STRENGTH_SUPPORTED: {
            ALOGW("%s set BASSBOOST_PARAM_STRENGTH_SUPPORTED not supported", __func__);
            return BAD_VALUE;
        }
        default: {
            ALOGW("%s unknown param %s", __func__, param.toString().c_str());
            return BAD_VALUE;
        }
    }

    return statusTFromBinderStatus(mEffect->setParameter(aidlParam));
}

status_t AidlConversionBassBoost::getParameter(EffectParamWriter& param) {
    uint32_t type = 0;
    if (!param.validateParamValueSize(sizeof(uint32_t), sizeof(uint16_t)) ||
        OK != param.readFromParameter(&type)) {
        ALOGE("%s invalid param %s", __func__, param.toString().c_str());
        param.setStatus(BAD_VALUE);
        return BAD_VALUE;
    }
    Parameter aidlParam;
    switch (type) {
        case BASSBOOST_PARAM_STRENGTH: {
            uint32_t value;
            Parameter::Id id =
                    MAKE_SPECIFIC_PARAMETER_ID(BassBoost, bassBoostTag, BassBoost::strengthPm);
            RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(mEffect->getParameter(id, &aidlParam)));
            value = VALUE_OR_RETURN_STATUS(
                    aidl::android::aidl2legacy_Parameter_BassBoost_uint16_strengthPm(aidlParam));
            return param.writeToValue(&value);
        }
        case BASSBOOST_PARAM_STRENGTH_SUPPORTED: {
            uint16_t value;
            const auto& cap =
                    VALUE_OR_RETURN_STATUS(aidl::android::UNION_GET(mDesc.capability, bassBoost));
            value = VALUE_OR_RETURN_STATUS(convertIntegral<uint32_t>(cap.strengthSupported));
            return param.writeToValue(&value);
        }
        default: {
            ALOGW("%s unknown param %s", __func__, param.toString().c_str());
            return BAD_VALUE;
        }
    }
}

} // namespace effect
} // namespace android
