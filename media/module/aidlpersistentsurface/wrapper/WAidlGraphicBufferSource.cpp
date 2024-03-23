/*
 * Copyright 2024, The Android Open Source Project
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
#define LOG_TAG "WAidlGraphicBufferSource"
#include <android/hardware_buffer_aidl.h>
#include <private/android/AHardwareBufferHelpers.h>
#include <utils/Log.h>

#include <aidl/android/media/BnAidlBufferSource.h>
#include <aidl/android/media/IAidlNode.h>

#include <media/stagefright/aidlpersistentsurface/AidlGraphicBufferSource.h>
#include <media/stagefright/aidlpersistentsurface/C2NodeDef.h>
#include <media/stagefright/aidlpersistentsurface/wrapper/WAidlGraphicBufferSource.h>
#include <media/stagefright/aidlpersistentsurface/wrapper/Conversion.h>

namespace android::media {
using ::android::binder::unique_fd;
using ::aidl::android::hardware::graphics::common::PixelFormat;
using ::aidl::android::hardware::graphics::common::Dataspace;
using ::aidl::android::media::AidlColorAspects;
using ::aidl::android::media::AidlNodeMessage;
using ::aidl::android::media::IAidlNode;
using ::aidl::android::media::BnAidlBufferSource;

// Conversion
using ::android::media::aidl_conversion::fromAidlStatus;
using ::android::media::aidl_conversion::toAidlStatus;
using ::android::media::aidl_conversion::compactFromAidlColorAspects;
using ::android::media::aidl_conversion::rawFromAidlDataspace;

struct WAidlGraphicBufferSource::WAidlNodeWrapper : public IAidlNodeWrapper {
    std::shared_ptr<IAidlNode> mNode;

    WAidlNodeWrapper(const std::shared_ptr<IAidlNode> &node): mNode(node) {
    }

    virtual status_t emptyBuffer(
            int32_t bufferId, uint32_t flags,
            const sp<GraphicBuffer> &buffer,
            int64_t timestamp, int fenceFd) override {
        AHardwareBuffer *ahwBuffer = nullptr;
        ::aidl::android::hardware::HardwareBuffer hBuffer;
        if (buffer.get()) {
            ahwBuffer = AHardwareBuffer_from_GraphicBuffer(buffer.get());
            AHardwareBuffer_acquire(ahwBuffer);
            hBuffer.reset(ahwBuffer);
        }

        ::ndk::ScopedFileDescriptor fence(fenceFd);

        return fromAidlStatus(mNode->emptyBuffer(
              bufferId,
              hBuffer,
              flags,
              timestamp,
              fence));
    }

    virtual void dispatchDataSpaceChanged(
            int32_t dataSpace, int32_t aspects, int32_t pixelFormat) override {
        AidlNodeMessage tMsg;
        tMsg.type = AidlNodeMessage::Type::EVENT;
        // tMsg.fence is null
        tMsg.data.set<AidlNodeMessage::Data::eventData>();
        tMsg.data.get<AidlNodeMessage::Data::eventData>().event =
                int32_t(EventDataSpaceChanged);
        tMsg.data.get<AidlNodeMessage::Data::eventData>().data1 = dataSpace;
        tMsg.data.get<AidlNodeMessage::Data::eventData>().data2 = aspects;
        tMsg.data.get<AidlNodeMessage::Data::eventData>().data3 = pixelFormat;
        if (!mNode->dispatchMessage(tMsg).isOk()) {
            ALOGE("WAidlNodeWrapper failed to dispatch message "
                    "EventDataSpaceChanged: "
                    "dataSpace = %ld, aspects = %ld, pixelFormat = %ld",
                    static_cast<long>(dataSpace),
                    static_cast<long>(aspects),
                    static_cast<long>(pixelFormat));
        }
    }
};

class WAidlGraphicBufferSource::WAidlBufferSource : public BnAidlBufferSource {
    sp<AidlGraphicBufferSource> mSource;

public:
    WAidlBufferSource(const sp<AidlGraphicBufferSource> &source): mSource(source) {
    }

    ::ndk::ScopedAStatus onExecuting() override {
        mSource->onExecuting();
        return ::ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus onIdle() override {
        mSource->onIdle();
        return ::ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus onLoaded() override {
        mSource->onLoaded();
        return ::ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus onInputBufferAdded(int32_t bufferId) override {
        mSource->onInputBufferAdded(bufferId);
        return ::ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus onInputBufferEmptied(
            int32_t bufferId, const ::ndk::ScopedFileDescriptor& fence) override {
        mSource->onInputBufferEmptied(bufferId, ::dup(fence.get()));
        return ::ndk::ScopedAStatus::ok();
    }
};

// WAidlGraphicBufferSource
WAidlGraphicBufferSource::WAidlGraphicBufferSource(
        sp<AidlGraphicBufferSource> const& base) :
    mBase(base),
    mBufferSource(::ndk::SharedRefBase::make<WAidlBufferSource>(base)) {
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::configure(
        const std::shared_ptr<IAidlNode>& node, Dataspace dataspace) {
    if (node == NULL) {
        return toAidlStatus(BAD_VALUE);
    }

    // Do setInputSurface() first, the node will try to enable metadata
    // mode on input, and does necessary error checking. If this fails,
    // we can't use this input surface on the node.
    ::ndk::ScopedAStatus err = node->setInputSurface(mBufferSource);
    status_t fnStatus = fromAidlStatus(err);
    if (fnStatus != NO_ERROR) {
        ALOGE("Unable to set input surface: %d", fnStatus);
        return err;
    }

    // use consumer usage bits queried from encoder, but always add
    // HW_VIDEO_ENCODER for backward compatibility.
    std::vector<uint8_t> inParams;
    std::vector<uint8_t> outParams;
    uint64_t  consumerUsage;
    fnStatus = OK;
    inParams.resize(sizeof(consumerUsage));
    memcpy(&inParams[0], (void *)&consumerUsage, sizeof(consumerUsage));
    err = node->getParameter(IndexConsumerUsageBits, inParams, &outParams);
    fnStatus = fromAidlStatus(err);
    if (fnStatus == NO_ERROR) {
        memcpy((void *)&consumerUsage, &outParams[0], sizeof(consumerUsage));
    } else {
        if (fnStatus == FAILED_TRANSACTION) {
            return err;
        }
        consumerUsage = 0;
    }

    C2NodeInputBufferParams def;
    C2NodeInputBufferParams rDef;

    inParams.clear();
    outParams.clear();
    inParams.resize(sizeof(def));
    ::memcpy(&inParams[0], (void *)&def, sizeof(def));

    err = node->getParameter(IndexInputBufferParams, inParams, &outParams);
    if (!err.isOk()) {
        return toAidlStatus(FAILED_TRANSACTION);
    }
    if (fnStatus != NO_ERROR) {
        ALOGE("Failed to get port definition: %d", fnStatus);
        return toAidlStatus(fnStatus);
    }
    ::memcpy((void *)&rDef, &outParams[0], sizeof(rDef));

    return toAidlStatus(mBase->configure(
            new WAidlNodeWrapper(node),
            rawFromAidlDataspace(dataspace),
            rDef.mBufferCountActual,
            rDef.mFrameWidth,
            rDef.mFrameHeight,
            consumerUsage));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setSuspend(
        bool suspend, int64_t timeUs) {
    return toAidlStatus(mBase->setSuspend(suspend, timeUs));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setRepeatPreviousFrameDelayUs(
        int64_t repeatAfterUs) {
    return toAidlStatus(mBase->setRepeatPreviousFrameDelayUs(repeatAfterUs));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setMaxFps(float maxFps) {
    return toAidlStatus(mBase->setMaxFps(maxFps));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setTimeLapseConfig(
        double fps, double captureFps) {
    return toAidlStatus(mBase->setTimeLapseConfig(fps, captureFps));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setStartTimeUs(int64_t startTimeUs) {
    return toAidlStatus(mBase->setStartTimeUs(startTimeUs));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setStopTimeUs(int64_t stopTimeUs) {
    return toAidlStatus(mBase->setStopTimeUs(stopTimeUs));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::getStopTimeOffsetUs(int64_t* _aidl_return) {
    status_t status = mBase->getStopTimeOffsetUs(_aidl_return);
    return toAidlStatus(status);
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setColorAspects(
        const AidlColorAspects& aspects) {
    return toAidlStatus(mBase->setColorAspects(compactFromAidlColorAspects(aspects)));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::setTimeOffsetUs(int64_t timeOffsetUs) {
    return toAidlStatus(mBase->setTimeOffsetUs(timeOffsetUs));
}

::ndk::ScopedAStatus WAidlGraphicBufferSource::signalEndOfInputStream() {
    return toAidlStatus(mBase->signalEndOfInputStream());
}


}  // namespace android::media
