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

#ifndef GaneshCanvas_h
#define GaneshCanvas_h

#if USE(ACCELERATED_CANVAS_LAYER)

#include "AcceleratedCanvas.h"
#include "GLContext.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <wtf/DelegateThread.h>
#include <wtf/Lambda.h>
#include <wtf/MainThread.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

class GrContext;

namespace WebCore {

class EGLImageBufferFromTexture;
class HWCanvasPixelRef;

class GaneshCanvas : public AcceleratedCanvas {
public:
    static PassRefPtr<GaneshCanvas> create(const IntSize& size)
    {
        bool success = false;
        RefPtr<GaneshCanvas> canvas = adoptRef(new GaneshCanvas(size, success));
        if (!success)
            return 0;

        return canvas.release();
    }

    static bool isSuitableFor(const IntSize&);

    ~GaneshCanvas();

    // AcceleratedCanvas/EGLImageSurface methods
    virtual bool isInverted() const { return true; }

    virtual void swapBuffers();

    virtual bool supportsQuadBuffering() const { return true; }
    virtual void submitBackBuffer();

    virtual void deleteFreeBuffers();

    virtual void prepareForDrawing()
    {
        ASSERT(WTF::isMainThread());
        makeContextCurrent();
    }
    virtual void syncSoftwareCanvas()
    {
        ASSERT(WTF::isMainThread());
        makeContextCurrent();
    }

    virtual void accessDeviceBitmap(SkBitmap*, bool changePixels);
    virtual void writePixels(const SkBitmap&, int x, int y, SkCanvas::Config8888);
    virtual bool readPixels(SkBitmap*, int x, int y, SkCanvas::Config8888);

    virtual SkPixelRef* borrowCanvasPixels(AcceleratedCanvas*);
    virtual void returnCanvasPixels(AcceleratedCanvas*, SkPixelRef*);

    virtual BorrowBackBuffer* borrowBackBuffer();
    virtual void reclaimBackBuffer(BorrowBackBuffer*);

#define DECLARE_FWD_FUNCTION(NAME, PARAMS, CALL_ARGS) \
    virtual void NAME PARAMS;

    FOR_EACH_GFX_CTX_VOID_FUNCTION(DECLARE_FWD_FUNCTION)

#undef DECLARE_FWD_FUNCTION

    virtual void drawEmojiFont(uint16_t index, SkScalar x, SkScalar y, const SkPaint&);
    virtual const SkMatrix& getTotalMatrix() const;

private:
    GaneshCanvas(const IntSize&, bool& success);
#if DEBUG
    void assertInGrThread();
#endif

    void makeContextCurrent();
    bool init();
    void destroy();
    PassOwnPtr<EGLImageBufferFromTexture> createBackBuffer();
    void performBufferSwap();
    void setupNextBackBuffer(EGLImageBufferFromTexture* previousBackBuffer);
    void updateRenderTarget();
    void returnBackBuffer(HWCanvasPixelRef* pixels);
    void lendBackBuffer(BorrowBackBuffer*, EGLImageBufferFromTexture*);

    static const unsigned ThreadQueueCapacity = 2048;
    typedef WTF::DelegateThread<ThreadQueueCapacity> Thread;

    OwnPtr<EGLImageBufferFromTexture> m_backBuffer;
    OwnPtr<Thread> m_thread;
    SkCanvas* m_workerCanvas;
    SkRefPtr<GrContext> m_ganeshContext;

    OwnPtr<GLContext> m_context;
    GLuint m_renderTargetFBO;
    GLuint m_stencilBuffer;
    mutable SkMatrix m_retMatrix;
};

} // namespace WebCore

#endif // USE(ACCELERATED_CANVAS_LAYER)

#endif // GaneshCanvas_h
