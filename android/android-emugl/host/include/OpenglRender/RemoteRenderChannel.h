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

#include "android/base/async/Looper.h"
#include "android/base/async/ThreadLooper.h"
#include "android/emulation/VmLock.h"


#include "android/base/async/ScopedSocketWatch.h"
#include "android/utils/sockets.h"
#include "android/base/sockets/SocketWaiter.h"

#include "android/base/sockets/ScopedSocket.h"
#include "android/base/sockets/SocketUtils.h"
#include "android/utils/debug.h"


#include <functional>
#include <memory>
#include <list>
#include <errno.h>
#include <atomic>

namespace emugl {

typedef struct _PagePacketHead {
    int packet_type : 8;
    //int session_id : 8;
    int packet_body_size : 24;
} __attribute__ ((packed)) PagePacketHead;

#define PAGE_PACKET_HEAD_LEN       (sizeof(PagePacketHead))

using SocketWaiter = android::base::SocketWaiter;
using Lock = android::base::Lock;
using AutoLock = android::base::AutoLock;
using ScopedSocket = android::base::ScopedSocket;
using FdWatch = android::base::Looper::FdWatch;
using ConditionVariable = android::base::ConditionVariable;


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

            if (mPageCount > 1000) {
                printf("page queue is full\n");
                assert(0);
                return std::shared_ptr<BufferPage>();
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

class OpenGLESDataHandler : public android::base::Thread {
public:
    OpenGLESDataHandler() : mIsWorking(false) {};

    virtual intptr_t main() override {
        while (mIsWorking) {

            AutoLock lock(mQueueLock);
            while (mHandlerQueue.size() == 0) {
                mDataReady.wait(&mQueueLock);
            }

            if (!mIsWorking)
                break;

            std::function<void()> func = mHandlerQueue.front();
            mHandlerQueue.pop_front();

            lock.unlock();

            (func)();
        }
        return 0;
    }

    void StartHandler() {
        mIsWorking = true;
        start();
    }

    void StopHandler() {
        mIsWorking = false;
        wait();
    }

    void NotifyDataReady() {
        mDataReady.signal();
    }

    void PushBack(std::function<void()> func) {
        AutoLock lock(mQueueLock);
        mHandlerQueue.push_back(func);
        mDataReady.signal();
    }

private:
    bool mIsWorking;
    Lock mQueueLock;
    Lock mLock;
    ConditionVariable mDataReady;
    std::list<std::function<void()> > mHandlerQueue;
};

static std::atomic_int gChannelCount;

class RemoteRenderChannel : public android::base::Thread {
public:
    RemoteRenderChannel () :
         mBufQueue(10 * 1024),
         mSocket(-1), mIsWorking(false) {
         mRemoteChannelId = gChannelCount.load();
         gChannelCount++;

         mSocketWaiter =
            std::shared_ptr<android::base::SocketWaiter>(
            android::base::SocketWaiter::create());
    }

    virtual intptr_t main() override {
        while (mIsWorking) {

            AutoLock lock(mPendingLock);
            while (mPendingPages.size() == 0) {
                mDataReady.wait(&mPendingLock);
            }

            if (!mIsWorking) {
                break;
            }

            //if (mSocketFD.get()) {
            //    mSocketFD->wantWrite();
            //}

            std::shared_ptr<BufferPage> page = mPendingPages.front();
            mPendingPages.pop_front();

            lock.unlock();

            if (!onNetworkDataPageReady(page))
                break;

            mBufQueue.returnToQueue(page);

            if (!mIsWorking) {
                break;
            }
        }

        mIsWorking = false;
        printf("exit remote render thread\n");
        return 0;
    }

    int sessionId() {
        return mRemoteChannelId;
    }

    bool initChannel(size_t queueSize) {
        mBufQueue.init(queueSize);

        // Add Tcp Channel for comunication
        const char* render_server_hostname = getenv("render_server_hostname");
        if (!render_server_hostname) {
            fprintf(stdout, "Cannot find render server hostname\n");
            render_server_hostname = "127.0.0.1";
        }
        const char* render_server_port = getenv("render_server_port");
        if (!render_server_port) {
            fprintf(stdout, "Cannot find render server port\n");
            render_server_port = "23432";
        }
        printf("new connection %s : %s\n", render_server_hostname, render_server_port);

        int socket = android::base::socketTcp4LoopbackClient(atoi(render_server_port));
        if (socket == -1) {
            fprintf(stderr, "%s: cannot connect to rendering server.(%s)\n", __func__, errno_str);
            return false;
        }

        mIsWorking = true;
        start();

        android::base::socketSetNonBlocking(socket);
        // socketSetNoDelay() reduces the latency of sending data, at the cost
        // of creating more TCP packets on the connection. It's useful when
        // doing lots of small send() calls, like the ADB protocol requires.
        // And since this is on localhost, the packet increase should not be
        // noticeable.
        android::base::socketSetNoDelay(socket);

        mSocket = socket;
        //mSocketFD.reset(android::base::ThreadLooper::get()->createFdWatch(
        //        socket,
        //        [](void* opaque, int fd, unsigned events) {
        //            ((RemoteRenderChannel*)opaque)->onHostSocketEvent(events);
        //        },
        //        this));

        return true;
    }

    bool writeChannel(char * data, size_t size) {
        if (!mIsWorking)
            return false;

        mBufQueue.pushQueue(data, size);
        flushOneWrite();

        return true;
    }

    // be compatible with old interface
    int socket() {
        return mSocket;
    }

    void flushOneFrame() {
        if (!mIsWorking)
            return;

        mBufQueue.flushQueue();

        while (1) {
            std::shared_ptr<BufferPage> page = mBufQueue.popQueue();
            if (!page)
                break;

            AutoLock lock(mPendingLock);
            mPendingPages.push_back(page);
        }

        mDataReady.signal();
    }

    void flushOnePage() {
        if (!mIsWorking)
            return;

        std::shared_ptr<BufferPage> page = mBufQueue.popQueue();
        if (!page)
            return;

        AutoLock lock(mPendingLock);
        mPendingPages.push_back(page);

        mDataReady.signal();
    }

    void flushOneWrite() {
        flushOneFrame();
    }

    void closeChannel() {
        mIsWorking = false;
        wait();
        android::base::socketClose(mSocket);
    }
private:
    void onHostSocketEvent(unsigned events) {
        //if ((events & FdWatch::kEventWrite) != 0) {
        //    mSocket->dontWantWrite();
        //    mDataReady.signal();
        //}
    }

    bool onNetworkDataPageReady(std::shared_ptr<BufferPage> page) {
        //printf("0x%lx %s : page size = %d, opcode = %d session = %d\n", android::base::getCurrentThreadId(), __func__, (int)(page->writePos() - page->beginPos()), *(int*)page->beginPos(), (int)mRemoteChannelId);
        if (mSocket < 0 || !page) {
            assert(0);
            return false;
        }

        PagePacketHead head;
        head.packet_type = 0;
        //head.session_id = mRemoteChannelId;
        head.packet_body_size = page->writePos() - page->beginPos();

        ssize_t sentLen = 0;
        while (mIsWorking) {
            mSocketWaiter->update(mSocket, android::base::SocketWaiter::kEventWrite);
            int ret = mSocketWaiter->wait(50);
            if (ret == 0 && errno == ETIMEDOUT)
                continue;

            if (ret < 0)
                return false;

            unsigned events = 0;
            int fd = mSocketWaiter->nextPendingFd(&events);
            if (fd < 0) {
                return false;
            }

            if ((events & SocketWaiter::kEventWrite) == SocketWaiter::kEventWrite) {
                ssize_t retHead = android::base::socketSend(mSocket,
                                &head + sentLen, PAGE_PACKET_HEAD_LEN - sentLen);

                if (retHead < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                        }
                    else {
                        android::base::socketClose(mSocket);
                        assert(0);
                        return false;
                    }
                }

                sentLen += retHead;

                if (sentLen == (ssize_t)PAGE_PACKET_HEAD_LEN)
                    break;
            }
        }

        sentLen = 0;

        while (mIsWorking) {
            mSocketWaiter->update(mSocket, android::base::SocketWaiter::kEventWrite);
            int ret = mSocketWaiter->wait(50);
            if (ret == 0 && errno == ETIMEDOUT)
                continue;

            if (ret < 0)
                return false;

            unsigned events = 0;
            int fd = mSocketWaiter->nextPendingFd(&events);
            if (fd < 0) {
                return false;
            }

            if ((events & SocketWaiter::kEventWrite) == SocketWaiter::kEventWrite) {
                ssize_t retBody = android::base::socketSend(mSocket,
                              page->beginPos() + sentLen, page->writePos() - page->beginPos() - sentLen);

                if (retBody < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                        }
                    else {
                        android::base::socketClose(mSocket);
                        assert(0);
                        return false;
                    }
                }

                sentLen += retBody;

                if (sentLen == (ssize_t)(page->writePos() - page->beginPos()))
                    break;
            }
        }

/*
        PagePacketHead head;
        head.packet_type = 0;
        head.session_id = mRemoteChannelId;
        head.packet_body_size = page->writePos() - page->beginPos();

        ssize_t sentLen = 0;
        while (mIsWorking) {
            ssize_t retHead;
            {
                //android::ScopedVmUnlock unlockBql;
                retHead = android::base::socketSend(mSocket,
                                                &head + sentLen, PAGE_PACKET_HEAD_LEN - sentLen);
            }

            if (retHead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                    }
                else {
                    android::base::socketClose(mSocket);
                    assert(0);
                    return;
                }
            }

            sentLen += retHead;

            if (sentLen == (ssize_t)PAGE_PACKET_HEAD_LEN)
                break;
        }

        sentLen = 0;
        while (mIsWorking) {
            ssize_t ret;
            {
                //android::ScopedVmUnlock unlockBql;
                ret = android::base::socketSend(mSocket,
                                                page->beginPos() + sentLen, page->writePos() - page->beginPos() - sentLen);
            }

            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                    }
                else {
                    android::base::socketClose(mSocket);
                    assert(0);
                    return;
                }
            }

            sentLen += ret;

            if (sentLen == (ssize_t)(page->writePos() - page->beginPos()))
                break;

        }
*/
        printf("0x%lx %s : send %d + %d\n", android::base::getCurrentThreadId(), __func__, (int)PAGE_PACKET_HEAD_LEN, (int)(page->writePos() - page->beginPos()));
        return true;
    }

private:
    PageQueue mBufQueue;
    //android::base::ScopedSocketWatch mSocketFD;
    int mSocket;
    std::shared_ptr<android::base::SocketWaiter> mSocketWaiter;

    std::atomic_bool mIsWorking;

    Lock mPendingLock;
    ConditionVariable mDataReady;
    std::list<std::shared_ptr<BufferPage> > mPendingPages;

    int mRemoteChannelId;
};

// Shared pointer to RenderChannel instance.
using RemoteRenderChannelPtr = std::shared_ptr<RemoteRenderChannel>;

}  // namespace emugl
