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
#include "GaneshCanvas.h"

#if USE(ACCELERATED_CANVAS_LAYER)
#include "AcceleratedCanvasLambdas.h"
#include "AndroidProperties.h"
#include "AutoRestoreGLState.h"
#include "EGLFence.h"
#include "EGLImage.h"
#include "EGLImageBuffer.h"
#include "EGLImageBufferRing.h"
#include "EmojiFont.h"
#include "GLUtils.h"
#include "GrContext.h"
#include "SkGpuDevice.h"
#include "SkGrTexturePixelRef.h"

#define LOG_TAG "GaneshCanvas"
#include <cutils/log.h>

#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

static const int maxCachedTextures = 256;
static const int maxCachedTextureBytes = 64 * 1024 * 1024;
static const int minGaneshCanvasHeight = 200;
static const int minGaneshCanvasWidth = 200;
static const int minGaneshCanvasArea = 300*300;

GaneshCanvas::GaneshCanvas(const IntSize& canvasSize, bool& success)
    : AcceleratedCanvas(canvasSize)
    , m_renderTargetFBO(0)
    , m_stencilBuffer(0)
{
    ASSERT(!size().isEmpty());
    ASSERT(WTF::isMainThread());

    m_workerCanvas = new SkCanvas;

    if (!AndroidProperties::getStringProperty("webkit.canvas.ganesh", "").contains("noparallel"))
        m_thread = Thread::create("GaneshCanvas");

    if (m_thread)
        success = m_thread->call(WTF::makeLambda(this, &GaneshCanvas::init)());
    else
        success = init();
}

GaneshCanvas::~GaneshCanvas()
{
    if (m_thread)
        m_thread->call(WTF::makeLambda(this, &GaneshCanvas::destroy)());
    else
        destroy();

    delete m_workerCanvas;
}

bool GaneshCanvas::isSuitableFor(const IntSize& size)
{
    String ganeshProperty = AndroidProperties::getStringProperty("webkit.canvas.ganesh", "");
    if (ganeshProperty.contains("disable"))
        return false;

    if (ganeshProperty.contains("force"))
        return true;

    return size.height() >= minGaneshCanvasHeight
        && size.width() >= minGaneshCanvasWidth
        && (size.height() * size.width()) >= minGaneshCanvasArea;
}

// FIXME: http://nvbugs/1007696 Race condition (?) causes assert in the driver during destruction.
// The test-case is a harness that creates and deletes huge amount of canvas contexts.
static WTF::Mutex& eglBugMutex()
{
    DEFINE_STATIC_LOCAL(WTF::Mutex, mutex, ());
    return mutex;
}

bool GaneshCanvas::init()
{
    if (m_thread)
        eglBugMutex().lock();

    m_context = GLContext::create(ResourceLimits::WebContent);
    if (!m_context) {
        ALOGV("Initializing Ganesh failed: failed to create an OpenGL context.");

        if (m_thread)
            eglBugMutex().unlock();

        return false;
    }

    m_backBuffer = EGLImageBufferFromTexture::create(size(), true);
    if (!m_backBuffer) {
        if (m_thread)
            eglBugMutex().unlock();

        return false;
    }

    m_backBuffer->lockSurface();
    glGenFramebuffers(1, &m_renderTargetFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_renderTargetFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backBuffer->sourceContextTextureId(), 0);

    glGenRenderbuffers(1, &m_stencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, size().width(), size().height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ALOGV("Initializing Ganesh failed: glCheckFramebufferStatus() did not return GL_FRAMEBUFFER_COMPLETE.");

        if (m_thread)
            eglBugMutex().unlock();

        return false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_ganeshContext = GrContext::Create(kOpenGL_Shaders_GrEngine, 0);
    m_ganeshContext->unref();
    m_ganeshContext->setTextureCacheLimits(maxCachedTextures, maxCachedTextureBytes);

    GrPlatformRenderTargetDesc targetDesc;
    targetDesc.fWidth = size().width();
    targetDesc.fHeight = size().height();
    targetDesc.fConfig = kSkia8888_PM_GrPixelConfig;
    targetDesc.fSampleCnt = 0;
    targetDesc.fStencilBits = 8;
    targetDesc.fRenderTargetHandle = m_renderTargetFBO;
    GrRenderTarget* renderTarget = m_ganeshContext->createPlatformRenderTarget(targetDesc);

    SkDevice* device = new SkGpuDevice(m_ganeshContext.get(), renderTarget);
    renderTarget->unref();
    m_workerCanvas->setDevice(device)->unref();

    if (m_thread)
        eglBugMutex().unlock();

    return true;
}

void GaneshCanvas::destroy()
{
#if DEBUG
    assertInGrThread();
#endif

    // If this is not constructed, don't destroy
    if (!m_context)
        return;

    if (m_thread)
        eglBugMutex().lock();

    makeContextCurrent();

    // Delete everybody that uses GL before destroying our context.
    m_workerCanvas->setDevice(0);
    m_ganeshContext = 0;

    if (m_stencilBuffer)
        glDeleteRenderbuffers(1, &m_stencilBuffer);

    if (m_renderTargetFBO)
        glDeleteFramebuffers(1, &m_renderTargetFBO);

    bufferRing()->deleteFreeBuffers();

    OwnPtr<EGLImageBuffer> frontBuffer = bufferRing()->takeFrontBufferAndLock();
    if (frontBuffer)
        frontBuffer->deleteBufferSource();
    bufferRing()->submitFrontBufferAndUnlock(frontBuffer.release());

    if (m_backBuffer) {
        m_backBuffer->unlockSurface();
        m_backBuffer.clear();
    }

    m_context.clear();

    if (m_thread) {
        EGLBoolean ret = eglReleaseThread();
        ASSERT_UNUSED(ret, ret == EGL_TRUE);
        eglBugMutex().unlock();
    }
}

#if DEBUG
void GaneshCanvas::assertInGrThread()
{
    if (m_thread)
        ASSERT(currentThread() == m_thread->id());
    else
        ASSERT(WTF::isMainThread());
}
#endif

void GaneshCanvas::swapBuffers()
{
    if (m_thread) {
        m_thread->call(WTF::makeLambda(this, &GaneshCanvas::performBufferSwap)());
        m_thread->callLater(WTF::makeLambda(this, &GaneshCanvas::updateRenderTarget)());
    } else {
        performBufferSwap();
        updateRenderTarget();
    }
}

void GaneshCanvas::submitBackBuffer()
{
    OwnPtr<EGLImageBufferFromTexture*> newBackBuffer = static_pointer_cast<EGLImageBufferFromTexture>(bufferRing()->takeFreeBuffer());

    if (!newBackBuffer) {
        if (m_thread)
            newBackBuffer = m_thread->call(WTF::makeLambda(this, &GaneshCanvas::createBackBuffer)());
        else
            newBackBuffer = createBackBuffer();
    }

    if (!newBackBuffer.get())
        return; // Swap failed because we failed to allocate new buffer.

    ASSERT(newBackBuffer->size() == m_backBuffer->size());

    // The swap will succeed, and thus we can send the old back buffer to the caller before
    // copyPreviousBackBuffer finishes. Assign the new back buffer here, so that WebKit thread will
    // always have the correct back buffer.

    OwnPtr<EGLImageBufferFromTexture> previousBackBuffer = m_backBuffer.release();
    m_backBuffer = newBackBuffer.release();
    if (m_thread)
        m_thread->callLater(WTF::makeLambda(this, &GaneshCanvas::setupNextBackBuffer)(previousBackBuffer.get()));
    else
        setupNextBackBuffer(previousBackBuffer.get());

    // The previousBackBuffer will be locked by GaneshCanvas until setupNextBackBuffer is finished with it.
    // This means that we can access it even though we release the ownership here.
    bufferRing()->submitBuffer(previousBackBuffer.release());
}

PassOwnPtr<EGLImageBufferFromTexture> GaneshCanvas::createBackBuffer()
{
#if DEBUG
    assertInGrThread();
#endif
    makeContextCurrent();

    return EGLImageBufferFromTexture::create(size(), true);
}

void GaneshCanvas::performBufferSwap()
{
#if DEBUG
    assertInGrThread();
#endif
    makeContextCurrent();

    m_workerCanvas->flush();

    m_backBuffer->setFence();

    m_backBuffer->unlockSurface();

    OwnPtr<EGLImageBufferFromTexture> newBackBuffer =
        static_pointer_cast<EGLImageBufferFromTexture>(bufferRing()->takeFrontBufferAndLock());

    if (!newBackBuffer)
        newBackBuffer = EGLImageBufferFromTexture::create(m_backBuffer->size(), true);

    if (!newBackBuffer) {
        // Couldn't create a new back buffer, we should not submit the old one.
        bufferRing()->submitFrontBufferAndUnlock(0);
    } else {
        ASSERT(newBackBuffer->size() == m_backBuffer->size());
        bufferRing()->submitFrontBufferAndUnlock(m_backBuffer.release());
        m_backBuffer = newBackBuffer.release();
    }

    m_backBuffer->lockSurface();
}

void GaneshCanvas::setupNextBackBuffer(EGLImageBufferFromTexture* previousBackBuffer)
{
#if DEBUG
    assertInGrThread();
#endif
    makeContextCurrent();

    m_workerCanvas->flush();

    previousBackBuffer->setFence();

    m_backBuffer->lockSurface();

    // After this, the caller is free to do anything it wants to the old back buffer. The buffer
    // will be deleted in this thread, so it will be valid at least during the copy below.
    previousBackBuffer->unlockSurface();

    updateRenderTarget();
}

void GaneshCanvas::updateRenderTarget()
{
#if DEBUG
    assertInGrThread();
#endif
    ASSERT(m_context->isCurrent());

    AutoRestoreFramebufferBinding bindFramebuffer(m_renderTargetFBO);

    // Copy the previous backbuffer (attached to m_renderTargetFBO) to m_backBuffer.
    m_backBuffer->finish();
    {
        AutoRestoreTextureBinding2D bindTexture2D(m_backBuffer->sourceContextTextureId());
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size().width(), size().height());
    }

    // Point Ganesh's rendering target at m_backBuffer.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backBuffer->sourceContextTextureId(), 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ASSERT_NOT_REACHED();
}

void GaneshCanvas::deleteFreeBuffers()
{
    if (m_thread)
        m_thread->callLater(WTF::makeLambda(bufferRing(), &EGLImageBufferRing::deleteFreeBuffers)());
    else {
        makeContextCurrent();
        bufferRing()->deleteFreeBuffers();
    }
}

void GaneshCanvas::makeContextCurrent()
{
    if (m_thread)
        return;

    m_context->makeCurrent();
}

void GaneshCanvas::accessDeviceBitmap(SkBitmap* bitmap, bool changePixels)
{
    if (m_thread) {
        // We have to return SkBitmap with no pixel ref. This call cannot be forwarded to the
        // worker thread, because the result of m_workerCanvas->accessBitmap contains a SkBitmapRef
        // which is valid only in the worker thread.
        SkBitmap tmp;
        tmp.setConfig(m_workerCanvas->getDevice()->config(), size().width(), size().height());
        *bitmap = tmp;
        return;
    }
    ASSERT(m_context->isCurrent());
    *bitmap = m_workerCanvas->getDevice()->accessBitmap(changePixels);
}

void GaneshCanvas::writePixels(const SkBitmap& bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    if (m_thread) {
        m_thread->call(WTF::makeLambda(m_workerCanvas, &SkCanvas::writePixels)(bitmap, x, y, config8888));
        return;
    }
    ASSERT(m_context->isCurrent());
    m_workerCanvas->writePixels(bitmap, x, y, config8888);
}

namespace {
static bool localReadPixels(SkCanvas* canvas, SkBitmap* bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    return canvas->readPixels(bitmap, x, y, config8888);
}
}

bool GaneshCanvas::readPixels(SkBitmap* bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    if (m_thread)
        return m_thread->call(WTF::makeLambda(localReadPixels)(m_workerCanvas, bitmap, x, y, config8888));
    ASSERT(m_context->isCurrent());
    return m_workerCanvas->readPixels(bitmap, x, y, config8888);
}

class HWCanvasPixelRef : public SkGrTexturePixelRef {
public:
    HWCanvasPixelRef(AcceleratedCanvas* canvas, GrContext* targetContext)
        : SkGrTexturePixelRef(0)
        , m_canvas(canvas)
        , m_targetContext(targetContext)
        , m_borrowBackBuffer(canvas->borrowBackBuffer())
        , m_backBuffer(0)
        , m_textureID(0)
#if DEBUG
        , m_hasReturnedBackBuffer(false)
#endif
    {}
    virtual ~HWCanvasPixelRef() { ASSERT(!m_backBuffer); }

    void ensureTexture()
    {
        ASSERT(!m_hasReturnedBackBuffer);
        ASSERT(m_canvas->isInverted());
        if (fSurface)
            return;

        m_backBuffer = m_borrowBackBuffer->borrowBackBuffer();
        if (!m_backBuffer || !m_backBuffer->lockBufferForReadingGL(&m_textureID))
            m_textureID = 0;

        GrPlatformTextureDesc textureDesc;
        textureDesc.fWidth = m_canvas->size().width();
        textureDesc.fHeight = m_canvas->size().height();
        textureDesc.fConfig = kRGBA_8888_GrPixelConfig;
        textureDesc.fTextureHandle = m_textureID;

        fSurface = m_targetContext->createPlatformTexture(textureDesc);
    }

    virtual SkGpuTexture* getTexture()
    {
        ensureTexture();
        return SkGrTexturePixelRef::getTexture();
    }

    virtual SkPixelRef* deepCopy(SkBitmap::Config dstConfig, const SkIRect* subset)
    {
        ensureTexture();
        return SkGrTexturePixelRef::deepCopy(dstConfig, subset);
    }

    virtual bool onReadPixels(SkBitmap* dst, const SkIRect* subset)
    {
        ensureTexture();
        return SkGrTexturePixelRef::onReadPixels(dst, subset);
    }

    void returnBackBuffer()
    {
        GrSafeUnref(fSurface);
        fSurface = 0;
        if (m_textureID) {
             m_backBuffer->unlockBufferGL(m_textureID);
             m_textureID = 0;
        }

        // Make sure we borrow the back buffer to unblock the other thread.
        if (!m_backBuffer)
            m_backBuffer = m_borrowBackBuffer->borrowBackBuffer();
        m_borrowBackBuffer->returnBackBuffer(true);
        m_backBuffer = 0;

#if DEBUG
        m_hasReturnedBackBuffer = true;
#endif
    }

    void canvasReclaimBackBuffer()
    {
        // 'reclaimBackBuffer' will cause m_canvas's thread to wait until we
        // call 'm_borrowBackBuffer->returnBackBuffer()' in deleteTexture.
        m_canvas->reclaimBackBuffer(m_borrowBackBuffer);
    }

private:
    AcceleratedCanvas* m_canvas;
    GrContext* m_targetContext;
    AcceleratedCanvas::BorrowBackBuffer* m_borrowBackBuffer;
    EGLImageBuffer* m_backBuffer;
    GLuint m_textureID;
#if DEBUG
    bool m_hasReturnedBackBuffer;
#endif
};

SkPixelRef* GaneshCanvas::borrowCanvasPixels(AcceleratedCanvas* canvas)
{
    if (!canvas->isInverted())
        return 0;
    return new HWCanvasPixelRef(canvas, m_ganeshContext.get());
}

void GaneshCanvas::returnCanvasPixels(AcceleratedCanvas* canvas, SkPixelRef* pixels)
{
    HWCanvasPixelRef* canvasPixelRef = static_cast<HWCanvasPixelRef*>(pixels);

    if (m_thread)
        m_thread->callLater(WTF::makeLambda(this, &GaneshCanvas::returnBackBuffer)(canvasPixelRef));
    else
        returnBackBuffer(canvasPixelRef);

    canvasPixelRef->canvasReclaimBackBuffer();
}

void GaneshCanvas::returnBackBuffer(HWCanvasPixelRef* canvasPixelRef)
{
#if DEBUG
    assertInGrThread();
#endif

    makeContextCurrent();

    m_workerCanvas->flush();

    canvasPixelRef->returnBackBuffer();
    canvasPixelRef->unref();
}

AcceleratedCanvas::BorrowBackBuffer* GaneshCanvas::borrowBackBuffer()
{
    BorrowBackBuffer* borrowBackBuffer = new BorrowBackBuffer;
    if (m_thread)
        m_thread->callLater(WTF::makeLambda(this, &GaneshCanvas::lendBackBuffer)(borrowBackBuffer, m_backBuffer.get()));
    else
        lendBackBuffer(borrowBackBuffer, m_backBuffer.get());

    return borrowBackBuffer;
}

void GaneshCanvas::lendBackBuffer(BorrowBackBuffer* borrowBackBuffer, EGLImageBufferFromTexture* backBuffer)
{
#if DEBUG
    assertInGrThread();
#endif

    makeContextCurrent();

    m_workerCanvas->flush();
    glFinish();

    borrowBackBuffer->lendBackBuffer(backBuffer);
}

namespace {
void localReclaimBackBuffer(AcceleratedCanvas::BorrowBackBuffer* borrowBackBuffer)
{
    borrowBackBuffer->reclaimBackBuffer();
    delete borrowBackBuffer;
}
}

void GaneshCanvas::reclaimBackBuffer(BorrowBackBuffer* borrowBackBuffer)
{
    if (m_thread)
        m_thread->callLater(WTF::makeLambda(localReclaimBackBuffer)(borrowBackBuffer));
    else
        localReclaimBackBuffer(borrowBackBuffer);
}

#define MAKE_CALL(func, args) \
    if (m_thread) { \
        m_thread->call(WTF::makeLambda(m_workerCanvas, &SkCanvas::func) args); \
        return; \
    } \
    ASSERT(m_context->isCurrent()); \
    m_workerCanvas->func args;

#define MAKE_CALL_LATER(func, args) \
    if (m_thread) { \
        m_thread->callLater(WTF::makeLambda(m_workerCanvas, &SkCanvas::func) args); \
        return; \
    } \
    ASSERT(m_context->isCurrent()); \
    m_workerCanvas->func args;

#define MAKE_CALL_LATER_LAMBDA(func, args1, args2) \
    if (m_thread) { \
        m_thread->callLater(new func##Lambda<> args1 ); \
        return; \
    } \
    ASSERT(m_context->isCurrent()); \
    m_workerCanvas->func args2;

void GaneshCanvas::save(SkCanvas::SaveFlags flags)
{
    MAKE_CALL_LATER(save, (flags));
}

void GaneshCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint, SkCanvas::SaveFlags flags)
{
    MAKE_CALL_LATER_LAMBDA(saveLayer, (m_workerCanvas, bounds, paint, flags), (bounds, paint, flags));
}

void GaneshCanvas::saveLayerAlpha(const SkRect* bounds, U8CPU alpha, SkCanvas::SaveFlags flags)
{
    if (alpha == 0xFF) {
        saveLayer(bounds, 0, flags);
        return;
    }

    SkPaint tmpPaint;
    tmpPaint.setAlpha(alpha);
    saveLayer(bounds, &tmpPaint, flags);
}

void GaneshCanvas::restore()
{
    MAKE_CALL_LATER(restore, ());
}

void GaneshCanvas::translate(SkScalar dx, SkScalar dy)
{
    MAKE_CALL_LATER(translate, (dx, dy));
}

void GaneshCanvas::scale(SkScalar sx, SkScalar sy)
{
    MAKE_CALL_LATER(scale, (sx, sy));
}

void GaneshCanvas::rotate(SkScalar degrees)
{
    MAKE_CALL_LATER(rotate, (degrees));
}

void GaneshCanvas::concat(const SkMatrix& matrix)
{
    MAKE_CALL_LATER_LAMBDA(concat, (m_workerCanvas, matrix), (matrix));
}

void GaneshCanvas::clipRect(const SkRect& rect, SkRegion::Op op, bool doAntiAlias)
{
    MAKE_CALL_LATER_LAMBDA(clipRect, (m_workerCanvas, rect, op, doAntiAlias), (rect, op, doAntiAlias));
}

void GaneshCanvas::clipPath(const SkPath& path, SkRegion::Op op, bool doAntiAlias)
{
    MAKE_CALL_LATER_LAMBDA(clipPath, (m_workerCanvas, path, op, doAntiAlias), (path, op, doAntiAlias));
}

void GaneshCanvas::drawPoints(SkCanvas::PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint)
{
    MAKE_CALL(drawPoints, (mode, count, pts, paint));
}

void GaneshCanvas::drawRect(const SkRect& rect, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawRect, (m_workerCanvas, rect, paint), (rect, paint));
}

void GaneshCanvas::drawPath(const SkPath& path, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawPath, (m_workerCanvas, path, paint), (path, paint));
}

namespace {
static bool canCopy(const SkBitmap& bitmap)
{
    return bitmap.isNull() || bitmap.pixelRef();
}
}

void GaneshCanvas::drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src, const SkRect& dst, const SkPaint* paint)
{
    if (m_thread) {
        if (canCopy(bitmap))
            m_thread->callLater(new drawBitmapRectLambda<>(m_workerCanvas, bitmap, src, dst, paint));
        else
            m_thread->call(new drawBitmapRectLambda<>(m_workerCanvas, bitmap, src, dst, paint));
        return;
    }
    ASSERT(m_context->isCurrent());
    m_workerCanvas->drawBitmapRect(bitmap, src, dst, paint);
}

void GaneshCanvas::drawText(const void* text, size_t byteLength, SkScalar x, SkScalar y, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawText, (m_workerCanvas, text, byteLength, x, y, paint), (text, byteLength, x, y, paint));
}

void GaneshCanvas::drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawPosText, (m_workerCanvas, text, byteLength, pos, paint), (text, byteLength, pos, paint));
}

void GaneshCanvas::drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawPosTextH, (m_workerCanvas, text, byteLength, xpos, constY, paint), (text, byteLength, xpos, constY, paint));
}

void GaneshCanvas::drawTextOnPath(const void* text, size_t byteLength, const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawTextOnPath, (m_workerCanvas, text, byteLength, path, matrix, paint), (text, byteLength, path, matrix, paint));
}

void GaneshCanvas::drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawLine, (m_workerCanvas, x0, y0, x1, y1, paint), (x0, y0, x1, y1, paint));
}

void GaneshCanvas::drawOval(const SkRect& oval, const SkPaint& paint)
{
    MAKE_CALL_LATER_LAMBDA(drawOval, (m_workerCanvas, oval, paint), (oval, paint));
}

void GaneshCanvas::drawEmojiFont(uint16_t index, SkScalar x, SkScalar y, const SkPaint& paint)
{
    if (m_thread) {
        m_thread->call(WTF::makeLambda(&android::EmojiFont::Draw)(m_workerCanvas, index, x, y, paint));
        return;
    }
    ASSERT(m_context->isCurrent());
    android::EmojiFont::Draw(m_workerCanvas, index, x, y, paint);
}

namespace {
static void localGetTotalMatrix(const SkCanvas* canvas, SkMatrix* matrix)
{
    *matrix = canvas->getTotalMatrix();
}
}

const SkMatrix& GaneshCanvas::getTotalMatrix() const
{
    if (m_thread) {
        m_thread->call(WTF::makeLambda(&localGetTotalMatrix)(m_workerCanvas, const_cast<SkMatrix*>(&m_retMatrix)));
        return m_retMatrix;
    }
    return m_workerCanvas->getTotalMatrix();
}

} // namespace WebCore

#endif // USE(ACCELERATED_CANVAS_LAYER)
