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

#include <inttypes.h>

//#define LOG_NDEBUG 0
#define LOG_TAG "CameraSource"
#include <utils/Log.h>

#include <OMX_Component.h>
#include <binder/IPCThreadState.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <gui/Surface.h>
#include <utils/String8.h>
#include <cutils/properties.h>

#if LOG_NDEBUG
#define UNUSED_UNLESS_VERBOSE(x) (void)(x)
#else
#define UNUSED_UNLESS_VERBOSE(x)
#endif

namespace android {

static const int64_t CAMERA_SOURCE_TIMEOUT_NS = 3000000000LL;

struct CameraSourceListener : public CameraListener {
    CameraSourceListener(const sp<CameraSource> &source);

    virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2);
    virtual void postData(int32_t msgType, const sp<IMemory> &dataPtr,
                          camera_frame_metadata_t *metadata);

    virtual void postDataTimestamp(
            nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);

protected:
    virtual ~CameraSourceListener();

private:
    wp<CameraSource> mSource;

    CameraSourceListener(const CameraSourceListener &);
    CameraSourceListener &operator=(const CameraSourceListener &);
};

CameraSourceListener::CameraSourceListener(const sp<CameraSource> &source)
    : mSource(source) {
}

CameraSourceListener::~CameraSourceListener() {
}

void CameraSourceListener::notify(int32_t msgType, int32_t ext1, int32_t ext2) {
    UNUSED_UNLESS_VERBOSE(msgType);
    UNUSED_UNLESS_VERBOSE(ext1);
    UNUSED_UNLESS_VERBOSE(ext2);
    ALOGV("notify(%d, %d, %d)", msgType, ext1, ext2);
}

void CameraSourceListener::postData(int32_t msgType, const sp<IMemory> &dataPtr,
                                    camera_frame_metadata_t * /* metadata */) {
    ALOGV("postData(%d, ptr:%p, size:%zu)",
         msgType, dataPtr->pointer(), dataPtr->size());

    sp<CameraSource> source = mSource.promote();
    if (source.get() != NULL) {
        source->dataCallback(msgType, dataPtr);
    }
}

void CameraSourceListener::postDataTimestamp(
        nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) {

    sp<CameraSource> source = mSource.promote();
    if (source.get() != NULL) {
        source->dataCallbackTimestamp(timestamp/1000, msgType, dataPtr);
    }
}

static int32_t getColorFormat(const char* colorFormat) {
    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420P)) {
       return OMX_COLOR_FormatYUV420Planar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422SP)) {
       return OMX_COLOR_FormatYUV422SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420SP)) {
        return OMX_COLOR_FormatYUV420SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422I)) {
        return OMX_COLOR_FormatYCbYCr;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_RGB565)) {
       return OMX_COLOR_Format16bitRGB565;
    }

    if (!strcmp(colorFormat, "OMX_TI_COLOR_FormatYUV420PackedSemiPlanar")) {
       return OMX_TI_COLOR_FormatYUV420PackedSemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_ANDROID_OPAQUE)) {
        return OMX_COLOR_FormatAndroidOpaque;
    }

    ALOGE("Uknown color format (%s), please add it to "
         "CameraSource::getColorFormat", colorFormat);

    CHECK(!"Unknown color format");
    return -1;
}

CameraSource *CameraSource::Create(const String16 &clientName) {
    Size size;
    size.width = -1;
    size.height = -1;

    sp<ICamera> camera;
    return new CameraSource(camera, NULL, 0, clientName, -1,
            size, -1, NULL, false);
}

// static
CameraSource *CameraSource::CreateFromCamera(
    const sp<ICamera>& camera,
    const sp<ICameraRecordingProxy>& proxy,
    int32_t cameraId,
    const String16& clientName,
    uid_t clientUid,
    Size videoSize,
    int32_t frameRate,
    const sp<IGraphicBufferProducer>& surface,
    bool storeMetaDataInVideoBuffers) {

    CameraSource *source = new CameraSource(camera, proxy, cameraId,
            clientName, clientUid, videoSize, frameRate, surface,
            storeMetaDataInVideoBuffers);
    return source;
}

CameraSource::CameraSource(
    const sp<ICamera>& camera,
    const sp<ICameraRecordingProxy>& proxy,
    int32_t cameraId,
    const String16& clientName,
    uid_t clientUid,
    Size videoSize,
    int32_t frameRate,
    const sp<IGraphicBufferProducer>& surface,
    bool storeMetaDataInVideoBuffers)
    : mCameraFlags(0),
      mNumInputBuffers(0),
      mVideoFrameRate(-1),
      mCamera(0),
      mSurface(surface),
      mNumFramesReceived(0),
      mLastFrameTimestampUs(0),
      mStarted(false),
      mNumFramesEncoded(0),
      mTimeBetweenFrameCaptureUs(0),
      mFirstFrameTimeUs(0),
      mNumFramesDropped(0),
      mNumGlitches(0),
      mGlitchDurationThresholdUs(200000),
      mCollectStats(false) {
    mVideoSize.width  = -1;
    mVideoSize.height = -1;

    mInitCheck = init(camera, proxy, cameraId,
                    clientName, clientUid,
                    videoSize, frameRate,
                    storeMetaDataInVideoBuffers);
    if (mInitCheck != OK) releaseCamera();
}

status_t CameraSource::initCheck() const {
    return mInitCheck;
}

status_t CameraSource::isCameraAvailable(
    const sp<ICamera>& camera, const sp<ICameraRecordingProxy>& proxy,
    int32_t cameraId, const String16& clientName, uid_t clientUid) {

    if (camera == 0) {
        mCamera = Camera::connect(cameraId, clientName, clientUid);
        if (mCamera == 0) return -EBUSY;
        mCameraFlags &= ~FLAGS_HOT_CAMERA;
    } else {
        // We get the proxy from Camera, not ICamera. We need to get the proxy
        // to the remote Camera owned by the application. Here mCamera is a
        // local Camera object created by us. We cannot use the proxy from
        // mCamera here.
        mCamera = Camera::create(camera);
        if (mCamera == 0) return -EBUSY;
        mCameraRecordingProxy = proxy;
        mCameraFlags |= FLAGS_HOT_CAMERA;
        mDeathNotifier = new DeathNotifier();
        // isBinderAlive needs linkToDeath to work.
        IInterface::asBinder(mCameraRecordingProxy)->linkToDeath(mDeathNotifier);
    }

    mCamera->lock();

    return OK;
}


/*
 * Check to see whether the requested video width and height is one
 * of the supported sizes.
 * @param width the video frame width in pixels
 * @param height the video frame height in pixels
 * @param suppportedSizes the vector of sizes that we check against
 * @return true if the dimension (width and height) is supported.
 */
static bool isVideoSizeSupported(
    int32_t width, int32_t height,
    const Vector<Size>& supportedSizes) {

    ALOGV("isVideoSizeSupported");
    for (size_t i = 0; i < supportedSizes.size(); ++i) {
        if (width  == supportedSizes[i].width &&
            height == supportedSizes[i].height) {
            return true;
        }
    }
    return false;
}

/*
 * If the preview and video output is separate, we only set the
 * the video size, and applications should set the preview size
 * to some proper value, and the recording framework will not
 * change the preview size; otherwise, if the video and preview
 * output is the same, we need to set the preview to be the same
 * as the requested video size.
 *
 */
/*
 * Query the camera to retrieve the supported video frame sizes
 * and also to see whether CameraParameters::setVideoSize()
 * is supported or not.
 * @param params CameraParameters to retrieve the information
 * @@param isSetVideoSizeSupported retunrs whether method
 *      CameraParameters::setVideoSize() is supported or not.
 * @param sizes returns the vector of Size objects for the
 *      supported video frame sizes advertised by the camera.
 */
static void getSupportedVideoSizes(
    const CameraParameters& params,
    bool *isSetVideoSizeSupported,
    Vector<Size>& sizes) {

    *isSetVideoSizeSupported = true;
    params.getSupportedVideoSizes(sizes);
    if (sizes.size() == 0) {
        ALOGD("Camera does not support setVideoSize()");
        params.getSupportedPreviewSizes(sizes);
        *isSetVideoSizeSupported = false;
    }
}

/*
 * Check whether the camera has the supported color format
 * @param params CameraParameters to retrieve the information
 * @return OK if no error.
 */
status_t CameraSource::isCameraColorFormatSupported(
        const CameraParameters& params) {
    mColorFormat = getColorFormat(params.get(
            CameraParameters::KEY_VIDEO_FRAME_FORMAT));
    if (mColorFormat == -1) {
        return BAD_VALUE;
    }
    return OK;
}

/*
 * Configure the camera to use the requested video size
 * (width and height) and/or frame rate. If both width and
 * height are -1, configuration on the video size is skipped.
 * if frameRate is -1, configuration on the frame rate
 * is skipped. Skipping the configuration allows one to
 * use the current camera setting without the need to
 * actually know the specific values (see Create() method).
 *
 * @param params the CameraParameters to be configured
 * @param width the target video frame width in pixels
 * @param height the target video frame height in pixels
 * @param frameRate the target frame rate in frames per second.
 * @return OK if no error.
 */
status_t CameraSource::configureCamera(
        CameraParameters* params,
        int32_t width, int32_t height,
        int32_t frameRate) {
    ALOGV("configureCamera");
    Vector<Size> sizes;
    bool isSetVideoSizeSupportedByCamera = true;
    getSupportedVideoSizes(*params, &isSetVideoSizeSupportedByCamera, sizes);
    bool isCameraParamChanged = false;
    if (width != -1 && height != -1) {
        if (!isVideoSizeSupported(width, height, sizes)) {
            ALOGE("Video dimension (%dx%d) is unsupported", width, height);
            return BAD_VALUE;
        }
        if (isSetVideoSizeSupportedByCamera) {
            params->setVideoSize(width, height);
        } else {
            params->setPreviewSize(width, height);
        }
        isCameraParamChanged = true;
    } else if ((width == -1 && height != -1) ||
               (width != -1 && height == -1)) {
        // If one and only one of the width and height is -1
        // we reject such a request.
        ALOGE("Requested video size (%dx%d) is not supported", width, height);
        return BAD_VALUE;
    } else {  // width == -1 && height == -1
        // Do not configure the camera.
        // Use the current width and height value setting from the camera.
    }

    if (frameRate != -1) {
        CHECK(frameRate > 0 && frameRate <= 120);
        const char* supportedFrameRates =
                params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES);
        CHECK(supportedFrameRates != NULL);
        ALOGV("Supported frame rates: %s", supportedFrameRates);
        char buf[4];
        snprintf(buf, 4, "%d", frameRate);
        if (strstr(supportedFrameRates, buf) == NULL) {
            ALOGE("Requested frame rate (%d) is not supported: %s",
                frameRate, supportedFrameRates);
            return BAD_VALUE;
        }

        // The frame rate is supported, set the camera to the requested value.
        params->setPreviewFrameRate(frameRate);
        isCameraParamChanged = true;
    } else {  // frameRate == -1
        // Do not configure the camera.
        // Use the current frame rate value setting from the camera
    }

    if (isCameraParamChanged) {
        // Either frame rate or frame size needs to be changed.
        String8 s = params->flatten();
        if (OK != mCamera->setParameters(s)) {
            ALOGE("Could not change settings."
                 " Someone else is using camera %p?", mCamera.get());
            return -EBUSY;
        }
    }
    return OK;
}

/*
 * Check whether the requested video frame size
 * has been successfully configured or not. If both width and height
 * are -1, check on the current width and height value setting
 * is performed.
 *
 * @param params CameraParameters to retrieve the information
 * @param the target video frame width in pixels to check against
 * @param the target video frame height in pixels to check against
 * @return OK if no error
 */
status_t CameraSource::checkVideoSize(
        const CameraParameters& params,
        int32_t width, int32_t height) {

    ALOGV("checkVideoSize");
    // The actual video size is the same as the preview size
    // if the camera hal does not support separate video and
    // preview output. In this case, we retrieve the video
    // size from preview.
    int32_t frameWidthActual = -1;
    int32_t frameHeightActual = -1;
    Vector<Size> sizes;
    params.getSupportedVideoSizes(sizes);
    if (sizes.size() == 0) {
        // video size is the same as preview size
        params.getPreviewSize(&frameWidthActual, &frameHeightActual);
    } else {
        // video size may not be the same as preview
        params.getVideoSize(&frameWidthActual, &frameHeightActual);
    }
    if (frameWidthActual < 0 || frameHeightActual < 0) {
        ALOGE("Failed to retrieve video frame size (%dx%d)",
                frameWidthActual, frameHeightActual);
        return UNKNOWN_ERROR;
    }

    // Check the actual video frame size against the target/requested
    // video frame size.
    if (width != -1 && height != -1) {
        if (frameWidthActual != width || frameHeightActual != height) {
            ALOGE("Failed to set video frame size to %dx%d. "
                    "The actual video size is %dx%d ", width, height,
                    frameWidthActual, frameHeightActual);
            return UNKNOWN_ERROR;
        }
    }

    // Good now.
    mVideoSize.width = frameWidthActual;
    mVideoSize.height = frameHeightActual;
    return OK;
}

/*
 * Check the requested frame rate has been successfully configured or not.
 * If the target frameRate is -1, check on the current frame rate value
 * setting is performed.
 *
 * @param params CameraParameters to retrieve the information
 * @param the target video frame rate to check against
 * @return OK if no error.
 */
status_t CameraSource::checkFrameRate(
        const CameraParameters& params,
        int32_t frameRate) {

    ALOGV("checkFrameRate");
    int32_t frameRateActual = params.getPreviewFrameRate();
    if (frameRateActual < 0) {
        ALOGE("Failed to retrieve preview frame rate (%d)", frameRateActual);
        return UNKNOWN_ERROR;
    }

    // Check the actual video frame rate against the target/requested
    // video frame rate.
    if (frameRate != -1 && (frameRateActual - frameRate) != 0) {
        ALOGE("Failed to set preview frame rate to %d fps. The actual "
                "frame rate is %d", frameRate, frameRateActual);
        return UNKNOWN_ERROR;
    }

    // Good now.
    mVideoFrameRate = frameRateActual;
    return OK;
}

/*
 * Initialize the CameraSource to so that it becomes
 * ready for providing the video input streams as requested.
 * @param camera the camera object used for the video source
 * @param cameraId if camera == 0, use camera with this id
 *      as the video source
 * @param videoSize the target video frame size. If both
 *      width and height in videoSize is -1, use the current
 *      width and heigth settings by the camera
 * @param frameRate the target frame rate in frames per second.
 *      if it is -1, use the current camera frame rate setting.
 * @param storeMetaDataInVideoBuffers request to store meta
 *      data or real YUV data in video buffers. Request to
 *      store meta data in video buffers may not be honored
 *      if the source does not support this feature.
 *
 * @return OK if no error.
 */
status_t CameraSource::init(
        const sp<ICamera>& camera,
        const sp<ICameraRecordingProxy>& proxy,
        int32_t cameraId,
        const String16& clientName,
        uid_t clientUid,
        Size videoSize,
        int32_t frameRate,
        bool storeMetaDataInVideoBuffers) {

    ALOGV("init");
    status_t err = OK;
    int64_t token = IPCThreadState::self()->clearCallingIdentity();
    err = initWithCameraAccess(camera, proxy, cameraId, clientName, clientUid,
                               videoSize, frameRate,
                               storeMetaDataInVideoBuffers);
    IPCThreadState::self()->restoreCallingIdentity(token);
    return err;
}

status_t CameraSource::initWithCameraAccess(
        const sp<ICamera>& camera,
        const sp<ICameraRecordingProxy>& proxy,
        int32_t cameraId,
        const String16& clientName,
        uid_t clientUid,
        Size videoSize,
        int32_t frameRate,
        bool storeMetaDataInVideoBuffers) {
    ALOGV("initWithCameraAccess");
    status_t err = OK;

    if ((err = isCameraAvailable(camera, proxy, cameraId,
            clientName, clientUid)) != OK) {
        ALOGE("Camera connection could not be established.");
        return err;
    }
    CameraParameters params(mCamera->getParameters());
    if ((err = isCameraColorFormatSupported(params)) != OK) {
        return err;
    }

    // Set the camera to use the requested video frame size
    // and/or frame rate.
    if ((err = configureCamera(&params,
                    videoSize.width, videoSize.height,
                    frameRate))) {
        return err;
    }

    // Check on video frame size and frame rate.
    CameraParameters newCameraParams(mCamera->getParameters());
    if ((err = checkVideoSize(newCameraParams,
                videoSize.width, videoSize.height)) != OK) {
        return err;
    }
    if ((err = checkFrameRate(newCameraParams, frameRate)) != OK) {
        return err;
    }

    // Set the preview display. Skip this if mSurface is null because
    // applications may already set a surface to the camera.
    if (mSurface != NULL) {
        // This CHECK is good, since we just passed the lock/unlock
        // check earlier by calling mCamera->setParameters().
        CHECK_EQ((status_t)OK, mCamera->setPreviewTarget(mSurface));
    }

    // By default, do not store metadata in video buffers
    mIsMetaDataStoredInVideoBuffers = false;
    mCamera->storeMetaDataInBuffers(false);
    if (storeMetaDataInVideoBuffers) {
        if (OK == mCamera->storeMetaDataInBuffers(true)) {
            mIsMetaDataStoredInVideoBuffers = true;
        }
    }

    int64_t glitchDurationUs = (1000000LL / mVideoFrameRate);
    if (glitchDurationUs > mGlitchDurationThresholdUs) {
        mGlitchDurationThresholdUs = glitchDurationUs;
    }

    // XXX: query camera for the stride and slice height
    // when the capability becomes available.
    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType,  MEDIA_MIMETYPE_VIDEO_RAW);
    mMeta->setInt32(kKeyColorFormat, mColorFormat);
    mMeta->setInt32(kKeyWidth,       mVideoSize.width);
    mMeta->setInt32(kKeyHeight,      mVideoSize.height);
    mMeta->setInt32(kKeyStride,      mVideoSize.width);
    mMeta->setInt32(kKeySliceHeight, mVideoSize.height);
    mMeta->setInt32(kKeyFrameRate,   mVideoFrameRate);
    return OK;
}

CameraSource::~CameraSource() {
    if (mStarted) {
        reset();
    } else if (mInitCheck == OK) {
        // Camera is initialized but because start() is never called,
        // the lock on Camera is never released(). This makes sure
        // Camera's lock is released in this case.
        releaseCamera();
    }
}

status_t CameraSource::startCameraRecording() {
    ALOGV("startCameraRecording");
    // Reset the identity to the current thread because media server owns the
    // camera and recording is started by the applications. The applications
    // will connect to the camera in ICameraRecordingProxy::startRecording.
    int64_t token = IPCThreadState::self()->clearCallingIdentity();
    status_t err;
    if (mNumInputBuffers > 0) {
        err = mCamera->sendCommand(
            CAMERA_CMD_SET_VIDEO_BUFFER_COUNT, mNumInputBuffers, 0);

        // This could happen for CameraHAL1 clients; thus the failure is
        // not a fatal error
        if (err != OK) {
            ALOGW("Failed to set video buffer count to %d due to %d",
                mNumInputBuffers, err);
        }
    }

    err = mCamera->sendCommand(
        CAMERA_CMD_SET_VIDEO_FORMAT, mEncoderFormat, mEncoderDataSpace);

    // This could happen for CameraHAL1 clients; thus the failure is
    // not a fatal error
    if (err != OK) {
        ALOGW("Failed to set video encoder format/dataspace to %d, %d due to %d",
                mEncoderFormat, mEncoderDataSpace, err);
    }

    err = OK;
    if (mCameraFlags & FLAGS_HOT_CAMERA) {
        mCamera->unlock();
        mCamera.clear();
        if ((err = mCameraRecordingProxy->startRecording(
                new ProxyListener(this))) != OK) {
            ALOGE("Failed to start recording, received error: %s (%d)",
                    strerror(-err), err);
        }
    } else {
        mCamera->setListener(new CameraSourceListener(this));
        mCamera->startRecording();
        if (!mCamera->recordingEnabled()) {
            err = -EINVAL;
            ALOGE("Failed to start recording");
        }
    }
    IPCThreadState::self()->restoreCallingIdentity(token);
    return err;
}

status_t CameraSource::start(MetaData *meta) {
    ALOGV("start");
    CHECK(!mStarted);
    if (mInitCheck != OK) {
        ALOGE("CameraSource is not initialized yet");
        return mInitCheck;
    }

    char value[PROPERTY_VALUE_MAX];
    if (property_get("media.stagefright.record-stats", value, NULL)
        && (!strcmp(value, "1") || !strcasecmp(value, "true"))) {
        mCollectStats = true;
    }

    mStartTimeUs = 0;
    mNumInputBuffers = 0;
    mEncoderFormat = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    mEncoderDataSpace = HAL_DATASPACE_BT709;

    if (meta) {
        int64_t startTimeUs;
        if (meta->findInt64(kKeyTime, &startTimeUs)) {
            mStartTimeUs = startTimeUs;
        }

        int32_t nBuffers;
        if (meta->findInt32(kKeyNumBuffers, &nBuffers)) {
            CHECK_GT(nBuffers, 0);
            mNumInputBuffers = nBuffers;
        }

        // apply encoder color format if specified
        if (meta->findInt32(kKeyPixelFormat, &mEncoderFormat)) {
            ALOGV("Using encoder format: %#x", mEncoderFormat);
        }
        if (meta->findInt32(kKeyColorSpace, &mEncoderDataSpace)) {
            ALOGV("Using encoder data space: %#x", mEncoderDataSpace);
        }
    }

    status_t err;
    if ((err = startCameraRecording()) == OK) {
        mStarted = true;
    }

    return err;
}

void CameraSource::stopCameraRecording() {
    ALOGV("stopCameraRecording");
    if (mCameraFlags & FLAGS_HOT_CAMERA) {
        mCameraRecordingProxy->stopRecording();
    } else {
        mCamera->setListener(NULL);
        mCamera->stopRecording();
    }
}

void CameraSource::releaseCamera() {
    ALOGV("releaseCamera");
    sp<Camera> camera;
    bool coldCamera = false;
    {
        Mutex::Autolock autoLock(mLock);
        // get a local ref and clear ref to mCamera now
        camera = mCamera;
        mCamera.clear();
        coldCamera = (mCameraFlags & FLAGS_HOT_CAMERA) == 0;
    }

    if (camera != 0) {
        int64_t token = IPCThreadState::self()->clearCallingIdentity();
        if (coldCamera) {
            ALOGV("Camera was cold when we started, stopping preview");
            camera->stopPreview();
            camera->disconnect();
        }
        camera->unlock();
        IPCThreadState::self()->restoreCallingIdentity(token);
    }

    {
        Mutex::Autolock autoLock(mLock);
        if (mCameraRecordingProxy != 0) {
            IInterface::asBinder(mCameraRecordingProxy)->unlinkToDeath(mDeathNotifier);
            mCameraRecordingProxy.clear();
        }
        mCameraFlags = 0;
    }
}

status_t CameraSource::reset() {
    ALOGD("reset: E");

    {
        Mutex::Autolock autoLock(mLock);
        mStarted = false;
        mFrameAvailableCondition.signal();

        int64_t token;
        bool isTokenValid = false;
        if (mCamera != 0) {
            token = IPCThreadState::self()->clearCallingIdentity();
            isTokenValid = true;
        }
        releaseQueuedFrames();
        while (!mFramesBeingEncoded.empty()) {
            if (NO_ERROR !=
                mFrameCompleteCondition.waitRelative(mLock,
                        mTimeBetweenFrameCaptureUs * 1000LL + CAMERA_SOURCE_TIMEOUT_NS)) {
                ALOGW("Timed out waiting for outstanding frames being encoded: %zu",
                    mFramesBeingEncoded.size());
            }
        }
        stopCameraRecording();
        if (isTokenValid) {
            IPCThreadState::self()->restoreCallingIdentity(token);
        }

        if (mCollectStats) {
            ALOGI("Frames received/encoded/dropped: %d/%d/%d in %" PRId64 " us",
                    mNumFramesReceived, mNumFramesEncoded, mNumFramesDropped,
                    mLastFrameTimestampUs - mFirstFrameTimeUs);
        }

        if (mNumGlitches > 0) {
            ALOGW("%d long delays between neighboring video frames", mNumGlitches);
        }

        CHECK_EQ(mNumFramesReceived, mNumFramesEncoded + mNumFramesDropped);
    }

    releaseCamera();

    ALOGD("reset: X");
    return OK;
}

void CameraSource::releaseRecordingFrame(const sp<IMemory>& frame) {
    ALOGV("releaseRecordingFrame");
    if (mCameraRecordingProxy != NULL) {
        mCameraRecordingProxy->releaseRecordingFrame(frame);
    } else if (mCamera != NULL) {
        int64_t token = IPCThreadState::self()->clearCallingIdentity();
        mCamera->releaseRecordingFrame(frame);
        IPCThreadState::self()->restoreCallingIdentity(token);
    }
}

void CameraSource::releaseQueuedFrames() {
    List<sp<IMemory> >::iterator it;
    while (!mFramesReceived.empty()) {
        it = mFramesReceived.begin();
        releaseRecordingFrame(*it);
        mFramesReceived.erase(it);
        ++mNumFramesDropped;
    }
}

sp<MetaData> CameraSource::getFormat() {
    return mMeta;
}

void CameraSource::releaseOneRecordingFrame(const sp<IMemory>& frame) {
    releaseRecordingFrame(frame);
}

void CameraSource::signalBufferReturned(MediaBuffer *buffer) {
    ALOGV("signalBufferReturned: %p", buffer->data());
    Mutex::Autolock autoLock(mLock);
    for (List<sp<IMemory> >::iterator it = mFramesBeingEncoded.begin();
         it != mFramesBeingEncoded.end(); ++it) {
        if ((*it)->pointer() ==  buffer->data()) {
            releaseOneRecordingFrame((*it));
            mFramesBeingEncoded.erase(it);
            ++mNumFramesEncoded;
            buffer->setObserver(0);
            buffer->release();
            mFrameCompleteCondition.signal();
            return;
        }
    }
    CHECK(!"signalBufferReturned: bogus buffer");
}

status_t CameraSource::read(
        MediaBuffer **buffer, const ReadOptions *options) {
    ALOGV("read");

    *buffer = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        return ERROR_UNSUPPORTED;
    }

    sp<IMemory> frame;
    int64_t frameTime;

    {
        Mutex::Autolock autoLock(mLock);
        while (mStarted && mFramesReceived.empty()) {
            if (NO_ERROR !=
                mFrameAvailableCondition.waitRelative(mLock,
                    mTimeBetweenFrameCaptureUs * 1000LL + CAMERA_SOURCE_TIMEOUT_NS)) {
                if (mCameraRecordingProxy != 0 &&
                    !IInterface::asBinder(mCameraRecordingProxy)->isBinderAlive()) {
                    ALOGW("camera recording proxy is gone");
                    return ERROR_END_OF_STREAM;
                }
                ALOGW("Timed out waiting for incoming camera video frames: %" PRId64 " us",
                    mLastFrameTimestampUs);
            }
        }
        if (!mStarted) {
            return OK;
        }
        frame = *mFramesReceived.begin();
        mFramesReceived.erase(mFramesReceived.begin());

        frameTime = *mFrameTimes.begin();
        mFrameTimes.erase(mFrameTimes.begin());
        mFramesBeingEncoded.push_back(frame);
        *buffer = new MediaBuffer(frame->pointer(), frame->size());
        (*buffer)->setObserver(this);
        (*buffer)->add_ref();
        (*buffer)->meta_data()->setInt64(kKeyTime, frameTime);
    }
    return OK;
}

void CameraSource::dataCallbackTimestamp(int64_t timestampUs,
        int32_t msgType __unused, const sp<IMemory> &data) {
    ALOGV("dataCallbackTimestamp: timestamp %" PRId64 " us", timestampUs);
    Mutex::Autolock autoLock(mLock);
    if (!mStarted || (mNumFramesReceived == 0 && timestampUs < mStartTimeUs)) {
        ALOGV("Drop frame at %lld/%lld us", (long long)timestampUs, (long long)mStartTimeUs);
        releaseOneRecordingFrame(data);
        return;
    }

    // May need to skip frame or modify timestamp. Currently implemented
    // by the subclass CameraSourceTimeLapse.
    if (skipCurrentFrame(timestampUs)) {
        releaseOneRecordingFrame(data);
        return;
    }

    if (mNumFramesReceived > 0) {
        if (timestampUs <= mLastFrameTimestampUs) {
            ALOGW("Dropping frame with backward timestamp %lld (last %lld)",
                    (long long)timestampUs, (long long)mLastFrameTimestampUs);
            releaseOneRecordingFrame(data);
            return;
        }
        if (timestampUs - mLastFrameTimestampUs > mGlitchDurationThresholdUs) {
            ++mNumGlitches;
        }
    }

    mLastFrameTimestampUs = timestampUs;
    if (mNumFramesReceived == 0) {
        mFirstFrameTimeUs = timestampUs;
        // Initial delay
        if (mStartTimeUs > 0) {
            if (timestampUs < mStartTimeUs) {
                // Frame was captured before recording was started
                // Drop it without updating the statistical data.
                releaseOneRecordingFrame(data);
                return;
            }
            mStartTimeUs = timestampUs - mStartTimeUs;
        }
    }
    ++mNumFramesReceived;

    CHECK(data != NULL && data->size() > 0);
    mFramesReceived.push_back(data);
    int64_t timeUs = mStartTimeUs + (timestampUs - mFirstFrameTimeUs);
    mFrameTimes.push_back(timeUs);
    ALOGV("initial delay: %" PRId64 ", current time stamp: %" PRId64,
        mStartTimeUs, timeUs);
    mFrameAvailableCondition.signal();
}

bool CameraSource::isMetaDataStoredInVideoBuffers() const {
    ALOGV("isMetaDataStoredInVideoBuffers");
    return mIsMetaDataStoredInVideoBuffers;
}

CameraSource::ProxyListener::ProxyListener(const sp<CameraSource>& source) {
    mSource = source;
}

void CameraSource::ProxyListener::dataCallbackTimestamp(
        nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) {
    mSource->dataCallbackTimestamp(timestamp / 1000, msgType, dataPtr);
}

void CameraSource::DeathNotifier::binderDied(const wp<IBinder>& who __unused) {
    ALOGI("Camera recording proxy died");
}

}  // namespace android
