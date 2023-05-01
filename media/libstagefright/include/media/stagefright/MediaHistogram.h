/*
 * Copyright 2023, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_HISTOGRAM_H_

#define MEDIA_HISTOGRAM_H_

#include <string>

namespace android {

class MediaHistogram {
    public:
    MediaHistogram() : mFloor(0), mWidth(0), mBelow(0), mAbove(0),
                    mMin(INT64_MAX), mMax(INT64_MIN), mSum(0), mCount(0),
                    mBucketCount(0), mBuckets(NULL) {};
    ~MediaHistogram() { clear(); };
    void clear() { if (mBuckets != NULL) free(mBuckets); mBuckets = NULL; };
    bool setup(int bucketCount, int64_t width, int64_t floor = 0);
    void insert(int64_t sample);
    int64_t getMin() const { return mMin; }
    int64_t getMax() const { return mMax; }
    int64_t getCount() const { return mCount; }
    int64_t getSum() const { return mSum; }
    int64_t getAvg() const { return mSum / (mCount == 0 ? 1 : mCount); }
    std::string emit();
private:
    int64_t mFloor, mCeiling, mWidth;
    int64_t mBelow, mAbove;
    int64_t mMin, mMax, mSum, mCount;

    int mBucketCount;
    int64_t *mBuckets;
};

} // android

#endif // MEDIA_HISTOGRAM_H_
