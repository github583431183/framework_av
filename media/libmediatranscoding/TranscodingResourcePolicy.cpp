/*
 * Copyright (C) 2020 The Android Open Source Project
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
#define LOG_TAG "TranscodingResourcePolicy"

#include <aidl/android/media/BnResourceObserver.h>
#include <aidl/android/media/IResourceObserverService.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <media/TranscodingResourcePolicy.h>
#include <utils/Log.h>

namespace android {

using Status = ::ndk::ScopedAStatus;
using ::aidl::android::media::BnResourceObserver;
using ::aidl::android::media::IResourceObserverService;
using ::aidl::android::media::MediaObservableEvent;
using ::aidl::android::media::MediaObservableFilter;
using ::aidl::android::media::MediaObservableParcel;
using ::aidl::android::media::MediaObservableType;

static std::string toString(const MediaObservableParcel& observable) {
    return "{" + ::aidl::android::media::toString(observable.type) + ", " +
           std::to_string(observable.value) + "}";
}

struct TranscodingResourcePolicy::ResourceObserver : public BnResourceObserver {
    explicit ResourceObserver(TranscodingResourcePolicy* owner) : mOwner(owner) {}

    // IResourceObserver
    ::ndk::ScopedAStatus onStatusChanged(
            MediaObservableEvent event, int32_t uid, int32_t pid,
            const std::vector<MediaObservableParcel>& observables) override {
        ALOGD("%s: %s, uid %d, pid %d, %s", __FUNCTION__,
              ::aidl::android::media::toString(event).c_str(), uid, pid,
              toString(observables[0]).c_str());

        // Only report kIdle event.
        if (((uint64_t)event & (uint64_t)MediaObservableEvent::kIdle) != 0) {
            for (auto& observable : observables) {
                if (observable.type == MediaObservableType::kVideoSecureCodec ||
                    observable.type == MediaObservableType::kVideoNonSecureCodec) {
                    mOwner->onResourceAvailable(pid);
                    break;
                }
            }
        }
        return ::ndk::ScopedAStatus::ok();
    }

    TranscodingResourcePolicy* mOwner;
};

// static
void TranscodingResourcePolicy::BinderDiedCallback(void* cookie) {
    TranscodingResourcePolicy* owner = reinterpret_cast<TranscodingResourcePolicy*>(cookie);
    if (owner != nullptr) {
        owner->unregisterSelf();
    }
    // TODO(chz): retry to connecting to IResourceObserverService after failure.
    // Also need to have back-up logic if IResourceObserverService is offline for
    // Prolonged period of time. A possible alternative could be, during period where
    // IResourceObserverService is not available, trigger onResourceAvailable() everytime
    // when top uid changes (in hope that'll free up some codec instances that we could
    // reclaim).
}

TranscodingResourcePolicy::TranscodingResourcePolicy()
      : mRegistered(false),
        mResourceLostPid(-1),
        mDeathRecipient(AIBinder_DeathRecipient_new(BinderDiedCallback)) {
    registerSelf();
}

TranscodingResourcePolicy::~TranscodingResourcePolicy() {
    unregisterSelf();
}

void TranscodingResourcePolicy::registerSelf() {
    ALOGI("TranscodingResourcePolicy: registerSelf");

    ::ndk::SpAIBinder binder(AServiceManager_getService("media.resource_observer"));

    std::scoped_lock lock{mRegisteredLock};

    if (mRegistered) {
        return;
    }

    // TODO(chz): retry to connecting to IResourceObserverService after failure.
    mService = IResourceObserverService::fromBinder(binder);
    if (mService == nullptr) {
        ALOGE("Failed to get IResourceObserverService");
        return;
    }

    // Only register filters for codec resource available.
    mObserver = ::ndk::SharedRefBase::make<ResourceObserver>(this);
    std::vector<MediaObservableFilter> filters = {
            {MediaObservableType::kVideoSecureCodec, MediaObservableEvent::kIdle},
            {MediaObservableType::kVideoNonSecureCodec, MediaObservableEvent::kIdle}};

    Status status = mService->registerObserver(mObserver, filters);
    if (!status.isOk()) {
        ALOGE("failed to register: error %d", status.getServiceSpecificError());
        mService = nullptr;
        mObserver = nullptr;
        return;
    }

    AIBinder_linkToDeath(binder.get(), mDeathRecipient.get(), reinterpret_cast<void*>(this));

    ALOGD("@@@ registered observer");
    mRegistered = true;
}

void TranscodingResourcePolicy::unregisterSelf() {
    ALOGI("TranscodingResourcePolicy: unregisterSelf");

    std::scoped_lock lock{mRegisteredLock};

    if (!mRegistered) {
        return;
    }

    ::ndk::SpAIBinder binder = mService->asBinder();
    if (binder.get() != nullptr) {
        Status status = mService->unregisterObserver(mObserver);
        AIBinder_unlinkToDeath(binder.get(), mDeathRecipient.get(), reinterpret_cast<void*>(this));
    }

    mService = nullptr;
    mObserver = nullptr;
    mRegistered = false;
}

void TranscodingResourcePolicy::setCallback(
        const std::shared_ptr<ResourcePolicyCallbackInterface>& cb) {
    std::scoped_lock lock{mCallbackLock};
    mResourcePolicyCallback = cb;
}

void TranscodingResourcePolicy::setPidResourceLost(pid_t pid) {
    std::scoped_lock lock{mCallbackLock};
    mResourceLostPid = pid;
}

void TranscodingResourcePolicy::onResourceAvailable(pid_t pid) {
    std::shared_ptr<ResourcePolicyCallbackInterface> cb;
    {
        std::scoped_lock lock{mCallbackLock};
        // Only callback if codec resource is released from other processes.
        if (mResourceLostPid != -1 && mResourceLostPid != pid) {
            cb = mResourcePolicyCallback.lock();
            mResourceLostPid = -1;
        }
    }

    if (cb != nullptr) {
        cb->onResourceAvailable();
    }
}
}  // namespace android
