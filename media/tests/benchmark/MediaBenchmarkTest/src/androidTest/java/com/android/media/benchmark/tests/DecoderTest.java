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

package com.android.media.benchmark.tests;

import android.content.Context;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.media.benchmark.R;
import com.android.media.benchmark.library.Decoder;
import com.android.media.benchmark.library.Extractor;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

@RunWith(Parameterized.class)
public class DecoderTest {
    private static final Context mContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private static final String mInputFilePath = mContext.getString(R.string.input_file_path);
    private static final String mOutputFilePath = mContext.getString(R.string.output_file_path);
    private static final String TAG = "DecoderTest";
    private static final boolean DEBUG = false;
    private static final boolean WRITE_OUTPUT = false;
    private String mInputFile;
    private String mCodecName;
    private boolean mAsyncMode;

    public DecoderTest(String inputFile, String codecName, boolean asyncMode) {
        this.mInputFile = inputFile;
        this.mCodecName = codecName;
        this.mAsyncMode = asyncMode;
    }

    @Parameterized.Parameters
    public static Collection<Object[]> input() {
        return Arrays.asList(new Object[][]{
                //Audio Sync Test
                {"bbb_44100hz_2ch_128kbps_aac_30sec.mp4", "", false},
                {"bbb_44100hz_2ch_128kbps_mp3_30sec.mp3", "", false},
                {"bbb_8000hz_1ch_8kbps_amrnb_30sec.3gp", "", false},
                {"bbb_16000hz_1ch_9kbps_amrwb_30sec.3gp", "", false},
                {"bbb_44100hz_2ch_80kbps_vorbis_30sec.mp4", "", false},
                {"bbb_44100hz_2ch_600kbps_flac_30sec.mp4", "", false},
                {"bbb_48000hz_2ch_100kbps_opus_30sec.webm", "", false},
                // Audio Async Test
                {"bbb_44100hz_2ch_128kbps_aac_30sec.mp4", "", true},
                {"bbb_44100hz_2ch_128kbps_mp3_30sec.mp3", "", true},
                {"bbb_8000hz_1ch_8kbps_amrnb_30sec.3gp", "", true},
                {"bbb_16000hz_1ch_9kbps_amrwb_30sec.3gp", "", true},
                {"bbb_44100hz_2ch_80kbps_vorbis_30sec.mp4", "", true},
                {"bbb_44100hz_2ch_600kbps_flac_30sec.mp4", "", true},
                {"bbb_48000hz_2ch_100kbps_opus_30sec.webm", "", true},
                // Video Sync Hardware Test
                {"crowd_1920x1080_25fps_4000kbps_vp9.webm", "", false},
                {"crowd_1920x1080_25fps_4000kbps_vp8.webm", "", false},
                {"crowd_1920x1080_25fps_4000kbps_av1.webm", "", false},
                {"crowd_1920x1080_25fps_7300kbps_mpeg2.mp4", "", false},
                {"crowd_1920x1080_25fps_6000kbps_mpeg4.mp4", "", false},
                {"crowd_352x288_25fps_6000kbps_h263.3gp", "", false},
                {"crowd_1920x1080_25fps_6700kbps_h264.ts", "", false},
                {"crowd_1920x1080_25fps_4000kbps_h265.mkv", "", false},
                // Video Sync Software Test
                {"crowd_1920x1080_25fps_4000kbps_vp9.webm", "c2.android.vp9.decoder", false},
                {"crowd_1920x1080_25fps_4000kbps_vp8.webm", "c2.android.vp8.decoder", false},
                {"crowd_1920x1080_25fps_4000kbps_av1.webm", "c2.android.av1.decoder", false},
                {"crowd_1920x1080_25fps_7300kbps_mpeg2.mp4", "c2.android.mpeg2.decoder", false},
                {"crowd_1920x1080_25fps_6000kbps_mpeg4.mp4", "c2.android.mpeg4.decoder", false},
                {"crowd_352x288_25fps_6000kbps_h263.3gp", "c2.android.h263.decoder", false},
                {"crowd_1920x1080_25fps_6700kbps_h264.ts", "c2.android.avc.decoder", false},
                {"crowd_1920x1080_25fps_4000kbps_h265.mkv", "c2.android.hevc.decoder", false},
                // Video Async Hardware Test
                {"crowd_1920x1080_25fps_4000kbps_vp9.webm", "", true},
                {"crowd_1920x1080_25fps_4000kbps_vp8.webm", "", true},
                {"crowd_1920x1080_25fps_4000kbps_av1.webm", "", true},
                {"crowd_1920x1080_25fps_7300kbps_mpeg2.mp4", "", true},
                {"crowd_1920x1080_25fps_6000kbps_mpeg4.mp4", "", true},
                {"crowd_352x288_25fps_6000kbps_h263.3gp", "", true},
                {"crowd_1920x1080_25fps_6700kbps_h264.ts", "", true},
                {"crowd_1920x1080_25fps_4000kbps_h265.mkv", "", true},
                // Video Async Software Test
                {"crowd_1920x1080_25fps_4000kbps_vp9.webm", "c2.android.vp9.decoder", true},
                {"crowd_1920x1080_25fps_4000kbps_vp8.webm", "c2.android.vp8.decoder", true},
                {"crowd_1920x1080_25fps_4000kbps_av1.webm", "c2.android.av1.decoder", true},
                {"crowd_1920x1080_25fps_7300kbps_mpeg2.mp4", "c2.android.mpeg2.decoder", true},
                {"crowd_1920x1080_25fps_6000kbps_mpeg4.mp4", "c2.android.mpeg4.decoder", true},
                {"crowd_352x288_25fps_6000kbps_h263.3gp", "c2.android.h263.decoder", true},
                {"crowd_1920x1080_25fps_6700kbps_h264.ts", "c2.android.avc.decoder", true},
                {"crowd_1920x1080_25fps_4000kbps_h265.mkv", "c2.android.hevc.decoder", true}});
    }

    @Test
    public void testDecoder() throws IOException {
        File inputFile = new File(mInputFilePath + mInputFile);
        if (inputFile.exists()) {
            FileInputStream fileInput = new FileInputStream(inputFile);
            FileDescriptor fileDescriptor = fileInput.getFD();
            Extractor extractor = new Extractor();
            int trackCount = extractor.setUpExtractor(fileDescriptor);
            ArrayList<ByteBuffer> inputBuffer = new ArrayList<>();
            ArrayList<MediaCodec.BufferInfo> frameInfo = new ArrayList<>();
            for (int currentTrack = 0; currentTrack < trackCount; currentTrack++) {
                extractor.selectExtractorTrack(currentTrack);
                MediaFormat format = extractor.getFormat(currentTrack);
                // Get samples from extractor
                int sampleSize;
                do {
                    sampleSize = extractor.getFrameSample();
                    MediaCodec.BufferInfo bufInfo = new MediaCodec.BufferInfo();
                    MediaCodec.BufferInfo info = extractor.getBufferInfo();
                    ByteBuffer dataBuffer = ByteBuffer.allocate(info.size);
                    dataBuffer.put(extractor.getFrameBuffer().array(), 0, info.size);
                    bufInfo.set(info.offset, info.size, info.presentationTimeUs, info.flags);
                    inputBuffer.add(dataBuffer);
                    frameInfo.add(bufInfo);
                    if (DEBUG) {
                        Log.d(TAG,
                                "Extracted bufInfo: flag = " + bufInfo.flags + " timestamp = "
                                        + bufInfo.presentationTimeUs + " size = " + bufInfo.size);
                    }
                } while (sampleSize > 0);
                FileOutputStream decodeOutputStream = null;
                if (WRITE_OUTPUT) {
                    if (!Paths.get(mOutputFilePath).toFile().exists()) {
                        Files.createDirectories(Paths.get(mOutputFilePath));
                    }
                    File outFile = new File(mOutputFilePath + "decoder.out");
                    if (outFile.exists()) {
                        if (!outFile.delete()) {
                            Log.e(TAG, " Unable to delete existing file" + outFile.toString());
                        }
                    }
                    if (outFile.createNewFile()) {
                        decodeOutputStream = new FileOutputStream(outFile);
                    } else {
                        Log.e(TAG, "Unable to create file: " + outFile.toString());
                    }
                }
                Decoder decoder = new Decoder();
                decoder.setupDecoder(decodeOutputStream);
                int status = decoder.decode(inputBuffer, frameInfo, mAsyncMode, format, mCodecName);
                decoder.deInitCodec();
                if (status == 0) {
                    decoder.dumpStatistics(mInputFile, extractor.getClipDuration());
                    Log.i(TAG, "Decoding Successful.");
                } else {
                    Log.e(TAG, "Decode returned error. " + status);
                }
                decoder.resetDecoder();
                extractor.unselectExtractorTrack(currentTrack);
                inputBuffer.clear();
                frameInfo.clear();
                if (decodeOutputStream != null) {
                    decodeOutputStream.close();
                }
            }
            extractor.deinitExtractor();
            fileInput.close();
        } else {
            Log.w(TAG,
                    "Warning: Test Skipped. Cannot find " + mInputFile + " in directory "
                            + mInputFilePath);
        }
    }
}
