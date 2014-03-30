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

#ifndef VideoSurface_h
#define VideoSurface_h

#include "IntRect.h"
#include <GLES2/gl2.h>
#include <gui/ConsumerBase.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class EGLFence;
class EGLImage;
class GLContext;
class MappedTexture;

class VideoSurface : public android::ConsumerBase {
    WTF_MAKE_NONCOPYABLE(VideoSurface);

public:
    VideoSurface();

    class Listener {
    public:
        virtual void onFrameAvailable() {}
        virtual ~Listener() {}
    };
    void addListener(Listener*);
    void removeListener(Listener*);

    GLuint lockTextureForCurrentFrame(float* textureMatrix);
    void unlockTexture(GLuint textureId);

private:
    void onFrameAvailable();

    HashSet<Listener*> m_listeners;
    int m_bufferId;
    OwnPtr<EGLImage> m_eglImage;
    float m_textureMatrix[16];
};

class DrawVideoSurface {
public:
    DrawVideoSurface();

    void prepareCurrentContext();
    void releaseGLResources();

    enum TransformFlag {
        NoTransform = 0,
        FlipVertically = 1 << 0,
        FlipHorizontally = 1 << 1
    };
    typedef unsigned TransformFlags;
    void drawCurrentFrame(const android::sp<VideoSurface>&, TransformFlags = NoTransform);

private:
    GLuint m_program;
    GLint m_positionIndex;
    GLint m_textureMatrixLocation;
    GLuint m_unitSquareBuffer;
};

class CopyVideoSurface {
public:
    static PassOwnPtr<CopyVideoSurface> create(EGLContext sharedContext = EGL_NO_CONTEXT)
    {
        bool success = false;
        CopyVideoSurface* surface = new CopyVideoSurface(sharedContext, success);
        if (!success) {
            delete surface;
            return 0;
        }
        return adoptPtr(surface);
    }
    ~CopyVideoSurface();

    bool copyCurrentFrame(MappedTexture* destTexture, const android::sp<VideoSurface>& videoSurface,
                          IntRect destRect, EGLFence* = 0);
    bool copyCurrentFrame(GLenum destTextureTarget, GLuint destTextureId,
                          const android::sp<VideoSurface>& videoSurface,
                          IntRect destRect, EGLFence* = 0);

private:
    CopyVideoSurface(EGLContext sharedContext, bool& success);

    bool copyCurrentFrameInternal(GLenum destTextureTarget, GLuint destTextureId,
                                  const android::sp<VideoSurface>& videoSurface,
                                  IntRect destRect, EGLFence*);

    OwnPtr<GLContext> m_context;
    GLuint m_fbo;
    DrawVideoSurface m_drawVideoSurface;
};

} // namespace WebCore

#endif // VideoSurface_h
