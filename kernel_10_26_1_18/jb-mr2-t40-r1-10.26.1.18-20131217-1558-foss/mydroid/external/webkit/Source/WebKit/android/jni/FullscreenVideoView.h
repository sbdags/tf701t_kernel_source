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

#ifndef FullscreenVideoView_h
#define FullscreenVideoView_h

#include "IntSize.h"
#include "MessageHandler.h"
#include <jni.h>
#include <utils/RefBase.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class HTMLVideoElement;
class MediaPlayerClient;
class MediaPlayerPrivateAndroid;

};

namespace android {

class MediaPlayer;
class IGraphicBufferProducer;

class FullscreenVideoClient {
public:
    virtual void onReadyStateChanged(const WebCore::MediaPlayerPrivateAndroid*) {}
    virtual void onPlaybackStateChanged(const WebCore::MediaPlayerPrivateAndroid*) {}
    virtual void onVideoSizeChanged(const WebCore::MediaPlayerPrivateAndroid*) {}
    virtual void onDownloadProgress(const WebCore::MediaPlayerPrivateAndroid*) {}
    virtual void onReset(const WebCore::MediaPlayerPrivateAndroid*) {}
    virtual ~FullscreenVideoClient() {}
};

class FullscreenVideoView
    : public MessageHandler
    , public FullscreenVideoClient {

    WTF_MAKE_NONCOPYABLE(FullscreenVideoView);
    struct JavaGlue;

public:
    class JNICallbacks;
    friend class JNICallbacks;

    static void enterFullscreenMode(WebCore::HTMLVideoElement*, jobject webViewClassic);
    static void exitFullscreenMode(WebCore::HTMLVideoElement*);
    static WebCore::MediaPlayerClient* currentFullscreenPlayer();
    static FullscreenVideoClient* client(const WebCore::MediaPlayerClient*);

private:
    FullscreenVideoView(WebCore::HTMLVideoElement*, jobject webViewClassic);
    ~FullscreenVideoView();

    // WebKit thread.
    virtual void handleMessage(unsigned what, int, int, void*);
    virtual void onReadyStateChanged(const WebCore::MediaPlayerPrivateAndroid*);
    virtual void onPlaybackStateChanged(const WebCore::MediaPlayerPrivateAndroid*);
    virtual void onVideoSizeChanged(const WebCore::MediaPlayerPrivateAndroid*);
    virtual void onDownloadProgress(const WebCore::MediaPlayerPrivateAndroid*);
    virtual void onReset(const WebCore::MediaPlayerPrivateAndroid*);
    void onExitFullscreen();

    // UI thread.
    void play();
    void pause();
    void seek(int position);
    int getCurrentTime();
    int getDuration();
    void exitFullscreen();
    void releasePointer();

    void setVideoSurfaceTexture(sp<IGraphicBufferProducer> gbp);

    RefPtr<WebCore::HTMLVideoElement> m_videoElement;
    sp<IGraphicBufferProducer> m_videoSurfaceTexture;
    WTF::Mutex m_videoSurfaceLock;
    WTF::ThreadCondition m_condition;
    sp<MediaPlayer> m_mediaPlayer;
    WTF::Mutex m_mediaPlayerLock;
    bool m_hasSentPrepared;
    OwnPtr<JavaGlue> m_glue;
};

} // namespace android

#endif // FullscreenVideoView_h
