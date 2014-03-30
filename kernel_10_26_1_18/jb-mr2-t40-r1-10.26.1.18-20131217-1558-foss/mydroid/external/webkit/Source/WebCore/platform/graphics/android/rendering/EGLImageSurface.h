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

#ifndef EGLImageSurface_h
#define EGLImageSurface_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntSize.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

class SkBitmap;

namespace WebCore {

class EGLImageBuffer;
class EGLImageBufferRing;
class FPSTimer;

class EGLImageSurface : public RefCounted<EGLImageSurface> {
public:
    EGLImageSurface(IntSize);
    virtual ~EGLImageSurface();

    IntSize size() const { return m_size; }
    EGLImageBufferRing* bufferRing() const { return m_bufferRing.get(); }

    virtual bool isInverted() const { return false; }
    virtual bool hasAlpha() const { return true; }
    virtual bool hasPremultipliedAlpha() const { return true; }

    virtual void swapBuffers() = 0;

    virtual bool supportsQuadBuffering() const { return false; }
    virtual void submitBackBuffer() {}

    virtual void deleteFreeBuffers() {}
    virtual void updateBackgroundStatus(bool inBackground) {}
    virtual void didDetachFromView() {}

    static bool isQuadBufferingDisabled();

protected:
    void updateSize(IntSize);

private:
    IntSize m_size;
    RefPtr<EGLImageBufferRing> m_bufferRing;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // EGLImageSurface_h
