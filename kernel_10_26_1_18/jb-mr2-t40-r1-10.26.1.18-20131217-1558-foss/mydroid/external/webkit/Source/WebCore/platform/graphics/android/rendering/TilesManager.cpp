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

#define LOG_TAG "TilesManager"
#define LOG_NDEBUG 1

#include "config.h"
#include "TilesManager.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLWebViewState.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include "Tile.h"
#include "TileTexture.h"
#include "UIThread.h"
#include "VideoLayerAndroid.h"

#include <android/native_window.h>
#include <cutils/atomic.h>
#include <gui/GLConsumer.h>
#include <gui/Surface.h>
#include <wtf/CurrentTime.h>

// Important: We need at least twice as many textures as is needed to cover
// one viewport, otherwise the allocation may stall.
// We need n textures for one TiledPage, and another n textures for the
// second page used when scaling.
// In our case, we use 256*256 textures. Both base and layers can use up to
// MAX_TEXTURE_ALLOCATION textures, which is 224MB GPU memory in total.
// For low end graphics systems, we cut this upper limit to half.
// We've found the viewport dependent value m_currentTextureCount is a reasonable
// number to cap the layer tile texturs, it worked on both phones and tablets.
// TODO: after merge the pool of base tiles and layer tiles, we should revisit
// the logic of allocation management.
#define MAX_TEXTURE_ALLOCATION ((10+TILE_PREFETCH_DISTANCE*2)*(7+TILE_PREFETCH_DISTANCE*2)*4)

#define LAYER_TEXTURES_DESTROY_TIMEOUT 60 // If we do not need layers for 60 seconds, free the textures

static void allocateTextureVector(Vector<TileTexture*>& textures, int neededTextureCount)
{
    int nbTexturesToAllocate = neededTextureCount - textures.size();
    for (int i = 0; i < nbTexturesToAllocate; i++)
        textures.append(new TileTexture());
}

static void filterTextureListByDrawCount(const Vector<TileTexture*>& textures,
                                         unsigned long long drawCount,
                                         Vector<TileTexture*>& newTextures)
{
    for (size_t i = 0; i < textures.size(); i++) {
        if (TextureOwner* owner = textures[i]->owner()) {
            if (owner->drawCount() >= drawCount) {
                newTextures.append(textures[i]);
                continue;
            }

            textures[i]->setOwner(0);
        }

        delete textures[i];
    }
}

static void clearTextureVector(Vector<TileTexture*>& textures)
{
    for (size_t i = 0; i < textures.size(); ++i) {
        if (TextureOwner* owner = textures[i]->owner())
            textures[i]->setOwner(0);
        delete textures[i];
    }
    textures.clear();
    textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
}

namespace WebCore {

size_t TilesManager::maxTextureAllocation() const
{
    if (m_highEndGfx)
        return MAX_TEXTURE_ALLOCATION;
    return MAX_TEXTURE_ALLOCATION / 2;
}

TilesManager::TilesManager()
    : m_layerTexturesRemain(true)
    , m_highEndGfx(false)
    , m_invertedScreen(false)
    , m_useMinimalMemory(true)
    , m_useDoubleBuffering(true)
    , m_contentUpdates(0)
    , m_webkitContentUpdates(0)
    , m_drawGLCount(1)
    , m_lastTimeLayersUsed(0)
    , m_eglContext(EGL_NO_CONTEXT)
{
    ALOGV("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_tilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
}

void TilesManager::deleteOldTextures()
{
    ASSERT(isUIThread());
    unsigned long long sparedDrawCount = 0;
    // Spare those with max drawcount.
    for (size_t i = 0; i < m_textures.size(); i++) {
        if (TextureOwner* owner = m_textures[i]->owner())
            sparedDrawCount = std::max(sparedDrawCount, owner->drawCount());
    }

    Vector<TileTexture*> newTextures;
    Vector<TileTexture*> newTilesTextures;
    newTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    newTilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    filterTextureListByDrawCount(m_textures, sparedDrawCount, newTextures);
    filterTextureListByDrawCount(m_tilesTextures, sparedDrawCount, newTilesTextures);

    m_textures = newTextures;
    m_tilesTextures = newTilesTextures;
}

void TilesManager::deleteAllTextures()
{
    ASSERT(isUIThread());
    clearTextureVector(m_textures);
    clearTextureVector(m_tilesTextures);
}

void TilesManager::didResetRenderingContext()
{
    ASSERT(isUIThread());
    for (unsigned int i = 0; i < m_textures.size(); i++){
        m_textures[i]->didResetRenderingContext();
    }

    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        m_tilesTextures[i]->didResetRenderingContext();
    }
}

void TilesManager::gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                                        int* nbLayerTextures, int* nbAllocatedLayerTextures)
{
    ASSERT(isUIThread());
    *nbTextures = m_textures.size();
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        TileTexture* texture = m_textures[i];
        if (texture->hasTexture())
            *nbAllocatedTextures += 1;
    }
    *nbLayerTextures = m_tilesTextures.size();
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        TileTexture* texture = m_tilesTextures[i];
        if (texture->hasTexture())
            *nbAllocatedLayerTextures += 1;
    }
}

void TilesManager::printTextures()
{
    ASSERT(isUIThread());
#ifdef DEBUG
    ALOGV("++++++");
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        TileTexture* texture = m_textures[i];
        Tile* o = 0;
        if (texture->owner())
            o = (Tile*) texture->owner();
        int x = -1;
        int y = -1;
        if (o) {
            x = o->x();
            y = o->y();
        }
        ALOGV("[%d] texture %x owner: %x (%d, %d) scale: %.2f",
              i, texture, o, x, y, o ? o->scale() : 0);
    }
    ALOGV("------");
#endif // DEBUG
}

void TilesManager::gatherTextures()
{
    ASSERT(isUIThread());
    m_availableTextures = m_textures;
    m_availableTilesTextures = m_tilesTextures;
    m_layerTexturesRemain = true;
}

TileTexture* TilesManager::getAvailableTexture(TextureOwner* owner, bool isLayerTile)
{
    ASSERT(isUIThread());
    WTF::Vector<TileTexture*>* availableTexturePool;
    if (isLayerTile)
        availableTexturePool = &m_availableTilesTextures;
    else
        availableTexturePool = &m_availableTextures;

    // The heuristic for selecting a texture is as follows:
    //  1. Skip textures currently being painted, they can't be painted while
    //         busy anyway
    //  2. If a tile isn't owned, break with that one
    //  3. Don't let tiles acquire their front textures
    //  4. Otherwise, use the least recently prepared tile, but ignoring tiles
    //         drawn in the last frame to avoid flickering

    TileTexture* farthestTexture = 0;
    unsigned long long oldestDrawCount = getDrawGLCount() - 1;
    const unsigned int max = availableTexturePool->size();
    for (unsigned int i = 0; i < max; i++) {
        TileTexture* texture = (*availableTexturePool)[i];
        TextureOwner* currentOwner = texture->owner();
        if (!currentOwner) {
            // unused texture! take it!
            farthestTexture = texture;
            break;
        }

        if (currentOwner == owner) {
            // Don't let a tile acquire its own front texture, as the
            // acquisition logic doesn't handle that
            continue;
        }

        unsigned long long textureDrawCount = currentOwner->drawCount();
        if (oldestDrawCount > textureDrawCount) {
            farthestTexture = texture;
            oldestDrawCount = textureDrawCount;
        }
    }

    if (farthestTexture) {
        farthestTexture->setOwner(owner);
        availableTexturePool->remove(availableTexturePool->find(farthestTexture));
        return farthestTexture;
    } else {
        if (isLayerTile) {
            // couldn't find a tile for a layer, layers shouldn't request redraw
            // TODO: once we do layer prefetching, don't set this for those
            // tiles
            m_layerTexturesRemain = false;
        }
    }

#ifdef DEBUG
    printTextures();
#endif // DEBUG
    return 0;
}

void TilesManager::setHighEndGfx(bool highEnd)
{
    m_highEndGfx = highEnd;
}

bool TilesManager::highEndGfx()
{
    return m_highEndGfx;
}

int TilesManager::currentLayerTextureCount()
{
    ASSERT(isUIThread());
    return m_tilesTextures.size();
}

void TilesManager::setCurrentTextureCount(size_t newTextureCount)
{
    ASSERT(isUIThread());
    newTextureCount = std::min(newTextureCount, maxTextureAllocation());
    if (m_textures.size() >= newTextureCount)
        return;

    allocateTextureVector(m_textures, newTextureCount);
}

void TilesManager::setCurrentLayerTextureCount(size_t newTextureCount)
{
    ASSERT(isUIThread());
    newTextureCount = std::min(newTextureCount, maxTextureAllocation());

    if (!newTextureCount) {
        if (!m_tilesTextures.size())
            return;

        double secondsSinceLayersUsed = WTF::currentTime() - m_lastTimeLayersUsed;
        if (secondsSinceLayersUsed < LAYER_TEXTURES_DESTROY_TIMEOUT)
            return;

        clearTextureVector(m_tilesTextures);
        return;
    }

    m_lastTimeLayersUsed = WTF::currentTime();

    if (m_tilesTextures.size() >= newTextureCount)
        return;

    allocateTextureVector(m_tilesTextures, newTextureCount);
}

// When GL context changed or we get a low memory signal, we want to cleanup all
// the GPU memory webview is using.
// The recreation will be on the next incoming draw call at the drawGL of
// GLWebViewState or the VideoLayerAndroid
void TilesManager::cleanupGLResources()
{
    if (m_eglContext == EGL_NO_CONTEXT)
        return;

    shader()->cleanupGLResources();
    VideoLayerAndroid::cleanupGLResources();
    deleteAllTextures();
    // TODO: MediaTexture does not clear its resources.

    m_eglContext = EGL_NO_CONTEXT;
    GLUtils::checkGlError("TilesManager::cleanupGLResources");
}

void TilesManager::updateTilesIfContextVerified()
{
    EGLContext ctx = eglGetCurrentContext();
    GLUtils::checkEglError("contextChanged");
    if (ctx != m_eglContext) {
        if (m_eglContext != EGL_NO_CONTEXT) {
            // A change in EGL context is an unexpected error, but we don't want to
            // crash or ANR. Therefore, abandon the GL resources;
            // they'll be recreated later in setupDrawing. (We can't delete them
            // since the context is gone)
            ALOGE("Unexpected : EGLContext changed! current %x , expected %x",
                  ctx, m_eglContext);
            shader()->didResetRenderingContext();
            VideoLayerAndroid::didResetRenderingContext();
            didResetRenderingContext();
            // TODO: MediaTexture does not clear its resources.
        } else {
            // This is the first time we went into this new EGL context.
            // We will have the GL resources to be re-inited and we can't update
            // dirty tiles yet.
            ALOGD("new EGLContext from framework: %x ", ctx);
        }
    }
    m_eglContext = ctx;
    return;
}

TilesManager* TilesManager::instance()
{
    if (!gInstance) {
        gInstance = new TilesManager();
        ALOGV("instance(), new gInstance is %x", gInstance);
    }
    return gInstance;
}

TilesManager* TilesManager::gInstance = 0;
} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
