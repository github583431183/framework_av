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
#define LOG_TAG "extractorTest"

#include <gtest/gtest.h>

#include "BenchmarkTestEnvironment.h"
#include "Extractor.h"

static MediaCodecTestEnvironment *gEnv = nullptr;

class ExtractorTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ExtractorTest, Extract) {
    Extractor *extractObj = new Extractor();

    std::string inputFile = gEnv->getRes() + GetParam();
    FILE *inputFp = fopen(inputFile.c_str(), "rb");
    if (!inputFp) {
        std::cout << "[   WARN   ] Test Skipped. Unable to open input file for reading \n";
        return;
    }

    // Read file properties
    size_t fileSize = 0;
    fseek(inputFp, 0, SEEK_END);
    fileSize = ftell(inputFp);
    fseek(inputFp, 0, SEEK_SET);
    int32_t fd = fileno(inputFp);

    int32_t trackCount = extractObj->initExtractor(fd, fileSize);
    if (trackCount <= 0) {
        std::cout << "[   WARN   ] Test Skipped. initExtractor failed\n";
        return;
    }
    for (int32_t currentTrack = 0; currentTrack < trackCount; currentTrack++) {
        int32_t status = extractObj->extract(currentTrack);
        if (status != 0) {
            std::cout << "[   WARN   ] Test Skipped. Extraction failed \n";
            return;
        }

        extractObj->dumpStatistics();
        extractObj->resetExtractor();
    }
    extractObj->deInitExtractor();
    fclose(inputFp);
    delete extractObj;
}

INSTANTIATE_TEST_SUITE_P(
        ExtractorTestAll, ExtractorTest,
        ::testing::Values("crowd_1920x1080_25_4000_vp9.webm", "crowd_1920x1080_25_6000_h263.3gp",
                          "crowd_1920x1080_25_6000_mpeg4.mp4", "crowd_1920x1080_25_6700_h264.ts",
                          "crowd_1920x1080_25_7300_mpeg2.mp4", "crowd_1920x1080_25_4000_av1.webm",
                          "crowd_1920x1080_25_4000_h265.mkv", "crowd_1920x1080_25_4000_vp8.webm"));

int main(int argc, char **argv) {
    gEnv = new MediaCodecTestEnvironment();
    ::testing::AddGlobalTestEnvironment(gEnv);
    ::testing::InitGoogleTest(&argc, argv);
    int status = gEnv->initFromOptions(argc, argv);
    if (status == 0) {
        status = RUN_ALL_TESTS();
        ALOGD("C2 Test result = %d\n", status);
    }
    return status;
}
