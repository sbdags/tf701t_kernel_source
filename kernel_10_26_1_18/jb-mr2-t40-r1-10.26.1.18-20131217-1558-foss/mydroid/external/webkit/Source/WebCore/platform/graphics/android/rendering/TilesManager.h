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

#ifndef TilesManager_h
#define TilesManager_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "ShaderProgram.h"
#include "TexturesGeneratorList.h"
#include "TilesProfiler.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class OperationFilter;
class Tile;
class TileTexture;

class TilesManager {
public:
    // May only be called from the UI thread
    static TilesManager* instance();

    static bool hardwareAccelerationEnabled()
    {
        return gInstance != 0;
    }

    void flushPendingPaintTileBatches()
    {
        TexturesGeneratorList::instance()->flushPendingPaintTileBatches();
    }

    void commitPaintTileBatchIfNeeded()
    {
        TexturesGeneratorList::instance()->commitPaintTileBatchIfNeeded();
    }

    void removeOperationsForFilter(PassRefPtr<OperationFilter> filter)
    {
        TexturesGeneratorList::instance()->removeOperationsForFilter(filter);
    }

    void scheduleOperation(PassOwnPtr<PaintTileOperation> operation)
    {
        TexturesGeneratorList::instance()->scheduleOperation(operation);
    }

    ShaderProgram* shader() { return &m_shader; }

    void updateTilesIfContextVerified();
    void cleanupGLResources();

    void gatherTextures();
    bool layerTexturesRemain() { return m_layerTexturesRemain; }
    void gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                               int* nbLayerTextures, int* nbAllocatedLayerTextures);

    TileTexture* getAvailableTexture(TextureOwner*, bool isLayerTile);

    void printTextures();

    // m_highEndGfx is written/read only on UI thread, no need for a lock.
    void setHighEndGfx(bool highEnd);
    bool highEndGfx();

    int currentLayerTextureCount();
    void setCurrentTextureCount(size_t newTextureCount);
    void setCurrentLayerTextureCount(size_t newTextureCount);

    static unsigned tileWidth() { return 256; }
    static unsigned tileHeight() { return 256; }
    static IntSize tileSize() { return IntSize(tileWidth(), tileHeight()); }

    void deleteOldTextures();
    void deleteAllTextures();

    TilesProfiler* getProfiler()
    {
        return &m_profiler;
    }

    bool invertedScreen()
    {
        return m_invertedScreen;
    }

    void setInvertedScreen(bool invert)
    {
        m_invertedScreen = invert;
    }

    void setInvertedScreenContrast(float contrast)
    {
        m_shader.setContrast(contrast);
    }

    void setUseMinimalMemory(bool useMinimalMemory)
    {
        m_useMinimalMemory = useMinimalMemory;
    }

    bool useMinimalMemory()
    {
        return m_useMinimalMemory;
    }

    void setUseDoubleBuffering(bool useDoubleBuffering)
    {
        m_useDoubleBuffering = useDoubleBuffering;
    }
    bool useDoubleBuffering() { return m_useDoubleBuffering; }


    unsigned int incWebkitContentUpdates() { return m_webkitContentUpdates++; }

    void incContentUpdates() { m_contentUpdates++; }
    unsigned int getContentUpdates() { return m_contentUpdates; }
    void clearContentUpdates() { m_contentUpdates = 0; }

    void incDrawGLCount()
    {
        android::AutoMutex lock(m_drawGLCountLock);
        m_drawGLCount++;
    }

    unsigned long long getDrawGLCount()
    {
        android::AutoMutex lock(m_drawGLCountLock);
        return m_drawGLCount;
    }

private:
    TilesManager();

    void didResetRenderingContext();
    size_t maxTextureAllocation() const;


    WTF::Vector<TileTexture*> m_textures;
    WTF::Vector<TileTexture*> m_availableTextures;

    WTF::Vector<TileTexture*> m_tilesTextures;
    WTF::Vector<TileTexture*> m_availableTilesTextures;
    bool m_layerTexturesRemain;

    bool m_highEndGfx;

    bool m_invertedScreen;

    bool m_useMinimalMemory;

    bool m_useDoubleBuffering;
    unsigned int m_contentUpdates; // nr of successful tiled paints
    unsigned int m_webkitContentUpdates; // nr of paints from webkit

    static TilesManager* gInstance;

    ShaderProgram m_shader;

    TilesProfiler m_profiler;

    android::Mutex m_drawGLCountLock;
    unsigned long long m_drawGLCount;

    double m_lastTimeLayersUsed;

    EGLContext m_eglContext;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesManager_h
