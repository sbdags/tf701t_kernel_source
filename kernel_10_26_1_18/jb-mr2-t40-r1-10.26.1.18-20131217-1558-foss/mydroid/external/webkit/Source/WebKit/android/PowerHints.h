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

#ifndef PowerHints_h
#define PowerHints_h

#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace android {

class PowerHints {
public:

    class EnableHighFPSScaling {
        WTF_MAKE_NONCOPYABLE(EnableHighFPSScaling);
        typedef int (*NvOsSetFpsTarget)(int target);
        typedef void (*NvOsCancelFpsTarget)(int fd);
        friend class PowerHints;

    public:
        void requestHighFPSScaling();
        void cancelHighFPSScaling();

    private:
        EnableHighFPSScaling();
        ~EnableHighFPSScaling();

        size_t m_highFPSRequestCount;
        void* m_libNvOs;
        NvOsSetFpsTarget m_nvOsSetFpsTarget;
        NvOsCancelFpsTarget m_nvOsCancelFpsTarget;
        int m_fpsTargetFD;
    };

    static PassRefPtr<EnableHighFPSScaling> requestHighFPSScaling();
};

}

namespace WTF {

template<> inline void refIfNotNull(android::PowerHints::EnableHighFPSScaling* enableHighFPSScaling)
{
    if (enableHighFPSScaling)
        enableHighFPSScaling->requestHighFPSScaling();
}

template<> inline void derefIfNotNull(android::PowerHints::EnableHighFPSScaling* enableHighFPSScaling)
{
    if (enableHighFPSScaling)
        enableHighFPSScaling->cancelHighFPSScaling();
}

}

#endif
