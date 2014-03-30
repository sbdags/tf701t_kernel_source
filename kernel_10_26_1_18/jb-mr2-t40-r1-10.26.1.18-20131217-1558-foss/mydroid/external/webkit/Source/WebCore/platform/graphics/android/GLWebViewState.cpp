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

#define LOG_TAG "GLWebViewState"
#define LOG_NDEBUG 1

#include "config.h"
#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "GLUtils.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "private/hwui/DrawGlInfo.h"
#include "ScrollableLayerAndroid.h"
#include "SkPath.h"
#include "TilesManager.h"
#include "SurfaceCollection.h"
#include "SurfaceCollectionManager.h"
#include <pthread.h>
#include <wtf/CurrentTime.h>

// fps indicator is FPS_INDICATOR_HEIGHT pixels high.
// The max width is equal to MAX_FPS_VALUE fps.
#define FPS_INDICATOR_HEIGHT 10
#define MAX_FPS_VALUE 60

#define COLLECTION_SWAPPED_COUNTER_MODULE 10

static double s_lastDrawTimestamp;
static const double s_renderPriorityCutoffMS = 100;

namespace WebCore {

using namespace android::uirenderer;

GLWebViewState::GLWebViewState()
    : m_frameworkLayersInval(0, 0, 0, 0)
    , m_doFrameworkFullInval(false)
    , m_scale(1)
    , m_minScale(1)
    , m_layersRenderingMode(kAllTextures)
    , m_surfaceCollectionManager()
    , m_lastDrawSuccessful(false)
    , m_showVisualIndicator(false)
    , m_forceSingleSurfaceRendering(false)
{
    m_scrollState.isScrollingSet = false;
    m_scrollState.isVisibleContentRectScrolling = false;
    m_scrollState.isGoingDown = true;
    m_scrollState.visibleContentRect.setEmpty();

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GLWebViewState");
#endif
#ifdef MEASURES_PERF
    m_timeCounter = 0;
    m_totalTimeCounter = 0;
    m_measurePerfs = false;
#endif
}

GLWebViewState::~GLWebViewState()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GLWebViewState");
#endif
    // Ensure that no remaining PaintTileOperations in TilesManager
    // will contain pointers to this GLWebViewState.
    m_surfaceCollectionManager.clearCollections();
    TilesManager::instance()->flushPendingPaintTileBatches();
}

bool GLWebViewState::setBaseLayer(BaseLayerAndroid* layer, bool showVisualIndicator,
                                  bool isPictureAfterFirstLayout)
{
    m_showVisualIndicator = showVisualIndicator;

    if (!layer || isPictureAfterFirstLayout)
        m_layersRenderingMode = kAllTextures;

    SurfaceCollection* collection = 0;
    if (layer) {
        ALOGV("layer tree %p, with child %p", layer, layer->getChild(0));
        layer->setState(this);
        collection = new SurfaceCollection(layer);
    }
    bool queueFull = m_surfaceCollectionManager.updateWithSurfaceCollection(
        collection, isPictureAfterFirstLayout);
    m_glExtras.setDrawExtra(0);

#ifdef MEASURES_PERF
    if (m_measurePerfs && !showVisualIndicator)
        dumpMeasures();
    m_measurePerfs = showVisualIndicator;
#endif

    return queueFull;
}

void GLWebViewState::scrollLayer(int layerId, int x, int y)
{
    m_surfaceCollectionManager.updateScrollableLayer(layerId, x, y);
}

void GLWebViewState::setVisibleContentRect(const SkRect& visibleContentRect, float scale, float minScale)
{
    // allocate max possible number of tiles visible with this visibleContentRect / expandedTileBounds
    const float invTileContentWidth = scale / TilesManager::tileWidth();
    const float invTileContentHeight = scale / TilesManager::tileHeight();

    int viewMaxTileX =
        static_cast<int>(ceilf((visibleContentRect.width()-1) * invTileContentWidth)) + 1;
    int viewMaxTileY =
        static_cast<int>(ceilf((visibleContentRect.height()-1) * invTileContentHeight)) + 1;

    TilesManager* tilesManager = TilesManager::instance();
    int maxTextureCount = viewMaxTileX * viewMaxTileY * (tilesManager->highEndGfx() ? 4 : 2);

    tilesManager->setCurrentTextureCount(static_cast<size_t>(maxTextureCount));

    m_minScale = minScale;
    {
        WTF::MutexLocker lock(m_scrollStateLock);
        // TODO: investigate whether we can move this return earlier.
        if ((m_scrollState.visibleContentRect == visibleContentRect)
            && (m_scale == scale)) {
            // everything below will stay the same, early return.
            m_scrollState.isVisibleContentRectScrolling = false;
            return;
        }
        m_scale = scale;

        m_scrollState.isGoingDown = m_scrollState.visibleContentRect.fTop - visibleContentRect.fTop <= 0;

        // detect if visibleContentRect is scrolling from short programmatic scrolls/jumps
        m_scrollState.isVisibleContentRectScrolling = m_scrollState.visibleContentRect != visibleContentRect
                                                      && SkRect::Intersects(m_scrollState.visibleContentRect, visibleContentRect);

        m_scrollState.visibleContentRect = visibleContentRect;
    }

    ALOGV("New visibleContentRect %.2f - %.2f %.2f - %.2f (w: %2.f h: %.2f scale: %.2f )",
          visibleContentRect.fLeft, visibleContentRect.fTop,
          visibleContentRect.fRight, visibleContentRect.fBottom,
          visibleContentRect.width(), visibleContentRect.height(), scale);
}

#ifdef MEASURES_PERF
void GLWebViewState::dumpMeasures()
{
    for (int i = 0; i < m_timeCounter; i++) {
        ALOGD("%d delay: %d ms", m_totalTimeCounter + i,
             static_cast<int>(m_delayTimes[i]*1000));
        m_delayTimes[i] = 0;
    }
    m_totalTimeCounter += m_timeCounter;
    m_timeCounter = 0;
}
#endif // MEASURES_PERF

void GLWebViewState::addDirtyArea(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    IntRect inflatedRect = rect;
    inflatedRect.inflate(8);
    if (m_frameworkLayersInval.isEmpty())
        m_frameworkLayersInval = inflatedRect;
    else
        m_frameworkLayersInval.unite(inflatedRect);
}

void GLWebViewState::resetLayersDirtyArea()
{
    m_frameworkLayersInval.setX(0);
    m_frameworkLayersInval.setY(0);
    m_frameworkLayersInval.setWidth(0);
    m_frameworkLayersInval.setHeight(0);
    m_doFrameworkFullInval = false;
}

void GLWebViewState::setIsScrolling(bool isScrolling)
{
    WTF::MutexLocker lock(m_scrollStateLock);
    m_scrollState.isScrollingSet = isScrolling;
}

bool GLWebViewState::isScrolling()
{
    WTF::MutexLocker lock(m_scrollStateLock);
    return m_scrollState.isScrolling();
}

void GLWebViewState::doFrameworkFullInval()
{
    m_doFrameworkFullInval = true;
}

double GLWebViewState::setupDrawing(const IntRect& invScreenRect,
                                    const SkRect& visibleContentRect,
                                    const IntRect& screenRect, int titleBarHeight,
                                    const IntRect& screenClip, float scale, float minScale)
{
    TilesManager* tilesManager = TilesManager::instance();

    // Make sure GL resources are created on the UI thread.
    // They are created either for the first time, or after EGL context
    // recreation caused by onTrimMemory in the framework.
    ShaderProgram* shader = tilesManager->shader();
    if (shader->needsInit()) {
        ALOGD("Reinit shader");
        shader->initGLResources();
    }
    shader->setupDrawing(invScreenRect, visibleContentRect, screenRect,
                         titleBarHeight, screenClip, scale);

    double currentTime = WTF::currentTime();

    setVisibleContentRect(visibleContentRect, scale, minScale);

    return currentTime;
}

bool GLWebViewState::setLayersRenderingMode(TexturesResult& nbTexturesNeeded)
{
    if (m_forceSingleSurfaceRendering) {
        if (m_layersRenderingMode == kSingleSurfaceRendering)
            return false;

        m_layersRenderingMode = kSingleSurfaceRendering;
        return true;
    }

    bool inval = false;
    LayersRenderingMode layersRenderingMode = m_layersRenderingMode;
    size_t maxTextures = 0;

    if (nbTexturesNeeded.full) {
        maxTextures = (2 * nbTexturesNeeded.full) + 1;

        TilesManager::instance()->setCurrentLayerTextureCount(maxTextures);
        maxTextures = TilesManager::instance()->currentLayerTextureCount();

        if (m_layersRenderingMode == kSingleSurfaceRendering) {
            // only switch out of SingleSurface mode, if we have 2x needed textures
            // to avoid changing too often
            maxTextures /= 2;
        }

        m_layersRenderingMode = kSingleSurfaceRendering;
        if (nbTexturesNeeded.fixed < maxTextures)
        m_layersRenderingMode = kFixedLayers;
        if (nbTexturesNeeded.scrollable < maxTextures)
            m_layersRenderingMode = kScrollableAndFixedLayers;
        if (nbTexturesNeeded.clipped < maxTextures)
            m_layersRenderingMode = kClippedTextures;
        if (nbTexturesNeeded.full < maxTextures)
            m_layersRenderingMode = kAllTextures;
    } else {
        TilesManager::instance()->setCurrentLayerTextureCount(0);
        m_layersRenderingMode = kAllTextures;
    }

    // For now, anything below kClippedTextures is equivalent
    // to kSingleSurfaceRendering
    // TODO: implement the other rendering modes
    if (m_layersRenderingMode > kClippedTextures)
        m_layersRenderingMode = kSingleSurfaceRendering;

    if (m_layersRenderingMode < layersRenderingMode
        && m_layersRenderingMode != kAllTextures)
        inval = true;

    if (m_layersRenderingMode > layersRenderingMode
        && m_layersRenderingMode != kClippedTextures)
        inval = true;

#ifdef DEBUG
    if (m_layersRenderingMode != layersRenderingMode) {
        static const char* mode[] = { "kAllTextures", "kClippedTextures",
            "kScrollableAndFixedLayers", "kFixedLayers", "kSingleSurfaceRendering" };
        ALOGD("Change from mode %s to %s -- We need textures: fixed: %d,"
              " scrollable: %d, clipped: %d, full: %d, max textures: %d",
              mode[layersRenderingMode],
              mode[m_layersRenderingMode],
              nbTexturesNeeded.fixed,
              nbTexturesNeeded.scrollable,
              nbTexturesNeeded.clipped,
              nbTexturesNeeded.full, maxTextures);
    }
#endif


    // update the base surface if needed
    return inval;
}

// -invScreenRect is the webView's rect with inverted Y screen coordinate.
// -visibleContentRect is the visible area in content coordinate.
// They are both based on  webView's rect and calculated in Java side.
//
// -screenClip is in screen coordinate, so we need to invert the Y axis before
// passing into GL functions. Clip can be smaller than the webView's rect.
//
// TODO: Try to decrease the number of parameters as some info is redundant.
int GLWebViewState::drawGL(IntRect& invScreenRect, SkRect& visibleContentRect,
                           IntRect* invalRect, IntRect& screenRect, int titleBarHeight,
                           IntRect& screenClip, float scale, float minScale,
                           bool* collectionsSwappedPtr, bool* newCollectionHasAnimPtr,
                           bool shouldDraw)
{
    TilesManager* tilesManager = TilesManager::instance();
    if (shouldDraw)
        tilesManager->getProfiler()->nextFrame(visibleContentRect.fLeft,
                                               visibleContentRect.fTop,
                                               visibleContentRect.fRight,
                                               visibleContentRect.fBottom,
                                               scale);
    tilesManager->incDrawGLCount();

    ALOGV("drawGL, invScreenRect(%d, %d, %d, %d), visibleContentRect(%.2f, %.2f, %.2f, %.2f)",
          invScreenRect.x(), invScreenRect.y(), invScreenRect.width(), invScreenRect.height(),
          visibleContentRect.fLeft, visibleContentRect.fTop,
          visibleContentRect.fRight, visibleContentRect.fBottom);

    ALOGV("drawGL, invalRect(%d, %d, %d, %d), screenRect(%d, %d, %d, %d)"
          "screenClip (%d, %d, %d, %d), scale %f titleBarHeight %d",
          invalRect->x(), invalRect->y(), invalRect->width(), invalRect->height(),
          screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height(),
          screenClip.x(), screenClip.y(), screenClip.width(), screenClip.height(), scale, titleBarHeight);

    m_inUnclippedDraw = shouldDraw && (screenRect == screenClip);

    resetLayersDirtyArea();

    tilesManager->updateTilesIfContextVerified();

    // gather the textures we can use, make sure this happens before any
    // texture preparation work.
    tilesManager->gatherTextures();

    // Upload any pending ImageTexture
    // Return true if we still have some images to upload.
    // TODO: upload as many textures as possible within a certain time limit
    int returnFlags = 0;
    if (ImagesManager::instance()->prepareTextures(this))
        returnFlags |= DrawGlInfo::kStatusDraw;

    double currentTime = setupDrawing(invScreenRect, visibleContentRect, screenRect,
                                      titleBarHeight, screenClip, scale, minScale);

    TexturesResult nbTexturesNeeded;
    bool scrolling = isScrolling();
    bool singleSurfaceMode = m_layersRenderingMode == kSingleSurfaceRendering;
    m_glExtras.setVisibleContentRect(visibleContentRect);

    returnFlags |= m_surfaceCollectionManager.drawGL(currentTime, invScreenRect,
                                                     visibleContentRect,
                                                     scale, scrolling,
                                                     singleSurfaceMode,
                                                     collectionsSwappedPtr,
                                                     newCollectionHasAnimPtr,
                                                     &nbTexturesNeeded, shouldDraw);
    if (shouldDraw)
        tilesManager->shader()->flushDrawing();

    int nbTexturesForImages = ImagesManager::instance()->nbTextures();
    ALOGV("*** We have %d textures for images, %d full, %d clipped, total %d / %d",
          nbTexturesForImages, nbTexturesNeeded.full, nbTexturesNeeded.clipped,
          nbTexturesNeeded.full + nbTexturesForImages,
          nbTexturesNeeded.clipped + nbTexturesForImages);
    nbTexturesNeeded.full += nbTexturesForImages;
    nbTexturesNeeded.clipped += nbTexturesForImages;

    if (setLayersRenderingMode(nbTexturesNeeded)) {
        m_surfaceCollectionManager.markAllAsDirty();
        returnFlags |= DrawGlInfo::kStatusDraw | DrawGlInfo::kStatusInvoke;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (returnFlags & DrawGlInfo::kStatusDraw) {
        // returnFlags & kStatusDraw && empty inval region means we've inval'd everything,
        // but don't have new content. Keep redrawing full view (0,0,0,0)
        // until tile generation catches up and we swap pages.
        bool fullScreenInval = m_frameworkLayersInval.isEmpty() || m_doFrameworkFullInval;

        if (!fullScreenInval) {
            m_frameworkLayersInval.inflate(1);

            invalRect->setX(m_frameworkLayersInval.x());
            invalRect->setY(m_frameworkLayersInval.y());
            invalRect->setWidth(m_frameworkLayersInval.width());
            invalRect->setHeight(m_frameworkLayersInval.height());

            ALOGV("invalRect(%d, %d, %d, %d)", invalRect->x(),
                  invalRect->y(), invalRect->width(), invalRect->height());

            if (!invalRect->intersects(invScreenRect)) {
                // invalidate is occurring offscreen, do full inval to guarantee redraw
                fullScreenInval = true;
            }
        }

        if (fullScreenInval) {
            invalRect->setX(0);
            invalRect->setY(0);
            invalRect->setWidth(0);
            invalRect->setHeight(0);
        }
    }

    m_lastDrawSuccessful = !returnFlags;

    if (shouldDraw)
        showFrameInfo(invScreenRect, *collectionsSwappedPtr);

    s_lastDrawTimestamp = currentTimeMS();
    // FIXME: the preparation process is too slow for kStatusInvoke callbacks. Make it faster or
    // remove the shouldDraw -related code.
    // WAR: Instead of asking for preparation, we should ask for full draw. If we process the
    // kStatusInvoke call, we don't have time to process the next draw. This causes a frame skip
    // during panning. In cases we would return kStatusInvoke, we have content that we can draw
    // during next invocation.
    if (returnFlags & DrawGlInfo::kStatusInvoke)
        returnFlags = DrawGlInfo::kStatusDraw;

    return returnFlags;
}

bool GLWebViewState::hasRenderPriority()
{
    return currentTimeMS() - s_lastDrawTimestamp < s_renderPriorityCutoffMS;
}

GLWebViewState::ScrollState GLWebViewState::copyScrollState()
{
    WTF::MutexLocker lock(m_scrollStateLock);
    return m_scrollState;
}

void GLWebViewState::showFrameInfo(const IntRect& rect, bool collectionsSwapped)
{
    bool drawOrDumpFrameInfo = m_showVisualIndicator;
#ifdef MEASURES_PERF
    drawOrDumpFrameInfo |= m_measurePerfs;
#endif
    if (!drawOrDumpFrameInfo)
        return;

    double currentDrawTime = WTF::currentTime();
    double delta = currentDrawTime - m_prevDrawTime;
    m_prevDrawTime = currentDrawTime;

#ifdef MEASURES_PERF
    if (m_measurePerfs) {
        m_delayTimes[m_timeCounter++] = delta;
        if (m_timeCounter >= MAX_MEASURES_PERF)
            dumpMeasures();
    }
#endif

    IntRect frameInfoRect = rect;
    frameInfoRect.setHeight(FPS_INDICATOR_HEIGHT);
    double ratio = (1.0 / delta) / MAX_FPS_VALUE;

    clearRectWithColor(frameInfoRect, 1, 1, 1, 1);
    frameInfoRect.setWidth(frameInfoRect.width() * ratio);
    clearRectWithColor(frameInfoRect, 1, 0, 0, 1);

    // Draw the collection swap counter as a circling progress bar.
    // This will basically show how fast we are updating the collection.
    static int swappedCounter = 0;
    if (collectionsSwapped)
        swappedCounter = (swappedCounter + 1) % COLLECTION_SWAPPED_COUNTER_MODULE;

    frameInfoRect = rect;
    frameInfoRect.setHeight(FPS_INDICATOR_HEIGHT);
    frameInfoRect.move(0, FPS_INDICATOR_HEIGHT);

    clearRectWithColor(frameInfoRect, 1, 1, 1, 1);
    ratio = (swappedCounter + 1.0) / COLLECTION_SWAPPED_COUNTER_MODULE;

    frameInfoRect.setWidth(frameInfoRect.width() * ratio);
    clearRectWithColor(frameInfoRect, 0, 1, 0, 1);
}

void GLWebViewState::clearRectWithColor(const IntRect& rect, float r, float g,
                                      float b, float a)
{
    glScissor(rect.x(), rect.y(), rect.width(), rect.height());
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
