/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef MEDIA_ERRORS_H_

#define MEDIA_ERRORS_H_

#include <utils/Errors.h>

namespace android {

enum {
    // status_t map for errors in the media framework
    // OK or NO_ERROR or 0 represents no error.

    // See system/core/include/utils/Errors.h
    // System standard errors from -1 through (possibly) -133
    //
    // Errors with special meanings and side effects.
    // INVALID_OPERATION:  Operation attempted in an illegal state (will try to signal to app).
    // DEAD_OBJECT:        Signal from CodecBase to MediaCodec that MediaServer has died.
    // NAME_NOT_FOUND:     Signal from CodecBase to MediaCodec that the component was not found.

    // Media errors
    MEDIA_ERROR_BASE        = -1000,

    ERROR_ALREADY_CONNECTED = MEDIA_ERROR_BASE,
    ERROR_NOT_CONNECTED     = MEDIA_ERROR_BASE - 1,
    ERROR_UNKNOWN_HOST      = MEDIA_ERROR_BASE - 2,
    ERROR_CANNOT_CONNECT    = MEDIA_ERROR_BASE - 3,
    ERROR_IO                = MEDIA_ERROR_BASE - 4,
    ERROR_CONNECTION_LOST   = MEDIA_ERROR_BASE - 5,
    ERROR_MALFORMED         = MEDIA_ERROR_BASE - 7,
    ERROR_OUT_OF_RANGE      = MEDIA_ERROR_BASE - 8,
    ERROR_BUFFER_TOO_SMALL  = MEDIA_ERROR_BASE - 9,
    ERROR_UNSUPPORTED       = MEDIA_ERROR_BASE - 10,
    ERROR_END_OF_STREAM     = MEDIA_ERROR_BASE - 11,

    // Not technically an error.
    INFO_FORMAT_CHANGED    = MEDIA_ERROR_BASE - 12,
    INFO_DISCONTINUITY     = MEDIA_ERROR_BASE - 13,
    INFO_OUTPUT_BUFFERS_CHANGED = MEDIA_ERROR_BASE - 14,

    // The following constant values should be in sync with
    // drm/drm_framework_common.h
    DRM_ERROR_BASE = -2000,

    ERROR_DRM_UNKNOWN                        = DRM_ERROR_BASE,
    ERROR_DRM_NO_LICENSE                     = DRM_ERROR_BASE - 1,
    ERROR_DRM_LICENSE_EXPIRED                = DRM_ERROR_BASE - 2,
    ERROR_DRM_SESSION_NOT_OPENED             = DRM_ERROR_BASE - 3,
    ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED   = DRM_ERROR_BASE - 4,
    ERROR_DRM_DECRYPT                        = DRM_ERROR_BASE - 5,
    ERROR_DRM_CANNOT_HANDLE                  = DRM_ERROR_BASE - 6,
    ERROR_DRM_TAMPER_DETECTED                = DRM_ERROR_BASE - 7,
    ERROR_DRM_NOT_PROVISIONED                = DRM_ERROR_BASE - 8,
    ERROR_DRM_DEVICE_REVOKED                 = DRM_ERROR_BASE - 9,
    ERROR_DRM_RESOURCE_BUSY                  = DRM_ERROR_BASE - 10,
    ERROR_DRM_INSUFFICIENT_OUTPUT_PROTECTION = DRM_ERROR_BASE - 11,
    ERROR_DRM_INSUFFICIENT_SECURITY          = DRM_ERROR_BASE - 12,
    ERROR_DRM_FRAME_TOO_LARGE                = DRM_ERROR_BASE - 13,
    ERROR_DRM_RESOURCE_CONTENTION            = DRM_ERROR_BASE - 14,
    ERROR_DRM_SESSION_LOST_STATE             = DRM_ERROR_BASE - 15,
    ERROR_DRM_INVALID_STATE                  = DRM_ERROR_BASE - 16,

    // New in S / drm@1.4:
    ERROR_DRM_CERTIFICATE_MALFORMED          = DRM_ERROR_BASE - 17,
    ERROR_DRM_CERTIFICATE_MISSING            = DRM_ERROR_BASE - 18,
    ERROR_DRM_CRYPTO_LIBRARY                 = DRM_ERROR_BASE - 19,
    ERROR_DRM_GENERIC_OEM                    = DRM_ERROR_BASE - 20,
    ERROR_DRM_GENERIC_PLUGIN                 = DRM_ERROR_BASE - 21,
    ERROR_DRM_INIT_DATA                      = DRM_ERROR_BASE - 22,
    ERROR_DRM_KEY_NOT_LOADED                 = DRM_ERROR_BASE - 23,
    ERROR_DRM_LICENSE_PARSE                  = DRM_ERROR_BASE - 24,
    ERROR_DRM_LICENSE_POLICY                 = DRM_ERROR_BASE - 25,
    ERROR_DRM_LICENSE_RELEASE                = DRM_ERROR_BASE - 26,
    ERROR_DRM_LICENSE_REQUEST_REJECTED       = DRM_ERROR_BASE - 27,
    ERROR_DRM_LICENSE_RESTORE                = DRM_ERROR_BASE - 28,
    ERROR_DRM_LICENSE_STATE                  = DRM_ERROR_BASE - 29,
    ERROR_DRM_MEDIA_FRAMEWORK                = DRM_ERROR_BASE - 30,
    ERROR_DRM_PROVISIONING_CERTIFICATE       = DRM_ERROR_BASE - 31,
    ERROR_DRM_PROVISIONING_CONFIG            = DRM_ERROR_BASE - 32,
    ERROR_DRM_PROVISIONING_PARSE             = DRM_ERROR_BASE - 33,
    ERROR_DRM_PROVISIONING_REQUEST_REJECTED  = DRM_ERROR_BASE - 34,
    ERROR_DRM_PROVISIONING_RETRY             = DRM_ERROR_BASE - 35,
    ERROR_DRM_SECURE_STOP_RELEASE            = DRM_ERROR_BASE - 36,
    ERROR_DRM_STORAGE_READ                   = DRM_ERROR_BASE - 37,
    ERROR_DRM_STORAGE_WRITE                  = DRM_ERROR_BASE - 38,
    ERROR_DRM_ZERO_SUBSAMPLES                = DRM_ERROR_BASE - 39,
    ERROR_DRM_LAST_USED_ERRORCODE            = ERROR_DRM_ZERO_SUBSAMPLES,

    ERROR_DRM_VENDOR_MAX                     = DRM_ERROR_BASE - 500,
    ERROR_DRM_VENDOR_MIN                     = DRM_ERROR_BASE - 999,

    // Heartbeat Error Codes
    HEARTBEAT_ERROR_BASE = -3000,
    ERROR_HEARTBEAT_TERMINATE_REQUESTED                     = HEARTBEAT_ERROR_BASE,

    // CAS-related error codes
    CAS_ERROR_BASE = -4000,

    ERROR_CAS_UNKNOWN                        = CAS_ERROR_BASE,
    ERROR_CAS_NO_LICENSE                     = CAS_ERROR_BASE - 1,
    ERROR_CAS_LICENSE_EXPIRED                = CAS_ERROR_BASE - 2,
    ERROR_CAS_SESSION_NOT_OPENED             = CAS_ERROR_BASE - 3,
    ERROR_CAS_DECRYPT_UNIT_NOT_INITIALIZED   = CAS_ERROR_BASE - 4,
    ERROR_CAS_DECRYPT                        = CAS_ERROR_BASE - 5,
    ERROR_CAS_CANNOT_HANDLE                  = CAS_ERROR_BASE - 6,
    ERROR_CAS_TAMPER_DETECTED                = CAS_ERROR_BASE - 7,
    ERROR_CAS_NOT_PROVISIONED                = CAS_ERROR_BASE - 8,
    ERROR_CAS_DEVICE_REVOKED                 = CAS_ERROR_BASE - 9,
    ERROR_CAS_RESOURCE_BUSY                  = CAS_ERROR_BASE - 10,
    ERROR_CAS_INSUFFICIENT_OUTPUT_PROTECTION = CAS_ERROR_BASE - 11,
    ERROR_CAS_NEED_ACTIVATION                = CAS_ERROR_BASE - 12,
    ERROR_CAS_NEED_PAIRING                   = CAS_ERROR_BASE - 13,
    ERROR_CAS_NO_CARD                        = CAS_ERROR_BASE - 14,
    ERROR_CAS_CARD_MUTE                      = CAS_ERROR_BASE - 15,
    ERROR_CAS_CARD_INVALID                   = CAS_ERROR_BASE - 16,
    ERROR_CAS_BLACKOUT                       = CAS_ERROR_BASE - 17,
    ERROR_CAS_REBOOTING                      = CAS_ERROR_BASE - 18,
    ERROR_CAS_LAST_USED_ERRORCODE            = CAS_ERROR_BASE - 18,

    ERROR_CAS_VENDOR_MAX                     = CAS_ERROR_BASE - 500,
    ERROR_CAS_VENDOR_MIN                     = CAS_ERROR_BASE - 999,

    // NDK Error codes
    // frameworks/av/include/ndk/NdkMediaError.h
    // from -10000 (0xFFFFD8F0 - 0xFFFFD8EC)
    // from -20000 (0xFFFFB1E0 - 0xFFFFB1D7)

    // Codec errors are permitted from 0x80001000 through 0x9000FFFF
    ERROR_CODEC_MAX    = (signed)0x9000FFFF,
    ERROR_CODEC_MIN    = (signed)0x80001000,

    // System unknown errors from 0x80000000 - 0x80000007 (INT32_MIN + 7)
    // See system/core/include/utils/Errors.h
};

// action codes for MediaCodecs that tell the upper layer and application
// the severity of any error.
enum ActionCode {
    ACTION_CODE_FATAL,
    ACTION_CODE_TRANSIENT,
    ACTION_CODE_RECOVERABLE,
};

// returns true if err is a recognized DRM error code
static inline bool isCryptoError(status_t err) {
    return (ERROR_DRM_LAST_USED_ERRORCODE <= err && err <= ERROR_DRM_UNKNOWN)
            || (ERROR_DRM_VENDOR_MIN <= err && err <= ERROR_DRM_VENDOR_MAX);
}

#define STATUS_CASE(STATUS) \
    case STATUS:            \
        return #STATUS

static inline std::string StrMediaError(status_t err) {
    switch(err) {
        STATUS_CASE(ERROR_ALREADY_CONNECTED);
        STATUS_CASE(ERROR_NOT_CONNECTED);
        STATUS_CASE(ERROR_UNKNOWN_HOST);
        STATUS_CASE(ERROR_CANNOT_CONNECT);
        STATUS_CASE(ERROR_IO);
        STATUS_CASE(ERROR_CONNECTION_LOST);
        STATUS_CASE(ERROR_MALFORMED);
        STATUS_CASE(ERROR_OUT_OF_RANGE);
        STATUS_CASE(ERROR_BUFFER_TOO_SMALL);
        STATUS_CASE(ERROR_UNSUPPORTED);
        STATUS_CASE(ERROR_END_OF_STREAM);
    }
    return statusToString(err);
}

static inline std::string StrCryptoError(status_t err) {
    switch (err) {
        STATUS_CASE(ERROR_DRM_UNKNOWN);
        STATUS_CASE(ERROR_DRM_NO_LICENSE);
        STATUS_CASE(ERROR_DRM_LICENSE_EXPIRED);
        STATUS_CASE(ERROR_DRM_SESSION_NOT_OPENED);
        STATUS_CASE(ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED);
        STATUS_CASE(ERROR_DRM_DECRYPT);
        STATUS_CASE(ERROR_DRM_CANNOT_HANDLE);
        STATUS_CASE(ERROR_DRM_TAMPER_DETECTED);
        STATUS_CASE(ERROR_DRM_NOT_PROVISIONED);
        STATUS_CASE(ERROR_DRM_DEVICE_REVOKED);
        STATUS_CASE(ERROR_DRM_RESOURCE_BUSY);
        STATUS_CASE(ERROR_DRM_INSUFFICIENT_OUTPUT_PROTECTION);
        STATUS_CASE(ERROR_DRM_INSUFFICIENT_SECURITY);
        STATUS_CASE(ERROR_DRM_FRAME_TOO_LARGE);
        STATUS_CASE(ERROR_DRM_RESOURCE_CONTENTION);
        STATUS_CASE(ERROR_DRM_SESSION_LOST_STATE);
        STATUS_CASE(ERROR_DRM_INVALID_STATE);
        STATUS_CASE(ERROR_DRM_CERTIFICATE_MALFORMED);
        STATUS_CASE(ERROR_DRM_CERTIFICATE_MISSING);
        STATUS_CASE(ERROR_DRM_CRYPTO_LIBRARY);
        STATUS_CASE(ERROR_DRM_GENERIC_OEM);
        STATUS_CASE(ERROR_DRM_GENERIC_PLUGIN);
        STATUS_CASE(ERROR_DRM_INIT_DATA);
        STATUS_CASE(ERROR_DRM_KEY_NOT_LOADED);
        STATUS_CASE(ERROR_DRM_LICENSE_PARSE);
        STATUS_CASE(ERROR_DRM_LICENSE_POLICY);
        STATUS_CASE(ERROR_DRM_LICENSE_RELEASE);
        STATUS_CASE(ERROR_DRM_LICENSE_REQUEST_REJECTED);
        STATUS_CASE(ERROR_DRM_LICENSE_RESTORE);
        STATUS_CASE(ERROR_DRM_LICENSE_STATE);
        STATUS_CASE(ERROR_DRM_MEDIA_FRAMEWORK);
        STATUS_CASE(ERROR_DRM_PROVISIONING_CERTIFICATE);
        STATUS_CASE(ERROR_DRM_PROVISIONING_CONFIG);
        STATUS_CASE(ERROR_DRM_PROVISIONING_PARSE);
        STATUS_CASE(ERROR_DRM_PROVISIONING_REQUEST_REJECTED);
        STATUS_CASE(ERROR_DRM_PROVISIONING_RETRY);
        STATUS_CASE(ERROR_DRM_SECURE_STOP_RELEASE);
        STATUS_CASE(ERROR_DRM_STORAGE_READ);
        STATUS_CASE(ERROR_DRM_STORAGE_WRITE);
        STATUS_CASE(ERROR_DRM_ZERO_SUBSAMPLES);
    }
    return statusToString(err);
}
#undef STATUS_CASE

}  // namespace android

#endif  // MEDIA_ERRORS_H_
