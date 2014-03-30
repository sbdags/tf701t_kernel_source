/*
 * Copyright 2009,2010 The Android Open Source Project
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

#ifndef MediaPlayerPrivateAndroid_h
#define MediaPlayerPrivateAndroid_h

#if ENABLE(VIDEO)

class SkBitmap;

#include "MediaPlayerPrivate.h"
#include "MessageHandler.h"
#include "TimeRanges.h"
#include <SkRefCnt.h>
#include <utils/RefBase.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace android {

class MediaPlayer;
class WakeLock;
class IGraphicBufferProducer;

};

namespace WebCore {

class CopyVideoSurface;
class MappedTexture;
class VideoLayerAndroid;
class VideoSurface;

class MediaPlayerPrivateAndroid
    : public MediaPlayerPrivateInterface
    , public android::MessageHandler {

    class MediaManager;
    friend class MediaManager;

    class MediaPlayerListener;
    friend class MediaPlayerListener;

public:
    struct Attributes {
        Attributes()
            : duration(std::numeric_limits<float>::quiet_NaN())
            , hasAudio(false)
            , hasVideo(false)
            , canPause(false)
            , canSeekBackward(false)
            , canSeekForward(false)
            , canSeek(false)
            , hasSingleSecurityOrigin(false)
            , hasFileSource(false)
        {}

        IntSize videoSize;
        float duration;
        bool hasAudio : 1;
        bool hasVideo : 1;
        bool canPause : 1;
        bool canSeekBackward : 1;
        bool canSeekForward : 1;
        bool canSeek : 1;
        bool hasSingleSecurityOrigin : 1;
        bool hasFileSource : 1;
    };

    static MediaPlayerPrivateInterface* create(MediaPlayer* owner)
    {
        return new MediaPlayerPrivateAndroid(owner);
    }
    ~MediaPlayerPrivateAndroid();

    VideoSurface* videoSurface() const { return m_videoSurface.get(); }
    android::MediaPlayer* mediaPlayer() const { return m_player.get(); }
    int audioSessionId() const;
    const Attributes& attrs() const { return m_attrs; }
    int percentLoaded() const { return m_percentLoaded; }
    bool isBackgroundVideo() const;
    void overrideVideoSurfaceTexture(android::sp<android::IGraphicBufferProducer> gbp);

    // MediaPlayerPrivateInterface overrides.
    virtual void load(const String& url);
    virtual void cancelLoad();

    virtual PlatformMedia platformMedia() const;
    virtual PlatformVideoSurface* platformVideoSurface() const;
    virtual PlatformLayer* platformLayer() const;

    virtual bool requestPermissionToPlay();
    virtual void play();
    virtual void pause();

    virtual IntSize naturalSize() const { return m_attrs.videoSize; }

    virtual bool supportsFullscreen() const { return true; }
    virtual bool hasAudio() const;
    virtual bool hasVideo() const;

    virtual void setVisible(bool) {}

    virtual float duration() const { return m_attrs.duration; }

    virtual float currentTime() const { return m_currentTime; }
    virtual void seek(float time);
    virtual bool seeking() const;

    virtual void setRate(float) {}

    virtual bool paused() const { return !m_isPlaying; }

    virtual void setVolume(float);

    virtual MediaPlayer::NetworkState networkState() const { return m_networkState; }
    virtual MediaPlayer::ReadyState readyState() const { return m_readyState; }

    virtual float maxTimeSeekable() const;
    virtual PassRefPtr<TimeRanges> buffered() const;

    virtual unsigned bytesLoaded() const;

    virtual void setSize(const IntSize&) {}

    virtual void paint(GraphicsContext*, const IntRect&) {}

    virtual void paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r);

    virtual bool hasAvailableVideoFrame() const { return m_hasFirstFrame; }

    virtual bool supportsAcceleratedRendering() const { return true; }

    virtual bool hasSingleSecurityOrigin() const;

    virtual void setPrivateBrowsingMode(bool privateBrowsing) { m_privateBrowsing = privateBrowsing; }

    static void registerMediaEngine(MediaEngineRegistrar);
    static void stopBackgroundVideos();

private:
    MediaPlayerPrivateAndroid(MediaPlayer*);

    virtual void handleMessage(unsigned what, int, int, void*);

    void updateReadyState(MediaPlayer::ReadyState);
    void updateNetworkState(MediaPlayer::NetworkState);
    void updatePlaybackState(bool isPlaying);
    void parseMetadata();
    void updateDuration();
    void updateCurrentTime(bool notifyOwner = true);
    void updateSuspending();
    bool updateFetching();
    void updateHasSingleSecurityOrigin();
    bool parseTrackInfo();
    void syncLayer();
    void stayAwake(bool);

    enum TimerFireType { FirstTimerFire, RepeatTimerFire };
    void scheduleTrackingTimer(TimerFireType);
    void stopTrackingTimer();

    MediaPlayer* m_owner;
    MediaPlayerClient* m_mediaPlayerClient;
    android::sp<android::MediaPlayer> m_player;
    android::sp<VideoSurface> m_videoSurface;
    android::sp<MediaPlayerListener> m_listener;
    android::sp<android::IGraphicBufferProducer> m_overrideVideoSurfaceTexture;
    SkRefPtr<VideoLayerAndroid> m_videoLayer;
    OwnPtr<android::WakeLock> m_wakeLock;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;
    OwnPtr<CopyVideoSurface> m_copyVideoSurface;
    OwnPtr<MappedTexture> m_frameDecodeTexture;
    Attributes m_attrs;
    unsigned m_percentLoaded;
    float m_currentTime;
    bool m_isPlaying;
    bool m_hasFirstFrame;
    bool m_playbackComplete;
    bool m_privateBrowsing;
    bool m_fetchingSuspended;
    double m_trackingTimerEpoch;
};

} // namespace WebCore

#endif

#endif // MediaPlayerPrivateAndroid_h
