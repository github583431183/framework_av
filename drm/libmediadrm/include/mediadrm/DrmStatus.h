/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <media/stagefright/foundation/ABase.h>
#include <stdint.h>

#include <mediadrm/DrmUtils.h>

#ifndef DRM_STATUS_
#define DRM_STATUS_

namespace android {

struct DrmStatus {
  DrmStatus(status_t status) {
    mStatus = status;
  }
  operator status_t() const { return mStatus; }
  bool operator==(status_t other) { return mStatus == other; }
  bool operator!=(status_t other) { return mStatus != other; }

  private:
    status_t mStatus;
    int32_t cdmErr(), oemErr();
    DISALLOW_EVIL_CONSTRUCTORS(DrmStatus);
};

}  // namespace android

#endif  // DRM_STATUS_