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

//#define LOG_NDEBUG 0
#define LOG_TAG "CodecCapabilitiesUtils"

#include <android-base/properties.h>
#include <utils/Log.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <media/CodecCapabilitiesUtils.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AUtils.h>

namespace android {

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

// Utils functions

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
    int lower, upper;
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        lower = strtol(str.substr(0, ix).c_str(), NULL, 10);
        upper = strtol(str.substr(ix + 1).c_str(), NULL, 10);
        if ((lower == 0 && str.substr(0, ix) != "0")
                || (upper == 0 && str.substr(ix + 1) != "0")) {
            ALOGW("could not parse integer range: %s", str.c_str());
            return std::nullopt;
        }
    } else {
        int value = strtol(str.c_str(), NULL, 10);
        if (value == 0 && str != "0") {
            ALOGW("could not parse integer range: %s", str.c_str());
            return std::nullopt;
        }
        lower = upper = value;
    }
    return std::make_optional<Range<int>>(lower, upper);
}

std::optional<Range<long>> ParseLongRange(const std::string str) {
    if (str.empty()) {
        ALOGW("could not parse long range: %s", str.c_str());
        return std::nullopt;
    }
    long lower, upper;
    size_t ix = str.find_first_of('-');
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

Range<int> GetSizeRange() {
#ifdef __LP64__
    return Range<int>(1, 32768);
#else
    std::string valueStr = base::GetProperty("media.resolution.limit.32bit", "4096");
    int value = std::atoi(valueStr.c_str());
    return Range<int>(1, value);
#endif
}

long divUpLong(long num, long den) {
    return (num + den - 1) / den;
}

void CheckPowerOfTwo(int value) {
    CHECK((value & (value - 1)) == 0);
}

}  // namespace android