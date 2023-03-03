/*
 * Copyright 2023 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#define LOG_TAG "EffectProxyTest"

#include <gtest/gtest.h>
#include <utils/RefBase.h>

#include <aidl/android/media/audio/common/AudioUuid.h>
#include <android/binder_manager.h>
#include "EffectProxy.h"

/**
 * This test suite is depending on audio effect AIDL service.
 */
namespace android {

using ::aidl::android::hardware::audio::effect::CommandId;
using ::aidl::android::hardware::audio::effect::Descriptor;
using ::aidl::android::hardware::audio::effect::Flags;
using ::aidl::android::hardware::audio::effect::IEffect;
using ::aidl::android::hardware::audio::effect::IFactory;
using ::aidl::android::hardware::audio::effect::Parameter;
using ::aidl::android::hardware::audio::effect::State;
using ::aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::media::audio::common::AudioFormatDescription;
using ::aidl::android::media::audio::common::AudioFormatType;
using ::aidl::android::media::audio::common::AudioUuid;
using ::aidl::android::media::audio::common::PcmType;
using ::android::effect::EffectProxy;

class EffectProxyTest : public testing::Test {
  public:
    void SetUp() override {
        auto serviceName = std::string(IFactory::descriptor) + "/default";
        mFactory = IFactory::fromBinder(
                ndk::SpAIBinder(AServiceManager_waitForService(serviceName.c_str())));
        if (mFactory) {
            mFactory->queryEffects(std::nullopt, std::nullopt, std::nullopt, &mDescs);
        }
        for (const auto& desc : mDescs) {
            if (desc.common.id.proxy.has_value()) {
                mProxyDescs.insert({desc.common.id, desc});
            }
        }
    }

    void TearDown() override {}

    const AudioFormatDescription kDefaultFormatDescription = {
            .type = AudioFormatType::PCM, .pcm = PcmType::FLOAT_32_BIT, .encoding = ""};

    Parameter::Common createParamCommon(
            int session = 0, int ioHandle = -1, int iSampleRate = 48000, int oSampleRate = 48000,
            long iFrameCount = 0x100, long oFrameCount = 0x100,
            AudioChannelLayout inputChannelLayout =
                    AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                            AudioChannelLayout::LAYOUT_STEREO),
            AudioChannelLayout outputChannelLayout =
                    AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                            AudioChannelLayout::LAYOUT_STEREO)) {
        Parameter::Common common;
        common.session = session;
        common.ioHandle = ioHandle;

        auto& input = common.input;
        auto& output = common.output;
        input.base.sampleRate = iSampleRate;
        input.base.channelMask = inputChannelLayout;
        input.base.format = kDefaultFormatDescription;
        input.frameCount = iFrameCount;
        output.base.sampleRate = oSampleRate;
        output.base.channelMask = outputChannelLayout;
        output.base.format = kDefaultFormatDescription;
        output.frameCount = oFrameCount;
        return common;
    }

    static bool isFlagSet(const ::aidl::android::hardware::audio::effect::Descriptor& desc,
                          Flags::HardwareAccelerator flag) {
        return desc.common.flags.hwAcceleratorMode == flag;
    }

    using EffectProxyTuple = std::tuple<std::shared_ptr<EffectProxy>, std::vector<Descriptor>>;

    std::map<AudioUuid, EffectProxyTuple> createAllProxies() {
        std::map<AudioUuid, EffectProxyTuple> proxyMap;
        for (const auto& itor : mProxyDescs) {
            const auto& uuid = itor.first.proxy.value();
            if (proxyMap.end() == proxyMap.find(uuid)) {
                std::get<0>(proxyMap[uuid]) =
                        ndk::SharedRefBase::make<EffectProxy>(itor.first, mFactory);
            }
        }
        return proxyMap;
    }

    bool addAllSubEffects(std::map<AudioUuid, EffectProxyTuple> proxyMap) {
        for (auto& itor : mProxyDescs) {
            const auto& uuid = itor.first.proxy.value();
            if (proxyMap.end() == proxyMap.find(uuid)) {
                return false;
            }
            auto& proxy = std::get<0>(proxyMap[uuid]);
            if (!proxy->addSubEffect(itor.second).isOk()) {
                return false;
            }
            std::get<1>(proxyMap[uuid]).emplace_back(itor.second);
        }
        return true;
    }

    std::shared_ptr<IFactory> mFactory;
    std::vector<Descriptor> mDescs;
    std::map<Descriptor::Identity, Descriptor> mProxyDescs;
};

TEST_F(EffectProxyTest, createProxy) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    // if there are some descriptor defined with proxy, then proxyMap can not be empty
    EXPECT_EQ(mProxyDescs.size() == 0, proxyMap.size() == 0);
}

TEST_F(EffectProxyTest, addSubEffectsCreateAndDestroy) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    ASSERT_TRUE(addAllSubEffects(proxyMap));

    for (const auto& itor : proxyMap) {
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

TEST_F(EffectProxyTest, addSubEffectsCreateOpenCloseDestroy) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    EXPECT_TRUE(addAllSubEffects(proxyMap));

    Parameter::Common common = createParamCommon();
    IEffect::OpenEffectReturn ret;
    for (const auto& itor : proxyMap) {
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->open(common, std::nullopt, &ret).isOk());
        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

// Add sub-effects, set active sub-effect with different checkers
TEST_F(EffectProxyTest, setActiveEffects) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    EXPECT_TRUE(addAllSubEffects(proxyMap));

    // Any flag exist should be able to set successfully
    bool isNoneExist = false, isSimpleExist = false, isTunnelExist = false;
    for (const auto& itor : mProxyDescs) {
        isNoneExist = isNoneExist || isFlagSet(itor.second, Flags::HardwareAccelerator::NONE);
        isSimpleExist = isSimpleExist || isFlagSet(itor.second, Flags::HardwareAccelerator::SIMPLE);
        isTunnelExist = isTunnelExist || isFlagSet(itor.second, Flags::HardwareAccelerator::TUNNEL);
    }

    Parameter::Common common = createParamCommon();
    IEffect::OpenEffectReturn ret;
    for (const auto& itor : proxyMap) {
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->open(common, std::nullopt, &ret).isOk());
        EXPECT_EQ(isNoneExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                        return isFlagSet(desc, Flags::HardwareAccelerator::NONE);
                                    }).isOk());
        EXPECT_EQ(isSimpleExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::SIMPLE);
                                      }).isOk());
        EXPECT_EQ(isTunnelExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::TUNNEL);
                                      }).isOk());
        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}
TEST_F(EffectProxyTest, destroyWithoutCreate) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    ASSERT_TRUE(addAllSubEffects(proxyMap));

    for (const auto& itor : proxyMap) {
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

TEST_F(EffectProxyTest, closeWithoutOpen) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    ASSERT_TRUE(addAllSubEffects(proxyMap));

    for (const auto& itor : proxyMap) {
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());

        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

// Add sub-effects, set active sub-effect, create, open, and send command, expect success handling
TEST_F(EffectProxyTest, normalSequency) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    ASSERT_TRUE(addAllSubEffects(proxyMap));

    bool isTunnelExist = [&]() {
        for (const auto& itor : mProxyDescs) {
            if (isFlagSet(itor.second, Flags::HardwareAccelerator::TUNNEL)) {
                return true;
            }
        }
        return false;
    }();

    Parameter::Common common = createParamCommon();
    IEffect::OpenEffectReturn ret;
    Parameter::VolumeStereo volumeStereo({.left = .1f, .right = -0.8f});
    Parameter param = Parameter::make<Parameter::volumeStereo>(volumeStereo);
    Parameter::Id id = Parameter::Id::make<Parameter::Id::commonTag>(Parameter::volumeStereo);
    State state;
    for (const auto& itor : proxyMap) {
        Parameter expect;
        auto& proxy = std::get<0>(itor.second);
        ASSERT_EQ(isTunnelExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::TUNNEL);
                                      }).isOk());
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->open(common, std::nullopt, &ret).isOk());

        EXPECT_TRUE(proxy->setParameter(param).isOk());
        EXPECT_TRUE(proxy->getParameter(id, &expect).isOk());
        EXPECT_EQ(expect, param);

        EXPECT_TRUE(proxy->command(CommandId::START).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::PROCESSING, state);

        EXPECT_TRUE(proxy->command(CommandId::STOP).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::IDLE, state);

        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

// setParameter, change active sub-effect, verify with getParameter
TEST_F(EffectProxyTest, changeActiveSubAndVerifyParameter) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    EXPECT_TRUE(addAllSubEffects(proxyMap));

    bool isNoneExist = false, isSimpleExist = false, isTunnelExist = false;
    for (const auto& itor : mProxyDescs) {
        isNoneExist = isNoneExist || isFlagSet(itor.second, Flags::HardwareAccelerator::NONE);
        isSimpleExist = isSimpleExist || isFlagSet(itor.second, Flags::HardwareAccelerator::SIMPLE);
        isTunnelExist = isTunnelExist || isFlagSet(itor.second, Flags::HardwareAccelerator::TUNNEL);
    }

    Parameter::Common common = createParamCommon();
    IEffect::OpenEffectReturn ret;
    Parameter::VolumeStereo volumeStereo({.left = .5f, .right = .8f});
    Parameter param = Parameter::make<Parameter::volumeStereo>(volumeStereo);
    Parameter::Id id = Parameter::Id::make<Parameter::Id::commonTag>(Parameter::volumeStereo);
    for (const auto& itor : proxyMap) {
        Parameter expect;
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->open(common, std::nullopt, &ret).isOk());
        EXPECT_TRUE(proxy->setParameter(param).isOk());
        EXPECT_TRUE(proxy->getParameter(id, &expect).isOk());
        EXPECT_EQ(expect, param);

        EXPECT_EQ(isNoneExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                        return isFlagSet(desc, Flags::HardwareAccelerator::NONE);
                                    }).isOk());
        EXPECT_TRUE(proxy->getParameter(id, &expect).isOk());
        EXPECT_EQ(expect, param);

        EXPECT_EQ(isSimpleExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::SIMPLE);
                                      }).isOk());
        EXPECT_TRUE(proxy->getParameter(id, &expect).isOk());
        EXPECT_EQ(expect, param);

        EXPECT_EQ(isTunnelExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::TUNNEL);
                                      }).isOk());
        EXPECT_TRUE(proxy->getParameter(id, &expect).isOk());
        EXPECT_EQ(expect, param);

        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

// send command, change active sub-effect, then verify the state with getState
TEST_F(EffectProxyTest, changeActiveSubAndVerifyState) {
    ASSERT_NE(nullptr, mFactory);
    auto proxyMap = createAllProxies();
    ASSERT_TRUE(addAllSubEffects(proxyMap));

    bool isNoneExist = false, isSimpleExist = false, isTunnelExist = false;
    for (const auto& itor : mProxyDescs) {
        isNoneExist = isNoneExist || isFlagSet(itor.second, Flags::HardwareAccelerator::NONE);
        isSimpleExist = isSimpleExist || isFlagSet(itor.second, Flags::HardwareAccelerator::SIMPLE);
        isTunnelExist = isTunnelExist || isFlagSet(itor.second, Flags::HardwareAccelerator::TUNNEL);
    }

    Parameter::Common common = createParamCommon();
    IEffect::OpenEffectReturn ret;
    State state;
    for (const auto& itor : proxyMap) {
        Parameter expect;
        auto& proxy = std::get<0>(itor.second);
        EXPECT_TRUE(proxy->create().isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::INIT, state);
        EXPECT_TRUE(proxy->open(common, std::nullopt, &ret).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::IDLE, state);
        EXPECT_TRUE(proxy->command(CommandId::START).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::PROCESSING, state);

        EXPECT_EQ(isNoneExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                        return isFlagSet(desc, Flags::HardwareAccelerator::NONE);
                                    }).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::PROCESSING, state);

        EXPECT_EQ(isSimpleExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::SIMPLE);
                                      }).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::PROCESSING, state);

        EXPECT_EQ(isTunnelExist, proxy->setActiveSubEffect([](const Descriptor& desc) {
                                          return isFlagSet(desc, Flags::HardwareAccelerator::TUNNEL);
                                      }).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::PROCESSING, state);

        EXPECT_TRUE(proxy->command(CommandId::STOP).isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::IDLE, state);

        EXPECT_TRUE(proxy->close().isOk());
        EXPECT_TRUE(proxy->getState(&state).isOk());
        EXPECT_EQ(State::INIT, state);
        EXPECT_TRUE(proxy->destroy().isOk());
    }
}

} // namespace android
