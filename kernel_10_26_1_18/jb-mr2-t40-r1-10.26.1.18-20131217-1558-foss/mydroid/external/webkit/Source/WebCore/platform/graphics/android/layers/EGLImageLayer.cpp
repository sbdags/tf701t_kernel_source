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
#include "EGLImageLayer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "DrawQuadData.h"
#include "EGLImage.h"
#include "EGLImageSurface.h"
#include "FPSTimer.h"
#include "LayerContent.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "TilesManager.h"
#include "Timer.h"
#include <JNIUtility.h>
#include <wtf/DelegateThread.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

EGLImageLayer::EGLImageLayer(RefPtr<EGLImageSurface> surface, const char* name)
    : LayerAndroid(reinterpret_cast<RenderLayer*>(0))
    , m_surface(surface)
    , m_fpsTimer(FPSTimer::createIfNeeded(name))
    , m_handlesUpdatesManually(surface->supportsQuadBuffering())
    , m_isInverted(surface->isInverted())
    , m_hasAlpha(surface->hasAlpha())
    , m_hasPremultipliedAlpha(surface->hasPremultipliedAlpha())
    , m_hasSkippedBackgroundUpdate(false)
{
}

EGLImageLayer::EGLImageLayer(const EGLImageLayer& layer)
    : LayerAndroid(layer)
    , m_bufferRing(layer.m_surface->bufferRing())
    , m_handlesUpdatesManually(layer.m_handlesUpdatesManually)
    , m_isInverted(layer.m_isInverted)
    , m_hasAlpha(layer.m_hasAlpha)
    , m_hasPremultipliedAlpha(layer.m_hasPremultipliedAlpha)
    , m_hasSkippedBackgroundUpdate(false)
{
}

EGLImageLayer::~EGLImageLayer()
{
    if (m_webViewCore)
        m_webViewCore->removeBackgroundModeListener(this);

    if (m_surface)
        m_surface->bufferRing()->setClient(0);
}

bool EGLImageLayer::needsBlendingLayer(float opacity) const
{
    if (opacity == 1)
        return false;

    if (!content() || content()->isEmpty())
        return false;

    return true;
}

class FinishedUsingFrontBuffer : public ShaderProgram::FinishedDrawingCallback {
public:
    FinishedUsingFrontBuffer(EGLImageBufferRing* bufferRing, EGLImageBuffer* frontBuffer, GLuint textureID)
        : m_bufferRing(bufferRing)
        , m_frontBuffer(frontBuffer)
        , m_textureID(textureID)
    {}

    virtual void didFinishDrawing()
    {
        m_bufferRing->unlockFrontBufferGL(m_frontBuffer, m_textureID);
    }

private:
    EGLImageBufferRing* m_bufferRing;
    EGLImageBuffer* m_frontBuffer;
    GLuint m_textureID;
};

bool EGLImageLayer::drawGL(bool layerTilesDisabled)
{
    if (!layerTilesDisabled) {
        GLuint textureID;
        if (EGLImageBuffer* frontBuffer = m_bufferRing->lockFrontBufferForReadingGL(&textureID)) {
            SkRect geometry = SkRect::MakeSize(getSize());
            if (m_isInverted)
                std::swap(geometry.fTop, geometry.fBottom);

            TextureQuadData::ContentFlags contentFlags = TextureQuadData::CanDeferRendering;
            if (!m_hasAlpha)
                contentFlags |= TextureQuadData::HasNoAlpha;
            else if (!m_hasPremultipliedAlpha)
                contentFlags |= TextureQuadData::AlphaNotPremultiplied;

            TextureQuadData data(textureID, GL_TEXTURE_2D, GL_LINEAR, LayerQuad,
                                 drawTransform(), &geometry, drawOpacity(), contentFlags);

            OwnPtr<FinishedUsingFrontBuffer> finishedUsingFrontBuffer =
                adoptPtr(new FinishedUsingFrontBuffer(m_bufferRing.get(), frontBuffer, textureID));

            TilesManager::instance()->shader()->drawQuad(&data, finishedUsingFrontBuffer.release());
        }
    }

    return LayerAndroid::drawGL(layerTilesDisabled);
}

void EGLImageLayer::onDraw(SkCanvas* canvas, SkScalar opacity, android::DrawExtra* extra, PaintStyle style)
{
    bool usingBlendingLayer = needsBlendingLayer(opacity);
    if (usingBlendingLayer) {
        const SkRect layerBounds = SkRect::MakeSize(getSize());
        canvas->saveLayerAlpha(&layerBounds, SkScalarRound(opacity * 255));
        opacity = SK_Scalar1;
    }

    LayerAndroid::onDraw(canvas, opacity, extra, style);

    SkBitmap bitmap;
    bool premultiplyAlpha = m_hasAlpha && !m_hasPremultipliedAlpha;
    bitmap.setIsOpaque(!m_hasAlpha);
    if (EGLImageBuffer* frontBuffer = m_bufferRing->lockFrontBufferForReading(&bitmap, premultiplyAlpha)) {
        const int surfaceOpacity = SkScalarRound(opacity * 255);
        SkPaint paint;
        if (surfaceOpacity < 255)
            paint.setAlpha(surfaceOpacity);

        // Draw the bitmap onto the screen upside-down.
        SkRect sourceRect = SkRect::MakeWH(bitmap.width(), bitmap.height());
        SkRect destRect = SkRect::MakeSize(getSize());
        SkMatrix matrix;
        matrix.setRectToRect(sourceRect, destRect, SkMatrix::kFill_ScaleToFit);
        if (m_isInverted) {
            matrix.postScale(1, -1);
            matrix.postTranslate(0, destRect.height());
        }
        canvas->drawBitmapMatrix(bitmap, matrix, &paint);

        m_bufferRing->unlockFrontBuffer(frontBuffer);
    }

    if (usingBlendingLayer)
        canvas->restore();
}

void EGLImageLayer::didAttachToView(android::WebViewCore* webViewCore)
{
    if (!m_handlesUpdatesManually)
        return;

    m_webViewCore = webViewCore;
    if (!m_webViewCore)
        return;

    if (!m_webViewCore->isInBackground()) {
        m_highFPSScalingRequest = android::PowerHints::requestHighFPSScaling();
        m_surface->updateBackgroundStatus(false);
    } else
        m_surface->updateBackgroundStatus(true);

    m_webViewCore->addBackgroundModeListener(this);

    m_surface->bufferRing()->setClient(this);
}

void EGLImageLayer::didDetachFromView()
{
    if (!m_handlesUpdatesManually)
        return;

    m_surface->bufferRing()->setClient(0);

    if (m_webViewCore) {
        m_webViewCore->removeBackgroundModeListener(this);
        m_webViewCore = 0;
    }

    m_highFPSScalingRequest.clear();
    m_surface->deleteFreeBuffers();

    m_surface->didDetachFromView();
}

void EGLImageLayer::viewDidEnterBackgroundMode()
{
    ASSERT(m_handlesUpdatesManually);
    ASSERT(m_webViewCore);
    ASSERT(m_webViewCore->isInBackground());
    ASSERT(!m_hasSkippedBackgroundUpdate);

    m_highFPSScalingRequest.clear();
    m_surface->deleteFreeBuffers();
    m_surface->updateBackgroundStatus(true);
}

void EGLImageLayer::viewDidExitBackgroundMode()
{
    ASSERT(m_handlesUpdatesManually);
    ASSERT(m_webViewCore);
    ASSERT(!m_webViewCore->isInBackground());

    m_highFPSScalingRequest = android::PowerHints::requestHighFPSScaling();

    if (m_hasSkippedBackgroundUpdate) {
        submitBackBuffer();
        m_hasSkippedBackgroundUpdate = false;
    }

    m_surface->updateBackgroundStatus(false);
}

bool EGLImageLayer::handleNeedsDisplay()
{
    if (!m_handlesUpdatesManually) {
        // Let GraphicsLayerAndroid call syncContents and handle the update.
        return false;
    }

    if (!m_webViewCore)
        return true;

    if (m_webViewCore->isInBackground()) {
        m_hasSkippedBackgroundUpdate = true;
        return true;
    }

    if (!m_syncTimer)
        m_syncTimer = new Timer<EGLImageLayer>(this, &EGLImageLayer::submitBackBuffer);

    if (!m_syncTimer->isActive())
        m_syncTimer->startOneShot(0);

    return true;
}

void EGLImageLayer::submitBackBuffer(Timer<EGLImageLayer>*)
{
    ASSERT(m_handlesUpdatesManually);

    if (!m_webViewCore) {
        // We got detached from the view while an update was scheduled.
        return;
    }

    if (m_webViewCore->isInBackground()) {
        m_hasSkippedBackgroundUpdate = true;
        return;
    }

    if (!EGLImageSurface::isQuadBufferingDisabled())
        m_surface->submitBackBuffer();
    else {
        m_surface->swapBuffers();
        m_webViewCore->viewInvalidateLayer(uniqueId());
    }

    if (m_fpsTimer)
        m_fpsTimer->frameComplete(m_surface->size());
}

void EGLImageLayer::viewInvalidate()
{
    if (m_webViewCore)
        m_webViewCore->viewInvalidateLayer(uniqueId());
}

class DidInvalidateEGLImageLayerCallback : public android::WebViewCore::DidInvalidateLayerCallback {
public:
    DidInvalidateEGLImageLayerCallback(PassRefPtr<EGLImageBufferRing> bufferRing)
        : m_bufferRing(bufferRing)
    {}
    virtual void didInvalidateLayer() { m_bufferRing->commitStagedBuffer(); }
private:
    RefPtr<EGLImageBufferRing> m_bufferRing;
};

bool EGLImageLayer::onNewFrontBufferReady()
{
    // We unregister ourselves from the buffer ring before clearing m_webViewCore.
    ASSERT(m_webViewCore);

    m_webViewCore->viewInvalidateLayer(uniqueId(), adoptPtr(new DidInvalidateEGLImageLayerCallback(m_surface->bufferRing())));

    // False indicates the buffer ring should NOT commit the new staged buffer.
    // We will commit the buffer on the UI thread after invalidating the layer.
    return false;
}

void EGLImageLayer::syncContents()
{
    m_surface->swapBuffers();

    if (m_fpsTimer)
        m_fpsTimer->frameComplete(m_surface->size());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
