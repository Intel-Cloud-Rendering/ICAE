#include "RemoteRenderChannel.h"

namespace emugl {

typedef struct _PagePacketHead {
    int packet_type : 8;
    //int session_id : 8;
    int packet_body_size : 24;
} __attribute__ ((packed)) PagePacketHead;

#define PAGE_PACKET_HEAD_LEN       (sizeof(PagePacketHead))

bool RemoteRenderChannel::initChannel(size_t queueSize) {
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

    int socket = android::base::socketTcp4Client(render_server_hostname, atoi(render_server_port));
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

bool RemoteRenderChannel::writeChannel(char * data, size_t size) {
    if (!mIsWorking)
        return false;

    mBufQueue.pushQueue(data, size);
    flushOneWrite();

    return true;
}

void RemoteRenderChannel::flushOneFrame() {
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

void RemoteRenderChannel::flushOnePage() {
    if (!mIsWorking)
        return;

    std::shared_ptr<BufferPage> page = mBufQueue.popQueue();
    if (!page)
        return;

    AutoLock lock(mPendingLock);
    mPendingPages.push_back(page);

    mDataReady.signal();
}

void RemoteRenderChannel::flushOneWrite() {
    flushOneFrame();
}

void RemoteRenderChannel::closeChannel() {
    mIsWorking = false;
    wait();
    android::base::socketClose(mSocket);
}

void RemoteRenderChannel::onHostSocketEvent(unsigned events) {
    //if ((events & FdWatch::kEventWrite) != 0) {
    //    mSocket->dontWantWrite();
    //    mDataReady.signal();
    //}
}

bool RemoteRenderChannel::onNetworkDataPageReady(std::shared_ptr<BufferPage> page) {
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
}
