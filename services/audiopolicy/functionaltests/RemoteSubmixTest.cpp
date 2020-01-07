/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "Helper.hpp"

#include <gtest/gtest.h>

using namespace android;

using UsageAndRules = std::vector<std::pair<audio_usage_t, uint32_t>>;
using RemoteSubmixTestParams = std::tuple<const audio_attributes_t>;

class DynamicPolicyMixPlaybackReRoutingTest :
        public ::testing::TestWithParam<RemoteSubmixTestParams>
{
public:
    void SetUp() override
    {
        android::status_t ret;
        int mixType = MIX_TYPE_PLAYERS;
        int mixFlag = MIX_ROUTE_FLAG_LOOP_BACK;
        audio_devices_t device_type = AUDIO_DEVICE_OUT_REMOTE_SUBMIX;

        android::Vector<android::AudioMixMatchCriterion> myMixMatchCriteria;

        for(const auto &usage: mUsageRules) {
            myMixMatchCriteria.add(android::AudioMixMatchCriterion(
                                       usage.first, AUDIO_SOURCE_DEFAULT, usage.second));
        }
        audio_config_t config = AUDIO_CONFIG_INITIALIZER;
        config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        config.format = AUDIO_FORMAT_PCM_16_BIT;
        config.sample_rate = 48000;

        android::AudioMix myAudioMix(myMixMatchCriteria, mixType, config, mixFlag,
                                     android::String8(mMixAddress.c_str()), 0);
        myAudioMix.mDeviceType = device_type;
        mAudioMixes.add(myAudioMix);

        // Register Dynamic Policy Mix
        std::cerr << "- Register Dynamic Policy Mix--------"  << std::endl;
        ret = android::AudioSystem::registerPolicyMixes(mAudioMixes, true);
        ASSERT_EQ(ret, android::OK) << "AudioSystem::registerPolicyMixes(address:" << mMixAddress
                                    << ") failed: " << ret;

        // Ensure extraction source port has been connected
        std::cerr << "- Ensure extraction source port has been connected--------"  << std::endl;
        audio_port expectedPort {
            .role = AUDIO_PORT_ROLE_SOURCE,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext.device.type = AUDIO_DEVICE_IN_REMOTE_SUBMIX,
        };
        strncpy(expectedPort.ext.device.address, mMixAddress.c_str(), AUDIO_DEVICE_MAX_ADDRESS_LEN);

        ret = Helper::findPort(expectedPort.role, expectedPort.type, expectedPort.ext.device.type,
                       mMixAddress, mExtractionPort);
        ASSERT_EQ(ret, android::OK)
                << "Could not find extraction port" << Helper::dumpPort(expectedPort);
        std::cerr << "Extraction Port found. " << Helper::dumpPort(mExtractionPort) << std::endl;

        // 3 - Launch the capture
        std::cerr << "- Launch the capture--------"  << std::endl;
        audio_source_t source = AUDIO_SOURCE_REMOTE_SUBMIX;
        audio_attributes_t attr = {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, source, 0, ""};
        std::string tags = std::string("addr=") + mMixAddress;
        strncpy(attr.tags, tags.c_str(), AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - 1);

        mAudioRecord = std::make_unique<AudioRecordTest>(attr);
        ASSERT_EQ(android::OK, mAudioRecord->createAudioRecord())
                << ", Failed to create AudioRecord for: " << android::toString(attr);

        audio_port_handle_t captureRoutedPortId;
        ret = mAudioRecord->record(captureRoutedPortId);
        ASSERT_EQ(ret, android::OK) << "failed to start the extraction";

        EXPECT_TRUE(mAudioRecord->waitForDeviceCb()) << "Timeout waiting for Device Callback";
        captureRoutedPortId = mAudioRecord->getRoutedDeviceId();

        ASSERT_EQ(mExtractionPort.id, captureRoutedPortId)
                << "Capture NOT routed on expected port: expecting "
                << Helper::dumpPort(mExtractionPort.id)
                << ", got port:" << Helper::dumpPort(captureRoutedPortId);

        // Ensure extraction injection sink port has been connected
        std::cerr << "- Ensure injection sink port has been connected--------"  << std::endl;
        audio_port expectedSinkPort {
            .role = AUDIO_PORT_ROLE_SINK,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext.device.type = AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
        };
        strncpy(expectedSinkPort.ext.device.address, mMixAddress.c_str(),
                AUDIO_DEVICE_MAX_ADDRESS_LEN);

        ret = Helper::findPort(expectedSinkPort.role, expectedSinkPort.type,
                               expectedSinkPort.ext.device.type, mMixAddress, mInjectionPort);
        ASSERT_EQ(ret, android::OK) << "Could not find " << Helper::dumpPort(expectedSinkPort);

        std::cerr << "Injection Port found. " << Helper::dumpPort(mInjectionPort) << std::endl;
    }

    void TearDown() override
    {
        android::status_t ret ;

        // Stop the capture and ensure injection point has been disconnected
        if (mAudioRecord != nullptr) {
            mAudioRecord->stop();
        }
        audio_port port;
        ret = Helper::findPort(mInjectionPort.role, mInjectionPort.type,
                               mInjectionPort.ext.device.type, mMixAddress, port);
        EXPECT_NE(ret, NO_ERROR) << "Injection port NOT disconnected: "
                                 << Helper::dumpPort(mInjectionPort);

        // Unegister Dynamic Policy Mix
        ret = android::AudioSystem::registerPolicyMixes(mAudioMixes, false);
        EXPECT_EQ(ret, NO_ERROR) << "AudioSystem::registerPolicyMixes(address:" << mMixAddress
                                 << ") failed to unregister: " << ret;

        // Ensure extractor point has been disconnected
        ret = Helper::findPort(mExtractionPort.role, mExtractionPort.type,
                               mExtractionPort.ext.device.type, mMixAddress, port);
        EXPECT_NE(ret, NO_ERROR) << "Extraction port NOT disconnected: "
                                 << Helper::dumpPort(mExtractionPort);
    }

public:
    android::Vector<android::AudioMix> mAudioMixes;
    std::string mMixAddress {"remote_submix_media"};
    audio_port mExtractionPort;
    audio_port mInjectionPort;
    std::unique_ptr<AudioRecordTest> mAudioRecord;
    std::vector<std::pair<audio_usage_t, uint32_t>> mUsageRules = {
        {AUDIO_USAGE_MEDIA, RULE_MATCH_ATTRIBUTE_USAGE},
        {AUDIO_USAGE_ALARM, RULE_MATCH_ATTRIBUTE_USAGE}
    };
};

TEST_P(DynamicPolicyMixPlaybackReRoutingTest, playbackReRouting)
{
    const audio_attributes_t attr = std::get<0>(GetParam());
    const audio_usage_t usage = attr.usage;

    std::unique_ptr<AudioTrackTest> audioTrack = std::make_unique<AudioTrackTest>(attr);
    ASSERT_EQ(android::OK, audioTrack->createTrack()) << "Failed to create AudioTrack with tags: "
            << attr.tags << " for usage: " << android::toString(usage);

    audio_port_handle_t playbackRoutedPortId;
    EXPECT_EQ(android::OK, audioTrack->playSine(playbackRoutedPortId))
              << "Failed to start AudioTrack with tags: " << attr.tags << " for usage: "
              << android::toString(usage);

    EXPECT_TRUE(audioTrack->waitForDeviceCb(playbackRoutedPortId)) << "Device callback timeout";
    playbackRoutedPortId = audioTrack->getRoutedDeviceId();

    if ((std::find_if(begin(mUsageRules), end(mUsageRules), [&usage](const auto &usageRule) {
                      return (usageRule.first == usage) &&
                      (usageRule.second == RULE_MATCH_ATTRIBUTE_USAGE);})
            != end(mUsageRules)) ||
            ((strncmp(attr.tags, "addr=", strlen("addr=")) == 0) &&
            (strncmp(attr.tags + strlen("addr="), mMixAddress.c_str(),
                     AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - strlen("addr=") - 1) == 0))) {
        EXPECT_EQ(mInjectionPort.id, playbackRoutedPortId)
                << "Injection with tags: " << attr.tags << " for usage: "
                << android::toString(usage)
                << " NOT routed on expected port: expecting " << Helper::dumpPort(mInjectionPort)
                << ", got port:" << Helper::dumpPort(playbackRoutedPortId);
    } else {
        EXPECT_NE(mInjectionPort.id, playbackRoutedPortId)
                << "Injection with tags: " << attr.tags << " for usage: "
                << android::toString(usage)
                << " routed on injection port: expecting " << mInjectionPort.id
                << ", got port:" << playbackRoutedPortId;
    }
    // Necessary to avoid a race condition leading to removing an active client.
    while (!audioTrack->hasStarted()) {
        usleep(50);
    }
    audioTrack->stop();
}

INSTANTIATE_TEST_CASE_P(
        PlaybackReroutingUsageMatch,
        DynamicPolicyMixPlaybackReRoutingTest,
        ::testing::Values(
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_MEDIA,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ALARM,
                                    AUDIO_SOURCE_DEFAULT, 0, ""})
            )
        );

INSTANTIATE_TEST_CASE_P(
        PlaybackReroutingAddressPriorityMatch,
        DynamicPolicyMixPlaybackReRoutingTest,
        ::testing::Values(
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_MEDIA, AUDIO_SOURCE_DEFAULT, 0,
                                    "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_VOICE_COMMUNICATION, AUDIO_SOURCE_DEFAULT, 0,
                                    "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ALARM,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_NOTIFICATION,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_NOTIFICATION_EVENT,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANCE_SONIFICATION,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_GAME,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_VIRTUAL_SOURCE,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANT,
                                    AUDIO_SOURCE_DEFAULT, 0, "addr=remote_submix_media"})
            )
        );

INSTANTIATE_TEST_CASE_P(
        PlaybackReroutingUnHandledUsages,
        DynamicPolicyMixPlaybackReRoutingTest,
        ::testing::Values(
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_VOICE_COMMUNICATION,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_NOTIFICATION,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_NOTIFICATION_EVENT,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC,
                                    AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANCE_SONIFICATION,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_GAME,
                                    AUDIO_SOURCE_DEFAULT, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_VIRTUAL_SOURCE,
//            AUDIO_SOURCE_DEFAULT, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_MUSIC, AUDIO_USAGE_ASSISTANT,
                                    AUDIO_SOURCE_DEFAULT, 0, ""})
            )
        );

class DynamicPolicyMixRecordInjectionTest : public ::testing::TestWithParam<RemoteSubmixTestParams>
{
public:
    void SetUp() override
    {
        android::status_t ret;
        int mixType = MIX_TYPE_RECORDERS;
        int mixFlag = MIX_ROUTE_FLAG_LOOP_BACK;
        audio_devices_t device_type = AUDIO_DEVICE_IN_REMOTE_SUBMIX;

        android::Vector<android::AudioMixMatchCriterion> myMixMatchCriteria;

        for(const auto &sourceRule: mSourceRules) {
            myMixMatchCriteria.add(android::AudioMixMatchCriterion(
                                       AUDIO_USAGE_UNKNOWN, sourceRule.first, sourceRule.second));
        }
        audio_config_t config = AUDIO_CONFIG_INITIALIZER;
        config.channel_mask = AUDIO_CHANNEL_IN_STEREO;
        config.format = AUDIO_FORMAT_PCM_16_BIT;
        config.sample_rate = 48000;

        android::AudioMix myAudioMix(myMixMatchCriteria, mixType, config, mixFlag,
                                     android::String8(mMixAddress.c_str()), 0);
        myAudioMix.mDeviceType = device_type;
        mAudioMixes.add(myAudioMix);

        // Register Dynamic Policy Mix
        std::cerr << "- Register Dynamic Policy Mix--------"  << std::endl;
        ret = android::AudioSystem::registerPolicyMixes(mAudioMixes, true);
        ASSERT_EQ(ret, android::OK) << "AudioSystem::registerPolicyMixes(address:" << mMixAddress
                                    << ") failed: " << ret;

        // Ensure injection sink port has been connected
        std::cerr << "- Ensure extraction sink port has been connected--------"  << std::endl;
        audio_port expectedSinkPort {
            .role = AUDIO_PORT_ROLE_SINK,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext.device.type = AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
        };
        strncpy(expectedSinkPort.ext.device.address, mMixAddress.c_str(),
                AUDIO_DEVICE_MAX_ADDRESS_LEN);

        ret = Helper::findPort(expectedSinkPort.role, expectedSinkPort.type,
                               expectedSinkPort.ext.device.type, mMixAddress, mInjectionPort);
        ASSERT_EQ(ret, android::OK)
                << "Could not find injection port" << Helper::dumpPort(expectedSinkPort);
        std::cerr << "Injection Port found. " << Helper::dumpPort(mInjectionPort) << std::endl;

        // Launch the playback
        std::cerr << "- Launch the playback--------"  << std::endl;
        audio_usage_t usage = AUDIO_USAGE_VIRTUAL_SOURCE;
        audio_attributes_t attr = {AUDIO_CONTENT_TYPE_UNKNOWN, usage, AUDIO_SOURCE_DEFAULT, 0, ""};
        std::string tags = std::string("addr=") + mMixAddress;
        strncpy(attr.tags, tags.c_str(), AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - 1);

        mAudioTrack = std::make_unique<AudioTrackTest>(attr);
        ASSERT_EQ(android::OK, mAudioTrack->createTrack()) << "Failed to create AudioTrack";

        audio_port_handle_t playbackRoutedPortId;
        ret = mAudioTrack->playSine(playbackRoutedPortId);
        ASSERT_EQ(ret, android::OK) << "failed to start the extraction";

        ASSERT_EQ(mInjectionPort.id, playbackRoutedPortId)
                << "Injection NOT routed on expected port: expecting " << mInjectionPort.id
                << ", got port:" << playbackRoutedPortId;

        // Ensure extraction source port has been connected
        std::cerr << "- Ensure extraction source port has been connected--------"  << std::endl;
        audio_port expectedSrcPort {
            .role = AUDIO_PORT_ROLE_SOURCE,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext.device.type = AUDIO_DEVICE_IN_REMOTE_SUBMIX,
        };
        strncpy(expectedSrcPort.ext.device.address, mMixAddress.c_str(),
                AUDIO_DEVICE_MAX_ADDRESS_LEN);

        ret = Helper::findPort(expectedSrcPort.role, expectedSrcPort.type,
                               expectedSrcPort.ext.device.type, mMixAddress, mExtractionPort);
        ASSERT_EQ(ret, android::OK) << "Could not find " << Helper::dumpPort(expectedSrcPort);

        std::cerr << "Extraction Port found. " << Helper::dumpPort(mExtractionPort) << std::endl;
    }

    void TearDown() override
    {
        android::status_t ret;

        // Stop the playback and ensure extraction point has been disconnected
        if (mAudioTrack != nullptr) {
            mAudioTrack->stop();
        }

        // Need to wait to be sure the stopOutput / releaseOutput is called and set the extraction
        // port as not available.
        usleep(250000);

        audio_port port;
        ret = Helper::findPort(mExtractionPort.role, mExtractionPort.type,
                               mExtractionPort.ext.device.type, mMixAddress, port);
        EXPECT_NE(ret, NO_ERROR) << "Extraction port NOT disconnected: "
                                 << Helper::dumpPort(mExtractionPort);

        // Unegister Dynamic Policy Mix
        ret = android::AudioSystem::registerPolicyMixes(mAudioMixes, false);
        EXPECT_EQ(ret, NO_ERROR) << "AudioSystem::registerPolicyMixes(address:" << mMixAddress
                                 << ") failed to unregister: " << ret;

        // Ensure injection point has been disconnected
        ret = Helper::findPort(mInjectionPort.role, mInjectionPort.type,
                               mInjectionPort.ext.device.type, mMixAddress, port);
        EXPECT_NE(ret, NO_ERROR) << "Injection port NOT disconnected: "
                                 << Helper::dumpPort(mInjectionPort);
    }

public:
    android::Vector<android::AudioMix> mAudioMixes;
    std::string mMixAddress {"remote_submix_media"};
    audio_port mExtractionPort;
    audio_port mInjectionPort;
    std::unique_ptr<AudioTrackTest> mAudioTrack;
    std::vector<std::pair<audio_source_t, uint32_t>> mSourceRules = {
        {AUDIO_SOURCE_CAMCORDER, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET},
        {AUDIO_SOURCE_MIC, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET},
        {AUDIO_SOURCE_VOICE_COMMUNICATION, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET}
    };
};

TEST_P(DynamicPolicyMixRecordInjectionTest, RecordingInjection)
{
    const audio_attributes_t attr = std::get<0>(GetParam());
    const audio_source_t source = attr.source;

    std::unique_ptr<AudioRecordTest> audioRecord = std::make_unique<AudioRecordTest>(attr);
    ASSERT_EQ(android::OK, audioRecord->createAudioRecord())
            << ", Failed to create AudioRecord with tags: "
            << attr.tags << " for source: " << android::toString(source);

    audio_port_handle_t captureRoutedPortId;
    EXPECT_EQ(android::OK, audioRecord->record(captureRoutedPortId))
              << "Failed to start AudioRecord with tags: " << attr.tags << " for source: "
              << android::toString(source);

    EXPECT_TRUE(audioRecord->waitForDeviceCb()) << "Timeout waiting for Device Callback";
    captureRoutedPortId = audioRecord->getRoutedDeviceId();

    if ((std::find_if(begin(mSourceRules), end(mSourceRules), [&source](const auto &sourceRule) {
                      return sourceRule.first == source &&
                      sourceRule.second == RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET; })
            != end(mSourceRules))) {
        EXPECT_EQ(mExtractionPort.id, captureRoutedPortId)
                << "Capture with tags: " << attr.tags << " for source: "
                << android::toString(source)
                << " NOT routed on expected port: expecting " << mExtractionPort.id
                << ", got port:" << captureRoutedPortId;
    } else {
        EXPECT_NE(mInjectionPort.id, captureRoutedPortId)
                << "Capture with tags: " << attr.tags << " for source: "
                << android::toString(source)
                << " routed on extractio, port: expecting " << mExtractionPort.id
                << ", got port:" << captureRoutedPortId;
    }
    audioRecord->stop();
}

// No address priority rule for remote recording, address is a "don't care"
INSTANTIATE_TEST_CASE_P(
        RecordInjectionSourceMatch,
        DynamicPolicyMixRecordInjectionTest,
        ::testing::Values(
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_CAMCORDER, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_CAMCORDER, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_MIC, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_MIC, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_VOICE_COMMUNICATION, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_VOICE_COMMUNICATION, 0,
                                    "addr=remote_submix_media"})
            )
        );

// No address priority rule for remote recording
INSTANTIATE_TEST_CASE_P(
        RecordInjectionSourceNotMatch,
        DynamicPolicyMixRecordInjectionTest,
        ::testing::Values(
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_VOICE_UPLINK, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//AUDIO_SOURCE_VOICE_DOWNLINK, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//AUDIO_SOURCE_VOICE_CALL, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_VOICE_RECOGNITION, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_REMOTE_SUBMIX, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_UNPROCESSED, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_FM_TUNER, 0, ""}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_HOTWORD, 0, ""}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_VOICE_UPLINK, 0, "addr=remote_submix_media"}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_VOICE_DOWNLINK, 0, "addr=remote_submix_media"}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_VOICE_CALL, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_VOICE_RECOGNITION, 0, "addr=remote_submix_media"}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_REMOTE_SUBMIX, 0, "addr=remote_submix_media"}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_UNPROCESSED, 0, "addr=remote_submix_media"}),
//            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
//            AUDIO_SOURCE_FM_TUNER, 0, "addr=remote_submix_media"}),
            RemoteSubmixTestParams({AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN,
                                    AUDIO_SOURCE_HOTWORD, 0, "addr=remote_submix_media"})
            )
        );
