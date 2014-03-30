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
#include "EGLImageBuffer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AutoRestoreGLState.h"
#include "ResourceLimits.h"
#include "GLUtils.h"
#include <SkBitmap.h>

namespace WebCore {

void EGLImageBuffer::finish()
{
    m_fence.finish();
    m_fence.clear();
}

bool EGLImageBuffer::lockBufferForReadingGL(GLuint* outTextureId, GLint filter, GLint wrap)
{
    if (!eglImage())
        return false;

    GLuint textureId = eglImage()->createTexture(filter, wrap);
    if (!textureId) {
        ASSERT_NOT_REACHED();
        return false;
    }

    *outTextureId = textureId;
    return true;
}

void EGLImageBuffer::unlockBufferGL(GLuint textureId)
{
    glDeleteTextures(1, &textureId);
}

EGLImageBufferFromTexture::EGLImageBufferFromTexture(const IntSize& size, bool hasAlpha, bool& success)
    : m_size(size)
    , m_textureId(0)
    , m_colorFormat(hasAlpha ? GL_RGBA : GL_RGB)
#if !ASSERT_DISABLED
    , m_creationContext(eglGetCurrentContext())
#endif
{
    success = false;

    int maxTextureDimension;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureDimension);
    if (m_size.width() > maxTextureDimension || m_size.height() > maxTextureDimension
        || m_size.width() < 0 || m_size.height() < 0)
        return;

    if (!ResourceLimits::canSatisfyGraphicsMemoryAllocation(size.width() * size.height() * 4))
        return;

    AutoRestoreTextureBinding2D restoreTex2D;

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!m_size.isEmpty()) {
        glTexImage2D(GL_TEXTURE_2D, 0, m_colorFormat, m_size.width(), m_size.height(), 0, m_colorFormat, GL_UNSIGNED_BYTE, 0);

        m_eglImage = EGLImage::createFromTexture(m_textureId);

        if (!m_eglImage) {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
            return;
        }
    }

    success = m_textureId;
}

EGLImageBufferFromTexture::~EGLImageBufferFromTexture()
{
    if (m_textureId) {
        // This path should be taken only in producer context. The assert in deleteBufferSource()
        // takes care of this.
        deleteBufferSource();
    }
}

void EGLImageBufferFromTexture::onSourceContextReset()
{
    // The buffer might have corrupted content.
    m_eglImage.clear();
    if (m_textureId) {
        // This path should be taken only in producer context. The assert in deleteBufferSource()
        // takes care of this.
        deleteBufferSource();
    }
    m_size = IntSize();
}

void EGLImageBufferFromTexture::deleteBufferSource()
{
    ASSERT(eglGetCurrentContext() == m_creationContext);
    glDeleteTextures(1, &m_textureId);
    m_textureId = 0;
}

bool EGLImageBufferFromTexture::lockBufferForReading(SkBitmap* bitmap, bool premultiplyAlpha)
{
    GLuint textureId = 0;
    if (!lockBufferForReadingGL(&textureId))
        return false;

    // The compositor GL context is still active during the SW draw path.
    AutoRestoreFramebufferBinding restoreFBO;

    bitmap->setConfig(SkBitmap::kARGB_8888_Config, size().width(), size().height(), 4 * size().width());
    if (!bitmap->allocPixels()) {
        glDeleteTextures(1, &textureId);
        return false;
    }

    GLuint framebufferId;
    glGenFramebuffers(1, &framebufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    GLUtils::AlphaOp alphaOp = premultiplyAlpha ? GLUtils::AlphaDoPremultiply : GLUtils::AlphaDoNothing;
    GLUtils::readPixels(IntRect(IntPoint(), size()), bitmap->getPixels(), GLUtils::BottomToTop, alphaOp);

    glDeleteFramebuffers(1, &framebufferId);

    unlockBufferGL(textureId);

    return true;
}

void EGLImageBufferFromTexture::unlockBuffer()
{
}

} // namespace WebCore


#endif
