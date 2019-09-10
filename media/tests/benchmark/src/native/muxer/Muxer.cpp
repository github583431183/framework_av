/*
 * Copyright (C) 2019 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "muxer"

#include <fstream>
#include <iostream>

#include "Muxer.h"

int32_t Muxer::initMuxer(int32_t fd, MUXER_OUTPUT_T outputFormat) {
    if (!mFormat) mFormat = mExtractor->getFormat();
    if (!mTimer) mTimer = new Timer();

    int64_t sTime = mTimer->getCurTime();
    mMuxer = AMediaMuxer_new(fd, (OutputFormat)outputFormat);
    if (!mMuxer) {
        cout << "[   WARN   ] Test Skipped. Unable to create muxer \n";
        return -1;
    }
    if (AMediaMuxer_addTrack(mMuxer, mFormat) < 0) {
        cout << "[   WARN   ] Test Skipped. Format not supported \n";
        return -1;
    }
    AMediaMuxer_start(mMuxer);
    int64_t eTime = mTimer->getCurTime();
    int64_t timeTaken = mTimer->getTimeDiff(sTime, eTime);
    mTimer->setInitTime(timeTaken);
    return 0;
}

void Muxer::deInitMuxer() {
    int64_t sTime = mTimer->getCurTime();
    if (mFormat) {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }
    if (!mMuxer) return;
    AMediaMuxer_stop(mMuxer);
    AMediaMuxer_delete(mMuxer);
    int64_t eTime = mTimer->getCurTime();
    int64_t timeTaken = mTimer->getTimeDiff(sTime, eTime);
    mTimer->setDeInitTime(timeTaken);
}

void Muxer::resetMuxer() {
    if (mTimer) mTimer->resetTimers();
}

void Muxer::dumpStatistics(string inputReference) {
    string operation = "mux";
    mTimer->dumpStatistics(operation, inputReference, mExtractor->getClipDuration());
}

int32_t Muxer::mux(uint8_t *inputBuffer, vector<AMediaCodecBufferInfo> &frameInfos) {
    // Mux frame data
    size_t frameIdx = 0;
    mTimer->setStartTime();
    while (frameIdx < frameInfos.size()) {
        AMediaCodecBufferInfo info = frameInfos.at(frameIdx);
        media_status_t status = AMediaMuxer_writeSampleData(mMuxer, 0, inputBuffer, &info);
        if (status != 0) {
            ALOGE("Error in AMediaMuxer_writeSampleData");
            return -1;
        }
        mTimer->addOutputTime();
        frameIdx++;
    }
    return 0;
}
