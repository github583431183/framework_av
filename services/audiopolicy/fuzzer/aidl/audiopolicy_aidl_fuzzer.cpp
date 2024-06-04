/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <AudioFlinger.h>
#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/media/IAudioPolicyService.h>
#include <core-mock/ConfigMock.h>
#include <core-mock/ModuleMock.h>
#include <effect-mock/FactoryMock.h>
#include <fakeservicemanager/FakeServiceManager.h>
#include <fuzzbinder/libbinder_driver.h>
#include <fuzzbinder/random_binder.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <media/IAudioFlinger.h>
#include <service/AudioPolicyService.h>

using namespace android;
using namespace android::binder;
using namespace android::hardware;
using android::fuzzService;

sp<FakeServiceManager> gFakeServiceManager;

bool addService(const String16& serviceName, const sp<FakeServiceManager>& fakeServiceManager,
                FuzzedDataProvider& fdp) {
    sp<IBinder> binder = getRandomBinder(&fdp);
    if (binder == nullptr) {
        return false;
    }
    CHECK_EQ(NO_ERROR, fakeServiceManager->addService(serviceName, binder));
    return true;
}

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
    /* Create a FakeServiceManager instance */
    gFakeServiceManager = sp<FakeServiceManager>::make();
    setDefaultServiceManager(gFakeServiceManager);

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    for (const char* service : {"activity", "sensor_privacy", "permission", "scheduling_policy",
                                "batterystats", "media.metrics"}) {
        if (!addService(String16(service), gFakeServiceManager, fdp)) {
            gFakeServiceManager->clear();
            return 0;
        }
    }

    const auto configService = ndk::SharedRefBase::make<ConfigMock>();
    CHECK_EQ(NO_ERROR, AServiceManager_addService(configService.get()->asBinder().get(),
                                                  "android.hardware.audio.core.IConfig/default"));

    const auto factoryService = ndk::SharedRefBase::make<FactoryMock>();
    CHECK_EQ(NO_ERROR,
             AServiceManager_addService(factoryService.get()->asBinder().get(),
                                        "android.hardware.audio.effect.IFactory/default"));

    const auto moduleService = ndk::SharedRefBase::make<ModuleMock>();
    CHECK_EQ(NO_ERROR, AServiceManager_addService(moduleService.get()->asBinder().get(),
                                                  "android.hardware.audio.core.IModule/default"));

    // Disable creating thread pool for fuzzer instance of audio flinger and audio policy services
    AudioSystem::disableThreadPool();

    const auto audioFlinger = sp<AudioFlinger>::make();
    const auto audioFlingerServerAdapter = sp<AudioFlingerServerAdapter>::make(audioFlinger);
    CHECK_EQ(NO_ERROR,
             gFakeServiceManager->addService(String16(IAudioFlinger::DEFAULT_SERVICE_NAME),
                                             IInterface::asBinder(audioFlingerServerAdapter),
                                             false /* allowIsolated */,
                                             IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT));

    const auto audioPolicyService = sp<AudioPolicyService>::make();
    CHECK_EQ(NO_ERROR,
             gFakeServiceManager->addService(String16("media.audio_policy"),
                                             audioPolicyService, false /* allowIsolated */,
                                             IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT));

    fuzzService(media::IAudioPolicyService::asBinder(audioPolicyService), std::move(fdp));

    audioFlinger->reset();
    gFakeServiceManager->clear();

    return 0;
}
