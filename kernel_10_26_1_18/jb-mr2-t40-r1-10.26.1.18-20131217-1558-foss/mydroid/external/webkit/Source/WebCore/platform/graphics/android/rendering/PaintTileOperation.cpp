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

#define LOG_TAG "PaintTileOperation"
#define LOG_NDEBUG 1

#include "config.h"
#include "PaintTileOperation.h"

#include "AndroidLog.h"
#include "ClassTracker.h"
#include "FloatRect.h"
#include "GLWebViewState.h"
#include "ImageTexture.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "TilesManager.h"

namespace WebCore {

static const float scrollingOffsetFactor = 0.5f;

static FloatPoint focusPoint(const GLWebViewState::ScrollState& scrollState)
{
    FloatRect viewport(scrollState.visibleContentRect);
    FloatPoint focus = viewport.center();

    if (scrollState.isScrollingSet) {
        const float scrollingOffset = (scrollState.isGoingDown ? 1.0f : -1.0f) *
                                      viewport.height() * scrollingOffsetFactor;

        focus.move(0, scrollingOffset);
    }

    return focus;
}

PaintTileOperation::PaintTileOperation(Tile* tile, TilePainter* painter,
                                       GLWebViewState* state, bool isLowResPrefetch)
    : m_tile(tile)
    , m_painter(painter)
    , m_state(state)
    , m_isLowResPrefetch(isLowResPrefetch)
    , m_showVisualIndicator(state->showVisualIndicator())
    , m_usePositionForPriority(!m_tile->isLayerTile())
{
    ASSERT(m_tile);
    SkSafeRef(m_painter);
    if (m_painter && m_painter->drawTransform()) {
        m_usePositionForPriority = true;
        m_drawTransform = *m_painter->drawTransform();
    }
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("PaintTileOperation");
#endif
}

PaintTileOperation::~PaintTileOperation()
{
    if (m_painter && m_painter->type() == TilePainter::Image) {
        ImageTexture* image = static_cast<ImageTexture*>(m_painter);
        ImagesManager::instance()->releaseImage(image->imageCRC());
    } else {
        SkSafeUnref(m_painter);
    }
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("PaintTileOperation");
#endif
}

void PaintTileOperation::run(BaseRenderer* renderer)
{
    TRACE_METHOD();
    m_tile->paintBitmap(m_painter, renderer, m_showVisualIndicator);
}

int PaintTileOperation::priority(unsigned long long currentDraw)
{
    int priority = 200000;
    GLWebViewState::ScrollState scrollState = m_state->copyScrollState();

    // prioritize low res while scrolling, otherwise set priority above gDeferPriorityCutoff
    if (m_isLowResPrefetch)
        priority = scrollState.isScrolling() ? 0 : gDeferPriorityCutoff;

    Tile::PriorityInfo tileInfo = m_tile->copyPriorityInfo();

    // prioritize higher draw count
    long long drawDelta = currentDraw - tileInfo.drawCount;
    priority += 100000 * (int)std::min(std::max((long long)0, drawDelta), (long long)1000);

    // prioritize unpainted tiles, within the same drawCount
    if (tileInfo.hasFrontTexture)
        priority += 50000;

    if (m_usePositionForPriority) {
        FloatPoint tilePosition(m_tile->x(), m_tile->y());
        tilePosition += FloatSize(0.5f, 0.5f);
        tilePosition.scale(TilesManager::tileWidth() / tileInfo.scale, TilesManager::tileHeight() / tileInfo.scale);
        tilePosition = m_drawTransform.mapPoint(tilePosition);

        FloatSize distance = focusPoint(scrollState) - tilePosition;
        priority += static_cast<int>(distance.diagonalLength());
    }

    return priority;
}

}
