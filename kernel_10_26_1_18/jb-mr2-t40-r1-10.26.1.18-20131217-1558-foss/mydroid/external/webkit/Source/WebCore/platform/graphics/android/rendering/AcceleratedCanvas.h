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

#ifndef AcceleratedCanvas_h
#define AcceleratedCanvas_h

#if USE(ACCELERATED_CANVAS_LAYER)

#include "EGLFence.h"
#include "EGLImageSurface.h"
#include "GraphicsContextFunctions.h"
#include "IntSize.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include <GLES2/gl2.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class LayerAndroid;
class GraphicsContext;
class PlatformGraphicsContextSkia;

class AcceleratedCanvas : public EGLImageSurface {
public:
    enum TextureVerticalOrientation { TopToBottom, BottomToTop };

    virtual ~AcceleratedCanvas() { }

    LayerAndroid* createPlatformLayer();

    virtual void prepareForDrawing() = 0;
    virtual void syncSoftwareCanvas() = 0;

    virtual void accessDeviceBitmap(SkBitmap*, bool changePixels) = 0;
    virtual bool readPixels(SkBitmap*, int x, int y, SkCanvas::Config8888) = 0;
    virtual void writePixels(const SkBitmap&, int x, int y, SkCanvas::Config8888) = 0;

#define DECLARE_FWD_FUNCTION(NAME, PARAMS, CALL_ARGS) \
    virtual void NAME PARAMS = 0;

    FOR_EACH_GFX_CTX_VOID_FUNCTION(DECLARE_FWD_FUNCTION)

#undef DECLARE_FWD_FUNCTION

    virtual void drawEmojiFont(uint16_t index, SkScalar x, SkScalar y, const SkPaint&) = 0;
    virtual const SkMatrix& getTotalMatrix() const = 0;

    virtual SkPixelRef* borrowCanvasPixels(AcceleratedCanvas*) { return 0; }
    virtual void returnCanvasPixels(AcceleratedCanvas*, SkPixelRef*) { }

    class BorrowBackBuffer {
    public:
        BorrowBackBuffer();
        EGLImageBuffer* borrowBackBuffer();
        void lendBackBuffer(EGLImageBuffer*);
        void reclaimBackBuffer();
        void returnBackBuffer(bool needsEGLFence = false);
    private:
        WTF::Mutex m_mutex;
        WTF::ThreadCondition m_condition;
        EGLFence m_returnFence;
        EGLImageBuffer* m_borrowedBackBuffer;
    };
    virtual BorrowBackBuffer* borrowBackBuffer() = 0;
    virtual void reclaimBackBuffer(BorrowBackBuffer*) = 0;

protected:
    AcceleratedCanvas(const IntSize& size)
        : EGLImageSurface(size)
    {
    }
};

} // namespace WebCore

#endif // USE(ACCELERATED_CANVAS_LAYER)

#endif // AcceleratedCanvas_h
