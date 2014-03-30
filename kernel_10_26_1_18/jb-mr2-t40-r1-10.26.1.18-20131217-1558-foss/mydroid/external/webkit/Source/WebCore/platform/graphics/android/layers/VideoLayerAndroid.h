/*
 * Copyright 2011 The Android Open Source Project
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

#ifndef VideoLayerAndroid_h
#define VideoLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "VideoSurface.h"
#include "WebViewCore.h"
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class VideoLayerAndroid : public LayerAndroid {
public:
    VideoLayerAndroid();
    explicit VideoLayerAndroid(const VideoLayerAndroid& layer);

    void setVideoSurface(VideoSurface*);
    enum Icon {
        NoIcon,
        PlayIcon,
        PauseIcon
    };
    void showIcon(Icon);
    void setBuffering(bool);
    void invalidate();

    virtual bool isVideo() const { return true; }
    virtual LayerAndroid* copy() const { return new VideoLayerAndroid(*this); }
    virtual bool needsIsolatedSurface() { return true; }
    virtual void didAttachToView(android::WebViewCore*);
    virtual void didDetachFromView();
    virtual bool drawGL(bool layerTilesDisabled);

    static void cleanupGLResources();
    static void didResetRenderingContext();

private:
    void drawIcon(GLuint textureId, const FloatRect& layerRect, float scale = 1, float opacity = 1, float rotateDegrees = 1);

    android::sp<VideoSurface> m_videoSurface;
    Icon m_icon;
    double m_iconTimestamp;
    bool m_isBuffering;
    SkRefPtr<android::WebViewCore> m_webViewCore;
    WTF::Mutex m_webViewCoreLock;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // VideoLayerAndroid_h
