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

#ifndef LIVE_SESSION_H_

#define LIVE_SESSION_H_

#include <media/stagefright/foundation/AHandler.h>

#include <utils/String8.h>

namespace android {

struct ABuffer;
struct AnotherPacketSource;
struct DataSource;
struct HTTPBase;
struct IMediaHTTPService;
struct LiveDataSource;
struct M3UParser;
struct PlaylistFetcher;
struct Parcel;

struct LiveSession : public AHandler {
    enum Flags {
        // Don't log any URLs.
        kFlagIncognito = 1,
    };
    LiveSession(
            const sp<AMessage> &notify,
            uint32_t flags,
            const sp<IMediaHTTPService> &httpService);

    enum StreamIndex {
        kAudioIndex    = 0,
        kVideoIndex    = 1,
        kSubtitleIndex = 2,
        kMaxStreams    = 3,
    };

    enum StreamType {
        STREAMTYPE_AUDIO        = 1 << kAudioIndex,
        STREAMTYPE_VIDEO        = 1 << kVideoIndex,
        STREAMTYPE_SUBTITLES    = 1 << kSubtitleIndex,
    };
    status_t dequeueAccessUnit(StreamType stream, sp<ABuffer> *accessUnit);

    status_t getStreamFormat(StreamType stream, sp<AMessage> *format);

    void connectAsync(
            const char *url,
            const KeyedVector<String8, String8> *headers = NULL);

    status_t disconnect();

    // Blocks until seek is complete.
    status_t seekTo(int64_t timeUs);

    status_t getDuration(int64_t *durationUs) const;
    size_t getTrackCount() const;
    sp<AMessage> getTrackInfo(size_t trackIndex) const;
    status_t selectTrack(size_t index, bool select);

    bool isSeekable() const;
    bool hasDynamicDuration() const;

    enum {
        kWhatStreamsChanged,
        kWhatError,
        kWhatPrepared,
        kWhatPreparationFailed,
    };

    // create a format-change discontinuity
    sp<ABuffer> createFormatChangeBuffer();
protected:
    virtual ~LiveSession();

    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    friend struct PlaylistFetcher;

    enum {
        kWhatConnect                    = 'conn',
        kWhatDisconnect                 = 'disc',
        kWhatSeek                       = 'seek',
        kWhatSelectTrack                = 'strk',
        kWhatFetcherNotify              = 'notf',
        kWhatChangeConfiguration        = 'chC0',
        kWhatChangeConfiguration2       = 'chC2',
        kWhatChangeConfiguration3       = 'chC3',
        kWhatFinishDisconnect2          = 'fin2',
    };

    struct BandwidthItem {
        size_t mPlaylistIndex;
        unsigned long mBandwidth;
    };

    struct FetcherInfo {
        sp<PlaylistFetcher> mFetcher;
        int64_t mDurationUs;
        bool mIsPrepared;
        bool mToBeRemoved;
    };

    struct StreamItem {
        const char *mType;
        AString mUri, mNewUri;
        size_t mCurDiscontinuitySeq;
        int64_t mLastDequeuedTimeUs;
        int64_t mLastSampleDurationUs;
        StreamItem()
            : mType(""),
              mCurDiscontinuitySeq(0),
              mLastDequeuedTimeUs(0),
              mLastSampleDurationUs(0) {}
        StreamItem(const char *type)
            : mType(type),
              mCurDiscontinuitySeq(0),
              mLastDequeuedTimeUs(0),
              mLastSampleDurationUs(0) {}
        AString uriKey() {
            AString key(mType);
            key.append("URI");
            return key;
        }
    };
    StreamItem mStreams[kMaxStreams];

    sp<AMessage> mNotify;
    uint32_t mFlags;
    sp<IMediaHTTPService> mHTTPService;

    bool mInPreparationPhase;
    bool mBuffering[kMaxStreams];

    sp<HTTPBase> mHTTPDataSource;
    KeyedVector<String8, String8> mExtraHeaders;

    AString mMasterURL;

    Vector<BandwidthItem> mBandwidthItems;
    ssize_t mCurBandwidthIndex;
    ssize_t mOldFetcherBandwidthIndex;

    sp<M3UParser> mPlaylist;

    KeyedVector<AString, FetcherInfo> mFetcherInfos;
    uint32_t mStreamMask;

    // Masks used during reconfiguration:
    // mNewStreamMask: streams in the variant playlist we're switching to;
    // we don't want to immediately overwrite the original value.
    uint32_t mNewStreamMask;

    KeyedVector<StreamType, sp<AnotherPacketSource> > mDiscontinuities;
    KeyedVector<StreamType, sp<AnotherPacketSource> > mPacketSources;

    int32_t mSwitchGeneration;
    int32_t mSubtitleGeneration;

    size_t mContinuationCounter;
    sp<AMessage> mContinuation;
    sp<AMessage> mSeekReply;

    int64_t mLastDequeuedTimeUs;
    int64_t mRealTimeBaseUs;

    enum ReconfigurationState {
        NONE,
        RECONFIGURATION_PENDING,
        RECONFIGURATION_IN_PROGRESS,
        DISCONNECTING
    };
    ReconfigurationState mReconfigurationState;
    bool mSwitchInProgress;
    uint32_t mDisconnectReplyID;
    uint32_t mSeekReplyID;

    bool mFirstTimeUsValid;
    int64_t mFirstTimeUs;
    int64_t mLastSeekTimeUs;
    KeyedVector<size_t, int64_t> mDiscontinuityAbsStartTimesUs;
    KeyedVector<size_t, int64_t> mDiscontinuityOffsetTimesUs;

    sp<PlaylistFetcher> addFetcher(const char *uri);

    void onConnect(const sp<AMessage> &msg);
    status_t onSeek(const sp<AMessage> &msg);
    void onFinishDisconnect2();

    // If given a non-zero block_size (default 0), it is used to cap the number of
    // bytes read in from the DataSource. If given a non-NULL buffer, new content
    // is read into the end.
    //
    // The DataSource we read from is responsible for signaling error or EOF to help us
    // break out of the read loop. The DataSource can be returned to the caller, so
    // that the caller can reuse it for subsequent fetches (within the initially
    // requested range).
    //
    // For reused HTTP sources, the caller must download a file sequentially without
    // any overlaps or gaps to prevent reconnection.
    ssize_t fetchFile(
            const char *url, sp<ABuffer> *out,
            /* request/open a file starting at range_offset for range_length bytes */
            int64_t range_offset = 0, int64_t range_length = -1,
            /* download block size */
            uint32_t block_size = 0,
            /* reuse DataSource if doing partial fetch */
            sp<DataSource> *source = NULL,
            String8 *actualUrl = NULL);

    sp<M3UParser> fetchPlaylist(
            const char *url, uint8_t *curPlaylistHash, bool *unchanged);

    size_t getBandwidthIndex();
    int64_t latestMediaSegmentStartTimeUs();

    static int SortByBandwidth(const BandwidthItem *, const BandwidthItem *);
    static StreamType indexToType(int idx);
    static ssize_t typeToIndex(int32_t type);

    void changeConfiguration(
            int64_t timeUs, size_t bandwidthIndex, bool pickTrack = false);
    void onChangeConfiguration(const sp<AMessage> &msg);
    void onChangeConfiguration2(const sp<AMessage> &msg);
    void onChangeConfiguration3(const sp<AMessage> &msg);

    void tryToFinishBandwidthSwitch();

    void cancelBandwidthSwitch();

    void getBandwidths(unsigned long &oldBandwidth, unsigned long &newBandwidth);

    void finishDisconnect();

    void postPrepared(status_t err);

    bool isReconfiguring();
    bool bandwidthChanged();

    void onSelectTrack(const sp<AMessage> &msg);

    DISALLOW_EVIL_CONSTRUCTORS(LiveSession);
};

}  // namespace android

#endif  // LIVE_SESSION_H_
