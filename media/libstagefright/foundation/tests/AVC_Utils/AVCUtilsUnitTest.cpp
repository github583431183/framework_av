/*
 * Copyright (C) 2020 The Android Open Source Project
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
#define LOG_TAG "AVCUtilsUnitTest"
#include <utils/Log.h>

#include <fstream>

#include "media/stagefright/foundation/ABitReader.h"
#include "media/stagefright/foundation/avc_utils.h"

#include "AVCUtilsTestEnvironment.h"

constexpr uint8_t kSPSmask = 0x1f;
constexpr uint8_t kSPSStartCode = 0x07;
constexpr uint8_t kConfigVersion = 0x01;

using namespace android;

static AVCUtilsTestEnvironment *gEnv = nullptr;

class MpegAudioUnitTest
    : public ::testing::TestWithParam<
              tuple</*audioHeader*/ uint32_t, /*frameSize*/ int32_t, /*sampleRate*/ int32_t,
                    /*numChannels*/ int32_t, /*bitRate*/ int32_t, /*numSamples*/ int32_t>> {};

class MpegVolDimensionTest
    : public ::testing::TestWithParam<
              tuple</*fileName*/ string, /*volWidth*/ int32_t, /*volHeight*/ int32_t>> {};

class AVCUtils {
  public:
    bool SetUpAVCUtils(string fileName, string infoFileName) {
        mInputFile = gEnv->getRes() + fileName;
        mInputFileStream.open(mInputFile, ifstream::in);
        if (!mInputFileStream.is_open()) return false;

        mInfoFile = gEnv->getRes() + infoFileName;
        mInfoFileStream.open(mInfoFile, ifstream::in);
        if (!mInputFileStream.is_open()) return false;
        return true;
    }

    ~AVCUtils() {
        if (mInputFileStream.is_open()) mInputFileStream.close();
        if (mInfoFileStream.is_open()) mInfoFileStream.close();
    }

    string mInputFile;
    string mInfoFile;

    ifstream mInputFileStream;
    ifstream mInfoFileStream;
};

class MpegAVCDimensionTest
    : public AVCUtils,
      public ::testing::TestWithParam<tuple</*fileName*/ string, /*infoFileName*/ string,
                                            /*avcWidth*/ size_t, /*avcHeight*/ size_t>> {
  public:
    virtual void SetUp() override {
        tuple<string, string, size_t, size_t> params = GetParam();
        string fileName = get<0>(params);
        string infoFileName = get<1>(params);
        AVCUtils::SetUpAVCUtils(fileName, infoFileName);

        mFrameWidth = get<2>(params);
        mFrameHeight = get<3>(params);
    }

    size_t mFrameWidth;
    size_t mFrameHeight;
};

class AvccBoxTest : public MpegAVCDimensionTest {
  public:
    virtual void SetUp() override { MpegAVCDimensionTest::SetUp(); }
};

class AVCFrameTest
    : public AVCUtils,
      public ::testing::TestWithParam<pair</*fileName*/ string, /*infoFileName*/ string>> {
  public:
    virtual void SetUp() override {
        string fileName = GetParam().first;
        string infoFileName = GetParam().second;
        AVCUtils::SetUpAVCUtils(fileName, infoFileName);
    }
};

TEST_P(MpegAudioUnitTest, AudioProfileTest) {
    tuple<uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t> params = GetParam();
    uint32_t header = get<0>(params);

    int32_t audioFrameSize = get<1>(params);
    int32_t audioSampleRate = get<2>(params);
    int32_t audioNumChannels = get<3>(params);
    int32_t audioBitRate = get<4>(params);
    int32_t audioNumSamples = get<5>(params);

    size_t frameSize = 0;
    int32_t sampleRate = 0;
    int32_t numChannels = 0;
    int32_t bitRate = 0;
    int32_t numSamples = 0;

    bool status = GetMPEGAudioFrameSize(header, &frameSize, &sampleRate, &numChannels, &bitRate,
                                        &numSamples);
    ASSERT_TRUE(status) << "Failed to get Audio properties";

    ASSERT_EQ(frameSize, audioFrameSize) << "Wrong frame size found";

    ASSERT_EQ(sampleRate, audioSampleRate) << "Wrong sample rate found";

    ASSERT_EQ(numChannels, audioNumChannels) << "Wrong number of channels found";

    ASSERT_EQ(bitRate, audioBitRate) << "Wrong bit rate found";

    ASSERT_EQ(numSamples, audioNumSamples) << "Wrong number of samples found";
}

TEST_P(MpegVolDimensionTest, VolDimensionTest) {
    tuple<string, int32_t, int32_t> params = GetParam();
    string inputFile = gEnv->getRes() + get<0>(params);
    ifstream inputFileStream;
    inputFileStream.open(inputFile, ifstream::in);
    ASSERT_TRUE(inputFileStream.is_open()) << "Failed to open: " << inputFile;

    struct stat buf;
    int8_t err = stat(inputFile.c_str(), &buf);
    ASSERT_EQ(err, 0) << "Failed to get information for file: " << inputFile;

    int16_t fileSize = buf.st_size;

    const uint8_t *volBuffer = new uint8_t[fileSize];
    ASSERT_NE(volBuffer, nullptr) << "Failed to allocate VOL buffer of size: " << fileSize;

    inputFileStream.read((char *)(volBuffer), fileSize);

    int32_t width = get<1>(params);
    int32_t height = get<2>(params);
    int32_t volWidth;
    int32_t volHeight;

    bool status = ExtractDimensionsFromVOLHeader(volBuffer, fileSize, &volWidth, &volHeight);
    delete[] volBuffer;
    ASSERT_TRUE(status)
            << "Failed to get VOL dimensions from function: ExtractDimensionsFromVOLHeader()";

    ASSERT_EQ(volWidth, width) << "Expected width: " << width << "Found: " << volWidth;

    ASSERT_EQ(volHeight, height) << "Expected height: " << height << "Found: " << volHeight;
}

TEST_P(MpegAVCDimensionTest, AVCDimensionTest) {
    int32_t numNalUnits = 0;
    int32_t avcWidth = 0;
    int32_t avcHeight = 0;
    string line;
    string type;
    size_t chunkLength;
    while (getline(mInfoFileStream, line)) {
        istringstream stringLine(line);
        stringLine >> type >> chunkLength;

        if (type.compare("SPS")) continue;

        const uint8_t *data = new uint8_t[chunkLength];
        ASSERT_NE(data, nullptr) << "Failed to create a data buffer of size: " << chunkLength;

        const uint8_t *nalStart;
        size_t nalSize;

        mInputFileStream.read((char *)data, (uint32_t)chunkLength);
        while (!getNextNALUnit(&data, &chunkLength, &nalStart, &nalSize, true)) {
            numNalUnits++;
            // Check if it's an SPS
            ASSERT_TRUE(nalSize > 0 && (nalStart[0] & kSPSmask) == kSPSStartCode)
                    << "Failed to get SPS";

            sp<ABuffer> spsBuffer = new ABuffer(nalSize);
            ASSERT_NE(spsBuffer, nullptr) << "ABuffer returned null for size: " << nalSize;

            memcpy(spsBuffer->data(), nalStart, nalSize);
            FindAVCDimensions(spsBuffer, &avcWidth, &avcHeight);
            spsBuffer.clear();
            ASSERT_EQ(avcWidth, mFrameWidth)
                    << "Expected width: " << mFrameWidth << "Found: " << avcWidth;

            ASSERT_EQ(avcHeight, mFrameHeight)
                    << "Expected height: " << mFrameHeight << "Found: " << avcHeight;
        }
        delete[] data;
    }
    ASSERT_GT(numNalUnits, 0) << "Failed to find a NAL Unit";
}

TEST_P(AvccBoxTest, AvccBoxValidationTest) {
    int32_t avcWidth = 0;
    int32_t avcHeight = 0;
    int32_t accessUnitLength = 0;
    uint8_t profile;
    uint8_t level;
    string line;
    string type;
    size_t chunkLength;
    while (getline(mInfoFileStream, line)) {
        istringstream stringLine(line);
        stringLine >> type >> chunkLength;

        if (type.compare("SPS") & type.compare("PPS")) continue;
        accessUnitLength += chunkLength;

        if (!type.compare("SPS")) {
            const uint8_t *data = new uint8_t[chunkLength];
            ASSERT_NE(data, nullptr) << "Failed to create a data buffer of size: " << chunkLength;

            const uint8_t *nalStart;
            size_t nalSize;

            mInputFileStream.read((char *)data, (uint32_t)chunkLength);
            while (!getNextNALUnit(&data, &chunkLength, &nalStart, &nalSize, true)) {
                // Check if it's an SPS
                ASSERT_TRUE(nalSize > 0 && (nalStart[0] & kSPSmask) == kSPSStartCode)
                        << "Failed to get SPS";

                profile = nalStart[1];
                level = nalStart[3];
            }
            delete[] data;
        }
    }
    const uint8_t *accessUnitData = new uint8_t[accessUnitLength];
    ASSERT_NE(accessUnitData, nullptr) << "Failed to create a buffer of size: " << accessUnitLength;

    mInputFileStream.seekg(0, ios::beg);
    mInputFileStream.read((char *)accessUnitData, accessUnitLength);
    sp<ABuffer> accessUnit = new ABuffer(accessUnitLength);
    ASSERT_NE(accessUnit, nullptr)
            << "Failed to create an android data buffer of size: " << accessUnitLength;

    memcpy(accessUnit->data(), accessUnitData, accessUnitLength);
    delete[] accessUnitData;
    sp<ABuffer> csdDataBuffer = MakeAVCCodecSpecificData(accessUnit, &avcWidth, &avcHeight);
    accessUnit.clear();
    ASSERT_NE(csdDataBuffer, nullptr) << "No data returned from MakeAVCCodecSpecificData()";

    ASSERT_EQ(avcWidth, mFrameWidth) << "Expected width: " << mFrameWidth << "Found: " << avcWidth;

    ASSERT_EQ(avcHeight, mFrameHeight)
            << "Expected height: " << mFrameHeight << "Found: " << avcHeight;

    uint8_t *csdData = csdDataBuffer->data();
    ASSERT_EQ(*csdData, kConfigVersion) << "Invalid configuration version";

    ASSERT_EQ(*(csdData + 1), profile) << "Invalid AVC profile";

    ASSERT_EQ(*(csdData + 3), level) << "Invalid AVC level";
    csdDataBuffer.clear();
}

TEST_P(AVCFrameTest, FrameTest) {
    string line;
    string type;
    int32_t chunkLength;
    int32_t frameLayerID;
    while (getline(mInfoFileStream, line)) {
        uint32_t layerID = 0;
        istringstream stringLine(line);
        stringLine >> type >> chunkLength >> frameLayerID;

        char *data = new char[chunkLength];
        ASSERT_NE(data, nullptr) << "Failed to allocation data buffer of size: " << chunkLength;

        mInputFileStream.read(data, chunkLength);

        if (!type.compare("IDR")) {
            bool isIDR = IsIDR((uint8_t *)data, chunkLength);
            ASSERT_TRUE(isIDR);

            layerID = FindAVCLayerId((uint8_t *)data, chunkLength);
            ASSERT_EQ(layerID, frameLayerID) << "Wrong layer ID found";
        } else if (!type.compare("P") || !type.compare("B")) {
            sp<ABuffer> accessUnit = new ABuffer(chunkLength);
            ASSERT_NE(accessUnit, nullptr) << "Unable to create access Unit";

            memcpy(accessUnit->data(), data, chunkLength);
            bool isReferenceFrame = IsAVCReferenceFrame(accessUnit);
            ASSERT_TRUE(isReferenceFrame);

            accessUnit.clear();
            layerID = FindAVCLayerId((uint8_t *)data, chunkLength);
            ASSERT_EQ(layerID, frameLayerID) << "Wrong layer ID found";
        }
        delete[] data;
    }
}

INSTANTIATE_TEST_SUITE_P(AVCUtilsTestAll, MpegAudioUnitTest,
                         ::testing::Values(make_tuple(0xFFFB9204, 418, 44100, 2, 128, 1152),
                                           make_tuple(0xFFFB7604, 289, 48000, 2, 96, 1152),
                                           make_tuple(0xFFFE5604, 164, 48000, 2, 160, 384)));

// Info File contains the type and length for each chunk/frame
INSTANTIATE_TEST_SUITE_P(
        AVCUtilsTestAll, MpegAVCDimensionTest,
        ::testing::Values(make_tuple("crowd_8x8p50f32_200kbps_bp.h264",
                                     "crowd_8x8p50f32_200kbps_bp.info", 8, 8),
                          make_tuple("crowd_640x360p24f300_1000kbps_bp.h264",
                                     "crowd_640x360p24f300_1000kbps_bp.info", 640, 360),
                          make_tuple("crowd_1280x720p30f300_5000kbps_bp.h264",
                                     "crowd_1280x720p30f300_5000kbps_bp.info", 1280, 720),
                          make_tuple("crowd_1920x1080p50f300_12000kbps_bp.h264",
                                     "crowd_1920x1080p50f300_12000kbps_bp.info", 1920, 1080),
                          make_tuple("crowd_3840x2160p60f300_68000kbps_bp.h264",
                                     "crowd_3840x2160p60f300_68000kbps_bp.info", 3840, 2160)));

// Info File contains the type and length for each chunk/frame
INSTANTIATE_TEST_SUITE_P(
        AVCUtilsTestAll, AvccBoxTest,
        ::testing::Values(make_tuple("crowd_8x8p50f32_200kbps_bp.h264",
                                     "crowd_8x8p50f32_200kbps_bp.info", 8, 8),
                          make_tuple("crowd_1280x720p30f300_5000kbps_bp.h264",
                                     "crowd_1280x720p30f300_5000kbps_bp.info", 1280, 720),
                          make_tuple("crowd_1920x1080p50f300_12000kbps_bp.h264",
                                     "crowd_1920x1080p50f300_12000kbps_bp.info", 1920, 1080)));

// Info File contains the type and length for each chunk/frame
INSTANTIATE_TEST_SUITE_P(AVCUtilsTestAll, MpegVolDimensionTest,
                         ::testing::Values(make_tuple("volData_720_480", 720, 480),
                                           make_tuple("volData_1280_720", 1280, 720),
                                           make_tuple("volData_1920_1080", 1920, 1080)));

// Info File contains the type, length and layer ID for each chunk/frame
INSTANTIATE_TEST_SUITE_P(AVCUtilsTestAll, AVCFrameTest,
                         ::testing::Values(make_tuple("crowd_8x8p50f32_200kbps_bp.h264",
                                                      "crowd_8x8p50f32_200kbps_bp.info"),
                                           make_tuple("crowd_640x360p24f300_1000kbps_bp.h264",
                                                      "crowd_640x360p24f300_1000kbps_bp.info"),
                                           make_tuple("crowd_1280x720p30f300_5000kbps_bp.h264",
                                                      "crowd_1280x720p30f300_5000kbps_bp.info"),
                                           make_tuple("crowd_1920x1080p50f300_12000kbps_bp.h264",
                                                      "crowd_1920x1080p50f300_12000kbps_bp.info"),
                                           make_tuple("crowd_3840x2160p60f300_68000kbps_bp.h264",
                                                      "crowd_3840x2160p60f300_68000kbps_bp.info")));

int main(int argc, char **argv) {
    gEnv = new AVCUtilsTestEnvironment();
    ::testing::AddGlobalTestEnvironment(gEnv);
    ::testing::InitGoogleTest(&argc, argv);
    int status = gEnv->initFromOptions(argc, argv);
    if (status == 0) {
        status = RUN_ALL_TESTS();
        ALOGV("Test result = %d\n", status);
    }
    return status;
}
