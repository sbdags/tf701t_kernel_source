/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
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
#include "AcceleratedCanvas.h"

#if USE(ACCELERATED_CANVAS_LAYER)

#include "EGLImageLayer.h"

namespace WebCore {

LayerAndroid* AcceleratedCanvas::createPlatformLayer()
{
    return new EGLImageLayer(this, "canvas");
}

AcceleratedCanvas::BorrowBackBuffer::BorrowBackBuffer()
    : m_borrowedBackBuffer(0)
{}

EGLImageBuffer* AcceleratedCanvas::BorrowBackBuffer::borrowBackBuffer()
{
    MutexLocker lock(m_mutex);
    while (!m_borrowedBackBuffer)
        m_condition.wait(m_mutex);
    return m_borrowedBackBuffer;
}

void AcceleratedCanvas::BorrowBackBuffer::lendBackBuffer(EGLImageBuffer* buffer)
{
    MutexLocker lock(m_mutex);
    m_borrowedBackBuffer = buffer;
    m_condition.signal();
}

void AcceleratedCanvas::BorrowBackBuffer::reclaimBackBuffer()
{
    MutexLocker lock(m_mutex);
    while (m_borrowedBackBuffer)
        m_condition.wait(m_mutex);
    m_returnFence.finish();
}

void AcceleratedCanvas::BorrowBackBuffer::returnBackBuffer(bool needsEGLFence)
{
    MutexLocker lock(m_mutex);
    m_borrowedBackBuffer = 0;
    if (needsEGLFence)
        m_returnFence.set();
    m_condition.signal();
}

} // namespace WebCore

#endif // USE(ACCELERATED_CANVAS_LAYER)
