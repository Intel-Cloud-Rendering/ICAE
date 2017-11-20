#pragma once

#include "android/utils/compiler.h"

#include <stdint.h>
#include <stdio.h>

ANDROID_BEGIN_HEADER

namespace emugl {

class TcpChannel {
public:
    TcpChannel(const char *hostName, int port);
    TcpChannel(int socket, int session) : mSockFd(socket), mSession(session) {};
    ~TcpChannel();

    bool start();
    void stop();

    int sndBufUntil(uint8_t *buf, int wantBufLen);
    int rcvBufUntil(uint8_t *buf, int wantBufLen);

private:
    int sockSndBufUntil(uint8_t *buf, int wantBufLen);

private:
    int              mSockFd;
    char             mSockIP[512] = {0};
    int              mSockPort;
    bool             mStop;
    FILE            *mDumpSndFP = NULL;
    FILE            *mDumpRcvFP = NULL;
    int              mSession;
};

}

ANDROID_END_HEADER

