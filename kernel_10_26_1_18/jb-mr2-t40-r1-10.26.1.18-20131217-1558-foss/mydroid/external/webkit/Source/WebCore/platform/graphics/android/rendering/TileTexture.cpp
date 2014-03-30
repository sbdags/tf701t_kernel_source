/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "TileTexture"
#define LOG_NDEBUG 1

#include "config.h"
#include "TileTexture.h"

#include "AndroidLog.h"
#include "Tile.h"
#include "ClassTracker.h"
#include "DrawQuadData.h"
#include "GLUtils.h"
#include "GLWebViewState.h"
#include "MappedTexture.h"
#include "TextureOwner.h"
#include "TilesManager.h"
#include "UIThread.h"

namespace WebCore {

TileTexture::TileTexture()
    : m_owner(0)
    , m_textureNeedsFlipY(false)
    , m_hasAlpha(true)
{
    ASSERT(isUIThread());
    m_ownTextureId = 0;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("TileTexture");
#endif
}

TileTexture::~TileTexture()
{
    ASSERT(isUIThread());
    ASSERT(!m_owner);
    discardGLTexture();

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("TileTexture");
#endif
}

void TileTexture::discardGLTexture()
{
    ASSERT(isUIThread());
    if (!m_ownTextureId)
        return;

    m_texture->unlockBufferGL(m_ownTextureId);
    m_ownTextureId = 0;
}

void TileTexture::discardBackingStore()
{
    ASSERT(isUIThread());
    discardGLTexture();

    // clear both Tile->Texture and Texture->Tile links
    setOwner(0);

    m_texture = 0;
}

void TileTexture::setOwner(TextureOwner* owner)
{
    ASSERT(isUIThread());
    TextureOwner* oldOwner;

    {
        WTF::MutexLocker locker(m_ownerMutex);
        if (m_owner == owner)
            return;
        oldOwner = m_owner;
        m_owner = owner;
    }

    if (oldOwner)
        oldOwner->removeTexture(this);
}

void TileTexture::release(TextureOwner* owner)
{
    // This can be called from multiple threads.
    WTF::MutexLocker locker(m_ownerMutex);
    if (m_owner == owner)
        m_owner = 0;
}

void TileTexture::drawGL(bool isLayer, const SkRect& rect, float opacity,
                         const TransformationMatrix* transform,
                         bool forceBlending, bool usePointSampling,
                         const FloatRect& fillPortion)
{
    ASSERT(isUIThread());
    ShaderProgram* shader = TilesManager::instance()->shader();

    if (isLayer && !transform) {
        ALOGE("ERROR: Missing tranform for layers!");
        return;
    }

    if (!m_texture)
        return;

    if (!m_ownTextureId && !m_texture->lockBufferForReadingGL(&m_ownTextureId))
        return;

    SkRect geometry = rect;
    if (m_textureNeedsFlipY)
        std::swap(geometry.fTop, geometry.fBottom);

    DrawQuadData commonData(isLayer ? LayerQuad : BaseQuad, transform, &geometry,
                            opacity, fillPortion);
    TextureQuadData::ContentFlags contentFlags = TextureQuadData::CanDeferRendering;
    if (!hasAlpha() || (!forceBlending && !isLayer))
        contentFlags |= TextureQuadData::HasNoAlpha;

    GLint filter = usePointSampling ? GL_NEAREST : GL_LINEAR;
    TextureQuadData data(commonData, m_ownTextureId, GL_TEXTURE_2D, filter, contentFlags);
    shader->drawQuad(&data);
}

void TileTexture::blitUpdate(const SkBitmap& subset, const SkIRect& textureInval)
{
    // FIXME: this might not support needsFlipY.
    GLUtils::updateTextureWithBitmap(m_ownTextureId, subset, textureInval);
}

void TileTexture::swapBuffer(OwnPtr<MappedTexture>& buffer, bool needsFlipY)
{
    m_texture.swap(buffer);
    m_textureNeedsFlipY = needsFlipY;
}

void TileTexture::didResetRenderingContext()
{
    ASSERT(isUIThread());
    if (!m_ownTextureId)
        return;

    // We must abandon the buffer if the texture was in use. Hope that the driver
    // manages to free the backing based on the buffer refcount.
    m_ownTextureId = 0;
    m_texture = 0;
    setOwner(0);
}


} // namespace WebCore
