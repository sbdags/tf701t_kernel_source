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

#ifndef MappedTexture_h
#define MappedTexture_h

#include "EGLImage.h"
#include "EGLFence.h"
#include "IntSize.h"
#include "ResourceLimits.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <ui/GraphicBuffer.h>
#include <utils/threads.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

class SkBitmap;

namespace android {

class GraphicBuffer;

};

namespace WebCore {

/**
 * MappedTexture is a class that encapsulates code for allocating a memory region in WebKit
 * thread and using that memory as an OpenGL texture in compositor thread. It is invalid to make
 * calls on the OpenGL texture that would reallocate (like glTexImage2D()) but calls like
 * glTexSubImage2D() are OK.
 * The MappedTexture instance is created, destroyed and manipulated in WebKit thread.
 * Caller should manage the mutual exclusion to the instance.
 */
class MappedTexture {
    WTF_MAKE_NONCOPYABLE(MappedTexture);

public:
    enum Format { HasAlpha, NoAlpha };
    enum WriteMode { WriteUsingSoftware = 1, WriteUsingHardware = 2, WriteUsingSoftwareAndHardware = 3};

    static PassOwnPtr<MappedTexture> create(ResourceLimits::Context resourceContext, const IntSize& size, Format format, WriteMode writeMode)
    {
        bool success = false;
        MappedTexture* obj = new MappedTexture(resourceContext, size, format, writeMode, success);
        if (!success) {
            delete obj;
            return 0;
        }
        return obj;
    }

    ~MappedTexture();

    IntSize size() const { return IntSize(m_graphicBuffer->getWidth(), m_graphicBuffer->getHeight()); }

    bool lockBufferForReadingGL(GLuint* outTextureId, GLint filter = GL_LINEAR, GLint wrap = GL_CLAMP_TO_EDGE)
    {
        return lockBufferGLInternal(outTextureId, false, filter, wrap);
    }

    bool lockBufferForWritingGL(GLuint* outTextureId, GLint filter = GL_LINEAR, GLint wrap = GL_CLAMP_TO_EDGE)
    {
        return lockBufferGLInternal(outTextureId, true, filter, wrap);
    }

    void unlockBufferGL(GLuint textureId);

    bool lockBufferForWriting(SkBitmap* bitmap) { return lockBufferInternal(bitmap, true); }
    bool lockBufferForReading(SkBitmap*, bool premultiplyAlpha);
    void unlockBuffer();

    bool copyTo(MappedTexture* dest);

    GLenum textureTarget() const { return m_eglImage->textureTarget(); }

    void didResetRenderingContext();

protected:
    MappedTexture(ResourceLimits::Context resourceContext, const IntSize&, Format, WriteMode, bool& success);
    OwnPtr<EGLImage> m_eglImage;

private:
    bool lockBufferGLInternal(GLuint* outTextureId, bool isWrite, GLint filter, GLint wrap);
    void unlockBufferGLInternal(GLuint outTextureId, bool isWrite);
    bool lockBufferInternal(SkBitmap*, bool isWrite);

    android::sp<android::GraphicBuffer> m_graphicBuffer;
    bool m_didWriteWithHardware;
    bool m_isLockedForHardwareWrite;
    EGLFence m_hardwareWriteFence;
    ResourceLimits::FileDescriptorGrant m_fileDescriptorGrant;
#if !ASSERT_DISABLED
    void* m_mappedPixels;
#endif
};

} // namespace WebCore

#endif // MappedTexture_h
