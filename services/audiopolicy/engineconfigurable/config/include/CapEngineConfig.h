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

#include <android/media/audio/common/AudioHalEngineConfig.h>

#include <string>
#include <vector>

namespace android {
namespace capEngineConfig {

using ParameterValues = std::vector<std::string>;

struct ConfigurableElement {
    std::string path;
    std::string name;
};

struct ConfigurableElementValue {
    ConfigurableElement configurableElement;
    std::string value;
};
using ConfigurableElementValues = std::vector<ConfigurableElementValue>;

struct CapSetting {
    std::string configurationName;
    ConfigurableElementValues configurableElementValues;
};
using CapSettings = std::vector<CapSetting>;

struct CapConfiguration {
    std::string name;
    std::string rule;
};

using ConfigurableElementPaths = std::vector<std::string>;
using CapConfigurations = std::vector<CapConfiguration>;

struct CapConfigurableDomain {
    std::string name;
    ConfigurableElementPaths configurableElementPaths;
    CapConfigurations configurations;
    CapSettings settings;
};

using CapConfigurableDomains = std::vector<CapConfigurableDomain>;

struct CapConfig {
    CapConfigurableDomains capConfigurableDomains;
};

/** Result of `parse(const char*)` */
struct ParsingResult {
    /** Parsed config, nullptr if the xml lib could not load the file */
    std::unique_ptr<CapConfig> parsedConfig;
    size_t nbSkippedElement; //< Number of skipped invalid product strategies
};

/** Convert the provided Cap Settings configuration.
 * @return audio policy usage @see Config
 */
ParsingResult convert(const ::android::media::audio::common::AudioHalEngineConfig& aidlConfig);

}
}
