#include "RemoteRenderChannel.h"

namespace emugl {

intptr_t RemoteRenderChannel::main() {
    PagePacketHead head;
    size_t rwLen = 0;
    std::shared_ptr<BufferPage> curPage;
    bool isHeadSending = true;
    unsigned char * readBuf = NULL;
    size_t readBufSize = 0;
    bool unKnownSizeDataReady = false;

    struct epoll_event events[1];
    
    while (mIsWorking.load()) {

        int ret = ::epoll_wait(mEpollFD, events, 1, 20);

        if (ret == 0) {
            continue;
        }

        if (ret == -1) {
            if (errno == EINTR)
                continue;
            else {
                exitChannel();
                assert(0);
                break;
            }
        }
        
        if ((events[0].events & EPOLLERR) ||  
              (events[0].events & EPOLLHUP) ||
              (events[0].events & EPOLLRDHUP)) {  
              //printf("found a connection exited\n");  
              exitChannel();
        } else if (events[0].events & EPOLLIN) {
            if (!mUpStream) {
                exitChannel();
                assert(0);
                continue;
            }

            //if ((int)mWantReadSize == 0) {
            //    printf("wait readChannel, hang up occur !!!!!!!!!!!!!\n");
            //    //assert(0);
            //    continue;
            //}

            while (1) {
                if (readBuf == NULL) {
                    rwLen = 0;
                    readBufSize = (size_t)mWantReadSize;
                    if (readBufSize <= 0) {
                        // in this case, data from peer arrived before decoding
                        // thus, we cannot know the recving data size
                        // make a assumtion first, use a default buffer to receive the data
                        // then update the received data size later.
                        printf("unknow size data ready, possible hang up occur !!!!!!!!!!!!!\n");
                        unKnownSizeDataReady = true;
                        readBufSize = 512; //because channel buffer default size is 512
                    }

                    readBuf = mUpStream->alloc(readBufSize);
                }

                if (onNetworkRecvDataReady((char*)readBuf, &rwLen, readBufSize - rwLen)) {
                    if (!unKnownSizeDataReady) {
                        if (rwLen == readBufSize) {
                            readBuf = NULL;
                            mWantReadSize -= readBufSize;
                            mUpStream->flush();
                        }

                        break;
                    } else {
                        if (rwLen == readBufSize) {
                            readBuf = NULL;
                            mWantReadSize -= rwLen;
                            mUpStream->flush(rwLen);
                            continue;
                        } else {
                            unKnownSizeDataReady = false;
                            readBuf = NULL;
                            mWantReadSize -= rwLen;
                            mUpStream->flush(rwLen);
                            break;
                        }
                    }
                } else {
                    exitChannel();
                    assert(0);
                    break;
                }
            }

           
        } else if (events[0].events & EPOLLOUT) {
            while (1) {//loop due to set EPOLLET
                if (!curPage) {
                    AutoLock lock(mPendingLock);
                    if (mPendingPages.size() == 0) {
                        modConnection(false);
                        break;
                    }
                        
                    curPage = mPendingPages.front();
                    mPendingPages.pop_front();
                    lock.unlock();
                    isHeadSending = true;

                    // build a packethead
                    head.packet_type = 0;
                    //head.session_id = mRemoteChannelId;
                    head.packet_body_size = curPage->writePos() - curPage->beginPos();

                    rwLen = 0;
                }
                
                if (isHeadSending) {
                    if (onNetworkSndDataHeadReady(head, &rwLen)) {
                        if (rwLen == PAGE_PACKET_HEAD_LEN) {
                            isHeadSending = false;
                            rwLen = 0;
                        } else
                            break;
                    } else {
                        exitChannel();
                        break;
                    }
                }

                if (onNetworkSndDataPageReady(curPage, &rwLen)) {
                    if (rwLen == (size_t)(curPage->writePos() - curPage->beginPos())) {
                        mBufQueue.returnToQueue(curPage);
                        curPage.reset();
                    } else
                        break;
                        
                } else {
                    exitChannel();
                    assert(0);
                }
            }
        }

    }

    // if need, enable the following code to send a packet close message to remote render
    // currently, remote can close the renderthread by detecting the socket disconnection
    //notifyCloseToPeer();
    printf("exit remote render channel thread\n");
    return 0;
}

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

    mIsWorking.store(true);

    android::base::socketSetNonBlocking(socket);
    // socketSetNoDelay() reduces the latency of sending data, at the cost
    // of creating more TCP packets on the connection. It's useful when
    // doing lots of small send() calls, like the ADB protocol requires.
    // And since this is on localhost, the packet increase should not be
    // noticeable.
    android::base::socketSetNoDelay(socket);

    mSocket = socket;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = socket;
    ::epoll_ctl(mEpollFD, EPOLL_CTL_ADD, socket, &ev);

    //startChannel();

    return true;
}

void RemoteRenderChannel::startChannel() {
    start();
}

void RemoteRenderChannel::modConnection(bool askWrite)
{
    if (mSocket <= 0)
        return;
    
    struct epoll_event ev;

    ev.data.fd = mSocket;
    if (askWrite)
        ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
    else
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

    ::epoll_ctl(mEpollFD, EPOLL_CTL_MOD, mSocket, &ev);
}

bool RemoteRenderChannel::writeChannel(char * data, size_t size) {
    if (!mIsWorking.load())
        return false;

    mBufQueue.pushQueue(data, size);
    flushOneWrite();

    return true;
}

void RemoteRenderChannel::flushChannel() {
    if (!mIsWorking.load())
        return;

    mBufQueue.flushQueue();

    while (1) {
        std::shared_ptr<BufferPage> page = mBufQueue.popQueue();
        if (!page)
            break;

        AutoLock lock(mPendingLock);
        mPendingPages.push_back(page);
    }

    modConnection(true);
}

void RemoteRenderChannel::flushOnePage() {
    if (!mIsWorking.load())
        return;

    std::shared_ptr<BufferPage> page = mBufQueue.popQueue();
    if (!page)
        return;

    AutoLock lock(mPendingLock);
    mPendingPages.push_back(page);

    mDataReady.signal();
}

void RemoteRenderChannel::flushOneWrite() {
    flushChannel();
}

void RemoteRenderChannel::flushOneFrame() {
    flushChannel();
}

void RemoteRenderChannel::exitChannel() {
    mIsWorking.store(false);

    ::epoll_ctl(mEpollFD, EPOLL_CTL_DEL, mSocket, NULL);
    
    android::base::socketShutdownWrites(mSocket);
    android::base::socketShutdownReads(mSocket);
    android::base::socketClose(mSocket);
    mSocket = -1;
}

void RemoteRenderChannel::closeChannel() {
    exitChannel();

    wait();

    if (mEpollFD > 0)
        ::close(mEpollFD);

    mEpollFD = -1;
}

void RemoteRenderChannel::onHostSocketEvent(unsigned events) {
    //if ((events & FdWatch::kEventWrite) != 0) {
    //    mSocket->dontWantWrite();
    //    mDataReady.signal();
    //}
}

void RemoteRenderChannel::notifyCloseToPeer()
{
    if (mSocket < 0) {
        assert(0);
        return;
    }

    PagePacketHead head;
    head.packet_type = 1;
    head.packet_body_size = 0;

    android::base::socketSendAll(mSocket,
          &head, PAGE_PACKET_HEAD_LEN);
}

bool RemoteRenderChannel::onNetworkRecvDataReady(char * buf, size_t * pOffset, size_t wantReadLen) {

    ssize_t readLen = 0;
    while (1) {
        ssize_t retRead = android::base::socketRecv(mSocket,
                            buf + *pOffset + readLen, wantReadLen - readLen);

        if (retRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                //android::base::socketClose(mSocket);
                assert(0);
                return false;
            }
        }

        readLen += retRead;

        if (wantReadLen == (size_t)readLen)
            break;
    }

    *pOffset += readLen;

    //printf("0x%lx %s : recv %d\n", android::base::getCurrentThreadId(), __func__, (int)readLen);
    
    return true;
}

bool RemoteRenderChannel::readChannel(size_t wantReadLen) {
    if (mSocket < 0) {
        assert(0);
        return false;
    }

    mWantReadSize += wantReadLen;
    //modConnection(false);

    return true;
    
}

bool RemoteRenderChannel::onNetworkSndDataHeadReady(PagePacketHead& head, size_t * pOffset) {
    if (mSocket < 0) {
        assert(0);
        return false;
    }

    ssize_t retHead = android::base::socketSend(mSocket,
                            &head + *pOffset, PAGE_PACKET_HEAD_LEN - *pOffset);

    if (retHead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
            }
        else {
            //android::base::socketClose(mSocket);
            assert(0);
            return false;
        }
    }

    *pOffset += retHead;

    return true;
}

bool RemoteRenderChannel::onNetworkSndDataPageReady(std::shared_ptr<BufferPage> page, size_t * pOffset) {

    if (mSocket < 0) {
        assert(0);
        return false;
    }

    ssize_t ret = android::base::socketSend(mSocket,
                            page->beginPos() + *pOffset, page->writePos() - page->beginPos() - *pOffset);

    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
            }
        else {
            //android::base::socketClose(mSocket);
            assert(0);
            return false;
        }
    }

    *pOffset += ret;

    //printf("0x%lx %s : send %d\n", android::base::getCurrentThreadId(), __func__, (int)ret);

    return true;
}

/*
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
    
    printf("0x%lx %s : send %d + %d\n", android::base::getCurrentThreadId(), __func__, (int)PAGE_PACKET_HEAD_LEN, (int)(page->writePos() - page->beginPos()));
    return true;
}
*/
}
