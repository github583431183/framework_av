#
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    FwdLockConv.c

LOCAL_C_INCLUDES := \
    frameworks/av/drm/libdrmframework/plugins/forward-lock/internal-format/common \
    external/openssl/include

LOCAL_SHARED_LIBRARIES := libcrypto

LOCAL_MODULE := libfwdlock-converter

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
