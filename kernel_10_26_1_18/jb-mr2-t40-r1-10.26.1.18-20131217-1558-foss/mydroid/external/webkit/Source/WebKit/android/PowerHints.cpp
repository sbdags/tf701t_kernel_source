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
#include "PowerHints.h"

#define LOG_TAG "PowerHints"
#include <cutils/log.h>
#include <dlfcn.h>

namespace android {

PowerHints::EnableHighFPSScaling::EnableHighFPSScaling()
    : m_highFPSRequestCount(0)
    , m_nvOsSetFpsTarget(0)
    , m_nvOsCancelFpsTarget(0)
    , m_fpsTargetFD(-1)
{
    m_libNvOs = dlopen("libnvos.so", RTLD_LAZY);
    if (!m_libNvOs) {
        ALOGE("Failed to load libnvos.so: %s\n", dlerror());
        return;
    }

    m_nvOsSetFpsTarget = reinterpret_cast<NvOsSetFpsTarget>(dlsym(m_libNvOs, "NvOsSetFpsTarget"));
    m_nvOsCancelFpsTarget = reinterpret_cast<NvOsCancelFpsTarget>(dlsym(m_libNvOs, "NvOsCancelFpsTarget"));

    if (!m_nvOsSetFpsTarget || !m_nvOsCancelFpsTarget)
        ALOGE("Failed to load NvOsSetFpsTarget/NvOsCancelFpsTarget: %s\n", dlerror());
}

PowerHints::EnableHighFPSScaling::~EnableHighFPSScaling()
{
    if (m_libNvOs)
        dlclose(m_libNvOs);
}

void PowerHints::EnableHighFPSScaling::requestHighFPSScaling()
{
    if (!m_nvOsSetFpsTarget || !m_nvOsCancelFpsTarget)
        return;

    if (m_highFPSRequestCount++)
        return;

    ASSERT(m_fpsTargetFD < 0);
    m_fpsTargetFD = m_nvOsSetFpsTarget(0);
}

void PowerHints::EnableHighFPSScaling::cancelHighFPSScaling()
{
    if (!m_nvOsSetFpsTarget || !m_nvOsCancelFpsTarget)
        return;

    ASSERT(m_highFPSRequestCount);
    if (--m_highFPSRequestCount)
        return;

    if (m_fpsTargetFD < 0)
        return;

    m_nvOsCancelFpsTarget(m_fpsTargetFD);
    m_fpsTargetFD = -1;
}

PassRefPtr<PowerHints::EnableHighFPSScaling> PowerHints::requestHighFPSScaling()
{
    AtomicallyInitializedStatic(EnableHighFPSScaling*, enableHighFPSScaling = new EnableHighFPSScaling());
    return enableHighFPSScaling;
}

}
