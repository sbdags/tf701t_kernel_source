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

#ifndef EGLImageBuffer_h
#define EGLImageBuffer_h

#if USE(ACCELERATED_COMPOSITING)

#include "EGLFence.h"
#include "IntSize.h"
#include "MappedTexture.h"
#include <GLES2/gl2.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>

class SkBitmap;

namespace WebCore {

class EGLImageBuffer {
public:
    virtual ~EGLImageBuffer() { }

    void lockSurface() { m_mutex.lock(); }
    void unlockSurface() { m_mutex.unlock(); }

    // The fence methods need to be called with the surface already locked.
    void setFence() { m_fence.set(); }
    void finish();

    bool lockBufferForReadingGL(GLuint* outTextureId, GLint filter = GL_LINEAR, GLint wrap = GL_CLAMP_TO_EDGE);
    void unlockBufferGL(GLuint textureId);

    virtual bool lockBufferForReading(SkBitmap*, bool premultiplyAlpha) = 0;
    virtual void unlockBuffer() = 0;
    virtual void deleteBufferSource() = 0;
    virtual bool isIntact() { return true; }

protected:
    virtual EGLImage* eglImage() = 0;

private:
    WTF::Mutex m_mutex;
    EGLFence m_fence;
};

// FIXME: merge this with the superclass this once EGLImageBuffer can be backed by a graphics
// buffer and EGLImageBuffer can inherit from MappedTexture.
class EGLImageBufferFromTexture : public EGLImageBuffer {
public:
    static PassOwnPtr<EGLImageBufferFromTexture> create(const IntSize& size, bool hasAlpha)
    {
        bool success = false;
        EGLImageBufferFromTexture* obj = new EGLImageBufferFromTexture(size, hasAlpha, success);
        if (!success) {
            delete obj;
            return 0;
        }
        return obj;
    }

    ~EGLImageBufferFromTexture();

    IntSize size() const { return m_size; }

    GLuint sourceContextTextureId() { return m_textureId; }

    void onSourceContextReset();

    // EGLImageBuffer overrides.
    virtual bool lockBufferForReading(SkBitmap*, bool premultiplyAlpha);
    virtual void unlockBuffer();
    virtual void deleteBufferSource();
    virtual bool isIntact() { return m_textureId; }

protected:
    virtual EGLImage* eglImage() { return m_eglImage.get(); }

private:
    EGLImageBufferFromTexture(const IntSize&, bool hasAlpha, bool& success);
    OwnPtr<EGLImage> m_eglImage;
    IntSize m_size;
    GLuint m_textureId;
    GLuint m_colorFormat;
#if !ASSERT_DISABLED
    EGLContext m_creationContext;
#endif
};

} // namespace WebCore

#endif

#endif
