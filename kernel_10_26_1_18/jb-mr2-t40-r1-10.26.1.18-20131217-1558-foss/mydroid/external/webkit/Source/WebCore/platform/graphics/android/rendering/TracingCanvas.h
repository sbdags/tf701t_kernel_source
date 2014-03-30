/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
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

#ifndef TracingCanvas_h
#define TracingCanvas_h

#if USE(ACCELERATED_COMPOSITING)
#if ENABLE(TRACE_TEXGEN_SKIA)

#include <SkCanvas.h>

namespace WebCore {

class TracingCanvas : public SkCanvas {
public:
    virtual int save(SaveFlags = kMatrixClip_SaveFlag);
    virtual int saveLayer(const SkRect* bounds, const SkPaint*,
                          SaveFlags = kARGB_ClipLayer_SaveFlag);
    virtual void restore();
    virtual bool translate(SkScalar dx, SkScalar dy);
    virtual bool scale(SkScalar sx, SkScalar sy);
    virtual bool rotate(SkScalar degrees);
    virtual bool skew(SkScalar sx, SkScalar sy);
    virtual bool concat(const SkMatrix&);
    virtual void setMatrix(const SkMatrix&);
    virtual bool clipRect(const SkRect&, SkRegion::Op, bool);
    virtual bool clipPath(const SkPath&, SkRegion::Op, bool);
    virtual bool clipRegion(const SkRegion& deviceRgn,
                            SkRegion::Op);

    virtual void drawPaint(const SkPaint&);
    virtual void drawPoints(PointMode, size_t count, const SkPoint pts[],
                            const SkPaint&);
    virtual void drawRect(const SkRect&, const SkPaint&);
    virtual void drawPath(const SkPath&, const SkPaint&);
    virtual void drawBitmap(const SkBitmap&, SkScalar left, SkScalar top,
                            const SkPaint*);
    virtual void drawBitmapRect(const SkBitmap&, const SkIRect* src,
                                const SkRect& dst, const SkPaint*);
    virtual void drawBitmapMatrix(const SkBitmap&, const SkMatrix&,
                                  const SkPaint*);
    virtual void drawSprite(const SkBitmap&, int left, int top,
                            const SkPaint*);
    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint&);
    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint&);
    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint&);
    virtual void drawTextOnPath(const void* text, size_t byteLength,
                                const SkPath&, const SkMatrix*,
                                const SkPaint&);
    virtual void drawPicture(SkPicture&);
    virtual void drawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint vertices[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint&);

    virtual SkBounder* setBounder(SkBounder*);
    virtual SkDrawFilter* setDrawFilter(SkDrawFilter*);
};

} // namespace WebCore

#endif // ENABLE(TRACE_TEXGEN_SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
#endif // TracingCanvas_h
