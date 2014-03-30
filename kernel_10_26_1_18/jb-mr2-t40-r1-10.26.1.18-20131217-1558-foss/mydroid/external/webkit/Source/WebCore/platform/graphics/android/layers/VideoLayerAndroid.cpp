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

#include "config.h"
#include "VideoLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "DrawQuadData.h"
#include "ShaderProgram.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <wtf/CurrentTime.h>

static const int iconSize = 64;
static const float playPauseShowTime = 1;
static const float spinnerRate = 0.75;

class VideoIcons {
public:
    static VideoIcons* instance()
    {
        if (!s_instance)
            s_instance = new VideoIcons();
        return s_instance;
    }

    static void cleanupGLResources()
    {
        delete s_instance;
        s_instance = 0;
    }

    static void didResetRenderingContext()
    {
        // The textures were lost. Abandon them and force a reload.
        if (s_instance) {
            s_instance->m_playTextureId = 0;
            s_instance->m_pauseTextureId = 0;
            s_instance->m_spinnerOuterTextureId = 0;
            s_instance->m_spinnerInnerTextureId = 0;
            delete s_instance;
            s_instance = 0;
        }
    }

    GLuint playTextureId() const { return m_playTextureId; }
    GLuint pauseTextureId() const { return m_pauseTextureId; }
    GLuint spinnerInnerTextureId() const { return m_spinnerInnerTextureId; }
    GLuint spinnerOuterTextureId() const { return m_spinnerOuterTextureId; }

private:
    VideoIcons()
        : m_playTextureId(createTextureFromIcon(RenderSkinMediaButton::PLAY))
        , m_pauseTextureId(createTextureFromIcon(RenderSkinMediaButton::PAUSE))
        , m_spinnerOuterTextureId(createTextureFromIcon(RenderSkinMediaButton::SPINNER_OUTER))
        , m_spinnerInnerTextureId(createTextureFromIcon(RenderSkinMediaButton::SPINNER_INNER))
    {}

    ~VideoIcons()
    {
        glDeleteTextures(1, &m_playTextureId);
        glDeleteTextures(1, &m_pauseTextureId);
        glDeleteTextures(1, &m_spinnerOuterTextureId);
        glDeleteTextures(1, &m_spinnerInnerTextureId);
    }

    static GLuint createTextureFromIcon(RenderSkinMediaButton::MediaButton buttonType)
    {
        IntRect iconRect(0, 0, iconSize, iconSize);

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, iconSize, iconSize, 4 * iconSize);
        bitmap.allocPixels();
        bitmap.eraseColor(0);

        SkCanvas canvas(bitmap);
        canvas.drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
        RenderSkinMediaButton::Draw(&canvas, iconRect, buttonType, Color());

        GLuint texture;
        glGenTextures(1, &texture);

        GLUtils::createTextureWithBitmap(texture, bitmap);
        return texture;
    }

    GLuint m_playTextureId;
    GLuint m_pauseTextureId;
    GLuint m_spinnerOuterTextureId;
    GLuint m_spinnerInnerTextureId;

    static VideoIcons* s_instance;
};

VideoIcons* VideoIcons::s_instance;

namespace WebCore {

VideoLayerAndroid::VideoLayerAndroid()
    : LayerAndroid((RenderLayer*)0)
    , m_icon(NoIcon)
    , m_isBuffering(false)
{}

VideoLayerAndroid::VideoLayerAndroid(const VideoLayerAndroid& layer)
    : LayerAndroid(layer)
    , m_videoSurface(layer.m_videoSurface)
    , m_icon(layer.m_icon)
    , m_iconTimestamp(layer.m_iconTimestamp)
    , m_isBuffering(layer.m_isBuffering)
{}

void VideoLayerAndroid::didAttachToView(android::WebViewCore* webViewCore)
{
    MutexLocker lock(m_webViewCoreLock);
    m_webViewCore = webViewCore;
}

void VideoLayerAndroid::didDetachFromView()
{
    MutexLocker lock(m_webViewCoreLock);
    m_webViewCore = 0;
}

void VideoLayerAndroid::setVideoSurface(VideoSurface* videoSurface)
{
    m_videoSurface = videoSurface;
}

void VideoLayerAndroid::showIcon(Icon icon)
{
    m_icon = icon;
    m_iconTimestamp = currentTime();
}

void VideoLayerAndroid::setBuffering(bool isBuffering)
{
    m_isBuffering = isBuffering;
}

void VideoLayerAndroid::invalidate()
{
    MutexLocker lock(m_webViewCoreLock);
    if (!m_webViewCore)
        return;

    m_webViewCore->viewInvalidateLayer(uniqueId());
}

bool VideoLayerAndroid::drawGL(bool layerTilesDisabled)
{
    if (!m_videoSurface.get())
        return false;

    ShaderProgram* shader = TilesManager::instance()->shader();
    VideoIcons* icons = VideoIcons::instance();
    double now = currentTime();

    float textureMatrix[16];
    GLuint textureId = m_videoSurface->lockTextureForCurrentFrame(textureMatrix);
    if (!textureId)
        return false;

    SkRect layerRect = SkRect::MakeSize(getSize());
    shader->drawVideoLayerQuad(m_drawTransform, textureMatrix, layerRect, textureId);
    m_videoSurface->unlockTexture(textureId);

    if (m_isBuffering) {
        double rotation = 360 * fmod(now * spinnerRate, 1);
        drawIcon(icons->spinnerOuterTextureId(), layerRect, 1, 1, rotation);
        drawIcon(icons->spinnerInnerTextureId(), layerRect, 1, 1, -rotation);
        return true;
    }

    if (m_icon == NoIcon)
        return false;

    float iconTimeLeft = playPauseShowTime - now + m_iconTimestamp;
    if (iconTimeLeft <= 0) {
        m_icon = NoIcon;
        return false;
    }

    float scale = iconTimeLeft / playPauseShowTime;
    drawIcon(m_icon == PlayIcon ? icons->playTextureId() : icons->pauseTextureId(),
             layerRect, 1 - scale / 2, scale);

    return true;
}

void VideoLayerAndroid::drawIcon(GLuint textureId, const FloatRect& layerRect, float scale, float opacity, float rotateDegrees)
{
    if (layerRect.width() < iconSize * scale || layerRect.height() < iconSize * scale)
        return;

    ShaderProgram* shader = TilesManager::instance()->shader();
    FloatPoint center = layerRect.center();

    SkRect iconRect = SkRect::MakeXYWH(center.x() - iconSize / 2 * scale,
                                       center.y() - iconSize / 2 * scale,
                                       iconSize * scale, iconSize * scale);

    TransformationMatrix iconTransform = m_drawTransform;
    if (rotateDegrees) {
        iconTransform.translate(center.x(), center.y());
        iconTransform.rotate(rotateDegrees);
        iconTransform.translate(-center.x(), -center.y());
    }

    TextureQuadData iconQuad(textureId, GL_TEXTURE_2D, GL_LINEAR, LayerQuad, &iconTransform, &iconRect, opacity);
    shader->drawQuad(&iconQuad);
}

void VideoLayerAndroid::cleanupGLResources()
{
    VideoIcons::cleanupGLResources();
}

void VideoLayerAndroid::didResetRenderingContext()
{
    VideoIcons::didResetRenderingContext();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
