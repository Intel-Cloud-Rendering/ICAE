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

#pragma once

#include "android/base/Log.h"


#include "android/base/async/Looper.h"
#include "android/base/async/ThreadLooper.h"

//#include "android/emulation/AndroidPipe.h"

//#include "android/utils/gl_cmd_net_format.h"
#include "android/utils/debug.h"

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

class RemoteInputDataConnection {
public:
    
    RemoteInputDataConnection(int socket);
    ~RemoteInputDataConnection() {
        CloseConnection();
    };

    int socketFD() {
        return mFd;
    }

    void CloseConnection();
    
    ssize_t receiveDataFromSocketNonBlocking(char * buf, ssize_t buf_size);
private:
    
    int mSessionId;

    int mFd;

};

}  // namespace emulation
}  // namespace android



