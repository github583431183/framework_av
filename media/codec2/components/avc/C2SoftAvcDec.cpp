/*
 * Copyright 2017 The Android Open Source Project
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
#define LOG_TAG "C2SoftAvcDec"
#include <log/log.h>

#include <media/stagefright/foundation/MediaDefs.h>

#include <C2Debug.h>
#include <C2PlatformSupport.h>
#include <Codec2Mapper.h>
#include <SimpleC2Interface.h>

#include "C2SoftAvcDec.h"
#include "ih264d.h"

namespace android {

namespace {

constexpr char COMPONENT_NAME[] = "c2.android.avc.decoder";
constexpr uint32_t kDefaultOutputDelay = 8;
constexpr uint32_t kMaxOutputDelay = 16;
}  // namespace

class C2SoftAvcDec::IntfImpl : public SimpleInterface<void>::BaseParams {
public:
    explicit IntfImpl(const std::shared_ptr<C2ReflectorHelper> &helper)
        : SimpleInterface<void>::BaseParams(
                helper,
                COMPONENT_NAME,
                C2Component::KIND_DECODER,
                C2Component::DOMAIN_VIDEO,
                MEDIA_MIMETYPE_VIDEO_AVC) {
        noPrivateBuffers(); // TODO: account for our buffers here
        noInputReferences();
        noOutputReferences();
        noInputLatency();
        noTimeStretch();

        // TODO: Proper support for reorder depth.
        addParameter(
                DefineParam(mActualOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
                .withDefault(new C2PortActualDelayTuning::output(kDefaultOutputDelay))
                .withFields({C2F(mActualOutputDelay, value).inRange(0, kMaxOutputDelay)})
                .withSetter(Setter<decltype(*mActualOutputDelay)>::StrictValueWithNoDeps)
                .build());

        // TODO: output latency and reordering

        addParameter(
                DefineParam(mAttrib, C2_PARAMKEY_COMPONENT_ATTRIBUTES)
                .withConstValue(new C2ComponentAttributesSetting(C2Component::ATTRIB_IS_TEMPORAL))
                .build());

        // coded and output picture size is the same for this codec
        addParameter(
                DefineParam(mSize, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(0u, 320, 240))
                .withFields({
                    C2F(mSize, width).inRange(2, 4080, 2),
                    C2F(mSize, height).inRange(2, 4080, 2),
                })
                .withSetter(SizeSetter)
                .build());

        addParameter(
                DefineParam(mMaxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(0u, 320, 240))
                .withFields({
                    C2F(mSize, width).inRange(2, 4080, 2),
                    C2F(mSize, height).inRange(2, 4080, 2),
                })
                .withSetter(MaxPictureSizeSetter, mSize)
                .build());

        addParameter(
                DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(0u,
                        C2Config::PROFILE_AVC_CONSTRAINED_BASELINE, C2Config::LEVEL_AVC_5_2))
                .withFields({
                    C2F(mProfileLevel, profile).oneOf({
                            C2Config::PROFILE_AVC_CONSTRAINED_BASELINE,
                            C2Config::PROFILE_AVC_BASELINE,
                            C2Config::PROFILE_AVC_MAIN,
                            C2Config::PROFILE_AVC_CONSTRAINED_HIGH,
                            C2Config::PROFILE_AVC_PROGRESSIVE_HIGH,
                            C2Config::PROFILE_AVC_HIGH}),
                    C2F(mProfileLevel, level).oneOf({
                            C2Config::LEVEL_AVC_1, C2Config::LEVEL_AVC_1B, C2Config::LEVEL_AVC_1_1,
                            C2Config::LEVEL_AVC_1_2, C2Config::LEVEL_AVC_1_3,
                            C2Config::LEVEL_AVC_2, C2Config::LEVEL_AVC_2_1, C2Config::LEVEL_AVC_2_2,
                            C2Config::LEVEL_AVC_3, C2Config::LEVEL_AVC_3_1, C2Config::LEVEL_AVC_3_2,
                            C2Config::LEVEL_AVC_4, C2Config::LEVEL_AVC_4_1, C2Config::LEVEL_AVC_4_2,
                            C2Config::LEVEL_AVC_5, C2Config::LEVEL_AVC_5_1, C2Config::LEVEL_AVC_5_2
                    })
                })
                .withSetter(ProfileLevelSetter, mSize)
                .build());

        addParameter(
                DefineParam(mMaxInputSize, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
                .withDefault(new C2StreamMaxBufferSizeInfo::input(0u, 320 * 240 * 3 / 4))
                .withFields({
                    C2F(mMaxInputSize, value).any(),
                })
                .calculatedAs(MaxInputSizeSetter, mMaxSize)
                .build());

        C2ChromaOffsetStruct locations[1] = { C2ChromaOffsetStruct::ITU_YUV_420_0() };
        std::shared_ptr<C2StreamColorInfo::output> defaultColorInfo =
            C2StreamColorInfo::output::AllocShared(
                    1u, 0u, 8u /* bitDepth */, C2Color::YUV_420);
        memcpy(defaultColorInfo->m.locations, locations, sizeof(locations));

        defaultColorInfo =
            C2StreamColorInfo::output::AllocShared(
                    { C2ChromaOffsetStruct::ITU_YUV_420_0() },
                    0u, 8u /* bitDepth */, C2Color::YUV_420);
        helper->addStructDescriptors<C2ChromaOffsetStruct>();

        addParameter(
                DefineParam(mColorInfo, C2_PARAMKEY_CODED_COLOR_INFO)
                .withConstValue(defaultColorInfo)
                .build());

        addParameter(
                DefineParam(mDefaultColorAspects, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
                .withDefault(new C2StreamColorAspectsTuning::output(
                        0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                        C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                .withFields({
                    C2F(mDefaultColorAspects, range).inRange(
                                C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                    C2F(mDefaultColorAspects, primaries).inRange(
                                C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                    C2F(mDefaultColorAspects, transfer).inRange(
                                C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                    C2F(mDefaultColorAspects, matrix).inRange(
                                C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                })
                .withSetter(DefaultColorAspectsSetter)
                .build());

        addParameter(
                DefineParam(mCodedColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
                .withDefault(new C2StreamColorAspectsInfo::input(
                        0u, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                        C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                .withFields({
                    C2F(mCodedColorAspects, range).inRange(
                                C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                    C2F(mCodedColorAspects, primaries).inRange(
                                C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                    C2F(mCodedColorAspects, transfer).inRange(
                                C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                    C2F(mCodedColorAspects, matrix).inRange(
                                C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                })
                .withSetter(CodedColorAspectsSetter)
                .build());

        addParameter(
                DefineParam(mColorAspects, C2_PARAMKEY_COLOR_ASPECTS)
                .withDefault(new C2StreamColorAspectsInfo::output(
                        0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                        C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                .withFields({
                    C2F(mColorAspects, range).inRange(
                                C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                    C2F(mColorAspects, primaries).inRange(
                                C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                    C2F(mColorAspects, transfer).inRange(
                                C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                    C2F(mColorAspects, matrix).inRange(
                                C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                })
                .withSetter(ColorAspectsSetter, mDefaultColorAspects, mCodedColorAspects)
                .build());

        // TODO: support more formats?
        addParameter(
                DefineParam(mPixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
                .withConstValue(new C2StreamPixelFormatInfo::output(
                                     0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
                .build());

        // default BT.2020 static info
        C2HdrStaticMetadataStruct defaultStaticInfo;
        defaultStaticInfo.hdrType = 1.f;
        defaultStaticInfo.validFields = 0.f;
        defaultStaticInfo.mastering = {
            .red   = { .x = 0.708,  .y = 0.292 },
            .green = { .x = 0.170,  .y = 0.797 },
            .blue  = { .x = 0.131,  .y = 0.046 },
            .white = { .x = 0.3127, .y = 0.3290 },
            .maxLuminance = 1000,
            .minLuminance = 0.1,
        };
        defaultStaticInfo.maxCll = 1000;
        defaultStaticInfo.maxFall = 120;
        defaultStaticInfo.ave = {
            .ambientIlluminance = 1,
            .ambientLight = {.x = 0.0, .y = 0.0},
        };
        defaultStaticInfo.ccv = {
            .cancelFlag = 0.0,
            .persistenceFlag = 1.0,
            .primariesPresentFlag = 1.0,
            .maxLuminancePresentFlag = 1.0,
            .minLuminancePresentFlag = 1.0,
            .avgLuminancePresentFlag = 1.0,
            .red   = { .x = 0.708,  .y = 0.292 },
            .green = { .x = 0.170,  .y = 0.797 },
            .blue  = { .x = 0.131,  .y = 0.046 },
            .maxLuminance = 100,
            .minLuminance = 0.1,
            .avgLuminance = 1.0,
        };
        helper->addStructDescriptors<C2ColorXyStruct, C2MasteringDisplayColorVolumeStruct,
                C2AmbientViewingEnvironmentStruct, C2ContentColorVolumeStruct>();
        addParameter(
                DefineParam(mC2HdrStaticInfo, C2_PARAMKEY_HDR_STATIC_INFO)
                .withDefault(new C2StreamHdrStaticInfo::output(0u, defaultStaticInfo))
                .withFields({
                    C2F(mC2HdrStaticInfo, hdrType).inRange(HDRStaticInfo::kType1,
                                                          HDRStaticInfo::kType2),
                    C2F(mC2HdrStaticInfo, validFields).inRange(0, 15),
                    C2F(mC2HdrStaticInfo, mastering.red.x).inRange(kDispPrimXLow,
                                                                   kDispPrimXHigh),
                    C2F(mC2HdrStaticInfo, mastering.red.y).inRange(kDispPrimYLow,
                                                                   kDispPrimYHigh),
                    C2F(mC2HdrStaticInfo, mastering.green.x).inRange(kDispPrimXLow,
                                                                     kDispPrimXHigh),
                    C2F(mC2HdrStaticInfo, mastering.green.y).inRange(kDispPrimYLow,
                                                                     kDispPrimYHigh),
                    C2F(mC2HdrStaticInfo, mastering.blue.x).inRange(kDispPrimXLow,
                                                                    kDispPrimXHigh),
                    C2F(mC2HdrStaticInfo, mastering.blue.y).inRange(kDispPrimYLow,
                                                                    kDispPrimYHigh),
                    C2F(mC2HdrStaticInfo, mastering.white.x).inRange(kDispPrimXLow,
                                                                     kDispPrimXHigh),
                    C2F(mC2HdrStaticInfo, mastering.white.x).inRange(kDispPrimYLow,
                                                                      kDispPrimYHigh),
                    C2F(mC2HdrStaticInfo, mastering.maxLuminance).inRange(kMaxDispLuminanceLow,
                                                                          kMaxDispLuminanceHigh),
                    C2F(mC2HdrStaticInfo, mastering.minLuminance).inRange(kMinDispLuminanceLow,
                                                                          kMinDispLuminanceHigh),
                    C2F(mC2HdrStaticInfo, maxCll).inRange(kContentLightLevelLow,
                                                          kContentLightLevelHigh),
                    C2F(mC2HdrStaticInfo, maxFall).inRange(kContentLightLevelLow,
                                                           kContentLightLevelHigh),
                    C2F(mC2HdrStaticInfo, ave.ambientIlluminance).inRange(kAmbientLuminanceLow,
                                                                          kAmbientLuminanceHigh),
                    C2F(mC2HdrStaticInfo, ave.ambientLight.x).inRange(kAmbientLightLow,
                                                                      kAmbientLightHigh),
                    C2F(mC2HdrStaticInfo, ave.ambientLight.y).inRange(kAmbientLightLow,
                                                                      kAmbientLightHigh),
                    C2F(mC2HdrStaticInfo, ccv.cancelFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.persistenceFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.primariesPresentFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.maxLuminancePresentFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.minLuminancePresentFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.avgLuminancePresentFlag).inRange(0, 1),
                    C2F(mC2HdrStaticInfo, ccv.red.x).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.red.y).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.green.x).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.green.y).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.blue.x).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.blue.y).inRange(kCCVPrimLow, kCCVPrimHigh),
                    C2F(mC2HdrStaticInfo, ccv.maxLuminance).inRange(kCCVLuminanceLow,
                                                                    kCCVLuminanceHigh),
                    C2F(mC2HdrStaticInfo, ccv.minLuminance).inRange(kCCVLuminanceLow,
                                                                    kCCVLuminanceHigh),
                    C2F(mC2HdrStaticInfo, ccv.avgLuminance).inRange(kCCVLuminanceLow,
                                                                    kCCVLuminanceHigh),
                })
                .withSetter(HdrStaticInfoSetter)
            .build());
    }
    static C2R SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::output> &oldMe,
                          C2P<C2StreamPictureSizeInfo::output> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
            me.set().width = oldMe.v.width;
        }
        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
            me.set().height = oldMe.v.height;
        }
        return res;
    }

    static C2R MaxPictureSizeSetter(bool mayBlock, C2P<C2StreamMaxPictureSizeTuning::output> &me,
                                    const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        // TODO: get max width/height from the size's field helpers vs. hardcoding
        me.set().width = c2_min(c2_max(me.v.width, size.v.width), 4080u);
        me.set().height = c2_min(c2_max(me.v.height, size.v.height), 4080u);
        return C2R::Ok();
    }

    static C2R MaxInputSizeSetter(bool mayBlock, C2P<C2StreamMaxBufferSizeInfo::input> &me,
                                  const C2P<C2StreamMaxPictureSizeTuning::output> &maxSize) {
        (void)mayBlock;
        // assume compression ratio of 2
        me.set().value = (((maxSize.v.width + 15) / 16) * ((maxSize.v.height + 15) / 16) * 192);
        return C2R::Ok();
    }

    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input> &me,
                                  const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        (void)size;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    static C2R DefaultColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsTuning::output> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
                me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
                me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
                me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
                me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R CodedColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::input> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
                me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
                me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
                me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
                me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                  const C2P<C2StreamColorAspectsTuning::output> &def,
                                  const C2P<C2StreamColorAspectsInfo::input> &coded) {
        (void)mayBlock;
        // take default values for all unspecified fields, and coded values for specified ones
        me.set().range = coded.v.range == RANGE_UNSPECIFIED ? def.v.range : coded.v.range;
        me.set().primaries = coded.v.primaries == PRIMARIES_UNSPECIFIED
                ? def.v.primaries : coded.v.primaries;
        me.set().transfer = coded.v.transfer == TRANSFER_UNSPECIFIED
                ? def.v.transfer : coded.v.transfer;
        me.set().matrix = coded.v.matrix == MATRIX_UNSPECIFIED ? def.v.matrix : coded.v.matrix;
        return C2R::Ok();
    }

    static C2R HdrStaticInfoSetter(bool mayBlock, C2P<C2StreamHdrStaticInfo::output> &me) {
        UNUSED(mayBlock);
        UNUSED(me);
        return C2R::Ok();
    }

    std::shared_ptr<C2StreamColorAspectsInfo::output> getColorAspects_l() {
        return mColorAspects;
    }

    std::shared_ptr<C2StreamHdrStaticInfo::output> getHdrStaticInfo_l() const {
        return mC2HdrStaticInfo;
    }

private:
    std::shared_ptr<C2StreamProfileLevelInfo::input> mProfileLevel;
    std::shared_ptr<C2StreamPictureSizeInfo::output> mSize;
    std::shared_ptr<C2StreamMaxPictureSizeTuning::output> mMaxSize;
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> mMaxInputSize;
    std::shared_ptr<C2StreamColorInfo::output> mColorInfo;
    std::shared_ptr<C2StreamColorAspectsInfo::input> mCodedColorAspects;
    std::shared_ptr<C2StreamColorAspectsTuning::output> mDefaultColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::output> mColorAspects;
    std::shared_ptr<C2StreamPixelFormatInfo::output> mPixelFormat;
    std::shared_ptr<C2StreamHdrStaticInfo::output> mC2HdrStaticInfo;
};

static size_t getCpuCoreCount() {
    long cpuCoreCount = 1;
#if defined(_SC_NPROCESSORS_ONLN)
    cpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
#else
    // _SC_NPROC_ONLN must be defined...
    cpuCoreCount = sysconf(_SC_NPROC_ONLN);
#endif
    CHECK(cpuCoreCount >= 1);
    ALOGV("Number of CPU cores: %ld", cpuCoreCount);
    return (size_t)cpuCoreCount;
}

static void *ivd_aligned_malloc(void *ctxt, WORD32 alignment, WORD32 size) {
    (void) ctxt;
    return memalign(alignment, size);
}

static void ivd_aligned_free(void *ctxt, void *mem) {
    (void) ctxt;
    free(mem);
}

static bool getMDCV(iv_obj_t* mDecHandle, HDRStaticInfo* hdrStaticInfo) {
    WORD32 ret = IV_SUCCESS;
    ih264d_ctl_get_sei_mdcv_params_ip_t s_mdcv_ip;
    ih264d_ctl_get_sei_mdcv_params_op_t s_mdcv_op;

    memset(&s_mdcv_ip, 0, sizeof(ih264d_ctl_get_sei_mdcv_params_ip_t));
    memset(&s_mdcv_op, 0, sizeof(ih264d_ctl_get_sei_mdcv_params_op_t));

    s_mdcv_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_mdcv_ip.e_sub_cmd =
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_GET_SEI_MDCV_PARAMS;
    s_mdcv_ip.u4_size =
        sizeof(ih264d_ctl_get_sei_mdcv_params_ip_t);
    s_mdcv_op.u4_size =
        sizeof(ih264d_ctl_get_sei_mdcv_params_op_t);

    ret = ivdec_api_function(mDecHandle, (void *)&s_mdcv_ip, (void *)&s_mdcv_op);

    if (IV_SUCCESS != ret) {
        ALOGV("Failed to get MDCV params: 0x%x", s_mdcv_op.u4_error_code);
        return false;
    }

    if (hdrStaticInfo->mID == HDRStaticInfo::kType2) {
        hdrStaticInfo->sType2.mValidFields |= HDRStaticInfo::Type2::kDisplayColorVolume;
        hdrStaticInfo->sType2.mG.x = s_mdcv_op.au2_display_primaries_x[0];
        hdrStaticInfo->sType2.mB.x = s_mdcv_op.au2_display_primaries_x[1];
        hdrStaticInfo->sType2.mR.x = s_mdcv_op.au2_display_primaries_x[2];
        hdrStaticInfo->sType2.mG.y = s_mdcv_op.au2_display_primaries_y[0];
        hdrStaticInfo->sType2.mB.y = s_mdcv_op.au2_display_primaries_y[1];
        hdrStaticInfo->sType2.mR.y = s_mdcv_op.au2_display_primaries_y[2];
        hdrStaticInfo->sType2.mW.x = s_mdcv_op.u2_white_point_x;
        hdrStaticInfo->sType2.mW.y = s_mdcv_op.u2_white_point_y;
        // conversion to cd/m^2
        hdrStaticInfo->sType2.mMaxDisplayLuminance =
            (UWORD16)(s_mdcv_op.u4_max_display_mastering_luminance / 10000);
        hdrStaticInfo->sType2.mMinDisplayLuminance =
            s_mdcv_op.u4_min_display_mastering_luminance;
    }

    return true;
}

static bool getCLL(iv_obj_t* mDecHandle, HDRStaticInfo*  hdrStaticInfo) {
    WORD32 ret = IV_SUCCESS;
    ih264d_ctl_get_sei_cll_params_ip_t s_cll_ip;
    ih264d_ctl_get_sei_cll_params_op_t s_cll_op;

    memset(&s_cll_ip, 0, sizeof(ih264d_ctl_get_sei_cll_params_ip_t));
    memset(&s_cll_op, 0, sizeof(ih264d_ctl_get_sei_cll_params_op_t));

    s_cll_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_cll_ip.e_sub_cmd =
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_GET_SEI_CLL_PARAMS;
    s_cll_ip.u4_size =
        sizeof(ih264d_ctl_get_sei_cll_params_ip_t);
    s_cll_op.u4_size =
        sizeof(ih264d_ctl_get_sei_cll_params_op_t);

    ret = ivdec_api_function(mDecHandle, (void *)&s_cll_ip, (void *)&s_cll_op);

    if (IV_SUCCESS != ret) {
        ALOGV("Failed to get CLL params: 0x%x", s_cll_op.u4_error_code);
        return false;
    }

    if (hdrStaticInfo->mID == HDRStaticInfo::kType2) {
        hdrStaticInfo->sType2.mValidFields |= HDRStaticInfo::Type2::kContentLightLevel;
        hdrStaticInfo->sType2.mMaxContentLightLevel =
                              s_cll_op.u2_max_content_light_level;
        hdrStaticInfo->sType2.mMaxFrameAverageLightLevel =
                              s_cll_op.u2_max_pic_average_light_level;
    }
    return true;
}

static bool getAVE(iv_obj_t* mDecHandle, HDRStaticInfo*  hdrStaticInfo) {
    WORD32 ret = IV_SUCCESS;
    ih264d_ctl_get_sei_ave_params_ip_t s_ave_ip;
    ih264d_ctl_get_sei_ave_params_op_t s_ave_op;

    memset(&s_ave_ip, 0, sizeof(ih264d_ctl_get_sei_ave_params_ip_t));
    memset(&s_ave_op, 0, sizeof(ih264d_ctl_get_sei_ave_params_op_t));

    s_ave_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_ave_ip.e_sub_cmd =
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_GET_SEI_AVE_PARAMS;
    s_ave_ip.u4_size =
        sizeof(ih264d_ctl_get_sei_ave_params_ip_t);
    s_ave_op.u4_size =
        sizeof(ih264d_ctl_get_sei_ave_params_op_t);

    ret = ivdec_api_function(mDecHandle, (void *)&s_ave_ip, (void *)&s_ave_op);

    if (IV_SUCCESS != ret) {
        ALOGV("Failed to get AVE params: 0x%x", s_ave_op.u4_error_code);
        return false;
    }

    if (hdrStaticInfo->mID == HDRStaticInfo::kType2) {
        hdrStaticInfo->sType2.mValidFields |= HDRStaticInfo::Type2::kAmbientViewingEnv;
        hdrStaticInfo->sType2.mAmbientLight.x = s_ave_op.u2_ambient_light_x;
        hdrStaticInfo->sType2.mAmbientLight.y = s_ave_op.u2_ambient_light_y;
        hdrStaticInfo->sType2.mAmbientIlluminance = s_ave_op.u4_ambient_illuminance;
    }
    return true;
}

static bool getCCV(iv_obj_t* mDecHandle, HDRStaticInfo*  hdrStaticInfo) {
    WORD32 ret = IV_SUCCESS;
    ih264d_ctl_get_sei_ccv_params_ip_t s_ccv_ip;
    ih264d_ctl_get_sei_ccv_params_op_t s_ccv_op;

    memset(&s_ccv_ip, 0, sizeof(ih264d_ctl_get_sei_ccv_params_ip_t));
    memset(&s_ccv_op, 0, sizeof(ih264d_ctl_get_sei_ccv_params_op_t));

    s_ccv_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_ccv_ip.e_sub_cmd =
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_GET_SEI_CCV_PARAMS;
    s_ccv_ip.u4_size =
        sizeof(ih264d_ctl_get_sei_ccv_params_ip_t);
    s_ccv_op.u4_size =
        sizeof(ih264d_ctl_get_sei_ccv_params_op_t);

    ret = ivdec_api_function(mDecHandle, (void *)&s_ccv_ip, (void *)&s_ccv_op);

    if (IV_SUCCESS != ret) {
        ALOGV("Failed to get CCV params: 0x%x", s_ccv_op.u4_error_code);
        return false;
    }

    if (hdrStaticInfo->mID == HDRStaticInfo::kType2) {
        hdrStaticInfo->sType2.mValidFields |= HDRStaticInfo::Type2::kContentColorVolume;
        hdrStaticInfo->sType2.mCCVPrimariesPresentFlag =
                                s_ccv_op.u1_ccv_primaries_present_flag;

        hdrStaticInfo->sType2.mCCVG.x = s_ccv_op.ai4_ccv_primaries_x[0];
        hdrStaticInfo->sType2.mCCVB.x = s_ccv_op.ai4_ccv_primaries_x[1];
        hdrStaticInfo->sType2.mCCVR.x = s_ccv_op.ai4_ccv_primaries_x[2];
        hdrStaticInfo->sType2.mCCVG.y = s_ccv_op.ai4_ccv_primaries_y[0];
        hdrStaticInfo->sType2.mCCVB.y = s_ccv_op.ai4_ccv_primaries_y[1];
        hdrStaticInfo->sType2.mCCVR.y = s_ccv_op.ai4_ccv_primaries_y[2];

        hdrStaticInfo->sType2.mCCVMinContentLuminancePresentFlag =
            s_ccv_op.u1_ccv_min_luminance_value_present_flag;
        hdrStaticInfo->sType2.mCCVMaxContentLuminancePresentFlag =
            s_ccv_op.u1_ccv_max_luminance_value_present_flag;
        hdrStaticInfo->sType2.mCCVAvgContentLuminancePresentFlag =
            s_ccv_op.u1_ccv_avg_luminance_value_present_flag;

        hdrStaticInfo->sType2.mMinContentLuminance = s_ccv_op.u4_ccv_min_luminance_value;
        hdrStaticInfo->sType2.mMaxContentLuminance = s_ccv_op.u4_ccv_max_luminance_value;
        hdrStaticInfo->sType2.mAvgContentLuminance = s_ccv_op.u4_ccv_avg_luminance_value;

        hdrStaticInfo->sType2.mCCVCancelFlag = s_ccv_op.u1_ccv_cancel_flag;
        hdrStaticInfo->sType2.mCCVPersistenceFlag = s_ccv_op.u1_ccv_persistence_flag;
    }

    return true;
}

C2SoftAvcDec::C2SoftAvcDec(
        const char *name,
        c2_node_id_t id,
        const std::shared_ptr<IntfImpl> &intfImpl)
    : SimpleC2Component(std::make_shared<SimpleInterface<IntfImpl>>(name, id, intfImpl)),
      mIntf(intfImpl),
      mDecHandle(nullptr),
      mOutBufferFlush(nullptr),
      mIvColorFormat(IV_YUV_420P),
      mOutputDelay(kDefaultOutputDelay),
      mWidth(320),
      mHeight(240),
      mHeaderDecoded(false),
      mOutIndex(0u) {
    GENERATE_FILE_NAMES();
    CREATE_DUMP_FILE(mInFile);
}

C2SoftAvcDec::~C2SoftAvcDec() {
    onRelease();
}

c2_status_t C2SoftAvcDec::onInit() {
    status_t err = initDecoder();
    return err == OK ? C2_OK : C2_CORRUPTED;
}

c2_status_t C2SoftAvcDec::onStop() {
    if (OK != resetDecoder()) return C2_CORRUPTED;
    resetPlugin();
    return C2_OK;
}

void C2SoftAvcDec::onReset() {
    (void) onStop();
}

void C2SoftAvcDec::onRelease() {
    (void) deleteDecoder();
    if (mOutBufferFlush) {
        ivd_aligned_free(nullptr, mOutBufferFlush);
        mOutBufferFlush = nullptr;
    }
    if (mOutBlock) {
        mOutBlock.reset();
    }
}

c2_status_t C2SoftAvcDec::onFlush_sm() {
    if (OK != setFlushMode()) return C2_CORRUPTED;

    uint32_t bufferSize = mStride * mHeight * 3 / 2;
    mOutBufferFlush = (uint8_t *)ivd_aligned_malloc(nullptr, 128, bufferSize);
    if (!mOutBufferFlush) {
        ALOGE("could not allocate tmp output buffer (for flush) of size %u ", bufferSize);
        return C2_NO_MEMORY;
    }

    while (true) {
        ivd_video_decode_ip_t s_decode_ip;
        ivd_video_decode_op_t s_decode_op;

        setDecodeArgs(&s_decode_ip, &s_decode_op, nullptr, nullptr, 0, 0, 0);
        (void) ivdec_api_function(mDecHandle, &s_decode_ip, &s_decode_op);
        if (0 == s_decode_op.u4_output_present) {
            resetPlugin();
            break;
        }
    }

    if (mOutBufferFlush) {
        ivd_aligned_free(nullptr, mOutBufferFlush);
        mOutBufferFlush = nullptr;
    }

    return C2_OK;
}

status_t C2SoftAvcDec::createDecoder() {
    ivdext_create_ip_t s_create_ip;
    ivdext_create_op_t s_create_op;

    s_create_ip.s_ivd_create_ip_t.u4_size = sizeof(ivdext_create_ip_t);
    s_create_ip.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
    s_create_ip.s_ivd_create_ip_t.u4_share_disp_buf = 0;
    s_create_ip.s_ivd_create_ip_t.e_output_format = mIvColorFormat;
    s_create_ip.s_ivd_create_ip_t.pf_aligned_alloc = ivd_aligned_malloc;
    s_create_ip.s_ivd_create_ip_t.pf_aligned_free = ivd_aligned_free;
    s_create_ip.s_ivd_create_ip_t.pv_mem_ctxt = nullptr;
    s_create_op.s_ivd_create_op_t.u4_size = sizeof(ivdext_create_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(nullptr,
                                                     &s_create_ip,
                                                     &s_create_op);
    if (status != IV_SUCCESS) {
        ALOGE("error in %s: 0x%x", __func__,
              s_create_op.s_ivd_create_op_t.u4_error_code);
        return UNKNOWN_ERROR;
    }
    mDecHandle = (iv_obj_t*)s_create_op.s_ivd_create_op_t.pv_handle;
    mDecHandle->pv_fxns = (void *)ivdec_api_function;
    mDecHandle->u4_size = sizeof(iv_obj_t);

    return OK;
}

status_t C2SoftAvcDec::setNumCores() {
    ivdext_ctl_set_num_cores_ip_t s_set_num_cores_ip;
    ivdext_ctl_set_num_cores_op_t s_set_num_cores_op;

    s_set_num_cores_ip.u4_size = sizeof(ivdext_ctl_set_num_cores_ip_t);
    s_set_num_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_set_num_cores_ip.e_sub_cmd = IVDEXT_CMD_CTL_SET_NUM_CORES;
    s_set_num_cores_ip.u4_num_cores = mNumCores;
    s_set_num_cores_op.u4_size = sizeof(ivdext_ctl_set_num_cores_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_set_num_cores_ip,
                                                     &s_set_num_cores_op);
    if (IV_SUCCESS != status) {
        ALOGD("error in %s: 0x%x", __func__, s_set_num_cores_op.u4_error_code);
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t C2SoftAvcDec::setParams(size_t stride, IVD_VIDEO_DECODE_MODE_T dec_mode) {
    ivd_ctl_set_config_ip_t s_set_dyn_params_ip;
    ivd_ctl_set_config_op_t s_set_dyn_params_op;

    s_set_dyn_params_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
    s_set_dyn_params_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_set_dyn_params_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
    s_set_dyn_params_ip.u4_disp_wd = (UWORD32) stride;
    s_set_dyn_params_ip.e_frm_skip_mode = IVD_SKIP_NONE;
    s_set_dyn_params_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
    s_set_dyn_params_ip.e_vid_dec_mode = dec_mode;
    s_set_dyn_params_op.u4_size = sizeof(ivd_ctl_set_config_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_set_dyn_params_ip,
                                                     &s_set_dyn_params_op);
    if (status != IV_SUCCESS) {
        ALOGE("error in %s: 0x%x", __func__, s_set_dyn_params_op.u4_error_code);
        return UNKNOWN_ERROR;
    }

    return OK;
}

void C2SoftAvcDec::getVersion() {
    ivd_ctl_getversioninfo_ip_t s_get_versioninfo_ip;
    ivd_ctl_getversioninfo_op_t s_get_versioninfo_op;
    UWORD8 au1_buf[512];

    s_get_versioninfo_ip.u4_size = sizeof(ivd_ctl_getversioninfo_ip_t);
    s_get_versioninfo_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_get_versioninfo_ip.e_sub_cmd = IVD_CMD_CTL_GETVERSION;
    s_get_versioninfo_ip.pv_version_buffer = au1_buf;
    s_get_versioninfo_ip.u4_version_buffer_size = sizeof(au1_buf);
    s_get_versioninfo_op.u4_size = sizeof(ivd_ctl_getversioninfo_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_get_versioninfo_ip,
                                                     &s_get_versioninfo_op);
    if (status != IV_SUCCESS) {
        ALOGD("error in %s: 0x%x", __func__,
              s_get_versioninfo_op.u4_error_code);
    } else {
        ALOGV("ittiam decoder version number: %s",
              (char *) s_get_versioninfo_ip.pv_version_buffer);
    }
}

bool C2SoftAvcDec::getHDRStaticParams(ivd_video_decode_op_t *ps_decode_op,
                                      const std::unique_ptr<C2Work> &work) {
    HDRStaticInfo hdrStaticInfoLocal;
    memset(&hdrStaticInfoLocal, 0, sizeof(HDRStaticInfo));
    hdrStaticInfoLocal.mID = HDRStaticInfo::kType2;
    // Get mastering display color volume parameters if they have changed
    if (1 == ps_decode_op->s_sei_decode_op.u1_sei_mdcv_params_present_flag) {
        if(!getMDCV(mDecHandle,&hdrStaticInfoLocal)) {
            ALOGV("Unable to find MDCV SEI params");
        }
    }

    // Get content light level parameters if they have changed
    if (1 == ps_decode_op->s_sei_decode_op.u1_sei_cll_params_present_flag) {
        if(!getCLL(mDecHandle,&hdrStaticInfoLocal)) {
            ALOGV("Unable to find CLL SEI params");
        }
    }

    // Get ambient viewing environment parameters if they have changed
    if (1 == ps_decode_op->s_sei_decode_op.u1_sei_ave_params_present_flag) {
        if(!getAVE(mDecHandle,&hdrStaticInfoLocal)) {
            ALOGV("Unable to find AVE SEI params");
        }
    }

    // Get content color volume parameters if they have changed
    if (1 == ps_decode_op->s_sei_decode_op.u1_sei_ccv_params_present_flag) {
        if(!getCCV(mDecHandle,&hdrStaticInfoLocal)) {
            ALOGV("Unable to find AVE SEI params");
        }
    }

    // Check if any of the fields has changed
    if (0 != memcmp(&mHdrStaticInfo.sType2, &hdrStaticInfoLocal.sType2,
                                              SIZE_HDRSTATICINFO_TYPE2)) {
        mHdrStaticInfo.mID = HDRStaticInfo::kType2;
        mHdrStaticInfo.sType2 = hdrStaticInfoLocal.sType2;

        //Convert HDRStaticInfo to C2StreamHdrStaticInfo
        {
            C2StreamHdrStaticInfo::output c2HdrStaticInfo;

            c2HdrStaticInfo.hdrType = mHdrStaticInfo.mID;
            c2HdrStaticInfo.validFields = mHdrStaticInfo.sType2.mValidFields;
            c2HdrStaticInfo.mastering.red.x = mHdrStaticInfo.sType2.mR.x * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.red.y = mHdrStaticInfo.sType2.mR.y * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.green.x = mHdrStaticInfo.sType2.mG.x * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.green.y = mHdrStaticInfo.sType2.mG.y * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.blue.x = mHdrStaticInfo.sType2.mB.x * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.blue.y = mHdrStaticInfo.sType2.mB.y * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.white.x = mHdrStaticInfo.sType2.mW.x * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.white.y = mHdrStaticInfo.sType2.mW.y * kNormDispPrimaries;
            c2HdrStaticInfo.mastering.maxLuminance = mHdrStaticInfo.sType2.mMaxDisplayLuminance;
            c2HdrStaticInfo.mastering.minLuminance =
                    mHdrStaticInfo.sType2.mMinDisplayLuminance * kNormDispLuminance;
            c2HdrStaticInfo.maxCll = mHdrStaticInfo.sType2.mMaxContentLightLevel;
            c2HdrStaticInfo.maxFall = mHdrStaticInfo.sType2.mMaxFrameAverageLightLevel;
            c2HdrStaticInfo.ave.ambientIlluminance = mHdrStaticInfo.sType2.mAmbientIlluminance;
            c2HdrStaticInfo.ave.ambientLight.x =
                    mHdrStaticInfo.sType2.mAmbientLight.x * kNormAmbientLight;
            c2HdrStaticInfo.ave.ambientLight.y =
                    mHdrStaticInfo.sType2.mAmbientLight.y * kNormAmbientLight;
            c2HdrStaticInfo.ccv.cancelFlag = mHdrStaticInfo.sType2.mCCVCancelFlag;
            c2HdrStaticInfo.ccv.persistenceFlag = mHdrStaticInfo.sType2.mCCVPersistenceFlag;
            c2HdrStaticInfo.ccv.primariesPresentFlag =
                    mHdrStaticInfo.sType2.mCCVPrimariesPresentFlag;
            c2HdrStaticInfo.ccv.maxLuminancePresentFlag =
                    mHdrStaticInfo.sType2.mCCVMaxContentLuminancePresentFlag;
            c2HdrStaticInfo.ccv.minLuminancePresentFlag =
                    mHdrStaticInfo.sType2.mCCVMinContentLuminancePresentFlag;
            c2HdrStaticInfo.ccv.avgLuminancePresentFlag =
                    mHdrStaticInfo.sType2.mCCVAvgContentLuminancePresentFlag;
            c2HdrStaticInfo.ccv.red.x = mHdrStaticInfo.sType2.mCCVR.x * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.red.y = mHdrStaticInfo.sType2.mCCVR.y * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.green.x = mHdrStaticInfo.sType2.mCCVG.x * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.green.y = mHdrStaticInfo.sType2.mCCVG.y * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.blue.x = mHdrStaticInfo.sType2.mCCVB.x * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.blue.y = mHdrStaticInfo.sType2.mCCVB.y * kNormCCVPrimaries;
            c2HdrStaticInfo.ccv.maxLuminance =
                    mHdrStaticInfo.sType2.mMaxContentLuminance * kNormCCVLuminance;
            c2HdrStaticInfo.ccv.minLuminance =
                    mHdrStaticInfo.sType2.mMinContentLuminance * kNormCCVLuminance;
            c2HdrStaticInfo.ccv.avgLuminance =
                    mHdrStaticInfo.sType2.mAvgContentLuminance * kNormCCVLuminance;

            std::vector<std::unique_ptr<C2SettingResult>> failures;
            c2_status_t err = mIntf->config({&c2HdrStaticInfo}, C2_MAY_BLOCK, &failures);
            if (err == OK) {
                work->worklets.front()->output.configUpdate.push_back(
                    C2Param::Copy(c2HdrStaticInfo));
            } else {
                ALOGE("Cannot set HDR static params");
                return false;
            }
        }

        ALOGV("Updated HDR static params!");
    }

    return true;
}

status_t C2SoftAvcDec::initDecoder() {
    if (OK != createDecoder()) return UNKNOWN_ERROR;
    mNumCores = MIN(getCpuCoreCount(), MAX_NUM_CORES);
    mStride = ALIGN128(mWidth);
    mSignalledError = false;
    resetPlugin();
    (void) setNumCores();
    if (OK != setParams(mStride, IVD_DECODE_FRAME)) return UNKNOWN_ERROR;
    (void) getVersion();
    memset(&mHdrStaticInfo, 0, sizeof(HDRStaticInfo));

    return OK;
}

bool C2SoftAvcDec::setDecodeArgs(ivd_video_decode_ip_t *ps_decode_ip,
                                 ivd_video_decode_op_t *ps_decode_op,
                                 C2ReadView *inBuffer,
                                 C2GraphicView *outBuffer,
                                 size_t inOffset,
                                 size_t inSize,
                                 uint32_t tsMarker) {
    uint32_t displayStride = mStride;
    uint32_t displayHeight = mHeight;
    size_t lumaSize = displayStride * displayHeight;
    size_t chromaSize = lumaSize >> 2;

    ps_decode_ip->u4_size = sizeof(ivd_video_decode_ip_t);
    ps_decode_ip->e_cmd = IVD_CMD_VIDEO_DECODE;
    if (inBuffer) {
        ps_decode_ip->u4_ts = tsMarker;
        ps_decode_ip->pv_stream_buffer = const_cast<uint8_t *>(inBuffer->data() + inOffset);
        ps_decode_ip->u4_num_Bytes = inSize;
    } else {
        ps_decode_ip->u4_ts = 0;
        ps_decode_ip->pv_stream_buffer = nullptr;
        ps_decode_ip->u4_num_Bytes = 0;
    }
    ps_decode_ip->s_out_buffer.u4_min_out_buf_size[0] = lumaSize;
    ps_decode_ip->s_out_buffer.u4_min_out_buf_size[1] = chromaSize;
    ps_decode_ip->s_out_buffer.u4_min_out_buf_size[2] = chromaSize;
    if (outBuffer) {
        if (outBuffer->width() < displayStride || outBuffer->height() < displayHeight) {
            ALOGE("Output buffer too small: provided (%dx%d) required (%ux%u)",
                  outBuffer->width(), outBuffer->height(), displayStride, displayHeight);
            return false;
        }
        ps_decode_ip->s_out_buffer.pu1_bufs[0] = outBuffer->data()[C2PlanarLayout::PLANE_Y];
        ps_decode_ip->s_out_buffer.pu1_bufs[1] = outBuffer->data()[C2PlanarLayout::PLANE_U];
        ps_decode_ip->s_out_buffer.pu1_bufs[2] = outBuffer->data()[C2PlanarLayout::PLANE_V];
    } else {
        ps_decode_ip->s_out_buffer.pu1_bufs[0] = mOutBufferFlush;
        ps_decode_ip->s_out_buffer.pu1_bufs[1] = mOutBufferFlush + lumaSize;
        ps_decode_ip->s_out_buffer.pu1_bufs[2] = mOutBufferFlush + lumaSize + chromaSize;
    }
    ps_decode_ip->s_out_buffer.u4_num_bufs = 3;
    ps_decode_op->u4_size = sizeof(ivd_video_decode_op_t);

    return true;
}

bool C2SoftAvcDec::getVuiParams() {
    ivdext_ctl_get_vui_params_ip_t s_get_vui_params_ip;
    ivdext_ctl_get_vui_params_op_t s_get_vui_params_op;

    s_get_vui_params_ip.u4_size = sizeof(ivdext_ctl_get_vui_params_ip_t);
    s_get_vui_params_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_get_vui_params_ip.e_sub_cmd =
            (IVD_CONTROL_API_COMMAND_TYPE_T) IH264D_CMD_CTL_GET_VUI_PARAMS;
    s_get_vui_params_op.u4_size = sizeof(ivdext_ctl_get_vui_params_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_get_vui_params_ip,
                                                     &s_get_vui_params_op);
    if (status != IV_SUCCESS) {
        ALOGD("error in %s: 0x%x", __func__, s_get_vui_params_op.u4_error_code);
        return false;
    }

    VuiColorAspects vuiColorAspects;
    vuiColorAspects.primaries = s_get_vui_params_op.u1_colour_primaries;
    vuiColorAspects.transfer = s_get_vui_params_op.u1_tfr_chars;
    vuiColorAspects.coeffs = s_get_vui_params_op.u1_matrix_coeffs;
    vuiColorAspects.fullRange = s_get_vui_params_op.u1_video_full_range_flag;

    // convert vui aspects to C2 values if changed
    if (!(vuiColorAspects == mBitstreamColorAspects)) {
        mBitstreamColorAspects = vuiColorAspects;
        ColorAspects sfAspects;
        C2StreamColorAspectsInfo::input codedAspects = { 0u };
        ColorUtils::convertIsoColorAspectsToCodecAspects(
                vuiColorAspects.primaries, vuiColorAspects.transfer, vuiColorAspects.coeffs,
                vuiColorAspects.fullRange, sfAspects);
        if (!C2Mapper::map(sfAspects.mPrimaries, &codedAspects.primaries)) {
            codedAspects.primaries = C2Color::PRIMARIES_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mRange, &codedAspects.range)) {
            codedAspects.range = C2Color::RANGE_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mMatrixCoeffs, &codedAspects.matrix)) {
            codedAspects.matrix = C2Color::MATRIX_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mTransfer, &codedAspects.transfer)) {
            codedAspects.transfer = C2Color::TRANSFER_UNSPECIFIED;
        }
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        (void)mIntf->config({&codedAspects}, C2_MAY_BLOCK, &failures);
    }
    return true;
}

status_t C2SoftAvcDec::setFlushMode() {
    ivd_ctl_flush_ip_t s_set_flush_ip;
    ivd_ctl_flush_op_t s_set_flush_op;

    s_set_flush_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
    s_set_flush_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_set_flush_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
    s_set_flush_op.u4_size = sizeof(ivd_ctl_flush_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_set_flush_ip,
                                                     &s_set_flush_op);
    if (status != IV_SUCCESS) {
        ALOGE("error in %s: 0x%x", __func__, s_set_flush_op.u4_error_code);
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t C2SoftAvcDec::resetDecoder() {
    ivd_ctl_reset_ip_t s_reset_ip;
    ivd_ctl_reset_op_t s_reset_op;

    s_reset_ip.u4_size = sizeof(ivd_ctl_reset_ip_t);
    s_reset_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_reset_ip.e_sub_cmd = IVD_CMD_CTL_RESET;
    s_reset_op.u4_size = sizeof(ivd_ctl_reset_op_t);
    IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                     &s_reset_ip,
                                                     &s_reset_op);
    if (IV_SUCCESS != status) {
        ALOGE("error in %s: 0x%x", __func__, s_reset_op.u4_error_code);
        return UNKNOWN_ERROR;
    }
    mStride = 0;
    (void) setNumCores();
    mSignalledError = false;
    mHeaderDecoded = false;

    return OK;
}

void C2SoftAvcDec::resetPlugin() {
    mSignalledOutputEos = false;
    gettimeofday(&mTimeStart, nullptr);
    gettimeofday(&mTimeEnd, nullptr);
}

status_t C2SoftAvcDec::deleteDecoder() {
    if (mDecHandle) {
        ivdext_delete_ip_t s_delete_ip;
        ivdext_delete_op_t s_delete_op;

        s_delete_ip.s_ivd_delete_ip_t.u4_size = sizeof(ivdext_delete_ip_t);
        s_delete_ip.s_ivd_delete_ip_t.e_cmd = IVD_CMD_DELETE;
        s_delete_op.s_ivd_delete_op_t.u4_size = sizeof(ivdext_delete_op_t);
        IV_API_CALL_STATUS_T status = ivdec_api_function(mDecHandle,
                                                         &s_delete_ip,
                                                         &s_delete_op);
        if (status != IV_SUCCESS) {
            ALOGE("error in %s: 0x%x", __func__,
                  s_delete_op.s_ivd_delete_op_t.u4_error_code);
            return UNKNOWN_ERROR;
        }
        mDecHandle = nullptr;
    }

    return OK;
}

static void fillEmptyWork(const std::unique_ptr<C2Work> &work) {
    uint32_t flags = 0;
    if (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) {
        flags |= C2FrameData::FLAG_END_OF_STREAM;
        ALOGV("signalling eos");
    }
    work->worklets.front()->output.flags = (C2FrameData::flags_t)flags;
    work->worklets.front()->output.buffers.clear();
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->workletsProcessed = 1u;
}

void C2SoftAvcDec::finishWork(uint64_t index, const std::unique_ptr<C2Work> &work) {
    std::shared_ptr<C2Buffer> buffer = createGraphicBuffer(std::move(mOutBlock),
                                                           C2Rect(mWidth, mHeight));
    mOutBlock = nullptr;
    {
        IntfImpl::Lock lock = mIntf->lock();
        buffer->setInfo(mIntf->getColorAspects_l());
    }

    class FillWork {
       public:
        FillWork(uint32_t flags, C2WorkOrdinalStruct ordinal,
                 const std::shared_ptr<C2Buffer>& buffer)
            : mFlags(flags), mOrdinal(ordinal), mBuffer(buffer) {}
        ~FillWork() = default;

        void operator()(const std::unique_ptr<C2Work>& work) {
            work->worklets.front()->output.flags = (C2FrameData::flags_t)mFlags;
            work->worklets.front()->output.buffers.clear();
            work->worklets.front()->output.ordinal = mOrdinal;
            work->workletsProcessed = 1u;
            work->result = C2_OK;
            if (mBuffer) {
                work->worklets.front()->output.buffers.push_back(mBuffer);
            }
            ALOGV("timestamp = %lld, index = %lld, w/%s buffer",
                  mOrdinal.timestamp.peekll(), mOrdinal.frameIndex.peekll(),
                  mBuffer ? "" : "o");
        }

       private:
        const uint32_t mFlags;
        const C2WorkOrdinalStruct mOrdinal;
        const std::shared_ptr<C2Buffer> mBuffer;
    };

    auto fillWork = [buffer](const std::unique_ptr<C2Work> &work) {
        work->worklets.front()->output.flags = (C2FrameData::flags_t)0;
        work->worklets.front()->output.buffers.clear();
        work->worklets.front()->output.buffers.push_back(buffer);
        work->worklets.front()->output.ordinal = work->input.ordinal;
        work->workletsProcessed = 1u;
    };
    if (work && c2_cntr64_t(index) == work->input.ordinal.frameIndex) {
        bool eos = ((work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0);
        // TODO: Check if cloneAndSend can be avoided by tracking number of frames remaining
        if (eos) {
            if (buffer) {
                mOutIndex = index;
                C2WorkOrdinalStruct outOrdinal = work->input.ordinal;
                cloneAndSend(
                    mOutIndex, work,
                    FillWork(C2FrameData::FLAG_INCOMPLETE, outOrdinal, buffer));
                buffer.reset();
            }
        } else {
            fillWork(work);
        }
    } else {
        finish(index, fillWork);
    }
}

c2_status_t C2SoftAvcDec::ensureDecoderState(const std::shared_ptr<C2BlockPool> &pool) {
    if (!mDecHandle) {
        ALOGE("not supposed to be here, invalid decoder context");
        return C2_CORRUPTED;
    }
    if (mStride != ALIGN128(mWidth)) {
        mStride = ALIGN128(mWidth);
        if (OK != setParams(mStride, IVD_DECODE_FRAME)) return C2_CORRUPTED;
    }
    if (mOutBlock &&
            (mOutBlock->width() != mStride || mOutBlock->height() != mHeight)) {
        mOutBlock.reset();
    }
    if (!mOutBlock) {
        uint32_t format = HAL_PIXEL_FORMAT_YV12;
        C2MemoryUsage usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        c2_status_t err = pool->fetchGraphicBlock(mStride, mHeight, format, usage, &mOutBlock);
        if (err != C2_OK) {
            ALOGE("fetchGraphicBlock for Output failed with status %d", err);
            return err;
        }
        ALOGV("provided (%dx%d) required (%dx%d)",
              mOutBlock->width(), mOutBlock->height(), mStride, mHeight);
    }

    return C2_OK;
}

// TODO: can overall error checking be improved?
// TODO: allow configuration of color format and usage for graphic buffers instead
//       of hard coding them to HAL_PIXEL_FORMAT_YV12
// TODO: pass coloraspects information to surface
// TODO: test support for dynamic change in resolution
// TODO: verify if the decoder sent back all frames
void C2SoftAvcDec::process(
        const std::unique_ptr<C2Work> &work,
        const std::shared_ptr<C2BlockPool> &pool) {
    // Initialize output work
    work->result = C2_OK;
    work->workletsProcessed = 0u;
    work->worklets.front()->output.flags = work->input.flags;
    if (mSignalledError || mSignalledOutputEos) {
        work->result = C2_BAD_VALUE;
        return;
    }

    size_t inOffset = 0u;
    size_t inSize = 0u;
    uint32_t workIndex = work->input.ordinal.frameIndex.peeku() & 0xFFFFFFFF;
    C2ReadView rView = mDummyReadView;
    if (!work->input.buffers.empty()) {
        rView = work->input.buffers[0]->data().linearBlocks().front().map().get();
        inSize = rView.capacity();
        if (inSize && rView.error()) {
            ALOGE("read view map failed %d", rView.error());
            work->result = rView.error();
            return;
        }
    }
    bool eos = ((work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0);
    bool hasPicture = false;

    ALOGV("in buffer attr. size %zu timestamp %d frameindex %d, flags %x",
          inSize, (int)work->input.ordinal.timestamp.peeku(),
          (int)work->input.ordinal.frameIndex.peeku(), work->input.flags);
    size_t inPos = 0;
    while (inPos < inSize) {
        if (C2_OK != ensureDecoderState(pool)) {
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return;
        }

        ivd_video_decode_ip_t s_decode_ip;
        ivd_video_decode_op_t s_decode_op;
        {
            C2GraphicView wView = mOutBlock->map().get();
            if (wView.error()) {
                ALOGE("graphic view map failed %d", wView.error());
                work->result = wView.error();
                return;
            }
            if (!setDecodeArgs(&s_decode_ip, &s_decode_op, &rView, &wView,
                               inOffset + inPos, inSize - inPos, workIndex)) {
                mSignalledError = true;
                work->workletsProcessed = 1u;
                work->result = C2_CORRUPTED;
                return;
            }

            if (false == mHeaderDecoded) {
                /* Decode header and get dimensions */
                setParams(mStride, IVD_DECODE_HEADER);
            }

            WORD32 delay;
            GETTIME(&mTimeStart, nullptr);
            TIME_DIFF(mTimeEnd, mTimeStart, delay);
            (void) ivdec_api_function(mDecHandle, &s_decode_ip, &s_decode_op);
            WORD32 decodeTime;
            GETTIME(&mTimeEnd, nullptr);
            TIME_DIFF(mTimeStart, mTimeEnd, decodeTime);
            ALOGV("decodeTime=%6d delay=%6d numBytes=%6d", decodeTime, delay,
                  s_decode_op.u4_num_bytes_consumed);
        }
        if (IVD_MEM_ALLOC_FAILED == (s_decode_op.u4_error_code & IVD_ERROR_MASK)) {
            ALOGE("allocation failure in decoder");
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return;
        } else if (IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED == (s_decode_op.u4_error_code & IVD_ERROR_MASK)) {
            ALOGE("unsupported resolution : %dx%d", mWidth, mHeight);
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return;
        } else if (IVD_RES_CHANGED == (s_decode_op.u4_error_code & IVD_ERROR_MASK)) {
            ALOGV("resolution changed");
            drainInternal(DRAIN_COMPONENT_NO_EOS, pool, work);
            resetDecoder();
            resetPlugin();
            work->workletsProcessed = 0u;

            /* Decode header and get new dimensions */
            setParams(mStride, IVD_DECODE_HEADER);
            (void) ivdec_api_function(mDecHandle, &s_decode_ip, &s_decode_op);
        } else if (IS_IVD_FATAL_ERROR(s_decode_op.u4_error_code)) {
            ALOGE("Fatal error in decoder 0x%x", s_decode_op.u4_error_code);
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return;
        }

        /* call getHDRStaticParams */
        if (!getHDRStaticParams(&s_decode_op, work)) {
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return;
        }

        if (s_decode_op.i4_reorder_depth >= 0 && mOutputDelay != s_decode_op.i4_reorder_depth) {
            mOutputDelay = s_decode_op.i4_reorder_depth;
            ALOGV("New Output delay %d ", mOutputDelay);

            C2PortActualDelayTuning::output outputDelay(mOutputDelay);
            std::vector<std::unique_ptr<C2SettingResult>> failures;
            c2_status_t err =
                mIntf->config({&outputDelay}, C2_MAY_BLOCK, &failures);
            if (err == OK) {
                work->worklets.front()->output.configUpdate.push_back(
                    C2Param::Copy(outputDelay));
            } else {
                ALOGE("Cannot set output delay");
                mSignalledError = true;
                work->workletsProcessed = 1u;
                work->result = C2_CORRUPTED;
                return;
            }
            continue;
        }
        if (0 < s_decode_op.u4_pic_wd && 0 < s_decode_op.u4_pic_ht) {
            if (mHeaderDecoded == false) {
                mHeaderDecoded = true;
                setParams(ALIGN128(s_decode_op.u4_pic_wd), IVD_DECODE_FRAME);
            }
            if (s_decode_op.u4_pic_wd != mWidth || s_decode_op.u4_pic_ht != mHeight) {
                mWidth = s_decode_op.u4_pic_wd;
                mHeight = s_decode_op.u4_pic_ht;
                CHECK_EQ(0u, s_decode_op.u4_output_present);

                C2StreamPictureSizeInfo::output size(0u, mWidth, mHeight);
                std::vector<std::unique_ptr<C2SettingResult>> failures;
                c2_status_t err = mIntf->config({&size}, C2_MAY_BLOCK, &failures);
                if (err == OK) {
                    work->worklets.front()->output.configUpdate.push_back(
                        C2Param::Copy(size));
                } else {
                    ALOGE("Cannot set width and height");
                    mSignalledError = true;
                    work->workletsProcessed = 1u;
                    work->result = C2_CORRUPTED;
                    return;
                }
                continue;
            }
        }
        (void)getVuiParams();
        hasPicture |= (1 == s_decode_op.u4_frame_decoded_flag);
        if (s_decode_op.u4_output_present) {
            finishWork(s_decode_op.u4_ts, work);
        }
        if (0 == s_decode_op.u4_num_bytes_consumed) {
            ALOGD("Bytes consumed is zero. Ignoring remaining bytes");
            break;
        }
        inPos += s_decode_op.u4_num_bytes_consumed;
        if (hasPicture && (inSize - inPos)) {
            ALOGD("decoded frame in current access nal, ignoring further trailing bytes %d",
                  (int)inSize - (int)inPos);
            break;
        }
    }
    if (eos) {
        drainInternal(DRAIN_COMPONENT_WITH_EOS, pool, work);
        mSignalledOutputEos = true;
    } else if (!hasPicture) {
        fillEmptyWork(work);
    }

    work->input.buffers.clear();
}

c2_status_t C2SoftAvcDec::drainInternal(
        uint32_t drainMode,
        const std::shared_ptr<C2BlockPool> &pool,
        const std::unique_ptr<C2Work> &work) {
    if (drainMode == NO_DRAIN) {
        ALOGW("drain with NO_DRAIN: no-op");
        return C2_OK;
    }
    if (drainMode == DRAIN_CHAIN) {
        ALOGW("DRAIN_CHAIN not supported");
        return C2_OMITTED;
    }

    if (OK != setFlushMode()) return C2_CORRUPTED;
    while (true) {
        if (C2_OK != ensureDecoderState(pool)) {
            mSignalledError = true;
            work->workletsProcessed = 1u;
            work->result = C2_CORRUPTED;
            return C2_CORRUPTED;
        }
        C2GraphicView wView = mOutBlock->map().get();
        if (wView.error()) {
            ALOGE("graphic view map failed %d", wView.error());
            return C2_CORRUPTED;
        }
        ivd_video_decode_ip_t s_decode_ip;
        ivd_video_decode_op_t s_decode_op;
        if (!setDecodeArgs(&s_decode_ip, &s_decode_op, nullptr, &wView, 0, 0, 0)) {
            mSignalledError = true;
            work->workletsProcessed = 1u;
            return C2_CORRUPTED;
        }
        (void) ivdec_api_function(mDecHandle, &s_decode_ip, &s_decode_op);
        if (s_decode_op.u4_output_present) {
            finishWork(s_decode_op.u4_ts, work);
        } else {
            fillEmptyWork(work);
            break;
        }
    }

    return C2_OK;
}

c2_status_t C2SoftAvcDec::drain(
        uint32_t drainMode,
        const std::shared_ptr<C2BlockPool> &pool) {
    return drainInternal(drainMode, pool, nullptr);
}

class C2SoftAvcDecFactory : public C2ComponentFactory {
public:
    C2SoftAvcDecFactory() : mHelper(std::static_pointer_cast<C2ReflectorHelper>(
        GetCodec2PlatformComponentStore()->getParamReflector())) {
    }

    virtual c2_status_t createComponent(
            c2_node_id_t id,
            std::shared_ptr<C2Component>* const component,
            std::function<void(C2Component*)> deleter) override {
        *component = std::shared_ptr<C2Component>(
                new C2SoftAvcDec(COMPONENT_NAME,
                                 id,
                                 std::make_shared<C2SoftAvcDec::IntfImpl>(mHelper)),
                deleter);
        return C2_OK;
    }

    virtual c2_status_t createInterface(
            c2_node_id_t id,
            std::shared_ptr<C2ComponentInterface>* const interface,
            std::function<void(C2ComponentInterface*)> deleter) override {
        *interface = std::shared_ptr<C2ComponentInterface>(
                new SimpleInterface<C2SoftAvcDec::IntfImpl>(
                        COMPONENT_NAME, id, std::make_shared<C2SoftAvcDec::IntfImpl>(mHelper)),
                deleter);
        return C2_OK;
    }

    virtual ~C2SoftAvcDecFactory() override = default;

private:
    std::shared_ptr<C2ReflectorHelper> mHelper;
};

}  // namespace android

extern "C" ::C2ComponentFactory* CreateCodec2Factory() {
    ALOGV("in %s", __func__);
    return new ::android::C2SoftAvcDecFactory();
}

extern "C" void DestroyCodec2Factory(::C2ComponentFactory* factory) {
    ALOGV("in %s", __func__);
    delete factory;
}
