/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "ICameraRecordingProxy"
#include <camera/CameraUtils.h>
#include <camera/ICameraRecordingProxy.h>
#include <binder/IMemory.h>
#include <binder/Parcel.h>
#include <media/hardware/HardwareAPI.h>
#include <stdint.h>
#include <utils/Log.h>

namespace android {

enum {
    START_RECORDING = IBinder::FIRST_CALL_TRANSACTION,
    STOP_RECORDING
};


class BpCameraRecordingProxy: public BpInterface<ICameraRecordingProxy>
{
public:
    explicit BpCameraRecordingProxy(const sp<IBinder>& impl)
        : BpInterface<ICameraRecordingProxy>(impl)
    {
    }

    status_t startRecording()
    {
        ALOGV("startRecording");
        Parcel data, reply;
        data.writeInterfaceToken(ICameraRecordingProxy::getInterfaceDescriptor());
        remote()->transact(START_RECORDING, data, &reply);
        return reply.readInt32();
    }

    void stopRecording()
    {
        ALOGV("stopRecording");
        Parcel data, reply;
        data.writeInterfaceToken(ICameraRecordingProxy::getInterfaceDescriptor());
        remote()->transact(STOP_RECORDING, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(CameraRecordingProxy, "android.hardware.ICameraRecordingProxy");

// ----------------------------------------------------------------------

status_t BnCameraRecordingProxy::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case START_RECORDING: {
            ALOGV("START_RECORDING");
            CHECK_INTERFACE(ICameraRecordingProxy, data, reply);
            reply->writeInt32(startRecording());
            return NO_ERROR;
        } break;
        case STOP_RECORDING: {
            ALOGV("STOP_RECORDING");
            CHECK_INTERFACE(ICameraRecordingProxy, data, reply);
            stopRecording();
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

}; // namespace android
