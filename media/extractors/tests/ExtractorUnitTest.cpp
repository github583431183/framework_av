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
#define LOG_TAG "ExtractorUnitTest"
#include <utils/Log.h>

#include <datasource/FileSource.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaDataUtils.h>
#include <media/stagefright/foundation/OpusHeader.h>

#include "aac/AACExtractor.h"
#include "amr/AMRExtractor.h"
#include "flac/FLACExtractor.h"
#include "midi/MidiExtractor.h"
#include "mkv/MatroskaExtractor.h"
#include "mp3/MP3Extractor.h"
#include "mp4/MPEG4Extractor.h"
#include "mp4/SampleTable.h"
#include "mpeg2/MPEG2PSExtractor.h"
#include "mpeg2/MPEG2TSExtractor.h"
#include "ogg/OggExtractor.h"
#include "wav/WAVExtractor.h"

#include "ExtractorUnitTestEnvironment.h"

using namespace android;

#define OUTPUT_DUMP_FILE "/data/local/tmp/extractorOutput"

constexpr int32_t kMaxCount = 10;
constexpr int32_t kAudioDefaultSampleDuration = 20000;                       // 20ms
constexpr int32_t kRandomSeekToleranceUs = 2 * kAudioDefaultSampleDuration;  // 40 ms;
constexpr int32_t kRandomSeed = 700;
constexpr int32_t kUndefined = -1;

// LookUpTable of clips and metadata for component testing
static const struct InputData {
    string mime;
    string inputFile;
    int32_t firstParam;
    int32_t secondParam;
    int32_t profile;
    int32_t frameRate;
} kInputData[] = {
        {MEDIA_MIMETYPE_AUDIO_AAC, "test_mono_44100Hz_aac.aac", 44100, 1, AACObjectLC, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_AMR_NB, "bbb_mono_8kHz_amrnb.amr", 8000, 1, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_AMR_WB, "bbb_mono_16kHz_amrwb.amr", 16000, 1, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_VORBIS, "bbb_stereo_48kHz_vorbis.ogg", 48000, 2, kUndefined,
         kUndefined},
        {MEDIA_MIMETYPE_AUDIO_MSGSM, "test_mono_8kHz_gsm.wav", 8000, 1, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_RAW, "bbb_stereo_48kHz_flac.flac", 48000, 2, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_OPUS, "test_stereo_48kHz_opus.opus", 48000, 2, kUndefined,
         kUndefined},
        {MEDIA_MIMETYPE_AUDIO_MPEG, "bbb_stereo_48kHz_mp3.mp3", 48000, 2, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_AUDIO_RAW, "midi_a.mid", 22050, 2, kUndefined, kUndefined},
        {MEDIA_MIMETYPE_VIDEO_MPEG2, "bbb_cif_768kbps_30fps_mpeg2.ts", 352, 288, MPEG2ProfileMain,
         30},
        {MEDIA_MIMETYPE_VIDEO_MPEG4, "bbb_cif_768kbps_30fps_mpeg4.mkv", 352, 288,
         MPEG4ProfileSimple, 30},
        // Test (b/151677264) for MP4 extractor
        {MEDIA_MIMETYPE_VIDEO_HEVC, "crowd_508x240_25fps_hevc.mp4", 508, 240, HEVCProfileMain,
         25},
        {MEDIA_MIMETYPE_VIDEO_VP9, "bbb_340x280_30fps_vp9.webm", 340, 280, VP9Profile0, 30},
        {MEDIA_MIMETYPE_VIDEO_MPEG2, "swirl_144x136_mpeg2.mpg", 144, 136, MPEG2ProfileMain, 12},
};

static ExtractorUnitTestEnvironment *gEnv = nullptr;

class ExtractorUnitTest {
  public:
    ExtractorUnitTest() : mInputFp(nullptr), mDataSource(nullptr), mExtractor(nullptr) {}

    ~ExtractorUnitTest() {
        if (mInputFp) {
            fclose(mInputFp);
            mInputFp = nullptr;
        }
        if (mDataSource) {
            mDataSource.clear();
            mDataSource = nullptr;
        }
        if (mExtractor) {
            delete mExtractor;
            mExtractor = nullptr;
        }
    }

    void setupExtractor(string writerFormat) {
        mExtractorName = unknown_comp;
        mDisableTest = false;

        static const std::map<std::string, standardExtractors> mapExtractor = {
                {"aac", AAC},     {"amr", AMR},         {"mp3", MP3},         {"ogg", OGG},
                {"wav", WAV},     {"mkv", MKV},         {"flac", FLAC},       {"midi", MIDI},
                {"mpeg4", MPEG4}, {"mpeg2ts", MPEG2TS}, {"mpeg2ps", MPEG2PS}, {"mp4", MPEG4},
                {"webm", MKV},    {"ts", MPEG2TS},      {"mpeg", MPEG2PS}};
        // Find the component type
        if (mapExtractor.find(writerFormat) != mapExtractor.end()) {
            mExtractorName = mapExtractor.at(writerFormat);
        }
        if (mExtractorName == standardExtractors::unknown_comp) {
            cout << "[   WARN   ] Test Skipped. Invalid extractor\n";
            mDisableTest = true;
        }
    }

    int32_t setDataSource(string inputFileName);

    int32_t createExtractor();

    enum standardExtractors {
        AAC,
        AMR,
        FLAC,
        MIDI,
        MKV,
        MP3,
        MPEG4,
        MPEG2PS,
        MPEG2TS,
        OGG,
        WAV,
        unknown_comp,
    };

    bool mDisableTest;
    standardExtractors mExtractorName;

    FILE *mInputFp;
    sp<DataSource> mDataSource;
    MediaExtractorPluginHelper *mExtractor;
};

class ExtractorFunctionalityTest : public ExtractorUnitTest,
                                   public ::testing::TestWithParam<pair<string, string>> {
  public:
    virtual void SetUp() override { setupExtractor(GetParam().first); }
};

class ConfigParamTest : public ExtractorUnitTest,
                        public ::testing::TestWithParam<pair<string, int32_t>> {
  public:
    virtual void SetUp() override { setupExtractor(GetParam().first); }

    struct configFormat {
        string mime;
        int32_t width;
        int32_t height;
        int32_t sampleRate;
        int32_t channelCount;
        int32_t profile;
        int32_t frameRate;
    };

    void getFileProperties(int32_t inputIdx, string &inputFile, configFormat &configParam);
};

int32_t ExtractorUnitTest::setDataSource(string inputFileName) {
    mInputFp = fopen(inputFileName.c_str(), "rb");
    if (!mInputFp) {
        ALOGE("Unable to open input file for reading");
        return -1;
    }
    struct stat buf;
    stat(inputFileName.c_str(), &buf);
    int32_t fd = fileno(mInputFp);
    mDataSource = new FileSource(dup(fd), 0, buf.st_size);
    if (!mDataSource) return -1;
    return 0;
}

int32_t ExtractorUnitTest::createExtractor() {
    switch (mExtractorName) {
        case AAC:
            mExtractor = new AACExtractor(new DataSourceHelper(mDataSource->wrap()), 0);
            break;
        case AMR:
            mExtractor = new AMRExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MP3:
            mExtractor = new MP3Extractor(new DataSourceHelper(mDataSource->wrap()), nullptr);
            break;
        case OGG:
            mExtractor = new OggExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case WAV:
            mExtractor = new WAVExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MKV:
            mExtractor = new MatroskaExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case FLAC:
            mExtractor = new FLACExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MPEG4:
            mExtractor = new MPEG4Extractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MPEG2TS:
            mExtractor = new MPEG2TSExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MPEG2PS:
            mExtractor = new MPEG2PSExtractor(new DataSourceHelper(mDataSource->wrap()));
            break;
        case MIDI:
            mExtractor = new MidiExtractor(mDataSource->wrap());
            break;
        default:
            return -1;
    }
    if (!mExtractor) return -1;
    return 0;
}

void ConfigParamTest::getFileProperties(int32_t inputIdx, string &inputFile,
                                        configFormat &configParam) {
    if (inputIdx >= sizeof(kInputData) / sizeof(kInputData[0])) {
        return;
    }
    inputFile += kInputData[inputIdx].inputFile;
    configParam.mime = kInputData[inputIdx].mime;
    size_t found = configParam.mime.find("audio/");
    // Check if 'audio/' is present in the begininig of the mime type
    if (found == 0) {
        configParam.sampleRate = kInputData[inputIdx].firstParam;
        configParam.channelCount = kInputData[inputIdx].secondParam;
    } else {
        configParam.width = kInputData[inputIdx].firstParam;
        configParam.height = kInputData[inputIdx].secondParam;
    }
    configParam.profile = kInputData[inputIdx].profile;
    configParam.frameRate = kInputData[inputIdx].frameRate;
    return;
}

void randomSeekTest(MediaTrackHelper *track, int64_t clipDuration) {
    int32_t status = 0;
    int32_t seekCount = 0;
    bool hasTimestamp = false;
    vector<int64_t> seekToTimeStamp;
    string seekPtsString;

    srand(kRandomSeed);
    while (seekCount < kMaxCount) {
        int64_t timeStamp = ((double)rand() / RAND_MAX) * clipDuration;
        seekToTimeStamp.push_back(timeStamp);
        seekPtsString.append(to_string(timeStamp));
        seekPtsString.append(", ");
        seekCount++;
    }

    for (int64_t seekPts : seekToTimeStamp) {
        MediaTrackHelper::ReadOptions *options = new MediaTrackHelper::ReadOptions(
                CMediaTrackReadOptions::SEEK_CLOSEST | CMediaTrackReadOptions::SEEK, seekPts);
        ASSERT_NE(options, nullptr) << "Cannot create read option";

        MediaBufferHelper *buffer = nullptr;
        status = track->read(&buffer, options);
        if (buffer) {
            AMediaFormat *metaData = buffer->meta_data();
            int64_t timeStamp = 0;
            hasTimestamp = AMediaFormat_getInt64(metaData, AMEDIAFORMAT_KEY_TIME_US, &timeStamp);
            ASSERT_TRUE(hasTimestamp) << "Extractor didn't set timestamp for the given sample";

            buffer->release();
            EXPECT_LE(abs(timeStamp - seekPts), kRandomSeekToleranceUs)
                    << "Seek unsuccessful. Expected timestamp range ["
                    << seekPts - kRandomSeekToleranceUs << ", " << seekPts + kRandomSeekToleranceUs
                    << "] "
                    << "received " << timeStamp << ", list of input seek timestamps ["
                    << seekPtsString << "]";
        }
        delete options;
    }
}

void getSeekablePoints(vector<int64_t> &seekablePoints, MediaTrackHelper *track) {
    int32_t status = 0;
    if (!seekablePoints.empty()) {
        seekablePoints.clear();
    }
    int64_t timeStamp;
    while (status != AMEDIA_ERROR_END_OF_STREAM) {
        MediaBufferHelper *buffer = nullptr;
        status = track->read(&buffer);
        if (buffer) {
            AMediaFormat *metaData = buffer->meta_data();
            int32_t isSync = 0;
            AMediaFormat_getInt32(metaData, AMEDIAFORMAT_KEY_IS_SYNC_FRAME, &isSync);
            if (isSync) {
                AMediaFormat_getInt64(metaData, AMEDIAFORMAT_KEY_TIME_US, &timeStamp);
                seekablePoints.push_back(timeStamp);
            }
            buffer->release();
        }
    }
}

TEST_P(ExtractorFunctionalityTest, CreateExtractorTest) {
    if (mDisableTest) return;

    ALOGV("Checks if a valid extractor is created for a given input file");
    string inputFileName = gEnv->getRes() + GetParam().second;

    ASSERT_EQ(setDataSource(inputFileName), 0)
            << "SetDataSource failed for" << GetParam().first << "extractor";

    ASSERT_EQ(createExtractor(), 0)
            << "Extractor creation failed for" << GetParam().first << "extractor";

    // A valid extractor instace should return success for following calls
    ASSERT_GT(mExtractor->countTracks(), 0);

    AMediaFormat *format = AMediaFormat_new();
    ASSERT_NE(format, nullptr) << "AMediaFormat_new returned null AMediaformat";

    ASSERT_EQ(mExtractor->getMetaData(format), AMEDIA_OK);
    AMediaFormat_delete(format);
}

TEST_P(ExtractorFunctionalityTest, ExtractorTest) {
    if (mDisableTest) return;

    ALOGV("Validates %s Extractor for a given input file", GetParam().first.c_str());
    string inputFileName = gEnv->getRes() + GetParam().second;

    int32_t status = setDataSource(inputFileName);
    ASSERT_EQ(status, 0) << "SetDataSource failed for" << GetParam().first << "extractor";

    status = createExtractor();
    ASSERT_EQ(status, 0) << "Extractor creation failed for" << GetParam().first << "extractor";

    int32_t numTracks = mExtractor->countTracks();
    ASSERT_GT(numTracks, 0) << "Extractor didn't find any track for the given clip";

    for (int32_t idx = 0; idx < numTracks; idx++) {
        MediaTrackHelper *track = mExtractor->getTrack(idx);
        ASSERT_NE(track, nullptr) << "Failed to get track for index " << idx;

        CMediaTrack *cTrack = wrap(track);
        ASSERT_NE(cTrack, nullptr) << "Failed to get track wrapper for index " << idx;

        MediaBufferGroup *bufferGroup = new MediaBufferGroup();
        status = cTrack->start(track, bufferGroup->wrap());
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to start the track";

        FILE *outFp = fopen((OUTPUT_DUMP_FILE + to_string(idx)).c_str(), "wb");
        if (!outFp) {
            ALOGW("Unable to open output file for dumping extracted stream");
        }

        while (status != AMEDIA_ERROR_END_OF_STREAM) {
            MediaBufferHelper *buffer = nullptr;
            status = track->read(&buffer);
            ALOGV("track->read Status = %d buffer %p", status, buffer);
            if (buffer) {
                ALOGV("buffer->data %p buffer->size() %zu buffer->range_length() %zu",
                      buffer->data(), buffer->size(), buffer->range_length());
                if (outFp) fwrite(buffer->data(), 1, buffer->range_length(), outFp);
                buffer->release();
            }
        }
        if (outFp) fclose(outFp);
        status = cTrack->stop(track);
        ASSERT_EQ(OK, status) << "Failed to stop the track";
        delete bufferGroup;
        delete track;
    }
}

TEST_P(ExtractorFunctionalityTest, MetaDataComparisonTest) {
    if (mDisableTest) return;

    ALOGV("Validates Extractor's meta data for a given input file");
    string inputFileName = gEnv->getRes() + GetParam().second;

    int32_t status = setDataSource(inputFileName);
    ASSERT_EQ(status, 0) << "SetDataSource failed for" << GetParam().first << "extractor";

    status = createExtractor();
    ASSERT_EQ(status, 0) << "Extractor creation failed for" << GetParam().first << "extractor";

    int32_t numTracks = mExtractor->countTracks();
    ASSERT_GT(numTracks, 0) << "Extractor didn't find any track for the given clip";

    AMediaFormat *extractorFormat = AMediaFormat_new();
    ASSERT_NE(extractorFormat, nullptr) << "AMediaFormat_new returned null AMediaformat";
    AMediaFormat *trackFormat = AMediaFormat_new();
    ASSERT_NE(trackFormat, nullptr) << "AMediaFormat_new returned null AMediaformat";

    for (int32_t idx = 0; idx < numTracks; idx++) {
        MediaTrackHelper *track = mExtractor->getTrack(idx);
        ASSERT_NE(track, nullptr) << "Failed to get track for index " << idx;

        CMediaTrack *cTrack = wrap(track);
        ASSERT_NE(cTrack, nullptr) << "Failed to get track wrapper for index " << idx;

        MediaBufferGroup *bufferGroup = new MediaBufferGroup();
        status = cTrack->start(track, bufferGroup->wrap());
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to start the track";

        status = mExtractor->getTrackMetaData(extractorFormat, idx, 1);
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to get trackMetaData";

        status = track->getFormat(trackFormat);
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to get track meta data";

        const char *extractorMime, *trackMime;
        AMediaFormat_getString(extractorFormat, AMEDIAFORMAT_KEY_MIME, &extractorMime);
        AMediaFormat_getString(trackFormat, AMEDIAFORMAT_KEY_MIME, &trackMime);
        ASSERT_TRUE(!strcmp(extractorMime, trackMime))
                << "Extractor's format doesn't match track format";

        if (!strncmp(extractorMime, "audio/", 6)) {
            int32_t exSampleRate, exChannelCount;
            int32_t trackSampleRate, trackChannelCount;
            ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                              &exChannelCount));
            ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                              &exSampleRate));
            ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                              &trackChannelCount));
            ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                              &trackSampleRate));
            ASSERT_EQ(exChannelCount, trackChannelCount) << "ChannelCount not as expected";
            ASSERT_EQ(exSampleRate, trackSampleRate) << "SampleRate not as expected";
        } else {
            int32_t exWidth, exHeight;
            int32_t trackWidth, trackHeight;
            ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat, AMEDIAFORMAT_KEY_WIDTH, &exWidth));
            ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat, AMEDIAFORMAT_KEY_HEIGHT, &exHeight));
            ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_WIDTH, &trackWidth));
            ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_HEIGHT, &trackHeight));
            ASSERT_EQ(exWidth, trackWidth) << "Width not as expected";
            ASSERT_EQ(exHeight, trackHeight) << "Height not as expected";
        }
        status = cTrack->stop(track);
        ASSERT_EQ(OK, status) << "Failed to stop the track";
        delete bufferGroup;
        delete track;
    }
    AMediaFormat_delete(trackFormat);
    AMediaFormat_delete(extractorFormat);
}

TEST_P(ExtractorFunctionalityTest, MultipleStartStopTest) {
    if (mDisableTest) return;

    ALOGV("Test %s extractor for multiple start and stop calls", GetParam().first.c_str());
    string inputFileName = gEnv->getRes() + GetParam().second;

    int32_t status = setDataSource(inputFileName);
    ASSERT_EQ(status, 0) << "SetDataSource failed for" << GetParam().first << "extractor";

    status = createExtractor();
    ASSERT_EQ(status, 0) << "Extractor creation failed for" << GetParam().first << "extractor";

    int32_t numTracks = mExtractor->countTracks();
    ASSERT_GT(numTracks, 0) << "Extractor didn't find any track for the given clip";

    // start/stop the tracks multiple times
    for (int32_t count = 0; count < kMaxCount; count++) {
        for (int32_t idx = 0; idx < numTracks; idx++) {
            MediaTrackHelper *track = mExtractor->getTrack(idx);
            ASSERT_NE(track, nullptr) << "Failed to get track for index " << idx;

            CMediaTrack *cTrack = wrap(track);
            ASSERT_NE(cTrack, nullptr) << "Failed to get track wrapper for index " << idx;

            MediaBufferGroup *bufferGroup = new MediaBufferGroup();
            status = cTrack->start(track, bufferGroup->wrap());
            ASSERT_EQ(OK, (media_status_t)status) << "Failed to start the track";
            MediaBufferHelper *buffer = nullptr;
            status = track->read(&buffer);
            if (buffer) {
                ALOGV("buffer->data %p buffer->size() %zu buffer->range_length() %zu",
                      buffer->data(), buffer->size(), buffer->range_length());
                buffer->release();
            }
            status = cTrack->stop(track);
            ASSERT_EQ(OK, status) << "Failed to stop the track";
            delete bufferGroup;
            delete track;
        }
    }
}

TEST_P(ExtractorFunctionalityTest, SeekTest) {
    if (mDisableTest) return;

    ALOGV("Validates %s Extractor behaviour for different seek modes", GetParam().first.c_str());
    string inputFileName = gEnv->getRes() + GetParam().second;

    int32_t status = setDataSource(inputFileName);
    ASSERT_EQ(status, 0) << "SetDataSource failed for" << GetParam().first << "extractor";

    status = createExtractor();
    ASSERT_EQ(status, 0) << "Extractor creation failed for" << GetParam().first << "extractor";

    int32_t numTracks = mExtractor->countTracks();
    ASSERT_GT(numTracks, 0) << "Extractor didn't find any track for the given clip";

    uint32_t seekFlag = mExtractor->flags();
    if (!(seekFlag & MediaExtractorPluginHelper::CAN_SEEK)) {
        cout << "[   WARN   ] Test Skipped. " << GetParam().first
             << " Extractor doesn't support seek\n";
        return;
    }

    vector<int64_t> seekablePoints;
    for (int32_t idx = 0; idx < numTracks; idx++) {
        MediaTrackHelper *track = mExtractor->getTrack(idx);
        ASSERT_NE(track, nullptr) << "Failed to get track for index " << idx;

        CMediaTrack *cTrack = wrap(track);
        ASSERT_NE(cTrack, nullptr) << "Failed to get track wrapper for index " << idx;

        // Get all the seekable points of a given input
        MediaBufferGroup *bufferGroup = new MediaBufferGroup();
        status = cTrack->start(track, bufferGroup->wrap());
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to start the track";

        // For Flac, Wav and Midi extractor, all samples are seek points.
        // We cannot create list of all seekable points for these.
        // This means that if we pass a seekToTimeStamp between two seek points, we may
        // end up getting the timestamp of next sample as a seekable timestamp.
        // This timestamp may/may not be a part of the seekable point vector thereby failing the
        // test. So we test these extractors using random seek test.
        if (mExtractorName == FLAC || mExtractorName == WAV || mExtractorName == MIDI) {
            AMediaFormat *trackMeta = AMediaFormat_new();
            ASSERT_NE(trackMeta, nullptr) << "AMediaFormat_new returned null AMediaformat";

            status = mExtractor->getTrackMetaData(trackMeta, idx, 1);
            ASSERT_EQ(OK, (media_status_t)status) << "Failed to get trackMetaData";

            int64_t clipDuration = 0;
            AMediaFormat_getInt64(trackMeta, AMEDIAFORMAT_KEY_DURATION, &clipDuration);
            ASSERT_GT(clipDuration, 0) << "Invalid clip duration ";
            randomSeekTest(track, clipDuration);
            AMediaFormat_delete(trackMeta);
            continue;
        }
        // Request seekable points for remaining extractors which will be used to validate the seek
        // accuracy for the extractors. Depending on SEEK Mode, we expect the extractors to return
        // the expected sync frame. We don't prefer random seek test for these extractors because
        // they aren't expected to seek to random samples. MP4 for instance can seek to
        // next/previous sync frames but not to samples between two sync frames.
        getSeekablePoints(seekablePoints, track);
        ASSERT_GT(seekablePoints.size(), 0)
                << "Failed to get seekable points for " << GetParam().first << " extractor";

        AMediaFormat *trackFormat = AMediaFormat_new();
        ASSERT_NE(trackFormat, nullptr) << "AMediaFormat_new returned null format";
        status = track->getFormat(trackFormat);
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to get track meta data";

        bool isOpus = false;
        int64_t opusSeekPreRollUs = 0;
        const char *mime;
        AMediaFormat_getString(trackFormat, AMEDIAFORMAT_KEY_MIME, &mime);
        if (!strcmp(mime, "audio/opus")) {
            isOpus = true;
            void *seekPreRollBuf = nullptr;
            size_t size = 0;
            if (!AMediaFormat_getBuffer(trackFormat, "csd-2", &seekPreRollBuf, &size)) {
                size_t opusHeadSize = 0;
                size_t codecDelayBufSize = 0;
                size_t seekPreRollBufSize = 0;
                void *csdBuffer = nullptr;
                void *opusHeadBuf = nullptr;
                void *codecDelayBuf = nullptr;
                AMediaFormat_getBuffer(trackFormat, "csd-0", &csdBuffer, &size);
                ASSERT_NE(csdBuffer, nullptr);

                GetOpusHeaderBuffers((uint8_t *)csdBuffer, size, &opusHeadBuf, &opusHeadSize,
                                     &codecDelayBuf, &codecDelayBufSize, &seekPreRollBuf,
                                     &seekPreRollBufSize);
            }
            ASSERT_NE(seekPreRollBuf, nullptr)
                    << "Invalid track format. SeekPreRoll info missing for Opus file";
            opusSeekPreRollUs = *((int64_t *)seekPreRollBuf);
        }
        AMediaFormat_delete(trackFormat);

        int32_t seekIdx = 0;
        size_t seekablePointsSize = seekablePoints.size();
        for (int32_t mode = CMediaTrackReadOptions::SEEK_PREVIOUS_SYNC;
             mode <= CMediaTrackReadOptions::SEEK_CLOSEST; mode++) {
            for (int32_t seekCount = 0; seekCount < kMaxCount; seekCount++) {
                seekIdx = rand() % seekablePointsSize + 1;
                if (seekIdx >= seekablePointsSize) seekIdx = seekablePointsSize - 1;

                int64_t seekToTimeStamp = seekablePoints[seekIdx];
                if (seekablePointsSize > 1) {
                    int64_t prevTimeStamp = seekablePoints[seekIdx - 1];
                    seekToTimeStamp = seekToTimeStamp - ((seekToTimeStamp - prevTimeStamp) >> 3);
                }

                // Opus has a seekPreRollUs. TimeStamp returned by the
                // extractor is calculated based on (seekPts - seekPreRollUs).
                // So we add the preRoll value to the timeStamp we want to seek to.
                if (isOpus) {
                    seekToTimeStamp += opusSeekPreRollUs;
                }

                MediaTrackHelper::ReadOptions *options = new MediaTrackHelper::ReadOptions(
                        mode | CMediaTrackReadOptions::SEEK, seekToTimeStamp);
                ASSERT_NE(options, nullptr) << "Cannot create read option";

                MediaBufferHelper *buffer = nullptr;
                status = track->read(&buffer, options);
                if (status == AMEDIA_ERROR_END_OF_STREAM) {
                    delete options;
                    continue;
                }
                if (buffer) {
                    AMediaFormat *metaData = buffer->meta_data();
                    int64_t timeStamp;
                    AMediaFormat_getInt64(metaData, AMEDIAFORMAT_KEY_TIME_US, &timeStamp);
                    buffer->release();

                    // CMediaTrackReadOptions::SEEK is 8. Using mask 0111b to get true modes
                    switch (mode & 0x7) {
                        case CMediaTrackReadOptions::SEEK_PREVIOUS_SYNC:
                            if (seekablePointsSize == 1) {
                                EXPECT_EQ(timeStamp, seekablePoints[seekIdx]);
                            } else {
                                EXPECT_EQ(timeStamp, seekablePoints[seekIdx - 1]);
                            }
                            break;
                        case CMediaTrackReadOptions::SEEK_NEXT_SYNC:
                        case CMediaTrackReadOptions::SEEK_CLOSEST_SYNC:
                        case CMediaTrackReadOptions::SEEK_CLOSEST:
                            EXPECT_EQ(timeStamp, seekablePoints[seekIdx]);
                            break;
                        default:
                            break;
                    }
                }
                delete options;
            }
        }
        status = cTrack->stop(track);
        ASSERT_EQ(OK, status) << "Failed to stop the track";
        delete bufferGroup;
        delete track;
    }
    seekablePoints.clear();
}

// This test validates config params for a given input file.
// For this test we only take single track files since the focus of this test is
// to validate the file properties reported by Extractor and not multi-track behavior
TEST_P(ConfigParamTest, ConfigParamValidation) {
    if (mDisableTest) return;

    ALOGV("Validates %s Extractor for input's file properties", GetParam().first.c_str());
    string inputFileName = gEnv->getRes();
    int32_t inputFileIdx = GetParam().second;
    configFormat configParam;
    getFileProperties(inputFileIdx, inputFileName, configParam);

    int32_t status = setDataSource(inputFileName);
    ASSERT_EQ(status, 0) << "SetDataSource failed for " << GetParam().first << "extractor";

    status = createExtractor();
    ASSERT_EQ(status, 0) << "Extractor creation failed for " << GetParam().first << "extractor";

    int32_t numTracks = mExtractor->countTracks();
    ASSERT_GT(numTracks, 0) << "Extractor didn't find any track for the given clip";

    MediaTrackHelper *track = mExtractor->getTrack(0);
    ASSERT_NE(track, nullptr) << "Failed to get track for index 0";

    AMediaFormat *trackFormat = AMediaFormat_new();
    ASSERT_NE(trackFormat, nullptr) << "AMediaFormat_new returned null format";

    status = track->getFormat(trackFormat);
    ASSERT_EQ(OK, (media_status_t)status) << "Failed to get track meta data";

    const char *trackMime;
    bool valueFound = AMediaFormat_getString(trackFormat, AMEDIAFORMAT_KEY_MIME, &trackMime);
    ASSERT_TRUE(valueFound) << "Mime type not set by extractor";
    ASSERT_STREQ(configParam.mime.c_str(), trackMime) << "Invalid track format";

    if (!strncmp(trackMime, "audio/", 6)) {
        int32_t trackSampleRate, trackChannelCount;
        ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                          &trackChannelCount));
        ASSERT_TRUE(
                AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &trackSampleRate));
        ASSERT_EQ(configParam.sampleRate, trackSampleRate) << "SampleRate not as expected";
        ASSERT_EQ(configParam.channelCount, trackChannelCount) << "ChannelCount not as expected";
    } else {
        int32_t trackWidth, trackHeight;
        ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_WIDTH, &trackWidth));
        ASSERT_TRUE(AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_HEIGHT, &trackHeight));
        ASSERT_EQ(configParam.width, trackWidth) << "Width not as expected";
        ASSERT_EQ(configParam.height, trackHeight) << "Height not as expected";

        if (configParam.frameRate != kUndefined) {
            int32_t frameRate;
            ASSERT_TRUE(
                    AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_FRAME_RATE, &frameRate));
            ASSERT_EQ(configParam.frameRate, frameRate) << "frameRate not as expected";
        }
    }
    // validate the profile for the input clip
    int32_t profile;
    if (configParam.profile != kUndefined) {
        if (AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_PROFILE, &profile)) {
            ASSERT_EQ(configParam.profile, profile) << "profile not as expected";
        } else if (mExtractorName == AAC &&
                   AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_AAC_PROFILE, &profile)) {
            ASSERT_EQ(configParam.profile, profile) << "profile not as expected";
        } else {
            ASSERT_TRUE(false) << "profile not returned in extractor";
        }
    }

    delete track;
    AMediaFormat_delete(trackFormat);
}

class ExtractorComparison
    : public ExtractorUnitTest,
      public ::testing::TestWithParam<pair<string /* InputFile0 */, string /* InputFile1 */>> {
  public:
    ~ExtractorComparison() {
        for (int8_t *extractorOp : mExtractorOutput) {
            if (extractorOp != nullptr) {
                free(extractorOp);
            }
        }
    }

    virtual void SetUp() override {
        string input0 = GetParam().first;
        string input1 = GetParam().second;

        // Allocate memory to hold extracted data for both extractors
        struct stat buf;
        int32_t status = stat((gEnv->getRes() + input0).c_str(), &buf);
        ASSERT_EQ(status, 0) << "Unable to get file properties";

        // allocating the buffer size as 2x since some
        // extractors like flac, midi and wav decodes the file.
        mExtractorOutput[0] = (int8_t *)calloc(1, buf.st_size * 2);
        ASSERT_NE(mExtractorOutput[0], nullptr)
                << "Unable to allocate memory for writing extractor's output";
        mExtractorOuputSize[0] = buf.st_size * 2;

        status = stat((gEnv->getRes() + input1).c_str(), &buf);
        ASSERT_EQ(status, 0) << "Unable to get file properties";

        // allocate buffer for extractor output, 2x input file size.
        mExtractorOutput[1] = (int8_t *)calloc(1, buf.st_size * 2);
        ASSERT_NE(mExtractorOutput[1], nullptr)
                << "Unable to allocate memory for writing extractor's output";
        mExtractorOuputSize[1] = buf.st_size * 2;
    }

    int8_t *mExtractorOutput[2]{};
    size_t mExtractorOuputSize[2]{};
};

// Compare output of two extractors for identical content
TEST_P(ExtractorComparison, ExtractorComparisonTest) {
    vector<string> inputFileNames = {GetParam().first, GetParam().second};
    size_t extractedOutputSize[2]{};
    AMediaFormat *extractorFormat[2]{};
    int32_t status = OK;

    for (int32_t idx = 0; idx < inputFileNames.size(); idx++) {
        string containerFormat = inputFileNames[idx].substr(inputFileNames[idx].find(".") + 1);
        setupExtractor(containerFormat);
        if (mDisableTest) {
            ALOGV("Unknown extractor %s. Skipping the test", containerFormat.c_str());
            return;
        }

        ALOGV("Validates %s Extractor for %s", containerFormat.c_str(),
              inputFileNames[idx].c_str());
        string inputFileName = gEnv->getRes() + inputFileNames[idx];

        status = setDataSource(inputFileName);
        ASSERT_EQ(status, 0) << "SetDataSource failed for" << containerFormat << "extractor";

        status = createExtractor();
        ASSERT_EQ(status, 0) << "Extractor creation failed for " << containerFormat << " extractor";

        int32_t numTracks = mExtractor->countTracks();
        ASSERT_EQ(numTracks, 1) << "This test expects inputs with one track only";

        int32_t trackIdx = 0;
        MediaTrackHelper *track = mExtractor->getTrack(trackIdx);
        ASSERT_NE(track, nullptr) << "Failed to get track for index " << trackIdx;

        extractorFormat[idx] = AMediaFormat_new();
        ASSERT_NE(extractorFormat[idx], nullptr) << "AMediaFormat_new returned null AMediaformat";

        status = track->getFormat(extractorFormat[idx]);
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to get track meta data";

        CMediaTrack *cTrack = wrap(track);
        ASSERT_NE(cTrack, nullptr) << "Failed to get track wrapper for index " << trackIdx;

        MediaBufferGroup *bufferGroup = new MediaBufferGroup();
        status = cTrack->start(track, bufferGroup->wrap());
        ASSERT_EQ(OK, (media_status_t)status) << "Failed to start the track";

        int32_t offset = 0;
        while (status != AMEDIA_ERROR_END_OF_STREAM) {
            MediaBufferHelper *buffer = nullptr;
            status = track->read(&buffer);
            ALOGV("track->read Status = %d buffer %p", status, buffer);
            if (buffer) {
                ASSERT_LE(offset + buffer->range_length(), mExtractorOuputSize[idx])
                        << "Memory overflow. Extracted output size more than expected";

                memcpy(mExtractorOutput[idx] + offset, buffer->data(), buffer->range_length());
                extractedOutputSize[idx] += buffer->range_length();
                offset += buffer->range_length();
                buffer->release();
            }
        }
        status = cTrack->stop(track);
        ASSERT_EQ(OK, status) << "Failed to stop the track";

        fclose(mInputFp);
        delete bufferGroup;
        delete track;
        mDataSource.clear();
        delete mExtractor;
        mInputFp = nullptr;
        mExtractor = nullptr;
    }

    // Compare the meta data from both the extractors
    const char *mime[2];
    AMediaFormat_getString(extractorFormat[0], AMEDIAFORMAT_KEY_MIME, &mime[0]);
    AMediaFormat_getString(extractorFormat[1], AMEDIAFORMAT_KEY_MIME, &mime[1]);
    ASSERT_STREQ(mime[0], mime[1]) << "Mismatch between extractor's format";

    if (!strncmp(mime[0], "audio/", 6)) {
        int32_t channelCount0, channelCount1;
        int32_t sampleRate0, sampleRate1;
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[0], AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                          &channelCount0));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[0], AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                          &sampleRate0));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[1], AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                          &channelCount1));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[1], AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                          &sampleRate1));
        ASSERT_EQ(channelCount0, channelCount1) << "Mismatch between extractor's channelCount";
        ASSERT_EQ(sampleRate0, sampleRate1) << "Mismatch between extractor's sampleRate";
    } else if (!strncmp(mime[0], "video/", 6)) {
        int32_t width0, height0;
        int32_t width1, height1;
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[0], AMEDIAFORMAT_KEY_WIDTH, &width0));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[0], AMEDIAFORMAT_KEY_HEIGHT, &height0));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[1], AMEDIAFORMAT_KEY_WIDTH, &width1));
        ASSERT_TRUE(AMediaFormat_getInt32(extractorFormat[1], AMEDIAFORMAT_KEY_HEIGHT, &height1));
        ASSERT_EQ(width0, width1) << "Mismatch between extractor's width";
        ASSERT_EQ(height0, height1) << "Mismatch between extractor's height";
    } else {
        ASSERT_TRUE(false) << "Invalid mime type " << mime[0];
    }

    for (AMediaFormat *exFormat : extractorFormat) {
        AMediaFormat_delete(exFormat);
    }

    // Compare the extracted outputs of both extractor
    ASSERT_EQ(extractedOutputSize[0], extractedOutputSize[1])
            << "Extractor's output size doesn't match between " << inputFileNames[0] << "and "
            << inputFileNames[1] << " extractors";
    status = memcmp(mExtractorOutput[0], mExtractorOutput[1], extractedOutputSize[0]);
    ASSERT_EQ(status, 0) << "Extracted content mismatch between " << inputFileNames[0] << "and "
                         << inputFileNames[1] << " extractors";
}

INSTANTIATE_TEST_SUITE_P(ExtractorComparisonAll, ExtractorComparison,
                         ::testing::Values(make_pair("swirl_144x136_vp9.mp4",
                                                     "swirl_144x136_vp9.webm"),
                                           make_pair("video_480x360_mp4_vp9_333kbps_25fps.mp4",
                                                     "video_480x360_webm_vp9_333kbps_25fps.webm"),
                                           make_pair("video_1280x720_av1_hdr_static_3mbps.mp4",
                                                     "video_1280x720_av1_hdr_static_3mbps.webm"),
                                           make_pair("loudsoftaac.aac", "loudsoftaac.mkv")));

INSTANTIATE_TEST_SUITE_P(ConfigParamTestAll, ConfigParamTest,
                         ::testing::Values(make_pair("aac", 0),
                                           make_pair("amr", 1),
                                           make_pair("amr", 2),
                                           make_pair("ogg", 3),
                                           make_pair("wav", 4),
                                           make_pair("flac", 5),
                                           make_pair("ogg", 6),
                                           make_pair("mp3", 7),
                                           make_pair("midi", 8),
                                           make_pair("mpeg2ts", 9),
                                           make_pair("mkv", 10),
                                           make_pair("mpeg4", 11),
                                           make_pair("mkv", 12),
                                           make_pair("mpeg2ps", 13)));

INSTANTIATE_TEST_SUITE_P(ExtractorUnitTestAll, ExtractorFunctionalityTest,
                         ::testing::Values(make_pair("aac", "loudsoftaac.aac"),
                                           make_pair("amr", "testamr.amr"),
                                           make_pair("amr", "amrwb.wav"),
                                           make_pair("ogg", "john_cage.ogg"),
                                           make_pair("wav", "monotestgsm.wav"),
                                           make_pair("mpeg2ts", "segment000001.ts"),
                                           make_pair("mpeg2ts", "testac3ts.ts"),
                                           make_pair("mpeg2ts", "testac4ts.ts"),
                                           make_pair("mpeg2ts", "testeac3ts.ts"),
                                           make_pair("flac", "sinesweepflac.flac"),
                                           make_pair("ogg", "testopus.opus"),
                                           make_pair("ogg", "sinesweepoggalbumart.ogg"),
                                           make_pair("midi", "midi_a.mid"),
                                           make_pair("mkv", "sinesweepvorbis.mkv"),
                                           make_pair("mkv", "sinesweepmp3lame.mkv"),
                                           make_pair("mkv", "loudsoftaac.mkv"),
                                           make_pair("mpeg4", "sinesweepoggmp4.mp4"),
                                           make_pair("mp3", "sinesweepmp3lame.mp3"),
                                           make_pair("mp3", "id3test10.mp3"),
                                           make_pair("mkv", "swirl_144x136_vp9.webm"),
                                           make_pair("mkv", "swirl_144x136_vp8.webm"),
                                           make_pair("mkv", "swirl_144x136_avc.mkv"),
                                           make_pair("mkv", "withoutcues.mkv"),
                                           make_pair("mpeg2ps", "swirl_144x136_mpeg2.mpg"),
                                           make_pair("mpeg2ps", "programstream.mpeg"),
                                           make_pair("mpeg4", "testac3mp4.mp4"),
                                           make_pair("mpeg4", "testeac3mp4.mp4"),
                                           make_pair("mpeg4", "swirl_132x130_mpeg4.mp4")));

int main(int argc, char **argv) {
    gEnv = new ExtractorUnitTestEnvironment();
    ::testing::AddGlobalTestEnvironment(gEnv);
    ::testing::InitGoogleTest(&argc, argv);
    int status = gEnv->initFromOptions(argc, argv);
    if (status == 0) {
        status = RUN_ALL_TESTS();
        ALOGV("Test result = %d\n", status);
    }
    return status;
}
