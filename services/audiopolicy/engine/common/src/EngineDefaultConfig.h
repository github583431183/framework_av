/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <EngineConfig.h>

#include <system/audio.h>

namespace android {
/**
 * @brief AudioProductStrategies hard coded array of strategies to fill new engine API contract.
 */
const engineConfig::ProductStrategies gOrderedStrategies = {
    {"STRATEGY_PHONE",
     {
         {"phone", AUDIO_STREAM_VOICE_CALL, "AUDIO_STREAM_VOICE_CALL", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_VOICE_COMMUNICATION, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_NONE, ""}},
         },
         {"sco", AUDIO_STREAM_BLUETOOTH_SCO, "AUDIO_STREAM_BLUETOOTH_SCO", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_SCO,
            ""}},
         }
     },
    },
    {"STRATEGY_SONIFICATION",
     {
         {"ring", AUDIO_STREAM_RING, "AUDIO_STREAM_RING", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE,
            AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}}
         },
         {"alarm", AUDIO_STREAM_ALARM, "AUDIO_STREAM_ALARM", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_ALARM, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_NONE, ""}},
         }
     },
    },
    {"STRATEGY_ENFORCED_AUDIBLE",
     {
         {"system_enforced", AUDIO_STREAM_ENFORCED_AUDIBLE, "AUDIO_STREAM_ENFORCED_AUDIBLE",
          "AUDIO_STREAM_RING",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_AUDIBILITY_ENFORCED, ""}}
         }
     },
    },
    {"STRATEGY_ACCESSIBILITY",
     {
         {"accessibility", AUDIO_STREAM_ACCESSIBILITY, "AUDIO_STREAM_ACCESSIBILITY",
          "AUDIO_STREAM_MUSIC",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY,
            AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}}
         }
     },
    },
    {"STRATEGY_SONIFICATION_RESPECTFUL",
     {
         {"notification", AUDIO_STREAM_NOTIFICATION, "AUDIO_STREAM_NOTIFICATION",
          "AUDIO_STREAM_RING",
          {
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION, AUDIO_SOURCE_DEFAULT,
               AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_NOTIFICATION_EVENT,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}
          }
         }
     },
    },
    {"STRATEGY_MEDIA",
     {
         {"assistant", AUDIO_STREAM_ASSISTANT, "AUDIO_STREAM_ASSISTANT", "AUDIO_STREAM_MUSIC",
          {{AUDIO_CONTENT_TYPE_SPEECH, AUDIO_USAGE_ASSISTANT,
            AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}}
         },
         {"music", AUDIO_STREAM_MUSIC, "AUDIO_STREAM_MUSIC", "",
          {
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_MEDIA, AUDIO_SOURCE_DEFAULT,
               AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_GAME, AUDIO_SOURCE_DEFAULT,
               AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_ASSISTANT, AUDIO_SOURCE_DEFAULT,
               AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""},
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT,
               AUDIO_FLAG_NONE, ""}
          },
         },
         {"system", AUDIO_STREAM_SYSTEM, "AUDIO_STREAM_SYSTEM", "AUDIO_STREAM_RING",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_ASSISTANCE_SONIFICATION,
            AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}}
         }
     },
    },
    {"STRATEGY_DTMF",
     {
         {"dtmf", AUDIO_STREAM_DTMF, "AUDIO_STREAM_DTMF", "AUDIO_STREAM_RING",
          {
              {AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING,
               AUDIO_SOURCE_DEFAULT, AUDIO_FLAG_NONE, ""}
          }
         }
     },
    },
    {"STRATEGY_CALL_ASSISTANT",
     {
         {"call_assistant", AUDIO_STREAM_CALL_ASSISTANT, "AUDIO_STREAM_CALL_ASSISTANT", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_CALL_ASSISTANT, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_NONE, ""}}
         }
     },
    },
    {"STRATEGY_TRANSMITTED_THROUGH_SPEAKER",
     {
         {"tts", AUDIO_STREAM_TTS, "AUDIO_STREAM_TTS", "AUDIO_STREAM_MUSIC",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_BEACON, ""}}
         }
     },
    }
};

/**
 * For Internal use of respectively audio policy and audioflinger
 * For compatibility reason why apm volume config file, volume group name is the stream type.
 */
const engineConfig::ProductStrategies gOrderedSystemStrategies = {
    {"rerouting",
     {
         {"", AUDIO_STREAM_REROUTING, "AUDIO_STREAM_REROUTING", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_VIRTUAL_SOURCE, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_NONE, AUDIO_TAG_INTERNAL}}
         }
     },
    },
    {"patch",
     {
         {"", AUDIO_STREAM_PATCH, "AUDIO_STREAM_PATCH", "",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT,
            AUDIO_FLAG_NONE, AUDIO_TAG_INTERNAL}}
         }
     },
    }
};
const engineConfig::VolumeGroups gSystemVolumeGroups = {
    {"AUDIO_STREAM_REROUTING", 0, 1,
     {
         {"DEVICE_CATEGORY_SPEAKER", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEADSET", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EARPIECE", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EXT_MEDIA", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEARING_AID", {{0,0}, {100, 0}}},

     }
    },
    {"AUDIO_STREAM_PATCH", 0, 1,
     {
         {"DEVICE_CATEGORY_SPEAKER", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEADSET", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EARPIECE", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EXT_MEDIA", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEARING_AID", {{0,0}, {100, 0}}},

     }
    }
};

const engineConfig::Config gDefaultEngineConfig = {
    1.0,
    gOrderedStrategies,
    {},
    {},
    {}
};
} // namespace android
