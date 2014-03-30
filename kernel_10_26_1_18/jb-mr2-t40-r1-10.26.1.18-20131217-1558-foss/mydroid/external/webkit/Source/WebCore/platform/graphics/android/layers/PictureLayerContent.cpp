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
#define LOG_TAG "PictureLayerContent"
#define LOG_NDEBUG 1

#include "config.h"
#include "PictureLayerContent.h"

#include "AndroidLog.h"
#include "InspectorCanvas.h"
#include "SkNWayCanvas.h"
#include "SkPicture.h"

#include <dlfcn.h>
#include "SkDevice.h"

namespace WebCore {

PictureLayerContent::PictureLayerContent()
    : m_checkedContent(false)
    , m_hasText(true)
{
    for (size_t i = 0; i < TexturesGeneratorList::threadCount; ++i) {
        m_threadPictures[i] = new SkPicture();
        m_threadPictures[i]->unref();
    }
}

PictureLayerContent::PictureLayerContent(const PictureLayerContent& content)
    : m_threadPictures(content.m_threadPictures)
    , m_checkedContent(content.m_checkedContent)
    , m_hasText(content.m_hasText)
{

}

PictureLayerContent::~PictureLayerContent()
{
}

PictureLayerContent* PictureLayerContent::createFromPicture(SkPicture* picture)
{
    PictureLayerContent* content = new PictureLayerContent();
    if (SkCanvas* recordingCanvas = content->beginRecording(IntSize(picture->width(), picture->height())))
        picture->draw(recordingCanvas);

    content->endRecording();
    return content;
}

PictureLayerContent* PictureLayerContent::createAndBeginRecording(const IntSize& size)
{
    PictureLayerContent* content = new PictureLayerContent();
    if (!content->beginRecording(size)) {
        delete content;
        return 0;
    }
    return content;
}

SkCanvas* PictureLayerContent::beginRecording(const IntSize& size)
{
    if (m_recordingCanvas)
        return m_recordingCanvas.get();

    if (TexturesGeneratorList::threadCount == 1) {
        m_recordingCanvas = m_threadPictures[0]->beginRecording(size.width(), size.height(), 0);
        return m_recordingCanvas.get();
    }

    SkCanvas* threadCanvases[TexturesGeneratorList::threadCount];
    bool canvasesFailed = false;
    for (size_t i = 0; i < TexturesGeneratorList::threadCount; ++i) {
        threadCanvases[i] = m_threadPictures[i]->beginRecording(size.width(), size.height(), 0);
        if (!threadCanvases[i])
            return 0;
    }

    SkNWayCanvas* nwayCanvas = new SkNWayCanvas(size.width(), size.height());
    for (size_t i = 0; i < TexturesGeneratorList::threadCount; i++)
        nwayCanvas->addCanvas(threadCanvases[i]);

    m_recordingCanvas = nwayCanvas;
    nwayCanvas->unref();
    return m_recordingCanvas.get();
}

void PictureLayerContent::endRecording()
{
    m_recordingCanvas = 0;
    for (size_t i = 0; i < TexturesGeneratorList::threadCount; ++i)
        m_threadPictures[i]->endRecording();
}

int PictureLayerContent::width()
{
    if (!m_threadPictures[0])
        return 0;
    return m_threadPictures[0]->width();
}

int PictureLayerContent::height()
{
    if (!m_threadPictures[0])
        return 0;
    return m_threadPictures[0]->height();
}

void PictureLayerContent::checkForOptimisations()
{
    if (!m_checkedContent)
        maxZoomScale(); // for now only check the maximum scale for painting
}

float PictureLayerContent::maxZoomScale()
{
    if (m_checkedContent)
        return m_hasText ? 1e6 : 1.0;

    // Let's check if we have text or not. If we don't, we can limit
    // ourselves to scale 1!
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     m_threadPictures[0]->width(),
                     m_threadPictures[0]->height());
    InspectorBounder inspectorBounder;
    InspectorCanvas checker(&inspectorBounder, m_threadPictures[0].get(), bitmap);
    m_threadPictures[0]->draw(&checker);
    m_hasText = checker.hasText();
    if (!checker.hasContent()) {
        // no content to draw, discard picture so UI / tile generation
        // doesn't bother with it
        for (size_t i = 0; i < TexturesGeneratorList::threadCount; ++i)
            m_threadPictures[i] = 0;
    }

    m_checkedContent = true;

    return m_hasText ? 1e6 : 1.0;
}

void PictureLayerContent::draw(SkCanvas* canvas)
{
    if (!m_threadPictures[0])
        return;

    TRACE_METHOD();

    size_t threadIndex = TexturesGeneratorList::instance()->threadIndexForCurrentThread(0);
    SkRect r = SkRect::MakeWH(width(), height());
    int saveCount = canvas->save();
    canvas->clipRect(r);
    m_threadPictures[threadIndex]->draw(canvas);
    canvas->restoreToCount(saveCount);
}

void PictureLayerContent::serialize(SkWStream* stream)
{
    if (!stream)
        return;

    SkPicture p;
    if (m_threadPictures[0]) {
        // We cannot be sure that SkPicture::serialize is thread-safe.
        // Serialize a copy.
        SkCanvas* c = p.beginRecording(width(), height(), 0);
        m_threadPictures[0]->draw(c);
        p.endRecording();
    }
    p.serialize(stream);
}


LegacyPictureLayerContent::LegacyPictureLayerContent(SkMemoryStream* pictureStream) {
    m_legacyPicture = NULL;
    m_width = 0;
    m_height = 0;

    // load legacy skia lib (all functions hidden except ones defined below)
    m_legacyLib = dlopen("libskia_legacy.so", RTLD_LAZY);
    *reinterpret_cast<void**>(&m_createPictureProc) = dlsym(m_legacyLib, "legacy_skia_create_picture");
    *reinterpret_cast<void**>(&m_deletePictureProc) = dlsym(m_legacyLib, "legacy_skia_delete_picture");
    *reinterpret_cast<void**>(&m_drawPictureProc) = dlsym(m_legacyLib, "legacy_skia_draw_picture");

    const char* error = dlerror();
    if (error) {
      SkDebugf("Unable to load legacy lib: %s", error);
      sk_throw();
    }

    // call into library to create picture and set width and height
    const int streamLength = pictureStream->getLength() - pictureStream->peek();
    int bytesRead = m_createPictureProc(pictureStream->getAtPos(), streamLength,
                                        &m_legacyPicture, &m_width, &m_height);
    pictureStream->skip(bytesRead);
}

LegacyPictureLayerContent::~LegacyPictureLayerContent() {
    if (m_legacyLib) {
        if (m_legacyPicture) {
          m_deletePictureProc(m_legacyPicture);
        }
        dlclose(m_legacyLib);
    }
}

void LegacyPictureLayerContent::draw(SkCanvas* canvas) {
    if (!m_legacyPicture) {
      return;
    }

    // if this is an InspectorCanvas we need to at least draw something to
    // ensure that the canvas is not discarded. (We perform a no-op text
    // draw in order to trigger the InspectorCanvas into performing high
    // fidelity rendering while zooming.
    SkPaint paint;
    canvas->drawText(NULL, 0, 0, 0, paint);

    // decompose the canvas into basics
    void* matrixStorage = malloc(canvas->getTotalMatrix().writeToMemory(NULL));
    void* clipStorage = malloc(canvas->getTotalClip().writeToMemory(NULL));

    canvas->getTotalMatrix().writeToMemory(matrixStorage);
    canvas->getTotalClip().writeToMemory(clipStorage);

    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(true);
    bitmap.lockPixels();

    // pass picture, matrix, clip, and bitmap
    m_drawPictureProc(m_legacyPicture, matrixStorage, clipStorage,
                      bitmap.width(), bitmap.height(), bitmap.getConfig(),
                      bitmap.rowBytes(), bitmap.getPixels());


    bitmap.unlockPixels();
    free(matrixStorage);
    free(clipStorage);
}

} // namespace WebCore
