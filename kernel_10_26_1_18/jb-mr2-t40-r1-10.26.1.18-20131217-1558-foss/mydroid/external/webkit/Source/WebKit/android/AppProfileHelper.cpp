/*
 * Copyright 2006, The Android Open Source Project
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AppProfileHelper.h"

#define LOG_TAG "AppProfileHelper"
#include <cutils/log.h>
#include <dlfcn.h>

namespace android {

AppProfileHelper::AppProfileHelper()
    : m_libNvCpl(0)
    , m_nvCplGetAppProfileSettingInt(0)
{
    m_libNvCpl = dlopen("libnvcpl.so", RTLD_LAZY);
    if (!m_libNvCpl) {
        ALOGE("Failed to load libnvcpl.so: %s\n", dlerror());
        return;
    }

    m_nvCplGetAppProfileSettingInt = reinterpret_cast<NvCplGetAppProfileSettingInt>(dlsym(m_libNvCpl, "NvCplGetAppProfileSettingInt"));

    if (!m_nvCplGetAppProfileSettingInt)
        ALOGE("Failed to load NvCplGetAppProfileSettingInt: %s\n", dlerror());
}

AppProfileHelper::~AppProfileHelper()
{
    if (m_libNvCpl)
        dlclose(m_libNvCpl);
}

int AppProfileHelper::getAppProfileSettingInt(const char* exeName, const char* setting, int defaultValue)
{
    int value = 0;

    if (m_nvCplGetAppProfileSettingInt) {
        if (!m_nvCplGetAppProfileSettingInt(exeName, setting, &value) && value != -1)
            return value;
    }

    return defaultValue;
}

}

