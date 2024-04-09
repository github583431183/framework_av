/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecInfoParserUtils"
#include <utils/Log.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <media/MediaCodecInfoParserUtils.h>

#include <media/stagefright/foundation/AUtils.h>

namespace android {

// template<typename T>
// struct Range {
//     Range() : lower_(), upper_() {}

//     Range(T l, T u) : lower_(l), upper_(u) {}

//     constexpr bool empty() const { return lower_ >= upper_; }

//     T lower() const { return lower_; }

//     T upper() const { return upper_; }

//     // Check if a value is in the range.
//     bool contains(T value) const {
//         return lower_ <= value && upper_ > value;
//     }

//     bool contains(Range<T> range) const {
//         return (range.lower_ >= lower_) && (range.upper_ <= upper_);
//     }

//     // Clamp a value in the range
//     T clamp(T value) const{
//         if (value < lower_) {
//             return lower_;
//         } else if (value > upper_) {
//             return upper_;
//         } else {
//             return value;
//         }
//     }

//     // Return the intersected range
//     Range<T> intersect(Range<T> range) const {
//         if (lower_ > range.lower() && range.upper() > upper_) {
//             // range includes this
//             return *this;
//         } else if (range.lower() > lower_ && range.upper() < upper_) {
//             // this includes range
//             return range;
//         } else {
//             // if ranges are disjoint returns an empty Range(lower > upper)
//             Range<T> result = Range<T>(std::max(lower_, range.lower_),
//                     std::min(upper_, range.upper_));
//             if (result.empty()) {
//                 ALOGE("Failed to intersect 2 ranges as they are disjoint");
//             }
//             return result;
//         }
//     }

//     /**
//      * Returns the intersection of this range and the inclusive range
//      * specified by {@code [lower, upper]}.
//      * <p>
//      * See {@link #intersect(Range)} for more details.</p>
//      *
//      * @param lower a non-{@code null} {@code T} reference
//      * @param upper a non-{@code null} {@code T} reference
//      * @return the intersection of this range and the other range
//      *
//      * @throws NullPointerException if {@code lower} or {@code upper} was {@code null}
//      * @throws IllegalArgumentException if the ranges are disjoint.
//      */
//     Range<T> intersect(T lower, T upper) {
//         return Range(std::max(lower_, lower), std::min(upper_, upper));
//     }

//     /**
//      * Returns the smallest range that includes this range and
//      * another range.
//      *
//      * E.g. if a < b < c < d, the
//      * extension of [a, c] and [b, d] ranges is [a, d].
//      * As the endpoints are object references, there is no guarantee
//      * which specific endpoint reference is used from the input ranges:
//      *
//      * E.g. if a == a' < b < c, the
//      * extension of [a, b] and [a', c] ranges could be either
//      * [a, c] or ['a, c], where ['a, c] could be either the exact
//      * input range, or a newly created range with the same endpoints.
//      *
//      * @param range a non-null Range<T> reference
//      * @return the extension of this range and the other range.
//      */
//     Range<T> extend(Range<T> range) {

//         if (lower_ >= range.lower_ && upper_ <= range.upper_) {
//             // other includes this
//             return range;
//         } else if (lower_ <= range.lower_ && upper_ >= range.upper_) {
//             // this includes other
//             return *this;
//         } else {
//             return Range<T>(std::min(lower_, range.lower_), std::max(upper_, range.upper_));
//         }
//     }

// private:
//     T lower_;
//     T upper_;
// };

// template<typename T>
// Range<T>::Range() : lower_(), upper_() {}

// template<typename T>
// Range<T>::Range(T l, T u) : lower_(l), upper_(u) {}

// template<typename T>
// bool Range<T>::empty() const { return lower_ >= upper_; }

// template<typename T>
// T Range<T>::lower() const { return lower_; }

// template<typename T>
// T Range<T>::upper() const { return upper_; }

// template<typename T>
// bool Range<T>::contains(T value) const {
//     return lower_ <= value && upper_ > value;
// }

// template<typename T>
// bool Range<T>::contains(Range<T> range) const {
//     return (range.lower_ >= lower_) && (range.upper_ <= upper_);
// }

// template<typename T>
// T Range<T>::clamp(T value) const{
//     if (value < lower_) {
//         return lower_;
//     } else if (value > upper_) {
//         return upper_;
//     } else {
//         return value;
//     }
// }

// template<typename T>
// Range<T> Range<T>::intersect(Range<T> range) const {
//     if (lower_ > range.lower() && range.upper() > upper_) {
//         // range includes this
//         return *this;
//     } else if (range.lower() > lower_ && range.upper() < upper_) {
//         // this includes range
//         return range;
//     } else {
//         // if ranges are disjoint returns an empty Range(lower > upper)
//         Range<T> result = Range<T>(std::max(lower_, range.lower_),
//                 std::min(upper_, range.upper_));
//         if (result.empty()) {
//             ALOGE("Failed to intersect 2 ranges as they are disjoint");
//         }
//         return result;
//     }
// }

// template<typename T>
// Range<T> Range<T>::intersect(T lower, T upper) {
//     return Range(std::max(lower_, lower), std::min(upper_, upper));
// }

// template<typename T>
// Range<T> Range<T>::extend(Range<T> range) {
//     if (lower_ >= range.lower_ && upper_ <= range.upper_) {
//         // other includes this
//         return range;
//     } else if (lower_ <= range.lower_ && upper_ >= range.upper_) {
//         // this includes other
//         return *this;
//     } else {
//         return Range<T>(std::min(lower_, range.lower_), std::max(upper_, range.upper_));
//     }
// }

// VideoSize

VideoSize::VideoSize(int width, int height) : mWidth(width), mHeight(height) {}

VideoSize::VideoSize() : mWidth(0), mHeight(0) {}

int VideoSize::getWidth() const { return mWidth; }

int VideoSize::getHeight() const { return mHeight; }

bool VideoSize::equals(VideoSize other) const {
    return mWidth == other.mWidth && mHeight == other.mHeight;
}

std::string VideoSize::toString() const {
    return std::to_string(mWidth) + "x" + std::to_string(mHeight);
}

std::optional<VideoSize> VideoSize::ParseSize(std::string str) {
    if (str.empty()) {
        return std::nullopt;
    }

    size_t sep_ix = str.find_first_of('*');
    if (sep_ix == std::string::npos) {
        sep_ix = str.find_first_of('x');
    }
    if (sep_ix == std::string::npos) {
        return std::nullopt;
    }

    // strtol() returns 0 if unable to parse a number
    int w = strtol(str.substr(0, sep_ix).c_str(), NULL, 10);
    int h = strtol(str.substr(sep_ix + 1).c_str(), NULL, 10);
    if ((w == 0 && (str.substr(0, sep_ix) != "0"))
            || (h == 0 && (str.substr(sep_ix + 1) != "0"))) {
        ALOGW("could not parse size %s", str.c_str());
        return std::nullopt;
    }

    return std::make_optional(VideoSize(w, h));
}

int VideoSize::hashCode() const {
    // assuming most sizes are <2^16, doing a rotate will give us perfect hashing
    return mHeight ^ ((mWidth << (sizeof(int) / 2)) | (mWidth >> (sizeof(int) / 2)));
}

bool VideoSize::empty() const {
    return mWidth <= 0 || mHeight <= 0;
}

//

std::optional<Rational> ParseRational(std::string str) {
    if (str.compare("NaN") == 0) {
        return std::make_optional(NaN);
    } else if (str.compare("Infinity") == 0) {
        return std::make_optional(POSITIVE_INFINITY);
    } else if (str.compare("-Infinity") == 0) {
        return std::make_optional(NEGATIVE_INFINITY);
    }

    size_t sep_ix = str.find_first_of(':');
    if (sep_ix == std::string::npos) {
        sep_ix = str.find_first_of('/');
    }
    if (sep_ix == std::string::npos) {
        return std::nullopt;
    }

    int numerator = strtol(str.substr(0, sep_ix).c_str(), NULL, 10);
    int denominator = strtol(str.substr(sep_ix + 1).c_str(), NULL, 10);
    if ((numerator == 0 && str.substr(0, sep_ix) != "0")
            || (denominator == 0 && str.substr(sep_ix + 1) != "0")) {
        ALOGW("could not parse string: %s to Rational", str.c_str());
        return std::nullopt;
    }
    return std::make_optional(Rational(numerator, denominator));
}

Range<int> FactorRange(Range<int> range, int factor) {
    if (factor == 1) {
        return range;
    }
    return Range(divUp(range.lower(), factor), range.upper() / factor);
}

Range<long> FactorRange(Range<long> range, long factor) {
    if (factor == 1) {
        return range;
    }
    return Range(divUp(range.lower(), factor), range.upper() / factor);
}

Rational ScaleRatio(Rational ratio, int num, int den) {
    int common = Rational::GCD(num, den);
    num /= common;
    den /= common;
    return Rational(
            (int)(ratio.getNumerator() * (double)num),     // saturate to int
            (int)(ratio.getDenominator() * (double)den));  // saturate to int
}

Range<Rational> ScaleRange(Range<Rational> range, int num, int den) {
    if (num == den) {
        return range;
    }
    return Range(
            ScaleRatio(range.lower(), num, den),
            ScaleRatio(range.upper(), num, den));
}

Range<int> IntRangeFor(double v) {
    return Range((int)v, (int)ceil(v));
}

Range<long> LongRangeFor(double v) {
    return Range((long)v, (long)ceil(v));
}

Range<int> AlignRange(Range<int> range, int align) {
    return range.intersect(
            divUp(range.lower(), align) * align,
            (range.upper() / align) * align);
}

// parse string into int range
std::optional<Range<int>> ParseIntRange(const std::string &str) {
    if (str.empty()) {
        ALOGW("could not parse integer range: %s", str.c_str());
        return std::nullopt;
    }
    ALOGD("Into ParseIntRang. str is not empty");
    int lower, upper;
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        ALOGD("ix > 0: %d", ix);
        lower = strtol(str.substr(0, ix).c_str(), NULL, 10);
        upper = strtol(str.substr(ix + 1).c_str(), NULL, 10);
        ALOGD("lower: %d, upper: %d", lower, upper);
        if ((lower == 0 && str.substr(0, ix) != "0")
                || (upper == 0 && str.substr(ix + 1) != "0")) {
            ALOGW("could not parse integer range: %s", str.c_str());
            return std::nullopt;
        }
    } else {
        ALOGD("ix < 0: %d", ix);
        int value = strtol(str.c_str(), NULL, 10);
        ALOGD("value: %d", value);
        if (value == 0 && str != "0") {
            ALOGW("could not parse integer range: %s", str.c_str());
            return std::nullopt;
        }
        lower = upper = value;
    }
    return std::make_optional<Range<int>>(lower, upper);
}

std::optional<Range<long>> ParseLongRange(const std::string str) {
    ALOGD("Into ParseLongRange");
    if (str.empty()) {
        ALOGW("could not parse long range: %s", str.c_str());
        return std::nullopt;
    }
    long lower, upper;
    size_t ix = str.find_first_of('-');
    ALOGD("ix: %d", ix);
    if (ix != std::string::npos) {
        lower = strtol(str.substr(0, ix).c_str(), NULL, 10);
        upper = strtol(str.substr(ix + 1).c_str(), NULL, 10);
        // differentiate between unable to parse a number and the parsed number is 0
        if ((lower == 0 && str.substr(0, ix) != "0") || (upper == 0 && str.substr(ix + 1) != "0")) {
            ALOGW("could not parse long range: %s", str.c_str());
            return std::nullopt;
        }
    } else {
        long value = strtol(str.c_str(), NULL, 10);
        if (value == 0 && str != "0") {
            ALOGW("could not parse long range: %s", str.c_str());
            return std::nullopt;
        }
        lower = upper = value;
    }
    ALOGD("lower: %d, upper: %d", lower, upper);
    return std::make_optional<Range<long>>(lower, upper);
}

std::optional<Range<Rational>> ParseRationalRange(const std::string str) {
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        std::optional<Rational> lower = ParseRational(str.substr(0, ix));
        std::optional<Rational> upper = ParseRational(str.substr(ix + 1));
        if (!lower || !upper) {
            return std::nullopt;
        }
        return std::make_optional<Range<Rational>>(lower.value(), upper.value());
    } else {
        std::optional<Rational> value = ParseRational(str);
        if (!value) {
            return std::nullopt;
        }
        return std::make_optional<Range<Rational>>(value.value(), value.value());
    }
}

std::optional<std::pair<VideoSize, VideoSize>> ParseSizeRange(const std::string str) {
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        std::optional<VideoSize> lowerOpt = VideoSize::ParseSize(str.substr(0, ix));
        std::optional<VideoSize> upperOpt = VideoSize::ParseSize(str.substr(ix + 1));
        if (!lowerOpt || !upperOpt) {
            return std::nullopt;
        }
        return std::make_optional(
                std::pair<VideoSize, VideoSize>(lowerOpt.value(), upperOpt.value()));
    } else {
        std::optional<VideoSize> opt = VideoSize::ParseSize(str);
        if (!opt) {
            return std::nullopt;
        }
        return std::make_optional(std::pair<VideoSize, VideoSize>(opt.value(), opt.value()));
    }
}

long divUpLong(long num, long den) {
    return (num + den - 1) / den;
}

}  // namespace android