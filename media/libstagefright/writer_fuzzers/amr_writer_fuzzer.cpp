/******************************************************************************
 *
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
 */

#include "WriterFuzzerBase.h"

#include <media/stagefright/AMRWriter.h>

using namespace android;

class AmrWriter : public WriterFuzzerBase {
   public:
    bool createWriter();
};

bool AmrWriter::createWriter() {
    mWriter = new AMRWriter(mFd);
    if (!mWriter) {
        return false;
    }
    mFileMeta = new MetaData;
#ifdef AMRNB
    mFileMeta->setInt32(kKeyFileType, output_format::OUTPUT_FORMAT_AMR_NB);
#else
    mFileMeta->setInt32(kKeyFileType, output_format::OUTPUT_FORMAT_AMR_WB);
#endif
    return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    AmrWriter writer = AmrWriter();
    writer.processData(data, size);
    return 0;
}
