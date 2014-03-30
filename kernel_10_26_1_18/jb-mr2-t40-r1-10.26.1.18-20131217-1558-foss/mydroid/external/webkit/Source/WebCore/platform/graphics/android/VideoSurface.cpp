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
#include "VideoSurface.h"

#include "EGLFence.h"
#include "GLContext.h"
#include "GLUtils.h"
#include "MappedTexture.h"
#include <gui/BufferQueue.h>
#include <ui/GraphicBuffer.h>

static const char videoVertexShader[] =
    "attribute vec2 position;\n"
    "uniform mat4 textureMatrix;\n"
    "varying vec2 texCoord;\n"
    "void main() {\n"
    "  gl_Position = vec4(2.0 * position - 1.0, 0, 1);\n"
    "  texCoord = vec2(textureMatrix * vec4(position, 0, 1));\n"
    "}\n";

static const char videoFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES texture;\n"
    "varying vec2 texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(texture, texCoord);\n"
    "}\n";

namespace WebCore {

VideoSurface::VideoSurface()
    : ConsumerBase(new android::BufferQueue())
    , m_bufferId(-1)
{
    mBufferQueue->setConsumerUsageBits(android::GraphicBuffer::USAGE_HW_TEXTURE);
    mBufferQueue->setConsumerName(android::String8("VideoSurface"));
}

void VideoSurface::addListener(Listener* listener)
{
    android::Mutex::Autolock lock(mMutex);
    m_listeners.add(listener);
}

void VideoSurface::removeListener(Listener* listener)
{
    android::Mutex::Autolock lock(mMutex);
    m_listeners.remove(listener);
}

GLuint VideoSurface::lockTextureForCurrentFrame(float* textureMatrix)
{
    mMutex.lock();

    if (!m_eglImage) {
        mMutex.unlock();
        return 0;
    }

    GLuint textureId = m_eglImage->createTexture();
    if (!textureId) {
        mMutex.unlock();
        return 0;
    }

    memcpy(textureMatrix, m_textureMatrix, sizeof(m_textureMatrix));

    return textureId;
}

void VideoSurface::unlockTexture(GLuint textureId)
{
    glDeleteTextures(1, &textureId);
    mMutex.unlock();
}

void VideoSurface::onFrameAvailable()
{
    {
        android::Mutex::Autolock lock(mMutex);

        if (m_bufferId >= 0) {
            ConsumerBase::releaseBufferLocked(m_bufferId, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);
            m_bufferId = -1;
        }
        m_eglImage.clear();

        android::BufferQueue::BufferItem item;
        if (ConsumerBase::acquireBufferLocked(&item) != android::NO_ERROR)
            return;

        ASSERT(item.mBuf >= 0);

        android::sp<android::GraphicBuffer> graphicBuffer = mSlots[item.mBuf].mGraphicBuffer;
        if (!graphicBuffer.get()) {
            ConsumerBase::releaseBufferLocked(item.mBuf, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);
            return;
        }

        EGLDisplay defaultDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        static const EGLint attrs[] = {
            EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
            EGL_NONE
        };
        EGLClientBuffer clientBuffer =
            reinterpret_cast<EGLClientBuffer>(graphicBuffer->getNativeBuffer());
        EGLImageKHR eglImage = eglCreateImageKHR(defaultDisplay, EGL_NO_CONTEXT,
                                                 EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attrs);
        GLUtils::checkEglError("eglCreateImageKHR", eglImage != EGL_NO_IMAGE_KHR);
        if (eglImage == EGL_NO_IMAGE_KHR) {
            ConsumerBase::releaseBufferLocked(item.mBuf, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);
            return;
        }

        m_bufferId = item.mBuf;
        m_eglImage = EGLImage::adopt(eglImage, defaultDisplay, GL_TEXTURE_EXTERNAL_OES);
        computeTextureMatrix(m_textureMatrix, graphicBuffer, item.mTransform, item.mCrop, true);

        HashSet<Listener*>::iterator iter = m_listeners.begin();
        for (; iter != m_listeners.end(); ++iter)
            (*iter)->onFrameAvailable();
    }

    ConsumerBase::onFrameAvailable();
}

DrawVideoSurface::DrawVideoSurface()
    : m_program(0)
    , m_positionIndex(0)
    , m_textureMatrixLocation(0)
{}

void DrawVideoSurface::prepareCurrentContext()
{
    glUseProgram(m_program = WebCore::GLUtils::createProgram(videoVertexShader, videoFragmentShader));
    glEnableVertexAttribArray(m_positionIndex = glGetAttribLocation(m_program, "position"));
    m_textureMatrixLocation = glGetUniformLocation(m_program, "textureMatrix");
    glUniform1i(glGetUniformLocation(m_program, "texture"), 0);

    float unitSquare[] = {0,0, 1,0, 1,1, 0,1};
    glGenBuffers(1, &m_unitSquareBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_unitSquareBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unitSquare), unitSquare, GL_STATIC_DRAW);
    glVertexAttribPointer(m_positionIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);
}

void DrawVideoSurface::releaseGLResources()
{
    glDeleteProgram(m_program);
    glDeleteBuffers(1, &m_unitSquareBuffer);
}

void DrawVideoSurface::drawCurrentFrame(const android::sp<VideoSurface>& videoSurface, TransformFlags transformFlags)
{
    float textureMatrix[16];
    GLuint textureId = videoSurface->lockTextureForCurrentFrame(textureMatrix);
    if (!textureId)
        return;

    if (transformFlags & FlipHorizontally) {
        for (int i = 0; i < 16; i += 4)
            textureMatrix[i] = -textureMatrix[i] + textureMatrix[i + 3];
    }
    if (transformFlags & FlipVertically) {
        for (int i = 1; i < 16; i += 4)
            textureMatrix[i] = -textureMatrix[i] + textureMatrix[i + 2];
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glUniformMatrix4fv(m_textureMatrixLocation, 1, GL_FALSE, textureMatrix);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    videoSurface->unlockTexture(textureId);
}

CopyVideoSurface::CopyVideoSurface(EGLContext sharedContext, bool& success)
{
    success = false;

    AutoRestoreGLContext restoreContext;
    m_context = GLContext::create(ResourceLimits::WebContent, 0, sharedContext);
    if (!m_context)
        return;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_drawVideoSurface.prepareCurrentContext();
    success = true;
}

CopyVideoSurface::~CopyVideoSurface()
{
    if (!m_context)
        return;

    AutoRestoreGLContext restoreContext(m_context);
    glDeleteFramebuffers(1, &m_fbo);
    m_drawVideoSurface.releaseGLResources();
}

bool CopyVideoSurface::copyCurrentFrame(MappedTexture* destTexture, const android::sp<VideoSurface>& videoSurface,
                                        IntRect destRect, EGLFence* fence)
{
    if (!destRect.width() || !destRect.height())
        return true;

    AutoRestoreGLContext restoreContext(m_context);

    GLuint destTextureId = 0;
    if (!destTexture->lockBufferForWritingGL(&destTextureId))
        return false;

    bool success = copyCurrentFrameInternal(destTexture->textureTarget(), destTextureId, videoSurface, destRect, fence);
    destTexture->unlockBufferGL(destTextureId);

    return success;
}

bool CopyVideoSurface::copyCurrentFrame(GLenum destTextureTarget, GLuint destTextureId,
                                        const android::sp<VideoSurface>& videoSurface,
                                        IntRect destRect, EGLFence* fence)
{
    if (!destRect.width() || !destRect.height())
        return true;

    AutoRestoreGLContext restoreContext(m_context);
    return copyCurrentFrameInternal(destTextureTarget, destTextureId, videoSurface, destRect, fence);
}

bool CopyVideoSurface::copyCurrentFrameInternal(GLenum destTextureTarget, GLuint destTextureId,
                                                const android::sp<VideoSurface>& videoSurface,
                                                IntRect destRect, EGLFence* fence)
{
    DrawVideoSurface::TransformFlags transformFlags = DrawVideoSurface::NoTransform;
    if (destRect.width() < 0) {
        transformFlags |= DrawVideoSurface::FlipHorizontally;
        destRect.setX(destRect.maxX());
        destRect.setWidth(-destRect.width());
    }
    if (destRect.height() < 0) {
        transformFlags |= DrawVideoSurface::FlipVertically;
        destRect.setY(destRect.maxY());
        destRect.setHeight(-destRect.height());
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, destTextureTarget, destTextureId, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;

    glViewport(destRect.x(), destRect.y(), destRect.width(), destRect.height());
    m_drawVideoSurface.drawCurrentFrame(videoSurface, transformFlags);

    if (fence)
        fence->set();

    return true;
}

} // namespace WebCore
