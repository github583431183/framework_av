/*
 * Copyright 2014, The Android Open Source Project
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

#ifndef CODEC_CAPABILITIES_H_

#define CODEC_CAPABILITIES_H_

#include <media/CodecCapabilitiesUtils.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodecConstants.h>

#include <system/audio.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/StrongPointer.h>

namespace android {
    struct Feature {
        std::string mName;
        int mValue;
        bool mDefault;
        bool mInternal;
        Feature(std::string name, int value, bool def, bool internal) {
            mName = name;
            mValue = value;
            mDefault = def;
            mInternal = internal;
        }
        Feature(std::string name, int value, bool def) {
            Feature(name, value, def, false /* internal */);
        }
    };

    struct CodecCapabilities;

    struct XCapabilitiesBase {
    protected:
        /**
         * Set mError of CodecCapabilities.
         *
         * @param error error code
         */
        void setParentError(int error);

        std::weak_ptr<CodecCapabilities> mParent;
    };

    struct AudioCapabilities : XCapabilitiesBase {
        /**
         * Create AudioCapabilities.
         */
        static std::shared_ptr<AudioCapabilities> Create(const sp<AMessage> &format,
                std::shared_ptr<CodecCapabilities> parent);

        /**
         * Returns the range of supported bitrates in bits/second.
         */
        Range<int> getBitrateRange() const;

        /**
         * Returns the array of supported sample rates if the codec
         * supports only discrete values. Otherwise, it returns an empty array.
         * The array is sorted in ascending order.
         */
        std::vector<int> getSupportedSampleRates() const;

        /**
         * Returns the array of supported sample rate ranges.  The
         * array is sorted in ascending order, and the ranges are
         * distinct.
         */
        std::vector<Range<int>> getSupportedSampleRateRanges() const;

        /**
         * Returns the maximum number of input channels supported.
         * The returned value should be between 1 and 255.
         *
         * Through {@link android.os.Build.VERSION_CODES#R}, this method indicated support
         * for any number of input channels between 1 and this maximum value.
         *
         * As of {@link android.os.Build.VERSION_CODES#S},
         * the implied lower limit of 1 channel is no longer valid.
         * As of {@link android.os.Build.VERSION_CODES#S}, {@link #getMaxInputChannelCount} is
         * superseded by {@link #getInputChannelCountRanges},
         * which returns an array of ranges of channels.
         * The {@link #getMaxInputChannelCount} method will return the highest value
         * in the ranges returned by {@link #getInputChannelCountRanges}
         */
        int getMaxInputChannelCount() const;

        /**
         * Returns the minimum number of input channels supported.
         * This is often 1, but does vary for certain mime types.
         *
         * This returns the lowest channel count in the ranges returned by
         * {@link #getInputChannelCountRanges}.
         */
        int getMinInputChannelCount() const;

        /**
         * Returns an array of ranges representing the number of input channels supported.
         * The codec supports any number of input channels within this range.
         *
         * This supersedes the {@link #getMaxInputChannelCount} method.
         *
         * For many codecs, this will be a single range [1..N], for some N.
         *
         * The returned array cannot be empty.
         */
        std::vector<Range<int>> getInputChannelCountRanges() const;

        /**
         * Query whether the sample rate is supported by the codec.
         */
        bool isSampleRateSupported(int sampleRate);

        /* For internal use only. Not exposed as a public API */
        void getDefaultFormat(sp<AMessage> &format);

        /* For internal use only. Not exposed as a public API */
        bool supportsFormat(const sp<AMessage> &format);

    private:
        Range<int> mBitrateRange;

        std::vector<int> mSampleRates;
        std::vector<Range<int>> mSampleRateRanges;
        std::vector<Range<int>> mInputChannelRanges;

        static constexpr int MAX_INPUT_CHANNEL_COUNT = 30;
        static constexpr uint32_t MAX_NUM_CHANNELS = FCC_LIMIT;

        /* no public constructor */
        AudioCapabilities() {};
        void init(const sp<AMessage> &format, std::shared_ptr<CodecCapabilities> parent);
        void initWithPlatformLimits();
        bool supports(int sampleRate, int inputChannels);
        void limitSampleRates(std::vector<int> rates);
        void createDiscreteSampleRates();
        void limitSampleRates(std::vector<Range<int>> rateRanges);
        void applyLevelLimits();
        void applyLimits(const std::vector<Range<int>> &inputChannels,
                const std::optional<Range<int>> &bitRates);
        void parseFromInfo(const sp<AMessage> &format);
    };

    struct VideoCapabilities : XCapabilitiesBase {
        struct PerformancePoint {
            /**
             * Maximum number of macroblocks in the frame.
             *
             * Video frames are conceptually divided into 16-by-16 pixel blocks called macroblocks.
             * Most coding standards operate on these 16-by-16 pixel blocks; thus, codec performance
             * is characterized using such blocks.
             *
             * Test API
             */
            int getMaxMacroBlocks() const;

            /**
             * Maximum frame rate in frames per second.
             *
             * Test API
             */
            int getMaxFrameRate() const;

            /**
             * Maximum number of macroblocks processed per second.
             *
             * Test API
             */
            long getMaxMacroBlockRate() const;

            /**
             * convert to a debug string
             */
            // Be careful about the serializable compatibility across API revisions.
            std::string toString() const;

            int hashCode() const;

            /**
             * Create a detailed performance point with custom max frame rate and macroblock size.
             *
             * @param width  frame width in pixels
             * @param height frame height in pixels
             * @param frameRate frames per second for frame width and height
             * @param maxFrameRate maximum frames per second for any frame size
             * @param blockSize block size for codec implementation. Must be powers of two in both
             *        width and height.
             *
             * Test API
             */
            PerformancePoint(int width, int height, int frameRate, int maxFrameRate,
                    VideoSize blockSize);

            /**
             * Convert a performance point to a larger blocksize.
             *
             * @param pp performance point. NonNull
             * @param blockSize block size for codec implementation. NonNull.
             *
             * Test API
             */
            PerformancePoint(const PerformancePoint &pp, VideoSize newBlockSize);

            /**
             * Create a performance point for a given frame size and frame rate.
             *
             * @param width width of the frame in pixels
             * @param height height of the frame in pixels
             * @param frameRate frame rate in frames per second
             */
            PerformancePoint(int width, int height, int frameRate);

            /**
             * Checks whether the performance point covers a media format.
             *
             * @param format Stream format considered
             *
             * @return {@code true} if the performance point covers the format.
             */
            bool covers(const sp<AMessage> &format) const;

            /**
             * Checks whether the performance point covers another performance point. Use this
             * method to determine if a performance point advertised by a codec covers the
             * performance point required. This method can also be used for loose ordering as this
             * method is transitive.
             *
             * @param other other performance point considered
             *
             * @return {@code true} if the performance point covers the other.
             */
            bool covers(const PerformancePoint &other) const;

            /**
             * Check if two PerformancePoint instances are equal.
             *
             * @param other other PerformancePoint instance for comparison.
             *
             * @return true if two PerformancePoint are equal.
             */
            bool equals(const PerformancePoint &other) const;

        private:
            VideoSize mBlockSize; // codec block size in macroblocks
            int mWidth; // width in macroblocks
            int mHeight; // height in macroblocks
            int mMaxFrameRate; // max frames per second
            long mMaxMacroBlockRate; // max macro block rate

            /** Saturates a long value to int */
            int saturateLongToInt(long value) const;

            /** This method may overflow */
            int align(int value, int alignment) const;

            /** Checks that value is a power of two. */
            void checkPowerOfTwo2(int value);

            /** @return NonNull */
            VideoSize getCommonBlockSize(const PerformancePoint &other) const;

        };

        /**
         * Find the equivalent VP9 profile level.
         *
         * Not a public API to developers.
         */
        static int EquivalentVP9Level(const sp<AMessage> &format);

        /**
         * Returns the range of supported bitrates in bits/second.
         */
        Range<int> getBitrateRange() const;

        /**
         * Returns the range of supported video widths.
         * <p class=note>
         * 32-bit processes will not support resolutions larger than 4096x4096 due to
         * the limited address space.
         */
        Range<int> getSupportedWidths() const;

        /**
         * Returns the range of supported video heights.
         * <p class=note>
         * 32-bit processes will not support resolutions larger than 4096x4096 due to
         * the limited address space.
         */
        Range<int> getSupportedHeights() const;

        /**
         * Returns the alignment requirement for video width (in pixels).
         *
         * This is a power-of-2 value that video width must be a
         * multiple of.
         */
        int getWidthAlignment() const;

        /**
         * Returns the alignment requirement for video height (in pixels).
         *
         * This is a power-of-2 value that video height must be a
         * multiple of.
         */
        int getHeightAlignment() const;

        /**
         * Return the upper limit on the smaller dimension of width or height.
         *
         * Some codecs have a limit on the smaller dimension, whether it be
         * the width or the height.  E.g. a codec may only be able to handle
         * up to 1920x1080 both in landscape and portrait mode (1080x1920).
         * In this case the maximum width and height are both 1920, but the
         * smaller dimension limit will be 1080. For other codecs, this is
         * {@code Math.min(getSupportedWidths().getUpper(),
         * getSupportedHeights().getUpper())}.
         */
        int getSmallerDimensionUpperLimit() const;

        /**
         * Returns the range of supported frame rates.
         *
         * This is not a performance indicator.  Rather, it expresses the
         * limits specified in the coding standard, based on the complexities
         * of encoding material for later playback at a certain frame rate,
         * or the decoding of such material in non-realtime.
         */
        Range<int> getSupportedFrameRates() const;

        /**
         * Returns the range of supported video widths for a video height.
         * @param height the height of the video
         */
        Range<int> getSupportedWidthsFor(int height) const;

        /**
         * Returns the range of supported video heights for a video width
         * @param width the width of the video
         */
        Range<int> getSupportedHeightsFor(int width) const;

        /**
         * Returns the range of supported video frame rates for a video size.
         *
         * This is not a performance indicator.  Rather, it expresses the limits specified in
         * the coding standard, based on the complexities of encoding material of a given
         * size for later playback at a certain frame rate, or the decoding of such material
         * in non-realtime.

         * @param width the width of the video
         * @param height the height of the video
         */
        Range<double> getSupportedFrameRatesFor(int width, int height) const;

        /**
         * Returns the range of achievable video frame rates for a video size.
         * May return {@code null}, if the codec did not publish any measurement
         * data.
         * <p>
         * This is a performance estimate provided by the device manufacturer based on statistical
         * sampling of full-speed decoding and encoding measurements in various configurations
         * of common video sizes supported by the codec. As such it should only be used to
         * compare individual codecs on the device. The value is not suitable for comparing
         * different devices or even different android releases for the same device.
         * <p>
         * <em>On {@link android.os.Build.VERSION_CODES#M} release</em> the returned range
         * corresponds to the fastest frame rates achieved in the tested configurations. As
         * such, it should not be used to gauge guaranteed or even average codec performance
         * on the device.
         * <p>
         * <em>On {@link android.os.Build.VERSION_CODES#N} release</em> the returned range
         * corresponds closer to sustained performance <em>in tested configurations</em>.
         * One can expect to achieve sustained performance higher than the lower limit more than
         * 50% of the time, and higher than half of the lower limit at least 90% of the time
         * <em>in tested configurations</em>.
         * Conversely, one can expect performance lower than twice the upper limit at least
         * 90% of the time.
         * <p class=note>
         * Tested configurations use a single active codec. For use cases where multiple
         * codecs are active, applications can expect lower and in most cases significantly lower
         * performance.
         * <p class=note>
         * The returned range value is interpolated from the nearest frame size(s) tested.
         * Codec performance is severely impacted by other activity on the device as well
         * as environmental factors (such as battery level, temperature or power source), and can
         * vary significantly even in a steady environment.
         * <p class=note>
         * Use this method in cases where only codec performance matters, e.g. to evaluate if
         * a codec has any chance of meeting a performance target. Codecs are listed
         * in {@link MediaCodecList} in the preferred order as defined by the device
         * manufacturer. As such, applications should use the first suitable codec in the
         * list to achieve the best balance between power use and performance.
         *
         * @param width the width of the video
         * @param height the height of the video
         */
        std::optional<Range<double>> getAchievableFrameRatesFor(int width, int height) const;

        /**
         * Returns the supported performance points. May return {@code null} if the codec did not
         * publish any performance point information (e.g. the vendor codecs have not been updated
         * to the latest android release). May return an empty list if the codec published that
         * if does not guarantee any performance points.
         * <p>
         * This is a performance guarantee provided by the device manufacturer for hardware codecs
         * based on hardware capabilities of the device.
         * <p>
         * The returned list is sorted first by decreasing number of pixels, then by decreasing
         * width, and finally by decreasing frame rate.
         * Performance points assume a single active codec. For use cases where multiple
         * codecs are active, should use that highest pixel count, and add the frame rates of
         * each individual codec.
         * <p class=note>
         * 32-bit processes will not support resolutions larger than 4096x4096 due to
         * the limited address space, but performance points will be presented as is.
         * In other words, even though a component publishes a performance point for
         * a resolution higher than 4096x4096, it does not mean that the resolution is supported
         * for 32-bit processes.
         */
        std::vector<PerformancePoint> getSupportedPerformancePoints() const;

        /**
         * Returns whether a given video size ({@code width} and
         * {@code height}) and {@code frameRate} combination is supported.
         */
        bool areSizeAndRateSupported(int width, int height, double frameRate) const;

        /**
         * Returns whether a given video size ({@code width} and
         * {@code height}) is supported.
         */
        bool isSizeSupported(int width, int height) const;

        /**
         * Returns if a media format is supported.
         *
         * Not exposed to public
         */
        bool supportsFormat(const sp<AMessage> &format) const;

        /**
         * Create VideoCapabilities.
         */
        static std::shared_ptr<VideoCapabilities> Create(const sp<AMessage> &format,
                std::shared_ptr<CodecCapabilities> parent);

        /**
         * Get the block size.
         *
         * Not a public API to developers
         */
        VideoSize getBlockSize() const;

        /**
         * Get the block count range.
         *
         * Not a public API to developers
         */
        Range<int> getBlockCountRange() const;

        /**
         * Get the blocks per second range.
         *
         * Not a public API to developers
         */
        Range<long> getBlocksPerSecondRange() const;

        /**
         * Get the aspect ratio range.
         *
         * Not a public API to developers
         */
        Range<Rational> getAspectRatioRange(bool blocks) const;

    private:
        Range<int> mBitrateRange;
        Range<int> mHeightRange;
        Range<int> mWidthRange;
        Range<int> mBlockCountRange;
        Range<int> mHorizontalBlockRange;
        Range<int> mVerticalBlockRange;
        Range<Rational> mAspectRatioRange;
        Range<Rational> mBlockAspectRatioRange;
        Range<long> mBlocksPerSecondRange;
        std::map<VideoSize, Range<long>, VideoSizeCompare> mMeasuredFrameRates;
        std::vector<PerformancePoint> mPerformancePoints;
        Range<int> mFrameRateRange;

        int mBlockWidth;
        int mBlockHeight;
        int mWidthAlignment;
        int mHeightAlignment;
        int mSmallerDimensionUpperLimit;

        bool mAllowMbOverride; // allow XML to override calculated limits

        int getBlockCount(int width, int height) const;
        std::optional<VideoSize> findClosestSize(int width, int height) const;
        std::optional<Range<double>> estimateFrameRatesFor(int width, int height) const;
        bool supports(int width, int height, double rate) const;
        /* no public constructor */
        VideoCapabilities() {};
        void init(const sp<AMessage> &format, std::shared_ptr<CodecCapabilities> parent);
        void initWithPlatformLimits();
        std::vector<PerformancePoint> getPerformancePoints(const sp<AMessage> &format) const;
        std::map<VideoSize, Range<long>, VideoSizeCompare>
                getMeasuredFrameRates(const sp<AMessage> &format) const;

        static std::optional<std::pair<Range<int>, Range<int>>> ParseWidthHeightRanges(
                const std::string &str);
        void parseFromInfo(const sp<AMessage> &format);
        void applyBlockLimits(int blockWidth, int blockHeight,
                Range<int> counts, Range<long> rates, Range<Rational> ratios);
        void applyAlignment(int widthAlignment, int heightAlignment);
        void updateLimits();
        void applyMacroBlockLimits(
                int maxHorizontalBlocks, int maxVerticalBlocks,
                int maxBlocks, long maxBlocksPerSecond,
                int blockWidth, int blockHeight,
                int widthAlignment, int heightAlignment);
        void applyMacroBlockLimits(
                int minHorizontalBlocks, int minVerticalBlocks,
                int maxHorizontalBlocks, int maxVerticalBlocks,
                int maxBlocks, long maxBlocksPerSecond,
                int blockWidth, int blockHeight,
                int widthAlignment, int heightAlignment);
        void applyLevelLimits();
    };

    /**
     * A class that supports querying the encoding capabilities of a codec.
     */
    struct EncoderCapabilities : XCapabilitiesBase {
        /**
        * Returns the supported range of quality values.
        *
        * Quality is implementation-specific. As a general rule, a higher quality
        * setting results in a better image quality and a lower compression ratio.
        */
        Range<int> getQualityRange();

        /**
         * Returns the supported range of encoder complexity values.
         * <p>
         * Some codecs may support multiple complexity levels, where higher
         * complexity values use more encoder tools (e.g. perform more
         * intensive calculations) to improve the quality or the compression
         * ratio.  Use a lower value to save power and/or time.
         */
        Range<int> getComplexityRange();

        /** Constant quality mode */
        static const int BITRATE_MODE_CQ = 0;
        /** Variable bitrate mode */
        static const int BITRATE_MODE_VBR = 1;
        /** Constant bitrate mode */
        static const int BITRATE_MODE_CBR = 2;
        /** Constant bitrate mode with frame drops */
        static const int BITRATE_MODE_CBR_FD =  3;

        /**
         * Query whether a bitrate mode is supported.
         */
        bool isBitrateModeSupported(int mode);

        /** @hide */
        static std::shared_ptr<EncoderCapabilities> Create(
                const sp<AMessage> &format, std::shared_ptr<CodecCapabilities> parent);

        /** @hide */
        void getDefaultFormat(sp<AMessage> &format);

        /** @hide */
        bool supportsFormat(const sp<AMessage> &format);

    private:
        static inline Feature bitrates[] = {
            Feature("VBR", BITRATE_MODE_VBR, true),
            Feature("CBR", BITRATE_MODE_CBR, false),
            Feature("CQ",  BITRATE_MODE_CQ,  false),
            Feature("CBR-FD", BITRATE_MODE_CBR_FD, false)
        };
        static int ParseBitrateMode(std::string mode);

        Range<int> mQualityRange;
        Range<int> mComplexityRange;
        int mBitControl;
        int mDefaultComplexity;
        int mDefaultQuality;
        std::string mQualityScale;

        /* no public constructor */
        EncoderCapabilities() { }
        void init(const sp<AMessage> &format, std::shared_ptr<CodecCapabilities> parent);
        void applyLevelLimits();
        void parseFromInfo(const sp<AMessage> &format);
        bool supports(std::optional<int> complexity, std::optional<int> quality,
                std::optional<int> profile);
    };

    struct CodecCapabilities : public std::enable_shared_from_this<CodecCapabilities> {
        static bool SupportsBitrate(Range<int> bitrateRange,
                const sp<AMessage> &format);

        /**
         * Returns the media type for which this codec-capability object was created.
         */
        AString getMediaType();

        /**
         * Returns the supported profile levels.
         */
        std::vector<ProfileLevel> getProfileLevels();

    private:
        AString mMediaType;
        std::vector<ProfileLevel> mProfileLevels;
        int mError;

        std::shared_ptr<AudioCapabilities> mAudioCaps;
        std::shared_ptr<VideoCapabilities> mVideoCaps;
        std::shared_ptr<EncoderCapabilities> mEncoderCaps;

        friend struct XCapabilitiesBase;
        friend struct AudioCapabilities;
        friend struct VideoCapabilities;
        friend struct EncoderCapabilities;
    };
}  // namespace android

#endif // CODEC_CAPABILITIES_H_