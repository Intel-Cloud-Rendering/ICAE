#include "tcp_channel.h"

#include "android/utils/sockets.h"
#include "android/utils/ipaddr.h"

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
        mSockFd = socket_network_client(mSockIP, mSockPort, SOCKET_STREAM);
        if (mSockFd == -1) {
            fprintf(stderr, "%s: cannot connect to rendering server.(%s)\n", __func__, errno_str);
            return false;
        }
    }

    return true;
}

void TcpChannel::stop() {
    if (mSockFd > 0) {
        socket_close(mSockFd);
        mSockFd = -1;
    }
}

int TcpChannel::sndBufUntil(uint8_t *buf, int wantBufLen) {
    int writePos = 0;
    int nLeft = wantBufLen;
    while (nLeft > 0) {
        int ret = socket_send(mSockFd, buf + writePos, nLeft);
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

    return 0;
}

int TcpChannel::rcvBufUntil(uint8_t *buf, int wantBufLen) {
    int totalLen = 0;
    while ((totalLen < wantBufLen) && (!mStop)) {
        int ret = socket_recv(mSockFd, (buf + totalLen), wantBufLen - totalLen);
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
    return totalLen;
}

}
