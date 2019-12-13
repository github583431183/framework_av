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
#define LOG_TAG "AmrwbDecoderTest"
#define OUTPUT_FILE "/data/local/tmp/amrwbDecode.out"

#include <utils/Log.h>

#include <audio_utils/sndfile.h>
#include <stdio.h>

#include "pvamrwbdecoder.h"
#include "pvamrwbdecoder_api.h"

#include "AmrwbDecTestEnvironment.h"

// Constants for AMR-WB.
constexpr int32_t kInputBufferSize = 64;
constexpr int32_t kSamplesPerFrame = 320;
constexpr int32_t kBitsPerSample = 16;
constexpr int32_t kSampleRate = 16000;
constexpr int32_t kChannels = 1;
constexpr int32_t kMaxSourceDataUnitSize = NBBITS_24k * sizeof(int16_t);
const uint32_t kFrameSizes[] = {17, 23, 32, 36, 40, 46, 50, 58, 60};
constexpr int32_t kNumFrameReset = 150;

constexpr int32_t kMaxCount = 10;

static AmrwbDecTestEnvironment *gEnv = nullptr;

class AmrwbDecoderTest : public ::testing::TestWithParam<string> {
  public:
    virtual void SetUp() override {
        mFpInput = nullptr;

        mInputBuf = static_cast<uint8_t *>(malloc(kInputBufferSize));
        ASSERT_NE(mInputBuf, nullptr) << "Unable to allocate input buffer";

        mInputSampleBuf = (int16_t *)malloc(kMaxSourceDataUnitSize);
        ASSERT_NE(mInputSampleBuf, nullptr) << "Unable to allocate input sample buffer";

        int32_t outputBufferSize = kSamplesPerFrame * kBitsPerSample / 8;
        mOutputBuf = static_cast<int16_t *>(malloc(outputBufferSize));
        ASSERT_NE(mOutputBuf, nullptr) << "Unable to allocate output buffer";
    }
    virtual void TearDown() override {
        if (mFpInput) {
            fclose(mFpInput);
            mFpInput = nullptr;
        }
        if (mInputBuf) {
            free(mInputBuf);
            mInputSampleBuf = nullptr;
        }
        if (mInputSampleBuf) {
            free(mInputSampleBuf);
            mInputSampleBuf = nullptr;
        }
        if (mOutputBuf) {
            free(mOutputBuf);
            mOutputBuf = nullptr;
        }
    }

    uint8_t *mInputBuf;
    int16_t *mInputSampleBuf;
    int16_t *mOutputBuf;
    FILE *mFpInput;

    int32_t DecodeFrames(int16_t *decoderCookie, void *decoderBuf, SNDFILE *outFileHandle,
                         int32_t frameCount = INT32_MAX);
    SNDFILE *openOutputFile(SF_INFO *sfInfo);
};

SNDFILE *AmrwbDecoderTest::openOutputFile(SF_INFO *sfInfo) {
    memset(sfInfo, 0, sizeof(SF_INFO));
    sfInfo->channels = kChannels;
    sfInfo->format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    sfInfo->samplerate = kSampleRate;
    SNDFILE *outFileHandle = sf_open(OUTPUT_FILE, SFM_WRITE, sfInfo);
    return outFileHandle;
}

int32_t AmrwbDecoderTest::DecodeFrames(int16_t *decoderCookie, void *decoderBuf,
                                       SNDFILE *outFileHandle, int32_t frameCount) {
    while (frameCount > 0) {
        uint8_t modeByte;
        int bytesRead = fread(&modeByte, 1, 1, mFpInput);
        if (bytesRead != 1) break;

        int16 mode = ((modeByte >> 3) & 0x0f);
        // AMR-WB file format cannot have mode 10, 11, 12 and 13.
        if (mode > 9 && mode < 14) {
            ALOGE("Illegal frame mode");
            return -1;
        }

        if (mode >= 9) {
            // Produce silence for comfort noise, speech lost and no data.
            int32_t outputBufferSize = kSamplesPerFrame * kBitsPerSample / 8;
            memset(mOutputBuf, 0, outputBufferSize);
        } else {
            // Read rest of the frame.
            int32_t frameSize = kFrameSizes[mode];
            bytesRead = fread(mInputBuf, 1, frameSize, mFpInput);
            if (bytesRead != frameSize) break;

            int16 frameMode = mode;
            int16 frameType;
            RX_State_wb rx_state;
            mime_unsorting(mInputBuf, mInputSampleBuf, &frameType, &frameMode, 1, &rx_state);

            int16_t numSamplesOutput;
            pvDecoder_AmrWb(frameMode, mInputSampleBuf, mOutputBuf, &numSamplesOutput, decoderBuf,
                            frameType, decoderCookie);
            if (numSamplesOutput != kSamplesPerFrame) {
                ALOGE("Failed to decode the input file");
                return -1;
            }
            for (int i = 0; i < kSamplesPerFrame; ++i) {
                mOutputBuf[i] &= 0xfffC;
            }
        }
        sf_writef_short(outFileHandle, mOutputBuf, kSamplesPerFrame / kChannels);
        frameCount--;
    }
    return 0;
}

TEST_F(AmrwbDecoderTest, MultiCreateAmrwbDecoderTest) {
    uint32_t memRequirements = pvDecoder_AmrWbMemRequirements();
    void *decoderBuf = malloc(memRequirements);
    ASSERT_NE(decoderBuf, nullptr)
            << "Failed to allocate decoder memory of size " << memRequirements;

    // Create AMR-WB decoder instance.
    void *amrHandle = nullptr;
    int16_t *decoderCookie;
    for (int i = 0; i < kMaxCount; i++) {
        pvDecoder_AmrWb_Init(&amrHandle, decoderBuf, &decoderCookie);
        ASSERT_NE(amrHandle, nullptr) << "Failed to initialize decoder";
        ALOGV("Decoder created successfully");
    }
    if (decoderBuf) {
        free(decoderBuf);
        decoderBuf = nullptr;
    }
}

TEST_P(AmrwbDecoderTest, DecodeTest) {
    uint32_t memRequirements = pvDecoder_AmrWbMemRequirements();
    void *decoderBuf = malloc(memRequirements);
    ASSERT_NE(decoderBuf, nullptr)
            << "Failed to allocate decoder memory of size " << memRequirements;

    void *amrHandle = nullptr;
    int16_t *decoderCookie;
    pvDecoder_AmrWb_Init(&amrHandle, decoderBuf, &decoderCookie);
    ASSERT_NE(amrHandle, nullptr) << "Failed to initialize decoder";

    string inputFile = gEnv->getRes() + GetParam();
    mFpInput = fopen(inputFile.c_str(), "rb");
    ASSERT_NE(mFpInput, nullptr) << "Error opening input file " << inputFile;

    // Open the output file.
    SF_INFO sfInfo;
    SNDFILE *outFileHandle = openOutputFile(&sfInfo);
    ASSERT_NE(outFileHandle, nullptr) << "Error opening output file for writing decoded output";

    int32_t decoderErr = DecodeFrames(decoderCookie, decoderBuf, outFileHandle);
    ASSERT_EQ(decoderErr, 0) << "DecodeFrames returned error";

    sf_close(outFileHandle);
    if (decoderBuf) {
        free(decoderBuf);
        decoderBuf = nullptr;
    }
}

TEST_P(AmrwbDecoderTest, ResetDecoderTest) {
    uint32_t memRequirements = pvDecoder_AmrWbMemRequirements();
    void *decoderBuf = malloc(memRequirements);
    ASSERT_NE(decoderBuf, nullptr)
            << "Failed to allocate decoder memory of size " << memRequirements;

    void *amrHandle = nullptr;
    int16_t *decoderCookie;
    pvDecoder_AmrWb_Init(&amrHandle, decoderBuf, &decoderCookie);
    ASSERT_NE(amrHandle, nullptr) << "Failed to initialize decoder";

    string inputFile = gEnv->getRes() + GetParam();
    mFpInput = fopen(inputFile.c_str(), "rb");
    ASSERT_NE(mFpInput, nullptr) << "Error opening input file " << inputFile;

    // Open the output file.
    SF_INFO sfInfo;
    SNDFILE *outFileHandle = openOutputFile(&sfInfo);
    ASSERT_NE(outFileHandle, nullptr) << "Error opening output file for writing decoded output";

    // Decode 150 frames first
    int32_t decoderErr = DecodeFrames(decoderCookie, decoderBuf, outFileHandle, kNumFrameReset);
    ASSERT_EQ(decoderErr, 0) << "DecodeFrames returned error";

    // Reset Decoder
    pvDecoder_AmrWb_Reset(decoderBuf, 1);

    // Start decoding again
    decoderErr = DecodeFrames(decoderCookie, decoderBuf, outFileHandle);
    ASSERT_EQ(decoderErr, 0) << "DecodeFrames returned error";

    sf_close(outFileHandle);
    if (decoderBuf) {
        free(decoderBuf);
    }
}

INSTANTIATE_TEST_SUITE_P(AmrwbDecoderTestAll, AmrwbDecoderTest,
                         ::testing::Values(("bbb_amrwb_1ch_14kbps_16000hz.amrwb"),
                                           ("bbb_16000hz_1ch_9kbps_amrwb_30sec.amrwb")));

int main(int argc, char **argv) {
    gEnv = new AmrwbDecTestEnvironment();
    ::testing::AddGlobalTestEnvironment(gEnv);
    ::testing::InitGoogleTest(&argc, argv);
    int status = gEnv->initFromOptions(argc, argv);
    if (status == 0) {
        status = RUN_ALL_TESTS();
        ALOGV("Test result = %d\n", status);
    }
    return status;
}
