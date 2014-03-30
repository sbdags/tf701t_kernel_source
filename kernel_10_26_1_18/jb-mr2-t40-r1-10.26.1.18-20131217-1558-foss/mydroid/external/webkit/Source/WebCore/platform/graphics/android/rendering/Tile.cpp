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

#define LOG_TAG "Tile"
#define LOG_NDEBUG 1

#include "config.h"
#include "Tile.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseRenderer.h"
#include "GLUtils.h"
#include "TileTexture.h"
#include "TilePureColorBacking.h"
#include "TilesManager.h"

namespace WebCore {

Tile::Tile(int x, int y, bool isLayerTile)
    : m_x(x)
    , m_y(y)
    , m_isLayerTile(isLayerTile)
    , m_frontTexture(0)
    , m_backTexture(0)
    , m_lastDrawnTexture(0)
    , m_scale(1)
    , m_painter(0)
    , m_pictureGeneration(1)
    , m_frontGeneration(0)
    , m_backGeneration(0)
    , m_drawCount(0)
{
    ASSERT(isUIThread());
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("Tile");
#endif
}

Tile::~Tile()
{
    if (m_backTexture)
        m_backTexture->release(this);
    if (m_frontTexture)
        m_frontTexture->release(this);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("Tile");
#endif
}
Tile::PriorityInfo Tile::copyPriorityInfo()
{
    Tile::PriorityInfo info;
    m_atomicSync.lock();
    info.hasFrontTexture = m_frontTexture;
    info.drawCount = m_drawCount;
    info.scale = m_scale;
    m_atomicSync.unlock();

    return info;
}

bool Tile::prepareGL(float scale, bool isExpandedPrefetchTile, bool shouldTryUpdateWithBlit, TilePainter* painter)
{
    ASSERT(isUIThread());

    const bool scaleChanges = m_scale != scale;

    if (!scaleChanges && m_pictureGeneration == m_frontGeneration) {
        android::AutoMutex lock(m_atomicSync);
        updateDrawCount(isExpandedPrefetchTile);
        return false;
    }

    if (!scaleChanges && shouldTryUpdateWithBlit && canBlitUpdate()) {
        // FIXME: in this path we lock twice. This should be fixed by moving the blit calls from
        // Surface to Tile.
        if (painter->blitFromContents(this)) {
            android::AutoMutex lock(m_atomicSync);
            updateDrawCount(isExpandedPrefetchTile);
            return false;
        }
    }

    android::AutoMutex lock(m_atomicSync);
    updateDrawCount(isExpandedPrefetchTile);

    if (scaleChanges) {
        discardTexturesInternal();
        markPictureChanged();
        m_scale = scale;
    }

    // Update m_painter in case scheduling a new paint operation is needed for any other reason
    // than markAsDirty call. Possible other reasons include:
    // - The tile is new
    // - scale changed
    // - removeTexture() took away one of this tile's textures
    // - a previously scheduled paint tile operation has not completed but the painter has changed
    //   (this case could also be handled by checking if there's a valid paint operation already in queue)
    m_painter = painter;

    if (m_backTexture)
        return true;

    m_backTexture = TilesManager::instance()->getAvailableTexture(this, isLayerTile());

    if (!m_backTexture)
        return false;

    // Release GL resources because back texture will be modified and swapped.
    m_backTexture->discardGLTexture();

    return true;
}

// Called while locked.
void Tile::updateDrawCount(bool isExpandedPrefetchTile)
{
    m_drawCount = TilesManager::instance()->getDrawGLCount();
    if (isExpandedPrefetchTile)
        m_drawCount--; // deprioritize expanded painting region
}

void Tile::removeTexture(TileTexture* texture)
{
    ASSERT(isUIThread());
    ALOGV("%p removeTexture %p, back %p front %p",
          this, texture, m_backTexture, m_frontTexture);

    if (m_frontTexture == texture) {
        m_dirtyArea.setEmpty(); // Front texture cannot be updated with blit.

        android::AutoMutex lock(m_atomicSync);
        m_frontTexture = 0;
        m_frontGeneration = 0;
    } else if (m_backTexture == texture) {
        android::AutoMutex lock(m_atomicSync);
        m_backTexture = 0;
        if (!m_pureColor.isValid())
            m_backGeneration = 0;
    }
}

int Tile::numberOfTexturesNeeded()
{
    if (m_pictureGeneration == m_frontGeneration)
        return m_frontTexture->reservesTexture() ? 1 : 0;

    android::AutoMutex lock(m_atomicSync);
    if (m_pictureGeneration == m_backGeneration)
        return (m_backTexture && m_backTexture->reservesTexture()) ? 1 : 0;

    // If the tile is dirty, it needs a back texture and it may have a front texture.
    return (m_frontTexture && m_frontTexture->reservesTexture()) ? 2 : 1;
}

void Tile::markAsDirty(const SkRegion& dirtyArea, TilePainter* painter)
{
    ASSERT(isUIThread());
    if (dirtyArea.isEmpty())
        return;
    m_dirtyArea.op(dirtyArea, SkRegion::kUnion_Op);
    // Check if we actually intersect with the area
    bool intersect = false;
    SkRegion::Iterator cliperator(dirtyArea);
    SkRect realTileRect;
    SkRect dirtyRect;
    while (!cliperator.done()) {
        dirtyRect.set(cliperator.rect());
        if (intersectWithRect(m_x, m_y, TilesManager::tileWidth(), TilesManager::tileHeight(),
                              m_scale, dirtyRect, realTileRect)) {
            intersect = true;
            break;
        }
        cliperator.next();
    }

    if (!intersect)
        return;

    android::AutoMutex lock(m_atomicSync);
    markPictureChanged();
    m_painter = painter;
}

bool Tile::isDirty()
{
    ASSERT(isUIThread());

    if (m_pictureGeneration == m_frontGeneration)
        return false;

    android::AutoMutex lock(m_atomicSync);
    return m_pictureGeneration != m_backGeneration;
}

bool Tile::drawGL(float opacity, const SkRect& rect, float scale,
                  const TransformationMatrix* transform,
                  bool forceBlending, bool usePointSampling,
                  const FloatRect& fillPortion)
{
    ASSERT(isUIThread());
    if (m_scale != scale)
        return false;

    // No need to mutex protect reads of m_frontTexture as it is only written to by
    // the consumer thread.
    if (!m_frontTexture)
        return false;

    if (fillPortion.maxX() < 1.0f || fillPortion.maxY() < 1.0f
        || fillPortion.x() > 0.0f || fillPortion.y() > 0.0f)
        ALOGV("drawing tile %p (%d, %d with fill portions %f %f->%f, %f",
              this, m_x, m_y, fillPortion.x(), fillPortion.y(),
              fillPortion.maxX(), fillPortion.maxY());

    m_frontTexture->drawGL(isLayerTile(), rect, opacity, transform,
                           forceBlending, usePointSampling, fillPortion);
    m_lastDrawnTexture = m_frontTexture;
    return true;
}

bool Tile::intersectWithRect(int x, int y, int tileWidth, int tileHeight,
                             float scale, const SkRect& dirtyRect,
                             SkRect& realTileRect)
{
    // compute the rect to corresponds to pixels
    realTileRect.fLeft = x * tileWidth;
    realTileRect.fTop = y * tileHeight;
    realTileRect.fRight = realTileRect.fLeft + tileWidth;
    realTileRect.fBottom = realTileRect.fTop + tileHeight;

    // scale the dirtyRect for intersect computation.
    SkRect realDirtyRect = SkRect::MakeWH(dirtyRect.width() * scale,
                                          dirtyRect.height() * scale);
    realDirtyRect.offset(dirtyRect.fLeft * scale, dirtyRect.fTop * scale);

    if (!realTileRect.intersect(realDirtyRect))
        return false;
    return true;
}

bool Tile::isTileVisible(const IntRect& viewTileBounds)
{
    return (m_x >= viewTileBounds.x()
            && m_x < viewTileBounds.x() + viewTileBounds.width()
            && m_y >= viewTileBounds.y()
            && m_y < viewTileBounds.y() + viewTileBounds.height());
}

// This is called from the texture generation thread
void Tile::paintBitmap(TilePainter* painter, BaseRenderer* renderer, bool showVisualIndicator)
{
    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    float scale = m_scale;

    if (m_painter != painter
        || m_pictureGeneration == m_frontGeneration
        || m_pictureGeneration == m_backGeneration
        || !m_backTexture) {
        m_atomicSync.unlock();
        return;
    }

    unsigned paintGeneration = m_pictureGeneration;

    m_atomicSync.unlock();

    Color background = Color::white;

    // Accessing isLayerTile() and painter->background() is thread-safe because
    // they won't change during lifetime of Tile and TilePainter, respectively.
    if (isLayerTile())
        background = Color::transparent;
    else {
        if (Color* pageBackground = painter->background())
            background = *pageBackground;
    }

    TileRenderInfo renderInfo(m_x, m_y, scale, background, painter, showVisualIndicator);
    TileContentHints tileContentHints = renderer->renderTiledContent(renderInfo);

    m_atomicSync.lock();

    if (tileContentHints.didRender && m_pictureGeneration == paintGeneration) {
        if (tileContentHints.isPureColor) {
            m_pureColor = tileContentHints.pureColor;
            m_backGeneration = paintGeneration;
        } else if (m_backTexture) {
            m_pureColor = Color();
            renderer->commitRenderedContentToTileTexture(m_backTexture);
            m_backTexture->setHasAlpha(tileContentHints.hasAlpha);
            m_backGeneration = paintGeneration;
        }
    }

    m_atomicSync.unlock();
}

void Tile::discardTextures() {
    ASSERT(isUIThread());
    android::AutoMutex lock(m_atomicSync);
    discardTexturesInternal();
}

void Tile::discardTexturesInternal()
{
    if (m_frontTexture) {
        m_frontTexture->release(this);
        m_frontTexture = 0;
        m_frontGeneration = 0;
    }

    if (m_backTexture) {
        m_backTexture->release(this);
        m_backTexture = 0;
        if (!m_pureColor.isValid())
            m_backGeneration = 0;
    }

    m_dirtyArea.setEmpty();
}

bool Tile::swapTexturesIfNeeded() {
    ASSERT(isUIThread());

    // Early out the frequent case without holding mutex.
    if (m_frontGeneration == m_pictureGeneration)
        return false;

    android::AutoMutex lock(m_atomicSync);
    if (m_backGeneration == m_pictureGeneration) {
        // discard old texture and swap the new one in its place
        if (m_frontTexture)
            m_frontTexture->release(this);

        if (m_pureColor.isValid()) {
            m_frontTexture = new TilePureColorBacking(m_pureColor);
            if (m_backTexture)
                m_backTexture->release(this);
        } else
            m_frontTexture = m_backTexture;

        m_frontGeneration = m_backGeneration;

        // We updated front texture. Disable blit only if we did not have a new update before swap.
        // If we already have new update, then we will use a bigger dirty area than strictly needed.
        if (m_pictureGeneration == m_frontGeneration)
            m_dirtyArea.setEmpty();

        // At this point we must have front texture, because the generations say so above.
        ASSERT(m_frontTexture);

        m_backTexture = 0;
        m_backGeneration = 0;

        ALOGV("display texture for %p at %d, %d front is now %p, back is %p",
              this, m_x, m_y, m_frontTexture, m_backTexture);

        return true;
    }

    return false;
}

bool Tile::canBlitUpdate() const
{
    return m_frontTexture && m_frontTexture->canBlitUpdate();
}

void Tile::blitUpdate(const SkBitmap& subset, const SkIRect& inval)
{
    ASSERT(canBlitUpdate());
    ASSERT(isUIThread());

    // First mark the Tile up to date and then blit. This way texture generator
    // has less chance of picking this tile and starting the rasterization.

    m_dirtyArea.setEmpty();

    {
        // The front texture was directly updated with a blit, so mark this as clean
        android::AutoMutex lock(m_atomicSync);
        m_frontGeneration = m_pictureGeneration;

        if (m_backTexture) {
            m_backTexture->release(this);
            m_backTexture = 0;
            if (!m_pureColor.isValid())
                m_backGeneration = 0;
        }
    }

    m_frontTexture->blitUpdate(subset, inval);
}

void Tile::prepareForBlit()
{
    ASSERT(canBlitUpdate());
    ASSERT(isUIThread());

    if (m_frontTexture != m_lastDrawnTexture) {
        // the below works around an issue where glTexSubImage2d can't update a
        // texture that hasn't drawn yet by drawing it off screen.
        // glFlush() and glFinish() work also, but are likely more wasteful.
        SkRect rect = SkRect::MakeXYWH(-100, -100, 0, 0);
        FloatRect fillPortion(0, 0, 0, 0);
        m_frontTexture->drawGL(false, rect, 1.0f, 0, false, true, fillPortion);
    }
}

//FIXME: REMOVE
unsigned int Tile::getImageTextureId()
{
    if (!m_frontTexture)
        return 0;

    return m_frontTexture->getImageTextureId();
}


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
