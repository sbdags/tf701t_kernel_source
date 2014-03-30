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

#ifndef Tile_h
#define Tile_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatPoint.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "TextureOwner.h"
#include "TilePainter.h"
#include "UIThread.h"
#include <utils/threads.h>

class SkBitmap;

namespace WebCore {

class BaseRenderer;
class GLWebViewState;
class TileTexture;
class TileBacking;


/**
 * An individual tile that is used to construct part of a webpage's BaseLayer of
 * content.  Each tile is assigned to a TiledPage and is responsible for drawing
 * and displaying their section of the page.  The lifecycle of a tile is:
 *
 * 1. Each tile is created on the main GL thread and assigned to a specific
 *    location within a TiledPage.
 * 2. When needed the tile is passed to the background thread where it paints
 *    the BaseLayer's most recent PictureSet to a bitmap which is then uploaded
 *    to the GPU.
 * 3. After the bitmap is uploaded to the GPU the main GL thread then uses the
 *    tile's drawGL() function to display the tile to the screen.
 * 4. Steps 2-3 are repeated as necessary.
 * 5. The tile is destroyed when the user navigates to a new page.
 *
 */
class Tile : public TextureOwner {
public:

    Tile(int x, int y, bool isLayerTile = false);
    ~Tile();

    bool isLayerTile() { return m_isLayerTile; }

    // Returns true if a TilePaintOperation should be scheduled.
    bool prepareGL(float scale, bool isExpandedPrefetchTile, bool shouldTryUpdateWithBlit, TilePainter*);

    struct PriorityInfo {
        bool hasFrontTexture;
        unsigned long long drawCount;
        float scale;
    };
    PriorityInfo copyPriorityInfo(); // Called by a TG thread.

    // Return false when real draw didn't happen for any reason.
    bool drawGL(float opacity, const SkRect& rect, float scale,
                const TransformationMatrix* transform,
                bool forceBlending, bool usePointSampling,
                const FloatRect& fillPortion);

    // Called by a TG thread.
    void paintBitmap(TilePainter*, BaseRenderer*, bool showVisualIndicator);

    bool intersectWithRect(int x, int y, int tileWidth, int tileHeight,
                           float scale, const SkRect& dirtyRect,
                           SkRect& realTileRect);
    bool isTileVisible(const IntRect& viewTileBounds);

    void markAsDirty(const SkRegion& dirtyArea, TilePainter*);
    bool isDirty();
    const SkRegion& dirtyArea() { return m_dirtyArea; }
    float scale() const { return m_scale; }

    int x() const { return m_x; }
    int y() const { return m_y; }
    bool hasFrontTexture() const { return m_frontTexture; }

    void prepareForBlit();
    void blitUpdate(const SkBitmap& subset, const SkIRect& inval);

    // only used for tile allocation - the higher, the more relevant the tile is
    unsigned long long drawCount()
    {
        ASSERT(isUIThread());
        return m_drawCount;
    }

    void discardTextures();
    bool swapTexturesIfNeeded();

    //FIXME: REMOVE
    unsigned int getImageTextureId();

    // TextureOwner implementation
    virtual void removeTexture(TileTexture* texture);

    int numberOfTexturesNeeded();

private:
    void updateDrawCount(bool isExpandedPrefetchTile);
    bool canBlitUpdate() const;
    void discardTexturesInternal();
    void markPictureChanged() {
        // Must be called with m_atomicSync locked.
        if (!++m_pictureGeneration) {
            m_frontGeneration = 0;
            m_backGeneration = 0;
            m_pictureGeneration = 1;
        }
    }

    const int m_x;
    const int m_y;
    const bool m_isLayerTile;

    // Synchronizes the variables marked as Locked.
    android::Mutex m_atomicSync;

    TileBacking* m_frontTexture;
    TileTexture* m_backTexture; // Locked, written in UI thread.
    TileBacking* m_lastDrawnTexture;
    float m_scale; // Locked, written in UI thread.
    TilePainter* m_painter; // Locked, written in UI thread. Only for comparison, not guaranteed to point to a valid object.

    unsigned m_pictureGeneration; // Generation for m_dirtyArea and the picture. Changes every time picture changes.
    unsigned m_frontGeneration; // Generation for m_frontTexture. Locked, written in UI thread.
    unsigned m_backGeneration;  // Generation for m_pureColor and m_backTexture. Locked, written in TG thread.

    // store the dirty region
    SkRegion m_dirtyArea;

    // the most recent GL draw before this tile was prepared. used for
    // prioritization and caching. tiles with old drawcounts and textures they
    // own are used for new tiles and rendering
    unsigned long long m_drawCount;

    Color m_pureColor; // Contains the color of the tile if it's pure color tile. Locked, written in TG thread.
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // Tile_h
