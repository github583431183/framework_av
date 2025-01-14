/*
**
** Copyright 2022, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#pragma once

#include "IAfPatchPanel.h"
#include "PatchCommandThread.h"

#include <audio_utils/mutex.h>
#include <sounddose/SoundDoseManager.h>

#include <unordered_map>

namespace android {

constexpr static int kMaxTimestampDeltaInSec = 120;

class IAfMelReporterCallback : public virtual RefBase {
public:
    virtual audio_utils::mutex& mutex() const
            RETURN_CAPABILITY(audio_utils::AudioFlinger_Mutex) = 0;
    virtual const sp<PatchCommandThread>& getPatchCommandThread() = 0;
    virtual sp<IAfThreadBase> checkOutputThread_l(audio_io_handle_t ioHandle) const
            REQUIRES(mutex()) = 0;
};

/**
 * Class for listening to new patches and starting the MEL computation. MelReporter is
 * concealed within AudioFlinger, their lifetimes are the same.
 */
class MelReporter : public PatchCommandThread::PatchCommandListener,
                    public IMelReporterCallback {
public:
    explicit MelReporter(const sp<IAfMelReporterCallback>& afMelReporterCallback)
        : mAfMelReporterCallback(afMelReporterCallback) {}

    void onFirstRef() override;

    /**
     * Activates the MEL reporting from the HAL sound dose interface. If the HAL
     * does not support the sound dose interface for this module, the internal MEL
     * calculation will be use.
     *
     * <p>If the device is using the audio AIDL HAL then this method will try to get the sound
     * dose interface from IModule#getSoundDose(). Otherwise, if the legacy audio HIDL HAL is used
     * this method will be looking for the standalone sound dose implementation. It falls back to
     * the internal MEL computation if no valid sound dose interface can be retrieved.
     *
     * @return true if the MEL reporting will be done from any sound dose HAL interface
     * implementation, false otherwise.
     */
    bool activateHalSoundDoseComputation(const std::string& module,
            const sp<DeviceHalInterface>& device) EXCLUDES_MelReporter_Mutex;

    /**
     * Activates the MEL reporting from internal framework values. These are used
     * as a fallback when there is no sound dose interface implementation from HAL.
     * Note: the internal CSD computation does not guarantee a certification with
     * IEC62368-1 3rd edition or EN50332-3
     */
    void activateInternalSoundDoseComputation() EXCLUDES_MelReporter_Mutex;

    sp<media::ISoundDose> getSoundDoseInterface(const sp<media::ISoundDoseCallback>& callback);

    std::string dump();

    // IMelReporterCallback methods
    void stopMelComputationForDeviceId(audio_port_handle_t deviceId) final
            EXCLUDES_MelReporter_Mutex;
    void startMelComputationForDeviceId(audio_port_handle_t deviceId) final
            EXCLUDES_MelReporter_Mutex;

    // PatchCommandListener methods
    void onCreateAudioPatch(audio_patch_handle_t handle,
            const IAfPatchPanel::Patch& patch) final
            EXCLUDES_AudioFlinger_Mutex;
    void onReleaseAudioPatch(audio_patch_handle_t handle) final EXCLUDES_AudioFlinger_Mutex;
    void onUpdateAudioPatch(audio_patch_handle_t oldHandle,
                            audio_patch_handle_t newHandle,
            const IAfPatchPanel::Patch& patch) final EXCLUDES_AudioFlinger_Mutex;

    /**
     * The new metadata can determine whether we should compute MEL for the given thread.
     * This is the case only if one of the tracks in the thread mix is using MEDIA or GAME.
     * Otherwise, this method will disable CSD.
     **/
    void updateMetadataForCsd(audio_io_handle_t streamHandle,
            const std::vector<playback_track_metadata_v7_t>& metadataVec)
            EXCLUDES_AudioFlinger_Mutex;

    void resetReferencesForTest();

private:
    struct ActiveMelPatch {
        audio_io_handle_t streamHandle{AUDIO_IO_HANDLE_NONE};
        /**
         * Stores device ids and whether they are compatible for CSD calculation.
         * The boolean value can change since BT audio device types are user-configurable
         * to headphones/headsets or other device types.
         */
        std::vector<std::pair<audio_port_handle_t,bool>> deviceStates;
        bool csdActive;
    };

    void stopInternalMelComputation();
    audio_utils::mutex& mutex() const RETURN_CAPABILITY(audio_utils::MelReporter_Mutex) {
        return mMutex;
    }

    /** Should be called with the following order of locks: mAudioFlinger.mutex() -> mutex(). */
    void stopMelComputationForPatch_l(const ActiveMelPatch& patch) REQUIRES(mutex());

    /** Should be called with the following order of locks: mAudioFlinger.mutex() -> mutex(). */
    void startMelComputationForActivePatch_l(const ActiveMelPatch& patch) REQUIRES(mutex());

    std::optional<audio_patch_handle_t>
    activePatchStreamHandle_l(audio_io_handle_t streamHandle) REQUIRES(mutex());

    bool useHalSoundDoseInterface_l() REQUIRES(mutex());

    sp<IAfMelReporterCallback> mAfMelReporterCallback;

    /* const */ sp<SoundDoseManager> mSoundDoseManager;  // set onFirstRef

    /**
     * Lock for protecting the active mel patches. Do not mix with the AudioFlinger lock.
     * Locking order AudioFlinger::mutex() -> PatchCommandThread::mutex() -> MelReporter::mutex().
     */
    mutable audio_utils::mutex mMutex{audio_utils::MutexOrder::kMelReporter_Mutex};
    std::unordered_map<audio_patch_handle_t, ActiveMelPatch> mActiveMelPatches
            GUARDED_BY(mutex());
    std::unordered_map<audio_port_handle_t, int> mActiveDevices GUARDED_BY(mutex());
    bool mUseHalSoundDoseInterface GUARDED_BY(mutex()) = false;
};

}  // namespace android
