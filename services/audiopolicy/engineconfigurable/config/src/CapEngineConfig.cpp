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

#include <cstdint>
#include <istream>
#include <map>
#include <sstream>
#include <stdarg.h>
#include <string>
#include <string>
#include <vector>

#define LOG_TAG "APM::AudioPolicyEngine/CapConfig"
//#define LOG_NDEBUG 0

#include "CapEngineConfig.h"
#include <TypeConverter.h>
#include <Volume.h>
#include <cutils/properties.h>
#include <media/AidlConversion.h>
#include <media/AidlConversionUtil.h>
#include <media/TypeConverter.h>
#include <media/convert.h>
#include <system/audio_config.h>
#include <utils/Log.h>

namespace android {

using utilities::convertTo;

namespace capEngineConfig {

static constexpr const char *gSystemClassNameAttribute = "SystemClassName";

namespace {

ConversionResult<CapConfiguration> aidl2legacy_AudioHalCapConfiguration_CapConfiguration(
        const media::audio::common::AudioHalCapConfiguration& aidl) {
    CapConfiguration legacy;
    legacy.name = aidl.name;
    legacy.rule = aidl.rule;
    return legacy;
}

ConversionResult<ConfigurableElementValue> aidl2legacy_ParameterSetting_ConfigurableElementValue(
        const media::audio::common::AudioHalCapSetting::ParameterSetting& aidl) {
    ConfigurableElementValue legacy;
    legacy.configurableElement.path = aidl.path;
    legacy.configurableElement.name = toString(aidl.name);
    legacy.value = aidl.value;
    return legacy;
}

ConversionResult<CapSetting> aidl2legacy_AudioHalCapSetting_CapSetting(
        const media::audio::common::AudioHalCapSetting& aidl) {
    CapSetting legacy;
    legacy.configurationName = aidl.configurationName;
    legacy.configurableElementValues = VALUE_OR_RETURN(convertContainer<ConfigurableElementValues>(
            aidl.parameterSettings,
            aidl2legacy_ParameterSetting_ConfigurableElementValue));
    return legacy;
}

ConversionResult<CapConfigurableDomain> aidl2legacy_AudioHalCapDomain_CapConfigurableDomain(
        const media::audio::common::AudioHalCapDomain& aidl) {
    CapConfigurableDomain legacy;
    legacy.name = aidl.name;
    legacy.configurableElementPaths = aidl.parameterPaths;
    legacy.configurations = VALUE_OR_RETURN(convertContainer<CapConfigurations>(
            aidl.configurations,
            aidl2legacy_AudioHalCapConfiguration_CapConfiguration));

    legacy.settings = VALUE_OR_RETURN(convertContainer<CapSettings>(
            aidl.capSettings,
            aidl2legacy_AudioHalCapSetting_CapSetting));

    return legacy;
}

}  // namespace

ParsingResult convert(const ::android::media::audio::common::AudioHalEngineConfig& aidlConfig) {
    auto config = std::make_unique<capEngineConfig::CapConfig>();

    if (!aidlConfig.capSpecificConfig.has_value() ||
            !aidlConfig.capSpecificConfig.value().domains.has_value()) {
        ALOGE("%s: no Cap Engine config", __func__);
        return ParsingResult{};
    }
    size_t skippedElement = 0;
    for (auto& aidlDomain: aidlConfig.capSpecificConfig.value().domains.value()) {
        if (aidlDomain.has_value()) {
            if (auto conv = aidl2legacy_AudioHalCapDomain_CapConfigurableDomain(aidlDomain.value());
                    conv.ok()) {
                config->capConfigurableDomains.push_back(std::move(conv.value()));
            } else {
                return ParsingResult{};
            }
        } else {
            skippedElement += 1;
        }
    }
    return {.parsedConfig=std::move(config), .nbSkippedElement=skippedElement};
}
} // namespace capEngineConfig
} // namespace android
