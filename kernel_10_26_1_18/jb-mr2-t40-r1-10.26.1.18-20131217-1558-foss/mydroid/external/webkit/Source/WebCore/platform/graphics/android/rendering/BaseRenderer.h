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

#ifndef BaseRenderer_h
#define BaseRenderer_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "MappedTexture.h"
#include <wtf/OwnPtr.h>

class SkCanvas;
class SkDevice;

namespace WebCore {

class InstrumentedPlatformCanvas;
class TextureInfo;
class Tile;
class TilePainter;
class TileTexture;

struct TileRenderInfo {
    TileRenderInfo(int x_, int y_, float scale_, const Color& background_, TilePainter* tilePainter_, bool showVisualIndicator_)
        : x(x_)
        , y(y_)
        , scale(scale_)
        , background(background_)
        , tilePainter(tilePainter_)
        , showVisualIndicator(showVisualIndicator_)
    {
    }

    // coordinates of the tile
    int x;
    int y;

    // current scale factor
    float scale;
    Color background;

    // the painter object in charge of drawing our content
    TilePainter* tilePainter;

    bool showVisualIndicator;
};

struct TileContentHints {
    TileContentHints()
        : didRender(false)
        , hasAlpha(true)
        , isPureColor(false)
        , pureColor(Color::transparent)
    {
    }

    bool didRender;
    bool hasAlpha;
    bool isPureColor;
    Color pureColor;
};

/**
 *
 */
class BaseRenderer {
public:
    enum RendererType { Raster, Ganesh };
    virtual ~BaseRenderer() {}
    virtual RendererType type() = 0;
    virtual bool renderedContentNeedsFlipY() const { return false; }

    TileContentHints renderTiledContent(const TileRenderInfo&);
    void commitRenderedContentToTileTexture(TileTexture*);

protected:
    virtual void setupCanvas(const TileRenderInfo&, SkCanvas*) = 0;
    virtual void renderingComplete(SkCanvas*) = 0;
    void checkForAlphaAndPureColor(TileContentHints&, InstrumentedPlatformCanvas&);

    // performs additional pure color check, renderInfo.isPureColor may already be set to true
    virtual void deviceCheckForAlphaAndPureColor(TileContentHints& , SkCanvas*) { }

    void drawTileInfo(SkCanvas* canvas, const TileRenderInfo& renderInfo,
            int updateCount, double renderDuration);

    OwnPtr<MappedTexture> m_renderBuffer;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // BaseRenderer_h
