/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef SOFT_VPX_ENCODER_H_

#define SOFT_VPX_ENCODER_H_

#include "SimpleSoftOMXComponent.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vpx_codec.h"
#include "vpx/vp8cx.h"

namespace android {

// Exposes a vpx encoder as an OMX Component
//
// Boilerplate for callback bindings are taken care
// by the base class SimpleSoftOMXComponent and its
// parent SoftOMXComponent.
//
// Only following encoder settings are available
//    - target bitrate
//    - rate control (constant / variable)
//    - frame rate
//
// Only following color formats are recognized
//    - YUV420Planar
//    - YUV420SemiPlanar
//    - AndroidOpaque
//
// Following settings are not configurable by the client
//    - encoding deadline is realtime
//    - multithreaded encoding utilizes a number of threads equal
// to online cpu's available
//    - the algorithm interface for encoder is vp8
//    - fractional bits of frame rate is discarded
//    - OMX timestamps are in microseconds, therefore
// encoder timebase is fixed to 1/1000000

class SoftVPXEncoder : public SimpleSoftOMXComponent {
 public:
    SoftVPXEncoder(const char *name,
                   const OMX_CALLBACKTYPE *callbacks,
                   OMX_PTR appData,
                   OMX_COMPONENTTYPE **component);

 protected:
    virtual ~SoftVPXEncoder();

    // Returns current values for requested OMX
    // parameters
    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR param);

    // Validates, extracts and stores relevant OMX
    // parameters
    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR param);

    // OMX callback when buffers available
    // Note that both an input and output buffer
    // is expected to be available to carry out
    // encoding of the frame
    virtual void onQueueFilled(OMX_U32 portIndex);

 private:
    enum {
        // number of buffers allocated per port
        kNumBuffers = 4
    };

    // vpx specific opaque data structure that
    // stores encoder state
    vpx_codec_ctx_t* mCodecContext;

    // vpx specific data structure that
    // stores encoder configuration
    vpx_codec_enc_cfg_t* mCodecConfiguration;

    // vpx specific read-only data structure
    // that specifies algorithm interface (e.g. vp8)
    vpx_codec_iface_t* mCodecInterface;

    // Width of the input frames
    int32_t mWidth;

    // Height of the input frames
    int32_t mHeight;

    // Target bitrate set for the encoder, in bits per second.
    int32_t mBitrate;

    // Bitrate control mode, either constant or variable
    vpx_rc_mode mBitrateControlMode;

    // Frame duration is the reciprocal of framerate, denoted
    // in microseconds
    uint64_t mFrameDuration;

    // Byte-alignment required for input buffers
    int32_t mInputBufferAlignment;

    // Color format for the input port
    OMX_COLOR_FORMATTYPE mColorFormat;

    // Conversion buffer is needed to convert semi
    // planar yuv420 to planar format
    // It is only allocated if input format is
    // indeed YUV420SemiPlanar.
    uint8_t* mConversionBuffer;

    // Initializes input and output OMX ports with sensible
    // default values.
    void initPorts();

    // Initializes vpx encoder with available settings.
    status_t initEncoder();

    // Releases vpx encoder instance, with it's associated
    // data structures.
    //
    // Unless called earlier, this is handled by the
    // dtor.
    status_t releaseEncoder();

    // Handles port changes with respect to color formats
    OMX_ERRORTYPE internalSetFormatParams(
        const OMX_VIDEO_PARAM_PORTFORMATTYPE* format);

    // Verifies the component role tried to be set to this OMX component is
    // strictly video_encoder.vpx
    OMX_ERRORTYPE internalSetRoleParams(
        const OMX_PARAM_COMPONENTROLETYPE* role);

    // Updates bitrate to reflect port settings.
    OMX_ERRORTYPE internalSetBitrateParams(
        const OMX_VIDEO_PARAM_BITRATETYPE* bitrate);

    // Handles port definition changes.
    OMX_ERRORTYPE internalSetPortParams(
        const OMX_PARAM_PORTDEFINITIONTYPE* port);

    DISALLOW_EVIL_CONSTRUCTORS(SoftVPXEncoder);
};

}  // namespace android

#endif  // SOFT_VPX_ENCODER_H_
