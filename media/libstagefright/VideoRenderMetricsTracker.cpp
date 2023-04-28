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

#define LOG_TAG "VideoRenderMetricsTracker"
#include <utils/Log.h>

#include <media/stagefright/VideoRenderMetricsTracker.h>

#include <cmath>
#include <sys/time.h>

namespace android {


VideoRenderMetrics::VideoRenderMetrics() {
    firstFrameRenderTimeUs = 0;
    frameReleasedCount = 0;
    frameRenderedCount = 0;
    frameDroppedCount = 0;
    frameSkippedCount = 0;
    contentFrameRate = FRAME_RATE_UNDETERMINED;
    desiredFrameRate = FRAME_RATE_UNDETERMINED;
    actualFrameRate = FRAME_RATE_UNDETERMINED;
}

VideoRenderMetricsTracker::Configuration::Configuration() {
    // Assume that the app is skipping frames because it's detected that the frame couldn't be
    // rendered in time.
    areSkippedFramesDropped = true;

    // 400ms is 8 frames at 20 frames per second and 24 frames at 60 frames per second
    maxExpectedContentFrameDurationUs = 400 * 1000;

    // Allow for 2 milliseconds of deviation when detecting frame rates
    frameRateDetectionToleranceUs = 2 * 1000;

    // Allow for a tolerance of 200 milliseconds for determining if we moved forward in content time
    // because of frame drops for live content, or because the user is seeking.
    contentTimeAdvancedForLiveContentToleranceUs = 200 * 1000;
}

VideoRenderMetricsTracker::VideoRenderMetricsTracker() :
        mConfiguration(Configuration()) {
    resetForDiscontinuity();
}

VideoRenderMetricsTracker::VideoRenderMetricsTracker(const Configuration & configuration) :
        mConfiguration(configuration) {
    resetForDiscontinuity();
}

void VideoRenderMetricsTracker::onFrameSkipped(int64_t contentTimeUs) {
    // Frames skipped at the beginning shouldn't really be counted as skipped frames, since the
    // app might be seeking to a starting point that isn't the first key frame.
    if (mLastRenderTimeUs == -1) {
        return;
    }
    // Frames skipped at the end of playback shouldn't be counted as skipped frames, since the
    // app could be terminating the playback. The pending count will be added to the metrics if and
    // when the next frame is released.
    mPendingSkippedFrameContentTimeUsList.push_back(contentTimeUs);
}

void VideoRenderMetricsTracker::onFrameReleased(int64_t contentTimeUs) {
    onFrameReleased(contentTimeUs, nowUs() * 1000);
}

void VideoRenderMetricsTracker::onFrameReleased(int64_t contentTimeUs, int64_t desiredRenderTimeNs) {
    int64_t desiredRenderTimeUs = desiredRenderTimeNs / 1000;
    resetIfDiscontinuity(contentTimeUs, desiredRenderTimeUs);
    mMetrics.frameReleasedCount++;
    mNextExpectedRenderedFrameQueue.push({contentTimeUs, desiredRenderTimeUs});
    mLastContentTimeUs = contentTimeUs;
}

void VideoRenderMetricsTracker::onFrameRendered(int64_t contentTimeUs, int64_t actualRenderTimeNs) {
    int64_t actualRenderTimeUs = actualRenderTimeNs / 1000;

    // Now that a frame has been rendered, the previously skipped frames can be processed as skipped
    // frames since the app is not skipping them to terminate playback.
    for (int64_t contentTimeUs : mPendingSkippedFrameContentTimeUsList) {
        processMetricsForSkippedFrame(contentTimeUs);
    }
    mPendingSkippedFrameContentTimeUsList.clear();

    static const FrameInfo noFrame = {-1, -1};
    FrameInfo nextExpectedFrame = noFrame;
    while (!mNextExpectedRenderedFrameQueue.empty()) {
        nextExpectedFrame = mNextExpectedRenderedFrameQueue.front();
        mNextExpectedRenderedFrameQueue.pop();
        // Happy path - the rendered frame is what we expected it to be
        if (contentTimeUs == nextExpectedFrame.contentTimeUs) {
            break;
        }
        // This isn't really supposed to happen - the next rendered frame should be the expected
        // frame, or, if there's frame drops, it will be a frame later in the content stream
        if (contentTimeUs < nextExpectedFrame.contentTimeUs) {
            ALOGW("Rendered frame is earlier than the next expected frame (%lld, %lld)",
                  (long long) contentTimeUs, (long long) nextExpectedFrame.contentTimeUs);
            break;
        }
        processMetricsForDroppedFrame(contentTimeUs, nextExpectedFrame.desiredRenderTimeUs);
    }
    processMetricsForRenderedFrame(nextExpectedFrame.contentTimeUs,
            nextExpectedFrame.desiredRenderTimeUs, actualRenderTimeUs);
    mLastRenderTimeUs = actualRenderTimeUs;
}

const VideoRenderMetrics& VideoRenderMetricsTracker::getMetrics() const {
    return mMetrics;
}

void VideoRenderMetricsTracker::resetForDiscontinuity() {
    mLastContentTimeUs = -1;
    mLastRenderTimeUs = -1;

    // Don't worry about tracking frame rendering times from now up until playback catches up to the
    // discontinuity. While stuttering or freezing could be found in the next few frames, the impact
    // to the user is is minimal, so better to just keep things simple and don't bother.
    while (!mNextExpectedRenderedFrameQueue.empty()) {
        mNextExpectedRenderedFrameQueue.pop();
    }

    // Ignore any frames that were skipped just prior to the discontinuity.
    mPendingSkippedFrameContentTimeUsList.clear();

    // All frame durations can be now ignored since all bets are off now on what the render durations
    // should be after the discontinuity.
    for (int i = 0; i < FrameDurationUs::SIZE; ++i) {
        mActualFrameDurationUs[i] = -1;
        mDesiredFrameDurationUs[i] = -1;
        mContentFrameDurationUs[i] = -1;
    }
}

bool VideoRenderMetricsTracker::resetIfDiscontinuity(int64_t contentTimeUs, int64_t desiredRenderTimeUs) {
    if (mLastContentTimeUs == -1) {
        resetForDiscontinuity();
        return true;
    }
    if (contentTimeUs < mLastContentTimeUs) {
        ALOGI("Video playback jumped %d ms backwards in content time (%d -> %d)",
              int((mLastContentTimeUs - contentTimeUs) / 1000), int(mLastContentTimeUs / 1000),
              int(contentTimeUs / 1000));
        resetForDiscontinuity();
        return true;
    }
    if (contentTimeUs - mLastContentTimeUs > mConfiguration.maxExpectedContentFrameDurationUs) {
        // The content frame duration could be long due to frame drops for live content. This can be
        // detected by looking at the app's desired rendering duration. If the app's rendered frame
        // duration is roughly the same as the content's frame duration, then it is assumed that
        // the forward discontinuity is due to frame drops for live content. A false positive can
        // occur if the time the user spends seeking is equal to the duration of the seek. This is
        // very unlikely to occur in practice but CAN occur - the user starts seeking forward, gets
        // distracted, and then returns to seeking forward.
        int64_t contentFrameDurationUs = contentTimeUs - mLastContentTimeUs;
        int64_t desiredFrameDurationUs = desiredRenderTimeUs - mLastRenderTimeUs;
        bool skippedForwardDueToLiveContentFrameDrops =
                abs(contentFrameDurationUs - desiredFrameDurationUs) <
                mConfiguration.contentTimeAdvancedForLiveContentToleranceUs;
        if (!skippedForwardDueToLiveContentFrameDrops) {
            ALOGI("Video playback jumped %d ms forward in content time (%d -> %d) ",
                int((contentTimeUs - mLastContentTimeUs) / 1000), int(mLastContentTimeUs / 1000),
                int(contentTimeUs / 1000));
            resetForDiscontinuity();
            return true;
        }
    }
    return false;
}

void VideoRenderMetricsTracker::processMetricsForSkippedFrame(int64_t contentTimeUs) {
    mMetrics.frameSkippedCount++;
    if (mConfiguration.areSkippedFramesDropped) {
        processMetricsForDroppedFrame(contentTimeUs, -1);
        return;
    }
    updateFrameDurations(mContentFrameDurationUs, contentTimeUs);
    updateFrameDurations(mDesiredFrameDurationUs, -1);
    updateFrameDurations(mActualFrameDurationUs, -1);
    updateFrameRate(mMetrics.contentFrameRate, mContentFrameDurationUs, mConfiguration);
}

void VideoRenderMetricsTracker::processMetricsForDroppedFrame(int64_t contentTimeUs,
        int64_t desiredRenderTimeUs) {
    mMetrics.frameDroppedCount++;
    updateFrameDurations(mContentFrameDurationUs, contentTimeUs);
    updateFrameDurations(mDesiredFrameDurationUs, desiredRenderTimeUs);
    updateFrameDurations(mActualFrameDurationUs, -1);
    updateFrameRate(mMetrics.contentFrameRate, mContentFrameDurationUs, mConfiguration);
    updateFrameRate(mMetrics.desiredFrameRate, mDesiredFrameDurationUs, mConfiguration);
}

void VideoRenderMetricsTracker::processMetricsForRenderedFrame(int64_t contentTimeUs,
        int64_t desiredRenderTimeUs, int64_t actualRenderTimeUs) {
    // Capture the timestamp at which the first frame was rendered
    if (mMetrics.firstFrameRenderTimeUs == 0) {
        mMetrics.firstFrameRenderTimeUs = actualRenderTimeUs;
    }

    mMetrics.frameRenderedCount++;
    // The content time is -1 when it was rendered after a discontinuity (e.g. seek) was detected.
    // So, even though a frame was rendered, it's impact on the user is insignificant, so don't do
    // anything other than counting it as a rendered frame.
    if (contentTimeUs == -1) {
        return;
    }
    updateFrameDurations(mContentFrameDurationUs, contentTimeUs);
    updateFrameDurations(mDesiredFrameDurationUs, desiredRenderTimeUs);
    updateFrameDurations(mActualFrameDurationUs, actualRenderTimeUs);
    updateFrameRate(mMetrics.contentFrameRate, mContentFrameDurationUs, mConfiguration);
    updateFrameRate(mMetrics.desiredFrameRate, mDesiredFrameDurationUs, mConfiguration);
    updateFrameRate(mMetrics.actualFrameRate, mActualFrameDurationUs, mConfiguration);
}

int64_t VideoRenderMetricsTracker::nowUs() {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return ((int64_t)(t.tv_sec) * 1000000000ll + t.tv_nsec) / 1000ll;
}

void VideoRenderMetricsTracker::updateFrameDurations(FrameDurationUs & durationUs,
        int64_t newTimestampUs) {
    durationUs[4] = durationUs[3];
    durationUs[3] = durationUs[2];
    durationUs[2] = durationUs[1];
    durationUs[1] = durationUs[0];
    if (newTimestampUs == -1) {
        durationUs[0] = -1;
    } else {
        durationUs[0] = durationUs.priorTimestampUs == -1 ? -1 :
                newTimestampUs - durationUs.priorTimestampUs;
        durationUs.priorTimestampUs = newTimestampUs;
    }
}

void VideoRenderMetricsTracker::updateFrameRate(float & frameRate, FrameDurationUs & durationUs,
        const Configuration & c) {
    float newFrameRate = detectFrameRate(durationUs, c);
    if (newFrameRate != FRAME_RATE_UNDETERMINED) {
        frameRate = newFrameRate;
    }
}

float VideoRenderMetricsTracker::detectFrameRate(const FrameDurationUs & durationUs,
        const Configuration & c) {
    if (durationUs[0] == -1 || durationUs[1] == -1 || durationUs[2] == -1) {
        return FRAME_RATE_UNDETERMINED;
    }
    // Allow for 2 milliseconds of tolerance
    if (abs(durationUs[0] - durationUs[1]) > c.frameRateDetectionToleranceUs ||
        abs(durationUs[0] - durationUs[2]) > c.frameRateDetectionToleranceUs) {
        return is32pulldown(durationUs, c) ? FRAME_RATE_24HZ_3_2_PULLDOWN : FRAME_RATE_UNDETERMINED;
    }
    return 1000.0 * 1000.0 / durationUs[0];
}

bool VideoRenderMetricsTracker::is32pulldown(const FrameDurationUs & durationUs,
        const Configuration & c) {
    if (durationUs[0] == -1 || durationUs[1] == -1 || durationUs[2] == -1 || durationUs[3] == -1 ||
        durationUs[4] == -1) {
        return false;
    }
    // 3:2 pulldown expects that every other frame has identical duration...
    if (abs(durationUs[0] - durationUs[2]) > c.frameRateDetectionToleranceUs ||
        abs(durationUs[1] - durationUs[3]) > c.frameRateDetectionToleranceUs ||
        abs(durationUs[0] - durationUs[4]) > c.frameRateDetectionToleranceUs) {
        return false;
    }
    // ... for either 2 vsysncs or 3 vsyncs
    if ((abs(durationUs[0] - 33333) < c.frameRateDetectionToleranceUs &&
         abs(durationUs[1] - 50000) < c.frameRateDetectionToleranceUs) ||
        (abs(durationUs[0] - 50000) < c.frameRateDetectionToleranceUs &&
         abs(durationUs[1] - 33333) < c.frameRateDetectionToleranceUs)) {
        return true;
    }
    return false;
}

} // namespace android
