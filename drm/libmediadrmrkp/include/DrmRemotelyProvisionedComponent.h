/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef DRM_RKP_COMPONENT_H_
#define DRM_RKP_COMPONENT_H_

#include <aidl/android/hardware/drm/IDrmPlugin.h>
#include <aidl/android/hardware/security/keymint/BnRemotelyProvisionedComponent.h>

using ::aidl::android::hardware::drm::IDrmPlugin;
namespace android::mediadrm {

class DrmRemotelyProvisionedComponent : public BnRemotelyProvisionedComponent {
public:
    DrmRemotelyProvisionedComponent(shared_ptr<IDrmPlugin> drm);
    ScopedAStatus getHardwareInfo(RpcHardwareInfo* info) override;

    ScopedAStatus generateEcdsaP256KeyPair(bool testMode, MacedPublicKey* macedPublicKey,
                                           std::vector<uint8_t>* privateKeyHandle) override;

    ScopedAStatus generateCertificateRequest(bool testMode,
                                             const std::vector<MacedPublicKey>& keysToSign,
                                             const std::vector<uint8_t>& endpointEncCertChain,
                                             const std::vector<uint8_t>& challenge,
                                             DeviceInfo* deviceInfo, ProtectedData* protectedData,
                                             std::vector<uint8_t>* keysToSignMac) override;

    ScopedAStatus generateCertificateRequestV2(const std::vector<MacedPublicKey>& keysToSign,
                                               const std::vector<uint8_t>& challenge,
                                               std::vector<uint8_t>* csr) override;
    std::string getDrmPropertyString(const std::string& in_propertyName) const;
private:
    shared_ptr<IDrmPlugin> mDrm;
}
} // namespace android::mediadrm

#endif  // DRM_RKP_COMPONENT_H_