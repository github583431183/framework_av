/*
 * Copyright 2021 The Android Open Source Project
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
#include "android/binder_auto_utils.h"
#define LOG_TAG "Codec2-Component-Aidl"
#include <android-base/logging.h>

#include <codec2/aidl/Component.h>
#include <codec2/aidl/ComponentStore.h>
#include <codec2/aidl/InputBufferManager.h>

#ifndef __ANDROID_APEX__
#include <FilterWrapper.h>
#endif

#include <utils/Timers.h>

#include <C2Debug.h>
#include <C2PlatformSupport.h>

#include <chrono>
#include <thread>

namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
namespace utils {

using ::aidl::android::hardware::common::NativeHandle;
using ::aidl::android::hardware::media::bufferpool2::IClientManager;
using ::ndk::ScopedAStatus;

// ComponentListener wrapper
struct Component::Listener : public C2Component::Listener {

    Listener(const std::shared_ptr<Component>& component) :
        mComponent(component),
        mListener(component->mListener) {
    }

    virtual void onError_nb(
            std::weak_ptr<C2Component> /* c2component */,
            uint32_t errorCode) override {
        std::shared_ptr<IComponentListener> listener = mListener.lock();
        if (listener) {
            ScopedAStatus transStatus = listener->onError(Status{Status::OK}, errorCode);
            if (!transStatus.isOk()) {
                LOG(ERROR) << "Component::Listener::onError_nb -- "
                           << "transaction failed.";
            }
        }
    }

    virtual void onTripped_nb(
            std::weak_ptr<C2Component> /* c2component */,
            std::vector<std::shared_ptr<C2SettingResult>> c2settingResult
            ) override {
      std::shared_ptr<IComponentListener> listener = mListener.lock();
        if (listener) {
            std::vector<SettingResult> settingResults(c2settingResult.size());
            size_t ix = 0;
            for (const std::shared_ptr<C2SettingResult> &c2result :
                    c2settingResult) {
                if (c2result) {
                    if (!ToAidl(&settingResults[ix++], *c2result)) {
                        break;
                    }
                }
            }
            settingResults.resize(ix);
            ScopedAStatus transStatus = listener->onTripped(settingResults);
            if (!transStatus.isOk()) {
                LOG(ERROR) << "Component::Listener::onTripped_nb -- "
                           << "transaction failed.";
            }
        }
    }

    virtual void onWorkDone_nb(
            std::weak_ptr<C2Component> /* c2component */,
            std::list<std::unique_ptr<C2Work>> c2workItems) override {
        // TODO
        (void) c2workItems;
        return;

//        for (const std::unique_ptr<C2Work>& work : c2workItems) {
//            if (work) {
//                if (work->worklets.empty()
//                        || !work->worklets.back()
//                        || (work->worklets.back()->output.flags &
//                            C2FrameData::FLAG_INCOMPLETE) == 0) {
//                    InputBufferManager::
//                            unregisterFrameData(mListener, work->input);
//                }
//            }
//        }
//
//        std::shared_ptr<IComponentListener> listener = mListener.lock();
//        if (listener) {
//            WorkBundle workBundle;
//
//            std::shared_ptr<Component> strongComponent = mComponent.lock();
//            beginTransferBufferQueueBlocks(c2workItems, true);
//            if (!objcpy(&workBundle, c2workItems, strongComponent ?
//                    &strongComponent->mBufferPoolSender : nullptr)) {
//                LOG(ERROR) << "Component::Listener::onWorkDone_nb -- "
//                           << "received corrupted work items.";
//                endTransferBufferQueueBlocks(c2workItems, false, true);
//                return;
//            }
//            Return<void> transStatus = listener->onWorkDone(workBundle);
//            if (!transStatus.isOk()) {
//                LOG(ERROR) << "Component::Listener::onWorkDone_nb -- "
//                           << "transaction failed.";
//                endTransferBufferQueueBlocks(c2workItems, false, true);
//                return;
//            }
//            endTransferBufferQueueBlocks(c2workItems, true, true);
//        }
    }

protected:
    std::weak_ptr<Component> mComponent;
    std::weak_ptr<IComponentListener> mListener;
};

// Component::DeathContext
struct Component::DeathContext {
    std::weak_ptr<Component> mWeakComp;
};

// Component
Component::Component(
        const std::shared_ptr<C2Component>& component,
        const std::shared_ptr<IComponentListener>& listener,
        const std::shared_ptr<ComponentStore>& store,
        const std::shared_ptr<IClientManager>& clientPoolManager)
      : mComponent{component},
        mInterface{SharedRefBase::make<ComponentInterface>(
                component->intf(), store->getParameterCache())},
        mListener{listener},
        mStore{store},
        mDeathContext(nullptr) {
        // mBufferPoolSender{clientPoolManager} {
    // TODO
    (void) clientPoolManager;
    // Retrieve supported parameters from store
    // TODO: We could cache this per component/interface type
    mInit = mInterface->status();
}

c2_status_t Component::status() const {
    return mInit;
}

// Methods from ::android::hardware::media::c2::V1_1::IComponent
ScopedAStatus Component::queue(const WorkBundle& workBundle) {
    // TODO
    (void) workBundle;
    return ScopedAStatus::fromServiceSpecificError(Status::OMITTED);
//    std::list<std::unique_ptr<C2Work>> c2works;
//
//    if (!FromAidl(&c2works, workBundle)) {
//        return Status::CORRUPTED;
//    }
//
//    // Register input buffers.
//    for (const std::unique_ptr<C2Work>& work : c2works) {
//        if (work) {
//            InputBufferManager::
//                    registerFrameData(mListener, work->input);
//        }
//    }
//
//    return static_cast<Status>(mComponent->queue_nb(&c2works));
}

ScopedAStatus Component::flush(WorkBundle *) {
    // TODO
    return ScopedAStatus::fromServiceSpecificError(Status::OMITTED);
//    std::list<std::unique_ptr<C2Work>> c2flushedWorks;
//    c2_status_t c2res = mComponent->flush_sm(
//            C2Component::FLUSH_COMPONENT,
//            &c2flushedWorks);
//
//    // Unregister input buffers.
//    for (const std::unique_ptr<C2Work>& work : c2flushedWorks) {
//        if (work) {
//            if (work->worklets.empty()
//                    || !work->worklets.back()
//                    || (work->worklets.back()->output.flags &
//                        C2FrameData::FLAG_INCOMPLETE) == 0) {
//                InputBufferManager::
//                        unregisterFrameData(mListener, work->input);
//            }
//        }
//    }
//
//    WorkBundle flushedWorkBundle;
//    Status res = static_cast<Status>(c2res);
//    beginTransferBufferQueueBlocks(c2flushedWorks, true);
//    if (c2res == C2_OK) {
//        if (!objcpy(&flushedWorkBundle, c2flushedWorks, &mBufferPoolSender)) {
//            res = Status::CORRUPTED;
//        }
//    }
//    _hidl_cb(res, flushedWorkBundle);
//    endTransferBufferQueueBlocks(c2flushedWorks, true, true);
//    return Void();
}

ScopedAStatus Component::drain(bool withEos) {
    c2_status_t res = mComponent->drain_nb(withEos ?
            C2Component::DRAIN_COMPONENT_WITH_EOS :
            C2Component::DRAIN_COMPONENT_NO_EOS);
    if (res == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(res);
}

//Return<Status> Component::setOutputSurface(
//        uint64_t blockPoolId,
//        const sp<HGraphicBufferProducer2>& surface) {
//    std::shared_ptr<C2BlockPool> pool;
//    GetCodec2BlockPool(blockPoolId, mComponent, &pool);
//    if (pool && pool->getAllocatorId() == C2PlatformAllocatorStore::BUFFERQUEUE) {
//        std::shared_ptr<C2BufferQueueBlockPool> bqPool =
//                std::static_pointer_cast<C2BufferQueueBlockPool>(pool);
//        C2BufferQueueBlockPool::OnRenderCallback cb =
//            [this](uint64_t producer, int32_t slot, int64_t nsecs) {
//                // TODO: batch this
//                hidl_vec<IComponentListener::RenderedFrame> rendered;
//                rendered.resize(1);
//                rendered[0] = { producer, slot, nsecs };
//                (void)mListener->onFramesRendered(rendered).isOk();
//        };
//        if (bqPool) {
//            bqPool->setRenderCallback(cb);
//            bqPool->configureProducer(surface);
//        }
//    }
//    return Status::OK;
//}

namespace /* unnamed */ {

struct BlockPoolIntf : public ConfigurableC2Intf {
    BlockPoolIntf(const std::shared_ptr<C2BlockPool>& pool)
          : ConfigurableC2Intf{
                "C2BlockPool:" +
                    (pool ? std::to_string(pool->getLocalId()) : "null"),
                0},
            mPool{pool} {
    }

    virtual c2_status_t config(
            const std::vector<C2Param*>& params,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2SettingResult>>* const failures
            ) override {
        (void)params;
        (void)mayBlock;
        (void)failures;
        return C2_OK;
    }

    virtual c2_status_t query(
            const std::vector<C2Param::Index>& indices,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2Param>>* const params
            ) const override {
        (void)indices;
        (void)mayBlock;
        (void)params;
        return C2_OK;
    }

    virtual c2_status_t querySupportedParams(
            std::vector<std::shared_ptr<C2ParamDescriptor>>* const params
            ) const override {
        (void)params;
        return C2_OK;
    }

    virtual c2_status_t querySupportedValues(
            std::vector<C2FieldSupportedValuesQuery>& fields,
            c2_blocking_t mayBlock) const override {
        (void)fields;
        (void)mayBlock;
        return C2_OK;
    }

protected:
    std::shared_ptr<C2BlockPool> mPool;
};

} // unnamed namespace

ScopedAStatus Component::createBlockPool(
        int32_t allocatorId,
        IComponent::BlockPool *blockPool) {
    std::shared_ptr<C2BlockPool> c2BlockPool;
#ifdef __ANDROID_APEX__
    c2_status_t status = CreateCodec2BlockPool(
            static_cast<C2PlatformAllocatorStore::id_t>(allocatorId),
            mComponent,
            &c2BlockPool);
#else
    c2_status_t status = ComponentStore::GetFilterWrapper()->createBlockPool(
            static_cast<::android::C2PlatformAllocatorStore::id_t>(allocatorId),
            mComponent,
            &c2BlockPool);
#endif
    if (status != C2_OK) {
        blockPool = nullptr;
    }
    if (blockPool) {
        mBlockPoolsMutex.lock();
        mBlockPools.emplace(c2BlockPool->getLocalId(), blockPool);
        mBlockPoolsMutex.unlock();
    } else if (status == C2_OK) {
        status = C2_CORRUPTED;
    }

    blockPool->blockPoolId = c2BlockPool ? c2BlockPool->getLocalId() : 0;
    blockPool->configurable = SharedRefBase::make<CachedConfigurable>(
            std::make_unique<BlockPoolIntf>(c2BlockPool));
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus Component::destroyBlockPool(int64_t blockPoolId) {
    std::lock_guard<std::mutex> lock(mBlockPoolsMutex);
    if (mBlockPools.erase(blockPoolId) == 1) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(Status::CORRUPTED);
}

ScopedAStatus Component::start() {
    c2_status_t status = mComponent->start();
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus Component::stop() {
    InputBufferManager::unregisterFrameData(mListener);
    c2_status_t status = mComponent->stop();
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus Component::reset() {
    c2_status_t status = mComponent->reset();
    {
        std::lock_guard<std::mutex> lock(mBlockPoolsMutex);
        mBlockPools.clear();
    }
    InputBufferManager::unregisterFrameData(mListener);
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus Component::release() {
    c2_status_t status = mComponent->release();
    {
        std::lock_guard<std::mutex> lock(mBlockPoolsMutex);
        mBlockPools.clear();
    }
    InputBufferManager::unregisterFrameData(mListener);
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus Component::getInterface(
        std::shared_ptr<IComponentInterface> *intf) {
    *intf = mInterface;
    return ScopedAStatus::ok();
}

ScopedAStatus Component::configureVideoTunnel(
        int32_t avSyncHwId, NativeHandle *handle) {
    (void)avSyncHwId;
    (void)handle;
    return ScopedAStatus::fromServiceSpecificError(Status::OMITTED);
}

void Component::initListener(const std::shared_ptr<Component>& self) {
    std::shared_ptr<C2Component::Listener> c2listener =
            std::make_shared<Listener>(self);
    c2_status_t res = mComponent->setListener_vb(c2listener, C2_DONT_BLOCK);
    if (res != C2_OK) {
        mInit = res;
    }

    mDeathRecipient = ::ndk::ScopedAIBinder_DeathRecipient(
            AIBinder_DeathRecipient_new(OnBinderDied));
    mDeathContext = new DeathContext{weak_from_this()};
    AIBinder_DeathRecipient_setOnUnlinked(mDeathRecipient.get(), OnBinderUnlinked);
    AIBinder_linkToDeath(mListener->asBinder().get(), mDeathRecipient.get(), mDeathContext);
}

// static
void Component::OnBinderDied(void *cookie) {
    DeathContext *context = (DeathContext *)cookie;
    std::shared_ptr<Component> comp = context->mWeakComp.lock();
    if (comp) {
        comp->release();
    }
}

// static
void Component::OnBinderUnlinked(void *cookie) {
    delete (DeathContext *)cookie;
}

Component::~Component() {
    InputBufferManager::unregisterFrameData(mListener);
    mStore->reportComponentDeath(this);
    if (mDeathRecipient.get()) {
        AIBinder_unlinkToDeath(mListener->asBinder().get(), mDeathRecipient.get(), mDeathContext);
    }
}

} // namespace utils
} // namespace c2
} // namespace media
} // namespace hardware
} // namespace android
} // namespace aidl
