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

#ifndef AppProfileHelper_h
#define AppProfileHelper_h

#include <wtf/Noncopyable.h>

namespace android {

class AppProfileHelper {
    WTF_MAKE_NONCOPYABLE(AppProfileHelper);

    typedef int (*NvCplGetAppProfileSettingInt)(const char* exeName, const char* setting, int* value);

public:
    static AppProfileHelper* instance()
    {
        static AppProfileHelper* instance = 0;
        if (!instance)
            instance = new AppProfileHelper();
        return instance;
    }

    int getAppProfileSettingInt(const char* exeName, const char* setting, int defaultValue);

private:
    AppProfileHelper();
    ~AppProfileHelper();

    void* m_libNvCpl;
    NvCplGetAppProfileSettingInt m_nvCplGetAppProfileSettingInt;
};

}

#endif
