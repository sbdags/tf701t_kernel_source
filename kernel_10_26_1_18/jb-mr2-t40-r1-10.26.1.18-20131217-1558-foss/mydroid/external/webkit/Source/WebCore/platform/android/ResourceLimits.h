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

#ifndef ResourceLimits_h
#define ResourceLimits_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

namespace ResourceLimits {

enum Context { NoContext = 1, WebContent, System };

bool canSatisfyMemoryAllocation(unsigned bytes);
bool canSatisfyGraphicsMemoryAllocation(unsigned bytes);

class FileDescriptorGrant {
    WTF_MAKE_NONCOPYABLE(FileDescriptorGrant);

public:
    FileDescriptorGrant(Context, size_t amount);
    explicit FileDescriptorGrant(FileDescriptorGrant& adoptedGrant)
    {
#if !ASSERT_DISABLED
        m_grantCheckDone = adoptedGrant.m_grantCheckDone;
        adoptedGrant.m_grantCheckDone = true;
#endif

        m_grantedAmount = adoptedGrant.m_grantedAmount;
        adoptedGrant.m_grantedAmount = 0;
    }

    ~FileDescriptorGrant();

    bool isGranted() const
    {
#if !ASSERT_DISABLED
        m_grantCheckDone = true;
#endif
        return m_grantedAmount;
    }

private:
    size_t m_grantedAmount;
#if !ASSERT_DISABLED
    mutable bool m_grantCheckDone;
#endif
};

} // namespace ResourceLimits

} // namespace WebCore

#endif