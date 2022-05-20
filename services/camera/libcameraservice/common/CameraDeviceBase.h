/*
 * Copyright (C) 2013-2018 The Android Open Source Project
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

#ifndef ANDROID_SERVERS_CAMERA_CAMERADEVICEBASE_H
#define ANDROID_SERVERS_CAMERA_CAMERADEVICEBASE_H

#include <list>

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <utils/Timers.h>
#include <utils/List.h>

#include "hardware/camera2.h"
#include "camera/CameraMetadata.h"
#include "camera/CaptureResult.h"
#include "gui/IGraphicBufferProducer.h"
#include "device3/Camera3StreamInterface.h"
#include "device3/StatusTracker.h"
#include "binder/Status.h"
#include "FrameProducer.h"
#include "utils/IPCTransport.h"

#include "CameraOfflineSessionBase.h"

namespace android {

namespace camera3 {

typedef enum camera_request_template {
    CAMERA_TEMPLATE_PREVIEW = 1,
    CAMERA_TEMPLATE_STILL_CAPTURE = 2,
    CAMERA_TEMPLATE_VIDEO_RECORD = 3,
    CAMERA_TEMPLATE_VIDEO_SNAPSHOT = 4,
    CAMERA_TEMPLATE_ZERO_SHUTTER_LAG = 5,
    CAMERA_TEMPLATE_MANUAL = 6,
    CAMERA_TEMPLATE_COUNT,
    CAMERA_VENDOR_TEMPLATE_START = 0x40000000
} camera_request_template_t;

typedef enum camera_stream_configuration_mode {
    CAMERA_STREAM_CONFIGURATION_NORMAL_MODE = 0,
    CAMERA_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE = 1,
    CAMERA_VENDOR_STREAM_CONFIGURATION_MODE_START = 0x8000
} camera_stream_configuration_mode_t;

// Matches definition of camera3_jpeg_blob in camera3.h and HIDL definition
// device@3.2:types.hal, needs to stay around till HIDL support is removed (for
// HIDL -> AIDL cameraBlob translation)
typedef struct camera_jpeg_blob {
    uint16_t jpeg_blob_id;
    uint32_t jpeg_size;
} camera_jpeg_blob_t;

enum {
    CAMERA_JPEG_BLOB_ID = 0x00FF,
    CAMERA_JPEG_APP_SEGMENTS_BLOB_ID = 0x0100,
};

} // namespace camera3

using camera3::camera_request_template_t;;
using camera3::camera_stream_configuration_mode_t;
using camera3::camera_stream_rotation_t;

class CameraProviderManager;

// Mapping of output stream index to surface ids
typedef std::unordered_map<int, std::vector<size_t> > SurfaceMap;

/**
 * Base interface for version >= 2 camera device classes, which interface to
 * camera HAL device versions >= 2.
 */
class CameraDeviceBase : public virtual FrameProducer {
  public:
    virtual ~CameraDeviceBase();

    virtual IPCTransport getTransportType() const = 0;

    /**
     * The device vendor tag ID
     */
    virtual metadata_vendor_id_t getVendorTagId() const = 0;

    virtual status_t initialize(sp<CameraProviderManager> manager, const String8& monitorTags) = 0;
    virtual status_t disconnect() = 0;

    virtual status_t dump(int fd, const Vector<String16> &args) = 0;
    virtual status_t startWatchingTags(const String8 &tags) = 0;
    virtual status_t stopWatchingTags() = 0;
    virtual status_t dumpWatchedEventsToVector(std::vector<std::string> &out) = 0;

    /**
     * The physical camera device's static characteristics metadata buffer, or
     * the logical camera's static characteristics if physical id is empty.
     */
    virtual const CameraMetadata& infoPhysical(const String8& physicalId) const = 0;

    struct PhysicalCameraSettings {
        std::string cameraId;
        CameraMetadata metadata;

        // Whether the physical camera supports testPatternMode/testPatternData
        bool mHasTestPatternModeTag = true;
        bool mHasTestPatternDataTag = true;

        // Original value of TEST_PATTERN_MODE and DATA so that they can be
        // restored when sensor muting is turned off
        int32_t mOriginalTestPatternMode = 0;
        int32_t mOriginalTestPatternData[4] = {};

    };
    typedef List<PhysicalCameraSettings> PhysicalCameraSettingsList;

    /**
     * Submit request for capture. The CameraDevice takes ownership of the
     * passed-in buffer.
     * Output lastFrameNumber is the expected frame number of this request.
     */
    virtual status_t capture(CameraMetadata &request, int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Submit a list of requests.
     * Output lastFrameNumber is the expected last frame number of the list of requests.
     */
    virtual status_t captureList(const List<const PhysicalCameraSettingsList> &requests,
                                 const std::list<const SurfaceMap> &surfaceMaps,
                                 int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Submit request for streaming. The CameraDevice makes a copy of the
     * passed-in buffer and the caller retains ownership.
     * Output lastFrameNumber is the last frame number of the previous streaming request.
     */
    virtual status_t setStreamingRequest(const CameraMetadata &request,
                                         int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Submit a list of requests for streaming.
     * Output lastFrameNumber is the last frame number of the previous streaming request.
     */
    virtual status_t setStreamingRequestList(const List<const PhysicalCameraSettingsList> &requests,
                                             const std::list<const SurfaceMap> &surfaceMaps,
                                             int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Clear the streaming request slot.
     * Output lastFrameNumber is the last frame number of the previous streaming request.
     */
    virtual status_t clearStreamingRequest(int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Wait until a request with the given ID has been dequeued by the
     * HAL. Returns TIMED_OUT if the timeout duration is reached. Returns
     * immediately if the latest request received by the HAL has this id.
     */
    virtual status_t waitUntilRequestReceived(int32_t requestId,
            nsecs_t timeout) = 0;

    /**
     * Create an output stream of the requested size, format, rotation and dataspace
     *
     * For HAL_PIXEL_FORMAT_BLOB formats, the width and height should be the
     * logical dimensions of the buffer, not the number of bytes.
     */
    virtual status_t createStream(sp<Surface> consumer,
            uint32_t width, uint32_t height, int format,
            android_dataspace dataSpace, camera_stream_rotation_t rotation, int *id,
            const String8& physicalCameraId,
            const std::unordered_set<int32_t>  &sensorPixelModesUsed,
            std::vector<int> *surfaceIds = nullptr,
            int streamSetId = camera3::CAMERA3_STREAM_SET_ID_INVALID,
            bool isShared = false, bool isMultiResolution = false,
            uint64_t consumerUsage = 0,
            int64_t dynamicProfile = ANDROID_REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP_STANDARD,
            int64_t streamUseCase = ANDROID_SCALER_AVAILABLE_STREAM_USE_CASES_DEFAULT,
            int timestampBase = OutputConfiguration::TIMESTAMP_BASE_DEFAULT,
            int mirrorMode = OutputConfiguration::MIRROR_MODE_AUTO) = 0;

    /**
     * Create an output stream of the requested size, format, rotation and
     * dataspace with a number of consumers.
     *
     * For HAL_PIXEL_FORMAT_BLOB formats, the width and height should be the
     * logical dimensions of the buffer, not the number of bytes.
     */
    virtual status_t createStream(const std::vector<sp<Surface>>& consumers,
            bool hasDeferredConsumer, uint32_t width, uint32_t height, int format,
            android_dataspace dataSpace, camera_stream_rotation_t rotation, int *id,
            const String8& physicalCameraId,
            const std::unordered_set<int32_t> &sensorPixelModesUsed,
            std::vector<int> *surfaceIds = nullptr,
            int streamSetId = camera3::CAMERA3_STREAM_SET_ID_INVALID,
            bool isShared = false, bool isMultiResolution = false,
            uint64_t consumerUsage = 0,
            int64_t dynamicProfile = ANDROID_REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP_STANDARD,
            int64_t streamUseCase = ANDROID_SCALER_AVAILABLE_STREAM_USE_CASES_DEFAULT,
            int timestampBase = OutputConfiguration::TIMESTAMP_BASE_DEFAULT,
            int mirrorMode = OutputConfiguration::MIRROR_MODE_AUTO) = 0;

    /**
     * Create an input stream of width, height, and format.
     *
     * Return value is the stream ID if non-negative and an error if negative.
     */
    virtual status_t createInputStream(uint32_t width, uint32_t height,
            int32_t format, bool multiResolution, /*out*/ int32_t *id) = 0;

    struct StreamInfo {
        uint32_t width;
        uint32_t height;

        uint32_t format;
        bool formatOverridden;
        uint32_t originalFormat;

        android_dataspace dataSpace;
        bool dataSpaceOverridden;
        android_dataspace originalDataSpace;
        int64_t dynamicRangeProfile;

        StreamInfo() : width(0), height(0), format(0), formatOverridden(false), originalFormat(0),
                dataSpace(HAL_DATASPACE_UNKNOWN), dataSpaceOverridden(false),
                originalDataSpace(HAL_DATASPACE_UNKNOWN),
                dynamicRangeProfile(ANDROID_REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP_STANDARD){}
        /**
         * Check whether the format matches the current or the original one in case
         * it got overridden.
         */
        bool matchFormat(uint32_t clientFormat) const {
            if ((formatOverridden && (originalFormat == clientFormat)) ||
                    (format == clientFormat)) {
                return true;
            }
            return false;
        }

        /**
         * Check whether the dataspace matches the current or the original one in case
         * it got overridden.
         */
        bool matchDataSpace(android_dataspace clientDataSpace) const {
            if ((dataSpaceOverridden && (originalDataSpace == clientDataSpace)) ||
                    (dataSpace == clientDataSpace)) {
                return true;
            }
            return false;
        }

    };

    /**
     * Get information about a given stream.
     */
    virtual status_t getStreamInfo(int id, StreamInfo *streamInfo) = 0;

    /**
     * Set stream gralloc buffer transform
     */
    virtual status_t setStreamTransform(int id, int transform) = 0;

    /**
     * Delete stream. Must not be called if there are requests in flight which
     * reference that stream.
     */
    virtual status_t deleteStream(int id) = 0;

    /**
     * Take the currently-defined set of streams and configure the HAL to use
     * them. This is a long-running operation (may be several hundered ms).
     *
     * The device must be idle (see waitUntilDrained) before calling this.
     *
     * Returns OK on success; otherwise on error:
     * - BAD_VALUE if the set of streams was invalid (e.g. fmts or sizes)
     * - INVALID_OPERATION if the device was in the wrong state
     */
    virtual status_t configureStreams(const CameraMetadata& sessionParams,
            int operatingMode =
            camera_stream_configuration_mode_t::CAMERA_STREAM_CONFIGURATION_NORMAL_MODE) = 0;

    /**
     * Retrieve a list of all stream ids that were advertised as capable of
     * supporting offline processing mode by Hal after the last stream configuration.
     */
    virtual void getOfflineStreamIds(std::vector<int> *offlineStreamIds) = 0;

    // get the buffer producer of the input stream
    virtual status_t getInputBufferProducer(
            sp<IGraphicBufferProducer> *producer) = 0;

    /**
     * Create a metadata buffer with fields that the HAL device believes are
     * best for the given use case
     */
    virtual status_t createDefaultRequest(camera_request_template_t templateId,
            CameraMetadata *request) = 0;

    /**
     * Wait until all requests have been processed. Returns INVALID_OPERATION if
     * the streaming slot is not empty, or TIMED_OUT if the requests haven't
     * finished processing in 10 seconds.
     */
    virtual status_t waitUntilDrained() = 0;

    /**
     * Get Jpeg buffer size for a given jpeg resolution.
     * Negative values are error codes.
     */
    virtual ssize_t getJpegBufferSize(const CameraMetadata &info, uint32_t width,
            uint32_t height) const = 0;

    /**
     * Connect HAL notifications to a listener. Overwrites previous
     * listener. Set to NULL to stop receiving notifications.
     */
    virtual status_t setNotifyCallback(wp<NotificationListener> listener) = 0;

    /**
     * Whether the device supports calling notifyAutofocus, notifyAutoExposure,
     * and notifyAutoWhitebalance; if this returns false, the client must
     * synthesize these notifications from received frame metadata.
     */
    virtual bool     willNotify3A() = 0;

    /**
     * Trigger auto-focus. The latest ID used in a trigger autofocus or cancel
     * autofocus call will be returned by the HAL in all subsequent AF
     * notifications.
     */
    virtual status_t triggerAutofocus(uint32_t id) = 0;

    /**
     * Cancel auto-focus. The latest ID used in a trigger autofocus/cancel
     * autofocus call will be returned by the HAL in all subsequent AF
     * notifications.
     */
    virtual status_t triggerCancelAutofocus(uint32_t id) = 0;

    /**
     * Trigger pre-capture metering. The latest ID used in a trigger pre-capture
     * call will be returned by the HAL in all subsequent AE and AWB
     * notifications.
     */
    virtual status_t triggerPrecaptureMetering(uint32_t id) = 0;

    /**
     * Flush all pending and in-flight requests. Blocks until flush is
     * complete.
     * Output lastFrameNumber is the last frame number of the previous streaming request.
     */
    virtual status_t flush(int64_t *lastFrameNumber = NULL) = 0;

    /**
     * Prepare stream by preallocating buffers for it asynchronously.
     * Calls notifyPrepared() once allocation is complete.
     */
    virtual status_t prepare(int streamId) = 0;

    /**
     * Free stream resources by dumping its unused gralloc buffers.
     */
    virtual status_t tearDown(int streamId) = 0;

    /**
     * Add buffer listener for a particular stream in the device.
     */
    virtual status_t addBufferListenerForStream(int streamId,
            wp<camera3::Camera3StreamBufferListener> listener) = 0;

    /**
     * Prepare stream by preallocating up to maxCount buffers for it asynchronously.
     * Calls notifyPrepared() once allocation is complete.
     */
    virtual status_t prepare(int maxCount, int streamId) = 0;

    /**
     * Set the deferred consumer surface and finish the rest of the stream configuration.
     */
    virtual status_t setConsumerSurfaces(int streamId,
            const std::vector<sp<Surface>>& consumers, std::vector<int> *surfaceIds /*out*/) = 0;

    /**
     * Update a given stream.
     */
    virtual status_t updateStream(int streamId, const std::vector<sp<Surface>> &newSurfaces,
            const std::vector<android::camera3::OutputStreamInfo> &outputInfo,
            const std::vector<size_t> &removedSurfaceIds,
            KeyedVector<sp<Surface>, size_t> *outputMap/*out*/) = 0;

    /**
     * Drop buffers for stream of streamId if dropping is true. If dropping is false, do not
     * drop buffers for stream of streamId.
     */
    virtual status_t dropStreamBuffers(bool /*dropping*/, int /*streamId*/) = 0;

    /**
     * Returns the maximum expected time it'll take for all currently in-flight
     * requests to complete, based on their settings
     */
    virtual nsecs_t getExpectedInFlightDuration() = 0;

    /**
     * switch to offline session
     */
    virtual status_t switchToOffline(
            const std::vector<int32_t>& streamsToKeep,
            /*out*/ sp<CameraOfflineSessionBase>* session) = 0;

    /**
     * Set the current behavior for the ROTATE_AND_CROP control when in AUTO.
     *
     * The value must be one of the ROTATE_AND_CROP_* values besides AUTO,
     * and defaults to NONE.
     */
    virtual status_t setRotateAndCropAutoBehavior(
            camera_metadata_enum_android_scaler_rotate_and_crop_t rotateAndCropValue) = 0;

    /**
     * Whether camera muting (producing black-only output) is supported.
     *
     * Calling setCameraMute(true) when this returns false will return an
     * INVALID_OPERATION error.
     */
    virtual bool supportsCameraMute() = 0;

    /**
     * Mute the camera.
     *
     * When muted, black image data is output on all output streams.
     */
    virtual status_t setCameraMute(bool enabled) = 0;

    /**
     * Get the status tracker of the camera device
     */
    virtual wp<camera3::StatusTracker> getStatusTracker() = 0;

    /**
     * Set bitmask for image dump flag
     */
    void setImageDumpMask(int mask) { mImageDumpMask = mask; }

    /**
     * The injection camera session to replace the internal camera
     * session.
     */
    virtual status_t injectCamera(const String8& injectedCamId,
            sp<CameraProviderManager> manager) = 0;

    /**
     * Stop the injection camera and restore to internal camera session.
     */
    virtual status_t stopInjection() = 0;

protected:
    bool mImageDumpMask = 0;
};

}; // namespace android

#endif
