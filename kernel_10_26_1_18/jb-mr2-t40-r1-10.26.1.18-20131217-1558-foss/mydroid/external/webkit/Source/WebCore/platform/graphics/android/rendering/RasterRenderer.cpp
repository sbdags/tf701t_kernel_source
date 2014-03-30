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

#define LOG_TAG "RasterRenderer"
#define LOG_NDEBUG 1

#include "config.h"
#include "RasterRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLUtils.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "Tile.h"
#include "TilesManager.h"

namespace WebCore {

RasterRenderer::RasterRenderer()
    : m_rendererContext(GLContext::create(WebCore::ResourceLimits::System))
{
    // The context is in order to be able to read EGL images with GL.

    // TODO: Failure to create a context is not considered an error at the moment. It will just make
    // the EGL texture creation fail.
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("RasterRenderer");
#endif
}

RasterRenderer::~RasterRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("RasterRenderer");
#endif
}

void RasterRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    TRACE_METHOD();

    IntSize tileSize = TilesManager::tileSize();

    if (!m_renderBuffer)
        m_renderBuffer = MappedTexture::create(ResourceLimits::System, tileSize, MappedTexture::HasAlpha, MappedTexture::WriteUsingSoftware);

    if (!m_renderBuffer)
        return;

    SkBitmap bitmap;
    if (!m_renderBuffer->lockBufferForWriting(&bitmap))
        return;

    Color background = renderInfo.background;

    bitmap.setIsOpaque(!background.hasAlpha());
    bitmap.eraseARGB(background.alpha(), background.red(), background.green(), background.blue());

    SkDevice* device = new SkDevice(bitmap);
    canvas->setDevice(device);
    device->unref();
}

void RasterRenderer::renderingComplete(SkCanvas*)
{
    if (m_renderBuffer)
        m_renderBuffer->unlockBuffer();
}

void RasterRenderer::deviceCheckForAlphaAndPureColor(TileContentHints& hints, SkCanvas* canvas)
{
    // base renderer may have already determined isPureColor, so only do the
    // brute force check if needed
    if (hints.isPureColor)
        return;

    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
    ASSERT(static_cast<unsigned>(bitmap.width()) == TilesManager::tileWidth());
    ASSERT(static_cast<unsigned>(bitmap.height()) == TilesManager::tileHeight());

    hints.hasAlpha = !bitmap.isOpaque();
    hints.isPureColor = false;
    hints.pureColor = Color(Color::transparent);

    SkAutoLockPixels autoLockPixels(bitmap);
    // From now on we will compare two pixels at a time with the first two
    // pixels. Step one is to make sure the first two pixels are the same.
    const uint32_t* const bitmapPixels = static_cast<const uint32_t*>(bitmap.getPixels());
    if (bitmapPixels[0] != bitmapPixels[1])
        return;

    // We divide the tile into clusters of 8 rows, and then analyze the
    // clusters in a pseudo-random order. This allows us get to an early exit
    // more quickly while still getting most the cache advantages of locality.
    const unsigned clusterHeight = 8;
    ASSERT(TilesManager::tileHeight() % clusterHeight == 0);
    const unsigned clusterCount = TilesManager::tileHeight() / clusterHeight;
    const unsigned firstClusterIndex = clusterCount / 4;
    const unsigned clusterIncrement = 11;
    // Since clusterIncrement is prime, as long as it does not divide
    // clusterCount we're guaranteed a full period over clusterCount.
    ASSERT(clusterCount % clusterIncrement != 0);

    const uint8_t* const imageData = static_cast<const uint8_t*>(bitmap.getPixels());
    // We analyze the bitmap as rows of uint64_t's, and check 64 pixels at a
    // time before trying an early exit.
    ASSERT(TilesManager::tileWidth() % 64 == 0);
    const unsigned rowSize = TilesManager::tileWidth() / 2;
    const unsigned runSize = 64 / 2;

    uint64_t andReduction = -1;
    uint64_t orReduction = 0;

    unsigned clusterIndex = firstClusterIndex;
    do {
        for (unsigned y = clusterIndex * clusterHeight; y < (1 + clusterIndex) * clusterHeight; y++) {
            const uint64_t* const row = reinterpret_cast<const uint64_t*>(&imageData[y * bitmap.rowBytes()]);
            for (unsigned x = 0; x < rowSize; x += runSize) {
                for (unsigned offset = 0; offset < runSize; offset++) {
                    // Prime the cache with a preload, this triples perf.
                    asm ("PLD [%0, #128]"::"r" (&row[x + offset]));
                    uint64_t value = row[x + offset];
                    andReduction &= value;
                    orReduction |= value;
                }
                if (andReduction != orReduction)
                    return;
            }
        }
        clusterIndex = (clusterIndex + clusterIncrement) % clusterCount;
    } while (clusterIndex != firstClusterIndex);

    hints.isPureColor = true;
    hints.pureColor = Color(imageData[0], imageData[1], imageData[2], imageData[3]);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
