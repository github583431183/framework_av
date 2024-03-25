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
// Defines decoder API using NdkMediaCodec
//

#ifndef MEDIA_CODEC_DECODER_NDK_H_
#define MEDIA_CODEC_DECODER_NDK_H_

#include <queue>
#include <string>
#include <thread>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaExtractor.h>

// An abstraction for MediaSample.
struct MediaSample {
  // media sample buffer and its info
  const uint8_t* buffer_;
  AMediaCodecBufferInfo buffer_info_;

  const uint8_t* buffer() const {
    return buffer_ + buffer_info_.offset;
  }

  const AMediaCodecBufferInfo& info() const {
      return buffer_info_;
  }

  int32_t size() const {
    return  buffer_info_.size;
  }

  int64_t pts() const {
    return buffer_info_.presentationTimeUs;
  }

  explicit MediaSample(const AMediaCodecBufferInfo& info,
                       const uint8_t* ptr) :
    buffer_(ptr),
    buffer_info_(info) {}
};

// Wait Queue that uses locks to ensure concurrency.
template <typename T>
class WaitQueue {
 private:
  std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable cv_;

 public:
  // Pop the front item from the queue.
  // This waits/blocks until the queue is not empty.
  // Returns as soon as an item is available to retrieve from the queue.
  // Always returns true upon return.
  bool pop(T* item) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      cv_.wait(lock);
    }

    *item = queue_.front();
    queue_.pop();
    return true;
  }

  // Pop the front item from the queue.
  // This may waits until:
  // - the queue is not empty.
  // - wait for timeOutMs if the queue is empty.
  // Returns true upon timeout (without poping any item from the queue)
  // Returns false if it successfully popped an item from the queue.
  bool popOrTimeout(T* item, int32_t timeOutMs) {
    auto timeOut = std::chrono::milliseconds(timeOutMs);
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      auto res = cv_.wait_for(lock, timeOut);
      if (res == std::cv_status::timeout) {
        // Timed out!
        return true;
      }
    }

    *item = queue_.front();
    queue_.pop();
    return false;
  }

  // Push an item to the queue.
  void push(const T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(item);
    cv_.notify_one();
  }

};

// Decoder using NdkMediaCodec.
class MediaCodecDecoderNdk {
 public:
  MediaCodecDecoderNdk(const MediaCodecDecoderNdk&) = delete;
  MediaCodecDecoderNdk(MediaCodecDecoderNdk&&) = delete;
  MediaCodecDecoderNdk& operator=(const MediaCodecDecoderNdk&) = delete;
  MediaCodecDecoderNdk& operator=(MediaCodecDecoderNdk&&) = delete;

  MediaCodecDecoderNdk();
  ~MediaCodecDecoderNdk();

  // create, configure and starts the codec.
  bool start(AMediaFormat* format, AMediaExtractor* extractor = nullptr);

  // Feeds the codec's input buffer with given media sample.
  // May block until input buffer is available.
  bool submitMediaSample(MediaSample&& media_sample);

  // Flushes the codec with end of stream.
  bool flush();

  // Wait until decoding is complete - that is, codec output buffers are consumed.
  bool waitForCompletion();

 private:

  struct BufferInfoWithIndex {
    int index = -1;
    AMediaCodecBufferInfo buffer_info{0, 0, 0, 0};
  };

  // Ensures that the decoder has been started and hasn't flushed.
  bool ensureDecoderIsRunning();

  // Recipients of callbacks from decoder.
  void onAsyncInputAvailable(int index);
  void onFeedInputBuffer(int index);
  void onAsyncOutputAvailable(int index, AMediaCodecBufferInfo* buffer_info);
  void onAsyncFormatChanged(AMediaFormat* format);
  void onAsyncError(media_status_t error, int32_t code, const char* detail);

  // Thread function that feeds video samples.
  void inputLoop();
  // Thread function that drives output sampling.
  void outputLoop();

  void logCodecStats() const;

  bool initialized_ = false;
  bool flush_submitted_ = false;
  bool saw_error_ = false;
  bool saw_input_eos_ = false;
  bool saw_output_eos_ = false;

  // The number of media samples consumed.
  int input_sample_count_ = 0;
  // The number of images decoded and sent to callback.
  int output_frame_count_ = 0;

  int64_t decoding_start_time_ns_ = 0;
  int64_t input_processing_time_ns_ = 0;
  int64_t output_processing_time_ns_ = 0;
  std::thread input_thread_;
  std::thread output_thread_;

  // Lock to protect access to the decoder across threads.
  std::mutex decoder_mutex_;
  AMediaCodec* decoder_;
  std::string decoder_name_;

  // The indexes of input buffers current available to load data onto.
  WaitQueue<int> available_input_buffers_;
  WaitQueue<BufferInfoWithIndex> available_output_buffers_;

  // A collection of MediaSamples that are awaiting decoding. Will be populated
  // when there are no available input buffers and processed as they become
  // available.
  std::queue<MediaSample> media_sample_queue_;
  AMediaExtractor* extractor_ = nullptr;
};

#endif  // MEDIA_CODEC_DECODER_NDK_H_
