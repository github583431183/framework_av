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

#define LOG_TAG "AHAL_DynamicsProcessingLibEffects"

#include <android-base/logging.h>
#include <system/audio_effects/effect_uuid.h>

#include "DynamicsProcessing.h"

#include <dsp/DPBase.h>
#include <dsp/DPFrequency.h>

using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::DynamicsProcessingImpl;
using aidl::android::hardware::audio::effect::getEffectImplUuidDynamicsProcessing;
using aidl::android::hardware::audio::effect::getEffectTypeUuidDynamicsProcessing;
using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::State;
using aidl::android::media::audio::common::AudioUuid;
using aidl::android::media::audio::common::PcmType;

extern "C" binder_exception_t createEffect(const AudioUuid* in_impl_uuid,
                                           std::shared_ptr<IEffect>* instanceSpp) {
    if (!in_impl_uuid || *in_impl_uuid != getEffectImplUuidDynamicsProcessing()) {
        LOG(ERROR) << __func__ << "uuid not supported";
        return EX_ILLEGAL_ARGUMENT;
    }
    if (instanceSpp) {
        *instanceSpp = ndk::SharedRefBase::make<DynamicsProcessingImpl>();
        LOG(DEBUG) << __func__ << " instance " << instanceSpp->get() << " created";
        return EX_NONE;
    } else {
        LOG(ERROR) << __func__ << " invalid input parameter!";
        return EX_ILLEGAL_ARGUMENT;
    }
}

extern "C" binder_exception_t queryEffect(const AudioUuid* in_impl_uuid, Descriptor* _aidl_return) {
    if (!in_impl_uuid || *in_impl_uuid != getEffectImplUuidDynamicsProcessing()) {
        LOG(ERROR) << __func__ << "uuid not supported";
        return EX_ILLEGAL_ARGUMENT;
    }
    *_aidl_return = DynamicsProcessingImpl::kDescriptor;
    return EX_NONE;
}

namespace aidl::android::hardware::audio::effect {

const std::string DynamicsProcessingImpl::kEffectName = "DynamicsProcessing";

static const Range::DynamicsProcessingRange kEngineConfigRange = {
        .min = DynamicsProcessing::make<
                DynamicsProcessing::engineArchitecture>(DynamicsProcessing::EngineArchitecture(
                {.resolutionPreference =
                         DynamicsProcessing::ResolutionPreference::FAVOR_FREQUENCY_RESOLUTION,
                 .preferredProcessingDurationMs = 0,
                 .preEqStage = {.inUse = false, .bandCount = 0},
                 .postEqStage = {.inUse = false, .bandCount = 0},
                 .mbcStage = {.inUse = false, .bandCount = 0},
                 .limiterInUse = false})),
        .max = DynamicsProcessing::make<
                DynamicsProcessing::engineArchitecture>(DynamicsProcessing::EngineArchitecture(
                {.resolutionPreference =
                         DynamicsProcessing::ResolutionPreference::FAVOR_FREQUENCY_RESOLUTION,
                 .preferredProcessingDurationMs = std::numeric_limits<float>::max(),
                 .preEqStage = {.inUse = true, .bandCount = std::numeric_limits<int>::max()},
                 .postEqStage = {.inUse = true, .bandCount = std::numeric_limits<int>::max()},
                 .mbcStage = {.inUse = true, .bandCount = std::numeric_limits<int>::max()},
                 .limiterInUse = true}))};

static const DynamicsProcessing::ChannelConfig kChannelConfigMin =
        DynamicsProcessing::ChannelConfig({.channel = 0, .enable = false});

static const DynamicsProcessing::ChannelConfig kChannelConfigMax =
        DynamicsProcessing::ChannelConfig(
                {.channel = std::numeric_limits<int>::max(), .enable = true});

static const Range::DynamicsProcessingRange kPreEqChannelConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::preEq>({kChannelConfigMin}),
        .max = DynamicsProcessing::make<DynamicsProcessing::preEq>({kChannelConfigMax})};

static const Range::DynamicsProcessingRange kPostEqChannelConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::postEq>({kChannelConfigMin}),
        .max = DynamicsProcessing::make<DynamicsProcessing::postEq>({kChannelConfigMax})};

static const Range::DynamicsProcessingRange kMbcChannelConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::mbc>({kChannelConfigMin}),
        .max = DynamicsProcessing::make<DynamicsProcessing::mbc>({kChannelConfigMax})};

static const DynamicsProcessing::EqBandConfig kEqBandConfigMin =
        DynamicsProcessing::EqBandConfig({.channel = 0,
                                          .band = 0,
                                          .enable = false,
                                          .cutoffFrequencyHz = 220,
                                          .gainDb = std::numeric_limits<float>::lowest()});

static const DynamicsProcessing::EqBandConfig kEqBandConfigMax =
        DynamicsProcessing::EqBandConfig({.channel = std::numeric_limits<int>::max(),
                                          .band = std::numeric_limits<int>::max(),
                                          .enable = true,
                                          .cutoffFrequencyHz = 20000,
                                          .gainDb = std::numeric_limits<float>::max()});

static const Range::DynamicsProcessingRange kPreEqBandConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::preEqBand>({kEqBandConfigMin}),
        .max = DynamicsProcessing::make<DynamicsProcessing::preEqBand>({kEqBandConfigMax})};

static const Range::DynamicsProcessingRange kPostEqBandConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::postEqBand>({kEqBandConfigMin}),
        .max = DynamicsProcessing::make<DynamicsProcessing::postEqBand>({kEqBandConfigMax})};

static const Range::DynamicsProcessingRange kMbcBandConfigRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::mbcBand>(
                {DynamicsProcessing::MbcBandConfig(
                        {.channel = 0,
                         .band = 0,
                         .enable = false,
                         .cutoffFrequencyHz = 220,
                         .attackTimeMs = 0,
                         .releaseTimeMs = 0,
                         .ratio = 0,
                         .thresholdDb = std::numeric_limits<float>::lowest(),
                         .kneeWidthDb = 0,
                         .noiseGateThresholdDb = std::numeric_limits<float>::lowest(),
                         .expanderRatio = 0,
                         .preGainDb = std::numeric_limits<float>::lowest(),
                         .postGainDb = std::numeric_limits<float>::lowest()})}),
        .max = DynamicsProcessing::make<DynamicsProcessing::mbcBand>(
                {DynamicsProcessing::MbcBandConfig(
                        {.channel = std::numeric_limits<int>::max(),
                         .band = std::numeric_limits<int>::max(),
                         .enable = true,
                         .cutoffFrequencyHz = 20000,
                         .attackTimeMs = std::numeric_limits<float>::max(),
                         .releaseTimeMs = std::numeric_limits<float>::max(),
                         .ratio = std::numeric_limits<float>::max(),
                         .thresholdDb = 0,
                         .kneeWidthDb = std::numeric_limits<float>::max(),
                         .noiseGateThresholdDb = 0,
                         .expanderRatio = std::numeric_limits<float>::max(),
                         .preGainDb = std::numeric_limits<float>::max(),
                         .postGainDb = std::numeric_limits<float>::max()})})};

static const Range::DynamicsProcessingRange kInputGainRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::inputGain>(
                {DynamicsProcessing::InputGain(
                        {.channel = 0, .gainDb = std::numeric_limits<float>::lowest()})}),
        .max = DynamicsProcessing::make<DynamicsProcessing::inputGain>(
                {DynamicsProcessing::InputGain({.channel = std::numeric_limits<int>::max(),
                                                .gainDb = std::numeric_limits<float>::max()})})};

static const Range::DynamicsProcessingRange kLimiterRange = {
        .min = DynamicsProcessing::make<DynamicsProcessing::limiter>(
                {DynamicsProcessing::LimiterConfig(
                        {.channel = 0,
                         .enable = false,
                         .linkGroup = std::numeric_limits<int>::min(),
                         .attackTimeMs = 0,
                         .releaseTimeMs = 0,
                         .ratio = 0,
                         .thresholdDb = std::numeric_limits<float>::min(),
                         .postGainDb = std::numeric_limits<float>::min()})}),
        .max = DynamicsProcessing::make<DynamicsProcessing::limiter>(
                {DynamicsProcessing::LimiterConfig(
                        {.channel = std::numeric_limits<int>::max(),
                         .enable = true,
                         .linkGroup = std::numeric_limits<int>::max(),
                         .attackTimeMs = std::numeric_limits<float>::max(),
                         .releaseTimeMs = std::numeric_limits<float>::max(),
                         .ratio = std::numeric_limits<float>::max(),
                         .thresholdDb = 0,
                         .postGainDb = std::numeric_limits<float>::max()})})};

const std::vector<Range::DynamicsProcessingRange> kRanges = {
        kEngineConfigRange,     kPreEqChannelConfigRange, kPostEqChannelConfigRange,
        kMbcChannelConfigRange, kPreEqBandConfigRange,    kPostEqBandConfigRange,
        kMbcBandConfigRange,    kInputGainRange,          kLimiterRange};

const Capability DynamicsProcessingImpl::kCapability = {.range = kRanges};

const Descriptor DynamicsProcessingImpl::kDescriptor = {
        .common = {.id = {.type = getEffectTypeUuidDynamicsProcessing(),
                          .uuid = getEffectImplUuidDynamicsProcessing(),
                          .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::INSERT,
                             .insert = Flags::Insert::LAST,
                             .volume = Flags::Volume::CTRL},
                   .name = DynamicsProcessingImpl::kEffectName,
                   .implementor = "The Android Open Source Project"},
        .capability = DynamicsProcessingImpl::kCapability};

ndk::ScopedAStatus DynamicsProcessingImpl::open(const Parameter::Common& common,
                                                const std::optional<Parameter::Specific>& specific,
                                                OpenEffectReturn* ret) {
    LOG(DEBUG) << __func__;
    // effect only support 32bits float
    RETURN_IF(common.input.base.format.pcm != common.output.base.format.pcm ||
                      common.input.base.format.pcm != PcmType::FLOAT_32_BIT,
              EX_ILLEGAL_ARGUMENT, "dataMustBe32BitsFloat");
    RETURN_OK_IF(mState != State::INIT);
    auto context = createContext(common);
    RETURN_IF(!context, EX_NULL_POINTER, "createContextFailed");

    RETURN_IF_ASTATUS_NOT_OK(setParameterCommon(common), "setCommParamErr");
    if (specific.has_value()) {
        RETURN_IF_ASTATUS_NOT_OK(setParameterSpecific(specific.value()), "setSpecParamErr");
    } else {
        Parameter::Specific defaultSpecific =
                Parameter::Specific::make<Parameter::Specific::dynamicsProcessing>(
                        DynamicsProcessing::make<DynamicsProcessing::engineArchitecture>(
                                mContext->getEngineArchitecture()));
        RETURN_IF_ASTATUS_NOT_OK(setParameterSpecific(defaultSpecific), "setDefaultEngineErr");
    }

    mState = State::IDLE;
    context->dupeFmq(ret);
    RETURN_IF(createThread(context, getEffectName()) != RetCode::SUCCESS, EX_UNSUPPORTED_OPERATION,
              "FailedToCreateWorker");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DynamicsProcessingImpl::getDescriptor(Descriptor* _aidl_return) {
    RETURN_IF(!_aidl_return, EX_ILLEGAL_ARGUMENT, "Parameter:nullptr");
    LOG(DEBUG) << __func__ << kDescriptor.toString();
    *_aidl_return = kDescriptor;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DynamicsProcessingImpl::commandImpl(CommandId command) {
    RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");
    switch (command) {
        case CommandId::START:
            mContext->enable();
            return ndk::ScopedAStatus::ok();
        case CommandId::STOP:
            mContext->disable();
            return ndk::ScopedAStatus::ok();
        case CommandId::RESET:
            mContext->disable();
            mContext->resetBuffer();
            return ndk::ScopedAStatus::ok();
        default:
            // Need this default handling for vendor extendable CommandId::VENDOR_COMMAND_*
            LOG(ERROR) << __func__ << " commandId " << toString(command) << " not supported";
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "commandIdNotSupported");
    }
}

bool DynamicsProcessingImpl::isInputGainConfigInRange(
        const std::vector<DynamicsProcessing::InputGain>& cfgs,
        const DynamicsProcessing::InputGain& min, const DynamicsProcessing::InputGain& max) {
    auto func = [](const DynamicsProcessing::InputGain& arg) {
        return std::make_tuple(arg.channel, arg.gainDb);
    };
    return isTupleInRange(cfgs, min, max, func);
}

bool DynamicsProcessingImpl::isLimiterConfigInRange(
        const std::vector<DynamicsProcessing::LimiterConfig>& cfgs,
        const DynamicsProcessing::LimiterConfig& min,
        const DynamicsProcessing::LimiterConfig& max) {
    auto func = [](const DynamicsProcessing::LimiterConfig& arg) {
        return std::make_tuple(arg.channel, arg.enable, arg.linkGroup, arg.attackTimeMs,
                               arg.releaseTimeMs, arg.ratio, arg.thresholdDb, arg.postGainDb);
    };
    return isTupleInRange(cfgs, min, max, func);
}

bool DynamicsProcessingImpl::isMbcBandConfigInRange(
        const std::vector<DynamicsProcessing::MbcBandConfig>& cfgs,
        const DynamicsProcessing::MbcBandConfig& min,
        const DynamicsProcessing::MbcBandConfig& max) {
    auto func = [](const DynamicsProcessing::MbcBandConfig& arg) {
        return std::make_tuple(arg.channel, arg.band, arg.enable, arg.cutoffFrequencyHz,
                               arg.attackTimeMs, arg.releaseTimeMs, arg.ratio, arg.thresholdDb,
                               arg.kneeWidthDb, arg.noiseGateThresholdDb, arg.expanderRatio,
                               arg.preGainDb, arg.postGainDb);
    };
    return isTupleInRange(cfgs, min, max, func);
}

bool DynamicsProcessingImpl::isEqBandConfigInRange(
        const std::vector<DynamicsProcessing::EqBandConfig>& cfgs,
        const DynamicsProcessing::EqBandConfig& min, const DynamicsProcessing::EqBandConfig& max) {
    auto func = [](const DynamicsProcessing::EqBandConfig& arg) {
        return std::make_tuple(arg.channel, arg.band, arg.enable, arg.cutoffFrequencyHz,
                               arg.gainDb);
    };
    return isTupleInRange(cfgs, min, max, func);
}

bool DynamicsProcessingImpl::isChannelConfigInRange(
        const std::vector<DynamicsProcessing::ChannelConfig>& cfgs,
        const DynamicsProcessing::ChannelConfig& min,
        const DynamicsProcessing::ChannelConfig& max) {
    auto func = [](const DynamicsProcessing::ChannelConfig& arg) {
        return std::make_tuple(arg.channel, arg.enable);
    };
    return isTupleInRange(cfgs, min, max, func);
}

bool DynamicsProcessingImpl::isEngineConfigInRange(
        const DynamicsProcessing::EngineArchitecture& cfg,
        const DynamicsProcessing::EngineArchitecture& min,
        const DynamicsProcessing::EngineArchitecture& max) {
    auto func = [](const DynamicsProcessing::EngineArchitecture& arg) {
        return std::make_tuple(arg.resolutionPreference, arg.preferredProcessingDurationMs,
                               arg.preEqStage.inUse, arg.preEqStage.bandCount,
                               arg.postEqStage.inUse, arg.postEqStage.bandCount, arg.mbcStage.inUse,
                               arg.mbcStage.bandCount, arg.limiterInUse);
    };
    return isTupleInRange(func(cfg), func(min), func(max));
}

int DynamicsProcessingImpl::locateMinMaxForTag(DynamicsProcessing::Tag tag) {
    for (int i = 0; i < kRanges.size(); i++) {
        if (tag == kRanges[i].min.getTag() && tag == kRanges[i].max.getTag()) {
            return i;
        }
    }
    return -1;
}

bool DynamicsProcessingImpl::isParamInRange(const Parameter::Specific& specific) {
    auto& dp = specific.get<Parameter::Specific::dynamicsProcessing>();
    auto tag = dp.getTag();
    int i = locateMinMaxForTag(tag);
    if (i == -1) return true;

    switch (tag) {
        case DynamicsProcessing::engineArchitecture: {
            return isEngineConfigInRange(
                    dp.get<DynamicsProcessing::engineArchitecture>(),
                    kRanges[i].min.get<DynamicsProcessing::engineArchitecture>(),
                    kRanges[i].max.get<DynamicsProcessing::engineArchitecture>());
        }
        case DynamicsProcessing::preEq: {
            return isChannelConfigInRange(dp.get<DynamicsProcessing::preEq>(),
                                          kRanges[i].min.get<DynamicsProcessing::preEq>()[0],
                                          kRanges[i].max.get<DynamicsProcessing::preEq>()[0]);
        }
        case DynamicsProcessing::postEq: {
            return isChannelConfigInRange(dp.get<DynamicsProcessing::postEq>(),
                                          kRanges[i].min.get<DynamicsProcessing::postEq>()[0],
                                          kRanges[i].max.get<DynamicsProcessing::postEq>()[0]);
        }
        case DynamicsProcessing::mbc: {
            return isChannelConfigInRange(dp.get<DynamicsProcessing::mbc>(),
                                          kRanges[i].min.get<DynamicsProcessing::mbc>()[0],
                                          kRanges[i].max.get<DynamicsProcessing::mbc>()[0]);
        }
        case DynamicsProcessing::preEqBand: {
            return isEqBandConfigInRange(dp.get<DynamicsProcessing::preEqBand>(),
                                         kRanges[i].min.get<DynamicsProcessing::preEqBand>()[0],
                                         kRanges[i].max.get<DynamicsProcessing::preEqBand>()[0]);
        }
        case DynamicsProcessing::postEqBand: {
            return isEqBandConfigInRange(dp.get<DynamicsProcessing::postEqBand>(),
                                         kRanges[i].min.get<DynamicsProcessing::postEqBand>()[0],
                                         kRanges[i].max.get<DynamicsProcessing::postEqBand>()[0]);
        }
        case DynamicsProcessing::mbcBand: {
            return isMbcBandConfigInRange(dp.get<DynamicsProcessing::mbcBand>(),
                                          kRanges[i].min.get<DynamicsProcessing::mbcBand>()[0],
                                          kRanges[i].max.get<DynamicsProcessing::mbcBand>()[0]);
        }
        case DynamicsProcessing::limiter: {
            return isLimiterConfigInRange(dp.get<DynamicsProcessing::limiter>(),
                                          kRanges[i].min.get<DynamicsProcessing::limiter>()[0],
                                          kRanges[i].max.get<DynamicsProcessing::limiter>()[0]);
        }
        case DynamicsProcessing::inputGain: {
            return isInputGainConfigInRange(dp.get<DynamicsProcessing::inputGain>(),
                                            kRanges[i].min.get<DynamicsProcessing::inputGain>()[0],
                                            kRanges[i].max.get<DynamicsProcessing::inputGain>()[0]);
        }
        default: {
            return true;
        }
    }
    return true;
}

ndk::ScopedAStatus DynamicsProcessingImpl::setParameterSpecific(
        const Parameter::Specific& specific) {
    RETURN_IF(Parameter::Specific::dynamicsProcessing != specific.getTag(), EX_ILLEGAL_ARGUMENT,
              "EffectNotSupported");
    RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

    RETURN_IF(!isParamInRange(specific), EX_ILLEGAL_ARGUMENT, "outOfRange");
    auto& param = specific.get<Parameter::Specific::dynamicsProcessing>();
    auto tag = param.getTag();

    switch (tag) {
        case DynamicsProcessing::engineArchitecture: {
            RETURN_IF(mContext->setEngineArchitecture(
                              param.get<DynamicsProcessing::engineArchitecture>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setEngineArchitectureFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::preEq: {
            RETURN_IF(
                    mContext->setPreEq(param.get<DynamicsProcessing::preEq>()) != RetCode::SUCCESS,
                    EX_ILLEGAL_ARGUMENT, "setPreEqFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::postEq: {
            RETURN_IF(mContext->setPostEq(param.get<DynamicsProcessing::postEq>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setPostEqFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::preEqBand: {
            RETURN_IF(mContext->setPreEqBand(param.get<DynamicsProcessing::preEqBand>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setPreEqBandFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::postEqBand: {
            RETURN_IF(mContext->setPostEqBand(param.get<DynamicsProcessing::postEqBand>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setPostEqBandFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::mbc: {
            RETURN_IF(mContext->setMbc(param.get<DynamicsProcessing::mbc>()) != RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setMbcFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::mbcBand: {
            RETURN_IF(mContext->setMbcBand(param.get<DynamicsProcessing::mbcBand>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setMbcBandFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::limiter: {
            RETURN_IF(mContext->setLimiter(param.get<DynamicsProcessing::limiter>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setLimiterFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::inputGain: {
            RETURN_IF(mContext->setInputGain(param.get<DynamicsProcessing::inputGain>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setInputGainFailed");
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::vendor: {
            LOG(ERROR) << __func__ << " unsupported tag: " << toString(tag);
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
                    EX_ILLEGAL_ARGUMENT, "DPVendorExtensionTagNotSupported");
        }
    }
}

ndk::ScopedAStatus DynamicsProcessingImpl::getParameterSpecific(const Parameter::Id& id,
                                                                Parameter::Specific* specific) {
    RETURN_IF(!specific, EX_NULL_POINTER, "nullPtr");
    auto tag = id.getTag();
    RETURN_IF(Parameter::Id::dynamicsProcessingTag != tag, EX_ILLEGAL_ARGUMENT, "wrongIdTag");
    auto dpId = id.get<Parameter::Id::dynamicsProcessingTag>();
    auto dpIdTag = dpId.getTag();
    switch (dpIdTag) {
        case DynamicsProcessing::Id::commonTag:
            return getParameterDynamicsProcessing(dpId.get<DynamicsProcessing::Id::commonTag>(),
                                                  specific);
        case DynamicsProcessing::Id::vendorExtensionTag:
            LOG(ERROR) << __func__ << " unsupported ID: " << toString(dpIdTag);
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
                    EX_ILLEGAL_ARGUMENT, "DPVendorExtensionIdNotSupported");
    }
}

ndk::ScopedAStatus DynamicsProcessingImpl::getParameterDynamicsProcessing(
        const DynamicsProcessing::Tag& tag, Parameter::Specific* specific) {
    RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

    switch (tag) {
        case DynamicsProcessing::engineArchitecture: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::engineArchitecture>(
                            mContext->getEngineArchitecture()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::preEq: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::preEq>(mContext->getPreEq()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::postEq: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::postEq>(mContext->getPostEq()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::preEqBand: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::preEqBand>(
                            mContext->getPreEqBand()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::postEqBand: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::postEqBand>(
                            mContext->getPostEqBand()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::mbc: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::mbc>(mContext->getMbc()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::mbcBand: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::mbcBand>(mContext->getMbcBand()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::limiter: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::limiter>(mContext->getLimiter()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::inputGain: {
            specific->set<Parameter::Specific::dynamicsProcessing>(
                    DynamicsProcessing::make<DynamicsProcessing::inputGain>(
                            mContext->getInputGain()));
            return ndk::ScopedAStatus::ok();
        }
        case DynamicsProcessing::vendor: {
            LOG(ERROR) << __func__ << " wrong vendor tag in CommonTag: " << toString(tag);
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
                    EX_ILLEGAL_ARGUMENT, "DPVendorExtensionTagInWrongId");
        }
    }
}

std::shared_ptr<EffectContext> DynamicsProcessingImpl::createContext(
        const Parameter::Common& common) {
    if (mContext) {
        LOG(DEBUG) << __func__ << " context already exist";
        return mContext;
    }

    mContext = std::make_shared<DynamicsProcessingContext>(1 /* statusFmqDepth */, common);
    return mContext;
}

RetCode DynamicsProcessingImpl::releaseContext() {
    if (mContext) {
        mContext->disable();
        mContext->resetBuffer();
        mContext.reset();
    }
    return RetCode::SUCCESS;
}

// Processing method running in EffectWorker thread.
IEffect::Status DynamicsProcessingImpl::effectProcessImpl(float* in, float* out, int samples) {
    IEffect::Status status = {EX_NULL_POINTER, 0, 0};
    RETURN_VALUE_IF(!mContext, status, "nullContext");
    return mContext->lvmProcess(in, out, samples);
}

}  // namespace aidl::android::hardware::audio::effect
