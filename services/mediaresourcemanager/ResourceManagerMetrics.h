/*
**
** Copyright 2023, The Android Open Source Project
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

#ifndef ANDROID_MEDIA_RESOURCEMANAGERMETRICS_H_
#define ANDROID_MEDIA_RESOURCEMANAGERMETRICS_H_

#include "ResourceManagerService.h"

namespace android {

using ::aidl::android::media::ClientInfoParcel;
using ::aidl::android::media::ClientConfigParcel;
using ::aidl::android::media::IResourceManagerClient;

struct ProcessInfoInterface;
class ProcessTerminationWatcher;

//
// Enumeration for Codec bucket based on:
//   - Encoder or Decoder
//   - hardware implementation or not
//   - Audio/Video/Image codec
//
enum CodecBucket {
    CodecBucketUnspecified = 0,
    HwAudioEncoder = 1,
    HwAudioDecoder = 2,
    HwVideoEncoder = 3,
    HwVideoDecoder = 4,
    HwImageEncoder = 5,
    HwImageDecoder = 6,
    SwAudioEncoder = 7,
    SwAudioDecoder = 8,
    SwVideoEncoder = 9,
    SwVideoDecoder = 10,
    SwImageEncoder = 11,
    SwImageDecoder = 12,
    CodecBucketMaxSize = 13,
};

// Map of client id and client configuration, when it was started last.
typedef std::map<int64_t, ClientConfigParcel> ClientConfigMap;

// Map of pid and the uid.
typedef std::map<int32_t, uid_t> PidUidMap;

// Map of concurrent codes by Codec type bucket.
struct ConcurrentCodecsMap {
    int& operator[](CodecBucket index) {
        return mCodec[index];
    }

    const int& operator[](CodecBucket index) const {
        return mCodec[index];
    }

private:
    int mCodec[CodecBucketMaxSize] = {0};
};

// Current and Peak ConcurrentCodecMap for a process.
struct ConcurrentCodecs {
    ConcurrentCodecsMap mCurrent;
    ConcurrentCodecsMap mPeak;
};

// Current and Peak pixel count for a process.
struct PixelCount {
    long mCurrent = 0;
    long mPeak = 0;
};

//
// ResourceManagerMetrics class that maintaines concurrent codec count based:
//
//  1. # of concurrent active codecs (initialized, but aren't released yet) of given
//     implementation (by codec name) across the system.
//
//  2. # of concurrent codec usage (started, but not stopped yet), which is
//  measured using codec type bucket (CodecBucket) for:
//   - each process/application.
//   - across the system.
//  Also the peak count of the same for each process/application is maintained.
//
//  3. # of Peak Concurrent Pixels for each process/application.
//  This should help with understanding the (video) memory usage per
//  application.
//
//
class ResourceManagerMetrics {
public:
    ResourceManagerMetrics(const sp<ProcessInfoInterface>& processInfo);
    ~ResourceManagerMetrics();

    // To be called when a client is created.
    void notifyClientCreated(const ClientInfoParcel& clientInfo);

    // To be called when a client is released.
    void notifyClientReleased(const ClientInfoParcel& clientInfo);

    // To be called when a client is started.
    void notifyClientStarted(const ClientConfigParcel& clientConfig);

    // To be called when a client is stopped.
    void notifyClientStopped(const ClientConfigParcel& clientConfig);

    // To be called when after a reclaim event.
    void pushReclaimAtom(const ClientInfoParcel& clientInfo,
                         const std::vector<int>& priorities,
                         const Vector<std::shared_ptr<IResourceManagerClient>>& clients,
                         const PidUidVector& idList, bool reclaimed);

    // To be called for a pid comes in as override for an existing pid.
    void addPid(int pid);

    // Get the peak concurrent pixel count (associated with the video codecs) for the process.
    long getPeakConcurrentPixelCount(int pid) const;
    // Get the current concurrent pixel count (associated with the video codecs) for the process.
    long getConcurrentPixelCount(int pid) const;

private:
    ResourceManagerMetrics() = delete;
    ResourceManagerMetrics(const ResourceManagerMetrics&) = delete;
    ResourceManagerMetrics(ResourceManagerMetrics&&) = delete;
    ResourceManagerMetrics& operator=(const ResourceManagerMetrics&) = delete;
    ResourceManagerMetrics& operator=(ResourceManagerMetrics&&) = delete;

    // To increase/decrease the concurrent codec usage for a given CodecBucket.
    void increaseConcurrentCodecs(int32_t pid, CodecBucket codecBucket);
    void decreaseConcurrentCodecs(int32_t pid, CodecBucket codecBucket);

    // To increase/decrease the concurrent pixels usage for a process.
    void increasePixelCount(int32_t pid, long pixels);
    void decreasePixelCount(int32_t pid, long pixels);

    // Issued when a list of processess/applications are terminated.
    void onProcessTerminated(const std::vector<int32_t>& pids);

    // To push conccuret codec usage of a process/application.
    void pushConcurrentUsageReport(int32_t pid);

private:
    std::mutex mLock;

    // Map of pid and the uid.
    PidUidMap mPidUidMap;

    // Map of client id and the configuration.
    ClientConfigMap mClientConfigMap;

    // Concurrent and Peak Pixel count for each process/application.
    std::map<int32_t, PixelCount> mProcessPixelsMap;

    // Map of resources (name) and number of concurrent instances
    std::map<std::string, int> mConcurrentResourceCountMap;

    // Map of concurrent codes by CodecBucket across the system.
    ConcurrentCodecsMap mConcurrentCodecsMap;
    // Map of concurrent and peak codes by CodecBucket for each process/application.
    std::map<int32_t, ConcurrentCodecs> mProcessConcurrentCodecsMap;

    // Process termination watcher.
    std::unique_ptr<ProcessTerminationWatcher> mProcessTerminationWatcher;
};

} // namespace android

#endif  // ANDROID_MEDIA_RESOURCEMANAGERMETRICS_H_
