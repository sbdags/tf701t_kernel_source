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

#define LOG_TAG "SurfaceBacking"
#define LOG_NDEBUG 1

#include "config.h"
#include "SurfaceBacking.h"

#include "AndroidLog.h"
#include "Color.h"
#include "ClassTracker.h"
#include "GLWebViewState.h"
#include "LayerAndroid.h"

#define LOW_RES_PREFETCH_SCALE_MODIFIER 0.3f
static const float extraZoomOutFactor = 0.20f;
static const double zoomOutTimeMargin = 0.1;

static int fitSpanToBorders(int start, int end, int min, int max)
{
    if (start < min && end > max)
        return (min + max) / 2 - (start + end) / 2;
    if (start < min)
        return min - start;
    if (end > max)
        return max - end;

    return 0;
}

static IntRect scaleRectInRect(IntRect area, const float scale, const IntRect& fullContentArea)
{
    const float inflateRatio = (scale - 1.0f) / 2.0f;

    area.inflateX(inflateRatio * area.width());
    area.inflateY(inflateRatio * area.height());

    int dx = fitSpanToBorders(area.x(), area.maxX(), fullContentArea.x(), fullContentArea.maxX());
    int dy = fitSpanToBorders(area.y(), area.maxY(), fullContentArea.y(), fullContentArea.maxY());

    area.move(dx, dy);

    return area;
}

namespace WebCore {

SurfaceBacking::SurfaceBacking(bool isBaseSurface)
    : m_frontTileGrid(new TileGrid(isBaseSurface))
    , m_backTileGrid(new TileGrid(isBaseSurface))
    , m_lowResTileGrid(new TileGrid(isBaseSurface))
    , m_scale(-1)
    , m_futureScale(-1)
    , m_maxZoomScale(1)
    , m_lastScale(-1)
    , m_lastZoomOut(0)
    , m_waitingForSwap(false)
{

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("SurfaceBacking");
#endif
}

SurfaceBacking::~SurfaceBacking()
{
    delete m_frontTileGrid;
    delete m_backTileGrid;
    delete m_lowResTileGrid;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("SurfaceBacking");
#endif
}

bool SurfaceBacking::hasZoomOutTimedOut() const
{
    return WTF::currentTime() - m_lastZoomOut >= zoomOutTimeMargin;
}

float SurfaceBacking::setupScale(GLWebViewState* state, float maxZoomScale)
{
    // If the surface backing has ever zoomed beyond 1.0 scale, it's always
    // allowed to (so repaints aren't necessary when allowZoom toggles). If not,
    // and allowZoom is false, don't allow scale greater than 1.0
    m_maxZoomScale = std::max(m_maxZoomScale, maxZoomScale);
    float scale = state->scale();
    if (scale > m_maxZoomScale)
        scale = m_maxZoomScale;

    if (m_scale == -1) {
        m_scale = scale;
        m_futureScale = scale;
        m_lastScale = scale;
    }

    // Assume that the user is still zooming out if the scale has recently decreased.
    if (scale < m_lastScale)
        m_lastZoomOut = WTF::currentTime();
    m_lastScale = scale;

    // Wait for the low resolution tile grid to finish painting before we swap it out
    // with the front tile grid.
    if (isZoomingOut() && scale <= m_lowResTileGrid->scale())
        m_waitingForSwap = true;

    // Avoid trashing the tile grids by not scheduling a zoom in operation if a zoom out operation is
    // still in flight. Instead wait until the zoomed out content has been painted and displayed before
    // beginning to zoom in.
    const bool canZoomIn = scale > m_scale || (hasZoomOutTimedOut() && !m_waitingForSwap);

    if (scale < m_futureScale && !m_waitingForSwap)
        m_futureScale = std::max(scale * extraZoomOutFactor, state->minScale());
    else if (scale > m_futureScale && canZoomIn) {
        m_futureScale = scale;
        m_waitingForSwap = false;
    }

    return scale;
}

void SurfaceBacking::scheduleZooming(const float scale, GLWebViewState* state, const IntRect& prepareArea,
                                     const IntRect& fullContentArea, TilePainter* painter)
{
    if (!isZooming())
        return;

    const bool shouldPredictZoom = isZoomingOut() && !hasZoomOutTimedOut();
    TileGrid* tileGrid = shouldPredictZoom ? m_lowResTileGrid : m_backTileGrid;
    const IntRect futureArea = shouldPredictZoom ? scaleRectInRect(prepareArea, scale / m_futureScale, fullContentArea)
                                                 : prepareArea;

    if (m_futureScale != tileGrid->scale())
        tileGrid->discardTextures();

    tileGrid->prepareGL(state, m_futureScale, futureArea, fullContentArea,
                                painter, TileGrid::StandardRegion, false);
    tileGrid->swapTiles();
}

void SurfaceBacking::prepareGL(GLWebViewState* state, float maxZoomScale,
                               const IntRect& prepareArea, const IntRect& fullContentArea,
                               TilePainter* painter, bool aggressiveRendering,
                               bool updateWithBlit)
{
    const float scale = setupScale(state, maxZoomScale);
    scheduleZooming(scale, state, prepareArea, fullContentArea, painter);

    int prepareRegionFlags = TileGrid::StandardRegion;
    if (aggressiveRendering)
        prepareRegionFlags |= TileGrid::ExpandedRegion;

    ALOGV("Prepare SurfBack %p, scale %.2f, m_scale %.2f, f %p, b %p",
          this, scale, m_scale,
          m_frontTileGrid, m_backTileGrid);

    // Clear the flags for the regions which the frontTileGrid already has prepared.
    if (isZooming())
        prepareRegionFlags &= ~swapGridsIfNeeded();

    if (!isZooming()) {
        if (prepareRegionFlags) {
            // If the front grid hasn't already prepared, or needs to prepare
            // expanded bounds do so now.
            m_frontTileGrid->prepareGL(state, m_scale,
                                       prepareArea, fullContentArea, painter,
                                       prepareRegionFlags, false, updateWithBlit);
        }

        if (aggressiveRendering) {
            // Prepare low resolution content.
            m_lowResTileGrid->prepareGL(state, m_scale * LOW_RES_PREFETCH_SCALE_MODIFIER,
                                       prepareArea, fullContentArea, painter,
                                       TileGrid::StandardRegion | TileGrid::ExpandedRegion, true);
            m_lowResTileGrid->swapTiles();
        }
    }
}

void SurfaceBacking::drawGL(const IntRect& visibleContentArea, float opacity,
                            const TransformationMatrix* transform,
                            bool aggressiveRendering, const Color* background)
{
    // Draw low resolution prefetch page if zooming or if the front texture is missing content.
    if (aggressiveRendering && opacity == 1.0f && (isZoomingOut() || isMissingContent()))
        m_lowResTileGrid->drawGL(visibleContentArea, opacity, transform);

    m_frontTileGrid->drawGL(visibleContentArea, opacity, transform, background);
}

void SurfaceBacking::markAsDirty(const SkRegion& dirtyArea)
{
    m_backTileGrid->markAsDirty(dirtyArea);
    m_frontTileGrid->markAsDirty(dirtyArea);
    m_lowResTileGrid->markAsDirty(dirtyArea);
}

bool SurfaceBacking::swapTiles()
{
    bool swap = m_backTileGrid->swapTiles();
    swap |= m_frontTileGrid->swapTiles();
    swap |= m_lowResTileGrid->swapTiles();
    return swap;
}

void SurfaceBacking::computeTexturesAmount(TexturesResult* result,
                                           const IntRect& visibleContentArea,
                                           const IntRect& allTexturesArea,
                                           LayerAndroid* layer)
{
    // get two numbers here:
    // - textures needed for a clipped area
    // - textures needed for an un-clipped area
    TileGrid* tileGrid = m_frontTileGrid;
    if (isZoomingOut() && !hasZoomOutTimedOut())
        tileGrid = m_lowResTileGrid;
    else if (isZooming())
        tileGrid = m_backTileGrid;

    int nbTexturesFull = tileGrid->nbTextures(allTexturesArea, m_futureScale);
    int nbTexturesClipped = tileGrid->nbTextures(visibleContentArea, m_futureScale);

    if (layer) {
        // TODO: should handle multi-layer case better

        // Set kFixedLayers level
        if (layer->isPositionFixed())
            result->fixed += nbTexturesClipped;

        // Set kScrollableAndFixedLayers level
        if (layer->contentIsScrollable()
            || layer->isPositionFixed())
            result->scrollable += nbTexturesClipped;
    }

    // Set kClippedTextures level
    result->clipped += nbTexturesClipped;

    // Set kAllTextures level
    result->full += nbTexturesFull;
}

int SurfaceBacking::swapGridsIfNeeded()
{
    if (m_waitingForSwap && m_lowResTileGrid->isReady()) {
        m_scale = m_lowResTileGrid->scale();
        m_waitingForSwap = false;

        // High resolution front tile grid not needed anymore, swap it
        // with the low resolution tile grid and discard its textures.
        std::swap(m_frontTileGrid, m_lowResTileGrid);
        m_lowResTileGrid->discardTextures();

        return 0;
    }

    if ((isZoomingIn() || (isZoomingOut() && hasZoomOutTimedOut())) && m_backTileGrid->isReady()) {
        m_scale = m_futureScale;
        m_waitingForSwap = false;

        std::swap(m_frontTileGrid, m_backTileGrid);
        m_frontTileGrid->swapTiles();

        // After zoom in neither the back tile grid or the low resolution
        // tile grid contain up-to date content.
        m_lowResTileGrid->discardTextures();
        m_backTileGrid->discardTextures();

        return TileGrid::StandardRegion;
    }

    return 0;
}

} // namespace WebCore
