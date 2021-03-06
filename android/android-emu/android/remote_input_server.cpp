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

#include "android/remote_input_server.h"

#include "android/base/files/StdioStream.h"
#include "android/base/files/Stream.h"
#include "android/base/memory/LazyInstance.h"
#include "android/base/system/System.h"


#include "android/remoteinput/RemoteInputListener.h"
#include "android/utils/debug.h"

#include <memory>
#include <string>

#include <stdio.h>
#include <stdlib.h>

namespace {

using android::remoteinput::RemoteInputListener;

// Global variables used here.
struct Globals {
    Globals() : hostListener() {}

    void registerServices() {

    }

    RemoteInputListener hostListener;
};

android::base::LazyInstance<Globals> sGlobals = LAZY_INSTANCE_INIT;

}  // namespace

int android_remote_input_server_init(const AndroidConsoleAgents* agents) {
    auto globals = sGlobals.ptr();

    // Add Tcp Channel for comunication
    const char* remote_input_server_hostname = getenv("remote_input_server_hostname");
    if (!remote_input_server_hostname) {
        printf("Cannot find remote input server hostname\n");
        remote_input_server_hostname = "127.0.0.1";
    }
    const char* remote_input_server_port = getenv("remote_input_server_port");
    if (!remote_input_server_port) {
        printf("Cannot find remote input port\n");
        remote_input_server_port = "30040";
    }
    
    if (!globals->hostListener.reset(atoi(remote_input_server_port), agents)) {
        return -1;
    }

    globals->hostListener.startListening();
    return 0;
}

void android_remote_input_server_undo_init(void) {
    sGlobals->hostListener.reset(-1, NULL);
}

void android_remote_input_service_init(void) {
    // Register adb pipe service.
    //sGlobals->registerServices();
}

