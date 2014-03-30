/*
 * Copyright 2009, The Android Open Source Project
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
#include "MediaPlayerPrivateAndroid.h"

#if ENABLE(VIDEO)

#define LOG_TAG "MediaPlayerPrivateAndroid"

#include "AudioManager.h"
#include "BitmapImage.h"
#include "EGLFence.h"
#include "FullscreenVideoView.h"
#include "GraphicsContext.h"
#include "MappedTexture.h"
#include "MediaMetadata.h"
#include "MessageHandler.h"
#include "SkBitmapRef.h"
#include "VideoLayerAndroid.h"
#include "VideoSurface.h"
#include "WakeLock.h"
#include "WebCookieJar.h"
#include "WebViewCore.h"
#include <binder/Parcel.h>
#include <cutils/log.h>
#include <media/mediaplayer.h>
#include <media/stagefright/MediaErrors.h>
#include <wtf/CurrentTime.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>

// The spec says the position should update every 250 ms or less.
static const double trackingInterval = 0.25;
static const char* mediaPlayernterfaceToken = "android.media.IMediaPlayer";

enum MediaMessages {
    Prepared,
    FrameAvailable,
    PlaybackComplete,
    BufferingUpdate,
    SetVideoSize,
    UpdateCurrentTime,
    TrackingTimerFired,
    Pause,
    Info,
    Error
};

static android::WebViewCore* getWebViewCore(WebCore::MediaPlayer* owner)
{
    WebCore::FrameView* frameView = owner->mediaPlayerClient()->mediaPlayerOwningDocument()->view();
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(frameView);
    ASSERT(core);
    return core;
}

static jobject getAndroidContext(WebCore::MediaPlayer* owner)
{
    android::WebViewCore* core = getWebViewCore(owner);
    return core ? core->getContext() : 0;
}


namespace WebCore {

class MediaPlayerPrivateAndroid::MediaManager
    : public android::AudioManager::AudioFocusListener
    , public android::MessageHandler {

public:
    static bool lockAudio(MediaPlayerPrivateAndroid* player)
    {
        if (s_current)
            return true;

        jobject androidContext = getAndroidContext(player->m_owner);
        if (!androidContext) {
            ASSERT_NOT_REACHED();
            return false;
        }
        OwnPtr<MediaManager> instance(new MediaManager(androidContext));
        if (!instance->m_audioManager->requestAudioFocus(android::AudioManager::StreamMusic))
            return false;

        s_current = instance.leakPtr();
        return true;
    }

    static bool activate(MediaPlayerPrivateAndroid* player)
    {
        if (!lockAudio(player))
            return false;

        s_current->m_activeMedia.add(player);
        return true;
    }

    static void deactivate(MediaPlayerPrivateAndroid* player)
    {
        if (!s_current)
            return;

        s_current->m_activeMedia.remove(player);

        if (s_current->m_activeMedia.isEmpty()) {
            s_current->m_audioManager->abandonAudioFocus();
            delete s_current;
            s_current = 0;
        }
    }

    static bool isMediaSuspended()
    {
        return s_current && s_current->m_isMediaSuspended;
    }

    static void stopBackgroundVideos()
    {
        if (!s_current)
            return;

        Vector<MediaPlayerPrivateAndroid*> backgroundVideos;
        backgroundVideos.reserveInitialCapacity(s_current->m_activeMedia.size());

        HashSet<MediaPlayerPrivateAndroid*>::iterator iter = s_current->m_activeMedia.begin();
        for (; iter != s_current->m_activeMedia.end(); ++iter) {
            MediaPlayerPrivateAndroid* player = *iter;
            if (player->isBackgroundVideo())
                backgroundVideos.append(player);
        }

        for (size_t i = 0; i < backgroundVideos.size(); i++)
            backgroundVideos[i]->pause();
    }

private:
    MediaManager(jobject androidContext)
        : m_audioManager(new android::AudioManager(androidContext, this))
        , m_isMediaSuspended(false)
    {}

    virtual void onAudioFocusChange(android::AudioManager::AudioFocusChange focusChange)
    {
        sendMessage(focusChange);
    }

    virtual void handleMessage(unsigned what, int arg1, int arg2, void*)
    {
        switch (what) {
        case android::AudioManager::AudioFocusLoss: {
            // When the last media element is paused, this object will be deleted.
            Vector<MediaPlayerPrivateAndroid*> localMedia;
            copyToVector(m_activeMedia, localMedia);
            for (size_t i = 0; i < localMedia.size(); i++)
                localMedia[i]->pause();
            ASSERT(!s_current);
            return;
        }

        case android::AudioManager::AudioFocusLossTransient:
        case android::AudioManager::AudioFocusLossTransientCanDuck: {
            m_isMediaSuspended = true;
            updateSuspending();
            return;
        }

        case android::AudioManager::AudioFocusGain: {
            m_isMediaSuspended = false;
            updateSuspending();
            return;
        }
        }
    }

    void updateSuspending()
    {
        HashSet<MediaPlayerPrivateAndroid*>::iterator iter = m_activeMedia.begin();
        for (; iter != m_activeMedia.end(); ++iter)
            (*iter)->updateSuspending();
    }

    OwnPtr<android::AudioManager> m_audioManager;
    bool m_isMediaSuspended;
    HashSet<MediaPlayerPrivateAndroid*> m_activeMedia;

    static MediaManager* s_current;
};

MediaPlayerPrivateAndroid::MediaManager* MediaPlayerPrivateAndroid::MediaManager::s_current;

class MediaPlayerPrivateAndroid::MediaPlayerListener
    : public android::MediaPlayerListener
    , public VideoSurface::Listener {

public:
    MediaPlayerListener(MediaPlayerPrivateAndroid* owner)
        : m_owner(owner)
    {}

    // Called from the android media player's IPC thread.
    virtual void notify(int msg, int ext1, int ext2, const android::Parcel*)
    {
        MutexLocker lock(m_mutex);
        if (!m_owner)
            return;

        unsigned what;

        switch (msg) {
        case android::MEDIA_PREPARED: what = Prepared; break;
        case android::MEDIA_PLAYBACK_COMPLETE: what = PlaybackComplete; break;
        case android::MEDIA_BUFFERING_UPDATE: what = BufferingUpdate; break;
        case android::MEDIA_SET_VIDEO_SIZE: what = SetVideoSize; break;
        case android::MEDIA_INFO: what = Info; break;
        case android::MEDIA_ERROR: what = Error; break;
        default: return;
        }

        m_owner->sendMessage(what, ext1, ext2);
    }

    virtual void onFrameAvailable()
    {
        MutexLocker lock(m_mutex);
        if (!m_owner)
            return;

        m_owner->sendMessage(FrameAvailable);
        m_owner->m_videoLayer->invalidate();
    }

    void detach()
    {
        MutexLocker lock(m_mutex);
        m_owner = 0;
    }

private:
    WTF::Mutex m_mutex;
    MediaPlayerPrivateAndroid* m_owner;
};

MediaPlayerPrivateAndroid::MediaPlayerPrivateAndroid(MediaPlayer* owner)
    : m_owner(owner)
    , m_mediaPlayerClient(m_owner->mediaPlayerClient())
    , m_player(new android::MediaPlayer())
    , m_videoSurface(new VideoSurface())
    , m_listener(new MediaPlayerListener(this))
    , m_videoLayer(new VideoLayerAndroid())
    , m_readyState(MediaPlayer::HaveNothing)
    , m_networkState(MediaPlayer::Empty)
    , m_percentLoaded(0)
    , m_currentTime(0)
    , m_isPlaying(false)
    , m_hasFirstFrame(false)
    , m_playbackComplete(false)
    , m_privateBrowsing(false)
    , m_fetchingSuspended(false)
{
    m_videoSurface->addListener(m_listener.get());
    m_player->setListener(m_listener);
    m_videoLayer->unref();
    // Ensure WebKit updates the media element.
    syncLayer();
}

MediaPlayerPrivateAndroid::~MediaPlayerPrivateAndroid()
{
    m_videoSurface->removeListener(m_listener.get());
    m_player->setListener(0);
    m_player->setVideoSurfaceTexture(0);
    m_player->reset();
    m_listener->detach();

    MediaManager::deactivate(this);
    android::FullscreenVideoView::client(m_mediaPlayerClient)->onReset(this);
}

void MediaPlayerPrivateAndroid::load(const String& url)
{
    cancelLoad();

    const CString& utf8 = url.utf8(true);
    if (utf8.isNull()) {
        ALOGE("Media Load Failed: Could not convert source url to utf8.");
        sendMessage(Error, android::MEDIA_ERROR_UNKNOWN, android::ERROR_CANNOT_CONNECT);
        return;
    }

    GURL gurl(utf8.data());
    if (!gurl.is_valid() || gurl.is_empty()) {
        ALOGE("Media Load Failed: Source url is invalid.");
        sendMessage(Error, android::MEDIA_ERROR_UNKNOWN, android::ERROR_CANNOT_CONNECT);
        return;
    }

    android::status_t error;
    if (gurl.SchemeIs("file")) {
        int fd = open(gurl.path().c_str(), O_RDONLY);
        if (fd < 0) {
            ALOGE("Media Load Failed: Could not open local file as source url.");
            sendMessage(Error, android::MEDIA_ERROR_UNKNOWN, android::ERROR_CANNOT_CONNECT);
            return;
        }
        int64_t length = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        error = m_player->setDataSource(fd, 0, length);
        m_attrs.hasFileSource = true;

        close(fd);
    } else {
        android::KeyedVector<android::String8, android::String8> headers;
        net::CookieStore* cookieStore = android::WebCookieJar::get(m_privateBrowsing)->cookieStore();
        net::CookieOptions allowHttpOnlyCookes;
        allowHttpOnlyCookes.set_include_httponly();
        std::string cookies = cookieStore->GetCookiesWithOptions(gurl, allowHttpOnlyCookes);
        headers.add(android::String8("Cookie"), android::String8(cookies.c_str()));
        if (m_privateBrowsing)
            headers.add(android::String8("x-hide-urls-from-log"), android::String8("true"));

        error = m_player->setDataSource(gurl.spec().c_str(), &headers);
    }

    if (error != android::NO_ERROR) {
        sendMessage(Error, android::MEDIA_ERROR_UNKNOWN, error);
        return;
    }
    m_player->setVideoSurfaceTexture(m_overrideVideoSurfaceTexture.get() != 0 ? m_overrideVideoSurfaceTexture.get() : m_videoSurface->getBufferQueue().get());
    m_player->prepareAsync();
    updateNetworkState(MediaPlayer::Loading);
}

void MediaPlayerPrivateAndroid::cancelLoad()
{
    if (m_networkState == MediaPlayer::Empty)
        return;

    m_player->reset();

    m_wakeLock.clear();

    m_readyState = MediaPlayer::HaveNothing;
    m_networkState = MediaPlayer::Empty;
    m_attrs = Attributes();
    m_percentLoaded = 0;
    m_currentTime = 0;
    m_isPlaying = false;
    m_hasFirstFrame = false;
    m_playbackComplete = false;
    m_fetchingSuspended = false;

    MediaManager::deactivate(this);

    m_videoLayer->showIcon(VideoLayerAndroid::NoIcon);
    syncLayer();

    removeAllMessages();

    android::FullscreenVideoView::client(m_mediaPlayerClient)->onReset(this);
}

int MediaPlayerPrivateAndroid::audioSessionId() const
{
    return m_player->getAudioSessionId();
}

bool MediaPlayerPrivateAndroid::isBackgroundVideo() const
{
    if (m_readyState == MediaPlayer::HaveNothing)
        return false;

    if (!m_attrs.hasVideo)
        return false;

    if (MediaPlayerClient* currentFullscreenPlayer = android::FullscreenVideoView::currentFullscreenPlayer())
        return currentFullscreenPlayer != m_owner->mediaPlayerClient();

    android::WebViewCore* core = getWebViewCore(m_owner);
    if (!core)
        return false;

    return core->isInBackground();
}

void MediaPlayerPrivateAndroid::overrideVideoSurfaceTexture(android::sp<android::IGraphicBufferProducer> gbp)
{
    m_player->setVideoSurfaceTexture(gbp.get() == 0 ? m_videoSurface->getBufferQueue().get() : gbp.get());
    if (m_isPlaying && !m_player->isPlaying()) // This WAR to solve race condition in media player in buffering case.
        m_player->start();
    m_overrideVideoSurfaceTexture = gbp;
}

bool MediaPlayerPrivateAndroid::requestPermissionToPlay()
{
    if (isBackgroundVideo())
        return false;

    if (!MediaManager::lockAudio(this))
        return false;

    return true;
}

void MediaPlayerPrivateAndroid::play()
{
    updatePlaybackState(true);

    if (!requestPermissionToPlay()) {
        // Sometimes there is a delay between when webkit requests permission
        // to play, and when it actually does play.
        MediaManager::deactivate(this);
        sendMessage(Pause);
        return;
    }

    if (!MediaManager::isMediaSuspended())
        m_player->start();
}

void MediaPlayerPrivateAndroid::pause()
{
    m_player->pause();
    updatePlaybackState(false);
}

void MediaPlayerPrivateAndroid::seek(float time)
{
    if (m_player->seekTo(static_cast<int>(1000 * time + 0.5f)) != android::OK)
        return;

    if (m_playbackComplete && time < m_attrs.duration)
        m_playbackComplete = false;

    // Ideally we'd just updateCurrentTime() once we get MEDIA_SEEK_COMPLETE,
    // but when paused, it sometimes won't come until after we resume playing.
    sendMessage(UpdateCurrentTime);
}

void MediaPlayerPrivateAndroid::setVolume(float volume)
{
    m_player->setVolume(volume, volume);
}

bool MediaPlayerPrivateAndroid::seeking() const
{
    // When paused, MediaPlayer sometimes won't send the MEDIA_SEEK_COMPLETE
    // notification until it plays again. So we just don't use it for now.
    return false;
}

float MediaPlayerPrivateAndroid::maxTimeSeekable() const
{
    if (m_readyState == MediaPlayer::HaveNothing)
        return 0;

    return m_attrs.duration;
}

PassRefPtr<TimeRanges> MediaPlayerPrivateAndroid::buffered() const
{
    if (m_readyState == MediaPlayer::HaveNothing)
        return TimeRanges::create();

    return TimeRanges::create(0, m_percentLoaded * m_attrs.duration / 100);
}

unsigned MediaPlayerPrivateAndroid::bytesLoaded() const
{
    if (m_readyState == MediaPlayer::HaveNothing)
        return 0;

    // This method is only used to detect when the download stalls, so it works
    // just fine to return m_percentLoaded. Although, if there was a way to find
    // the actual size of the media, we would probably multiply it in.
    return m_percentLoaded;
}

bool MediaPlayerPrivateAndroid::hasAudio() const
{
    if (m_readyState == MediaPlayer::HaveNothing) {
        // Make our best guess as to whether we have audio.
        return true;
    }
    return m_attrs.hasAudio;
}

bool MediaPlayerPrivateAndroid::hasVideo() const
{
    if (m_readyState == MediaPlayer::HaveNothing) {
        // Make our best guess as to whether we have video.
        return m_owner->mediaElementType() == MediaPlayer::Video;
    }
    return m_attrs.hasVideo;
}

bool MediaPlayerPrivateAndroid::hasSingleSecurityOrigin() const
{
    if (m_readyState == MediaPlayer::HaveNothing) {
        // Nothing is loaded yet, so there's no sensitive data.
        return true;
    }
    return m_attrs.hasSingleSecurityOrigin;
}

PlatformMedia MediaPlayerPrivateAndroid::platformMedia() const
{
    PlatformMedia platformMedia;
    platformMedia.type = PlatformMedia::AndroidMediaPlayerType;
    platformMedia.media.androidMediaPlayer = const_cast<MediaPlayerPrivateAndroid*>(this);
    return platformMedia;
}

PlatformVideoSurface* MediaPlayerPrivateAndroid::platformVideoSurface() const
{
    if (m_readyState == MediaPlayer::HaveNothing || !m_hasFirstFrame)
        return 0;
    return m_videoSurface.get();
}

LayerAndroid* MediaPlayerPrivateAndroid::platformLayer() const
{
    return m_videoLayer.get();
}

void MediaPlayerPrivateAndroid::paintCurrentFrameInContext(GraphicsContext* c, const IntRect& r)
{
    if (m_readyState == MediaPlayer::HaveNothing) {
        c->fillRect(r, Color::black, ColorSpaceDeviceRGB);
        return;
    }

    if (!m_frameDecodeTexture) {
        m_frameDecodeTexture = MappedTexture::create(ResourceLimits::WebContent, m_attrs.videoSize, MappedTexture::HasAlpha, MappedTexture::WriteUsingHardware);
        if (!m_frameDecodeTexture) {
            c->fillRect(r, Color::black, ColorSpaceDeviceRGB);
            return;
        }
    }

    if (!m_copyVideoSurface) {
        m_copyVideoSurface = CopyVideoSurface::create();
        if (!m_copyVideoSurface) {
            c->fillRect(r, Color::black, ColorSpaceDeviceRGB);
            return;
        }
    }

    // Flip the frame vertically since graphic buffers have a reverse orientation.
    IntRect destRect(0, m_attrs.videoSize.height(), m_attrs.videoSize.width(), -m_attrs.videoSize.height());

    if (!m_copyVideoSurface->copyCurrentFrame(m_frameDecodeTexture.get(), m_videoSurface, destRect, 0)) {
        c->fillRect(r, Color::black, ColorSpaceDeviceRGB);
        return;
    }

    SkBitmap bitmap;

    {
        SkBitmap frameDecodeBitmap;

        if (!m_frameDecodeTexture->lockBufferForReading(&frameDecodeBitmap, false)) {
            c->fillRect(r, Color::black, ColorSpaceDeviceRGB);
            return;
        }

        if (c->platformContext()->acceleratedCanvas()) {
            // Copy the frame since the accelerated canvas may defer rendering.
            if (!frameDecodeBitmap.copyTo(&bitmap, SkBitmap::kARGB_8888_Config))
                bitmap.reset();
        } else
            bitmap = frameDecodeBitmap;
    }

    if (!bitmap.isNull()) {
        SkBitmapRef* bitmapRef = new SkBitmapRef(bitmap);
        RefPtr<Image> frameImage = BitmapImage::create(bitmapRef, 0);
        bitmapRef->unref();
        c->drawImage(frameImage.get(), ColorSpaceDeviceRGB, r, CompositeCopy);
    } else
        c->fillRect(r, Color::black, ColorSpaceDeviceRGB);

    m_frameDecodeTexture->unlockBuffer();
}

void MediaPlayerPrivateAndroid::handleMessage(unsigned what, int arg1, int arg2, void*)
{
    switch (what) {

    case Prepared: {
        ASSERT(!m_isPlaying); // HTMLMediaElement shouldn't call play before we have metadata.
        parseMetadata();
        updateDuration();
        updateHasSingleSecurityOrigin();
        if (!parseTrackInfo()) {
            ALOGE("Media Load Error: Failed to parse track info.");
            // We have to make our best guesses about whether it has audio/video.
            m_attrs.hasAudio = true;
            m_attrs.hasVideo = m_owner->mediaElementType() == MediaPlayer::Video;
            updateReadyState(MediaPlayer::HaveEnoughData);
        } else if (!m_attrs.hasVideo || !m_attrs.canSeek) // Live streams don't decode frames while loading.
            updateReadyState(MediaPlayer::HaveEnoughData);
        else
            updateReadyState(MediaPlayer::HaveMetadata);
        // Don't let it draw frames until after the style recalc. Otherwise
        // the first frame might flash on before the layer has adjusted.
        m_videoLayer->setVideoSurface(m_videoSurface.get());
        if (m_attrs.hasFileSource) {
            // File sources don't send buffering updates.
            sendMessage(BufferingUpdate, 100);
        }
        return;
    }

    case FrameAvailable: {
        if (m_hasFirstFrame)
            return;
        if (m_readyState < MediaPlayer::HaveCurrentData)
            updateReadyState(MediaPlayer::HaveCurrentData);
        m_hasFirstFrame = true;
        m_owner->firstVideoFrameAvailable();
        return;
    }

    case PlaybackComplete: {
        m_playbackComplete = true;
        updateCurrentTime();
        if (!m_playbackComplete) {
            // WebKit looped - restart the media player since it pauses on completion.
            if (m_isPlaying && !MediaManager::isMediaSuspended())
                m_player->start();
            m_videoLayer->showIcon(VideoLayerAndroid::NoIcon);
        } else
            pause();
        syncLayer();
        return;
    }

    case BufferingUpdate: {
        unsigned percentLoaded = std::min(std::max(arg1, 0), 100);
        if (percentLoaded == m_percentLoaded)
            return;
        m_percentLoaded = percentLoaded;
        if (m_percentLoaded == 100 && m_networkState == MediaPlayer::Loading
            && m_readyState == MediaPlayer::HaveEnoughData)
            updateNetworkState(MediaPlayer::Loaded);
        else if (m_percentLoaded < 100 && m_networkState == MediaPlayer::Loaded)
            updateNetworkState(MediaPlayer::Loading);
        android::FullscreenVideoView::client(m_mediaPlayerClient)->onDownloadProgress(this);
        return;
    }

    case SetVideoSize: {
        IntSize newSize(arg1, arg2);
        if (newSize == m_attrs.videoSize)
            return;
        m_attrs.videoSize = newSize;
        m_owner->sizeChanged();
        android::FullscreenVideoView::client(m_mediaPlayerClient)->onVideoSizeChanged(this);
        return;
    }

    case UpdateCurrentTime: {
        updateCurrentTime();
        return;
    }

    case TrackingTimerFired: {
        ASSERT(m_isPlaying);
        // Call scheduleTrackingTimer first since updateCurrentTime may trigger a pause.
        scheduleTrackingTimer(RepeatTimerFire);
        updateCurrentTime();
        return;
    }

    case Pause: {
        pause();
        return;
    }

    case Info: {
        switch (arg1) {
        case android::MEDIA_INFO_RENDERING_START:
            updateReadyState(MediaPlayer::HaveEnoughData);
            break;
        case android::MEDIA_INFO_BUFFERING_START:
            updateReadyState(MediaPlayer::HaveCurrentData);
            m_videoLayer->showIcon(VideoLayerAndroid::NoIcon);
            m_videoLayer->setBuffering(true);
            syncLayer();
            break;
        case android::MEDIA_INFO_BUFFERING_END:
            updateReadyState(MediaPlayer::HaveEnoughData);
            if (m_isPlaying)
                m_videoLayer->showIcon(VideoLayerAndroid::PlayIcon);
            m_videoLayer->setBuffering(false);
            syncLayer();
            break;
        };
        return;
    }

    case Error: {
        if (arg1 == android::MEDIA_ERROR_UNKNOWN) {
            ALOGE("Media Error: Encountered error code 0x%x.", arg2);
            updateNetworkState(MediaPlayer::FormatError);
        }
        pause();
        return;
    }

    default: {
        ASSERT_NOT_REACHED();
        return;
    }

    }
}

void MediaPlayerPrivateAndroid::updateReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    m_readyState = readyState;
    m_owner->readyStateChanged();
    updateFetching();
    android::FullscreenVideoView::client(m_mediaPlayerClient)->onReadyStateChanged(this);

    if (m_readyState == MediaPlayer::HaveEnoughData
        && m_networkState == MediaPlayer::Loading && m_percentLoaded == 100)
        updateNetworkState(MediaPlayer::Loaded);
    else if (m_readyState < MediaPlayer::HaveEnoughData && m_networkState == MediaPlayer::Loaded)
        updateNetworkState(MediaPlayer::Loading);
}

void MediaPlayerPrivateAndroid::updateNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    m_networkState = networkState;
    m_owner->networkStateChanged();
}

void MediaPlayerPrivateAndroid::updatePlaybackState(bool isPlaying)
{
    if (m_isPlaying == isPlaying)
        return;

    m_isPlaying = isPlaying;

    if (m_isPlaying) {
        m_playbackComplete = false;
        if (!MediaManager::isMediaSuspended()) {
            scheduleTrackingTimer(FirstTimerFire);
            stayAwake(m_attrs.hasVideo && m_owner->mediaElementType() == MediaPlayer::Video);
        }
        if (!MediaManager::activate(this))
            ASSERT_NOT_REACHED();
        m_videoLayer->showIcon(VideoLayerAndroid::PlayIcon);
    } else {
        stopTrackingTimer();
        updateCurrentTime(false);
        stayAwake(false);
        MediaManager::deactivate(this);
        m_videoLayer->showIcon(VideoLayerAndroid::PauseIcon);
    }

    m_owner->playbackStateChanged();
    updateFetching();
    android::FullscreenVideoView::client(m_mediaPlayerClient)->onPlaybackStateChanged(this);
    syncLayer();
}

void MediaPlayerPrivateAndroid::parseMetadata()
{
    m_attrs.canPause = true;
    m_attrs.canSeekBackward = true;
    m_attrs.canSeekForward = true;
    m_attrs.canSeek = true;

    OwnPtr<android::Parcel> metadataParcel(new android::Parcel());
    if (m_player->getMetadata(false, false, metadataParcel.get()) != android::OK)
        return;

    android::MediaMetadata metadata;
    if (!metadata.parse(metadataParcel.release()))
        return;

    if (metadata.has(android::MediaMetadata::PauseAvailable))
        m_attrs.canPause = metadata.getBool(android::MediaMetadata::PauseAvailable);
    if (metadata.has(android::MediaMetadata::SeekBackwardAvailable))
        m_attrs.canSeekBackward = metadata.getBool(android::MediaMetadata::SeekBackwardAvailable);
    if (metadata.has(android::MediaMetadata::SeekForwardAvailable))
        m_attrs.canSeekForward = metadata.getBool(android::MediaMetadata::SeekForwardAvailable);
    if (metadata.has(android::MediaMetadata::SeekAvailable))
        m_attrs.canSeek = metadata.getBool(android::MediaMetadata::SeekAvailable);
}

void MediaPlayerPrivateAndroid::updateDuration()
{
    float oldDuration = m_attrs.duration;

    int duration;
    if (m_player->getDuration(&duration) != android::OK)
        return;
    m_attrs.duration = duration / 1000.0f;

    if (m_attrs.duration != oldDuration && m_readyState != MediaPlayer::HaveNothing)
        m_owner->durationChanged();
}

void MediaPlayerPrivateAndroid::updateCurrentTime(bool notifyOwner)
{
    float oldTime = m_currentTime;

    if (m_playbackComplete) {
        // When playback is complete, the player's current time may not be exactly
        // equal to the duration. We force it so webkit can detect completion.
        m_currentTime = m_attrs.duration;
    } else {
        int time;
        if (m_player->getCurrentPosition(&time) != android::OK)
            return;
        m_currentTime = time / 1000.0f;
    }

    if (m_currentTime != oldTime && m_readyState != MediaPlayer::HaveNothing && notifyOwner)
        m_owner->timeChanged();
}

void MediaPlayerPrivateAndroid::updateSuspending()
{
    ASSERT(m_isPlaying);
    updateFetching();
    if (MediaManager::isMediaSuspended()) {
        // Silently pause.
        stopTrackingTimer();
        m_player->pause();
        stayAwake(false);
    } else {
        // Silently resume.
        m_player->start();
        scheduleTrackingTimer(FirstTimerFire);
        stayAwake(m_attrs.hasVideo && m_owner->mediaElementType() == MediaPlayer::Video);
    }
}

bool MediaPlayerPrivateAndroid::updateFetching()
{
    bool shouldSuspendFetching = m_readyState == MediaPlayer::HaveEnoughData
                                 && (!m_isPlaying || MediaManager::isMediaSuspended());
    if (shouldSuspendFetching == m_fetchingSuspended)
        return true;

    m_fetchingSuspended = shouldSuspendFetching;

    android::Parcel request;
    android::Parcel reply;
    request.writeInterfaceToken(android::String16(mediaPlayernterfaceToken));
    request.writeInt32(android::INVOKE_ID_SUSPEND_PREFETCHING);
    request.writeInt32(m_fetchingSuspended);

    return m_player->invoke(request, &reply) == android::OK;
}

void MediaPlayerPrivateAndroid::updateHasSingleSecurityOrigin()
{
    android::Parcel request;
    android::Parcel reply;
    request.writeInterfaceToken(android::String16(mediaPlayernterfaceToken));
    request.writeInt32(android::INVOKE_ID_QUERY_HAS_SINGLE_SECURITY_ORIGIN);
    if (m_player->invoke(request, &reply) != android::OK)
        return;
    int hasSingleSecurityOrigin;
    if (reply.readInt32(&hasSingleSecurityOrigin) != android::OK)
        return;
    m_attrs.hasSingleSecurityOrigin = hasSingleSecurityOrigin;
}

bool MediaPlayerPrivateAndroid::parseTrackInfo()
{
    android::Parcel request;
    android::Parcel reply;
    request.writeInterfaceToken(android::String16(mediaPlayernterfaceToken));
    request.writeInt32(android::INVOKE_ID_GET_TRACK_INFO);
    if (m_player->invoke(request, &reply) != android::OK)
        return false;

    int32_t trackCount;
    if (reply.readInt32(&trackCount) != android::OK)
        return false;

    while (trackCount--) {
        int32_t fieldCount;
        if (reply.readInt32(&fieldCount) != android::OK
            || fieldCount != 2)
            return false;

        int32_t trackType;
        if (reply.readInt32(&trackType) != android::OK)
            return false;

        if (trackType == android::MEDIA_TRACK_TYPE_AUDIO)
            m_attrs.hasAudio = true;
        else if (trackType == android::MEDIA_TRACK_TYPE_VIDEO)
            m_attrs.hasVideo = true;

        android::String16 trackLanguage = reply.readString16();
    }

    return true;
}

void MediaPlayerPrivateAndroid::syncLayer()
{
    m_mediaPlayerClient->mediaPlayerRenderingModeChanged(m_owner);
}

void MediaPlayerPrivateAndroid::stayAwake(bool awake)
{
    if (!m_wakeLock) {
        if (!awake)
            return;
        m_wakeLock = android::WakeLock::create(getAndroidContext(m_owner), LOG_TAG);
        if (!m_wakeLock)
            return;
        m_wakeLock->setReferenceCounted(false);
    }

    if (awake && !m_wakeLock->isHeld())
        m_wakeLock->acquire();
    if (!awake && m_wakeLock->isHeld())
        m_wakeLock->release();
}

void MediaPlayerPrivateAndroid::scheduleTrackingTimer(TimerFireType fireType)
{
    if (!m_isPlaying || MediaManager::isMediaSuspended() || hasMessages(TrackingTimerFired)) {
        ASSERT_NOT_REACHED();
        return;
    }

    double delay;
    if (fireType == FirstTimerFire) {
        m_trackingTimerEpoch = currentTime();
        delay = trackingInterval;
    } else
        delay = trackingInterval - fmod(currentTime() - m_trackingTimerEpoch, trackingInterval);

    // We can't use Timer<MediaPlayerPrivateAndroid> because audio-only media
    // needs to keep updating the time while browser is in the background.
    sendMessageDelayed(TrackingTimerFired, delay);
}

void MediaPlayerPrivateAndroid::stopTrackingTimer()
{
    removeMessages(TrackingTimerFired);
}

static void getSupportedTypes(HashSet<String>&) {}

static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs)
{
    if (android::WebViewCore::isSupportedMediaMimeType(type))
        return MediaPlayer::MayBeSupported;
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateAndroid::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

void MediaPlayerPrivateAndroid::stopBackgroundVideos()
{
    MediaManager::stopBackgroundVideos();
}

} // namespace WebCore

#endif // VIDEO
