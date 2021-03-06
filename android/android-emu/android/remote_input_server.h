/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "android/utils/compiler.h"
#include "android/utils/looper.h"
#include "android/console.h"


ANDROID_BEGIN_HEADER

int android_remote_input_server_init(const AndroidConsoleAgents* agents);
void android_remote_input_server_undo_init(void);
void android_remote_input_service_init(void);

ANDROID_END_HEADER

