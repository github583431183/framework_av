/*
 * Copyright (C) 2006-2007 The Android Open Source Project
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

#define LOG_TAG "AudioSystem"
//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <android/media/IAudioPolicyService.h>
#include <android/media/AudioMixUpdate.h>
#include <android/media/BnCaptureStateListener.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <media/AidlConversion.h>
#include <media/AudioResamplerPublic.h>
#include <media/AudioSystem.h>
#include <media/IAudioFlinger.h>
#include <media/PolicyAidlConversion.h>
#include <media/TypeConverter.h>
#include <math.h>

#include <system/audio.h>
#include <android/media/GetInputForAttrResponse.h>
#include <android/media/AudioMixerAttributesInternal.h>

#define VALUE_OR_RETURN_BINDER_STATUS(x) \
    ({ auto _tmp = (x); \
       if (!_tmp.ok()) return aidl_utils::binderStatusFromStatusT(_tmp.error()); \
       std::move(_tmp.value()); })

// ----------------------------------------------------------------------------

namespace android {
using aidl_utils::statusTFromBinderStatus;
using binder::Status;
using content::AttributionSourceState;
using media::IAudioPolicyService;
using media::audio::common::AudioConfig;
using media::audio::common::AudioConfigBase;
using media::audio::common::AudioDevice;
using media::audio::common::AudioDeviceDescription;
using media::audio::common::AudioFormatDescription;
using media::audio::common::AudioMMapPolicyInfo;
using media::audio::common::AudioMMapPolicyType;
using media::audio::common::AudioOffloadInfo;
using media::audio::common::AudioSource;
using media::audio::common::AudioStreamType;
using media::audio::common::AudioUsage;
using media::audio::common::Int;

std::mutex AudioSystem::gMutex;
dynamic_policy_callback AudioSystem::gDynPolicyCallback = NULL;
record_config_callback AudioSystem::gRecordConfigCallback = NULL;
routing_callback AudioSystem::gRoutingCallback = NULL;
vol_range_init_req_callback AudioSystem::gVolRangeInitReqCallback = NULL;

std::mutex AudioSystem::gApsCallbackMutex;
std::mutex AudioSystem::gErrorCallbacksMutex;
std::set<audio_error_callback> AudioSystem::gAudioErrorCallbacks;

std::mutex AudioSystem::gSoundTriggerMutex;
sp<CaptureStateListenerImpl> AudioSystem::gSoundTriggerCaptureStateListener;

// Sets the Binder for the AudioFlinger service, passed to this client process
// from the system server.
// This allows specific isolated processes to access the audio system. Currently used only for the
// HotwordDetectionService.
template <typename ServiceInterface, typename Client, typename AidlInterface,
        typename ServiceTraits>
class ServiceHandler {
public:
    sp<ServiceInterface> getService(bool canStartThreadPool = true)
            EXCLUDES(mMutex) NO_THREAD_SAFETY_ANALYSIS {  // std::unique_ptr
        sp<ServiceInterface> service;
        sp<Client> client;

        bool reportNoError = false;
        {
            std::lock_guard _l(mMutex);
            if (mService != nullptr) {
                return mService;
            }
        }

        std::unique_lock ul_only1thread(mSingleGetter);
        std::unique_lock ul(mMutex);
        if (mService != nullptr) {
            return mService;
        }
        if (mClient == nullptr) {
            mClient = sp<Client>::make();
        } else {
            reportNoError = true;
        }
        while (true) {
            mService = mLocalService;
            if (mService != nullptr) break;

            sp<IBinder> binder = mBinder;
            if (binder == nullptr) {
                sp <IServiceManager> sm = defaultServiceManager();
                binder = sm->checkService(String16(ServiceTraits::SERVICE_NAME));
                if (binder == nullptr) {
                    ALOGD("%s: waiting for %s", __func__, ServiceTraits::SERVICE_NAME);

                    // if the condition variable is present, setLocalService() and
                    // setBinder() is allowed to use it to notify us.
                    if (mCvGetter == nullptr) {
                        mCvGetter = std::make_shared<std::condition_variable>();
                    }
                    mCvGetter->wait_for(ul, std::chrono::seconds(1));
                    continue;
                }
            }
            binder->linkToDeath(mClient);
            auto aidlInterface = interface_cast<AidlInterface>(binder);
            LOG_ALWAYS_FATAL_IF(aidlInterface == nullptr);
            if constexpr (std::is_same_v<ServiceInterface, AidlInterface>) {
                mService = std::move(aidlInterface);
            } else /* constexpr */ {
                mService = ServiceTraits::createServiceAdapter(aidlInterface);
            }
            break;
        }
        if (mCvGetter) mCvGetter.reset();  // remove condition variable.
        client = mClient;
        service = mService;
        // Make sure callbacks can be received by the client
        if (canStartThreadPool) {
            ProcessState::self()->startThreadPool();
        }
        ul.unlock();
        ul_only1thread.unlock();
        ServiceTraits::onServiceCreate(service, client);
        if (reportNoError) AudioSystem::reportError(NO_ERROR);
        return service;
    }

    status_t setLocalService(const sp<ServiceInterface>& service) EXCLUDES(mMutex) {
        std::lock_guard _l(mMutex);
        // we allow clearing once set, but not a double non-null set.
        if (mService != nullptr && service != nullptr) return INVALID_OPERATION;
        mLocalService = service;
        if (mCvGetter) mCvGetter->notify_one();
        return OK;
    }

    sp<Client> getClient() EXCLUDES(mMutex)  {
        const auto service = getService();
        if (service == nullptr) return nullptr;
        std::lock_guard _l(mMutex);
        return mClient;
    }

    void setBinder(const sp<IBinder>& binder) EXCLUDES(mMutex)  {
        std::lock_guard _l(mMutex);
        if (mService != nullptr) {
            ALOGW("%s: ignoring; %s connection already established.",
                    __func__, ServiceTraits::SERVICE_NAME);
            return;
        }
        mBinder = binder;
        if (mCvGetter) mCvGetter->notify_one();
    }

    void clearService() EXCLUDES(mMutex)  {
        std::lock_guard _l(mMutex);
        mService.clear();
        if (mClient) ServiceTraits::onClearService(mClient);
    }

private:
    std::mutex mSingleGetter;
    std::mutex mMutex;
    std::shared_ptr<std::condition_variable> mCvGetter GUARDED_BY(mMutex);
    sp<IBinder> mBinder GUARDED_BY(mMutex);
    sp<ServiceInterface> mLocalService GUARDED_BY(mMutex);
    sp<ServiceInterface> mService GUARDED_BY(mMutex);
    sp<Client> mClient GUARDED_BY(mMutex);
};

struct AudioFlingerTraits {
    static void onServiceCreate(
            const sp<IAudioFlinger>& af, const sp<AudioSystem::AudioFlingerClient>& afc) {
        const int64_t token = IPCThreadState::self()->clearCallingIdentity();
        af->registerClient(afc);
        IPCThreadState::self()->restoreCallingIdentity(token);
    }

    static sp<IAudioFlinger> createServiceAdapter(
            const sp<media::IAudioFlingerService>& aidlInterface) {
        return sp<AudioFlingerClientAdapter>::make(aidlInterface);
    }

    static void onClearService(const sp<AudioSystem::AudioFlingerClient>& afc) {
        afc->clearIoCache();
    }

    static constexpr const char* SERVICE_NAME = IAudioFlinger::DEFAULT_SERVICE_NAME;
};

[[clang::no_destroy]] static constinit ServiceHandler<IAudioFlinger,
        AudioSystem::AudioFlingerClient, media::IAudioFlingerService,
        AudioFlingerTraits> gAudioFlingerServiceHandler;

sp<IAudioFlinger> AudioSystem::get_audio_flinger() {
    return gAudioFlingerServiceHandler.getService();
}

sp<IAudioFlinger> AudioSystem::get_audio_flinger_for_fuzzer() {
    return gAudioFlingerServiceHandler.getService(false /* canStartThreadPool */);
}

sp<AudioSystem::AudioFlingerClient> AudioSystem::getAudioFlingerClient() {
    return gAudioFlingerServiceHandler.getClient();
}

void AudioSystem::setAudioFlingerBinder(const sp<IBinder>& audioFlinger) {
    if (audioFlinger->getInterfaceDescriptor() != media::IAudioFlingerService::descriptor) {
        ALOGE("%s: received a binder of type %s",
                __func__, String8(audioFlinger->getInterfaceDescriptor()).c_str());
        return;
    }
    gAudioFlingerServiceHandler.setBinder(audioFlinger);
}

status_t AudioSystem::setLocalAudioFlinger(const sp<IAudioFlinger>& af) {
    return gAudioFlingerServiceHandler.setLocalService(af);
}

sp<AudioIoDescriptor> AudioSystem::getIoDescriptor(audio_io_handle_t ioHandle) {
    sp<AudioIoDescriptor> desc;
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc != 0) {
        desc = afc->getIoDescriptor(ioHandle);
    }
    return desc;
}

/* static */ status_t AudioSystem::checkAudioFlinger() {
    if (defaultServiceManager()->checkService(String16("media.audio_flinger")) != 0) {
        return NO_ERROR;
    }
    return DEAD_OBJECT;
}

// FIXME Declare in binder opcode order, similarly to IAudioFlinger.h and IAudioFlinger.cpp

status_t AudioSystem::muteMicrophone(bool state) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setMicMute(state);
}

status_t AudioSystem::isMicrophoneMuted(bool* state) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    *state = af->getMicMute();
    return NO_ERROR;
}

status_t AudioSystem::setMasterVolume(float value) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    af->setMasterVolume(value);
    return NO_ERROR;
}

status_t AudioSystem::setMasterMute(bool mute) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    af->setMasterMute(mute);
    return NO_ERROR;
}

status_t AudioSystem::getMasterVolume(float* volume) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    *volume = af->masterVolume();
    return NO_ERROR;
}

status_t AudioSystem::getMasterMute(bool* mute) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    *mute = af->masterMute();
    return NO_ERROR;
}

status_t AudioSystem::setStreamVolume(audio_stream_type_t stream, float value,
                                      audio_io_handle_t output) {
    if (uint32_t(stream) >= AUDIO_STREAM_CNT) return BAD_VALUE;
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    af->setStreamVolume(stream, value, output);
    return NO_ERROR;
}

status_t AudioSystem::setStreamMute(audio_stream_type_t stream, bool mute) {
    if (uint32_t(stream) >= AUDIO_STREAM_CNT) return BAD_VALUE;
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    af->setStreamMute(stream, mute);
    return NO_ERROR;
}

status_t AudioSystem::getStreamVolume(audio_stream_type_t stream, float* volume,
                                      audio_io_handle_t output) {
    if (uint32_t(stream) >= AUDIO_STREAM_CNT) return BAD_VALUE;
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    *volume = af->streamVolume(stream, output);
    return NO_ERROR;
}

status_t AudioSystem::getStreamMute(audio_stream_type_t stream, bool* mute) {
    if (uint32_t(stream) >= AUDIO_STREAM_CNT) return BAD_VALUE;
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    *mute = af->streamMute(stream);
    return NO_ERROR;
}

status_t AudioSystem::setMode(audio_mode_t mode) {
    if (uint32_t(mode) >= AUDIO_MODE_CNT) return BAD_VALUE;
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setMode(mode);
}

status_t AudioSystem::setSimulateDeviceConnections(bool enabled) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setSimulateDeviceConnections(enabled);
}

status_t AudioSystem::setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setParameters(ioHandle, keyValuePairs);
}

String8 AudioSystem::getParameters(audio_io_handle_t ioHandle, const String8& keys) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    String8 result = String8("");
    if (af == 0) return result;

    result = af->getParameters(ioHandle, keys);
    return result;
}

status_t AudioSystem::setParameters(const String8& keyValuePairs) {
    return setParameters(AUDIO_IO_HANDLE_NONE, keyValuePairs);
}

String8 AudioSystem::getParameters(const String8& keys) {
    return getParameters(AUDIO_IO_HANDLE_NONE, keys);
}

// convert volume steps to natural log scale

// change this value to change volume scaling
constexpr float kdbPerStep = 0.5f;
// shouldn't need to touch these
constexpr float kdBConvert = -kdbPerStep * 2.302585093f / 20.0f;
constexpr float kdBConvertInverse = 1.0f / kdBConvert;

float AudioSystem::linearToLog(int volume) {
    // float v = volume ? exp(float(100 - volume) * kdBConvert) : 0;
    // ALOGD("linearToLog(%d)=%f", volume, v);
    // return v;
    return volume ? exp(float(100 - volume) * kdBConvert) : 0;
}

int AudioSystem::logToLinear(float volume) {
    // int v = volume ? 100 - int(kdBConvertInverse * log(volume) + 0.5) : 0;
    // ALOGD("logTolinear(%d)=%f", v, volume);
    // return v;
    return volume ? 100 - int(kdBConvertInverse * log(volume) + 0.5) : 0;
}

/* static */ size_t AudioSystem::calculateMinFrameCount(
        uint32_t afLatencyMs, uint32_t afFrameCount, uint32_t afSampleRate,
        uint32_t sampleRate, float speed /*, uint32_t notificationsPerBufferReq*/) {
    // Ensure that buffer depth covers at least audio hardware latency
    uint32_t minBufCount = afLatencyMs / ((1000 * afFrameCount) / afSampleRate);
    if (minBufCount < 2) {
        minBufCount = 2;
    }
#if 0
        // The notificationsPerBufferReq parameter is not yet used for non-fast tracks,
        // but keeping the code here to make it easier to add later.
        if (minBufCount < notificationsPerBufferReq) {
            minBufCount = notificationsPerBufferReq;
        }
#endif
    ALOGV("calculateMinFrameCount afLatency %u  afFrameCount %u  afSampleRate %u  "
          "sampleRate %u  speed %f  minBufCount: %u" /*"  notificationsPerBufferReq %u"*/,
          afLatencyMs, afFrameCount, afSampleRate, sampleRate, speed, minBufCount
    /*, notificationsPerBufferReq*/);
    return minBufCount * sourceFramesNeededWithTimestretch(
            sampleRate, afFrameCount, afSampleRate, speed);
}


status_t
AudioSystem::getOutputSamplingRate(uint32_t* samplingRate, audio_stream_type_t streamType) {
    audio_io_handle_t output;

    if (streamType == AUDIO_STREAM_DEFAULT) {
        streamType = AUDIO_STREAM_MUSIC;
    }

    output = getOutput(streamType);
    if (output == 0) {
        return PERMISSION_DENIED;
    }

    return getSamplingRate(output, samplingRate);
}

status_t AudioSystem::getSamplingRate(audio_io_handle_t ioHandle,
                                      uint32_t* samplingRate) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    sp<AudioIoDescriptor> desc = getIoDescriptor(ioHandle);
    if (desc == 0) {
        *samplingRate = af->sampleRate(ioHandle);
    } else {
        *samplingRate = desc->getSamplingRate();
    }
    if (*samplingRate == 0) {
        ALOGE("AudioSystem::getSamplingRate failed for ioHandle %d", ioHandle);
        return BAD_VALUE;
    }

    ALOGV("getSamplingRate() ioHandle %d, sampling rate %u", ioHandle, *samplingRate);

    return NO_ERROR;
}

status_t AudioSystem::getOutputFrameCount(size_t* frameCount, audio_stream_type_t streamType) {
    audio_io_handle_t output;

    if (streamType == AUDIO_STREAM_DEFAULT) {
        streamType = AUDIO_STREAM_MUSIC;
    }

    output = getOutput(streamType);
    if (output == AUDIO_IO_HANDLE_NONE) {
        return PERMISSION_DENIED;
    }

    return getFrameCount(output, frameCount);
}

status_t AudioSystem::getFrameCount(audio_io_handle_t ioHandle,
                                    size_t* frameCount) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    sp<AudioIoDescriptor> desc = getIoDescriptor(ioHandle);
    if (desc == 0) {
        *frameCount = af->frameCount(ioHandle);
    } else {
        *frameCount = desc->getFrameCount();
    }
    if (*frameCount == 0) {
        ALOGE("AudioSystem::getFrameCount failed for ioHandle %d", ioHandle);
        return BAD_VALUE;
    }

    ALOGV("getFrameCount() ioHandle %d, frameCount %zu", ioHandle, *frameCount);

    return NO_ERROR;
}

status_t AudioSystem::getOutputLatency(uint32_t* latency, audio_stream_type_t streamType) {
    audio_io_handle_t output;

    if (streamType == AUDIO_STREAM_DEFAULT) {
        streamType = AUDIO_STREAM_MUSIC;
    }

    output = getOutput(streamType);
    if (output == AUDIO_IO_HANDLE_NONE) {
        return PERMISSION_DENIED;
    }

    return getLatency(output, latency);
}

status_t AudioSystem::getLatency(audio_io_handle_t output,
                                 uint32_t* latency) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    sp<AudioIoDescriptor> outputDesc = getIoDescriptor(output);
    if (outputDesc == 0) {
        *latency = af->latency(output);
    } else {
        *latency = outputDesc->getLatency();
    }

    ALOGV("getLatency() output %d, latency %d", output, *latency);

    return NO_ERROR;
}

status_t AudioSystem::getInputBufferSize(uint32_t sampleRate, audio_format_t format,
                                         audio_channel_mask_t channelMask, size_t* buffSize) {
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    return afc->getInputBufferSize(sampleRate, format, channelMask, buffSize);
}

status_t AudioSystem::setVoiceVolume(float value) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setVoiceVolume(value);
}

status_t AudioSystem::getRenderPosition(audio_io_handle_t output, uint32_t* halFrames,
                                        uint32_t* dspFrames) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;

    return af->getRenderPosition(halFrames, dspFrames, output);
}

uint32_t AudioSystem::getInputFramesLost(audio_io_handle_t ioHandle) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    uint32_t result = 0;
    if (af == 0) return result;
    if (ioHandle == AUDIO_IO_HANDLE_NONE) return result;

    result = af->getInputFramesLost(ioHandle);
    return result;
}

audio_unique_id_t AudioSystem::newAudioUniqueId(audio_unique_id_use_t use) {
    // Must not use AF as IDs will re-roll on audioserver restart, b/130369529.
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return AUDIO_UNIQUE_ID_ALLOCATE;
    return af->newAudioUniqueId(use);
}

void AudioSystem::acquireAudioSessionId(audio_session_t audioSession, pid_t pid, uid_t uid) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af != 0) {
        af->acquireAudioSessionId(audioSession, pid, uid);
    }
}

void AudioSystem::releaseAudioSessionId(audio_session_t audioSession, pid_t pid) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af != 0) {
        af->releaseAudioSessionId(audioSession, pid);
    }
}

audio_hw_sync_t AudioSystem::getAudioHwSyncForSession(audio_session_t sessionId) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return AUDIO_HW_SYNC_INVALID;
    return af->getAudioHwSyncForSession(sessionId);
}

status_t AudioSystem::systemReady() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return NO_INIT;
    return af->systemReady();
}

status_t AudioSystem::audioPolicyReady() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return NO_INIT;
    return af->audioPolicyReady();
}

status_t AudioSystem::getFrameCountHAL(audio_io_handle_t ioHandle,
                                       size_t* frameCount) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    sp<AudioIoDescriptor> desc = getIoDescriptor(ioHandle);
    if (desc == 0) {
        *frameCount = af->frameCountHAL(ioHandle);
    } else {
        *frameCount = desc->getFrameCountHAL();
    }
    if (*frameCount == 0) {
        ALOGE("AudioSystem::getFrameCountHAL failed for ioHandle %d", ioHandle);
        return BAD_VALUE;
    }

    ALOGV("getFrameCountHAL() ioHandle %d, frameCount %zu", ioHandle, *frameCount);

    return NO_ERROR;
}

// ---------------------------------------------------------------------------


void AudioSystem::AudioFlingerClient::clearIoCache() {
    std::lock_guard _l(mMutex);
    mIoDescriptors.clear();
    mInBuffSize = 0;
    mInSamplingRate = 0;
    mInFormat = AUDIO_FORMAT_DEFAULT;
    mInChannelMask = AUDIO_CHANNEL_NONE;
}

void AudioSystem::AudioFlingerClient::binderDied(const wp<IBinder>& who __unused) {
    gAudioFlingerServiceHandler.clearService();
    reportError(DEAD_OBJECT);

    ALOGW("AudioFlinger server died!");
}

Status AudioSystem::AudioFlingerClient::ioConfigChanged(
        media::AudioIoConfigEvent _event,
        const media::AudioIoDescriptor& _ioDesc) {
    audio_io_config_event_t event = VALUE_OR_RETURN_BINDER_STATUS(
            aidl2legacy_AudioIoConfigEvent_audio_io_config_event_t(_event));
    sp<AudioIoDescriptor> ioDesc(
            VALUE_OR_RETURN_BINDER_STATUS(
                    aidl2legacy_AudioIoDescriptor_AudioIoDescriptor(_ioDesc)));

    ALOGV("ioConfigChanged() event %d", event);

    if (ioDesc->getIoHandle() == AUDIO_IO_HANDLE_NONE) return Status::ok();

    audio_port_handle_t deviceId = AUDIO_PORT_HANDLE_NONE;
    std::vector<sp<AudioDeviceCallback>> callbacksToCall;
    {
        std::lock_guard _l(mMutex);
        auto callbacks = std::map<audio_port_handle_t, wp<AudioDeviceCallback>>();

        switch (event) {
            case AUDIO_OUTPUT_OPENED:
            case AUDIO_OUTPUT_REGISTERED:
            case AUDIO_INPUT_OPENED:
            case AUDIO_INPUT_REGISTERED: {
                if (sp<AudioIoDescriptor> oldDesc = getIoDescriptor_l(ioDesc->getIoHandle())) {
                    deviceId = oldDesc->getDeviceId();
                }
                mIoDescriptors[ioDesc->getIoHandle()] = ioDesc;

                if (ioDesc->getDeviceId() != AUDIO_PORT_HANDLE_NONE) {
                    deviceId = ioDesc->getDeviceId();
                    if (event == AUDIO_OUTPUT_OPENED || event == AUDIO_INPUT_OPENED) {
                        auto it = mAudioDeviceCallbacks.find(ioDesc->getIoHandle());
                        if (it != mAudioDeviceCallbacks.end()) {
                            callbacks = it->second;
                        }
                    }
                }
                ALOGV("ioConfigChanged() new %s %s %s",
                      event == AUDIO_OUTPUT_OPENED || event == AUDIO_OUTPUT_REGISTERED ?
                      "output" : "input",
                      event == AUDIO_OUTPUT_OPENED || event == AUDIO_INPUT_OPENED ?
                      "opened" : "registered",
                      ioDesc->toDebugString().c_str());
            }
                break;
            case AUDIO_OUTPUT_CLOSED:
            case AUDIO_INPUT_CLOSED: {
                if (getIoDescriptor_l(ioDesc->getIoHandle()) == 0) {
                    ALOGW("ioConfigChanged() closing unknown %s %d",
                          event == AUDIO_OUTPUT_CLOSED ? "output" : "input", ioDesc->getIoHandle());
                    break;
                }
                ALOGV("ioConfigChanged() %s %d closed",
                      event == AUDIO_OUTPUT_CLOSED ? "output" : "input", ioDesc->getIoHandle());

                mIoDescriptors.erase(ioDesc->getIoHandle());
                mAudioDeviceCallbacks.erase(ioDesc->getIoHandle());
            }
                break;

            case AUDIO_OUTPUT_CONFIG_CHANGED:
            case AUDIO_INPUT_CONFIG_CHANGED: {
                sp<AudioIoDescriptor> oldDesc = getIoDescriptor_l(ioDesc->getIoHandle());
                if (oldDesc == 0) {
                    ALOGW("ioConfigChanged() modifying unknown %s! %d",
                          event == AUDIO_OUTPUT_CONFIG_CHANGED ? "output" : "input",
                          ioDesc->getIoHandle());
                    break;
                }

                deviceId = oldDesc->getDeviceId();
                mIoDescriptors[ioDesc->getIoHandle()] = ioDesc;

                if (deviceId != ioDesc->getDeviceId()) {
                    deviceId = ioDesc->getDeviceId();
                    auto it = mAudioDeviceCallbacks.find(ioDesc->getIoHandle());
                    if (it != mAudioDeviceCallbacks.end()) {
                        callbacks = it->second;
                    }
                }
                ALOGV("ioConfigChanged() new config for %s %s",
                      event == AUDIO_OUTPUT_CONFIG_CHANGED ? "output" : "input",
                      ioDesc->toDebugString().c_str());

            }
                break;
            case AUDIO_CLIENT_STARTED: {
                sp<AudioIoDescriptor> oldDesc = getIoDescriptor_l(ioDesc->getIoHandle());
                if (oldDesc == 0) {
                    ALOGW("ioConfigChanged() start client on unknown io! %d",
                            ioDesc->getIoHandle());
                    break;
                }
                ALOGV("ioConfigChanged() AUDIO_CLIENT_STARTED  io %d port %d num callbacks %zu",
                      ioDesc->getIoHandle(), ioDesc->getPortId(), mAudioDeviceCallbacks.size());
                oldDesc->setPatch(ioDesc->getPatch());
                auto it = mAudioDeviceCallbacks.find(ioDesc->getIoHandle());
                if (it != mAudioDeviceCallbacks.end()) {
                    auto cbks = it->second;
                    auto it2 = cbks.find(ioDesc->getPortId());
                    if (it2 != cbks.end()) {
                        callbacks.emplace(ioDesc->getPortId(), it2->second);
                        deviceId = oldDesc->getDeviceId();
                    }
                }
            }
                break;
        }

        for (auto wpCbk : callbacks) {
            sp<AudioDeviceCallback> spCbk = wpCbk.second.promote();
            if (spCbk != nullptr) {
                callbacksToCall.push_back(spCbk);
            }
        }
    }

    // Callbacks must be called without mMutex held. May lead to dead lock if calling for
    // example getRoutedDevice that updates the device and tries to acquire mMutex.
    for (auto cb  : callbacksToCall) {
        // If callbacksToCall is not empty, it implies ioDesc->getIoHandle() and deviceId are valid
        cb->onAudioDeviceUpdate(ioDesc->getIoHandle(), deviceId);
    }

    return Status::ok();
}

Status AudioSystem::AudioFlingerClient::onSupportedLatencyModesChanged(
        int output, const std::vector<media::audio::common::AudioLatencyMode>& latencyModes) {
    audio_io_handle_t outputLegacy = VALUE_OR_RETURN_BINDER_STATUS(
            aidl2legacy_int32_t_audio_io_handle_t(output));
    std::vector<audio_latency_mode_t> modesLegacy = VALUE_OR_RETURN_BINDER_STATUS(
            convertContainer<std::vector<audio_latency_mode_t>>(
                    latencyModes, aidl2legacy_AudioLatencyMode_audio_latency_mode_t));

    std::vector<sp<SupportedLatencyModesCallback>> callbacks;
    {
        std::lock_guard _l(mMutex);
        for (auto callback : mSupportedLatencyModesCallbacks) {
            if (auto ref = callback.promote(); ref != nullptr) {
                callbacks.push_back(ref);
            }
        }
    }
    for (const auto& callback : callbacks) {
        callback->onSupportedLatencyModesChanged(outputLegacy, modesLegacy);
    }

    return Status::ok();
}

status_t AudioSystem::AudioFlingerClient::getInputBufferSize(
        uint32_t sampleRate, audio_format_t format,
        audio_channel_mask_t channelMask, size_t* buffSize) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) {
        return PERMISSION_DENIED;
    }
    std::lock_guard _l(mMutex);
    // Do we have a stale mInBuffSize or are we requesting the input buffer size for new values
    if ((mInBuffSize == 0) || (sampleRate != mInSamplingRate) || (format != mInFormat)
        || (channelMask != mInChannelMask)) {
        size_t inBuffSize = af->getInputBufferSize(sampleRate, format, channelMask);
        if (inBuffSize == 0) {
            ALOGE("AudioSystem::getInputBufferSize failed sampleRate %d format %#x channelMask %#x",
                  sampleRate, format, channelMask);
            return BAD_VALUE;
        }
        // A benign race is possible here: we could overwrite a fresher cache entry
        // save the request params
        mInSamplingRate = sampleRate;
        mInFormat = format;
        mInChannelMask = channelMask;

        mInBuffSize = inBuffSize;
    }

    *buffSize = mInBuffSize;

    return NO_ERROR;
}

sp<AudioIoDescriptor>
AudioSystem::AudioFlingerClient::getIoDescriptor_l(audio_io_handle_t ioHandle) {
    if (const auto it = mIoDescriptors.find(ioHandle);
        it != mIoDescriptors.end()) {
        return it->second;
    }
    return {};
}

sp<AudioIoDescriptor> AudioSystem::AudioFlingerClient::getIoDescriptor(audio_io_handle_t ioHandle) {
    std::lock_guard _l(mMutex);
    return getIoDescriptor_l(ioHandle);
}

status_t AudioSystem::AudioFlingerClient::addAudioDeviceCallback(
        const wp<AudioDeviceCallback>& callback, audio_io_handle_t audioIo,
        audio_port_handle_t portId) {
    ALOGV("%s audioIo %d portId %d", __func__, audioIo, portId);
    std::lock_guard _l(mMutex);
    auto& callbacks = mAudioDeviceCallbacks.emplace(
            audioIo,
            std::map<audio_port_handle_t, wp<AudioDeviceCallback>>()).first->second;
    auto result = callbacks.try_emplace(portId, callback);
    if (!result.second) {
        return INVALID_OPERATION;
    }
    return NO_ERROR;
}

status_t AudioSystem::AudioFlingerClient::removeAudioDeviceCallback(
        const wp<AudioDeviceCallback>& callback __unused, audio_io_handle_t audioIo,
        audio_port_handle_t portId) {
    ALOGV("%s audioIo %d portId %d", __func__, audioIo, portId);
    std::lock_guard _l(mMutex);
    auto it = mAudioDeviceCallbacks.find(audioIo);
    if (it == mAudioDeviceCallbacks.end()) {
        return INVALID_OPERATION;
    }
    if (it->second.erase(portId) == 0) {
        return INVALID_OPERATION;
    }
    if (it->second.size() == 0) {
        mAudioDeviceCallbacks.erase(audioIo);
    }
    return NO_ERROR;
}

status_t AudioSystem::AudioFlingerClient::addSupportedLatencyModesCallback(
        const sp<SupportedLatencyModesCallback>& callback) {
    std::lock_guard _l(mMutex);
    if (std::find(mSupportedLatencyModesCallbacks.begin(),
                  mSupportedLatencyModesCallbacks.end(),
                  callback) != mSupportedLatencyModesCallbacks.end()) {
        return INVALID_OPERATION;
    }
    mSupportedLatencyModesCallbacks.push_back(callback);
    return NO_ERROR;
}

status_t AudioSystem::AudioFlingerClient::removeSupportedLatencyModesCallback(
        const sp<SupportedLatencyModesCallback>& callback) {
    std::lock_guard _l(mMutex);
    auto it = std::find(mSupportedLatencyModesCallbacks.begin(),
                                 mSupportedLatencyModesCallbacks.end(),
                                 callback);
    if (it == mSupportedLatencyModesCallbacks.end()) {
        return INVALID_OPERATION;
    }
    mSupportedLatencyModesCallbacks.erase(it);
    return NO_ERROR;
}

/* static */ uintptr_t AudioSystem::addErrorCallback(audio_error_callback cb) {
    std::lock_guard _l(gErrorCallbacksMutex);
    gAudioErrorCallbacks.insert(cb);
    return reinterpret_cast<uintptr_t>(cb);
}

/* static */ void AudioSystem::removeErrorCallback(uintptr_t cb) {
    std::lock_guard _l(gErrorCallbacksMutex);
    gAudioErrorCallbacks.erase(reinterpret_cast<audio_error_callback>(cb));
}

/* static */ void AudioSystem::reportError(status_t err) {
    std::lock_guard _l(gErrorCallbacksMutex);
    for (auto callback : gAudioErrorCallbacks) {
        callback(err);
    }
}

/*static*/ void AudioSystem::setDynPolicyCallback(dynamic_policy_callback cb) {
    std::lock_guard _l(gMutex);
    gDynPolicyCallback = cb;
}

/*static*/ void AudioSystem::setRecordConfigCallback(record_config_callback cb) {
    std::lock_guard _l(gMutex);
    gRecordConfigCallback = cb;
}

/*static*/ void AudioSystem::setRoutingCallback(routing_callback cb) {
    std::lock_guard _l(gMutex);
    gRoutingCallback = cb;
}

/*static*/ void AudioSystem::setVolInitReqCallback(vol_range_init_req_callback cb) {
    std::lock_guard _l(gMutex);
    gVolRangeInitReqCallback = cb;
}

struct AudioPolicyTraits {
    static void onServiceCreate(const sp<IAudioPolicyService>& ap,
            const sp<AudioSystem::AudioPolicyServiceClient>& apc) {
        const int64_t token = IPCThreadState::self()->clearCallingIdentity();
        ap->registerClient(apc);
        ap->setAudioPortCallbacksEnabled(apc->isAudioPortCbEnabled());
        ap->setAudioVolumeGroupCallbacksEnabled(apc->isAudioVolumeGroupCbEnabled());
        IPCThreadState::self()->restoreCallingIdentity(token);
    }

    static void onClearService(const sp<AudioSystem::AudioPolicyServiceClient>&) {}

    static constexpr const char *SERVICE_NAME = "media.audio_policy";
};

[[clang::no_destroy]] static constinit ServiceHandler<IAudioPolicyService,
        AudioSystem::AudioPolicyServiceClient, IAudioPolicyService,
        AudioPolicyTraits> gAudioPolicyServiceHandler;

status_t AudioSystem::setLocalAudioPolicyService(const sp<IAudioPolicyService>& aps) {
    return gAudioPolicyServiceHandler.setLocalService(aps);
}

sp<IAudioPolicyService> AudioSystem::get_audio_policy_service() {
    return gAudioPolicyServiceHandler.getService();
}

void AudioSystem::clearAudioPolicyService() {
    gAudioPolicyServiceHandler.clearService();
}

// ---------------------------------------------------------------------------

void AudioSystem::onNewAudioModulesAvailable() {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return;
    aps->onNewAudioModulesAvailable();
}

status_t AudioSystem::setDeviceConnectionState(audio_policy_dev_state_t state,
                                               const android::media::audio::common::AudioPort& port,
                                               audio_format_t encodedFormat) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();

    if (aps == 0) return PERMISSION_DENIED;

    return statusTFromBinderStatus(
            aps->setDeviceConnectionState(
                    VALUE_OR_RETURN_STATUS(
                            legacy2aidl_audio_policy_dev_state_t_AudioPolicyDeviceState(state)),
                    port,
                    VALUE_OR_RETURN_STATUS(
                            legacy2aidl_audio_format_t_AudioFormatDescription(encodedFormat))));
}

audio_policy_dev_state_t AudioSystem::getDeviceConnectionState(audio_devices_t device,
                                                               const char* device_address) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;

    auto result = [&]() -> ConversionResult<audio_policy_dev_state_t> {
        AudioDevice deviceAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_device_AudioDevice(device, device_address));

        media::AudioPolicyDeviceState result;
        RETURN_IF_ERROR(statusTFromBinderStatus(
                aps->getDeviceConnectionState(deviceAidl, &result)));

        return aidl2legacy_AudioPolicyDeviceState_audio_policy_dev_state_t(result);
    }();
    return result.value_or(AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE);
}

status_t AudioSystem::handleDeviceConfigChange(audio_devices_t device,
                                               const char* device_address,
                                               const char* device_name,
                                               audio_format_t encodedFormat) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    const char* address = "";
    const char* name = "";

    if (aps == 0) return PERMISSION_DENIED;

    if (device_address != NULL) {
        address = device_address;
    }
    if (device_name != NULL) {
        name = device_name;
    }

    AudioDevice deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_device_AudioDevice(device, address));

    return statusTFromBinderStatus(
            aps->handleDeviceConfigChange(deviceAidl, name, VALUE_OR_RETURN_STATUS(
                    legacy2aidl_audio_format_t_AudioFormatDescription(encodedFormat))));
}

status_t AudioSystem::setPhoneState(audio_mode_t state, uid_t uid) {
    if (uint32_t(state) >= AUDIO_MODE_CNT) return BAD_VALUE;
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    return statusTFromBinderStatus(aps->setPhoneState(
            VALUE_OR_RETURN_STATUS(legacy2aidl_audio_mode_t_AudioMode(state)),
            VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid))));
}

status_t
AudioSystem::setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    return statusTFromBinderStatus(
            aps->setForceUse(
                    VALUE_OR_RETURN_STATUS(
                            legacy2aidl_audio_policy_force_use_t_AudioPolicyForceUse(usage)),
                    VALUE_OR_RETURN_STATUS(
                            legacy2aidl_audio_policy_forced_cfg_t_AudioPolicyForcedConfig(
                                    config))));
}

audio_policy_forced_cfg_t AudioSystem::getForceUse(audio_policy_force_use_t usage) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return AUDIO_POLICY_FORCE_NONE;

    auto result = [&]() -> ConversionResult<audio_policy_forced_cfg_t> {
        media::AudioPolicyForceUse usageAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_policy_force_use_t_AudioPolicyForceUse(usage));
        media::AudioPolicyForcedConfig configAidl;
        RETURN_IF_ERROR(statusTFromBinderStatus(
                aps->getForceUse(usageAidl, &configAidl)));
        return aidl2legacy_AudioPolicyForcedConfig_audio_policy_forced_cfg_t(configAidl);
    }();

    return result.value_or(AUDIO_POLICY_FORCE_NONE);
}


audio_io_handle_t AudioSystem::getOutput(audio_stream_type_t stream) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return AUDIO_IO_HANDLE_NONE;

    auto result = [&]() -> ConversionResult<audio_io_handle_t> {
        AudioStreamType streamAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
        int32_t outputAidl;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->getOutput(streamAidl, &outputAidl)));
        return aidl2legacy_int32_t_audio_io_handle_t(outputAidl);
    }();

    return result.value_or(AUDIO_IO_HANDLE_NONE);
}

status_t AudioSystem::getOutputForAttr(audio_attributes_t* attr,
                                       audio_io_handle_t* output,
                                       audio_session_t session,
                                       audio_stream_type_t* stream,
                                       const AttributionSourceState& attributionSource,
                                       audio_config_t* config,
                                       audio_output_flags_t flags,
                                       audio_port_handle_t* selectedDeviceId,
                                       audio_port_handle_t* portId,
                                       std::vector<audio_io_handle_t>* secondaryOutputs,
                                       bool *isSpatialized,
                                       bool *isBitPerfect) {
    if (attr == nullptr) {
        ALOGE("%s NULL audio attributes", __func__);
        return BAD_VALUE;
    }
    if (output == nullptr) {
        ALOGE("%s NULL output - shouldn't happen", __func__);
        return BAD_VALUE;
    }
    if (selectedDeviceId == nullptr) {
        ALOGE("%s NULL selectedDeviceId - shouldn't happen", __func__);
        return BAD_VALUE;
    }
    if (portId == nullptr) {
        ALOGE("%s NULL portId - shouldn't happen", __func__);
        return BAD_VALUE;
    }
    if (secondaryOutputs == nullptr) {
        ALOGE("%s NULL secondaryOutputs - shouldn't happen", __func__);
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return NO_INIT;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    int32_t sessionAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_session_t_int32_t(session));
    AudioConfig configAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_config_t_AudioConfig(*config, false /*isInput*/));
    int32_t flagsAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_output_flags_t_int32_t_mask(flags));
    int32_t selectedDeviceIdAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_handle_t_int32_t(*selectedDeviceId));

    media::GetOutputForAttrResponse responseAidl;

    status_t status = statusTFromBinderStatus(
            aps->getOutputForAttr(attrAidl, sessionAidl, attributionSource, configAidl, flagsAidl,
                                  selectedDeviceIdAidl, &responseAidl));
    if (status != NO_ERROR) {
        config->format = VALUE_OR_RETURN_STATUS(
            aidl2legacy_AudioFormatDescription_audio_format_t(responseAidl.configBase.format));
        config->channel_mask = VALUE_OR_RETURN_STATUS(
            aidl2legacy_AudioChannelLayout_audio_channel_mask_t(
                    responseAidl.configBase.channelMask, false /*isInput*/));
        config->sample_rate = responseAidl.configBase.sampleRate;
        return status;
    }

    *output = VALUE_OR_RETURN_STATUS(
            aidl2legacy_int32_t_audio_io_handle_t(responseAidl.output));

    if (stream != nullptr) {
        *stream = VALUE_OR_RETURN_STATUS(
                aidl2legacy_AudioStreamType_audio_stream_type_t(responseAidl.stream));
    }
    *selectedDeviceId = VALUE_OR_RETURN_STATUS(
            aidl2legacy_int32_t_audio_port_handle_t(responseAidl.selectedDeviceId));
    *portId = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_port_handle_t(responseAidl.portId));
    *secondaryOutputs = VALUE_OR_RETURN_STATUS(convertContainer<std::vector<audio_io_handle_t>>(
            responseAidl.secondaryOutputs, aidl2legacy_int32_t_audio_io_handle_t));
    *isSpatialized = responseAidl.isSpatialized;
    *isBitPerfect = responseAidl.isBitPerfect;
    *attr = VALUE_OR_RETURN_STATUS(
            aidl2legacy_AudioAttributes_audio_attributes_t(responseAidl.attr));

    return OK;
}

status_t AudioSystem::startOutput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(aps->startOutput(portIdAidl));
}

status_t AudioSystem::stopOutput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(aps->stopOutput(portIdAidl));
}

void AudioSystem::releaseOutput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return;

    auto status = [&]() -> status_t {
        int32_t portIdAidl = VALUE_OR_RETURN_STATUS(
                legacy2aidl_audio_port_handle_t_int32_t(portId));
        RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(aps->releaseOutput(portIdAidl)));
        return OK;
    }();

    // Ignore status.
    (void) status;
}

status_t AudioSystem::getInputForAttr(const audio_attributes_t* attr,
                                      audio_io_handle_t* input,
                                      audio_unique_id_t riid,
                                      audio_session_t session,
                                      const AttributionSourceState &attributionSource,
                                      audio_config_base_t* config,
                                      audio_input_flags_t flags,
                                      audio_port_handle_t* selectedDeviceId,
                                      audio_port_handle_t* portId) {
    if (attr == NULL) {
        ALOGE("getInputForAttr NULL attr - shouldn't happen");
        return BAD_VALUE;
    }
    if (input == NULL) {
        ALOGE("getInputForAttr NULL input - shouldn't happen");
        return BAD_VALUE;
    }
    if (selectedDeviceId == NULL) {
        ALOGE("getInputForAttr NULL selectedDeviceId - shouldn't happen");
        return BAD_VALUE;
    }
    if (portId == NULL) {
        ALOGE("getInputForAttr NULL portId - shouldn't happen");
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return NO_INIT;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    int32_t inputAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_io_handle_t_int32_t(*input));
    int32_t riidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_unique_id_t_int32_t(riid));
    int32_t sessionAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_session_t_int32_t(session));
    AudioConfigBase configAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_config_base_t_AudioConfigBase(*config, true /*isInput*/));
    int32_t flagsAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_input_flags_t_int32_t_mask(flags));
    int32_t selectedDeviceIdAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_handle_t_int32_t(*selectedDeviceId));

    media::GetInputForAttrResponse response;

    status_t status = statusTFromBinderStatus(
            aps->getInputForAttr(attrAidl, inputAidl, riidAidl, sessionAidl, attributionSource,
                configAidl, flagsAidl, selectedDeviceIdAidl, &response));
    if (status != NO_ERROR) {
        *config = VALUE_OR_RETURN_STATUS(
                aidl2legacy_AudioConfigBase_audio_config_base_t(response.config, true /*isInput*/));
        return status;
    }

    *input = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_io_handle_t(response.input));
    *selectedDeviceId = VALUE_OR_RETURN_STATUS(
            aidl2legacy_int32_t_audio_port_handle_t(response.selectedDeviceId));
    *portId = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_port_handle_t(response.portId));

    return OK;
}

status_t AudioSystem::startInput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(aps->startInput(portIdAidl));
}

status_t AudioSystem::stopInput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(aps->stopInput(portIdAidl));
}

void AudioSystem::releaseInput(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return;

    auto status = [&]() -> status_t {
        int32_t portIdAidl = VALUE_OR_RETURN_STATUS(
                legacy2aidl_audio_port_handle_t_int32_t(portId));
        RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(aps->releaseInput(portIdAidl)));
        return OK;
    }();

    // Ignore status.
    (void) status;
}

status_t AudioSystem::initStreamVolume(audio_stream_type_t stream,
                                       int indexMin,
                                       int indexMax) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    AudioStreamType streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
    int32_t indexMinAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(indexMin));
    int32_t indexMaxAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(indexMax));
    status_t status = statusTFromBinderStatus(
            aps->initStreamVolume(streamAidl, indexMinAidl, indexMaxAidl));
    if (status == DEAD_OBJECT) {
        // This is a critical operation since w/o proper stream volumes no audio
        // will be heard. Make sure we recover from a failure in any case.
        ALOGE("Received DEAD_OBJECT from APS, clearing the client");
        clearAudioPolicyService();
    }
    return status;
}

status_t AudioSystem::setStreamVolumeIndex(audio_stream_type_t stream,
                                           int index,
                                           audio_devices_t device) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    AudioStreamType streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
    int32_t indexAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(index));
    AudioDeviceDescription deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
    return statusTFromBinderStatus(
            aps->setStreamVolumeIndex(streamAidl, deviceAidl, indexAidl));
}

status_t AudioSystem::getStreamVolumeIndex(audio_stream_type_t stream,
                                           int* index,
                                           audio_devices_t device) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    AudioStreamType streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
    AudioDeviceDescription deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
    int32_t indexAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getStreamVolumeIndex(streamAidl, deviceAidl, &indexAidl)));
    if (index != nullptr) {
        *index = VALUE_OR_RETURN_STATUS(convertIntegral<int>(indexAidl));
    }
    return OK;
}

status_t AudioSystem::setVolumeIndexForAttributes(const audio_attributes_t& attr,
                                                  int index,
                                                  audio_devices_t device) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(attr));
    int32_t indexAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(index));
    AudioDeviceDescription deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
    return statusTFromBinderStatus(
            aps->setVolumeIndexForAttributes(attrAidl, deviceAidl, indexAidl));
}

status_t AudioSystem::getVolumeIndexForAttributes(const audio_attributes_t& attr,
                                                  int& index,
                                                  audio_devices_t device) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(attr));
    AudioDeviceDescription deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
    int32_t indexAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getVolumeIndexForAttributes(attrAidl, deviceAidl, &indexAidl)));
    index = VALUE_OR_RETURN_STATUS(convertIntegral<int>(indexAidl));
    return OK;
}

status_t AudioSystem::getMaxVolumeIndexForAttributes(const audio_attributes_t& attr, int& index) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(attr));
    int32_t indexAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getMaxVolumeIndexForAttributes(attrAidl, &indexAidl)));
    index = VALUE_OR_RETURN_STATUS(convertIntegral<int>(indexAidl));
    return OK;
}

status_t AudioSystem::getMinVolumeIndexForAttributes(const audio_attributes_t& attr, int& index) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(attr));
    int32_t indexAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getMinVolumeIndexForAttributes(attrAidl, &indexAidl)));
    index = VALUE_OR_RETURN_STATUS(convertIntegral<int>(indexAidl));
    return OK;
}

product_strategy_t AudioSystem::getStrategyForStream(audio_stream_type_t stream) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PRODUCT_STRATEGY_NONE;

    auto result = [&]() -> ConversionResult<product_strategy_t> {
        AudioStreamType streamAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
        int32_t resultAidl;
        RETURN_IF_ERROR(statusTFromBinderStatus(
                aps->getStrategyForStream(streamAidl, &resultAidl)));
        return aidl2legacy_int32_t_product_strategy_t(resultAidl);
    }();
    return result.value_or(PRODUCT_STRATEGY_NONE);
}

status_t AudioSystem::getDevicesForAttributes(const audio_attributes_t& aa,
                                              AudioDeviceTypeAddrVector* devices,
                                              bool forVolume) {
    if (devices == nullptr) {
        return BAD_VALUE;
    }
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes aaAidl = VALUE_OR_RETURN_STATUS(
             legacy2aidl_audio_attributes_t_AudioAttributes(aa));
    std::vector<AudioDevice> retAidl;
    RETURN_STATUS_IF_ERROR(
            statusTFromBinderStatus(aps->getDevicesForAttributes(aaAidl, forVolume, &retAidl)));
    *devices = VALUE_OR_RETURN_STATUS(
            convertContainer<AudioDeviceTypeAddrVector>(
                    retAidl,
                    aidl2legacy_AudioDeviceTypeAddress));
    return OK;
}

audio_io_handle_t AudioSystem::getOutputForEffect(const effect_descriptor_t* desc) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    // FIXME change return type to status_t, and return PERMISSION_DENIED here
    if (aps == 0) return AUDIO_IO_HANDLE_NONE;

    auto result = [&]() -> ConversionResult<audio_io_handle_t> {
        media::EffectDescriptor descAidl = VALUE_OR_RETURN(
                legacy2aidl_effect_descriptor_t_EffectDescriptor(*desc));
        int32_t retAidl;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->getOutputForEffect(descAidl, &retAidl)));
        return aidl2legacy_int32_t_audio_io_handle_t(retAidl);
    }();

    return result.value_or(AUDIO_IO_HANDLE_NONE);
}

status_t AudioSystem::registerEffect(const effect_descriptor_t* desc,
                                     audio_io_handle_t io,
                                     product_strategy_t strategy,
                                     audio_session_t session,
                                     int id) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::EffectDescriptor descAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_effect_descriptor_t_EffectDescriptor(*desc));
    int32_t ioAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_io_handle_t_int32_t(io));
    int32_t strategyAidl = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_product_strategy_t(strategy));
    int32_t sessionAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_session_t_int32_t(session));
    int32_t idAidl = VALUE_OR_RETURN_STATUS(convertReinterpret<int32_t>(id));
    return statusTFromBinderStatus(
            aps->registerEffect(descAidl, ioAidl, strategyAidl, sessionAidl, idAidl));
}

status_t AudioSystem::unregisterEffect(int id) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t idAidl = VALUE_OR_RETURN_STATUS(convertReinterpret<int32_t>(id));
    return statusTFromBinderStatus(
            aps->unregisterEffect(idAidl));
}

status_t AudioSystem::setEffectEnabled(int id, bool enabled) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t idAidl = VALUE_OR_RETURN_STATUS(convertReinterpret<int32_t>(id));
    return statusTFromBinderStatus(
            aps->setEffectEnabled(idAidl, enabled));
}

status_t AudioSystem::moveEffectsToIo(const std::vector<int>& ids, audio_io_handle_t io) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<int32_t> idsAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<int32_t>>(ids, convertReinterpret<int32_t, int>));
    int32_t ioAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_io_handle_t_int32_t(io));
    return statusTFromBinderStatus(aps->moveEffectsToIo(idsAidl, ioAidl));
}

status_t AudioSystem::isStreamActive(audio_stream_type_t stream, bool* state, uint32_t inPastMs) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    if (state == NULL) return BAD_VALUE;

    AudioStreamType streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
    int32_t inPastMsAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(inPastMs));
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->isStreamActive(streamAidl, inPastMsAidl, state)));
    return OK;
}

status_t AudioSystem::isStreamActiveRemotely(audio_stream_type_t stream, bool* state,
                                             uint32_t inPastMs) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    if (state == NULL) return BAD_VALUE;

    AudioStreamType streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
    int32_t inPastMsAidl = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(inPastMs));
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->isStreamActiveRemotely(streamAidl, inPastMsAidl, state)));
    return OK;
}

status_t AudioSystem::isSourceActive(audio_source_t stream, bool* state) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    if (state == NULL) return BAD_VALUE;

    AudioSource streamAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(stream));
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->isSourceActive(streamAidl, state)));
    return OK;
}

uint32_t AudioSystem::getPrimaryOutputSamplingRate() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return 0;
    return af->getPrimaryOutputSamplingRate();
}

size_t AudioSystem::getPrimaryOutputFrameCount() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return 0;
    return af->getPrimaryOutputFrameCount();
}

status_t AudioSystem::setLowRamDevice(bool isLowRamDevice, int64_t totalMemory) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setLowRamDevice(isLowRamDevice, totalMemory);
}

void AudioSystem::clearAudioConfigCache() {
    // called by restoreTrack_l(), which needs new IAudioFlinger and IAudioPolicyService instances
    ALOGV("clearAudioConfigCache()");
    gAudioFlingerServiceHandler.clearService();
    clearAudioPolicyService();
}

status_t AudioSystem::setSupportedSystemUsages(const std::vector<audio_usage_t>& systemUsages) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) return PERMISSION_DENIED;

    std::vector<AudioUsage> systemUsagesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioUsage>>(systemUsages,
                                                      legacy2aidl_audio_usage_t_AudioUsage));
    return statusTFromBinderStatus(aps->setSupportedSystemUsages(systemUsagesAidl));
}

status_t AudioSystem::setAllowedCapturePolicy(uid_t uid, audio_flags_mask_t capturePolicy) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) return PERMISSION_DENIED;

    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    int32_t capturePolicyAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_flags_mask_t_int32_t_mask(capturePolicy));
    return statusTFromBinderStatus(aps->setAllowedCapturePolicy(uidAidl, capturePolicyAidl));
}

audio_offload_mode_t AudioSystem::getOffloadSupport(const audio_offload_info_t& info) {
    ALOGV("%s", __func__);
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return AUDIO_OFFLOAD_NOT_SUPPORTED;

    auto result = [&]() -> ConversionResult<audio_offload_mode_t> {
        AudioOffloadInfo infoAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_offload_info_t_AudioOffloadInfo(info));
        media::AudioOffloadMode retAidl;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->getOffloadSupport(infoAidl, &retAidl)));
        return aidl2legacy_AudioOffloadMode_audio_offload_mode_t(retAidl);
    }();

    return result.value_or(static_cast<audio_offload_mode_t>(0));
}

status_t AudioSystem::listAudioPorts(audio_port_role_t role,
                                     audio_port_type_t type,
                                     unsigned int* num_ports,
                                     struct audio_port_v7* ports,
                                     unsigned int* generation) {
    if (num_ports == nullptr || (*num_ports != 0 && ports == nullptr) ||
        generation == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::AudioPortRole roleAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_role_t_AudioPortRole(role));
    media::AudioPortType typeAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_type_t_AudioPortType(type));
    Int numPortsAidl;
    numPortsAidl.value = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(*num_ports));
    std::vector<media::AudioPortFw> portsAidl;
    int32_t generationAidl;

    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->listAudioPorts(roleAidl, typeAidl, &numPortsAidl, &portsAidl, &generationAidl)));
    *num_ports = VALUE_OR_RETURN_STATUS(convertIntegral<unsigned int>(numPortsAidl.value));
    *generation = VALUE_OR_RETURN_STATUS(convertIntegral<unsigned int>(generationAidl));
    RETURN_STATUS_IF_ERROR(convertRange(portsAidl.begin(), portsAidl.end(), ports,
                                        aidl2legacy_AudioPortFw_audio_port_v7));
    return OK;
}

status_t AudioSystem::listDeclaredDevicePorts(media::AudioPortRole role,
                                              std::vector<media::AudioPortFw>* result) {
    if (result == nullptr) return BAD_VALUE;
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(aps->listDeclaredDevicePorts(role, result)));
    return OK;
}

status_t AudioSystem::getAudioPort(struct audio_port_v7* port) {
    if (port == nullptr) {
        return BAD_VALUE;
    }
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::AudioPortFw portAidl;
    RETURN_STATUS_IF_ERROR(
            statusTFromBinderStatus(aps->getAudioPort(port->id, &portAidl)));
    *port = VALUE_OR_RETURN_STATUS(aidl2legacy_AudioPortFw_audio_port_v7(portAidl));
    return OK;
}

status_t AudioSystem::createAudioPatch(const struct audio_patch* patch,
                                       audio_patch_handle_t* handle) {
    if (patch == nullptr || handle == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::AudioPatchFw patchAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_patch_AudioPatchFw(*patch));
    int32_t handleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_patch_handle_t_int32_t(*handle));
    RETURN_STATUS_IF_ERROR(
            statusTFromBinderStatus(aps->createAudioPatch(patchAidl, handleAidl, &handleAidl)));
    *handle = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_patch_handle_t(handleAidl));
    return OK;
}

status_t AudioSystem::releaseAudioPatch(audio_patch_handle_t handle) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t handleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_patch_handle_t_int32_t(handle));
    return statusTFromBinderStatus(aps->releaseAudioPatch(handleAidl));
}

status_t AudioSystem::listAudioPatches(unsigned int* num_patches,
                                       struct audio_patch* patches,
                                       unsigned int* generation) {
    if (num_patches == nullptr || (*num_patches != 0 && patches == nullptr) ||
        generation == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;


    Int numPatchesAidl;
    numPatchesAidl.value = VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(*num_patches));
    std::vector<media::AudioPatchFw> patchesAidl;
    int32_t generationAidl;

    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->listAudioPatches(&numPatchesAidl, &patchesAidl, &generationAidl)));
    *num_patches = VALUE_OR_RETURN_STATUS(convertIntegral<unsigned int>(numPatchesAidl.value));
    *generation = VALUE_OR_RETURN_STATUS(convertIntegral<unsigned int>(generationAidl));
    RETURN_STATUS_IF_ERROR(convertRange(patchesAidl.begin(), patchesAidl.end(), patches,
                                        aidl2legacy_AudioPatchFw_audio_patch));
    return OK;
}

status_t AudioSystem::setAudioPortConfig(const struct audio_port_config* config) {
    if (config == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::AudioPortConfigFw configAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_config_AudioPortConfigFw(*config));
    return statusTFromBinderStatus(aps->setAudioPortConfig(configAidl));
}

status_t AudioSystem::addAudioPortCallback(const sp<AudioPortCallback>& callback) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    const auto apc = gAudioPolicyServiceHandler.getClient();
    if (apc == nullptr) return NO_INIT;

    std::lock_guard _l(gApsCallbackMutex);
    const int ret = apc->addAudioPortCallback(callback);
    if (ret == 1) {
        aps->setAudioPortCallbacksEnabled(true);
    }
    return (ret < 0) ? INVALID_OPERATION : NO_ERROR;
}

/*static*/
status_t AudioSystem::removeAudioPortCallback(const sp<AudioPortCallback>& callback) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    const auto apc = gAudioPolicyServiceHandler.getClient();
    if (apc == nullptr) return NO_INIT;

    std::lock_guard _l(gApsCallbackMutex);
    const int ret = apc->removeAudioPortCallback(callback);
    if (ret == 0) {
        aps->setAudioPortCallbacksEnabled(false);
    }
    return (ret < 0) ? INVALID_OPERATION : NO_ERROR;
}

status_t AudioSystem::addAudioVolumeGroupCallback(const sp<AudioVolumeGroupCallback>& callback) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    const auto apc = gAudioPolicyServiceHandler.getClient();
    if (apc == nullptr) return NO_INIT;

    std::lock_guard _l(gApsCallbackMutex);
    const int ret = apc->addAudioVolumeGroupCallback(callback);
    if (ret == 1) {
        aps->setAudioVolumeGroupCallbacksEnabled(true);
    }
    return (ret < 0) ? INVALID_OPERATION : NO_ERROR;
}

status_t AudioSystem::removeAudioVolumeGroupCallback(const sp<AudioVolumeGroupCallback>& callback) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    const auto apc = gAudioPolicyServiceHandler.getClient();
    if (apc == nullptr) return NO_INIT;

    std::lock_guard _l(gApsCallbackMutex);
    const int ret = apc->removeAudioVolumeGroupCallback(callback);
    if (ret == 0) {
        aps->setAudioVolumeGroupCallbacksEnabled(false);
    }
    return (ret < 0) ? INVALID_OPERATION : NO_ERROR;
}

status_t AudioSystem::addAudioDeviceCallback(
        const wp<AudioDeviceCallback>& callback, audio_io_handle_t audioIo,
        audio_port_handle_t portId) {
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    status_t status = afc->addAudioDeviceCallback(callback, audioIo, portId);
    if (status == NO_ERROR) {
        const sp<IAudioFlinger> af = get_audio_flinger();
        if (af != 0) {
            af->registerClient(afc);
        }
    }
    return status;
}

status_t AudioSystem::removeAudioDeviceCallback(
        const wp<AudioDeviceCallback>& callback, audio_io_handle_t audioIo,
        audio_port_handle_t portId) {
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    return afc->removeAudioDeviceCallback(callback, audioIo, portId);
}

status_t AudioSystem::addSupportedLatencyModesCallback(
        const sp<SupportedLatencyModesCallback>& callback) {
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    return afc->addSupportedLatencyModesCallback(callback);
}

status_t AudioSystem::removeSupportedLatencyModesCallback(
        const sp<SupportedLatencyModesCallback>& callback) {
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    return afc->removeSupportedLatencyModesCallback(callback);
}

audio_port_handle_t AudioSystem::getDeviceIdForIo(audio_io_handle_t audioIo) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    const sp<AudioIoDescriptor> desc = getIoDescriptor(audioIo);
    if (desc == 0) {
        return AUDIO_PORT_HANDLE_NONE;
    }
    return desc->getDeviceId();
}

status_t AudioSystem::acquireSoundTriggerSession(audio_session_t* session,
                                                 audio_io_handle_t* ioHandle,
                                                 audio_devices_t* device) {
    if (session == nullptr || ioHandle == nullptr || device == nullptr) {
        return BAD_VALUE;
    }
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::SoundTriggerSession retAidl;
    RETURN_STATUS_IF_ERROR(
            statusTFromBinderStatus(aps->acquireSoundTriggerSession(&retAidl)));
    *session = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_session_t(retAidl.session));
    *ioHandle = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_io_handle_t(retAidl.ioHandle));
    *device = VALUE_OR_RETURN_STATUS(
            aidl2legacy_AudioDeviceDescription_audio_devices_t(retAidl.device));
    return OK;
}

status_t AudioSystem::releaseSoundTriggerSession(audio_session_t session) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t sessionAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_session_t_int32_t(session));
    return statusTFromBinderStatus(aps->releaseSoundTriggerSession(sessionAidl));
}

audio_mode_t AudioSystem::getPhoneState() {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return AUDIO_MODE_INVALID;

    auto result = [&]() -> ConversionResult<audio_mode_t> {
        media::audio::common::AudioMode retAidl;
        RETURN_IF_ERROR(statusTFromBinderStatus(aps->getPhoneState(&retAidl)));
        return aidl2legacy_AudioMode_audio_mode_t(retAidl);
    }();

    return result.value_or(AUDIO_MODE_INVALID);
}

status_t AudioSystem::registerPolicyMixes(const Vector<AudioMix>& mixes, bool registration) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    size_t mixesSize = std::min(mixes.size(), size_t{MAX_MIXES_PER_POLICY});
    std::vector<media::AudioMix> mixesAidl;
    RETURN_STATUS_IF_ERROR(
            convertRange(mixes.begin(), mixes.begin() + mixesSize, std::back_inserter(mixesAidl),
                         legacy2aidl_AudioMix));
    return statusTFromBinderStatus(aps->registerPolicyMixes(mixesAidl, registration));
}

status_t AudioSystem::updatePolicyMixes(
        const std::vector<std::pair<AudioMix, std::vector<AudioMixMatchCriterion>>>&
                mixesWithUpdates) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<media::AudioMixUpdate> updatesAidl;
    updatesAidl.reserve(mixesWithUpdates.size());

    for (const auto& update : mixesWithUpdates) {
        media::AudioMixUpdate updateAidl;
        updateAidl.audioMix = VALUE_OR_RETURN_STATUS(legacy2aidl_AudioMix(update.first));
        RETURN_STATUS_IF_ERROR(convertRange(update.second.begin(), update.second.end(),
                                            std::back_inserter(updateAidl.newCriteria),
                                            legacy2aidl_AudioMixMatchCriterion));
        updatesAidl.emplace_back(updateAidl);
    }

    return statusTFromBinderStatus(aps->updatePolicyMixes(updatesAidl));
}

status_t AudioSystem::setUidDeviceAffinities(uid_t uid, const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                              legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(aps->setUidDeviceAffinities(uidAidl, devicesAidl));
}

status_t AudioSystem::removeUidDeviceAffinities(uid_t uid) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    return statusTFromBinderStatus(aps->removeUidDeviceAffinities(uidAidl));
}

status_t AudioSystem::setUserIdDeviceAffinities(int userId,
                                                const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t userIdAidl = VALUE_OR_RETURN_STATUS(convertReinterpret<int32_t>(userId));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->setUserIdDeviceAffinities(userIdAidl, devicesAidl));
}

status_t AudioSystem::removeUserIdDeviceAffinities(int userId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    int32_t userIdAidl = VALUE_OR_RETURN_STATUS(convertReinterpret<int32_t>(userId));
    return statusTFromBinderStatus(aps->removeUserIdDeviceAffinities(userIdAidl));
}

status_t AudioSystem::startAudioSource(const struct audio_port_config* source,
                                       const audio_attributes_t* attributes,
                                       audio_port_handle_t* portId) {
    if (source == nullptr || attributes == nullptr || portId == nullptr) {
        return BAD_VALUE;
    }
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::AudioPortConfigFw sourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_port_config_AudioPortConfigFw(*source));
    media::audio::common::AudioAttributes attributesAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attributes));
    int32_t portIdAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->startAudioSource(sourceAidl, attributesAidl, &portIdAidl)));
    *portId = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_port_handle_t(portIdAidl));
    return OK;
}

status_t AudioSystem::stopAudioSource(audio_port_handle_t portId) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(aps->stopAudioSource(portIdAidl));
}

status_t AudioSystem::setMasterMono(bool mono) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    return statusTFromBinderStatus(aps->setMasterMono(mono));
}

status_t AudioSystem::getMasterMono(bool* mono) {
    if (mono == nullptr) {
        return BAD_VALUE;
    }
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    return statusTFromBinderStatus(aps->getMasterMono(mono));
}

status_t AudioSystem::setMasterBalance(float balance) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->setMasterBalance(balance);
}

status_t AudioSystem::getMasterBalance(float* balance) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->getMasterBalance(balance);
}

float
AudioSystem::getStreamVolumeDB(audio_stream_type_t stream, int index, audio_devices_t device) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return NAN;

    auto result = [&]() -> ConversionResult<float> {
        AudioStreamType streamAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_stream_type_t_AudioStreamType(stream));
        int32_t indexAidl = VALUE_OR_RETURN(convertIntegral<int32_t>(index));
        AudioDeviceDescription deviceAidl = VALUE_OR_RETURN(
                legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
        float retAidl;
        RETURN_IF_ERROR(statusTFromBinderStatus(
                aps->getStreamVolumeDB(streamAidl, indexAidl, deviceAidl, &retAidl)));
        return retAidl;
    }();
    return result.value_or(NAN);
}

status_t AudioSystem::getMicrophones(std::vector<media::MicrophoneInfoFw>* microphones) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == 0) return PERMISSION_DENIED;
    return af->getMicrophones(microphones);
}

status_t AudioSystem::setAudioHalPids(const std::vector<pid_t>& pids) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) return PERMISSION_DENIED;
    return af->setAudioHalPids(pids);
}

status_t AudioSystem::getSurroundFormats(unsigned int* numSurroundFormats,
                                         audio_format_t* surroundFormats,
                                         bool* surroundFormatsEnabled) {
    if (numSurroundFormats == nullptr || (*numSurroundFormats != 0 &&
                                          (surroundFormats == nullptr ||
                                           surroundFormatsEnabled == nullptr))) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    Int numSurroundFormatsAidl;
    numSurroundFormatsAidl.value =
            VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(*numSurroundFormats));
    std::vector<AudioFormatDescription> surroundFormatsAidl;
    std::vector<bool> surroundFormatsEnabledAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getSurroundFormats(&numSurroundFormatsAidl, &surroundFormatsAidl,
                                    &surroundFormatsEnabledAidl)));

    *numSurroundFormats = VALUE_OR_RETURN_STATUS(
            convertIntegral<unsigned int>(numSurroundFormatsAidl.value));
    RETURN_STATUS_IF_ERROR(
            convertRange(surroundFormatsAidl.begin(), surroundFormatsAidl.end(), surroundFormats,
                         aidl2legacy_AudioFormatDescription_audio_format_t));
    std::copy(surroundFormatsEnabledAidl.begin(), surroundFormatsEnabledAidl.end(),
            surroundFormatsEnabled);
    return OK;
}

status_t AudioSystem::getReportedSurroundFormats(unsigned int* numSurroundFormats,
                                                 audio_format_t* surroundFormats) {
    if (numSurroundFormats == nullptr || (*numSurroundFormats != 0 && surroundFormats == nullptr)) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    Int numSurroundFormatsAidl;
    numSurroundFormatsAidl.value =
            VALUE_OR_RETURN_STATUS(convertIntegral<int32_t>(*numSurroundFormats));
    std::vector<AudioFormatDescription> surroundFormatsAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getReportedSurroundFormats(&numSurroundFormatsAidl, &surroundFormatsAidl)));

    *numSurroundFormats = VALUE_OR_RETURN_STATUS(
            convertIntegral<unsigned int>(numSurroundFormatsAidl.value));
    RETURN_STATUS_IF_ERROR(
            convertRange(surroundFormatsAidl.begin(), surroundFormatsAidl.end(), surroundFormats,
                         aidl2legacy_AudioFormatDescription_audio_format_t));
    return OK;
}

status_t AudioSystem::setSurroundFormatEnabled(audio_format_t audioFormat, bool enabled) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    AudioFormatDescription audioFormatAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_format_t_AudioFormatDescription(audioFormat));
    return statusTFromBinderStatus(
            aps->setSurroundFormatEnabled(audioFormatAidl, enabled));
}

status_t AudioSystem::setAssistantServicesUids(const std::vector<uid_t>& uids) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<int32_t> uidsAidl = VALUE_OR_RETURN_STATUS(
                convertContainer<std::vector<int32_t>>(uids, legacy2aidl_uid_t_int32_t));
    return statusTFromBinderStatus(aps->setAssistantServicesUids(uidsAidl));
}

status_t AudioSystem::setActiveAssistantServicesUids(const std::vector<uid_t>& activeUids) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<int32_t> activeUidsAidl = VALUE_OR_RETURN_STATUS(
                convertContainer<std::vector<int32_t>>(activeUids, legacy2aidl_uid_t_int32_t));
    return statusTFromBinderStatus(aps->setActiveAssistantServicesUids(activeUidsAidl));
}

status_t AudioSystem::setA11yServicesUids(const std::vector<uid_t>& uids) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<int32_t> uidsAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<int32_t>>(uids, legacy2aidl_uid_t_int32_t));
    return statusTFromBinderStatus(aps->setA11yServicesUids(uidsAidl));
}

status_t AudioSystem::setCurrentImeUid(uid_t uid) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    return statusTFromBinderStatus(aps->setCurrentImeUid(uidAidl));
}

bool AudioSystem::isHapticPlaybackSupported() {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return false;

    auto result = [&]() -> ConversionResult<bool> {
        bool retVal;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->isHapticPlaybackSupported(&retVal)));
        return retVal;
    }();
    return result.value_or(false);
}

bool AudioSystem::isUltrasoundSupported() {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return false;

    auto result = [&]() -> ConversionResult<bool> {
        bool retVal;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->isUltrasoundSupported(&retVal)));
        return retVal;
    }();
    return result.value_or(false);
}

status_t AudioSystem::getHwOffloadFormatsSupportedForBluetoothMedia(
        audio_devices_t device, std::vector<audio_format_t>* formats) {
    if (formats == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<AudioFormatDescription> formatsAidl;
    AudioDeviceDescription deviceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_devices_t_AudioDeviceDescription(device));
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getHwOffloadFormatsSupportedForBluetoothMedia(deviceAidl, &formatsAidl)));
    *formats = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<audio_format_t>>(
                    formatsAidl,
                    aidl2legacy_AudioFormatDescription_audio_format_t));
    return OK;
}

status_t AudioSystem::listAudioProductStrategies(AudioProductStrategyVector& strategies) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<media::AudioProductStrategy> strategiesAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->listAudioProductStrategies(&strategiesAidl)));
    strategies = VALUE_OR_RETURN_STATUS(
            convertContainer<AudioProductStrategyVector>(strategiesAidl,
                                                         aidl2legacy_AudioProductStrategy));
    return OK;
}

audio_attributes_t AudioSystem::streamTypeToAttributes(audio_stream_type_t stream) {
    AudioProductStrategyVector strategies;
    listAudioProductStrategies(strategies);
    for (const auto& strategy : strategies) {
        auto attrVect = strategy.getVolumeGroupAttributes();
        auto iter = std::find_if(begin(attrVect), end(attrVect), [&stream](const auto& attributes) {
            return attributes.getStreamType() == stream;
        });
        if (iter != end(attrVect)) {
            return iter->getAttributes();
        }
    }
    ALOGE("invalid stream type %s when converting to attributes", toString(stream).c_str());
    return AUDIO_ATTRIBUTES_INITIALIZER;
}

audio_stream_type_t AudioSystem::attributesToStreamType(const audio_attributes_t& attr) {
    product_strategy_t psId;
    status_t ret = AudioSystem::getProductStrategyFromAudioAttributes(attr, psId);
    if (ret != NO_ERROR) {
        ALOGE("no strategy found for attributes %s", toString(attr).c_str());
        return AUDIO_STREAM_MUSIC;
    }
    AudioProductStrategyVector strategies;
    listAudioProductStrategies(strategies);
    for (const auto& strategy : strategies) {
        if (strategy.getId() == psId) {
            auto attrVect = strategy.getVolumeGroupAttributes();
            auto iter = std::find_if(begin(attrVect), end(attrVect), [&attr](const auto& refAttr) {
                return refAttr.matchesScore(attr) > 0;
            });
            if (iter != end(attrVect)) {
                return iter->getStreamType();
            }
        }
    }
    switch (attr.usage) {
        case AUDIO_USAGE_VIRTUAL_SOURCE:
            // virtual source is not expected to have an associated product strategy
            break;
        default:
            ALOGE("invalid attributes %s when converting to stream", toString(attr).c_str());
            break;
    }
    return AUDIO_STREAM_MUSIC;
}

status_t AudioSystem::getProductStrategyFromAudioAttributes(const audio_attributes_t& aa,
                                                            product_strategy_t& productStrategy,
                                                            bool fallbackOnDefault) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes aaAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(aa));
    int32_t productStrategyAidl;

    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getProductStrategyFromAudioAttributes(aaAidl, fallbackOnDefault,
            &productStrategyAidl)));
    productStrategy = VALUE_OR_RETURN_STATUS(
            aidl2legacy_int32_t_product_strategy_t(productStrategyAidl));
    return OK;
}

status_t AudioSystem::listAudioVolumeGroups(AudioVolumeGroupVector& groups) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    std::vector<media::AudioVolumeGroup> groupsAidl;
    RETURN_STATUS_IF_ERROR(
            statusTFromBinderStatus(aps->listAudioVolumeGroups(&groupsAidl)));
    groups = VALUE_OR_RETURN_STATUS(
            convertContainer<AudioVolumeGroupVector>(groupsAidl, aidl2legacy_AudioVolumeGroup));
    return OK;
}

status_t AudioSystem::getVolumeGroupFromAudioAttributes(const audio_attributes_t &aa,
                                                        volume_group_t& volumeGroup,
                                                        bool fallbackOnDefault) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;

    media::audio::common::AudioAttributes aaAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(aa));
    int32_t volumeGroupAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getVolumeGroupFromAudioAttributes(aaAidl, fallbackOnDefault, &volumeGroupAidl)));
    volumeGroup = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_volume_group_t(volumeGroupAidl));
    return OK;
}

status_t AudioSystem::setRttEnabled(bool enabled) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    return statusTFromBinderStatus(aps->setRttEnabled(enabled));
}

bool AudioSystem::isCallScreenModeSupported() {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) return false;

    auto result = [&]() -> ConversionResult<bool> {
        bool retAidl;
        RETURN_IF_ERROR(
                statusTFromBinderStatus(aps->isCallScreenModeSupported(&retAidl)));
        return retAidl;
    }();
    return result.value_or(false);
}

status_t AudioSystem::setDevicesRoleForStrategy(product_strategy_t strategy,
                                                device_role_t role,
                                                const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    int32_t strategyAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_product_strategy_t_int32_t(strategy));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->setDevicesRoleForStrategy(strategyAidl, roleAidl, devicesAidl));
}

status_t AudioSystem::removeDevicesRoleForStrategy(product_strategy_t strategy,
                                                   device_role_t role,
                                                   const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    int32_t strategyAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_product_strategy_t_int32_t(strategy));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->removeDevicesRoleForStrategy(strategyAidl, roleAidl, devicesAidl));
}

status_t
AudioSystem::clearDevicesRoleForStrategy(product_strategy_t strategy, device_role_t role) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    int32_t strategyAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_product_strategy_t_int32_t(strategy));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    return statusTFromBinderStatus(
            aps->clearDevicesRoleForStrategy(strategyAidl, roleAidl));
}

status_t AudioSystem::getDevicesForRoleAndStrategy(product_strategy_t strategy,
                                                   device_role_t role,
                                                   AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    int32_t strategyAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_product_strategy_t_int32_t(strategy));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getDevicesForRoleAndStrategy(strategyAidl, roleAidl, &devicesAidl)));
    devices = VALUE_OR_RETURN_STATUS(
            convertContainer<AudioDeviceTypeAddrVector>(devicesAidl,
                                                        aidl2legacy_AudioDeviceTypeAddress));
    return OK;
}

status_t AudioSystem::setDevicesRoleForCapturePreset(audio_source_t audioSource,
                                                     device_role_t role,
                                                     const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    AudioSource audioSourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(audioSource));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->setDevicesRoleForCapturePreset(audioSourceAidl, roleAidl, devicesAidl));
}

status_t AudioSystem::addDevicesRoleForCapturePreset(audio_source_t audioSource,
                                                     device_role_t role,
                                                     const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    AudioSource audioSourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(audioSource));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->addDevicesRoleForCapturePreset(audioSourceAidl, roleAidl, devicesAidl));
}

status_t AudioSystem::removeDevicesRoleForCapturePreset(
        audio_source_t audioSource, device_role_t role, const AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    AudioSource audioSourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(audioSource));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    return statusTFromBinderStatus(
            aps->removeDevicesRoleForCapturePreset(audioSourceAidl, roleAidl, devicesAidl));
}

status_t AudioSystem::clearDevicesRoleForCapturePreset(audio_source_t audioSource,
                                                       device_role_t role) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    AudioSource audioSourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(audioSource));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    return statusTFromBinderStatus(
            aps->clearDevicesRoleForCapturePreset(audioSourceAidl, roleAidl));
}

status_t AudioSystem::getDevicesForRoleAndCapturePreset(audio_source_t audioSource,
                                                        device_role_t role,
                                                        AudioDeviceTypeAddrVector& devices) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    AudioSource audioSourceAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_source_t_AudioSource(audioSource));
    media::DeviceRole roleAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_device_role_t_DeviceRole(role));
    std::vector<AudioDevice> devicesAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getDevicesForRoleAndCapturePreset(audioSourceAidl, roleAidl, &devicesAidl)));
    devices = VALUE_OR_RETURN_STATUS(
            convertContainer<AudioDeviceTypeAddrVector>(devicesAidl,
                                                        aidl2legacy_AudioDeviceTypeAddress));
    return OK;
}

status_t AudioSystem::getSpatializer(const sp<media::INativeSpatializerCallback>& callback,
                                          sp<media::ISpatializer>* spatializer) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (spatializer == nullptr) {
        return BAD_VALUE;
    }
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    media::GetSpatializerResponse response;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getSpatializer(callback, &response)));

    *spatializer = response.spatializer;
    return OK;
}

status_t AudioSystem::canBeSpatialized(const audio_attributes_t *attr,
                                    const audio_config_t *config,
                                    const AudioDeviceTypeAddrVector &devices,
                                    bool *canBeSpatialized) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (canBeSpatialized == nullptr) {
        return BAD_VALUE;
    }
    if (aps == 0) {
        return PERMISSION_DENIED;
    }
    audio_attributes_t attributes = attr != nullptr ? *attr : AUDIO_ATTRIBUTES_INITIALIZER;
    audio_config_t configuration = config != nullptr ? *config : AUDIO_CONFIG_INITIALIZER;

    std::optional<media::audio::common::AudioAttributes> attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(attributes));
    std::optional<AudioConfig> configAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_config_t_AudioConfig(configuration, false /*isInput*/));
    std::vector<AudioDevice> devicesAidl = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<AudioDevice>>(devices,
                                                       legacy2aidl_AudioDeviceTypeAddress));
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->canBeSpatialized(attrAidl, configAidl, devicesAidl, canBeSpatialized)));
    return OK;
}

status_t AudioSystem::getSoundDoseInterface(const sp<media::ISoundDoseCallback>& callback,
                                            sp<media::ISoundDose>* soundDose) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    if (soundDose == nullptr) {
        return BAD_VALUE;
    }

    RETURN_STATUS_IF_ERROR(af->getSoundDoseInterface(callback, soundDose));
    return OK;
}

status_t AudioSystem::getDirectPlaybackSupport(const audio_attributes_t *attr,
                                               const audio_config_t *config,
                                               audio_direct_mode_t* directMode) {
    if (attr == nullptr || config == nullptr || directMode == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    AudioConfig configAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_config_t_AudioConfig(*config, false /*isInput*/));

    media::AudioDirectMode retAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getDirectPlaybackSupport(attrAidl, configAidl, &retAidl)));
    *directMode = VALUE_OR_RETURN_STATUS(aidl2legacy_int32_t_audio_direct_mode_t_mask(
            static_cast<int32_t>(retAidl)));
    return NO_ERROR;
}

status_t AudioSystem::getDirectProfilesForAttributes(const audio_attributes_t* attr,
                                                std::vector<audio_profile>* audioProfiles) {
    if (attr == nullptr || audioProfiles == nullptr) {
        return BAD_VALUE;
    }

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));

    std::vector<media::audio::common::AudioProfile> audioProfilesAidl;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getDirectProfilesForAttributes(attrAidl, &audioProfilesAidl)));
    *audioProfiles = VALUE_OR_RETURN_STATUS(convertContainer<std::vector<audio_profile>>(
                    audioProfilesAidl, aidl2legacy_AudioProfile_audio_profile, false /*isInput*/));

    return NO_ERROR;
}

status_t AudioSystem::setRequestedLatencyMode(
            audio_io_handle_t output, audio_latency_mode_t mode) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->setRequestedLatencyMode(output, mode);
}

status_t AudioSystem::getSupportedLatencyModes(audio_io_handle_t output,
        std::vector<audio_latency_mode_t>* modes) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->getSupportedLatencyModes(output, modes);
}

status_t AudioSystem::setBluetoothVariableLatencyEnabled(bool enabled) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->setBluetoothVariableLatencyEnabled(enabled);
}

status_t AudioSystem::isBluetoothVariableLatencyEnabled(
        bool *enabled) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->isBluetoothVariableLatencyEnabled(enabled);
}

status_t AudioSystem::supportsBluetoothVariableLatency(
        bool *support) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->supportsBluetoothVariableLatency(support);
}

status_t AudioSystem::getAudioPolicyConfig(media::AudioPolicyConfig *config) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->getAudioPolicyConfig(config);
}

class CaptureStateListenerImpl : public media::BnCaptureStateListener,
                                 public IBinder::DeathRecipient {
public:
    CaptureStateListenerImpl(
            const sp<IAudioPolicyService>& aps,
            const sp<AudioSystem::CaptureStateListener>& listener)
            : mAps(aps), mListener(listener) {}

    void init() {
        bool active;
        status_t status = statusTFromBinderStatus(
                mAps->registerSoundTriggerCaptureStateListener(this, &active));
        if (status != NO_ERROR) {
            mListener->onServiceDied();
            return;
        }
        mListener->onStateChanged(active);
        IInterface::asBinder(mAps)->linkToDeath(this);
    }

    binder::Status setCaptureState(bool active) override {
        std::lock_guard _l(AudioSystem::gSoundTriggerMutex);
        mListener->onStateChanged(active);
        return binder::Status::ok();
    }

    void binderDied(const wp<IBinder>&) override {
        std::lock_guard _l(AudioSystem::gSoundTriggerMutex);
        mListener->onServiceDied();
        AudioSystem::gSoundTriggerCaptureStateListener = nullptr;
    }

private:
    // Need this in order to keep the death receipent alive.
    sp<IAudioPolicyService> mAps;
    sp<AudioSystem::CaptureStateListener> mListener;
};

status_t AudioSystem::registerSoundTriggerCaptureStateListener(
        const sp<CaptureStateListener>& listener) {
    LOG_ALWAYS_FATAL_IF(listener == nullptr);

    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == 0) {
        return PERMISSION_DENIED;
    }

    std::lock_guard _l(AudioSystem::gSoundTriggerMutex);
    gSoundTriggerCaptureStateListener = new CaptureStateListenerImpl(aps, listener);
    gSoundTriggerCaptureStateListener->init();

    return NO_ERROR;
}

status_t AudioSystem::setVibratorInfos(
        const std::vector<media::AudioVibratorInfo>& vibratorInfos) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->setVibratorInfos(vibratorInfos);
}

status_t AudioSystem::getMmapPolicyInfo(
        AudioMMapPolicyType policyType, std::vector<AudioMMapPolicyInfo> *policyInfos) {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->getMmapPolicyInfos(policyType, policyInfos);
}

int32_t AudioSystem::getAAudioMixerBurstCount() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->getAAudioMixerBurstCount();
}

int32_t AudioSystem::getAAudioHardwareBurstMinUsec() {
    const sp<IAudioFlinger> af = get_audio_flinger();
    if (af == nullptr) {
        return PERMISSION_DENIED;
    }
    return af->getAAudioHardwareBurstMinUsec();
}

status_t AudioSystem::getSupportedMixerAttributes(
        audio_port_handle_t portId, std::vector<audio_mixer_attributes_t> *mixerAttrs) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) {
        return PERMISSION_DENIED;
    }

    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    std::vector<media::AudioMixerAttributesInternal> _aidlReturn;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getSupportedMixerAttributes(portIdAidl, &_aidlReturn)));
    *mixerAttrs = VALUE_OR_RETURN_STATUS(
            convertContainer<std::vector<audio_mixer_attributes_t>>(
                    _aidlReturn,
                    aidl2legacy_AudioMixerAttributesInternal_audio_mixer_attributes_t));
    return OK;
}

status_t AudioSystem::setPreferredMixerAttributes(const audio_attributes_t *attr,
                                                  audio_port_handle_t portId,
                                                  uid_t uid,
                                                  const audio_mixer_attributes_t *mixerAttr) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) {
        return PERMISSION_DENIED;
    }

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    media::AudioMixerAttributesInternal mixerAttrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_mixer_attributes_t_AudioMixerAttributesInternal(*mixerAttr));
    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));

    return statusTFromBinderStatus(
            aps->setPreferredMixerAttributes(attrAidl, portIdAidl, uidAidl, mixerAttrAidl));
}

status_t AudioSystem::getPreferredMixerAttributes(
        const audio_attributes_t *attr,
        audio_port_handle_t portId,
        std::optional<audio_mixer_attributes_t> *mixerAttr) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) {
        return PERMISSION_DENIED;
    }

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    std::optional<media::AudioMixerAttributesInternal> _aidlReturn;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            aps->getPreferredMixerAttributes(attrAidl, portIdAidl, &_aidlReturn)));

    if (_aidlReturn.has_value()) {
         *mixerAttr = VALUE_OR_RETURN_STATUS(
                 aidl2legacy_AudioMixerAttributesInternal_audio_mixer_attributes_t(
                         _aidlReturn.value()));
    }
    return NO_ERROR;
}

status_t AudioSystem::clearPreferredMixerAttributes(const audio_attributes_t *attr,
                                                    audio_port_handle_t portId,
                                                    uid_t uid) {
    const sp<IAudioPolicyService> aps = get_audio_policy_service();
    if (aps == nullptr) {
        return PERMISSION_DENIED;
    }

    media::audio::common::AudioAttributes attrAidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_audio_attributes_t_AudioAttributes(*attr));
    int32_t uidAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_uid_t_int32_t(uid));
    int32_t portIdAidl = VALUE_OR_RETURN_STATUS(legacy2aidl_audio_port_handle_t_int32_t(portId));
    return statusTFromBinderStatus(
            aps->clearPreferredMixerAttributes(attrAidl, portIdAidl, uidAidl));
}

// ---------------------------------------------------------------------------

int AudioSystem::AudioPolicyServiceClient::addAudioPortCallback(
        const sp<AudioPortCallback>& callback) {
    std::lock_guard _l(mMutex);
    return mAudioPortCallbacks.insert(callback).second ? mAudioPortCallbacks.size() : -1;
}

int AudioSystem::AudioPolicyServiceClient::removeAudioPortCallback(
        const sp<AudioPortCallback>& callback) {
    std::lock_guard _l(mMutex);
    return mAudioPortCallbacks.erase(callback) > 0 ? mAudioPortCallbacks.size() : -1;
}

Status AudioSystem::AudioPolicyServiceClient::onAudioPortListUpdate() {
    std::lock_guard _l(mMutex);
    for (const auto& callback : mAudioPortCallbacks) {
        callback->onAudioPortListUpdate();
    }
    return Status::ok();
}

Status AudioSystem::AudioPolicyServiceClient::onAudioPatchListUpdate() {
    std::lock_guard _l(mMutex);
    for (const auto& callback : mAudioPortCallbacks) {
        callback->onAudioPatchListUpdate();
    }
    return Status::ok();
}

// ----------------------------------------------------------------------------
int AudioSystem::AudioPolicyServiceClient::addAudioVolumeGroupCallback(
        const sp<AudioVolumeGroupCallback>& callback) {
    std::lock_guard _l(mMutex);
    return mAudioVolumeGroupCallbacks.insert(callback).second
            ? mAudioVolumeGroupCallbacks.size() : -1;
}

int AudioSystem::AudioPolicyServiceClient::removeAudioVolumeGroupCallback(
        const sp<AudioVolumeGroupCallback>& callback) {
    std::lock_guard _l(mMutex);
    return mAudioVolumeGroupCallbacks.erase(callback) > 0
            ? mAudioVolumeGroupCallbacks.size() : -1;
}

Status AudioSystem::AudioPolicyServiceClient::onAudioVolumeGroupChanged(int32_t group,
                                                                        int32_t flags) {
    volume_group_t groupLegacy = VALUE_OR_RETURN_BINDER_STATUS(
            aidl2legacy_int32_t_volume_group_t(group));
    int flagsLegacy = VALUE_OR_RETURN_BINDER_STATUS(convertReinterpret<int>(flags));

    std::lock_guard _l(mMutex);
    for (const auto& callback : mAudioVolumeGroupCallbacks) {
        callback->onAudioVolumeGroupChanged(groupLegacy, flagsLegacy);
    }
    return Status::ok();
}
// ----------------------------------------------------------------------------

Status AudioSystem::AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(
        const ::std::string& regId, int32_t state) {
    ALOGV("AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(%s, %d)", regId.c_str(), state);

    String8 regIdLegacy = VALUE_OR_RETURN_BINDER_STATUS(aidl2legacy_string_view_String8(regId));
    int stateLegacy = VALUE_OR_RETURN_BINDER_STATUS(convertReinterpret<int>(state));
    dynamic_policy_callback cb = NULL;
    {
        std::lock_guard _l(AudioSystem::gMutex);
        cb = gDynPolicyCallback;
    }

    if (cb != NULL) {
        cb(DYNAMIC_POLICY_EVENT_MIX_STATE_UPDATE, regIdLegacy, stateLegacy);
    }
    return Status::ok();
}

Status AudioSystem::AudioPolicyServiceClient::onRecordingConfigurationUpdate(
        int32_t event,
        const media::RecordClientInfo& clientInfo,
        const AudioConfigBase& clientConfig,
        const std::vector<media::EffectDescriptor>& clientEffects,
        const AudioConfigBase& deviceConfig,
        const std::vector<media::EffectDescriptor>& effects,
        int32_t patchHandle,
        AudioSource source) {
    record_config_callback cb = NULL;
    {
        std::lock_guard _l(AudioSystem::gMutex);
        cb = gRecordConfigCallback;
    }

    if (cb != NULL) {
        int eventLegacy = VALUE_OR_RETURN_BINDER_STATUS(convertReinterpret<int>(event));
        record_client_info_t clientInfoLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                aidl2legacy_RecordClientInfo_record_client_info_t(clientInfo));
        audio_config_base_t clientConfigLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                aidl2legacy_AudioConfigBase_audio_config_base_t(clientConfig, true /*isInput*/));
        std::vector<effect_descriptor_t> clientEffectsLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                convertContainer<std::vector<effect_descriptor_t>>(
                        clientEffects,
                        aidl2legacy_EffectDescriptor_effect_descriptor_t));
        audio_config_base_t deviceConfigLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                aidl2legacy_AudioConfigBase_audio_config_base_t(deviceConfig, true /*isInput*/));
        std::vector<effect_descriptor_t> effectsLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                convertContainer<std::vector<effect_descriptor_t>>(
                        effects,
                        aidl2legacy_EffectDescriptor_effect_descriptor_t));
        audio_patch_handle_t patchHandleLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                aidl2legacy_int32_t_audio_patch_handle_t(patchHandle));
        audio_source_t sourceLegacy = VALUE_OR_RETURN_BINDER_STATUS(
                aidl2legacy_AudioSource_audio_source_t(source));
        cb(eventLegacy, &clientInfoLegacy, &clientConfigLegacy, clientEffectsLegacy,
           &deviceConfigLegacy, effectsLegacy, patchHandleLegacy, sourceLegacy);
    }
    return Status::ok();
}

Status AudioSystem::AudioPolicyServiceClient::onRoutingUpdated() {
    routing_callback cb = NULL;
    {
        std::lock_guard _l(AudioSystem::gMutex);
        cb = gRoutingCallback;
    }

    if (cb != NULL) {
        cb();
    }
    return Status::ok();
}

Status AudioSystem::AudioPolicyServiceClient::onVolumeRangeInitRequest() {
    vol_range_init_req_callback cb = NULL;
    {
        std::lock_guard _l(AudioSystem::gMutex);
        cb = gVolRangeInitReqCallback;
    }

    if (cb != NULL) {
        cb();
    }
    return Status::ok();
}

void AudioSystem::AudioPolicyServiceClient::binderDied(const wp<IBinder>& who __unused) {
    {
        std::lock_guard _l(mMutex);
        for (const auto& callback : mAudioPortCallbacks) {
            callback->onServiceDied();
        }
        for (const auto& callback : mAudioVolumeGroupCallbacks) {
            callback->onServiceDied();
        }
    }
    AudioSystem::clearAudioPolicyService();

    ALOGW("AudioPolicyService server died!");
}

ConversionResult<record_client_info_t>
aidl2legacy_RecordClientInfo_record_client_info_t(const media::RecordClientInfo& aidl) {
    record_client_info_t legacy;
    legacy.riid = VALUE_OR_RETURN(aidl2legacy_int32_t_audio_unique_id_t(aidl.riid));
    legacy.uid = VALUE_OR_RETURN(aidl2legacy_int32_t_uid_t(aidl.uid));
    legacy.session = VALUE_OR_RETURN(aidl2legacy_int32_t_audio_session_t(aidl.session));
    legacy.source = VALUE_OR_RETURN(aidl2legacy_AudioSource_audio_source_t(aidl.source));
    legacy.port_id = VALUE_OR_RETURN(aidl2legacy_int32_t_audio_port_handle_t(aidl.portId));
    legacy.silenced = aidl.silenced;
    return legacy;
}

ConversionResult<media::RecordClientInfo>
legacy2aidl_record_client_info_t_RecordClientInfo(const record_client_info_t& legacy) {
    media::RecordClientInfo aidl;
    aidl.riid = VALUE_OR_RETURN(legacy2aidl_audio_unique_id_t_int32_t(legacy.riid));
    aidl.uid = VALUE_OR_RETURN(legacy2aidl_uid_t_int32_t(legacy.uid));
    aidl.session = VALUE_OR_RETURN(legacy2aidl_audio_session_t_int32_t(legacy.session));
    aidl.source = VALUE_OR_RETURN(legacy2aidl_audio_source_t_AudioSource(legacy.source));
    aidl.portId = VALUE_OR_RETURN(legacy2aidl_audio_port_handle_t_int32_t(legacy.port_id));
    aidl.silenced = legacy.silenced;
    return aidl;
}

} // namespace android