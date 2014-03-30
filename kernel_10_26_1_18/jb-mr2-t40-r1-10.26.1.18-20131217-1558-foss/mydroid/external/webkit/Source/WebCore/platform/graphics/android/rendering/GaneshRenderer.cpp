/*
 * Copyright 2011, The Android Open Source Project
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

#define LOG_TAG "GaneshRenderer"
#define LOG_NDEBUG 1

#include "config.h"
#include "GaneshRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "AutoRestoreGLState.h"
#include "GrContext.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "TilesManager.h"

namespace WebCore {

GaneshRenderer::GaneshRenderer()
    : m_tileFBO(0)
    , m_tileStencil(0)
    , m_renderBufferDevice(0)
    , m_renderBufferTextureId(0)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GaneshRenderer");
#endif
}

GaneshRenderer::~GaneshRenderer()
{
    ASSERT(!m_renderBufferTextureId);

    if (m_renderBufferDevice) {
        m_renderBufferDevice->unref();

        m_context->makeCurrent();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &m_tileStencil);
        glDeleteFramebuffers(1, &m_tileFBO);

        m_context.clear();
    }

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GaneshRenderer");
#endif
}

void GaneshRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (!m_renderBuffer)
        m_renderBuffer = MappedTexture::create(ResourceLimits::System, TilesManager::tileSize(), MappedTexture::HasAlpha, MappedTexture::WriteUsingHardware);
    if (!m_renderBuffer)
        return;
    // FIXME: here we should wait until it's certain that renderBuffer is not used
    // by the other context anymore.

    SkDevice* device = getDeviceForRenderBuffer(renderInfo.background);
    if (!device)
        return;

    canvas->setDevice(device);
}

void GaneshRenderer::renderingComplete(SkCanvas* canvas)
{
    canvas->flush();
    glFinish();
    canvas->setDevice(0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_tileFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    if (m_renderBufferTextureId) {
        m_renderBuffer->unlockBufferGL(m_renderBufferTextureId);
        m_renderBufferTextureId = 0;
    }
}

SkDevice* GaneshRenderer::getDeviceForRenderBuffer(const Color& background)
{
    if (!m_renderBufferDevice) {
        m_context = GLContext::create(ResourceLimits::System);
        if (!m_context)
            return 0;

        GrContext* grContext = GrContext::Create(kOpenGL_Shaders_GrEngine, 0);
        glGenFramebuffers(1, &m_tileFBO);
        glGenRenderbuffers(1, &m_tileStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, m_tileStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, TilesManager::tileWidth(), TilesManager::tileHeight());
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, m_tileFBO);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_tileStencil);

        GrPlatformRenderTargetDesc renderTargetDesc;
        renderTargetDesc.fWidth = TilesManager::tileWidth();
        renderTargetDesc.fHeight = TilesManager::tileHeight();
        renderTargetDesc.fConfig = kRGBA_8888_GrPixelConfig;
        renderTargetDesc.fSampleCnt = 0;
        renderTargetDesc.fStencilBits = 8;
        renderTargetDesc.fRenderTargetHandle = m_tileFBO;

        GrRenderTarget* renderTarget = grContext->createPlatformRenderTarget(renderTargetDesc);
        m_renderBufferDevice = new SkGpuDevice(grContext, renderTarget);
        renderTarget->unref();
        grContext->unref();
    }

    ASSERT(m_context->isCurrent());

    AutoRestoreTextureBinding2D textureBinding2DRestore;
    AutoRestoreFramebufferBinding frameBufferRestore;
    AutoRestoreClearColor clearColorRestore;
    AutoRestoreClearStencil clearStencilRestore;

    glBindFramebuffer(GL_FRAMEBUFFER, m_tileFBO);

    if (m_renderBuffer->lockBufferForWritingGL(&m_renderBufferTextureId)) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_renderBufferTextureId, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ASSERT_NOT_REACHED();
            m_renderBuffer->unlockBufferGL(m_renderBufferTextureId);
            m_renderBufferTextureId = 0;
            return 0;
        }
    } else
        return 0;

    float r, g, b, a;
    background.getRGBA(r, g, b, a);
    glClearColor(r, g, b, a);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    GLUtils::checkGlError("getDeviceForTile");
    return m_renderBufferDevice;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
