/*
 * Copyright 2024 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecDecoderNdk"

#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <tuple>

#include <gtest/gtest.h>
#include <utils/RefBase.h>
#include <utils/Log.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include "media_codec_decoder_ndk.h"

using std::string;
using std::tuple;
using std::make_tuple;

namespace android {

class NdkMediaCodecTest : public ::testing::Test {
};


TEST(NdkMediaCodec_tests, error_while_configuring_decoder) {
    // Test scenario:
    //
    // Create decoder and configure with some missing parameters
    // and expect the configurations to fail.
    // - Configure Video Decoder without width
    // - Configure Video Decoder without height

    int32_t width = 1280;
    int32_t height = 720;
    int32_t bitrate = 5000000;
    int32_t framerate = 30;
    int32_t profile = 1;
    int32_t level = 1;
    int32_t priority = 1;
    const char* mime = "video/avc";
    media_status_t err = AMEDIA_OK;

    // Create AVC decoder
    AMediaCodec* decoder = AMediaCodec_createDecoderByType(mime);
    if (!decoder) {
      ALOGE("The device doesn't support any decoder for mime: %s!"
            " So the test is being skipped", mime);
      return;
    }

    AMediaFormat* decoderFormat = AMediaFormat_new();
    EXPECT_NE(decoderFormat, nullptr);
    AMediaFormat_setString(decoderFormat, "mime", mime);
    AMediaFormat_setInt32(decoderFormat, "bitrate", bitrate);
    AMediaFormat_setInt32(decoderFormat, "frame-rate", framerate);
    AMediaFormat_setInt32(decoderFormat, "profile", profile);
    AMediaFormat_setInt32(decoderFormat, "level", level);
    AMediaFormat_setInt32(decoderFormat, "priority", priority);

    // Configure without width
    err = AMediaCodec_configure(decoder, decoderFormat, nullptr, nullptr, 0);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    AMediaFormat_setInt32(decoderFormat, "width", width);

    // Configure without height
    err = AMediaCodec_configure(decoder, decoderFormat, nullptr, nullptr, 0);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    AMediaFormat_setInt32(decoderFormat, "height", height);

    err = AMediaCodec_configure(decoder, decoderFormat, nullptr, nullptr, 0);
    ASSERT_EQ(err, AMEDIA_OK) << "Configure is expected to Succeed";

    AMediaCodec_delete(decoder);
    AMediaFormat_delete(decoderFormat);
}

TEST(NdkMediaCodec_tests, error_while_configuring_encoder) {
    // Test scenario:
    //
    // Create encoder and configure with some missing parameters
    // and expect the configurations to fail.
    // - Configure Video Encoder without resolution
    // - Configure Video Encoder without i-frame-interval
    // - Configure Video Encoder without frame-rate
    // - Configure Video Encoder in CQ mode and without quality

    int32_t width = 1280;
    int32_t height = 720;
    int32_t bitrate = 5000000;
    int32_t framerate = 30;
    int32_t profile = 1;
    int32_t level = 1;
    int32_t priority = 1;
    int32_t colorFormat = 1;
    int32_t iFrameInterval = 1;
    int32_t bitrateMode = 0; //BITRATE_MODE_CQ;
    const char* mime = "video/avc";
    media_status_t err = AMEDIA_OK;

    // Create AVC encoder
    AMediaCodec* encoder = AMediaCodec_createEncoderByType(mime);
    if (!encoder) {
      ALOGE("The device doesn't support any encoder for mime: %s!"
            " So the test is being skipped", mime);
      return;
    }

    AMediaFormat* encoderFormat = AMediaFormat_new();
    EXPECT_NE(encoderFormat, nullptr);

    // Configure without resolution
    AMediaFormat_setString(encoderFormat, "mime", mime);
    err = AMediaCodec_configure(encoder, encoderFormat, nullptr, nullptr,
                                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    // Configure without i-frame-interval
    AMediaFormat_setInt32(encoderFormat, "width", width);
    AMediaFormat_setInt32(encoderFormat, "height", height);
    err = AMediaCodec_configure(encoder, encoderFormat, nullptr, nullptr,
                                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    // Configure without framerate
    AMediaFormat_setInt32(encoderFormat, "i-frame-interval", iFrameInterval);
    err = AMediaCodec_configure(encoder, encoderFormat, nullptr, nullptr,
                                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    // Configure bitrate-mode as CQ mode and without quality
    AMediaFormat_setInt32(encoderFormat, "frame-rate", framerate);
    AMediaFormat_setInt32(encoderFormat, "profile", profile);
    AMediaFormat_setInt32(encoderFormat, "level", level);
    AMediaFormat_setInt32(encoderFormat, "priority", priority);
    AMediaFormat_setInt32(encoderFormat, "color-format", colorFormat);
    AMediaFormat_setInt32(encoderFormat, "bitrate-mode", bitrateMode);
    err = AMediaCodec_configure(encoder, encoderFormat, nullptr, nullptr,
                                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    ASSERT_NE(err, AMEDIA_OK) << "Configure is expected to fail";

    // One last config which would succeed
    AMediaFormat_setInt32(encoderFormat, "quality", 1);
    // NOTE that ACodec will fail if we set bitrate with CQ mode
    // while CCodec doesn't complain
    AMediaFormat_setInt32(encoderFormat, "bitrate", bitrate);
    err = AMediaCodec_configure(encoder, encoderFormat, nullptr, nullptr,
                                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    ASSERT_EQ(err, AMEDIA_OK) << "Configure is expected to succeed";

    AMediaCodec_delete(encoder);
    AMediaFormat_delete(encoderFormat);
}

// Buffer of size 64MB
constexpr uint32_t kMaxBufferSize = 1024 * 1024 * 64;

class NdkVideoCodecTest : public ::testing::TestWithParam<tuple<string, string, bool>> {

  void feedDecoder(MediaCodecDecoderNdk* decoder, AMediaExtractor* extractor) {
      // Start feeding the frame to the decoder
      bool done = false;
      AMediaCodecBufferInfo frameInfo;
      uint32_t inputBufferOffset = 0;
      int32_t size = 0;
      std::unique_ptr<uint8_t[]> inputBuffer(new (std::nothrow) uint8_t[kMaxBufferSize]);
      ASSERT_NE(inputBuffer, nullptr) << "Insufficient memory";

      while (!done) {
        frameInfo.flags = AMediaExtractor_getSampleFlags(extractor);
        frameInfo.presentationTimeUs = AMediaExtractor_getSampleTime(extractor);
        frameInfo.offset = inputBufferOffset;
        size = AMediaExtractor_readSampleData(extractor,
                                              inputBuffer.get() + inputBufferOffset,
                                              kMaxBufferSize - inputBufferOffset);
        if (size <= 0) {
          frameInfo.size = 0;
          done = true;
        } else {
          frameInfo.size = size;
        }

        MediaSample media_sample(frameInfo, inputBuffer.get());
        decoder->submitMediaSample(std::move(media_sample));
        inputBufferOffset += frameInfo.size;
        AMediaExtractor_advance(extractor);
      }
  }

 public:
  void doDecoding(bool testFeedsDecoder = false) {
    tuple<string /* InputFile */, string /* CodecName */, bool /* asyncMode */> params = GetParam();

    string inputFile = "/data/local/tmp/MediaBenchmark/res/" + get<0>(params);
    FILE* inputFp = fopen(inputFile.c_str(), "rb");

    if (!inputFp) {
      ALOGE("%s: Unable to open input file: %s for reading!."
            " Make sure it has been copied to /data/local/tmp/MediaBenchmark/res/",
            __func__, inputFile.c_str());
      return;
    }

    ALOGI("InputFile: %s", inputFile.c_str());

    AMediaExtractor* extractor = AMediaExtractor_new();
    if (!extractor) {
      ALOGE("Failed to create the AMediaExtractor");
      fclose(inputFp);
      return;
    }

    // Read file properties
    struct stat buf;
    stat(inputFile.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t fd = fileno(inputFp);
    media_status_t status = AMediaExtractor_setDataSourceFd(extractor, fd, 0, fileSize);
    if (status != AMEDIA_OK) {
      ALOGE("AMediaExtractor_setDataSourceFd failed with %d", status);
      AMediaExtractor_delete(extractor);
      fclose(inputFp);
      return;
    }

    int32_t trackCount = AMediaExtractor_getTrackCount(extractor);
    if (trackCount <= 0) {
      ALOGE("No Media Tracks in %s", inputFile.c_str());
      AMediaExtractor_delete(extractor);
      fclose(inputFp);
      return;
    }

    for (int curTrack = 0; curTrack < trackCount; curTrack++) {
      AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, curTrack);
      if (format != nullptr) {
        const char* mime = nullptr;
        AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
        if (!mime) {
          break;
        } else {
          if (strncmp(mime, "video/", 6)) {
            continue;
          }
        }
      } else {
        ALOGE("No MediaFormat!");
        break;
      }

      // Select the track
      AMediaExtractor_selectTrack(extractor, curTrack);

      // Create NdkMediaCodec and start using this format
      auto decoder = std::make_unique<MediaCodecDecoderNdk>();
      if (!decoder) {
        ALOGE("Failed to Create NDK MediaCodec: %d", __LINE__);
        break;
      }
      bool success = false;
      if (testFeedsDecoder) {
        success = decoder->start(format);
      } else {
        success = decoder->start(format, extractor);
      }
      if (!success) {
        ALOGE("Failed to start the Codec: %d", __LINE__);
        break;
      }

      if (testFeedsDecoder) {
        // Start feeding the frames to the decoder
        feedDecoder(decoder.get(), extractor);
      }
      success = decoder->waitForCompletion();
      if (!success) {
        ALOGE("Decoding Failed: %d", __LINE__);
        break;
      }
    }

    AMediaExtractor_delete(extractor);
    fclose(inputFp);
  }
};

TEST_P(NdkVideoCodecTest, DecodeTestFeedInput) {
  // Test will feed the video sample to the decoder.
  doDecoding(true);
}

TEST_P(NdkVideoCodecTest, DecodeSelfFeedInput) {
  // Decoder will feed sample to self (so we pass the extractor to decoder).
  doDecoding(false);
}

/*
 *
 * Test setup:
 * ==========
 * The test reads the input media from /data/local/tmp/MediaBenchmark/res/
 * So, before running this test, make sure that the test contents are copied to
 * the device.
 * Some of the test contents are available @
 *  frameworks/av/media/module/libmediatranscoding/tests/assets/TranscodingTestAssets/
 * For example:
 * adb shell mkdir -p /data/local/tmp/MediaBenchmark/res/
 * adb push \
 *  frameworks/av/media/module/libmediatranscoding/tests/assets/TranscodingTestAssets/<file>  \
 *  /data/local/tmp/MediaBenchmark/res/
 *
 *
 * To build the test:
 * =================
 * make libmediandk_test -j
 * adb shell mkdir -p /data/nativetest64/libmediandk_test/
 * adb push $OUT/data/nativetest64/libmediandk_test/libmediandk_test \
 *          /data/nativetest64/libmediandk_test/
 *
 * To run the test:
 * ================
 * - adb shell "logcat -c; ./data/nativetest64/libmediandk_test/libmediandk_test"
 *
 * To capture the test (specific) logs:
 * ====================================
 * - adb shell "logcat -vtime" | grep MediaCodecDecoderNdk
 *
 */
INSTANTIATE_TEST_SUITE_P(NdkVideoDecoderAsyncTest, NdkVideoCodecTest,
                         ::testing::Values(
                                 // Hardware codecs
                                 make_tuple("backyard_hevc_1920x1080_20Mbps.mp4", "", true),
                                 make_tuple("plex_hevc_3840x2160_20Mbps.mp4", "", true),
                                 make_tuple("adarsh_plant_tof.mp4", "", true),
                                 make_tuple("data_02_dark.mp4", "", true)));
} // namespace android
