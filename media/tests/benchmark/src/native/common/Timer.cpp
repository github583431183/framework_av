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
#define LOG_TAG "Timer"

#include <iostream>
#include <stdint.h>
#include <utils/Log.h>

#include "Timer.h"

void Timer::dumpStatistics(std::string inputReference, int64_t duarationUs) {
    ALOGV("In %s", __func__);
    if (!mOutputTimer.size()) {
        ALOGE("No output produced");
        return;
    }
    nsecs_t totalTimeTakenNs = getTotalTime();
    nsecs_t timeTakenPerSec = (totalTimeTakenNs * 1000000) / duarationUs;
    nsecs_t timeToFirstFrameNs = *mOutputTimer.begin() - mStartTimeNs;
    // get min and max output intervals.
    nsecs_t intervalNs;
    nsecs_t minTimeTakenNs = INT64_MAX;
    nsecs_t maxTimeTakenNs = 0;
    nsecs_t prevIntervalNs = mStartTimeNs;
    for (int32_t idx = 0; idx < mOutputTimer.size() - 1; idx++) {
        intervalNs = mOutputTimer.at(idx) - prevIntervalNs;
        prevIntervalNs = mOutputTimer.at(idx);
        if (minTimeTakenNs > intervalNs) minTimeTakenNs = intervalNs;
        else if (maxTimeTakenNs < intervalNs) maxTimeTakenNs = intervalNs;
    }

    int32_t index = inputReference.find(":");
    std::string operation = inputReference.substr(0, index);
    std::string reference = inputReference.substr(index + 1);
    // Print the Stats
    std::cout << "Reference : " << reference << endl;
    std::cout << "Setup Time in nano sec : " << mInitTimeNs << endl;
    std::cout << "Average Time in nano sec : " << totalTimeTakenNs / mOutputTimer.size() << endl;
    std::cout << "Time to first frame in nano sec : " << timeToFirstFrameNs << endl;
    std::cout << "Time taken (in nano sec) to " << operation
              << " 1 sec of content : " << timeTakenPerSec << endl;
    std::cout << "Minimum Time in nano sec : " << minTimeTakenNs << endl;
    std::cout << "Maximum Time in nano sec : " << maxTimeTakenNs << endl;
    std::cout << "Destroy Time in nano sec : " << mDeInitTimeNs << endl;
}
