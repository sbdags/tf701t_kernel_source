/*
 * Copyright 2012, The Android Open Source Project
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

#ifndef PictureLayerContent_h
#define PictureLayerContent_h

#include "LayerContent.h"
#include "SkStream.h"
#include "TexturesGeneratorList.h"
#include <SkPicture.h>

namespace WebCore {

class PictureLayerContent : public LayerContent {
public:
    static PictureLayerContent* createAndBeginRecording(const IntSize&);
    static PictureLayerContent* createFromPicture(SkPicture*);

    ~PictureLayerContent();

    void endRecording();
    SkCanvas* recordingCanvas() const { return m_recordingCanvas.get(); }

    virtual int width();
    virtual int height();
    virtual void setCheckForOptimisations(bool check) { m_checkedContent = !check; }
    virtual void checkForOptimisations();
    virtual float maxZoomScale();
    virtual void draw(SkCanvas* canvas);
    virtual void serialize(SkWStream* stream);

private:
    PictureLayerContent();
    PictureLayerContent(const PictureLayerContent&);
    SkCanvas* beginRecording(const IntSize&);

    // Not using TexturesGeneratorList::PerThread because of copy construction.
    // The m_threadPicture instances are thread safe as long as SkPicture locks
    // on draw.
    SkRefPtr<SkPicture> m_threadPictures[TexturesGeneratorList::threadCount];
    SkRefPtr<SkCanvas> m_recordingCanvas;
    bool m_checkedContent;
    bool m_hasText;
};

class LegacyPictureLayerContent : public LayerContent {
public:
    LegacyPictureLayerContent(SkMemoryStream* pictureStream);
    ~LegacyPictureLayerContent();

    virtual int width() { return m_width; }
    virtual int height() { return m_height; }
    virtual void setCheckForOptimisations(bool check) {}
    virtual void checkForOptimisations() {}
    virtual float maxZoomScale() { return 1e6; }
    virtual void draw(SkCanvas* canvas);
    virtual void serialize(SkWStream* stream) { }

private:
    void* m_legacyLib;
    void* m_legacyPicture;
    int m_width;
    int m_height;

    typedef int  (*legacy_skia_create_picture_proc)(const void*, int, void**, int*, int*);
    typedef void (*legacy_skia_delete_picture_proc)(void*);
    typedef void (*legacy_skia_draw_picture_proc)(void*, void*, void*, int, int, int, int, void*);

    legacy_skia_create_picture_proc m_createPictureProc;
    legacy_skia_delete_picture_proc m_deletePictureProc;
    legacy_skia_draw_picture_proc m_drawPictureProc;
};

} // WebCore

#endif // PictureLayerContent_h
