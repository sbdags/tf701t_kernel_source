/*
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
#include "FullscreenVideoView.h"

#include "GLUtils.h"
#include "HTMLVideoElement.h"
#include "MediaPlayerPrivateAndroid.h"
#include "VideoSurface.h"
#include "WebViewCore.h"
#include <android_runtime/android_view_Surface.h>
#include <gui/Surface.h>
#include <JNIHelp.h>
#include <JNIUtility.h>
#include <media/mediaplayer.h>

static const char* fullscreenVideoClassName = "android/webkit/HTML5VideoFullScreen";

static android::FullscreenVideoView* currentFullscreenView;

enum MediaPlayerMessages {
    Play,
    Pause,
    Seek,
    ExitFullscreen,
    SetVideoSurfaceTexture,
    ReleasePointer
};

namespace android {

struct FullscreenVideoView::JavaGlue {
    jmethodID newInstance;
    jmethodID onPrepared;
    jmethodID onReset;
    jmethodID setPlaying;
    jmethodID setWaiting;
    jmethodID setVideoSize;
    jmethodID setPercentLoaded;
    jmethodID exitFullscreen;
    jobject javaInstance;
};

void FullscreenVideoView::enterFullscreenMode(WebCore::HTMLVideoElement* videoElement, jobject webViewClassic)
{
    if (currentFullscreenView) {
        if (currentFullscreenView->m_videoElement == videoElement)
            return;
        currentFullscreenView->m_videoElement->webkitExitFullscreen();
        ASSERT(!currentFullscreenView);
    }
    currentFullscreenView = new FullscreenVideoView(videoElement, webViewClassic);
    WebCore::MediaPlayerPrivateAndroid::stopBackgroundVideos();
}

void FullscreenVideoView::exitFullscreenMode(WebCore::HTMLVideoElement* videoElement)
{
    if (!currentFullscreenView || currentFullscreenView->m_videoElement != videoElement) {
        ASSERT_NOT_REACHED();
        return;
    }
    currentFullscreenView->onExitFullscreen();
    currentFullscreenView = 0;
}

WebCore::MediaPlayerClient* FullscreenVideoView::currentFullscreenPlayer()
{
    if (!currentFullscreenView)
        return 0;
    return static_cast<MediaPlayerClient*>(currentFullscreenView->m_videoElement.get());
}

FullscreenVideoClient* FullscreenVideoView::client(const WebCore::MediaPlayerClient* mediaPlayerClient)
{
    if (currentFullscreenView
        && static_cast<MediaPlayerClient*>(currentFullscreenView->m_videoElement.get()) == mediaPlayerClient)
        return currentFullscreenView;

    static FullscreenVideoClient* nullClient;
    if (!nullClient)
        nullClient = new FullscreenVideoClient();
    return nullClient;
}

FullscreenVideoView::FullscreenVideoView(WebCore::HTMLVideoElement* videoElement, jobject webViewClassic)
    : m_videoElement(videoElement)
    , m_hasSentPrepared(false)
    , m_glue(0)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env) {
        ASSERT_NOT_REACHED();
        sendMessage(ReleasePointer);
        return;
    }

    jclass clazz = env->FindClass(fullscreenVideoClassName);
    m_glue.set(new JavaGlue());
    m_glue->newInstance = env->GetMethodID(clazz, "<init>", "(ILandroid/webkit/WebViewClassic;)V");
    m_glue->onPrepared = env->GetMethodID(clazz, "onPrepared", "(IZZZ)V");
    m_glue->onReset = env->GetMethodID(clazz, "onReset", "()V");
    m_glue->setPlaying = env->GetMethodID(clazz, "setPlaying", "(Z)V");
    m_glue->setWaiting = env->GetMethodID(clazz, "setWaiting", "(Z)V");
    m_glue->setVideoSize = env->GetMethodID(clazz, "setVideoSize", "(II)V");
    m_glue->setPercentLoaded = env->GetMethodID(clazz, "setPercentLoaded", "(I)V");
    m_glue->exitFullscreen = env->GetMethodID(clazz, "exitFullscreen", "()V");

    jobject localInstance = env->NewObject(clazz, m_glue->newInstance, this, webViewClassic);
    m_glue->javaInstance = env->NewGlobalRef(localInstance);

    if (m_videoElement->platformMedia().type == PlatformMedia::AndroidMediaPlayerType) {
        WebCore::MediaPlayerPrivateAndroid* player = m_videoElement->platformMedia().media.androidMediaPlayer;
        onDownloadProgress(player);
        onVideoSizeChanged(player);
        onPlaybackStateChanged(player);
        onReadyStateChanged(player);
    }
}

FullscreenVideoView::~FullscreenVideoView()
{
    if (currentFullscreenView == this)
        m_videoElement->webkitExitFullscreen();

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (env && m_glue)
        env->DeleteGlobalRef(m_glue->javaInstance);
}

void FullscreenVideoView::handleMessage(unsigned what, int arg1, int, void*)
{
    switch (what) {

    case Play: {
        if (currentFullscreenView != this)
            return;
        m_videoElement->play(true);
        return;
    }

    case Pause: {
        if (currentFullscreenView != this)
            return;
        m_videoElement->pause(true);
        return;
    }

    case Seek: {
        if (currentFullscreenView != this)
            return;
        WebCore::ExceptionCode ec;
        m_videoElement->setCurrentTime(arg1 / 1000.0f, ec);
        return;
    }

    case ReleasePointer: {
        delete this;
        return;
    }

    case SetVideoSurfaceTexture: {
        MutexLocker lock(m_videoSurfaceLock);
        if (m_videoElement->platformMedia().type == PlatformMedia::AndroidMediaPlayerType) {
            WebCore::MediaPlayerPrivateAndroid* player = m_videoElement->platformMedia().media.androidMediaPlayer;
            player->overrideVideoSurfaceTexture(m_videoSurfaceTexture);
        }
        m_videoSurfaceTexture = 0;
        m_condition.signal();
        return;
    }

    default: {
        ASSERT_NOT_REACHED();
        return;
    }

    };
}

void FullscreenVideoView::onReadyStateChanged(const WebCore::MediaPlayerPrivateAndroid* player)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    if (!m_mediaPlayer.get() && player->readyState() >= WebCore::MediaPlayer::HaveMetadata) {
        MutexLocker lock(m_mediaPlayerLock);
        m_mediaPlayer = player->mediaPlayer();
    }

    if (!m_hasSentPrepared && player->readyState() >= WebCore::MediaPlayer::HaveMetadata) {
        const WebCore::MediaPlayerPrivateAndroid::Attributes& attrs = player->attrs();
        env->CallVoidMethod(m_glue->javaInstance, m_glue->onPrepared, player->audioSessionId(), attrs.canPause, attrs.canSeekBackward, attrs.canSeekForward);
        m_hasSentPrepared = true;
    }

    env->CallVoidMethod(m_glue->javaInstance, m_glue->setWaiting, player->readyState() <= WebCore::MediaPlayer::HaveCurrentData);
}

void FullscreenVideoView::onPlaybackStateChanged(const WebCore::MediaPlayerPrivateAndroid* player)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->setPlaying, !player->paused());
}

void FullscreenVideoView::onVideoSizeChanged(const WebCore::MediaPlayerPrivateAndroid* player)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->setVideoSize, player->naturalSize().width(), player->naturalSize().height());
}

void FullscreenVideoView::onDownloadProgress(const WebCore::MediaPlayerPrivateAndroid* player)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->setPercentLoaded, player->percentLoaded());
}

void FullscreenVideoView::onReset(const WebCore::MediaPlayerPrivateAndroid*)
{
    m_hasSentPrepared = false;

    {
        MutexLocker lock(m_mediaPlayerLock);
        m_mediaPlayer = 0;
    }

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (env && m_glue)
        env->CallVoidMethod(m_glue->javaInstance, m_glue->onReset);
}

void FullscreenVideoView::onExitFullscreen()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->exitFullscreen);
}

void FullscreenVideoView::play()
{
    sendMessage(Play);
}

void FullscreenVideoView::pause()
{
    sendMessage(Pause);
}

void FullscreenVideoView::seek(int position)
{
    sendMessage(Seek, position);
}

int FullscreenVideoView::getCurrentTime()
{
    MutexLocker lock(m_mediaPlayerLock);
    if (!m_mediaPlayer.get())
        return 0;
    int time;
    if (m_mediaPlayer->getCurrentPosition(&time) != android::OK)
        return 0;
    return time;
}

int FullscreenVideoView::getDuration()
{
    MutexLocker lock(m_mediaPlayerLock);
    if (!m_mediaPlayer.get())
        return 0;
    int duration;
    if (m_mediaPlayer->getDuration(&duration) != android::OK)
        return 0;
    return duration;
}

void FullscreenVideoView::releasePointer()
{
    sendMessage(ReleasePointer);
}

void FullscreenVideoView::setVideoSurfaceTexture(sp<IGraphicBufferProducer> gbp) {
    MutexLocker lock(m_videoSurfaceLock);
    m_videoSurfaceTexture = gbp;
    sendMessage(SetVideoSurfaceTexture);
    do {
        m_condition.wait(m_videoSurfaceLock);
    } while (m_videoSurfaceTexture.get());
}

class FullscreenVideoView::JNICallbacks {
public:
    static void play(JNIEnv*, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            fullscreenVideoView->play();
    }

    static void pause(JNIEnv*, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            fullscreenVideoView->pause();
    }

    static void seek(JNIEnv*, jobject, jint pointer, jint position)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            fullscreenVideoView->seek(position);
    }

    static jint getCurrentTime(JNIEnv*, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            return fullscreenVideoView->getCurrentTime();
        return 0;
    }

    static jint getDuration(JNIEnv*, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            return fullscreenVideoView->getDuration();
        return 0;
    }

    static void releasePointer(JNIEnv*, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            fullscreenVideoView->releasePointer();
    }

    static void setSurface(JNIEnv* env, jobject, jint pointer, jobject jsurface)
    {
        sp<IGraphicBufferProducer> bufferProducer;
        if (jsurface) {
            sp<Surface> surface(android_view_Surface_getSurface(env, jsurface));
            if (surface.get()) {
                bufferProducer = surface->getIGraphicBufferProducer();
            }
        }
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer))
            fullscreenVideoView->setVideoSurfaceTexture(bufferProducer);
    }
    static void surfaceDestroyed(JNIEnv* env, jobject, jint pointer)
    {
        if (FullscreenVideoView* fullscreenVideoView = reinterpret_cast<FullscreenVideoView*>(pointer)) {
            // restore inline surfaceTexture
            fullscreenVideoView->setVideoSurfaceTexture(0);
        }
    }
};

static JNINativeMethod fullscreenVideoViewRegistration[] = {
    { "nativePlay", "(I)V",
        (void*) &FullscreenVideoView::JNICallbacks::play },
    { "nativePause", "(I)V",
        (void*) &FullscreenVideoView::JNICallbacks::pause },
    { "nativeSeekTo", "(II)V",
        (void*) &FullscreenVideoView::JNICallbacks::seek },
    { "nativeGetCurrentTime", "(I)I",
        (void*) &FullscreenVideoView::JNICallbacks::getCurrentTime },
    { "nativeGetDuration", "(I)I",
        (void*) &FullscreenVideoView::JNICallbacks::getDuration },
    { "nativeReleasePointer", "(I)V",
        (void*) &FullscreenVideoView::JNICallbacks::releasePointer },
    { "nativeSetSurface", "(ILandroid/view/Surface;)V",
        (void*) &FullscreenVideoView::JNICallbacks::setSurface },
    { "nativeSurfaceDestroyed", "(I)V",
        (void*) &FullscreenVideoView::JNICallbacks::surfaceDestroyed },
};

int registerFullscreenVideoView(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, fullscreenVideoClassName,
                                    fullscreenVideoViewRegistration,
                                    NELEM(fullscreenVideoViewRegistration));
}

} // namespace android
