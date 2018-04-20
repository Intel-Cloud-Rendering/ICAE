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

#include "android/remoteinput/RemoteInputListener.h"

//#include "android/base/async/AsyncSocketServer.h"
#include "android/base/async/ThreadLooper.h"
#include "android/base/Log.h"
#include "android/base/sockets/ScopedSocket.h"
#include "android/base/sockets/SocketUtils.h"

//#include "android/emulation/AdbTypes.h"

#include <memory>
#include <vector>

namespace android {
namespace remoteinput {
    using Lock = android::base::Lock;
    using AutoLock = android::base::AutoLock;

static bool systemSupportsIPv4() {
    int socket = base::socketCreateTcp4();
    if (socket < 0 && errno == EAFNOSUPPORT)
        return false;
    base::socketClose(socket);
    return true;
}

using android::base::AsyncSocketServer;
using android::base::ScopedSocket;

bool RemoteInputListener::reset(int port, const AndroidConsoleAgents* agents) {
    if (port < 0) {
        mServer.reset();
    } else if (!mServer || port != mServer->port()) {
        CHECK(port < 65536);
        AsyncSocketServer::LoopbackMode mode =
            AsyncSocketServer::kIPv4AndOptionalIPv6;
        if (!systemSupportsIPv4()) {
            mode = AsyncSocketServer::kIPv6;
        }
        mServer = AsyncSocketServer::createTcpAnyServer(
                port,
                [this](int socket) { return onHostServerConnection(socket); },
                mode,
                android::base::ThreadLooper::get());
        if (!mServer) {
            // This can happen when the emulator is probing for a free port
            // at startup, so don't print a warning here.
            return false;
        }
        // Don't start listening until startListening() is called.
    }

    mDataHandler.setUserEventAgent(agents);

    printf("remote input server listener is established at %d\n", port);

    return true;
}

void RemoteInputListener::startListening() {
    if (mServer) {
        mServer->startListening();
    }
}

void RemoteInputListener::stopListening() {
    if (mServer) {
        mServer->stopListening();
    }
}

bool RemoteInputListener::onHostServerConnection(int socket) {
    printf("remote input server found a guest connection\n");

    if (!mDataHandler.addConnection(socket)) {
        printf("remote input server reject the connection because only one allowed\n");
        android::base::socketClose(socket);
    }
    
    //start listening again
    mServer->startListening();

    return true;
}

}  // namespace emulation
}  // namespace android

