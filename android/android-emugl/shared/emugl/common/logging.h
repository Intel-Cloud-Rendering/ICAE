/*
* Copyright (C) 2016 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "OpenglRender/render_api_types.h"

#include "android/utils/system.h"

#include <assert.h>

extern emugl_logger_t emugl_logger;
extern emugl_logger_t emugl_cxt_logger;
void set_emugl_logger(emugl_logger_t f);
void set_emugl_cxt_logger(emugl_logger_t f);

#define GL_LOGGING 1

#if GL_LOGGING

#define GL_LOG(...) do { \
    emugl_logger(__VA_ARGS__); \
} while (0)

#define GL_CXT_LOG(...) do { \
    emugl_cxt_logger(__VA_ARGS__); \
} while (0)

#else
#define GL_LOG(...) 0
#endif

class AutoLogger {
public:
    AutoLogger(const char *name, void *id) {
        mTid = android_get_thread_id();
        mId = id;
        assert(strlen(name) < sizeof(mFuncName));
        strcpy(mFuncName, name);
        printf("[DEBUG][%p][0x%" ANDROID_THREADID_FMT "][%s <<<<<]\n", mId, mTid, mFuncName);
    };
    ~AutoLogger() {
        printf("[DEBUG][%p][0x%" ANDROID_THREADID_FMT "][%s >>>>>]\n", mId, mTid, mFuncName);
    };
private:
    char mFuncName[512] = {0};
    void *mId;
    android_thread_id_t mTid;
};

