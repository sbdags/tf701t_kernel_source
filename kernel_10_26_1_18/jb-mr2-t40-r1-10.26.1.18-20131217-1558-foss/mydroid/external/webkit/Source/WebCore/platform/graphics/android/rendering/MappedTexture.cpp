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
#include "MappedTexture.h"

#include "GLContext.h"
#include "GLUtils.h"
#include "UIThread.h"
#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>

static void copyAndPremultiplyAlpha(uint8_t* source, uint8_t* target, size_t numPixels)
{
    size_t numChannels = 4 * numPixels;
    for (size_t i = 0; i < numChannels; i += 4) {
        // To save a divide by 255, we do `color = color * (1 + alpha) / 256`.
        // This keeps the properties that 0 alpha always gives a 0, and that
        // 255 alpha leaves the color unchanged.
        unsigned onePlusAlpha = 1 + source[3 + i];
        target[0 + i] = (source[0 + i] * onePlusAlpha) >> 8;
        target[1 + i] = (source[1 + i] * onePlusAlpha) >> 8;
        target[2 + i] = (source[2 + i] * onePlusAlpha) >> 8;
        target[3 + i] = source[3 + i];
    }
}

namespace WebCore {

static bool makeCopyContextCurrentContext()
{
    static GLContext* copyContext = 0;

    if (!copyContext) {
        copyContext = GLContext::create(ResourceLimits::System).leakPtr();
        if (!copyContext)
            return false;
    }

    if (copyContext->isCurrent())
        return true;

    return copyContext->makeCurrent();
}


MappedTexture::MappedTexture(ResourceLimits::Context resourceContext, const IntSize& size, Format format, WriteMode writeMode, bool& success)
    : m_didWriteWithHardware(false)
    , m_isLockedForHardwareWrite(false)
    , m_fileDescriptorGrant(resourceContext, 1)
#if !ASSERT_DISABLED
    , m_mappedPixels(0)
#endif
{
    success = false;
    if (!m_fileDescriptorGrant.isGranted())
        return;

    int usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN | android::GraphicBuffer::USAGE_HW_TEXTURE;

    if (writeMode & WriteUsingHardware)
        usage |= android::GraphicBuffer::USAGE_HW_RENDER;

    // FIXME: make this conditional to writeMode & WriteUsingSoftware after http://nvbugs/1175689 is fixed.
    usage |= android::GraphicBuffer::USAGE_SW_WRITE_OFTEN;

    const int graphicFormat = format == HasAlpha ? android::PIXEL_FORMAT_RGBA_8888 : android::PIXEL_FORMAT_RGBX_8888;
    m_graphicBuffer = new android::GraphicBuffer(size.width(), size.height(), graphicFormat, usage);

    if (m_graphicBuffer->initCheck() != android::NO_ERROR) {
        m_graphicBuffer = 0;
        return;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    GLUtils::checkEglError("eglGetDisplay", display != EGL_NO_DISPLAY);

    static const EGLint attrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    EGLClientBuffer clientBuffer = reinterpret_cast<EGLClientBuffer>(m_graphicBuffer->getNativeBuffer());
    EGLImageKHR image = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attrs);
    GLUtils::checkEglError("eglCreateImageKHR", image != EGL_NO_IMAGE_KHR);

    if (image == EGL_NO_IMAGE_KHR) {
        m_graphicBuffer = 0;
        return;
    }

    m_eglImage = EGLImage::adopt(image, display);

    // Map the buffer to CPU memory once. This works around an apparent gralloc
    // issue where random noise can appear during the first mapping.
    // http://nvbugs/1175689
    SkBitmap tmp;
    if (!lockBufferForWriting(&tmp)) {
        m_graphicBuffer = 0;
        return;
    }
    tmp.eraseColor(0);
    unlockBuffer();

    success = true;
}

MappedTexture::~MappedTexture()
{
    ASSERT(!m_mappedPixels);

    m_eglImage.clear();
    m_graphicBuffer.clear();
}

bool MappedTexture::lockBufferGLInternal(GLuint* outTextureId, bool isWrite, GLint filter, GLint wrap)
{
    ASSERT(!m_mappedPixels);

    GLuint textureId = m_eglImage->createTexture(filter, wrap);
    if (!textureId) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_isLockedForHardwareWrite = isWrite;
    m_didWriteWithHardware |= isWrite;

    *outTextureId = textureId;
    return true;
}

void MappedTexture::unlockBufferGL(GLuint textureId)
{
    ASSERT(!m_mappedPixels);
    glDeleteTextures(1, &textureId);
    if (m_isLockedForHardwareWrite) {
        m_isLockedForHardwareWrite = false;
        m_hardwareWriteFence.set();
    }
}

bool MappedTexture::lockBufferInternal(SkBitmap* bitmap, bool isWrite)
{
    ASSERT(!m_mappedPixels);

    int usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN;
    if (isWrite)
        usage |= android::GraphicBuffer::USAGE_SW_WRITE_OFTEN;

    int width = size().width();
    int height = size().height();

    if (m_didWriteWithHardware) {
        // TODO: android native buffers do not really support writing to a texture with GL.
        // Wait until we know the write is done.
        m_hardwareWriteFence.finish();
        m_hardwareWriteFence.clear();

        // TODO: android native buffers do not really support writing to a texture with
        // GL. Notify gralloc that we need to do cache maintenance.
        void* dummy;
        m_graphicBuffer->lock(android::GraphicBuffer::USAGE_HW_RENDER, &dummy);
        android::status_t result = m_graphicBuffer->unlock();
        ASSERT_UNUSED(result, result == android::OK);
        m_didWriteWithHardware = false;
    }


    int rowBytes = 0;
    void* mappedPixels = 0;

    if (m_graphicBuffer->lock(usage, &mappedPixels) == android::OK)
        rowBytes = m_graphicBuffer->getStride() * android::bytesPerPixel(m_graphicBuffer->getPixelFormat());
    else
        mappedPixels = 0;

    if (!mappedPixels) {
        ASSERT_NOT_REACHED();
        return false;
    }

#if !ASSERT_DISABLED
    m_mappedPixels = mappedPixels;
#endif

    bitmap->setConfig(SkBitmap::kARGB_8888_Config, width, height, rowBytes);
    bitmap->setPixels(reinterpret_cast<uint8_t*>(mappedPixels));

    return true;
}

bool MappedTexture::lockBufferForReading(SkBitmap* bitmap, bool premultiplyAlpha)
{
    if (!lockBufferInternal(bitmap, false))
        return false;

    if (!premultiplyAlpha)
        return true;

    uint8_t* pixels = reinterpret_cast<uint8_t*>(bitmap->getPixels());
    bitmap->setPixels(0);

    if (!bitmap->allocPixels()) {
        unlockBuffer();
        return false;
    }

    copyAndPremultiplyAlpha(pixels, reinterpret_cast<uint8_t*>(bitmap->getPixels()), bitmap->getSize());

    return true;
}

void MappedTexture::unlockBuffer()
{
    ASSERT(m_mappedPixels);
#if !ASSERT_DISABLED
    m_mappedPixels = 0;
#endif

    m_graphicBuffer->unlock();
}

bool MappedTexture::copyTo(MappedTexture* dest)
{
    ASSERT(!m_mappedPixels);
    ASSERT(!isUIThread()); // This function modifies current context.
    ASSERT(dest);

    if (!makeCopyContextCurrentContext())
        return false;

    GLuint sourceTextureId = 0;
    if (!lockBufferForReadingGL(&sourceTextureId))
        return false;

    GLuint destTextureId = 0;
    if (!dest->lockBufferForWritingGL(&destTextureId)) {
        unlockBufferGL(sourceTextureId);
        return false;
    }

    GLuint copyFbo;
    glGenFramebuffers(1, &copyFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, copyFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sourceTextureId, 0);

    glBindTexture(GL_TEXTURE_2D, destTextureId);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size().width(), size().height());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &copyFbo);

    dest->unlockBufferGL(destTextureId);
    unlockBufferGL(sourceTextureId);

    return true;
}

} // namespace WebCore
