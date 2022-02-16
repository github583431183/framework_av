/*
 * Copyright 2018 The Android Open Source Project
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

#include <algorithm>
#include <unistd.h>
#include "AudioProcessorBase.h"
#include "SinkFloat.h"

using namespace flowgraph;

SinkFloat::SinkFloat(int32_t channelCount)
        : AudioSink(channelCount) {
}

int32_t SinkFloat::read(void *data, int32_t numFrames) {
    float *floatData = (float *) data;
    int32_t channelCount = input.getSamplesPerFrame();

    int32_t framesLeft = numFrames;
    while (framesLeft > 0) {
        // Run the graph and pull data through the input port.
        int32_t framesRead = pull(framesLeft);
        if (framesRead <= 0) {
            break;
        }
        const float *signal = input.getBlock();
        int32_t numSamples = framesRead * channelCount;
        memcpy(floatData, signal, numSamples * sizeof(float));
        floatData += numSamples;
        framesLeft -= framesRead;
    }
    return numFrames - framesLeft;
}
