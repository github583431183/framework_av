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

#ifndef ANDROID_MEDIA_RESOURCEMANAGERSERVICENEW_H
#define ANDROID_MEDIA_RESOURCEMANAGERSERVICENEW_H

#include "ResourceManagerService.h"

namespace android {

class ResourceTracker;

class ResourceManagerServiceNew : public ResourceManagerService {
public:

    explicit ResourceManagerServiceNew(const sp<ProcessInfoInterface>& processInfo,
                                       const sp<SystemCallbackInterface>& systemResource);
    virtual ~ResourceManagerServiceNew();

    // IResourceManagerService interface
    Status config(const std::vector<MediaResourcePolicyParcel>& policies) override;

    Status addResource(const ClientInfoParcel& clientInfo,
                       const std::shared_ptr<IResourceManagerClient>& client,
                       const std::vector<MediaResourceParcel>& resources) override;

    Status removeResource(const ClientInfoParcel& clientInfo,
                          const std::vector<MediaResourceParcel>& resources) override;

    Status removeClient(const ClientInfoParcel& clientInfo) override;

    Status reclaimResource(const ClientInfoParcel& clientInfo,
                           const std::vector<MediaResourceParcel>& resources,
                           bool* _aidl_return) override;

    Status overridePid(int32_t originalPid, int32_t newPid) override;

    Status overrideProcessInfo(const std::shared_ptr<IResourceManagerClient>& client,
                               int32_t pid, int32_t procState, int32_t oomScore) override;

    Status markClientForPendingRemoval(const ClientInfoParcel& clientInfo) override;

    Status reclaimResourcesFromClientsPendingRemoval(int32_t pid) override;

    Status notifyClientCreated(const ClientInfoParcel& clientInfo) override;

    Status notifyClientStarted(const ClientConfigParcel& clientConfig) override;

    Status notifyClientStopped(const ClientConfigParcel& clientConfig) override;

    Status notifyClientConfigChanged(const ClientConfigParcel& clientConfig) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

    friend class ResourceTracker;

private:

    // Eventually this implementation of IResourceManagerService
    // (ResourceManagerServiceNew) will replace the older implementation
    // (ResourceManagerService).
    // To make the transition easier, this implementation overrides the
    // following private virtual methods from ResourceManagerService.

    // Initializes the internal state of the ResourceManagerService
    void init() override;

    void setObserverService(
            const std::shared_ptr<ResourceObserverService>& observerService) override;

    // Removes the pid from the override map.
    void removeProcessInfoOverride(int pid) override;

    // Gets the list of all the clients who own the specified resource type.
    // Returns false if any client belongs to a process with higher priority than the
    // calling process. The clients will remain unchanged if returns false.
    bool getAllClients_l(const ResourceRequestInfo& resourceRequestInfo,
                         std::vector<ClientInfo>& clientsInfo) override;

    // Gets the client who owns biggest piece of specified resource type from pid.
    // Returns false with no change to client if there are no clients holding resources of this
    // type.
    bool getBiggestClient_l(int pid, MediaResource::Type type,
                            MediaResource::SubType subType,
                            ClientInfo& clientsInfo,
                            bool pendingRemovalOnly = false) override;

    bool overridePid_l(int32_t originalPid, int32_t newPid) override;

    bool overrideProcessInfo_l(const std::shared_ptr<IResourceManagerClient>& client,
                               int pid, int procState, int oomScore) override;

    // Get priority from process's pid
    bool getPriority_l(int pid, int* priority) const override;

    // Gets lowest priority process that has the specified resource type.
    // Returns false if failed. The output parameters will remain unchanged if failed.
    bool getLowestPriorityPid_l(MediaResource::Type type, MediaResource::SubType subType,
                                int* lowestPriorityPid, int* lowestPriority) override;

    // Get the client for given pid and the clientId from the map
    std::shared_ptr<IResourceManagerClient> getClient(
        int pid, const int64_t& clientId) const override;

    // Remove the client for given pid and the clientId from the map
    bool removeClient(int pid, const int64_t& clientId) override;

    // Get all the resource status for dump
    void getResourceDump(std::string& resourceLog) const override;

    // Returns a unmodifiable reference to the internal resource state as a map
    const std::map<int, ResourceInfos>& getResourceMap() const override;

    Status removeResource(const ClientInfoParcel& clientInfo, bool checkValid) override;

private:
    std::shared_ptr<ResourceTracker> mResourceTracker;
};

// ----------------------------------------------------------------------------
} // namespace android

#endif // ANDROID_MEDIA_RESOURCEMANAGERSERVICENEW_H
