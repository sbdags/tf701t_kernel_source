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
#define LOG_TAG "PicturePileLayerContent"
#define LOG_NDEBUG 1

#include "config.h"
#include "PicturePileLayerContent.h"

#include "AndroidLog.h"
#include "SkCanvas.h"

namespace WebCore {

PicturePileLayerContent::PicturePileLayerContent(const PicturePile& picturePile)
    : m_picturePile(picturePile)
    , m_maxZoomScale(picturePile.maxZoomScale())
    , m_hasContent(!picturePile.isEmpty())
{
}

void PicturePileLayerContent::draw(SkCanvas* canvas)
{
    TRACE_METHOD();
    m_picturePile.draw(canvas);

    if (CC_UNLIKELY(!m_hasContent))
        ALOGW("Warning: painting PicturePile without content!");
}

void PicturePileLayerContent::serialize(SkWStream* stream)
{
    if (!stream)
       return;
    // We cannot be sure that SkPicture::serialize is thread-safe.
    // Serialize a copy.
    SkPicture picture;
    draw(picture.beginRecording(width(), height(),
                                SkPicture::kUsePathBoundsForClip_RecordingFlag));
    picture.endRecording();
    picture.serialize(stream);
}

PrerenderedInval* PicturePileLayerContent::prerenderForRect(const IntRect& dirty)
{
    return m_picturePile.prerenderedInvalForArea(dirty);
}

void PicturePileLayerContent::clearPrerenders()
{
    m_picturePile.clearPrerenders();
}

} // namespace WebCore
