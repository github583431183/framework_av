/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <utils/Log.h>
#include <sstream>

#include <gtest/gtest.h>

using namespace android;

const std::vector<audio_usage_t> Helper::mAndroidUsages = { AUDIO_USAGE_MEDIA,
                AUDIO_USAGE_VOICE_COMMUNICATION,
                AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING ,
                AUDIO_USAGE_ALARM,
                AUDIO_USAGE_NOTIFICATION,
                AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE,
                AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST,
                AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT,
                AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED,
                AUDIO_USAGE_NOTIFICATION_EVENT,
                AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY,
                AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
                AUDIO_USAGE_ASSISTANCE_SONIFICATION,
                AUDIO_USAGE_GAME,
//                AUDIO_USAGE_VIRTUAL_SOURCE,
                AUDIO_USAGE_ASSISTANT };


/*static*/
void Helper::changeMode(audio_mode_t mode)
{
    status_t ret = AudioSystem::setMode(mode);
    EXPECT_EQ(ret, NO_ERROR) << "AudioSystem::setMode(" << toString(mode) << ") failed: "
                             << ret;
    ret = AudioSystem::setPhoneState(mode);
    EXPECT_EQ(ret, NO_ERROR) << "AudioSystem::setPhoneState(" << toString(mode)
                             << ") failed: " << ret;
}

bool Helper::connectPort(const audio_port &portToConnect, audio_port &connectedPort)
{
    return Helper::connectAndCheckDevice(portToConnect.ext.device.type,
                                         AUDIO_POLICY_DEVICE_STATE_AVAILABLE,
                                         portToConnect.ext.device.address,
                                         portToConnect.ext.device.address,
                                         connectedPort);
}

bool Helper::disconnectPort(const audio_port &portToDisconnect)
{
    audio_port disconnectedPort;
    return Helper::connectAndCheckDevice(portToDisconnect.ext.device.type,
                                         AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                                         portToDisconnect.ext.device.address,
                                         portToDisconnect.ext.device.address,
                                         disconnectedPort);
}

/*static*/
bool Helper::connectAndCheckDevice(audio_devices_t type, audio_policy_dev_state_t state,
                                   const std::string &address, const std::string &name,
                                   audio_port &devicePort)
{
    status_t ret = AudioSystem::setDeviceConnectionState(
                type, state, address.c_str(), name.c_str(), AUDIO_FORMAT_DEFAULT);
    EXPECT_EQ(ret, NO_ERROR) << "AudioSystem::setDeviceConnectionState(device:"
                             << toString(type).c_str()
                             << ", state:" << state << ", address:" << address
                             << ", name:" << name << ") failed: " << ret;

    std::cerr << "AudioSystem::setDeviceConnectionState(device:"
              << toString(type).c_str()
              << ", state:" << state << ", address:" << address
              << ", name:" << name << ")" << std::endl;

    audio_port expectedPort {};
    expectedPort.role =
            audio_is_output_device(type) ? AUDIO_PORT_ROLE_SINK : AUDIO_PORT_ROLE_SOURCE;
    expectedPort.type = AUDIO_PORT_TYPE_DEVICE;
    expectedPort.ext.device.type = type;
    strncpy(expectedPort.ext.device.address, address.c_str(), AUDIO_DEVICE_MAX_ADDRESS_LEN);

    ret = findPort(expectedPort.role, expectedPort.type, expectedPort.ext.device.type,
                   expectedPort.ext.device.address, devicePort);

    if (state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
        EXPECT_EQ(ret, NO_ERROR) << "Device port NOT connected: "
                                 << dumpPort(devicePort);

        std::cerr << "AudioSystem::setDeviceConnectionState(device:"
                  << toString(type).c_str()
                  << ", state:" << state << ", address:" << address
                  << ", name:" << name << ") Port: " << dumpPort(devicePort) << std::endl;
    } else {
        EXPECT_NE(ret, NO_ERROR) << "Device port NOT disconnected: "
                                 << dumpPort(devicePort);
    }
    return state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE ||
            ret == NO_ERROR;
}

/*static*/
std::string Helper::getPortInfo(audio_port_handle_t portId)
{
    auto dumpPort = [](const auto& port) {
       std::string result { "Port Id=" + std::to_string(port.id) };
       if (port.type == AUDIO_PORT_TYPE_DEVICE) {
           std::string typeName;
           if (port.ext.device.type & AUDIO_DEVICE_BIT_IN) {
               InputDeviceConverter::maskToString(port.ext.device.type, typeName);
           } else {
               OutputDeviceConverter::maskToString(port.ext.device.type, typeName);
           }
           result += std::string(", Type=") + typeName;
           result += std::string(", Address=") + port.ext.device.address;
       }
       result += std::string(", Role=") + (port.role == AUDIO_PORT_ROLE_SOURCE ? "source" : "sink")
                 + ", Type=" + (port.type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix");
       return result;
    };

    uint32_t numPorts = 0;
    uint32_t generation;

    // First retrieve the number of ports
    status_t ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,
                                                                 AUDIO_PORT_TYPE_NONE,
                                                                 &numPorts, NULL, &generation);
    if (ret != NO_ERROR) {
         ALOGE("getPortInfo::listAudioPorts failed to retrieve number of port.");
        return {};
    }
    struct audio_port nPorts[numPorts];

    // Then retrieve the ports
    ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE, AUDIO_PORT_TYPE_NONE,
                                      &numPorts, nPorts, &generation);
    if (ret != NO_ERROR) {
       ALOGE("getPortInfo::listAudioPorts failed to retrieve ports.");
        return {};
    }
    for (const auto &port : nPorts) {
        if (port.id == portId) {
            return dumpPort(port);
        }
    }
    return {};
}

/*static*/
status_t Helper::getPortById(const audio_port_handle_t& portId,
                                      audio_port &portConfig)
{
    uint32_t numPorts = 0;
    uint32_t generation;

    // First retrieve the number of ports
    status_t ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,
                                                                 AUDIO_PORT_TYPE_NONE,
                                                                 &numPorts, NULL, &generation);
    if (ret != NO_ERROR) {
         ALOGE("AudioSystem::listAudioPorts failed to retrieve number of port.");
        return ret;
    }
    struct audio_port nPorts[numPorts];

    // Then retrieve the ports
    ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE, AUDIO_PORT_TYPE_NONE,
                                               &numPorts, nPorts, &generation);
    if (ret != NO_ERROR) {
         ALOGE("AudioSystem::listAudioPorts failed to retrieve ports.");
        exit(1);
    }
    for (const auto &port : nPorts) {
        if (port.id == portId) {
            memcpy(&portConfig, &port, sizeof(audio_port));
            return OK;
        }
    }
    ALOGE("AudioSystem::listAudioPorts port %d  not found.", portId);
    return BAD_VALUE;
}


/*static*/
std::string Helper::dumpPortConfig(const audio_port_config &port)
{
    std::ostringstream result;
    std::string deviceInfo;
    if (port.type == AUDIO_PORT_TYPE_DEVICE) {
        if (port.ext.device.type & AUDIO_DEVICE_BIT_IN) {
            InputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        } else {
            OutputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        }
        deviceInfo += std::string(", @: ") + port.ext.device.address;
    }
    result << "Port Config Id: " << port.id
           << ", Role= " << (port.role == AUDIO_PORT_ROLE_SOURCE ? "source" : "sink")
           << ", Type=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix")
           << ", Name=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? deviceInfo : "")
           << ", config_mask=0x" << std::hex << port.config_mask << std::dec
           << ", gain index=" << port.gain.index
           << ", gain mode=" << port.gain.mode
           << ", gain values=" << port.gain.values[0]
           << ", mix io handle=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? 0 : port.ext.mix.handle);
    return result.str();
}

/*static*/
std::string Helper::dumpPort(audio_port_handle_t portHandle)
{
    audio_port port;
    if (getPortById(portHandle, port) != OK) {
        return {};
    }
    std::ostringstream result;
    std::string deviceInfo;
    if (port.type == AUDIO_PORT_TYPE_DEVICE) {
        if (port.ext.device.type & AUDIO_DEVICE_BIT_IN) {
            InputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        } else {
            OutputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        }
        deviceInfo += std::string(", @: ") + port.ext.device.address;
    }
    result << "Port Id: " << port.id
           << ", Role= " << (port.role == AUDIO_PORT_ROLE_SOURCE ? "source" : "sink")
           << ", Type=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix")
           << ", Name=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? deviceInfo : port.name)
           << ", mix io handle=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? 0 : port.ext.mix.handle);
    return result.str();
}

/*static*/
std::string Helper::dumpPort(const audio_port &port)
{
    std::ostringstream result;
    std::string deviceInfo;
    if (port.type == AUDIO_PORT_TYPE_DEVICE) {
        if (port.ext.device.type & AUDIO_DEVICE_BIT_IN) {
            InputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        } else {
            OutputDeviceConverter::maskToString(port.ext.device.type, deviceInfo);
        }
        deviceInfo += std::string(", @: ") + port.ext.device.address;
    }
    result << "Port Id: " << port.id
           << ", Role= " << (port.role == AUDIO_PORT_ROLE_SOURCE ? "source" : "sink")
           << ", Type=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix")
           << ", Name=" << (port.type == AUDIO_PORT_TYPE_DEVICE ? deviceInfo : port.name);
    return result.str();
}

/*static*/
std::string Helper::dumpPatch(const audio_patch &patch)
{
    std::ostringstream result;
    result << "Patch Id: " << patch.id
           << ", sources: " << patch.num_sources
           << ", sink: " << patch.num_sinks << "\n";
    for (uint32_t indexSource = 0; indexSource < patch.num_sources; indexSource++) {
        result << dumpPortConfig(patch.sources[indexSource]) << "\n";
    }
    for (uint32_t indexSink = 0; indexSink < patch.num_sinks; indexSink++) {
        result << dumpPortConfig(patch.sinks[indexSink]) << "\n";
    }
    return result.str();
}

/*static*/
status_t Helper::getPatchByHandle(audio_patch_handle_t handle, audio_patch &patch)
{
    uint32_t numPatches = 0;
    struct audio_patch *patches = nullptr;
    status_t ret = getPatches(patches, numPatches);
    if (ret != NO_ERROR) {
        std::cerr << "AudioSystem::getPatchByHandle failed to getPatches" << std::endl;
        return BAD_VALUE;
    }
    for (uint32_t i = 0; i < numPatches; i++) {
        if (patches[i].id == handle) {
            patch = patches[i];
            return OK;
        }
    }
    std::cerr << "AudioSystem::getPatchByHandle failed to get handle=" << handle << std::endl;
    return BAD_VALUE;
}

/*static*/
status_t Helper::getPatches(struct audio_patch *&patches, unsigned int &numPatches)
{
    uint32_t sizeGeneration;
    uint32_t generation;

    int attempts = 5;

    do {
        if (attempts-- == 0) {
            return TIMED_OUT;
        }
        // First retrieve the number of patches
        status_t ret = AudioSystem::listAudioPatches(&numPatches, nullptr, &sizeGeneration);
        if (ret != NO_ERROR) {
            std::cerr << "AudioSystem::listAudioPatches failed to retrieve number of patches" << std::endl;
            ALOGE("AudioSystem::listAudioPatches failed to retrieve number of patches");
            return BAD_VALUE;
        }
        patches = (struct audio_patch *)realloc(patches, numPatches * sizeof(struct audio_patch));

        // Then retrieve the patches
        ret = AudioSystem::listAudioPatches(&numPatches, patches, &generation);
        if (ret != NO_ERROR) {
            std::cerr << "AudioSystem::listAudioPatches  failed to retrieve patches" << std::endl;
            ALOGE("AudioSystem::listAudioPatches failed to retrieve patches");
            return ret;
        }
    } while (sizeGeneration != generation);
    return OK;
}

/*static*/
bool Helper::checkPatch(audio_io_handle_t mixHandle, audio_port_handle_t portId)
{
    struct audio_patch patch;
    if (getPatchForOutputMix(mixHandle, patch) == OK) {
        return patchInvolvesSinkDevicePort(patch, portId);
    } else if (getPatchForInputMix(mixHandle, patch) == OK) {
        return patchInvolvesSourceDevicePort(patch, portId);
    }
    return false;
}

/*static*/
bool Helper::checkPatch(audio_io_handle_t mixHandle, audio_devices_t deviceType)
{
    struct audio_patch patch;
    if (audio_is_output_devices(deviceType)) {
        return (getPatchForOutputMix(mixHandle, patch) == OK) &&
                patchInvolvesDeviceTypes(patch, deviceType);
    }
    return (getPatchForInputMix(mixHandle, patch) == OK) &&
            patchInvolvesDeviceTypes(patch, deviceType);
}

/*static*/
status_t Helper::getPatchForOutputMix(audio_io_handle_t mixHandle, audio_patch &patch)
{
    uint32_t numPatches = 0;
    struct audio_patch *patches = nullptr;
    status_t ret = getPatches(patches, numPatches);
    if (ret != NO_ERROR) {
        return BAD_VALUE;
    }
    for (uint32_t i = 0; i < numPatches; i++) {
//        std::cerr << " getPatchForOutputMix looking for mixport " << mixHandle << std::endl;
//        std::cerr << dumpPatch(patches[i]) << std::endl;
        for( unsigned int j = 0; j < patches[i].num_sources; j++ ) {
            if (patches[i].sources[j].type == AUDIO_PORT_TYPE_MIX &&
                    patches[i].sources[j].ext.mix.handle == mixHandle) {
                patch = patches[i];
                return OK;
            }
        }
    }
    return BAD_VALUE;
}

/*static*/
status_t Helper::getPatchForInputMix(audio_io_handle_t mixHandle, audio_patch &patch)
{
    uint32_t numPatches = 0;
    struct audio_patch *patches = nullptr;
    status_t ret = getPatches(patches, numPatches);
    if (ret != NO_ERROR) {
        return BAD_VALUE;
    }
    for (uint32_t i = 0; i < numPatches; i++) {
        for( unsigned int j = 0; j < patches[i].num_sinks; j++ ) {
            if (patches[i].sinks[j].type == AUDIO_PORT_TYPE_MIX &&
                    patches[i].sinks[j].ext.mix.handle == mixHandle) {
                patch = patches[i];
                return OK;
            }
        }
    }
    return BAD_VALUE;
}

/*static*/
bool Helper::patchInvolvesDeviceTypes(const audio_patch &patch, audio_devices_t deviceTypes)
{
    audio_devices_t foundDevices = AUDIO_DEVICE_NONE;

    if (audio_is_output_devices(deviceTypes)) {
        for( unsigned int i = 0; i < patch.num_sinks; i++ ) {
            EXPECT_EQ(AUDIO_PORT_TYPE_DEVICE, patch.sinks[i].type)
                    << "Wrong port type selected, expecting " << AUDIO_PORT_TYPE_DEVICE << ", got "
                    << patch.sinks[i].type;
            if (patch.sinks[i].type == AUDIO_PORT_TYPE_DEVICE) {
                foundDevices |= patch.sinks[i].ext.device.type;
            }
        }
    } else {
        for( unsigned int i = 0; i < patch.num_sources; i++ ) {
            EXPECT_EQ(AUDIO_PORT_TYPE_DEVICE, patch.sources[i].type)
                    << "Wrong port type selected, expecting " << AUDIO_PORT_TYPE_DEVICE << ", got "
                    << patch.sources[i].type;
            if (patch.sources[i].type == AUDIO_PORT_TYPE_DEVICE) {
                foundDevices |= patch.sources[i].ext.device.type;
            }
        }
    }
    std::string deviceLiteral(toString(foundDevices));
    std::string expectedDeviceLiteral(toString(deviceTypes));

    EXPECT_EQ(foundDevices, deviceTypes)
            << "Wrong device selected for output, expecting type=0x" << std::hex << deviceTypes
            << " (" << expectedDeviceLiteral << "), got type=0x" << foundDevices
            << " (" << deviceLiteral << ")";
    return foundDevices == deviceTypes;
}

/*static*/
bool Helper::patchInvolvesSinkDevicePort(const audio_patch &patch, audio_port_handle_t sinkPort)
{
    for( unsigned int i = 0; i < patch.num_sinks; i++ ) {
        EXPECT_EQ(AUDIO_PORT_TYPE_DEVICE, patch.sinks[i].type)
                << "Wrong port type selected, expecting " << AUDIO_PORT_TYPE_DEVICE << ", got "
                << patch.sinks[i].type;
        if (patch.sinks[i].type == AUDIO_PORT_TYPE_DEVICE && patch.sinks[i].id == sinkPort) {
            return true;
        }
    }
    return false;
}

/*static*/
bool Helper::patchInvolvesSourceDevicePort(const audio_patch &patch, audio_port_handle_t sourcePort)
{
    for( unsigned int i = 0; i < patch.num_sources; i++ ) {
        EXPECT_EQ(AUDIO_PORT_TYPE_DEVICE, patch.sources[i].type)
                << "Wrong port type selected, expecting " << AUDIO_PORT_TYPE_DEVICE << ", got "
                << patch.sources[i].type;
        if (patch.sources[i].type == AUDIO_PORT_TYPE_DEVICE && patch.sources[i].id == sourcePort) {
            return true;
        }
    }
    return false;
}

/*static*/
std::string Helper::toString(audio_devices_t type)
{
    std::string deviceLiteral;
    if (audio_is_output_device(type)) {
        if (not OutputDeviceConverter::toString(type, deviceLiteral)) {
            ALOGE("failed to convert output device: %d", type);
            return {"AUDIO_DEVICE_NONE"};
        }
    } else if (audio_is_input_device(type)) {
        if (not InputDeviceConverter::toString(type, deviceLiteral)) {
            ALOGE("failed to convert input device: %d", type);
            return {"AUDIO_DEVICE_NONE"};
        }
    } else {
        ALOGE("invalid device: %d", type);
        return {"AUDIO_DEVICE_NONE"};
    }
    return deviceLiteral;
}

/*static*/
status_t Helper::findPort(const audio_port &expectedPort, audio_port &foundPort)
{
    return Helper::findPort(expectedPort.role, expectedPort.type, expectedPort.ext.device.type,
                            expectedPort.ext.device.address, foundPort);
}

/*static*/
status_t Helper::findPort(audio_port_role_t role, audio_port_type_t type,
                                   audio_devices_t deviceType,
                                   const std::string &address, audio_port &foundPort)
{
    uint32_t numPorts = 0;
    uint32_t generation;
    status_t ret;

    // First retrieve the number of ports
    ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE, AUDIO_PORT_TYPE_NONE,
                                               &numPorts, NULL, &generation);
    if (ret != NO_ERROR) {
        ALOGE("AudioSystem::listAudioPorts failed to retrieve number of port.");
        return ret;
    }
    struct audio_port nPorts[numPorts];

    // Then retrieve the ports
    ret = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE, AUDIO_PORT_TYPE_NONE,
                                               &numPorts, nPorts, &generation);
    if (ret != NO_ERROR) {
        ALOGE("AudioSystem::listAudioPorts failed to retrieve ports.");
        return ret;
    }

    for (const auto &port : nPorts) {
        if (port.role == role && port.type == type && port.ext.device.type == deviceType &&
                (strncmp(port.ext.device.address, address.c_str(),
                         AUDIO_DEVICE_MAX_ADDRESS_LEN) == 0)) {
            foundPort = port;
            return OK;
        }
    }
    return BAD_VALUE;
}

void Helper::getPort(audio_devices_t type, const std::string &address, audio_port &port) {
    audio_port expectedPort {};
    expectedPort.role = AUDIO_PORT_ROLE_SINK;
    expectedPort.type = AUDIO_PORT_TYPE_DEVICE;
    expectedPort.ext.device.type = type;
    strncpy(expectedPort.ext.device.address, address.c_str(), AUDIO_DEVICE_MAX_ADDRESS_LEN);
    status_t ret = findPort(expectedPort.role, expectedPort.type,
                                     expectedPort.ext.device.type,
                                     expectedPort.ext.device.address,
                                     port);

    ASSERT_EQ(ret, NO_ERROR) << "Could not find port for device type "
                                      << Helper::toString(expectedPort.type).c_str()
                                      << " and address:" << expectedPort.ext.device.address;
}

void Helper::waitEndOfActiveStreams()
{
    int activeStreamCount = 0;
    for (audio_stream_type_t stream = AUDIO_STREAM_MIN; stream < AUDIO_STREAM_PUBLIC_CNT;
         stream = (audio_stream_type_t) (stream + 1)) {
        bool isActive = false;
        AudioSystem::isStreamActive(stream,
                                             &isActive,
                                             SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY);
        if (isActive) {
            activeStreamCount += 1;
        }
    }
    if (activeStreamCount != 0) {
        usleep((SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY + 500) * 1000);
    }
}

product_strategy_t Helper::getStrategyByName(const std::string &name)
{
    AudioProductStrategyVector strategies;
    status_t ret = AudioSystem::listAudioProductStrategies(strategies);
    if (ret != NO_ERROR) {
        return PRODUCT_STRATEGY_NONE;
    }

    auto iter = std::find_if(begin(strategies), end(strategies),
                             [&name](const auto &strategy) {
        return strategy.getName() == name; });
    if (iter ==  end(strategies)) {
        return PRODUCT_STRATEGY_NONE;
    }
    return (*iter).getId();
}

/*static*/
std::string Helper::getStrategyInfo(product_strategy_t psId)
{
    std::string result;
    AudioProductStrategyVector strategies;
    status_t ret = AudioSystem::listAudioProductStrategies(strategies);
    if (ret != NO_ERROR) {
        return {"Invalid strategy id" + std::to_string(psId)};
    }
    for (const auto &strategy : strategies) {
        if (strategy.getId() == psId) {
            result += std::string("Strategy Id=") + std::to_string(psId);
            result += std::string(", Name=") + strategy.getName();
        }
    }
    return result;
}

//static
void Helper::launchPlayer(std::unique_ptr<AudioTrackTest> &audioTrack,
                          product_strategy_t strategy,
                          audio_stream_type_t stream,
                          audio_port_handle_t explicitRoutingPortId,
                          audio_port_handle_t expectedRoutingPortId,
                          bool shallBeRouted)
{
    bool useProductStrategy = strategy != PRODUCT_STRATEGY_NONE;
    bool useExplicitRouting = explicitRoutingPortId != AUDIO_PORT_HANDLE_NONE;
    if (useProductStrategy)
        audioTrack = std::make_unique<AudioTrackTest>(strategy);
    else if (useExplicitRouting)
        audioTrack = std::make_unique<AudioTrackTest>(stream, explicitRoutingPortId);
    else
        audioTrack = std::make_unique<AudioTrackTest>(stream);

    ASSERT_EQ(OK, audioTrack->createTrack()) << "Failed to create AudioTrack for "
            <<  (useProductStrategy ? dumpProductStrategy(strategy, true) :
                                      useExplicitRouting ? dumpPort(explicitRoutingPortId) :
                                                           toString(stream));

    audio_port_handle_t playbackRoutedPortId = AUDIO_PORT_HANDLE_NONE;
    ASSERT_EQ(OK, audioTrack->playSine(playbackRoutedPortId))
              << "Failed to start AudioTrack for "
              <<  (useProductStrategy ?
                      dumpProductStrategy(strategy, true) :
                      useExplicitRouting ?
                          dumpPort(explicitRoutingPortId) :
                          toString(stream));

    EXPECT_TRUE(audioTrack->waitForDeviceCb(expectedRoutingPortId)) << "Device callback timeout";

    // Check routed port
    playbackRoutedPortId = audioTrack->getRoutedDeviceId();

    if (expectedRoutingPortId != AUDIO_PORT_HANDLE_NONE) {
        EXPECT_EQ(playbackRoutedPortId, expectedRoutingPortId)
                << "AudioTrack for "
                <<  (useProductStrategy ?
                         dumpProductStrategy(strategy, true) :
                         useExplicitRouting ?
                             dumpPort(explicitRoutingPortId) :
                             toString(stream)) << " routed on wrong port:\n"
                 << "\t expected: " << dumpPort(expectedRoutingPortId) << "\n"
                 << "\t got: " << dumpPort(playbackRoutedPortId);
    }
    // Check Patch
    // If no expected routed port, just ensure, the routed device and the track are really
    // connected through an audio patch
    expectedRoutingPortId = expectedRoutingPortId != AUDIO_PORT_HANDLE_NONE ?
                expectedRoutingPortId : playbackRoutedPortId;
    EXPECT_EQ(shallBeRouted,
              Helper::checkPatch(audioTrack->getOutput(), expectedRoutingPortId))
            << (shallBeRouted ? "No" : "") << " patch found involving mix port "
            << audioTrack->getOutput() << " and device port " << expectedRoutingPortId;
}

//static
void Helper::launchPlayer(std::unique_ptr<AudioTrackTest> &audioTrack,
                          audio_stream_type_t stream,
                          audio_port_handle_t explicitRoutingPortId,
                          audio_port_handle_t expectedRoutingPortId,
                          bool shallBeRouted)
{
    launchPlayer(audioTrack, PRODUCT_STRATEGY_NONE, stream, explicitRoutingPortId,
                 expectedRoutingPortId, shallBeRouted);
}

//static
std::string Helper::dumpProductStrategy(product_strategy_t psId, bool oneline)
{
    std::ostringstream result;
    AudioProductStrategyVector strategies;
    status_t ret = AudioSystem::listAudioProductStrategies(strategies);
    if (ret != NO_ERROR) {
        return {"Invalid strategy id" + std::to_string(psId)};
    }
    for (const auto &strategy : strategies) {
        if (strategy.getId() == psId) {
            result << "Strategy Id=" << psId << " Name=" <<strategy.getName() << "\n";
            if (oneline) {
                continue;
            }
            result << " Applicable AA:" << std::endl;
            for (const auto & attributes : strategy.getAudioAttributes()) {
                audio_attributes_t aa = attributes.getAttributes();
                std::string contentTypeLiteral;
                if (!AudioContentTypeConverter::toString(aa.content_type,
                                                                  contentTypeLiteral)) {
                    ALOGE("failed to convert usage: %d", aa.content_type);
                    return {};
                }

                std::string usageLiteral;
                if (!UsageTypeConverter::toString(aa.usage, usageLiteral)) {
                    ALOGE("failed to convert usage : %d", aa.usage);
                    return {};
                }

                std::string flagsLiteral;
                AudioFlagConverter::maskToString(aa.flags, flagsLiteral);

                result << "  { Content type: " << contentTypeLiteral
                       << ", Usage:" << usageLiteral
                       << ", Flags:" << flagsLiteral
                       << ", Tags:" << aa.tags << " } " << std::endl;
            }
            result << " Supported Streams:";
            for (const auto & attributes : strategy.getAudioAttributes()) {
                auto stream = attributes.getStreamType();
                std::string streamTypeLiteral;
                if (!StreamTypeConverter::toString(stream, streamTypeLiteral)) {
                    ALOGE("failed to convert stream %d", stream);
                    return {};
                }
                result << " " << streamTypeLiteral;
            }
            result << "\n";
        }
    }
    return result.str();
}

//static
std::string Helper::dumpProductStrategies()
{
    std::ostringstream result;
    result << "Audio Product Strategies:" << "\n";
    AudioProductStrategyVector strategies;
    status_t ret = AudioSystem::listAudioProductStrategies(strategies);
    if (ret != NO_ERROR) {
        return {};
    }
    for (const auto &strategy : strategies) {
        result << dumpProductStrategy(strategy.getId(), false);
    }
    return result.str();
}

//static
status_t Helper::getAudioVolumeGroups(AudioVolumeGroupVector &groups)
{
    status_t ret = AudioSystem::listAudioVolumeGroups(groups);
    if (ret != NO_ERROR) {
        std::cout << "AudioSystem::listAudioVolumeGroups ::"
                     "Failed to retrieve Volume Groups, error=" << ret << std::endl;
    }
    return ret;
}

//static
std::string Helper::dumpVolumeGroups()
{
    std::ostringstream result;
    result << "Audio Volume Groups:" << "\n";
    AudioVolumeGroupVector groups;
    status_t ret = getAudioVolumeGroups(groups);
    if (ret != NO_ERROR) {
        return {};
    }
    for (const auto &group : groups) {
        result << "Group Id=" << group.getId() << " Name=" << group.getName() << "\n";
        result << "\nAttributes: " << group.getAudioAttributes().size() << "\n";
        for (const auto &attributes : group.getAudioAttributes()) {
            result << toString(attributes) << "\n";
        }
        result << " Streams: { ";
        for (const auto &stream : group.getStreamTypes()) {
            result << toString(stream) << " ";
        }
        result << " }\n";
    }
    return result.str();
}

//static
StreamTypeVector Helper::getVolumeGroupsStreams(volume_group_t groupId)
{
    AudioVolumeGroupVector groups;
    status_t ret = getAudioVolumeGroups(groups);
    if (ret != NO_ERROR) {
        return {};
    }
    for (const auto &group : groups) {
        if (group.getId() != groupId) {
            continue;
        }
        return group.getStreamTypes();
    }
    return {};
}

//static
std::vector<audio_attributes_t> Helper::getVolumeGroupsAttributes(volume_group_t groupId)
{
    AudioVolumeGroupVector groups;
    status_t ret = getAudioVolumeGroups(groups);
    if (ret != NO_ERROR) {
        return {};
    }
    for (const auto &group : groups) {
        if (group.getId() != groupId) {
            continue;
        }
        return group.getAudioAttributes();
    }
    return {};
}

status_t Helper::setEffectParameter(const sp<AudioEffect>& effect,
                                             int32_t param, uint32_t paramSizeMax,
                                             void *pValue, uint32_t valueSize)
{

    status_t status;
    uint32_t buf32[(paramSizeMax - 1) / sizeof(uint32_t) + 1];
    effect_param_t *p = (effect_param_t *)buf32;

    p->psize = sizeof(int32_t);
    *(int32_t *)p->data = param;
    p->vsize = valueSize;
    memcpy(p->data + p->psize, pValue, p->vsize);
    status = effect->setParameter(p);
    EXPECT_EQ(OK, status) << "Failed to set param status=" << status;
    if (NO_ERROR == status) {
        EXPECT_EQ(OK, status) << "Failed to set param p->status=" << status;
        status = p->status;
    }
    return status;
}

status_t Helper::getEffectParameter(const sp<AudioEffect>& effect,
                                             int32_t param, uint32_t paramSizeMax, void *pValue,
                                             uint32_t valueSize)
{
    status_t status;
    uint32_t buf32[(paramSizeMax - 1) / sizeof(uint32_t) + 1];
    effect_param_t *p = (effect_param_t *)buf32;

    p->psize = sizeof(int32_t);
    *(int32_t *)p->data = param;
    p->vsize = valueSize;
    status = effect->getParameter(p);
    EXPECT_EQ(OK, status) << "Failed to get param status=" << status;
    if (NO_ERROR == status) {
        status = p->status;
        EXPECT_EQ(OK, status) << "Failed to get param p->status=" << status;
        if (NO_ERROR == status) {
            memcpy(pValue, p->data + p->psize, p->vsize);
        }
    }
    return status;
}

void Helper::playbackOnExplicitDevice(const audio_port &explicitDevicePort,
                                      audio_port_handle_t &routedPort)
 {
     std::unique_ptr<AudioTrackTest> audioTrack;
     launchPlayer(audioTrack, AUDIO_STREAM_MUSIC, explicitDevicePort.id, explicitDevicePort.id);

     EXPECT_TRUE(audioTrack->waitForDeviceCb(explicitDevicePort.id)) << "Timeout on Device cb";

     // Check routed port
     routedPort = audioTrack->getRoutedDeviceId();

     // Check Patch
     EXPECT_TRUE(checkPatch(audioTrack->getOutput(), explicitDevicePort.id))
             << "No patch found involving mix port " << audioTrack->getOutput()
             << " and device port " << explicitDevicePort.id;

     if (audioTrack->isPlaying()) {
         audioTrack->stop();
     }
 }

void Helper::captureFromExplicitDevice(const audio_port &explicitDevicePort,
                                       audio_port_handle_t &routedPort)
{
    std::unique_ptr<AudioRecordTest> audioRecord =
            std::make_unique<AudioRecordTest>(explicitDevicePort.id);
    ASSERT_EQ(OK, audioRecord->createAudioRecord())
               << ", Failed to create AudioRecord on explicit device port: "
               <<  Helper::dumpPort(explicitDevicePort);

    ASSERT_EQ(OK, audioRecord->record(routedPort))
              << "Failed to start Capture on explicit device port: "
              <<  Helper::dumpPort(routedPort);

    EXPECT_TRUE(audioRecord->waitForDeviceCb()) << "Timeout waiting for Device Callback";

    // Check Patch
    EXPECT_TRUE(checkPatch(audioRecord->getInput(), explicitDevicePort.id))
            << "No patch found involving mix port " << audioRecord->getInput()
            << " and device port " << explicitDevicePort.id;

    audioRecord->stop();
}

bool Helper::isPublicStrategy(const AudioProductStrategy &strategy)
{
    bool isPublicStrategy = true;
    for (const auto &attribute : strategy.getAudioAttributes()) {
        if (attribute.getAttributes() == defaultAttr &&
                (uint32_t(attribute.getStreamType()) >= AUDIO_STREAM_PUBLIC_CNT)) {
            // Native AudioTrack will prevent us to create the track.
            std::cerr << "Strategy " << strategy.getName() << " has invalid attributes "
                      << "and non-public stream "
                      << toString(attribute.getStreamType()) << std::endl;
            isPublicStrategy = false;
            break;
        }
    }
    return isPublicStrategy;
}
