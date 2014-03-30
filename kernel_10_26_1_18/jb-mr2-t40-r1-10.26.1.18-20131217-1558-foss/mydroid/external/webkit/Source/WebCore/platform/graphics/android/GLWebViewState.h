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

#ifndef GLWebViewState_h
#define GLWebViewState_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "DrawExtra.h"
#include "GLExtras.h"
#include "IntRect.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SurfaceCollectionManager.h"
#include <wtf/Threading.h>

// Performance measurements probe
// To use it, enable the visual indicators in debug mode.
// turning off the visual indicators will flush the measures.
// #define MEASURES_PERF
#define MAX_MEASURES_PERF 2000

// Prefetch and render 1 tiles ahead of the scroll
// TODO: We should either dynamically change the outer bound by detecting the
// HW limit or save further in the GPU memory consumption.
#define TILE_PREFETCH_DISTANCE 1

namespace WebCore {

class BaseLayerAndroid;
class LayerAndroid;
class ScrollableLayerAndroid;
class TexturesResult;

/////////////////////////////////////////////////////////////////////////////////
// GL Architecture
/////////////////////////////////////////////////////////////////////////////////
//
// To draw things, WebView use a tree of layers. The root of that tree is a
// BaseLayerAndroid, which may have numerous LayerAndroid over it. The content
// of a layer is either a PicturePile (BaseLayer, LayerAndroid) or buffer such
// as a video frame as a GL texture.
//
// When drawing, we therefore have one large "surface" that is the BaseLayerAndroid,
// and (possibly) additional surfaces (usually smaller), which are the
// LayerAndroids. The BaseLayerAndroid usually corresponds to the normal web page
// content, the LayerAndroids are used for some parts such as specific divs (e.g. fixed
// position divs, or elements using CSS3D transforms, or containing video,
// plugins, etc.).
//
// The rendering model is to use tiles to display the BaseLayerAndroid (as obviously
// the area of a BaseLayerAndroid can be arbitrarly large). The idea is to compute a set of
// tiles covering the visibleContentRect's area, paint those tiles using the webview's
// content (i.e. the PicturePile of BaseLayerAndroid), then display those tiles.
// We check which tile we should use at every frame.
//
// Overview
// ---------
//
// A set of layers is painted to a Surface. Surface represents the configuration of the layers at
// the moment of the specific frame that the Surface depicts. The configuration means relative
// positions of the layers in the Surface. The amount of layers in a single Surface depends on
// what kind of layers there are. Compatible layers can be flattened to a single Surface.
//
// Multiple Surface instances form a SurfaceCollection. This reprensents a single frame.
//
// Multiple SurfaceCollections are stored in SurfaceCollectionManager. This class holds the various
// different frames: currently drawn frame, a future frame currently being painted and a queued
// frame. The queued frame will be painted after currently painted frame is swapped to a new drawn
// frame.
//
// A Surface is backed by a SurfaceBacking. SurfaceBacking represents the pixels of the
// Surface. SurfaceBacking records also the invalidation area, eg. the area of pixels that is
// not up to date relative to the owning Surface. One SurfaceBacking can be referred to
// by multiple Surface instances from different frames.
// FIXME: As can be deduced, SurfaceBacking / Surface abstraction is not correct as multiple
// owners vs. one damage area points out.
//
// SurfaceBacking holds multiple TileGrid instances. The TileGrid instances represent various
// different paint versions of the content that is painted to the Surface and SurfaceBacking.
//
// The tiles are grouped into a TileGrid -- basically a map of tiles covering
// the surface of the layer. When drawing, we ask the TileGrid to prepareGL()
// itself then draw itself on screen. The prepareGL() function is the one
// that schedules tiles to be painted -- i.e. the subset of tiles that intersect
// with the current visibleContentRect. When they are ready, we can display
// the TileGrid.
//
// Note that BaseLayerAndroid::drawGL() will return true to the java side if
// there is a need to be called again (i.e. if we do not have up to date
// textures or a transition is going on).
//
// Tiles are implemented as a Tile. It knows how to paint itself with the
// TilePainter, and to display itself. A GL texture is usually associated to it.
//
// SurfaceBacking works with multiple TileGrid instances. For example, one to display the page at the
// current scale factor, and another we use to paint the page at a different
// scale factor. I.e. when we zoom, we use TileGrid A, with its tiles scaled
// accordingly (and therefore possible loss of quality): this is fast as it's
// purely a hardware operation. When the user is done zooming, we ask for
// TileGrid B to be painted at the new scale factor, covering the
// visibleContentRect's area. When B is ready, we swap it with A.
//
// Texture allocation
// ------------------
//
// Obviously we cannot have every Tile having a GL texture -- we need to
// get the GL textures from an existing pool, and reuse them.
//
// The way we do it is that when we call TileGrid::prepareGL(), we group the
// tiles we need (i.e. in the visibleContentRect and dirty) and allocate
// new backbuffer textures for the tiles from TilesManager.
//
// The allocation TilesManager alloctaion mechanism goal is to (in order):
// - prefers to allocate the same texture as the previous time
// - prefers to allocate textures that are as far from the visibleContentRect as possible
// - prefers to allocate textures that are used by different TileGrids
//
// During each TileGrid::prepareGL() we compute which tiles are dirty based on the info we have
// received from webkit.
//
// Tile Invalidation
// ------------------
//
// We do not want to repaint a tile if the tile is up-to-date. A tile is
// considered to be dirty an in need of redrawing in the following cases
//  - the tile has acquires a new texture
//  - webkit invalidates all or part of the tiles contents
//
// To handle the case of webkit invalidation of the base layer, we store the invalidation area from
// WebKit in the BaseLayerAndroid.
//
// Painting scheduling
// -------------------
//
// The prepareGL step submits the tiles to be painted in the TexturesGenerator.
//
// Tile::paintBitmap() will paint the texture using the content of the layer.
//
// Note that TexturesGenerator is running in separate threads, the textures
// are mapped to main memory using GraphicsBuffers.
//
/////////////////////////////////////////////////////////////////////////////////

class GLWebViewState {
public:
    GLWebViewState();
    ~GLWebViewState();
    void setForceSingleSurfaceRendering(bool forced)
    {
        m_forceSingleSurfaceRendering = forced;
    }

    bool showVisualIndicator() const { return m_showVisualIndicator; }

    bool setBaseLayer(BaseLayerAndroid* layer, bool showVisualIndicator,
                      bool isPictureAfterFirstLayout);
    void paintExtras();

    GLExtras* glExtras() { return &m_glExtras; }

    void setIsScrolling(bool isScrolling);
    bool isScrolling();

    bool setLayersRenderingMode(TexturesResult&);

    int drawGL(IntRect& rect, SkRect& visibleContentRect, IntRect* invalRect,
               IntRect& screenRect, int titleBarHeight,
               IntRect& clip, float scale, float minScale,
               bool* collectionsSwappedPtr, bool* newCollectionHasAnimPtr,
               bool shouldDraw);

    bool wasLastDrawSuccessful() const { return m_lastDrawSuccessful; }

#ifdef MEASURES_PERF
    void dumpMeasures();
#endif

    void addDirtyArea(const IntRect& rect);
    void resetLayersDirtyArea();
    void doFrameworkFullInval();
    bool inUnclippedDraw() { return m_inUnclippedDraw; }

    float scale() const { return m_scale; }
    float minScale() const { return m_minScale; }

    // Currently, we only use 3 modes : kAllTextures, kClippedTextures and
    // kSingleSurfaceRendering ( for every mode > kClippedTextures ) .
    enum LayersRenderingMode {
        kAllTextures              = 0, // all layers are drawn with textures fully covering them
        kClippedTextures          = 1, // all layers are drawn, but their textures will be clipped
        kScrollableAndFixedLayers = 2, // only scrollable and fixed layers will be drawn
        kFixedLayers              = 3, // only fixed layers will be drawn
        kSingleSurfaceRendering   = 4  // no layers will be drawn on separate textures
                                       // -- everything is drawn on the base surface.
    };

    LayersRenderingMode layersRenderingMode() { return m_layersRenderingMode; }
    bool isSingleSurfaceRenderingMode() { return m_layersRenderingMode == kSingleSurfaceRendering; }
    void scrollLayer(int layerId, int x, int y);

    static bool hasRenderPriority();

    struct ScrollState {
        bool isScrollingSet;
        bool isVisibleContentRectScrolling;
        bool isGoingDown;
        SkRect visibleContentRect;

        bool isScrolling() const { return isScrollingSet || isVisibleContentRectScrolling; }
    };

    ScrollState copyScrollState();

private:
    void setVisibleContentRect(const SkRect& visibleContentRect, float scale, float minScale);
    double setupDrawing(const IntRect& invScreenRect, const SkRect& visibleContentRect,
                        const IntRect& screenRect, int titleBarHeight,
                        const IntRect& screenClip, float scale, float minScale);
    void showFrameInfo(const IntRect& rect, bool collectionsSwapped);
    void clearRectWithColor(const IntRect& rect, float r, float g,
                            float b, float a);
    double m_prevDrawTime;

    IntRect m_frameworkLayersInval;
    bool m_doFrameworkFullInval;
    bool m_inUnclippedDraw;

#ifdef MEASURES_PERF
    unsigned int m_totalTimeCounter;
    int m_timeCounter;
    double m_delayTimes[MAX_MEASURES_PERF];
    bool m_measurePerfs;
#endif
    GLExtras m_glExtras;

    float m_scale;
    float m_minScale;

    // Ensures atomicity of the scrolling state needed to prioritize tiles.
    ScrollState m_scrollState;
    WTF::Mutex m_scrollStateLock;

    LayersRenderingMode m_layersRenderingMode;
    SurfaceCollectionManager m_surfaceCollectionManager;

    bool m_lastDrawSuccessful;
    bool m_showVisualIndicator;
    bool m_forceSingleSurfaceRendering;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // GLWebViewState_h
