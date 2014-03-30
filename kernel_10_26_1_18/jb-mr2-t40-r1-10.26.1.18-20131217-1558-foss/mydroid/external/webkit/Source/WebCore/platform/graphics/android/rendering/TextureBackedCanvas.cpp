/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
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
#include "TextureBackedCanvas.h"

#if USE(ACCELERATED_CANVAS_LAYER)

#include "AcceleratedCanvasLambdas.h"
#include "AndroidProperties.h"
#include "EGLImageBuffer.h"
#include "EGLImageBufferRing.h"
#include "EGLImageLayer.h"
#include "EmojiFont.h"
#include "GLUtils.h"
#include "MappedTexture.h"
#include "SkDevice.h"

namespace WebCore {

static const int minParallelizableHeight = 200;
static const int minParallelizableWidth = 200;
static const int minParallelizableArea = 300*300;
static const int partitioningThreshold = 300;

static int maxPartitionCount()
{
    static int processorCount = sysconf(_SC_NPROCESSORS_ONLN);
    return processorCount - 1;
}

class FlipDevice : public SkDevice {
public:
    FlipDevice(SkCanvas* canvas, const SkBitmap& bitmap)
        : SkDevice(bitmap)
        , m_pixels(bitmap.getPixels())
        , m_rowBytes(bitmap.rowBytes())
    { }

    void backBufferChanged(const SkBitmap& bitmap)
    {
        m_pixels = bitmap.getPixels();
        m_rowBytes = bitmap.rowBytes();
    }

protected:
    const SkBitmap& onAccessBitmap(SkBitmap* bitmap)
    {
        // FIXME: this is not strictly allowed by Skia API.
        if (bitmap->rowBytes() != m_rowBytes && m_pixels)
            bitmap->setConfig(SkBitmap::kARGB_8888_Config, bitmap->width(), bitmap->height(), m_rowBytes);

        if (bitmap->getPixels() != m_pixels)
            bitmap->setPixels(m_pixels);

        return *bitmap;
    }

private:
    void* m_pixels;
    int m_rowBytes;
};

class MappedCanvasTexture
    : public MappedTexture
    , public EGLImageBuffer {
public:
    static PassOwnPtr<MappedCanvasTexture> create(const IntSize& size, MappedTexture::Format format, MappedTexture::WriteMode writeMode)
    {
        bool success = false;
        MappedCanvasTexture* obj = new MappedCanvasTexture(size, format, writeMode, success);
        if (!success) {
            delete obj;
            return 0;
        }
        return obj;
    }

    // EGLImageBuffer overrides.
    virtual bool lockBufferForReading(SkBitmap* bitmap, bool premultiplyAlpha) { return MappedTexture::lockBufferForReading(bitmap, premultiplyAlpha); }

    virtual void unlockBuffer() { MappedTexture::unlockBuffer(); }

    virtual void deleteBufferSource() { }

protected:
    virtual EGLImage* eglImage() { return m_eglImage.get(); }

private:
    MappedCanvasTexture(const IntSize& size, MappedTexture::Format format, MappedTexture::WriteMode writeMode, bool& success)
        : MappedTexture(ResourceLimits::WebContent, size, format, writeMode, success)
    {
    }
};

TextureBackedCanvas::TextureBackedCanvas(const IntSize& sz, bool& success)
    : AcceleratedCanvas(sz)
    , m_partitionCount(0)
    , m_saveLayerCount(0)
    , m_hasScheduledWork(false)
{
    ASSERT(!size().isEmpty());
    success = false;

    // FIXME: currently we assume that the rowBytes is purely function of bitmap width.
    // This should be true for now, but in the future we could want to have an
    // GraphicsBuffer flag for expressing this constraint.
    m_backBuffer = MappedCanvasTexture::create(size(), MappedTexture::HasAlpha, MappedTexture::WriteUsingSoftwareAndHardware);
    if (!m_backBuffer)
        return;

    m_backBuffer->lockSurface();

    if (!m_backBuffer->lockBufferForWriting(&m_backBufferBitmap)) {
        m_backBuffer->unlockSurface();
        m_backBuffer = 0;
        return;
    }

    m_mainCanvas = new SkCanvas(m_backBufferBitmap);
    SkDevice* device = new FlipDevice(m_mainCanvas.get(), m_backBufferBitmap);
    m_mainCanvas->setDevice(device)->unref();

    if (shouldCreatePartitions())
        createPartitions();

    success = true;
}

TextureBackedCanvas::~TextureBackedCanvas()
{
    if (!m_backBuffer)
        return;

    // Make sure there are no latent references to objects that will get deleted.
    flushDrawing();

    if (m_canvases) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            delete m_canvases[i];
    }

    m_mainCanvas.clear();

    if (isBackBufferLocked())
        m_backBuffer->unlockBuffer();

    m_backBuffer->unlockSurface();
}

bool TextureBackedCanvas::shouldCreatePartitions() const
{
    String textureBackedCanvasProperty = AndroidProperties::getStringProperty("webkit.canvas.texture", "");

    if (textureBackedCanvasProperty.contains("noparallel"))
        return false;

    if (textureBackedCanvasProperty.contains("forceparallel"))
        return true;

    if (maxPartitionCount() <= 1)
        return false;

    return size().height() >= minParallelizableHeight
        && size().width() >= minParallelizableWidth
        && (size().height() * size().width()) >= minParallelizableArea;
}

void TextureBackedCanvas::createPartitions()
{
    const IntSize sz = size();
    const bool partitionAxisIsY = sz.height() >= sz.width();
    const int partitionAxisLength = partitionAxisIsY ? sz.height() : sz.width();

    size_t partitionCount;
    partitionCount = std::max(1, std::min(partitionAxisLength / partitioningThreshold + 1, maxPartitionCount()));

    OwnArrayPtr<OwnPtr<Thread> > ownThreads = adoptArrayPtr(new OwnPtr<Thread>[partitionCount]);

    for (size_t i = 0; i < partitionCount; i++) {
        ownThreads[i] = Thread::create("job");
        if (!ownThreads[i])
            return;
    }

    m_partitionCount = partitionCount;
    m_canvases = adoptArrayPtr(new SkCanvas*[m_partitionCount]);
    m_jobs = ownThreads.release();

    const size_t lastPartition = m_partitionCount - 1;
    const size_t partitionLength = partitionAxisLength / m_partitionCount;
    const size_t lastPartitionLength = partitionAxisLength - lastPartition * partitionLength;

    for (size_t i = 0; i < m_partitionCount; ++i) {
        size_t start = i * partitionLength;
        size_t length = partitionLength;
        if (i == lastPartition)
            length = lastPartitionLength;

        SkCanvas* canvas = SkNEW(SkCanvas);

        SkDevice* device = new FlipDevice(canvas, m_backBufferBitmap);
        canvas->setDevice(device)->unref();

        SkRect clipRect;
        if (partitionAxisIsY)
            clipRect = SkRect::MakeXYWH(0, start, sz.width(), length);
        else
            clipRect = SkRect::MakeXYWH(start, 0, length, sz.height());

        canvas->clipRect(clipRect);
        m_canvases[i] = canvas;
    }
}

void TextureBackedCanvas::prepareForDrawing()
{
    if (isBackBufferLocked())
        return;

    ASSERT(!m_hasScheduledWork);

    m_backBuffer->lockBufferForWriting(&m_backBufferBitmap);

    // Call change callback anyway, regardless of lockBuffer success or failure. This
    // way we catch invalid access with crashes instead of random memory overwrites.

    for (unsigned i = 0; i < m_partitionCount; ++i)
        static_cast<FlipDevice*>(m_canvases[i]->getDevice())->backBufferChanged(m_backBufferBitmap);

    static_cast<FlipDevice*>(m_mainCanvas->getDevice())->backBufferChanged(m_backBufferBitmap);
}

void TextureBackedCanvas::syncSoftwareCanvas()
{
    prepareForDrawing();
}

AcceleratedCanvas::BorrowBackBuffer* TextureBackedCanvas::borrowBackBuffer()
{
    BorrowBackBuffer* borrowBackBuffer = new BorrowBackBuffer;

    if (isBackBufferLocked())
        unlockBackBuffer();

    borrowBackBuffer->lendBackBuffer(m_backBuffer.get());

    return borrowBackBuffer;
}

void TextureBackedCanvas::reclaimBackBuffer(BorrowBackBuffer* borrowBackBuffer)
{
    borrowBackBuffer->reclaimBackBuffer();
    delete borrowBackBuffer;
}

void TextureBackedCanvas::swapBuffers()
{
    if (!isBackBufferLocked())
        return;

    OwnPtr<MappedCanvasTexture> nextBackBuffer =
        static_pointer_cast<MappedCanvasTexture>(bufferRing()->takeFrontBufferAndLock());
    if (!nextBackBuffer) {
        nextBackBuffer = MappedCanvasTexture::create(size(), MappedTexture::HasAlpha, MappedTexture::WriteUsingSoftwareAndHardware);
        if (!nextBackBuffer) {
            bufferRing()->submitFrontBufferAndUnlock(0);
            return;
        }
    }

    unlockBackBuffer();

    nextBackBuffer->lockSurface();
    nextBackBuffer->finish();
    m_backBuffer->copyTo(nextBackBuffer.get());

    m_backBuffer->unlockSurface();
    bufferRing()->submitFrontBufferAndUnlock(m_backBuffer.release());
    m_backBuffer = nextBackBuffer.release();
}

void TextureBackedCanvas::unlockBackBuffer()
{
    flushDrawing();
    m_backBuffer->unlockBuffer();
    m_backBufferBitmap.reset();
    // This way we catch invalid access with crashes instead of random memory overwrites.
    for (unsigned i = 0; i < m_partitionCount; ++i)
        static_cast<FlipDevice*>(m_canvases[i]->getDevice())->backBufferChanged(m_backBufferBitmap);
    static_cast<FlipDevice*>(m_mainCanvas->getDevice())->backBufferChanged(m_backBufferBitmap);
}


void TextureBackedCanvas::flushDrawing()
{
    if (!m_hasScheduledWork)
        return;

    for (size_t i = 0; i < m_partitionCount; ++i)
        m_jobs[i]->finish();
    m_hasScheduledWork = false;
}

void TextureBackedCanvas::accessDeviceBitmap(SkBitmap* bm, bool changePixels)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    flushDrawing();
    *bm = m_backBufferBitmap;
}

void TextureBackedCanvas::writePixels(const SkBitmap& bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    flushDrawing();
    m_mainCanvas->writePixels(bitmap, x, y, config8888);
}

bool TextureBackedCanvas::readPixels(SkBitmap* bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    if (!isBackBufferLocked())
        return false;

    ASSERT(!m_hasScheduledWork); // Caller has called accessDeviceBitmap().
    return m_mainCanvas->readPixels(bitmap, x, y, config8888);
}

#define MAKE_CALL_LATER_LAMBDA(func, args1, args2) \
    if (canParallelize()) { \
        for (size_t i = 0; i < m_partitionCount; ++i) \
            scheduleWork(*m_jobs[i], new func##Lambda<LambdaAutoSync> args1); \
    } else {                                                            \
        flushDrawing(); \
        m_mainCanvas->func args2;                       \
    }

#define MAKE_CALL_LATER_LAMBDA_IF(condition, func, args1, args2) \
    if (condition) { \
        for (size_t i = 0; i < m_partitionCount; ++i) \
            scheduleWork(*m_jobs[i], new func##Lambda<LambdaAutoSync> args1); \
    } else {                                                            \
        flushDrawing(); \
        m_mainCanvas->func args2;                       \
    }

void TextureBackedCanvas::save(SkCanvas::SaveFlags flags)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], WTF::makeLambda(m_canvases[i], &SkCanvas::save)(flags));
    }
    m_mainCanvas->save(flags);
}

void TextureBackedCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint, SkCanvas::SaveFlags flags)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    // We cannot parallelize saveLayer. If we did, and then ran into an operation that could not be
    // parallelized, then we would need to paint that to the m_mainCanvas. The m_mainCanvas cannot
    // contain a layer if the worker canvases already have layers.

    flushDrawing();
    int saveCount = m_mainCanvas->saveLayer(bounds, paint, flags);
    if (!m_saveLayerCount)
        m_saveLayerCount = saveCount;
}

void TextureBackedCanvas::saveLayerAlpha(const SkRect* bounds, U8CPU alpha, SkCanvas::SaveFlags flags)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (alpha == 0xFF) {
        saveLayer(bounds, 0, flags);
        return;
    }

    SkPaint tmpPaint;
    tmpPaint.setAlpha(alpha);
    saveLayer(bounds, &tmpPaint, flags);
}

void TextureBackedCanvas::restore()
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (m_saveLayerCount && m_saveLayerCount == m_mainCanvas->getSaveCount() - 1) {
        ASSERT(!canParallelize());

        m_mainCanvas->restore();
        m_saveLayerCount = 0;
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], WTF::makeLambda(m_canvases[i], &SkCanvas::restore)());
    }

    m_mainCanvas->restore();
}

void TextureBackedCanvas::translate(SkScalar dx, SkScalar dy)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], WTF::makeLambda(m_canvases[i], &SkCanvas::translate)(dx, dy));
    }
    m_mainCanvas->translate(dx, dy);
}

void TextureBackedCanvas::scale(SkScalar sx, SkScalar sy)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], WTF::makeLambda(m_canvases[i], &SkCanvas::scale)(sx, sy));
    }
    m_mainCanvas->scale(sx, sy);
}

void TextureBackedCanvas::rotate(SkScalar degrees)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], WTF::makeLambda(m_canvases[i], &SkCanvas::rotate)(degrees));
    }
    m_mainCanvas->rotate(degrees);
}

void TextureBackedCanvas::concat(const SkMatrix& matrix)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], new concatLambda<LambdaAutoSync>(m_canvases[i], matrix));
    }
    m_mainCanvas->concat(matrix);
}

void TextureBackedCanvas::clipRect(const SkRect& rect, SkRegion::Op op, bool doAntiAlias)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], new clipRectLambda<LambdaAutoSync>(m_canvases[i], rect, op, doAntiAlias));
    }
    m_mainCanvas->clipRect(rect, op, doAntiAlias);
}


void TextureBackedCanvas::clipPath(const SkPath& path, SkRegion::Op op, bool doAntiAlias)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    if (canParallelize()) {
        for (size_t i = 0; i < m_partitionCount; ++i)
            scheduleWork(*m_jobs[i], new clipPathLambda<LambdaAutoSync>(m_canvases[i], path, op, doAntiAlias));
    }
    m_mainCanvas->clipPath(path, op, doAntiAlias);
}

void TextureBackedCanvas::drawPoints(SkCanvas::PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawPoints
                              , (m_canvases[i], mode, count, pts, paint, lockFor(paint))
                              , (mode, count, pts, paint));
}

void TextureBackedCanvas::drawRect(const SkRect& rect, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawRect
                              , (m_canvases[i], rect, paint, lockFor(paint))
                              , (rect, paint));
}

void TextureBackedCanvas::drawPath(const SkPath& path, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawPath
                              , (m_canvases[i], path, paint, lockFor(paint))
                              , (path, paint));
}

namespace {
static bool canCopy(const SkBitmap& bitmap)
{
    return bitmap.isNull() || bitmap.pixelRef();
}
}

void TextureBackedCanvas::drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src, const SkRect& dst, const SkPaint* paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canCopy(bitmap) && canParallelize(paint)
                              , drawBitmapRect
                              , (m_canvases[i], bitmap, src, dst, paint, lockFor(paint))
                              , (bitmap, src, dst, paint));
}

void TextureBackedCanvas::drawText(const void* text, size_t byteLength, SkScalar x, SkScalar y, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawText
                              , (m_canvases[i], text, byteLength, x, y, paint, lockFor(paint))
                              , (text, byteLength, x, y, paint));
}

void TextureBackedCanvas::drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawPosText
                              , (m_canvases[i], text, byteLength, pos, paint, lockFor(paint))
                              , (text, byteLength, pos, paint));
}

void TextureBackedCanvas::drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawPosTextH
                              , (m_canvases[i], text, byteLength, xpos, constY, paint, lockFor(paint))
                              , (text, byteLength, xpos, constY, paint));
}

void TextureBackedCanvas::drawTextOnPath(const void* text, size_t byteLength, const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawTextOnPath
                              , (m_canvases[i], text, byteLength, path, matrix, paint, lockFor(paint))
                              , (text, byteLength, path, matrix, paint));
}

void TextureBackedCanvas::drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawLine
                              , (m_canvases[i], x0, y0, x1, y1, paint, lockFor(paint))
                              , (x0, y0, x1, y1, paint));
}

void TextureBackedCanvas::drawOval(const SkRect& oval, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    MAKE_CALL_LATER_LAMBDA_IF(canParallelize(paint), drawOval
                              , (m_canvases[i], oval, paint, lockFor(paint))
                              , (oval, paint));
}

void TextureBackedCanvas::drawEmojiFont(uint16_t index, SkScalar x, SkScalar y, const SkPaint& paint)
{
    if (!isBackBufferLocked()) {
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.
        return;
    }

    flushDrawing();
    android::EmojiFont::Draw(m_mainCanvas.get(), index, x, y, paint);
}

const SkMatrix& TextureBackedCanvas::getTotalMatrix() const
{
    if (!isBackBufferLocked())
        ASSERT_NOT_REACHED(); // prepareForDrawing() should have been called.


    return m_mainCanvas->getTotalMatrix();
}


} // namespace WebCore

#endif // USE(ACCELERATED_CANVAS_LAYER)
