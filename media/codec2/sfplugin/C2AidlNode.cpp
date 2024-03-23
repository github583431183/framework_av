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
#define LOG_TAG "C2AidlNode"
#include <log/log.h>
#include <private/android/AHardwareBufferHelpers.h>

#include <media/OMXBuffer.h>
#include <media/omx/1.0/Conversion.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/aidlpersistentsurface/wrapper/Conversion.h>

#include "C2NodeImpl.h"
#include "C2AidlNode.h"

namespace android {

using ::aidl::android::media::IAidlBufferSource;
using ::aidl::android::media::AidlNodeMessage;


using ::android::media::C2NodeInputBufferParams;
using ::android::media::CommandStateSet;
using ::android::media::EventDataSpaceChanged;
using ::android::media::IndexAdjustTimestamp;
using ::android::media::IndexConsumerUsageBits;
using ::android::media::IndexInputBufferParams;
using ::android::media::NodeStatusLoaded;
// Conversion
using ::android::media::aidl_conversion::toAidlStatus;

C2AidlNode::C2AidlNode(const std::shared_ptr<Codec2Client::Component> &comp)
    : mImpl(new C2NodeImpl(comp, true)) {}

// aidl ndk interfaces
::ndk::ScopedAStatus C2AidlNode::freeNode() {
    return toAidlStatus(mImpl->freeNode());
}

::ndk::ScopedAStatus C2AidlNode::sendCommand(int32_t cmd, int32_t param) {
    if (cmd == CommandStateSet && param == NodeStatusLoaded) {
        mImpl->onFirstInputFrame();
    }
    return toAidlStatus(ERROR_UNSUPPORTED);
}

::ndk::ScopedAStatus C2AidlNode::getParameter(
        int32_t index,
        const std::vector<uint8_t>& inParams,
        std::vector<uint8_t>* _aidl_return) {
    _aidl_return->resize(inParams.size());
    status_t err = ERROR_UNSUPPORTED;
    switch (index) {
        case IndexConsumerUsageBits: {
            uint64_t usage;
            if (_aidl_return->size() != sizeof(usage)) {
                ALOGE("get consumerUsage: output size does not match");
                err = BAD_VALUE;
                break;
            }
            mImpl->getConsumerUsageBits(&usage);
            ::memcpy(&_aidl_return->front(), (void *)&usage, sizeof(usage));
            err = OK;
            break;
        }
        case IndexInputBufferParams: {
            C2NodeInputBufferParams bufferParams;
            if (_aidl_return->size() != sizeof(bufferParams)) {
                ALOGE("get inputBufferParams: output size does not match");
                err = BAD_VALUE;
                break;
            }
            mImpl->getInputBufferParams(&bufferParams);
            ::memcpy(&_aidl_return->front(), (void *)&bufferParams, sizeof(bufferParams));
            err = OK;
            break;
        }
        default:
            break;
    }
    if (err != OK) {
        ALOGE("getParameter failed: index(%d), err(%d)", index, err);
    }
    return toAidlStatus(err);
}

::ndk::ScopedAStatus C2AidlNode::setParameter(
        int32_t index,
        const std::vector<uint8_t>& params) {
    switch (index) {
        case IndexAdjustTimestamp: {
            if (params.size() != sizeof(int32_t)) {
                return toAidlStatus(BAD_VALUE);
            }
            int32_t gapUs = *((int32_t *)&params[0]);
            mImpl->setAdjustTimestampGapUs(gapUs);
            return toAidlStatus(OK);
        }
        case IndexConsumerUsageBits: {
            if (params.size() != sizeof(uint64_t)) {
                return toAidlStatus(BAD_VALUE);
            }
            uint64_t usage = *((uint64_t *)&params[0]);
            mImpl->setConsumerUsageBits(usage);
            return toAidlStatus(OK);
        }
        default:
            break;
    }
    return toAidlStatus(ERROR_UNSUPPORTED);
}

::ndk::ScopedAStatus C2AidlNode::setInputSurface(
        const std::shared_ptr<IAidlBufferSource>& bufferSource) {
    return toAidlStatus(mImpl->setAidlInputSurface(bufferSource));
}

::ndk::ScopedAStatus C2AidlNode::emptyBuffer(
        int32_t buffer, const ::aidl::android::hardware::HardwareBuffer& hBuffer,
        int32_t flags, int64_t timestamp, const ::ndk::ScopedFileDescriptor& fence) {
    sp<GraphicBuffer> gBuf;
    AHardwareBuffer *ahwb = hBuffer.get();
    if (ahwb) {
        gBuf = AHardwareBuffer_to_GraphicBuffer(ahwb);
    }
    return toAidlStatus(mImpl->emptyBuffer(
            buffer, gBuf, flags, timestamp, ::dup(fence.get())));
}

::ndk::ScopedAStatus C2AidlNode::dispatchMessage(const AidlNodeMessage& msg) {
    if (msg.type != AidlNodeMessage::Type::EVENT) {
        return toAidlStatus(ERROR_UNSUPPORTED);
    }
    if (msg.data.getTag() != AidlNodeMessage::Data::eventData) {
        return toAidlStatus(ERROR_UNSUPPORTED);
    }
    if (msg.data.get<AidlNodeMessage::Data::eventData>().event != EventDataSpaceChanged) {
        return toAidlStatus(ERROR_UNSUPPORTED);
    }
    uint32_t dataspace = msg.data.get<AidlNodeMessage::Data::eventData>().data1;
    uint32_t pixelFormat = msg.data.get<AidlNodeMessage::Data::eventData>().data3;

    return toAidlStatus(mImpl->onDataspaceChanged(dataspace, pixelFormat));
}

// cpp interface

std::shared_ptr<IAidlBufferSource> C2AidlNode::getSource() {
    return mImpl->getAidlSource();
}

void C2AidlNode::setFrameSize(uint32_t width, uint32_t height) {
    return mImpl->setFrameSize(width, height);
}

void C2AidlNode::onInputBufferDone(c2_cntr64_t index) {
    return mImpl->onInputBufferDone(index);
}

android_dataspace C2AidlNode::getDataspace() {
    return mImpl->getDataspace();
}

uint32_t C2AidlNode::getPixelFormat() {
    return mImpl->getPixelFormat();
}

void C2AidlNode::setPriority(int priority) {
    return mImpl->setPriority(priority);
}

}  // namespace android
