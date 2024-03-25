/*
 * Copyright (C) 2024 The Android Open Source Project
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

//
// Implements decoder using NdkMediaCodec
//

#define LOG_TAG "MediaCodecDecoderNdk"
#include <utils/Log.h>

#include "media_codec_decoder_ndk.h"

namespace {

// The amount of time to wait for an input buffer to become available when
// attempting to decode samples.
constexpr int32_t kInputBufferWaitTimeoutMs = 500;

int64_t systemTime() {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

}  // namespace

MediaCodecDecoderNdk::MediaCodecDecoderNdk() {}

MediaCodecDecoderNdk::~MediaCodecDecoderNdk() {
  if (initialized_) {
    AMediaCodec_stop(decoder_);
    AMediaCodec_delete(decoder_);

    if (output_thread_.joinable()) {
      output_thread_.join();
    }
  }
}

bool MediaCodecDecoderNdk::start(AMediaFormat* format, AMediaExtractor* extractor) {
  std::lock_guard<std::mutex> lock(decoder_mutex_);
  // Already initialized, return immediately.
  if (initialized_) {
    return true;
  }

  // Setup the decoder.
  const char* mime = nullptr;
  AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
  decoder_ = AMediaCodec_createDecoderByType(mime);
  if (decoder_ == nullptr) {
    ALOGE("%s: Failed to create decoder for mime: %s", __func__, mime);
    return false;
  }

  // Setup decoder callbacks.
  AMediaCodecOnAsyncNotifyCallback decoder_callbacks;

  decoder_callbacks.onAsyncInputAvailable = [](AMediaCodec* codec,
                                               void* userdata, int32_t index) {
    (void)codec;
    reinterpret_cast<MediaCodecDecoderNdk*>(userdata)->onAsyncInputAvailable(index);
  };

  decoder_callbacks.onAsyncOutputAvailable =
      [](AMediaCodec* codec, void* userdata, int32_t index,
         AMediaCodecBufferInfo* buffer_info) {
        (void)codec;
        reinterpret_cast<MediaCodecDecoderNdk*>(userdata)
            ->onAsyncOutputAvailable(index, buffer_info);
      };

  decoder_callbacks.onAsyncFormatChanged =
      [](AMediaCodec* codec, void* userdata, AMediaFormat* format) {
        (void)codec;
        reinterpret_cast<MediaCodecDecoderNdk*>(userdata)->onAsyncFormatChanged(
            format);
      };

  decoder_callbacks.onAsyncError = [](AMediaCodec* codec, void* userdata,
                                      media_status_t error, int32_t code,
                                      const char* detail) {
        (void)codec;
        reinterpret_cast<MediaCodecDecoderNdk*>(userdata)->onAsyncError(error, code, detail);
  };

  if (AMediaCodec_setAsyncNotifyCallback(decoder_, decoder_callbacks, this) !=
      AMEDIA_OK) {
    ALOGE("%s: AMediaCodec_setAsyncNotifyCallback failed", __func__);
    return false;
  }

  if (AMediaCodec_configure(decoder_, format, nullptr, nullptr,
                            /*flags=*/0) != AMEDIA_OK) {
    ALOGE("%s: AMediaCodec_configure failed", __func__);
    return false;
  }

  char* name = nullptr;
  if (AMediaCodec_getName(decoder_, &name) != AMEDIA_OK) {
    ALOGE("%s: AMediaCodec_getName failed", __func__);
    return false;
  }
  decoder_name_ = name;

  decoding_start_time_ns_ = systemTime();
  if (AMediaCodec_start(decoder_) != AMEDIA_OK) {
    ALOGE("%s: AMediaCodec_start failed", __func__);
    return false;
  }

  if (extractor != nullptr) {
    extractor_ = extractor;
    if (input_thread_.joinable()) {
      input_thread_.join();
    }
    input_thread_ = std::thread(&MediaCodecDecoderNdk::inputLoop, this);
  }

  if (output_thread_.joinable()) {
    output_thread_.join();
  }
  output_thread_ = std::thread(&MediaCodecDecoderNdk::outputLoop, this);

  initialized_ = true;

  return true;
}

void MediaCodecDecoderNdk::logCodecStats() const {
  ALOGI("Decoder Name: %s", decoder_name_.c_str());
  ALOGI("No of Input Samples: %d Input processing time(ns): %jd",
        input_sample_count_, input_processing_time_ns_);
  ALOGI("No of Output Frames: %d Output processing time(ns): %jd",
        output_frame_count_, output_processing_time_ns_);
}

bool MediaCodecDecoderNdk::waitForCompletion() {
  // Wait for Input and Output threads, if they are still running.
  if (input_thread_.joinable()) {
    input_thread_.join();
  }
  if (output_thread_.joinable()) {
    output_thread_.join();
  }

  logCodecStats();
  return true;
}

bool MediaCodecDecoderNdk::submitMediaSample(MediaSample&& media_sample) {
  if (!ensureDecoderIsRunning()) {
    return false;
  }

  media_sample_queue_.push(std::move(media_sample));

  // Iterate through the samples currently queued and send to input buffers as
  // available.
  while (!media_sample_queue_.empty()) {
    // Attempt to find an available input buffer to load the sample onto.
    int input_buffer_index = -1;
    bool timed_out = available_input_buffers_.popOrTimeout(
        &input_buffer_index, kInputBufferWaitTimeoutMs);

    if (timed_out) {
      ALOGW("%s: No input buffer available at the momemt", __func__);
      break;
    }

    // Retrieve the media sample.
    MediaSample sample = std::move(media_sample_queue_.front());
    media_sample_queue_.pop();

    // Send the sample data into the input buffer.

    // Retrieve the available buffer.
    size_t buffer_size;
    uint8_t* buffer = nullptr;
    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      buffer = AMediaCodec_getInputBuffer(decoder_, input_buffer_index, &buffer_size);
    }

    if (!buffer) {
      ALOGE("%s: AMediaCodec_getInputBuffer failed", __func__);
      return false;
    }

    // Copy the sample into the buffer (verify it can fit into the buffer).
    if (sample.size() > buffer_size) {
      ALOGE("%s: payload.size(): %d buffer_size: %zu", __func__, sample.size(), buffer_size);
      return false;
    }
    if (sample.size() > 0) {
      std::memcpy(buffer, sample.buffer(), sample.size());
    } else {
      saw_input_eos_ = true;
    }

    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      // Now that the buffer is loaded, send it to the codec for processing.
      if (AMediaCodec_queueInputBuffer(
              decoder_, input_buffer_index,
              /*offset=*/0, sample.size(), sample.pts(),
              sample.size() > 0 ? 0 : AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != AMEDIA_OK) {
        ALOGE("%s: AMediaCodec_queueInputBuffer failed", __func__);
      }
    }
    ++input_sample_count_;
  }

  if (saw_input_eos_) {
    // Record the input processing time.
    int64_t decoding_input_eos_time_ns = systemTime();
    input_processing_time_ns_ = decoding_input_eos_time_ns - decoding_start_time_ns_;
  }

  return true;
}

bool MediaCodecDecoderNdk::flush() {
  if (!ensureDecoderIsRunning()) {
    return false;
  }

  // Attempt to find an available input buffer to load the sample onto.
  int input_buffer_index;
  bool timed_out = available_input_buffers_.popOrTimeout(
      &input_buffer_index, kInputBufferWaitTimeoutMs);
  if (timed_out) {
    ALOGE("Failed to flush MediaCodecDecoderNdk.");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    // Send end of stream message to the input buffer.
    if (AMediaCodec_queueInputBuffer(decoder_, input_buffer_index,
                                     /*offset=*/0, /*size=*/0,
                                     /*presentation_timestamp_us=*/-1,
                                     AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) !=
        AMEDIA_OK) {
      ALOGE("AMediaCodec_queueInputBuffer failed");
    }

    flush_submitted_ = true;
  }
  return true;
}

bool MediaCodecDecoderNdk::ensureDecoderIsRunning() {
  std::lock_guard<std::mutex> lock(decoder_mutex_);
  // Init() hasn't been called yet.
  if (!initialized_) {
    ALOGE("MediaCodec is not initialized yet.");
    return false;
  }

  if (flush_submitted_) {
    ALOGE("Not allowed to submit media sample when stream is already flushed.");
    return false;
  }

  return true;
}

void MediaCodecDecoderNdk::inputLoop() {
  while (!saw_input_eos_ && !saw_error_) {
    // Attempt to find an available input buffer to load the sample onto.
    int input_buffer_index = -1;
    bool timed_out = available_input_buffers_.popOrTimeout(
        &input_buffer_index, kInputBufferWaitTimeoutMs);

    if (timed_out) {
      ALOGW("%s: No input buffer available at the momemt", __func__);
      continue;
    }
    onFeedInputBuffer(input_buffer_index);
  }

  // Record the input processing time.
  int64_t decoding_input_eos_time_ns = systemTime();
  input_processing_time_ns_ = decoding_input_eos_time_ns - decoding_start_time_ns_;
}

void MediaCodecDecoderNdk::outputLoop() {
  BufferInfoWithIndex buffer_info_with_index;

  while (!saw_output_eos_ && !saw_error_) {
    available_output_buffers_.pop(&buffer_info_with_index);

    int64_t presentation_timestamp_ns =
        buffer_info_with_index.buffer_info.presentationTimeUs;
    int32_t buffer_info_size = buffer_info_with_index.buffer_info.size;

    if (buffer_info_size <= 0) {
      if (buffer_info_with_index.index < 0) {
        ALOGI("That was a dummy buffer to mark error, nothing to release");
      } else {
        // A null image means end-of-stream. Release the buffer and we notify
        // the client.
        std::lock_guard<std::mutex> lock(decoder_mutex_);
        AMediaCodec_releaseOutputBuffer(decoder_, buffer_info_with_index.index,
                                        /*render=*/false);
      }

      saw_output_eos_ = true;
      ALOGI("Output Done. Ending the thread");
      break;
    }

    size_t buffer_size = 0;
    const uint8_t* buffer = nullptr;
    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      buffer = AMediaCodec_getOutputBuffer(
          decoder_, buffer_info_with_index.index, &buffer_size);
    }
    if (buffer != nullptr) {
      //Consume the buffer.
    }
    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      AMediaCodec_releaseOutputBuffer(decoder_, buffer_info_with_index.index,
                                      /*render=*/false);
    }
    saw_output_eos_ = (0 != (buffer_info_with_index.buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
    ++output_frame_count_;
  }

  // Record the output processing time.
  int64_t decoding_output_eos_time_ns = systemTime();
  output_processing_time_ns_ = decoding_output_eos_time_ns - decoding_start_time_ns_;
}

void MediaCodecDecoderNdk::onAsyncInputAvailable(int index) {
  available_input_buffers_.push(index);
}

void MediaCodecDecoderNdk::onAsyncOutputAvailable(
    int index, AMediaCodecBufferInfo* buffer_info) {
  available_output_buffers_.push(BufferInfoWithIndex{
      .index = index,
      .buffer_info = *buffer_info});
}

void MediaCodecDecoderNdk::onAsyncError(media_status_t error,
                                        int32_t code,
                                        const char* detail) {
  saw_error_ = true;
  BufferInfoWithIndex dummyBuffer;
  available_output_buffers_.push(dummyBuffer);
  ALOGW("AMediaCodecOnAsyncError: error: %d code: %d details: %s",
        error, code, detail);
}

void MediaCodecDecoderNdk::onAsyncFormatChanged(AMediaFormat* format) {
  (void)format;
  ALOGI("MediaCodecDecoderNdk::OnAsyncFormatChanged");
}

void MediaCodecDecoderNdk::onFeedInputBuffer(int32_t index) {
  if (saw_input_eos_ || index < 0 || saw_error_) {
    return;
  }

  // Get the buffer and the size associated with this input buffer
  size_t bufSize = 0;
  uint8_t* buf = nullptr;
  {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    buf = AMediaCodec_getInputBuffer(decoder_, index, &bufSize);
  }
  // Make sure its valid.
  if (!buf) {
    onAsyncError(AMEDIA_ERROR_IO, -1, "Failed to get InputBuffer");
    return;
  }

  // Write the media sample onto this buffer
  ssize_t bytesRead = AMediaExtractor_readSampleData(extractor_, buf, bufSize);
  if (bytesRead < 0) {
    // EOS - Done reading samples.
    bytesRead = 0;
  }

  // Get the sample time and the flags.
  uint32_t flags = AMediaExtractor_getSampleFlags(extractor_);
  int64_t presentationTimeUs = AMediaExtractor_getSampleTime(extractor_);
  AMediaExtractor_advance(extractor_);

  if (flags == AMEDIA_ERROR_MALFORMED) {
    onAsyncError(AMEDIA_ERROR_MALFORMED, -1, "Failed to get MediaSample");
    return;
  }

  if (bytesRead == 0 || flags == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
    saw_input_eos_ = true;
    presentationTimeUs = 0;
    flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
    ALOGI("%s: Marking end of input stream", __func__);
  }

  // Queue the input buffer for processing.
  media_status_t status = AMEDIA_OK;
  {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    status = AMediaCodec_queueInputBuffer(decoder_, index, 0 /* offset */,
                                          bytesRead, presentationTimeUs, flags);
  }
  if (AMEDIA_OK != status) {
    onAsyncError(status, -1, "Failed to get Queue Input Buffer");
    return;
  }

  ++input_sample_count_;
}
