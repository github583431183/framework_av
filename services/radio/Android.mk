# Copyright 2014 The Android Open Source Project
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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_SRC_FILES:= \
    RadioService.cpp

LOCAL_SHARED_LIBRARIES:= \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libaudioclient \
    libhardware \
    libradio \
    libradio_metadata

ifeq ($(USE_LEGACY_LOCAL_AUDIO_HAL),true)
# libhardware configuration
LOCAL_SRC_FILES +=               \
    RadioHalLegacy.cpp
else
# Treble configuration

LOCAL_SRC_FILES += \
    HidlUtils.cpp \
    RadioHalHidl.cpp

LOCAL_SHARED_LIBRARIES += \
    libhwbinder \
    libhidlbase \
    libhidltransport \
    libbase \
    libaudiohal \
    android.hardware.broadcastradio@1.0
endif

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MULTILIB := $(AUDIOSERVER_MULTILIB)

LOCAL_MODULE:= libradioservice

include $(BUILD_SHARED_LIBRARY)
