#include "tcp_channel.h"

#include "android/utils/sockets.h"
#include "android/utils/ipaddr.h"
#include "android/emulation/VmLock.h"
#include "android/base/sockets/SocketUtils.h"

#include "logging.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

namespace emugl {

#define DEBUG 0
#if DEBUG >= 1
#define AutoLog() AutoLogger autoLogger(__func__, this)
#else
#define AutoLog() ((void)0)
#endif

TcpChannel::TcpChannel(const char *hostName, int port) {
    AutoLog();

    strcpy(mSockIP, hostName);
    mSockPort = port;
    mSockFd   = -1;

    const char* dump_dir = getenv("TCPCHANNEL_DUMP_DIR");
    if (dump_dir) {
        size_t bsize = strlen(dump_dir) + 32;
        char* fname = new char[bsize];

        snprintf(fname, bsize, "%s/tcp_channel_%p_snd", dump_dir, this);
        mDumpSndFP= fopen(fname, "wb");
        if (!mDumpSndFP) {
            fprintf(stderr, "Warning: send stream dump failed to open file %s\n",
                    fname);
        }

        snprintf(fname, bsize, "%s/tcp_channel_%p_rcv", dump_dir, this);
        mDumpRcvFP= fopen(fname, "wb");
        if (!mDumpRcvFP) {
            fprintf(stderr, "Warning: receive stream dump failed to open file %s\n",
                    fname);
        }
        delete[] fname;
    }
}

TcpChannel::~TcpChannel() {
    AutoLog();
    stop();

    if (mDumpSndFP != NULL) {
        fclose(mDumpSndFP);
    }

    if (mDumpRcvFP != NULL) {
        fclose(mDumpRcvFP);
    }
}

bool TcpChannel::start() {
    AutoLog();
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
    AutoLog();
    if (mSockFd > 0) {
        //socket_close(mSockFd);
        android::base::socketClose(mSockFd);
        mSockFd = -1;
    }
}

int TcpChannel::sndBufUntil(uint8_t *buf, int wantBufLen) {
    AutoLog();
    if (mSockFd < 0) {
        fprintf(stderr, "%s: invalid socket FD\n", __func__);
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

        if (mDumpSndFP) {
            fwrite(buf + writePos, 1, ret, mDumpSndFP);
            fflush(mDumpSndFP);
        }

        writePos += ret;
        nLeft    -= ret;
    }

    //printf("send  to server %d bytes\n", wantBufLen);
    return wantBufLen;
}

int TcpChannel::rcvBufUntil(uint8_t *buf, int wantBufLen) {
    AutoLog();
    if (mSockFd < 0) {
        fprintf(stderr, "%s: invalid socket FD\n", __func__);
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

        if (mDumpRcvFP) {
            fwrite(buf + totalLen, 1, ret, mDumpRcvFP);
            fflush(mDumpRcvFP);
        }

        totalLen += ret;
    }

    //printf("recv from server %d bytes\n", wantBufLen);
    return totalLen;
}

}
