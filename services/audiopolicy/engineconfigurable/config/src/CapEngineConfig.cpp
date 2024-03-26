/*
 * Copyright (C) 2018 The Android Open Source Project
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
#define LOG_NDEBUG 0

#include "CapEngineConfig.h"
#include <TypeConverter.h>
#include <Volume.h>
#include <cutils/properties.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
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
    legacy.configurableElement = aidl.parameter;
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
    legacy.configurableElements = aidl.parameters;
    legacy.configurations = VALUE_OR_RETURN(convertContainer<CapConfigurations>(
            aidl.configurations,
            aidl2legacy_AudioHalCapConfiguration_CapConfiguration));

    legacy.settings = VALUE_OR_RETURN(convertContainer<CapSettings>(
            aidl.capSettings,
            aidl2legacy_AudioHalCapSetting_CapSetting));

    return legacy;
}

}  // namespace

template<typename E, typename C>
struct BaseSerializerTraits {
    typedef E Element;
    typedef C Collection;
    typedef void* PtrSerializingCtx;
};

struct CapConfigurableDomainTraits : p
        ublic BaseSerializerTraits<CapConfigurableDomain, CapConfigurableDomains> {
    static constexpr const char *tag = "ConfigurableDomain";
    static constexpr const char *collectionTag = "ConfigurableDomains";

    struct Attributes {
        static constexpr const char *sequenceAware = "SequenceAware";
        static constexpr const char *name = "Name";
    };
    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &ps);
};

struct ConfigurationTraits : public BaseSerializerTraits<CapConfiguration, CapConfigurations> {
    static constexpr const char *tag = "Configuration";
    static constexpr const char *collectionTag = "Configurations";

    struct Attributes {
        static constexpr const char *name = "Name";
    };
    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &ps);
};

struct ConfigurableElementTraits :
        public BaseSerializerTraits<ConfigurableElement, ConfigurableElements> {
    static constexpr const char *tag = "ConfigurableElement";
    static constexpr const char *collectionTag = "ConfigurableElements";

    struct Attributes {
        static constexpr const char *path = "Path";
    };
    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &ps);
};

struct CapSettingTraits : public BaseSerializerTraits<CapSetting, CapSettings> {
    static constexpr const char *tag = "Configuration";
    static constexpr const char *collectionTag = "Settings";
    struct Attributes {
        static constexpr const char *name = "Name";
    };
    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &ps);
};

template <class T>
constexpr void (*xmlDeleter)(T* t);
template <>
constexpr auto xmlDeleter<xmlDoc> = xmlFreeDoc;
template <>
constexpr auto xmlDeleter<xmlChar> = [](xmlChar *s) { xmlFree(s); };

/** @return a unique_ptr with the correct deleter for the libxml2 object. */
template <class T>
constexpr auto make_xmlUnique(T *t) {
    // Wrap deleter in lambda to enable empty base optimization
    auto deleter = [](T *t) { xmlDeleter<T>(t); };
    return std::unique_ptr<T, decltype(deleter)>{t, deleter};
}

std::string getXmlAttribute(const xmlNode *cur, const char *attribute)
{
    auto charPtr = make_xmlUnique(xmlGetProp(cur, reinterpret_cast<const xmlChar *>(attribute)));
    if (charPtr == NULL) {
        return "";
    }
    std::string value(reinterpret_cast<const char*>(charPtr.get()));
    return value;
}

template <class Trait>
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection,
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
                status_t status = Trait::deserialize(doc, child, collection);
                if (status != NO_ERROR) {
                    nbSkippedElement += 1;
                }
            }
        }
        if (!xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
    }
    return NO_ERROR;
}

static constexpr const char *compoundRuleTag = "CompoundRule";
static constexpr const char *selectionCriterionRuleTag = "SelectionCriterionRule";
static constexpr const char *typeAttribute = "Type";

static constexpr const char *selectionCriterionAttribute = "SelectionCriterion";
static constexpr const char *matchesWhenAttribute = "MatchesWhen";
static constexpr const char *valueAttribute = "Value";

status_t deserializeRule(_xmlDoc *doc, const _xmlNode *cur, std::string &rule)
{
    bool isPreviousCompoundRule = true;
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        if (xmlStrcmp(cur->name, (const xmlChar *) compoundRuleTag) &&
            xmlStrcmp(cur->name, (const xmlChar *) selectionCriterionRuleTag)) {
            continue;
        }
        const xmlNode *child = cur;
        if (!xmlStrcmp(child->name, (const xmlChar *) compoundRuleTag)) {
            std::string type = getXmlAttribute(child, typeAttribute);
            if (type.empty()) {
                ALOGE("%s No attribute %s found", __func__, typeAttribute);
                return BAD_VALUE;
            }
            rule += (isPreviousCompoundRule? "" : " , " ) + type + "{";
            deserializeRule(doc, child, rule);
            rule += "}";
        }
        if (!xmlStrcmp(child->name, (const xmlChar *) selectionCriterionRuleTag)) {
            if (!isPreviousCompoundRule) {
                rule += " , ";
            }
            isPreviousCompoundRule = false;
            std::string selectionCriterion = getXmlAttribute(child, selectionCriterionAttribute);
            if (selectionCriterion.empty()) {
                ALOGE("%s No attribute %s found", __func__, selectionCriterionAttribute);
                return BAD_VALUE;
            }
            std::string matchesWhen = getXmlAttribute(child, matchesWhenAttribute);
            if (matchesWhen.empty()) {
                ALOGE("%s No attribute %s found", __func__, matchesWhenAttribute);
                return BAD_VALUE;
            }
            std::string value = getXmlAttribute(child, valueAttribute);
            if (value.empty()) {
                ALOGE("%s No attribute %s found", __func__, valueAttribute);
                return BAD_VALUE;
            }
            rule += " " + selectionCriterion + " " + matchesWhen + " " + value + " ";
        }
    }
    return NO_ERROR;
}

status_t ConfigurationTraits::deserialize(_xmlDoc *doc, const _xmlNode *child,
                                                Collection &configurations)
{
    std::string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        ALOGE("%s No attribute %s found", __func__, Attributes::name);
        return BAD_VALUE;
    }
    std::string rule;
    deserializeRule(doc, child, rule);

    configurations.push_back({name, rule});
    return NO_ERROR;
}


static constexpr const char *configurableElementTag = "ConfigurableElement";
static constexpr const char *configurableElementPathAttribute = "Path";
static constexpr const char *stringParameterTag = "StringParameter";
static constexpr const char *enumParameterTag = "EnumParameter";
static constexpr const char *bitParameterTag = "BitParameter";
static constexpr const char *fixedPointParameterTag = "FixedPointParameter";
static constexpr const char *booleanParameterTag = "BooleanParameter";
static constexpr const char *integerParameterTag = "IntegerParameter";
static constexpr const char *floatingPointParameterTag = "FloatingPointParameter";

status_t CapSettingTraits::deserialize(_xmlDoc *doc __unused, const _xmlNode *cur,
        Collection &settings)
{
    std::string configurationName = getXmlAttribute(cur, Attributes::name);
    if (configurationName.empty()) {
        ALOGE("%s No attribute %s found", __func__, Attributes::name);
        return BAD_VALUE;
    }
    ConfigurableElementValues configurableElementValues;
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        if (xmlStrcmp(cur->name, (const xmlChar *) configurableElementTag)) {
            continue;
        }
        std::string name = getXmlAttribute(cur, configurableElementPathAttribute);
        if (name.empty()) {
            ALOGE("%s No attribute %s found", __func__, configurableElementPathAttribute);
            return BAD_VALUE;
        }
        const xmlNode *child = cur->xmlChildrenNode;
        for (; child != NULL; child = child->next) {
            if (!xmlStrcmp(child->name, (const xmlChar *)stringParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)bitParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)fixedPointParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)booleanParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)integerParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)floatingPointParameterTag) ||
                    !xmlStrcmp(child->name, (const xmlChar *)enumParameterTag)) {
                auto valXml = make_xmlUnique(xmlNodeListGetString(doc, child->xmlChildrenNode, 1));
                if (valXml == NULL) {
                    return BAD_VALUE;
                }
                std::string value = reinterpret_cast<const char*>(valXml.get());
                configurableElementValues.push_back({name, value});
                break;
            }
        }
    }
    settings.push_back({configurationName, configurableElementValues});
    return NO_ERROR;
}

status_t ConfigurableElementTraits::deserialize(_xmlDoc *doc __unused, const _xmlNode *child,
                                               Collection &configurableElements)
{
    std::string path = getXmlAttribute(child, Attributes::path);
    if (path.empty()) {
        ALOGE("%s No attribute %s found", __func__, Attributes::path);
        return BAD_VALUE;
    }
    configurableElements.push_back(path);
    return NO_ERROR;
}

status_t CapConfigurableDomainTraits::deserialize(_xmlDoc *doc, const _xmlNode *child,
        Collection &domains)
{
    std::string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        ALOGE("%s No attribute %s found", __func__, Attributes::name);
        return BAD_VALUE;
    }
    bool sequenceAware = false;
    std::string sequenceAwareLiteral = getXmlAttribute(child, Attributes::sequenceAware);
    if (!sequenceAwareLiteral.empty()) {
        if (!convertTo(sequenceAwareLiteral, sequenceAware)) {
            return BAD_VALUE;
        }
    }
    size_t skipped = 0;
    CapConfigurations configurations;
    deserializeCollection<ConfigurationTraits>(doc, child, configurations, skipped);

    ConfigurableElements configurableElements;
    deserializeCollection<ConfigurableElementTraits>(doc, child, configurableElements, skipped);

    CapSettings settings;
    deserializeCollection<CapSettingTraits>(doc, child, settings, skipped);

    domains.push_back({name, configurableElements, configurations, settings});
    return NO_ERROR;
}


namespace {

class XmlErrorHandler {
public:
    XmlErrorHandler() {
        xmlSetGenericErrorFunc(this, &xmlErrorHandler);
    }
    XmlErrorHandler(const XmlErrorHandler&) = delete;
    XmlErrorHandler(XmlErrorHandler&&) = delete;
    XmlErrorHandler& operator=(const XmlErrorHandler&) = delete;
    XmlErrorHandler& operator=(XmlErrorHandler&&) = delete;
    ~XmlErrorHandler() {
        xmlSetGenericErrorFunc(NULL, NULL);
        if (!mErrorMessage.empty()) {
            ALOG(LOG_ERROR, "libxml2", "%s", mErrorMessage.c_str());
        }
    }
    static void xmlErrorHandler(void* ctx, const char* msg, ...) {
        char buffer[256];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        static_cast<XmlErrorHandler*>(ctx)->mErrorMessage += buffer;
    }
private:
    std::string mErrorMessage;
};

}  // namespace

ParsingResult parse(const char* path) {
    XmlErrorHandler errorHandler;
    auto doc = make_xmlUnique(xmlParseFile(path));
    if (doc == NULL) {
        // It is OK not to find an engine config file at the default location
        // as the caller will default to hardcoded default config
        if (strncmp(path, DEFAULT_PATH, strlen(DEFAULT_PATH))) {
            ALOGW("%s: Could not parse document %s", __FUNCTION__, path);
        }
        return {nullptr, 0};
    }
    xmlNodePtr cur = xmlDocGetRootElement(doc.get());
    if (cur == NULL) {
        ALOGE("%s: Could not parse: empty document %s", __FUNCTION__, path);
        return {nullptr, 0};
    }
    if (xmlXIncludeProcess(doc.get()) < 0) {
        ALOGE("%s: libxml failed to resolve XIncludes on document %s", __FUNCTION__, path);
        return {nullptr, 0};
    }
    std::string systemClass = getXmlAttribute(cur, gSystemClassNameAttribute);
    if (systemClass.empty() || systemClass != "Policy") {
        ALOGE("%s: No systemClass found", __func__);
        return {nullptr, 0};
    }
    size_t nbSkippedElements = 0;
    auto capConfig = std::make_unique<CapConfig>();
    deserializeCollection<CapConfigurableDomainTraits>(
                doc.get(), cur, capConfig->capConfigurableDomains, nbSkippedElements);

    return {std::move(capConfig), nbSkippedElements};
}

ParsingResult convert(const ::android::media::audio::common::AudioHalEngineConfig& aidlConfig) {
    auto config = std::make_unique<capEngineConfig::CapConfig>();

    if (auto conv = convertContainer<capEngineConfig::CapConfigurableDomains>(
                aidlConfig.capSpecificConfig.value().domains.value().domains,
                aidl2legacy_AudioHalCapDomain_CapConfigurableDomain); conv.ok()) {
        config->capConfigurableDomains = std::move(conv.value());
    } else {
        return ParsingResult{};
    }
    return {.parsedConfig=std::move(config), .nbSkippedElement=0};
}
} // namespace capEngineConfig
} // namespace android
