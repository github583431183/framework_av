/*
 * Copyright (C) 2011 NXP Software
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_NDEBUG 1
#define LOG_TAG "VideoEditorPlayer"
#include <utils/Log.h>

#include "VideoEditorPlayer.h"
#include "PreviewPlayer.h"

#include <media/Metadata.h>
#include <media/stagefright/MediaExtractor.h>

namespace android {

VideoEditorPlayer::VideoEditorPlayer()
    : mPlayer(new PreviewPlayer) {

    LOGV("VideoEditorPlayer");
    mPlayer->setListener(this);
}

VideoEditorPlayer::~VideoEditorPlayer() {
    LOGV("~VideoEditorPlayer");

    reset();
    mVeAudioSink.clear();

    delete mPlayer;
    mPlayer = NULL;
}

status_t VideoEditorPlayer::initCheck() {
    LOGV("initCheck");
    return OK;
}

status_t VideoEditorPlayer::setDataSource(
        const char *url, const KeyedVector<String8, String8> *headers) {
    LOGI("setDataSource('%s')", url);

    mVeAudioSink = new VeAudioOutput();
    mPlayer->setAudioSink(mVeAudioSink);

    return mPlayer->setDataSource(url, headers);
}

//We donot use this in preview, dummy implimentation as this is pure virtual
status_t VideoEditorPlayer::setDataSource(int fd, int64_t offset,
    int64_t length) {
    LOGE("setDataSource(%d, %lld, %lld) Not supported", fd, offset, length);
    return (!OK);
}

status_t VideoEditorPlayer::setVideoISurface(const sp<ISurface> &surface) {
    LOGV("setVideoISurface");

    mPlayer->setISurface(surface);
    return OK;
}

status_t VideoEditorPlayer::setVideoSurface(const sp<Surface> &surface) {
    LOGV("setVideoSurface");

    mPlayer->setSurface(surface);
    return OK;
}

status_t VideoEditorPlayer::prepare() {
    LOGV("prepare");
    return mPlayer->prepare();
}

status_t VideoEditorPlayer::prepareAsync() {
    return mPlayer->prepareAsync();
}

status_t VideoEditorPlayer::start() {
    LOGV("start");
    return mPlayer->play();
}

status_t VideoEditorPlayer::stop() {
    LOGV("stop");
    return pause();
}

status_t VideoEditorPlayer::pause() {
    LOGV("pause");
    return mPlayer->pause();
}

bool VideoEditorPlayer::isPlaying() {
    LOGV("isPlaying");
    return mPlayer->isPlaying();
}

status_t VideoEditorPlayer::seekTo(int msec) {
    LOGV("seekTo");
    status_t err = mPlayer->seekTo((int64_t)msec * 1000);
    return err;
}

status_t VideoEditorPlayer::getCurrentPosition(int *msec) {
    LOGV("getCurrentPosition");
    int64_t positionUs;
    status_t err = mPlayer->getPosition(&positionUs);

    if (err != OK) {
        return err;
    }

    *msec = (positionUs + 500) / 1000;
    return OK;
}

status_t VideoEditorPlayer::getDuration(int *msec) {
    LOGV("getDuration");

    int64_t durationUs;
    status_t err = mPlayer->getDuration(&durationUs);

    if (err != OK) {
        *msec = 0;
        return OK;
    }

    *msec = (durationUs + 500) / 1000;
    return OK;
}

status_t VideoEditorPlayer::reset() {
    LOGV("reset");
    mPlayer->reset();
    return OK;
}

status_t VideoEditorPlayer::setLooping(int loop) {
    LOGV("setLooping");
    return mPlayer->setLooping(loop);
}

player_type VideoEditorPlayer::playerType() {
    LOGV("playerType");
    return STAGEFRIGHT_PLAYER;
}

status_t VideoEditorPlayer::suspend() {
    LOGV("suspend");
    return mPlayer->suspend();
}

status_t VideoEditorPlayer::resume() {
    LOGV("resume");
    return mPlayer->resume();
}

status_t VideoEditorPlayer::invoke(const Parcel &request, Parcel *reply) {
    return INVALID_OPERATION;
}

void VideoEditorPlayer::setAudioSink(const sp<AudioSink> &audioSink) {
    MediaPlayerInterface::setAudioSink(audioSink);

    mPlayer->setAudioSink(audioSink);
}

status_t VideoEditorPlayer::getMetadata(
        const media::Metadata::Filter& ids, Parcel *records) {
    using media::Metadata;

    uint32_t flags = mPlayer->flags();

    Metadata metadata(records);

    metadata.appendBool(
            Metadata::kPauseAvailable,
            flags & MediaExtractor::CAN_PAUSE);

    metadata.appendBool(
            Metadata::kSeekBackwardAvailable,
            flags & MediaExtractor::CAN_SEEK_BACKWARD);

    metadata.appendBool(
            Metadata::kSeekForwardAvailable,
            flags & MediaExtractor::CAN_SEEK_FORWARD);

    metadata.appendBool(
            Metadata::kSeekAvailable,
            flags & MediaExtractor::CAN_SEEK);

    return OK;
}

status_t VideoEditorPlayer::loadEffectsSettings(
    M4VSS3GPP_EffectSettings* pEffectSettings, int nEffects) {
    LOGV("loadEffectsSettings");
    return mPlayer->loadEffectsSettings(pEffectSettings, nEffects);
}

status_t VideoEditorPlayer::loadAudioMixSettings(
    M4xVSS_AudioMixingSettings* pAudioMixSettings) {
    LOGV("VideoEditorPlayer: loadAudioMixSettings");
    return mPlayer->loadAudioMixSettings(pAudioMixSettings);
}

status_t VideoEditorPlayer::setAudioMixPCMFileHandle(
    M4OSA_Context pAudioMixPCMFileHandle) {

    LOGV("VideoEditorPlayer: loadAudioMixSettings");
    return mPlayer->setAudioMixPCMFileHandle(pAudioMixPCMFileHandle);
}

status_t VideoEditorPlayer::setAudioMixStoryBoardParam(
    M4OSA_UInt32 audioMixStoryBoardTS,
    M4OSA_UInt32 currentMediaBeginCutTime,
    M4OSA_UInt32 primaryTrackVolValue) {

    LOGV("VideoEditorPlayer: loadAudioMixSettings");
    return mPlayer->setAudioMixStoryBoardParam(audioMixStoryBoardTS,
     currentMediaBeginCutTime, primaryTrackVolValue);
}

status_t VideoEditorPlayer::setPlaybackBeginTime(uint32_t msec) {
    LOGV("setPlaybackBeginTime");
    return mPlayer->setPlaybackBeginTime(msec);
}

status_t VideoEditorPlayer::setPlaybackEndTime(uint32_t msec) {
    LOGV("setPlaybackEndTime");
    return mPlayer->setPlaybackEndTime(msec);
}

status_t VideoEditorPlayer::setStoryboardStartTime(uint32_t msec) {
    LOGV("setStoryboardStartTime");
    return mPlayer->setStoryboardStartTime(msec);
}

status_t VideoEditorPlayer::setProgressCallbackInterval(uint32_t cbInterval) {
    LOGV("setProgressCallbackInterval");
    return mPlayer->setProgressCallbackInterval(cbInterval);
}

status_t VideoEditorPlayer::setMediaRenderingMode(
    M4xVSS_MediaRendering mode,
    M4VIDEOEDITING_VideoFrameSize outputVideoSize) {

    LOGV("setMediaRenderingMode");
    return mPlayer->setMediaRenderingMode(mode, outputVideoSize);
}

status_t VideoEditorPlayer::resetJniCallbackTimeStamp() {
    LOGV("resetJniCallbackTimeStamp");
    return mPlayer->resetJniCallbackTimeStamp();
}

status_t VideoEditorPlayer::setImageClipProperties(
    uint32_t width, uint32_t height) {
    return mPlayer->setImageClipProperties(width, height);
}

status_t VideoEditorPlayer::readFirstVideoFrame() {
    return mPlayer->readFirstVideoFrame();
}

status_t VideoEditorPlayer::getLastRenderedTimeMs(uint32_t *lastRenderedTimeMs) {
    mPlayer->getLastRenderedTimeMs(lastRenderedTimeMs);
    return NO_ERROR;
}

/* Implementation of AudioSink interface */
#undef LOG_TAG
#define LOG_TAG "VeAudioSink"

int VideoEditorPlayer::VeAudioOutput::mMinBufferCount = 4;
bool VideoEditorPlayer::VeAudioOutput::mIsOnEmulator = false;

VideoEditorPlayer::VeAudioOutput::VeAudioOutput()
    : mCallback(NULL),
      mCallbackCookie(NULL) {
    mTrack = 0;
    mStreamType = AudioSystem::MUSIC;
    mLeftVolume = 1.0;
    mRightVolume = 1.0;
    mLatency = 0;
    mMsecsPerFrame = 0;
    mNumFramesWritten = 0;
    setMinBufferCount();
}

VideoEditorPlayer::VeAudioOutput::~VeAudioOutput() {
    close();
}

void VideoEditorPlayer::VeAudioOutput::setMinBufferCount() {

    mIsOnEmulator = false;
    mMinBufferCount = 4;
}

bool VideoEditorPlayer::VeAudioOutput::isOnEmulator() {

    setMinBufferCount();
    return mIsOnEmulator;
}

int VideoEditorPlayer::VeAudioOutput::getMinBufferCount() {

    setMinBufferCount();
    return mMinBufferCount;
}

ssize_t VideoEditorPlayer::VeAudioOutput::bufferSize() const {

    if (mTrack == 0) return NO_INIT;
    return mTrack->frameCount() * frameSize();
}

ssize_t VideoEditorPlayer::VeAudioOutput::frameCount() const {

    if (mTrack == 0) return NO_INIT;
    return mTrack->frameCount();
}

ssize_t VideoEditorPlayer::VeAudioOutput::channelCount() const
{
    if (mTrack == 0) return NO_INIT;
    return mTrack->channelCount();
}

ssize_t VideoEditorPlayer::VeAudioOutput::frameSize() const
{
    if (mTrack == 0) return NO_INIT;
    return mTrack->frameSize();
}

uint32_t VideoEditorPlayer::VeAudioOutput::latency () const
{
    return mLatency;
}

float VideoEditorPlayer::VeAudioOutput::msecsPerFrame() const
{
    return mMsecsPerFrame;
}

status_t VideoEditorPlayer::VeAudioOutput::getPosition(uint32_t *position) {

    if (mTrack == 0) return NO_INIT;
    return mTrack->getPosition(position);
}

status_t VideoEditorPlayer::VeAudioOutput::open(
        uint32_t sampleRate, int channelCount, int format, int bufferCount,
        AudioCallback cb, void *cookie) {

    mCallback = cb;
    mCallbackCookie = cookie;

    // Check argument "bufferCount" against the mininum buffer count
    if (bufferCount < mMinBufferCount) {
        LOGV("bufferCount (%d) is too small and increased to %d",
            bufferCount, mMinBufferCount);
        bufferCount = mMinBufferCount;

    }
    LOGV("open(%u, %d, %d, %d)", sampleRate, channelCount, format, bufferCount);
    if (mTrack) close();
    int afSampleRate;
    int afFrameCount;
    int frameCount;

    if (AudioSystem::getOutputFrameCount(&afFrameCount, mStreamType) !=
     NO_ERROR) {
        return NO_INIT;
    }
    if (AudioSystem::getOutputSamplingRate(&afSampleRate, mStreamType) !=
     NO_ERROR) {
        return NO_INIT;
    }

    frameCount = (sampleRate*afFrameCount*bufferCount)/afSampleRate;

    AudioTrack *t;
    if (mCallback != NULL) {
        t = new AudioTrack(
                mStreamType,
                sampleRate,
                format,
                (channelCount == 2) ?
                 AudioSystem::CHANNEL_OUT_STEREO : AudioSystem::CHANNEL_OUT_MONO,
                frameCount,
                0 /* flags */,
                CallbackWrapper,
                this);
    } else {
        t = new AudioTrack(
                mStreamType,
                sampleRate,
                format,
                (channelCount == 2) ?
                 AudioSystem::CHANNEL_OUT_STEREO : AudioSystem::CHANNEL_OUT_MONO,
                frameCount);
    }

    if ((t == 0) || (t->initCheck() != NO_ERROR)) {
        LOGE("Unable to create audio track");
        delete t;
        return NO_INIT;
    }

    LOGV("setVolume");
    t->setVolume(mLeftVolume, mRightVolume);
    mMsecsPerFrame = 1.e3 / (float) sampleRate;
    mLatency = t->latency();
    mTrack = t;
    return NO_ERROR;
}

void VideoEditorPlayer::VeAudioOutput::start() {

    LOGV("start");
    if (mTrack) {
        mTrack->setVolume(mLeftVolume, mRightVolume);
        mTrack->start();
        mTrack->getPosition(&mNumFramesWritten);
    }
}

void VideoEditorPlayer::VeAudioOutput::snoopWrite(
    const void* buffer, size_t size) {
    // Visualization buffers not supported
    return;

}

ssize_t VideoEditorPlayer::VeAudioOutput::write(
     const void* buffer, size_t size) {

    LOG_FATAL_IF(mCallback != NULL, "Don't call write if supplying a callback.");

    //LOGV("write(%p, %u)", buffer, size);
    if (mTrack) {
        snoopWrite(buffer, size);
        ssize_t ret = mTrack->write(buffer, size);
        mNumFramesWritten += ret / 4; // assume 16 bit stereo
        return ret;
    }
    return NO_INIT;
}

void VideoEditorPlayer::VeAudioOutput::stop() {

    LOGV("stop");
    if (mTrack) mTrack->stop();
}

void VideoEditorPlayer::VeAudioOutput::flush() {

    LOGV("flush");
    if (mTrack) mTrack->flush();
}

void VideoEditorPlayer::VeAudioOutput::pause() {

    LOGV("VeAudioOutput::pause");
    if (mTrack) mTrack->pause();
}

void VideoEditorPlayer::VeAudioOutput::close() {

    LOGV("close");
    delete mTrack;
    mTrack = 0;
}

void VideoEditorPlayer::VeAudioOutput::setVolume(float left, float right) {

    LOGV("setVolume(%f, %f)", left, right);
    mLeftVolume = left;
    mRightVolume = right;
    if (mTrack) {
        mTrack->setVolume(left, right);
    }
}

// static
void VideoEditorPlayer::VeAudioOutput::CallbackWrapper(
        int event, void *cookie, void *info) {
    //LOGV("VeAudioOutput::callbackwrapper");
    if (event != AudioTrack::EVENT_MORE_DATA) {
        return;
    }

    VeAudioOutput *me = (VeAudioOutput *)cookie;
    AudioTrack::Buffer *buffer = (AudioTrack::Buffer *)info;

    size_t actualSize = (*me->mCallback)(
            me, buffer->raw, buffer->size, me->mCallbackCookie);

    buffer->size = actualSize;

    if (actualSize > 0) {
        me->snoopWrite(buffer->raw, actualSize);
    }
}

status_t VideoEditorPlayer::VeAudioOutput::dump(int fd, const Vector<String16>& args) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    result.append(" VeAudioOutput\n");
    snprintf(buffer, SIZE-1, "  stream type(%d), left - right volume(%f, %f)\n",
            mStreamType, mLeftVolume, mRightVolume);
    result.append(buffer);
    snprintf(buffer, SIZE-1, "  msec per frame(%f), latency (%d)\n",
            mMsecsPerFrame, mLatency);
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    if (mTrack != 0) {
        mTrack->dump(fd, args);
    }
    return NO_ERROR;
}

int VideoEditorPlayer::VeAudioOutput::getSessionId() {

    return mSessionId;
}

}  // namespace android
