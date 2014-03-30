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

#ifndef EGLImageLayer_h
#define EGLImageLayer_h

#if USE(ACCELERATED_COMPOSITING)

#include "EGLImageBufferRing.h"
#include "LayerAndroid.h"
#include "PowerHints.h"
#include "WebViewCore.h"
#include <wtf/RefPtr.h>

namespace WTF {
template<unsigned Capacity> class DelegateThread;
}

namespace WebCore {

class EGLImageSurface;
class FPSTimer;
template <typename TimerFiredClass> class Timer;

class EGLImageLayer
    : public LayerAndroid
    , public android::WebViewCore::BackgroundModeListener
    , public EGLImageBufferRing::Client {

public:
    EGLImageLayer(RefPtr<EGLImageSurface>, const char* name);
    EGLImageLayer(const EGLImageLayer&);
    ~EGLImageLayer();

    virtual LayerAndroid* copy() const { return new EGLImageLayer(*this); }
    virtual SubclassType subclassType() const { return LayerAndroid::EGLImageLayer; }
    virtual bool needsIsolatedSurface() { return true; }
    virtual bool drawGL(bool layerTilesDisabled);
    virtual void didAttachToView(android::WebViewCore*);
    virtual void didDetachFromView();
    virtual void viewDidEnterBackgroundMode();
    virtual void viewDidExitBackgroundMode();
    virtual bool handleNeedsDisplay();
    virtual bool onNewFrontBufferReady();
    virtual void syncContents();
    virtual void viewInvalidate();

    bool needsBlendingLayer(float opacity) const;

protected:
    virtual void onDraw(SkCanvas* canvas, SkScalar opacity, android::DrawExtra* extra, PaintStyle style);

private:
    void submitBackBuffer(Timer<EGLImageLayer>* = 0);
    void submitBufferForComposition(PassOwnPtr<EGLImageBuffer>);

    RefPtr<EGLImageSurface> m_surface;
    RefPtr<EGLImageBufferRing> m_bufferRing;
    OwnPtr<Timer<EGLImageLayer> > m_syncTimer;
    SkRefPtr<android::WebViewCore> m_webViewCore;
    OwnPtr<FPSTimer> m_fpsTimer;
    RefPtr<android::PowerHints::EnableHighFPSScaling> m_highFPSScalingRequest;
    const bool m_handlesUpdatesManually;
    const bool m_isInverted;
    const bool m_hasAlpha;
    const bool m_hasPremultipliedAlpha;
    bool m_hasSkippedBackgroundUpdate;
};


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // EGLImageLayer_h
