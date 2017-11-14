#include "tcp_channel.h"

#include "android/utils/sockets.h"
#include "android/utils/ipaddr.h"
#include "android/emulation/VmLock.h"
#include "android/base/sockets/SocketUtils.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

namespace emugl {

TcpChannel::TcpChannel(const char *hostName, int port) {
    strcpy(mSockIP, hostName);
    mSockPort = port;
    mSockFd   = -1;
}

TcpChannel::~TcpChannel() {
    stop();
}

bool TcpChannel::start() {
    if (mSockFd == -1) {
        //mSockFd = socket_network_client(mSockIP, mSockPort, SOCKET_STREAM);
        mSockFd = android::base::socketTcp4LoopbackClient(mSockPort);
        if (mSockFd == -1) {
            fprintf(stderr, "%s: cannot connect to rendering server.(%s)\n", __func__, errno_str);
            return false;
        }
    }

    return true;
}

void TcpChannel::stop() {
    if (mSockFd > 0) {
        //socket_close(mSockFd);
        android::base::socketClose(mSockFd);
        mSockFd = -1;
    }
}

int TcpChannel::sndBufUntil(uint8_t *buf, int wantBufLen) {
    if (mSockFd < 0) {
        return -1;
    }

    int writePos = 0;
    int nLeft = wantBufLen;
    while (nLeft > 0) {
        int ret;
        {
            //android::ScopedVmUnlock unlockBql;
            //ret = socket_send(mSockFd, buf + writePos, nLeft);
            ret = android::base::socketSend(mSockFd, buf + writePos, nLeft);
        }

        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                fprintf(stderr, "%s: cannot send %d bytes data to renderint server.(%s)\n", __func__, wantBufLen, errno_str);
                return -1;
            }
        }

        writePos += ret;
        nLeft    -= ret;
    }

    //printf("send  to server %d bytes\n", wantBufLen);
    return wantBufLen;
}

int TcpChannel::rcvBufUntil(uint8_t *buf, int wantBufLen) {
    if (mSockFd < 0) {
        return -1;
    }
    
    int totalLen = 0;
    while ((totalLen < wantBufLen) && (!mStop)) {
        int ret;
        {
            //android::ScopedVmUnlock unlockBql;
            //ret = socket_recv(mSockFd, (buf + totalLen), wantBufLen - totalLen);
            ret = android::base::socketRecv(mSockFd, (buf + totalLen), wantBufLen - totalLen);
        }

        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                fprintf(stderr, "%s: cannot receive %d bytes data from renderint server.(%s)\n", __func__, wantBufLen, errno_str);
                return -1;
            }
        }
        totalLen += ret;
    }

    //printf("recv from server %d bytes\n", wantBufLen);
    return totalLen;
}

}
