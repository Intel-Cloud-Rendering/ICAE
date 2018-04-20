// Copyright 2016 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "android/base/Log.h"

#include "android/base/sockets/SocketUtils.h"

#include "android/emulation/VmLock.h"


#include "android/utils/debug.h"
#include "RemoteInputDataConnection.h"

#include <atomic>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <memory>

namespace android {
namespace remoteinput {

RemoteInputDataConnection::RemoteInputDataConnection(int socket) :
        mFd(socket) {
    
    printf("%s: create\n", __func__);

    android::base::socketSetNonBlocking(socket);
    // socketSetNoDelay() reduces the latency of sending data, at the cost
    // of creating more TCP packets on the connection. It's useful when
    // doing lots of small send() calls, like the ADB protocol requires.
    // And since this is on localhost, the packet increase should not be
    // noticeable.
    android::base::socketSetNoDelay(socket);
};

ssize_t RemoteInputDataConnection::receiveDataFromSocketNonBlocking(char * buf, ssize_t buf_size) {
    ssize_t readLen;
    {
        ScopedVmUnlock unlockBql;
        readLen = android::base::socketRecv(
            mFd,
            buf,
            buf_size);
    }

    if (readLen < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        } else {
            //mSocket.reset();
            assert(0);
            return -1;
        }
    }

    return readLen;
}

void RemoteInputDataConnection::CloseConnection() {
    printf("close connection now!\n");
    android::base::socketClose(mFd);
    mFd = -1;
}


}
}




