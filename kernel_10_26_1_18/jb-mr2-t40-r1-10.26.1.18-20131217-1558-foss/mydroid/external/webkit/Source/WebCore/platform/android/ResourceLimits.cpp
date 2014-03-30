/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include "ResourceLimits.h"

#include "MemoryUsage.h"
#include "PlatformBridge.h"
#include <sys/resource.h>
#include <sys/time.h>
#include <wtf/Threading.h>

static unsigned tegraDriverMemoryLimit = 400 * 1024 * 1024;

static size_t acquiredFileDescriptors;
static Mutex& fileDescriptorMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

namespace WebCore {

namespace ResourceLimits {

bool canSatisfyMemoryAllocation(unsigned bytes)
{
    if (!bytes)
        return true;

    return PlatformBridge::canSatisfyMemoryAllocation(bytes);
}

bool canSatisfyGraphicsMemoryAllocation(unsigned bytes)
{
    return MemoryUsage::graphicsMemoryUsage() + bytes <= tegraDriverMemoryLimit;
}

FileDescriptorGrant::FileDescriptorGrant(Context context, size_t requestedAmount)
    : m_grantedAmount(0)
#if !ASSERT_DISABLED
    , m_grantCheckDone(false)
#endif
{
    ASSERT(requestedAmount > 0);

    struct rlimit fdlimit = { 0, 0 };

    if (getrlimit(RLIMIT_NOFILE, &fdlimit))
        return;

    MutexLocker lock(fileDescriptorMutex());

    size_t reserve = 0;

    switch (context) {
    case WebContent:
        // TODO: unfortunately we cannot know how many descriptors are used.
        // Thus we make a guess that we must reserve at least half of the limit.
        // One is reserved for fallback option when deserializing SkPictures
        // containing ashmem ImageRefs (see BitmapAllocatorAndroid.cpp).
        reserve = fdlimit.rlim_cur / 2;
        break;
    default:
        break;
    }

    if (fdlimit.rlim_cur >= (acquiredFileDescriptors + reserve + requestedAmount)) {
        acquiredFileDescriptors += requestedAmount;
        m_grantedAmount = requestedAmount;
    }
}

FileDescriptorGrant::~FileDescriptorGrant()
{
    ASSERT(m_grantCheckDone);
    if (!m_grantedAmount)
        return;

    MutexLocker lock(fileDescriptorMutex());
    ASSERT(acquiredFileDescriptors >= m_grantedAmount);
    acquiredFileDescriptors -= m_grantedAmount;
}

} // namespace ResourceLimits

} // namespace WebCore
