/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.drm.test.testapp;

/**
 * Constant value defines for drm test application.
 */
public class Constants {

    /**
     * Log tag
     */
    public static final String TAG = "DrmTestApplication";

    /**
     * mimetype of a oma drm v1 message file.
     */
    public static final String MIME_OMA_DRM_V1_MESSAGE = "application/vnd.oma.drm.message";

    /**
     * default file extension of a oma drm v1 message file type.
     */
    public static final String FILE_EXT_OMA_DRM_V1_MESSAGE = ".dm";

    /**
     * default file extension of a internal forward lock file type
     */
    public static final String FILE_EXT_FWDL_INTERNAL = ".fl";

    /**
     * option menu items
     */
    public static final int MENU_PLAY = 0;
    public static final int MENU_CONVERT_FORWARD_LOCK = 1;
    public static final int MENU_FILE_INFO = 2;
    public static final int MENU_PLAY_ANDROID_AUDIO = 3;
    public static final int MENU_PLAY_ANDROID_VIDEO = 4;

    /**
     * default buffersize when reading from files.
     */
    public static final int FILE_BUFFER_SIZE = 4096;
}
