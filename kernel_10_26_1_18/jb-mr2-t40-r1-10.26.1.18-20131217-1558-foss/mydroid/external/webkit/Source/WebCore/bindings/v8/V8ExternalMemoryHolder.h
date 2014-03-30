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


#ifndef V8ExternalMemoryHolder_h
#define V8ExternalMemoryHolder_h

#include "V8Binding.h"

namespace WebCore {

template<typename T>
class V8ExternalMemoryHolder {
public:
    V8ExternalMemoryHolder()
        : m_externalMemoryRefCount(0)
        , m_externalMemorySize(0)
    {
    }

    ~V8ExternalMemoryHolder()
    {
        derefExternalMemoryJSBeforeUpdate();
        if (m_externalMemorySize)
            v8::V8::AdjustAmountOfExternalAllocatedMemory(-m_externalMemorySize);
    }

    void refExternalMemoryJS()
    {
        if (++m_externalMemoryRefCount == 1)
            static_cast<T*>(this)->firstRefExternalMemoryJS();
    }

    void derefExternalMemoryJS()
    {
        if (!--m_externalMemoryRefCount)
            static_cast<T*>(this)->lastDerefExternalMemoryJS();
    }

    void refExternalMemoryJSAfterUpdate()
    {
        if (m_externalMemoryRefCount)
            static_cast<T*>(this)->firstRefExternalMemoryJS();
    }

    void derefExternalMemoryJSBeforeUpdate()
    {
        if (m_externalMemoryRefCount)
            static_cast<T*>(this)->lastDerefExternalMemoryJS();
    }

    void addExternalAllocatedMemory(int size)
    {
        m_externalMemorySize += size;
        v8::V8::AdjustAmountOfExternalAllocatedMemory(size);
    }

    void removeExternalAllocatedMemory()
    {
        v8::V8::AdjustAmountOfExternalAllocatedMemory(-m_externalMemorySize);
        m_externalMemorySize = 0;
    }

private:
    unsigned m_externalMemoryRefCount;
    int m_externalMemorySize;
};

} // namespace WebCore

#endif
