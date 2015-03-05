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

#ifndef A_MESSAGE_H_

#define A_MESSAGE_H_

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/ALooper.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>

namespace android {

struct ABuffer;
struct AHandler;
struct AString;
struct Parcel;

struct AReplyToken : public RefBase {
    AReplyToken(const sp<ALooper> &looper)
        : mLooper(looper),
          mReplied(false) {
    }

private:
    friend struct AMessage;
    friend struct ALooper;
    wp<ALooper> mLooper;
    sp<AMessage> mReply;
    bool mReplied;

    sp<ALooper> getLooper() const {
        return mLooper.promote();
    }
    // if reply is not set, returns false; otherwise, it retrieves the reply and returns true
    bool retrieveReply(sp<AMessage> *reply) {
        if (mReplied) {
            *reply = mReply;
            mReply.clear();
        }
        return mReplied;
    }
    // sets the reply for this token. returns OK or error
    status_t setReply(const sp<AMessage> &reply);
};

struct AMessage : public RefBase {
    AMessage();
    AMessage(uint32_t what, const sp<const AHandler> &handler);

    static sp<AMessage> FromParcel(const Parcel &parcel);
    void writeToParcel(Parcel *parcel) const;

    void setWhat(uint32_t what);
    uint32_t what() const;

    void setTarget(const sp<const AHandler> &handler);

    void clear();

    void setInt32(const char *name, int32_t value);
    void setInt64(const char *name, int64_t value);
    void setSize(const char *name, size_t value);
    void setFloat(const char *name, float value);
    void setDouble(const char *name, double value);
    void setPointer(const char *name, void *value);
    void setString(const char *name, const char *s, ssize_t len = -1);
    void setString(const char *name, const AString &s);
    void setObject(const char *name, const sp<RefBase> &obj);
    void setBuffer(const char *name, const sp<ABuffer> &buffer);
    void setMessage(const char *name, const sp<AMessage> &obj);

    void setRect(
            const char *name,
            int32_t left, int32_t top, int32_t right, int32_t bottom);

    bool contains(const char *name) const;

    bool findInt32(const char *name, int32_t *value) const;
    bool findInt64(const char *name, int64_t *value) const;
    bool findSize(const char *name, size_t *value) const;
    bool findFloat(const char *name, float *value) const;
    bool findDouble(const char *name, double *value) const;
    bool findPointer(const char *name, void **value) const;
    bool findString(const char *name, AString *value) const;
    bool findObject(const char *name, sp<RefBase> *obj) const;
    bool findBuffer(const char *name, sp<ABuffer> *buffer) const;
    bool findMessage(const char *name, sp<AMessage> *obj) const;

    bool findRect(
            const char *name,
            int32_t *left, int32_t *top, int32_t *right, int32_t *bottom) const;

    status_t post(int64_t delayUs = 0);

    // Posts the message to its target and waits for a response (or error)
    // before returning.
    status_t postAndAwaitResponse(sp<AMessage> *response);

    // If this returns true, the sender of this message is synchronously
    // awaiting a response and the reply token is consumed from the message
    // and stored into replyID. The reply token must be used to send the response
    // using "postReply" below.
    bool senderAwaitsResponse(sp<AReplyToken> *replyID);

    // Posts the message as a response to a reply token.  A reply token can
    // only be used once. Returns OK if the response could be posted; otherwise,
    // an error.
    status_t postReply(const sp<AReplyToken> &replyID);

    // Performs a deep-copy of "this", contained messages are in turn "dup'ed".
    // Warning: RefBase items, i.e. "objects" are _not_ copied but only have
    // their refcount incremented.
    sp<AMessage> dup() const;

    AString debugString(int32_t indent = 0) const;

    enum Type {
        kTypeInt32,
        kTypeInt64,
        kTypeSize,
        kTypeFloat,
        kTypeDouble,
        kTypePointer,
        kTypeString,
        kTypeObject,
        kTypeMessage,
        kTypeRect,
        kTypeBuffer,
    };

    size_t countEntries() const;
    const char *getEntryNameAt(size_t index, Type *type) const;

protected:
    virtual ~AMessage();

private:
    friend struct ALooper; // deliver()

    uint32_t mWhat;

    // used only for debugging
    ALooper::handler_id mTarget;

    wp<AHandler> mHandler;
    wp<ALooper> mLooper;

    struct Rect {
        int32_t mLeft, mTop, mRight, mBottom;
    };

    struct Item {
        union {
            int32_t int32Value;
            int64_t int64Value;
            size_t sizeValue;
            float floatValue;
            double doubleValue;
            void *ptrValue;
            RefBase *refValue;
            AString *stringValue;
            Rect rectValue;
        } u;
        const char *mName;
        size_t      mNameLength;
        Type mType;
        void setName(const char *name, size_t len);
    };

    enum {
        kMaxNumItems = 64
    };
    Item mItems[kMaxNumItems];
    size_t mNumItems;

    Item *allocateItem(const char *name);
    void freeItemValue(Item *item);
    const Item *findItem(const char *name, Type type) const;

    void setObjectInternal(
            const char *name, const sp<RefBase> &obj, Type type);

    size_t findItemIndex(const char *name, size_t len) const;

    void deliver();

    DISALLOW_EVIL_CONSTRUCTORS(AMessage);
};

}  // namespace android

#endif  // A_MESSAGE_H_
