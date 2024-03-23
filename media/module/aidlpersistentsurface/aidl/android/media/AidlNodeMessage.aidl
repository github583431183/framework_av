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

package android.media;

import android.os.ParcelFileDescriptor;

/**
 * Message definition for IAidlNode which is a thin abstraction for
 * Codec2 AIDL encoder instance.
 *
 * In order to support Persistent InputSurface and/or MediaRecorder.
 *
 * Data structure for a message to IAidlNode. This is essentially a union of
 * different message types.
 */
parcelable AidlNodeMessage {

    /**
     * There are four main types of messages.
     */
    @Backing(type="int")
    enum Type {
        EVENT,
        EMPTY_BUFFER_DONE,
        FILL_BUFFER_DONE,
        FRAME_RENDERED,
    }

    parcelable EventData {
        int event;
        int data1;
        int data2;
        int data3;
        int data4;
    }

    parcelable BufferData {
        int buffer;
    }

    parcelable ExtendedBufferData {
        int buffer;
        int rangeOffset;
        int rangeLength;
        int flags;
        long timestampUs;
    }

    parcelable RenderData {
        long timestampUs;
        long systemTimeNs;
    }

    union Data {
        // if type == EVENT
        EventData eventData;

        // if type == EMPTY_BUFFER_DONE
        BufferData bufferData;

        // if type == FILL_BUFFER_DONE
        ExtendedBufferData extendedBufferData;

        // if type == FRAME_RENDERED
        RenderData renderData;
    }

    /**
     * The type of the message.
     */
    Type type;

    /**
     * The fence associated with the message.
     */
    @nullable ParcelFileDescriptor fence;

    /**
     * The union of data, discriminated by type.
     */
    Data data;
}
