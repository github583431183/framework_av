/*
 * Copyright 2021, The Android Open Source Project
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

#ifndef _LIBMEDIAFORMATSHAPER_CODECPROPERTIES_H_
#define _LIBMEDIAFORMATSHAPER_CODECPROPERTIES_H_

#include <map>
#include <mutex>
#include <string>

#include <utils/RefBase.h>

namespace android {
namespace mediaformatshaper {

class CodecProperties {

  public:
    CodecProperties(std::string name, std::string mediaType);

    // seed the codec with some preconfigured values
    // (e.g. mediaType-granularity defaults)
    // runs from the constructor
    void Seed();
    void Finish();

    std::string getName();
    std::string getMediaType();

    // establish a mapping from standard 'key' to non-standard 'value' in the namespace 'kind'
    void setMapping(std::string kind, std::string key, std::string value);

    // translate from from standard key to non-standard key
    // return original standard key if there is no mapping
    std::string getMapping(std::string key, std::string kind);

    // returns an array of char *, which are paired "from" and "to" values
    // for mapping (or unmapping). it's always expressed as from->to
    // and 'reverse' describes which strings are to be on which side.
    const char **getMappings(std::string kind, bool reverse);

    // keep a map of all features and their parameters
    void setFeatureValue(std::string key, int32_t value);
    bool getFeatureValue(std::string key, int32_t *valuep);

    // does the codec support the Android S minimum quality rules
    void setSupportedMinimumQuality(int vmaf);
    int supportedMinimumQuality();

    // qp max bound used to compensate when SupportedMinimumQuality == 0
    // 0 == let a system default handle it
    void setTargetQpMax(int qpmax);
    int targetQpMax();

    // target bits-per-pixel (per second) for encoding operations.
    // This is used to calculate a minimum bitrate for any particular resolution.
    // A 1080p (1920*1080 = 2073600 pixels) to be encoded at 5Mbps has a bpp == 2.41
    void setBpp(double bpp) { mBpp = bpp;}
    double getBpp() {return mBpp;}

    // Does this codec support QP bounding
    // The getMapping() methods provide any needed mapping to non-standard keys.
    void setSupportsQp(bool supported) { mSupportsQp = supported;}
    bool supportsQp() { return mSupportsQp;}

    int  supportedApi();

    // a codec is not usable until it has been registered with its
    // name/mediaType.
    bool isRegistered() { return mIsRegistered;}
    void setRegistered(bool registered) { mIsRegistered = registered;}

  private:
    std::string mName;
    std::string mMediaType;
    int mApi = 0;
    int mMinimumQuality = 0;
    int mTargetQpMax = 0;
    bool mSupportsQp = false;
    double mBpp = 0.0;

    std::mutex mMappingLock;
    // XXX figure out why I'm having problems getting compiler to like GUARDED_BY
    std::map<std::string, std::string> mMappings /*GUARDED_BY(mMappingLock)*/ ;

    std::map<std::string, int32_t> mFeatures /*GUARDED_BY(mMappingLock)*/ ;

    bool mIsRegistered = false;

    // debugging of what's in the mapping dictionary
    void showMappings();

    // DISALLOW_EVIL_CONSTRUCTORS(CodecProperties);
};

extern CodecProperties *findCodec(const char *codecName, const char *mediaType);
extern CodecProperties *registerCodec(CodecProperties *codec, const char *codecName,
                               const char *mediaType);


} // namespace mediaformatshaper
} // namespace android

#endif  //  _LIBMEDIAFORMATSHAPER_CODECPROPERTIES_H_
