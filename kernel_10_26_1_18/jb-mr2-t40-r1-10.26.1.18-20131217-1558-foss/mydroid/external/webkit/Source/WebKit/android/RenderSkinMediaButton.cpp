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

#define LOG_TAG "WebCore"

#include "config.h"
#include "RenderSkinMediaButton.h"

#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderObject.h"
#include "RenderSkinAndroid.h"
#include "RenderSlider.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include <androidfw/AssetManager.h>
#include <utils/Debug.h>
#include <utils/Log.h>
#include <wtf/text/CString.h>

using namespace WebCore;

extern android::AssetManager* globalAssetManager();

struct ButtonBitmapData {
    const char* name;
    int8_t margin;
};

static const ButtonBitmapData gNormalButtonBitmapData[] = {
    { "ic_media_pause.png", 8}, // PAUSE
    { "ic_media_play.png", 8}, // PLAY
    { "ic_media_pause.png", 8}, // MUTE
    { "ic_media_rew.png", 8}, // REWIND
    { "ic_media_ff.png", 8}, // FORWARD
    { "ic_media_fullscreen.png", 8}, // FULLSCREEN
    { "spinner_76_outer_holo.png", 8}, // SPINNER_OUTER
    { "spinner_76_inner_holo.png", 8}, // SPINNER_INNER
    { "ic_media_video_poster.png", 8}, // VIDEO
    { "scrubber_control_normal_holo.png", 0}       // SLIDER_THUMB
};

COMPILE_ASSERT(sizeof(gNormalButtonBitmapData) / sizeof(gNormalButtonBitmapData[0]) == (RenderSkinMediaButton::SLIDER_THUMB + 1), NormalButtonBitmapDataMatchesMediaButtonEnumValues);

static const char sliderTrackButtonBitmapName[] = "scrubber_track_holo_dark.9.png"; // For SLIDER_TRACK.
static const char sliderTrackAlreadyPlayedButtonBitmapName[] = "scrubber_primary_holo.9.png"; // For left of the SLIDER_THUMB.

static bool loadButtonBitmap(int bitmapIndex, const char* name, SkBitmap* targetBitmap)
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);

    MutexLocker lock(mutex);

    static SkBitmap* buttonBitmaps[RenderSkinMediaButton::LAST_NORMAL_BITMAP_BUTTON + 3]; // Normal bitmaps + two nine-patch slider bitmaps.
    static bool decodingHasFailed;

    if (decodingHasFailed)
        return false;

    if (!buttonBitmaps[bitmapIndex]) {
        String path = RenderSkinAndroid::DrawableDirectory() + name;
        SkBitmap loadedBitmap;
        if (!RenderSkinAndroid::DecodeBitmap(globalAssetManager(), path.utf8().data(), &loadedBitmap)) {
            // Any error will cause none of the bitmaps work.
            decodingHasFailed = true;
            return false;
        }

        buttonBitmaps[bitmapIndex] = new SkBitmap(loadedBitmap);
    }

    *targetBitmap = *buttonBitmaps[bitmapIndex]; // Copy the object in order to paint the bitmap in threadsafe manner.
    return true;
}

static inline bool loadNormalButtonBitmap(RenderSkinMediaButton::MediaButton buttonType, SkBitmap* targetBitmap)
{
    ASSERT(buttonType >= 0 && buttonType <= RenderSkinMediaButton::LAST_NORMAL_BITMAP_BUTTON);
    return loadButtonBitmap(buttonType, gNormalButtonBitmapData[buttonType].name, targetBitmap);
}

static inline bool loadSliderTrackAlreadyPlayedBitmap(SkBitmap* targetBitmap)
{
    return loadButtonBitmap(RenderSkinMediaButton::LAST_NORMAL_BITMAP_BUTTON + 1, sliderTrackAlreadyPlayedButtonBitmapName, targetBitmap);
}

static inline bool loadSliderTrackBitmap(SkBitmap* targetBitmap)
{
    return loadButtonBitmap(RenderSkinMediaButton::LAST_NORMAL_BITMAP_BUTTON + 2, sliderTrackButtonBitmapName, targetBitmap);
}

static void drawSliderTrackBitmaps(SkCanvas* canvas, const IntRect& r, const IntRect& thumb)
{
    // Cut the height in half (with some extra slop determined by trial
    // and error to get the placement just right.
    SkRect bounds = r;
    SkScalar quarterHeight = SkScalarHalf(SkScalarHalf(bounds.height()));
    bounds.fTop += quarterHeight + SkScalarHalf(3);
    bounds.fBottom += -quarterHeight + SK_ScalarHalf;
    if (!thumb.isEmpty()) {
        // Inset the track by half the width of the thumb, so the track
        // does not appear to go beyond the space where the thumb can
        // be.
        SkScalar thumbHalfWidth = SkIntToScalar(thumb.width()/2);
        bounds.fLeft += thumbHalfWidth;
        bounds.fRight -= thumbHalfWidth;
        if (thumb.x() > 0) {
            // The video is past the starting point.  Show the area to
            // left of the thumb as having been played.
            SkScalar alreadyPlayed = SkIntToScalar(thumb.center().x() + r.x());
            SkRect playedRect(bounds);
            playedRect.fRight = alreadyPlayed;
            SkBitmap alreadyPlayedBitmap;
            if (loadSliderTrackAlreadyPlayedBitmap(&alreadyPlayedBitmap))
                SkNinePatch::DrawNine(canvas, playedRect, alreadyPlayedBitmap, SkIRect());
            bounds.fLeft = alreadyPlayed;
        }
    }

    SkBitmap sliderTrackBitmap;
    if (!loadSliderTrackBitmap(&sliderTrackBitmap))
        return;

    SkNinePatch::DrawNine(canvas, bounds, sliderTrackBitmap, SkIRect());
}

static void drawNormalButtonBitmap(SkCanvas* canvas, const IntRect& r, RenderSkinMediaButton::MediaButton buttonType)
{
    SkBitmap bitmap;
    if (!loadNormalButtonBitmap(buttonType, &bitmap))
        return;

    const int imageMargin = gNormalButtonBitmapData[buttonType].margin;

    SkPaint paint;
    paint.setFlags(SkPaint::kFilterBitmap_Flag);

    float scale = (r.width() - 2 * imageMargin) / (float) bitmap.width();
    int saveScaleCount = canvas->save();
    canvas->translate(r.x() + imageMargin, r.y() + imageMargin);
    canvas->scale(scale, scale);
    canvas->drawBitmap(bitmap, 0, 0, &paint);
    canvas->restoreToCount(saveScaleCount);
}

namespace WebCore {

void RenderSkinMediaButton::Draw(SkCanvas* canvas, const IntRect& r,
                                 MediaButton buttonType, const Color& backgroundColor, const IntRect& thumb)
{
    if (!canvas)
        return;

    if (backgroundColor.isValid())  {
        SkPaint paint;
        paint.setColor(backgroundColor.rgb());
        canvas->drawRect(r, paint);
    }

    if (buttonType >= 0 && buttonType <= LAST_NORMAL_BITMAP_BUTTON) {
        drawNormalButtonBitmap(canvas, r, buttonType);
        return;
    } else if (buttonType == SLIDER_TRACK) {
        drawSliderTrackBitmaps(canvas, r, thumb);
        return;
    }

    ASSERT(buttonType == BACKGROUND_SLIDER);
    // Background SLIDER doesn't draw an image.
}

} // WebCore
