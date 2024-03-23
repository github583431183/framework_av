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

import android.hardware.HardwareBuffer;
import android.media.AidlNodeMessage;
import android.media.IAidlBufferSource;
import android.os.ParcelFileDescriptor;

/**
 * Binder interface abstraction for codec2 encoder instance.
 *
 * In order to support Persistent InputSurface and/or MediaRecorder.
 */
interface IAidlNode {
    void freeNode();
    void sendCommand(int cmd, int param);
    byte[] getParameter(int index, in byte[] inParams);
    void setParameter(int index, in byte[] params);
    void setInputSurface(IAidlBufferSource bufferSource);
    void emptyBuffer(
            int buffer,
            in HardwareBuffer hBuffer,
            int flags,
            long timestampUs,
            in @nullable ParcelFileDescriptor fence);
    void dispatchMessage(in AidlNodeMessage message);
}
