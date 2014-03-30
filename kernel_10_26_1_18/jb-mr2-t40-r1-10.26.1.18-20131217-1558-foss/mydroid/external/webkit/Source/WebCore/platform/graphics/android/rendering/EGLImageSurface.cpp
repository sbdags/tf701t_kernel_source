/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "config.h"
#include "EGLImageSurface.h"

#if USE(ACCELERATED_COMPOSITING)

#include "EGLImageBufferRing.h"
#include <wtf/Vector.h>

#define LOG_TAG "EGLImageSurface"
#include <cutils/log.h>

enum {
    // Disable quad buffering if it would require more than 256 MB.
    MaxCombinedArea = 256 * 1024 * 1024 / 4 / 4
};

static bool isQuadBufferingDisabled;

static Vector<WebCore::EGLImageSurface*>& surfaces()
{
    DEFINE_STATIC_LOCAL(Vector<WebCore::EGLImageSurface*>, surfaces, ());
    return surfaces;
}

static void updateQuadBufferingState()
{
    size_t totalArea = 0;

    Vector<WebCore::EGLImageSurface*>& surfaces = ::surfaces();
    for (size_t i = 0; i < surfaces.size(); i++)
        totalArea += surfaces[i]->size().height() * surfaces[i]->size().width();

    bool shouldDisableQuadBuffering = totalArea > MaxCombinedArea;
    if (isQuadBufferingDisabled == shouldDisableQuadBuffering)
        return;

    isQuadBufferingDisabled = shouldDisableQuadBuffering;

    if (isQuadBufferingDisabled) {
        ALOGV("Disabling quad buffering to conserve memory");
        for (size_t i = 0; i < surfaces.size(); i++)
            surfaces[i]->deleteFreeBuffers();
    } else
        ALOGV("Resuming quad buffering");
}


namespace WebCore {

EGLImageSurface::EGLImageSurface(IntSize size)
    : m_size(size)
    , m_bufferRing(adoptRef(new EGLImageBufferRing()))
{
    surfaces().append(this);
    updateQuadBufferingState();
}

EGLImageSurface::~EGLImageSurface()
{
    Vector<EGLImageSurface*>& surfaces = ::surfaces();
    for (size_t i = 0; i < surfaces.size(); i++) {
        if (surfaces[i] == this) {
            surfaces.remove(i);
            break;
        }
    }

    updateQuadBufferingState();
}

bool EGLImageSurface::isQuadBufferingDisabled()
{
    return ::isQuadBufferingDisabled;
}

void EGLImageSurface::updateSize(IntSize newSize)
{
    m_size = newSize;
    updateQuadBufferingState();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
