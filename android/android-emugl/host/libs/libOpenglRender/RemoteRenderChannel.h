// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "android/base/Log.h"

#include "android/base/synchronization/Lock.h"
#include "android/base/synchronization/ConditionVariable.h"

#include "emugl/common/thread.h"
#include "android/emulation/VmLock.h"
#include "OpenglRender/IOStream.h"

#include "android/utils/sockets.h"

#include "android/base/sockets/SocketUtils.h"
#include "android/utils/debug.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <memory>
#include <list>
#include <errno.h>
#include <atomic>

namespace emugl {

class ChannelStream;

using Lock = android::base::Lock;
using AutoLock = android::base::AutoLock;

using ConditionVariable = android::base::ConditionVariable;

typedef struct _PagePacketHead {
    int packet_type : 8;
    //int session_id : 8;
    int packet_body_size : 24;
} __attribute__ ((packed)) PagePacketHead;

#define PAGE_PACKET_HEAD_LEN       (sizeof(PagePacketHead))

class BufferPage {
public:
    BufferPage(int pageId, size_t pageSize) :
        mPageId(pageId),
        mPos(0),
        mSize(pageSize),
        mIsTailPage(false) {
        mBuf = (char*)malloc(pageSize);
    }

    ~BufferPage() {
        if (mBuf) {
            free(mBuf);
        }
    }

    inline int pageID() {
        return mPageId;
    }

    inline char * beginPos() {
        return mBuf;
    }

    inline char * writePos() {
        return mBuf + mPos;
    }

    inline void updateWritePos(size_t pos) {
        mPos = pos;
        assert(mPos <= mSize);
    }

    inline size_t capacity() {
        return mSize - mPos;
    }

    inline bool isFull() {
        return (mPos == mSize);
    }

    inline bool isEmpty() {
        return (mPos == 0);
    }

    inline size_t appendData(char * data, size_t size) {
        assert(data);
        assert(size > 0);
        size_t ret = 0;
        size_t avail = capacity();
        if (size <= avail) {
            ret = size;
        } else {
            ret = avail;
        }

        memcpy(mBuf + mPos, data, ret);
        updateWritePos(mPos + ret);
        //printf("cur write pos (%d) in page(%d)\n", (int)(mPos), mPageId);
        return ret;
    }

    inline void setFlagTail() {
        mIsTailPage = true;
    }

    inline void resetFlagTail() {
        mIsTailPage = false;
    }

    inline bool isTailPage() {
        return mIsTailPage;
    }

    inline void reset() {
        mPos = 0;
    }

private:
    int mPageId;
    size_t mPos;
    size_t mSize;
    char * mBuf;
    bool mIsTailPage;
};

class PageQueue {
public:
    PageQueue(size_t pageSize) : mPageSize(pageSize) {};

    void init(size_t pageCount) {
        if (mPageSize <= 0 || pageCount <= 0)
            return;

        mPageCount = pageCount;

        for (size_t i = 0; i < pageCount; i ++) {
            std::shared_ptr<BufferPage> page = std::make_shared<BufferPage>(i, mPageSize);
            mFreePages.push_back(page);
        }
    }

    bool pushQueue(char * data, size_t size) {
        size_t left = size;

        while (left > 0) {
            if (!mCurPage)
                mCurPage = popFreePage();

            if (!mCurPage)
                return true;

            char * cur = data + size - left;
            size_t ret = pushPage(mCurPage, cur, left);
            left -= ret;

            if (left > 0) {
                pushToCache(mCurPage);
                mCurPage.reset();
            }
        }

        return true;
    }

    void flushQueue() {
        if (mCurPage && !mCurPage->isEmpty()) {
            pushToCache(mCurPage);
            mCurPage.reset();
        }
    }

    std::shared_ptr<BufferPage> popQueue() {
        AutoLock lock(mCachedLock);
        if (mCachedPages.size() > 0) {
            std::shared_ptr<BufferPage> page = mCachedPages.front();
            mCachedPages.pop_front();
            return page;
        } else {
            return std::shared_ptr<BufferPage>();
        }
    }

    void returnToQueue(std::shared_ptr<BufferPage> page) {
        page->reset();
        AutoLock lock(mFreeLock);
        mFreePages.push_back(page);
    }

private:
    size_t pushPage(std::shared_ptr<BufferPage> page, char * data, size_t size) {
        return page->appendData(data,size);
    }

    void pushToCache(std::shared_ptr<BufferPage> page) {
        AutoLock lock(mCachedLock);
        mCachedPages.push_back(page);
    }

    std::shared_ptr<BufferPage> popFreePage() {
        AutoLock lock(mFreeLock);
        if (mFreePages.size() > 0) {
            std::shared_ptr<BufferPage> page = mFreePages.front();
            mFreePages.pop_front();
            return page;
        } else {
            std::shared_ptr<BufferPage> page = std::make_shared<BufferPage>(mPageCount, mPageSize);
            mPageCount ++ ;

            if (mPageCount > (8 * 1024 / 4)) {
                printf("page queue is full\n");
                assert(0);
            }
            return page;
        }
    }

private:
    size_t mPageCount;
    size_t mPageSize;
    std::shared_ptr<BufferPage> mCurPage;
    mutable android::base::Lock mFreeLock;
    std::list<std::shared_ptr<BufferPage> > mFreePages;
    mutable android::base::Lock mCachedLock;
    std::list<std::shared_ptr<BufferPage> > mCachedPages;

};

static std::atomic_int gChannelCount;

class RemoteRenderChannel : public android::base::Thread {
public:
    RemoteRenderChannel () :
         mEpollFD(-1),
         mBufQueue(4 * 1024),
         mSocket(-1), mIsWorking(true),
         mUpStream(NULL), mWantReadSize(0) {
         mRemoteChannelId = gChannelCount.load();
         gChannelCount++;

         //mSocketWaiter =
         //   std::shared_ptr<android::base::SocketWaiter>(
         //   android::base::SocketWaiter::create());

         mEpollFD = ::epoll_create(1);
    }

    ~RemoteRenderChannel() {
        if (mEpollFD > 0)
            ::close(mEpollFD);

        mEpollFD = -1;
    }

    virtual intptr_t main() override;

    inline int sessionId() {
        return mRemoteChannelId;
    }

    void setUpStream(IOStream * stream) {
        mUpStream = stream;
    }

    void startChannel();

    bool initChannel(size_t queueSize);
    
    bool writeChannel(char * data, size_t size);

    bool readChannel(size_t wantReadLen);

    void flushChannel();
    
    void closeChannel();
private:
    
    void modConnection(bool askWrite);
    
    void exitChannel();

    void flushOneFrame();

    void flushOnePage();

    void flushOneWrite();
    
    void onHostSocketEvent(unsigned events);

    bool onNetworkSndDataHeadReady(PagePacketHead& head, size_t * pOffset);
    bool onNetworkSndDataPageReady(std::shared_ptr<BufferPage> page, size_t * pOffset);
    bool onNetworkRecvDataReady(char * buf, size_t * pOffset, size_t wantReadLen);

    void notifyCloseToPeer();
private:
    int mEpollFD;
    PageQueue mBufQueue;
    //android::base::ScopedSocketWatch mSocketFD;
    int mSocket;
    //std::shared_ptr<android::base::SocketWaiter> mSocketWaiter;

    std::atomic_bool mIsWorking;

    Lock mPendingLock;
    ConditionVariable mDataReady;
    std::list<std::shared_ptr<BufferPage> > mPendingPages;

    int mRemoteChannelId;

    IOStream * mUpStream;

    std::atomic_int mWantReadSize;
};

// Shared pointer to RenderChannel instance.
using RemoteRenderChannelPtr = std::shared_ptr<RemoteRenderChannel>;

}  // namespace emugl
