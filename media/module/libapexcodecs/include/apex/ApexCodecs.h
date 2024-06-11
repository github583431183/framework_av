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

#pragma once

#include <errno.h>
#include <stdint.h>

#include <android/api-level.h>
#include <android/versioning.h>

/* A placeholder for when the next API level is available
 *      __INTRODUCED_IN(__ANDROID_API_FUTURE__) */
#define PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE

/**
 * Handle for component traits such as name, media type, kind (decoder/encoder),
 * domain (audio/video/image), etc.
 *
 * Introduced in API FUTURE.
 */
typedef struct ApexCodec_ComponentTraits ApexCodec_ComponentTraits;

/**
 * Error code for ApexCodec APIs.
 *
 * Introduced in API FUTURE.
 */
typedef enum codec_status_t : int32_t {
    CODEC_STATUS_OK        = 0,

    /* bad input */
    CODEC_STATUS_BAD_VALUE = EINVAL,
    CODEC_STATUS_BAD_INDEX = ENXIO,
    CODEC_STATUS_CANNOT_DO = ENOTSUP,

    /* bad sequencing of events */
    CODEC_STATUS_DUPLICATE = EEXIST,
    CODEC_STATUS_NOT_FOUND = ENOENT,
    CODEC_STATUS_BAD_STATE = EPERM,
    CODEC_STATUS_BLOCKING  = EWOULDBLOCK,
    CODEC_STATUS_CANCELED  = EINTR,

    /* bad environment */
    CODEC_STATUS_NO_MEMORY = ENOMEM,
    CODEC_STATUS_REFUSED   = EACCES,

    CODEC_STATUS_TIMED_OUT = ETIMEDOUT,

    /* bad versioning */
    CODEC_STATUS_OMITTED   = ENOSYS,

    /* unknown fatal */
    CODEC_STATUS_CORRUPTED = EFAULT,
    CODEC_STATUS_NO_INIT   = ENODEV,
} codec_status_t;

/**
 * Enum that represents the kind of component
 *
 * Introduced in API FUTURE.
 */
typedef enum ApexCodec_Kind : uint32_t {
    /**
     * The component is of a kind that is not listed below.
     */
    APEXCODEC_KIND_OTHER = 0x0,
    /**
     * The component is a decoder, which decodes coded bitstream
     * into raw buffers.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_KIND_DECODER = 0x1,
    /**
     * The component is an encoder, which encodes raw buffers
     * into coded bitstream.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_KIND_ENCODER = 0x2,
} ApexCodec_Kind;

typedef enum ApexCodec_Domain : uint32_t {
    /**
     * A component domain that is not listed below.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_DOMAIN_OTHER = 0x0,
    /**
     * A component domain that operates on video.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_DOMAIN_VIDEO = 0x1,
    /**
     * A component domain that operates on audio.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_DOMAIN_AUDIO = 0x2,
    /**
     * A component domain that operates on image.
     *
     * Introduced in API FUTURE.
     */
    APEXCODEC_DOMAIN_IMAGE = 0x3,
} ApexCodec_Domain;

/**
 * Get the traits object of a component at given index. ApexCodecs_Traits_*
 * functions are used to extract information from the traits object.
 *
 * Returns nullptr if index is out of bounds.
 *
 * \param index index of the traits object to query
 * \return traits object at the index, or nullptr if the index is out of bounds.
 */
ApexCodec_ComponentTraits *ApexCodec_Traits_get(
        size_t index) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the name of the component from the traits object.
 *
 * The returned string is valid for the lifetime of the traits object,
 * and the client should not free the string.
 *
 * \param traits the traits object
 * \return the name of the component
 */
const char *ApexCodec_Traits_getComponentName(ApexCodec_ComponentTraits *traits)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;
/**
 * Get the supported media type of the component from the traits object.
 * Note that a component can support only one media type.
 *
 * The returned string is valid for the lifetime of the traits object,
 * and the client should not free the string.
 *
 * \param traits the traits object
 * \return the supported media type
 */
const char *ApexCodec_Traits_getMediaType(ApexCodec_ComponentTraits *traits)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;
/**
 * Get the kind of the component from the traits object. See ApexCodec_Kind for
 * the possible values.
 *
 * \param traits the traits object
 * \return the kind of the component
 */
ApexCodec_Kind ApexCodec_Traits_getKind(ApexCodec_ComponentTraits *traits)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;
/**
 * Get the domain on which the component operates from the traits object.
 * See ApexCodec_Domain for the possible values.
 *
 * \param traits the traits object
 * \return the domain that the component operates on
 */
ApexCodec_Domain ApexCodec_Traits_getDomain(ApexCodec_ComponentTraits *traits)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * An opaque struct that represents a codec.
 */
struct ApexCodec_Component;

/**
 * Create an component by the name.
 *
 * \param name the name of the component
 * \return the component handle, or nullptr if not found or cannot create
 */
ApexCodec_Component *ApexCodec_Component_create(
        const char *name) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Destroy the component by the handle.
 *
 * \param comp the handle for the component
 */
void ApexCodec_Component_destroy(ApexCodec_Component *comp) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Resets the component to the initial state, right after creation.
 *
 * \param comp the handle for the component
 */
codec_status_t ApexCodec_Component_reset(ApexCodec_Component *comp) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * An opaque struct that represents a configurable part of the component.
 *
 * Introduced in API FUTURE.
 */
struct ApexCodec_Configurable;

/**
 * Return the configurable object for the given ApexCodec_Component.
 * The returned object has the same lifecycle as |comp|.
 *
 * \param comp the handle for the component
 * \return the configurable object handle
 */
ApexCodec_Configurable *ApexCodec_Component_getConfigurable(
        ApexCodec_Component *comp) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Enum that represents the flags for ApexCodec_Buffer.
 *
 * Introduced in API FUTURE.
 */
typedef enum codec_buffer_flags_t : uint32_t {
    CODEC_FLAG_DROP_FRAME    = (1 << 0),
    CODEC_FLAG_END_OF_STREAM = (1 << 1),
    CODEC_FLAG_DISCARD_FRAME = (1 << 2),
    CODEC_FLAG_INCOMPLETE    = (1 << 3),
    CODEC_FLAG_CORRECTED     = (1 << 4),
    CODEC_FLAG_CORRUPT       = (1 << 5),
    CODEC_FLAG_CODEC_CONFIG  = (1u << 31),
} codec_buffer_flags_t;

typedef struct ApexCodec_Memory_Properties ApexCodec_Memory_Properties;

/**
 * Struct that represents a video plane in ApexCodec_Memory.
 *
 * Introduced in API FUTURE.
 */
typedef struct ApexCodec_PlaneInfo {
    codec_plane_component_t component;
    uint32_t width;
    uint32_t height;
    int32_t colInc;
    int32_t rowInc;
    uint32_t colSampling;
    uint32_t rowSampling;
    uint32_t allocatedDepth;
    uint32_t bitDepth;
    uint32_t rightShift;
} ApexCodec_PlaneInfo;

/**
 * Get the plane info from the memory properties.
 *
 * \param props the memory properties
 * \param planeInfo the output plane info
 * \return true if the plane info is present, false otherwise
 */
bool ApexCodec_Memory_Properties_getPlaneInfo(
        ApexCodec_Memory_Properties *props,
        ApexCodec_PlaneInfo *planeInfo);

/**
 * Struct that represents the memory for ApexCodec_Buffer.
 *
 * All memory regions have the simple 1D representation, with optional properties to describe
 * the memory layout, e.g. video planes.
 *
 * Introduced in API FUTURE.
 */
typedef struct ApexCodec_Memory {
    uint8_t *data;
    size_t size;
    ApexCodec_Memory_Properties *props;
} ApexCodec_Memory;

/**
 * Struct that represents a buffer for ApexCodec_Component.
 *
 * Introduced in API FUTURE.
 */
 */
typedef struct ApexCodec_Buffer {
    codec_buffer_flags_t flags;
    uint64_t timestampUs;
    ApexCodec_Memory memory[4];
    size_t numMemories;
} ApexCodec_Buffer;

/**
 * Process one frame from |input|, and produce one frame to |output|
 * if possible.
 *
 * \param comp      the component to process the buffers
 * \param input     the input buffer, may be nullptr
 * \param output    the output buffer, may be nullptr
 * \param consumed  the number of consumed bytes from the input buffer
 *                  set to 0 if no input buffer has been consumed, including |input| is nullptr.
 *                  for graphic buffers, any non-zero value means that the input buffer is consumed.
 */
codec_status_t ApexCodec_Component_process(
        ApexCodec_Component *comp,
        const ApexCodec_Component_Buffer *input,
        ApexCodec_Component_Buffer *output,
        size_t *consumed) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Configure the component with the given config.
 *
 * Configurations are Codec 2.0 configs in binary blobs,
 * concatenated if there are multiple configs.
 *
 * \param comp the handle for the component
 * \param config the config blob
 * \param configSize the size of the config blob
 */
codec_status_t ApexCodec_Configurable_config(
        ApexCodec_Configurable *comp,
        const uint8_t *config,
        size_t configSize) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Query the component for the given indices.
 *
 * \param comp the handle for the component
 * \param indices the array of indices to query
 * \param numIndices the size of the indices array
 * \param config the output buffer for the config blob
 * \param configSize the size of the config buffer
 */
codec_status_t ApexCodec_Configurable_query(
        ApexCodec_Configurable *comp,
        uint32_t indices[],
        size_t numIndices,
        uint8_t *config,
        size_t *configSize) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Query the component for the supported parameters.
 *
 * \param comp the handle for the component
 * \param indices the output buffer for the supported indices
 * \param numIndices the size of the output buffer
 */
codec_status_t ApexCodec_Configurable_querySupportedParams(
        ApexCodec_Configurable *comp,
        uint32_t *indices,
        size_t *numIndices) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Opaque struct that represents the supported values of a parameter.
 *
 * Introduced in API FUTURE.
 */
struct ApexCodec_SupportedValues;

/**
 * Struct that represents the query for the supported values of a parameter.
 *
 * Introduced in API FUTURE.
 */
struct ApexCodec_SupportedValuesQuery {
    /* in-params */
    uint32_t index;
    size_t offset;
    ApexCodec_SupportedValuesQueryType type;
    /* out-params */
    codec_status_t status;
    ApexCodec_SupportedValues *result;
};

/**
 * Query the component for the supported values of the given indices.
 *
 * \param comp the handle for the component
 * \param queries the array of queries
 * \param numQueries the size of the queries array
 */
codec_status_t ApexCodec_Configurable_querySupportedValues(
        ApexCodec_Configurable *comp,
        ApexCodec_SupportedValuesQuery queries[],
        size_t numQueries) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Enum that represents the query type for the supported values.
 *
 * Introduced in API FUTURE.
 */
typedef enum ApexCodec_SupportedValuesQueryType : uint32_t {
    APEXCODEC_SUPPORTED_VALUES_QUERY_CURRENT,
    APEXCODEC_SUPPORTED_VALUES_QUERY_POSSIBLE,
} ApexCodec_SupportedValuesQueryType;

/**
 * Enum that represents the type of the supported values.
 *
 * Introduced in API FUTURE.
 */
typedef enum ApexCodec_SupportedValuesType : uint32_t {
    APEXCODEC_SUPPORTED_VALUES_EMPTY,
    APEXCODEC_SUPPORTED_VALUES_RANGE,
    APEXCODEC_SUPPORTED_VALUES_VALUES,
    APEXCODEC_SUPPORTED_VALUES_FLAGS,
} ApexCodec_SupportedValuesType;

/**
 * Enum that represents numeric types of the supported values.
 *
 * Introduced in API FUTURE.
 */
typedef enum ApexCodec_SupportedValuesNumberType : uint32_t {
    APEXCODEC_SUPPORTED_VALUES_TYPE_NONE   = 0,
    APEXCODEC_SUPPORTED_VALUES_TYPE_INT32  = 1,
    APEXCODEC_SUPPORTED_VALUES_TYPE_UINT32 = 2,
    // UNUSED                              = 3,
    APEXCODEC_SUPPORTED_VALUES_TYPE_INT64  = 4,
    APEXCODEC_SUPPORTED_VALUES_TYPE_UINT64 = 5,
    // UNUSED                              = 6,
    APEXCODEC_SUPPORTED_VALUES_TYPE_FLOAT  = 7,
} ApexCodec_SupportedValuesNumberType;

/**
 * Create an ApexCodec_SupportedValues object.
 */
ApexCodec_SupportedValues *ApexCodec_SupportedValues_create()
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Destroy the ApexCodec_SupportedValues object.
 */
void ApexCodec_SupportedValues_destroy(ApexCodec_SupportedValues *supported);

/**
 * Get the type of the supported values.
 */
ApexCodec_SupportedValuesType ApexCodec_SupportedValues_getType(
        ApexCodec_SupportedValues *supported) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the number type of the supported values.
 */
ApexCodec_SupportedValuesNumberType ApexCodec_SupportedValues_getNumberType(
        ApexCodec_SupportedValues *supported) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the range of the supported values. The values are 32-bit signed integers.
 */
codec_status_t ApexCodec_SupportedValues_getInt32Range(
        ApexCodec_SupportedValues *supported,
        int32_t *min, int32_t *max, int32_t *step, int32_t *num, int32_t *denom)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the range of the supported values. The values are 32-bit unsigned integers.
 */
codec_status_t ApexCodec_SupportedValues_getUint32Range(
        ApexCodec_SupportedValues *supported,
        uint32_t *min, uint32_t *max, uint32_t *step, uint32_t *num, uint32_t *denom)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the range of the supported values. The values are 64-bit signed integers.
 */
codec_status_t ApexCodec_SupportedValues_getInt64Range(
        ApexCodec_SupportedValues *supported,
        int64_t *min, int64_t *max, int64_t *step, int64_t *num, int64_t *denom)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the range of the supported values. The values are 64-bit unsigned integers.
 */
codec_status_t ApexCodec_SupportedValues_getUint64Range(
        ApexCodec_SupportedValues *supported,
        uint64_t *min, uint64_t *max, uint64_t *step, uint64_t *num, uint64_t *denom)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the range of the supported values. The values are floats.
 */
codec_status_t ApexCodec_SupportedValues_getFloatRange(
        ApexCodec_SupportedValues *supported,
        float *min, float *max, float *step, float *num, float *denom)
        PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the list of the supported values. The values are 32-bit signed integers.
 *
 * The returned array is valid for the lifetime of the ApexCodec_SupportedValues object,
 * and the client should not free the array.
 */
codec_status_t ApexCodec_SupportedValues_getInt32Values(
        ApexCodec_SupportedValues *supported,
        int32_t *values, size_t *numValues) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the list of the supported values. The values are 32-bit unsigned integers.
 *
 * The returned array is valid for the lifetime of the ApexCodec_SupportedValues object,
 * and the client should not free the array.
 */
codec_status_t ApexCodec_SupportedValues_getUint32Values(
        ApexCodec_SupportedValues *supported,
        uint32_t *values, size_t *numValues) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the list of the supported values. The values are 64-bit signed integers.
 *
 * The returned array is valid for the lifetime of the ApexCodec_SupportedValues object,
 * and the client should not free the array.
 */
codec_status_t ApexCodec_SupportedValues_getInt64Values(
        ApexCodec_SupportedValues *supported,
        int64_t *values, size_t *numValues) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the list of the supported values. The values are 64-bit unsigned integers.
 *
 * The returned array is valid for the lifetime of the ApexCodec_SupportedValues object,
 * and the client should not free the array.
 */
codec_status_t ApexCodec_SupportedValues_getUint64Values(
        ApexCodec_SupportedValues *supported,
        uint64_t *values, size_t *numValues) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;

/**
 * Get the list of the supported values. The values are floats.
 *
 * The returned array is valid for the lifetime of the ApexCodec_SupportedValues object,
 * and the client should not free the array.
 */
codec_status_t ApexCodec_SupportedValues_getFloatValues(
        ApexCodec_SupportedValues *supported,
        float *values, size_t *numValues) PLACEHOLDER_INTRO_IN_ANDROID_API_FUTURE;